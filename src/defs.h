/*
 * Copyright (c) 2011-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
 */

#ifndef DEFS_H_
#define DEFS_H_

#define __STDC_FORMAT_MACROS

#ifdef __windows__
#include <WS2tcpip.h>
#include <Winsock2.h>
#include <unordered_map>
#include <Winbase.h>
#include <stdint.h>
#include <afunix.h>
typedef unsigned short int sa_family_t;

#else

#ifdef __linux__
#include <features.h>
#endif

/* every file that use fd_set must include this section first for using big fd set size */
#if (__GLIBC__ > 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 2)
#include <bits/types.h>
#undef __FD_SETSIZE
#define __FD_SETSIZE 32768
#include <sys/select.h>
#endif

#ifdef __linux__
#include <sys/epoll.h>
#endif

#include <stdbool.h>
#include <stdint.h>
#include <unordered_map>
#include <unistd.h>   /* getopt() and sleep()*/
#include <poll.h>
#include <pthread.h>
#include <sys/time.h>   /* timers*/
#include <sys/socket.h> /* sockets*/
#include <sys/select.h> /* select() According to POSIX 1003.1-2001 */
#include <sys/syscall.h>
#include <arpa/inet.h>   /* internet address manipulation */
#include <netinet/in.h>  /* internet address manipulation */
#include <netdb.h>       /* getaddrinfo() */
#include <netinet/tcp.h> /* tcp specific */
#include <sys/resource.h>

#endif

#include <inttypes.h> /* printf PRItn */
#include <stdlib.h> /* random()*/
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <signal.h>
#include <time.h>  /* clock_gettime()*/
#include <ctype.h> /* isprint()*/
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h> /* sockets*/
#include <queue>
#include <map>

#include "ticks.h"
#include "message.h"
#include "playback.h"
#include "ip_address.h"

#if !defined(__windows__) && !defined(__FreeBSD__)
#include "vma-xlio-redirect.h"
#ifdef USING_VMA_EXTRA_API // VMA
#define RING_LOGIC_PER_INTERFACE VMA_RING_LOGIC_PER_INTERFACE
#define RING_LOGIC_PER_IP VMA_RING_LOGIC_PER_IP
#define RING_LOGIC_PER_SOCKET VMA_RING_LOGIC_PER_SOCKET
#define RING_LOGIC_PER_USER_ID VMA_RING_LOGIC_PER_USER_ID
#define RING_LOGIC_PER_THREAD VMA_RING_LOGIC_PER_THREAD
#define RING_LOGIC_PER_CORE VMA_RING_LOGIC_PER_CORE
#define RING_LOGIC_PER_CORE_ATTACH_THREADS VMA_RING_LOGIC_PER_CORE_ATTACH_THREADS
#define RING_LOGIC_LAST VMA_RING_LOGIC_LAST
#define ring_logic_t vma_ring_logic_t
#include <mellanox/vma_extra.h>
#undef RING_LOGIC_PER_INTERFACE
#undef RING_LOGIC_PER_IP
#undef RING_LOGIC_PER_SOCKET
#undef RING_LOGIC_PER_USER_ID
#undef RING_LOGIC_PER_THREAD
#undef RING_LOGIC_PER_CORE
#undef RING_LOGIC_PER_CORE_ATTACH_THREADS
#undef RING_LOGIC_LAST
#undef ring_logic_t
#endif // USING_VMA_EXTRA_API
#ifdef USING_XLIO_EXTRA_API // XLIO
#define RING_LOGIC_PER_INTERFACE XLIO_RING_LOGIC_PER_INTERFACE
#define RING_LOGIC_PER_IP XLIO_RING_LOGIC_PER_IP
#define RING_LOGIC_PER_SOCKET XLIO_RING_LOGIC_PER_SOCKET
#define RING_LOGIC_PER_USER_ID XLIO_RING_LOGIC_PER_USER_ID
#define RING_LOGIC_PER_THREAD XLIO_RING_LOGIC_PER_THREAD
#define RING_LOGIC_PER_CORE XLIO_RING_LOGIC_PER_CORE
#define RING_LOGIC_PER_CORE_ATTACH_THREADS XLIO_RING_LOGIC_PER_CORE_ATTACH_THREADS
#define RING_LOGIC_LAST XLIO_RING_LOGIC_LAST
#define ring_logic_t xlio_ring_logic_t
#include <mellanox/xlio_extra.h>
#undef RING_LOGIC_PER_INTERFACE
#undef RING_LOGIC_PER_IP
#undef RING_LOGIC_PER_SOCKET
#undef RING_LOGIC_PER_USER_ID
#undef RING_LOGIC_PER_THREAD
#undef RING_LOGIC_PER_CORE
#undef RING_LOGIC_PER_CORE_ATTACH_THREADS
#undef RING_LOGIC_LAST
#undef ring_logic_t
#endif // USING_XLIO_EXTRA_API
#endif // !WIN32 && !__FreeBSD__

#define MIN_PAYLOAD_SIZE (MsgHeader::EFFECTIVE_SIZE)
extern int MAX_PAYLOAD_SIZE;
#define MAX_STREAM_SIZE (50 * 1024 * 1024)
#define MAX_TCP_SIZE ((1 << 20) - 1)

const uint32_t MPS_MAX_UL =
    10 * 1000 * 1000; //  10 M MPS is 4 times the maximum possible under VMA today
const uint32_t MPS_MAX_PP =
    600 * 1000; // 600 K MPS for ping-pong will be break only when we reach RTT of 1.667 usec
extern uint32_t MPS_MAX;

const uint32_t MPS_DEFAULT = 10 * 1000;
const uint32_t REPLY_EVERY_DEFAULT = 100;

const uint32_t TEST_START_WARMUP_MSEC = 400;
const uint32_t TEST_END_COOLDOWN_MSEC = 50;
const uint64_t TEST_START_WARMUP_NUM = 8000;
const uint64_t TEST_END_COOLDOWN_NUM = 1000;

const uint32_t TEST_FIRST_CONNECTION_FIRST_PACKET_TTL_THRESHOLD_MSEC = 50;
#define TEST_ANY_CONNECTION_FIRST_PACKET_TTL_THRESHOLD_MSEC (0.1)

#define DEFAULT_CLIENT_WORK_WITH_SRV_NUM 1

#define DEFAULT_TEST_DURATION 1 /* [sec] */
#define DEFAULT_TEST_NUMBER 0   /* [number of packets] */
#define DEFAULT_PORT_STR "11111"
#define DEFAULT_IP_MTU 1500
#define DEFAULT_IP_PAYLOAD_SZ (DEFAULT_IP_MTU - 28)
#define DEFAULT_CI_SIG_LEVEL 99
#define DUMMY_PORT 57341
#define MAX_ACTIVE_FD_NUM                                                                          \
    1024 /* maximum number of active connection to the single TCP addr:port                        \
            */
#ifdef USING_VMA_EXTRA_API // For VMA socketxtreme Only
#define MAX_VMA_COMPS 1024 /* maximum size for the VMA completions array for VMA Poll */
#endif // USING_VMA_EXTRA_API

#ifndef MAX_PATH_LENGTH
#define MAX_PATH_LENGTH 1024
#endif
#define MAX_MCFILE_LINE_LENGTH 1024 /* big enough to store IPv6 addresses and hostnames */

#define IP_PORT_FORMAT_REG_EXP                                                                     \
    "^([UuTt]:)?"                                                                                  \
    "([a-zA-Z0-9\\.\\-]+|\\[([a-fA-F0-9.:]+(%[0-9a-zA-z])?)\\])"                                   \
    ":(6553[0-5]|655[0-2][0-9]|65[0-4][0-9]{2}|6[0-4][0-9]{3}|[0-5]?[0-9]{1,4})"                   \
    "(:([a-zA-Z0-9\\.\\-]+|\\[([a-fA-F0-9.:]+)\\]))?[\r\n]"

#ifdef __windows__
#define UNIX_DOMAIN_SOCKET_FORMAT_REG_EXP                                                          \
        "^[UuTt]:([A-Za-z]:[\\\\/].*)[\r\n]*"
#define RESOLVE_ADDR_FORMAT_SOCKET                                                                 \
        "[A-Za-z]:[\\\\/].*"
#elif __linux__
#define UNIX_DOMAIN_SOCKET_FORMAT_REG_EXP                                                          \
        "^[UuTt]:(/.+)[\r\n]"
#define RESOLVE_ADDR_FORMAT_SOCKET                                                                 \
        "/.+"
#endif

#define PRINT_PROTOCOL(type)                                                                       \
    ((type) == SOCK_DGRAM ? "UDP" : ((type) == SOCK_STREAM ? "TCP" : "<?>"))
#define PRINT_SOCKET_TYPE(type)                                                                     \
    ((type) == SOCK_DGRAM ? "SOCK_DGRAM" : ((type) == SOCK_STREAM ? "SOCK_STREAM" : "<?>"))

#define MAX_ARGV_SIZE 256
#define MAX_DURATION 36000000
#define MAX_PACKET_NUMBER 100000000
extern const int MAX_FDS_NUM;
#define SOCK_BUFF_DEFAULT_SIZE 0
#define DEFAULT_SELECT_TIMEOUT_MSEC 10
#define DEFAULT_DEBUG_LEVEL 0

#ifndef SO_MAX_PACING_RATE
#define SO_MAX_PACING_RATE 47
#endif
/*
Used by offload libraries to do egress path warm-up of caches.
It is not in use by kernel. WARNING: it will actually end this packet on the wire.
DUMMY_SEND_FLAG value should be compatible with the value of VMA_SND_FLAGS_DUMMY (More info at
vma_extra.h).
*/
#define DUMMY_SEND_FLAG 0x400 // equals to MSG_SYN
#define DUMMY_SEND_MPS_DEFAULT 10000

enum {
    OPT_RX_MC_IF = 1,
    OPT_TX_MC_IF,                 // 2
    OPT_SELECT_TIMEOUT,           // 3
    OPT_THREADS_NUM,              // 4
    OPT_CLIENT_CYCLE_DURATION,    // 5
    OPT_BUFFER_SIZE,              // 6
    OPT_DATA_INTEGRITY,           // 7
    OPT_DAEMONIZE,                // 8
    OPT_NONBLOCKED,               // 9
    OPT_DONTWARMUP,               // 10
    OPT_PREWARMUPWAIT,            // 11
    OPT_RXFILTERCB,               // 12
    OPT_ZCOPYREAD,                // 13
    OPT_SOCKETXTREME,             // 14
    OPT_MC_LOOPBACK_ENABLE,       // 15
    OPT_CLIENT_WORK_WITH_SRV_NUM, // 16
    OPT_FORCE_UC_REPLY,           // 17
    OPT_MPS,                      // 18
    OPT_REPLY_EVERY,              // 19
    OPT_NO_RDTSC,                 // 20
    OPT_SENDER_AFFINITY,          // 21
    OPT_RECEIVER_AFFINITY,        // 22
    OPT_LOAD_VMA,                 // 23
    OPT_FULL_LOG,                 // 24
    OPT_GIGA_SIZE,                // 25
    OPT_PLAYBACK_DATA,            // 26
    OPT_TCP,                      // 27
    OPT_TCP_NODELAY_OFF,          // 28
    OPT_NONBLOCKED_SEND,          // 29
    OPT_IP_MULTICAST_TTL,         // 30
    OPT_SOCK_ACCL,                // 31
    OPT_THREADS_AFFINITY,         // 32
    OPT_DONT_REPLY,               // 33
    OPT_RECV_LOOPING,             // 34
    OPT_OUTPUT_PRECISION,         // 35
    OPT_CLIENTPORT,               // 36
    OPT_CLIENTADDR,               // 37
    OPT_TOS,                      // 38
    OPT_LLS,                      // 39
    OPT_MC_SOURCE_IP,             // 40
    OPT_DUMMY_SEND,               // 41
    OPT_RATE_LIMIT,               // 42
    OPT_UC_REUSEADDR,             // 43
    OPT_FULL_RTT,                 // 44
    OPT_CI_SIG_LVL,               // 45
    OPT_HISTOGRAM,                // 46
    OPT_LOAD_XLIO,                // 47
#if defined(DEFINED_TLS)
    OPT_TLS
#endif /* DEFINED_TLS */
};

static const char *const round_trip_str[] = { "latency", "rtt" };

#define MODULE_NAME "sockperf"
#define MODULE_COPYRIGHT                                                                           \
    "Copyright (C) 2011-2022 Mellanox Technologies Ltd."                                           \
    "\nSockPerf is open source software, see http://github.com/mellanox/sockperf"
#define log_msg(log_fmt, ...) printf(MODULE_NAME ": " log_fmt "\n", ##__VA_ARGS__)
#define log_msg_file(file, log_fmt, ...) fprintf(file, MODULE_NAME ": " log_fmt "\n", ##__VA_ARGS__)
#define log_msg_file2(file, log_fmt, ...)                                                          \
    if (1) {                                                                                       \
        log_msg(log_fmt, ##__VA_ARGS__);                                                           \
        if (file) log_msg_file(file, log_fmt, ##__VA_ARGS__);                                      \
    } else

#define log_err(log_fmt, ...)                                                                      \
    printf("sockperf: ERROR: " log_fmt " (errno=%d %s)\n", ##__VA_ARGS__, errno, strerror(errno))
#ifdef DEBUG
#undef log_err
#define log_err(log_fmt, ...)                                                                      \
    printf(MODULE_NAME ": "                                                                        \
                       "%s:%d:ERROR: " log_fmt " (errno=%d %s)\n",                                 \
           __FILE__, __LINE__, ##__VA_ARGS__, errno, strerror(errno))
#endif
#define log_dbg(log_fmt, ...)                                                                      \
    if (g_debug_level >= LOG_LVL_DEBUG) {                                                          \
        printf(MODULE_NAME ": " log_fmt "\n", ##__VA_ARGS__);                                      \
    } else

#define TRACE(msg) log_msg("TRACE <%s>: %s() %s:%d\n", msg, __func__, __FILE__, __LINE__)
#define ERROR_MSG(msg)                                                                             \
    if (1) {                                                                                       \
        TRACE(msg);                                                                                \
        exit_with_log(-1);                                                                         \
    } else

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
#define PRIXPTR "lX"
#endif

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) ((void)P)
#endif

#ifndef likely
#if defined(__GNUC__) || defined(__INTEL_COMPILER)
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif
#endif

#define RET_SOCKET_SKIPPED (-2) /**< socket operation is skipped */
#define RET_SOCKET_SHUTDOWN 0   /**< socket is shutdown */

/**
 * @enum SOCKPERF_ERROR
 * @brief List of supported error codes.
 */
typedef enum {
    SOCKPERF_ERR_NONE = 0x0,   /**< the function completed */
    SOCKPERF_ERR_BAD_ARGUMENT, /**< incorrect parameter */
    SOCKPERF_ERR_INCORRECT,    /**< incorrect format of object */
    SOCKPERF_ERR_UNSUPPORTED,  /**< this function is not supported */
    SOCKPERF_ERR_NOT_EXIST,    /**< requested object does not exist */
    SOCKPERF_ERR_NO_MEMORY,    /**< dynamic memory error */
    SOCKPERF_ERR_FATAL,        /**< system fatal error */
    SOCKPERF_ERR_SOCKET,       /**< socket operation error */
    SOCKPERF_ERR_TIMEOUT,      /**< the time limit expires */
    SOCKPERF_ERR_UNKNOWN       /**< general error */
} SOCKPERF_ERROR;

/*
 *  Debug configuration settings
 */
#ifdef DEBUG
#define LOG_TRACE_SEND FALSE
#define LOG_TRACE_RECV FALSE
#define LOG_TRACE_MSG_IN FALSE
#define LOG_TRACE_MSG_OUT FALSE
#define LOG_MEMORY_CHECK FALSE
#define LOG_LOCK_CHECK FALSE

#define LOG_TRACE(category, format, ...)                                                           \
    log_send(category, 1, __FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)

inline void log_send(const char *name, int priority, const char *file_name, const int line_no,
                     const char *func_name, const char *format, ...) {
    if (priority) {
        char buf[250];
        va_list va;
        int n = 0;

        va_start(va, format);
        n = vsnprintf(buf, sizeof(buf) - 1, format, va);
        va_end(va);

        printf("[%s] %s: %s <%s: %s #%d> size %d\n", "debug", (name ? name : ""), buf, file_name,
               func_name, line_no, n);
    }
}

#if defined(LOG_MEMORY_CHECK) && (LOG_MEMORY_CHECK == TRUE)
#define MALLOC(size) __malloc(size, __FUNCTION__, __LINE__)
#define FREE(ptr)                                                                                  \
    do {                                                                                           \
        __free(ptr, __FUNCTION__, __LINE__);                                                       \
        ptr = NULL;                                                                                \
    } while (0)
#else
#define MALLOC(size) malloc(size)
#define FREE(ptr)                                                                                  \
    do {                                                                                           \
        free(ptr);                                                                                 \
        ptr = NULL;                                                                                \
    } while (0)
#endif /* LOG_MEMORY_CHECK */

inline void *__malloc(int size, const char *func, int line) {
    void *ptr = malloc(size);
    printf("malloc: %p (%d) <%s:%d>\n", ptr, size, func, line);
    return ptr;
}

inline void __free(void *ptr, const char *func, int line) {
    free(ptr);
    printf("free  : %p <%s:%d>\n", ptr, func, line);
    return;
}
#else
#define LOG_TRACE(category, format, ...)
#define MALLOC(size) malloc(size)
#define FREE(ptr)                                                                                  \
    do {                                                                                           \
        free(ptr);                                                                                 \
        ptr = NULL;                                                                                \
    } while (0) // FREE is not expected to be called from fast path
#endif          /* DEBUG */

/**
 * @name Synchronization
 * @brief Multi-threading Synchronization operations.
 */
/** @{ */
#define CRITICAL_SECTION os_mutex_t
#if defined(_GNU_SOURCE)
#define INIT_CRITICAL(x)                                                                           \
    {                                                                                              \
        pthread_mutexattr_t attr;                                                                  \
        pthread_mutexattr_init(&attr);                                                             \
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_DEFAULT);                                   \
        pthread_mutex_init(x.mutex, &attr);                                                        \
        pthread_mutexattr_destroy(&attr);                                                          \
    }
#else
#define INIT_CRITICAL(x) os_mutex_init(x)
#endif
#define DELETE_CRITICAL(x) os_mutex_close(x)
#define DBG_ENTER_CRITICAL(x) os_mutex_lock(x)
#define DBG_LEAVE_CRITICAL(x) os_mutex_unlock(x)

/* Debugging Multi-threading operations. */
#if defined(LOG_LOCK_CHECK) && (LOG_LOCK_CHECK == TRUE)
#define ENTER_CRITICAL(x)                                                                          \
    {                                                                                              \
        printf("lock = 0x%" PRIXPTR " <%s: %s #%d>\n", (uintptr_t)x, __FILE__, __FUNCTION__,       \
               __LINE__);                                                                          \
        DBG_ENTER_CRITICAL(x);                                                                     \
    }
#define LEAVE_CRITICAL(x)                                                                          \
    {                                                                                              \
        printf("unlock = 0x%" PRIXPTR " <%s: %s #%d>\n", (uintptr_t)x, __FILE__, __FUNCTION__,     \
               __LINE__);                                                                          \
        DBG_LEAVE_CRITICAL(x);                                                                     \
    }
#else
#define ENTER_CRITICAL(x) DBG_ENTER_CRITICAL(x)
#define LEAVE_CRITICAL(x) DBG_LEAVE_CRITICAL(x)
#endif /* LOG_LOCK_CHECK */
/** @} */

/* Global variables */
extern bool g_b_exit;
extern bool g_b_errorOccured;
extern uint64_t g_receiveCount;
extern uint64_t g_skipCount;

extern unsigned long long g_cycle_wait_loop_counter;
extern TicksTime g_cycleStartTime;

extern debug_level_t g_debug_level;

#if defined(USING_VMA_EXTRA_API) || defined(USING_XLIO_EXTRA_API)
#ifdef USING_VMA_EXTRA_API // VMA
extern struct vma_buff_t *g_vma_buff;
extern struct vma_completion_t *g_vma_comps;
#endif // USING_VMA_EXTRA_API
class ZeroCopyData {
public:
    ZeroCopyData();
    void allocate();
    ~ZeroCopyData();
    unsigned char *m_pkt_buf;
    void *m_pkts;
};
// map from fd to zeroCopyData
typedef std::map<int, ZeroCopyData *> zeroCopyMap;
extern zeroCopyMap g_zeroCopyData;
#endif // USING_VMA_EXTRA_API || USING_XLIO_EXTRA_API

class Message;

class PacketTimes;
extern PacketTimes *g_pPacketTimes;

extern TicksTime g_lastTicks;

typedef struct spike {
    //   double usec;
    TicksDuration ticks;
    unsigned long long packet_counter_at_spike;
    int next;
} spike;

struct SocketRecvData {
    uint8_t *buf = nullptr;         // buffer for input messages (double size is reserved)
    int max_size = 0;               // maximum message size
    uint8_t *cur_addr = nullptr;    // start of current message (may point outside buf)
    int cur_offset = 0;             // number of available message bytes
    int cur_size = 0;               // maximum number of bytes for the next chunk
};

// big enough to store sockaddr_in and sockaddr_in6
struct sockaddr_store_t {
    union {
        sa_family_t ss_family;
        sockaddr_in addr4;
        sockaddr_in6 addr6;
        sockaddr_un addr_un;
    };
};

/**
 * @struct fds_data
 * @brief Socket related info
 */

struct fds_data {
    struct sockaddr_store_t server_addr; /**< server address information */
    socklen_t server_addr_len = 0;  /**< server address length */
    int is_multicast = 0;           /**< if this socket is multicast */
    int sock_type = 0;              /**< SOCK_STREAM (tcp), SOCK_DGRAM (udp), SOCK_RAW (ip) */
    int next_fd = 0;
    int active_fd_count = 0;        /**< number of active connections (by default 1-for UDP; 0-for TCP) */
    int *active_fd_list = nullptr;  /**< list of fd related active connections (UDP has the same fd by default) */
    struct sockaddr_store_t *memberships_addr = nullptr; /**< more servers on the same socket information */
    IPAddress mc_source_ip_addr;    /**< message source ip for multicast packet filtering */
    int memberships_size = 0;
    struct SocketRecvData recv;
#ifdef USING_VMA_EXTRA_API // VMA callback-extra-api Only
    Message *p_msg = nullptr; // For VMA callback API.
#endif // USING_VMA_EXTRA_API
#if defined(DEFINED_TLS)
    void *tls_handle = nullptr;
#endif /* DEFINED_TLS */

    fds_data()
    {
        memset(&server_addr, 0, sizeof(server_addr));
    }
};

/**
 * @struct handler_info
 * @brief Handler configuration data
 */
typedef struct handler_info {
    int id;     /**< handler ID */
    int fd_min; /**< minimum descriptor (fd) */
    int fd_max; /**< maximum socket descriptor (fd) */
    int fd_num; /**< number of socket descriptors */
} handler_info;

typedef struct clt_session_info {
    uint64_t seq_num;
    uint64_t total_drops;
    sockaddr_store_t addr;
    bool started;
} clt_session_info_t;

// The only types that TR1 has built-in hash/equal_to functions for, are scalar types,
// std:: string, and std::wstring. For any other type, we need to write a
// hash/equal_to functions, by ourself.
namespace std {
template <> struct hash<struct sockaddr_store_t> : public std::unary_function<struct sockaddr_store_t, int> {
    int operator()(struct sockaddr_store_t const &key) const {
        // XOR "a.b" part of "a.b.c.d" address with 16bit port; leave "c.d" part untouched for
        // maximum hashing
        switch (key.ss_family) {
        case AF_INET: {
            const sockaddr_in &k = reinterpret_cast<const sockaddr_in &>(key);
            return k.sin_addr.s_addr ^ (k.sin_port << 16);
        }
        case AF_INET6: {
            const sockaddr_in6 &k = reinterpret_cast<const sockaddr_in6 &>(key);
            const uint32_t *addr = reinterpret_cast<const uint32_t *>(&k.sin6_addr);
            return addr[0] ^ addr[1] ^ addr[2] ^ addr[3] ^ (k.sin6_port << 16);
        }
        default:
            return 0;
        }
    }
};

template <>
struct equal_to<struct sockaddr_store_t> :
        public std::binary_function<struct sockaddr_store_t,
                struct sockaddr_store_t, bool> {
    bool operator()(struct sockaddr_store_t const &key1, struct sockaddr_store_t const &key2) const {
        if (key1.ss_family != key2.ss_family) {
            return false;
        }
        switch (key1.ss_family) {
        case AF_INET: {
            const sockaddr_in &k1 = reinterpret_cast<const sockaddr_in &>(key1);
            const sockaddr_in &k2 = reinterpret_cast<const sockaddr_in &>(key2);
            return k1.sin_port == k2.sin_port &&
                k1.sin_addr.s_addr == k2.sin_addr.s_addr;
        }
        case AF_INET6: {
            const sockaddr_in6 &k1 = reinterpret_cast<const sockaddr_in6 &>(key1);
            const sockaddr_in6 &k2 = reinterpret_cast<const sockaddr_in6 &>(key2);
            return k1.sin6_port == k2.sin6_port &&
                IN6_ARE_ADDR_EQUAL(&k1.sin6_addr, &k2.sin6_addr);
        }
        }
        return true;
    }
};
} // namespace std

#ifdef USING_VMA_EXTRA_API // VMA socketxtreme-extra-api Only
struct vma_ring_comps {
    vma_completion_t vma_comp_list[MAX_VMA_COMPS];
    int vma_comp_list_size;
    bool is_freed;
};
#endif // USING_VMA_EXTRA_API

typedef std::unordered_map<struct sockaddr_store_t, clt_session_info_t> seq_num_map;
typedef std::unordered_map<IPAddress, size_t> addr_to_id;
#ifdef USING_VMA_EXTRA_API // VMA socketxtreme-extra-api Only
typedef std::unordered_map<int, struct vma_ring_comps *> rings_vma_comps_map;
#endif // USING_VMA_EXTRA_API

extern fds_data **g_fds_array;
extern int IGMP_MAX_MEMBERSHIPS;

#ifdef USING_VMA_EXTRA_API // VMA
typedef std::queue<int> vma_comps_queue;
extern struct vma_api_t *g_vma_api;
#else
extern void *g_vma_api; // Dummy variable
#endif // USING_VMA_EXTRA_API

#ifdef USING_XLIO_EXTRA_API // XLIO
extern struct xlio_api_t *g_xlio_api;
#else
extern void *g_xlio_api; // Dummy variable
#endif // USING_XLIO_EXTRA_API

typedef enum {
    MODE_CLIENT = 0,
    MODE_SERVER,
    MODE_BRIDGE
} work_mode_t;

typedef enum {
    TIME_BASED = 1,
    NUMBER_BASED
} measurement_mode_t;

typedef enum { // must be coordinated with s_fds_handle_desc in common.cpp
    RECVFROM = 0,
    RECVFROMMUX,
    SELECT,
#ifndef __windows__
    POLL,
    EPOLL,
#endif
    SOCKETXTREME,
    FD_HANDLE_MAX } fd_block_handler_t;

struct user_params_t {
    work_mode_t mode = MODE_SERVER; // either client or server
    measurement_mode_t measurement = TIME_BASED; // either time or number
    int rx_mc_if_ix;
    bool rx_mc_if_ix_specified;
    int tx_mc_if_ix;
    bool tx_mc_if_ix_specified;
    IPAddress rx_mc_if_addr;
    IPAddress tx_mc_if_addr;
    IPAddress mc_source_ip_addr;
    int msg_size = MIN_PAYLOAD_SIZE;
    int msg_size_range = 0;
    int sec_test_duration = DEFAULT_TEST_DURATION;
    uint64_t number_test_target = DEFAULT_TEST_NUMBER;
    bool data_integrity = false;
    fd_block_handler_t fd_handler_type = RECVFROM;
    unsigned int packetrate_stats_print_ratio = 0;
    unsigned int burst_size = 1;
    bool packetrate_stats_print_details = false;
    int mthread_server = 0;
    struct timeval *select_timeout = nullptr;
    int sock_buff_size = SOCK_BUFF_DEFAULT_SIZE;
    int threads_num = 1;
    char threads_affinity[MAX_ARGV_SIZE];
    bool is_rxfiltercb = false;
    bool is_zcopyread = false;
    bool is_blocked = true;
    bool do_warmup = true;
    unsigned int pre_warmup_wait = 0;
    uint32_t cooldown_msec = 0;
    uint32_t warmup_msec = 0;
    uint64_t cooldown_num = 0;
    uint64_t warmup_num = 0;
    bool is_vmarxfiltercb = false;
    bool is_vmazcopyread = false;
    TicksDuration cycleDuration;
    bool mc_loop_disable = true;
    bool uc_reuseaddr = false;
    int client_work_with_srv_num = DEFAULT_CLIENT_WORK_WITH_SRV_NUM;
    bool b_server_reply_via_uc = false;
    bool b_server_dont_reply = false;
    bool b_server_detect_gaps = false;
    uint32_t mps = MPS_DEFAULT; // client side only
    struct sockaddr_store_t client_bind_info;
    socklen_t client_bind_info_len = 0;
    uint32_t reply_every = REPLY_EVERY_DEFAULT;    // client side only
    bool b_client_ping_pong = false; // client side only
    bool b_no_rdtsc = false;
    char sender_affinity[MAX_ARGV_SIZE];
    char receiver_affinity[MAX_ARGV_SIZE];
    FILE *fileFullLog = NULL;                   // client side only
    bool full_rtt = false;                      // client side only
    bool giga_size = false;                     // client side only
    bool increase_output_precision = false;     // client side only
    bool b_stream = false;                      // client side only
    PlaybackVector *pPlaybackVector = NULL;     // client side only
    uint32_t ci_significance_level = DEFAULT_CI_SIG_LEVEL;// client side only
    bool b_histogram;                           // client side only
    uint32_t histogram_lower_range = 0;         // client side only
    uint32_t histogram_upper_range = 2000000;   // client side only
    uint32_t histogram_bin_size = 10;           // client side only
    struct sockaddr_store_t addr;
    socklen_t addr_len = 0;
    int sock_type = SOCK_DGRAM;
    bool tcp_nodelay = true;
    bool is_nonblocked_send = false;
    int mc_ttl = 2;
    int daemonize = false;
    char feedfile_name[MAX_PATH_LENGTH];
    bool withsock_accl = false;
    int max_looping_over_recv = 1;
    int tos = 0x00;
    unsigned int lls_usecs = 0;
    bool lls_is_set = false;
    uint32_t dummy_mps = 0;                   // client side only
    TicksDuration dummySendCycleDuration; // client side only
    uint32_t rate_limit = 0;
#if defined(DEFINED_TLS)
    bool tls = false;
#endif /* DEFINED_TLS */

    user_params_t() {
        memset(&client_bind_info, 0, sizeof(client_bind_info));
        memset(&addr, 0, sizeof(addr));
        memset(sender_affinity, 0, sizeof(sender_affinity));
        memset(receiver_affinity, 0, sizeof(receiver_affinity));
    }
};

struct mutable_params_t {};

//==============================================================================
class App {
public:
    App(const struct user_params_t &_user_params, const struct mutable_params_t &_mutable_params)
        : m_const_params(_user_params), m_mutable_params(_mutable_params) {
    }
    ~App() {}

    const struct user_params_t m_const_params;
    struct mutable_params_t m_mutable_params;
};

extern const App *g_pApp;
//------------------------------------------------------------------------------

#endif /* DEFS_H_ */
