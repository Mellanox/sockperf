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

#include "iohandlers.h"

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
IoHandler::IoHandler(int _fd_min, int _fd_max, int _fd_num, int _look_start, int _look_end)
    : m_fd_min(_fd_min), m_fd_max(_fd_max), m_fd_num(_fd_num), m_look_start(_look_start),
      m_look_end(_look_end) {}

//------------------------------------------------------------------------------
IoHandler::~IoHandler() {}

//------------------------------------------------------------------------------
void IoHandler::warmup(Message *pMsgRequest) const {
    if (!g_pApp->m_const_params.do_warmup) return;
    pMsgRequest->setWarmupMessage();

    log_msg("Warmup stage (sending a few dummy messages)...");
    for (int ifd = m_fd_min; ifd <= m_fd_max; ifd++) {
        if (g_fds_array[ifd] && g_fds_array[ifd]->is_multicast) {
            for (int count = 0; count < 2; count++) {
                int length = pMsgRequest->getLength();
                pMsgRequest->setHeaderToNetwork();
                msg_sendto(ifd, pMsgRequest->getBuf(), length, &(g_fds_array[ifd]->server_addr));
                pMsgRequest->setHeaderToHost();
            }
        }
    }
    pMsgRequest->resetWarmupMessage();
}

//==============================================================================
//------------------------------------------------------------------------------
IoRecvfrom::IoRecvfrom(int _fd_min, int _fd_max, int _fd_num)
    : IoHandler(_fd_min, _fd_max, _fd_num, _fd_min, _fd_min + 1) {}

//------------------------------------------------------------------------------
IoRecvfrom::~IoRecvfrom() {}

//------------------------------------------------------------------------------
int IoRecvfrom::prepareNetwork() {
    int rc = SOCKPERF_ERR_NONE;
    int list_count = 0;

    printf("\n");
    for (int ifd = m_fd_min; ifd <= m_fd_max; ifd++) {
        if (g_fds_array[ifd]) {
            printf("[%2d] IP = %-15s PORT = %5d # %s\n", list_count++,
                   inet_ntoa(g_fds_array[ifd]->server_addr.sin_addr),
                   ntohs(g_fds_array[ifd]->server_addr.sin_port),
                   PRINT_PROTOCOL(g_fds_array[ifd]->sock_type));
            for (int i = 0; i < g_fds_array[ifd]->memberships_size; i++) {
                printf("[%2d] IP = %-15s PORT = %5d # %s\n", list_count++,
                       inet_ntoa(g_fds_array[ifd]->memberships_addr[i].sin_addr),
                       ntohs(g_fds_array[ifd]->server_addr.sin_port),
                       PRINT_PROTOCOL(g_fds_array[ifd]->sock_type));
            }
        }
    }

    return rc;
}

//==============================================================================
//------------------------------------------------------------------------------
IoRecvfromMUX::IoRecvfromMUX(int _fd_min, int _fd_max, int _fd_num)
    : IoHandler(_fd_min, _fd_max, _fd_num, _fd_num == 1 ? _fd_min : _fd_min - 1,
                _fd_num == 1 ? _fd_num + 1 : _fd_min),
      m_fd_min_all(_fd_min), m_fd_max_all(_fd_max) {}

//------------------------------------------------------------------------------
IoRecvfromMUX::~IoRecvfromMUX() {}

//------------------------------------------------------------------------------
int IoRecvfromMUX::prepareNetwork() {
    int rc = SOCKPERF_ERR_NONE;
    int list_count = 0;

    printf("\n");
    for (int ifd = m_fd_min; ifd <= m_fd_max; ifd++) {
        if (g_fds_array[ifd]) {
            printf("[%2d] IP = %-15s PORT = %5d # %s\n", list_count++,
                   inet_ntoa(g_fds_array[ifd]->server_addr.sin_addr),
                   ntohs(g_fds_array[ifd]->server_addr.sin_port),
                   PRINT_PROTOCOL(g_fds_array[ifd]->sock_type));
            for (int i = 0; i < g_fds_array[ifd]->memberships_size; i++) {
                printf("[%2d] IP = %-15s PORT = %5d # %s\n", list_count++,
                       inet_ntoa(g_fds_array[ifd]->memberships_addr[i].sin_addr),
                       ntohs(g_fds_array[ifd]->server_addr.sin_port),
                       PRINT_PROTOCOL(g_fds_array[ifd]->sock_type));
            }
        }
    }

    return rc;
}

//==============================================================================
//------------------------------------------------------------------------------
IoSelect::IoSelect(int _fd_min, int _fd_max, int _fd_num)
    : IoHandler(_fd_min, _fd_max, _fd_num, _fd_min, _fd_max + 1),
      mp_timeout_timeval(g_pApp->m_const_params.select_timeout ? &m_timeout_timeval : NULL) {}

//------------------------------------------------------------------------------
IoSelect::~IoSelect() {}

//------------------------------------------------------------------------------
int IoSelect::prepareNetwork() {
    int rc = SOCKPERF_ERR_NONE;
    int list_count = 0;
    FD_ZERO(&m_save_fds);

    printf("\n");
    for (int ifd = m_fd_min; ifd <= m_fd_max; ifd++) {
        if (g_fds_array[ifd]) {
            printf("[%2d] IP = %-15s PORT = %5d # %s\n", list_count++,
                   inet_ntoa(g_fds_array[ifd]->server_addr.sin_addr),
                   ntohs(g_fds_array[ifd]->server_addr.sin_port),
                   PRINT_PROTOCOL(g_fds_array[ifd]->sock_type));
            FD_SET(ifd, &m_save_fds);
            for (int i = 0; i < g_fds_array[ifd]->memberships_size; i++) {
                printf("[%2d] IP = %-15s PORT = %5d # %s\n", list_count++,
                       inet_ntoa(g_fds_array[ifd]->memberships_addr[i].sin_addr),
                       ntohs(g_fds_array[ifd]->server_addr.sin_port),
                       PRINT_PROTOCOL(g_fds_array[ifd]->sock_type));
            }
        }
    }

    return rc;
}
#ifndef WIN32
//==============================================================================
//------------------------------------------------------------------------------
IoPoll::IoPoll(int _fd_min, int _fd_max, int _fd_num)
    : IoHandler(_fd_min, _fd_max, _fd_num, 0, _fd_num),
      m_timeout_msec(g_pApp->m_const_params.select_timeout
                         ? g_pApp->m_const_params.select_timeout->tv_sec * 1000 +
                               g_pApp->m_const_params.select_timeout->tv_usec / 1000
                         : -1) {
    mp_poll_fd_arr = NULL;
}

//------------------------------------------------------------------------------
IoPoll::~IoPoll() {
    if (mp_poll_fd_arr) {
        FREE(mp_poll_fd_arr);
    }
}

//------------------------------------------------------------------------------
int IoPoll::prepareNetwork() {
    int rc = SOCKPERF_ERR_NONE;

    mp_poll_fd_arr = (struct pollfd *)MALLOC(MAX_FDS_NUM * sizeof(struct pollfd));
    if (!mp_poll_fd_arr) {
        log_err("Failed to allocate memory for poll fd array");
        rc = SOCKPERF_ERR_NO_MEMORY;
    } else {
        printf("\n");
        int fd_count = 0;
        int list_count = 0;
        for (int ifd = m_fd_min; ifd <= m_fd_max; ifd++) {
            if (g_fds_array[ifd]) {
                printf("[%2d] IP = %-15s PORT = %5d # %s\n", list_count++,
                       inet_ntoa(g_fds_array[ifd]->server_addr.sin_addr),
                       ntohs(g_fds_array[ifd]->server_addr.sin_port),
                       PRINT_PROTOCOL(g_fds_array[ifd]->sock_type));
                for (int i = 0; i < g_fds_array[ifd]->memberships_size; i++) {
                    printf("[%2d] IP = %-15s PORT = %5d # %s\n", list_count++,
                           inet_ntoa(g_fds_array[ifd]->memberships_addr[i].sin_addr),
                           ntohs(g_fds_array[ifd]->server_addr.sin_port),
                           PRINT_PROTOCOL(g_fds_array[ifd]->sock_type));
                }
                mp_poll_fd_arr[fd_count].fd = ifd;
                mp_poll_fd_arr[fd_count].events = POLLIN | POLLPRI;
                fd_count++;
            }
        }
    }

    return rc;
}
#ifndef __FreeBSD__
//==============================================================================
//------------------------------------------------------------------------------
IoEpoll::IoEpoll(int _fd_min, int _fd_max, int _fd_num)
    : IoHandler(_fd_min, _fd_max, _fd_num, 0, 0),
      m_timeout_msec(g_pApp->m_const_params.select_timeout
                         ? g_pApp->m_const_params.select_timeout->tv_sec * 1000 +
                               g_pApp->m_const_params.select_timeout->tv_usec / 1000
                         : -1) {
    mp_epoll_events = NULL;
    m_max_events = 0;
}

//------------------------------------------------------------------------------
IoEpoll::~IoEpoll() {
    if (mp_epoll_events) {
        close(m_epfd);
        FREE(mp_epoll_events);
    }
}

//------------------------------------------------------------------------------
int IoEpoll::prepareNetwork() {
    int rc = SOCKPERF_ERR_NONE;
    struct epoll_event ev = { 0, { 0 } };

    int list_count = 0;
    mp_epoll_events = (struct epoll_event *)MALLOC(MAX_FDS_NUM * sizeof(struct epoll_event));
    if (!mp_epoll_events) {
        log_err("Failed to allocate memory for epoll event array");
        rc = SOCKPERF_ERR_NO_MEMORY;
    } else {
        printf("\n");
        m_max_events = 0;
        m_epfd = epoll_create(MAX_FDS_NUM);
        for (int ifd = m_fd_min; ifd <= m_fd_max; ifd++) {
            if (g_fds_array[ifd]) {
                printf("[%2d] IP = %-15s PORT = %5d # %s\n", list_count++,
                       inet_ntoa(g_fds_array[ifd]->server_addr.sin_addr),
                       ntohs(g_fds_array[ifd]->server_addr.sin_port),
                       PRINT_PROTOCOL(g_fds_array[ifd]->sock_type));
                for (int i = 0; i < g_fds_array[ifd]->memberships_size; i++) {
                    printf("[%2d] IP = %-15s PORT = %5d # %s\n", list_count++,
                           inet_ntoa(g_fds_array[ifd]->memberships_addr[i].sin_addr),
                           ntohs(g_fds_array[ifd]->server_addr.sin_port),
                           PRINT_PROTOCOL(g_fds_array[ifd]->sock_type));
                }
                ev.events = EPOLLIN | EPOLLPRI;
                ev.data.fd = ifd;
                epoll_ctl(m_epfd, EPOLL_CTL_ADD, ev.data.fd, &ev);
                m_max_events++;
            }
        }
    }

    return rc;
}
#endif
#ifdef USING_VMA_EXTRA_API
//==============================================================================
//------------------------------------------------------------------------------
IoSocketxtreme::IoSocketxtreme(int _fd_min, int _fd_max, int _fd_num)
    : IoHandler(_fd_min, _fd_max, _fd_num, 0, 0) {
    m_current_vma_ring_comp = NULL;
}

//------------------------------------------------------------------------------
IoSocketxtreme::~IoSocketxtreme() {
    for (m_rings_vma_comps_map_itr = m_rings_vma_comps_map.begin();
         m_rings_vma_comps_map_itr != m_rings_vma_comps_map.end(); ++m_rings_vma_comps_map_itr) {
        FREE(m_rings_vma_comps_map_itr->second);
    }
}

//------------------------------------------------------------------------------
int IoSocketxtreme::prepareNetwork() {
    int rc = SOCKPERF_ERR_NONE;
    int list_count = 0;
    int ring_fd = 0;

    printf("\n");
    for (int ifd = m_fd_min; ifd <= m_fd_max; ifd++) {
        if (g_fds_array[ifd]) {
            ring_fd = 0;
            int rings = g_vma_api->get_socket_rings_fds(ifd, &ring_fd, 1);
            if (rings == -1) {
                rc = SOCKPERF_ERR_SOCKET;
                return rc;
            }
            rings_vma_comps_map::iterator itr = m_rings_vma_comps_map.find(ring_fd);
            if (itr == m_rings_vma_comps_map.end()) {
                vma_ring_comps *temp = NULL;
                temp = (struct vma_ring_comps *)MALLOC(sizeof(vma_ring_comps));
                if (!temp) {
                    log_err("Failed to allocate memory");
                    rc = SOCKPERF_ERR_NO_MEMORY;
                }
                memset(temp, 0, sizeof(vma_ring_comps));
                temp->is_freed = true;
                temp->vma_comp_list_size = 0;

                std::pair<rings_vma_comps_map::iterator, bool> ret =
                    m_rings_vma_comps_map.insert(std::make_pair(ring_fd, temp));
                if (!ret.second) {
                    log_err("Failed to insert new ring.");
                    rc = SOCKPERF_ERR_NO_MEMORY;
                }
            }

            printf("[%2d] IP = %-15s PORT = %5d # %s\n", list_count++,
                   inet_ntoa(g_fds_array[ifd]->server_addr.sin_addr),
                   ntohs(g_fds_array[ifd]->server_addr.sin_port),
                   PRINT_PROTOCOL(g_fds_array[ifd]->sock_type));
            for (int i = 0; i < g_fds_array[ifd]->memberships_size; i++) {
                printf("[%2d] IP = %-15s PORT = %5d # %s\n", list_count++,
                       inet_ntoa(g_fds_array[ifd]->memberships_addr[i].sin_addr),
                       ntohs(g_fds_array[ifd]->server_addr.sin_port),
                       PRINT_PROTOCOL(g_fds_array[ifd]->sock_type));
            }
        }
    }
    return rc;
}
#endif
#endif
