/*
 * Copyright (c) 2011-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef COMMON_H_
#define COMMON_H_

#include <string>

#include "defs.h"
#include "message.h"
#include "tls.h"

extern user_params_t s_user_params;
//------------------------------------------------------------------------------
void recvfromError(int fd); // take out error code from inline function
std::string sockaddr_to_hostport(const struct sockaddr *addr);
inline std::string sockaddr_to_hostport(const struct sockaddr_store_t *addr) {
    return sockaddr_to_hostport(reinterpret_cast<const sockaddr *>(addr));
}
inline std::string sockaddr_to_hostport(const struct sockaddr_in &addr) {
    return sockaddr_to_hostport(reinterpret_cast<const sockaddr *>(&addr));
}
inline std::string sockaddr_to_hostport(const struct sockaddr_store_t &addr) {
    return sockaddr_to_hostport(reinterpret_cast<const sockaddr *>(&addr));
}
bool is_multicast_addr(const sockaddr_store_t &addr);
void sendtoError(int fd, int nbytes,
                 const struct sockaddr *sendto_addr); // take out error code from inline function
void exit_with_log(int status);
void exit_with_log(const char *error, int status);
void exit_with_log(const char *error, int status, const fds_data *fds);
void exit_with_err(const char *error, int status);
void print_log_dbg(struct sockaddr *addr, int ifd);

int set_affinity_list(os_thread_t thread, const char *cpu_list);
void hexdump(void *ptr, int buflen);
const char *handler2str(fd_block_handler_t type);
int read_int_from_sys_file(const char *path);

// inline functions
//------------------------------------------------------------------------------
static inline int msg_sendto(int fd, uint8_t *buf, int nbytes,
                     const struct sockaddr *sendto_addr, socklen_t addrlen) {
    int ret = 0;
    int flags = 0;

#if defined(LOG_TRACE_MSG_OUT) && (LOG_TRACE_MSG_OUT == TRUE)
    printf("<<< ");
    hexdump(buf, MsgHeader::EFFECTIVE_SIZE);
#endif /* LOG_TRACE_SEND */

/*
 * MSG_NOSIGNAL:
 * When writing onto a connection-oriented socket that has been shut down
 * (by the local or the remote end) SIGPIPE is sent to the writing process
 * and EPIPE is returned. The signal is not sent when the write call specified
 * the MSG_NOSIGNAL flag.
 * Note: another way is call signal (SIGPIPE,SIG_IGN);
 */
#ifndef __windows__
    flags = MSG_NOSIGNAL;

    /*
     * MSG_DONTWAIT:
     * Enables non-blocking operation; if the operation would block,
     * EAGAIN is returned (this can also be enabled using the O_NONBLOCK with
     * the F_SETFL fcntl()).
     */
    if (g_pApp->m_const_params.is_nonblocked_send) {
        flags |= MSG_DONTWAIT;
    }
#endif // __windows__

    int size = nbytes;
    if (g_fds_array[fd]->sock_type == SOCK_STREAM) {
        /* If sendto() is used on a connection-mode (SOCK_STREAM, SOCK_SEQPACKET) socket,
         * the arguments dest_addr and addrlen are ignored
         * (and the error EISCONN may be returned when they are not NULL and 0)
         */
        sendto_addr = NULL;
        addrlen = 0;
    }

    while (nbytes) {
#if defined(DEFINED_TLS)
        if (g_fds_array[fd]->tls_handle) {
            ret = tls_write(g_fds_array[fd]->tls_handle, buf, nbytes);
        } else
#endif /* DEFINED_TLS */
        {
            ret = sendto(fd, buf, nbytes, flags, sendto_addr, addrlen);
        }

#if defined(LOG_TRACE_SEND) && (LOG_TRACE_SEND == TRUE)
        std::string hostport = sockaddr_to_hostport(sendto_addr);
        LOG_TRACE("raw", "%s IP: %s [fd=%d ret=%d] %s", __FUNCTION__,
                  hostport.c_str(), fd, ret,
                  strerror(errno));
#endif /* LOG_TRACE_SEND */

        if (likely(ret > 0)) {
            nbytes -= ret;
            buf += ret;
            ret = size;
        } else if (ret == 0 || errno == EPIPE || os_err_conn_reset()) {
            /* If no messages are available to be received and the peer has performed an orderly
             * shutdown,
             * send()/sendto() shall return (RET_SOCKET_SHUTDOWN)
             */
            errno = 0;
            ret = RET_SOCKET_SHUTDOWN;
            break;
        } else if (ret < 0 && (os_err_eagain() || errno == EWOULDBLOCK)) {
            /* If space is not available at the sending socket to hold the message to be transmitted
             * and
             * the socket file descriptor does have O_NONBLOCK set and
             * no bytes related message sent before
             * send()/sendto() shall return (RET_SOCKET_SKIPPED)
             */
            errno = 0;
            if (nbytes < size) continue;

            ret = RET_SOCKET_SKIPPED;
            break;
        } else if (ret < 0 && (errno == EINTR)) {
            /* A signal occurred.
             */
            errno = 0;
            break;
        } else {
            /* Unprocessed error
             */
            sendtoError(fd, nbytes, sendto_addr);
            errno = 0;
            break;
        }
    }

    return ret;
}

int sock_set_rate_limit(int fd, uint32_t rate_limit);

/** @brief extract port in network byte order from socket address.
 */
static inline uint16_t sockaddr_get_portn(const sockaddr_store_t &addr)
{
    switch (addr.ss_family) {
    case AF_INET:
        return reinterpret_cast<const sockaddr_in &>(addr).sin_port;
    case AF_INET6:
        return reinterpret_cast<const sockaddr_in6 &>(addr).sin6_port;
    }
    return 0;
}
/** @brief set port in network byte order in socket address.
 */
static inline void sockaddr_set_portn(sockaddr_store_t &addr, uint16_t port)
{
    switch (addr.ss_family) {
    case AF_INET:
        reinterpret_cast<sockaddr_in &>(addr).sin_port = port;
    case AF_INET6:
        reinterpret_cast<sockaddr_in6 &>(addr).sin6_port = port;
    }
}

static inline void copy_relevant_sockaddr_params(sockaddr_store_t &dst_addr, const sockaddr_store_t &src_addr)
{
    switch (src_addr.ss_family) {
    case AF_INET:
        dst_addr.addr4 = src_addr.addr4;
    case AF_INET6:
        dst_addr.addr6 = src_addr.addr6;
    case AF_UNIX:
        dst_addr.addr_un = src_addr.addr_un;
    }
}

static inline std::string unix_sockaddr_to_string(const sockaddr_un *sa)
{
    std::string str(sa->sun_path, sizeof(sa->sun_path));
    size_t len = str.find('\0');
    if (len != std::string::npos)
        str.resize(len);
    return str;
}

/**
 * @brief
 * @return build client socket address for UNIX sockets.
 */
static inline std::string build_client_socket_name(const sockaddr_un *sa, int pid, int ifd)
{
    return unix_sockaddr_to_string(sa) + "_" + std::to_string(pid) + "_" + std::to_string(ifd);
}

#endif /* COMMON_H_ */
