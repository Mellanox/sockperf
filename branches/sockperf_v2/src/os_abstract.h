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
/**
 * @file os_abstract.h
 *
 * @details: OS abstract file compatible for both Linux and Windows.
 *
 * @author  Meny Yossefi <menyy@mellanox.com>
 *  reviewed by Avner Ben Hanoch <avnerb@mellanox.com>
 *
 **/
#ifndef _OS_ABSTRACT_H_
#define _OS_ABSTRACT_H_

#include <time.h>		/* clock_gettime()*/
#include <sys/types.h>		/* sockets*/
#include "ticks_os.h"


/***********************************************************************************
*				 WIN32
***********************************************************************************/

#ifdef WIN32 

#include <Dbghelp.h>		// backtrace
#include <signal.h>

#ifdef _M_IX86
	#define PRIu64			"llu"
	#define PRId64			"lld"
#elif defined _M_X64
	#define PRIu64			"lu"
	#define PRId64			"ld"
#endif

#define		sleep(x)		Sleep (x*1000)
#define		close(x)		CloseHandle(&x)

#define		IP_MAX_MEMBERSHIPS	20	//ported from Linux
#define		MAX_OPEN_FILES     	65535
#define		_SECOND			10000000 //timer (SetWaitableTimer)
#define		SIGALRM			999

#define		__CPU_SETSIZE		1024	 //ported from Linux
#define		__NCPUBITS		(8 * sizeof (__cpu_mask)) //ported from Linux
#define		CPU_SETSIZE		__CPU_SETSIZE
#define		__func__		__FUNCTION__

//Socket api
#define 	inet_aton(x,y)		inet_pton(AF_INET,x,y)
#define 	getsockopt(a,b,c,d,e)	getsockopt (a,b,c,(char *)d,e)
#define 	setsockopt(a,b,c,d,e)	setsockopt (a,b,c,(char *)d,e)
#define 	recvfrom(a,b,c,d,e,f)	recvfrom(a,(char *)b,c,d,e,f)
#define 	sendto(a,b,c,d,e,f)	sendto(a,(char *)b,c,d,e,f)

 /* Type of the second argument to `getitimer' and
   the second and third arguments `setitimer'.  */
struct itimerval
  {
    /* Value to put into `it_value' when the timer expires.  */
    struct timeval it_interval;
    /* Time to the next timer expiration.  */
    struct timeval it_value;
  };

void* win_set_timer(void *p_timer);

#else

/***********************************************************************************
*				Linux
***********************************************************************************/

#include <execinfo.h>  // for backtraces
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/resource.h>
#include <fcntl.h>

#define INVALID_SOCKET 		(-1)

#endif

/***********************************************************************************
*				Common
***********************************************************************************/

typedef struct os_thread_t {
#ifdef WIN32
	HANDLE hThread;
	DWORD tid;
#else
	pthread_t tid;
#endif
} os_thread_t;

typedef struct os_mutex_t {
#ifndef WIN32
	pthread_mutex_t mutex;
#else
	HANDLE mutex;
#endif
} os_mutex_t;

typedef struct os_cpuset_t {
#ifdef WIN32
	DWORD_PTR cpuset;
#else
	cpu_set_t cpuset;
#endif
} os_cpuset_t;


typedef void 		sig_handler(int signum);
void 			os_set_signal_action(int signum, sig_handler handler);
void			os_printf_backtrace(void);
int			os_set_nonblocking_socket(int fd);
int			os_daemonize();
int 			os_set_duration_timer(const itimerval &timer, sig_handler handler);
int			os_get_max_active_fds_num();
bool			os_sock_startup();
bool			os_sock_cleanup();

//Colors
#ifdef WIN32
	#define MAGNETA		""
	#define RED		""
	#define ENDCOLOR	""
#else
	#define MAGNETA		"\e[2;35m"
	#define RED		"\e[0;31m"
	#define ENDCOLOR	"\e[0m"
#endif

//Thread functions

void			os_thread_init (os_thread_t * thr);
void			os_thread_close (os_thread_t * thr);
void			os_thread_detach (os_thread_t * thr);
int			os_thread_exec (os_thread_t * thr, void *(*start) (void *), void *arg);
void			os_thread_kill (os_thread_t * thr);
void			os_thread_join (os_thread_t *thr);
os_thread_t		os_getthread(void);

//Mutex functions

void			os_mutex_init (os_mutex_t * lock);
void			os_mutex_close (os_mutex_t * lock);
void			os_mutex_lock (os_mutex_t * lock);
void			os_mutex_unlock (os_mutex_t * lock);

//CPUset functions

void 			os_init_cpuset(os_cpuset_t *_mycpuset);
void			os_cpu_set(os_cpuset_t *_mycpuset, long _cpu_from, long _cpu_cur);
int			os_set_affinity(const os_thread_t &thread, const os_cpuset_t &_mycpuset);


// ERRORS

inline bool os_err_in_progress()
{
#ifdef WIN32
	// In Windows it's WSAEINPROGRESS for blocking sockets and WSAEWOULDBLOCK for non-vlocking sockets
	return (WSAGetLastError() == WSAEWOULDBLOCK || WSAGetLastError() == WSAEINPROGRESS);
#else
	return (errno == EINPROGRESS);
#endif
}

inline bool os_err_eagain()
{
#ifdef WIN32
	return (WSAGetLastError() == WSAEWOULDBLOCK);
#else
	return (errno == EAGAIN);
#endif
}

inline bool os_err_conn_reset()
{
#ifdef WIN32
	return (WSAGetLastError() == WSAECONNRESET);
#else
	return (errno == ECONNRESET);
#endif
}

#ifdef WIN32
#define _max(x,y)	max(x,y)
#define _min(x,y)	min(x,y)
#else
#define _max(x,y)	({typeof(x) _x = (x); typeof(y) _y = (y); (void)(&_x == &_y); _x > _y ? _x : _y; })
#define _min(x,y)	({typeof(x) _x = (x); typeof(y) _y = (y); (void)(&_x == &_y); _x < _y ? _x : _y; })
#endif


#endif /*_OS_ABSTRACT_H_*/
