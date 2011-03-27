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

class IoHandler;

class ServerBase {
private:

//protected:
public:
	//------------------------------------------------------------------------------
	ServerBase(IoHandler & _ioHandler) : m_ioHandlerRef(_ioHandler){}
	virtual ~ServerBase(){}

	void doHandler(){initBeforeLoop(); doLoop(); cleanupAfterLoop();}

	void initBeforeLoop();
	void cleanupAfterLoop();
	virtual void doLoop() = 0; // don't implement doLoop here because we want the compiler to provide distinct
	                           // implementation with inlined functions of derived classes  in each derived class

protected:

	// Note: for static binding at compilation time, we use the
	// reference to IoHandler base class ONLY for accessing non-virtual functions
	IoHandler & m_ioHandlerRef;

	/*
	** Check that msg arrived from CLIENT and not a loop from a server
	** Return 0 for successful, -1 for failure
	*/
	/*inline*/ void server_prepare_msg_reply();
};


//------------------------------------------------------------------------------
/*
** Check that msg arrived from CLIENT and not a loop from a server
** Return 0 for successful, -1 for failure
*/
inline void ServerBase::server_prepare_msg_reply()
{
	if (g_pApp->m_const_params.mode != MODE_BRIDGE) {
		g_pReply->setServer();
		//g_pReply->getHeader()->setPongRequest(false);
	}
}

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
	/*inline*/ void server_receive_then_send(int ifd);

	//------------------------------------------------------------------------------
private:
	SwitchActivityInfo m_switchActivityInfo;
	SwitchCalcGaps m_switchCalcGaps;
};

//------------------------------------------------------------------------------
/*
** receive from and send to selected socket
*/
template <class IoType, class SwitchActivityInfo, class SwitchCalcGaps>
inline void Server<IoType, SwitchActivityInfo, SwitchCalcGaps>::server_receive_then_send(int ifd)
{
	int nbytes;
	struct sockaddr_in recvfrom_addr;
	struct sockaddr_in sendto_addr;

	nbytes = msg_recvfrom(ifd, &recvfrom_addr);
	if (g_b_exit) return;
	if (nbytes < 0) return;
	if (! g_pReply->isClient()) return;

	if (g_pReply->isWarmupMessage()) {
		m_switchCalcGaps.execute(&recvfrom_addr, 0, true);
		return;
	}

	if (g_pReply->getHeader()->isPongRequest()) {
		server_prepare_msg_reply();
		// get source addr to reply to
		sendto_addr = g_fds_array[ifd]->addr;
		if (!g_fds_array[ifd]->is_multicast || g_pApp->m_const_params.b_server_reply_via_uc) {// In unicast case reply to sender
			sendto_addr.sin_addr = recvfrom_addr.sin_addr;
		}
		msg_sendto(ifd, g_pReply->getData(), nbytes, &sendto_addr);
		//log_msg("%8lu: --> sent pong reply mask=0x%x", g_pReply->getSequenceCounter(), g_pReply->getFlags());
	}

	g_receiveCount++; //// should move to setRxTime (once we use it in server side)
	m_switchCalcGaps.execute(&recvfrom_addr, g_pReply->getSequenceCounter(), false);
	m_switchActivityInfo.execute(g_receiveCount);
	return;
}

#endif /* SERVER_H_ */
