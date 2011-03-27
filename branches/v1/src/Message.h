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
#ifndef MESSAGE_H_
#define MESSAGE_H_

#include <exception>
#include <stdexcept>
#include <stdint.h>// for uint64_t

class MsgHeader {
	friend class Message;
public:
	MsgHeader(uint64_t _sequence_number = 0) : m_sequence_number(_sequence_number){}
	~MsgHeader(){}

//	uint32_t isClient() const {return m_isClient;}
//	void setClient() {m_isClient = 1;}
//	void setServer() {m_isClient = 0;}
//	uint32_t isPongRequest() const {return m_isPongRequest;}
//	void setPongRequest(bool on = true) {m_isPongRequest = on ? 1 : 0;}

	uint32_t isClient() const {return m_flags & MASK_CLIENT;}
	void setClient() {m_flags |=  MASK_CLIENT;}
	void setServer() {m_flags &= ~MASK_CLIENT;}

	uint32_t isPongRequest() const {return m_flags & MASK_PONG;}
	void setPongRequest()   {m_flags |=  MASK_PONG;}
	void resetPongRequest() {m_flags &= ~MASK_PONG;}

	uint32_t isWarmupMessage() const {return m_flags & MASK_WARMUP_MSG;}
	void setWarmupMessage()   {m_flags |=  MASK_WARMUP_MSG;}
	void resetWarmupMessage() {m_flags &= ~MASK_WARMUP_MSG;}

	// this is different than sizeof(MsgHeader) and safe only for the current implementation of the class
	static const int EFFECTIVE_SIZE = (int)( sizeof(uint64_t) + sizeof(uint32_t) );
//	static const int EFFECTIVE_SIZE = 16;

private:
	// NOTE: m_sequence_number must be the 1st field, because we want EFFECTIVE_SIZE of header to be 12 bytes
	// hence we need the padding (to 16 bytes) to be after last field and not between fields
	uint64_t m_sequence_number;
	uint32_t m_flags;

	static const uint32_t MASK_CLIENT=1;
	static const uint32_t MASK_PONG  =2;
	static const uint32_t MASK_WARMUP_MSG =4;
/*
	uint32_t m_isClient:1;
	uint32_t m_isPongRequest:1;
	uint32_t m_reservedFlags:6;
	uint32_t m_reserved:24;
*/
};


class Message {
public:
	Message() throw (std::exception); //throws in case of fatal error
	~Message();

	static void initMaxSize(int size) throw (std::exception); //throws in case of fatal error
	static size_t getMaxSize() {return ms_maxSize;}

	uint8_t * getData() const {return m_data;}
	const MsgHeader * getHeader() const {return m_header;}
	MsgHeader * getHeader() {return m_header;}

	uint32_t isClient() const {return m_header->isClient();}
	void setClient() {m_header->setClient();}
	void setServer() {m_header->setServer();}

	uint32_t isPongRequest() const {return m_header->isPongRequest();}

	uint64_t getSequenceCounter() const {return m_header->m_sequence_number;}
	const uint64_t & getSequenceCounterRef() const {return m_header->m_sequence_number;}
	void setSequenceCounter(uint64_t _sequence) {m_header->m_sequence_number = _sequence;}
	void incSequenceCounter() {m_header->m_sequence_number++;}

	uint32_t isWarmupMessage() const {return m_header->isWarmupMessage();}
	void setWarmupMessage()   {m_header->setWarmupMessage();}
	void resetWarmupMessage() {m_header->resetWarmupMessage();}

	uint32_t getFlags() const {return m_header->m_flags;}

private:
	void *m_buf;
	uint8_t *m_data; //points to 1st 8 aligned adrs inside m_buf
	MsgHeader *m_header; //points to same adrs as m_data

	static int ms_maxSize; // use int (instead of size_t to save casting to 'int' in recvfrom)
};

#endif /* MESSAGE_H_ */
