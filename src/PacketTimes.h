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
#ifndef PACKETTIMES_H_
#define PACKETTIMES_H_

#include <stdint.h> // for uint64_t
#include "Ticks.h"
#include <stdexcept>
#include "common.h"

class PacketTimes {
public:
	PacketTimes(uint64_t _maxSequenceNo, uint64_t _replyEvery, uint64_t _numServers);
	~PacketTimes();

	bool verifyError(uint64_t _seqNo);

	uint64_t seq2index (uint64_t _seqNo) {
		return ((_seqNo <= m_maxSequenceNo) ?
					_seqNo / m_replyEvery * m_blockSize :
					verifyError(_seqNo));
	}//index 0 is not used for real packets

	const TicksTime & getTxTime  (uint64_t _seqNo) {
		return  m_pTimes[seq2index(_seqNo)];
	}
	const TicksTime * getRxTimeArray(uint64_t _seqNo) {
		return &m_pInternalUse[seq2index(_seqNo)];
	}

	void clearTxTime(uint64_t _seqNo) {
		m_pTimes[seq2index(_seqNo)] = TicksTime::TICKS0;
	}
	void setTxTime(uint64_t _seqNo){
		m_pTimes[seq2index(_seqNo)].setNow();
		//log_msg(">>> %lu: tx=%.3lf", _seqNo, (double)m_pTimes[seq2index(_seqNo)].debugToNsec()/1000/1000 );//TODO: remove
	}
	void setRxTime(uint64_t _seqNo, uint64_t _serverNo = 0) {
		setRxTime(_seqNo, TicksTime().setNow(), _serverNo);
	}
	void setRxTime(uint64_t _seqNo, const TicksTime &_time, uint64_t _serverNo = 0) {
		TicksTime *rxTimes = &m_pInternalUse[seq2index(_seqNo)];
		if (rxTimes[_serverNo] == TicksTime::TICKS0)
		{
			rxTimes[_serverNo] = _time;
			++g_receiveCount;
			//log_msg("<<< %lu: rx=%.3lf", _seqNo, (double)_time.debugToNsec()/1000/1000 );//TODO: remove
		}
		else if (!g_b_exit)
		{
			incDupCount(_serverNo); /*log_err("dup-packket at _seqNo=%lu", _seqNo);*/
		}
	}

	void incDupCount    (uint64_t serverNo) {m_pErrors[serverNo].duplicates++;}
	void incOooCount    (uint64_t serverNo) {m_pErrors[serverNo].ooo++;}
	void incDroppedCount(uint64_t serverNo) {m_pErrors[serverNo].dropped++;}

	size_t getDupCount    (uint64_t serverNo) {return m_pErrors[serverNo].duplicates;}
	size_t getOooCount    (uint64_t serverNo) {return m_pErrors[serverNo].ooo;}
	size_t getDroppedCount(uint64_t serverNo) {return m_pErrors[serverNo].dropped;}

	const uint64_t m_maxSequenceNo;
	const uint64_t m_replyEvery;
	const uint64_t m_blockSize;
private:
	TicksTime * const m_pTimes;
	TicksTime * const m_pInternalUse;

	//prevent creation by compiler
	PacketTimes(const PacketTimes&);
	PacketTimes & operator= (const PacketTimes&);

	struct ArrivalErrors {
		size_t duplicates;
		size_t ooo; //out-of-order
		size_t dropped;
		ArrivalErrors() : duplicates(0), ooo(0), dropped(0) {}
	};
	ArrivalErrors * const m_pErrors; //array with one line per server
/*
	//------------------------------------------------------------------------------
	void dumpFullLog(FILE * f)
	{
		if (!f) return;

		fprintf(f, "------------------------------\n");
		const uint64_t NUM_SERVERS = m_blockSize -1;
		fprintf(f, "txTime");
		for (uint64_t i = 0; i < NUM_SERVERS; i++) fprintf(f, ", rxTime%d", i);
		fprintf(f, "\n");

		//

		for (uint64_t i = 0; i <  m_maxSequenceNo; i++) {
			printf("%.9lf", debugToNsec())
			double tx = (double)pFullLog[i][0].debugToNsec()/1000/1000/1000;
			double rx = (double)pFullLog[i][1].debugToNsec()/1000/1000/1000;
			fprintf(f, "%.9lf, %.9lf\n", tx, rx);
		}
		fprintf(f, "------------------------------\n");
	}

*/

};

#endif /* PACKETTIMES_H_ */
