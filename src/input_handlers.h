/*
 * Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef INPUT_HANDLERS_H_
#define INPUT_HANDLERS_H_

#include "message_parser.h"

class RecvFromInputHandler : public MessageParser<InPlaceAccumulation> {
private:
    SocketRecvData &m_recv_data;
    uint8_t *m_actual_buf;
    int m_actual_buf_size;
public:
    inline RecvFromInputHandler(Message *msg, SocketRecvData &recv_data):
        MessageParser<InPlaceAccumulation>(msg),
        m_recv_data(recv_data),
        m_actual_buf_size(0)
    {}

    /** Receive pending data from a socket
     * @param [in] socket descriptor
     * @param [out] recvfrom_addr address to save peer address into
     * @param [inout] in - storage size, out - actual address size
     * @return status code
     */
    inline int receive_pending_data(int fd, struct sockaddr *recvfrom_addr, socklen_t &size)
    {
        int ret = 0;
        int flags = 0;
        uint8_t *buf = m_recv_data.cur_addr + m_recv_data.cur_offset;

/*
    When writing onto a connection-oriented socket that has been shut down
    (by the local or the remote end) SIGPIPE is sent to the writing process
    and EPIPE is returned. The signal is not sent when the write call specified
    the MSG_NOSIGNAL flag.
    Note: another way is call signal (SIGPIPE,SIG_IGN);
 */
#ifndef __windows__
        flags = MSG_NOSIGNAL;
#endif

#if defined(DEFINED_TLS)
        if (g_fds_array[fd]->tls_handle) {
            ret = tls_read(g_fds_array[fd]->tls_handle, buf, m_recv_data.cur_size);
        } else
#endif /* DEFINED_TLS */
        {
            ret = recvfrom(fd, buf, m_recv_data.cur_size,
                    flags, (struct sockaddr *)recvfrom_addr, &size);
        }
        m_actual_buf = buf;
        m_actual_buf_size = ret;

#if defined(LOG_TRACE_MSG_IN) && (LOG_TRACE_MSG_IN == TRUE)
        printf(">   ");
        hexdump(buf, MsgHeader::EFFECTIVE_SIZE);
#endif /* LOG_TRACE_MSG_IN */

#if defined(LOG_TRACE_RECV) && (LOG_TRACE_RECV == TRUE)
        std::string hostport = sockaddr_to_hostport(recvfrom_addr);
        LOG_TRACE("raw", "%s IP: %s [fd=%d ret=%d] %s", __FUNCTION__,
                  hostport.c_str(), fd, ret,
                  strerror(errno));
#endif /* LOG_TRACE_RECV */

        if (ret == 0 || errno == EPIPE || os_err_conn_reset()) {
            /* If no messages are available to be received and the peer has
             * performed an orderly shutdown, recv()/recvfrom() shall return 0
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

    template <class Callback>
    inline bool iterate_over_buffers(Callback &callback)
    {
        return process_buffer(callback, m_recv_data, m_actual_buf, m_actual_buf_size);
    }

    inline void cleanup()
    {
    }
};

#if defined(USING_VMA_EXTRA_API) || defined(USING_XLIO_EXTRA_API)
typedef int(* recvfrom_zcopy_func_t)(int __fd, void *__buf, size_t __nbytes, int *__flags,
                                     struct sockaddr *__from, socklen_t *__fromlen);

template <typename PacketDataType, typename PacketsDataType, int MsgZCopyFlag>
class ZCopyReadInputHandler : public MessageParser<BufferAccumulation> {
protected:
    SocketRecvData &m_recv_data;
    int m_fd;
    ZeroCopyData *m_ptr;
    int m_non_zcopy_len;
    recvfrom_zcopy_func_t recvfrom_zcopy;

public:
    inline ZCopyReadInputHandler(Message *msg, SocketRecvData &recv_data,
                                 recvfrom_zcopy_func_t recvfrom_zcopy_func):
        MessageParser<BufferAccumulation>(msg),
        m_recv_data(recv_data),
        m_fd(0),
        m_ptr(NULL),
        m_non_zcopy_len(0),
        recvfrom_zcopy(recvfrom_zcopy_func)
    {}

    /** Receive pending data from a socket
     * @param [in] socket descriptor
     * @param [out] recvfrom_addr address to save peer address into
     * @param [inout] in - storage size, out - actual address size
     * @return status code
     */
    inline int receive_pending_data(int fd, struct sockaddr *recvfrom_addr, socklen_t &size)
    {
        int ret = 0;
        int flags = 0;

        m_fd = fd;
        m_non_zcopy_len = 0;
        m_ptr = g_zeroCopyData[fd];
        if (unlikely(!m_ptr)) {
            return 0;
        }
        // Receive the next packet with zero copy API
        ret = recvfrom_zcopy(fd, m_ptr->m_pkt_buf, Message::getMaxSize(), &flags,
                             (struct sockaddr *)recvfrom_addr, &size);

        if (likely(ret > 0)) {
            if (flags & MsgZCopyFlag) {
                // Zcopy receive is performed
                m_ptr->m_pkts = m_ptr->m_pkt_buf;
                m_non_zcopy_len = 0;
                // Note: function return value is not equal to number of bytes
                // received. This is not a problem because caller only checks
                // that retval > 0 to continue processing.
            } else {
                // zero-copy not performed so using buffer as a plain old data buffer
                m_ptr->m_pkts = nullptr;
                m_non_zcopy_len = ret;
            }
        }

        if (ret == 0 || errno == EPIPE || os_err_conn_reset()) {
            /* If no messages are available to be received and the peer has
             * performed an orderly shutdown, recv()/recvfrom() shall return 0
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

    template <class Callback>
    inline bool iterate_over_buffers(Callback &callback)
    {
        assert(m_ptr && "zero-copy data pointer must be initialized");
        if (likely(m_ptr->m_pkts)) {
            // iterate over zerocopy buffers
            PacketsDataType *pkts = reinterpret_cast<PacketsDataType *>(m_ptr->m_pkts);
            uint8_t *cur_ptr = reinterpret_cast<uint8_t *>(&pkts->pkts[0]);
            for (size_t p = 0; p < pkts->n_packet_num; ++p) {
                PacketDataType *pkt = reinterpret_cast<PacketDataType *>(cur_ptr);
                cur_ptr += offsetof(PacketDataType, iov);
                for (size_t i = 0; i < pkt->sz_iov; ++i) {
                    bool ok = process_buffer(callback, m_recv_data,
                                             reinterpret_cast<uint8_t *>(pkt->iov[i].iov_base),
                                             static_cast<int>(pkt->iov[i].iov_len));
                    if (unlikely(!ok)) {
                        return false;
                    }
                }
                cur_ptr += sizeof(pkt->iov[0]) * pkt->sz_iov;
            }
            return true;
        } else {
            // iterate over non-zerocopy buffer
            assert(m_ptr->m_pkt_buf && "data buffer pointer must be initialized");
            return process_buffer(callback, m_recv_data, m_ptr->m_pkt_buf, m_non_zcopy_len);
        }
    }
};

#ifdef USING_VMA_EXTRA_API // VMA
class SocketXtremeInputHandler : public MessageParser<BufferAccumulation> {
private:
    SocketRecvData &m_recv_data;
    vma_buff_t *m_vma_buff;
public:
    inline SocketXtremeInputHandler(Message *msg, SocketRecvData &recv_data):
        MessageParser<BufferAccumulation>(msg),
        m_recv_data(recv_data),
        m_vma_buff(NULL)
    {}

    /** Receive pending data from a socket
     * @param [in] socket descriptor
     * @param [out] recvfrom_addr address to save peer address into
     * @param [inout] in - storage size, out - actual address size
     * @return status code
     */
    inline int receive_pending_data(int fd, struct sockaddr *recvfrom_addr, socklen_t &size)
    {
        size = sizeof(sockaddr_in);
        std::memcpy(recvfrom_addr, &g_vma_comps->src, size);
        m_vma_buff = g_vma_buff;
        if (likely(m_vma_buff)) {
            return m_vma_buff->len;
        } else {
            return 0;
        }
    }

    template <class Callback>
    inline bool iterate_over_buffers(Callback &callback)
    {
        for (vma_buff_t *cur = m_vma_buff; cur; cur = cur->next) {
            bool res = process_buffer(callback, m_recv_data, (uint8_t *)cur->payload, cur->len);
            if (unlikely(!res)) {
                return false;
            }
        }
        return true;
    }

    inline void cleanup()
    {
    }
};

class VmaZCopyReadInputHandler : public ZCopyReadInputHandler<vma_packet_t, vma_packets_t, MSG_VMA_ZCOPY> {
public:
    inline VmaZCopyReadInputHandler(Message *msg, SocketRecvData &recv_data):
        ZCopyReadInputHandler<vma_packet_t, vma_packets_t, MSG_VMA_ZCOPY>(msg, recv_data, g_vma_api->recvfrom_zcopy)
    {}

    inline void cleanup()
    {
        if (likely(m_ptr && m_ptr->m_pkts)) {
            vma_packets_t* vma_pkts = reinterpret_cast<vma_packets_t *>(m_ptr->m_pkts);
            g_vma_api->free_packets(m_fd, vma_pkts->pkts, vma_pkts->n_packet_num);
            m_ptr->m_pkts = nullptr;
        }
    }
};
#endif // USING_VMA_EXTRA_API

#ifdef USING_XLIO_EXTRA_API // XLIO extra-api Only
class XlioZCopyReadInputHandler :
    public ZCopyReadInputHandler<xlio_recvfrom_zcopy_packet_t, xlio_recvfrom_zcopy_packets_t, MSG_XLIO_ZCOPY> {
public:
    inline XlioZCopyReadInputHandler(Message *msg, SocketRecvData &recv_data):
        ZCopyReadInputHandler<xlio_recvfrom_zcopy_packet_t, xlio_recvfrom_zcopy_packets_t, MSG_XLIO_ZCOPY>(
            msg, recv_data, g_xlio_api->recvfrom_zcopy)
    {}

    inline void cleanup()
    {
        if (likely(m_ptr && m_ptr->m_pkts)) {
            xlio_recvfrom_zcopy_packets_t *xlio_pkts = reinterpret_cast<xlio_recvfrom_zcopy_packets_t *>(m_ptr->m_pkts);
            g_xlio_api->recvfrom_zcopy_free_packets(m_fd, xlio_pkts->pkts, xlio_pkts->n_packet_num);
            m_ptr->m_pkts = nullptr;
        }
    }
};
#endif // USING_XLIO_EXTRA_API

#endif // USING_VMA_EXTRA_API || USING_XLIO_EXTRA_API

#endif // INPUT_HANDLERS_H_
