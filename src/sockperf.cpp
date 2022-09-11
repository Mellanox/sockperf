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

/*
 * How to Build: 'g++ *.cpp -O3 --param inline-unit-growth=200 -lrt -lpthread -ldl -o sockperf'
 */

/*
 * Still to do:
 *
 * V 2. stream mode should support respect --mps
 * *  playback file
 * * * finalize playback (possible conflicts with other cmd-args, including disclaimer, print info
 *before looping)
 * * * support factor 2x, 4x, etc in playback
 * * * playback should use the defined WARMUP and not hard-coded 100*1000usec (before & after)!
 * * * verify/support of playback behavior in case user presses CTRL-C during test
 * * * edit printout to full-log file for playback (duration & num records)
 * * * save 50% of temp memory in parsePlaybackData
 * 3. param for printing highest N spikes
 * 4. support multiple servers (infra structure exists in PacketTimes and in client_statistics)
 * 5. gap analysis in server
 * 6. packaging, probably like iperf, include tar/rpm, readme, make clean all install
 * V 7. dup-packets are not considered in receive-count.  Is this correct?
 * 8. Server should print send-count (in addition to receive-count)
 * 11 sockperf should work in exclusive modes (like svn): relevant modes are: latency-under-load,
 *stream, ping-pong, playback, subscriber (aka server)
 *
 *
 * 7. support OOO check for select/poll/epoll (currently check will fail unless recvfrom is used)
 * 8. configure and better handling of warmup/cooldown (flag on warmup packets)
 * 9. merge with perf envelope
 * 10. improve format of statistics at the end (tx/rx statistics line excluding warmup)
 * V 11. don't consider dropped packets at the end
 * X 12. consider: automatically set --mps=max in case of --ping-pong
 * 13. consider: loop till desirable seqNo instead of using timer (will distinguish between test and
 *cool down)
 * 14. remove error printouts about dup packets, or illegal seqno (and recheck illegal seqno with
 *mps=100 when server listen on same MC with 120 sockets)
 * 15. in case server is registered twice to same MC group+port, we consider the 2 replies as dup
 *packet.  is this correct?
 * 16. handshake between C/S at the beginning of run will allow the client to configure N servers
 *(like perf envelope) and more
 * 17. support sockperf version with -v
 * 18. consider preventing --range when --stream is used (for best statistcs)
 * 19. warmup also for uc (mainly server side)
 * 20. less permuatation in templates, for example receiver thread can be different entity than
 *sender thread
 * 21. use ptpd for latency test using synced clocks across machines (without RTT)
 * 22. use --mps=max as the default for ping-pong and throughput modes
 *
 */

#if defined(__GNUC__) && (__GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 9))
#   define NEED_REGEX_WORKAROUND
#endif

#define __STDC_LIMIT_MACROS // for UINT32_MAX in C++
#include <stdint.h>
#include <stdlib.h>

#ifdef NEED_REGEX_WORKAROUND
#   include <regex.h>
#else // NEED_REGEX_WORKAROUND
#   include <regex>
#endif // NEED_REGEX_WORKAROUND

#include <memory>
#include "common.h"
#include "message.h"
#include "message_parser.h"
#include "packet.h"
#include "port_descriptor.h"
#include "switches.h"
#include "aopt.h"
#include <stdio.h>
#include <sys/stat.h>

#ifndef __windows__
#include <dlfcn.h>
#endif

// forward declarations from Client.cpp & Server.cpp
extern void client_sig_handler(int signum);
extern void client_handler(handler_info *);
extern void server_sig_handler(int signum);
extern void server_handler(handler_info *);
extern void server_select_per_thread(int fd_num);

static bool sock_lib_started = 0; //
static int s_fd_max = 0;
static int s_fd_min = 0; /* used as THE fd when single mc group is given (RECVFROM blocked mode) */
static int s_fd_num = 0;
static struct mutable_params_t s_mutable_params;

static void set_select_timeout(int time_out_msec);
static int set_sockets_from_feedfile(const char *feedfile_name);
#ifdef ST_TEST
int prepare_socket(int fd, struct fds_data *p_data, bool stTest = false);
#else
int prepare_socket(int fd, struct fds_data *p_data);
#endif

void cleanup();

#ifndef VERSION
#define VERSION "unknown"
#endif

static int proc_mode_help(int, int, const char **);
static int proc_mode_version(int, int, const char **);
static int proc_mode_under_load(int, int, const char **);
static int proc_mode_ping_pong(int, int, const char **);
static int proc_mode_throughput(int, int, const char **);
static int proc_mode_playback(int, int, const char **);
static int proc_mode_server(int, int, const char **);

static const struct app_modes {
    int (*func)(int, int, const char **); /* proc function */
    const char *name;                     /* mode name to use from command line as an argument */
    const char *const shorts[4];          /* short name */
    const char *note;                     /* mode description */
} sockperf_modes[] = {
      { proc_mode_help, "help", aopt_set_string("h", "?"), "Display list of supported commands." },
      { proc_mode_version, "--version", aopt_set_string(NULL), "Tool version information." },
      { proc_mode_under_load,  "under-load",
        aopt_set_string("ul"), "Run " MODULE_NAME " client for latency under load test." },
      { proc_mode_ping_pong,   "ping-pong",
        aopt_set_string("pp"), "Run " MODULE_NAME " client for latency test in ping pong mode." },
      { proc_mode_playback,    "playback",
        aopt_set_string("pb"), "Run " MODULE_NAME " client for latency test using playback of predefined "
                               "traffic, based on timeline and message size." },
      { proc_mode_throughput,  "throughput",
        aopt_set_string("tp"), "Run " MODULE_NAME " client for one way throughput test." },
      { proc_mode_server, "server", aopt_set_string("sr"), "Run " MODULE_NAME " as a server." },
      { NULL, NULL, aopt_set_string(NULL), NULL }
  };

os_mutex_t _mutex;
static int parse_common_opt(const AOPT_OBJECT *);
static int parse_client_opt(const AOPT_OBJECT *);
static char *display_opt(int, char *, size_t);
static int resolve_sockaddr(const char *host, const char *port, int sock_type,
        bool is_server_mode, sockaddr *addr, socklen_t &addr_len);

/*
 * List of supported general options.
 */
static const AOPT_DESC common_opt_desc[] = {
    { 'h',                              AOPT_NOARG,
      aopt_set_literal('h', '?'),       aopt_set_string("help", "usage"),
      "Show the help message and exit." },
    { OPT_TCP,                AOPT_NOARG,                       aopt_set_literal(0),
      aopt_set_string("tcp", "stream"), "Use stream socket/TCP protocol (default dgram socket/UDP protocol)." },
    { 'i', AOPT_ARG, aopt_set_literal('i'), aopt_set_string("addr", "ip"), "Listen on/send to address in IPv4, IPv6, UNIX domain socket format"},
    { 'p',                                                AOPT_ARG,
      aopt_set_literal('p'),                              aopt_set_string("port"),
      "Listen on/connect to port <port> (default 11111)." },
    { 'f',                                                                AOPT_ARG,
      aopt_set_literal('f'),                                              aopt_set_string("file"),
      "Read list of connections from file (used in pair with -F option)." },
    { 'F',
      AOPT_ARG,
      aopt_set_literal('F'),
      aopt_set_string("iomux-type"),
#ifdef __windows__
      "Type of multiple file descriptors handle [s|select|r|recvfrom](default select)."
#elif __FreeBSD__
      "Type of multiple file descriptors handle [s|select|p|poll|r|recvfrom](default select)."
#else
      "Type of multiple file descriptors handle "
      "[s|select|p|poll|e|epoll|r|recvfrom|x|socketxtreme](default epoll)."
#endif
    },
    { OPT_SELECT_TIMEOUT,
      AOPT_ARG,
      aopt_set_literal(0),
      aopt_set_string("timeout"),
#ifdef __windows__
      "Set select timeout to <msec>, -1 for infinite (default is 10 msec)."
#elif __FreeBSD__
      "Set select/poll timeout to <msec>, -1 for infinite (default is 10 msec)."
#else
      "Set select/poll/epoll timeout to <msec>, -1 for infinite (default is 10 msec)."
#endif
    },
    { 'a',
      AOPT_ARG,
      aopt_set_literal('a'),
      aopt_set_string("activity"),
      "Measure activity by printing a '.' for the last <N> messages processed." },
    { 'A',
      AOPT_ARG,
      aopt_set_literal('A'),
      aopt_set_string("Activity"),
      "Measure activity by printing the duration for last <N>  messages processed." },
    { OPT_TCP_NODELAY_OFF, AOPT_NOARG, aopt_set_literal(0), aopt_set_string("tcp-avoid-nodelay"),
      "Stop/Start delivering TCP Messages Immediately (Enable/Disable Nagel). Default is Nagel "
      "Disabled except in Throughput where the default is Nagel enabled." },
    { OPT_NONBLOCKED_SEND,
      AOPT_NOARG,
      aopt_set_literal(0),
      aopt_set_string("tcp-skip-blocking-send"),
      "Enables non-blocking send operation (default OFF)." },
    { OPT_TOS, AOPT_ARG, aopt_set_literal(0), aopt_set_string("tos"), "Allows setting tos" },
    { OPT_RX_MC_IF, AOPT_ARG, aopt_set_literal(0), aopt_set_string("mc-rx-ip", "mc-rx-if"),
      "Use mc-rx-ip (IPv4) / mc-rx-if (IPv6). Set ipv4 address / interface index of interface on which to receive multicast messages (can be other then "
      "route table)."},
    { OPT_TX_MC_IF, AOPT_ARG, aopt_set_literal(0), aopt_set_string("mc-tx-ip", "mc-tx-if"),
      "Use mc-tx-ip (IPv4) / mc-tx-if (IPv6). Set ipv4 address / interface index of interface on which to transmit multicast messages (can be other then "
      "route table)."},
    { OPT_MC_LOOPBACK_ENABLE,                   AOPT_NOARG,
      aopt_set_literal(0),                      aopt_set_string("mc-loopback-enable"),
      "Enables mc loopback (default disabled)." },
    { OPT_IP_MULTICAST_TTL,                            AOPT_ARG,
      aopt_set_literal(0),                             aopt_set_string("mc-ttl"),
      "Limit the lifetime of the message (default 2)." },
    { OPT_MC_SOURCE_IP,
      AOPT_ARG,
      aopt_set_literal(0),
      aopt_set_string("mc-source-filter"),
      "Set address <ip, hostname> of multicast messages source which is allowed to receive from." },
    { OPT_UC_REUSEADDR,                                   AOPT_NOARG,
      aopt_set_literal(0),                                aopt_set_string("uc-reuseaddr"),
      "Enables unicast reuse address (default disabled)." },
    { OPT_LLS,                                                AOPT_ARG,
      aopt_set_literal(0),                                    aopt_set_string("lls"),
      "Turn on LLS via socket option (value = usec to poll)." },
    { OPT_BUFFER_SIZE,
      AOPT_ARG,
      aopt_set_literal(0),
      aopt_set_string("buffer-size"),
      "Set total socket receive/send buffer <size> in bytes (system defined by default)." },
    { OPT_NONBLOCKED,                AOPT_NOARG,                 aopt_set_literal(0),
      aopt_set_string("nonblocked"), "Open non-blocked sockets." },
    { OPT_RECV_LOOPING, AOPT_ARG, aopt_set_literal(0), aopt_set_string("recv_looping_num"),
      "Set sockperf to loop over recvfrom() until EAGAIN or <N> good received packets, -1 for "
      "infinite, must be used with --nonblocked (default 1). " },
    { OPT_DONTWARMUP,                AOPT_NOARG,                             aopt_set_literal(0),
      aopt_set_string("dontwarmup"), "Don't send warm up messages on start." },
    { OPT_PREWARMUPWAIT,                                        AOPT_ARG,
      aopt_set_literal(0),                                      aopt_set_string("pre-warmup-wait"),
      "Time to wait before sending warm up messages (seconds)." },
#ifndef __windows__
    { OPT_ZCOPYREAD,                                      AOPT_NOARG,
      aopt_set_literal(0),                                aopt_set_string("zcopyread", "vmazcopyread"),
      "Use VMA's zero copy reads API (See VMA's readme)." },
    { OPT_DAEMONIZE, AOPT_NOARG, aopt_set_literal(0), aopt_set_string("daemonize"), "Run as "
                                                                                    "daemon." },
    { OPT_NO_RDTSC,
      AOPT_NOARG,
      aopt_set_literal(0),
      aopt_set_string("no-rdtsc"),
      "Don't use register when taking time; instead use monotonic clock." },
    { OPT_LOAD_VMA,                                             AOPT_OPTARG,
      aopt_set_literal(0),                                      aopt_set_string("load-vma"),
      "Load VMA dynamically even when LD_PRELOAD was not used." },
    { OPT_LOAD_XLIO,                                            AOPT_OPTARG,
      aopt_set_literal(0),                                      aopt_set_string("load-xlio"),
      "Load XLIO dynamically even when LD_PRELOAD was not used." },
    { OPT_RATE_LIMIT, AOPT_ARG, aopt_set_literal(0), aopt_set_string("rate-limit"),
      "use rate limit (packet-pacing), with VMA must be run with VMA_RING_ALLOCATION_LOGIC_TX "
      "mode." },
#endif
    { OPT_SOCK_ACCL,
      AOPT_NOARG,
      aopt_set_literal(0),
      aopt_set_string("set-sock-accl"),
      "Set socket acceleration before run (available for some of Mellanox systems)" },
#if defined(DEFINED_TLS)
    { OPT_TLS,                AOPT_OPTARG,                                    aopt_set_literal(0),
      aopt_set_string("tls"), "Use TLSv1.2 (default " TLS_CHIPER_DEFAULT ")." },
#endif /* DEFINED_TLS */
    { 'd',                      AOPT_NOARG,                      aopt_set_literal('d'),
      aopt_set_string("debug"), "Print extra debug information." },
    { 0, AOPT_NOARG, aopt_set_literal(0), aopt_set_string(NULL), NULL }
};

/*
 * List of supported client options.
 */
static const AOPT_DESC client_opt_desc[] = {
    { OPT_SENDER_AFFINITY,
      AOPT_ARG,
      aopt_set_literal(0),
      aopt_set_string("sender-affinity"),
      "Set sender thread affinity to the given core ids in list format (see: cat /proc/cpuinfo)." },
    { OPT_RECEIVER_AFFINITY, AOPT_ARG, aopt_set_literal(0), aopt_set_string("receiver-affinity"),
      "Set receiver thread affinity to the given core ids in list format (see: cat "
      "/proc/cpuinfo)." },
    { OPT_FULL_LOG,
      AOPT_ARG,
      aopt_set_literal(0),
      aopt_set_string("full-log"),
      "Dump full log of all messages send/receive time to the given file in CSV format." },
    { OPT_FULL_RTT,                                         AOPT_NOARG,
      aopt_set_literal(0),                                  aopt_set_string("full-rtt"),
      "Show results in round-trip-time instead of latency." },
    { OPT_GIGA_SIZE,                AOPT_NOARG,                aopt_set_literal(0),
      aopt_set_string("giga-size"), "Print sizes in GigaByte." },
    { OPT_OUTPUT_PRECISION,
      AOPT_NOARG,
      aopt_set_literal(0),
      aopt_set_string("increase_output_precision"),
      "Increase number of digits after decimal point of the throughput output (from 3 to 9). " },
    { OPT_DUMMY_SEND, AOPT_OPTARG, aopt_set_literal(0), aopt_set_string("dummy-send"),
      "Use VMA's dummy send API instead of busy wait, must be higher than regular msg rate. "
      "\n\t\t\t\t optional: set dummy-send rate per second (default 10,000), usage: --dummy-send "
      "[<rate>|max]" },
    { 0, AOPT_NOARG, aopt_set_literal(0), aopt_set_string(NULL), NULL }
};

//------------------------------------------------------------------------------
static int proc_mode_help(int id, int argc, const char **argv) {
    int i = 0;

    printf(MODULE_NAME " is a tool for testing network latency and throughput.\n");
    printf("version %s\n", VERSION);
    printf("\n");
    printf("Usage: " MODULE_NAME " <subcommand> [options] [args]\n");
    printf("Type: \'" MODULE_NAME " <subcommand> --help\' for help on a specific subcommand.\n");
    printf("Type: \'" MODULE_NAME " --version\' to see the program version number.\n");
    printf("\n");
    printf("Available subcommands:\n");

    for (i = 0; sockperf_modes[i].name != NULL; i++) {
        char tmp_buf[21];

        /* skip commands with prefix '--', they are special */
        if (sockperf_modes[i].name[0] != '-') {
            printf("   %-20.20s\t%-s\n", display_opt(i, tmp_buf, sizeof(tmp_buf)),
                   sockperf_modes[i].note);
        }
    }

    printf("\n");
    printf("For additional information visit our website http://github.com/mellanox/sockperf , see "
           "README file, or Type \'sockperf <subcommand> --help\'.\n\n");

    return -1; /* this return code signals to do exit */
}

//------------------------------------------------------------------------------
static int proc_mode_version(int id, int argc, const char **argv) {
    printf(MODULE_NAME ", version %s\n", VERSION);
    printf("   compiled %s, %s\n", __DATE__, __TIME__);
    printf("\n%s\n", MODULE_COPYRIGHT);

    return -1; /* this return code signals to do exit */
}

//------------------------------------------------------------------------------
static int parse_client_bind_info(const AOPT_OBJECT *common_obj, const AOPT_OBJECT *self_obj)
{
    int rc = SOCKPERF_ERR_NONE;
    const char *host_str = NULL;
    const char *port_str = NULL;

    if (self_obj) {
        if (!rc && aopt_check(self_obj, OPT_CLIENTPORT)) {
            if (aopt_check(common_obj, 'f')) {
                log_msg("--client_port conflicts with -f option");
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            } else {
                const char *optarg = aopt_value(self_obj, OPT_CLIENTPORT);
                if (optarg && isNumeric(optarg)) {
                    port_str = optarg;
                } else {
                    log_msg("'--client_port' Invalid value");
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                }
            }
        }

        if (!rc && aopt_check(self_obj, OPT_CLIENTADDR)) {
            const char *optarg = aopt_value(self_obj, OPT_CLIENTADDR);
            if (optarg) {
                host_str = optarg;
            } else {
                log_msg("'--client_addr' Invalid address");
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }
    }

    if (!rc && (host_str || port_str)) {
        int res = resolve_sockaddr(host_str, port_str, s_user_params.sock_type,
                true, reinterpret_cast<sockaddr*>(&s_user_params.client_bind_info),
                s_user_params.client_bind_info_len);
        if (res != 0) {
            log_msg("'--client_addr/--client_port': invalid host:port values: %s\n",
                os_get_error(res));
            rc = SOCKPERF_ERR_BAD_ARGUMENT;
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
static int proc_mode_under_load(int id, int argc, const char **argv) {
    int rc = SOCKPERF_ERR_NONE;
    const AOPT_OBJECT *common_obj = NULL;
    const AOPT_OBJECT *client_obj = NULL;
    const AOPT_OBJECT *self_obj = NULL;

    /*
     * List of supported under-load options.
     */
    const AOPT_DESC self_opt_desc[] = {
        { 't',                                                 AOPT_ARG,
          aopt_set_literal('t'),                               aopt_set_string("time"),
          "Run for <sec> seconds (default 1, max = 36000000)." },
        { OPT_CLIENTPORT,
          AOPT_ARG,
          aopt_set_literal(0),
          aopt_set_string("client_port"),
          "Force the client side to bind to a specific port (default = 0). " },
        { OPT_CLIENTADDR,
          AOPT_ARG,
          aopt_set_literal(0),
          aopt_set_string("client_ip", "client_addr"),
          "Force the client side to bind to a specific address in IPv4, IPv6, UNIX domain socket format (default = 0). " },
        { 'b',                                                             AOPT_ARG,
          aopt_set_literal('b'),                                           aopt_set_string("burst"),
          "Control the client's number of a messages sent in every burst." },
        { OPT_REPLY_EVERY,
          AOPT_ARG,
          aopt_set_literal(0),
          aopt_set_string("reply-every"),
          "Set number of send messages between reply messages (default = 100)." },
        { OPT_MPS, AOPT_ARG, aopt_set_literal(0), aopt_set_string("mps"),
          "Set number of messages-per-second (default = 10000 - for under-load mode, or max - for "
          "ping-pong and throughput modes; for maximum use --mps=max; \n\t\t\t\t support --pps for "
          "old compatibility)." },
        { OPT_MPS, AOPT_ARG, aopt_set_literal(0), aopt_set_string("pps"), NULL },
        { 'm',                                                      AOPT_ARG,
          aopt_set_literal('m'),                                    aopt_set_string("msg-size"),
          "Use messages of size <size> bytes (minimum default 14)." },
        { 'r',
          AOPT_ARG,
          aopt_set_literal('r'),
          aopt_set_string("range"),
          "comes with -m <size>, randomly change the messages size in range: <size> +- <N>." },
        { OPT_CI_SIG_LVL,
          AOPT_OPTARG,
          aopt_set_literal(0),
          aopt_set_string("ci_sig_level"),
          "Normal confidence interval significance level for stat reported. Values are between 0 and 100 "
          "exclusive (default 99). " },
        { OPT_HISTOGRAM,
          AOPT_ARG,
          aopt_set_literal(0),
          aopt_set_string("histogram"),
          "Build histogram of latencies. Histogram arguments formated as binsize:lowerrange:upperrange " },
        { 0, AOPT_NOARG, aopt_set_literal(0), aopt_set_string(NULL), NULL }
    };

    /* Load supported option and create option objects */
    {
        int valid_argc = 0;
        int temp_argc = 0;

        temp_argc = argc;
        common_obj = aopt_init(&temp_argc, (const char **)argv, common_opt_desc);
        valid_argc += temp_argc;
        temp_argc = argc;
        client_obj = aopt_init(&temp_argc, (const char **)argv, client_opt_desc);
        valid_argc += temp_argc;
        temp_argc = argc;
        self_obj = aopt_init(&temp_argc, (const char **)argv, self_opt_desc);
        valid_argc += temp_argc;
        if (valid_argc < (argc - 1)) {
            rc = SOCKPERF_ERR_BAD_ARGUMENT;
        }
    }

    if (rc || aopt_check(common_obj, 'h')) {
        rc = -1;
    }

    /* Set default values */
    s_user_params.mode = MODE_CLIENT;
    s_user_params.mps = MPS_DEFAULT;
    s_user_params.reply_every = REPLY_EVERY_DEFAULT;

    /* Set command line common options */
    if (!rc) {
        rc = parse_common_opt(common_obj);
    }

    /* Set command line client values */
    if (!rc && client_obj) {
        rc = parse_client_opt(client_obj);
    }

    /* Set command line specific values */
    if (!rc && self_obj) {
        if (!rc && aopt_check(self_obj, 't')) {
            const char *optarg = aopt_value(self_obj, 't');
            if (optarg) {
                errno = 0;
                int value = strtol(optarg, NULL, 0);
                if (errno != 0 || value <= 0 || value > MAX_DURATION) {
                    log_msg("'-%c' Invalid duration: %s", 't', optarg);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                } else {
                    s_user_params.sec_test_duration = value;
                }
            } else {
                log_msg("'-%c' Invalid value", 't');
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(self_obj, 'b')) {
            const char *optarg = aopt_value(self_obj, 'b');
            if (optarg) {
                errno = 0;
                int value = strtol(optarg, NULL, 0);
                if (errno != 0 || value < 1) {
                    log_msg("'-%c' Invalid burst size: %s", 'b', optarg);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                } else {
                    s_user_params.burst_size = value;
                }
            } else {
                log_msg("'-%c' Invalid value", 'b');
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(self_obj, OPT_REPLY_EVERY)) {
            const char *optarg = aopt_value(self_obj, OPT_REPLY_EVERY);
            if (optarg) {
                errno = 0;
                long long value = strtol(optarg, NULL, 0);
                if (errno != 0 || value <= 0 || value > 1 << 30) {
                    log_msg("Invalid %d val: %s", OPT_REPLY_EVERY, optarg);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                } else {
                    s_user_params.reply_every = (uint32_t)value;
                }
            } else {
                log_msg("'-%d' Invalid value", OPT_REPLY_EVERY);
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(self_obj, OPT_MPS)) {
            const char *optarg = aopt_value(self_obj, OPT_MPS);
            if (optarg) {
                if (0 == strcmp("MAX", optarg) || 0 == strcmp("max", optarg)) {
                    s_user_params.mps = UINT32_MAX;
                } else {
                    errno = 0;
                    long long value = strtol(optarg, NULL, 0);
                    if (errno != 0 || value <= 0 || value > 1 << 30) {
                        log_msg("Invalid %d val: %s", OPT_MPS, optarg);
                        rc = SOCKPERF_ERR_BAD_ARGUMENT;
                    } else {
                        s_user_params.mps = (uint32_t)value;
                    }
                }
            } else {
                log_msg("'-%d' Invalid value", OPT_MPS);
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(self_obj, 'm')) {
            const char *optarg = aopt_value(self_obj, 'm');
            if (optarg) {
                errno = 0;
                int value = strtol(optarg, NULL, 0);
                if (errno != 0 || value < MIN_PAYLOAD_SIZE) {
                    log_msg("'-%c' Invalid message size: %s (min: %d)", 'm', optarg,
                            MIN_PAYLOAD_SIZE);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                } else if (!aopt_check(common_obj, OPT_TCP) && value > MAX_PAYLOAD_SIZE) {
                    log_msg("'-%c' Invalid message size: %s (max: %d)", 'm', optarg,
                            MAX_PAYLOAD_SIZE);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                } else if (aopt_check(common_obj, OPT_TCP) && value > MAX_TCP_SIZE) {
                    log_msg("'-%c' Invalid message size: %s (max: %d)", 'm', optarg, MAX_TCP_SIZE);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                } else {
                    MAX_PAYLOAD_SIZE = value;
                    s_user_params.msg_size = value;
                }
            } else {
                log_msg("'-%c' Invalid value", 'm');
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(self_obj, 'r')) {
            const char *optarg = aopt_value(self_obj, 'r');
            if (optarg && (isNumeric(optarg))) {
                errno = 0;
                int value = strtol(optarg, NULL, 0);
                if (errno != 0 || value < 0) {
                    log_msg("'-%c' Invalid message range: %s", 'r', optarg);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                } else {
                    s_user_params.msg_size_range = value;
                }
            } else {
                log_msg("'-%c' Invalid value", 'r');
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(self_obj, OPT_CI_SIG_LVL)) {
            const char *optarg = aopt_value(self_obj, OPT_CI_SIG_LVL);
            if (optarg) {
                errno = 0;
                int value = strtol(optarg, NULL, 0);
                if (errno != 0 || value <= 0 || value >= 100) {
                    log_msg("'--%s' Invalid Significance level: %s",
                        aopt_get_long_name(self_opt_desc, OPT_CI_SIG_LVL), optarg);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                } else {
                    s_user_params.ci_significance_level = value;
                }
            } else {
                log_msg("'--%s' Invalid value",
                    aopt_get_long_name(self_opt_desc, OPT_CI_SIG_LVL));
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(self_obj, OPT_HISTOGRAM)) {
            s_user_params.b_histogram = true;
            const char *optarg = aopt_value(self_obj, OPT_HISTOGRAM);

            if (optarg && optarg[0]) {
                char *buf = strdup(optarg);
                int required_args = 3;

                /* Parse histogram options list */
                char suffix; //< needed to check for garbage at the end
                if (sscanf(optarg, "%" PRIu32 ":%" PRIu32 ":%" PRIu32 "%c",
                    &s_user_params.histogram_bin_size, &s_user_params.histogram_lower_range,
                    &s_user_params.histogram_upper_range, &suffix) != required_args) {
                    log_err("Invalid argument: %s "
                            "Format should be binsize:lowerrange:upperrange", optarg);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                }

                if (buf) {
                    free(buf);
                }
            } else {
                log_msg("'--%s' Invalid value",
                    aopt_get_long_name(self_opt_desc, OPT_HISTOGRAM));
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }
    }

    if (!rc) {
        // --tcp option must be processed before
        rc = parse_client_bind_info(common_obj, self_obj);
    }

    if (rc) {
        const char *help_str = NULL;
        char temp_buf[30];

        printf("%s: %s\n", display_opt(id, temp_buf, sizeof(temp_buf)), sockperf_modes[id].note);
        printf("\n");
        printf("Usage: " MODULE_NAME " %s [options] [args]...\n", sockperf_modes[id].name);
        printf(" " MODULE_NAME
               " %s -i ip / --addr address [-p port] [-m message_size] [-t time] [--data_integrity]\n",
               sockperf_modes[id].name);
        printf(" " MODULE_NAME
               " %s -f file [-F s/p/e] [-m message_size] [-r msg_size_range] [-t time]\n",
               sockperf_modes[id].name);
        printf("\n");
        printf("Options:\n");
        help_str = aopt_help(common_opt_desc);
        if (help_str) {
            printf("%s\n", help_str);
            free((void *)help_str);
        }
        printf("Valid arguments:\n");
        help_str = aopt_help(client_opt_desc);
        if (help_str) {
            printf("%s", help_str);
            free((void *)help_str);
            help_str = aopt_help(self_opt_desc);
            if (help_str) {
                printf("%s", help_str);
                free((void *)help_str);
            }
            printf("\n");
        }
    }

    /* Destroy option objects */
    aopt_exit((AOPT_OBJECT *)common_obj);
    aopt_exit((AOPT_OBJECT *)client_obj);
    aopt_exit((AOPT_OBJECT *)self_obj);

    return rc;
}

//------------------------------------------------------------------------------
static int proc_mode_ping_pong(int id, int argc, const char **argv) {
    int rc = SOCKPERF_ERR_NONE;
    const AOPT_OBJECT *common_obj = NULL;
    const AOPT_OBJECT *client_obj = NULL;
    const AOPT_OBJECT *self_obj = NULL;

    /*
     * List of supported ping-pong options.
     */
    const AOPT_DESC self_opt_desc[] = {
        { 't',                                                 AOPT_ARG,
          aopt_set_literal('t'),                               aopt_set_string("time"),
          "Run for <sec> seconds (default 1, max = 36000000)." },
        { 'n',                                                 AOPT_ARG,
          aopt_set_literal('n'),                               aopt_set_string("number-of-packets"),
          "Run for n packets sent and received (default 0, max = 100000000)." },
        { OPT_CLIENTPORT,
          AOPT_ARG,
          aopt_set_literal(0),
          aopt_set_string("client_port"),
          "Force the client side to bind to a specific port (default = 0). " },
        { OPT_CLIENTADDR,
          AOPT_ARG,
          aopt_set_literal(0),
          aopt_set_string("client_ip", "client_addr"),
          "Force the client side to bind to a specific address in IPv4, IPv6, UNIX domain socket format (default = 0). " },
        { 'b',                                                             AOPT_ARG,
          aopt_set_literal('b'),                                           aopt_set_string("burst"),
          "Control the client's number of a messages sent in every burst." },
        { OPT_MPS, AOPT_ARG, aopt_set_literal(0), aopt_set_string("mps"),
          "Set number of messages-per-second (default = 10000 - for under-load mode, or max - for "
          "ping-pong and throughput modes; for maximum use --mps=max; \n\t\t\t\t support --pps for "
          "old compatibility)." },
        { OPT_MPS, AOPT_ARG, aopt_set_literal(0), aopt_set_string("pps"), NULL },
        { 'm',                                                      AOPT_ARG,
          aopt_set_literal('m'),                                    aopt_set_string("msg-size"),
          "Use messages of size <size> bytes (minimum default 14)." },
        { 'r',
          AOPT_ARG,
          aopt_set_literal('r'),
          aopt_set_string("range"),
          "comes with -m <size>, randomly change the messages size in range: <size> +- <N>." },
        { OPT_DATA_INTEGRITY,                AOPT_NOARG,                    aopt_set_literal(0),
          aopt_set_string("data-integrity"), "Perform data integrity test." },
        { OPT_CI_SIG_LVL,
          AOPT_OPTARG,
          aopt_set_literal(0),
          aopt_set_string("ci_sig_level"),
          "Normal confidence interval significance level for stat reported. Values are between 0 and 100 "
          "exclusive (default 99). " },
        { OPT_HISTOGRAM,
          AOPT_ARG,
          aopt_set_literal(0),
          aopt_set_string("histogram"),
          "Build histogram of latencies. Histogram arguments formated as binsize:lowerrange:upperrange " },
        { 0, AOPT_NOARG, aopt_set_literal(0), aopt_set_string(NULL), NULL }
    };

    /* Load supported option and create option objects */
    {
        int valid_argc = 0;
        int temp_argc = 0;

        temp_argc = argc;
        common_obj = aopt_init(&temp_argc, (const char **)argv, common_opt_desc);
        valid_argc += temp_argc;
        temp_argc = argc;
        client_obj = aopt_init(&temp_argc, (const char **)argv, client_opt_desc);
        valid_argc += temp_argc;
        temp_argc = argc;
        self_obj = aopt_init(&temp_argc, (const char **)argv, self_opt_desc);
        valid_argc += temp_argc;
        if (valid_argc < (argc - 1)) {
            rc = SOCKPERF_ERR_BAD_ARGUMENT;
        }
    }

    if (rc || aopt_check(common_obj, 'h')) {
        rc = -1;
    }

    /* Set default values */
    s_user_params.mode = MODE_CLIENT;
    s_user_params.measurement = TIME_BASED;
    s_user_params.b_client_ping_pong = true;
    s_user_params.mps = UINT32_MAX;
    s_user_params.reply_every = 1;

    /* Set command line common options */
    if (!rc) {
        rc = parse_common_opt(common_obj);
    }

    /* Set command line client values */
    if (!rc && client_obj) {
        rc = parse_client_opt(client_obj);
    }

    /* Set command line specific values */
    if (!rc && self_obj) {
        if (!rc && aopt_check(self_obj, 'n')) {
            if (!aopt_check(self_obj, 't') && !aopt_check(self_obj, OPT_MPS)) {
                const char *optarg = aopt_value(self_obj, 'n');
                if (optarg) {
                    errno = 0;
                    int value = strtol(optarg, NULL, 0);
                    if (errno != 0 || value <= 0 || value > MAX_PACKET_NUMBER) {
                        log_msg("'-%c' Invalid number of packets: %s", 'n', optarg);
                        rc = SOCKPERF_ERR_BAD_ARGUMENT;
                    } else {
                        s_user_params.measurement = NUMBER_BASED;
                        s_user_params.number_test_target = value;
                    }
                } else {
                    log_msg("'-%c' Invalid value", 'n');
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                }
            } else {
                log_msg("-n conflicts with -t,--mps options");
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(self_obj, 't')) {
            if (!aopt_check(self_obj, 'n')) {
                const char *optarg = aopt_value(self_obj, 't');
                if (optarg) {
                    errno = 0;
                    int value = strtol(optarg, NULL, 0);
                    if (errno != 0 || value <= 0 || value > MAX_DURATION) {
                        log_msg("'-%c' Invalid duration: %s", 't', optarg);
                        rc = SOCKPERF_ERR_BAD_ARGUMENT;
                    } else {
                        s_user_params.measurement = TIME_BASED;
                        s_user_params.sec_test_duration = value;
                    }
                } else {
                    log_msg("'-%c' Invalid value", 't');
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                }
            } else {
                log_msg("-t conflicts with -n option");
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(self_obj, 'b')) {
            const char *optarg = aopt_value(self_obj, 'b');
            if (optarg) {
                errno = 0;
                int value = strtol(optarg, NULL, 0);
                if (errno != 0 || value < 1) {
                    log_msg("'-%c' Invalid burst size: %s", 'b', optarg);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                } else {
                    s_user_params.burst_size = value;
                }
            } else {
                log_msg("'-%c' Invalid value", 'b');
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(self_obj, OPT_MPS)) {
            const char *optarg = aopt_value(self_obj, OPT_MPS);
            if (optarg) {
                if (0 == strcmp("MAX", optarg) || 0 == strcmp("max", optarg)) {
                    s_user_params.mps = UINT32_MAX;
                } else {
                    errno = 0;
                    long long value = strtol(optarg, NULL, 0);
                    if (errno != 0 || value <= 0 || value > 1 << 30) {
                        log_msg("Invalid %d val: %s", OPT_MPS, optarg);
                        rc = SOCKPERF_ERR_BAD_ARGUMENT;
                    } else {
                        s_user_params.mps = (uint32_t)value;
                    }
                }
            } else {
                log_msg("'-%d' Invalid value", OPT_MPS);
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(self_obj, 'm')) {
            const char *optarg = aopt_value(self_obj, 'm');
            if (optarg) {
                errno = 0;
                int value = strtol(optarg, NULL, 0);
                if (errno != 0 || value < MIN_PAYLOAD_SIZE) {
                    log_msg("'-%c' Invalid message size: %s (min: %d)", 'm', optarg,
                            MIN_PAYLOAD_SIZE);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                } else if (!aopt_check(common_obj, OPT_TCP) && value > MAX_PAYLOAD_SIZE) {
                    log_msg("'-%c' Invalid message size: %s (max: %d)", 'm', optarg,
                            MAX_PAYLOAD_SIZE);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                } else if (aopt_check(common_obj, OPT_TCP) && value > MAX_TCP_SIZE) {
                    log_msg("'-%c' Invalid message size: %s (max: %d)", 'm', optarg, MAX_TCP_SIZE);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                } else {
                    MAX_PAYLOAD_SIZE = value;
                    s_user_params.msg_size = value;
                }
            } else {
                log_msg("'-%c' Invalid value", 'm');
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(self_obj, 'r')) {
            const char *optarg = aopt_value(self_obj, 'r');
            if (optarg && (isNumeric(optarg))) {
                errno = 0;
                int value = strtol(optarg, NULL, 0);
                if (errno != 0 || value < 0) {
                    log_msg("'-%c' Invalid message range: %s", 'r', optarg);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                } else {
                    s_user_params.msg_size_range = value;
                }
            } else {
                log_msg("'-%c' Invalid value", 'r');
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(self_obj, OPT_DATA_INTEGRITY)) {
            if (!aopt_check(self_obj, 'b')) {
                s_user_params.data_integrity = true;
            } else {
                log_msg("--data-integrity conflicts with -b option");
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(self_obj, OPT_CI_SIG_LVL)) {
            const char *optarg = aopt_value(self_obj, OPT_CI_SIG_LVL);
            if (optarg) {
                errno = 0;
                int value = strtol(optarg, NULL, 0);
                if (errno != 0 || value <= 0 || value >= 100) {
                    log_msg("'--%s' Invalid Significance level: %s",
                        aopt_get_long_name(self_opt_desc, OPT_CI_SIG_LVL), optarg);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                } else {
                    s_user_params.ci_significance_level = value;
                }
            } else {
                log_msg("'--%s' Invalid value",
                    aopt_get_long_name(self_opt_desc, OPT_CI_SIG_LVL));
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(self_obj, OPT_HISTOGRAM)) {
            s_user_params.b_histogram = true;
            const char *optarg = aopt_value(self_obj, OPT_HISTOGRAM);

            if (optarg && optarg[0]) {
                char *buf = strdup(optarg);
                int required_args = 3;

                /* Parse histogram options list */
                char suffix; //< needed to check for garbage at the end
                if (sscanf(optarg, "%" PRIu32 ":%" PRIu32 ":%" PRIu32 "%c",
                    &s_user_params.histogram_bin_size, &s_user_params.histogram_lower_range,
                    &s_user_params.histogram_upper_range, &suffix) != required_args) {
                    log_err("Invalid argument: %s "
                            "Format should be binsize:lowerrange:upperrange", optarg);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                }

                if (buf) {
                    free(buf);
                }
            } else {
                log_msg("'--%s' Invalid value",
                    aopt_get_long_name(self_opt_desc, OPT_HISTOGRAM));
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }
    }

    if (!rc) {
        // --tcp option must be processed before
        rc = parse_client_bind_info(common_obj, self_obj);
    }

    if (rc) {
        const char *help_str = NULL;
        char temp_buf[30];

        printf("%s: %s\n", display_opt(id, temp_buf, sizeof(temp_buf)), sockperf_modes[id].note);
        printf("\n");
        printf("Usage: " MODULE_NAME " %s [options] [args]...\n", sockperf_modes[id].name);
        printf(" " MODULE_NAME " %s -i ip / --addr address [-p port] [-m message_size] [-t time | -n number-of-packets]\n",
               sockperf_modes[id].name);
        printf(" " MODULE_NAME
               " %s -f file [-F s/p/e] [-m message_size] [-r msg_size_range] [-t time | -n number-of-packets]\n",
               sockperf_modes[id].name);
        printf("\n");
        printf("Options:\n");
        help_str = aopt_help(common_opt_desc);
        if (help_str) {
            printf("%s\n", help_str);
            free((void *)help_str);
        }
        printf("Valid arguments:\n");
        help_str = aopt_help(client_opt_desc);
        if (help_str) {
            printf("%s", help_str);
            free((void *)help_str);
            help_str = aopt_help(self_opt_desc);
            if (help_str) {
                printf("%s", help_str);
                free((void *)help_str);
            }
            printf("\n");
        }
    }

    /* Destroy option objects */
    aopt_exit((AOPT_OBJECT *)common_obj);
    aopt_exit((AOPT_OBJECT *)client_obj);
    aopt_exit((AOPT_OBJECT *)self_obj);

    /* It is set to reduce memory needed for PacketTime buffer */
    if (s_user_params.mps == UINT32_MAX) { // MAX MPS mode
        MPS_MAX = MPS_MAX_PP * s_user_params.burst_size;
        if (MPS_MAX > MPS_MAX_UL) MPS_MAX = MPS_MAX_UL;
    }

    return rc;
}

//------------------------------------------------------------------------------
static int proc_mode_throughput(int id, int argc, const char **argv) {
    int rc = SOCKPERF_ERR_NONE;
    const AOPT_OBJECT *common_obj = NULL;
    const AOPT_OBJECT *client_obj = NULL;
    const AOPT_OBJECT *self_obj = NULL;

    /*
     * List of supported throughput options.
     */
    const AOPT_DESC self_opt_desc[] = {
        { 't',                                                 AOPT_ARG,
          aopt_set_literal('t'),                               aopt_set_string("time"),
          "Run for <sec> seconds (default 1, max = 36000000)." },
        { OPT_CLIENTPORT,
          AOPT_ARG,
          aopt_set_literal(0),
          aopt_set_string("client_port"),
          "Force the client side to bind to a specific port (default = 0). " },
        { OPT_CLIENTADDR,
          AOPT_ARG,
          aopt_set_literal(0),
          aopt_set_string("client_ip", "client_addr"),
          "Force the client side to bind to a specific address in IPv4, IPv6, UNIX domain socket format (default = 0). " },
        { 'b',                                                             AOPT_ARG,
          aopt_set_literal('b'),                                           aopt_set_string("burst"),
          "Control the client's number of a messages sent in every burst." },
        { OPT_MPS, AOPT_ARG, aopt_set_literal(0), aopt_set_string("mps"),
          "Set number of messages-per-second (default = 10000 - for under-load mode, or max - for "
          "ping-pong and throughput modes; for maximum use --mps=max; \n\t\t\t\t support --pps for "
          "old compatibility)." },
        { OPT_MPS, AOPT_ARG, aopt_set_literal(0), aopt_set_string("pps"), NULL },
        { 'm',                                                      AOPT_ARG,
          aopt_set_literal('m'),                                    aopt_set_string("msg-size"),
          "Use messages of size <size> bytes (minimum default 14)." },
        { 'r',
          AOPT_ARG,
          aopt_set_literal('r'),
          aopt_set_string("range"),
          "comes with -m <size>, randomly change the messages size in range: <size> +- <N>." },
        { 0, AOPT_NOARG, aopt_set_literal(0), aopt_set_string(NULL), NULL }
    };

    /* Load supported option and create option objects */
    {
        int valid_argc = 0;
        int temp_argc = 0;

        temp_argc = argc;
        common_obj = aopt_init(&temp_argc, (const char **)argv, common_opt_desc);
        valid_argc += temp_argc;
        temp_argc = argc;
        client_obj = aopt_init(&temp_argc, (const char **)argv, client_opt_desc);
        valid_argc += temp_argc;
        temp_argc = argc;
        self_obj = aopt_init(&temp_argc, (const char **)argv, self_opt_desc);
        valid_argc += temp_argc;
        if (valid_argc < (argc - 1)) {
            rc = SOCKPERF_ERR_BAD_ARGUMENT;
        }
    }

    if (rc || aopt_check(common_obj, 'h')) {
        rc = -1;
    }

    /* Set default values */
    s_user_params.mode = MODE_CLIENT;
    s_user_params.b_stream = true;
    s_user_params.mps = UINT32_MAX;
    s_user_params.reply_every = 1 << (8 * sizeof(s_user_params.reply_every) - 2);

    /* Set command line common options */
    if (!rc) {
        rc = parse_common_opt(common_obj);
    }

    /* Set command line client values */
    if (!rc && client_obj) {
        rc = parse_client_opt(client_obj);
    }

    /* Set command line specific values */
    if (!rc && self_obj) {
        if (!rc && aopt_check(self_obj, 't')) {
            const char *optarg = aopt_value(self_obj, 't');
            if (optarg) {
                errno = 0;
                int value = strtol(optarg, NULL, 0);
                if (errno != 0 || value <= 0 || value > MAX_DURATION) {
                    log_msg("'-%c' Invalid duration: %s", 't', optarg);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                } else {
                    s_user_params.sec_test_duration = value;
                }
            } else {
                log_msg("'-%c' Invalid value", 't');
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(self_obj, 'b')) {
            const char *optarg = aopt_value(self_obj, 'b');
            if (optarg) {
                errno = 0;
                int value = strtol(optarg, NULL, 0);
                if (errno != 0 || value < 1) {
                    log_msg("'-%c' Invalid burst size: %s", 'b', optarg);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                } else {
                    s_user_params.burst_size = value;
                }
            } else {
                log_msg("'-%c' Invalid value", 'b');
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(self_obj, OPT_MPS)) {
            const char *optarg = aopt_value(self_obj, OPT_MPS);
            if (optarg) {
                if (0 == strcmp("MAX", optarg) || 0 == strcmp("max", optarg)) {
                    s_user_params.mps = UINT32_MAX;
                } else {
                    errno = 0;
                    long long value = strtol(optarg, NULL, 0);
                    if (errno != 0 || value <= 0 || value > 1 << 30) {
                        log_msg("Invalid %d val: %s", OPT_MPS, optarg);
                        rc = SOCKPERF_ERR_BAD_ARGUMENT;
                    } else {
                        s_user_params.mps = (uint32_t)value;
                    }
                }
            } else {
                log_msg("'-%d' Invalid value", OPT_MPS);
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(self_obj, 'm')) {
            const char *optarg = aopt_value(self_obj, 'm');
            if (optarg) {
                errno = 0;
                int value = strtol(optarg, NULL, 0);
                if (errno != 0 || value < MIN_PAYLOAD_SIZE) {
                    log_msg("'-%c' Invalid message size: %s (min: %d)", 'm', optarg,
                            MIN_PAYLOAD_SIZE);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                } else if (!aopt_check(common_obj, OPT_TCP) && value > MAX_PAYLOAD_SIZE) {
                    log_msg("'-%c' Invalid message size: %s (max: %d)", 'm', optarg,
                            MAX_PAYLOAD_SIZE);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                } else if (aopt_check(common_obj, OPT_TCP) && value > MAX_TCP_SIZE) {
                    log_msg("'-%c' Invalid message size: %s (max: %d)", 'm', optarg, MAX_TCP_SIZE);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                } else {
                    MAX_PAYLOAD_SIZE = value;
                    s_user_params.msg_size = value;
                }
            } else {
                log_msg("'-%c' Invalid value", 'm');
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(self_obj, 'r')) {
            const char *optarg = aopt_value(self_obj, 'r');
            if (optarg && (isNumeric(optarg))) {
                errno = 0;
                int value = strtol(optarg, NULL, 0);
                if (errno != 0 || value < 0) {
                    log_msg("'-%c' Invalid message range: %s", 'r', optarg);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                } else {
                    s_user_params.msg_size_range = value;
                }
            } else {
                log_msg("'-%c' Invalid value", 'r');
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }
    }

    if (!rc) {
        // --tcp option must be processed before
        rc = parse_client_bind_info(common_obj, self_obj);
    }

    if (rc) {
        const char *help_str = NULL;
        char temp_buf[30];

        printf("%s: %s\n", display_opt(id, temp_buf, sizeof(temp_buf)), sockperf_modes[id].note);
        printf("\n");
        printf("Usage: " MODULE_NAME " %s [options] [args]...\n", sockperf_modes[id].name);
        printf(" " MODULE_NAME " %s -i ip / --addr address [-p port] [-m message_size] [-t time]\n",
               sockperf_modes[id].name);
        printf(" " MODULE_NAME
               " %s -f file [-F s/p/e] [-m message_size] [-r msg_size_range] [-t time]\n",
               sockperf_modes[id].name);
        printf("\n");
        printf("Options:\n");
        help_str = aopt_help(common_opt_desc);
        if (help_str) {
            printf("%s\n", help_str);
            free((void *)help_str);
        }
        printf("Valid arguments:\n");
        help_str = aopt_help(client_opt_desc);
        if (help_str) {
            printf("%s", help_str);
            free((void *)help_str);
            help_str = aopt_help(self_opt_desc);
            if (help_str) {
                printf("%s", help_str);
                free((void *)help_str);
            }
            printf("\n");
        }
    }

    /* Destroy option objects */
    aopt_exit((AOPT_OBJECT *)common_obj);
    aopt_exit((AOPT_OBJECT *)client_obj);
    aopt_exit((AOPT_OBJECT *)self_obj);

    // In Throughput the default is nagel enabled while the rest mode uses nagel disabled.
    s_user_params.tcp_nodelay = !s_user_params.tcp_nodelay;

    return rc;
}

//------------------------------------------------------------------------------
static int proc_mode_playback(int id, int argc, const char **argv) {
    int rc = SOCKPERF_ERR_NONE;
    const AOPT_OBJECT *common_obj = NULL;
    const AOPT_OBJECT *client_obj = NULL;
    const AOPT_OBJECT *self_obj = NULL;

    /*
     * List of supported under-load options.
     */
    const AOPT_DESC self_opt_desc[] = {
        { OPT_REPLY_EVERY,
          AOPT_ARG,
          aopt_set_literal(0),
          aopt_set_string("reply-every"),
          "Set number of send messages between reply messages (default = 100)." },
        { OPT_PLAYBACK_DATA,                                         AOPT_ARG,
          aopt_set_literal(0),                                       aopt_set_string("data-file"),
          "Pre-prepared CSV file with timestamps and message sizes." },
        { OPT_CI_SIG_LVL,
          AOPT_OPTARG,
          aopt_set_literal(0),
          aopt_set_string("ci_sig_level"),
          "Normal confidence interval significance level for stat reported. Values are between 0 and 100 "
          "exclusive (default 99). " },
        { OPT_HISTOGRAM,
          AOPT_ARG,
          aopt_set_literal(0),
          aopt_set_string("histogram"),
          "Build histogram of latencies. Histogram arguments formated as binsize:lowerrange:upperrange " },
        { 0, AOPT_NOARG, aopt_set_literal(0), aopt_set_string(NULL), NULL }
    };

    /* Load supported option and create option objects */
    {
        int valid_argc = 0;
        int temp_argc = 0;

        temp_argc = argc;
        common_obj = aopt_init(&temp_argc, (const char **)argv, common_opt_desc);
        valid_argc += temp_argc;
        temp_argc = argc;
        client_obj = aopt_init(&temp_argc, (const char **)argv, client_opt_desc);
        valid_argc += temp_argc;
        temp_argc = argc;
        self_obj = aopt_init(&temp_argc, (const char **)argv, self_opt_desc);
        valid_argc += temp_argc;
        if (valid_argc < (argc - 1)) {
            rc = SOCKPERF_ERR_BAD_ARGUMENT;
        }
    }

    if (rc || aopt_check(common_obj, 'h')) {
        rc = -1;
    }

    /* Set default values */
    s_user_params.mode = MODE_CLIENT;
    s_user_params.mps = MPS_DEFAULT;
    s_user_params.reply_every = REPLY_EVERY_DEFAULT;

    /* Set command line common options */
    if (!rc) {
        rc = parse_common_opt(common_obj);
    }

    /* Set command line client values */
    if (!rc && client_obj) {
        rc = parse_client_opt(client_obj);
    }

    /* Set command line specific values */
    if (!rc && self_obj) {
        if (!rc && aopt_check(self_obj, OPT_PLAYBACK_DATA)) {
            const char *optarg = aopt_value(self_obj, OPT_PLAYBACK_DATA);
            if (optarg) {
                static PlaybackVector pv;
                loadPlaybackData(pv, optarg);
                s_user_params.pPlaybackVector = &pv;
            } else {
                log_msg("'-%d' Invalid value", OPT_PLAYBACK_DATA);
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(self_obj, OPT_REPLY_EVERY)) {
            const char *optarg = aopt_value(self_obj, OPT_REPLY_EVERY);
            if (optarg) {
                errno = 0;
                long long value = strtol(optarg, NULL, 0);
                if (errno != 0 || value <= 0 || value > 1 << 30) {
                    log_msg("Invalid %d val: %s", OPT_REPLY_EVERY, optarg);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                } else {
                    s_user_params.reply_every = (uint32_t)value;
                }
            } else {
                log_msg("'-%d' Invalid value", OPT_REPLY_EVERY);
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(self_obj, OPT_CI_SIG_LVL)) {
            const char *optarg = aopt_value(self_obj, OPT_CI_SIG_LVL);
            if (optarg) {
                errno = 0;
                int value = strtol(optarg, NULL, 0);
                if (errno != 0 || value <= 0 || value >= 100) {
                    log_msg("'--%s' Invalid Significance level: %s",
                        aopt_get_long_name(self_opt_desc, OPT_CI_SIG_LVL), optarg);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                } else {
                    s_user_params.ci_significance_level = value;
                }
            } else {
                log_msg("'--%s' Invalid value",
                    aopt_get_long_name(self_opt_desc, OPT_CI_SIG_LVL));
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(self_obj, OPT_HISTOGRAM)) {
            s_user_params.b_histogram = true;
            const char *optarg = aopt_value(self_obj, OPT_HISTOGRAM);

            if (optarg && optarg[0]) {
                char *buf = strdup(optarg);
                int required_args = 3;

                /* Parse histogram options list */
                char suffix; //< needed to check for garbage at the end
                if (sscanf(optarg, "%" PRIu32 ":%" PRIu32 ":%" PRIu32 "%c",
                    &s_user_params.histogram_bin_size, &s_user_params.histogram_lower_range,
                    &s_user_params.histogram_upper_range, &suffix) != required_args) {
                    log_err("Invalid argument: %s "
                            "Format should be binsize:lowerrange:upperrange", optarg);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                }

                if (buf) {
                    free(buf);
                }
            } else {
                log_msg("'--%s' Invalid value",
                    aopt_get_long_name(self_opt_desc, OPT_HISTOGRAM));
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }
    }

    /* force --data-file */
    if (!rc && (!self_obj || (self_obj && !aopt_check(self_obj, OPT_PLAYBACK_DATA)))) {
        log_msg("--data-file must be used with playback mode");
        rc = SOCKPERF_ERR_BAD_ARGUMENT;
    }

    if (rc) {
        const char *help_str = NULL;
        char temp_buf[30];

        printf("%s: %s\n", display_opt(id, temp_buf, sizeof(temp_buf)), sockperf_modes[id].note);
        printf("\n");
        printf("Usage: " MODULE_NAME " %s [options] [args]...\n", sockperf_modes[id].name);
        printf(" " MODULE_NAME " %s -i ip / --addr address [-p port] --data-file playback.csv\n",
               sockperf_modes[id].name);
        printf("\n");
        printf("Options:\n");
        help_str = aopt_help(common_opt_desc);
        if (help_str) {
            printf("%s\n", help_str);
            free((void *)help_str);
        }
        printf("Valid arguments:\n");
        help_str = aopt_help(client_opt_desc);
        if (help_str) {
            printf("%s", help_str);
            free((void *)help_str);
            help_str = aopt_help(self_opt_desc);
            if (help_str) {
                printf("%s", help_str);
                free((void *)help_str);
            }
            printf("\n");
        }
    }

    /* Destroy option objects */
    aopt_exit((AOPT_OBJECT *)common_obj);
    aopt_exit((AOPT_OBJECT *)client_obj);
    aopt_exit((AOPT_OBJECT *)self_obj);

    return rc;
}

//------------------------------------------------------------------------------
static int proc_mode_server(int id, int argc, const char **argv) {
    int rc = SOCKPERF_ERR_NONE;
    const AOPT_OBJECT *common_obj = NULL;
    const AOPT_OBJECT *server_obj = NULL;

    /*
     * List of supported server options.
     */
    static const AOPT_DESC server_opt_desc[] = {
        /*
                {
                    'B', AOPT_NOARG,	aopt_set_literal( 'B' ),	aopt_set_string( "Bridge" ),
                    "Run in Bridge mode."
                },
        */
        { OPT_THREADS_NUM,                                         AOPT_ARG,
          aopt_set_literal(0),                                     aopt_set_string("threads-num"),
          "Run <N> threads on server side (requires '-f' option)." },
        { OPT_THREADS_AFFINITY,
          AOPT_ARG,
          aopt_set_literal(0),
          aopt_set_string("cpu-affinity"),
          "Set threads affinity to the given core ids in list format (see: cat /proc/cpuinfo)." },
#ifndef __windows__
        { OPT_RXFILTERCB,
          AOPT_NOARG,
          aopt_set_literal(0),
          aopt_set_string("rxfiltercb", "vmarxfiltercb"),
          "Use VMA's receive path message filter callback API (See VMA's readme)." },
#endif
        { OPT_FORCE_UC_REPLY,                  AOPT_NOARG,
          aopt_set_literal(0),                 aopt_set_string("force-unicast-reply"),
          "Force server to reply via unicast." },
        { OPT_DONT_REPLY,                              AOPT_NOARG,
          aopt_set_literal(0),                         aopt_set_string("dont-reply"),
          "Server won't reply to the client messages." },
        { 'm',
          AOPT_ARG,
          aopt_set_literal('m'),
          aopt_set_string("msg-size"),
          "Set maximum message size that the server can receive <size> bytes (default 65507)." },
        { 'g',                              AOPT_NOARG,             aopt_set_literal('g'),
          aopt_set_string("gap-detection"), "Enable gap-detection." },
        { 0, AOPT_NOARG, aopt_set_literal(0), aopt_set_string(NULL), NULL }
    };

    /* Load supported option and create option objects */
    {
        int valid_argc = 0;
        int temp_argc = 0;

        temp_argc = argc;
        common_obj = aopt_init(&temp_argc, (const char **)argv, common_opt_desc);
        valid_argc += temp_argc;
        temp_argc = argc;
        server_obj = aopt_init(&temp_argc, (const char **)argv, server_opt_desc);
        valid_argc += temp_argc;
        if (valid_argc < (argc - 1)) {
            rc = SOCKPERF_ERR_BAD_ARGUMENT;
        }
    }

    if (rc || aopt_check(common_obj, 'h')) {
        rc = -1;
    }

    /* Set default values */
    s_user_params.mode = MODE_SERVER;
    s_user_params.mps = MPS_DEFAULT;
    s_user_params.msg_size = MAX_PAYLOAD_SIZE;

    /* Set command line common options */
    if (!rc) {
        rc = parse_common_opt(common_obj);
    }

    /* Set command line specific values */
    if (!rc && server_obj) {
        struct sockaddr_store_t *p_addr = &s_user_params.addr;

        if (!rc && aopt_check(server_obj, 'B')) {
            log_msg("update to bridge mode");
            s_user_params.mode = MODE_BRIDGE;
            sockaddr_set_portn(*p_addr, htons(5001)); /*iperf's default port*/
        }

        if (!rc && aopt_check(server_obj, OPT_THREADS_NUM)) {
            if (aopt_check(common_obj, 'f')) {
                const char *optarg = aopt_value(server_obj, OPT_THREADS_NUM);
                if (optarg) {
                    s_user_params.mthread_server = 1;
                    errno = 0;
                    int threads_num = strtol(optarg, NULL, 0);
                    if (errno != 0 || threads_num <= 0) {
                        log_msg("'-%d' Invalid threads number: %s", OPT_THREADS_NUM, optarg);
                        rc = SOCKPERF_ERR_BAD_ARGUMENT;
                    } else {
                        s_user_params.threads_num = threads_num;
                    }
                } else {
                    log_msg("'-%d' Invalid value", OPT_THREADS_NUM);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                }
            } else {
                log_msg("--threads-num must be used with feed file (option '-f')");
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(server_obj, OPT_THREADS_AFFINITY)) {
            const char *optarg = aopt_value(server_obj, OPT_THREADS_AFFINITY);
            if (optarg) {
                strcpy(s_user_params.threads_affinity, optarg);
            } else {
                log_msg("'-%d' Invalid threads affinity", OPT_THREADS_AFFINITY);
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }
#ifndef __windows__
        if (!rc && aopt_check(server_obj, OPT_RXFILTERCB)) {
            s_user_params.is_rxfiltercb = true;
        }
#endif

        if (!rc && aopt_check(server_obj, OPT_FORCE_UC_REPLY)) {
            s_user_params.b_server_reply_via_uc = true;
        }
        if (!rc && aopt_check(server_obj, OPT_DONT_REPLY)) {
            if (aopt_check(common_obj, OPT_TCP)) {
                log_msg("--tcp conflicts with --dont-reply option");
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            } else {
                s_user_params.b_server_dont_reply = true;
            }
        }
        if (!rc && aopt_check(server_obj, 'm')) {
            const char *optarg = aopt_value(server_obj, 'm');
            if (optarg) {
                int value = strtol(optarg, NULL, 0);
                if ((value > MAX_TCP_SIZE) || !(isNumeric(optarg))) {
                    log_msg("'-%c' Invalid message size: %s (max: %d)", 'm', optarg, MAX_TCP_SIZE);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                } else {
                    MAX_PAYLOAD_SIZE = _max(MAX_PAYLOAD_SIZE, value);
                }
            }
        }

        if (!rc && aopt_check(server_obj, 'g')) {
            s_user_params.b_server_detect_gaps = true;
        }
    }

    if (rc) {
        const char *help_str = NULL;
        char temp_buf[30];

        printf("%s: %s\n", display_opt(id, temp_buf, sizeof(temp_buf)), sockperf_modes[id].note);
        printf("\n");
        printf("Usage: " MODULE_NAME " %s [options] [args]...\n", sockperf_modes[id].name);
        printf(" " MODULE_NAME " %s\n", sockperf_modes[id].name);
        printf(" " MODULE_NAME
               " %s [-i ip / --addr address] [-p port] [--mc-rx-if ip] [--mc-tx-if ip] [--mc-source-filter ip]\n",
               sockperf_modes[id].name);
#ifndef __windows__
        printf(" " MODULE_NAME
               " %s -f file [-F s/p/e] [--mc-rx-if ip] [--mc-tx-if ip] [--mc-source-filter ip]\n",
               sockperf_modes[id].name);
#else
        printf(" " MODULE_NAME
               " %s -f file [-F s] [--mc-rx-if ip] [--mc-tx-if ip] [--mc-source-filter ip]\n",
               sockperf_modes[id].name);
#endif
        printf("\n");
        printf("Options:\n");
        help_str = aopt_help(common_opt_desc);
        if (help_str) {
            printf("%s\n", help_str);
            free((void *)help_str);
        }
        printf("Valid arguments:\n");
        help_str = aopt_help(server_opt_desc);
        if (help_str) {
            printf("%s\n", help_str);
            free((void *)help_str);
        }
    }

    /* Destroy option objects */
    aopt_exit((AOPT_OBJECT *)common_obj);
    aopt_exit((AOPT_OBJECT *)server_obj);

    return rc;
}

// Get resolved socket address from getaddrinfo() result. IPv6 is preferred.
static void get_socket_address(struct addrinfo *result, struct sockaddr *addr, socklen_t &addrlen)
{
    for (addrinfo *rp = result; rp != NULL; rp = rp->ai_next) {
        if (rp->ai_family == AF_INET6) {
            std::memcpy(addr, rp->ai_addr, rp->ai_addrlen);
            addrlen = static_cast<socklen_t>(rp->ai_addrlen);
            return;
        }
    }
    for (addrinfo *rp = result; rp != NULL; rp = rp->ai_next) {
        std::memcpy(addr, rp->ai_addr, rp->ai_addrlen);
        addrlen = static_cast<socklen_t>(rp->ai_addrlen);
        return;
    }
    // This point is unreachable because getaddrinfo() returns EAI_NODATA if
    // there are no network addresses defined for a host
}

//------------------------------------------------------------------------------
static int resolve_sockaddr(const char *host, const char *port, int sock_type,
        bool is_server_mode, sockaddr *addr, socklen_t &addr_len)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    // allow IPv4 or IPv6
    hints.ai_family = AF_UNSPEC;
    // return addresses only from configured address families
    hints.ai_flags = AI_ADDRCONFIG;
    // if host is NULL then server address will be 0.0.0.0/:: and client address will be 127.0.0.1/::1
    if (is_server_mode) {
        // use wilcard address if host is NULL
        hints.ai_flags |= AI_PASSIVE;
    }
    // any protocol
    hints.ai_protocol = 0;
    hints.ai_socktype = sock_type;
    int res;
    struct addrinfo *result;
#ifdef NEED_REGEX_WORKAROUND
    regex_t regexpr_unix;
    int regexp_unix_error = regcomp(&regexpr_unix, RESOLVE_ADDR_FORMAT_SOCKET, REG_EXTENDED);
    if (regexp_unix_error != 0) {
        log_msg("Failed to compile regexp for unix format");
        res = SOCKPERF_ERR_FATAL;
        return res;
    }
#else
    const std::regex regexpr_unix(RESOLVE_ADDR_FORMAT_SOCKET);
#endif //NEED_REGEX_WORKAROUND
    if (host != NULL) {
        std::string path = host;
        struct sockaddr_store_t *tmp = ((sockaddr_store_t *)addr);
#ifdef NEED_REGEX_WORKAROUND
        if (path.size() > 0 && regexec(&regexpr_unix, path.c_str(), 0, NULL, 0) == 0) {
#else
        if (path.size() > 0 && std::regex_match(path, regexpr_unix)) {
#endif //NEED_REGEX_WORKAROUND
            log_dbg("provided path for Unix Domain Socket is %s\n", host);
            if (path.length() >= sizeof(tmp->addr_un.sun_path)) {
                log_err("length of name is greater-equal %zu bytes", sizeof(tmp->addr_un.sun_path));
                res = SOCKPERF_ERR_SOCKET;
                return res;
            }
            addr_len = sizeof(struct sockaddr_un);
            memset(tmp->addr_un.sun_path, 0, sizeof(tmp->addr_un.sun_path));
            memcpy(tmp->addr_un.sun_path, path.c_str(), path.length());
            tmp->ss_family = AF_UNIX;

#ifdef NEED_REGEX_WORKAROUND
            regfree(&regexpr_unix);
#endif // NEED_REGEX_WORKAROUND
            return 0;
        }
    }
    res = getaddrinfo(host, port, &hints, &result);
    if (res == 0) {
        get_socket_address(result, addr, addr_len);
        freeaddrinfo(result);
    }

    return res;
}

//------------------------------------------------------------------------------
static int parse_common_opt(const AOPT_OBJECT *common_obj) {
    int rc = SOCKPERF_ERR_NONE;
    const char *host_str = NULL;
    const char *port_str = DEFAULT_PORT_STR;

    if (common_obj) {
        int *p_daemonize = &s_user_params.daemonize;
        char *feedfile_name = s_user_params.feedfile_name;

        if (!rc && aopt_check(common_obj, 'd')) {
            g_debug_level = LOG_LVL_DEBUG;
        }

        if (!rc && aopt_check(common_obj, 'i')) {
            const char *optarg = aopt_value(common_obj, 'i');
            if (!optarg) {
                log_msg("'-%c' Invalid address", 'i');
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            } else {
                host_str = optarg;
                s_user_params.fd_handler_type = RECVFROM;
            }
        }

        if (!rc && aopt_check(common_obj, 'p')) {
            const char *optarg = aopt_value(common_obj, 'p');
            if (optarg && (isNumeric(optarg))) {
                port_str = optarg;
                s_user_params.fd_handler_type = RECVFROM;
            } else {
                log_msg("'-%c' Invalid port", 'p');
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(common_obj, 'f')) {
            if (!aopt_check(common_obj, 'i') && !aopt_check(common_obj, 'p')) {
                const char *optarg = aopt_value(common_obj, 'f');
                if (optarg) {
                    strncpy(feedfile_name, optarg, MAX_ARGV_SIZE);
                    feedfile_name[MAX_PATH_LENGTH - 1] = '\0';
#if defined(__windows__) || defined(__FreeBSD__)
                    s_user_params.fd_handler_type = SELECT;
#else
                    s_user_params.fd_handler_type = EPOLL;
#endif
                } else {
                    log_msg("'-%c' Invalid value", 'f');
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                }
            } else {
                log_msg("-f conflicts with -i,-p options");
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(common_obj, 'F')) {
            if (aopt_check(common_obj, 'f')) {
                const char *optarg = aopt_value(common_obj, 'F');
                if (optarg) {
                    char fd_handle_type[MAX_ARGV_SIZE];

                    strncpy(fd_handle_type, optarg, MAX_ARGV_SIZE);
                    fd_handle_type[MAX_ARGV_SIZE - 1] = '\0';
#ifndef __windows__
#ifndef __FreeBSD__
                    if (!strcmp(fd_handle_type, "epoll") || !strcmp(fd_handle_type, "e")) {
                        s_user_params.fd_handler_type = EPOLL;
                    } else
#endif
                        if (!strcmp(fd_handle_type, "poll") || !strcmp(fd_handle_type, "p")) {
                        s_user_params.fd_handler_type = POLL;
                    } else if (!strcmp(fd_handle_type, "socketxtreme") ||
                        !strcmp(fd_handle_type, "x")) {
                        s_user_params.fd_handler_type = SOCKETXTREME;
                        s_user_params.is_blocked = false;
                    } else
#endif
                        if (!strcmp(fd_handle_type, "select") || !strcmp(fd_handle_type, "s")) {
                        s_user_params.fd_handler_type = SELECT;
                    } else if (!strcmp(fd_handle_type, "recvfrom") ||
                               !strcmp(fd_handle_type, "r")) {
                        s_user_params.fd_handler_type = RECVFROMMUX;
                    } else {
                        log_msg("'-%c' Invalid muliply io hanlde type: %s", 'F', optarg);
                        rc = SOCKPERF_ERR_BAD_ARGUMENT;
                    }
                } else {
                    log_msg("'-%c' Invalid value", 'F');
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                }
            } else {
                log_msg("-F must be used with feed file (option '-f')");
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(common_obj, 'a')) {
            const char *optarg = aopt_value(common_obj, 'a');
            if (optarg) {
                errno = 0;
                s_user_params.packetrate_stats_print_ratio = strtol(optarg, NULL, 0);
                s_user_params.packetrate_stats_print_details = false;
                if (errno != 0) {
                    log_msg("'-%c' Invalid message rate stats print value: %d", 'a',
                            s_user_params.packetrate_stats_print_ratio);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                }
            } else {
                log_msg("'-%c' Invalid value", 'a');
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(common_obj, 'A')) {
            const char *optarg = aopt_value(common_obj, 'A');
            if (optarg) {
                errno = 0;
                s_user_params.packetrate_stats_print_ratio = strtol(optarg, NULL, 0);
                s_user_params.packetrate_stats_print_details = true;
                if (errno != 0) {
                    log_msg("'-%c' Invalid message rate stats print value: %d", 'A',
                            s_user_params.packetrate_stats_print_ratio);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                }
            } else {
                log_msg("'-%c' Invalid value", 'A');
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(common_obj, OPT_RX_MC_IF)) {
            const char *optarg = aopt_value(common_obj, OPT_RX_MC_IF);
            if (optarg) {
                std::string err;
                char dummy;
                if (sscanf(optarg, "%d%c", &s_user_params.rx_mc_if_ix, &dummy) == 1) {
                    s_user_params.rx_mc_if_ix_specified = true;
                } else if (!IPAddress::resolve(optarg, s_user_params.rx_mc_if_addr, err)) {
                    log_msg("'--mc-rx-if' Invalid address '%s': %s", optarg, err.c_str());
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                }
            } else {
                log_msg("'--mc-rx-if' value is expected");
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(common_obj, OPT_TX_MC_IF)) {
            const char *optarg = aopt_value(common_obj, OPT_TX_MC_IF);
            if (optarg) {
                std::string err;
                char dummy;
                if (sscanf(optarg, "%d%c", &s_user_params.tx_mc_if_ix, &dummy) == 1) {
                    s_user_params.tx_mc_if_ix_specified = true;
                } else if (!IPAddress::resolve(optarg, s_user_params.tx_mc_if_addr, err)) {
                    log_msg("'--mc-tx-if' Invalid address '%s': %s", optarg, err.c_str());
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                }
            } else {
                log_msg("'--mc-tx-if' value is expected");
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(common_obj, OPT_MC_SOURCE_IP)) {
            const char *optarg = aopt_value(common_obj, OPT_MC_SOURCE_IP);
            if (optarg) {
                std::string err;
                if (!IPAddress::resolve(optarg, s_user_params.mc_source_ip_addr, err)) {
                    log_msg("Invalid multicast source address '%s': %s", optarg, err.c_str());
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                }
            } else {
                log_msg("'--mc-source-filter' value is expected");
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(common_obj, OPT_SELECT_TIMEOUT)) {
            const char *optarg = aopt_value(common_obj, OPT_SELECT_TIMEOUT);
            if (optarg) {
                errno = 0;
                int value = strtol(optarg, NULL, 0);
                if (errno != 0 || value < -1) {
#ifdef __windows__
                    log_msg("'-%d' Invalid select timeout val: %s", OPT_SELECT_TIMEOUT, optarg);
#elif __FreeBSD__
                    log_msg("'-%d' Invalid select/poll timeout val: %s", OPT_SELECT_TIMEOUT,
                            optarg);
#else
                    log_msg("'-%d' Invalid select/poll/epoll timeout val: %s", OPT_SELECT_TIMEOUT,
                            optarg);
#endif
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                } else {
                    set_select_timeout(value);
                }
            } else {
                log_msg("'-%d' Invalid value", OPT_SELECT_TIMEOUT);
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(common_obj, OPT_MC_LOOPBACK_ENABLE)) {
            s_user_params.mc_loop_disable = false;
        }

        if (!rc && aopt_check(common_obj, OPT_UC_REUSEADDR)) {
            s_user_params.uc_reuseaddr = true;
        }

        if (!rc && aopt_check(common_obj, OPT_BUFFER_SIZE)) {
            const char *optarg = aopt_value(common_obj, OPT_BUFFER_SIZE);
            if (optarg) {
                errno = 0;
                int value = strtol(optarg, NULL, 0);
                if (errno != 0 || value <= 0) {
                    log_msg("'-%d' Invalid socket buffer size: %s", OPT_BUFFER_SIZE, optarg);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                } else {
                    s_user_params.sock_buff_size = value;
                }
            } else {
                log_msg("'-%d' Invalid value", OPT_BUFFER_SIZE);
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(common_obj, OPT_NONBLOCKED)) {
            s_user_params.is_blocked = false;
        }

        if (!rc && aopt_check(common_obj, OPT_TOS)) {
            const char *optarg = aopt_value(common_obj, OPT_TOS);
            if (optarg) {
#if defined(__windows__) || defined(___windows__)
                log_msg("TOS option not supported for Windows");
                rc = SOCKPERF_ERR_UNSUPPORTED;
#else
                int value = strtol(optarg, NULL, 0);
                s_user_params.tos = value;
#endif
            }
        }

        if (!rc && aopt_check(common_obj, OPT_LLS)) {
            const char *optarg = aopt_value(common_obj, OPT_LLS);
            if (optarg) {
#if defined(__windows__) || defined(___windows__)
                log_msg("LLS option not supported for Windows");
                rc = SOCKPERF_ERR_UNSUPPORTED;
#else
                errno = 0;
                int value = strtoul(optarg, NULL, 0);
                if (errno != 0 || value < 0) {
                    log_msg("'-%d' Invalid LLS value: %s", OPT_LLS, optarg);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                } else {
                    s_user_params.lls_usecs = value;
                    s_user_params.lls_is_set = true;
                }
            } else {
                log_msg("'-%d' Invalid value", OPT_LLS);
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
#endif
            }
        }

        if (!rc && aopt_check(common_obj, OPT_NONBLOCKED_SEND)) {
            s_user_params.is_nonblocked_send = true;
        }

        if (!rc && aopt_check(common_obj, OPT_RECV_LOOPING)) {

            const char *optarg = aopt_value(common_obj, OPT_RECV_LOOPING);
            if (optarg) {
                errno = 0;
                int value = strtol(optarg, NULL, 0);
                if (errno != 0) {
                    log_msg("Invalid number of loops - %d: %s", OPT_RECV_LOOPING, optarg);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                } else {
                    if (1 != value) {
                        if (!aopt_check(common_obj, OPT_NONBLOCKED)) {
                            log_msg("recv_looping_num larger then one must be used in a "
                                    "none-blocked mode only. add --nonblocked.");
                            rc = SOCKPERF_ERR_BAD_ARGUMENT;
                        }
                    } else if (0 == value) {
                        log_msg("recv_looping_num cannot be equal to 0.");
                        rc = SOCKPERF_ERR_BAD_ARGUMENT;
                    } else {
                        s_user_params.max_looping_over_recv = value;
                    }
                }
            } else {
                log_msg("'-%d' Invalid value", OPT_RECV_LOOPING);
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }
        if (!rc && aopt_check(common_obj, OPT_DONTWARMUP)) {
            s_user_params.do_warmup = false;
        }

        if (!rc && aopt_check(common_obj, OPT_PREWARMUPWAIT)) {
            const char *optarg = aopt_value(common_obj, OPT_PREWARMUPWAIT);
            if (optarg) {
                errno = 0;
                int value = strtol(optarg, NULL, 0);
                if (errno != 0 || value <= 0) {
                    log_msg("'-%d' Invalid pre warmup wait: %s", OPT_PREWARMUPWAIT, optarg);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                } else {
                    s_user_params.pre_warmup_wait = value;
                }
            } else {
                log_msg("'-%d' Invalid value", OPT_PREWARMUPWAIT);
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(common_obj, OPT_SOCK_ACCL)) {
            s_user_params.withsock_accl = true;
        }
#ifndef __windows__

        if (!rc && aopt_check(common_obj, OPT_ZCOPYREAD)) {
            s_user_params.is_zcopyread = true;
        }

        if (!rc && aopt_check(common_obj, OPT_DAEMONIZE)) {
            *p_daemonize = true;
        }

        if (!rc && aopt_check(common_obj, OPT_NO_RDTSC)) {
            s_user_params.b_no_rdtsc = true;
        }

        if (!rc && aopt_check(common_obj, OPT_RATE_LIMIT)) {
            const char *optarg = aopt_value(common_obj, OPT_RATE_LIMIT);
            if (optarg) {
                errno = 0;
                uint32_t value = strtol(optarg, NULL, 0);
                if (errno != 0 || value <= 0) {
                    log_msg("'-%d' Invalid rate limit : %s", OPT_RATE_LIMIT, optarg);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                } else {
                    s_user_params.rate_limit = value;
                }
            } else {
                log_msg("'-%d' Invalid value", OPT_RATE_LIMIT);
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

#ifndef __FreeBSD__
        if (!rc && aopt_check(common_obj, OPT_LOAD_VMA)) {
            const char *optarg = aopt_value(common_obj, OPT_LOAD_VMA);
            if (!optarg || !*optarg) optarg = (char *)"libvma.so"; // default value
            bool success = vma_xlio_set_func_pointers(optarg);
            if (!success) {
                log_msg("Invalid --load-vma value: %s: failed to set function pointers using the "
                        "given libvma.so path",
                        optarg);
                log_msg("dlerror() says: %s", dlerror());
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }
        if (!rc && aopt_check(common_obj, OPT_LOAD_XLIO)) {
            const char *optarg = aopt_value(common_obj, OPT_LOAD_XLIO);
            if (!optarg || !*optarg) optarg = (char *)"libxlio.so"; // default value
            bool success = vma_xlio_set_func_pointers(optarg);
            if (!success) {
                log_msg("Invalid --load-xlio value: %s: failed to set function pointers using the "
                        "given libxlio.so path",
                        optarg);
                log_msg("dlerror() says: %s", dlerror());
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }
#endif // !defined(__FreeBSD__)
#endif // !defined(__windows__)

        if (!rc && aopt_check(common_obj, OPT_TCP)) {
            if (!aopt_check(common_obj, 'f')) {
                s_user_params.sock_type = SOCK_STREAM;
            } else {
                log_msg("--tcp conflicts with -f option");
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(common_obj, OPT_TCP_NODELAY_OFF)) {
            s_user_params.tcp_nodelay = false;
        }

        if (!rc && aopt_check(common_obj, OPT_IP_MULTICAST_TTL)) {
            const char *optarg = aopt_value(common_obj, OPT_IP_MULTICAST_TTL);
            if (optarg) {
                errno = 0;
                int value = strtol(optarg, NULL, 0);
                if (errno != 0 || value < 0 || value > 255) {
                    log_msg("'-%d' Invalid value: %s", OPT_IP_MULTICAST_TTL, optarg);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                } else {
                    s_user_params.mc_ttl = value;
                }
            } else {
                log_msg("'-%d' Invalid value", OPT_IP_MULTICAST_TTL);
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

#if defined(DEFINED_TLS)
        if (!rc && aopt_check(common_obj, OPT_TLS)) {
            if (!aopt_check(common_obj, OPT_LOAD_VMA) && !aopt_check(common_obj, OPT_LOAD_XLIO)) {
                const char *optarg = aopt_value(common_obj, OPT_TLS);
                s_user_params.tls = true;
                if (optarg && *optarg) {
                    tls_chipher(optarg);
                }
            } else {
                log_msg("--tls conflicts with --load-vma and --load-xlio options");
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }
#endif /* DEFINED_TLS */
    }

    // resolve address: -i, -p and --tcp options must be processed before
    if (!rc) {
        int res = resolve_sockaddr(host_str, port_str, s_user_params.sock_type,
                s_user_params.mode == MODE_SERVER, (sockaddr*)&s_user_params.addr,
                s_user_params.addr_len);
        if (res != 0) {
            log_msg("'-i/-p': invalid host:port value: %s\n", os_get_error(res));
            rc = SOCKPERF_ERR_BAD_ARGUMENT;
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
static int parse_client_opt(const AOPT_OBJECT *client_obj) {
    int rc = SOCKPERF_ERR_NONE;

    s_user_params.mode = MODE_CLIENT;

    if (client_obj) {
        if (!rc && aopt_check(client_obj, OPT_CLIENT_WORK_WITH_SRV_NUM)) {
            const char *optarg = aopt_value(client_obj, OPT_CLIENT_WORK_WITH_SRV_NUM);
            if (optarg) {
                errno = 0;
                int value = strtol(optarg, NULL, 0);
                if (errno != 0 || value < 1) {
                    log_msg("'-%d' Invalid server num val: %s", OPT_CLIENT_WORK_WITH_SRV_NUM,
                            optarg);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                } else {
                    s_user_params.client_work_with_srv_num = value;
                }
            } else {
                log_msg("'-%d' Invalid value", OPT_CLIENT_WORK_WITH_SRV_NUM);
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(client_obj, OPT_SENDER_AFFINITY)) {
            const char *optarg = aopt_value(client_obj, OPT_SENDER_AFFINITY);
            if (optarg) {
                strcpy(s_user_params.sender_affinity, optarg);
            } else {
                log_msg("'-%d' Invalid sender affinity", OPT_SENDER_AFFINITY);
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(client_obj, OPT_RECEIVER_AFFINITY)) {
            const char *optarg = aopt_value(client_obj, OPT_RECEIVER_AFFINITY);
            if (optarg) {
                strcpy(s_user_params.receiver_affinity, optarg);
            } else {
                log_msg("'-%d' Invalid receiver affinity", OPT_RECEIVER_AFFINITY);
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(client_obj, OPT_FULL_LOG)) {
            const char *optarg = aopt_value(client_obj, OPT_FULL_LOG);
            if (optarg) {
                errno = 0;
                s_user_params.fileFullLog = fopen(optarg, "w");
                if (errno || !s_user_params.fileFullLog) {
                    log_msg("Invalid %d val. Can't open file %s for writing: %s", OPT_FULL_LOG,
                            optarg, strerror(errno));
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                }
            } else {
                log_msg("'-%d' Invalid value", OPT_FULL_LOG);
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }
        if (!rc && aopt_check(client_obj, OPT_FULL_RTT)) {
            s_user_params.full_rtt = true;
        }
        if (!rc && aopt_check(client_obj, OPT_GIGA_SIZE)) {
            s_user_params.giga_size = true;
        }
        if (!rc && aopt_check(client_obj, OPT_OUTPUT_PRECISION)) {
            s_user_params.increase_output_precision = true;
        }
        if (!rc && aopt_check(client_obj, OPT_DUMMY_SEND)) {
            s_user_params.dummy_mps = DUMMY_SEND_MPS_DEFAULT;
            const char *optarg = aopt_value(client_obj, OPT_DUMMY_SEND);
            if (optarg) {
                if (0 == strcmp("MAX", optarg) || 0 == strcmp("max", optarg)) {
                    s_user_params.dummy_mps = UINT32_MAX;
                } else {
                    errno = 0;
                    int value = strtol(optarg, NULL, 0);
                    if (errno != 0 || value <= 0 || value > 1 << 30) {
                        log_msg("'-%d' Invalid value of dummy send rate : %s", OPT_DUMMY_SEND,
                                optarg);
                        rc = SOCKPERF_ERR_BAD_ARGUMENT;
                    } else {
                        s_user_params.dummy_mps = (uint32_t)value;
                    }
                }
            }
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
static char *display_opt(int id, char *buf, size_t buf_size) {
    char *cur_ptr = buf;

    if (buf && buf_size) {
        int cur_len = 0;
        int ret = 0;
        int j = 0;

        ret = snprintf((cur_ptr + cur_len), (buf_size - cur_len), "%s", sockperf_modes[id].name);
        cur_len += (ret < 0 ? 0 : ret);

        for (j = 0; (ret >= 0) && (sockperf_modes[id].shorts[j] != NULL); j++) {
            if (j == 0) {
                ret = snprintf((cur_ptr + cur_len), (buf_size - cur_len), " (%s",
                               sockperf_modes[id].shorts[j]);
            } else {
                ret = snprintf((cur_ptr + cur_len), (buf_size - cur_len), " ,%s",
                               sockperf_modes[id].shorts[j]);
            }
            cur_len += (ret < 0 ? 0 : ret);
        }

        if ((j > 0) && (ret >= 0)) {
            ret = snprintf((cur_ptr + cur_len), (buf_size - cur_len), ")");
            cur_len += (ret < 0 ? 0 : ret);
        }
    }

    return (cur_ptr ? cur_ptr : (char *)"");
}

//#define ST_TEST
// use it with command such as: VMA ./sockperf -s -f conf/conf.inp --rxfiltercb

#ifdef ST_TEST
static int st1, st2;
#endif
//------------------------------------------------------------------------------
void cleanup() {
    os_mutex_lock(&_mutex);
#if defined(DEFINED_TLS)
    tls_exit();
#endif /* DEFINED_TLS */
    int ifd;
    if (g_fds_array) {
        for (ifd = 0; ifd <= s_fd_max; ifd++) {
            if (g_fds_array[ifd]) {
                close(ifd);
                if (g_fds_array[ifd]->active_fd_list) {
                    FREE(g_fds_array[ifd]->active_fd_list);
                }
                if (g_fds_array[ifd]->recv.buf) {
                    FREE(g_fds_array[ifd]->recv.buf);
                }
                if (g_fds_array[ifd]->is_multicast) {
                    FREE(g_fds_array[ifd]->memberships_addr);
                }
                if (s_user_params.addr.ss_family == AF_UNIX) {
                    os_unlink_unix_path(s_user_params.client_bind_info.addr_un.sun_path);
#ifndef __windows__ // AF_UNIX with DGRAM isn't supported in __windows__
                    if (s_user_params.mode == MODE_CLIENT && s_user_params.sock_type == SOCK_DGRAM) { // unlink binded client
                        std::string sun_path = build_client_socket_name(&s_user_params.addr.addr_un, getpid(), ifd);
                        log_dbg("unlinking %s", sun_path.c_str());
                        unlink(sun_path.c_str());
                    }
#endif // __windows__
                    if (s_user_params.mode == MODE_SERVER)
                        os_unlink_unix_path(g_fds_array[ifd]->server_addr.addr_un.sun_path);
                }
                delete g_fds_array[ifd];
            }
        }
    }

    if (s_user_params.select_timeout) {
        FREE(s_user_params.select_timeout);
    }
#if defined(USING_VMA_EXTRA_API) || defined(USING_XLIO_EXTRA_API)
    if ((g_vma_api || g_xlio_api) && s_user_params.is_zcopyread) {
        zeroCopyMap::iterator it;
        while ((it = g_zeroCopyData.begin()) != g_zeroCopyData.end()) {
            delete it->second;
            g_zeroCopyData.erase(it);
        }
    }
#endif // USING_VMA_EXTRA_API || USING_XLIO_EXTRA_API

    if (g_fds_array) {
        FREE(g_fds_array);
    }

    if (NULL != g_pPacketTimes) {
        delete g_pPacketTimes;
        g_pPacketTimes = NULL;
    }
    os_mutex_unlock(&_mutex);

    if (sock_lib_started && !os_sock_cleanup()) {
        log_err("Failed to cleanup WSA"); // Only relevant for Windows
    }
}

//------------------------------------------------------------------------------
/* set the timeout of select*/
static void set_select_timeout(int time_out_msec) {
    if (!s_user_params.select_timeout) {
        s_user_params.select_timeout = (struct timeval *)MALLOC(sizeof(struct timeval));
        if (!s_user_params.select_timeout) {
            log_err("Failed to allocate memory for pointer select timeout structure");
            exit_with_log(SOCKPERF_ERR_NO_MEMORY);
        }
    }
    if (time_out_msec >= 0) {
        // Update timeout
        s_user_params.select_timeout->tv_sec = time_out_msec / 1000;
        s_user_params.select_timeout->tv_usec =
            1000 * (time_out_msec - s_user_params.select_timeout->tv_sec * 1000);
    } else {
        // Clear timeout
        FREE(s_user_params.select_timeout);
    }
}

//------------------------------------------------------------------------------
void set_defaults() {
#if !defined(__windows__) && !defined(__FreeBSD__)
    bool success = vma_xlio_try_set_func_pointers();
    if (!success) {
        log_dbg("Failed to set function pointers for system functions.");
        log_dbg("Check vma-xlio-redirect.cpp for functions which your OS implementation is missing. "
                "Re-compile sockperf without them.");
    }
#elif defined __windows__
    int rc = 0;
    if (os_sock_startup() == false) { // Only relevant for Windows
        log_err("Failed to initialize WSA");
        rc = SOCKPERF_ERR_FATAL;
    }
    else {
        sock_lib_started = 1;
    }
#endif
    g_fds_array = (fds_data **)MALLOC(MAX_FDS_NUM * sizeof(fds_data *));
    if (!g_fds_array) {
        log_err("Failed to allocate memory for global pointer fds_array");
        exit_with_log(SOCKPERF_ERR_NO_MEMORY);
    }
    int igmp_max_memberships = read_int_from_sys_file("/proc/sys/net/ipv4/igmp_max_memberships");
    if (igmp_max_memberships != -1) IGMP_MAX_MEMBERSHIPS = igmp_max_memberships;

    memset(g_fds_array, 0, sizeof(fds_data *) * MAX_FDS_NUM);
    s_user_params.rx_mc_if_ix = 0;
    s_user_params.tx_mc_if_ix = 0;
    s_user_params.rx_mc_if_ix_specified = false;
    s_user_params.tx_mc_if_ix_specified = false;
    s_user_params.rx_mc_if_addr = IPAddress::zero();
    s_user_params.tx_mc_if_addr = IPAddress::zero();
    s_user_params.mc_source_ip_addr = IPAddress::zero();
    s_user_params.client_bind_info.ss_family = AF_UNSPEC;

    set_select_timeout(DEFAULT_SELECT_TIMEOUT_MSEC);
    memset(s_user_params.threads_affinity, 0, sizeof(s_user_params.threads_affinity));

    g_debug_level = LOG_LVL_INFO;

    memset(s_user_params.feedfile_name, 0, sizeof(s_user_params.feedfile_name));

}

//------------------------------------------------------------------------------
#ifdef USING_VMA_EXTRA_API // Only for VMA callback-extra-api
class CallbackMessageHandler {
    int m_fd;
    fds_data *m_fds_ifd;
    struct vma_info_t *m_vma_info;

public:
    inline CallbackMessageHandler(int fd, fds_data *l_fds_ifd, struct vma_info_t *vma_info) :
        m_fd(fd),
        m_fds_ifd(l_fds_ifd),
        m_vma_info(vma_info)
    {}

    inline bool handle_message();
};

vma_recv_callback_retval_t myapp_vma_recv_pkt_filter_callback(int fd, size_t iov_sz,
                                                              struct iovec iov[],
                                                              struct vma_info_t *vma_info,
                                                              void *context) {
#ifdef ST_TEST
    if (st1) {
        log_msg("DEBUG: ST_TEST - myapp_vma_recv_pkt_filter_callback fd=%d", fd);
        close(st1);
        close(st2);
        st1 = st2 = 0;
    }
#endif

    // Check info structure version
    if (vma_info->struct_sz < sizeof(struct vma_info_t)) {
        log_msg("VMA's info struct is not something we can handle so un-register the application's "
                "callback function");
        g_vma_api->register_recv_callback(fd, NULL, NULL);
        return VMA_PACKET_RECV;
    }

    // If there is data in local buffer, then push new packet in TCP queue.Otherwise handle received
    // packet inside callback.
    if (g_zeroCopyData[fd] && g_zeroCopyData[fd]->m_pkts &&
        reinterpret_cast<vma_packets_t *>(g_zeroCopyData[fd]->m_pkts)->n_packet_num > 0) {
        return VMA_PACKET_RECV;
    }

    struct fds_data *l_fds_ifd;

    l_fds_ifd = g_fds_array[fd];
    if (unlikely(!l_fds_ifd)) {
        return VMA_PACKET_RECV;
    }
    Message *msgReply = l_fds_ifd->p_msg;
    SocketRecvData &recv_data = l_fds_ifd->recv;

    CallbackMessageHandler handler(fd, l_fds_ifd, vma_info);
    MessageParser<BufferAccumulation> parser(msgReply);
    for (size_t i = 0; i < iov_sz; ++i) {
        bool ok = parser.process_buffer(handler, recv_data, (uint8_t *)iov[i].iov_base,
                (int)iov[i].iov_len);
        if (unlikely(!ok)) {
            return VMA_PACKET_RECV;
        }
    }

    return VMA_PACKET_DROP;
}

inline bool CallbackMessageHandler::handle_message()
{
    struct sockaddr_store_t sendto_addr;
    socklen_t sendto_len;

    Message *msgReply = m_fds_ifd->p_msg;

    if (unlikely(g_b_exit)) {
        return false;
    }
    if (unlikely(!msgReply->isValidHeader())) {
        log_msg("Message received was larger than expected, handle from recv_from");
        return false;
    }
    if (unlikely(!msgReply->isClient())) {
        return true;
    }
    if (unlikely(msgReply->isWarmupMessage())) {
        return true;
    }

    g_receiveCount++;

    if (msgReply->getHeader()->isPongRequest()) {
        /* if server in a no reply mode - shift to start of cycle buffer*/
        if (g_pApp->m_const_params.b_server_dont_reply) {
            return true;
        }
        /* prepare message header */
        if (g_pApp->m_const_params.mode != MODE_BRIDGE) {
            msgReply->setServer();
        }
        /* get source addr to reply. memcpy is not used to improve performance */
        copy_relevant_sockaddr_params(sendto_addr, m_fds_ifd->server_addr);
        sendto_len = m_fds_ifd->server_addr_len;

        if (m_fds_ifd->memberships_size || !m_fds_ifd->is_multicast ||
            g_pApp->m_const_params.b_server_reply_via_uc) { // In unicast case reply to sender
            /* get source addr to reply. memcpy is not used to improve performance */
            //TODO: update after IPv6 will be supported in libvma
            sendto_len = sizeof(sockaddr_in);
            std::memcpy(&sendto_addr, m_vma_info->src, sendto_len);
        } else if (m_fds_ifd->is_multicast) {
            /* always send to the same port recved from */
            sockaddr_set_portn(sendto_addr, sockaddr_get_portn((sockaddr_store_t &)*m_vma_info->src));
        }
        int length = msgReply->getLength();
        msgReply->setHeaderToNetwork();
        msg_sendto(m_fd, msgReply->getBuf(), length, reinterpret_cast<sockaddr *>(&sendto_addr), sendto_len);
        /*if (ret == RET_SOCKET_SHUTDOWN) {
            if (m_fds_ifd->sock_type == SOCK_STREAM) {
                close_ifd( m_fds_ifd->next_fd,ifd,m_fds_ifd);
            }
            return VMA_PACKET_DROP;
        }*/
        msgReply->setHeaderToHost();
    }
    /*
     * TODO
     * To support other server functionality when using zero callback,
     * pass the server as user_context or as we pass the replyMsg, and call the server functions
     */
    // m_switchCalcGaps.execute(vma_info->src, msgReply->getSequenceCounter(), false);
    // m_switchActivityInfo.execute(g_receiveCount);

    return true;
}
#endif // USING_VMA_EXTRA_API

int sock_set_accl(int fd) {
    int rc = SOCKPERF_ERR_NONE;
    if (setsockopt(fd, SOL_SOCKET, 100, NULL, 0) < 0) {
        log_err("setsockopt(100), set sock-accl failed.  It could be that this option is not "
                "supported in your system");
        rc = SOCKPERF_ERR_SOCKET;
    }
    log_msg("succeed to set sock-accl");
    return rc;
}

int sock_set_reuseaddr(int fd) {
    int rc = SOCKPERF_ERR_NONE;
    u_int reuseaddr_true = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_true, sizeof(reuseaddr_true)) < 0) {
        log_err("setsockopt(SO_REUSEADDR) failed");
        rc = SOCKPERF_ERR_SOCKET;
    }
    return rc;
}

#ifndef SO_LL
#define SO_LL 46
#endif
int sock_set_lls(int fd) {
    int rc = SOCKPERF_ERR_NONE;
    if (setsockopt(fd, SOL_SOCKET, SO_LL, &(s_user_params.lls_usecs),
                   sizeof(s_user_params.lls_usecs)) < 0) {
        log_err("setsockopt(SO_LL) failed");
        rc = SOCKPERF_ERR_SOCKET;
    }
    return rc;
}

int sock_set_snd_rcv_bufs(int fd) {
    /*
     * Sets or gets the maximum socket receive buffer in bytes. The kernel
     * doubles this value (to allow space for bookkeeping overhead) when it
     * is set using setsockopt(), and this doubled value is returned by
     * getsockopt().
     */

    int rc = SOCKPERF_ERR_NONE;
    int size = sizeof(int);
    int rcv_buff_size = 0;
    int snd_buff_size = 0;

    if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &(s_user_params.sock_buff_size),
                   sizeof(s_user_params.sock_buff_size)) < 0) {
        log_err("setsockopt(SO_RCVBUF) failed");
        rc = SOCKPERF_ERR_SOCKET;
    }
    if (!rc && (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcv_buff_size, (socklen_t *)&size) < 0)) {
        log_err("getsockopt(SO_RCVBUF) failed");
        rc = SOCKPERF_ERR_SOCKET;
    }
    /*
     * Sets or gets the maximum socket send buffer in bytes. The kernel
     * doubles this value (to allow space for bookkeeping overhead) when it
     * is set using setsockopt(), and this doubled value is returned by
     * getsockopt().
     */
    if (!rc && (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &(s_user_params.sock_buff_size),
                           sizeof(s_user_params.sock_buff_size)) < 0)) {
        log_err("setsockopt(SO_SNDBUF) failed");
        rc = SOCKPERF_ERR_SOCKET;
    }
    if (!rc && (getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &snd_buff_size, (socklen_t *)&size) < 0)) {
        log_err("getsockopt(SO_SNDBUF) failed");
        rc = SOCKPERF_ERR_SOCKET;
    }
    if (!rc) {
        log_msg("Socket buffers sizes of fd %d: RX: %d Byte, TX: %d Byte", fd, rcv_buff_size,
                snd_buff_size);
        if (rcv_buff_size < s_user_params.sock_buff_size * 2 ||
            snd_buff_size < s_user_params.sock_buff_size * 2) {
            log_msg("WARNING: Failed setting receive or send socket buffer size to %d bytes (check "
                    "'sysctl net.core.rmem_max' value)",
                    s_user_params.sock_buff_size);
        }
    }

    return rc;
}

int sock_set_tcp_nodelay(int fd) {
    int rc = SOCKPERF_ERR_NONE;
    if (s_user_params.tcp_nodelay) {
        /* set Delivering Messages Immediately */
        int tcp_nodelay = 1;
        if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&tcp_nodelay, sizeof(tcp_nodelay)) <
            0) {
            log_err("setsockopt(TCP_NODELAY)");
            rc = SOCKPERF_ERR_SOCKET;
        }
    }
    return rc;
}

int sock_set_tos(int fd) {
    int rc = SOCKPERF_ERR_NONE;
    if (s_user_params.tos) {
        socklen_t len = sizeof(s_user_params.tos);
        if (setsockopt(fd, IPPROTO_IP, IP_TOS, (char *)&s_user_params.tos, len) < 0) {
            log_err("setsockopt(TOS), set  failed.  It could be that this option is not supported "
                    "in your system");
            rc = SOCKPERF_ERR_SOCKET;
        }
    }
    return rc;
}

static int sock_join_multicast_v4(int fd, struct fds_data *p_data, const sockaddr_in *p_addr)
{
    if (s_user_params.rx_mc_if_addr.family() != AF_UNSPEC &&
            s_user_params.rx_mc_if_addr.family() != AF_INET) {
        log_err("multicast source IP address must be IPv4!");
        return SOCKPERF_ERR_SOCKET;
    }

    if (p_data->mc_source_ip_addr.is_specified()) {
        if (p_data->mc_source_ip_addr.family() != AF_INET) {
            log_err("multicast source IP address must be IPv4!");
            return SOCKPERF_ERR_SOCKET;
        }

        struct ip_mreq_source mreq_src;
        memset(&mreq_src, 0, sizeof(struct ip_mreq_source));
        mreq_src.imr_multiaddr = p_addr->sin_addr;
        mreq_src.imr_interface.s_addr = s_user_params.rx_mc_if_addr.addr4().s_addr;
        mreq_src.imr_sourceaddr.s_addr = p_data->mc_source_ip_addr.addr4().s_addr;
        if (setsockopt(fd, IPPROTO_IP, IP_ADD_SOURCE_MEMBERSHIP, &mreq_src, sizeof(mreq_src)) <
                0) {
            if (errno == ENOBUFS) {
                log_err("setsockopt(IP_ADD_SOURCE_MEMBERSHIP) - Maximum multicast source "
                        "addresses that can be filtered is limited by "
                        "/proc/sys/net/ipv4/igmp_max_msf");
            } else {
                log_err("setsockopt(IP_ADD_SOURCE_MEMBERSHIP)");
            }
            return SOCKPERF_ERR_SOCKET;
        }
    } else {
        struct ip_mreq mreq;
        memset(&mreq, 0, sizeof(struct ip_mreq));
        mreq.imr_multiaddr = p_addr->sin_addr;
        mreq.imr_interface.s_addr = s_user_params.rx_mc_if_addr.addr4().s_addr;
        if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
            if (errno == ENOBUFS) {
                log_err("setsockopt(IP_ADD_MEMBERSHIP) - Maximum multicast addresses that can "
                        "join same group is limited by "
                        "/proc/sys/net/ipv4/igmp_max_memberships");
            } else {
                log_err("setsockopt(IP_ADD_MEMBERSHIP)");
            }
            return SOCKPERF_ERR_SOCKET;
        }
    }
    return SOCKPERF_ERR_NONE;
}

static int sock_join_multicast_v6(int fd, struct fds_data *p_data, const sockaddr_in6 *p_addr)
{
    if (s_user_params.rx_mc_if_ix_specified && s_user_params.rx_mc_if_ix < 0) {
        log_err("RX interface index must be >=0 !");
        return SOCKPERF_ERR_SOCKET;
    }

    if (p_data->mc_source_ip_addr.is_specified()) {
        if (p_data->mc_source_ip_addr.family() != AF_INET6) {
            log_err("multicast source IP address must be IPv6!");
            return SOCKPERF_ERR_SOCKET;
        }

        struct group_source_req gsreq;
        memset(&gsreq, 0, sizeof(struct group_source_req));
        struct sockaddr_in6 *sa_grp = (struct sockaddr_in6 *)(&gsreq.gsr_group);
        struct sockaddr_in6 *sa_src = (struct sockaddr_in6 *)(&gsreq.gsr_source);
        sa_grp->sin6_family = sa_src->sin6_family = AF_INET6;
        sa_grp->sin6_addr = p_addr->sin6_addr;
        sa_src->sin6_addr = p_data->mc_source_ip_addr.addr6();
        gsreq.gsr_interface = s_user_params.rx_mc_if_ix;
        if (setsockopt(fd, IPPROTO_IPV6, MCAST_JOIN_SOURCE_GROUP, &gsreq, sizeof(gsreq)) < 0) {
            if (errno == ENOBUFS) {
                log_err("setsockopt(MCAST_JOIN_SOURCE_GROUP) - Maximum multicast source "
                        "addresses that can be filtered is limited by "
                        "/proc/sys/net/ipv6/mld_max_msf");
            } else {
                log_err("setsockopt(MCAST_JOIN_SOURCE_GROUP)");
            }
            return SOCKPERF_ERR_SOCKET;
        }
    } else {
        struct group_req greq;
        memset(&greq, 0, sizeof(struct group_req));
        struct sockaddr_in6 *sa_grp = (struct sockaddr_in6 *)(&greq.gr_group);
        sa_grp->sin6_family = AF_INET6;
        sa_grp->sin6_addr = p_addr->sin6_addr;
        greq.gr_interface = s_user_params.rx_mc_if_ix;
        if (setsockopt(fd, IPPROTO_IPV6, MCAST_JOIN_GROUP, &greq, sizeof(greq)) < 0) {
            log_err("setsockopt(MCAST_JOIN_GROUP)");
            return SOCKPERF_ERR_SOCKET;
        }
    }
    return SOCKPERF_ERR_NONE;
}

int sock_set_multicast_v4(int fd, struct fds_data *p_data) {
    int rc = SOCKPERF_ERR_NONE;

    /* use setsockopt() to request that the kernel join a multicast group */
    /* and specify a specific interface address on which to receive the packets of this socket */
    /* and may specify message source IP address on which to receive from */
    /* NOTE: we don't do this if case of client (sender) in stream mode */
    struct sockaddr_in *p_addr = reinterpret_cast<sockaddr_in *>(&(p_data->server_addr));
    if (!s_user_params.b_stream || s_user_params.mode != MODE_CLIENT) {
        rc = sock_join_multicast_v4(fd, p_data, p_addr);
    }

    /* specify a specific interface address on which to transmitted the multicast packets of this
     * socket */
    if (!rc && s_user_params.tx_mc_if_addr.is_specified()) {
        if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF,
                    &s_user_params.tx_mc_if_addr.addr4(), sizeof(in_addr)) < 0) {
            log_err("setsockopt(IP_MULTICAST_IF)");
            rc = SOCKPERF_ERR_SOCKET;
        }
    }

    if (!rc && (s_user_params.mc_loop_disable)) {
        /* disable multicast loop of all transmitted packets */
        u_char loop_disabled = 0;
        if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop_disabled, sizeof(loop_disabled)) <
            0) {
            log_err("setsockopt(IP_MULTICAST_LOOP)");
            rc = SOCKPERF_ERR_SOCKET;
        }
    }

    if (!rc) {
        /* the IP_MULTICAST_TTL socket option allows the application to primarily
         * limit the lifetime of the packet in the Internet and prevent it from
         * circulating
         */
        int value = s_user_params.mc_ttl;
        if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&value, sizeof(value)) < 0) {
            log_err("setsockopt(IP_MULTICAST_TTL)");
            rc = SOCKPERF_ERR_SOCKET;
        }
    }

    return rc;
}

int sock_set_multicast_v6(int fd, struct fds_data *p_data) {
    int rc = SOCKPERF_ERR_NONE;

    /* specify interface index on which to transmitted the multicast packets of this socket */
    if (!rc && s_user_params.tx_mc_if_addr.is_specified()) {
        log_err("For IPv6 - input must be an integer");
        rc = SOCKPERF_ERR_UNSUPPORTED;
    }


    /* use setsockopt() to request that the kernel join a multicast group */
    /* and specify a specific interface address on which to receive the packets of this socket */
    /* and may specify message source IP address on which to receive from */
    /* NOTE: we don't do this if case of client (sender) in stream mode */
    struct sockaddr_in6 *p_addr = reinterpret_cast<sockaddr_in6 *>(&(p_data->server_addr));
    if (!s_user_params.b_stream || s_user_params.mode != MODE_CLIENT) {
        rc = sock_join_multicast_v6(fd, p_data, p_addr);
    }



    if (!rc && s_user_params.tx_mc_if_ix_specified) {
        int if_ix = s_user_params.tx_mc_if_ix;
        if (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &if_ix, sizeof(int)) < 0) {
            log_err("setsockopt(IPV6_MULTICAST_IF)");
            rc = SOCKPERF_ERR_SOCKET;
        }
    }

    if (!rc && (s_user_params.mc_loop_disable)) {
        /* Control whether the socket sees multicast packets that it has send itself */
        int loop_disabled = 0;
        if (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &loop_disabled, sizeof(loop_disabled)) <
            0) {
            log_err("setsockopt(IPV6_MULTICAST_LOOP)");
            rc = SOCKPERF_ERR_SOCKET;
        }
    }

    if (!rc) {
        /* the IPV6_MULTICAST_HOPS socket option allows the application to primarily
         * limit the lifetime of the packet in the Internet and prevent it from
         * circulating
         */
        int value = s_user_params.mc_ttl;
        if (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, (char *)&value, sizeof(value)) < 0) {
            log_err("setsockopt(IPV6_MULTICAST_HOPS)");
            rc = SOCKPERF_ERR_SOCKET;
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
/* returns the new socket fd
 or exit with error code */
#ifdef ST_TEST
int prepare_socket(int fd, struct fds_data *p_data, bool stTest = false)
#else
int prepare_socket(int fd, struct fds_data *p_data)
#endif
{
    int rc = SOCKPERF_ERR_NONE;

    if (!p_data) {
        return (int)INVALID_SOCKET; // TODO: use SOCKET all over the way and avoid this cast
    }

    if (!rc && !s_user_params.is_blocked) {
        /*Uncomment to test FIONBIO command of ioctl
         * int opt = 1;
         * ioctl(fd, FIONBIO, &opt);
         */

        /* change socket to non-blocking */
        if (os_set_nonblocking_socket(fd)) {
            log_err("failed setting socket as nonblocking\n");
            rc = SOCKPERF_ERR_SOCKET;
        }
    }

    if (!rc && (s_user_params.withsock_accl == true)) {
        rc = sock_set_accl(fd);
    }

    if (!rc && ((p_data->is_multicast) || (s_user_params.uc_reuseaddr))) {
        /* allow multiple sockets to use the same PORT (SO_REUSEADDR) number
         * only if it is a well know L4 port only for MC or if uc_reuseaddr parameter was set.
         */
        const sockaddr_store_t *addr;
        switch (s_user_params.mode) {
        case MODE_SERVER:
            addr = &p_data->server_addr;
            break;
        case MODE_CLIENT:
            addr = &s_user_params.client_bind_info;
            break;
        default:
            addr = NULL;
        }
        if (addr && sockaddr_get_portn(*addr) != 0) {
            rc = sock_set_reuseaddr(fd);
        }
    }

    if (!rc && (s_user_params.lls_is_set == true)) {
        rc = sock_set_lls(fd);
    }

    if (!rc && (s_user_params.sock_buff_size > 0)) {
        rc = sock_set_snd_rcv_bufs(fd);
    }

    if (!rc && (p_data->is_multicast)) {
        struct sockaddr_store_t *p_addr = &(p_data->server_addr);
        switch (p_addr->ss_family) {
        case AF_INET:
            rc = sock_set_multicast_v4(fd, p_data);
            break;
        case AF_INET6:
            rc = sock_set_multicast_v6(fd, p_data);
            break;
        default:
            log_err("multicast failed, can't fine IP version");
            rc = SOCKPERF_ERR_UNSUPPORTED;
            break;
        }
    }

    if (!rc && (p_data->sock_type == SOCK_STREAM)) {
        rc = sock_set_tcp_nodelay(fd);
    }

    if (!rc && (s_user_params.tos)) {
        rc = sock_set_tos(fd);
    }

#if defined(USING_VMA_EXTRA_API) || defined(USING_XLIO_EXTRA_API)
#ifdef ST_TEST
    if (!stTest)
#endif
#ifdef USING_VMA_EXTRA_API
        if (!rc && (s_user_params.is_rxfiltercb && g_vma_api)) { // XLIO does not support callback-extra-api
            // Try to register application with VMA's special receive notification callback logic
            if (g_vma_api->register_recv_callback(fd, myapp_vma_recv_pkt_filter_callback, NULL) <
                0) {
                log_err("vma_api->register_recv_callback failed. Try running without option "
                        "'rxfiltercb'/'vmarxfiltercb'");
            } else {
                log_dbg("vma_api->register_recv_callback successful registered");
            }
        } else
#endif // USING_VMA_EXTRA_API
        if (!rc && (s_user_params.is_zcopyread && (g_vma_api || g_xlio_api))) {
            g_zeroCopyData[fd] = new ZeroCopyData();
            g_zeroCopyData[fd]->allocate();
        }
#endif // USING_VMA_EXTRA_API || USING_XLIO_EXTRA_API

    return (!rc ? fd
                : (int)INVALID_SOCKET); // TODO: use SOCKET all over the way and avoid this cast
}

#ifdef USING_VMA_EXTRA_API
//------------------------------------------------------------------------------
static bool is_unspec_addr(const sockaddr_store_t &addr)
{
    switch (addr.ss_family) {
    case AF_INET:
        return reinterpret_cast<const sockaddr_in &>(addr).sin_addr.s_addr == INADDR_ANY;
    case AF_INET6:
        return IN6_IS_ADDR_UNSPECIFIED(&reinterpret_cast<const sockaddr_in6 &>(addr).sin6_addr);
    }
    return true;
}
#endif // USING_VMA_EXTRA_API

//------------------------------------------------------------------------------
/* get IP:port pairs from the file and initialize the list */
/* Example file content:
# old format
192.168.1.2:1123
# current format
T:192.168.1.3:1124
T:localhost:1120
U:192.168.1.4:1125
U:192.168.1.5:1126:1.2.3.4
U:main.example.org:1125:src.example.org
# IPv6 addresses
U:[::1]:1127
U:[2001:0db8:0000:0000:0000:ff00:0042:8329]:1128
U:[2001:0db8:0000:0000:0000:ff00:0042:8329]:1129:[ff02::1]
T:[::ffff:192.0.2.128]:1130
U:[fe80::9a03:9bff:fea3:b01c%enp3s0f0]:1131
# Unix domain socket Format
U:/tmp/test
T:/tmp/test2
# Windows AF_UNIX format
u:c:\tmp\test
t:C:\tmp\test2
U:d:\tmp\test3
*/
static int set_sockets_from_feedfile(const char *feedfile_name) {
    int rc = SOCKPERF_ERR_NONE;
    FILE *file_fd = NULL;
    char line[MAX_MCFILE_LINE_LENGTH];
    char *res = NULL;
    int sock_type = SOCK_DGRAM;
    int curr_fd = 0, last_fd = 0;
#ifdef NEED_REGEX_WORKAROUND
    regex_t regexpr_ip;
    regex_t regexpr_unix;
#else // NEED_REGEX_WORKAROUND
    const std::regex regexpr_ip(IP_PORT_FORMAT_REG_EXP);
    const std::regex regexpr_unix(UNIX_DOMAIN_SOCKET_FORMAT_REG_EXP);
#endif // NEED_REGEX_WORKAROUND

    struct stat st_buf;
    const int status = stat(feedfile_name, &st_buf);
    // Get the status of the file system object.
    if (status != 0) {
        log_msg("Can't open file: %s\n", feedfile_name);
        return SOCKPERF_ERR_NOT_EXIST;
    }
#ifndef __windows__
    if (!S_ISREG(st_buf.st_mode)) {
        log_msg("Can't open file: %s -not a regular file.\n", feedfile_name);
        return SOCKPERF_ERR_NOT_EXIST;
    }
#endif
    if ((file_fd = fopen(feedfile_name, "r")) == NULL) {
        log_msg("Can't open file: %s\n", feedfile_name);
        return SOCKPERF_ERR_NOT_EXIST;
    }
/* a map to keep records on the address we received */
    std::unordered_map<port_descriptor, int> fd_socket_map; //<port,fd>

#ifdef NEED_REGEX_WORKAROUND
    int regexp_ip_error = regcomp(&regexpr_ip, IP_PORT_FORMAT_REG_EXP, REG_EXTENDED);
    if (regexp_ip_error != 0) {
        log_msg("Failed to compile regexp for ip format");
        rc = SOCKPERF_ERR_FATAL;
    }

    int regexp_unix_error = regcomp(&regexpr_unix, UNIX_DOMAIN_SOCKET_FORMAT_REG_EXP, REG_EXTENDED);
    if (regexp_unix_error != 0) {
        log_msg("Failed to compile regexp for unix format");
        rc = SOCKPERF_ERR_FATAL;
    }
#endif // NEED_REGEX_WORKAROUND

    while (!rc && (res = fgets(line, MAX_MCFILE_LINE_LENGTH, file_fd))) {
        /* skip empty lines and comments */
        if (line[0] == ' ' || line[0] == '\r' || line[0] == '\n' || line[0] == '#') {
            continue;
        }
        std::string type, addr, port, mc_src_ip;

        const size_t NUM_RE_GROUPS = 9;
#ifdef NEED_REGEX_WORKAROUND
        regmatch_t m[NUM_RE_GROUPS];
        int regexpres = regexec(&regexpr_ip, line, NUM_RE_GROUPS, m, 0);
        if (regexpres == 0) {
            type = std::string(line + m[1].rm_so, line + m[1].rm_eo);
            // server address
            if (m[3].rm_so < 0) {
                // IPv4 or hostname
                addr = std::string(line + m[2].rm_so, line + m[2].rm_eo);
            } else {
                // IPv6 in brackets
                addr = std::string(line + m[3].rm_so, line + m[3].rm_eo);
            }
            port = std::string(line + m[5].rm_so, line + m[5].rm_eo);
            // multicast source address
            if (m[8].rm_so < 0) {
                // IPv4 or hostname
                mc_src_ip = std::string(line + m[7].rm_so, line + m[7].rm_eo);
            } else {
                // IPv6 in brackets
                mc_src_ip = std::string(line + m[8].rm_so, line + m[8].rm_eo);
            }
        } else if (regexec(&regexpr_unix, line, NUM_RE_GROUPS, m, 0) == 0) {
            type = line[0];
            addr = std::string(line + m[1].rm_so, line + m[1].rm_eo);
        }
#else // NEED_REGEX_WORKAROUND
        std::smatch m;
        std::string line_str(line);
        bool matched = std::regex_match(line_str, m, regexpr_ip);
        if (matched && m.size() == NUM_RE_GROUPS) {
            type = m[1];
            // server address
            if (m[3].length() == 0) {
                // IPv4 or hostname
                addr = m[2];
            } else {
                // IPv6 in brackets
                addr = m[3];
            }
            port = m[5];
            // multicast source address
            if (m[8].length() == 0) {
                // IPv4 or hostname
                mc_src_ip = m[7];
            } else {
                // IPv6 in brackets
                mc_src_ip = m[8];
            }
        } else if (std::regex_match(line_str, m, regexpr_unix)) {
            type = line_str[0];
            addr = std::string(m[1]);
        }
#endif // NEED_REGEX_WORKAROUND
        else {
            log_msg("Invalid input in line %s: "
                    "each line must have the following format: ip:port or type:ip:port or "
                    "type:ip:port:mc_src_ip or type:/PATH (linux), type:PATH (windows). "
                    "IPv6 addresses must be enclosed in square brackets."
                    "UNIX domain socket addresses must be an absolute paths (starting with '/') for linux environment only.",
                    line);
            rc = SOCKPERF_ERR_INCORRECT;
            break;
        }

        if (!type.empty()) {
            /* this code support backward compatibility with old format of file */
            sock_type = (type[0] == 'T' || type[0] == 't' ? SOCK_STREAM : SOCK_DGRAM);
        } else {
            sock_type = SOCK_DGRAM;
        }
#ifdef USING_VMA_EXTRA_API
        if (sock_type == SOCK_DGRAM && s_user_params.mode == MODE_CLIENT) {
            if (s_user_params.fd_handler_type == SOCKETXTREME &&
                is_unspec_addr(s_user_params.client_bind_info)) {
                log_msg("socketxtreme requires forcing the client side to bind to a specific ip "
                        "address (use --client_ip/--client_addr) option");
                rc = SOCKPERF_ERR_INCORRECT;
                break;
            }
        }
#endif

        std::unique_ptr<fds_data> tmp{ new fds_data };

        int res = resolve_sockaddr(addr.c_str(), port.c_str(), sock_type,
                false, reinterpret_cast<sockaddr *>(&tmp->server_addr), tmp->server_addr_len);
        if (res != 0) {
            log_msg("Invalid address in line %s: %s\n", line, os_get_error(res));
            rc = SOCKPERF_ERR_INCORRECT;
            break;
        }

        tmp->mc_source_ip_addr = s_user_params.mc_source_ip_addr;
        if (!mc_src_ip.empty()) {
            std::string err;
            if (!IPAddress::resolve(mc_src_ip.c_str(), tmp->mc_source_ip_addr, err)) {
                log_msg("Invalid multicast source address '%s' in line %s: %s",
                    mc_src_ip.c_str(), line, err.c_str());
                rc = SOCKPERF_ERR_INCORRECT;
                break;
            }
        }
        tmp->is_multicast = is_multicast_addr(tmp->server_addr);
        tmp->sock_type = sock_type;

        /* Check if the same value exists */
        bool is_exist = false;
        in_port_t port_tmp = ntohs(sockaddr_get_portn(tmp->server_addr));
        port_descriptor port_desc_tmp = { tmp->sock_type, tmp->server_addr.ss_family, port_tmp };
        for (int i = s_fd_min; i <= s_fd_max; i++) {
            /* duplicated values are accepted in case client connection using TCP */
            /* or in case source address is set for multicast socket */
            if (((s_user_params.mode == MODE_CLIENT) && (tmp->sock_type == SOCK_STREAM)) ||
                ((tmp->is_multicast) && tmp->mc_source_ip_addr.is_specified())) {
                continue;
            }

            if (g_fds_array[i] && !memcmp(&(g_fds_array[i]->server_addr), &(tmp->server_addr),
                                          sizeof(tmp->server_addr)) &&
                fd_socket_map[port_desc_tmp]) {
                is_exist = true;
                break;
            }
        }

        if (is_exist) {
            if (tmp->recv.buf) {
                FREE(tmp->recv.buf);
            }
            continue;
        }

        if (tmp->server_addr.ss_family == AF_UNIX) {
            s_user_params.tcp_nodelay = false;
            s_user_params.addr = tmp->server_addr;
            if (tmp->sock_type == SOCK_DGRAM) // for later binding client
                s_user_params.client_bind_info.ss_family = AF_UNIX;
        }
        tmp->active_fd_count = 0;
        tmp->active_fd_list = (int *)MALLOC(MAX_ACTIVE_FD_NUM * sizeof(int));
        bool new_socket_flag = true;
        if (!tmp->active_fd_list) {
            log_err("Failed to allocate memory with malloc()");
            rc = SOCKPERF_ERR_NO_MEMORY;
        } else {
            /* if this port already been received before, join socket - multicast only */
            if ((0 != fd_socket_map[port_desc_tmp]) && (tmp->is_multicast)) {
                /* join socket */
                curr_fd = fd_socket_map[port_desc_tmp];
                new_socket_flag = false;
                if (g_fds_array[curr_fd]->memberships_addr == NULL) {
                    g_fds_array[curr_fd]->memberships_addr = reinterpret_cast<sockaddr_store_t *>(MALLOC(
                        IGMP_MAX_MEMBERSHIPS * sizeof(struct sockaddr_store_t)));
                }
                g_fds_array[curr_fd]->memberships_addr[g_fds_array[curr_fd]->memberships_size] =
                    tmp->server_addr;
                g_fds_array[curr_fd]->memberships_size++;
            } else {
                /* create a socket */
                if ((curr_fd = (int)socket(tmp->server_addr.ss_family, tmp->sock_type, 0)) <
                    0) { // TODO: use SOCKET all over the way and avoid this cast
                    log_err("socket(AF_INET4/6, SOCK_x)");
                    rc = SOCKPERF_ERR_SOCKET;
                }
                fd_socket_map[port_desc_tmp] = curr_fd;
                if (tmp->is_multicast) {
                    tmp->memberships_addr = reinterpret_cast<sockaddr_store_t *>(MALLOC(
                        IGMP_MAX_MEMBERSHIPS * sizeof(struct sockaddr_store_t)));
                } else {
                    tmp->memberships_addr = NULL;
                }
                tmp->memberships_size = 0;

                s_fd_num++;
            }
            if (curr_fd >= 0) {
                if ((curr_fd >= MAX_FDS_NUM) ||
                    (prepare_socket(curr_fd, tmp.get()) == (int)
                     INVALID_SOCKET)) { // TODO: use SOCKET all over the way and avoid this cast
                    log_err("Invalid socket");
                    close(curr_fd);
                    rc = SOCKPERF_ERR_SOCKET;
                } else {
                    int i = 0;

                    for (i = 0; i < MAX_ACTIVE_FD_NUM; i++) {
                        tmp->active_fd_list[i] = (int)INVALID_SOCKET; // TODO: use SOCKET all
                                                                      // over the way and avoid
                                                                      // this cast
                    }
                    // TODO: In the following malloc we have a one time memory allocation of
                    // 128KB that are not reclaimed
                    // This O(1) leak was introduced in revision 133
                    tmp->recv.buf = (uint8_t *)MALLOC(sizeof(uint8_t) * 2 * MAX_PAYLOAD_SIZE);
                    if (!tmp->recv.buf) {
                        log_err("Failed to allocate memory with malloc()");
                        rc = SOCKPERF_ERR_NO_MEMORY;
                    } else {
                        tmp->recv.cur_addr = tmp->recv.buf;
                        tmp->recv.max_size = MAX_PAYLOAD_SIZE;
                        tmp->recv.cur_offset = 0;
                        tmp->recv.cur_size = tmp->recv.max_size;

                        if (new_socket_flag) {
                            if (s_fd_num == 1) { /*it is the first fd*/
                                s_fd_min = curr_fd;
                                s_fd_max = curr_fd;
                            } else {
                                g_fds_array[last_fd]->next_fd = curr_fd;
                                s_fd_min = _min(s_fd_min, curr_fd);
                                s_fd_max = _max(s_fd_max, curr_fd);
                            }
                            last_fd = curr_fd;
                            g_fds_array[curr_fd] = tmp.release();
                        }
                    }
                }
            }
        }

        /* Failure check */
        if (rc) {
            if (tmp->active_fd_list) {
                FREE(tmp->active_fd_list);
            }
            if (tmp->recv.buf) {
                FREE(tmp->recv.buf);
            }
        }
    }
#ifdef NEED_REGEX_WORKAROUND
    if (regexp_ip_error == 0) {
        regfree(&regexpr_ip);
    }

    if (regexp_unix_error == 0) {
        regfree(&regexpr_unix);
    }
#endif // NEED_REGEX_WORKAROUND

    if (!rc && (NULL == res) && ferror(file_fd)) {
        /* An I/O error occured */
        log_err("fread() failed to read feedfile '%s'!", feedfile_name);
        rc = SOCKPERF_ERR_FATAL;
    }

    fclose(file_fd);

    /* Check for special case when feedfile is empty */
    if (!rc && (0 == s_fd_num)) {
        log_msg("Feedfile '%s' is empty!", feedfile_name);
        rc = SOCKPERF_ERR_BAD_ARGUMENT;
    }

    if (!rc) {
        g_fds_array[s_fd_max]->next_fd = s_fd_min; /* close loop for fast wrap around in client */

#ifdef ST_TEST
        {
            const char *ip = "224.3.2.1";
            uint32_t port = 11111;
            static fds_data data1, data2;

            tmp = &data1;
            tmp->server_addr_len = sizeof(sockaddr_in);
            sockaddr_in &sa1 = reinterpret_cast<sockaddr_in &>(tmp->server_addr);
            sa1.sin_family = AF_INET;
            sockaddr_set_portn(tmp->server_addr, htons(port));
            if (!inet_aton(ip, &((sockaddr_in &)tmp->server_addr).sin_addr)) {
                log_msg("Invalid input: '%s:%d'", ip, port);
                exit_with_log(SOCKPERF_ERR_INCORRECT);
            }
            tmp->sock_type = sock_type;
            tmp->is_multicast = IN_MULTICAST(ntohl(sa1.sin_addr.s_addr));
            st1 = prepare_socket(tmp, true);

            tmp = &data2;
            tmp->server_addr_len = sizeof(sockaddr_in);
            sockaddr_in &sa2 = reinterpret_cast<sockaddr_in &>(tmp->server_addr);
            sa2.sin_family = AF_INET;
            sockaddr_set_portn(tmp->server_addr, htons(port));
            if (!inet_aton(ip, &((sockaddr_in &)tmp->server_addr).sin_addr)) {
                log_msg("Invalid input: '%s:%d'", ip, port);
                exit_with_log(SOCKPERF_ERR_INCORRECT);
            }
            tmp->sock_type = sock_type;
            tmp->is_multicast = IN_MULTICAST(ntohl(sa2.sin_addr.s_addr));
            st2 = prepare_socket(tmp, true);
        }
#endif
    }

    return rc;
}

//------------------------------------------------------------------------------
/* Sanity check for the sockets list inside g_fds_array. */
static bool fds_array_is_valid() {
    int i;
    int fd;

    /*
     * This function checks that s_fd_max is reachable from s_fd_min in exactly
     * s_fd_num steps. Additionally, s_fd_max's list element must point to
     * s_fd_min.
     * Note, the function doesn't require the list to be sorted.
     */

    for (fd = s_fd_min, i = 0; i < s_fd_num && fd != s_fd_max; ++i) {
        if (g_fds_array[fd] == NULL) {
            return false;
        }
        fd = g_fds_array[fd]->next_fd;
    }
    return ((fd == s_fd_max) && ((i + 1) == s_fd_num) && (g_fds_array[fd]->next_fd == s_fd_min));
}

//------------------------------------------------------------------------------
int bringup(const int *p_daemonize) {
    int rc = SOCKPERF_ERR_NONE;

    os_mutex_init(&_mutex);

    if (*p_daemonize) {
        if (os_daemonize()) {
            log_err("Failed to daemonize");
            rc = SOCKPERF_ERR_FATAL;
        } else {
            log_msg("Running as daemon");
        }
    }

    /* Setup VMA */
    int _vma_pkts_desc_size = 0;

#if defined(USING_VMA_EXTRA_API) || defined(USING_XLIO_EXTRA_API)
    if (!rc && (s_user_params.is_rxfiltercb || s_user_params.is_zcopyread ||
                s_user_params.fd_handler_type == SOCKETXTREME)) {
        // Get VMA extended API
#ifdef USING_VMA_EXTRA_API
        g_vma_api = vma_get_api();
#endif // USING_VMA_EXTRA_API
        if (!g_vma_api) { // Try VMA Extra API
            // Callback and Socketxtreme APIs are supported only by VMA
            if (s_user_params.is_rxfiltercb || s_user_params.fd_handler_type == SOCKETXTREME) {
                exit_with_err("VMA Extra API is not available", SOCKPERF_ERR_FATAL);
            }
        } else {
            log_msg("VMA Extra API is in use");
        }

        if (!g_vma_api) { // Try XLIO
#ifdef USING_XLIO_EXTRA_API
            g_xlio_api = xlio_get_api();
#endif // USING_XLIO_EXTRA_API
            if (!g_xlio_api) {
                errno = EPERM;
                exit_with_err("VMA or XLIO Extra API is not available", SOCKPERF_ERR_FATAL);
            }

            log_msg("XLIO Extra API is in use");
        }

        if (g_vma_api) {
#ifdef USING_VMA_EXTRA_API
            _vma_pkts_desc_size =
                sizeof(struct vma_packets_t) + sizeof(struct vma_packet_t) + sizeof(struct iovec) * 16;
#endif // USING_VMA_EXTRA_API
        } else {
#ifdef USING_XLIO_EXTRA_API
            _vma_pkts_desc_size =
                sizeof(struct xlio_recvfrom_zcopy_packets_t) +
                sizeof(struct xlio_recvfrom_zcopy_packet_t) + sizeof(struct iovec) * 16;
#endif // USING_XLIO_EXTRA_API
        }

    }
#else
    if (!rc && (s_user_params.is_rxfiltercb || s_user_params.is_zcopyread ||
        s_user_params.fd_handler_type == SOCKETXTREME)) {
        errno = EPERM;
        exit_with_err("Please compile with VMA or XLIO Extra API to use these options", SOCKPERF_ERR_FATAL);
    }
#endif // USING_VMA_EXTRA_API || USING_XLIO_EXTRA_API


#if defined(DEFINED_TLS)
    rc = tls_init();
#endif /* DEFINED_TLS */

    /* Create and initialize sockets */
    if (!rc) {
        setbuf(stdout, NULL);

        int _max_buff_size = _max(s_user_params.msg_size + 1, _vma_pkts_desc_size);
        _max_buff_size = _max(_max_buff_size, MAX_PAYLOAD_SIZE);

        Message::initMaxSize(_max_buff_size);

        /* initialize g_fds_array array */
        if (strlen(s_user_params.feedfile_name)) {
            rc = set_sockets_from_feedfile(s_user_params.feedfile_name);
        } else {
            int curr_fd =
                (int)INVALID_SOCKET; // TODO: use SOCKET all over the way and avoid this cast

            std::unique_ptr<fds_data> tmp{ new fds_data };

            if (s_user_params.addr.ss_family == AF_UNIX) {
                log_dbg("UNIX domain socket was provided %s\n", s_user_params.addr.addr_un.sun_path);
                s_user_params.tcp_nodelay = false;
                if (s_user_params.sock_type == SOCK_DGRAM) { // Need to bind localy
                    s_user_params.client_bind_info.ss_family = AF_UNIX;
                }
            }
            memcpy(&tmp->server_addr, &(s_user_params.addr), sizeof(s_user_params.addr));
            tmp->server_addr_len = s_user_params.addr_len;
            tmp->mc_source_ip_addr = s_user_params.mc_source_ip_addr;
            tmp->is_multicast = is_multicast_addr(tmp->server_addr);
            tmp->sock_type = s_user_params.sock_type;

            tmp->active_fd_count = 0;
            tmp->active_fd_list = (int *)MALLOC(MAX_ACTIVE_FD_NUM * sizeof(int));
            if (!tmp->active_fd_list) {
                log_err("Failed to allocate memory with malloc()");
                rc = SOCKPERF_ERR_NO_MEMORY;
            } else {
                /* create a socket */
                if ((curr_fd = (int)socket(tmp->server_addr.ss_family, tmp->sock_type, 0)) <
                    0) { // TODO: use SOCKET all over the way and avoid this cast
                    log_err("socket(AF_INET4/6/AF_UNIX, SOCK_x)");
                    rc = SOCKPERF_ERR_SOCKET;
                } else {
                    if ((curr_fd >= MAX_FDS_NUM) ||
                        (prepare_socket(curr_fd, tmp.get()) ==
                        (int)INVALID_SOCKET)) { // TODO: use SOCKET all over the way and avoid
                                                // this cast
                        log_err("Invalid socket");
                        close(curr_fd);
                        rc = SOCKPERF_ERR_SOCKET;
                    } else {
                        int i = 0;

                        s_fd_num = 1;

                        for (i = 0; i < MAX_ACTIVE_FD_NUM; i++) {
                            tmp->active_fd_list[i] = (int)INVALID_SOCKET;
                        }
                        tmp->recv.buf =
                            (uint8_t *)MALLOC(sizeof(uint8_t) * 2 * MAX_PAYLOAD_SIZE);
                        if (!tmp->recv.buf) {
                            log_err("Failed to allocate memory with malloc()");
                            rc = SOCKPERF_ERR_NO_MEMORY;
                        } else {
                            tmp->recv.cur_addr = tmp->recv.buf;
                            tmp->recv.max_size = MAX_PAYLOAD_SIZE;
                            tmp->recv.cur_offset = 0;
                            tmp->recv.cur_size = tmp->recv.max_size;

                            s_fd_min = s_fd_max = curr_fd;
                            g_fds_array[s_fd_min] = tmp.release();
                            g_fds_array[s_fd_min]->next_fd = s_fd_min;
                        }
                    }
                }
            }

            /* Failure check */
            if (rc) {
                if (tmp->active_fd_list) {
                    FREE(tmp->active_fd_list);
                }
                if (tmp->recv.buf) {
                    FREE(tmp->recv.buf);
                }
            }
        }

        if (!rc && !fds_array_is_valid()) {
            log_err("Sanity check failed for sockets list");
            rc = SOCKPERF_ERR_FATAL;
        }

        if (!rc && (s_user_params.threads_num > s_fd_num || s_user_params.threads_num == 0)) {
            log_msg("Number of threads should be less than sockets count");
            rc = SOCKPERF_ERR_BAD_ARGUMENT;
        }

        if (!rc && s_user_params.dummy_mps && s_user_params.mps >= s_user_params.dummy_mps) {
            log_err(
                "Dummy send is allowed only if dummy-send rate is higher than regular msg rate");
            rc = SOCKPERF_ERR_BAD_ARGUMENT;
        }
    }

    /* Setup internal data */
    if (!rc) {
        int64_t cycleDurationNsec = NSEC_IN_SEC * s_user_params.burst_size / s_user_params.mps;

        if (s_user_params.mps == UINT32_MAX) { // MAX MPS mode
            s_user_params.mps = MPS_MAX;
            cycleDurationNsec = 0;
        }

        /* TicksBase initialization should be done before first TicksBase, TicksDuration etc usage */
        TicksBase::init(s_user_params.b_no_rdtsc ? TicksBase::CLOCK : TicksBase::RDTSC);
        s_user_params.cycleDuration = TicksDuration(cycleDurationNsec);

        if (s_user_params.dummy_mps) { // Calculate dummy send rate
            int64_t dummySendCycleDurationNsec =
                (s_user_params.dummy_mps == UINT32_MAX) ? 0 : NSEC_IN_SEC / s_user_params.dummy_mps;
            s_user_params.dummySendCycleDuration = TicksDuration(dummySendCycleDurationNsec);
        }

        s_user_params.cooldown_msec = TEST_END_COOLDOWN_MSEC;
        s_user_params.warmup_msec = static_cast<uint32_t>(TEST_FIRST_CONNECTION_FIRST_PACKET_TTL_THRESHOLD_MSEC +
                                    s_fd_num * TEST_ANY_CONNECTION_FIRST_PACKET_TTL_THRESHOLD_MSEC);
        if (s_user_params.warmup_msec < TEST_START_WARMUP_MSEC) {
            s_user_params.warmup_msec = TEST_START_WARMUP_MSEC;
        } else {
            log_dbg(
                "Warm-up set in relation to number of active connections. Warm up time: %" PRIu32
                " usec; first connection's first packet TTL: %d usec; following connections' first "
                "packet TTL: %d usec\n",
                s_user_params.warmup_msec,
                TEST_FIRST_CONNECTION_FIRST_PACKET_TTL_THRESHOLD_MSEC * 1000,
                (int)(TEST_ANY_CONNECTION_FIRST_PACKET_TTL_THRESHOLD_MSEC * 1000));
        }
        s_user_params.warmup_num = TEST_START_WARMUP_NUM;
        s_user_params.cooldown_num = TEST_END_COOLDOWN_NUM;

        uint64_t _maxTestDuration = 1 + s_user_params.sec_test_duration +
                                    (s_user_params.warmup_msec + s_user_params.cooldown_msec) /
                                        1000; // + 1sec for timer inaccuracy safety
        uint64_t _maxSequenceNo = _maxTestDuration * s_user_params.mps +
                                  10 * s_user_params.reply_every; // + 10 replies for safety
        _maxSequenceNo += s_user_params.burst_size; // needed for the case burst_size > mps

        if (s_user_params.measurement == NUMBER_BASED) {
            // override to reach max packet count during number based
            _maxSequenceNo = TEST_START_WARMUP_NUM + MAX_PACKET_NUMBER + TEST_END_COOLDOWN_NUM;
        }

        if (s_user_params.pPlaybackVector) {
            _maxSequenceNo = s_user_params.pPlaybackVector->size();
        }

        /* SERVER does not have info about max number of expected packets */
        if (s_user_params.mode == MODE_SERVER) {
            _maxSequenceNo = UINT64_MAX;
        }

        Message::initMaxSeqNo(_maxSequenceNo);

        if (!s_user_params.b_stream && s_user_params.mode == MODE_CLIENT) {
            g_pPacketTimes = new PacketTimes(_maxSequenceNo, s_user_params.reply_every,
                                             s_user_params.client_work_with_srv_num);
        }

        os_set_signal_action(SIGINT, s_user_params.mode ? server_sig_handler : client_sig_handler);
    }

    return rc;
}

//------------------------------------------------------------------------------
void do_test() {
    handler_info info;

    info.id = 0;
    info.fd_min = s_fd_min;
    info.fd_max = s_fd_max;
    info.fd_num = s_fd_num;
    switch (s_user_params.mode) {
    case MODE_CLIENT:
        client_handler(&info);
        break;
    case MODE_SERVER:
        if (s_user_params.mthread_server) {
            server_select_per_thread(s_fd_num);
        } else {
            server_handler(&info);
        }
        break;
    case MODE_BRIDGE:
        server_handler(&info);
        break;
    }

    cleanup();
}

//------------------------------------------------------------------------------
int main(int argc, char *argv[]) {
    try {
        int rc = SOCKPERF_ERR_NONE;

        // step #1:  set default values for command line args
        set_defaults();

        // step #2:  parse command line args
        if (argc > 1) {
            int i = 0;
            int j = 0;
            int found = 0;

            for (i = 0; sockperf_modes[i].name != NULL; i++) {
                if (strcmp(sockperf_modes[i].name, argv[1]) == 0) {
                    found = 1;
                } else {
                    for (j = 0; sockperf_modes[i].shorts[j] != NULL; j++) {
                        if (strcmp(sockperf_modes[i].shorts[j], argv[1]) == 0) {
                            found = 1;
                            break;
                        }
                    }
                }

                if (found) {
                    rc = sockperf_modes[i].func(i, argc - 1, (const char **)(argv + 1));
                    break;
                }
            }
            /*  check if the first option is invalid or rc > 0 */
            if ((sockperf_modes[i].name == NULL) || (rc > 0)) {
                rc = proc_mode_help(0, 0, NULL);
            }
        } else {
            rc = proc_mode_help(0, 0, NULL);
        }

        if (rc) {
            cleanup();
            exit(0);
        }

        // Prepare application to start
        rc = bringup(&s_user_params.daemonize);
        if (rc) {
            exit_with_log(rc); // will also perform cleanup
        }

        std::string client_bind_info_str = sockaddr_to_hostport(s_user_params.client_bind_info);

        log_dbg(
            "+INFO:\n\t\
mode = %d \n\t\
measurement = %d \n\t\
with_sock_accl = %d \n\t\
msg_size = %d \n\t\
msg_size_range = %d \n\t\
sec_test_duration = %d \n\t\
number_test_target = %" PRIu64 " \n\t\
data_integrity = %d \n\t\
packetrate_stats_print_ratio = %d \n\t\
burst_size = %d \n\t\
packetrate_stats_print_details = %d \n\t\
fd_handler_type = %d \n\t\
mthread_server = %d \n\t\
sock_buff_size = %d \n\t\
threads_num = %d \n\t\
threads_affinity = %s \n\t\
is_blocked = %d \n\t\
is_nonblocked_send = %d \n\t\
do_warmup = %d \n\t\
pre_warmup_wait = %d \n\t\
is_rxfiltercb = %d \n\t\
is_zcopyread = %d \n\t\
mc_loop_disable = %d \n\t\
mc_ttl = %d \n\t\
uc_reuseaddr = %d \n\t\
tcp_nodelay = %d \n\t\
client_work_with_srv_num = %d \n\t\
b_server_reply_via_uc = %d \n\t\
b_server_dont_reply = %d \n\t\
b_server_detect_gaps = %d\n\t\
mps = %d \n\t\
client_bind_info = %s \n\t\
reply_every = %d \n\t\
b_client_ping_pong = %d \n\t\
b_no_rdtsc = %d \n\t\
sender_affinity = %s \n\t\
receiver_affinity = %s \n\t\
b_stream = %d \n\t\
daemonize = %d \n\t\
feedfile_name = %s \n\t\
tos = %d \n\t\
packet pace limit = %d",
            s_user_params.mode, s_user_params.measurement, s_user_params.withsock_accl,
            s_user_params.msg_size, s_user_params.msg_size_range, s_user_params.sec_test_duration,
            s_user_params.number_test_target, s_user_params.data_integrity,
            s_user_params.packetrate_stats_print_ratio, s_user_params.burst_size,
            s_user_params.packetrate_stats_print_details, s_user_params.fd_handler_type,
            s_user_params.mthread_server, s_user_params.sock_buff_size, s_user_params.threads_num,
            s_user_params.threads_affinity, s_user_params.is_blocked, s_user_params.is_nonblocked_send,
            s_user_params.do_warmup, s_user_params.pre_warmup_wait, s_user_params.is_rxfiltercb,
            s_user_params.is_zcopyread, s_user_params.mc_loop_disable, s_user_params.mc_ttl,
            s_user_params.uc_reuseaddr, s_user_params.tcp_nodelay,
            s_user_params.client_work_with_srv_num, s_user_params.b_server_reply_via_uc,
            s_user_params.b_server_dont_reply, s_user_params.b_server_detect_gaps,
            s_user_params.mps, client_bind_info_str.c_str(), s_user_params.reply_every,
            s_user_params.b_client_ping_pong, s_user_params.b_no_rdtsc,
            (strlen(s_user_params.sender_affinity) ? s_user_params.sender_affinity : "<empty>"),
            (strlen(s_user_params.receiver_affinity) ? s_user_params.receiver_affinity : "<empty>"),
            s_user_params.b_stream, s_user_params.daemonize,
            (strlen(s_user_params.feedfile_name) ? s_user_params.feedfile_name : "<empty>"),
            s_user_params.tos, s_user_params.rate_limit);

        // Display application version
        log_msg(MAGNETA "== version #%s == " ENDCOLOR, VERSION);

// Display VMA version
#ifdef VMA_LIBRARY_MAJOR
        log_msg("Linked with VMA version: %d.%d.%d.%d", VMA_LIBRARY_MAJOR, VMA_LIBRARY_MINOR,
                VMA_LIBRARY_REVISION, VMA_LIBRARY_RELEASE);
#endif
#ifdef VMA_DATE_TIME
        log_msg("VMA Build Date: %s", VMA_DATE_TIME);
#endif

        // step #4:  get prepared for test - store application context in global variables (will use
        // const member)
        App app(s_user_params, s_mutable_params);
        g_pApp = &app;

        // temp test for measuring time of taking time
        const int SIZE = 1000;
        TicksTime start, end;
        start.setNow();
        for (int i = 0; i < SIZE; i++) {
            end.setNow();
        }
        log_dbg("+INFO: taking time, using the given settings, consumes %.3lf nsec",
                (double)(end - start).toNsec() / SIZE);

        ticks_t tstart = 0, tend = 0;
        tstart = os_gettimeoftsc();

        for (int i = 0; i < SIZE; i++) {
            tend = os_gettimeoftsc();
        }
        double tdelta = (double)tend - (double)tstart;
        double ticks_per_second = (double)get_tsc_rate_per_second();
        log_dbg("+INFO: taking rdtsc directly consumes %.3lf nsec",
                tdelta / SIZE * 1000 * 1000 * 1000 / ticks_per_second);

        // step #5: check is user defined a specific SEED value to be used in all rand() calls
        // if no seed value is provided, the rand() function is automatically seeded with a value of
        // 1.
        char *env_ptr = NULL;
        if ((env_ptr = getenv("SEED")) != NULL) {
            int seed = (unsigned)atoi(env_ptr);
            srand(seed);
        }

        // step #6: do run the test
        /*
         ** TEST START
         */
        do_test();
        /*
         ** TEST END
         */
    }
    catch (const std::exception &e) {
        printf(MODULE_NAME
               ": test failed because of an exception with the following information:\n\t%s\n",
               e.what());
        exit_with_log(SOCKPERF_ERR_FATAL);
    }
    catch (...) {
        printf(MODULE_NAME ": test failed because of an unknown exception \n");
        exit_with_log(SOCKPERF_ERR_FATAL);
    }

    exit(0);
}
