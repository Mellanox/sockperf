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
#include "Message.h"

//------------------------------------------------------------------------------
void recvfromError(int fd);//take out error code from inline function
void sendtoError(int fd, int nbytes, const struct sockaddr_in *sendto_addr);//take out error code from inline function
void printf_backtrace(void);
pid_t gettid(void);
void exit_with_log(int status);

void set_affinity(pthread_t tid, int cpu);

//inline functions
//------------------------------------------------------------------------------
static inline int msg_recvfrom(int fd, struct sockaddr_in *recvfrom_addr)
{
	int ret = 0;
	socklen_t size = sizeof(struct sockaddr);

#ifdef  USING_VMA_EXTRA_API

	if (g_pApp->m_const_params.is_vmazcopyread && g_vma_api) {
		int flags = 0;

		// Free VMA's previously received zero copied datagram
		if (g_dgram) {
			g_vma_api->free_datagrams(fd, &g_dgram->datagram_id, 1);
			g_dgram = NULL;
		}

		// Receive the next datagram with zero copy API
		ret = g_vma_api->recvfrom_zcopy(fd, g_dgram_buf, Message::getMaxSize(),
		                                  &flags, (struct sockaddr*)recvfrom_addr, &size);
		if (ret >= MsgHeader::EFFECTIVE_SIZE) {
			if (flags & MSG_VMA_ZCOPY) {
				// zcopy
				g_dgram = (struct vma_datagram_t*)g_dgram_buf;

				// copy signature
				memcpy(g_pReply->getData(), g_dgram->iov[0].iov_base, MsgHeader::EFFECTIVE_SIZE);
			}
			else {
				// copy signature
				memcpy(g_pReply->getData(), g_dgram_buf, MsgHeader::EFFECTIVE_SIZE);
			}
		}
	}
	else
#endif
	{
		//ret = recvfrom(fd, g_pReply->getData(), g_msg_size, 0, (struct sockaddr*)recvfrom_addr, &size);
		ret = recvfrom(fd, g_pReply->getData(), MAX_PAYLOAD_SIZE, 0, (struct sockaddr*)recvfrom_addr, &size);
	}

	if (ret < MsgHeader::EFFECTIVE_SIZE && errno != EAGAIN && errno != EINTR) {
		recvfromError(fd);
	}

	return ret;
}

//------------------------------------------------------------------------------
static inline int msg_sendto(int fd, uint8_t* buf, int nbytes, const struct sockaddr_in *sendto_addr)
{
	int ret = sendto(fd, buf, nbytes, 0, (struct sockaddr*)sendto_addr, sizeof(struct sockaddr));
	if (ret < 0 && errno && errno != EINTR) {
		sendtoError(fd, nbytes, sendto_addr);
	}
	return ret;
}


#endif // ! __INC_COMMON_H__
