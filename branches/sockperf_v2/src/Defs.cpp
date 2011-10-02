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
#include "Message.h"
#include "PacketTimes.h"

/* Global variables */
bool g_b_exit = false;
uint64_t g_receiveCount = 0; // TODO: should be one per server
uint64_t g_skipCount = 0; // TODO: should be one per server

unsigned long long g_cycle_wait_loop_counter = 0;
TicksTime g_cycleStartTime;

debug_level_t g_debug_level = LOG_LVL_INFO;

#ifdef  USING_VMA_EXTRA_API
unsigned char* g_dgram_buf = NULL;
struct vma_datagram_t *g_dgram = NULL;
#endif


uint32_t MPS_MAX = MPS_MAX_UL;     // will be overwrite at runtime in case of ping-pong test
PacketTimes* g_pPacketTimes = NULL;

TicksTime g_lastTicks;


int get_max_active_fds_num() {
	static int max_active_fd_num = 0;
	if (!max_active_fd_num) {
		struct rlimit curr_limits;
		if (getrlimit (RLIMIT_NOFILE, &curr_limits) == -1) {
			perror ("getrlimit");
			return 1024; // try the common default
		}
		max_active_fd_num = (int) curr_limits.rlim_max;
	}
	return max_active_fd_num;
}
const int MAX_FDS_NUM = get_max_active_fds_num();
fds_data** g_fds_array = NULL;
int MAX_PAYLOAD_SIZE = 65506;

#ifdef  USING_VMA_EXTRA_API
struct vma_api_t *g_vma_api;
#endif

const App* g_pApp = NULL;


