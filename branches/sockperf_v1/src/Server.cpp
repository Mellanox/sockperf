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
#include "Server.h"
#include "IoHandlers.h"
#include "Switches.h"

//==============================================================================

//------------------------------------------------------------------------------
void ServerBase::initBeforeLoop()
{

	set_affinity(pthread_self(), g_pApp->m_const_params.receiver_affinity);

	char to_array[20];
	log_dbg("thread %d: fd_min: %d, fd_max : %d, fd_num: %d"
			, gettid(), m_ioHandlerRef.m_fd_min, m_ioHandlerRef.m_fd_max, m_ioHandlerRef.m_fd_num);

	if (g_pApp->m_const_params.mode == MODE_BRIDGE) {
		sprintf(to_array, "%s", inet_ntoa(g_pApp->m_const_params.tx_mc_if_addr));
		printf(MODULE_NAME ": [BRIDGE] transferring packets from %s to %s on:", inet_ntoa(g_pApp->m_const_params.rx_mc_if_addr), to_array);
	}
	else {
		printf(MODULE_NAME ": [SERVER] listen on:");
	}

	m_ioHandlerRef.prepareNetwork();
	sleep(g_pApp->m_const_params.pre_warmup_wait);
	m_ioHandlerRef.warmup();
	log_msg("[tid %d] using %s() to block on socket(s)", gettid(), g_fds_handle_desc[g_pApp->m_const_params.fd_handler_type]);
}

//------------------------------------------------------------------------------
void ServerBase::cleanupAfterLoop() {
	// cleanup
	log_dbg("thread %d released allocations",gettid());

	if (!g_pApp->m_const_params.mthread_server) {
		log_msg("%s() exit", __func__);
	}
}

//==============================================================================
//==============================================================================

//------------------------------------------------------------------------------
template <class IoType, class SwitchActivityInfo, class SwitchCalcGaps>
Server<IoType, SwitchActivityInfo, SwitchCalcGaps>::Server(int _fd_min, int _fd_max, int _fd_num)
	: ServerBase(m_ioHandler), m_ioHandler(_fd_min, _fd_max, _fd_num)
{
}

//------------------------------------------------------------------------------
template <class IoType, class SwitchActivityInfo, class SwitchCalcGaps>
Server<IoType, SwitchActivityInfo, SwitchCalcGaps>::~Server()
{
}

//------------------------------------------------------------------------------
template <class IoType, class SwitchActivityInfo, class SwitchCalcGaps>
void Server<IoType, SwitchActivityInfo, SwitchCalcGaps>::doLoop()
{
	int numReady = 0;
	int actual_fd = 0;

	while (!g_b_exit) {
		// wait for arrival
		numReady = m_ioHandler.waitArrival();

		// check errors
		if (g_b_exit) continue;
		if (numReady < 0) {
			log_err("%s()", g_fds_handle_desc[g_pApp->m_const_params.fd_handler_type]);
			exit_with_log(1);
		}
		if (numReady == 0) {
			if (!g_pApp->m_const_params.select_timeout)
				log_msg("Error: %s() returned without fd ready", g_fds_handle_desc[g_pApp->m_const_params.fd_handler_type]);
			continue;
		}

		// handle arrival and response
	for (int ifd = m_ioHandler.get_look_start(); ifd < m_ioHandler.get_look_end(); ifd++) {
			actual_fd = m_ioHandler.analyzeArrival(ifd);
			if (actual_fd){
				server_receive_then_send(actual_fd);
			}
		}
	}
}

//------------------------------------------------------------------------------
template <class IoType, class SwitchActivityInfo, class SwitchCheckGaps>
void server_handler(int _fd_min, int _fd_max, int _fd_num) {
	Server<IoType, SwitchActivityInfo, SwitchCheckGaps> s(_fd_min, _fd_max, _fd_num);
	s.doHandler();
}

//------------------------------------------------------------------------------
template <class IoType, class SwitchActivityInfo>
void server_handler(int _fd_min, int _fd_max, int _fd_num) {
	if (g_pApp->m_const_params.b_server_detect_gaps)
		server_handler<IoType, SwitchActivityInfo, SwitchOnCalcGaps> (_fd_min, _fd_max, _fd_num);
	else
		server_handler<IoType, SwitchActivityInfo, SwitchOff> (_fd_min, _fd_max, _fd_num);
}

//------------------------------------------------------------------------------
template <class IoType>
void server_handler(int _fd_min, int _fd_max, int _fd_num) {
	if (g_pApp->m_const_params.packetrate_stats_print_ratio > 0)
		server_handler<IoType, SwitchOnActivityInfo> (_fd_min, _fd_max, _fd_num);
	else
		server_handler<IoType, SwitchOff> (_fd_min, _fd_max, _fd_num);
}


//------------------------------------------------------------------------------
void server_handler(int _fd_min, int _fd_max, int _fd_num) {
	switch (g_pApp->m_const_params.fd_handler_type) {
	case RECVFROM:
	{
		server_handler<IoRecvfrom>(_fd_min, _fd_max, _fd_num);
		break;
	}
	case SELECT:
	{
		server_handler<IoSelect>(_fd_min, _fd_max, _fd_num);
		break;
	}
	case POLL:
	{
		server_handler<IoPoll>(_fd_min, _fd_max, _fd_num);
		break;
	}
	case EPOLL:
	{
		server_handler<IoEpoll>(_fd_min, _fd_max, _fd_num);
		break;
	}
	default:
		ERROR("unknown file handler");
	}

}


//------------------------------------------------------------------------------
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

// some helper functions ---> may need to move to common
//------------------------------------------------------------------------------
void devide_fds_arr_between_threads(int *p_num_of_remainded_fds, int *p_fds_arr_len) {

	*p_num_of_remainded_fds = g_sockets_num%g_pApp->m_const_params.threads_num;
	*p_fds_arr_len = g_sockets_num/g_pApp->m_const_params.threads_num;
}

//------------------------------------------------------------------------------
void find_min_max_fds(int start_look_from, int len, int* p_fd_min, int* p_fd_max) {
	int num_of_detected_fds;
	int i;

	for(num_of_detected_fds = 0, i = start_look_from; num_of_detected_fds < len;i++) {
		if (g_fds_array[i]) {
			if (!num_of_detected_fds) {
				*p_fd_min = i;
			}
			num_of_detected_fds++;
		}
	}
	*p_fd_max = i - 1;
}


//------------------------------------------------------------------------------
void server_sig_handler(int signum) {
	if (g_b_exit) {
		log_msg("Test end (interrupted by signal %d)", signum);
		return;
	}

	// Just in case not Activity updates where logged add a '\n'
	if (g_pApp->m_const_params.packetrate_stats_print_ratio && !g_pApp->m_const_params.packetrate_stats_print_details &&
	   (g_pApp->m_const_params.packetrate_stats_print_ratio < g_receiveCount))
		printf("\n");

	if (g_pApp->m_const_params.mthread_server) {
		if (gettid() == g_pid_arr[0]) {  //main thread
			if (g_debug_level >= LOG_LVL_DEBUG) {
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

	if (!g_receiveCount) {
		log_msg("No messages were received on the server.");
	}
	else {
		log_msg("Total %" PRIu64 " messages received and handled", g_receiveCount); //TODO: print also send count
	}
	SwitchOnCalcGaps::print_summary(&g_seq_num_map);
	g_b_exit = true;

}


//------------------------------------------------------------------------------
void server_select_per_thread() {
	int i;
	pthread_t tid;
	int fd_num;
	int num_of_remainded_fds;
	int last_fds = 0;

	g_pid_arr[0] = gettid();
	devide_fds_arr_between_threads(&num_of_remainded_fds, &fd_num);

	for (i = 0; i < g_pApp->m_const_params.threads_num; i++) {
		sub_fds_arr_info *thread_fds_arr_info = (sub_fds_arr_info*)malloc(sizeof(sub_fds_arr_info));
		if (!thread_fds_arr_info) {
			log_err("Failed to allocate memory for sub_fds_arr_info");
			exit_with_log(1);
		}
		thread_fds_arr_info->fd_num = fd_num;
		if (num_of_remainded_fds) {
			thread_fds_arr_info->fd_num++;
			num_of_remainded_fds--;
		}
		find_min_max_fds(last_fds, thread_fds_arr_info->fd_num, &(thread_fds_arr_info->fd_min), &(thread_fds_arr_info->fd_max));
		pthread_create(&tid, 0, server_handler_for_multi_threaded, (void *)thread_fds_arr_info);
		g_pid_arr[i + 1] = tid;
		last_fds = thread_fds_arr_info->fd_max + 1;
	}
	while (!g_b_exit) {
		sleep(1);
	}
	for (i = 1; i <= g_pApp->m_const_params.threads_num; i++) {
		pthread_kill(g_pid_arr[i], SIGINT);
		pthread_join(g_pid_arr[i], 0);
	}
	log_msg("%s() exit", __func__);
}


// Temp location because of compilation issue with the way this method was inlined
void SwitchOnCalcGaps::execute(struct sockaddr_in *clt_addr, uint64_t seq_num, bool is_warmup)
{
	seq_num_map::iterator itr = g_seq_num_map.find(*clt_addr);
	bool starting_new_session = false;
	bool print_summary = false;

	if (itr == g_seq_num_map.end()) {
		clt_session_info_t new_session;
		memcpy(&new_session.addr, clt_addr, sizeof(struct sockaddr_in));
		new_session.seq_num = seq_num;
		new_session.total_drops = 0;
		new_session.started = false;
		std::pair<seq_num_map::iterator, bool> ret_val = g_seq_num_map.insert(seq_num_map::value_type(*clt_addr, new_session));
		if (ret_val.second)
			itr = ret_val.first;
		else {
			log_err("Failed to insert new session info, so the gap detection is not supported.");
			return;
		}
		starting_new_session = true;

	}
	else if (is_warmup && itr->second.started) {
		//first warmup packet and old session was found in DB =>
		//needed to print old session summary.
		itr->second.started = false;
		starting_new_session = true;
		print_summary = true;

	}

	//print summary of the previous session + reset the counters
	if (print_summary) {
		print_session_summary(&(itr->second));
		itr->second.seq_num = seq_num;
		itr->second.total_drops = 0;
	}

	//received first packet of the new session
	if (starting_new_session)
		print_new_session_info(&itr->second);

	if(!is_warmup) {
		if (!itr->second.started)
			itr->second.started = true;
		check_gaps(seq_num, itr);
	}
}
