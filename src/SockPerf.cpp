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

/*
 * How to Build: 'g++ *.cpp -O3 --param inline-unit-growth=200 -lrt -lpthread -ldl -o sockperf'
 */

/*
 * Still to do:
 *
 * V 2. stream mode should respect --pps
 * *  playback file
 * * * finalize playback (possible conflicts with other cmd-args, including disclaimer, print info before looping)
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
 * 11 sockperf should work in exclusive modes (like svn): relevant modes are: latency-under-load, stream, ping-pong, playback, subscriber (aka server)
 *
 *
 * 7. support OOO check for select/poll/epoll (currently check will fail unless recvfrom is used)
 * 8. configure and better handling of warmup/cooldown (flag on warmup packets)
 * 9. merge with perf envelope
 * 10. improve format of statistics at the end (tx/rx statistics line excluding warmup)
 * V 11. don't consider dropped packets at the end
 * X 12. consider: automatically set --pps=max in case of --ping-pong
 * 13. consider: loop till desirable seqNo instead of using timer (will distinguish between test and cool down)
 * 14. remove error printouts about dup packets, or illegal seqno (and recheck illegal seqno with pps=100 when server listen on same MC with 120 sockets)
 * 15. in case server is registered twice to same MC group+port, we consider the 2 replies as dup packet.  is this correct?
 * 16. handshake between C/S at the beginning of run will allow the client to configure N servers (like perf envelope) and more
 * 17. support sockperf version with -v
 * 18. consider preventing --range when --stream is used (for best statistcs)
 * 19. warmup also for uc (mainly server side)
 * 20. less permuatation in templates, for example receiver thread can be different entity than sender thread
 * 21. use ptpd for latency test using synced clocks across machines (without RTT)
 * 22. use --pps=max as the default for ping-pong and throughput modes
 *
 */


#define __STDC_LIMIT_MACROS // for UINT32_MAX in C++
#include <stdint.h>
#include "common.h"
#include "Message.h"
#include "PacketTimes.h"
#include "Switches.h"
#include "aopt.h"

// forward declarations from Client.cpp & Server.cpp
void client_sig_handler(int signum);
void client_handler(int _fd_min, int _fd_max, int _fd_num);
void server_sig_handler(int signum);
void server_select_per_thread();
void server_handler(int fd_min, int fd_max, int fd_num);

static int s_fd_max = 0;
static int s_fd_min = 0;	/* used as THE fd when single mc group is given (RECVFROM blocked mode) */
static int s_fd_num = 0;
static struct user_params_t    s_user_params;
static struct mutable_params_t s_mutable_params;

static void set_select_timeout(int time_out_msec);
static int set_mcgroups_fromfile(const char *mcg_filename);
#ifdef ST_TEST
static int prepare_socket(struct sockaddr_in* p_addr, bool stTest = false);
#else
static int prepare_socket(struct sockaddr_in* p_addr);
#endif

#ifndef VERSION
#define VERSION	unknown
#endif

static int proc_mode_help( int, int, const char ** );
static int proc_mode_version( int, int, const char ** );
static int proc_mode_under_load( int, int, const char ** );
static int proc_mode_ping_pong( int, int, const char ** );
static int proc_mode_throughput( int, int, const char ** );
static int proc_mode_playback( int, int, const char ** );
static int proc_mode_server( int, int, const char ** );

static const struct app_modes
{
   int (*func)(int, int, const char **); 	/* proc function */
   const char* name;   					/* mode name to use from command line as an argument */
   const char* const shorts[4];   		/* short name */
   const char* note;                    /* mode description */
} sockperf_modes[] =
{
   { proc_mode_help,  		"help",			aopt_set_string( "h", "?" ), 	"Display list of supported commands."},
   { proc_mode_version,		"--version",	aopt_set_string( NULL ),		"Tool version information."},
   { proc_mode_under_load, 	"under-load",	aopt_set_string( "ul" ),   		"Run " MODULE_NAME " client for latency under load test."},
   { proc_mode_ping_pong, 	"ping-pong",	aopt_set_string( "pp" ),   		"Run " MODULE_NAME " client for latency test in ping pong mode."},
   { proc_mode_playback, 	"playback",		aopt_set_string( "pb" ),   		"Run " MODULE_NAME " client for latency test using playback of predefined traffic, based on timeline and message size."},
   { proc_mode_throughput,	"throughput",	aopt_set_string( "tp" ),   		"Run " MODULE_NAME " client for one way throughput test."},
   { proc_mode_server, 		"server",   	aopt_set_string( "sr" ),		"Run " MODULE_NAME " as a server."},
   { NULL,   NULL,	aopt_set_string( NULL ),   NULL }
};

static int parse_common_opt( const AOPT_OBJECT * );
static int parse_client_opt( const AOPT_OBJECT * );
static char* display_opt( int, char *, size_t );

/*
 * List of supported general options.
 */
static const AOPT_DESC  common_opt_desc[] =
{
	{
		'h', AOPT_NOARG,	aopt_set_literal( 'h', '?' ),	aopt_set_string( "help", "usage" ),
             "Show the help message and exit."
	},
	{
		'd', AOPT_NOARG, aopt_set_literal( 'd' ),	aopt_set_string( "debug" ),
             "Print extra debug information."
	},
	{
		'i', AOPT_ARG, aopt_set_literal( 'i' ), aopt_set_string( "ip" ),
             "Listen on/send to ip <ip>."
	},
	{
		'p', AOPT_ARG, aopt_set_literal( 'p' ), aopt_set_string( "port" ),
             "Listen on/connect to port <port> (default 11111)."
	},
	{
		'm', AOPT_ARG, aopt_set_literal( 'm' ), aopt_set_string( "msg-size" ),
             "Use messages of size <size> bytes (minimum default 12)."
	},
	{
		'f', AOPT_ARG, aopt_set_literal( 'f' ), aopt_set_string( "file" ),
             "Tread multiple ip+port combinations from file <file> (server uses select)."
	},
	{
		'F', AOPT_ARG, aopt_set_literal( 'F' ), aopt_set_string( "io-hanlder-type" ),
             "Type of multiple file descriptors handle [s|select|p|poll|e|epoll](default select)."
	},
	{
		'a', AOPT_ARG, aopt_set_literal( 'a' ), aopt_set_string( "activity" ),
             "Measure activity by printing a '.' for the last <N> packets processed."
	},
	{
		'A', AOPT_ARG, aopt_set_literal( 'A' ), aopt_set_string( "Activity" ),
             "Measure activity by printing the duration for last <N> packets processed."
	},
	{
		OPT_RX_MC_IF, AOPT_ARG, aopt_set_literal( 0 ), aopt_set_string( "rx-mc-if" ),
             "<ip> address of interface on which to receive mulitcast packets (can be other then route table)."
	},
	{
		OPT_TX_MC_IF, AOPT_ARG, aopt_set_literal( 0 ), aopt_set_string( "tx-mc-if" ),
             "<ip> address of interface on which to transmit mulitcast packets (can be other then route table)."
	},
	{
		OPT_SELECT_TIMEOUT, AOPT_ARG, aopt_set_literal( 0 ), aopt_set_string( "timeout" ),
             "Set select/poll/epoll timeout to <msec>, -1 for infinite (default is 10 msec)."
	},
	{
		OPT_MC_LOOPBACK_ENABLE, AOPT_NOARG, aopt_set_literal( 0 ), aopt_set_string( "mc-loopback-enable" ),
             "Enables mc loopback (default disabled)."
	},
	{
		OPT_UDP_BUFFER_SIZE, AOPT_ARG, aopt_set_literal( 0 ), aopt_set_string( "udp-buffer-size" ),
             "Set udp buffer size to <size> bytes."
	},
	{
		OPT_VMAZCOPYREAD, AOPT_NOARG, aopt_set_literal( 0 ), aopt_set_string( "vmazcopyread" ),
             "If possible use VMA's zero copy reads API (See VMA's readme)."
	},
	{
		OPT_DAEMONIZE, AOPT_NOARG, aopt_set_literal( 0 ), aopt_set_string( "daemonize" ),
             "Run as daemon."
	},
	{
		OPT_NONBLOCKED, AOPT_NOARG, aopt_set_literal( 0 ), aopt_set_string( "nonblocked" ),
             "Open non-blocked sockets."
	},
	{
		OPT_DONTWARMUP, AOPT_NOARG, aopt_set_literal( 0 ), aopt_set_string( "dontwarmup" ),
             "Don't send warm up packets on start."
	},
	{
		OPT_PREWARMUPWAIT, AOPT_ARG, aopt_set_literal( 0 ), aopt_set_string( "pre-warmup-wait" ),
             "Time to wait before sending warm up packets (seconds)."
	},
	{
		OPT_SOCK_ACCL, AOPT_NOARG,	aopt_set_literal( 0 ),	aopt_set_string( "set-sock-accl" ),
             "set sock_accl before run (available for some of Mellanox systems)"
	},
	{ 0, AOPT_NOARG, aopt_set_literal( 0 ), aopt_set_string( NULL ), NULL }
};

/*
 * List of supported client options.
 */
static const AOPT_DESC  client_opt_desc[] =
{
	{
		't', AOPT_ARG,	aopt_set_literal( 't' ),	aopt_set_string( "time" ),
             "Run for <sec> seconds (default 1, max = 36000000)."
	},
	{
		'b', AOPT_ARG,	aopt_set_literal( 'b' ),	aopt_set_string( "burst" ),
             "Control the client's number of a packets sent in every burst."
	},
	{
		'r', AOPT_ARG,	aopt_set_literal( 'r' ),	aopt_set_string( "range" ),
             "comes with -m <size>, randomly change the messages size in range: <size> +- <N>."
	},
	{
		OPT_DATA_INTEGRITY, AOPT_NOARG,	aopt_set_literal( 0 ),	aopt_set_string( "data-integrity" ),
             "Perform data integrity test."
	},
	{
		OPT_CLIENT_WORK_WITH_SRV_NUM, AOPT_ARG,	aopt_set_literal( 0 ),	aopt_set_string( "srv-num" ),
             "Set num of servers the client works with to N."
	},
	{
		OPT_PPS, AOPT_ARG,	aopt_set_literal( 0 ),	aopt_set_string( "pps" ),
             "Set number of packets-per-second (default = 10000 - for under-load mode, or max - for ping-pong and throughput modes; for maximum use --pps=max)."
	},
	{
		OPT_SENDER_AFFINITY, AOPT_ARG,	aopt_set_literal( 0 ),	aopt_set_string( "sender-affinity" ),
             "Set sender thread affinity to the given core id (see: cat /proc/cpuinfo)."
	},
	{
		OPT_RECEIVER_AFFINITY, AOPT_ARG,	aopt_set_literal( 0 ),	aopt_set_string( "receiver-affinity" ),
             "Set receiver thread affinity to the given core id (see: cat /proc/cpuinfo)."
	},
	{
		OPT_FULL_LOG, AOPT_ARG,	aopt_set_literal( 0 ),	aopt_set_string( "full-log" ),
             "Dump full log of all packets send/receive time to the given file in CSV format."
	},
	{
		OPT_NO_RDTSC, AOPT_NOARG,	aopt_set_literal( 0 ),	aopt_set_string( "no-rdtsc" ),
             "Don't use register when taking time; instead use monotonic clock."
	},
	{
		OPT_LOAD_VMA, AOPT_NOARG,	aopt_set_literal( 0 ),	aopt_set_string( "load-vma" ),
             "Load VMA dynamically even when LD_PRELOAD was not used."
	},
	{ 0, AOPT_NOARG, aopt_set_literal( 0 ), aopt_set_string( NULL ), NULL }
};


//------------------------------------------------------------------------------
static int proc_mode_help( int id, int argc, const char **argv )
{
	int   i = 0;

	printf(MODULE_NAME " is a tool for testing network latency and throughput.\n");
	printf("version %s\n", STR(VERSION));
	printf("\n");
	printf("Usage: " MODULE_NAME " <subcommand> [options] [args]\n");
	printf("Type: \'" MODULE_NAME " <subcommand> --help\' for help on a specific subcommand.\n");
	printf("Type: \'" MODULE_NAME " --version\' to see the program version number.\n");
	printf("\n");
	printf("Available subcommands:\n");

	for ( i = 0; sockperf_modes[i].name != NULL; i++ ) {
		char tmp_buf[21];

		/* skip commands with prefix '--', they are special */
		if (sockperf_modes[i].name[0] != '-') {
			printf("   %-20.20s\t%-s\n", display_opt(i, tmp_buf, sizeof(tmp_buf)), sockperf_modes[i].note);
		}
	}

	printf("\n");
	printf("For additional information see README file, or Type \'sockperf <subcommand> --help\'.\n");

	return -1;	/* this return code signals to do exit */
}

//------------------------------------------------------------------------------
static int proc_mode_version( int id, int argc, const char **argv )
{
	printf(MODULE_NAME ", version %s\n", STR(VERSION));
	printf("   compiled %s, %s\n", __DATE__, __TIME__);
	printf("\n%s\n", MODULE_COPYRIGHT);

	return -1;	/* this return code signals to do exit */
}

//------------------------------------------------------------------------------
static int proc_mode_under_load( int id, int argc, const char **argv )
{
	int rc = 0;
	const AOPT_OBJECT *common_obj = NULL;
	const AOPT_OBJECT *client_obj = NULL;
	const AOPT_OBJECT *self_obj = NULL;

	/*
	 * List of supported under-load options.
	 */
	const AOPT_DESC  self_opt_desc[] =
	{
		{
			OPT_REPLY_EVERY, AOPT_ARG,	aopt_set_literal( 0 ),	aopt_set_string( "reply-every" ),
	             "Set number of send packets between reply packets (default = 100)."
		},
		{ 0, AOPT_NOARG, aopt_set_literal( 0 ), aopt_set_string( NULL ), NULL }
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
			rc = 1;
		}
	}

	if (rc || aopt_check(common_obj, 'h')) {
		rc = -1;
	}

	/* Set default values */
	s_user_params.mode = MODE_CLIENT;
	s_user_params.pps = PPS_DEFAULT;
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
		if ( !rc && aopt_check(client_obj, OPT_REPLY_EVERY) ) {
			const char* optarg = aopt_value(client_obj, OPT_REPLY_EVERY);
			if (optarg) {
				errno = 0;
				long long temp = strtol(optarg, NULL, 0);
				if (errno != 0  || temp <= 0 || temp > 1<<30 ) {
					log_msg("Invalid %d val: %s", OPT_REPLY_EVERY, optarg);
					rc = 1;
				}
				else {
					s_user_params.reply_every = (uint32_t)temp;
				}
			}
			else {
				log_msg("'-%d' Invalid value", OPT_REPLY_EVERY);
				rc = 1;
			}
		}
	}

	if (rc) {
	    const char* help_str = NULL;
	    char temp_buf[30];

		printf("%s: %s\n", display_opt(id, temp_buf, sizeof(temp_buf)), sockperf_modes[id].note);
		printf("\n");
		printf("Usage: " MODULE_NAME " %s [options] [args]...\n", sockperf_modes[id].name);
		printf(" " MODULE_NAME " %s -i ip  [-p port] [-m message_size] [-t time] [--data_integrity]\n", sockperf_modes[id].name);
		printf(" " MODULE_NAME " %s -f file [-F s/p/e] [-m message_size] [-r msg_size_range] [-t time]\n", sockperf_modes[id].name);
		printf("\n");
		printf("Options:\n");
		help_str = aopt_help(common_opt_desc);
	    if (help_str)
	    {
	        printf("%s\n", help_str);
	        free((void*)help_str);
	    }
		printf("Valid arguments:\n");
		help_str = aopt_help(client_opt_desc);
	    if (help_str)
	    {
	        printf("%s", help_str);
	        free((void*)help_str);
			help_str = aopt_help(self_opt_desc);
		    if (help_str)
		    {
		        printf("%s", help_str);
		        free((void*)help_str);
		    }
		    printf("\n");
	    }
	}

	/* Destroy option objects */
	aopt_exit((AOPT_OBJECT*)common_obj);
	aopt_exit((AOPT_OBJECT*)client_obj);
	aopt_exit((AOPT_OBJECT*)self_obj);

	return rc;
}

//------------------------------------------------------------------------------
static int proc_mode_ping_pong( int id, int argc, const char **argv )
{
	int rc = 0;
	const AOPT_OBJECT *common_obj = NULL;
	const AOPT_OBJECT *client_obj = NULL;

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
		if (valid_argc < (argc - 1)) {
			rc = 1;
		}
	}

	if (rc || aopt_check(common_obj, 'h')) {
		rc = -1;
	}

	/* Set default values */
	s_user_params.mode = MODE_CLIENT;
	s_user_params.b_client_ping_pong = true;
	s_user_params.pps = UINT32_MAX;
	s_user_params.reply_every = 1;

	/* Set command line common options */
	if (!rc && common_obj) {
		rc = parse_common_opt(common_obj);
	}

	/* Set command line specific values */
	if (!rc && client_obj) {
		rc = parse_client_opt(client_obj);
	}

	if (rc) {
	    const char* help_str = NULL;
	    char temp_buf[30];

		printf("%s: %s\n", display_opt(id, temp_buf, sizeof(temp_buf)), sockperf_modes[id].note);
		printf("\n");
		printf("Usage: " MODULE_NAME " %s [options] [args]...\n", sockperf_modes[id].name);
		printf(" " MODULE_NAME " %s -i ip  [-p port] [-m message_size] [-t time] [--data_integrity]\n", sockperf_modes[id].name);
		printf(" " MODULE_NAME " %s -f file [-F s/p/e] [-m message_size] [-r msg_size_range] [-t time]\n", sockperf_modes[id].name);
		printf("\n");
		printf("Options:\n");
		help_str = aopt_help(common_opt_desc);
	    if (help_str)
	    {
	        printf("%s\n", help_str);
	        free((void*)help_str);
	    }
		printf("Valid arguments:\n");
		help_str = aopt_help(client_opt_desc);
	    if (help_str)
	    {
	        printf("%s\n", help_str);
	        free((void*)help_str);
	    }
	}

	/* Destroy option objects */
	aopt_exit((AOPT_OBJECT*)common_obj);
	aopt_exit((AOPT_OBJECT*)client_obj);

	return rc;
}

//------------------------------------------------------------------------------
static int proc_mode_throughput( int id, int argc, const char **argv )
{
	int rc = 0;
	const AOPT_OBJECT *common_obj = NULL;
	const AOPT_OBJECT *client_obj = NULL;

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
		if (valid_argc < (argc - 1)) {
			rc = 1;
		}
	}

	if (rc || aopt_check(common_obj, 'h')) {
		rc = -1;
	}

	/* Set default values */
	s_user_params.mode = MODE_CLIENT;
	s_user_params.b_stream = true;
	s_user_params.pps = UINT32_MAX;
	s_user_params.reply_every = 1 << (8 * sizeof(s_user_params.reply_every) - 2);

	/* Set command line common options */
	if (!rc && common_obj) {
		rc = parse_common_opt(common_obj);
	}

	/* Set command line specific values */
	if (!rc && client_obj) {
		rc = parse_client_opt(client_obj);
	}

	if (rc) {
	    const char* help_str = NULL;
	    char temp_buf[30];

		printf("%s: %s\n", display_opt(id, temp_buf, sizeof(temp_buf)), sockperf_modes[id].note);
		printf("\n");
		printf("Usage: " MODULE_NAME " %s [options] [args]...\n", sockperf_modes[id].name);
		printf(" " MODULE_NAME " %s -i ip  [-p port] [-m message_size] [-t time] [--data_integrity]\n", sockperf_modes[id].name);
		printf(" " MODULE_NAME " %s -f file [-F s/p/e] [-m message_size] [-r msg_size_range] [-t time]\n", sockperf_modes[id].name);
		printf("\n");
		printf("Options:\n");
		help_str = aopt_help(common_opt_desc);
	    if (help_str)
	    {
	        printf("%s\n", help_str);
	        free((void*)help_str);
	    }
		printf("Valid arguments:\n");
		help_str = aopt_help(client_opt_desc);
	    if (help_str)
	    {
	        printf("%s\n", help_str);
	        free((void*)help_str);
	    }
	}

	/* Destroy option objects */
	aopt_exit((AOPT_OBJECT*)common_obj);
	aopt_exit((AOPT_OBJECT*)client_obj);

	return rc;
}

//------------------------------------------------------------------------------
static int proc_mode_playback( int id, int argc, const char **argv )
{
	int rc = 0;
	const AOPT_OBJECT *common_obj = NULL;
	const AOPT_OBJECT *client_obj = NULL;
	const AOPT_OBJECT *self_obj = NULL;

	/*
	 * List of supported under-load options.
	 */
	const AOPT_DESC  self_opt_desc[] =
	{
		{
			OPT_REPLY_EVERY, AOPT_ARG,	aopt_set_literal( 0 ),	aopt_set_string( "reply-every" ),
	             "Set number of send packets between reply packets (default = 100)."
		},
		{
			OPT_PLAYBACK_DATA, AOPT_ARG, aopt_set_literal( 0 ), aopt_set_string( "data-file" ),
	             "Pre-prepared CSV file with timestamps and message sizes."
		},
		{ 0, AOPT_NOARG, aopt_set_literal( 0 ), aopt_set_string( NULL ), NULL }
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
			rc = 1;
		}
	}

	if (rc || aopt_check(common_obj, 'h')) {
		rc = -1;
	}

	/* Set default values */
	s_user_params.mode = MODE_CLIENT;
	s_user_params.pps = PPS_DEFAULT;
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
		if ( !rc && aopt_check(client_obj, OPT_REPLY_EVERY) ) {
			const char* optarg = aopt_value(client_obj, OPT_REPLY_EVERY);
			if (optarg) {
				errno = 0;
				long long temp = strtol(optarg, NULL, 0);
				if (errno != 0  || temp <= 0 || temp > 1<<30 ) {
					log_msg("Invalid %d val: %s", OPT_REPLY_EVERY, optarg);
					rc = 1;
				}
				else {
					s_user_params.reply_every = (uint32_t)temp;
				}
			}
			else {
				log_msg("'-%d' Invalid value", OPT_REPLY_EVERY);
				rc = 1;
			}
		}

		if ( !rc && aopt_check(client_obj, OPT_PLAYBACK_DATA) ) {
			const char * optarg = aopt_value(client_obj, OPT_PLAYBACK_DATA);
			if (optarg) {
				static PlaybackVector pv;
				loadPlaybackData(pv, optarg);
				s_user_params.pPlaybackVector = &pv;
			}
			else {
				log_msg("'-%d' Invalid value", OPT_PLAYBACK_DATA);
				rc = 1;
			}
		}
	}

	if (rc) {
	    const char* help_str = NULL;
	    char temp_buf[30];

		printf("%s: %s\n", display_opt(id, temp_buf, sizeof(temp_buf)), sockperf_modes[id].note);
		printf("\n");
		printf("Usage: " MODULE_NAME " %s [options] [args]...\n", sockperf_modes[id].name);
		printf(" " MODULE_NAME " %s -i ip  [-p port] [-m message_size] [-t time] [--data_integrity]\n", sockperf_modes[id].name);
		printf(" " MODULE_NAME " %s -f file [-F s/p/e] [-m message_size] [-r msg_size_range] [-t time]\n", sockperf_modes[id].name);
		printf("\n");
		printf("Options:\n");
		help_str = aopt_help(common_opt_desc);
	    if (help_str)
	    {
	        printf("%s\n", help_str);
	        free((void*)help_str);
	    }
		printf("Valid arguments:\n");
		help_str = aopt_help(client_opt_desc);
	    if (help_str)
	    {
	        printf("%s", help_str);
	        free((void*)help_str);
			help_str = aopt_help(self_opt_desc);
		    if (help_str)
		    {
		        printf("%s", help_str);
		        free((void*)help_str);
		    }
		    printf("\n");
	    }
	}

	/* Destroy option objects */
	aopt_exit((AOPT_OBJECT*)common_obj);
	aopt_exit((AOPT_OBJECT*)client_obj);
	aopt_exit((AOPT_OBJECT*)self_obj);

	return rc;
}

//------------------------------------------------------------------------------
static int proc_mode_server( int id, int argc, const char **argv )
{
	int rc = 0;
	const AOPT_OBJECT *common_obj = NULL;
	const AOPT_OBJECT *server_obj = NULL;

	/*
	 * List of supported server options.
	 */
	static const AOPT_DESC  server_opt_desc[] =
	{
		{
			'B', AOPT_NOARG,	aopt_set_literal( 'B' ),	aopt_set_string( "Bridge" ),
	             "Run in Bridge mode."
		},
		{
			OPT_MULTI_THREADED_SERVER, AOPT_ARG, aopt_set_literal( 0 ), aopt_set_string( "threads-num" ),
	             "Run <N> threads on server side (requires '-f' option)."
		},
		{
			OPT_THREADS_AFFINITY, AOPT_ARG,	aopt_set_literal( 0 ),	aopt_set_string( "cpu-affinity" ),
	             "Set threads affinity to the given core ids in list format (see: cat /proc/cpuinfo)."
		},
		{
			OPT_VMARXFILTERCB, AOPT_NOARG,	aopt_set_literal( 0 ),	aopt_set_string( "vmarxfiltercb" ),
	             "If possible use VMA's receive path packet filter callback API (See VMA's readme)."
		},
		{
			OPT_FORCE_UC_REPLY, AOPT_NOARG,	aopt_set_literal( 0 ),	aopt_set_string( "force-unicast-reply" ),
	             "Force server to reply via unicast."
		},
		{
			'g', AOPT_NOARG,	aopt_set_literal( 'g' ),	aopt_set_string( "gap-detection" ),
	             "Enable gap-detection."
		},
		{ 0, AOPT_NOARG, aopt_set_literal( 0 ), aopt_set_string( NULL ), NULL }
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
			rc = 1;
		}
	}

	if (rc || aopt_check(common_obj, 'h')) {
		rc = -1;
	}

	/* Set default values */
	s_user_params.mode = MODE_SERVER;
	s_user_params.pps = PPS_DEFAULT;
	/* set message size by default; it can be redefined by option --msg_size or -m */
	g_msg_size = MAX_PAYLOAD_SIZE;

	/* Set command line common options */
	if (!rc && common_obj) {
		rc = parse_common_opt(common_obj);
	}

	/* Set command line specific values */
	if (!rc && server_obj) {
		struct sockaddr_in *p_addr = &s_user_params.addr;

		if ( !rc && aopt_check(server_obj, 'B') ) {
			log_msg("update to bridge mode");
			s_user_params.mode = MODE_BRIDGE;
			g_msg_size = MAX_PAYLOAD_SIZE;
			p_addr->sin_port = htons(5001); /*iperf's default port*/
		}

		if ( !rc && aopt_check(server_obj, OPT_MULTI_THREADED_SERVER) ) {
			if (aopt_check(common_obj, 'f')) {
				const char* optarg = aopt_value(server_obj, OPT_MULTI_THREADED_SERVER);
				if (optarg) {
					s_user_params.mthread_server = 1;
					errno = 0;
					int threads_num = strtol(optarg, NULL, 0);
					if (errno != 0  || threads_num < 0) {
						log_msg("-%c' Invalid threads number: %s", OPT_MULTI_THREADED_SERVER, optarg);
						rc = 1;
					}
					else {
						s_user_params.threads_num = threads_num;
					}
				}
				else {
					log_msg("'-%d' Invalid value", OPT_MULTI_THREADED_SERVER);
					rc = 1;
				}
			}
			else {
				log_msg("--threads-num must be used with feed file (option '-f')");
				rc = 1;
			}
		}

		if ( !rc && aopt_check(server_obj, OPT_THREADS_AFFINITY) ) {
			const char* optarg = aopt_value(server_obj, OPT_THREADS_AFFINITY);
			if (optarg) {
				strcpy(s_user_params.threads_affinity, optarg);
			}
			else {
				log_msg("'-%d' Invalid threads affinity", OPT_THREADS_AFFINITY);
				rc = 1;
			}
		}

		if ( !rc && aopt_check(server_obj, OPT_VMARXFILTERCB) ) {
			s_user_params.is_vmarxfiltercb = true;
		}

		if ( !rc && aopt_check(server_obj, OPT_FORCE_UC_REPLY) ) {
			s_user_params.b_server_reply_via_uc = true;
		}

		if ( !rc && aopt_check(server_obj, 'g') ) {
			s_user_params.b_server_detect_gaps = true;
		}
	}

	if (rc) {
	    const char* help_str = NULL;
	    char temp_buf[30];

		printf("%s: %s\n", display_opt(id, temp_buf, sizeof(temp_buf)), sockperf_modes[id].note);
		printf("\n");
		printf("Usage: " MODULE_NAME " %s [options] [args]...\n", sockperf_modes[id].name);
		printf(" " MODULE_NAME " %s\n", sockperf_modes[id].name);
		printf(" " MODULE_NAME " %s [-i ip] [-p port] [-m message_size] [--rx_mc_if ip] [--tx_mc_if ip]\n", sockperf_modes[id].name);
		printf(" " MODULE_NAME " %s -f file [-F s/p/e] [-m message_size] [--rx_mc_if ip] [--tx_mc_if ip]\n", sockperf_modes[id].name);
		printf("\n");
		printf("Options:\n");
		help_str = aopt_help(common_opt_desc);
	    if (help_str)
	    {
	        printf("%s\n", help_str);
	        free((void*)help_str);
	    }
		printf("Valid arguments:\n");
		help_str = aopt_help(server_opt_desc);
	    if (help_str)
	    {
	        printf("%s\n", help_str);
	        free((void*)help_str);
	    }
	}

	/* Destroy option objects */
	aopt_exit((AOPT_OBJECT*)common_obj);
	aopt_exit((AOPT_OBJECT*)server_obj);

	return rc;
}

//------------------------------------------------------------------------------
static int parse_common_opt( const AOPT_OBJECT *common_obj )
{
	int rc = 0;

	if (common_obj) {
		struct sockaddr_in *p_addr = &s_user_params.addr;
		int *p_daemonize = &s_user_params.daemonize;
		char *mcg_filename = s_user_params.mcg_filename;

		if ( !rc && aopt_check(common_obj, 'd') ) {
			g_debug_level = LOG_LVL_DEBUG;
		}

		if ( !rc && aopt_check(common_obj, 'i') ) {
			const char* optarg = aopt_value(common_obj, 'i');
			if (!optarg || !inet_aton(optarg, &p_addr->sin_addr)) {	/* already in network byte order*/
				log_msg("'-%c' Invalid address: %s", 'i', optarg);
				rc = 1;
			}
			else {
				s_user_params.fd_handler_type = RECVFROM;
			}
		}

		if ( !rc && aopt_check(common_obj, 'p') ) {
			const char* optarg = aopt_value(common_obj, 'p');
			if (optarg) {
				errno = 0;
				long mc_dest_port = strtol(optarg, NULL, 0);
				/* strtol() returns 0 if there were no digits at all */
				if (errno != 0) {
					log_msg("'-%c' Invalid port: %d", 'p', (int)mc_dest_port);
					rc = 1;
				}
				else {
					p_addr->sin_port = htons((uint16_t)mc_dest_port);
					s_user_params.fd_handler_type = RECVFROM;
				}
			}
			else {
				log_msg("'-%c' Invalid value", 'p');
				rc = 1;
			}
		}

		if ( !rc && aopt_check(common_obj, 'm') ) {
			const char* optarg = aopt_value(common_obj, 'm');
			if (optarg) {
				g_msg_size = strtol(optarg, NULL, 0);
				if (g_msg_size < MIN_PAYLOAD_SIZE) {
					log_msg("'-%c' Invalid message size: %d (min: %d)", 'm', g_msg_size, MIN_PAYLOAD_SIZE);
					rc = 1;
				}
			}
			else {
				log_msg("'-%c' Invalid value", 'm');
				rc = 1;
			}
		}

		if ( !rc && aopt_check(common_obj, 'f') ) {
			if (!aopt_check(common_obj, 'i') && !aopt_check(common_obj, 'p')) {
				const char* optarg = aopt_value(common_obj, 'f');
				if (optarg) {
					strncpy(mcg_filename, optarg, MAX_ARGV_SIZE);
					mcg_filename[MAX_PATH_LENGTH - 1] = '\0';
					s_user_params.fd_handler_type = SELECT;
					}
				else {
					log_msg("'-%c' Invalid value", 'f');
					rc = 1;
				}
			}
			else {
				log_msg("-f conflicts with -i,-p options");
				rc = 1;
			}
		}

		if ( !rc && aopt_check(common_obj, 'F') ) {
			if (aopt_check(common_obj, 'f')) {
				const char* optarg = aopt_value(common_obj, 'F');
				if (optarg) {
					char fd_handle_type[MAX_ARGV_SIZE];

					strncpy(fd_handle_type, optarg, MAX_ARGV_SIZE);
					fd_handle_type[MAX_ARGV_SIZE - 1] = '\0';
					if (!strcmp( fd_handle_type, "epoll" ) || !strcmp( fd_handle_type, "e")) {
						s_user_params.fd_handler_type = EPOLL;
					}
					else if (!strcmp( fd_handle_type, "poll" )|| !strcmp( fd_handle_type, "p")) {
						s_user_params.fd_handler_type = POLL;
					}
					else if (!strcmp( fd_handle_type, "select" ) || !strcmp( fd_handle_type, "s")) {
						s_user_params.fd_handler_type = SELECT;
					}
					else {
						log_msg("'-%c' Invalid muliply io hanlde type: %s", 'F', optarg);
						rc = 1;
					}
				}
				else {
					log_msg("'-%c' Invalid value", 'F');
					rc = 1;
				}
			}
			else {
				log_msg("-F must be used with feed file (option '-f')");
				rc = 1;
			}
		}

		if ( !rc && aopt_check(common_obj, 'a') ) {
			const char* optarg = aopt_value(common_obj, 'a');
			if (optarg) {
				errno = 0;
				s_user_params.packetrate_stats_print_ratio = strtol(optarg, NULL, 0);
				s_user_params.packetrate_stats_print_details = false;
				if (errno != 0) {
					log_msg("'-%c' Invalid packet rate stats print value: %d", 'a', s_user_params.packetrate_stats_print_ratio);
					rc = 1;
				}
			}
			else {
				log_msg("'-%c' Invalid value", 'a');
				rc = 1;
			}
		}

		if ( !rc && aopt_check(common_obj, 'A') ) {
			const char* optarg = aopt_value(common_obj, 'A');
			if (optarg) {
				errno = 0;
				s_user_params.packetrate_stats_print_ratio = strtol(optarg, NULL, 0);
				s_user_params.packetrate_stats_print_details = true;
				if (errno != 0) {
					log_msg("'-%c' Invalid packet rate stats print value: %d", 'A', s_user_params.packetrate_stats_print_ratio);
					rc = 1;
				}
			}
			else {
				log_msg("'-%c' Invalid value", 'A');
				rc = 1;
			}
		}

		if ( !rc && aopt_check(common_obj, OPT_RX_MC_IF) ) {
			const char* optarg = aopt_value(common_obj, OPT_RX_MC_IF);
			if (!optarg ||
				((s_user_params.rx_mc_if_addr.s_addr = inet_addr(optarg)) == INADDR_NONE)) {	/* already in network byte order*/
				log_msg("'-%c' Invalid address: %s", OPT_RX_MC_IF, optarg);
				rc = 1;
			}
		}

		if ( !rc && aopt_check(common_obj, OPT_TX_MC_IF) ) {
			const char* optarg = aopt_value(common_obj, OPT_TX_MC_IF);
			if (!optarg ||
				((s_user_params.rx_mc_if_addr.s_addr = inet_addr(optarg)) == INADDR_NONE)) {	/* already in network byte order*/
				log_msg("'-%c' Invalid address: %s", OPT_TX_MC_IF, optarg);
				rc = 1;
			}
		}

		if ( !rc && aopt_check(common_obj, OPT_SELECT_TIMEOUT) ) {
			const char* optarg = aopt_value(common_obj, OPT_SELECT_TIMEOUT);
			if (optarg) {
				errno = 0;
				int timeout = strtol(optarg, NULL, 0);
				if (errno != 0  || timeout < -1) {
					log_msg("'-%c' Invalid select/poll/epoll timeout val: %s", OPT_SELECT_TIMEOUT, optarg);
					rc = 1;
				}
				else {
					set_select_timeout(timeout);
				}
			}
			else {
				log_msg("'-%d' Invalid value", OPT_SELECT_TIMEOUT);
				rc = 1;
			}
		}

		if ( !rc && aopt_check(common_obj, OPT_MC_LOOPBACK_ENABLE) ) {
			s_user_params.mc_loop_disable = false;
		}

		if ( !rc && aopt_check(common_obj, OPT_UDP_BUFFER_SIZE) ) {
			const char* optarg = aopt_value(common_obj, OPT_UDP_BUFFER_SIZE);
			if (optarg) {
				errno = 0;
				int udp_buff_size = strtol(optarg, NULL, 0);
				if (errno != 0 || udp_buff_size <= 0) {
					log_msg("'-%c' Invalid udp buffer size: %s", OPT_UDP_BUFFER_SIZE, optarg);
					rc = 1;
				}
				else {
					s_user_params.udp_buff_size = udp_buff_size;
				}
			}
			else {
				log_msg("'-%d' Invalid value", OPT_UDP_BUFFER_SIZE);
				rc = 1;
			}
		}

		if ( !rc && aopt_check(common_obj, OPT_VMAZCOPYREAD) ) {
			s_user_params.is_vmazcopyread = true;
		}

		if ( !rc && aopt_check(common_obj, OPT_DAEMONIZE) ) {
			*p_daemonize = true;
		}

		if ( !rc && aopt_check(common_obj, OPT_NONBLOCKED) ) {
			s_user_params.is_blocked = false;
		}

		if ( !rc && aopt_check(common_obj, OPT_DONTWARMUP) ) {
			s_user_params.do_warmup = false;
		}

		if ( !rc && aopt_check(common_obj, OPT_PREWARMUPWAIT) ) {
			const char* optarg = aopt_value(common_obj, OPT_PREWARMUPWAIT);
			if (optarg) {
				errno = 0;
				int pre_warmup_wait = strtol(optarg, NULL, 0);
				if (errno != 0 || pre_warmup_wait <= 0) {
					log_msg("'-%c' Invalid pre warmup wait: %s", OPT_PREWARMUPWAIT, optarg);
					rc = 1;
				}
				else {
					s_user_params.pre_warmup_wait = pre_warmup_wait;
				}
			}
			else {
				log_msg("'-%d' Invalid value", OPT_PREWARMUPWAIT);
				rc = 1;
			}
		}
		if ( !rc && aopt_check(common_obj, OPT_SOCK_ACCL) ) {
			s_user_params.withsock_accl = true;
		}

	}

	return rc;
}

//------------------------------------------------------------------------------
static int parse_client_opt( const AOPT_OBJECT *client_obj )
{
	int rc = 0;

	s_user_params.mode = MODE_CLIENT;

	if (client_obj) {
		if ( !rc && aopt_check(client_obj, 't') ) {
			const char* optarg = aopt_value(client_obj, 't');
			if (optarg) {
				s_user_params.sec_test_duration = strtol(optarg, NULL, 0);
				if (s_user_params.sec_test_duration <= 0 || s_user_params.sec_test_duration > MAX_DURATION) {
					log_msg("'-%c' Invalid duration: %d", 't', s_user_params.sec_test_duration);
					rc = 1;
				}
			}
			else {
				log_msg("'-%c' Invalid value", 't');
				rc = 1;
			}
		}

		if ( !rc && aopt_check(client_obj, 'b') ) {
			const char* optarg = aopt_value(client_obj, 'b');
			if (optarg) {
				errno = 0;
				int burst_size = strtol(optarg, NULL, 0);
				if (errno != 0 || burst_size < 1) {
					log_msg("'-%c' Invalid burst size: %s", 'b', optarg);
					rc = 1;
				}
				else {
					s_user_params.burst_size = burst_size;
				}
			}
			else {
				log_msg("'-%c' Invalid value", 'b');
				rc = 1;
			}
		}

		if ( !rc && aopt_check(client_obj, 'r') ) {
			const char* optarg = aopt_value(client_obj, 'r');
			if (optarg) {
				errno = 0;
				int range = strtol(optarg, NULL, 0);
				if (errno != 0 || range < 0) {
					log_msg("'-%c' Invalid message range: %s", 'r', optarg);
					rc = 1;
				}
				else {
					s_user_params.msg_size_range = range;
				}
			}
			else {
				log_msg("'-%c' Invalid value", 'r');
				rc = 1;
			}
		}

		if ( !rc && aopt_check(client_obj, OPT_DATA_INTEGRITY) ) {
			s_user_params.data_integrity = true;
		}

		if ( !rc && aopt_check(client_obj, OPT_CLIENT_WORK_WITH_SRV_NUM) ) {
			const char* optarg = aopt_value(client_obj, OPT_CLIENT_WORK_WITH_SRV_NUM);
			if (optarg) {
				errno = 0;
				int srv_num = strtol(optarg, NULL, 0);
				if (errno != 0  || srv_num < 1) {
					log_msg("'-%c' Invalid server num val: %s", OPT_CLIENT_WORK_WITH_SRV_NUM, optarg);
					rc = 1;
				}
				else {
					s_user_params.client_work_with_srv_num = srv_num;
				}
			}
			else {
				log_msg("'-%d' Invalid value", OPT_CLIENT_WORK_WITH_SRV_NUM);
				rc = 1;
			}
		}

		if ( !rc && aopt_check(client_obj, OPT_PPS) ) {
			const char* optarg = aopt_value(client_obj, OPT_PPS);
			if (optarg) {
				if (0 == strcmp("MAX", optarg) || 0 == strcmp("max", optarg)) {
					s_user_params.pps = UINT32_MAX;
				}
				else {
					errno = 0;
					long long temp = strtol(optarg, NULL, 0);
					if (errno != 0  || temp <= 0 || temp > 1<<30 ) {
						log_msg("Invalid %d val: %s", OPT_PPS, optarg);
						rc = 1;
					}
					else {
						s_user_params.pps = (uint32_t)temp;
					}
				}
			}
			else {
				log_msg("'-%d' Invalid value", OPT_PPS);
				rc = 1;
			}
		}


		if ( !rc && aopt_check(client_obj, OPT_SENDER_AFFINITY) ) {
			const char* optarg = aopt_value(client_obj, OPT_SENDER_AFFINITY);
			if (optarg) {
				strcpy(s_user_params.sender_affinity, optarg);
			}
			else {
				log_msg("'-%d' Invalid sender affinity", OPT_SENDER_AFFINITY);
				rc = 1;
			}
		}

		if ( !rc && aopt_check(client_obj, OPT_RECEIVER_AFFINITY) ) {
			const char* optarg = aopt_value(client_obj, OPT_RECEIVER_AFFINITY);
			if (optarg) {
				strcpy(s_user_params.receiver_affinity, optarg);
			}
			else {
				log_msg("'-%d' Invalid receiver affinity", OPT_RECEIVER_AFFINITY);
				rc = 1;
			}
		}

		if ( !rc && aopt_check(client_obj, OPT_FULL_LOG) ) {
			const char* optarg = aopt_value(client_obj, OPT_FULL_LOG);
			if (optarg) {
				errno = 0;
				s_user_params.fileFullLog = fopen(optarg, "w");
				if (errno  || !s_user_params.fileFullLog) {
					log_err("Invalid %d val. Can't open file %s for writing: %m", OPT_FULL_LOG, optarg);
					rc = 1;
				}
			}
			else {
				log_msg("'-%d' Invalid value", OPT_FULL_LOG);
				rc = 1;
			}
		}

		if ( !rc && aopt_check(client_obj, OPT_NO_RDTSC) ) {
			s_user_params.b_no_rdtsc = true;
		}

		if ( !rc && aopt_check(client_obj, OPT_LOAD_VMA) ) {
			const char* optarg = aopt_value(client_obj, OPT_LOAD_VMA);
			//s_user_params.b_load_vma = true;
			if (!optarg || !*optarg) optarg = (char*)"libvma.so"; //default value
			bool success = vma_set_func_pointers(optarg);
			if (!success) {
				log_err("Invalid %d val: %s: failed to set function pointers using the given libvma.so path:", OPT_LOAD_VMA, optarg);
				rc = 1;
			}
		}
	}

	return rc;
}

//------------------------------------------------------------------------------
static char* display_opt( int id, char *buf, size_t buf_size )
{
    char *cur_ptr = buf;

	if ( buf && buf_size ) {
        int cur_len = 0;
        int ret = 0;
        int j = 0;

        ret = snprintf( (cur_ptr + cur_len), (buf_size - cur_len), "%s", sockperf_modes[id].name );
        cur_len += (ret < 0 ? 0 : ret );

	    for ( j = 0; (ret >= 0) && (sockperf_modes[id].shorts[j] != NULL); j++ ) {
	    	if (j == 0) {
	    		ret = snprintf( (cur_ptr + cur_len), (buf_size - cur_len), " (%s", sockperf_modes[id].shorts[j] );
	    	}
	    	else {
	    		ret = snprintf( (cur_ptr + cur_len), (buf_size - cur_len), " ,%s", sockperf_modes[id].shorts[j] );
	    	}
	    	cur_len += (ret < 0 ? 0 : ret );
	    }

	    if ((j > 0) && (ret >= 0)) {
	        ret = snprintf( (cur_ptr + cur_len), (buf_size - cur_len), ")" );
	        cur_len += (ret < 0 ? 0 : ret );
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
void cleanup()
{
	if (s_user_params.fd_handler_type == RECVFROM) {
		close(s_fd_min);
	}
	else {
		int ifd;
		for (ifd = 0; ifd <= s_fd_max; ifd++) {
			if (g_fds_array[ifd]) {
				close(ifd);
				free(g_fds_array[ifd]);
			}
		}

	}
	if(s_user_params.select_timeout) {
		free(s_user_params.select_timeout);
		s_user_params.select_timeout = NULL;
	}
#ifdef  USING_VMA_EXTRA_API
	if (g_dgram_buf) {
		free(g_dgram_buf);
		g_dgram_buf = NULL;
	}
#endif

	delete g_pMessage;
	delete g_pReply;

	if (g_pid_arr) {
		free(g_pid_arr);
	}
}

//------------------------------------------------------------------------------
/* set the action taken when signal received */
void set_signal_action()
{
	static struct sigaction sigact;//Avner: is 'static' needed?

	sigact.sa_handler = s_user_params.mode ? server_sig_handler : client_sig_handler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;

	sigaction(SIGINT, &sigact, NULL);

	if (s_user_params.mode == MODE_CLIENT)
		sigaction(SIGALRM, &sigact, NULL);
}

//------------------------------------------------------------------------------
/* set the timeout of select*/
static void set_select_timeout(int time_out_msec)
{
	if (!s_user_params.select_timeout) {
		s_user_params.select_timeout = (struct timeval*)malloc(sizeof(struct timeval));
		if (!s_user_params.select_timeout) {
			log_err("Failed to allocate memory for pointer select timeout structure");
			exit_with_log(1);
		}
	}
	if (time_out_msec >= 0) {
		// Update timeout
		s_user_params.select_timeout->tv_sec = time_out_msec/1000;
		s_user_params.select_timeout->tv_usec = 1000 * (time_out_msec - s_user_params.select_timeout->tv_sec*1000);
	}
	else {
		// Clear timeout
		free(s_user_params.select_timeout);
		s_user_params.select_timeout = NULL;
	}
}

//------------------------------------------------------------------------------
void set_defaults()
{
	bool success = vma_set_func_pointers(false);
	if (!success) {
		log_err("failed to set function pointers for system functions");
		exit (1);
	}

	memset(&s_user_params, 0, sizeof(struct user_params_t));
	memset(g_fds_array, 0, sizeof(fds_data*)*MAX_FDS_NUM);
	s_user_params.rx_mc_if_addr.s_addr = htonl(INADDR_ANY);
	s_user_params.tx_mc_if_addr.s_addr = htonl(INADDR_ANY);
	s_user_params.sec_test_duration = DEFAULT_TEST_DURATION;
	g_msg_size = MIN_PAYLOAD_SIZE;
	s_user_params.mode = MODE_SERVER;
	s_user_params.packetrate_stats_print_ratio = 0;
	s_user_params.packetrate_stats_print_details = false;
	s_user_params.burst_size	= 1;
	s_user_params.data_integrity = false;
	s_user_params.fd_handler_type = RECVFROM;
	s_user_params.mthread_server = 0;
	s_user_params.msg_size_range = 0;
	s_user_params.udp_buff_size = UDP_BUFF_DEFAULT_SIZE;
	set_select_timeout(DEFAULT_SELECT_TIMEOUT_MSEC);
	s_user_params.threads_num = 1;
	memset(s_user_params.threads_affinity, 0, sizeof(s_user_params.threads_affinity));
	s_user_params.is_blocked = true;
	s_user_params.do_warmup = true;
	s_user_params.pre_warmup_wait = 0;
	s_user_params.is_vmarxfiltercb = false;
	s_user_params.is_vmazcopyread = false;
	g_debug_level = LOG_LVL_INFO;
	s_user_params.mc_loop_disable = true;
	s_user_params.client_work_with_srv_num = 1;
	s_user_params.b_server_reply_via_uc = false;
	s_user_params.b_server_detect_gaps = false;

	s_user_params.pps = PPS_DEFAULT;
	s_user_params.reply_every = REPLY_EVERY_DEFAULT;
	s_user_params.b_client_ping_pong = false;
	s_user_params.b_no_rdtsc = false;
	memset(s_user_params.sender_affinity, 0, sizeof(s_user_params.sender_affinity));
	memset(s_user_params.receiver_affinity, 0, sizeof(s_user_params.receiver_affinity));
	//s_user_params.b_load_vma = false;
	s_user_params.fileFullLog = NULL;
	s_user_params.b_stream = false;
	s_user_params.pPlaybackVector = NULL;

	s_user_params.addr.sin_family = AF_INET;
	s_user_params.addr.sin_port = htons(DEFAULT_PORT);
	inet_aton(DEFAULT_MC_ADDR, &s_user_params.addr.sin_addr);

	s_user_params.daemonize = false;
	s_user_params.withsock_accl = false;
	memset(&s_user_params.mcg_filename, 0, sizeof(s_user_params.mcg_filename));
}

//------------------------------------------------------------------------------
#ifdef  USING_VMA_EXTRA_API
vma_recv_callback_retval_t myapp_vma_recv_pkt_filter_callback(
	int fd, size_t iov_sz, struct iovec iov[], struct vma_info_t* vma_info, void *context)
{
#ifdef ST_TEST
	if (st1) {
		log_msg("DEBUG: ST_TEST - myapp_vma_recv_pkt_filter_callback fd=%d", fd);
		close (st1);
		close (st2);
		st1 = st2 = 0;
	}
#endif

	if (iov_sz) {};
	if (context) {};

	// Check info structure version
	if (vma_info->struct_sz < sizeof(struct vma_info_t)) {
		log_msg("VMA's info struct is not something we can handle so un-register the application's callback function");
		g_vma_api->register_recv_callback(fd, NULL, &fd);
		return VMA_PACKET_RECV;
	}

	int recvsize     = iov[0].iov_len;
	uint8_t* recvbuf = (uint8_t*)iov[0].iov_base;

	if (recvsize < MsgHeader::EFFECTIVE_SIZE) {
		log_err("packet is too small");
		return VMA_PACKET_DROP;

	}

/*
	if ("rule to check if packet should be dropped")
		return VMA_PACKET_DROP;
*/

/*
	if ("Do we support zero copy logic?") {
		// Application must duplicate the iov' & 'vma_info' parameters for later usage
		struct iovec* my_iov = calloc(iov_sz, sizeof(struct iovec));
		memcpy(my_iov, iov, sizeof(struct iovec)*iov_sz);
		myapp_queue_task_new_rcv_pkt(my_iov, iov_sz, vma_info->pkt_desc_id);
		return VMA_PACKET_HOLD;
	}
*/
	/* This does the server_recev_then_send all in the VMA's callback */

	MsgHeader* pHeader = (MsgHeader*) recvbuf;
	if (s_user_params.mode != MODE_BRIDGE) {
		if (! pHeader->isClient() ) {
			if (s_user_params.mc_loop_disable)
				log_err("got != CLIENT_MASK");
			return VMA_PACKET_DROP;
		}
		pHeader->setServer();
	}

	if (pHeader->isPongRequest()) {
		/* get source addr to reply to */
		struct sockaddr_in sendto_addr = g_fds_array[fd]->addr;
		if (!g_fds_array[fd]->is_multicast)  /* In unicast case reply to sender*/
			sendto_addr.sin_addr = vma_info->src->sin_addr;
		msg_sendto(fd, recvbuf, recvsize, &sendto_addr);
	}

	g_receiveCount++;  // should move to setRxTime (once we use it in server side)

	if (g_pApp->m_const_params.packetrate_stats_print_ratio > 0) {
		SwitchOnActivityInfo().execute(g_receiveCount);
	}

	return VMA_PACKET_DROP;
}
#endif

//------------------------------------------------------------------------------
/* returns the new socket fd
 or exit with error code */
#ifdef ST_TEST
static int prepare_socket(struct sockaddr_in* p_addr, bool stTest = false)
#else
static int prepare_socket(struct sockaddr_in* p_addr)
#endif
{
	int fd = -1;
	int is_mulicast = 0;
	u_int reuseaddr_true = 1;
	struct sockaddr_in bind_addr;
	int rcv_buff_size = 0;
	int snd_buff_size = 0;
	int size = sizeof(int);
	int flags, ret;

	is_mulicast = IN_MULTICAST(ntohl(p_addr->sin_addr.s_addr));

	/* create a UDP socket */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		log_err("socket(AF_INET, SOCK_DGRAM)");
		exit_with_log(1);
	}

	if (!s_user_params.is_blocked) {
		/*Uncomment to test FIONBIO command of ioctl
		 * int opt = 1;
		 * ioctl(fd, FIONBIO, &opt);
		*/

		/* change socket to non-blocking */
		flags = fcntl(fd, F_GETFL);
		if (flags < 0) {
			log_err("fcntl(F_GETFL)");
		}
		flags |=  O_NONBLOCK;
		ret = fcntl(fd, F_SETFL, flags);
		if (ret < 0) {
			log_err("fcntl(F_SETFL)");
		}
		//log_msg("fd %d is non-blocked now", fd);
	}

	if (s_user_params.withsock_accl == true)
	{
			if (setsockopt(fd, SOL_SOCKET, 100, NULL, 0) < 0) {
				log_err("setsockopt(100), set sock-accl failed.  It could be that this option is not supported in your system");
				exit_with_log(1);
			}
			log_msg("succeed to set sock-accl");
	}

	/* allow multiple sockets to use the same PORT number */
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_true, sizeof(reuseaddr_true)) < 0) {
		log_err("setsockopt(SO_REUSEADDR) failed");
		exit_with_log(1);
	}

	if (s_user_params.udp_buff_size > 0) {
		/* enlarge socket's buffer depth */

		if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &(s_user_params.udp_buff_size), sizeof(s_user_params.udp_buff_size)) < 0) {
			log_err("setsockopt(SO_RCVBUF) failed");
			exit_with_log(1);
		}
		if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcv_buff_size,(socklen_t *)&size) < 0) {
			log_err("getsockopt(SO_RCVBUF) failed");
			exit_with_log(1);
		}
		if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &(s_user_params.udp_buff_size), sizeof(s_user_params.udp_buff_size)) < 0) {
			log_err("setsockopt(SO_SNDBUF) failed");
			exit_with_log(1);
		}
		if (getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &snd_buff_size, (socklen_t *)&size) < 0) {
			log_err("getsockopt(SO_SNDBUF) failed");
			exit_with_log(1);
		}

		log_msg("UDP buffers sizes of fd %d: RX: %d Byte, TX: %d Byte", fd, rcv_buff_size, snd_buff_size);
		if (rcv_buff_size < s_user_params.udp_buff_size*2 ||
		    snd_buff_size < s_user_params.udp_buff_size*2  ) {
			log_msg("WARNING: Failed setting receive or send udp socket buffer size to %d bytes (check 'sysctl net.core.rmem_max' value)", s_user_params.udp_buff_size);
		}
	}

	memset(&bind_addr, 0, sizeof(struct sockaddr_in));
	bind_addr.sin_family = AF_INET;
	bind_addr.sin_port = p_addr->sin_port;
	bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	/*log_dbg ("IP to bind: %s",inet_ntoa(client_addr.sin_addr));*/
	if (bind(fd, (struct sockaddr*)&bind_addr, sizeof(struct sockaddr)) < 0) {
		log_err("bind()");
		exit_with_log(1);
	}

	if (is_mulicast) {
		struct ip_mreq mreq;
		memset(&mreq,0,sizeof(struct ip_mreq));

		/* use setsockopt() to request that the kernel join a multicast group */
		/* and specify a specific interface address on which to receive the packets of this socket */
		/* NOTE: we don't do this if case of client (sender) in stream mode */
		if (!s_user_params.b_stream || s_user_params.mode != MODE_CLIENT) {
			mreq.imr_multiaddr = p_addr->sin_addr;
			mreq.imr_interface.s_addr = s_user_params.rx_mc_if_addr.s_addr;
			if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
				log_err("setsockopt(IP_ADD_MEMBERSHIP)");
				exit_with_log(1);
			}
		}

		/* specify a specific interface address on which to transmitted the multicast packets of this socket */
		if (s_user_params.tx_mc_if_addr.s_addr != INADDR_ANY) {
			if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &s_user_params.tx_mc_if_addr, sizeof(s_user_params.tx_mc_if_addr)) < 0) {
				log_err("setsockopt(IP_MULTICAST_IF)");
				exit_with_log(1);
			}
		}

		if (s_user_params.mc_loop_disable) {
			/* disable multicast loop of all transmitted packets */
			u_char loop_disabled = 0;
			if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop_disabled, sizeof(loop_disabled)) < 0) {
				log_err("setsockopt(IP_MULTICAST_LOOP)");
				exit_with_log(1);
			}
		}
	}

#ifdef  USING_VMA_EXTRA_API
#ifdef ST_TEST
	if (!stTest)
#endif
	if (s_user_params.is_vmarxfiltercb && g_vma_api) {
		// Try to register application with VMA's special receive notification callback logic
		if (g_vma_api->register_recv_callback(fd, myapp_vma_recv_pkt_filter_callback, &fd) < 0) {
			log_err("vma_api->register_recv_callback failed. Try running without option 'vmarxfiltercb'");
		}
		else {
			log_dbg("vma_api->register_recv_callback successful registered");
		}
	}
#endif

	return fd;
}

//------------------------------------------------------------------------------
/* get IP:port pairs from the file and initialize the list */
static int set_mcgroups_fromfile(const char *mcg_filename)
{
	FILE *file_fd = NULL;
	char line[MAX_MCFILE_LINE_LENGTH];
	char *res;
	char *ip;
	char *port;
	fds_data *tmp;
	int curr_fd = 0, last_fd = 0;
	int regexpres;

	regex_t regexpr;

	if ((file_fd = fopen(mcg_filename, "r")) == NULL) {
		printf("No such file: %s \n", mcg_filename);
		exit_with_log(4);
	}

	while ((res = fgets(line, MAX_MCFILE_LINE_LENGTH, file_fd))) {
		if (!res) {
			if (ferror(file_fd)) {
				log_err("fread()");
				return -1;
			}
			else
				return 0;	/* encountered EOF */
		}
		g_sockets_num++;

		regexpres = regcomp(&regexpr, IP_PORT_FORMAT_REG_EXP, REG_EXTENDED|REG_NOSUB);
		if (regexpres) {
			log_err("Failed to compile regexp");
			exit_with_log(1);
		}
		regexpres = regexec(&regexpr, line, (size_t)0, NULL, 0);
		regfree(&regexpr);
		if (regexpres) {
			log_msg("Invalid input in line %d: "
				"each line must have the following format: ip:port",
				g_sockets_num);
			exit_with_log(1);
		}

		ip = strtok(line, ":");
		port = strtok(NULL, ":\n");
		if (!ip || !port) {
			log_msg("Invalid input in line %d: "
				"each line must have the following format: ip:port",
				g_sockets_num);
			exit_with_log(8);
		}
		tmp = (struct fds_data *)malloc(sizeof(struct fds_data));
		if (!tmp) {
			log_err("Failed to allocate memory with malloc()");
			exit_with_log(1);
		}
		memset(tmp,0,sizeof(struct fds_data));
		tmp->addr.sin_family = AF_INET;
		tmp->addr.sin_port = htons(atoi(port));
		if (!inet_aton(ip, &tmp->addr.sin_addr)) {
			log_msg("Invalid input in line %d: '%s:%s'", g_sockets_num, ip, port);
			exit_with_log(8);
		}
		tmp->is_multicast = IN_MULTICAST(ntohl(tmp->addr.sin_addr.s_addr));
		curr_fd = prepare_socket(&tmp->addr);
		if (g_sockets_num != 1) { /*it is not the first fd*/
			g_fds_array[last_fd]->next_fd = curr_fd;
		}
		else {
			s_fd_min = curr_fd;
		}
		last_fd = curr_fd;
		g_fds_array[curr_fd] = tmp;
		s_fd_max = max(s_fd_max, curr_fd);
		s_fd_min = min(s_fd_min, curr_fd);
		s_fd_num++;
	}

	g_fds_array[s_fd_max]->next_fd = s_fd_min; /* close loop for fast wrap around in client */

	fclose(file_fd);

#ifdef ST_TEST
	{
		char *ip   = "224.3.2.1";
		char *port = "11111";
		static fds_data data1, data2;

		tmp = &data1;
		memset(tmp,0,sizeof(struct fds_data));
		tmp->addr.sin_family = AF_INET;
		tmp->addr.sin_port = htons(atoi(port));
		if (!inet_aton(ip, &tmp->addr.sin_addr)) {
			log_msg("Invalid input: '%s:%s'", ip, port);
			exit_with_log(8);
		}
		tmp->is_multicast = IN_MULTICAST(ntohl(tmp->addr.sin_addr.s_addr));
		st1 = prepare_socket(&tmp->addr, true);

		tmp = &data2;
		memset(tmp,0,sizeof(struct fds_data));
		tmp->addr.sin_family = AF_INET;
		tmp->addr.sin_port = htons(atoi(port));
		if (!inet_aton(ip, &tmp->addr.sin_addr)) {
			log_msg("Invalid input: '%s:%s'", ip, port);
			exit_with_log(8);
		}
		tmp->is_multicast = IN_MULTICAST(ntohl(tmp->addr.sin_addr.s_addr));
		st2 = prepare_socket(&tmp->addr, true);

	}
#endif
	return 0;
}


//------------------------------------------------------------------------------
int bringup(const int *p_daemonize)
{

#if 0
	// AlexR: for testing daemonize with allready opened UDP socket
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd) {
		log_msg("new socket - dummy");
	}
#endif

	if (*p_daemonize) {
		if (daemon(1, 1)) {
			log_err("Failed to daemonize");
		}
		log_msg("Running as daemon");
	}

	/* TODO it should be moved in suitable place */
	{
		if (strlen(s_user_params.mcg_filename) > 0 || s_user_params.mthread_server) {
			if (strlen(s_user_params.mcg_filename) > 0) {
				set_mcgroups_fromfile(s_user_params.mcg_filename);
			}
			if (s_user_params.mthread_server) {
				if (s_user_params.threads_num > g_sockets_num  || s_user_params.threads_num == 0) {
					s_user_params.threads_num = g_sockets_num;
				}
				g_pid_arr = (int*)malloc(sizeof(int)*(s_user_params.threads_num + 1));
				if(!g_pid_arr) {
					log_err("Failed to allocate memory for pid array");
					exit_with_log(1);
				}
				else {
					log_msg("Running %d threads to manage %d sockets",s_user_params.threads_num,g_sockets_num);
				}
			}
		}
		else {
			fds_data *tmp = (struct fds_data *)malloc(sizeof(struct fds_data));
			memset(tmp, 0, sizeof(struct fds_data));
			memcpy(&tmp->addr, &(s_user_params.addr), sizeof(struct sockaddr_in));
			tmp->is_multicast = IN_MULTICAST(ntohl(tmp->addr.sin_addr.s_addr));
			s_fd_min = s_fd_max = prepare_socket(&tmp->addr);
			s_fd_num = 1;
			g_fds_array[s_fd_min] = tmp;
			g_fds_array[s_fd_min]->next_fd = s_fd_min;
		}
	}

	if (s_user_params.is_vmarxfiltercb || s_user_params.is_vmazcopyread) {
#ifdef  USING_VMA_EXTRA_API
		// Get VMA extended API
		g_vma_api = vma_get_api();
		if (g_vma_api == NULL)
			log_err("VMA Extra API not found - working with default socket APIs");
		else
			log_msg("VMA Extra API found - using VMA's receive zero copy and packet filter APIs");

		g_vma_dgram_desc_size = sizeof(struct vma_datagram_t) + sizeof(struct iovec) * 16;
#else
		log_msg("This version is not compiled with VMA extra API");
#endif
	}

	int max_buff_size = max(g_msg_size+1, g_vma_dgram_desc_size);
	max_buff_size = max(max_buff_size, MAX_PAYLOAD_SIZE);


#ifdef  USING_VMA_EXTRA_API
	if (s_user_params.is_vmazcopyread && g_vma_api){
		   g_dgram_buf = (unsigned char*)malloc(max_buff_size);
	}
#endif

	Message::initMaxSize(max_buff_size);
	g_pMessage = new Message();

	if (s_user_params.mode == MODE_CLIENT) {
		g_pMessage->getHeader()->setClient();
	}
	else {
		g_pMessage->getHeader()->setServer();
	}

	g_pReply = new Message();

	int64_t cycleDurationNsec = NSEC_IN_SEC * s_user_params.burst_size / s_user_params.pps;

	if (s_user_params.pps == UINT32_MAX) { // MAX PPS mode
		s_user_params.pps = PPS_MAX;
		cycleDurationNsec = 0;
	}

	s_user_params.cycleDuration = TicksDuration(cycleDurationNsec);

	uint64_t _maxTestDuration = 1 + s_user_params.sec_test_duration;       // + 1sec for timer inaccuracy safety
	uint64_t _maxSequenceNo = _maxTestDuration * s_user_params.pps + 10 * s_user_params.reply_every; // + 10 replies for safety
	_maxSequenceNo += s_user_params.burst_size; // needed for the case burst_size > pps


	if (s_user_params.pPlaybackVector) {
		_maxSequenceNo = s_user_params.pPlaybackVector->size();
	}
	g_pPacketTimes = new PacketTimes(_maxSequenceNo, s_user_params.reply_every, s_user_params.client_work_with_srv_num);

	set_signal_action();

	return 0;
}

//------------------------------------------------------------------------------
void do_test()
{
	switch (s_user_params.mode) {
	case MODE_CLIENT:
		client_handler(s_fd_min, s_fd_max, s_fd_num);
		break;
	case MODE_SERVER:
	   if (s_user_params.mthread_server) {
	           server_select_per_thread();
	   }
	   else {
		   server_handler(s_fd_min, s_fd_max, s_fd_num);
	   }
	   break;
	case MODE_BRIDGE:
		server_handler(s_fd_min, s_fd_max, s_fd_num);
		break;
	}

	cleanup();
}

//------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
	try {
		int rc =0;

		// step #1:  set default values for command line args
		set_defaults();

		// step #2:  parse command line args
		if ( argc > 1 ) {
			int i = 0;
			int j = 0;
			int found = 0;

		    for (i = 0; sockperf_modes[i].name != NULL; i++) {
		         if (strcmp(sockperf_modes[i].name, argv[1]) == 0) {
					 found = 1;
		         }
		         else {
		        	 for ( j = 0; sockperf_modes[i].shorts[j] != NULL; j++ ) {
						 if (strcmp(sockperf_modes[i].shorts[j], argv[1]) == 0) {
							 found = 1;
							 break;
						 }
		        	 }
		         }

		         if (found) {
		        	 rc = sockperf_modes[i].func(i, argc - 1, (const char**)(argv + 1));
		        	 break;
		         }
		    }
		    /*  check if the first option is invalid or rc > 0 */
		    if ((sockperf_modes[i].name == NULL) ||
		    	(rc > 0)) {
		    	rc = proc_mode_help(0, 0, NULL);
		    }
		}
		else {
			rc = proc_mode_help(0, 0, NULL);
		}

	    if (rc) {
	    	exit(0);
	    }

		// Prepare application to start
		if (bringup(&s_user_params.daemonize)) {
			return 1;
		}

		log_dbg("+INFO:\n\t\
mode = %d \n\t\
with_sock_accl = %d \n\t\
msg_size_range = %d \n\t\
sec_test_duration = %d \n\t\
data_integrity = %d \n\t\
packetrate_stats_print_ratio = %d \n\t\
burst_size = %d \n\t\
packetrate_stats_print_details = %d \n\t\
fd_handler_type = %d \n\t\
mthread_server = %d \n\t\
udp_buff_size = %d \n\t\
threads_num = %d \n\t\
threads_affinity = %s \n\t\
is_blocked = %d \n\t\
do_warmup = %d \n\t\
pre_warmup_wait = %d \n\t\
is_vmarxfiltercb = %d \n\t\
is_vmazcopyread = %d \n\t\
mc_loop_disable = %d \n\t\
client_work_with_srv_num = %d \n\t\
b_server_reply_via_uc = %d \n\t\
b_server_detect_gaps = %d\n\t\
pps = %d \n\t\
reply_every = %d \n\t\
b_client_ping_pong = %d \n\t\
b_no_rdtsc = %d \n\t\
sender_affinity = %s \n\t\
receiver_affinity = %s \n\t\
b_stream = %d \n\t\
daemonize = %d \n\t\
mcg_filename = %s \n",
s_user_params.mode,
s_user_params.withsock_accl,
s_user_params.msg_size_range,
s_user_params.sec_test_duration,
s_user_params.data_integrity,
s_user_params.packetrate_stats_print_ratio,
s_user_params.burst_size,
s_user_params.packetrate_stats_print_details,
s_user_params.fd_handler_type,
s_user_params.mthread_server,
s_user_params.udp_buff_size,
s_user_params.threads_num,
s_user_params.threads_affinity,
s_user_params.is_blocked,
s_user_params.do_warmup,
s_user_params.pre_warmup_wait,
s_user_params.is_vmarxfiltercb,
s_user_params.is_vmazcopyread,
s_user_params.mc_loop_disable,
s_user_params.client_work_with_srv_num,
s_user_params.b_server_reply_via_uc,
s_user_params.b_server_detect_gaps,
s_user_params.pps,
s_user_params.reply_every,
s_user_params.b_client_ping_pong,
s_user_params.b_no_rdtsc,
(strlen(s_user_params.sender_affinity) ? s_user_params.sender_affinity : "<empty>"),
(strlen(s_user_params.receiver_affinity) ? s_user_params.receiver_affinity : "<empty>"),
s_user_params.b_stream,
s_user_params.daemonize,
(strlen(s_user_params.mcg_filename) ? s_user_params.mcg_filename : "<empty>"));

		// Display application version
		log_msg("\e[2;35m == version #%s == \e[0m", STR(VERSION));

		// Display VMA version
#ifdef VMA_LIBRARY_MAJOR
		log_msg("Linked with VMA version: %d.%d.%d.%d", VMA_LIBRARY_MAJOR, VMA_LIBRARY_MINOR, VMA_LIBRARY_REVISION, VMA_LIBRARY_RELEASE);
#endif
#ifdef VMA_DATE_TIME
		log_msg("VMA Build Date: %s", VMA_DATE_TIME);
#endif

		// step #4:  get prepared for test - store application context in global variables (will use const member)
		App app(s_user_params, s_mutable_params);
		g_pApp = &app;

		// temp test for measuring time of taking time
		const int SIZE = 1000;
		TicksTime start, end;
		start.setNow();
		for (int i = 0; i < SIZE; i++)
			end.setNow();
		log_dbg("+INFO: taking time, using the given settings, consumes %.3lf nsec", (double)(end-start).toNsec()/SIZE);
		tscval_t tstart, tend;
		tstart = gettimeoftsc();
		for (int i = 0; i < SIZE; i++)
			tend = gettimeoftsc();
		double tdelta = (double)tend - (double)tstart;
		double ticks_per_second = get_tsc_rate_per_second();
		log_dbg("+INFO: taking rdtsc directly consumes %.3lf nsec", tdelta / SIZE * 1000*1000*1000 / ticks_per_second );


		// step #5: do run the test
		/*
		** TEST START
		*/
		do_test();
		/*
		** TEST END
		*/

	}
	catch (const std::exception & e) {
		printf(MODULE_NAME ": test failed because of an exception with the following information:\n\t%s\n", e.what());
		exit_with_log (1);
	}
	catch (...) {
		printf(MODULE_NAME ": test failed because of an unknown exception \n");
		exit_with_log (1);
	}

	exit (0);
}
