/*
 * Copyright (c) 2011-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include "defs.h"
#include "message.h"
#include "packet.h"

/* Global variables */
bool g_b_exit = false;
bool g_b_errorOccured = false;
uint64_t g_receiveCount = 0; // TODO: should be one per server
uint64_t g_skipCount = 0;    // TODO: should be one per server

unsigned long long g_cycle_wait_loop_counter = 0;
TicksTime g_cycleStartTime;

debug_level_t g_debug_level = LOG_LVL_INFO;

#ifdef USING_EXTRA_API
ZeroCopyData::ZeroCopyData() : m_pkt_buf(NULL), m_pkts(NULL) {}

void ZeroCopyData::allocate() { m_pkt_buf = (unsigned char *)MALLOC(Message::getMaxSize()); }

ZeroCopyData::~ZeroCopyData() {
    if (m_pkt_buf) FREE(m_pkt_buf);
}

zeroCopyMap g_zeroCopyData;
#endif // USING_EXTRA_API

uint32_t MPS_MAX = MPS_MAX_UL; // will be overwrite at runtime in case of ping-pong test
PacketTimes *g_pPacketTimes = NULL;

TicksTime g_lastTicks;

int max_fds_num = 0;
fds_data **g_fds_array = NULL;
int MAX_PAYLOAD_SIZE = 65507;
int IGMP_MAX_MEMBERSHIPS = IP_MAX_MEMBERSHIPS;

#ifdef USING_VMA_EXTRA_API // VMA
struct vma_api_t *g_vma_api;
#else
void *g_vma_api = nullptr; // Dummy variable
#endif // USING_VMA_EXTRA_API

#ifdef USING_XLIO_EXTRA_API // XLIO
struct xlio_api_t *g_xlio_api;
#else
void *g_xlio_api = nullptr; // Dummy variable
#endif // USING_XLIO_EXTRA_API

const App *g_pApp = NULL;
