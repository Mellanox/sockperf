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
#include "Defs.h"
#include "Playback.h"
#include "Client.h"
#include "IoHandlers.h"
#include "PacketTimes.h"
#include "Switches.h"

TicksTime s_startTime, s_endTime;

//==============================================================================
//==============================================================================

//------------------------------------------------------------------------------
void print_average_latency(double usecAvarageLatency)
{
	if (g_pApp->m_const_params.burst_size == 1) {
		log_msg("Summary: Latency is %.3lf usec", usecAvarageLatency);
	}
	else {
		log_msg("Summary: Latency of burst of %d messages is %.3lf usec", g_pApp->m_const_params.burst_size, usecAvarageLatency);
	}
}

//------------------------------------------------------------------------------
/* set the timer on client to the [-t sec] parameter given by user */
void set_client_timer(struct itimerval *timer)
{

	// extra_sec and extra_msec will be excluded from results
	int extra_sec  = (TEST_START_WARMUP_MSEC + TEST_END_COOLDOWN_MSEC) / 1000;
	int extra_msec = (TEST_START_WARMUP_MSEC + TEST_END_COOLDOWN_MSEC) % 1000;

	timer->it_value.tv_sec = g_pApp->m_const_params.sec_test_duration + extra_sec;
	timer->it_value.tv_usec = 1000 * extra_msec;
	timer->it_interval.tv_sec = 0;
	timer->it_interval.tv_usec = 0;
}

//------------------------------------------------------------------------------
void printPercentiles(FILE* f, TicksDuration* pLat, size_t size)
{
	qsort(pLat, size, sizeof(TicksDuration), TicksDuration::compare);

	double percentile[] = {0.9999, 0.999, 0.995, 0.99, 0.95, 0.90, 0.75, 0.50, 0.25};
	int num = sizeof(percentile)/sizeof(percentile[0]);
	double observationsInPercentile = (double)size/100;

	log_msg_file2(f, "\e[2;35mTotal %lu observations\e[0m; each percentile contains %.2lf observations", (long unsigned)size, observationsInPercentile);

	log_msg_file2(f, "---> <MAX> observation = %8.3lf", pLat[size-1].toDecimalUsec());
	for (int i = 0; i < num; i++) {
		int index = (int)( 0.5 + percentile[i]*size ) - 1;
		if (index >= 0) {
			log_msg_file2(f, "---> percentile %6.2lf = %8.3lf", 100*percentile[i], pLat[index].toDecimalUsec());
		}
	}
	log_msg_file2(f, "---> <MIN> observation = %8.3lf", pLat[0].toDecimalUsec());
}

//------------------------------------------------------------------------------
typedef TicksTime RecordLog[2];

//------------------------------------------------------------------------------
void dumpFullLog(int serverNo, RecordLog* pFullLog, size_t size, TicksDuration *pLat)
{
	FILE * f = g_pApp->m_const_params.fileFullLog;
	if (!f || !size) return;

	fprintf(f, "------------------------------\n");
	fprintf(f, "txTime, rxTime\n");
	for (size_t i = 0; i < size; i++) {
		double tx = (double)pFullLog[i][0].debugToNsec()/1000/1000/1000;
		double rx = (double)pFullLog[i][1].debugToNsec()/1000/1000/1000;
		fprintf(f, "%.9lf, %.9lf\n", tx, rx);
	}
	fprintf(f, "------------------------------\n");
}

//------------------------------------------------------------------------------
void client_statistics(int serverNo, Message *pMsgRequest)
{
	const uint64_t receiveCount = g_receiveCount;
	const uint64_t sendCount    = pMsgRequest->getSequenceCounter();
	const uint64_t replyEvery   = g_pApp->m_const_params.reply_every;
	const size_t SIZE = receiveCount;
	const int SERVER_NO = serverNo;

	FILE* f = g_pApp->m_const_params.fileFullLog;

	if (!receiveCount) {
		log_msg_file2(f, "No messages were received from the server. Is the server down?");
		return;
	}

	/* Print total statistic that is independent on server count */
	if (SERVER_NO == 0) {
		TicksDuration totalRunTime = s_endTime - s_startTime;
		if (g_skipCount) {
			log_msg_file2(f, "[Total Run] RunTime=%.3lf sec; SentMessages=%" PRIu64 "; ReceivedMessages=%" PRIu64 "; SkippedMessages=%" PRIu64 "",
				totalRunTime.toDecimalUsec()/1000000, sendCount, receiveCount, g_skipCount);
		}
		else 
		{
			log_msg_file2(f, "[Total Run] RunTime=%.3lf sec; SentMessages=%" PRIu64 "; ReceivedMessages=%" PRIu64 "",
				totalRunTime.toDecimalUsec()/1000000, sendCount, receiveCount);
		}
	}

	/* Print server related statistic */
	log_msg_file2(f, "========= Printing statistics for Server No: %d", SERVER_NO);

	/*
	 * There are few reasons to ignore warmup/cooldown packets:
	 *
	 * 1. At the head of the test the load is not real, since only few packets were sent so far.
	 * 2. At the tail of the test the load is not real since the sender stopped sending; hence,
	 *    the receiver accept packets without load
	 * 3. The sender thread starts sending packets and generating load, before the receiver
	 *    thread has started, and before its code was cached to memory/cpu.
	 * 4. There are some packets that were sent close to s_end time; the legitimate replies to
	 *    them will arrive after s_end time and may be lost.  Hence, your fix may cause us to
	 *    report on those packets as dropped packets.
	 */

	TicksTime testStart = g_pPacketTimes->getTxTime(replyEvery);// first pong request packet
	TicksTime testEnd   = g_pPacketTimes->getTxTime(sendCount); // will be "truncated" to last pong request packet

	if (!g_pApp->m_const_params.pPlaybackVector) { // no warmup in playback mode
		testStart += TicksDuration::TICKS1MSEC * TEST_START_WARMUP_MSEC;
		testEnd   -= TicksDuration::TICKS1MSEC * TEST_END_COOLDOWN_MSEC;
	}

	TicksDuration *pLat = new TicksDuration[SIZE];
	RecordLog *pFullLog = new RecordLog[SIZE];

	TicksDuration rtt;
	TicksDuration sumRtt(0);
	size_t counter = 0;
	size_t lcounter = 0;
	TicksTime prevRxTime;
	TicksTime startValidTime;
	TicksTime endValidTime;
	uint64_t startValidSeqNo = 0;
	uint64_t endValidSeqNo = 0;
	for (size_t i = 1; i < SIZE; i++) {
		uint64_t seqNo    = i * replyEvery;
		const TicksTime & txTime   = g_pPacketTimes->getTxTime(seqNo);
		const TicksTime & rxTime   = g_pPacketTimes->getRxTimeArray(seqNo)[SERVER_NO];

		if (txTime < testStart || txTime > testEnd) {
			continue;
		}

		if (startValidSeqNo == 0) {
			startValidSeqNo = seqNo;
			startValidTime = txTime;
		}
		endValidSeqNo = seqNo;
		endValidTime = rxTime;

		pFullLog[lcounter][0] = txTime;
		pFullLog[lcounter][1] = rxTime;
		lcounter++;

		if (rxTime == TicksTime::TICKS0) {
			g_pPacketTimes->incDroppedCount(SERVER_NO);
			continue;
		}
		if (rxTime < prevRxTime) {
			g_pPacketTimes->incOooCount(SERVER_NO);
			continue;
		}

		rtt = rxTime - txTime;

/*	test: ignore spikes in avg calculation
		if (rtt > TicksDuration::TICKS1USEC * 20) {
			continue;
		}
//*/
		sumRtt += rtt;
		pLat[counter] = rtt/2;

		prevRxTime = rxTime;
		counter++;
	}

	if (!counter) {
		log_msg_file2(f, "No valid observations found.  Try increasing test duration and/or --mps/--reply-every parameters");
	}
	else {
		TicksDuration validRunTime = endValidTime - startValidTime;
		log_msg_file2(f, "[Valid Duration] RunTime=%.3lf sec; SentMessages=%" PRIu64 "; ReceivedMessages=%" PRIu64 "",
				validRunTime.toDecimalUsec()/1000000, (endValidSeqNo - startValidSeqNo + 1), (uint64_t)counter);

		TicksDuration avgRtt = counter ? sumRtt / counter : TicksDuration::TICKS0 ;
		TicksDuration avgLatency = avgRtt / 2;

		TicksDuration stdDev = TicksDuration::stdDev(pLat, counter);
		log_msg_file2(f, "\e[2;35m====> avg-lat=%7.3lf (std-dev=%.3lf)\e[0m", avgLatency.toDecimalUsec(), stdDev.toDecimalUsec());

		/* Display ERROR statistic */
		bool isColor = (g_pPacketTimes->getDroppedCount(SERVER_NO) ||
				        g_pPacketTimes->getDupCount(SERVER_NO) ||
				        g_pPacketTimes->getOooCount(SERVER_NO));
		const char* colorRedStr   = isColor ? "\e[0;31m" : "";
		const char* colorResetStr = isColor ? "\e[0m" : "";
		log_msg_file2(f, "%s# dropped messages = %lu; # duplicated messages = %lu; # out-of-order messages = %lu%s"
				, colorRedStr
				, (long unsigned)g_pPacketTimes->getDroppedCount(SERVER_NO)
				, (long unsigned)g_pPacketTimes->getDupCount(SERVER_NO)
				, (long unsigned)g_pPacketTimes->getOooCount(SERVER_NO)
				, colorResetStr
				);

		double usecAvarageLatency = avgLatency.toDecimalUsec();
		if (usecAvarageLatency)
			print_average_latency(usecAvarageLatency);

		printPercentiles(f, pLat, counter);

		dumpFullLog(SERVER_NO, pFullLog, lcounter, pLat);
	}

	delete[] pLat;
	delete[] pFullLog;
}

//------------------------------------------------------------------------------
void stream_statistics(Message *pMsgRequest)
{
	TicksDuration totalRunTime = s_endTime - s_startTime;

	if (totalRunTime <= TicksDuration::TICKS0) return;
	if (!g_pApp->m_const_params.b_stream) return;

	const uint64_t sendCount = pMsgRequest->getSequenceCounter();

	// Send only mode!
	if (g_skipCount) {
		log_msg("Total of %" PRIu64 " messages sent in %.3lf sec (%" PRIu64 " messages skipped)\n",
				sendCount, totalRunTime.toDecimalUsec()/1000000, g_skipCount);
	}
	else 
	{
		log_msg("Total of %" PRIu64 " messages sent in %.3lf sec\n",
				sendCount, totalRunTime.toDecimalUsec()/1000000);
	}
	if (g_pApp->m_const_params.mps != MPS_MAX) {
		if (g_pApp->m_const_params.msg_size_range)
			log_msg("\e[2;35mNOTE: test was performed, using average msg-size=%d (+/-%d), mps=%u. For getting maximum throughput use --mps=max (and consider --msg-size=1472 or --msg-size=4096)\e[0m",
					g_pApp->m_const_params.msg_size,
					g_pApp->m_const_params.msg_size_range,
					g_pApp->m_const_params.mps);
		else
			log_msg("\e[2;35mNOTE: test was performed, using msg-size=%d, mps=%u. For getting maximum throughput use --mps=max (and consider --msg-size=1472 or --msg-size=4096)\e[0m",
					g_pApp->m_const_params.msg_size,
					g_pApp->m_const_params.mps);
	}
	else if (g_pApp->m_const_params.msg_size != 1472) {
		if (g_pApp->m_const_params.msg_size_range)
			log_msg("\e[2;35mNOTE: test was performed, using average msg-size=%d (+/-%d). For getting maximum throughput consider using --msg-size=1472\e[0m",
					g_pApp->m_const_params.msg_size,
					g_pApp->m_const_params.msg_size_range);
		else
			log_msg("\e[2;35mNOTE: test was performed, using msg-size=%d. For getting maximum throughput consider using --msg-size=1472\e[0m",
					g_pApp->m_const_params.msg_size);
	}

	int ip_frags_per_msg = (g_pApp->m_const_params.msg_size + DEFAULT_IP_PAYLOAD_SZ - 1) / DEFAULT_IP_PAYLOAD_SZ;
	int msgps = (int)(0.5 + ((double)sendCount)*1000*1000 / totalRunTime.toDecimalUsec());

	int pktps = msgps * ip_frags_per_msg;
	int total_line_ip_data = g_pApp->m_const_params.msg_size;
	double MBps = ((double)msgps * total_line_ip_data)/1024/1024; /* No including IP + UDP Headers per fragment */
	if (ip_frags_per_msg == 1)
		log_msg("Summary: Message Rate is %d [msg/sec]", msgps);
	else
		log_msg("Summary: Message Rate is %d [msg/sec], Packet Rate is about %d [pkt/sec] (%d ip frags / msg)", msgps, pktps, ip_frags_per_msg);
	if (g_pApp->m_const_params.giga_size){
		log_msg("Summary: BandWidth is %.3f GBps (%.3f Gbps)", MBps/1000, MBps*8/1000);
	}
	else if (g_pApp->m_const_params.increase_output_precision){
			log_msg("Summary: BandWidth is %.9f GBps (%.9f Gbps)", MBps, MBps*8);
	}
	else{
		log_msg("Summary: BandWidth is %.3f MBps (%.3f Mbps)", MBps, MBps*8);
	}
}

//------------------------------------------------------------------------------
void client_sig_handler(int signum)
{
	if (g_b_exit) {
		log_msg("Test end (interrupted by signal %d)", signum);
		return;
	}
	s_endTime.setNowNonInline();
	g_b_exit = true;

	// Just in case not Activity updates where logged add a '\n'
	if (g_pApp->m_const_params.packetrate_stats_print_ratio && !g_pApp->m_const_params.packetrate_stats_print_details)
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
}

//==============================================================================
//==============================================================================

//------------------------------------------------------------------------------
ClientBase::ClientBase()
{
	m_pMsgReply = new Message();
	m_pMsgReply->setLength(MAX_PAYLOAD_SIZE);

	m_pMsgRequest = new Message();
	m_pMsgRequest->getHeader()->setClient();
	m_pMsgRequest->setLength(g_pApp->m_const_params.msg_size);
}

//------------------------------------------------------------------------------
ClientBase::~ClientBase()
{
	delete m_pMsgReply;
	delete m_pMsgRequest;
}

//------------------------------------------------------------------------------
template <class IoType, class SwitchDataIntegrity, class SwitchActivityInfo, class SwitchCycleDuration, class SwitchMsgSize , class PongModeCare >
Client<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchCycleDuration, SwitchMsgSize , PongModeCare>::Client(int _fd_min, int _fd_max, int _fd_num):
	ClientBase(),
	m_receiverTid(0),
	m_ioHandler(_fd_min, _fd_max, _fd_num),
	m_pongModeCare(m_pMsgRequest)
{
}

//------------------------------------------------------------------------------
template <class IoType, class SwitchDataIntegrity, class SwitchActivityInfo, class SwitchCycleDuration, class SwitchMsgSize , class PongModeCare >
Client<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchCycleDuration, SwitchMsgSize , PongModeCare>::~Client()
{
}

//------------------------------------------------------------------------------
template <class IoType, class SwitchDataIntegrity, class SwitchActivityInfo, class SwitchCycleDuration, class SwitchMsgSize , class PongModeCare>
void Client<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchCycleDuration, SwitchMsgSize , PongModeCare>
::client_receiver_thread() {
	while (!g_b_exit) {
		client_receive();
	}
}

//------------------------------------------------------------------------------
void *client_receiver_thread(void *arg)
{
	ClientBase *_this = (ClientBase*)arg;
	_this->client_receiver_thread();
	return 0;
}

//------------------------------------------------------------------------------
template <class IoType, class SwitchDataIntegrity, class SwitchActivityInfo, class SwitchCycleDuration, class SwitchMsgSize , class PongModeCare>
void Client<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchCycleDuration, SwitchMsgSize , PongModeCare>
::cleanupAfterLoop()
{
	usleep(100*1000);//0.1 sec
	if (m_receiverTid) {
		pthread_kill(m_receiverTid, SIGINT);
		//pthread_join(m_receiverTid, 0);
	}

	log_msg("Test ended");

	if (!m_pMsgRequest->getSequenceCounter())
	{
		log_msg("No messages were sent");
	}
	else if (g_pApp->m_const_params.b_stream)
	{
		stream_statistics(m_pMsgRequest);
	}
	else
	{
		FILE* f = g_pApp->m_const_params.fileFullLog;
		if (f) {
			fprintf(f, "------------------------------\n");
			fprintf(f, "test was performed using the following parameters: "
					"--mps=%d --burst=%d --reply-every=%d --msg-size=%d --time=%d\n"
					, (int)g_pApp->m_const_params.mps
					, (int)g_pApp->m_const_params.burst_size
					, (int)g_pApp->m_const_params.reply_every
					, (int)g_pApp->m_const_params.msg_size
					, (int)g_pApp->m_const_params.sec_test_duration);

			fprintf(f, "------------------------------\n");
		}

		for (int i = 0; i < g_pApp->m_const_params.client_work_with_srv_num; i++) {
			client_statistics(i, m_pMsgRequest);
		}
	}

	if (g_pApp->m_const_params.fileFullLog)
		fclose(g_pApp->m_const_params.fileFullLog);

	if (g_pApp->m_const_params.cycleDuration > TicksDuration::TICKS0 && !g_cycle_wait_loop_counter)
		log_msg("Warning: the value of the clients cycle duration might be too small (%" PRId64 " usec).  Try changing --mps and --burst arguments ",
			g_pApp->m_const_params.cycleDuration.toUsec());
}

//------------------------------------------------------------------------------
template <class IoType, class SwitchDataIntegrity, class SwitchActivityInfo, class SwitchCycleDuration, class SwitchMsgSize , class PongModeCare>
int Client<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchCycleDuration, SwitchMsgSize , PongModeCare>
::initBeforeLoop()
{
	int rc = SOCKPERF_ERR_NONE;

	if (g_b_exit) return rc;

	/* bind/connect socket */
	if (rc == SOCKPERF_ERR_NONE)
	{
		struct sockaddr_in bind_addr;

		// cycle through all set fds in the array (with wrap around to beginning)
		for (int ifd = m_ioHandler.m_fd_min; ifd <= m_ioHandler.m_fd_max; ifd++) {

			if (!(g_fds_array[ifd] && (g_fds_array[ifd]->active_fd_list))) continue;

			memset(&bind_addr, 0, sizeof(struct sockaddr_in));

			if (g_fds_array[ifd]->sock_type == SOCK_DGRAM) {
				bind_addr.sin_family = AF_INET;
				if (g_pApp->m_const_params.client_port)
				{
					bind_addr.sin_port = g_pApp->m_const_params.client_port;
				}
				else
				{
					bind_addr.sin_port = g_fds_array[ifd]->addr.sin_port;
				}
				bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
				log_dbg ("binding to: %s:%d [%d]...", inet_ntoa(bind_addr.sin_addr), ntohs(bind_addr.sin_port), ifd);
				if (bind(ifd, (struct sockaddr*)&bind_addr, sizeof(struct sockaddr)) < 0) {
					log_err("Can`t bind socket %s:%d [%d].", inet_ntoa(bind_addr.sin_addr), ntohs(bind_addr.sin_port), ifd);
					rc = SOCKPERF_ERR_SOCKET;
					break;
				}
			}
			else {
				memcpy(&bind_addr, &(g_fds_array[ifd]->addr), sizeof(struct sockaddr_in));
				log_dbg ("connecting to: %s:%d [%d]...", inet_ntoa(bind_addr.sin_addr), ntohs(bind_addr.sin_port), ifd);
				if (connect(ifd, (struct sockaddr*)&bind_addr, sizeof(struct sockaddr)) < 0) {
					if(errno == EINPROGRESS) {
						fd_set rfds, wfds;
						struct timeval tv;
						tv.tv_sec = 0; tv.tv_usec = 500;

						FD_ZERO(&rfds);
						FD_ZERO(&wfds);

						int max_fd = -1;

						FD_SET(ifd, &wfds);
						FD_SET(ifd, &rfds);
						if(ifd > max_fd) max_fd = ifd;

						select(max_fd + 1, &rfds, &wfds, NULL, &tv);
						if(FD_ISSET(ifd, &wfds) || FD_ISSET(ifd, &rfds)) {
							socklen_t err_len;
							int error;

							err_len = sizeof(error);
							if(getsockopt(ifd, SOL_SOCKET, SO_ERROR, &error, &err_len) < 0 || error != 0) {
								log_err("Can`t connect socket");
								rc = SOCKPERF_ERR_SOCKET;
								break;
						    }
						}
					}
					else {
						log_err("Can`t connect socket");
						rc = SOCKPERF_ERR_SOCKET;
						break;
					}
				}
			}
		}
	}

	if (g_b_exit) return rc;

	if (rc == SOCKPERF_ERR_NONE) {

		printf(MODULE_NAME "[CLIENT] send on:");

		if (!g_pApp->m_const_params.b_stream) {
			log_msg("using %s() to block on socket(s)", handler2str(g_pApp->m_const_params.fd_handler_type));
		}

		rc = m_ioHandler.prepareNetwork();
		if (rc == SOCKPERF_ERR_NONE) {
			sleep(g_pApp->m_const_params.pre_warmup_wait);

			m_ioHandler.warmup(m_pMsgRequest);

			sleep(2);

			if (g_b_exit) return rc;

			rc = set_affinity_list(pthread_self(), g_pApp->m_const_params.sender_affinity);
			if (rc == SOCKPERF_ERR_NONE) {
				if (!g_pApp->m_const_params.b_client_ping_pong && !g_pApp->m_const_params.b_stream) { // latency_under_load
					if (0 != pthread_create(&m_receiverTid, 0, ::client_receiver_thread, this)){
						log_err("pthread_create has failed");
						rc = SOCKPERF_ERR_FATAL;
					}
					else {
						rc = set_affinity_list(m_receiverTid, g_pApp->m_const_params.receiver_affinity);
					}
				}

				if (rc == SOCKPERF_ERR_NONE) {
					log_msg("Starting test...");

					if (!g_pApp->m_const_params.pPlaybackVector) {
						struct itimerval timer;
						set_client_timer(&timer);
						int ret = setitimer(ITIMER_REAL, &timer, NULL);
						if (ret) {
							log_err("setitimer()");
							rc = SOCKPERF_ERR_FATAL;
						}
					}

					if (rc == SOCKPERF_ERR_NONE) {
						s_startTime.setNowNonInline();
						g_lastTicks = s_startTime;
						g_cycleStartTime = s_startTime - g_pApp->m_const_params.cycleDuration;
					}
				}
			}
		}
	}

	return rc;
}

//------------------------------------------------------------------------------
template <class IoType, class SwitchDataIntegrity, class SwitchActivityInfo, class SwitchCycleDuration, class SwitchMsgSize , class PongModeCare>
void Client<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchCycleDuration, SwitchMsgSize , PongModeCare>
::doSendThenReceiveLoop()
{
	// cycle through all set fds in the array (with wrap around to beginning)
	for (int curr_fds = m_ioHandler.m_fd_min; !g_b_exit; curr_fds = g_fds_array[curr_fds]->next_fd)
		client_send_then_receive(curr_fds);
}

//------------------------------------------------------------------------------
template <class IoType, class SwitchDataIntegrity, class SwitchActivityInfo, class SwitchCycleDuration, class SwitchMsgSize , class PongModeCare>
void Client<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchCycleDuration, SwitchMsgSize , PongModeCare>
::doSendLoop()
{
	// cycle through all set fds in the array (with wrap around to beginning)
	for (int curr_fds = m_ioHandler.m_fd_min; !g_b_exit; curr_fds = g_fds_array[curr_fds]->next_fd)
		client_send_burst(curr_fds);

}


//------------------------------------------------------------------------------
static inline void playbackCycleDurationWait(const TicksDuration &i_cycleDuration)
{
	static TicksTime s_cycleStartTime = TicksTime().setNowNonInline(); //will only be executed once

	TicksTime nextCycleStartTime = s_cycleStartTime + i_cycleDuration;
	while (!g_b_exit) {
		if (TicksTime::now() >= nextCycleStartTime) {
			break;
		}
	}
	s_cycleStartTime = nextCycleStartTime;
}

//------------------------------------------------------------------------------
template <class IoType, class SwitchDataIntegrity, class SwitchActivityInfo, class SwitchCycleDuration, class SwitchMsgSize , class PongModeCare>
void Client<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchCycleDuration, SwitchMsgSize , PongModeCare>
::doPlayback()
{
	usleep(100*1000);//wait for receiver thread to start (since we don't use warmup) //TODO: configure!
	s_startTime.setNowNonInline();//reduce code size by calling non inline func from slow path
	const PlaybackVector &pv = * g_pApp->m_const_params.pPlaybackVector;

	size_t i = 0;
	const size_t size = pv.size();

	// cycle through all set fds in the array (with wrap around to beginning)
	for (int ifd = m_ioHandler.m_fd_min; i < size && !g_b_exit; ifd = g_fds_array[ifd]->next_fd, ++i) {

		m_pMsgRequest->setLength(pv[i].size);

		//idle
		playbackCycleDurationWait(pv[i].duration);

		//send
		client_send_packet(ifd);

		m_switchActivityInfo.execute(m_pMsgRequest->getSequenceCounter());
	}
	g_cycle_wait_loop_counter++; //for silenting waring at the end
	s_endTime.setNowNonInline();//reduce code size by calling non inline func from slow path
	usleep(20*1000);//wait for reply of last packet //TODO: configure!
	g_b_exit = true;
}

//------------------------------------------------------------------------------
template <class IoType, class SwitchDataIntegrity, class SwitchActivityInfo, class SwitchCycleDuration, class SwitchMsgSize , class PongModeCare>
void Client<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchCycleDuration, SwitchMsgSize , PongModeCare>
::doHandler()
 {
	int rc = SOCKPERF_ERR_NONE;

	rc = initBeforeLoop();
	
	if (rc == SOCKPERF_ERR_NONE) {
		if (g_pApp->m_const_params.pPlaybackVector)
			doPlayback();
		else if (g_pApp->m_const_params.b_client_ping_pong)
			doSendThenReceiveLoop();
		else
			doSendLoop();

		cleanupAfterLoop();
	}
}


//------------------------------------------------------------------------------
template <class IoType, class SwitchDataIntegrity, class SwitchActivityInfo, class SwitchCycleDuration, class SwitchMsgSize, class PongModeCare>
void client_handler(int _fd_min, int _fd_max, int _fd_num) {
	Client<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchCycleDuration, SwitchMsgSize, PongModeCare> c(_fd_min, _fd_max, _fd_num);
	c.doHandler();
}

//------------------------------------------------------------------------------
template <class IoType, class SwitchDataIntegrity, class SwitchActivityInfo, class SwitchCycleDuration, class SwitchMsgSize>
void client_handler(int _fd_min, int _fd_max, int _fd_num) {
	if (g_pApp->m_const_params.b_stream)
		client_handler<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchCycleDuration, SwitchMsgSize, PongModeNever> (_fd_min, _fd_max, _fd_num);
	else if (g_pApp->m_const_params.reply_every == 1)
		client_handler<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchCycleDuration, SwitchMsgSize, PongModeAlways> (_fd_min, _fd_max, _fd_num);
	else
		client_handler<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchCycleDuration, SwitchMsgSize, PongModeNormal> (_fd_min, _fd_max, _fd_num);
}

//------------------------------------------------------------------------------
template <class IoType, class SwitchDataIntegrity, class SwitchActivityInfo, class SwitchCycleDuration>
void client_handler(int _fd_min, int _fd_max, int _fd_num) {
	if (g_pApp->m_const_params.msg_size_range > 0)
		client_handler<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchCycleDuration, SwitchOnMsgSize> (_fd_min, _fd_max, _fd_num);
	else
		client_handler<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchCycleDuration, SwitchOff> (_fd_min, _fd_max, _fd_num);
}

//------------------------------------------------------------------------------
template <class IoType, class SwitchDataIntegrity, class SwitchActivityInfo>
void client_handler(int _fd_min, int _fd_max, int _fd_num) {
	if (g_pApp->m_const_params.cycleDuration > TicksDuration::TICKS0)
		client_handler<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchOnCycleDuration> (_fd_min, _fd_max, _fd_num);
	else
		client_handler<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchOff> (_fd_min, _fd_max, _fd_num);
}

//------------------------------------------------------------------------------
template <class IoType, class SwitchDataIntegrity>
void client_handler(int _fd_min, int _fd_max, int _fd_num) {
	if (g_pApp->m_const_params.packetrate_stats_print_ratio > 0)
		client_handler<IoType, SwitchDataIntegrity, SwitchOnActivityInfo> (_fd_min, _fd_max, _fd_num);
	else
		client_handler<IoType, SwitchDataIntegrity, SwitchOff> (_fd_min, _fd_max, _fd_num);
}

//------------------------------------------------------------------------------
template <class IoType>
void client_handler(int _fd_min, int _fd_max, int _fd_num) {
	if (g_pApp->m_const_params.data_integrity)
		client_handler<IoType, SwitchOnDataIntegrity> (_fd_min, _fd_max, _fd_num);
	else
		client_handler<IoType, SwitchOff> (_fd_min, _fd_max, _fd_num);
}

//------------------------------------------------------------------------------
void client_handler(handler_info *p_info)
{
	if (p_info) {
		switch (g_pApp->m_const_params.fd_handler_type) {
			case SELECT:
			{
				client_handler<IoSelect> (p_info->fd_min, p_info->fd_max, p_info->fd_num);
				break;
			}
			case RECVFROM:
			{
				client_handler<IoRecvfrom> (p_info->fd_min, p_info->fd_max, p_info->fd_num);
				break;
			}
			case POLL:
			{
				client_handler<IoPoll> (p_info->fd_min, p_info->fd_max, p_info->fd_num);
				break;
			}
			case EPOLL:
			{
				client_handler<IoEpoll> (p_info->fd_min, p_info->fd_max, p_info->fd_num);
				break;
			}
			default:
			{
				ERROR("unknown file handler");
				break;
			}
		}
	}
}
