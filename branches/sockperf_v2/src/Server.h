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
			cleanupAfterLoop();
		}
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
	/*inline*/ int server_accept(int ifd);
private:
	SwitchActivityInfo m_switchActivityInfo;
	SwitchCalcGaps m_switchCalcGaps;
};


#if defined(LOG_TRACE_CONNECT) && (LOG_TRACE_CONNECT==TRUE)
	#define _DBG_FDS_NUM  10
	static int _dbg_fds[_DBG_FDS_NUM];
#endif /* LOG_TRACE_CONNECT */

//------------------------------------------------------------------------------
template <class IoType, class SwitchActivityInfo, class SwitchCalcGaps>
inline int Server<IoType, SwitchActivityInfo, SwitchCalcGaps>::server_accept(int ifd)
{
	bool do_accept = false;
	int active_ifd = ifd;

	if (g_fds_array[ifd]->sock_type == SOCK_STREAM && g_fds_array[ifd]->active_fd_list) {
		struct sockaddr_in addr;
		socklen_t addr_size = sizeof(addr);
		fds_data *tmp;

		tmp = (struct fds_data *)MALLOC(sizeof(struct fds_data));
		if (!tmp) {
			log_err("Failed to allocate memory with malloc()");
			return INVALID_SOCKET;
		}
		memcpy(tmp, g_fds_array[ifd], sizeof(struct fds_data));
		tmp->next_fd = ifd;
		tmp->active_fd_list = NULL;
		tmp->active_fd_count = 0;
		tmp->recv.cur_addr = tmp->recv.buf;
		tmp->recv.max_size = sizeof(tmp->recv.buf) - MAX_PAYLOAD_SIZE;
		tmp->recv.cur_offset = 0;
		tmp->recv.cur_size = tmp->recv.max_size;

		active_ifd = accept(ifd, (struct sockaddr *)&addr, (socklen_t*)&addr_size);
        if (active_ifd < 0)
        {
        	active_ifd = INVALID_SOCKET;
            FREE(tmp);
            log_dbg("Can`t accept connection\n");
        }
        else {
    		/* Check if it is exceeded internal limitations
    		 * MAX_FDS_NUM and MAX_ACTIVE_FD_NUM
    		 */
    		if ( (active_ifd < MAX_FDS_NUM) &&
        	     (g_fds_array[ifd]->active_fd_count < (MAX_ACTIVE_FD_NUM - 1)) ) {
            	if (prepare_socket(tmp, active_ifd) != INVALID_SOCKET) {
        			int *active_fd_list = g_fds_array[ifd]->active_fd_list;
        			int i = 0;

        			for (i = 0; i < MAX_ACTIVE_FD_NUM; i++) {
        				if (active_fd_list[i] == INVALID_SOCKET) {
        					active_fd_list[i] = active_ifd;
        					g_fds_array[ifd]->active_fd_count++;
        					g_fds_array[active_ifd] = tmp;

        					log_dbg ("peer address to accept: %s:%d [%d]", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), active_ifd);

#if defined(LOG_TRACE_CONNECT) && (LOG_TRACE_CONNECT==TRUE)
        					{
        						printf("0[%d] :", active_ifd);
        						for (int _k = 3; _k < _DBG_FDS_NUM; _k++) printf("%9d ", _dbg_fds[_k]);
        						printf("\n");
        					}
#endif /* LOG_TRACE_CONNECT */
           					do_accept = true;
           					break;
        				}
        			}
            	}
        	}

        	if (!do_accept) {
        		close(active_ifd);
        		active_ifd = INVALID_SOCKET;
        		FREE(tmp);
                log_dbg ("peer address to refuse: %s:%d [%d]", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), active_ifd);
        	}
        }
	}

	return active_ifd;
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
	int nbytes = 0;

	ret = msg_recvfrom(ifd,
			           g_fds_array[ifd]->recv.cur_addr + g_fds_array[ifd]->recv.cur_offset,
			           g_fds_array[ifd]->recv.cur_size,
			           &recvfrom_addr);

#if defined(LOG_TRACE_CONNECT) && (LOG_TRACE_CONNECT==TRUE)
	if (ifd < _DBG_FDS_NUM) _dbg_fds[ifd]++;
#endif /* LOG_TRACE_CONNECT */

	if (ret == 0) {
		if (g_fds_array[ifd]->sock_type == SOCK_STREAM) {
			int next_fd = g_fds_array[ifd]->next_fd;
			int i = 0;

			for (i = 0; i < MAX_ACTIVE_FD_NUM; i++) {
				if (g_fds_array[next_fd]->active_fd_list[i] == ifd) {
	 			log_dbg ("peer address to close: %s:%d [%d]", inet_ntoa(g_fds_array[ifd]->addr.sin_addr), ntohs(g_fds_array[ifd]->addr.sin_port), ifd);

					close(ifd);
					g_fds_array[next_fd]->active_fd_count--;
					g_fds_array[next_fd]->active_fd_list[i] = INVALID_SOCKET;
					FREE(g_fds_array[ifd]);
					g_fds_array[ifd] = NULL;

#if defined(LOG_TRACE_CONNECT) && (LOG_TRACE_CONNECT==TRUE)
		            {
		        		printf("X[%d] :", ifd);
		            	for (int _k = 3; _k < _DBG_FDS_NUM; _k++) printf("%9d ", _dbg_fds[_k]);
		        		if (ifd < _DBG_FDS_NUM) _dbg_fds[ifd] = 0;
		            	printf("\n");
		            }
#endif /* LOG_TRACE_CONNECT */
					break;
				}
			}
		}
		return (do_update);
	}
	if (ret < 0) return (!do_update);

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
			return (!do_update);
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
			return (!do_update);
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

		if (g_b_exit) return (!do_update);
		if (!m_pMsgReply->isClient()) {
			/* 6: shift to start of cycle buffer in case receiving buffer is empty and
			 * there is no uncompleted message
			 */
			if (!nbytes) {
				g_fds_array[ifd]->recv.cur_addr = g_fds_array[ifd]->recv.buf;
				g_fds_array[ifd]->recv.cur_size = g_fds_array[ifd]->recv.max_size;
				g_fds_array[ifd]->recv.cur_offset = 0;
			}
			return (!do_update);
		}

		if (m_pMsgReply->isWarmupMessage()) {
			m_switchCalcGaps.execute(&recvfrom_addr, 0, true);
			/* 6: shift to start of cycle buffer in case receiving buffer is empty and
			 * there is no uncompleted message
			 */
			if (!nbytes) {
				g_fds_array[ifd]->recv.cur_addr = g_fds_array[ifd]->recv.buf;
				g_fds_array[ifd]->recv.cur_size = g_fds_array[ifd]->recv.max_size;
				g_fds_array[ifd]->recv.cur_offset = 0;
			}
			return (!do_update);
		}

		g_receiveCount++; //// should move to setRxTime (once we use it in server side)

		if (m_pMsgReply->getHeader()->isPongRequest()) {
			/* prepare message header */
			if (g_pApp->m_const_params.mode != MODE_BRIDGE) {
				m_pMsgReply->setServer();
			}
			/* get source addr to reply. memcpy is not used to improve performance */
			sendto_addr = g_fds_array[ifd]->addr;
			if (!g_fds_array[ifd]->is_multicast || g_pApp->m_const_params.b_server_reply_via_uc) {// In unicast case reply to sender
				/* get source addr to reply. memcpy is not used to improve performance */
				sendto_addr = recvfrom_addr;
			}
			ret = msg_sendto(ifd, m_pMsgReply->getBuf(), m_pMsgReply->getLength(), &sendto_addr);
			if (ret == 0) {
				if (g_fds_array[ifd]->sock_type == SOCK_STREAM) {
					int next_fd = g_fds_array[ifd]->next_fd;
					int i = 0;

					for (i = 0; i < MAX_ACTIVE_FD_NUM; i++) {
						if (g_fds_array[next_fd]->active_fd_list[i] == ifd) {
							log_dbg ("peer address to close: %s:%d [%d]", inet_ntoa(g_fds_array[ifd]->addr.sin_addr), ntohs(g_fds_array[ifd]->addr.sin_port), ifd);

							close(ifd);
							g_fds_array[next_fd]->active_fd_count--;
							g_fds_array[next_fd]->active_fd_list[i] = INVALID_SOCKET;
							FREE(g_fds_array[ifd]);
							g_fds_array[ifd] = NULL;

#if defined(LOG_TRACE_CONNECT) && (LOG_TRACE_CONNECT==TRUE)
				            {
				        		printf("X[%d] :", ifd);
				            	for (int _k = 3; _k < _DBG_FDS_NUM; _k++) printf("%9d ", _dbg_fds[_k]);
				        		if (ifd < _DBG_FDS_NUM) _dbg_fds[ifd] = 0;
				            	printf("\n");
				            }
#endif /* LOG_TRACE_CONNECT */
							break;
						}
					}
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
	if (!nbytes) {
		g_fds_array[ifd]->recv.cur_addr = g_fds_array[ifd]->recv.buf;
		g_fds_array[ifd]->recv.cur_size = g_fds_array[ifd]->recv.max_size;
		g_fds_array[ifd]->recv.cur_offset = 0;
	}

	return (!do_update);
}

#endif /* SERVER_H_ */
