/*
 * Copyright (c) 2011-2021 Mellanox Technologies Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the Mellanox Technologies Ltd nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

#ifndef IOHANDLERS_H_
#define IOHANDLERS_H_
#include "common.h"

//==============================================================================
class IoHandler {
public:
    IoHandler(int _fd_min, int _fd_max, int _fd_num, int _look_start, int _look_end);
    virtual ~IoHandler();

    inline int get_look_start() const { return m_look_start; }
    inline int get_look_end() const { return m_look_end; }

    virtual int prepareNetwork() = 0;
    void warmup(Message *pMsgRequest) const;

    const int m_fd_min, m_fd_max, m_fd_num;

protected:
    int m_look_start;
    int m_look_end; // non const because of epoll
};

//==============================================================================
class IoRecvfrom : public IoHandler {
public:
    IoRecvfrom(int _fd_min, int _fd_max, int _fd_num);
    virtual ~IoRecvfrom();

    inline void update() {}
    inline int waitArrival() { return (m_fd_num); }
    inline int analyzeArrival(int ifd) const {
        assert(g_fds_array[ifd] && "invalid fd");

        int active_fd_count = g_fds_array[ifd]->active_fd_count;
        int *active_fd_list = g_fds_array[ifd]->active_fd_list;

        assert(active_fd_list && "corrupted fds_data object");

        return (active_fd_count ? active_fd_list[0] : ifd);
    }

    virtual int prepareNetwork();
};

//==============================================================================
/*
 * limitations (due to no real iomux):
 * 1. client must open sockets and send packets in the same order as the server try to receive them
 *(client and server feed files must have the same order).
 * 2. no support for multiple clients (parallel/serial) for the same server (except for TCP serial
 *clients).
 * 3. no support for TCP listen socket to accept more than one connection (e.g. identical two lines
 *in feed file).
 *
 * In order to overcome this limitations we must know the state of each socket at every iteration
 *(like real iomux).
 * It can be done by loop of non-blocking recvfrom with MSG_PEEK over all sockets at each iteration
 *(similar to select internal implementation).
 *
 * NOTE: currently, IoRecvfromMUX can replace IoRecvfrom, but it is less efficient.
 */
class IoRecvfromMUX : public IoHandler {
public:
    IoRecvfromMUX(int _fd_min, int _fd_max, int _fd_num);
    virtual ~IoRecvfromMUX();

    inline void update() {
        m_fd_min_all = m_fd_min;
        m_fd_max_all = m_fd_max;
        for (int ifd = m_fd_min; ifd <= m_fd_max; ifd++) {
            if (g_fds_array[ifd]) {
                int i = 0;
                int active_fd_count = g_fds_array[ifd]->active_fd_count;
                int *active_fd_list = g_fds_array[ifd]->active_fd_list;

                assert(active_fd_list && "corrupted fds_data object");

                while (active_fd_count) {
                    /* process active sockets in case TCP (listen sockets are set in
                     * prepareNetwork()) and
                     * skip active socket in case UDP (it is the same with set in prepareNetwork())
                     */
                    if (active_fd_list[i] != (int)INVALID_SOCKET) {
                        if (active_fd_list[i] != ifd) {
                            m_fd_min_all = _min(m_fd_min_all, active_fd_list[i]);
                            m_fd_max_all = _max(m_fd_max_all, active_fd_list[i]);

                            /* it is possible to set the same socket */
                            errno = 0;
                        }
                        active_fd_count--;
                    }
                    i++;

                    assert((i < MAX_ACTIVE_FD_NUM) &&
                           "maximum number of active connection to the single TCP addr:port");
                }
            }
        }

        if (m_look_start > m_fd_max || m_look_start < m_fd_min)
            m_look_start = m_fd_max;
        else if (m_look_start == m_fd_max)
            m_look_start = m_fd_min_all - 1;
    }

    inline int waitArrival() {
        do {
            m_look_start++;
            if (m_look_start > m_fd_max_all) m_look_start = m_fd_min_all;
        } while (!g_fds_array[m_look_start] || g_fds_array[m_look_start]->active_fd_count);
        m_look_end = m_look_start + 1;
        return 1;
    }

    inline int analyzeArrival(int ifd) const {
        assert(g_fds_array[ifd] && "invalid fd");

        int active_fd_count = g_fds_array[ifd]->active_fd_count;
        int *active_fd_list = g_fds_array[ifd]->active_fd_list;

        assert(active_fd_list && "corrupted fds_data object");

        return (active_fd_count ? active_fd_list[0] : ifd);
    }

    virtual int prepareNetwork();

    int m_fd_min_all, m_fd_max_all;
};

//==============================================================================
class IoSelect : public IoHandler {
public:
    IoSelect(int _fd_min, int _fd_max, int _fd_num);
    virtual ~IoSelect();

    //------------------------------------------------------------------------------
    inline void update() {
        int ifd = 0;

        FD_ZERO(&m_save_fds);
        m_look_start = m_fd_min;
        m_look_end = m_fd_max;
        for (ifd = m_fd_min; ifd <= m_fd_max; ifd++) {
            if (g_fds_array[ifd]) {
                int i = 0;
                int active_fd_count = g_fds_array[ifd]->active_fd_count;
                int *active_fd_list = g_fds_array[ifd]->active_fd_list;

                FD_SET(ifd, &m_save_fds);

                assert(active_fd_list && "corrupted fds_data object");

                while (active_fd_count) {
                    /* process active sockets in case TCP (listen sockets are set in
                     * prepareNetwork()) and
                     * skip active socket in case UDP (it is the same with set in prepareNetwork())
                     */
                    if (active_fd_list[i] != (int)INVALID_SOCKET) {
                        if (active_fd_list[i] != ifd) {
                            FD_SET(active_fd_list[i], &m_save_fds);
                            m_look_start = _min(m_look_start, active_fd_list[i]);
                            m_look_end = _max(m_look_end, active_fd_list[i]);

                            /* it is possible to set the same socket */
                            errno = 0;
                        }
                        active_fd_count--;
                    }
                    i++;

                    assert((i < MAX_ACTIVE_FD_NUM) &&
                           "maximum number of active connection to the single TCP addr:port");
                }
            }
        }
        m_look_end++;
    }

    //------------------------------------------------------------------------------
    inline int waitArrival() {
        if (mp_timeout_timeval) {
            memcpy(mp_timeout_timeval, g_pApp->m_const_params.select_timeout,
                   sizeof(struct timeval));
        }
        memcpy(&m_readfds, &m_save_fds, sizeof(fd_set));
        return select(m_look_end, &m_readfds, NULL, NULL, mp_timeout_timeval);
    }

    //------------------------------------------------------------------------------
    inline int analyzeArrival(int ifd) const { return FD_ISSET(ifd, &m_readfds) ? ifd : 0; }

    virtual int prepareNetwork();

private:
    struct timeval m_timeout_timeval;
    struct timeval *const mp_timeout_timeval;
    fd_set m_readfds, m_save_fds;
};

#ifndef WIN32
//==============================================================================
class IoPoll : public IoHandler {
public:
    IoPoll(int _fd_min, int _fd_max, int _fd_num);
    IoPoll(const IoPoll &);
    virtual ~IoPoll();

    //------------------------------------------------------------------------------
    inline void update() {
        int ifd = 0;

        m_look_start = 0;
        m_look_end = m_fd_num;
        for (ifd = m_fd_min; ifd <= m_fd_max; ifd++) {
            if (g_fds_array[ifd]) {
                int i = 0;
                int active_fd_count = g_fds_array[ifd]->active_fd_count;
                int *active_fd_list = g_fds_array[ifd]->active_fd_list;

                assert(active_fd_list && "corrupted fds_data object");

                while (active_fd_count) {
                    /* process active sockets in case TCP (listen sockets are set in
                     * prepareNetwork()) and
                     * skip active socket in case UDP (it is the same with set in prepareNetwork())
                     */
                    if (active_fd_list[i] != (int)INVALID_SOCKET) {
                        if (active_fd_list[i] != ifd) {
                            mp_poll_fd_arr[m_look_end].fd = active_fd_list[i];
                            mp_poll_fd_arr[m_look_end].events = POLLIN | POLLPRI;

                            /* it is possible to set the same socket
                             * EEXIST error appears in this case but it harmless condition
                             */
                            errno = 0;

                            m_look_end++;
                        }
                        active_fd_count--;
                    }
                    i++;

                    assert((i < MAX_ACTIVE_FD_NUM) &&
                           "maximum number of active connection to the single TCP addr:port");
                }
            }
        }
    }

    //------------------------------------------------------------------------------
    inline int waitArrival() { return poll(mp_poll_fd_arr, m_look_end, m_timeout_msec); }

    //------------------------------------------------------------------------------
    inline int analyzeArrival(int ifd) const {
        assert((ifd < MAX_FDS_NUM) && "exceeded tool limitation (MAX_FDS_NUM)");

        if (mp_poll_fd_arr[ifd].revents & POLLIN || mp_poll_fd_arr[ifd].revents & POLLPRI ||
            mp_poll_fd_arr[ifd].revents & POLLERR || mp_poll_fd_arr[ifd].revents & POLLHUP) {
            return mp_poll_fd_arr[ifd].fd;
        } else {
            return 0;
        }
    }

    virtual int prepareNetwork();

private:
    const int m_timeout_msec;
    struct pollfd *mp_poll_fd_arr;
};

#ifndef __FreeBSD__
//==============================================================================
class IoEpoll : public IoHandler {
public:
    IoEpoll(int _fd_min, int _fd_max, int _fd_num);
    virtual ~IoEpoll();

    //------------------------------------------------------------------------------
    inline void update() {
        int ifd = 0;
        struct epoll_event ev = { 0, { 0 } };

        m_look_start = 0;
        m_look_end = m_fd_num;
        m_max_events = m_fd_num;
        for (ifd = m_fd_min; ifd <= m_fd_max; ifd++) {
            if (g_fds_array[ifd]) {
                int i = 0;
                int active_fd_count = g_fds_array[ifd]->active_fd_count;
                int *active_fd_list = g_fds_array[ifd]->active_fd_list;

                assert(active_fd_list && "corrupted fds_data object");

                while (active_fd_count) {
                    /* process active sockets in case TCP (listen sockets are set in
                     * prepareNetwork()) and
                     * skip active socket in case UDP (it is the same with set in prepareNetwork())
                     */
                    if (active_fd_list[i] != (int)INVALID_SOCKET) {
                        if (active_fd_list[i] != ifd) {
                            ev.data.fd = active_fd_list[i];
                            ev.events = EPOLLIN | EPOLLPRI;
                            epoll_ctl(m_epfd, EPOLL_CTL_ADD, ev.data.fd, &ev);

                            /* it is possible to set the same socket
                             * EEXIST error appears in this case but it harmless condition
                             */
                            errno = 0;

                            m_max_events++;
                        }
                        active_fd_count--;
                    }
                    i++;

                    assert((i < MAX_ACTIVE_FD_NUM) &&
                           "maximum number of active connection to the single TCP addr:port");
                    assert(m_max_events < MAX_FDS_NUM);
                }
            }
        }
        /* It can be omitted */
        m_look_end = m_max_events;
    }

    //------------------------------------------------------------------------------
    inline int waitArrival() {
        m_look_end = epoll_wait(m_epfd, mp_epoll_events, m_max_events, m_timeout_msec);
        return m_look_end;
    }
    //------------------------------------------------------------------------------
    inline int analyzeArrival(int ifd) const {
        assert((ifd < MAX_FDS_NUM) && "exceeded tool limitation (MAX_FDS_NUM)");

        return mp_epoll_events[ifd].data.fd;
    }

    virtual int prepareNetwork();

private:
    const int m_timeout_msec;
    struct epoll_event *mp_epoll_events;
    int m_epfd;
    int m_max_events;
};
#endif

#ifdef USING_VMA_EXTRA_API
//==============================================================================
class IoSocketxtreme : public IoHandler {
public:
    IoSocketxtreme(int _fd_min, int _fd_max, int _fd_num);
    virtual ~IoSocketxtreme();

    //------------------------------------------------------------------------------
    inline void update() {
        int ifd = 0;

        m_look_start = 0;
        m_look_end = m_fd_num;
        for (ifd = m_fd_min; ifd <= m_fd_max; ifd++) {
            if (g_fds_array[ifd]) {
                int i = 0;
                int active_fd_count = g_fds_array[ifd]->active_fd_count;
                int *active_fd_list = g_fds_array[ifd]->active_fd_list;

                assert(active_fd_list && "corrupted fds_data object");

                while (active_fd_count) {
                    /* process active sockets in case TCP (listen sockets are set in
                     * prepareNetwork()) and
                     * skip active socket in case UDP (it is the same with set in prepareNetwork())
                     */
                    if (active_fd_list[i] != (int)INVALID_SOCKET) {
                        active_fd_count--;
                    }
                    i++;

                    assert((i < MAX_ACTIVE_FD_NUM) &&
                           "maximum number of active connection to the single TCP addr:port");
                }
            }
        }
    }
    //------------------------------------------------------------------------------
    inline int waitArrival() {
        m_look_end = 0;
        for (m_rings_vma_comps_map_itr = m_rings_vma_comps_map.begin();
             m_rings_vma_comps_map_itr != m_rings_vma_comps_map.end();
             ++m_rings_vma_comps_map_itr) {
            int ring_fd = m_rings_vma_comps_map_itr->first;
            if (!m_rings_vma_comps_map_itr->second->is_freed) {
                for (int i = 0; i < m_rings_vma_comps_map_itr->second->vma_comp_list_size; i++) {
                    if (m_rings_vma_comps_map_itr->second->vma_comp_list[i].events &
                        VMA_SOCKETXTREME_PACKET) {
                        g_vma_api->socketxtreme_free_vma_packets(
                            &m_rings_vma_comps_map_itr->second->vma_comp_list[i].packet, 1);
                    }
                }
                memset(m_rings_vma_comps_map_itr->second->vma_comp_list, 0,
                       m_rings_vma_comps_map_itr->second->vma_comp_list_size *
                           sizeof(vma_completion_t));
                m_rings_vma_comps_map_itr->second->is_freed = true;
                m_rings_vma_comps_map_itr->second->vma_comp_list_size = 0;
            }
            m_rings_vma_comps_map_itr->second->vma_comp_list_size = g_vma_api->socketxtreme_poll(
                ring_fd, (vma_completion_t *)(&m_rings_vma_comps_map_itr->second->vma_comp_list),
                MAX_VMA_COMPS, 0);

            if (m_rings_vma_comps_map_itr->second->vma_comp_list_size > 0) {
                m_vma_comps_queue.push(ring_fd);
                m_rings_vma_comps_map_itr->second->is_freed = false;
                m_look_end += m_rings_vma_comps_map_itr->second->vma_comp_list_size;
            }
        }
        return m_look_end;
    }
    //------------------------------------------------------------------------------
    inline int analyzeArrival(int ifd) {
        assert((ifd < MAX_FDS_NUM) && "exceeded tool limitation (MAX_FDS_NUM)");
        int ring_fd = 0;
        g_vma_buff = NULL;
        if (!m_current_vma_ring_comp) {
            ring_fd = m_vma_comps_queue.front();
            m_vma_comps_queue.pop();
            m_rings_vma_comps_map_itr = m_rings_vma_comps_map.find(ring_fd);
            if (m_rings_vma_comps_map_itr != m_rings_vma_comps_map.end()) {
                m_current_vma_ring_comp = m_rings_vma_comps_map_itr->second;
                m_vma_comp_index = 0;
            }
        }

        g_vma_comps = (vma_completion_t *)&m_current_vma_ring_comp->vma_comp_list[m_vma_comp_index];
        if (g_vma_comps->events & VMA_SOCKETXTREME_NEW_CONNECTION_ACCEPTED) {
            ifd = g_vma_comps->listen_fd;
        } else if (g_vma_comps->events & VMA_SOCKETXTREME_PACKET) {
            g_vma_buff = g_vma_comps->packet.buff_lst;
            ifd = g_vma_comps->user_data;
        } else if (g_vma_comps->events & (EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
            ifd = g_vma_comps->user_data;
        } else {
            ifd = 0;
        }

        m_vma_comp_index++;
        if (m_vma_comp_index == m_current_vma_ring_comp->vma_comp_list_size) {
            m_vma_comp_index = 0;
            m_current_vma_ring_comp = NULL;
        }
        return ifd;
    }

    virtual int prepareNetwork();

private:
    int m_vma_comp_index;
    vma_ring_comps *m_current_vma_ring_comp;
    vma_comps_queue m_vma_comps_queue;
    rings_vma_comps_map m_rings_vma_comps_map;
    rings_vma_comps_map::iterator m_rings_vma_comps_map_itr;
};
#endif
#endif
#endif /* IOHANDLERS_H_ */
