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
#ifndef IOHANDLERS_H_
#define IOHANDLERS_H_
#include "common.h"

//==============================================================================
class IoHandler {
public:
	IoHandler(int _fd_min, int _fd_max, int _fd_num, int _look_start, int _look_end);
	virtual ~IoHandler();

	inline int get_look_start() const {return m_look_start;}
	inline int get_look_end() const {return m_look_end;}

	virtual void prepareNetwork() = 0;
	void warmup() const;

	const int m_fd_min, m_fd_max, m_fd_num;
protected:
	const int m_look_start;
	int m_look_end; //non const because of epoll
};

//==============================================================================
class IoRecvfrom: public IoHandler {
public:
	IoRecvfrom(int _fd_min, int _fd_max, int _fd_num);
	virtual ~IoRecvfrom();

	inline int waitArrival() {return 1;}
	inline int analyzeArrival(int ifd) const {return ifd;}

	virtual void prepareNetwork();
};

//==============================================================================
class IoSelect: public IoHandler {
public:
	IoSelect(int _fd_min, int _fd_max, int _fd_num);
	virtual ~IoSelect();

	//------------------------------------------------------------------------------
	inline int waitArrival() {

		if (mp_timeout_timeval) {
			memcpy(mp_timeout_timeval, g_pApp->m_const_params.select_timeout, sizeof(struct timeval));
		}
		memcpy(&m_readfds, &m_save_fds, sizeof(fd_set));
		return select(m_fd_max+1, &m_readfds, NULL, NULL, mp_timeout_timeval);
	}

	//------------------------------------------------------------------------------
	inline int analyzeArrival(int ifd) const {
		return FD_ISSET(ifd, &m_readfds) ? ifd : 0;
	}

	virtual void prepareNetwork();
private:
	struct timeval m_timeout_timeval;
	struct timeval* const mp_timeout_timeval;
	fd_set m_readfds, m_save_fds;
};

//==============================================================================
class IoPoll: public IoHandler {
public:
	IoPoll(int _fd_min, int _fd_max, int _fd_num);
	IoPoll(const IoPoll&);
	virtual ~IoPoll();

	virtual void prepareNetwork();

	//------------------------------------------------------------------------------
	inline int waitArrival(){
		return poll(mp_poll_fd_arr, m_fd_num, m_timeout_msec);
	}

	//------------------------------------------------------------------------------
	inline int analyzeArrival(int ifd) const{
		if ( mp_poll_fd_arr[ifd].revents & POLLIN || mp_poll_fd_arr[ifd].revents & POLLPRI) {
			return mp_poll_fd_arr[ifd].fd;
		}
		else {
			return 0;
		}
	}
private:
	const int m_timeout_msec;
	struct pollfd *mp_poll_fd_arr;
};


//==============================================================================
class IoEpoll: public IoHandler {
public:
	IoEpoll(int _fd_min, int _fd_max, int _fd_num);
	virtual ~IoEpoll();

	//------------------------------------------------------------------------------
	inline int waitArrival(){
		m_look_end = epoll_wait(m_epfd, mp_epoll_events, m_fd_num, m_timeout_msec);
		return m_look_end;
	}
	//------------------------------------------------------------------------------
	inline int analyzeArrival(int ifd) const {
		return mp_epoll_events[ifd].data.fd;
	}

	virtual void prepareNetwork();
private:
	const int m_timeout_msec;
	struct epoll_event *mp_epoll_events;
	int m_epfd;
};

#endif /* IOHANDLERS_H_ */
