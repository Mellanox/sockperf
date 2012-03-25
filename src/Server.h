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
#ifndef SERVER_H_
#define SERVER_H_

#include "common.h"

#ifdef ST_TEST
extern int prepare_socket(struct fds_data *p_data, int fd, bool stTest = false);
#else
extern int prepare_socket(struct fds_data *p_data, int fd);
#endif


class IoHandler;

class ServerBase {
private:

//protected:
public:
	//------------------------------------------------------------------------------
	ServerBase(IoHandler & _ioHandler);
	virtual ~ServerBase();

	void doHandler(){
		int rc = SOCKPERF_ERR_NONE;

		rc = initBeforeLoop();

		if (rc == SOCKPERF_ERR_NONE) {
			doLoop();
		}

		cleanupAfterLoop();
	}

	int initBeforeLoop();
	void cleanupAfterLoop();
	virtual void doLoop() = 0; // don't implement doLoop here because we want the compiler to provide distinct
	                           // implementation with inlined functions of derived classes  in each derived class

protected:

	// Note: for static binding at compilation time, we use the
	// reference to IoHandler base class ONLY for accessing non-virtual functions
	IoHandler & m_ioHandlerRef;
	Message * m_pMsgReply;
	Message * m_pMsgRequest;
};

//==============================================================================
//==============================================================================

template <class IoType, class SwitchActivityInfo, class SwitchCalcGaps>
class Server : public ServerBase{
private:
	IoType m_ioHandler;

//protected:
public:
	//------------------------------------------------------------------------------
	Server(int _fd_min, int _fd_max, int _fd_num);
	virtual ~Server();
	virtual void doLoop();

	//------------------------------------------------------------------------------
	/*
	** receive from and send to selected socket
	*/
	/*inline*/ bool server_receive_then_send(int ifd);

	//------------------------------------------------------------------------------
	int server_accept(int ifd);
private:
	SwitchActivityInfo m_switchActivityInfo;
	SwitchCalcGaps m_switchCalcGaps;
};
void print_log(const char* error, fds_data* fds)
{
	printf("IP = %-15s PORT = %5d # %s ",
			inet_ntoa(fds->addr.sin_addr),
			ntohs(fds->addr.sin_port),
			PRINT_PROTOCOL(fds->sock_type));
				log_err("%s",error);
}
void print_log(const char* error, int fds)
{
	printf("actual_fd = %d ",fds);
	log_err("%s",error);
}
//------------------------------------------------------------------------------
/*
** when ret == RET_SOCKET_SHUTDOWN
** close ifd
*/
void close_ifd(int fd,int ifd,fds_data* l_fds_ifd){
	fds_data* l_next_fd =  g_fds_array[fd];
	for (int i = 0; i < MAX_ACTIVE_FD_NUM; i++) {
		if (l_next_fd->active_fd_list[i] == ifd) {
			print_log_dbg( l_fds_ifd->addr.sin_addr, l_fds_ifd->addr.sin_port, ifd);
			close(ifd);
			l_next_fd->active_fd_count--;
			l_next_fd->active_fd_list[i] = INVALID_SOCKET;
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
inline bool Server<IoType, SwitchActivityInfo, SwitchCalcGaps>::server_receive_then_send(int ifd)
{
	struct sockaddr_in recvfrom_addr;
	struct sockaddr_in sendto_addr;
	bool do_update = true;
	int ret = 0;
	fds_data* l_fds_ifd = g_fds_array[ifd];
	if (!l_fds_ifd)
		return (do_update);
	ret = msg_recvfrom(ifd,
			           l_fds_ifd->recv.cur_addr + l_fds_ifd->recv.cur_offset,
			           l_fds_ifd->recv.cur_size,
			           &recvfrom_addr);

	if (ret == RET_SOCKET_SHUTDOWN) {
		if (l_fds_ifd->sock_type == SOCK_STREAM) {
			close_ifd( l_fds_ifd->next_fd,ifd,l_fds_ifd);
		}
		return (do_update);
	}
	if (ret < 0) return (!do_update);

	int nbytes = ret;
	while (nbytes) {

		/* 1: message header is not received yet */
		if ((l_fds_ifd->recv.cur_offset + nbytes) < MsgHeader::EFFECTIVE_SIZE) {
			l_fds_ifd->recv.cur_size -= nbytes;
			l_fds_ifd->recv.cur_offset += nbytes;

			/* 4: set current buffer size to size of remained part of message header to
			 *    guarantee getting full message header on next iteration
			 */
			if (l_fds_ifd->recv.cur_size < MsgHeader::EFFECTIVE_SIZE) {
				l_fds_ifd->recv.cur_size = MsgHeader::EFFECTIVE_SIZE - l_fds_ifd->recv.cur_offset;
			}
			return (!do_update);
		}

		/* 2: message header is got, match message to cycle buffer */
		m_pMsgReply->setBuf(l_fds_ifd->recv.cur_addr);

		if ( m_pMsgReply->getLength() > MAX_PAYLOAD_SIZE){
			//Message received was larger than expected, message ignored. - only on stream mode.
			print_log("Message received was larger than expected, message ignored.", l_fds_ifd);

			close_ifd( l_fds_ifd->next_fd,ifd,l_fds_ifd);
			return (do_update);
		}

		/* 3: message is not complete */
		if ((l_fds_ifd->recv.cur_offset + nbytes) < m_pMsgReply->getLength()) {
			l_fds_ifd->recv.cur_size -= nbytes;
			l_fds_ifd->recv.cur_offset += nbytes;

			/* 4: set current buffer size to size of remained part of message to
			 *    guarantee getting full message on next iteration (using extended reserved memory)
			 *    and shift to start of cycle buffer
			 */
			if (l_fds_ifd->recv.cur_size < (int)m_pMsgReply->getMaxSize()) {
				l_fds_ifd->recv.cur_size = m_pMsgReply->getLength() - l_fds_ifd->recv.cur_offset;
			}
			return (!do_update);
		}

		/* 5: message is complete shift to process next one */
		nbytes -= m_pMsgReply->getLength() - l_fds_ifd->recv.cur_offset;
		l_fds_ifd->recv.cur_addr += m_pMsgReply->getLength();
		l_fds_ifd->recv.cur_size -= m_pMsgReply->getLength() - l_fds_ifd->recv.cur_offset;
		l_fds_ifd->recv.cur_offset = 0;

#if defined(LOG_TRACE_MSG_IN) && (LOG_TRACE_MSG_IN==TRUE)
		printf(">>> ");
		hexdump(m_pMsgReply->getBuf(), MsgHeader::EFFECTIVE_SIZE);
#endif /* LOG_TRACE_MSG_IN */

		if (g_b_exit) return (!do_update);
		if (!m_pMsgReply->isClient()) {
			/* 6: shift to start of cycle buffer in case receiving buffer is empty and
			 * there is no uncompleted message
			 */
			if (!nbytes) {
				l_fds_ifd->recv.cur_addr = l_fds_ifd->recv.buf;
				l_fds_ifd->recv.cur_size = l_fds_ifd->recv.max_size;
				l_fds_ifd->recv.cur_offset = 0;
			}
			return (!do_update);
		}

		if (m_pMsgReply->isWarmupMessage()) {
			m_switchCalcGaps.execute(&recvfrom_addr, 0, true);
			/* 6: shift to start of cycle buffer in case receiving buffer is empty and
			 * there is no uncompleted message
			 */
			if (!nbytes) {
				l_fds_ifd->recv.cur_addr = l_fds_ifd->recv.buf;
				l_fds_ifd->recv.cur_size = l_fds_ifd->recv.max_size;
				l_fds_ifd->recv.cur_offset = 0;
			}
			return (!do_update);
		}

		g_receiveCount++; //// should move to setRxTime (once we use it in server side)

		if (m_pMsgReply->getHeader()->isPongRequest()) {
			/* if server in a no reply mode - shift to start of cycle buffer*/
			if (g_pApp->m_const_params.b_server_dont_reply)
			{
				l_fds_ifd->recv.cur_addr = l_fds_ifd->recv.buf;
				l_fds_ifd->recv.cur_size = l_fds_ifd->recv.max_size;
				l_fds_ifd->recv.cur_offset = 0;
				return (do_update);
			}
			/* prepare message header */
			if (g_pApp->m_const_params.mode != MODE_BRIDGE) {
				m_pMsgReply->setServer();
			}
			/* get source addr to reply. memcpy is not used to improve performance */
			sendto_addr =l_fds_ifd->addr;

			if (l_fds_ifd->memberships_size || !l_fds_ifd->is_multicast || g_pApp->m_const_params.b_server_reply_via_uc) {// In unicast case reply to sender
				/* get source addr to reply. memcpy is not used to improve performance */
				sendto_addr = recvfrom_addr;
			}else if (l_fds_ifd->is_multicast)
			{
				/* always send to the same port recved from */
				sendto_addr.sin_port = recvfrom_addr.sin_port;
			}
			ret = msg_sendto(ifd, m_pMsgReply->getBuf(), m_pMsgReply->getLength(), &sendto_addr);
			if (ret == RET_SOCKET_SHUTDOWN) {
				if (l_fds_ifd->sock_type == SOCK_STREAM) {
					close_ifd( l_fds_ifd->next_fd,ifd,l_fds_ifd);
				}
				return (do_update);
			}
		}

		m_switchCalcGaps.execute(&recvfrom_addr, m_pMsgReply->getSequenceCounter(), false);
		m_switchActivityInfo.execute(g_receiveCount);
	}

	/* 6: shift to start of cycle buffer in case receiving buffer is empty and
	 * there is no uncompleted message
	 */
	// nbytes == 0
	l_fds_ifd->recv.cur_addr = l_fds_ifd->recv.buf;
	l_fds_ifd->recv.cur_size = l_fds_ifd->recv.max_size;
	l_fds_ifd->recv.cur_offset = 0;

	return (!do_update);
}

#endif /* SERVER_H_ */
