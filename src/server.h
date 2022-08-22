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

#ifndef SERVER_H_
#define SERVER_H_

#include "defs.h"
#include "common.h"
#include "input_handlers.h"

#ifdef ST_TEST
extern int prepare_socket(int fd, struct fds_data *p_data, bool stTest = false);
#else
extern int prepare_socket(int fd, struct fds_data *p_data);
#endif

class IoHandler;

class ServerBase {
private:
    // protected:
public:
    //------------------------------------------------------------------------------
    ServerBase(IoHandler &_ioHandler);
    virtual ~ServerBase();

    void doHandler() {
        int rc = SOCKPERF_ERR_NONE;

        rc = initBeforeLoop();

        if (rc == SOCKPERF_ERR_NONE) {
            doLoop();
        }

        cleanupAfterLoop();
    }

    int initBeforeLoop();
    void cleanupAfterLoop();
    virtual void doLoop() = 0; // don't implement doLoop here because we want the compiler to
                               // provide distinct
    // implementation with inlined functions of derived classes  in each derived class

protected:
    // Note: for static binding at compilation time, we use the
    // reference to IoHandler base class ONLY for accessing non-virtual functions
    IoHandler &m_ioHandlerRef;
    Message *m_pMsgReply;
    Message *m_pMsgRequest;
};

//==============================================================================
//==============================================================================

template <class IoType, class SwitchActivityInfo, class SwitchCalcGaps>
class Server : public ServerBase {
private:
    IoType m_ioHandler;

    class ServerMessageHandlerCallback {
        Server<IoType, SwitchActivityInfo, SwitchCalcGaps> &m_server;
        int m_ifd;
        struct sockaddr_store_t &m_recvfrom_addr;
        socklen_t m_recvfrom_len;
        fds_data *m_fds_ifd;

    public:
        inline ServerMessageHandlerCallback(Server<IoType, SwitchActivityInfo, SwitchCalcGaps> &server,
                int ifd, struct sockaddr_store_t &recvfrom_addr, socklen_t recvfrom_len,
                fds_data *l_fds_ifd) :
            m_server(server),
            m_ifd(ifd),
            m_recvfrom_addr(recvfrom_addr),
            m_recvfrom_len(recvfrom_len),
            m_fds_ifd(l_fds_ifd)
        {
        }

        inline bool handle_message()
        {
            return m_server.handle_message(m_ifd, m_recvfrom_addr, m_recvfrom_len,
                    m_fds_ifd);
        }
    };

    // protected:
public:
    //------------------------------------------------------------------------------
    Server(int _fd_min, int _fd_max, int _fd_num);
    virtual ~Server();
    virtual void doLoop();

    //------------------------------------------------------------------------------
    /*
    ** receive from and send to selected socket
    */
    template <class InputHandler>
    /*inline*/ bool server_receive_then_send(int ifd);

    inline bool server_receive_then_send(int ifd)
    {
#ifdef USING_VMA_EXTRA_API // VMA
        if (SOCKETXTREME == g_pApp->m_const_params.fd_handler_type && g_vma_api) {
            return server_receive_then_send<SocketXtremeInputHandler>(ifd);
        }

        if (g_pApp->m_const_params.is_zcopyread && g_vma_api) {
            return server_receive_then_send<VmaZCopyReadInputHandler>(ifd);
        }
#endif // USING_VMA_EXTRA_API
#ifdef USING_XLIO_EXTRA_API // XLIO
        if (g_pApp->m_const_params.is_zcopyread && g_xlio_api) {
            return server_receive_then_send<XlioZCopyReadInputHandler>(ifd);
        }
#endif // USING_XLIO_EXTRA_API
        return server_receive_then_send<RecvFromInputHandler>(ifd);
    }

    /*inline*/ bool handle_message(int ifd, struct sockaddr_store_t &recvfrom_addr,
            socklen_t recvfrom_len, fds_data *l_fds_ifd);

    //------------------------------------------------------------------------------
    int server_accept(int ifd);

private:
    SwitchActivityInfo m_switchActivityInfo;
    SwitchCalcGaps m_switchCalcGaps;
};

void print_log(const char *error, const fds_data *fds) {
    std::string hostport = sockaddr_to_hostport(reinterpret_cast<const sockaddr *>(&fds->server_addr));
    printf("ADDR = %s # %s ", hostport.c_str(), PRINT_PROTOCOL(fds->sock_type));
    log_err("%s", error);
}

void print_log(const char *error, int fds) {
    printf("actual_fd = %d ", fds);
    log_err("%s", error);
}
//------------------------------------------------------------------------------
/*
** when ret == RET_SOCKET_SHUTDOWN
** close ifd
*/
void close_ifd(int fd, int ifd, fds_data *l_fds_ifd) {
    fds_data *l_next_fd = g_fds_array[fd];

#ifdef USING_VMA_EXTRA_API // VMA
    if (g_vma_api) {
        ZeroCopyData *z_ptr = g_zeroCopyData[fd];
        if (z_ptr && z_ptr->m_pkts) {
            vma_packets_t* vma_pkts = reinterpret_cast<vma_packets_t *>(z_ptr->m_pkts);
            g_vma_api->free_packets(fd, vma_pkts->pkts, vma_pkts->n_packet_num);
            z_ptr->m_pkts = NULL;
        }

        g_vma_api->register_recv_callback(fd, NULL, NULL);
    }
#endif // USING_VMA_EXTRA_API
#ifdef USING_XLIO_EXTRA_API // XLIO
    if (g_xlio_api) {
        ZeroCopyData *z_ptr = g_zeroCopyData[fd];
        if (z_ptr && z_ptr->m_pkts) {
            xlio_recvfrom_zcopy_packets_t *xlio_pkts = reinterpret_cast<xlio_recvfrom_zcopy_packets_t *>(z_ptr->m_pkts);
            g_xlio_api->recvfrom_zcopy_free_packets(fd, xlio_pkts->pkts, xlio_pkts->n_packet_num);
            z_ptr->m_pkts = NULL;
        }
    }
#endif // USING_XLIO_EXTRA_API

    for (int i = 0; i < MAX_ACTIVE_FD_NUM; i++) {
        if (l_next_fd->active_fd_list[i] == ifd) {
            print_log_dbg(reinterpret_cast<sockaddr *>(&l_fds_ifd->server_addr), ifd);
            close(ifd);
            l_next_fd->active_fd_count--;
            l_next_fd->active_fd_list[i] =
                (int)INVALID_SOCKET; // TODO: use SOCKET all over the way and avoid this cast
            if (g_fds_array[ifd]->active_fd_list) {
                FREE(g_fds_array[ifd]->active_fd_list);
            }
            if (g_fds_array[ifd]->recv.buf) {
                FREE(g_fds_array[ifd]->recv.buf);
            }
            free(g_fds_array[ifd]);
            g_fds_array[ifd] = NULL;
            break;
        }
    }
}
//------------------------------------------------------------------------------
/*
** receive from and send to selected socket
*/
template <class IoType, class SwitchActivityInfo, class SwitchCalcGaps>
template <class InputHandler>
inline bool Server<IoType, SwitchActivityInfo, SwitchCalcGaps>::server_receive_then_send(int ifd) {
    struct sockaddr_store_t recvfrom_addr;
    socklen_t recvfrom_len = sizeof(recvfrom_addr);
    static const bool do_update = true;
    int ret = 0;
    fds_data *l_fds_ifd = g_fds_array[ifd];

    if (unlikely(!l_fds_ifd)) {
        return (do_update);
    }

    InputHandler input_handler(m_pMsgReply, l_fds_ifd->recv);
    ret = input_handler.receive_pending_data(ifd, reinterpret_cast<sockaddr *>(&recvfrom_addr), recvfrom_len);
    if (unlikely(ret <= 0)) {
        input_handler.cleanup();
        if (ret == RET_SOCKET_SHUTDOWN) {
            if (l_fds_ifd->sock_type == SOCK_STREAM) {
                close_ifd(l_fds_ifd->next_fd, ifd, l_fds_ifd);
            }
            return (do_update);
        } else /* (ret < 0) */ {
            return (!do_update);
        }
    }

    ServerMessageHandlerCallback callback(*this, ifd, recvfrom_addr,
            recvfrom_len, l_fds_ifd);
    bool ok = input_handler.iterate_over_buffers(callback);
    input_handler.cleanup();

    if (likely(ok)) {
        return (!do_update);
    } else {
        if (l_fds_ifd->sock_type == SOCK_STREAM) {
            close_ifd(l_fds_ifd->next_fd, ifd, l_fds_ifd);
        }
        return (do_update);
    }
}

template <class IoType, class SwitchActivityInfo, class SwitchCalcGaps>
inline bool Server<IoType, SwitchActivityInfo, SwitchCalcGaps>::handle_message(int ifd,
        struct sockaddr_store_t &recvfrom_addr, socklen_t recvfrom_len, fds_data *l_fds_ifd)
{
    struct sockaddr_store_t sendto_addr;
    socklen_t sendto_addr_len = 0;

#if defined(LOG_TRACE_MSG_IN) && (LOG_TRACE_MSG_IN == TRUE)
    printf(">>> ");
    hexdump(m_pMsgReply->getBuf(), MsgHeader::EFFECTIVE_SIZE);
#endif /* LOG_TRACE_MSG_IN */

    if (unlikely(!m_pMsgReply->isValidHeader())) {
        print_log("Message received was larger than expected, message ignored.", l_fds_ifd);
        return false;
    }
    if (g_b_exit) {
        return false;
    }
    if (unlikely(!m_pMsgReply->isClient())) {
        return true;
    }
    if (unlikely(m_pMsgReply->isWarmupMessage())) {
        m_switchCalcGaps.execute(recvfrom_addr, recvfrom_len, 0, true);
        return true;
    }

    g_receiveCount++; //// should move to setRxTime (once we use it in server side)

    if (m_pMsgReply->getHeader()->isPongRequest()) {
        /* if server in a no reply mode - shift to start of cycle buffer*/
        if (g_pApp->m_const_params.b_server_dont_reply) {
            return true;
        }
        /* prepare message header */
        if (g_pApp->m_const_params.mode != MODE_BRIDGE) {
            m_pMsgReply->setServer();
        }
        /* get source addr to reply. memcpy is not used to improve performance */
        copy_relevant_sockaddr_params(sendto_addr, l_fds_ifd->server_addr);
        sendto_addr_len = l_fds_ifd->server_addr_len;

        if (l_fds_ifd->memberships_size || !l_fds_ifd->is_multicast ||
            g_pApp->m_const_params.b_server_reply_via_uc) { // In unicast case reply to sender
            /* get source addr to reply. memcpy is not used to improve performance */
            copy_relevant_sockaddr_params(sendto_addr, recvfrom_addr);
            sendto_addr_len = recvfrom_len;
        } else if (l_fds_ifd->is_multicast) {
            /* always send to the same port recved from */
            sockaddr_set_portn(sendto_addr, sockaddr_get_portn(recvfrom_addr));
        }
        int length = m_pMsgReply->getLength();
        m_pMsgReply->setHeaderToNetwork();

        int ret = msg_sendto(ifd, m_pMsgReply->getBuf(), length,
                reinterpret_cast<sockaddr *>(&sendto_addr), sendto_addr_len);
        if (unlikely(ret == RET_SOCKET_SHUTDOWN)) {
            if (l_fds_ifd->sock_type == SOCK_STREAM) {
                close_ifd(l_fds_ifd->next_fd, ifd, l_fds_ifd);
            }
            return false;
        }
        m_pMsgReply->setHeaderToHost();
    }

    m_switchCalcGaps.execute(recvfrom_addr, recvfrom_len, m_pMsgReply->getSequenceCounter(), false);
    m_switchActivityInfo.execute(g_receiveCount);

    return true;
}

#endif /* SERVER_H_ */
