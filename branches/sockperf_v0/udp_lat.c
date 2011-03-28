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
 * How to Build: 'gcc -lpthread -lrt -o udp_lat udp_lat.c'
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>		/* random()*/
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>		/* clock_gettime()*/
#include <unistd.h>		/* getopt() and sleep()*/
#include <getopt.h>		/* getopt()*/
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
#include <arpa/inet.h>		/* internet address manipulation*/
#include <netinet/in.h>		/* internet address manipulation*/


//#define USING_VMA_EXTRA_API
#ifdef  USING_VMA_EXTRA_API
#include <vma/vma_extra.h>
#endif

int prepare_socket(struct sockaddr_in* p_addr);

#define MIN_PAYLOAD_SIZE        	2
#define MAX_PAYLOAD_SIZE        	(65506)
#define MAX_STREAM_SIZE         	(50*1024*1024)

#define DEFAULT_TEST_DURATION		1	/* [sec] */
#define DEFAULT_MC_ADDR			"0.0.0.0"
#define DEFAULT_PORT			11111
#define DEFAULT_IP_MTU 			1500
#define DEFAULT_IP_PAYLOAD_SZ 		(DEFAULT_IP_MTU-28)
#define DUMMY_PORT			57341

#ifndef MAX_PATH_LENGTH
#define MAX_PATH_LENGTH         	1024
#endif
#define MAX_MCFILE_LINE_LENGTH  	23	/* sizeof("255.255.255.255:11111\0") */
#define IP_PORT_FORMAT_REG_EXP		"^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}"\
					"(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?):"\
					"(6553[0-5]|655[0-2][0-9]|65[0-4][0-9]{2}|6[0-4][0-9]{3}|[0-5]?[0-9]{1,4})\n"

#define CLIENT_MASK 			0x55
#define SERVER_MASK 			0xAA
#define MAX_ARGV_SIZE			256
#define RECIEVE_AGAIN_S_SELECT 		2
#define MAX_DURATION 			36000000
#define MAX_FDS_NUM 			1024
#define UDP_BUFF_DEFAULT_SIZE 		0
#define DEFAULT_SELECT_TIMEOUT_MSEC	10
#define DEFAULT_DEBUG_LEVEL		0		

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
	OPT_VMAZCOPYREAD,		//13
	OPT_MC_LOOPBACK_DISABLE,	//14
	OPT_CLIENT_WORK_WITH_SRV_NUM,   //15
	OPT_FORCE_UC_REPLY              //16

};

#define SCALE_UP_1000(_n_)		(((_n_) + 500) / 1000)
#define SCALE_DOWN_1000(_n_)		((_n_) * 1000)


#define NANO_TO_MICRO(n)		SCALE_UP_1000(n)
#define MICRO_TO_NANO(n)		SCALE_DOWN_1000(n)
#define MICRO_TO_SEC(n)			SCALE_UP_1000( SCALE_UP_1000(n) )
#define SEC_TO_MICRO(n)			SCALE_DOWN_1000( SCALE_DOWN_1000(n) )
#define SEC_TO_NANO(n)			SCALE_DOWN_1000( SCALE_DOWN_1000( SCALE_DOWN_1000(n) ) )

#define TS_TO_NANO(x) 			(SEC_TO_NANO((long long)((x)->tv_sec)) + (long long)((x)->tv_nsec))

#define TIME_DIFF_in_NANO(start,end)	(SEC_TO_NANO((end).tv_sec-(start).tv_sec) + \
					((end).tv_nsec-(start).tv_nsec))
#define TIME_DIFF_in_MICRO(start,end)	(SEC_TO_MICRO((end).tv_sec-(start).tv_sec) + \
					(NANO_TO_MICRO((end).tv_nsec-(start).tv_nsec)))


#define MODULE_NAME			"udp_lat: "
#define log_msg(log_fmt, log_args...)	printf(MODULE_NAME log_fmt "\n", ##log_args)
#define log_err(log_fmt, log_args...)	printf(MODULE_NAME "%d:ERROR: " log_fmt " (errno=%d %s)\n", __LINE__, ##log_args, errno, strerror(errno))
#define log_dbg(log_fmt, log_args...)	if (debug_level >= LOG_LVL_DEBUG) { printf(MODULE_NAME log_fmt "\n", ##log_args); }

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

typedef enum {
	LOG_LVL_INFO = 0,
	LOG_LVL_DEBUG
} debug_level_t;

int epfd;
bool b_exit = false;
struct sigaction sigact;
unsigned long long packet_counter = 0;

unsigned long long cycle_counter = 0; 
unsigned long long cycle_wait_loop_counter = 0; 
unsigned long long cycle_start_time_nsec; 

double latency_usec_max = 0.0;
unsigned int packet_counter_at_max_latency = 0;

struct {
   unsigned int min_usec, count;
} latency_hist[] = { {0,0}, {3,0}, {5,0}, {7,0}, {10,0}, {15,0}, {20,0}, {50,0}, {100,0}, {200,0}, {500,0}, {1000,0}, {2000,0}, {5000,0}, {-1,0}};
int latency_hist_size = (int)(sizeof(latency_hist)/sizeof(latency_hist[0]));

struct timespec start_time, end_time;
struct timespec start_round_time, end_round_time;

debug_level_t debug_level = LOG_LVL_INFO;
int fd_max = 0;
int fd_min = 0;	/* used as THE fd when single mc group is given (RECVFROM blocked mode) */
int fd_num = 0;
int *pid_arr = NULL;
fd_set readfds;
unsigned char *msgbuf = NULL;

#ifdef  USING_VMA_EXTRA_API
unsigned char* dgram_buf = NULL;
struct vma_datagram_t *dgram = NULL;
#endif

int max_buff_size = 0;
int vma_dgram_desc_size = 0;
unsigned char *pattern = NULL;
unsigned int data_integrity_failed = 0;
unsigned int duplicate_packets_counter = 0;
int sockets_num = 0;
int read_from_file = 0;
regex_t regexpr;
struct pollfd *poll_fd_arr = NULL;
struct epoll_event *epoll_events = NULL;   
struct timeval curr_tv, last_tv;
unsigned long long last_packet_counter = 0;

const char* fds_handle_desc[FD_HANDLE_MAX] = 
{
	"recvfrom",     
	"select",
	"poll",
	"epoll"
};


struct user_params_t {
	work_mode_t mode;
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
	bool b_client_calc_details;
	bool stream_mode;
	int mthread_server;
	struct timeval* select_timeout;
	int udp_buff_size;
	int threads_num;
	bool is_blocked;
	bool do_warmup;
	unsigned int pre_warmup_wait;
	bool is_vmarxfiltercb;
	bool is_vmazcopyread;
	unsigned long long cycle_duration_nsec;
	bool mc_loop_disable;
	int client_work_with_srv_num;
	bool b_server_reply_via_uc;
} user_params;

typedef struct spike{
   double usec;
   unsigned long long packet_counter_at_spike;
   int next;
 }spike;

typedef struct static_lst{
	int head, tail;
}static_lst;

typedef struct fds_data {
	struct sockaddr_in addr;
	int is_multicast;
	int next_fd;
} fds_data;

typedef union packet_rtt_data {
	struct timespec start_round_time;
	double rtt;
}packet_rtt_data;

typedef struct sub_fds_arr_info {
	int fd_min;
	int fd_max;
	int fd_num;
}sub_fds_arr_info;

fds_data* fds_array[MAX_FDS_NUM];
static_lst spikes_lst;
spike *spikes = NULL;
int max_spikes_num = 1;
int spikes_num = 0;
packet_rtt_data *rtt_data = NULL;
unsigned long long * packet_counter_arr = NULL;
int min_msg_size = MIN_PAYLOAD_SIZE;
int max_msg_size = MIN_PAYLOAD_SIZE;

#ifdef  USING_VMA_EXTRA_API
struct vma_api_t *vma_api;
#endif

#define max(x,y)	({typeof(x) _x = (x); typeof(y) _y = (y); (void)(&_x == &_y); _x > _y ? _x : _y; })
#define min(x,y)	({typeof(x) _x = (x); typeof(y) _y = (y); (void)(&_x == &_y); _x < _y ? _x : _y; })

static void usage(const char *argv0)
{
	printf("\nUdp Latency Test\n");
	printf("Usage:\n");
	printf("\t%s [OPTIONS]\n", argv0);
	printf("\t%s -s\n", argv0);
	printf("\t%s -s [-i ip] [-p port] [-m message_size] [--rx_mc_if ip] [--tx_mc_if ip]\n", argv0);
	printf("\t%s -s -f file [-F s/p/e] [-m message_size] [--rx_mc_if ip] [--tx_mc_if ip]\n", argv0);
	printf("\t%s -c -i ip  [-p port] [-m message_size] [-t time] [--data_integrity] [-I 5]\n", argv0);
	printf("\t%s -c -f file [-F s/p/e] [-m message_size] [-r msg_size_range] [-t time]\n", argv0);
	printf("\t%s -B -i ip [-p port] [--rx_mc_if ip] [--tx_mc_if ip] [-A 10000]\n", argv0);
	printf("\t%s -B -f file [-F s/p/e] [--rx_mc_if ip] [--tx_mc_if ip] [-a 10000]\n", argv0);
	printf("\n");
	printf("Options:\n");
	printf("  -i, --ip=<ip>\t\t\tlisten on/send to ip <ip>\n");
	printf("  -p, --port=<port>\t\tlisten on/connect to port <port> (default %d)\n", DEFAULT_PORT);
	printf("  -m, --msg_size=<size>\t\tuse messages of size <size> bytes (minimum default %d)\n", MIN_PAYLOAD_SIZE);
	printf("  -f, --file=<file>\t\tread multiple ip+port combinations from file <file> (server uses select)\n");
	printf("  -F, --io_hanlder_type\t\ttype of multiple file descriptors handle [s|select|p|poll|e|epoll](default select)\n");
	printf("  -a, --activity=<N>\t\tmeasure activity by printing a '.' for the last <N> packets processed\n");
	printf("  -A, --Activity=<N>\t\tmeasure activity by printing the duration for last <N> packets processed\n");
	printf("  --rx_mc_if=<ip>\t\t<ip> address of interface on which to receive mulitcast packets (can be other then route table)\n");
	printf("  --tx_mc_if=<ip>\t\t<ip> address of interface on which to transmit mulitcast packets (can be other then route table)\n");
	printf("  --timeout=<msec>\t\tset select/poll/epoll timeout to <msec>, -1 for infinite (default is 10 msec)\n");
	printf("  --mc_loopback_disable\t\tdisables mc loopback (default enables).\n");
	printf("  --udp-buffer-size=<size>\tset udp buffer size to <size> bytes\n");
	printf("  --vmazcopyread\t\tIf possible use VMA's zero copy reads API (See VMA's readme)\n");
	printf("  --daemonize\t\t\trun as daemon\n");
	printf("  --nonblocked\t\t\topen non-blocked sockets\n");
	printf("  --dontwarmup\t\t\tdon't send warm up packets on start\n");
	printf("  --pre_warmup_wait\t\ttime to wait before sending warm up packets (seconds)\n");
	printf("  -d, --debug\t\t\tprint extra debug information\n");
	printf("  -v, --version\t\t\tprint version\n");
	printf("  -h, --help\t\t\tprint this help message\n");	
	printf("Server:\n");
	printf("  -s, --server\t\t\trun server (default - unicast)\n");
	printf("  -B, --Bridge\t\t\trun in Bridge mode\n");
	printf("  --threads-num=<N>\t\trun <N> threads on server side (requires '-f' option)\n");
	printf("  --vmarxfiltercb\t\tIf possible use VMA's receive path packet filter callback API (See VMA's readme)\n");
	printf("  --force_unicast_reply\t\tforce server to reply via unicast\n");
	printf("Client:\n");
	printf("  -c, --client\t\t\trun client\n");
	printf("  -t, --time=<sec>\t\trun for <sec> seconds (default %d, max = %d)\n", DEFAULT_TEST_DURATION, MAX_DURATION);
	printf("  -b, --burst=<size>\t\tcontrol the client's number of a packets sent in every burst\n");
	printf("  -r, --range=<N>\t\tcomes with -m <size>, randomly change the messages size in range: <size> +- <N>\n");
	printf("  -I, --information=<N>\t\tcollect and print client side additional latency information including details about <N> highest spikes\n");
	printf("  --data_integrity\t\tperform data integrity test\n");
	printf("  --cycle_duration=<usec>\tsets the client's send+receive cycle duration to at least <usec>\n");
	printf("  --srv_num=<N>\t\t\tset num of servers the client works with to N\n");
	printf("\n");
}

void print_version()
{
#ifdef VMA_LIBRARY_MAJOR
	log_msg("Linked with VMA version: %d.%d.%d.%d", VMA_LIBRARY_MAJOR, VMA_LIBRARY_MINOR, VMA_LIBRARY_REVISION, VMA_LIBRARY_RELEASE);
#else 
	log_msg("No version info");
#endif
#ifdef VMA_DATE_TIME
	log_msg("Build Date: %s", VMA_DATE_TIME);
#endif
}

void cleanup()
{   
	if (user_params.fd_handler_type == RECVFROM) {
		close(fd_min);
	}
	else {
		int ifd;
		for (ifd = 0; ifd <= fd_max; ifd++) {
			if (fds_array[ifd]) {
				close(ifd);
				free(fds_array[ifd]);
			}
		}            
                
	}
	if(user_params.b_client_calc_details == true){
		free(packet_counter_arr);
		free(rtt_data);
		free(spikes);
	}	
	if(user_params.select_timeout) {
		free(user_params.select_timeout);
		user_params.select_timeout = NULL;
	}
	if (msgbuf) {
		free(msgbuf);
		msgbuf = NULL;
	}
#ifdef USING_VMA_EXTRA_API
	if (dgram_buf) {
		free(dgram_buf);
		dgram_buf = NULL;
	}
#endif
	if (pattern) {
		free(pattern);
		pattern = NULL;
	}
	if (pid_arr) {
		free(pid_arr);
	}
}

pid_t gettid(void)
{
        return syscall(__NR_gettid);
}

void sig_handler(int signum)
{
	if (b_exit) {
		log_msg("Test end (interrupted by signal %d)", signum);
		return;
	}

	// Just in case not Activity updates where logged add a '\n'
	if (user_params.packetrate_stats_print_ratio && !user_params.packetrate_stats_print_details && 
	   (user_params.packetrate_stats_print_ratio < packet_counter))
		printf("\n");

	if (user_params.mthread_server) {
		if (gettid() == pid_arr[0]) {  //main thread
			if (debug_level >= LOG_LVL_DEBUG) {
				log_dbg("Main thread %d got signal %d - exiting",gettid(),signum);
			}
			else {
				log_msg("Got signal %d - exiting", signum);
			}
		}
		else {
			log_dbg("Secondary thread %d got signal %d - exiting", gettid(),signum);
		}
	}
	else {
		switch (signum) {
		case SIGINT:
			log_msg("Test end (interrupted by user)");
			break;
		default:
			log_msg("Test end (interrupted by signal %d)", signum);
			break;
		}
	}

	if (!packet_counter) {
		log_msg("No messages were received on the server.");
	}
	else {
		if (user_params.stream_mode) {
			// Send only mode!
			log_msg("Total of %lld messages received", packet_counter);
		}
		else {
			// Default latency test mode
			log_msg("Total %lld messages received and echoed back", packet_counter);
		}
	}

	b_exit = true;
}

static inline void print_activity_info(unsigned long long counter)
{
	static int print_activity_info_header = 0;

	if (user_params.packetrate_stats_print_details) {
		gettimeofday(&curr_tv, NULL);
		if ((curr_tv.tv_sec - last_tv.tv_sec) < 3600) {
			unsigned long long interval_usec = (curr_tv.tv_sec - last_tv.tv_sec)*1000000 + (curr_tv.tv_usec - last_tv.tv_usec);
			if (interval_usec) {
				unsigned long long interval_packet_rate = (1000000 * (unsigned long long)user_params.packetrate_stats_print_ratio) / interval_usec;
				if (print_activity_info_header <= 0) {
					print_activity_info_header = 20;
					printf("    -- Interval --     -- Message Rate --  -- Total Message Count --\n");
				}
				printf(" %10llu [usec]    %10llu [msg/s]    %13llu [msg]\n",
				       interval_usec, interval_packet_rate, counter);
				print_activity_info_header--;
			}
			else {
				printf("Interval: %8lld [usec]\n", interval_usec);
			}
		}
		last_tv = curr_tv;
	}
	else {
		printf(".");
	}
	fflush(stdout);
}
void print_average_latency(double usecAvarageLatency)
{
	if (user_params.burst_size == 1) {
		log_msg("Summary: Latency is %.3lf usec", usecAvarageLatency);
	}
	else {
		log_msg("Summary: Latency of burst of %d packets is %.3lf usec", user_params.burst_size, usecAvarageLatency);
	}
}

void print_histogram()
{
	int pos_usec = 0;
	bool found_first_non_zero_latency_value = false;
	
	printf("Latency histogram [usec]: \n");
	for (pos_usec = latency_hist_size-1; pos_usec >= 0; pos_usec--) {
		if (found_first_non_zero_latency_value == false && pos_usec > 0 && latency_hist[pos_usec-1].count > 0)
			found_first_non_zero_latency_value = true;
		if (found_first_non_zero_latency_value == true && pos_usec < latency_hist_size - 1)
			printf ("\tmin_usec: %5d  count: %d\n", latency_hist[pos_usec].min_usec, latency_hist[pos_usec].count);
	}
}	

void print_spike_info(spike* spike)
{
	printf("\tspike: %6.3lf   at packet counter: %lld\n", spike->usec, spike->packet_counter_at_spike);
}

void print_spikes_list()
{
	int count = 1;
	int curr = spikes_lst.head;
	
	printf("Spikes details [usec]: \n");
	while(curr != -1){
		//printf("%d ",count);
		print_spike_info(&spikes[curr]);		
		curr = spikes[curr].next;
		count++;
	}	
}

void client_sig_handler(int signum)
{
	if (b_exit) {
		log_msg("Test end (interrupted by signal %d)", signum);
		return;
	}

	// Just in case not Activity updates where logged add a '\n'
	if (user_params.packetrate_stats_print_ratio && !user_params.packetrate_stats_print_details && 
	   (user_params.packetrate_stats_print_ratio < packet_counter))
		printf("\n");

	switch (signum) {
	case SIGALRM:
		log_msg("Test end (interrupted by timer)");
		break;		
	case SIGINT:
		log_msg("Test end (interrupted by user)");
		break;
	default:
		log_msg("Test end (interrupted by signal %d)", signum);
		break;
	}

	if (clock_gettime(CLOCK_MONOTONIC, &end_time)) {
		log_err("clock_gettime()");
		exit(1);
	}
	if (!packet_counter) {
		if (user_params.stream_mode) {
			log_msg("No messages were sent");
		}
		else {
			log_msg("No messages were received from the server. Is the server down?");
		}
	}
	else {
		double usecTotalRunTime = TIME_DIFF_in_MICRO(start_time, end_time);

		if (user_params.stream_mode) {
			// Send only mode!
			printf(MODULE_NAME "Total of %lld messages sent in %.3lf sec", packet_counter, usecTotalRunTime/1000000);
			if (cycle_counter != packet_counter) {
				printf(", cycles counter = %lld\n", cycle_counter);
			}
			else {
				printf("\n");
			}
			if (usecTotalRunTime) {
				int ip_frags_per_msg = (user_params.msg_size + DEFAULT_IP_PAYLOAD_SZ - 1) / DEFAULT_IP_PAYLOAD_SZ;
				int mps = packet_counter / (unsigned long long)MICRO_TO_SEC(usecTotalRunTime);
				int pps = mps * ip_frags_per_msg;
				int total_line_ip_data = user_params.msg_size + ip_frags_per_msg*28;
				double MBps = ((double)mps * total_line_ip_data)/1024/1024; /* No including IP + UDP Headers per fragment */
				if (ip_frags_per_msg == 1)
					log_msg("Summary: Message Rate is %d [msg/sec]", mps);
				else
					log_msg("Summary: Message Rate is %d [msg/sec], Packet Rate is %d [pkt/sec] (%d ip frags / msg)", mps, pps, ip_frags_per_msg);
				log_msg("Summary: BandWidth is %.3f MBps (%.3f Mbps)", MBps, MBps*8);
			}
		}
		else {
			if (duplicate_packets_counter)
				log_msg("Warning: Mismatched packets counter = %d (Drops, Duplicates or Out of order)", duplicate_packets_counter);

			if (user_params.data_integrity) {
				if (data_integrity_failed)
					log_msg("Data integrity test failed!");
				else
					log_msg("Data integrity test succeeded");
			}

			// Default latency test mode
			double usecAvarageLatency = (usecTotalRunTime / (packet_counter * 2)) * user_params.burst_size;
			log_msg("Total %lld messages sent in %.3lf sec", packet_counter, usecTotalRunTime/1000000);
			print_average_latency(usecAvarageLatency);
			if (user_params.b_client_calc_details) {
				print_spikes_list();
				print_histogram();
			}
		}

		if (user_params.cycle_duration_nsec > 0 && !cycle_wait_loop_counter)
			log_msg("Warning: the value of the clients cycle duration might be too small (--cycle_duration=%lld usec)", 
				NANO_TO_MICRO(user_params.cycle_duration_nsec));
	}

	b_exit = true;
}

/* set the timer on client to the [-t sec] parameter given by user */
void set_client_timer(struct itimerval *timer)
{
	timer->it_value.tv_sec = user_params.sec_test_duration;
	timer->it_value.tv_usec = 0;
	timer->it_interval.tv_sec = 0;
	timer->it_interval.tv_usec = 0;
}

/* set the action taken when signal received */
void set_signal_action()
{
	sigact.sa_handler = user_params.mode ? sig_handler : client_sig_handler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;	

	sigaction(SIGINT, &sigact, NULL);
	
	if (user_params.mode == MODE_CLIENT)
		sigaction(SIGALRM, &sigact, NULL);
}

/* set the timeout of select*/
void set_select_timeout(int time_out_msec)
{
	if (!user_params.select_timeout) {
		user_params.select_timeout = (struct timeval*)malloc(sizeof(struct timeval));
		if (!user_params.select_timeout) {
			log_err("Failed to allocate memory for pointer select timeout structure");
			exit(1);
		}		
	}
	if (time_out_msec >= 0) {
		// Update timeout
		user_params.select_timeout->tv_sec = time_out_msec/1000;
		user_params.select_timeout->tv_usec = 1000 * (time_out_msec - user_params.select_timeout->tv_sec*1000);
	}
	else {
		// Clear timeout
		free(user_params.select_timeout);			
		user_params.select_timeout = NULL;
	}	
}

void set_defaults()
{       
	memset(&user_params, 0, sizeof(struct user_params_t));
	memset(fds_array, 0, sizeof(fds_data*)*MAX_FDS_NUM);    
	user_params.rx_mc_if_addr.s_addr = htonl(INADDR_ANY);
	user_params.tx_mc_if_addr.s_addr = htonl(INADDR_ANY);
	user_params.sec_test_duration = DEFAULT_TEST_DURATION;
	user_params.msg_size = MIN_PAYLOAD_SIZE;
	user_params.mode = MODE_SERVER;
	user_params.packetrate_stats_print_ratio = 0;
	user_params.packetrate_stats_print_details = false;
	user_params.burst_size	= 1;
	user_params.data_integrity = false;
	user_params.b_client_calc_details = false;
	user_params.fd_handler_type = RECVFROM;
	user_params.stream_mode = false;
	user_params.mthread_server = 0;
	user_params.msg_size_range = 0;
	user_params.udp_buff_size = UDP_BUFF_DEFAULT_SIZE;
	set_select_timeout(DEFAULT_SELECT_TIMEOUT_MSEC);
	last_tv.tv_sec  = 0; last_tv.tv_usec = 0;
	curr_tv.tv_sec  = 0; curr_tv.tv_usec = 0;	
	user_params.threads_num = 1;
	user_params.is_blocked = true;
	user_params.do_warmup = true;
	user_params.pre_warmup_wait = 0;
	user_params.is_vmarxfiltercb = false;
	user_params.is_vmazcopyread = false;
	user_params.cycle_duration_nsec = 0;	
	debug_level = LOG_LVL_INFO;
	user_params.mc_loop_disable = false;
	user_params.client_work_with_srv_num = 1;
	user_params.b_server_reply_via_uc = false;
}

/* write a pattern to buffer */
void write_pattern(unsigned char *buf, int buf_size)
{
	int len = 0;
	srand((unsigned)time(NULL));
	for (len = 0; len < buf_size; len++)
		buf[len] = (char)(rand() % 128);
}

/* returns 1 if buffers are identical */
static inline int check_data_integrity(unsigned char *pattern_buf, size_t buf_size)
{
	/*static int to_print = 1;
	if (to_print == 1) {
		printf("%s\n", rcvd_buf);
		to_print = 0;
	}*/
#ifdef USING_VMA_EXTRA_API
	if (dgram) {
		size_t i, pos, len;

		((char*)dgram->iov[0].iov_base)[1] = CLIENT_MASK; /*match to client so data_integrity will pass*/

		pos = 0;
		for (i = 0; i < dgram->sz_iov; ++i) {
			len = dgram->iov[i].iov_len;
			
			if (buf_size < pos + len ||
			    memcmp((char*)dgram->iov[i].iov_base, 
			           (char*)pattern_buf + pos, len)) {
				return 0;
			}
			pos += len;
		}
		return pos == buf_size;
	} else {
		printf("dgram is NULL\n");
	}
#endif
	msgbuf[1] = CLIENT_MASK; /*match to client so data_integrity will pass*/
	return !memcmp((char*)msgbuf,(char*)pattern_buf, buf_size);

}
/* get IP:port pairs from the file and initialize the list */
int set_mcgroups_fromfile(char *mcg_filename)
{
	FILE *file_fd = NULL;
	char line[MAX_MCFILE_LINE_LENGTH];
	char *res;
	char *ip;
	char *port;
	fds_data *tmp;
	int curr_fd = 0, last_fd = 0;
	int regexpres;  

	if ((file_fd = fopen(mcg_filename, "r")) == NULL) {
		printf("No such file: %s \n", mcg_filename);
		exit(4);
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
		sockets_num++;

		regexpres = regcomp(&regexpr, IP_PORT_FORMAT_REG_EXP, REG_EXTENDED|REG_NOSUB);
		if (regexpres) {
			log_err("Failed to compile regexp");
			exit(1);
		}
		regexpres = regexec(&regexpr, line, (size_t)0, NULL, 0);
		regfree(&regexpr);
		if (regexpres) {
			log_msg("Invalid input in line %d: "
				"each line must have the following format: ip:port",
				sockets_num);
			exit(1);
		}

		ip = strtok(line, ":");
		port = strtok(NULL, ":\n");
		if (!ip || !port) {
			log_msg("Invalid input in line %d: "
				"each line must have the following format: ip:port", 
				sockets_num);
			exit(8);
		}
		tmp = (struct fds_data *)malloc(sizeof(struct fds_data));
		if (!tmp) {
			log_err("Failed to allocate memory with malloc()");
			exit(1);
		}
		memset(tmp,0,sizeof(struct fds_data));
		tmp->addr.sin_family = AF_INET;
		tmp->addr.sin_port = htons(atoi(port));
		if (!inet_aton(ip, &tmp->addr.sin_addr)) {
			log_msg("Invalid input in line %d: '%s:%s'", sockets_num, ip, port);
			exit(8);
		}
		tmp->is_multicast = IN_MULTICAST(ntohl(tmp->addr.sin_addr.s_addr));
		curr_fd = prepare_socket(&tmp->addr);
		if (sockets_num != 1) { /*it is not the first fd*/
			fds_array[last_fd]->next_fd = curr_fd;
		}
		else {
			fd_min = curr_fd;
		}               
		last_fd = curr_fd;
		fds_array[curr_fd] = tmp;
		fd_max = max(fd_max, curr_fd);
		fd_min = min(fd_min, curr_fd);
		fd_num++;
	}

	fds_array[fd_max]->next_fd = fd_min; /* close loop for fast wrap around in client */

	fclose(file_fd);
	return 0;
}

#ifdef  USING_VMA_EXTRA_API
extern vma_recv_callback_retval_t myapp_vma_recv_pkt_filter_callback(int fd, size_t iov_sz, struct iovec iov[], struct vma_info_t* vma_info, void *context);
#endif

/* returns the new socket fd
 or exit with error code */
int prepare_socket(struct sockaddr_in* p_addr)
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
		exit(1);
	}

	if (!user_params.is_blocked) {
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

	/* allow multiple sockets to use the same PORT number */
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_true, sizeof(reuseaddr_true)) < 0) {
		log_err("setsockopt(SO_REUSEADDR) failed");
		exit(1);
	}

	if (user_params.udp_buff_size > 0) {
		/* enlarge socket's buffer depth */

		if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &(user_params.udp_buff_size), sizeof(user_params.udp_buff_size)) < 0) {
			log_err("setsockopt(SO_RCVBUF) failed");
			exit(1);
		}
		if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcv_buff_size,(socklen_t *)&size) < 0) {
			log_err("getsockopt(SO_RCVBUF) failed");
			exit(1);
		}
		if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &(user_params.udp_buff_size), sizeof(user_params.udp_buff_size)) < 0) {
			log_err("setsockopt(SO_SNDBUF) failed");
			exit(1);
		}
		if (getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &snd_buff_size, (socklen_t *)&size) < 0) {
			log_err("getsockopt(SO_SNDBUF) failed");
			exit(1);
		}

		log_msg("UDP buffers sizes of fd %d: RX: %d Byte, TX: %d Byte", fd, rcv_buff_size, snd_buff_size);              
		if (rcv_buff_size < user_params.udp_buff_size*2 || 
		    snd_buff_size < user_params.udp_buff_size*2  ) {
			log_msg("WARNING: Failed setting receive or send udp socket buffer size to %d bytes (check 'sysctl net.core.rmem_max' value)", user_params.udp_buff_size);
		}
	}

	memset(&bind_addr, 0, sizeof(struct sockaddr_in));
	bind_addr.sin_family = AF_INET;
	bind_addr.sin_port = p_addr->sin_port;
	bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	/*log_dbg ("IP to bind: %s",inet_ntoa(client_addr.sin_addr));*/
	if (bind(fd, (struct sockaddr*)&bind_addr, sizeof(struct sockaddr)) < 0) {
		log_err("bind()");
		exit(1);
	}

	if (is_mulicast) {
		struct ip_mreq mreq;
		memset(&mreq,0,sizeof(struct ip_mreq));

		/* use setsockopt() to request that the kernel join a multicast group */
		/* and specify a specific interface address on which to receive the packets of this socket */
		/* NOTE: we don't do this if case of client (sender) in stream mode */
		if (!user_params.stream_mode || user_params.mode != MODE_CLIENT) {
			mreq.imr_multiaddr = p_addr->sin_addr;
			mreq.imr_interface.s_addr = user_params.rx_mc_if_addr.s_addr;
			if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
				log_err("setsockopt(IP_ADD_MEMBERSHIP)");
				exit(1);
			}
		}

		/* specify a specific interface address on which to transmitted the multicast packets of this socket */
		if (user_params.tx_mc_if_addr.s_addr != INADDR_ANY) {
			if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &user_params.tx_mc_if_addr, sizeof(user_params.tx_mc_if_addr)) < 0) {
				log_err("setsockopt(IP_MULTICAST_IF)");
				exit(1);
			}
		}

		if (user_params.mc_loop_disable) {
			/* disable multicast loop of all transmitted packets */
			u_char loop_disabled = 0;
			if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop_disabled, sizeof(loop_disabled)) < 0) {
				log_err("setsockopt(IP_MULTICAST_LOOP)");
				exit(1);
			}
		}
	}

#ifdef  USING_VMA_EXTRA_API
	if (user_params.is_vmarxfiltercb && vma_api) {
		// Try to register application with VMA's special receive notification callback logic
		if (vma_api->register_recv_callback(fd, myapp_vma_recv_pkt_filter_callback, &fd) < 0) {
			log_err("vma_api->register_recv_callback failed. Try running without option 'vmarxfiltercb'");
		}
		else {
			log_dbg("vma_api->register_recv_callback successful registered");
		}
	}
#endif

	return fd;
}

void prepare_network(int fd_min, int fd_max, int fd_num,fd_set *p_s_readfds,struct pollfd **poll_fd_arr,struct epoll_event **epoll_events, int *epfd)
{
	int ifd, fd_count = 0;
	struct epoll_event ev;	
	
	if (read_from_file == 0) {
		printf(" IP = %s PORT = %d\n", inet_ntoa(fds_array[fd_min]->addr.sin_addr), ntohs(fds_array[fd_min]->addr.sin_port)); 
	}
	else {
		int list_count = 0;
		switch (user_params.fd_handler_type) {
		case SELECT:
			FD_ZERO(p_s_readfds); 
			break;
		case POLL:      
			*poll_fd_arr = (struct pollfd *)malloc(fd_num * sizeof(struct pollfd));
			if (!*poll_fd_arr) {
				log_err("Failed to allocate memory for poll fd array");
				exit(1);
			}
			break;
		case EPOLL:
			*epoll_events = (struct epoll_event *)malloc(sizeof(struct epoll_event)*fd_num);
			if (!*epoll_events) {
				log_err("Failed to allocate memory for epoll event array");
				exit(1);
			}
			break;
		default:
			break;
		}                               

		printf("\n");
		for (ifd = fd_min; ifd <= fd_max; ifd++) {
			if (fds_array[ifd]) {
				printf("[%2d] IP = %-15s PORT = %5d\n", list_count++, inet_ntoa(fds_array[ifd]->addr.sin_addr), ntohs(fds_array[ifd]->addr.sin_port));

				switch (user_params.fd_handler_type) {
				case SELECT:
					FD_SET(ifd, p_s_readfds);  
					break;
				case POLL:      
					(*poll_fd_arr)[fd_count].fd = ifd;
					(*poll_fd_arr)[fd_count].events = POLLIN | POLLPRI;
					break;
				case EPOLL:
					ev.events = EPOLLIN | EPOLLPRI;
					ev.data.fd = ifd;
					epoll_ctl(*epfd, EPOLL_CTL_ADD, ev.data.fd, &ev);
					break;
				default:
					break;
				}                               
				fd_count++;
			}
		}
	}
}

static inline int msg_recvfrom(int fd, struct sockaddr_in *recvfrom_addr)
{
	int ret = 0;
	socklen_t size = sizeof(struct sockaddr);

	//log_msg("Calling recvfrom with FD %d", fd);

#ifdef  USING_VMA_EXTRA_API

	if (user_params.is_vmazcopyread && vma_api) {
		int flags = 0;

		// Free VMA's previously received zero copied datagram
		if (dgram) {
			vma_api->free_datagrams(fd, &dgram->datagram_id, 1);
			dgram = NULL;
		}

		// Receive the next datagram with zero copy API
		ret = vma_api->recvfrom_zcopy(fd, dgram_buf, max_buff_size,
		                                  &flags, (struct sockaddr*)recvfrom_addr, &size);
		if (ret >= 2) {
			if (flags & MSG_VMA_ZCOPY) {
				// zcopy
				dgram = (struct vma_datagram_t*)dgram_buf;

				// copy signature
				msgbuf[0] = ((uint8_t*)dgram->iov[0].iov_base)[0];
				msgbuf[1] = ((uint8_t*)dgram->iov[0].iov_base)[1];
			}
			else {
				// copy signature
				msgbuf[0] = dgram_buf[0];
				msgbuf[1] = dgram_buf[1];
			}

		}
	}
	else
#endif
	{
		ret = recvfrom(fd, msgbuf, user_params.msg_size, 0, (struct sockaddr*)recvfrom_addr, &size);
	}

	if (ret < 2 && errno != EAGAIN && errno != EINTR) {
		log_err("recvfrom() Failed receiving on fd[%d]", fd);
		exit(1);
	}

	//log_msg("Data received from fd=%d, ret=%d", fd, ret);
	return ret;
}

static inline int msg_sendto(int fd, uint8_t* buf, int nbytes, struct sockaddr_in *sendto_addr)
{
	//log_msg("Sending on fd[%d] to %s:%d msg size of %d bytes", fd, inet_ntoa(sendto_addr->sin_addr), ntohs(sendto_addr->sin_port), nbytes);
	int ret = sendto(fd, buf, nbytes, 0, (struct sockaddr*)sendto_addr, sizeof(struct sockaddr));
	if (ret < 0 && errno && errno != EINTR) {
		log_err("sendto() Failed sending on fd[%d] to %s:%d msg size of %d bytes", fd, inet_ntoa(sendto_addr->sin_addr), ntohs(sendto_addr->sin_port), nbytes);
		exit(1);
	}
	//log_msg("Done sending");
	return ret;
}

void warmup()
{
	if (!user_params.do_warmup)
		return;

	log_msg("Warmup stage (sending a few dummy packets)...");
	int ifd, count;
	for (ifd = fd_min; ifd <= fd_max; ifd++) {
		if (fds_array[ifd] && fds_array[ifd]->is_multicast) {
			struct sockaddr_in sendto_addr = fds_array[ifd]->addr;
			sendto_addr.sin_port = htons(DUMMY_PORT);

			for (count=0; count<2; count++) {
				msg_sendto(ifd, pattern, user_params.msg_size, &sendto_addr);
			}
		}
	}
}

#ifdef  USING_VMA_EXTRA_API
vma_recv_callback_retval_t myapp_vma_recv_pkt_filter_callback(
	int fd, size_t iov_sz, struct iovec iov[], struct vma_info_t* vma_info, void *context)
{
	if (iov_sz) {};
	if (context) {};

	// Check info structure version
	if (vma_info->struct_sz < sizeof(struct vma_info_t)) {
		log_msg("VMA's info struct is not something we can handle so un-register the application's callback function");
		vma_api->register_recv_callback(fd, NULL, &fd);
		return VMA_PACKET_RECV;
	}

	int recvsize     = iov[0].iov_len;
	uint8_t* recvbuf = iov[0].iov_base;

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
	if (user_params.mode != MODE_BRIDGE) {
		if (recvbuf[1] != CLIENT_MASK) {
			if (user_params.mc_loop_disable)
				log_err("got != CLIENT_MASK");
			return VMA_PACKET_DROP;
		}
		recvbuf[1] = SERVER_MASK;
	}
	if (!user_params.stream_mode) {
		/* get source addr to reply to */
		struct sockaddr_in sendto_addr = fds_array[fd]->addr;
		if (!fds_array[fd]->is_multicast)  /* In unicast case reply to sender*/
			sendto_addr.sin_addr = vma_info->src->sin_addr;
		msg_sendto(fd, recvbuf, recvsize, &sendto_addr);
	}

	packet_counter++;
	if ((user_params.packetrate_stats_print_ratio > 0) && ((packet_counter % user_params.packetrate_stats_print_ratio) == 0)) {
		print_activity_info(packet_counter);
	}

	return VMA_PACKET_DROP;
}
#endif

/*
** Check that msg arrived from CLIENT and not a loop from a server
** Return 0 for successful
**        -1 for failure
*/
static inline int server_prepare_msg_reply()
{
	uint8_t* msg_mode_mask = NULL;
	if (user_params.mode != MODE_BRIDGE) {
		msg_mode_mask = &msgbuf[1];

		if (*msg_mode_mask != CLIENT_MASK)
			return -1;

		*msg_mode_mask = SERVER_MASK;
	}
	return 0;
}

/*
** receive from and send to selected socket
*/
static inline void server_receive_then_send(int ifd)
{
	int nbytes;
	struct sockaddr_in recvfrom_addr;
	struct sockaddr_in sendto_addr;
	
	nbytes = msg_recvfrom(ifd, &recvfrom_addr);
	if (b_exit) return;
	if (nbytes < 0) return;

	if (server_prepare_msg_reply()) return;

	if (!user_params.stream_mode) {
		/* get source addr to reply to */
		sendto_addr = fds_array[ifd]->addr;
		if (!fds_array[ifd]->is_multicast || user_params.b_server_reply_via_uc) {/* In unicast case reply to sender*/
			sendto_addr.sin_addr = recvfrom_addr.sin_addr;
		}
		msg_sendto(ifd, msgbuf, nbytes, &sendto_addr);
	}

	packet_counter++;
	if ((user_params.packetrate_stats_print_ratio > 0) && ((packet_counter % user_params.packetrate_stats_print_ratio) == 0)) {
		print_activity_info(packet_counter);
	}

	return;
}

void devide_fds_arr_between_threads(int *p_num_of_remainded_fds, int *p_fds_arr_len) {
	
	*p_num_of_remainded_fds = sockets_num%user_params.threads_num;	
	*p_fds_arr_len = sockets_num/user_params.threads_num;	
}

void find_min_max_fds(int start_look_from, int len, int* p_fd_min, int* p_fd_max) {
	int num_of_detected_fds;
	int i;
	
	for(num_of_detected_fds = 0, i = start_look_from; num_of_detected_fds < len;i++) {
		if (fds_array[i]) {
			if (!num_of_detected_fds) {
				*p_fd_min = i;
			}
			num_of_detected_fds++;
		}
	}	
	*p_fd_max = i - 1;
}

void server_handler(int fd_min, int fd_max, int fd_num)
{
	int epfd;
	int save_timeout_sec = 0; 
	int save_timeout_usec = 0;
	int timeout_msec = -1; 
	fd_set s_readfds, save_fds;
	int res = 0;
	int ifd, look_start = 0, look_end = 0;
	int fd_handler_type = user_params.fd_handler_type;
	char *to_array=(char *)malloc(20*sizeof(char));	
	struct pollfd *poll_fd_arr = NULL;
	struct epoll_event *epoll_events = NULL; 
	
	log_dbg("thread %d: fd_min: %d, fd_max : %d, fd_num: %d", gettid(), fd_min, fd_max,fd_num);

	if (user_params.mode == MODE_BRIDGE) {
		sprintf(to_array, "%s", inet_ntoa(user_params.tx_mc_if_addr));
		printf(MODULE_NAME "[BRIDGE] transferring packets from %s to %s on:", inet_ntoa(user_params.rx_mc_if_addr), to_array);
	}
	else {
		printf(MODULE_NAME "[SERVER] listen on:");
	}	
	
	if (fd_handler_type == EPOLL) {		
		epfd = epoll_create(fd_num);
	}
	prepare_network(fd_min, fd_max, fd_num, &s_readfds,&poll_fd_arr,&epoll_events,&epfd);
	memcpy(&save_fds, &s_readfds, sizeof(fd_set));

        sleep(user_params.pre_warmup_wait);
	
	warmup();

	log_msg("[tid %d] using %s() to block on socket(s)", gettid(), fds_handle_desc[user_params.fd_handler_type]);

	switch (fd_handler_type) {
	case RECVFROM:
		res = 1;
		look_start = fd_min;
		look_end = fd_min+1;
		break;
	case SELECT:
		if (user_params.select_timeout) {
			save_timeout_sec = user_params.select_timeout->tv_sec;
			save_timeout_usec = user_params.select_timeout->tv_usec;
		}		
		look_start = fd_min;
		look_end = fd_max+1;
		break;
	case POLL:      
		if (user_params.select_timeout) {
			timeout_msec = user_params.select_timeout->tv_sec * 1000 + user_params.select_timeout->tv_usec / 1000;
		}
		look_end = fd_num;
		break;
	case EPOLL:
		if (user_params.select_timeout) {
			timeout_msec = user_params.select_timeout->tv_sec * 1000 + user_params.select_timeout->tv_usec / 1000;
		}
		look_end = 0; 
		break;
	}           

	/*
	** SERVER LOOP
	*/
	while (!b_exit) {

		switch (fd_handler_type) {
		case RECVFROM:
			break;
		case SELECT:
			if (user_params.select_timeout) {
				user_params.select_timeout->tv_sec = save_timeout_sec;
				user_params.select_timeout->tv_usec = save_timeout_usec;
			}
			memcpy(&s_readfds, &save_fds, sizeof(fd_set));
			res = select(fd_max+1, &s_readfds, NULL, NULL, user_params.select_timeout);
			break;
		case POLL:      
			res = poll(poll_fd_arr, fd_num, timeout_msec);
			break;
		case EPOLL:
			res = epoll_wait(epfd, epoll_events, fd_num, timeout_msec);
			look_end = res; 
			break;
		}           

		if (b_exit) continue;
		if (res < 0) {
			log_err("%s()", fds_handle_desc[user_params.fd_handler_type]);
			exit(1);
		}
		if (res == 0) {
			if (!user_params.select_timeout)
				log_msg("Error: %s() returned without fd ready", fds_handle_desc[user_params.fd_handler_type]);
			continue;
		}

		for (ifd = look_start; ifd < look_end; ifd++) {
			switch (fd_handler_type) {
			case RECVFROM:
				server_receive_then_send(ifd);
				break;
			case SELECT:
				if (FD_ISSET(ifd, &s_readfds)) {
					server_receive_then_send(ifd);
				}
				break;
			case POLL:      
				if ((poll_fd_arr[ifd].revents & POLLIN) || (poll_fd_arr[ifd].revents & POLLPRI)) {
					server_receive_then_send(poll_fd_arr[ifd].fd);
				}
				break;
			case EPOLL:
				server_receive_then_send(epoll_events[ifd].data.fd);
				break;
			}
		}
	}
	if (to_array != NULL) {
		free(to_array);	
	}
	switch (user_params.fd_handler_type) {
	case POLL:   
		if (poll_fd_arr) {
			free(poll_fd_arr);
		}			
		break;
	case EPOLL:
		close(epfd);
		free(epoll_events);
		break;
	default:
		break;
	}

	log_dbg("thread %d released allocations",gettid());
	
	if (!user_params.mthread_server) {
		log_msg("%s() exit", __func__);
		cleanup();
	}
}

void *server_handler_for_multi_threaded(void *arg)
{      
	int fd_min;
	int fd_max;
	int fd_num;
	sub_fds_arr_info *p_sub_fds_arr_info = (sub_fds_arr_info*)arg; 
	
	fd_min = p_sub_fds_arr_info->fd_min;
	fd_max = p_sub_fds_arr_info->fd_max;
	fd_num = p_sub_fds_arr_info->fd_num;	
	server_handler(fd_min, fd_max, fd_num);
	if (p_sub_fds_arr_info != NULL){
		free(p_sub_fds_arr_info);
	}
	return 0;
}

void server_select_per_thread()
{
	int i;
	pthread_t tid;	
	int fd_num;
	int num_of_remainded_fds;
	int last_fds = 0;
	
	pid_arr[0] = gettid();
	devide_fds_arr_between_threads(&num_of_remainded_fds, &fd_num);
	
	for (i = 0; i < user_params.threads_num; i++) {
		sub_fds_arr_info *thread_fds_arr_info = (sub_fds_arr_info*)malloc(sizeof(sub_fds_arr_info));
		if (!thread_fds_arr_info) {
			log_err("Failed to allocate memory for sub_fds_arr_info");
			exit(1);
		}
		thread_fds_arr_info->fd_num = fd_num;
		if (num_of_remainded_fds) {
			thread_fds_arr_info->fd_num++;
			num_of_remainded_fds--;
		}
		find_min_max_fds(last_fds, thread_fds_arr_info->fd_num, &(thread_fds_arr_info->fd_min), &(thread_fds_arr_info->fd_max));
		pthread_create(&tid, 0, server_handler_for_multi_threaded, (void *)thread_fds_arr_info);
		pid_arr[i + 1] = tid;
		last_fds = thread_fds_arr_info->fd_max + 1;
	}
	while (!b_exit) {
		sleep(1);
	}	
	for (i = 1; i <= user_params.threads_num; i++) {
		pthread_kill(pid_arr[i], SIGINT);
		pthread_join(pid_arr[i], 0);
	}
	log_msg("%s() exit", __func__);
	cleanup();	
}

void make_empty_spikes_list()
{
	spikes_lst.head = -1;
	spikes_lst.tail = -1;
	spikes=(spike *)malloc(max_spikes_num*sizeof(spike));
	if (!spikes) {
		log_err("Failed to allocate memory for list of highest spikes");
		exit(1);
	}
}

static inline void insert_node_to_begining_of_spikes_lst(unsigned int node_index)
{
	spikes[node_index].next = spikes_lst.head;
	spikes_lst.head = node_index;
}

static inline void insert_node_to_end_of_spikes_lst(unsigned int node_index)
{
	spikes[spikes_lst.tail].next = node_index;
	spikes[node_index].next = -1;
	spikes_lst.tail = node_index;
}

static inline void insert_node_to_middle_of_spikes_lst(unsigned int prev_node, unsigned int node_index)
{
	int temp = spikes[prev_node].next;
	spikes[prev_node].next = node_index;	
	spikes[node_index].next = temp;
}

static inline int is_spikes_list_empty()
{
	return (spikes_lst.head == -1 &&  spikes_lst.tail == -1);	
}

static inline void delete_node_from_spikes_lst_head()
{
	int temp;
	if(!is_spikes_list_empty()){
		if(spikes_lst.head == spikes_lst.tail){ //list of one node
			spikes_lst.head = -1;
			spikes_lst.tail = -1;
		}
		else{
			temp = spikes_lst.head;
			spikes_lst.head = spikes[spikes_lst.head].next;
			spikes[temp].next = -1;
		}
	}	
}

static inline void insert_node_2_empty_spikes_list(unsigned int node_index)
{
	spikes_lst.head = node_index;
	spikes_lst.tail = node_index;
}

static inline void insert_new_spike_vals_2_list_node(unsigned int location, unsigned int usec, unsigned long long packet_counter)
{
	spikes[location].packet_counter_at_spike = packet_counter;
	spikes[location].usec = usec;
	spikes[location].next = -1;		
}

static inline void locate_node_in_spikes_list(unsigned int node_index)
{
	int usec_val = spikes[node_index].usec;
	int prev_node = spikes_lst.head;
	int curr_node = spikes_lst.head;	
	
	if (is_spikes_list_empty()){
		insert_node_2_empty_spikes_list(node_index);
	}
	else{
		while(curr_node != -1 ){
			if(usec_val > spikes[curr_node].usec){
				prev_node = curr_node;
				curr_node = spikes[curr_node].next;
			}
			else{
				break;
			}				
		}
		
		if(curr_node == spikes_lst.head){
			insert_node_to_begining_of_spikes_lst(node_index);
		}
		else if(curr_node == -1){
			insert_node_to_end_of_spikes_lst(node_index);
		}
		else{
			insert_node_to_middle_of_spikes_lst(prev_node, node_index);
		}
	}	
}

static inline void update_spikes_list(unsigned int usec, unsigned long long packet_counter)
{
	unsigned int location;
	
	if(spikes_num < max_spikes_num){
		location = spikes_num;
		spikes_num++;		
	}
	else{
		if(usec <= spikes[spikes_lst.head].usec)
			return;
		else{
			location = spikes_lst.head;
			delete_node_from_spikes_lst_head();			
		}		
	}	
	insert_new_spike_vals_2_list_node(location,usec, packet_counter);	
	locate_node_in_spikes_list(location);
}

static inline void calc_round_trip_times_details(double i_rtt, unsigned long long i_packet_counter)
{
	int pos_usec = 0;
	
	double latency_usec = i_rtt / 2;	
	update_spikes_list(latency_usec, i_packet_counter);
	for (pos_usec = 1; pos_usec < latency_hist_size; pos_usec++) {
		if (latency_usec < latency_hist[pos_usec].min_usec) { 
			latency_hist[pos_usec-1].count++;
			break;
		}
	}	
}

static inline void client_receive_from_selected(int ifd, int time_stamp_index)
{
	int i;

	for(i = 0; i < user_params.client_work_with_srv_num; i++) {
		int nbytes = 0;
		int recived_legal_message = 0;
		struct sockaddr_in recvfrom_addr;

		do {
			if (b_exit) return;

			nbytes = msg_recvfrom(ifd, &recvfrom_addr);
			if (b_exit) return;
			if (nbytes < 0) continue;

			/* log_dbg("Received from: FD = %d; IP = %s; PORT = %d", ifd, inet_ntoa(recvfrom_addr.sin_addr), ntohs(recvfrom_addr.sin_port));*/
			if (nbytes != user_params.msg_size) {
				log_msg("received message size test failed (sent:%d received:%d)", user_params.msg_size, nbytes);
				exit(16);
			}

			if (msgbuf[1] != SERVER_MASK) {
				if (user_params.mc_loop_disable)
					log_err("got != SERVER_MASK");
				continue;
			}

			if (pattern[0] != msgbuf[0]){
				//log_dbg("duplicate message recieved expected=%d, recieved=%d",pattern[0], msgbuf[0] );
				duplicate_packets_counter++;
				continue;
			}

			recived_legal_message = 1;

		} while (recived_legal_message == 0);

		if (user_params.data_integrity && !check_data_integrity(pattern, user_params.msg_size)) {
			data_integrity_failed = 1;
			log_msg("data integrity test failed");
			exit(16);
		}
	}
	if (user_params.b_client_calc_details) {
		if (clock_gettime(CLOCK_MONOTONIC, &end_round_time)) {
			log_err("clock_gettime()");
			exit(1);
		}
		rtt_data[time_stamp_index].rtt = TIME_DIFF_in_MICRO(rtt_data[time_stamp_index].start_round_time, end_round_time);
	}
}

static inline void client_inc_sequnce_counter()
{
	if (pattern[0] == 0xff)
		pattern[0] = 0;
	pattern[0]++;
}

static inline void client_update_counters(unsigned int* p_recived_packets_num, int packet_cnt_index)
{
	int recived_packets_num = *p_recived_packets_num;
	recived_packets_num++;
	packet_counter++;
	if (packet_counter >= (UINT64_MAX-1000000)) {
		log_err("Error: counter overflow");
	}
	client_inc_sequnce_counter();
	*p_recived_packets_num = recived_packets_num;
	if (user_params.b_client_calc_details) {
		packet_counter_arr[packet_cnt_index] = packet_counter;
	}
}

static inline void client_send_packet(int ifd, int i)
{
	struct timespec start_round_time; 
	
	if (user_params.b_client_calc_details){
		if (clock_gettime(CLOCK_MONOTONIC, &start_round_time)) {
			log_err("clock_gettime()");
			exit(1);
		}
		rtt_data[i].start_round_time = start_round_time;
	}
	msg_sendto(ifd, pattern, user_params.msg_size, &(fds_array[ifd]->addr));
}

static inline unsigned int client_recieve(int ifd, int time_stamp_index)
{
	fd_set s_readfds;
	int res = 0;
	struct timeval timeout_timeval = {0, 0};
	struct timeval* p_timeout_timeval = NULL;
	int timeout_msec = -1; 
	int look_start = 0, look_end = 0;
	int fd_handler_type = user_params.fd_handler_type;
	int ready_fds_found = 0;
	int fd = 0;
	unsigned int recived_packets_num = 0;
	
	switch (fd_handler_type) {
	case RECVFROM: /*recive only from the file descriptor which function got as input parameter : ifd */
		res = 1;
		look_start = ifd;
		look_end = ifd+1;
		break;
	case SELECT:
		if (user_params.select_timeout) {
			p_timeout_timeval = &timeout_timeval;
		}		
		look_start = fd_min;
		look_end = fd_max+1;
		break;
	case POLL:      
		if (user_params.select_timeout) {
			timeout_msec = user_params.select_timeout->tv_sec * 1000 + user_params.select_timeout->tv_usec / 1000;
		}
		look_end = fd_num;
		break;
	case EPOLL:
		if (user_params.select_timeout) {
			timeout_msec = user_params.select_timeout->tv_sec * 1000 + user_params.select_timeout->tv_usec / 1000;
		}
		look_end = 0; 
		break;
	} 

	if ((user_params.packetrate_stats_print_ratio > 0) && ((packet_counter % user_params.packetrate_stats_print_ratio) == 0)) {
		print_activity_info(packet_counter);
	}

	do {
		switch (fd_handler_type) {      
		case RECVFROM:
			ready_fds_found = 1;
			continue;
			break;
		case SELECT:
			if (user_params.select_timeout) {
				timeout_timeval = *user_params.select_timeout;
			}
			memcpy(&s_readfds, &readfds, sizeof(fd_set));
			res = select(fd_max+1, &s_readfds, NULL, NULL, p_timeout_timeval);
			break;
		case POLL:      
			res = poll(poll_fd_arr, fd_num, timeout_msec);
			break;
		case EPOLL:
			res = epoll_wait(epfd, epoll_events, fd_num, timeout_msec);
			look_end = res; 
			break;
		}

		if (b_exit) return recived_packets_num;
		if (res < 0) {
			log_err("%s()", fds_handle_desc[user_params.fd_handler_type]);
			exit(1);
		}
		if (res == 0) {
			if (!user_params.select_timeout)
				log_msg("Error: %s() returned without fd ready", fds_handle_desc[user_params.fd_handler_type]);
			continue;
		}
		ready_fds_found = 1;

	} while (ready_fds_found == 0);  

	/* ready fds were found so receive from the relevant sockets*/

	for (fd = look_start; fd < look_end; fd++) {
		switch (fd_handler_type) {
		case RECVFROM:
			client_receive_from_selected(fd, time_stamp_index);
			client_update_counters(&recived_packets_num,time_stamp_index);
			break;
		case SELECT:
			if (FD_ISSET(fd, &s_readfds)) {
				client_receive_from_selected(fd, time_stamp_index);
				client_update_counters(&recived_packets_num,time_stamp_index);
		    }
			break;
		case POLL:
			if ((poll_fd_arr[fd].revents & POLLIN) || (poll_fd_arr[fd].revents & POLLPRI)) {
				client_receive_from_selected(poll_fd_arr[fd].fd, time_stamp_index);
				client_update_counters(&recived_packets_num,time_stamp_index);
			}
			break;
		case EPOLL:
			client_receive_from_selected(epoll_events[fd].data.fd, time_stamp_index);
			client_update_counters(&recived_packets_num,time_stamp_index);
			break;
		}		
	}
	return recived_packets_num;
}

static inline void client_update_msg_size()
{
	if (user_params.msg_size_range > 0) {
		user_params.msg_size = min(MAX_PAYLOAD_SIZE, (min_msg_size + (int)(rand() % (user_params.msg_size_range))));
		//log_dbg("sending message size: %d",user_params.msg_size);
	}
}

static inline void calc_round_trip_times_details_of_burst()
{
	unsigned int i;
	for (i = 0; i < user_params.burst_size; i++) {
		calc_round_trip_times_details(rtt_data[i].rtt, packet_counter_arr[i]);
	}
}


/*
** busy wait between two cycles starting point and take starting point of next cycle
*/
static inline void cycle_duration_wait()
{
	long long delta;
	struct timespec et;

	while (!b_exit) {
		if (clock_gettime(CLOCK_MONOTONIC, &et) < 0)
			log_err("Error: clock_gettime failed");

		//delta = (long long)(et.tv_sec) * 1000000000 + (long long)(et.tv_nsec)  - cycle_start_time_nsec - user_params.cycle_duration_nsec;	        
		delta = TS_TO_NANO(&et) - cycle_start_time_nsec - user_params.cycle_duration_nsec;
		if (delta >= 0) {
			/*long long int end_of_cycle = TS_TO_NANO(&et);
			log_msg("end of cycle #%lld %llu\n",cycle_counter,end_of_cycle); */
			break;
		}
		cycle_wait_loop_counter++;
		if (!cycle_wait_loop_counter)
			log_err("Error: cycle_wait_loop_counter overflow");
	}
	cycle_start_time_nsec += user_params.cycle_duration_nsec;
	//log_msg("start of cycle #%lld %llu\n",cycle_counter,cycle_start_time_nsec);
}

/*
** send to and recive from selected socket
*/
static inline void client_send_then_receive(int ifd)
{
	int starting_pattern_val = 0;
	unsigned int i;
	
	client_update_msg_size();
	client_inc_sequnce_counter();
	starting_pattern_val = pattern[0];
	
	if (user_params.cycle_duration_nsec)
		cycle_duration_wait();
	
	if (user_params.stream_mode)
		++cycle_counter;

	for (i = 0; i < user_params.burst_size && !b_exit; i++) {			
		if (user_params.stream_mode)
			++packet_counter;

		//log_msg("%s() sending to FD %d", __func__, ifd);
		client_send_packet(ifd, i);
		client_inc_sequnce_counter();			
	}	
	if (user_params.stream_mode) {
		if ((user_params.packetrate_stats_print_ratio > 0) && ((packet_counter % user_params.packetrate_stats_print_ratio) == 0)) {
			print_activity_info(packet_counter);
		}
		return;
	}
		               
	pattern[0] = starting_pattern_val; 
	
	for (i = 0; i < user_params.burst_size && !b_exit; ) {
		//log_msg("%s() Done sending, Waiting to receive from FD %d", __func__, ifd);
		i += client_recieve(ifd, i);	
	}
	if (user_params.b_client_calc_details) {
		calc_round_trip_times_details_of_burst();
	}

	cycle_counter++;
	return;
}

void client_handler()
{
	int ret;
	struct itimerval timer;
	int curr_fds;
	fds_data *tmp; 
	      

	printf(MODULE_NAME "[CLIENT] send on:");
	prepare_network(fd_min, fd_max, fd_num, &readfds,&poll_fd_arr,&epoll_events,&epfd);
	if (b_exit) return;

	sleep(user_params.pre_warmup_wait);
	warmup();

	if (!user_params.stream_mode) {
		log_msg("using %s() to block on socket(s)", fds_handle_desc[user_params.fd_handler_type]);
	}

	sleep(2);
	if (b_exit) return;

	log_msg("Starting test...");

	gettimeofday(&last_tv, NULL);
	set_client_timer(&timer);
	ret = setitimer(ITIMER_REAL, &timer, NULL);
	if (ret) {
		log_err("setitimer()");
		exit(1);
	}

	if (clock_gettime(CLOCK_MONOTONIC, &start_time)) {
		log_err("clock_gettime()");
		exit(1);
	}
	start_round_time = start_time;

	struct timespec et;
	if (clock_gettime(CLOCK_MONOTONIC, &et) < 0)
		log_err("Error: clock_gettime failed");	
	cycle_start_time_nsec = TS_TO_NANO(&et) - user_params.cycle_duration_nsec;
	/* log_msg("%s() using %s", __func__, user_params.fds_handle_desc);*/
			
	curr_fds = fd_min;
	while (!b_exit) {
		tmp = fds_array[curr_fds];
		client_send_then_receive(curr_fds);		
		curr_fds = tmp->next_fd; /* cycle through all set fds in the array (with wrap around to beginning)*/
	}
	cleanup();
	return;
}

void update_min_max_msg_sizes()
{
	min_msg_size = max(MIN_PAYLOAD_SIZE, user_params.msg_size - user_params.msg_size_range);
	max_msg_size = min(MAX_PAYLOAD_SIZE, user_params.msg_size + user_params.msg_size_range);	
	user_params.msg_size_range = max_msg_size - min_msg_size + 1;
	log_msg("Message size range: [%d - %d]", min_msg_size, max_msg_size);	
}

void prepare_to_info_mode()
{
	if (rtt_data) {
		free(rtt_data);
		rtt_data = NULL;
	}	
	if (packet_counter_arr) {
		free(packet_counter_arr);
		packet_counter_arr = NULL;
	}
	
	rtt_data = (packet_rtt_data*)malloc(sizeof(packet_rtt_data)*user_params.burst_size);
	if(!rtt_data) {
		log_err("Failed to allocate memory for timestamps array");
		exit(1);
	}	
	packet_counter_arr = (unsigned long long *)malloc(sizeof(double)*user_params.burst_size);
	if(!packet_counter_arr) {
		log_err("Failed to allocate memory for timestamps array");
		exit(1);
	}	
}

int main(int argc, char *argv[])
{
	int daemonize = false;
	struct sockaddr_in addr;
	char mcg_filename[MAX_PATH_LENGTH];
	char fd_handle_type[MAX_ARGV_SIZE];
	if (argc == 1){
		usage(argv[0]);
		return 1;
	}
	/* set default values */
	set_defaults();
	mcg_filename[0] = '\0';
	addr.sin_family = AF_INET;
	addr.sin_port = htons(DEFAULT_PORT);
	inet_aton(DEFAULT_MC_ADDR, &addr.sin_addr);

	/* Parse the parameters */
	while (1) {
		int c = 0;       
		static struct option long_options[] = {
			{.name = "client",		.has_arg = 0,	.val = 'c'},
			{.name = "server",		.has_arg = 0,	.val = 's'},
			{.name = "bridge",		.has_arg = 0,	.val = 'B'},
			{.name = "ip",			.has_arg = 1,	.val = 'i'},
			{.name = "port",	    	.has_arg = 1,	.val = 'p'},
			{.name = "msg_size",		.has_arg = 1,	.val = 'm'},
			{.name = "range",		.has_arg = 1,	.val = 'r'},
			{.name = "burst",		.has_arg = 1,	.val = 'b'},
			{.name = "time",		.has_arg = 1,	.val = 't'},
			{.name = "file",		.has_arg = 1,	.val = 'f'},
			{.name = "fd_hanlder_type",	.has_arg = 1,	.val = 'F'},
			{.name = "information",		.has_arg = 1,	.val = 'I'},
			{.name = "streammode",		.has_arg = 0,	.val = 'k'},
			{.name = "activity",		.has_arg = 0,	.val = 'a'},
			{.name = "Activity",		.has_arg = 0,	.val = 'A'},
			{.name = "rx_mc_if",		.has_arg = 1,	.val = OPT_RX_MC_IF },
			{.name = "tx_mc_if",		.has_arg = 1,	.val = OPT_TX_MC_IF },
			{.name = "timeout",		.has_arg = 1,   .val = OPT_SELECT_TIMEOUT },
			{.name = "threads-num",		.has_arg = 1,   .val = OPT_MULTI_THREADED_SERVER },
			{.name = "cycle_duration",	.has_arg = 1,	.val = OPT_CLIENT_CYCLE_DURATION },
			{.name = "udp-buffer-size",	.has_arg = 1,   .val = OPT_UDP_BUFFER_SIZE },
			{.name = "data_integrity",	.has_arg = 0,	.val = OPT_DATA_INTEGRITY },
			{.name = "daemonize",		.has_arg = 0,   .val = OPT_DAEMONIZE },
			{.name = "nonblocked",		.has_arg = 0,   .val = OPT_NONBLOCKED },
			{.name = "dontwarmup",		.has_arg = 0,   .val = OPT_DONTWARMUP },
			{.name = "pre_warmup_wait",	.has_arg = 1,   .val = OPT_PREWARMUPWAIT },
			{.name = "vmarxfiltercb",	.has_arg = 0,   .val = OPT_VMARXFILTERCB },
			{.name = "vmazcopyread",	.has_arg = 0,   .val = OPT_VMAZCOPYREAD },
			{.name = "mc_loopback_disable",	.has_arg = 0,   .val = OPT_MC_LOOPBACK_DISABLE },
			{.name = "srv_num",		.has_arg = 1,   .val = OPT_CLIENT_WORK_WITH_SRV_NUM },
			{.name = "force_unicast_reply",	.has_arg = 0,   .val = OPT_FORCE_UC_REPLY},
			{.name = "version",		.has_arg = 0,	.val = 'v'},
			{.name = "debug",		.has_arg = 0,	.val = 'd'},
			{.name = "help",		.has_arg = 0,	.val = 'h'},
			{0,0,0,0}
		};

		if ((c = getopt_long(argc, argv, "csBdi:p:t:f:F:m:r:b:x:g:a:A:kI:vh?", long_options, NULL)) == -1)
			break;

		switch (c) {
		case 'c':
			user_params.mode = MODE_CLIENT;
			break;
		case 's':
			user_params.mode = MODE_SERVER;
			user_params.msg_size = MAX_PAYLOAD_SIZE;
			break;
		case 'B':
			log_msg("update to bridge mode");
			user_params.mode = MODE_BRIDGE;
			user_params.msg_size = MAX_PAYLOAD_SIZE;
			addr.sin_port = htons(5001); /*iperf's default port*/
			break;
		case 'i':
			if (!inet_aton(optarg, &addr.sin_addr)) {	/* already in network byte order*/
				log_msg("'-%c' Invalid address: %s", c, optarg);
				usage(argv[0]);
				return 1;
			}
			user_params.fd_handler_type = RECVFROM;
			break;
		case 'p':
			{
				errno = 0;
				long mc_dest_port = strtol(optarg, NULL, 0);
				/* strtol() returns 0 if there were no digits at all */
				if (errno != 0) {
					log_msg("'-%c' Invalid port: %d", c, (int)mc_dest_port);
					usage(argv[0]);
					return 1;
				}
				addr.sin_port = htons((uint16_t)mc_dest_port);
				user_params.fd_handler_type = RECVFROM;
			}
			break;
		case 'm':
			user_params.msg_size = strtol(optarg, NULL, 0);
			if (user_params.msg_size < MIN_PAYLOAD_SIZE) {
				log_msg("'-%c' Invalid message size: %d (min: %d)", c, user_params.msg_size, MIN_PAYLOAD_SIZE);
				usage(argv[0]);                         
				return 1;
			}
			break;
		case 'r':
			errno = 0;
			int range = strtol(optarg, NULL, 0);
			if (errno != 0 || range < 0) {
				log_msg("'-%c' Invalid message range: %s", c,optarg);
				usage(argv[0]);                         
				return 1;
			}
			user_params.msg_size_range = range;			
			break;	
		case 'b':
			errno = 0;
			int burst_size = strtol(optarg, NULL, 0);
			if (errno != 0 || burst_size < 1) {
				log_msg("'-%c' Invalid burst size: %s", c, optarg);
				usage(argv[0]);                         
				return 1;
			}
			user_params.burst_size = burst_size;
			break;
		case 't':
			user_params.sec_test_duration = strtol(optarg, NULL, 0);
			if (user_params.sec_test_duration <= 0 || user_params.sec_test_duration > MAX_DURATION) {
				log_msg("'-%c' Invalid duration: %d", c, user_params.sec_test_duration);
				usage(argv[0]);                         
				return 1;
			}
			break;
		case 'f':
			strncpy(mcg_filename, optarg, MAX_ARGV_SIZE);
			mcg_filename[MAX_PATH_LENGTH - 1] = '\0';
			read_from_file = 1;
			break;
		case 'F':                       
			strncpy(fd_handle_type, optarg, MAX_ARGV_SIZE);
			fd_handle_type[MAX_ARGV_SIZE - 1] = '\0';
			if (!strcmp( fd_handle_type, "epoll" ) || !strcmp( fd_handle_type, "e")) {
				user_params.fd_handler_type = EPOLL;                                                            
			}
			else if (!strcmp( fd_handle_type, "poll" )|| !strcmp( fd_handle_type, "p")) {
				user_params.fd_handler_type = POLL;                     
			}
			else if (!strcmp( fd_handle_type, "select" ) || !strcmp( fd_handle_type, "s")) {
				user_params.fd_handler_type = SELECT;                           
			}
			else {
				log_msg("'-%c' Invalid muliply io hanlde type: %s", c, optarg);
				usage(argv[0]);                         
				return 1;
			}
			break;
		case 'd':
			debug_level = LOG_LVL_DEBUG;
			break;	
		case OPT_RX_MC_IF:
			if ((user_params.rx_mc_if_addr.s_addr = inet_addr(optarg)) == INADDR_NONE) {	/* already in network byte order*/
				log_msg("'-%c' Invalid address: %s", c, optarg);
				usage(argv[0]);
				return 1;
			}
			break;
		case OPT_TX_MC_IF:
			if ((user_params.tx_mc_if_addr.s_addr = inet_addr(optarg)) == INADDR_NONE) {	/* already in network byte order*/
				log_msg("'-%c' Invalid address: %s", c, optarg);
				usage(argv[0]);
				return 1;
			}
			break;
		case OPT_SELECT_TIMEOUT:
			{
				errno = 0;
				int timeout = strtol(optarg, NULL, 0);
				if (errno != 0  || timeout < -1) {
					log_msg("'-%c' Invalid select/poll/epoll timeout val: %s", c,optarg);
					usage(argv[0]);                         
					return 1;
				}
				set_select_timeout(timeout);    
			}
			break;
		case OPT_MULTI_THREADED_SERVER:
			{
				user_params.mthread_server = 1;        
				errno = 0;
				int threads_num = strtol(optarg, NULL, 0);
				if (errno != 0  || threads_num < 0) {
					log_msg("-%c' Invalid threads number: %s", c,optarg);
					usage(argv[0]);                         
					return 1;
				}
				user_params.threads_num = threads_num;              
			}
			break;
		case OPT_CLIENT_CYCLE_DURATION:
			errno = 0;
			long long time_interval = strtol(optarg, NULL, 0);
			if (errno != 0  || time_interval < -1) {
				log_msg("'-%c' Invalid duration val: %s", c, optarg);
				usage(argv[0]);                         
				return 1;
			}
			user_params.cycle_duration_nsec = MICRO_TO_NANO(time_interval);
			break;
		case OPT_UDP_BUFFER_SIZE:
			{
				errno = 0;
				int udp_buff_size = strtol(optarg, NULL, 0);
				if (errno != 0 || udp_buff_size <= 0) {
					log_msg("'-%c' Invalid udp buffer size: %s", c,optarg);
					usage(argv[0]);
					return 1;
				}
				user_params.udp_buff_size = udp_buff_size; 
			}
			break;
		case OPT_DATA_INTEGRITY:
			user_params.data_integrity = true;
			break;
		case OPT_DAEMONIZE:
			daemonize = true;
			break;
		case OPT_NONBLOCKED:
			user_params.is_blocked = false;
			break;
		case OPT_DONTWARMUP:
			user_params.do_warmup = false;
			break;
		case OPT_PREWARMUPWAIT:
			errno = 0;
			int pre_warmup_wait = strtol(optarg, NULL, 0);
			if (errno != 0 || pre_warmup_wait <= 0) {
				log_msg("'-%c' Invalid pre warmup wait: %s", c,optarg);
				usage(argv[0]);
				return 1;
			}
			user_params.pre_warmup_wait = pre_warmup_wait;
			break;
		case OPT_VMARXFILTERCB:
			user_params.is_vmarxfiltercb = true;
			break;
		case OPT_VMAZCOPYREAD:
			user_params.is_vmazcopyread = true;
			break;
		case OPT_MC_LOOPBACK_DISABLE:
			user_params.mc_loop_disable = true;
			break;
		case OPT_CLIENT_WORK_WITH_SRV_NUM:
			{
				errno = 0;
				int srv_num = strtol(optarg, NULL, 0);
				if (errno != 0  || srv_num < 1) {
					log_msg("'-%c' Invalid server num val: %s", c, optarg);
					usage(argv[0]);
					return 1;
				}
				user_params.client_work_with_srv_num = srv_num;
			}
			break;
		case OPT_FORCE_UC_REPLY:
			user_params.b_server_reply_via_uc = true;
			break;
		case 'k':
			user_params.stream_mode = true;
			break;
		case 'I':
			errno = 0;
			max_spikes_num = strtol(optarg, NULL, 0);
			if (errno != 0 || max_spikes_num < 1) {
				log_msg("'-%c' Invalid spikes quantity: %s", c,optarg);
				usage(argv[0]);                         
				return 1;
			}
			user_params.b_client_calc_details = true;
			break;
		case 'a':
		case 'A':
			errno = 0;
			user_params.packetrate_stats_print_ratio = strtol(optarg, NULL, 0);
			user_params.packetrate_stats_print_details = (c == 'A')?true:false;
			if (errno != 0) {
				log_msg("'-%c' Invalid packet rate stats print value: %d", c, user_params.packetrate_stats_print_ratio);
				usage(argv[0]);                         
				return 1;
			}
			break;
		case 'v':
			print_version();
			return 0;
		case '?':
		case 'h':
			usage(argv[0]);
			return 1;
			break;
		default:
			usage(argv[0]);
			return 1;
		}
	}
	if (optind < argc) {
		printf(MODULE_NAME  "non-option ARGV-elements: ");
		while (optind < argc)
			printf("%s ", argv[optind++]);
		printf("\n");
		usage(argv[0]);
		return 1;
	}

	if (user_params.mode != MODE_SERVER && user_params.mthread_server) {
		log_msg("--threads-num can only work on server side");
		return 1;
	}
	
	if (strlen(mcg_filename) == 0 && (user_params.mthread_server || user_params.threads_num > 1)) {
		log_msg("--threads-num must be used with feed file (option '-f')");
		return 1;
	}
	
	if (user_params.mode != MODE_CLIENT && user_params.msg_size_range > 0) {
		log_msg("dynamic message size mode can be used on client side only");
		return 1;
	}

	if (user_params.mode == MODE_CLIENT && user_params.is_vmarxfiltercb) {
		log_msg("--vmarxfiltercb can only work on server side");
		return 1;
	}

	if ((user_params.fd_handler_type != RECVFROM) && (strlen(mcg_filename) <= 0)) {
		log_msg("[-F | fd_hanlder_type] has to come with option: [-f | --file]");
		usage(argv[0]);
		return 1;
	}

#if 0
	// AlexR: for testing daemonize with allready opened UDP socket
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd) {
		log_msg("new socket - dummy");
	}
#endif

	if (daemonize) {
		if (daemon(1, 1)) {
			log_err("Failed to daemonize");
		}
		log_msg("Running as daemon");
	}

	if (user_params.is_vmarxfiltercb || user_params.is_vmazcopyread) {
#ifdef  USING_VMA_EXTRA_API
		// Get VMA extended API
		vma_api = vma_get_api();
		if (vma_api == NULL)
			log_err("VMA Extra API not found - working with default socket APIs");
		else
			log_msg("VMA Extra API found - using VMA's receive zero copy and packet filter APIs");

		vma_dgram_desc_size = sizeof(struct vma_datagram_t) + sizeof(struct iovec) * 16;
#else
		log_msg("This udp_lat version is not compiled with VMA extra API");
#endif
	}

	if (user_params.b_client_calc_details == true) {
		make_empty_spikes_list();
		prepare_to_info_mode();
	}

	if (user_params.fd_handler_type == RECVFROM && read_from_file == 1) {
		if (user_params.mode == MODE_SERVER) { /* if mode equal to MODE_CLIENT the handler type will be RECVFROM*/
			user_params.fd_handler_type = SELECT;
		}
	}

	if (strlen(mcg_filename) > 0 || user_params.mthread_server) {
		if (strlen(mcg_filename) > 0) {
			set_mcgroups_fromfile(mcg_filename);
		}		
		if (user_params.mthread_server) {
			if (user_params.threads_num > sockets_num  || user_params.threads_num == 0) {
				user_params.threads_num = sockets_num;
			}
			pid_arr = (int*)malloc(sizeof(int)*(user_params.threads_num + 1));
			if(!pid_arr) {
				log_err("Failed to allocate memory for pid array");
				exit(1);
			}
			log_msg("Running %d threads to manage %d sockets",user_params.threads_num,sockets_num);
		}
	}
	else {
		fds_data *tmp = (struct fds_data *)malloc(sizeof(struct fds_data));
		memset(tmp, 0, sizeof(struct fds_data));
		memcpy(&tmp->addr, &addr, sizeof(struct sockaddr_in));
		tmp->is_multicast = IN_MULTICAST(ntohl(tmp->addr.sin_addr.s_addr));
		fd_min = fd_max = prepare_socket(&tmp->addr);
		fd_num = 1;
		fds_array[fd_min] = tmp;
		fds_array[fd_min]->next_fd = fd_min;
	}

	if ((user_params.fd_handler_type != RECVFROM) && (user_params.fd_handler_type == EPOLL) && user_params.threads_num == 1) {
		epfd = epoll_create(sockets_num);
	}

	max_buff_size = max(user_params.msg_size+1, vma_dgram_desc_size);
	
	if(user_params.msg_size_range > 0){
		update_min_max_msg_sizes();
		max_buff_size = max_msg_size + 1;
	}
	
#ifdef  USING_VMA_EXTRA_API
	if (user_params.is_vmazcopyread && vma_api){
		   dgram_buf = malloc(max_buff_size);
	}
#endif

	msgbuf = malloc(max_buff_size);
	msgbuf[0] = '$';

	pattern = malloc(max_buff_size);	
	pattern[0] = '$';
	write_pattern(pattern, max_buff_size - 1);
	pattern[1] = CLIENT_MASK;

	set_signal_action();


	/*
	** TEST START
	*/
	switch (user_params.mode) {
	case MODE_CLIENT:
		client_handler();
		break;
	case MODE_SERVER:
	   if (user_params.mthread_server) {
	           server_select_per_thread();
	           break;
	   }	   
	case MODE_BRIDGE:		
		server_handler(fd_min, fd_max, fd_num); 
		break;
	}
	/* 
	** TEST END 
	*/

	return 0;
}
