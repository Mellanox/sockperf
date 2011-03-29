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

//#define USING_VMA_EXTRA_API
#ifdef  USING_VMA_EXTRA_API
#include <voltaire/vma_extra.h>
#endif

#define MIN_PAYLOAD_SIZE        	(MsgHeader::EFFECTIVE_SIZE)
#define MAX_PAYLOAD_SIZE        	(65506)
#define MAX_STREAM_SIZE         	(50*1024*1024)

const uint32_t PPS_MAX_UL          = 10*1000*1000; //  10 M PPS is 4 times the maximum possible under VMA today
const uint32_t PPS_MAX_PP          = 400*1000;     // 400 K PPS for ping-pong will be break only when we reach RTT of 2.5 usec
extern uint32_t PPS_MAX;

const uint32_t PPS_DEFAULT         = 10*1000;
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
#define MAX_FDS_NUM 				1024
#define UDP_BUFF_DEFAULT_SIZE 		0
#define DEFAULT_SELECT_TIMEOUT_MSEC	10
#define DEFAULT_DEBUG_LEVEL			0
#define INVALID_SOCKET 				(-1)

enum {
	OPT_RX_MC_IF 			 = 1,
	OPT_TX_MC_IF,			// 2
	OPT_SELECT_TIMEOUT,		// 3
	OPT_MULTI_THREADED_SERVER, 	// 4
	OPT_CLIENT_CYCLE_DURATION,	// 5
	OPT_UDP_BUFFER_SIZE,		// 6
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
	OPT_PPS,                     //17
	OPT_REPLY_EVERY,             //18
	OPT_NO_RDTSC,                //20
	OPT_SENDER_AFFINITY,         //21
	OPT_RECEIVER_AFFINITY,       //22
	OPT_LOAD_VMA,                //23
	OPT_FULL_LOG,                //24
	OPT_PLAYBACK_DATA,                  //26
	OPT_TCP,					//27
	OPT_TCP_NODELAY_OFF,		//28
	OPT_IP_MULTICAST_TTL,			//29
	OPT_SOCK_ACCL,				//30
};

#define MODULE_NAME			"sockperf"
#define MODULE_COPYRIGHT	"Copyright (C) 2011 Mellanox Technologies Ltd." \
	"\nSockPerf is open source software, see http://sockperf.googlecode.com/"
#define log_msg(log_fmt, log_args...)	printf(MODULE_NAME ": " log_fmt "\n", ##log_args)
#define log_msg_file(file, log_fmt, log_args...)	fprintf(file, MODULE_NAME ": " log_fmt "\n", ##log_args)
#define log_msg_file2(file, log_fmt, log_args...)	if (1) {log_msg(log_fmt, ##log_args); if (file) log_msg_file(file, log_fmt, ##log_args);} else

#define log_err(log_fmt, log_args...)	printf(MODULE_NAME ": " "%s:%d:ERROR: " log_fmt " (errno=%d %s)\n", __FILE__, __LINE__, ##log_args, errno, strerror(errno))
#define log_dbg(log_fmt, log_args...)	if (g_debug_level >= LOG_LVL_DEBUG) { printf(MODULE_NAME ": " log_fmt "\n", ##log_args); } else;

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


/**
 * @enum SOCKPERF_ERROR
 * @brief List of supported error codes.
 */
typedef enum
{
	SOCKPERF_ERR_NONE  =  0x0,  /**< the function completed */
	SOCKPERF_ERR_BAD_ARGUMENT,  /**< incorrect parameter */
	SOCKPERF_ERR_INCORRECT,     /**< incorrect format of object */
	SOCKPERF_ERR_UNSUPPORTED,   /**< this function is not supported */
	SOCKPERF_ERR_NOT_EXIST,     /**< requested object does not exist */
	SOCKPERF_ERR_NO_MEMORY,     /**< dynamic memory error */
	SOCKPERF_ERR_FATAL,         /**< system fatal error */
	SOCKPERF_ERR_SOCKET,        /**< socket operation error */
	SOCKPERF_ERR_TIMEOUT,       /**< the time limit expires */
	SOCKPERF_ERR_UNKNOWN        /**< general error */
} SOCKPERF_ERROR;


/*
 *  Debug configuration settings
 */
#ifdef DEBUG
	#define LOG_TRACE_SEND		FALSE
	#define LOG_TRACE_RECV		FALSE
	#define LOG_TRACE_MSG_IN	FALSE
	#define LOG_TRACE_MSG_OUT	FALSE
	#define LOG_TRACE_CONNECT	FALSE
	#define LOG_MEMORY_CHECK	FALSE

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
		#define FREE(ptr) 		__free(ptr, __FUNCTION__, __LINE__)
	#else
		#define MALLOC(size) 	malloc(size)
		#define FREE(ptr) 		free(ptr)
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
	#define FREE(ptr) 		free(ptr)
#endif /* DEBUG */


extern bool g_b_exit;
extern uint64_t g_receiveCount;
extern uint64_t g_serverSendCount;

extern unsigned long long g_cycle_wait_loop_counter;
extern TicksTime g_cycleStartTime;

extern debug_level_t g_debug_level;
extern int *g_pid_arr;

#ifdef  USING_VMA_EXTRA_API
extern unsigned char* g_dgram_buf;
extern struct vma_datagram_t *g_dgram;
#endif

extern int g_vma_dgram_desc_size;
class Message;

class PacketTimes;
extern PacketTimes* g_pPacketTimes;

extern unsigned int g_data_integrity_failed;
extern unsigned int g_duplicate_packets_counter;

extern int g_sockets_num;

extern TicksTime g_lastTicks;
extern unsigned long long g_last_packet_counter;


typedef struct spike{
//   double usec;
   TicksDuration ticks;
   unsigned long long packet_counter_at_spike;
   int next;
 }spike;


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
	struct {
		uint8_t buf[2 * MAX_PAYLOAD_SIZE];
		int max_size;
		uint8_t *cur_addr;
		int cur_offset;
		int cur_size;
	} recv;
} fds_data;


/**
 * @struct sub_fds_arr_info
 * @brief Interval of fds related the thread
 */
typedef struct sub_fds_arr_info {
	int fd_min;					/**< minimum descriptor (fd) */
	int fd_max;					/**< maximum socket descriptor (fd) */
	int fd_num;					/**< number of socket descriptors */
}sub_fds_arr_info;

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
	}

	template<>
	struct equal_to<struct sockaddr_in>: public std::binary_function<struct sockaddr_in, struct sockaddr_in, bool>
	{
		bool operator()(struct sockaddr_in const &key1, struct sockaddr_in const &key2) const
		{
			return key1.sin_port == key2.sin_port && key1.sin_addr.s_addr == key2.sin_addr.s_addr;
		}
	};
}

typedef std::tr1::unordered_map<struct sockaddr_in, clt_session_info_t> seq_num_map;

extern seq_num_map g_seq_num_map;

extern fds_data* g_fds_array[MAX_FDS_NUM];

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

extern const char* g_fds_handle_desc[FD_HANDLE_MAX];

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
	int udp_buff_size;
	int threads_num;
	bool is_blocked;
	bool do_warmup;
	unsigned int pre_warmup_wait;
	bool is_vmarxfiltercb;
	bool is_vmazcopyread;
	TicksDuration cycleDuration;
	bool mc_loop_disable;
	int client_work_with_srv_num;
	bool b_server_reply_via_uc;
	bool b_server_detect_gaps;
	uint32_t pps; //client side only
	uint32_t reply_every; //client side only
	bool b_client_ping_pong; //client side only
	bool b_no_rdtsc;
	int sender_affinity;
	int receiver_affinity; // client side only
	//bool b_load_vma;
	FILE* fileFullLog; //client side only
	bool b_stream; //client side only
	PlaybackVector *pPlaybackVector; //client side only
	struct sockaddr_in addr;
	int sock_type;
	bool tcp_nodelay;
	int mc_ttl;
	int daemonize;
	char mcg_filename[MAX_PATH_LENGTH];
	bool withsock_accl;
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
