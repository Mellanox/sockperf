/*
 * Copyright (c) 2011-2020 Mellanox Technologies Ltd.
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

#include "defs.h"
//#include "switches.h"
#include "message.h"

extern user_params_t s_user_params;
//------------------------------------------------------------------------------
void recvfromError(int fd); // take out error code from inline function
void sendtoError(int fd, int nbytes,
                 const struct sockaddr_in *sendto_addr); // take out error code from inline function
void exit_with_log(int status);
void exit_with_log(const char *error, int status);
void exit_with_log(const char *error, int status, fds_data *fds);
void exit_with_err(const char *error, int status);
void print_log_dbg(struct in_addr sin_addr, in_port_t sin_port, int ifd);

int set_affinity_list(os_thread_t thread, const char *cpu_list);
void hexdump(void *ptr, int buflen);
const char *handler2str(fd_block_handler_t type);
int read_int_from_sys_file(const char *path);

// inline functions
#ifdef USING_VMA_EXTRA_API
//------------------------------------------------------------------------------
static inline int msg_recv_socketxtreme(fds_data *l_fds_ifd, vma_buff_t *tmp_vma_buff,
                                        struct sockaddr_in *recvfrom_addr) {
    *recvfrom_addr = g_vma_comps->src;
    if (l_fds_ifd->recv.cur_offset) {
        l_fds_ifd->recv.cur_addr = l_fds_ifd->recv.buf;
        l_fds_ifd->recv.cur_size = l_fds_ifd->recv.max_size - l_fds_ifd->recv.cur_offset;
        memmove(l_fds_ifd->recv.cur_addr + l_fds_ifd->recv.cur_offset,
                (uint8_t *)tmp_vma_buff->payload, tmp_vma_buff->len);
    } else {
        l_fds_ifd->recv.cur_addr = (uint8_t *)tmp_vma_buff->payload;
        l_fds_ifd->recv.cur_size = l_fds_ifd->recv.max_size;
        l_fds_ifd->recv.cur_offset = 0;
    }
    return tmp_vma_buff->len;
}

//------------------------------------------------------------------------------
static inline int msg_process_next(fds_data *l_fds_ifd, vma_buff_t **tmp_vma_buff, int *nbytes) {
    if (l_fds_ifd->recv.cur_offset) {
        memmove(l_fds_ifd->recv.buf, l_fds_ifd->recv.cur_addr, l_fds_ifd->recv.cur_offset);
        l_fds_ifd->recv.cur_addr = l_fds_ifd->recv.buf;
        l_fds_ifd->recv.cur_size = l_fds_ifd->recv.max_size - l_fds_ifd->recv.cur_offset;
        if ((*tmp_vma_buff)->next) {
            *tmp_vma_buff = (*tmp_vma_buff)->next;
            memmove(l_fds_ifd->recv.cur_addr + l_fds_ifd->recv.cur_offset,
                    (uint8_t *)(*tmp_vma_buff)->payload, (*tmp_vma_buff)->len);
            *nbytes = (*tmp_vma_buff)->len;
        } else {
            return 1;
        }
    } else if (0 == *nbytes && (*tmp_vma_buff)->next) {
        *tmp_vma_buff = (*tmp_vma_buff)->next;
        l_fds_ifd->recv.cur_addr = (uint8_t *)(*tmp_vma_buff)->payload;
        l_fds_ifd->recv.cur_size = l_fds_ifd->recv.max_size;
        l_fds_ifd->recv.cur_offset = 0;
        *nbytes = (*tmp_vma_buff)->len;
    }
    return 0;
}

//------------------------------------------------------------------------------
static inline int free_vma_packets(int fd, int nbytes) {
    int data_to_copy;
    int remain_buffer = 0;
    struct vma_packet_t *pkt;
    ZeroCopyData *z_ptr = g_zeroCopyData[fd];

    if (z_ptr) {
        remain_buffer = nbytes;
        // Receive held data, and free VMA's previously received zero copied packets
        if (z_ptr->m_pkts && z_ptr->m_pkts->n_packet_num > 0) {

            pkt = &z_ptr->m_pkts->pkts[0];

            while (z_ptr->m_pkt_index < pkt->sz_iov) {
                data_to_copy = _min(remain_buffer, (int)(pkt->iov[z_ptr->m_pkt_index].iov_len -
                                                         z_ptr->m_pkt_offset));
                remain_buffer -= data_to_copy;
                z_ptr->m_pkt_offset += data_to_copy;

                // Handled buffer is filled
                if (z_ptr->m_pkt_offset < pkt->iov[z_ptr->m_pkt_index].iov_len) return 0;

                z_ptr->m_pkt_offset = 0;
                z_ptr->m_pkt_index++;
            }

            g_vma_api->free_packets(fd, z_ptr->m_pkts->pkts, z_ptr->m_pkts->n_packet_num);
            z_ptr->m_pkts = NULL;
            z_ptr->m_pkt_index = 0;
            z_ptr->m_pkt_offset = 0;

            // Handled buffer is filled
            if (remain_buffer == 0) return 0;
        }

        return remain_buffer;
    }

    return nbytes;
}
#endif

//------------------------------------------------------------------------------
static inline int msg_recvfrom(int fd, uint8_t *buf, int nbytes, struct sockaddr_in *recvfrom_addr,
                               uint8_t **zcopy_pkt_addr, int remain_buffer) {
    int ret = 0;
    socklen_t size = sizeof(struct sockaddr_in);
    int flags = 0;

#ifdef USING_VMA_EXTRA_API
    if (g_pApp->m_const_params.is_vmazcopyread) {
        int data_to_copy;
        struct vma_packet_t *pkt;
        ZeroCopyData *z_ptr = g_zeroCopyData[fd];

        if (z_ptr) {
            // Receive the next packet with zero copy API
            ret = g_vma_api->recvfrom_zcopy(fd, z_ptr->m_pkt_buf, Message::getMaxSize(), &flags,
                                            (struct sockaddr *)recvfrom_addr, &size);

            if (ret > 0) {
                // Zcopy receive is performed
                if (flags & MSG_VMA_ZCOPY) {
                    z_ptr->m_pkts = (struct vma_packets_t *)z_ptr->m_pkt_buf;
                    if (z_ptr->m_pkts->n_packet_num > 0) {

                        pkt = &z_ptr->m_pkts->pkts[0];

                        // Make receive address point to the beginning of returned recvfrom_zcopy
                        // buffer
                        *zcopy_pkt_addr = (uint8_t *)pkt->iov[z_ptr->m_pkt_index].iov_base;

                        while (z_ptr->m_pkt_index < pkt->sz_iov) {
                            data_to_copy =
                                _min(remain_buffer, (int)pkt->iov[z_ptr->m_pkt_index].iov_len);
                            remain_buffer -= data_to_copy;
                            z_ptr->m_pkt_offset += data_to_copy;

                            // Handled buffer is filled
                            if (z_ptr->m_pkt_offset < pkt->iov[z_ptr->m_pkt_index].iov_len)
                                return nbytes;

                            z_ptr->m_pkt_offset = 0;
                            z_ptr->m_pkt_index++;
                        }
                        ret = nbytes - remain_buffer;
                    } else {
                        ret = (remain_buffer == nbytes) ? -1 : (nbytes - remain_buffer);
                    }
                } else {
                    data_to_copy = _min(remain_buffer, ret);
                    memcpy(buf + (nbytes - remain_buffer), &z_ptr->m_pkt_buf[fd], data_to_copy);
                    ret = nbytes - (remain_buffer - data_to_copy);
                }
            }
            // Non_blocked with held packet.
            else if (ret < 0 && os_err_eagain() && (remain_buffer < nbytes)) {
                return nbytes - remain_buffer;
            }
        }
    } else
#endif
    {
/*
    When writing onto a connection-oriented socket that has been shut down
    (by the local or the remote end) SIGPIPE is sent to the writing process
    and EPIPE is returned. The signal is not sent when the write call specified
    the MSG_NOSIGNAL flag.
    Note: another way is call signal (SIGPIPE,SIG_IGN);
 */
#ifndef WIN32
        flags = MSG_NOSIGNAL;
#endif

        ret = recvfrom(fd, buf, nbytes, flags, (struct sockaddr *)recvfrom_addr, &size);

#if defined(LOG_TRACE_MSG_IN) && (LOG_TRACE_MSG_IN == TRUE)
        printf(">   ");
        hexdump(buf, MsgHeader::EFFECTIVE_SIZE);
#endif /* LOG_TRACE_MSG_IN */

#if defined(LOG_TRACE_RECV) && (LOG_TRACE_RECV == TRUE)
        LOG_TRACE("raw", "%s IP: %s:%d [fd=%d ret=%d] %s", __FUNCTION__,
                  inet_ntoa(recvfrom_addr->sin_addr), ntohs(recvfrom_addr->sin_port), fd, ret,
                  strerror(errno));
#endif /* LOG_TRACE_RECV */
    }

    if (ret == 0 || errno == EPIPE || os_err_conn_reset()) {
        /* If no messages are available to be received and the peer has performed an orderly
         * shutdown,
         * recv()/recvfrom() shall return 0
         * */
        ret = RET_SOCKET_SHUTDOWN;
        errno = 0;
    }
    /* ret < MsgHeader::EFFECTIVE_SIZE
     * ret value less than MsgHeader::EFFECTIVE_SIZE
     * is bad case for UDP so error could be actual but it is possible value for TCP
     */
    else if (ret < 0 && !os_err_eagain() && errno != EINTR) {
        recvfromError(fd);
    }

    return ret;
}

//------------------------------------------------------------------------------
static inline int msg_sendto(int fd, uint8_t *buf, int nbytes,
                             const struct sockaddr_in *sendto_addr) {
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
#ifndef WIN32
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
#endif

    int size = nbytes;

    while (nbytes) {
        ret =
            sendto(fd, buf, nbytes, flags, (struct sockaddr *)sendto_addr, sizeof(struct sockaddr));

#if defined(LOG_TRACE_SEND) && (LOG_TRACE_SEND == TRUE)
        LOG_TRACE("raw", "%s IP: %s:%d [fd=%d ret=%d] %s", __FUNCTION__,
                  inet_ntoa(sendto_addr->sin_addr), ntohs(sendto_addr->sin_port), fd, ret,
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

#endif /* COMMON_H_ */
