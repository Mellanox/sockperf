/*
 * Copyright (c) 2011-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

void print_addresses(const fds_data *data, int &list_count)
{
    {
        char hbuf[NI_MAXHOST] = "(unknown)";
        char pbuf[NI_MAXSERV] = "(unk)";
        getnameinfo(reinterpret_cast<const sockaddr *>(&data->server_addr), data->server_addr_len,
                hbuf, sizeof(hbuf), pbuf, sizeof(pbuf),
                NI_NUMERICHOST | NI_NUMERICSERV);
        switch (data->server_addr.addr.sa_family) {
            case AF_UNIX:
                printf("[%2d] ADDR = %s # %s\n", list_count++, data->server_addr.addr_un.sun_path, PRINT_PROTOCOL(data->sock_type));
                break;
            default:
                printf("[%2d] IP = %-15s PORT = %5s # %s\n", list_count++, hbuf, pbuf, PRINT_PROTOCOL(data->sock_type));
        }
    }
    for (int i = 0; i < data->memberships_size; i++) {
        char hbuf[NI_MAXHOST] = "(unknown)";
        char pbuf[NI_MAXSERV] = "(unk)";
        getnameinfo(reinterpret_cast<const sockaddr *>(&data->memberships_addr[i]), data->server_addr_len,
                hbuf, sizeof(hbuf), pbuf, sizeof(pbuf),
                NI_NUMERICHOST | NI_NUMERICSERV);
        printf("[%2d] IP = %-15s PORT = %5s # %s\n", list_count++,
                hbuf, pbuf,
                PRINT_PROTOCOL(data->sock_type));
    }
}

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
        fds_data *data = g_fds_array[ifd];
        if (data && data->is_multicast) {
            for (int count = 0; count < 2; count++) {
                int length = pMsgRequest->getLength();
                pMsgRequest->setHeaderToNetwork();
                msg_sendto(ifd, pMsgRequest->getBuf(), length, &reinterpret_cast<const sockaddr &>(data->server_addr),
                        data->server_addr_len);
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
            print_addresses(g_fds_array[ifd], list_count);
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
            print_addresses(g_fds_array[ifd], list_count);
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
            print_addresses(g_fds_array[ifd], list_count);
            FD_SET(ifd, &m_save_fds);
        }
    }

    return rc;
}
#ifndef __windows__
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

    mp_poll_fd_arr = (struct pollfd *)MALLOC(max_fds_num * sizeof(struct pollfd));
    if (!mp_poll_fd_arr) {
        log_err("Failed to allocate memory for poll fd array");
        rc = SOCKPERF_ERR_NO_MEMORY;
    } else {
        printf("\n");
        int fd_count = 0;
        int list_count = 0;
        for (int ifd = m_fd_min; ifd <= m_fd_max; ifd++) {
            if (g_fds_array[ifd]) {
                print_addresses(g_fds_array[ifd], list_count);
                mp_poll_fd_arr[fd_count].fd = ifd;
                mp_poll_fd_arr[fd_count].events = POLLIN | POLLPRI;
                fd_count++;
            }
        }
    }

    return rc;
}
#if !defined(__FreeBSD__) && !defined(__APPLE__)
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
    mp_epoll_events = (struct epoll_event *)MALLOC(max_fds_num * sizeof(struct epoll_event));
    if (!mp_epoll_events) {
        log_err("Failed to allocate memory for epoll event array");
        rc = SOCKPERF_ERR_NO_MEMORY;
    } else {
        printf("\n");
        m_max_events = 0;
        m_epfd = epoll_create(max_fds_num);
        for (int ifd = m_fd_min; ifd <= m_fd_max; ifd++) {
            if (g_fds_array[ifd]) {
                print_addresses(g_fds_array[ifd], list_count);
                ev.events = EPOLLIN | EPOLLPRI;
                ev.data.fd = ifd;
                epoll_ctl(m_epfd, EPOLL_CTL_ADD, ev.data.fd, &ev);
                m_max_events++;
            }
        }
    }

    return rc;
}
#endif // !defined(__FreeBSD__) && !defined(__APPLE__)
#if defined(__FreeBSD__) || defined(__APPLE__)
//==============================================================================
//------------------------------------------------------------------------------
IoKqueue::IoKqueue(int _fd_min, int _fd_max, int _fd_num)
    : IoHandler(_fd_min, _fd_max, _fd_num, 0, 0),
      m_timeout({g_pApp->m_const_params.select_timeout ? g_pApp->m_const_params.select_timeout->tv_sec : 0,
                 g_pApp->m_const_params.select_timeout ? g_pApp->m_const_params.select_timeout->tv_usec * 1000 : 0}),
      mp_timeout(g_pApp->m_const_params.select_timeout ? &m_timeout : NULL) {
    mp_kqueue_events = NULL;
    mp_kqueue_changes = NULL;
    m_max_events = 0;
}

//------------------------------------------------------------------------------
IoKqueue::~IoKqueue() {
    if (mp_kqueue_events) {
        close(m_kqfd);
        FREE(mp_kqueue_events);
        if (mp_kqueue_changes) {
            FREE(mp_kqueue_changes);
        }
    }

}

//------------------------------------------------------------------------------
int IoKqueue::prepareNetwork() {
    int rc = SOCKPERF_ERR_NONE;

    int list_count = 0; 
    mp_kqueue_events = (struct kevent *)MALLOC(max_fds_num * sizeof(struct kevent));
    mp_kqueue_changes = (struct kevent *)MALLOC(max_fds_num * sizeof(struct kevent));
    if (!mp_kqueue_events) {
        log_err("Failed to allocate memory for kqueue event array");
        rc = SOCKPERF_ERR_NO_MEMORY;
    } else if (!mp_kqueue_changes) {
        log_err("Failed to allocate memory for kqueue change array");
        rc = SOCKPERF_ERR_NO_MEMORY;
    } else {
        printf("\n");
        m_max_events = 0;
        m_kqfd = kqueue();
        if (m_kqfd == -1){
            log_err("Failed to create kqueue");
            rc = SOCKPERF_ERR_FATAL;
        } else {
            for (int ifd = m_fd_min; ifd <= m_fd_max; ifd++) {
                if (g_fds_array[ifd]) {
                    print_addresses(g_fds_array[ifd], list_count);
                    EV_SET(&mp_kqueue_changes[m_max_events], ifd, EVFILT_READ, EV_FLAGS, 0, 0, 0);
                    m_max_events++;
                }
            }
            int success = kevent(m_kqfd, mp_kqueue_changes, m_max_events, NULL, 0, NULL);
            if (success == -1){
                rc = SOCKPERF_ERR_FATAL;
            }
        } 
    }

    return rc;
}
#endif // defined(__FreeBSD__) || defined(__APPLE__)
#endif // !WIN32
