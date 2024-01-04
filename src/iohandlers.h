/*
 * Copyright (c) 2011-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include "defs.h"
#include "common.h"

void print_addresses(const fds_data *data, int &list_count);

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

#ifndef __windows__
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
        assert((ifd < max_fds_num) && "exceeded tool limitation (max_fds_num)");

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

#if !defined(__FreeBSD__) && !defined(__APPLE__) 
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
                    assert(m_max_events < max_fds_num);
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
        assert((ifd < max_fds_num) && "exceeded tool limitation (max_fds_num)");

        return mp_epoll_events[ifd].data.fd;
    }

    virtual int prepareNetwork();

private:
    const int m_timeout_msec;
    struct epoll_event *mp_epoll_events;
    int m_epfd;
    int m_max_events;
};
#endif // !defined(__FreeBSD__) && !defined(__APPLE__) 
#if defined(__FreeBSD__) || defined(__APPLE__) 
//==============================================================================
class IoKqueue : public IoHandler {
public:
    IoKqueue(int _fd_min, int _fd_max, int _fd_num);
    virtual ~IoKqueue();

    //------------------------------------------------------------------------------
    inline void update() {
        int ifd = 0;
        int change_num = 0;
        int rc = 0;

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
                            EV_SET(&mp_kqueue_changes[change_num], active_fd_list[i], EVFILT_READ, EV_FLAGS, 0, 0, 0);
                            change_num++;
                            m_max_events++;
                        }
                        active_fd_count--;
                    }
                    i++;

                    assert((i < MAX_ACTIVE_FD_NUM) &&
                           "maximum number of active connection to the single TCP addr:port");
                    assert(m_max_events < max_fds_num);
                }
            }
        }
        rc = kevent(m_kqfd, mp_kqueue_changes, change_num, NULL, 0, NULL);
        assert(rc != -1);
        /* It can be omitted */
        m_look_end = m_max_events;
    }

    //------------------------------------------------------------------------------
    inline int waitArrival() {
        m_look_end = kevent(m_kqfd, NULL, 0, mp_kqueue_events, m_max_events, mp_timeout);
        return m_look_end;
    }
    //------------------------------------------------------------------------------
    inline int analyzeArrival(int ifd) const {
        assert((ifd < max_fds_num) && "exceeded tool limitation (max_fds_num)");

        return mp_kqueue_events[ifd].ident;
    }

    virtual int prepareNetwork();

private:
    const struct timespec m_timeout;
    const struct timespec* mp_timeout;
    struct kevent *mp_kqueue_events;
    struct kevent *mp_kqueue_changes;
    int m_kqfd;
    int m_max_events;
};
#endif // defined(__FreeBSD__) || defined(__APPLE__) 
#ifdef USING_EXTRA_API // socketxtreme-extra-api Only
//==============================================================================
// T is vma_buff_t | xlio_buff_t
// C is vma_completion_t | xlio_socketxtreme_completion_t
template <class T, class C, typename API>
class IoSocketxtreme : public IoHandler {
public:
    typedef T buff_type;

#ifdef USING_VMA_EXTRA_API // VMA socketxtreme-extra-api Only
    template <class K = T, typename std::enable_if_t<std::is_same<vma_buff_t,K>::value, bool> = true>
    IoSocketxtreme(int _fd_min, int _fd_max, int _fd_num)
        : IoHandler(_fd_min, _fd_max, _fd_num, 0, 0)
        , m_extra_api(g_vma_api), m_flag_sx_packet(VMA_SOCKETXTREME_PACKET)
        , m_flag_sx_new_conn_accepted(VMA_SOCKETXTREME_NEW_CONNECTION_ACCEPTED)
        , m_current_ring_comp(nullptr)
    {
    }
#endif // USING_VMA_EXTRA_API

#ifdef USING_XLIO_EXTRA_API // XLIO socketxtreme-extra-api Only
    template <class K = T, typename std::enable_if_t<std::is_same<xlio_buff_t,K>::value, bool> = true>
    IoSocketxtreme(int _fd_min, int _fd_max, int _fd_num)
        : IoHandler(_fd_min, _fd_max, _fd_num, 0, 0)
        , m_extra_api(g_xlio_api), m_flag_sx_packet(XLIO_SOCKETXTREME_PACKET)
        , m_flag_sx_new_conn_accepted(XLIO_SOCKETXTREME_NEW_CONNECTION_ACCEPTED)
        , m_current_ring_comp(nullptr)
    {
    }
#endif // USING_XLIO_EXTRA_API

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

#ifdef USING_VMA_EXTRA_API // VMA
    template <typename K = T>
    inline std::enable_if_t<std::is_same<K,vma_buff_t>::value, void>
    sx_free_packets(int i) {
        m_extra_api->socketxtreme_free_vma_packets(
            &m_rings_comps_map_itr->second->comp_list[i].packet, 1);
    }
#endif

#ifdef USING_XLIO_EXTRA_API // XLIO
    template <typename K = T>
    inline std::enable_if_t<std::is_same<K,xlio_buff_t>::value, void>
    sx_free_packets(int i) {
        m_extra_api->socketxtreme_free_packets(
            &m_rings_comps_map_itr->second->comp_list[i].packet, 1);
    }
#endif

    //------------------------------------------------------------------------------
    inline int waitArrival() {
        m_look_end = 0;
        for (m_rings_comps_map_itr = m_rings_comps_map.begin();
             m_rings_comps_map_itr != m_rings_comps_map.end();
             ++m_rings_comps_map_itr) {
            int ring_fd = m_rings_comps_map_itr->first;
            if (!m_rings_comps_map_itr->second->is_freed) {
                for (int i = 0; i < m_rings_comps_map_itr->second->comp_list_size; i++) {
                    if (m_rings_comps_map_itr->second->comp_list[i].events & m_flag_sx_packet) {
                        sx_free_packets(i);
                    }
                }
                memset(m_rings_comps_map_itr->second->comp_list, 0,
                       m_rings_comps_map_itr->second->comp_list_size *
                           sizeof(C));
                m_rings_comps_map_itr->second->is_freed = true;
                m_rings_comps_map_itr->second->comp_list_size = 0;
            }
            m_rings_comps_map_itr->second->comp_list_size = m_extra_api->socketxtreme_poll(
                ring_fd, (C *)(&m_rings_comps_map_itr->second->comp_list),
                MAX_SOCKETXTREME_COMPS, 0);

            if (m_rings_comps_map_itr->second->comp_list_size > 0) {
                m_comps_queue.push(ring_fd);
                m_rings_comps_map_itr->second->is_freed = false;
                m_look_end += m_rings_comps_map_itr->second->comp_list_size;
            }
        }
        return m_look_end;
    }
    //------------------------------------------------------------------------------
    inline int analyzeArrival(int ifd) {
        assert((ifd < max_fds_num) && "exceeded tool limitation (max_fds_num)");
        int ring_fd = 0;
        m_sx_curr_buff = NULL;
        if (!m_current_ring_comp) {
            ring_fd = m_comps_queue.front();
            m_comps_queue.pop();
            m_rings_comps_map_itr = m_rings_comps_map.find(ring_fd);
            if (m_rings_comps_map_itr != m_rings_comps_map.end()) {
                m_current_ring_comp = m_rings_comps_map_itr->second;
                m_comp_index = 0;
            }
        }

        m_sx_curr_comp = (C *)&m_current_ring_comp->comp_list[m_comp_index];
        if (m_sx_curr_comp->events & m_flag_sx_new_conn_accepted) {
            ifd = m_sx_curr_comp->listen_fd;
        } else if (m_sx_curr_comp->events & m_flag_sx_packet) {
            m_sx_curr_buff = m_sx_curr_comp->packet.buff_lst;
            ifd = m_sx_curr_comp->user_data;
        } else if (m_sx_curr_comp->events & (EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
            ifd = m_sx_curr_comp->user_data;
        } else {
            ifd = 0;
        }

        m_comp_index++;
        if (m_comp_index == m_current_ring_comp->comp_list_size) {
            m_comp_index = 0;
            m_current_ring_comp = NULL;
        }
        return ifd;
    }

    virtual int prepareNetwork();

    C *get_last_comp() { return m_sx_curr_comp; }
    T *get_last_buff() { return m_sx_curr_buff; }

private:
    API m_extra_api;
    C *m_sx_curr_comp;
    T *m_sx_curr_buff;
    uint64_t m_flag_sx_packet;
    uint64_t m_flag_sx_new_conn_accepted;

    int m_comp_index;
    socketxtreme_ring_comps<C> *m_current_ring_comp;
    socketxtreme_comps_queue m_comps_queue;
    socketxtreme_rings_comps_map<C> m_rings_comps_map;
    typename socketxtreme_rings_comps_map<C>::iterator m_rings_comps_map_itr;
};

template <class T, class C, typename API>
IoSocketxtreme<T,C,API>::~IoSocketxtreme() {
    for (m_rings_comps_map_itr = m_rings_comps_map.begin();
         m_rings_comps_map_itr != m_rings_comps_map.end(); ++m_rings_comps_map_itr) {
        FREE(m_rings_comps_map_itr->second);
    }
}

template <class T, class C, typename API>
int IoSocketxtreme<T,C,API>::prepareNetwork() {
    int rc = SOCKPERF_ERR_NONE;
    int list_count = 0;
    int ring_fd = 0;

    printf("\n");
    for (int ifd = m_fd_min; ifd <= m_fd_max; ifd++) {
        if (g_fds_array[ifd]) {
            ring_fd = 0;
            int rings = -1;
            if (g_vma_api) {
#ifdef USING_VMA_EXTRA_API // VMA Socketxtreme Only
                rings = g_vma_api->get_socket_rings_fds(ifd, &ring_fd, 1);
#endif // USING_VMA_EXTRA_API
            } else {
#ifdef USING_XLIO_EXTRA_API // XLIO Socketxtreme Only
                rings = g_xlio_api->get_socket_rings_fds(ifd, &ring_fd, 1);
#endif // USING_XLIO_EXTRA_API
            }

            if (rings == -1) {
                rc = SOCKPERF_ERR_SOCKET;
                return rc;
            }
            typename socketxtreme_rings_comps_map<C>::iterator itr = m_rings_comps_map.find(ring_fd);
            if (itr == m_rings_comps_map.end()) {
                socketxtreme_ring_comps<C> *temp = NULL;
                temp = (struct socketxtreme_ring_comps<C> *)MALLOC(sizeof(socketxtreme_ring_comps<C>));
                if (!temp) {
                    log_err("Failed to allocate memory");
                    rc = SOCKPERF_ERR_NO_MEMORY;
                }
                memset(temp, 0, sizeof(socketxtreme_ring_comps<C>));
                temp->is_freed = true;
                temp->comp_list_size = 0;

                std::pair<typename socketxtreme_rings_comps_map<C>::iterator, bool> ret =
                    m_rings_comps_map.insert(std::make_pair(ring_fd, temp));
                if (!ret.second) {
                    log_err("Failed to insert new ring.");
                    rc = SOCKPERF_ERR_NO_MEMORY;
                }
            }

            print_addresses(g_fds_array[ifd], list_count);
        }
    }
    return rc;
}

#ifdef USING_VMA_EXTRA_API // VMA Socketxtreme Only
typedef IoSocketxtreme<vma_buff_t, vma_completion_t, decltype(g_vma_api)> IoSocketxtremeVMA;
#endif // USING_VMA_EXTRA_API

#ifdef USING_XLIO_EXTRA_API // XLIO Socketxtreme Only
typedef IoSocketxtreme<xlio_buff_t, xlio_socketxtreme_completion_t, decltype(g_xlio_api)> IoSocketxtremeXLIO;
#endif // USING_XLIO_EXTRA_API

#endif // USING_EXTRA_API
#endif // !WIN32
#endif // IOHANDLERS_H_
