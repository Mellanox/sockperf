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
	virtual void client_receiver_thread() = 0;
protected:
	ClientBase();
	virtual ~ClientBase();
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
	void initBeforeLoop();
	void doSendThenReceiveLoop();
	void doSendLoop();
	void doPlayback();
	void cleanupAfterLoop();

	//------------------------------------------------------------------------------
	inline void client_send_packet(int ifd)
	{
		g_pMessage->incSequenceCounter();

		m_pongModeCare.msg_sendto(ifd);
	}

	//------------------------------------------------------------------------------
	inline unsigned int client_receive_from_selected(int ifd)
	{
		static const int SERVER_NO = 0;

		int nbytes = 0;
		struct sockaddr_in recvfrom_addr;

		TicksTime rxTime;

		do {
			if (g_b_exit) return 0;
			nbytes = ::msg_recvfrom(ifd, &recvfrom_addr);

			if (g_pReply->isClient()) {
				#ifdef DEBUG
				if (g_pApp->m_const_params.mc_loop_disable)
					log_err("got client packet");
				#endif
				return 0; // got 0 valid packets
			}

			rxTime.setNow();

			#ifdef DEBUG //should be part of check-data-integrity
			if (g_pApp->m_const_params.msg_size_range == 0) { //ABH: added 'if', otherwise, size check will not suit latency-under-load
				if (nbytes != g_msg_size) {
					log_msg("received message size test failed (sent:%d received:%d)", g_msg_size, nbytes);
					exit_with_log(16);
				}
			}
			#endif

			#ifdef DEBUG //should not occur in real test
			if (g_pReply->getSequenceCounter() % g_pApp->m_const_params.reply_every) {
				log_err("skipping unexpected received packet: seqNo=%" PRIu64 " mask=0x%x", g_pReply->getSequenceCounter(), g_pReply->getFlags());
				return 0; // got 0 valid packets
			}
			#endif

			break;
		} while (true);

		g_pPacketTimes->setRxTime(g_pReply->getSequenceCounter(), rxTime, SERVER_NO);
		m_switchDataIntegrity.execute();
		return 1; // got 1 valid packet
	}

	//------------------------------------------------------------------------------
	inline unsigned int client_receive(/*int packet_cnt_index*/)
	{
		int numReady = 0;
		int actual_fd = 0;
		unsigned int recived_packets_num = 0;

		do {
			// wait for arrival
			numReady = m_ioHandler.waitArrival();

			// check errors
			if (g_b_exit) break;
			if (numReady < 0) {
				log_err("%s() failed", g_fds_handle_desc[g_pApp->m_const_params.fd_handler_type]);
				exit_with_log(1);
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
					recived_packets_num += client_receive_from_selected(actual_fd/*, packet_cnt_index*/);
				}
			}

		} while (numReady <= 0);

		return recived_packets_num;
	}

	//------------------------------------------------------------------------------
	inline void client_send_burst(int ifd)
	{
		//init
		m_switchMsgSize.execute();

		//idle
		m_switchCycleDuration.execute();

		//send
		for (unsigned i = 0; i < g_pApp->m_const_params.burst_size && !g_b_exit; i++) {
			client_send_packet(ifd);
		}

		m_switchActivityInfo.execute(g_pMessage->getSequenceCounter());
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
