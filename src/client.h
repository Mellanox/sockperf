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

#ifndef CLIENT_H_
#define CLIENT_H_

#include "defs.h"
#include "common.h"
#include "input_handlers.h"
#include "packet.h"

//==============================================================================
//==============================================================================
class ClientBase {
public:
    ClientBase();
    virtual ~ClientBase();
    virtual void client_receiver_thread() = 0;

protected:
    Message *m_pMsgReply;
    Message *m_pMsgRequest;
};

//==============================================================================
//==============================================================================
template <class IoType, class SwitchDataIntegrity, class SwitchActivityInfo,
          class SwitchCycleDuration, class SwitchMsgSize, class PongModeCare>
class Client : public ClientBase {
private:
    os_thread_t m_receiverTid;
    IoType m_ioHandler;
    addr_to_id m_ServerList;

    SwitchDataIntegrity m_switchDataIntegrity;
    SwitchActivityInfo m_switchActivityInfo;
    SwitchCycleDuration m_switchCycleDuration;
    SwitchMsgSize m_switchMsgSize;
    PongModeCare m_pongModeCare; // has msg_sendto() method and can be one of: PongModeNormal,
                                 // PongModeAlways, PongModeNever

    class ClientMessageHandlerCallback {
        Client<IoType, SwitchDataIntegrity, SwitchActivityInfo,
              SwitchCycleDuration, SwitchMsgSize, PongModeCare> &m_client;
        int m_ifd;
        struct sockaddr_store_t &m_recvfrom_addr;
        socklen_t m_recvfrom_addrlen;
        int m_receiveCount;

    public:
        inline ClientMessageHandlerCallback(Client<IoType, SwitchDataIntegrity, SwitchActivityInfo,
                SwitchCycleDuration, SwitchMsgSize, PongModeCare> &client,
                int ifd, struct sockaddr_store_t &recvfrom_addr,
                socklen_t recvfrom_addrlen) :
            m_client(client),
            m_ifd(ifd),
            m_recvfrom_addr(recvfrom_addr),
            m_recvfrom_addrlen(recvfrom_addrlen),
            m_receiveCount(0)
        {
        }

        inline bool handle_message()
        {
            return m_client.handle_message(m_ifd, m_recvfrom_addr, m_recvfrom_addrlen, m_receiveCount);
        }

        inline int getReceiveCount() const
        {
            return m_receiveCount;
        }
    };

public:
    Client(int _fd_min, int _fd_max, int _fd_num);
    virtual ~Client();
    void doHandler();
    void client_receiver_thread();

private:
    int initBeforeLoop();
    void doSendThenReceiveLoop();
    void doSendLoop();
    void doPlayback();
    void cleanupAfterLoop();

    //------------------------------------------------------------------------------
    inline int client_get_server_id(int ifd, struct sockaddr_store_t &recvfrom_addr, socklen_t recvfrom_addrlen) {
        int serverNo = 0;

        IPAddress ip_addr(reinterpret_cast<sockaddr *>(&recvfrom_addr), recvfrom_addrlen);

        assert((g_fds_array[ifd]) && "invalid fd");
        if (g_pApp->m_const_params.client_work_with_srv_num == DEFAULT_CLIENT_WORK_WITH_SRV_NUM) {
            // if client_work_with_srv_num param is default- act as working with one server only
            // (even if working with multiple multicast groups)
            return serverNo;
        }
        if (g_fds_array[ifd] && g_fds_array[ifd]->is_multicast) {

            addr_to_id::iterator itr = m_ServerList.find(ip_addr);
            if (itr == m_ServerList.end()) {
                if ((int)m_ServerList.size() >= g_pApp->m_const_params.client_work_with_srv_num) {
                    /* To recognize case when more then expected servers are working */
                    serverNo = -1;
                } else {
                    serverNo = (int)m_ServerList.size();
                    std::pair<addr_to_id::iterator, bool> ret = m_ServerList.insert(
                        addr_to_id::value_type(ip_addr, m_ServerList.size()));
                    if (!ret.second) {
                        log_err("Failed to insert new server.");
                        serverNo = -1;
                    }
                }
            } else {
                serverNo = (int)itr->second;
            }
        }

        return serverNo;
    }

    //------------------------------------------------------------------------------
    inline void client_send_packet(int ifd) {
        int ret = 0;

        m_pMsgRequest->incSequenceCounter();

        ret = m_pongModeCare.msg_sendto(ifd);

        /* return on success */
        if (likely(ret > 0)) {
            return;
        }
        /* check dead peer case */
        else if (ret == RET_SOCKET_SHUTDOWN) {
            if (g_fds_array[ifd]->sock_type == SOCK_STREAM) {
                exit_with_log("A connection was forcibly closed by a peer", SOCKPERF_ERR_SOCKET,
                              g_fds_array[ifd]);
            }
        }
        /* check skip send operation case */
        else if (ret == RET_SOCKET_SKIPPED) {
            g_skipCount++;
            m_pMsgRequest->decSequenceCounter();
        }
    }

    //------------------------------------------------------------------------------
    template <class InputHandler>
    inline unsigned int client_receive_from_selected(int ifd) {
        int ret = 0;
        struct sockaddr_store_t recvfrom_addr;
        socklen_t recvfrom_len = sizeof(recvfrom_addr);
        fds_data *l_fds_ifd = g_fds_array[ifd];

        InputHandler input_handler(m_pMsgReply, l_fds_ifd->recv);
        ret = input_handler.receive_pending_data(ifd, reinterpret_cast<sockaddr *>(&recvfrom_addr), recvfrom_len);
        if (unlikely(ret <= 0)) {
            input_handler.cleanup();
            if (ret == RET_SOCKET_SHUTDOWN) {
                if (l_fds_ifd->sock_type == SOCK_STREAM) {
                    exit_with_log("A connection was forcibly closed by a peer", SOCKPERF_ERR_SOCKET,
                                  l_fds_ifd);
                }
            }
            else /* (ret < 0) */ return 0;
        }

        ClientMessageHandlerCallback callback(*this, ifd, recvfrom_addr, recvfrom_len);
        input_handler.iterate_over_buffers(callback);
        input_handler.cleanup();

        return callback.getReceiveCount();
    }

    inline unsigned int client_receive_from_selected(int ifd) {
#ifdef USING_VMA_EXTRA_API // VMA
        if (SOCKETXTREME == g_pApp->m_const_params.fd_handler_type && g_vma_api) {
            return client_receive_from_selected<SocketXtremeInputHandler>(ifd);
        }

        if (g_pApp->m_const_params.is_zcopyread && g_vma_api) {
            return client_receive_from_selected<VmaZCopyReadInputHandler>(ifd);
        }
#endif // USING_VMA_EXTRA_API
#ifdef USING_XLIO_EXTRA_API // XLIO
        if (g_pApp->m_const_params.is_zcopyread && g_xlio_api) {
            return client_receive_from_selected<XlioZCopyReadInputHandler>(ifd);
        }
#endif // USING_XLIO_EXTRA_API
        return client_receive_from_selected<RecvFromInputHandler>(ifd);
    }

    inline bool handle_message(int ifd, struct sockaddr_store_t &recvfrom_addr, socklen_t recvfrom_addrlen, int &receiveCount)
    {
        int serverNo = 0;

#if defined(LOG_TRACE_MSG_IN) && (LOG_TRACE_MSG_IN == TRUE)
        printf(">>> ");
        hexdump(m_pMsgReply->getBuf(), MsgHeader::EFFECTIVE_SIZE);
#endif /* LOG_TRACE_MSG_IN */
        if (unlikely(!m_pMsgReply->isValidHeader())) {
            exit_with_err("Message received was larger than expected.", SOCKPERF_ERR_FATAL);
        }
        if (unlikely(m_pMsgReply->getSequenceCounter() > m_pMsgRequest->getSequenceCounter())) {
            exit_with_err("Sequence Number received was higher than expected",
                    SOCKPERF_ERR_FATAL);
        }

        if (unlikely(g_b_exit)) {
            return false;
        }
        if (m_pMsgReply->isClient()) {
            assert(!(g_fds_array[ifd]->is_multicast && g_pApp->m_const_params.mc_loop_disable));
            return true;
        }

        // should not count the warmup messages
        if (unlikely(m_pMsgReply->isWarmupMessage())) {
            return true;
        }

        receiveCount++;
        TicksTime rxTime;
        rxTime.setNow();

#if 0 // should be part of check-data-integrity
        if (g_pApp->m_const_params.msg_size_range == 0) { //ABH: added 'if', otherwise, size check will not suit latency-under-load
            if (nbytes != g_msg_size && errno != EINTR) {
                exit_with_log("received message size test failed (sent:%d received:%d)", g_msg_size, nbytes,SOCKPERF_ERR_FATAL);
            }
        }
#endif

#ifdef DEBUG // should not occur in real test
        if (m_pMsgReply->getSequenceCounter() % g_pApp->m_const_params.reply_every) {
            log_err("skipping unexpected received message: seqNo=%" PRIu64 " mask=0x%x",
                    m_pMsgReply->getSequenceCounter(), m_pMsgReply->getFlags());
            return true;
        }
#endif

        serverNo = client_get_server_id(ifd, recvfrom_addr, recvfrom_addrlen);
        if (unlikely(serverNo < 0)) {
            exit_with_log("Number of servers more than expected", SOCKPERF_ERR_FATAL);
        } else {
            g_pPacketTimes->setRxTime(m_pMsgReply->getSequenceCounter(), rxTime, serverNo);
            m_switchDataIntegrity.execute(m_pMsgRequest, m_pMsgReply);
        }

        return true;
    }

    //------------------------------------------------------------------------------
    inline unsigned int client_receive(/*int packet_cnt_index*/) {
        int numReady = 0;

        do {
            // wait for arrival
            numReady = m_ioHandler.waitArrival();
        } while (!numReady && !g_b_exit);

        if (g_b_exit) return 0;

        // check errors
        if (unlikely(numReady < 0)) {
            exit_with_log(handler2str(g_pApp->m_const_params.fd_handler_type), SOCKPERF_ERR_FATAL);
        }

        /* ready fds were found so receive from the relevant sockets*/
        unsigned int recieved_packets_num = 0;
        int actual_fd = 0;
        for (int _fd = m_ioHandler.get_look_start(); _fd < m_ioHandler.get_look_end() && !g_b_exit;
             _fd++) {
            actual_fd = m_ioHandler.analyzeArrival(_fd);
            if (actual_fd) {
                int m_recived = g_pApp->m_const_params.max_looping_over_recv;
                while ((0 != m_recived) && (!g_b_exit)) {
                    if (m_recived > 0) {
                        m_recived--;
                    }
                    unsigned int recieved_packets =
                        client_receive_from_selected(actual_fd /*, packet_cnt_index*/);
                    if ((0 == recieved_packets) && (os_err_eagain())) {
                        break;
                    }
                    recieved_packets_num += recieved_packets;
                }
            }
        }
        return recieved_packets_num;
    }

    //------------------------------------------------------------------------------
    inline void client_send_burst(int ifd) {
        // init
        m_switchMsgSize.execute(m_pMsgRequest);

        // idle
        m_switchCycleDuration.execute(m_pMsgRequest, ifd);

        // send
        for (unsigned i = 0; i < g_pApp->m_const_params.burst_size && !g_b_exit; i++) {
            client_send_packet(ifd);
#ifdef USING_VMA_EXTRA_API // For VMA socketxtreme Only
            if (g_pApp->m_const_params.fd_handler_type == SOCKETXTREME &&
                !g_pApp->m_const_params.b_client_ping_pong) {
                m_ioHandler.waitArrival();
            }
#endif // USING_VMA_EXTRA_API
        }

        m_switchActivityInfo.execute(m_pMsgRequest->getSequenceCounter());
    }

    //------------------------------------------------------------------------------
    inline void client_receive_burst() {
        for (unsigned int i = 0; i < (g_pApp->m_const_params.burst_size *
                                      g_pApp->m_const_params.client_work_with_srv_num) &&
                                     !g_b_exit;) {
            i += client_receive();
        }
    }

    //------------------------------------------------------------------------------
    /*
    ** send to and receive from selected socket
    */
    //------------------------------------------------------------------------------

    inline void client_send_then_receive(int ifd) {
        client_send_burst(ifd);
        client_receive_burst();
    }
};

#endif /* CLIENT_H_ */
