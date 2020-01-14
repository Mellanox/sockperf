/*
 * Copyright (c) 2011-2020 Mellanox Technologies Ltd.
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

#define __STDC_LIMIT_MACROS // for UINT32_MAX in C++
#include <stdint.h>
#include <stdlib.h>
#include "common.h"
#include "message.h"
#include "packet.h"
#include "switches.h"
#include "aopt.h"
#include <stdio.h>
#include <sys/stat.h>

#ifndef WIN32
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
      { proc_mode_playback, "playback", aopt_set_string("pb"),
        "Run " MODULE_NAME " client for latency test using playback of predefined traffic, based "
        "on timeline and message size." },
      { proc_mode_throughput,  "throughput",
        aopt_set_string("tp"), "Run " MODULE_NAME " client for one way throughput test." },
      { proc_mode_server, "server", aopt_set_string("sr"), "Run " MODULE_NAME " as a server." },
      { NULL, NULL, aopt_set_string(NULL), NULL }
  };

os_mutex_t _mutex;
static int parse_common_opt(const AOPT_OBJECT *);
static int parse_client_opt(const AOPT_OBJECT *);
static char *display_opt(int, char *, size_t);

/*
 * List of supported general options.
 */
static const AOPT_DESC common_opt_desc[] = {
    { 'h',                              AOPT_NOARG,
      aopt_set_literal('h', '?'),       aopt_set_string("help", "usage"),
      "Show the help message and exit." },
    { OPT_TCP,                AOPT_NOARG,                       aopt_set_literal(0),
      aopt_set_string("tcp"), "Use TCP protocol (default UDP)." },
    { 'i', AOPT_ARG, aopt_set_literal('i'), aopt_set_string("ip"), "Listen on/send to ip <ip>." },
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
#ifdef WIN32
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
#ifdef WIN32
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
    { OPT_RX_MC_IF, AOPT_ARG, aopt_set_literal(0), aopt_set_string("mc-rx-if"),
      "Set address <ip> of interface on which to receive mulitcast messages (can be other then "
      "route table)." },
    { OPT_TX_MC_IF, AOPT_ARG, aopt_set_literal(0), aopt_set_string("mc-tx-if"),
      "Set address <ip> of interface on which to transmit mulitcast messages (can be other then "
      "route table)." },
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
      "Set address <ip, hostname> of mulitcast messages source which is allowed to receive from." },
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
#ifndef WIN32
    { OPT_VMAZCOPYREAD,                                   AOPT_NOARG,
      aopt_set_literal(0),                                aopt_set_string("vmazcopyread"),
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
    { OPT_RATE_LIMIT, AOPT_ARG, aopt_set_literal(0), aopt_set_string("rate-limit"),
      "use rate limit (packet-pacing), with VMA must be run with VMA_RING_ALLOCATION_LOGIC_TX "
      "mode." },
#endif
    { OPT_SOCK_ACCL,
      AOPT_NOARG,
      aopt_set_literal(0),
      aopt_set_string("set-sock-accl"),
      "Set socket accleration before run (available for some of Mellanox systems)" },
    { 'd',                      AOPT_NOARG,                      aopt_set_literal('d'),
      aopt_set_string("debug"), "Print extra debug information." },
    { 0, AOPT_NOARG, aopt_set_literal(0), aopt_set_string(NULL), NULL }
};

/*
 * List of supported client options.
 */
static const AOPT_DESC client_opt_desc[] = {
    { OPT_CLIENT_WORK_WITH_SRV_NUM,                    AOPT_ARG,
      aopt_set_literal(0),                             aopt_set_string("srv-num"),
      "Set num of servers the client works with to N." },
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
        { OPT_CLIENTIP,
          AOPT_ARG,
          aopt_set_literal(0),
          aopt_set_string("client_ip"),
          "Force the client side to bind to a specific ip address (default = 0). " },
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
    if (!rc && common_obj) {
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

        if (!rc && aopt_check(self_obj, OPT_CLIENTPORT)) {
            if (aopt_check(common_obj, 'f')) {
                log_msg("--client_port conflicts with -f option");
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            } else {
                const char *optarg = aopt_value(self_obj, OPT_CLIENTPORT);
                if (optarg) {
                    errno = 0;
                    long value = strtol(optarg, NULL, 0);
                    /* strtol() returns 0 if there were no digits at all */
                    if (errno != 0) {
                        log_msg("'-%c' Invalid port: %s", OPT_CLIENTPORT, optarg);
                        rc = SOCKPERF_ERR_BAD_ARGUMENT;
                    } else {
                        s_user_params.client_bind_info.sin_port = htons((uint16_t)value);
                    }
                } else {
                    log_msg("'-%c' Invalid value", OPT_CLIENTPORT);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                }
            }
        }

        if (!rc && aopt_check(self_obj, OPT_CLIENTIP)) {
            int len;
            const char *optarg = aopt_value(self_obj, OPT_CLIENTIP);
            if (!optarg) { /* already in network byte order*/
                log_msg("'-%c' Invalid address", OPT_CLIENTIP);
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            } else if (4 != sscanf(optarg, "%d.%d.%d.%d", &len, &len, &len, &len)) {
                log_msg("'-%c' Invalid address: %s", OPT_CLIENTIP, optarg);
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            } else if (!inet_aton(optarg, &s_user_params.client_bind_info.sin_addr)) {
                log_msg("'-%c' Invalid address: %s", OPT_CLIENTIP, optarg);
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

    if (rc) {
        const char *help_str = NULL;
        char temp_buf[30];

        printf("%s: %s\n", display_opt(id, temp_buf, sizeof(temp_buf)), sockperf_modes[id].note);
        printf("\n");
        printf("Usage: " MODULE_NAME " %s [options] [args]...\n", sockperf_modes[id].name);
        printf(" " MODULE_NAME
               " %s -i ip  [-p port] [-m message_size] [-t time] [--data_integrity]\n",
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
        { OPT_CLIENTPORT,
          AOPT_ARG,
          aopt_set_literal(0),
          aopt_set_string("client_port"),
          "Force the client side to bind to a specific port (default = 0). " },
        { OPT_CLIENTIP,
          AOPT_ARG,
          aopt_set_literal(0),
          aopt_set_string("client_ip"),
          "Force the client side to bind to a specific ip address (default = 0). " },
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
    s_user_params.b_client_ping_pong = true;
    s_user_params.mps = UINT32_MAX;
    s_user_params.reply_every = 1;

    /* Set command line common options */
    if (!rc && common_obj) {
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

        if (!rc && aopt_check(self_obj, OPT_CLIENTPORT)) {
            if (aopt_check(common_obj, 'f')) {
                log_msg("--client_port conflicts with -f option");
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            } else {
                const char *optarg = aopt_value(self_obj, OPT_CLIENTPORT);
                if (optarg) {
                    errno = 0;
                    long value = strtol(optarg, NULL, 0);
                    /* strtol() returns 0 if there were no digits at all */
                    if (errno != 0) {
                        log_msg("'-%c' Invalid port: %s", OPT_CLIENTPORT, optarg);
                        rc = SOCKPERF_ERR_BAD_ARGUMENT;
                    } else {
                        s_user_params.client_bind_info.sin_port = htons((uint16_t)value);
                    }
                } else {
                    log_msg("'-%c' Invalid value", OPT_CLIENTPORT);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                }
            }
        }

        if (!rc && aopt_check(self_obj, OPT_CLIENTIP)) {
            int len;
            const char *optarg = aopt_value(self_obj, OPT_CLIENTIP);
            if (!optarg) { /* already in network byte order*/
                log_msg("'-%c' Invalid address", OPT_CLIENTIP);
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            } else if (4 != sscanf(optarg, "%d.%d.%d.%d", &len, &len, &len, &len)) {
                log_msg("'-%c' Invalid address: %s", OPT_CLIENTIP, optarg);
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            } else if (!inet_aton(optarg, &s_user_params.client_bind_info.sin_addr)) {
                log_msg("'-%c' Invalid address: %s", OPT_CLIENTIP, optarg);
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
    }

    if (rc) {
        const char *help_str = NULL;
        char temp_buf[30];

        printf("%s: %s\n", display_opt(id, temp_buf, sizeof(temp_buf)), sockperf_modes[id].note);
        printf("\n");
        printf("Usage: " MODULE_NAME " %s [options] [args]...\n", sockperf_modes[id].name);
        printf(" " MODULE_NAME " %s -i ip  [-p port] [-m message_size] [-t time]\n",
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
        { OPT_CLIENTIP,
          AOPT_ARG,
          aopt_set_literal(0),
          aopt_set_string("client_ip"),
          "Force the client side to bind to a specific ip address (default = 0). " },
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
    if (!rc && common_obj) {
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

        if (!rc && aopt_check(self_obj, OPT_CLIENTPORT)) {
            if (aopt_check(common_obj, 'f')) {
                log_msg("--client_port conflicts with -f option");
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            } else {
                const char *optarg = aopt_value(self_obj, OPT_CLIENTPORT);
                if (optarg) {
                    errno = 0;
                    long value = strtol(optarg, NULL, 0);
                    /* strtol() returns 0 if there were no digits at all */
                    if (errno != 0) {
                        log_msg("'-%c' Invalid port: %s", OPT_CLIENTPORT, optarg);
                        rc = SOCKPERF_ERR_BAD_ARGUMENT;
                    } else {
                        s_user_params.client_bind_info.sin_port = htons((uint16_t)value);
                    }
                } else {
                    log_msg("'-%c' Invalid value", OPT_CLIENTPORT);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                }
            }
        }

        if (!rc && aopt_check(self_obj, OPT_CLIENTIP)) {
            int len;
            const char *optarg = aopt_value(self_obj, OPT_CLIENTIP);
            if (!optarg) { /* already in network byte order*/
                log_msg("'-%c' Invalid address", OPT_CLIENTIP);
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            } else if (4 != sscanf(optarg, "%d.%d.%d.%d", &len, &len, &len, &len)) {
                log_msg("'-%c' Invalid address: %s", OPT_CLIENTIP, optarg);
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            } else if (!inet_aton(optarg, &s_user_params.client_bind_info.sin_addr)) {
                log_msg("'-%c' Invalid address: %s", OPT_CLIENTIP, optarg);
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

    if (rc) {
        const char *help_str = NULL;
        char temp_buf[30];

        printf("%s: %s\n", display_opt(id, temp_buf, sizeof(temp_buf)), sockperf_modes[id].note);
        printf("\n");
        printf("Usage: " MODULE_NAME " %s [options] [args]...\n", sockperf_modes[id].name);
        printf(" " MODULE_NAME " %s -i ip  [-p port] [-m message_size] [-t time]\n",
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
    if (!rc && common_obj) {
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
        printf(" " MODULE_NAME " %s -i ip  [-p port] --data-file playback.csv\n",
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
#ifndef WIN32
        { OPT_VMARXFILTERCB,
          AOPT_NOARG,
          aopt_set_literal(0),
          aopt_set_string("vmarxfiltercb"),
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
    if (!rc && common_obj) {
        rc = parse_common_opt(common_obj);
    }

    /* Set command line specific values */
    if (!rc && server_obj) {
        struct sockaddr_in *p_addr = &s_user_params.addr;

        if (!rc && aopt_check(server_obj, 'B')) {
            log_msg("update to bridge mode");
            s_user_params.mode = MODE_BRIDGE;
            p_addr->sin_port = htons(5001); /*iperf's default port*/
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
#ifndef WIN32
        if (!rc && aopt_check(server_obj, OPT_VMARXFILTERCB)) {
            s_user_params.is_vmarxfiltercb = true;
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
               " %s [-i ip] [-p port] [--mc-rx-if ip] [--mc-tx-if ip] [--mc-source-filter ip]\n",
               sockperf_modes[id].name);
#ifndef WIN32
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

//------------------------------------------------------------------------------
static int parse_common_opt(const AOPT_OBJECT *common_obj) {
    int rc = SOCKPERF_ERR_NONE;

    if (common_obj) {
        struct sockaddr_in *p_addr = &s_user_params.addr;
        int *p_daemonize = &s_user_params.daemonize;
        char *feedfile_name = s_user_params.feedfile_name;

        if (!rc && aopt_check(common_obj, 'd')) {
            g_debug_level = LOG_LVL_DEBUG;
        }

        if (!rc && aopt_check(common_obj, 'i')) {
            const char *optarg = aopt_value(common_obj, 'i');
            int len;
            if (!optarg) { /* already in network byte order*/
                log_msg("'-%c' Invalid address", 'i');
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            } else if (4 != sscanf(optarg, "%d.%d.%d.%d", &len, &len, &len, &len)) {
                log_msg("'-%c' Invalid address: %s", 'i', optarg);
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            } else if (!inet_aton(optarg, &p_addr->sin_addr)) {
                struct hostent *hostip = gethostbyname(optarg);
                if (hostip) {
                    memcpy(&p_addr->sin_addr, hostip->h_addr_list[0], hostip->h_length);
                    s_user_params.fd_handler_type = RECVFROM;
                } else {
                    log_msg("'-%c' Invalid address: %s", 'i', optarg);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                }
            } else {
                s_user_params.fd_handler_type = RECVFROM;
            }
        }

        if (!rc && aopt_check(common_obj, 'p')) {
            const char *optarg = aopt_value(common_obj, 'p');
            if (optarg && (isNumeric(optarg))) {
                errno = 0;
                long value = strtol(optarg, NULL, 0);
                /* strtol() returns 0 if there were no digits at all */
                if (errno != 0) {
                    log_msg("'-%c' Invalid port: %s", 'p', optarg);
                    rc = SOCKPERF_ERR_BAD_ARGUMENT;
                } else {
                    p_addr->sin_port = htons((uint16_t)value);
                    s_user_params.fd_handler_type = RECVFROM;
                }
            } else {
                log_msg("'-%c' Invalid value", 'p');
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(common_obj, 'f')) {
            if (!aopt_check(common_obj, 'i') && !aopt_check(common_obj, 'p')) {
                const char *optarg = aopt_value(common_obj, 'f');
                if (optarg) {
                    strncpy(feedfile_name, optarg, MAX_ARGV_SIZE);
                    feedfile_name[MAX_PATH_LENGTH - 1] = '\0';
#if defined(WIN32) || defined(__FreeBSD__)
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
#ifndef WIN32
#ifndef __FreeBSD__
                    if (!strcmp(fd_handle_type, "epoll") || !strcmp(fd_handle_type, "e")) {
                        s_user_params.fd_handler_type = EPOLL;
                    } else
#endif
                        if (!strcmp(fd_handle_type, "poll") || !strcmp(fd_handle_type, "p")) {
                        s_user_params.fd_handler_type = POLL;
                    } else
#endif
                        if (!strcmp(fd_handle_type, "select") || !strcmp(fd_handle_type, "s")) {
                        s_user_params.fd_handler_type = SELECT;
                    } else if (!strcmp(fd_handle_type, "recvfrom") ||
                               !strcmp(fd_handle_type, "r")) {
                        s_user_params.fd_handler_type = RECVFROMMUX;
                    } else if (!strcmp(fd_handle_type, "socketxtreme") ||
                               !strcmp(fd_handle_type, "x")) {
                        s_user_params.fd_handler_type = SOCKETXTREME;
                        s_user_params.is_blocked = false;
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
            if (!optarg || ((s_user_params.rx_mc_if_addr.s_addr = inet_addr(optarg)) ==
                            INADDR_NONE)) { /* already in network byte order*/
                log_msg("'-%d' Invalid address: %s", OPT_RX_MC_IF, optarg);
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(common_obj, OPT_TX_MC_IF)) {
            const char *optarg = aopt_value(common_obj, OPT_TX_MC_IF);
            if (!optarg || ((s_user_params.tx_mc_if_addr.s_addr = inet_addr(optarg)) ==
                            INADDR_NONE)) { /* already in network byte order*/
                log_msg("'-%d' Invalid address: %s", OPT_TX_MC_IF, optarg);
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(common_obj, OPT_MC_SOURCE_IP)) {
            const char *optarg = aopt_value(common_obj, OPT_MC_SOURCE_IP);
            if (optarg) {
                if (!inet_aton(optarg, &(s_user_params.mc_source_ip_addr))) {
                    struct hostent *hostip = gethostbyname(optarg);
                    if (hostip) {
                        memcpy(&(s_user_params.mc_source_ip_addr), hostip->h_addr_list[0],
                               hostip->h_length);
                    } else {
                        log_msg("Invalid multicast source address: '%s'", optarg);
                        rc = SOCKPERF_ERR_BAD_ARGUMENT;
                    }
                }
            } else {
                log_msg("'-%d' Invalid address: %s", OPT_MC_SOURCE_IP, optarg);
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }

        if (!rc && aopt_check(common_obj, OPT_SELECT_TIMEOUT)) {
            const char *optarg = aopt_value(common_obj, OPT_SELECT_TIMEOUT);
            if (optarg) {
                errno = 0;
                int value = strtol(optarg, NULL, 0);
                if (errno != 0 || value < -1) {
#ifdef WIN32
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
#if defined(WIN32) || defined(_WIN32)
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
#if defined(WIN32) || defined(_WIN32)
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
#ifndef WIN32

        if (!rc && aopt_check(common_obj, OPT_VMAZCOPYREAD)) {
            s_user_params.is_vmazcopyread = true;
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

#if defined(__arm__) || defined(__aarch64__)
        if (s_user_params.b_no_rdtsc == false) {
            log_msg("ARM target build does not support rdtsc, use --no-rdtsc");
            rc = SOCKPERF_ERR_BAD_ARGUMENT;
        }
#endif
#ifndef __FreeBSD__
        if (!rc && aopt_check(common_obj, OPT_LOAD_VMA)) {
            const char *optarg = aopt_value(common_obj, OPT_LOAD_VMA);
            // s_user_params.b_load_vma = true;
            if (!optarg || !*optarg) optarg = (char *)"libvma.so"; // default value
            bool success = vma_set_func_pointers(optarg);
            if (!success) {
                log_msg("Invalid --load-vma value: %s: failed to set function pointers using the "
                        "given libvma.so path:",
                        optarg);
                log_msg("dlerror() says: %s:", dlerror());
                rc = SOCKPERF_ERR_BAD_ARGUMENT;
            }
        }
#endif
#endif

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
                    log_msg("Invalid %d val. Can't open file %s for writing: %m", OPT_FULL_LOG,
                            optarg);
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
// use it with command such as: VMA ./sockperf -s -f conf/conf.inp --vmarxfiltercb

#ifdef ST_TEST
static int st1, st2;
#endif
//------------------------------------------------------------------------------
void cleanup() {
    os_mutex_lock(&_mutex);
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
                FREE(g_fds_array[ifd]);
            }
        }
    }

    if (s_user_params.select_timeout) {
        FREE(s_user_params.select_timeout);
    }
#ifdef USING_VMA_EXTRA_API
    if (g_vma_api && s_user_params.is_vmazcopyread) {
        zeroCopyMap::iterator it;
        while ((it = g_zeroCopyData.begin()) != g_zeroCopyData.end()) {
            delete it->second;
            g_zeroCopyData.erase(it);
        }
    }
#endif

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
#if !defined(WIN32) && !defined(__FreeBSD__)
    bool success = vma_set_func_pointers(false);
    if (!success) {
        log_dbg("Failed to set function pointers for system functions.");
        log_dbg("Check vma-redirect.cpp for functions which your OS implementation is missing. "
                "Re-compile sockperf without them.");
    }
#endif

    g_fds_array = (fds_data **)MALLOC(MAX_FDS_NUM * sizeof(fds_data *));
    if (!g_fds_array) {
        log_err("Failed to allocate memory for global pointer fds_array");
        exit_with_log(SOCKPERF_ERR_NO_MEMORY);
    }
    int igmp_max_memberships = read_int_from_sys_file("/proc/sys/net/ipv4/igmp_max_memberships");
    if (igmp_max_memberships != -1) IGMP_MAX_MEMBERSHIPS = igmp_max_memberships;

    memset((void *)&s_user_params, 0, sizeof(s_user_params));
    memset(g_fds_array, 0, sizeof(fds_data *) * MAX_FDS_NUM);
    s_user_params.rx_mc_if_addr.s_addr = htonl(INADDR_ANY);
    s_user_params.tx_mc_if_addr.s_addr = htonl(INADDR_ANY);
    s_user_params.mc_source_ip_addr.s_addr = htonl(INADDR_ANY);
    s_user_params.sec_test_duration = DEFAULT_TEST_DURATION;
    s_user_params.client_bind_info.sin_family = AF_INET;
    s_user_params.client_bind_info.sin_addr.s_addr = INADDR_ANY;
    s_user_params.client_bind_info.sin_port = 0;
    s_user_params.mode = MODE_SERVER;
    s_user_params.packetrate_stats_print_ratio = 0;
    s_user_params.packetrate_stats_print_details = false;
    s_user_params.burst_size = 1;
    s_user_params.data_integrity = false;
    s_user_params.fd_handler_type = RECVFROM;
    s_user_params.mthread_server = 0;
    s_user_params.msg_size = MIN_PAYLOAD_SIZE;
    s_user_params.msg_size_range = 0;
    s_user_params.sock_buff_size = SOCK_BUFF_DEFAULT_SIZE;
    set_select_timeout(DEFAULT_SELECT_TIMEOUT_MSEC);
    s_user_params.threads_num = 1;
    memset(s_user_params.threads_affinity, 0, sizeof(s_user_params.threads_affinity));
    s_user_params.is_blocked = true;
    s_user_params.is_nonblocked_send = false;
    s_user_params.max_looping_over_recv = 1;
    s_user_params.do_warmup = true;
    s_user_params.pre_warmup_wait = 0;
    s_user_params.is_vmarxfiltercb = false;
    s_user_params.is_vmazcopyread = false;
    g_debug_level = LOG_LVL_INFO;
    s_user_params.mc_loop_disable = true;
    s_user_params.uc_reuseaddr = false;
    s_user_params.client_work_with_srv_num = DEFAULT_CLIENT_WORK_WITH_SRV_NUM;
    s_user_params.b_server_reply_via_uc = false;
    s_user_params.b_server_dont_reply = false;
    s_user_params.b_server_detect_gaps = false;

    s_user_params.mps = MPS_DEFAULT;
    s_user_params.reply_every = REPLY_EVERY_DEFAULT;
    s_user_params.b_client_ping_pong = false;
#if defined(__arm__) || defined(__aarch64__)
    s_user_params.b_no_rdtsc = true;
#else
    s_user_params.b_no_rdtsc = false;
#endif
    memset(s_user_params.sender_affinity, 0, sizeof(s_user_params.sender_affinity));
    memset(s_user_params.receiver_affinity, 0, sizeof(s_user_params.receiver_affinity));
    // s_user_params.b_load_vma = false;
    s_user_params.fileFullLog = NULL;
    s_user_params.b_stream = false;
    s_user_params.full_rtt = false;
    s_user_params.giga_size = false;
    s_user_params.increase_output_precision = false;
    s_user_params.pPlaybackVector = NULL;

    s_user_params.addr.sin_family = AF_INET;
    s_user_params.addr.sin_port = htons(DEFAULT_PORT);
    inet_aton(DEFAULT_MC_ADDR, &s_user_params.addr.sin_addr);
    s_user_params.sock_type = SOCK_DGRAM;
    s_user_params.tcp_nodelay = true;
    s_user_params.mc_ttl = 2;

    s_user_params.daemonize = false;

    s_user_params.withsock_accl = false;
    s_user_params.dummy_mps = 0;
    memset(s_user_params.feedfile_name, 0, sizeof(s_user_params.feedfile_name));
    s_user_params.tos = 0x00;
}

//------------------------------------------------------------------------------
#ifdef USING_VMA_EXTRA_API
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

    if (iov_sz) {
    };
    if (context) {
    };

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
        g_zeroCopyData[fd]->m_pkts->n_packet_num > 0) {
        return VMA_PACKET_RECV;
    }

    size_t index;
    int nbytes;
    struct fds_data *l_fds_ifd;
    Message *msgReply;
    uint8_t *start_addrs;
    struct sockaddr_in sendto_addr;

    l_fds_ifd = g_fds_array[fd];

    if (!l_fds_ifd) {
        return VMA_PACKET_RECV;
    }

    msgReply = l_fds_ifd->p_msg;

    // Copy and concatenate received data in local reserved buffer
    nbytes = 0;
    for (index = 0; index < iov_sz; index++) {
        nbytes += iov[index].iov_len;
    }
    if (nbytes > l_fds_ifd->recv.cur_size) {
        memmove(l_fds_ifd->recv.buf, l_fds_ifd->recv.cur_addr, l_fds_ifd->recv.cur_offset);
        l_fds_ifd->recv.cur_addr = l_fds_ifd->recv.buf;
        l_fds_ifd->recv.cur_size = l_fds_ifd->recv.max_size - l_fds_ifd->recv.cur_offset;
        if (nbytes > l_fds_ifd->recv.cur_size) {
            log_msg("Can't handle data in callback : Received data bigger than available buffer");
            /*
             * TODO going to recvfrom will not work if this callback call is coming from recvfrom
             * context.
             * this is because the callback is working on the same buffer that was given to
             * recvfrom, so
             * recvfrom will return and override the data that the callback wrote.
             * if working with recvfrom, and not with iomux, need to give recvfrom and callback
             * different buffers.
             * This should be fixed in sockperf, so we also won't need to call the above memmove.
             */
            return VMA_PACKET_RECV;
        }
    }
    nbytes = 0;
    for (index = 0; index < iov_sz; index++) {
        start_addrs = l_fds_ifd->recv.cur_addr + l_fds_ifd->recv.cur_offset + nbytes;
        memcpy(start_addrs, iov[index].iov_base, iov[index].iov_len);
        nbytes += iov[index].iov_len;
    }

    while (nbytes) {
        /* 1: message header is not received yet */
        if ((l_fds_ifd->recv.cur_offset + nbytes) < MsgHeader::EFFECTIVE_SIZE) {
            l_fds_ifd->recv.cur_size -= nbytes;
            l_fds_ifd->recv.cur_offset += nbytes;

            /* 4: set current buffer size to size of remained part of message header to
             *    guarantee getting full message header on next iteration
             */
            if (l_fds_ifd->recv.cur_size < MsgHeader::EFFECTIVE_SIZE) {
                l_fds_ifd->recv.cur_size = MsgHeader::EFFECTIVE_SIZE - l_fds_ifd->recv.cur_offset;
            }
            return VMA_PACKET_DROP;
        } else if (l_fds_ifd->recv.cur_offset < MsgHeader::EFFECTIVE_SIZE) {
            /* 2: message header is got, match message to cycle buffer */
            msgReply->setBuf(l_fds_ifd->recv.cur_addr);
            msgReply->setHeaderToHost();
        } else {
            /* 2: message header is got, match message to cycle buffer */
            msgReply->setBuf(l_fds_ifd->recv.cur_addr);
        }

        if ((unsigned)msgReply->getLength() > (unsigned)MAX_PAYLOAD_SIZE) {
            log_msg("Message received was larger than expected, handle from recv_from");
            return VMA_PACKET_RECV;
        }
        /* 3: message is not complete */
        if ((l_fds_ifd->recv.cur_offset + nbytes) < msgReply->getLength()) {
            l_fds_ifd->recv.cur_size -= nbytes;
            l_fds_ifd->recv.cur_offset += nbytes;
            /* 4: set current buffer size to size of remained part of message to
             *    guarantee getting full message on next iteration (using extended reserved memory)
             *    and shift to start of cycle buffer
             */
            if (l_fds_ifd->recv.cur_size < (int)msgReply->getMaxSize()) {
                l_fds_ifd->recv.cur_size = msgReply->getLength() - l_fds_ifd->recv.cur_offset;
            }
            return VMA_PACKET_DROP;
        }
        /* 5: message is complete shift to process next one */
        nbytes -= msgReply->getLength() - l_fds_ifd->recv.cur_offset;
        l_fds_ifd->recv.cur_addr += msgReply->getLength();
        l_fds_ifd->recv.cur_size -= msgReply->getLength() - l_fds_ifd->recv.cur_offset;
        l_fds_ifd->recv.cur_offset = 0;

        if (g_b_exit) return VMA_PACKET_DROP;
        if (!msgReply->isClient()) {
            /* 6: shift to start of cycle buffer in case receiving buffer is empty and
             * there is no uncompleted message
             */
            if (!nbytes) {
                l_fds_ifd->recv.cur_addr = l_fds_ifd->recv.buf;
                l_fds_ifd->recv.cur_size = l_fds_ifd->recv.max_size;
                l_fds_ifd->recv.cur_offset = 0;
            }
            return VMA_PACKET_DROP;
        }

        if (msgReply->isWarmupMessage()) {
            // m_switchCalcGaps.execute(vma_info->src, 0, true);
            /* 6: shift to start of cycle buffer in case receiving buffer is empty and
             * there is no uncompleted message
             */
            if (!nbytes) {
                l_fds_ifd->recv.cur_addr = l_fds_ifd->recv.buf;
                l_fds_ifd->recv.cur_size = l_fds_ifd->recv.max_size;
                l_fds_ifd->recv.cur_offset = 0;
            }
            return VMA_PACKET_DROP;
        }

        g_receiveCount++;

        if (msgReply->getHeader()->isPongRequest()) {
            /* if server in a no reply mode - shift to start of cycle buffer*/
            if (g_pApp->m_const_params.b_server_dont_reply) {
                l_fds_ifd->recv.cur_addr = l_fds_ifd->recv.buf;
                l_fds_ifd->recv.cur_size = l_fds_ifd->recv.max_size;
                l_fds_ifd->recv.cur_offset = 0;
                return VMA_PACKET_DROP;
            }
            /* prepare message header */
            if (g_pApp->m_const_params.mode != MODE_BRIDGE) {
                msgReply->setServer();
            }
            /* get source addr to reply. memcpy is not used to improve performance */
            sendto_addr = l_fds_ifd->server_addr;

            if (l_fds_ifd->memberships_size || !l_fds_ifd->is_multicast ||
                g_pApp->m_const_params.b_server_reply_via_uc) { // In unicast case reply to sender
                /* get source addr to reply. memcpy is not used to improve performance */
                sendto_addr = *vma_info->src;
            } else if (l_fds_ifd->is_multicast) {
                /* always send to the same port recved from */
                sendto_addr.sin_port = vma_info->src->sin_port;
            }
            int length = msgReply->getLength();
            msgReply->setHeaderToNetwork();
            msg_sendto(fd, msgReply->getBuf(), length, &sendto_addr);
            /*if (ret == RET_SOCKET_SHUTDOWN) {
                if (l_fds_ifd->sock_type == SOCK_STREAM) {
                    close_ifd( l_fds_ifd->next_fd,ifd,l_fds_ifd);
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
    }
    /* 6: shift to start of cycle buffer in case receiving buffer is empty and
     * there is no uncompleted message
     */
    l_fds_ifd->recv.cur_addr = l_fds_ifd->recv.buf;
    l_fds_ifd->recv.cur_size = l_fds_ifd->recv.max_size;
    l_fds_ifd->recv.cur_offset = 0;

    return VMA_PACKET_DROP;
}
#endif

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

int sock_set_multicast(int fd, struct fds_data *p_data) {
    int rc = SOCKPERF_ERR_NONE;
    struct sockaddr_in *p_addr = NULL;
    p_addr = &(p_data->server_addr);

    /* use setsockopt() to request that the kernel join a multicast group */
    /* and specify a specific interface address on which to receive the packets of this socket */
    /* and may specify message source IP address on which to receive from */
    /* NOTE: we don't do this if case of client (sender) in stream mode */
    if (!s_user_params.b_stream || s_user_params.mode != MODE_CLIENT) {
        if (p_data->mc_source_ip_addr.s_addr != INADDR_ANY) {
            struct ip_mreq_source mreq_src;
            memset(&mreq_src, 0, sizeof(struct ip_mreq_source));
            mreq_src.imr_multiaddr = p_addr->sin_addr;
            mreq_src.imr_interface.s_addr = s_user_params.rx_mc_if_addr.s_addr;
            mreq_src.imr_sourceaddr.s_addr = p_data->mc_source_ip_addr.s_addr;
            if (setsockopt(fd, IPPROTO_IP, IP_ADD_SOURCE_MEMBERSHIP, &mreq_src, sizeof(mreq_src)) <
                0) {
                if (errno == ENOBUFS) {
                    log_err("setsockopt(IP_ADD_SOURCE_MEMBERSHIP) - Maximum multicast source "
                            "addresses that can be filtered is limited by "
                            "/proc/sys/net/ipv4/igmp_max_msf");
                } else {
                    log_err("setsockopt(IP_ADD_SOURCE_MEMBERSHIP)");
                }
                rc = SOCKPERF_ERR_SOCKET;
            }
        } else {
            struct ip_mreq mreq;
            memset(&mreq, 0, sizeof(struct ip_mreq));
            mreq.imr_multiaddr = p_addr->sin_addr;
            mreq.imr_interface.s_addr = s_user_params.rx_mc_if_addr.s_addr;
            if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
                if (errno == ENOBUFS) {
                    log_err("setsockopt(IP_ADD_MEMBERSHIP) - Maximum multicast addresses that can "
                            "join same group is limited by "
                            "/proc/sys/net/ipv4/igmp_max_memberships");
                } else {
                    log_err("setsockopt(IP_ADD_MEMBERSHIP)");
                }
                rc = SOCKPERF_ERR_SOCKET;
            }
        }
    }

    /* specify a specific interface address on which to transmitted the multicast packets of this
     * socket */
    if (!rc && (s_user_params.tx_mc_if_addr.s_addr != INADDR_ANY)) {
        if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &s_user_params.tx_mc_if_addr,
                       sizeof(s_user_params.tx_mc_if_addr)) < 0) {
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

    if (!rc && ((p_data->is_multicast) || (s_user_params.uc_reuseaddr)) &&
        ((s_user_params.mode == MODE_SERVER && p_data->server_addr.sin_port) ||
         (s_user_params.mode == MODE_CLIENT && s_user_params.client_bind_info.sin_port))) {
        /* allow multiple sockets to use the same PORT (SO_REUSEADDR) number
         * only if it is a well know L4 port only for MC or if uc_reuseaddr parameter was set.
         */
        rc = sock_set_reuseaddr(fd);
    }

    if (!rc && (s_user_params.lls_is_set == true)) {
        rc = sock_set_lls(fd);
    }

    if (!rc && (s_user_params.sock_buff_size > 0)) {
        rc = sock_set_snd_rcv_bufs(fd);
    }

    if (!rc && (p_data->is_multicast)) {
        rc = sock_set_multicast(fd, p_data);
    }

    if (!rc && (p_data->sock_type == SOCK_STREAM)) {
        rc = sock_set_tcp_nodelay(fd);
    }

    if (!rc && (s_user_params.tos)) {
        rc = sock_set_tos(fd);
    }

#ifdef USING_VMA_EXTRA_API
#ifdef ST_TEST
    if (!stTest)
#endif
        if (!rc && (s_user_params.is_vmarxfiltercb && g_vma_api)) {
            // Try to register application with VMA's special receive notification callback logic
            if (g_vma_api->register_recv_callback(fd, myapp_vma_recv_pkt_filter_callback, NULL) <
                0) {
                log_err("vma_api->register_recv_callback failed. Try running without option "
                        "'vmarxfiltercb'");
            } else {
                log_dbg("vma_api->register_recv_callback successful registered");
            }
        } else if (!rc && (s_user_params.is_vmazcopyread && g_vma_api)) {
            g_zeroCopyData[fd] = new ZeroCopyData();
            g_zeroCopyData[fd]->allocate();
        }
#endif

    return (!rc ? fd
                : (int)INVALID_SOCKET); // TODO: use SOCKET all over the way and avoid this cast
}

//------------------------------------------------------------------------------
/* get IP:port pairs from the file and initialize the list */
static int set_sockets_from_feedfile(const char *feedfile_name) {
    int rc = SOCKPERF_ERR_NONE;
    FILE *file_fd = NULL;
    char line[MAX_MCFILE_LINE_LENGTH];
    char *res;
    int sock_type = SOCK_DGRAM;
    char *ip = NULL;
    char *port = NULL;
    char *mc_src_ip = NULL;
    fds_data *tmp;
    int curr_fd = 0, last_fd = 0;
#ifndef WIN32
    int regexpres;
    regex_t regexpr;
#endif

    struct stat st_buf;
    const int status = stat(feedfile_name, &st_buf);
    // Get the status of the file system object.
    if (status != 0) {
        log_msg("Can't open file: %s\n", feedfile_name);
        return SOCKPERF_ERR_NOT_EXIST;
    }
#ifndef WIN32
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
#ifndef __FreeBSD__
    std::tr1::unordered_map<port_and_type, int> fd_socket_map; //<port,fd>
#else
    std::unordered_map<port_and_type, int> fd_socket_map; //<port,fd>
#endif

    while (!rc && (res = fgets(line, MAX_MCFILE_LINE_LENGTH, file_fd))) {
        /* skip empty lines and comments */
        if (line[0] == ' ' || line[0] == '\r' || line[0] == '\n' || line[0] == '#') {
            continue;
        }
#ifndef WIN32
        regexpres = regcomp(&regexpr, IP_PORT_FORMAT_REG_EXP, REG_EXTENDED | REG_NOSUB);
        if (regexpres) {
            log_msg("Failed to compile regexp");
            rc = SOCKPERF_ERR_FATAL;
            break;
        } else {
            regexpres = regexec(&regexpr, line, (size_t)0, NULL, 0);
            regfree(&regexpr);
            if (regexpres) {
                log_msg("Invalid input in line %s: "
                        "each line must have the following format: ip:port or type:ip:port or "
                        "type:ip:port:mc_src_ip",
                        line);
                rc = SOCKPERF_ERR_INCORRECT;
                break;
            }
        }
#endif

        /* this code support backward compatibility with old format of file */
        if (line[0] == 'U' || line[0] == 'u' || line[0] == 'T' || line[0] == 't') {
            sock_type = (line[0] == 'T' || line[0] == 't' ? SOCK_STREAM : SOCK_DGRAM);
            strtok(line, ":");
            ip = strtok(NULL, ":");
        } else {
            sock_type = SOCK_DGRAM;
            ip = strtok(line, ":");
        }
        if (sock_type == SOCK_DGRAM && s_user_params.mode == MODE_CLIENT) {
            if (s_user_params.fd_handler_type == SOCKETXTREME &&
                s_user_params.client_bind_info.sin_addr.s_addr == INADDR_ANY) {
                log_msg("socketxtreme requires forcing the client side to bind to a specific ip "
                        "address (use --client_ip) option");
                rc = SOCKPERF_ERR_INCORRECT;
                break;
            }
        }
        port = strtok(NULL, ":\n");
        mc_src_ip = strtok(NULL, ":\n");
        if (!ip || !port) {
            log_msg("Invalid input in line %s: "
                    "each line must have the following format: ip:port or type:ip:port or "
                    "type:ip:port:mc_src_ip",
                    line);
            rc = SOCKPERF_ERR_INCORRECT;
            break;
        }

        tmp = (struct fds_data *)MALLOC(sizeof(struct fds_data));
        if (!tmp) {
            log_err("Failed to allocate memory with malloc()");
            rc = SOCKPERF_ERR_NO_MEMORY;
        } else {
            memset(tmp, 0, sizeof(struct fds_data));
            tmp->server_addr.sin_family = AF_INET;
            tmp->server_addr.sin_port = htons(atoi(port));
            tmp->mc_source_ip_addr.s_addr = s_user_params.mc_source_ip_addr.s_addr;
            if (!inet_aton(ip, &tmp->server_addr.sin_addr)) {
                struct hostent *hostip = gethostbyname(ip);
                if (hostip) {
                    memcpy(&(tmp->server_addr.sin_addr), hostip->h_addr_list[0], hostip->h_length);
                } else {
                    log_msg("Invalid address in line %s: '%s:%s'", line, ip, port);
                    FREE(tmp);
                    rc = SOCKPERF_ERR_INCORRECT;
                    break;
                }
            }
            if ((mc_src_ip) && (!inet_aton(mc_src_ip, &(tmp->mc_source_ip_addr)))) {
                struct hostent *hostip = gethostbyname(mc_src_ip);
                if (hostip) {
                    memcpy(&(tmp->mc_source_ip_addr), hostip->h_addr_list[0], hostip->h_length);
                } else {
                    log_msg("Invalid multicast source address in line %s: '%s'", line, mc_src_ip);
                    FREE(tmp);
                    rc = SOCKPERF_ERR_INCORRECT;
                    break;
                }
            }
            tmp->is_multicast = IN_MULTICAST(ntohl(tmp->server_addr.sin_addr.s_addr));
            tmp->sock_type = sock_type;

            /* Check if the same value exists */
            bool is_exist = false;
            port_and_type port_type_tmp = { tmp->sock_type, tmp->server_addr.sin_port };
            for (int i = s_fd_min; i <= s_fd_max; i++) {
                /* duplicated values are accepted in case client connection using TCP */
                /* or in case source address is set for multicast socket */
                if (((s_user_params.mode == MODE_CLIENT) && (tmp->sock_type == SOCK_STREAM)) ||
                    ((tmp->is_multicast) && (tmp->mc_source_ip_addr.s_addr != INADDR_ANY))) {
                    continue;
                }

                if (g_fds_array[i] && !memcmp(&(g_fds_array[i]->server_addr), &(tmp->server_addr),
                                              sizeof(tmp->server_addr)) &&
                    fd_socket_map[port_type_tmp]) {
                    is_exist = true;
                    break;
                }
            }

            if (is_exist) {
                if (tmp->recv.buf) {
                    FREE(tmp->recv.buf);
                }
                FREE(tmp);
                continue;
            }

            tmp->active_fd_count = 0;
            tmp->active_fd_list = (int *)MALLOC(MAX_ACTIVE_FD_NUM * sizeof(int));
            bool new_socket_flag = true;
            if (!tmp->active_fd_list) {
                log_err("Failed to allocate memory with malloc()");
                rc = SOCKPERF_ERR_NO_MEMORY;
            } else {
                /* if this port already been received before, join socket - multicast only */
                if ((0 != fd_socket_map[port_type_tmp]) && (tmp->is_multicast)) {
                    /* join socket */
                    curr_fd = fd_socket_map[port_type_tmp];
                    new_socket_flag = false;
                    if (g_fds_array[curr_fd]->memberships_addr == NULL) {
                        g_fds_array[curr_fd]->memberships_addr = (struct sockaddr_in *)MALLOC(
                            IGMP_MAX_MEMBERSHIPS * sizeof(struct sockaddr_in));
                    }
                    g_fds_array[curr_fd]->memberships_addr[g_fds_array[curr_fd]->memberships_size] =
                        tmp->server_addr;
                    g_fds_array[curr_fd]->memberships_size++;
                } else {
                    /* create a socket */
                    if ((curr_fd = (int)socket(AF_INET, tmp->sock_type, 0)) <
                        0) { // TODO: use SOCKET all over the way and avoid this cast
                        log_err("socket(AF_INET, SOCK_x)");
                        rc = SOCKPERF_ERR_SOCKET;
                    }
                    fd_socket_map[port_type_tmp] = curr_fd;
                    if (tmp->is_multicast) {
                        tmp->memberships_addr = (struct sockaddr_in *)MALLOC(
                            IGMP_MAX_MEMBERSHIPS * sizeof(struct sockaddr_in));
                    } else {
                        tmp->memberships_addr = NULL;
                    }
                    tmp->memberships_size = 0;

                    s_fd_num++;
                }
                if (curr_fd >= 0) {
                    if ((curr_fd >= MAX_FDS_NUM) ||
                        (prepare_socket(curr_fd, tmp) == (int)
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
                                g_fds_array[curr_fd] = tmp;
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
                FREE(tmp);
            }
        }
    }

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
            char *ip = "224.3.2.1";
            char *port = "11111";
            static fds_data data1, data2;

            tmp = &data1;
            memset(tmp, 0, sizeof(struct fds_data));
            tmp->server_addr.sin_family = AF_INET;
            tmp->server_addr.sin_port = htons(atoi(port));
            if (!inet_aton(ip, &tmp->server_addr.sin_addr)) {
                log_msg("Invalid input: '%s:%s'", ip, port);
                exit_with_log(SOCKPERF_ERR_INCORRECT);
            }
            tmp->sock_type = sock_type;
            tmp->is_multicast = IN_MULTICAST(ntohl(tmp->server_addr.sin_addr.s_addr));
            st1 = prepare_socket(tmp, true);

            tmp = &data2;
            memset(tmp, 0, sizeof(struct fds_data));
            tmp->server_addr.sin_family = AF_INET;
            tmp->server_addr.sin_port = htons(atoi(port));
            if (!inet_aton(ip, &tmp->server_addr.sin_addr)) {
                log_msg("Invalid input: '%s:%s'", ip, port);
                exit_with_log(SOCKPERF_ERR_INCORRECT);
            }
            tmp->sock_type = sock_type;
            tmp->is_multicast = IN_MULTICAST(ntohl(tmp->server_addr.sin_addr.s_addr));
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

    if (os_sock_startup() == false) { // Only relevant for Windows
        log_err("Failed to initialize WSA");
        rc = SOCKPERF_ERR_FATAL;
    } else {
        sock_lib_started = 1;
    }

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

#ifdef USING_VMA_EXTRA_API
    if (!rc && (s_user_params.is_vmarxfiltercb || s_user_params.is_vmazcopyread ||
                s_user_params.fd_handler_type == SOCKETXTREME)) {
        // Get VMA extended API
        g_vma_api = vma_get_api();
        if (g_vma_api == NULL) {
            errno = EPERM;
            exit_with_err("VMA Extra API is not available", SOCKPERF_ERR_FATAL);
        } else {
            log_msg("VMA Extra API is in use");
        }

        _vma_pkts_desc_size =
            sizeof(struct vma_packets_t) + sizeof(struct vma_packet_t) + sizeof(struct iovec) * 16;
    }
#else
    if (!rc && (s_user_params.is_vmarxfiltercb || s_user_params.is_vmazcopyread ||
                s_user_params.fd_handler_type == SOCKETXTREME)) {
        errno = EPERM;
        exit_with_err("Please compile with VMA Extra API to use these options", SOCKPERF_ERR_FATAL);
    }
#endif

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
            fds_data *tmp = (struct fds_data *)MALLOC(sizeof(struct fds_data));
            if (!tmp) {
                log_err("Failed to allocate memory with malloc()");
                rc = SOCKPERF_ERR_NO_MEMORY;
            } else {
                memset(tmp, 0, sizeof(struct fds_data));
                memcpy(&tmp->server_addr, &(s_user_params.addr), sizeof(struct sockaddr_in));
                tmp->mc_source_ip_addr.s_addr = s_user_params.mc_source_ip_addr.s_addr;
                tmp->is_multicast = IN_MULTICAST(ntohl(tmp->server_addr.sin_addr.s_addr));
                tmp->sock_type = s_user_params.sock_type;

                tmp->active_fd_count = 0;
                tmp->active_fd_list = (int *)MALLOC(MAX_ACTIVE_FD_NUM * sizeof(int));
                if (!tmp->active_fd_list) {
                    log_err("Failed to allocate memory with malloc()");
                    rc = SOCKPERF_ERR_NO_MEMORY;
                } else {
                    /* create a socket */
                    if ((curr_fd = (int)socket(AF_INET, tmp->sock_type, 0)) <
                        0) { // TODO: use SOCKET all over the way and avoid this cast
                        log_err("socket(AF_INET, SOCK_x)");
                        rc = SOCKPERF_ERR_SOCKET;
                    } else {
                        if ((curr_fd >= MAX_FDS_NUM) ||
                            (prepare_socket(curr_fd, tmp) ==
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
                                g_fds_array[s_fd_min] = tmp;
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
                    FREE(tmp);
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

        s_user_params.cycleDuration = TicksDuration(cycleDurationNsec);

        if (s_user_params.dummy_mps) { // Calculate dummy send rate
            int64_t dummySendCycleDurationNsec =
                (s_user_params.dummy_mps == UINT32_MAX) ? 0 : NSEC_IN_SEC / s_user_params.dummy_mps;
            s_user_params.dummySendCycleDuration = TicksDuration(dummySendCycleDurationNsec);
        }

        s_user_params.cooldown_msec = TEST_END_COOLDOWN_MSEC;
        s_user_params.warmup_msec = TEST_FIRST_CONNECTION_FIRST_PACKET_TTL_THRESHOLD_MSEC +
                                    s_fd_num * TEST_ANY_CONNECTION_FIRST_PACKET_TTL_THRESHOLD_MSEC;
        if (s_user_params.warmup_msec < TEST_START_WARMUP_MSEC) {
            s_user_params.warmup_msec = TEST_START_WARMUP_MSEC;
        } else {
            log_dbg(
                "Warm-up set in relation to number of active connections. Warm up time: %" PRIu32
                " usec; first connection's first packet TTL: %d usec; following connections' first "
                "packet TTL: %d usec\n",
                g_pApp->m_const_params.warmup_msec,
                TEST_FIRST_CONNECTION_FIRST_PACKET_TTL_THRESHOLD_MSEC * 1000,
                (int)(TEST_ANY_CONNECTION_FIRST_PACKET_TTL_THRESHOLD_MSEC * 1000));
        }

        uint64_t _maxTestDuration = 1 + s_user_params.sec_test_duration +
                                    (s_user_params.warmup_msec + s_user_params.cooldown_msec) /
                                        1000; // + 1sec for timer inaccuracy safety
        uint64_t _maxSequenceNo = _maxTestDuration * s_user_params.mps +
                                  10 * s_user_params.reply_every; // + 10 replies for safety
        _maxSequenceNo += s_user_params.burst_size; // needed for the case burst_size > mps

        if (s_user_params.pPlaybackVector) {
            _maxSequenceNo = s_user_params.pPlaybackVector->size();
        }

        /* SERVER does not have info about max number of expected packets */
        if (s_user_params.mode == MODE_SERVER) {
            _maxSequenceNo = UINT64_MAX;
        }

        Message::initMaxSeqNo(_maxSequenceNo);

        if (!s_user_params.b_stream && (!s_user_params.mode) == MODE_SERVER) {
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

        log_dbg(
            "+INFO:\n\t\
mode = %d \n\t\
with_sock_accl = %d \n\t\
msg_size = %d \n\t\
msg_size_range = %d \n\t\
sec_test_duration = %d \n\t\
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
is_vmarxfiltercb = %d \n\t\
is_vmazcopyread = %d \n\t\
mc_loop_disable = %d \n\t\
mc_ttl = %d \n\t\
uc_reuseaddr = %d \n\t\
tcp_nodelay = %d \n\t\
client_work_with_srv_num = %d \n\t\
b_server_reply_via_uc = %d \n\t\
b_server_dont_reply = %d \n\t\
b_server_detect_gaps = %d\n\t\
mps = %d \n\t\
client_bind_info = %s:%d \n\t\
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
            s_user_params.mode, s_user_params.withsock_accl, s_user_params.msg_size,
            s_user_params.msg_size_range, s_user_params.sec_test_duration,
            s_user_params.data_integrity, s_user_params.packetrate_stats_print_ratio,
            s_user_params.burst_size, s_user_params.packetrate_stats_print_details,
            s_user_params.fd_handler_type, s_user_params.mthread_server,
            s_user_params.sock_buff_size, s_user_params.threads_num, s_user_params.threads_affinity,
            s_user_params.is_blocked, s_user_params.is_nonblocked_send, s_user_params.do_warmup,
            s_user_params.pre_warmup_wait, s_user_params.is_vmarxfiltercb,
            s_user_params.is_vmazcopyread, s_user_params.mc_loop_disable, s_user_params.mc_ttl,
            s_user_params.uc_reuseaddr, s_user_params.tcp_nodelay,
            s_user_params.client_work_with_srv_num, s_user_params.b_server_reply_via_uc,
            s_user_params.b_server_dont_reply, s_user_params.b_server_detect_gaps,
            s_user_params.mps, inet_ntoa(s_user_params.client_bind_info.sin_addr),
            ntohs(s_user_params.client_bind_info.sin_port), s_user_params.reply_every,
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
        for (int i = 0; i < SIZE; i++)
            end.setNow();
        log_dbg("+INFO: taking time, using the given settings, consumes %.3lf nsec",
                (double)(end - start).toNsec() / SIZE);

#if !defined(__arm__) && !defined(__aarch64__)
        ticks_t tstart = 0, tend = 0;
        tstart = os_gettimeoftsc();

        for (int i = 0; i < SIZE; i++)
            tend = os_gettimeoftsc();
        double tdelta = (double)tend - (double)tstart;
        double ticks_per_second = (double)get_tsc_rate_per_second();
        log_dbg("+INFO: taking rdtsc directly consumes %.3lf nsec",
                tdelta / SIZE * 1000 * 1000 * 1000 / ticks_per_second);
#endif

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
