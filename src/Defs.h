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
#ifndef DEFS_H_
#define DEFS_H_

#include <features.h>
#if  (__GLIBC__ > 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 2)
#include <bits/types.h>
#undef __FD_SETSIZE
#define __FD_SETSIZE 32768
#endif

#include "vma-redirect.h"

#include <stdbool.h>
#include <stdint.h>
#include <tr1/unordered_map>
#include <stdlib.h>		/* random()*/
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <signal.h>
#include <time.h>		/* clock_gettime()*/
#include <unistd.h>		/* getopt() and sleep()*/
#define __STDC_FORMAT_MACROS
#include <inttypes.h>	/* printf PRItn */
#include <ctype.h>		/* isprint()*/
#include <regex.h>
#include <poll.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/types.h>		/* sockets*/
#include <sys/time.h>		/* timers*/
#include <sys/socket.h>		/* sockets*/
#include <sys/select.h>		/* select() According to POSIX 1003.1-2001 */
#include <sys/epoll.h>
#include <sys/syscall.h>
#include <arpa/inet.h>		/* internet address manipulation */
#include <netinet/in.h>		/* internet address manipulation */
#include <netdb.h>			/* gethostbyname() */
#include <netinet/tcp.h>	/* tcp specific */
#include "Ticks.h"
#include "Message.h"
#include "Playback.h"
#include <sys/resource.h>


//#define USING_VMA_EXTRA_API
#ifdef  USING_VMA_EXTRA_API
#include <voltaire/vma_extra.h>
#endif

#define MIN_PAYLOAD_SIZE        	(MsgHeader::EFFECTIVE_SIZE)
extern int MAX_PAYLOAD_SIZE;
#define MAX_STREAM_SIZE         	(50*1024*1024)
#define MAX_TCP_SIZE				((1<<20)-1)

const uint32_t MPS_MAX_UL          = 10*1000*1000; //  10 M MPS is 4 times the maximum possible under VMA today
const uint32_t MPS_MAX_PP          = 400*1000;     // 400 K MPS for ping-pong will be break only when we reach RTT of 2.5 usec
extern uint32_t MPS_MAX;

const uint32_t MPS_DEFAULT         = 10*1000;
const uint32_t REPLY_EVERY_DEFAULT = 100;

const uint32_t TEST_START_WARMUP_MSEC = 50;
const uint32_t TEST_END_COOLDOWN_MSEC = 50;


#define DEFAULT_TEST_DURATION		1	/* [sec] */
#define DEFAULT_MC_ADDR				"0.0.0.0"
#define DEFAULT_PORT				11111
#define DEFAULT_IP_MTU 				1500
#define DEFAULT_IP_PAYLOAD_SZ 		(DEFAULT_IP_MTU-28)
#define DUMMY_PORT					57341
#define MAX_ACTIVE_FD_NUM			1024 /* maximum number of active connection to the single TCP addr:port */

#ifndef MAX_PATH_LENGTH
#define MAX_PATH_LENGTH         	1024
#endif
#define MAX_MCFILE_LINE_LENGTH  	25	/* sizeof("U:255.255.255.255:11111\0") */

#define IP_PORT_FORMAT_REG_EXP		"^([UuTt]:)*([a-zA-Z0-9\\.\\-]+):(6553[0-5]|655[0-2][0-9]|65[0-4][0-9]{2}|6[0-4][0-9]{3}|[0-5]?[0-9]{1,4})[\r\n]"
/*
#define IP_PORT_FORMAT_REG_EXP		"^([UuTt]:)*((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}"\
					"(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?):"\
					"(6553[0-5]|655[0-2][0-9]|65[0-4][0-9]{2}|6[0-4][0-9]{3}|[0-5]?[0-9]{1,4})\n"
*/
#define PRINT_PROTOCOL(type)	((type) == SOCK_DGRAM ? "UDP" : ((type) == SOCK_STREAM ? "TCP" : "<?>"))

#define MAX_ARGV_SIZE				256
#define MAX_DURATION 				36000000
extern const int MAX_FDS_NUM;
#define SOCK_BUFF_DEFAULT_SIZE 		0
#define DEFAULT_SELECT_TIMEOUT_MSEC	10
#define DEFAULT_DEBUG_LEVEL			0
#define INVALID_SOCKET 				(-1)

enum {
	OPT_RX_MC_IF 			 = 1,
	OPT_TX_MC_IF,			// 2
	OPT_SELECT_TIMEOUT,		// 3
	OPT_THREADS_NUM, 	// 4
	OPT_CLIENT_CYCLE_DURATION,	// 5
	OPT_BUFFER_SIZE,		// 6
	OPT_DATA_INTEGRITY, 		// 7
	OPT_DAEMONIZE,			// 8
	OPT_NONBLOCKED,			// 9
	OPT_DONTWARMUP,			//10
	OPT_PREWARMUPWAIT,		//11
	OPT_VMARXFILTERCB,		//12
	OPT_VMAZCOPYREAD,            //13
	OPT_MC_LOOPBACK_ENABLE,     //14
	OPT_CLIENT_WORK_WITH_SRV_NUM,//15
	OPT_FORCE_UC_REPLY,          //16
	OPT_MPS,                     //17
	OPT_REPLY_EVERY,             //18
	OPT_NO_RDTSC,                //20
	OPT_SENDER_AFFINITY,         //21
	OPT_RECEIVER_AFFINITY,       //22
	OPT_LOAD_VMA,                //23
	OPT_FULL_LOG,                //24
	OPT_GIGA_SIZE,				//25
	OPT_PLAYBACK_DATA,                  //26
	OPT_TCP,					//27
	OPT_TCP_NODELAY_OFF,		//28
	OPT_NONBLOCKED_SEND,		//29
	OPT_IP_MULTICAST_TTL,			//30
	OPT_SOCK_ACCL,				//31
	OPT_THREADS_AFFINITY,				//32
	OPT_DONT_REPLY,		//33
	OPT_RECV_LOOPING,	//34
	OPT_OUTPUT_PRECISION, //35
	OPT_CLIENTPORT,			//36
};

#define MODULE_NAME			"sockperf"
#define MODULE_COPYRIGHT	"Copyright (C) 2011 Mellanox Technologies Ltd." \
	"\nSockPerf is open source software, see http://sockperf.googlecode.com/"
#define log_msg(log_fmt, log_args...)	printf(MODULE_NAME ": " log_fmt "\n", ##log_args)
#define log_msg_file(file, log_fmt, log_args...)	fprintf(file, MODULE_NAME ": " log_fmt "\n", ##log_args)
#define log_msg_file2(file, log_fmt, log_args...)	if (1) {log_msg(log_fmt, ##log_args); if (file) log_msg_file(file, log_fmt, ##log_args);} else

#define log_err(log_fmt, log_args...)	printf("ERROR: " log_fmt " (errno=%d %s)\n", ##log_args, errno, strerror(errno))
#ifdef DEBUG
#undef log_err
#define log_err(log_fmt, log_args...)	printf(MODULE_NAME ": " "%s:%d:ERROR: " log_fmt " (errno=%d %s)\n", __FILE__, __LINE__, ##log_args, errno, strerror(errno))
#endif
#define log_dbg(log_fmt, log_args...)	if (g_debug_level >= LOG_LVL_DEBUG) { printf(MODULE_NAME ": " log_fmt "\n", ##log_args); } else

#define TRACE(msg) log_msg("TRACE <%s>: %s() %s:%d\n", msg, __func__, __FILE__, __LINE__)
#define ERROR(msg) if (1) {TRACE(msg); exit_with_log (-1);} else

#define QUOTE(name) #name
#define STR(macro) QUOTE(macro)

typedef enum {
	LOG_LVL_INFO = 0,
	LOG_LVL_DEBUG
} debug_level_t;


#ifndef TRUE
#define TRUE (1 == 1)
#endif /* TRUE */

#ifndef FALSE
#define FALSE (1 == 0)
#endif /* FALSE */


/* This macros should be used in printf to display uintptr_t */
#ifndef PRIXPTR
#define PRIXPTR    "lX"
#endif

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) ((void)P)
#endif


#define RET_SOCKET_SKIPPED		(-2)   	/**< socket operation is skipped */
#define RET_SOCKET_SHUTDOWN		0		/**< socket is shutdown */


/**
 * @enum SOCKPERF_ERROR
 * @brief List of supported error codes.
 */
typedef enum
{
	SOCKPERF_ERR_NONE  =  0x0,  	/**< the function completed */
	SOCKPERF_ERR_BAD_ARGUMENT,  	/**< incorrect parameter */
	SOCKPERF_ERR_INCORRECT,     	/**< incorrect format of object */
	SOCKPERF_ERR_UNSUPPORTED,   	/**< this function is not supported */
	SOCKPERF_ERR_NOT_EXIST,     	/**< requested object does not exist */
	SOCKPERF_ERR_NO_MEMORY,     	/**< dynamic memory error */
	SOCKPERF_ERR_FATAL,         	/**< system fatal error */
	SOCKPERF_ERR_SOCKET,        	/**< socket operation error */
	SOCKPERF_ERR_TIMEOUT,       	/**< the time limit expires */
	SOCKPERF_ERR_UNKNOWN        	/**< general error */
} SOCKPERF_ERROR;


/*
 *  Debug configuration settings
 */
#ifdef DEBUG
	#define LOG_TRACE_SEND		FALSE
	#define LOG_TRACE_RECV		FALSE
	#define LOG_TRACE_MSG_IN	FALSE
	#define LOG_TRACE_MSG_OUT	FALSE
	#define LOG_MEMORY_CHECK	FALSE
	#define LOG_LOCK_CHECK		FALSE

	#define LOG_TRACE(category, format, ...) \
		log_send(category, 1, __FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)

	inline void log_send( const char* name,
						  int priority,
						  const char* file_name,
						  const int line_no,
						  const char* func_name,
						  const char* format,
						  ...)
	{
		char buf[250];
		va_list va;
		int n = 0;

		if (priority) {
			va_start(va, format);
			n = vsnprintf(buf, sizeof(buf) - 1, format, va);
			va_end(va);

			printf("[%s] %s: %s <%s: %s #%d>\n",
				"debug",
				(name ? name : ""),
				buf,
				file_name,
				func_name,
				line_no
				);
		}
	}

	#if defined(LOG_MEMORY_CHECK) && (LOG_MEMORY_CHECK==TRUE)
		#define MALLOC(size) 	__malloc(size, __FUNCTION__, __LINE__)
		#define FREE(ptr) 		do {__free(ptr, __FUNCTION__, __LINE__); ptr = NULL;} while(0)
	#else
		#define MALLOC(size) 	malloc(size)
		#define FREE(ptr) 		do {free(ptr); ptr = NULL;} while(0)
	#endif /* LOG_MEMORY_CHECK */

	inline void *__malloc(int size, const char * func, int line)
	{
		void * ptr = malloc(size);
		printf ("malloc: %p (%d) <%s:%d>\n", ptr, size, func, line);
		return ptr;
	}

	inline void __free(void * ptr, const char * func, int line)
	{
		free(ptr);
		printf ("free  : %p <%s:%d>\n", ptr, func, line);
		return ;
	}
#else
	#define LOG_TRACE(category, format, ...)
	#define MALLOC(size) 	malloc(size)
	#define FREE(ptr) 		do {free(ptr); ptr = NULL;} while(0) // FREE is not expected to be called from fast path
#endif /* DEBUG */


/**
 * @name Synchronization
 * @brief Multi-threading Synchronization operations.
 */
/** @{ */
	#define CRITICAL_SECTION        pthread_mutex_t
	#if defined(_GNU_SOURCE)
		#define INIT_CRITICAL(x)    \
									{ \
										pthread_mutexattr_t attr; \
										pthread_mutexattr_init(&attr); \
										pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_DEFAULT); \
										pthread_mutex_init(x, &attr); \
										pthread_mutexattr_destroy(&attr); \
									}
	#else
		#define INIT_CRITICAL(x)    pthread_mutex_init(x, NULL)
	#endif
	#define DELETE_CRITICAL(x)      pthread_mutex_destroy(x)
	#define DBG_ENTER_CRITICAL(x)   pthread_mutex_lock(x)
	#define DBG_LEAVE_CRITICAL(x)   pthread_mutex_unlock(x)

    /* Debugging Multi-threading operations. */
	#if defined(LOG_LOCK_CHECK) && (LOG_LOCK_CHECK==TRUE)
        #define ENTER_CRITICAL(x)   \
                                        { \
                                        printf("lock = 0x%" PRIXPTR " <%s: %s #%d>\n", (uintptr_t)x, __FILE__, __FUNCTION__, __LINE__); \
                                        DBG_ENTER_CRITICAL(x); \
                                        }
        #define LEAVE_CRITICAL(x)   \
                                        { \
                                        printf("unlock = 0x%" PRIXPTR " <%s: %s #%d>\n", (uintptr_t)x, __FILE__, __FUNCTION__, __LINE__); \
                                        DBG_LEAVE_CRITICAL(x); \
                                        }
    #else
        #define ENTER_CRITICAL(x)       DBG_ENTER_CRITICAL(x)
        #define LEAVE_CRITICAL(x)       DBG_LEAVE_CRITICAL(x)
    #endif /* LOG_LOCK_CHECK */
/** @} */

/* Global variables */
extern bool g_b_exit;
extern uint64_t g_receiveCount;
extern uint64_t g_skipCount;

extern unsigned long long g_cycle_wait_loop_counter;
extern TicksTime g_cycleStartTime;

extern debug_level_t g_debug_level;

#ifdef  USING_VMA_EXTRA_API
extern unsigned char* g_dgram_buf;
extern struct vma_datagram_t *g_dgram;
#endif

class Message;

class PacketTimes;
extern PacketTimes* g_pPacketTimes;

extern TicksTime g_lastTicks;


typedef struct spike{
//   double usec;
   TicksDuration ticks;
   unsigned long long packet_counter_at_spike;
   int next;
 }spike;

typedef struct port_and_type{
	int sock_type;				/**< SOCK_STREAM (tcp), SOCK_DGRAM (udp), SOCK_RAW (ip) */
    in_port_t port;
}port_type;

/**
 * @struct fds_data
 * @brief Socket related info
 */
typedef struct fds_data {
	struct sockaddr_in addr;	/**< server address information */
	int is_multicast;			/**< if this socket is multicast */
	int sock_type;				/**< SOCK_STREAM (tcp), SOCK_DGRAM (udp), SOCK_RAW (ip) */
	int next_fd;
	int active_fd_count;		/**< number of active connections (by default 1-for UDP; 0-for TCP) */
	int *active_fd_list;		/**< list of fd related active connections (UDP has the same fd by default) */
	struct sockaddr_in memberships_addr[IP_MAX_MEMBERSHIPS];	/**< more servers on the same socket information */
	int memberships_size;	
	struct {
		uint8_t *buf;
		int max_size;
		uint8_t *cur_addr;
		int cur_offset;
		int cur_size;
	} recv;
} fds_data;


/**
 * @struct handler_info
 * @brief Handler configuration data
 */
typedef struct handler_info {
	int id;						/**< handler ID */
	int fd_min;					/**< minimum descriptor (fd) */
	int fd_max;					/**< maximum socket descriptor (fd) */
	int fd_num;					/**< number of socket descriptors */
}handler_info;

typedef struct clt_session_info {
	uint64_t seq_num;
	uint64_t total_drops;
	sockaddr_in addr;
	bool started;
}clt_session_info_t;

//The only types that TR1 has built-in hash/equal_to functions for, are scalar types,
//std:: string, and std::wstring. For any other type, we need to write a
//hash/equal_to functions, by ourself.
namespace std
{
	namespace tr1
	{
		template<>
		struct hash<struct sockaddr_in> : public std::unary_function<struct sockaddr_in, int>
		    {
			int operator()(struct sockaddr_in const &key) const
			{
				//XOR "a.b" part of "a.b.c.d" address with 16bit port; leave "c.d" part untouched for maximum hashing
				return key.sin_addr.s_addr ^ key.sin_port;
			}
		};

		template<>
		struct hash<struct port_and_type> : public std::unary_function<struct port_and_type, int>
		{
			int operator()(struct port_and_type const &key) const
			{
				//XOR "a.b" part of "a.b.c.d" address with 16bit port; leave "c.d" part untouched for maximum hashing
				return key.sock_type ^ key.port;
			}
		};

		template<>
		struct hash<struct in_addr> : public std::unary_function<struct in_addr, int>
		    {
			int operator()(struct in_addr const &key) const
			{
				return key.s_addr & 0xFF;
			}
		};
	}

	template<>
	struct equal_to<struct sockaddr_in>: public std::binary_function<struct sockaddr_in, struct sockaddr_in, bool>
	{
		bool operator()(struct sockaddr_in const &key1, struct sockaddr_in const &key2) const
		{
			return key1.sin_port == key2.sin_port && key1.sin_addr.s_addr == key2.sin_addr.s_addr;
		}
	};

	template<>
	struct equal_to<struct port_and_type>: public std::binary_function<struct port_and_type, struct port_and_type, bool>
		{
			bool operator()(struct port_and_type const &key1, struct port_and_type const &key2) const
			{
				return key1.sock_type == key2.sock_type && key1.port == key2.port;
			}
		};

	template<>
	struct equal_to<struct in_addr>: public std::binary_function<struct in_addr, struct in_addr, bool>
	{
		bool operator()(struct in_addr const &key1, struct in_addr const &key2) const
		{
			return key1.s_addr == key2.s_addr;
		}
	};
}

typedef std::tr1::unordered_map<struct sockaddr_in, clt_session_info_t> seq_num_map;
typedef std::tr1::unordered_map<struct in_addr, int> addr_to_id;

extern fds_data** g_fds_array;

#ifdef  USING_VMA_EXTRA_API
extern struct vma_api_t *g_vma_api;
#endif

#define max(x,y)	({typeof(x) _x = (x); typeof(y) _y = (y); (void)(&_x == &_y); _x > _y ? _x : _y; })
#define min(x,y)	({typeof(x) _x = (x); typeof(y) _y = (y); (void)(&_x == &_y); _x < _y ? _x : _y; })

typedef enum {
	MODE_CLIENT = 0,
	MODE_SERVER,
	MODE_BRIDGE
} work_mode_t;

typedef enum {
	RECVFROM = 0,
	SELECT,
	POLL,
	EPOLL,
	FD_HANDLE_MAX
} fd_block_handler_t;


struct user_params_t {
	work_mode_t mode; // either  client or server
	struct in_addr rx_mc_if_addr;
	struct in_addr tx_mc_if_addr;
	int msg_size;
	int msg_size_range;
	int sec_test_duration;
	bool data_integrity;
	fd_block_handler_t fd_handler_type;
	unsigned int  packetrate_stats_print_ratio;
	unsigned int  burst_size;
	bool packetrate_stats_print_details;
//	bool stream_mode; - use b_stream instead
	int mthread_server;
	struct timeval* select_timeout;
	int sock_buff_size;
	int threads_num;
	char threads_affinity[MAX_ARGV_SIZE];
	bool is_blocked;
	bool do_warmup;
	unsigned int pre_warmup_wait;
	bool is_vmarxfiltercb;
	bool is_vmazcopyread;
	TicksDuration cycleDuration;
	bool mc_loop_disable;
	int client_work_with_srv_num;
	bool b_server_reply_via_uc;
	bool b_server_dont_reply;
	bool b_server_detect_gaps;
	uint32_t mps; //client side only
	in_port_t client_port; //client side only
	uint32_t reply_every; //client side only
	bool b_client_ping_pong; //client side only
	bool b_no_rdtsc;
	char sender_affinity[MAX_ARGV_SIZE];
	char receiver_affinity[MAX_ARGV_SIZE];
	FILE* fileFullLog; //client side only
	bool giga_size; //client side only
	bool increase_output_precision; //client side only
	bool b_stream; //client side only
	PlaybackVector *pPlaybackVector; //client side only
	struct sockaddr_in addr;
	int sock_type;
	bool tcp_nodelay;
	bool is_nonblocked_send;
	int mc_ttl;
	int daemonize;
	char mcg_filename[MAX_PATH_LENGTH];
	bool withsock_accl;
	int max_looping_over_recv;
};

struct mutable_params_t {
};

//==============================================================================
class App {
public:
	App(const struct user_params_t & _user_params, const struct mutable_params_t & _mutable_params)
	: m_const_params(_user_params), m_mutable_params(_mutable_params){

		TicksBase::init(m_const_params.b_no_rdtsc ? TicksBase::CLOCK : TicksBase::RDTSC);
	}
	~App(){}

	const struct user_params_t m_const_params;
	struct mutable_params_t  m_mutable_params;
};

extern const App* g_pApp;
//------------------------------------------------------------------------------

#endif /* DEFS_H_ */
