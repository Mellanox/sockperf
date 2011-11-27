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
#include <stdexcept>
#include "Defs.h"
#include "PacketTimes.h"

#include "common.h"

PacketTimes::PacketTimes(uint64_t _maxSequenceNo, uint64_t _replyEvery, uint64_t _numServers)
	: m_maxSequenceNo(_maxSequenceNo)
	, m_replyEvery(_replyEvery)
	, m_blockSize(1 + _numServers) // 1 sent + N replies
	, m_pTimes (new TicksTime[ (_maxSequenceNo / _replyEvery + 1) * m_blockSize])//_maxSequenceNo/_replyEvery+1 is _numBlocks rounded up
	, m_pInternalUse(&m_pTimes[1])
	, m_pErrors(new ArrivalErrors[_numServers])
{
/*
	log_msg("m_maxSequenceNo=%lu, m_replyEvery=%lu, m_blockSize=%lu, m_pTimes=%p[%lu], m_pInternalUse=%p, m_pErrors=%p[%lu]"

			, m_maxSequenceNo, m_replyEvery, m_blockSize, m_pTimes
			, (_maxSequenceNo / _replyEvery + 1) * m_blockSize
			, m_pInternalUse
			, m_pErrors, _numServers
			);
//*/
}

PacketTimes::~PacketTimes() {
	delete[] m_pTimes;
	delete[] m_pErrors;
}

bool PacketTimes::verifyError(uint64_t _seqNo) {
	exit_with_err("_seqN > m_maxSequenceNo", SOCKPERF_ERR_FATAL);
	return false;
}
