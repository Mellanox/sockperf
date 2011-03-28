 /*
 * Copyright (c) 2011 Mellanox Technologies Ltd.
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
 *
 */
#ifndef CLIENT_H_
#define CLIENT_H_

#include "common.h"
#include "PacketTimes.h"

//==============================================================================
//==============================================================================
class ClientBase {
public:
	ClientBase();
	virtual ~ClientBase();
	virtual void client_receiver_thread() = 0;
protected:

	Message * m_pMsgReply;
	Message * m_pMsgRequest;
};

//==============================================================================
//==============================================================================
template <class IoType, class SwitchDataIntegrity, class SwitchActivityInfo, class SwitchCycleDuration, class SwitchMsgSize , class PongModeCare >
class Client : public ClientBase{
private:
	pthread_t m_receiverTid;
	IoType m_ioHandler;

	SwitchDataIntegrity m_switchDataIntegrity;
	SwitchActivityInfo  m_switchActivityInfo;
	SwitchCycleDuration m_switchCycleDuration;
	SwitchMsgSize       m_switchMsgSize;
	PongModeCare        m_pongModeCare; // has msg_sendto() method and can be one of: PongModeNormal, PongModeAlways, PongModeNever

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
	inline void client_send_packet(int ifd)
	{
		int ret = 0;

		m_pMsgRequest->incSequenceCounter();

		ret = m_pongModeCare.msg_sendto(ifd);
		if (ret == 0) {
			if (g_fds_array[ifd]->sock_type == SOCK_STREAM) {
				log_err("A connection was forcibly closed by a peer");
				exit_with_log(SOCKPERF_ERR_SOCKET);
			}
		}
	}

	//------------------------------------------------------------------------------
	inline unsigned int client_receive_from_selected(int ifd)
	{
		static const int SERVER_NO = 0;
		int ret = 0;
		int nbytes = 0;
		struct sockaddr_in recvfrom_addr;
		int receiveCount = 0;

		TicksTime rxTime;

		ret = msg_recvfrom(ifd,
				           g_fds_array[ifd]->recv.cur_addr + g_fds_array[ifd]->recv.cur_offset,
				           g_fds_array[ifd]->recv.cur_size,
				           &recvfrom_addr);
		if (ret == 0) {
			if (g_fds_array[ifd]->sock_type == SOCK_STREAM) {
				log_err("A connection was forcibly closed by a peer");
				exit_with_log(SOCKPERF_ERR_SOCKET);
			}
		}
		if (ret < 0) return 0;

		nbytes = ret;
		while (nbytes) {

			/* 1: message header is not received yet */
			if ((g_fds_array[ifd]->recv.cur_offset + nbytes) < MsgHeader::EFFECTIVE_SIZE) {
				g_fds_array[ifd]->recv.cur_size -= nbytes;
				g_fds_array[ifd]->recv.cur_offset += nbytes;

				/* 4: set current buffer size to size of remained part of message header to
				 *    guarantee getting full message header on next iteration
				 */
				if (g_fds_array[ifd]->recv.cur_size < MsgHeader::EFFECTIVE_SIZE) {
					g_fds_array[ifd]->recv.cur_size = MsgHeader::EFFECTIVE_SIZE - g_fds_array[ifd]->recv.cur_offset;
				}
				return (receiveCount);
			}

			/* 2: message header is got, match message to cycle buffer */
			m_pMsgReply->setBuf(g_fds_array[ifd]->recv.cur_addr);

			/* 3: message is not complete */
			if ((g_fds_array[ifd]->recv.cur_offset + nbytes) < m_pMsgReply->getLength()) {
				g_fds_array[ifd]->recv.cur_size -= nbytes;
				g_fds_array[ifd]->recv.cur_offset += nbytes;

				/* 4: set current buffer size to size of remained part of message to
				 *    guarantee getting full message on next iteration (using extended reserved memory)
				 *    and shift to start of cycle buffer
				 */
				if (g_fds_array[ifd]->recv.cur_size < (int)m_pMsgReply->getMaxSize()) {
					g_fds_array[ifd]->recv.cur_size = m_pMsgReply->getLength() - g_fds_array[ifd]->recv.cur_offset;
				}
				return (receiveCount);
			}

			/* 5: message is complete shift to process next one */
			nbytes -= m_pMsgReply->getLength() - g_fds_array[ifd]->recv.cur_offset;
			g_fds_array[ifd]->recv.cur_addr += m_pMsgReply->getLength();
			g_fds_array[ifd]->recv.cur_size -= m_pMsgReply->getLength() - g_fds_array[ifd]->recv.cur_offset;
			g_fds_array[ifd]->recv.cur_offset = 0;

#if defined(LOG_TRACE_MSG_IN) && (LOG_TRACE_MSG_IN==TRUE)
			printf(">>> ");
			hexdump(m_pMsgReply->getBuf(), MsgHeader::EFFECTIVE_SIZE);
#endif /* LOG_TRACE_MSG_IN */

			if (g_b_exit) return 0;
			if (m_pMsgReply->isClient()) {
				assert(!(g_fds_array[ifd]->is_multicast && g_pApp->m_const_params.mc_loop_disable));
				continue;
			}

			receiveCount++;
			rxTime.setNow();

			#if 0 //should be part of check-data-integrity
			if (g_pApp->m_const_params.msg_size_range == 0) { //ABH: added 'if', otherwise, size check will not suit latency-under-load
				if (nbytes != g_msg_size && errno != EINTR) {
					log_msg("received message size test failed (sent:%d received:%d)", g_msg_size, nbytes);
					exit_with_log(SOCKPERF_ERR_FATAL);
				}
			}
			#endif

			#ifdef DEBUG //should not occur in real test
			if (m_pMsgReply->getSequenceCounter() % g_pApp->m_const_params.reply_every) {
				log_err("skipping unexpected received packet: seqNo=%" PRIu64 " mask=0x%x",
						m_pMsgReply->getSequenceCounter(), m_pMsgReply->getFlags());
				continue;
			}
			#endif

			g_pPacketTimes->setRxTime(m_pMsgReply->getSequenceCounter(), rxTime, SERVER_NO);
			m_switchDataIntegrity.execute(m_pMsgRequest, m_pMsgReply);
		}

		/* 6: shift to start of cycle buffer in case receiving buffer is empty and
		 * there is no uncompleted message
		 */
		if (!nbytes) {
			g_fds_array[ifd]->recv.cur_addr = g_fds_array[ifd]->recv.buf;
			g_fds_array[ifd]->recv.cur_size = g_fds_array[ifd]->recv.max_size;
			g_fds_array[ifd]->recv.cur_offset = 0;
		}

		return (receiveCount);
	}

	//------------------------------------------------------------------------------
	inline unsigned int client_receive(/*int packet_cnt_index*/)
	{
		int numReady = 0;
		int actual_fd = 0;
		unsigned int recieved_packets_num = 0;

		do {
			// wait for arrival
			numReady = m_ioHandler.waitArrival();

			// check errors
			if (g_b_exit) break;
			if (numReady < 0) {
				log_err("%s() failed", g_fds_handle_desc[g_pApp->m_const_params.fd_handler_type]);
				exit_with_log(SOCKPERF_ERR_FATAL);
			}
			if (numReady == 0) {
				//if (!g_pApp->m_const_params.select_timeout) - ABH: who cares?
				//	log_msg("Error: %s() returned without fd ready", g_fds_handle_desc[g_pApp->m_const_params.fd_handler_type]);
				continue;
			}

			/* ready fds were found so receive from the relevant sockets*/
			for (int _fd = m_ioHandler.get_look_start(); (_fd < m_ioHandler.get_look_end()); _fd++) {
				actual_fd = m_ioHandler.analyzeArrival(_fd);
				if (actual_fd){
					recieved_packets_num += client_receive_from_selected(actual_fd/*, packet_cnt_index*/);
				}
			}

		} while (numReady <= 0);

		return recieved_packets_num;
	}

	//------------------------------------------------------------------------------
	inline void client_send_burst(int ifd)
	{
		//init
		m_switchMsgSize.execute(m_pMsgRequest);

		//idle
		m_switchCycleDuration.execute();

		//send
		for (unsigned i = 0; i < g_pApp->m_const_params.burst_size && !g_b_exit; i++) {
			client_send_packet(ifd);
		}

		m_switchActivityInfo.execute(m_pMsgRequest->getSequenceCounter());
	}

	//------------------------------------------------------------------------------
	inline void client_receive_burst()
	{
		for (unsigned int i = 0; i < g_pApp->m_const_params.burst_size && !g_b_exit; ) {
			i += client_receive();
		}
	}

	//------------------------------------------------------------------------------
	/*
	** send to and receive from selected socket
	*/
	//------------------------------------------------------------------------------

	inline void client_send_then_receive(int ifd)
	{
		client_send_burst(ifd);
		client_receive_burst();
	}

};

#endif /* CLIENT_H_ */
