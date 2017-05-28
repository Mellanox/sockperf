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
#ifndef __INC_COMMON_H__
#define __INC_COMMON_H__

#include "Defs.h"
//#include "Switches.h"
#include "Message.h"

//------------------------------------------------------------------------------
void recvfromError(int fd);//take out error code from inline function
void sendtoError(int fd, int nbytes, const struct sockaddr_in *sendto_addr);//take out error code from inline function
void exit_with_log(int status);
void exit_with_log(const char* error,int status);
void exit_with_log(const char* error, int status, fds_data* fds);
void exit_with_err(const char* error, int status);
void print_log_dbg(struct in_addr sin_addr,in_port_t sin_port, int ifd);

int set_affinity_list(os_thread_t thread, const char * cpu_list);
void hexdump(void *ptr, int buflen);
const char* handler2str( fd_block_handler_t type );
int read_int_from_sys_file(const char *path);

//inline functions
//------------------------------------------------------------------------------
static inline int msg_recvfrom(int fd, uint8_t* buf, int nbytes, struct sockaddr_in *recvfrom_addr)
{
	int ret = 0;
	socklen_t size = sizeof(struct sockaddr_in);
	int flags = 0;

#ifdef USING_VMA_EXTRA_API
	int remain_buffer, data_to_copy;
	uint8_t* start_addrs;
	struct vma_packet_t *pkt;

	if (g_pApp->m_const_params.is_vmazcopyread && g_vma_api) {
		remain_buffer = nbytes;
		// Receive held data, and free VMA's previously received zero copied packets
		if (get_zero_data()->pkts && get_zero_data()->pkts->n_packet_num > 0) {

			pkt = &get_zero_data()->pkts->pkts[0];

			while (get_zero_data()->pkt_index < pkt->sz_iov) {
				start_addrs = buf + (nbytes - remain_buffer);
				data_to_copy = _min(remain_buffer,
						(int )(pkt->iov[get_zero_data()->pkt_index].iov_len
								- get_zero_data()->pkt_offset));
				memcpy(start_addrs,
						(uint8_t*)pkt->iov[get_zero_data()->pkt_index].iov_base
								+ get_zero_data()->pkt_offset, data_to_copy);
				remain_buffer -= data_to_copy;
				get_zero_data()->pkt_offset += data_to_copy;

				//Handled buffer is filled
				if (get_zero_data()->pkt_offset <
						pkt->iov[get_zero_data()->pkt_index].iov_len)
					return nbytes;

				get_zero_data()->pkt_offset = 0;
				get_zero_data()->pkt_index++;
			}

			g_vma_api->free_packets(fd, get_zero_data()->pkts->pkts,
									get_zero_data()->pkts->n_packet_num);
			get_zero_data()->pkts = NULL;
			get_zero_data()->pkt_index = 0;
			get_zero_data()->pkt_offset = 0;

			//Handled buffer is filled
			if (remain_buffer == 0)
				return nbytes;
		}

		// Receive the next packet with zero copy API
		ret = g_vma_api->recvfrom_zcopy(fd, get_zero_data()->pkt_buf,
										Message::getMaxSize(), &flags,
										(struct sockaddr*)recvfrom_addr,
										&size);

		if (ret > 0) {
			// Zcopy receive is perfomed
			if (flags & MSG_VMA_ZCOPY) {
				get_zero_data()->pkts =
						(struct vma_packets_t*)get_zero_data()->pkt_buf;
				if (get_zero_data()->pkts->n_packet_num > 0) {
					pkt = &get_zero_data()->pkts->pkts[0];
					while (get_zero_data()->pkt_index < pkt->sz_iov) {
						start_addrs = buf + (nbytes - remain_buffer);
						data_to_copy =
								_min(remain_buffer,
									(int )pkt->iov[get_zero_data()->pkt_index].iov_len);
						memcpy(start_addrs,
								pkt->iov[get_zero_data()->pkt_index].iov_base,
								data_to_copy);
						remain_buffer -= data_to_copy;
						get_zero_data()->pkt_offset += data_to_copy;

						//Handled buffer is filled
						if (get_zero_data()->pkt_offset <
								pkt->iov[get_zero_data()->pkt_index].iov_len)
							return nbytes;

						get_zero_data()->pkt_offset = 0;
						get_zero_data()->pkt_index++;
					}
					ret = nbytes - remain_buffer;
				} else {
					ret = (remain_buffer == nbytes) ?
							-1 : (nbytes - remain_buffer);
				}
			} else {
				data_to_copy = _min(remain_buffer, ret);
				memcpy(buf + (nbytes - remain_buffer), get_zero_data()->pkt_buf,
						data_to_copy);
				ret = nbytes - (remain_buffer - data_to_copy);
			}
		}
		// Non_blocked with held packet.
		else if (ret < 0 && os_err_eagain() && (remain_buffer < nbytes)) {
			return nbytes - remain_buffer;
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

		ret = recvfrom(fd, buf, nbytes, flags, (struct sockaddr*)recvfrom_addr, &size);

#if defined(LOG_TRACE_MSG_IN) && (LOG_TRACE_MSG_IN==TRUE)
		printf(">   ");
		hexdump(buf, MsgHeader::EFFECTIVE_SIZE);
#endif /* LOG_TRACE_MSG_IN */

#if defined(LOG_TRACE_RECV) && (LOG_TRACE_RECV==TRUE)
		LOG_TRACE ("raw", "%s IP: %s:%d [fd=%d ret=%d] %s", __FUNCTION__,
					                   inet_ntoa(recvfrom_addr->sin_addr),
					                   ntohs(recvfrom_addr->sin_port),
					                   fd,
					                   ret,
					                   strerror(errno));
#endif /* LOG_TRACE_RECV */
	}

	if (ret == 0 || errno == EPIPE || os_err_conn_reset()) {
		/* If no messages are available to be received and the peer has performed an orderly shutdown,
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

inline SOCKPERF_ERROR vma_buffer_init()
{
#ifdef USING_VMA_EXTRA_API
	if (g_vma_api) {
		g_zero_data.pkt_buf = (unsigned char*)MALLOC(Message::getMaxSize());
		if (g_zero_data.pkt_buf == NULL) {
			log_err("Failed to allocate g_pkt_buf");
			return SOCKPERF_ERR_NO_MEMORY;;
		}
	}
#endif
	return SOCKPERF_ERR_NONE;
}

inline void vma_buffer_free()
{
#ifdef USING_VMA_EXTRA_API
	if (g_zero_data.pkt_buf) {
		FREE(g_zero_data.pkt_buf);
	}
#endif
}

//------------------------------------------------------------------------------
static inline int msg_sendto(int fd, uint8_t* buf, int nbytes, const struct sockaddr_in *sendto_addr)
{
	int ret = 0;
    int flags = 0;

#if defined(LOG_TRACE_MSG_OUT) && (LOG_TRACE_MSG_OUT==TRUE)
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
		ret = sendto(fd, buf, nbytes, flags, (struct sockaddr*)sendto_addr, sizeof(struct sockaddr));

#if defined(LOG_TRACE_SEND) && (LOG_TRACE_SEND==TRUE)
		LOG_TRACE ("raw", "%s IP: %s:%d [fd=%d ret=%d] %s", __FUNCTION__,
								   inet_ntoa(sendto_addr->sin_addr),
								   ntohs(sendto_addr->sin_port),
								   fd,
								   ret,
								   strerror(errno));
#endif /* LOG_TRACE_SEND */

		if (ret > 0) {
			nbytes -= ret;
			buf += ret;
			ret = size;
		}
		else if (ret == 0 || errno == EPIPE || os_err_conn_reset()) {
			/* If no messages are available to be received and the peer has performed an orderly shutdown,
			 * send()/sendto() shall return (RET_SOCKET_SHUTDOWN)
			 */
			errno = 0;
			ret = RET_SOCKET_SHUTDOWN;
			break;
		}
		else if (ret < 0 && (os_err_eagain() || errno == EWOULDBLOCK)) {
			/* If space is not available at the sending socket to hold the message to be transmitted and
			 * the socket file descriptor does have O_NONBLOCK set and
			 * no bytes related message sent before
			 * send()/sendto() shall return (RET_SOCKET_SKIPPED)
			 */
			errno = 0;
			if (nbytes < size) continue;

			ret = RET_SOCKET_SKIPPED;
			break;
		}
		else if (ret < 0 && (errno == EINTR)) {
			/* A signal occurred.
			 */
			errno = 0;
			break;
		}
		else {
			/* Unprocessed error
			 */
			sendtoError(fd, nbytes, sendto_addr);
			errno = 0;
			break;
		}
	}

	return ret;
}


#endif // ! __INC_COMMON_H__
