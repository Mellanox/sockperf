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
#ifndef SWITCHES_H_
#define SWITCHES_H_

#include "Defs.h"
#include "common.h"
#include "Message.h"
#include "Ticks.h"
#include "PacketTimes.h"

//==============================================================================
class SwitchOff {
public:
	inline void execute() {}
	inline void execute(int) {}
//	inline void execute(unsigned long long) {}
	inline void execute(uint64_t) {}
	inline void execute(int, uint64_t) {}
	inline void execute(TicksTime & _ticks) {}
	inline void execute(struct sockaddr_in *clt_addr, uint64_t seq_num, bool is_warmup) {}
	inline void execute(Message *pMsgRequest, Message * pMsgReply) {}
	inline void execute(Message *pMsgRequest) {}

/*
	inline void execute2() {}
	inline void execute(struct timespec &) {}
*/
};


//------------------------------------------------------------------------------
class SwitchOnMsgSize {
public:
	SwitchOnMsgSize() {
		assert(g_pApp);

	    m_min_msg_size = max(MIN_PAYLOAD_SIZE,
	    		g_pApp->m_const_params.msg_size - g_pApp->m_const_params.msg_size_range);
	    m_range_msg_size = min(MAX_PAYLOAD_SIZE,
	    		g_pApp->m_const_params.msg_size + g_pApp->m_const_params.msg_size_range) -
	    		m_min_msg_size + 1;
	}
	inline void execute(Message *pMsgRequest) {client_update_msg_size(pMsgRequest);}

private:
	inline void client_update_msg_size(Message *pMsgRequest) {
	    int m_msg_size = min(MAX_PAYLOAD_SIZE, (m_min_msg_size + (int)(rand() % m_range_msg_size)));
	    pMsgRequest->setLength(m_msg_size);
	}
	int m_min_msg_size;
	int m_range_msg_size;

};

//==============================================================================
class SwitchOnCycleDuration {
public:
	//busy wait between two cycles starting point and take starting point of next cycle
	inline void execute() {

		TicksTime nextCycleStartTime = g_cycleStartTime + g_pApp->m_const_params.cycleDuration;
		while (!g_b_exit) {
			if (TicksTime::now() >= nextCycleStartTime) {
				break;
			}
			g_cycle_wait_loop_counter++; //count delta between time takings vs. num of cycles
		}
		g_cycleStartTime = nextCycleStartTime;
	}
};
//*
//==============================================================================
class PongModeNormal { // indicate that pong-request bit is set for part of the packets
public:
	PongModeNormal(){ assert(0); /* do not call this constructor */ }
	PongModeNormal(Message *pMsgRequest){
		m_pMsgRequest = pMsgRequest;
		m_pMsgRequest->getHeader()->resetPongRequest();
	}

	inline int msg_sendto(int ifd)
	{
		if (m_pMsgRequest->getSequenceCounter() % g_pApp->m_const_params.reply_every == 0) {
			m_pMsgRequest->getHeader()->setPongRequest();
			g_pPacketTimes->setTxTime(m_pMsgRequest->getSequenceCounter());
			int ret = ::msg_sendto(ifd, m_pMsgRequest->getBuf(), m_pMsgRequest->getLength(), &(g_fds_array[ifd]->addr));
			/* check skip send operation case */
			if (ret == RET_SOCKET_SKIPPED) {
				g_pPacketTimes->clearTxTime(m_pMsgRequest->getSequenceCounter());
			}
			m_pMsgRequest->getHeader()->resetPongRequest();
			return ret;
		}
		else
		{
			return ::msg_sendto(ifd, m_pMsgRequest->getBuf(), m_pMsgRequest->getLength(), &(g_fds_array[ifd]->addr));
		}
	}

private:
	Message *m_pMsgRequest;
};

//==============================================================================
class PongModeAlways { // indicate that pong-request bit is always on
public:
	PongModeAlways(){ assert(0); /* do not call this constructor */ }
	PongModeAlways(Message *pMsgRequest){
		m_pMsgRequest = pMsgRequest;
		m_pMsgRequest->getHeader()->setPongRequest();
	}

	inline int msg_sendto(int ifd) {
		g_pPacketTimes->setTxTime(m_pMsgRequest->getSequenceCounter());
		int ret = ::msg_sendto(ifd, m_pMsgRequest->getBuf(), m_pMsgRequest->getLength(), &(g_fds_array[ifd]->addr));
		/* check skip send operation case */
		if (ret == RET_SOCKET_SKIPPED) {
			g_pPacketTimes->clearTxTime(m_pMsgRequest->getSequenceCounter());
		}
		return ret;
	}

private:
	Message *m_pMsgRequest;
};

//==============================================================================
class PongModeNever { // indicate that pong-request bit is never on (no need to take tXtime)
public:
	PongModeNever(){ assert(0); /* do not call this constructor */ }
	PongModeNever(Message *pMsgRequest){
		m_pMsgRequest = pMsgRequest;
		m_pMsgRequest->getHeader()->resetPongRequest();
	}

	inline int msg_sendto(int ifd) {
		return ::msg_sendto(ifd, m_pMsgRequest->getBuf(), m_pMsgRequest->getLength(), &(g_fds_array[ifd]->addr));
	}

private:
	Message *m_pMsgRequest;
};

//*/
//==============================================================================
class SwitchOnActivityInfo {
public:
	/*inline*/ void execute (uint64_t counter);
};

//==============================================================================
class SwitchOnDataIntegrity {
public:
	//----------------------
	inline void execute(Message *pMsgSend, Message * pMsgReply) {
		if (!check_data_integrity(pMsgSend, pMsgReply)) {
			exit_with_log("data integrity test failed",SOCKPERF_ERR_INCORRECT);
		}
	}
private:
	//----------------------
	/* returns 1 if buffers are identical */
	inline int check_data_integrity(Message *pMsgSend, Message * pMsgReply)
	{
		uint8_t *message_buf = pMsgSend->getBuf();
		size_t buf_size = pMsgSend->getLength();
		/*static int to_print = 1;
		if (to_print == 1) {
			printf("%s\n", rcvd_buf);
			to_print = 0;
		}*/
	#ifdef USING_VMA_EXTRA_API
		if (g_dgram) {
			size_t i, pos, len;

			((MsgHeader*)(g_dgram->iov[0].iov_base))->setClient();/*match to client so data_integrity will pass*/

			pos = 0;
			for (i = 0; i < g_dgram->sz_iov; ++i) {
				len = g_dgram->iov[i].iov_len;

				if (buf_size < pos + len ||
					memcmp((char*)g_dgram->iov[i].iov_base,
						   message_buf + pos, len)) {
					return 0;
				}
				pos += len;
			}
			return pos == buf_size;
		} else {
			printf("dgram is NULL\n");
		}
	#endif

		//TODO: this is bug in orig udp_lat's code, in case USING_VMA_EXTRA_API and ! g_dgram we should compare to g_dgram_buf
		pMsgReply->setClient();
		return !memcmp(pMsgReply->getBuf(), message_buf, buf_size);

	}
};

class SwitchOnCalcGaps {
public:
	/*inline*/ void execute(struct sockaddr_in *clt_addr, uint64_t seq_num, bool is_warmup);


	static void print_summary()
	{
		seq_num_map::iterator itr;
		seq_num_map *p_seq_num_map = &ms_seq_num_map;

		if (!p_seq_num_map)
			return;
		for (itr = p_seq_num_map->begin(); itr !=  p_seq_num_map->end(); itr++) {
			print_session_summary(&(itr->second));
		}

	}

	static void print_session_summary(clt_session_info *p_clt_session)
	{
		char ip_port_str[30];

		if (p_clt_session) {
			sprintf(ip_port_str, "[%s:%d]", inet_ntoa(p_clt_session->addr.sin_addr),
				ntohs(p_clt_session->addr.sin_port));
			log_msg("%-23s Summary: Total Dropped/OOO: %" PRIu64, ip_port_str, p_clt_session->total_drops);
		}
	}

private:
	inline void check_gaps(uint64_t received_seq_num, seq_num_map::iterator &seq_num_map_itr)
	{
		if (received_seq_num != ++seq_num_map_itr->second.seq_num) {
			uint64_t drops_num;
			drops_num = calc_gaps_num(seq_num_map_itr->second.seq_num, received_seq_num);
			seq_num_map_itr->second.total_drops += drops_num;

			//Unordered packet
			if (!drops_num)
				seq_num_map_itr->second.seq_num--;
			else {
				char drops_num_str[10];
				char seq_num_info_str[50];
				char ip_port_str[30];
				sprintf(ip_port_str, "[%s:%d]", inet_ntoa(seq_num_map_itr->second.addr.sin_addr),
					ntohs(seq_num_map_itr->second.addr.sin_port));
				sprintf(drops_num_str, "(%" PRIu64 ")", drops_num);
				sprintf(seq_num_info_str, "[%" PRIu64 " - %" PRIu64 "]", seq_num_map_itr->second.seq_num, received_seq_num - 1);
				log_msg("%-23s Total Dropped/OOO: %-12" PRIu64 "GAP:%-7s %s",
					ip_port_str, seq_num_map_itr->second.total_drops,
					drops_num_str, seq_num_info_str);
				seq_num_map_itr->second.seq_num = received_seq_num;
			}
		}
	}

	inline uint64_t calc_gaps_num(uint64_t expected_seq, uint64_t received_seq)
	{
		return (received_seq > expected_seq) ? (received_seq - expected_seq) : 0;
	}

	inline void print_new_session_info(clt_session_info *p_clt_session)
	{
		if (p_clt_session) {
			char ip_port_str[30];
			sprintf(ip_port_str, "[%s:%d]", inet_ntoa(p_clt_session->addr.sin_addr),
				ntohs(p_clt_session->addr.sin_port));
			log_msg("Starting new session: %-23s", ip_port_str);
		}
	}

	static seq_num_map ms_seq_num_map;
};

#endif /* SWITCHES_H_ */
