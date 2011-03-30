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
#include "Message.h"
#include <malloc.h>
#include <string>
#include "common.h"

// static memebers initialization
/*static*/ uint64_t Message::ms_maxSequenceNo;
/*static*/ int Message::ms_maxSize;

//------------------------------------------------------------------------------
class MemException : public std::exception {
public:
	MemException (const char *file, int line, const char * handler, size_t size) throw();
	virtual ~MemException () throw(){}
	virtual const char* what() const throw() { return m_what.c_str();}
private:
  std::string m_what;
};
//------------------
MemException::MemException (const char *file, int line, const char * handler, size_t size) throw() {
	const size_t LEN = 256;
	char buf[LEN+1];
	snprintf(buf, LEN, "%s:%d: %s failed allocating %d bytes",file, line, handler, (int)size);
	buf[LEN] = '\0';
	m_what = buf;
}

//------------------------------------------------------------------------------
/*static*/ void Message::initMaxSize(int size) throw (std::exception)
{
	if (size < 0)
		throw std::out_of_range("size < 0");
	else if (ms_maxSize)
		throw std::logic_error("MaxSize is already initialized");
	else
		ms_maxSize = size;
}

//------------------------------------------------------------------------------
/*static*/ void Message::initMaxSeqNo(uint64_t seqno) throw (std::exception)
{
	if (ms_maxSequenceNo)
		throw std::logic_error("MaxSeqNo is already initialized");
	else
		ms_maxSequenceNo = seqno;
}

//------------------------------------------------------------------------------
Message::Message()  throw (std::exception) { //throws in case fatal error occurred

	if (! ms_maxSize)
		throw std::logic_error("MaxSize was NOT initialized");

	if (ms_maxSize < MsgHeader::EFFECTIVE_SIZE)
		throw std::out_of_range("maxSize < MsgHeader::EFFECTIVE_SIZE");

	m_buf = MALLOC(ms_maxSize + 7); // extra +7 for enabling 8 alignment of m_sequence_number
	if (! m_buf) {
		throw MemException(__FILE__, __LINE__, "malloc", ms_maxSize);
	}

	setBuf();

	for (int len = 0; len < ms_maxSize; len++)
		m_addr[len] = (uint8_t) rand();
	memset(m_header, 0, MsgHeader::EFFECTIVE_SIZE);

/*
	log_msg("ms_maxSize=%d, m_buf=%p, alignment=%d, m_data=%p, m_header=%p", ms_maxSize, m_buf, alignment, m_data, m_header);
	log_msg("header adresses: m_sequence_number=%p", &m_header->m_sequence_number);
//*/
}

//------------------------------------------------------------------------------
Message::~Message()
{
	if (m_buf) {
		FREE (m_buf);
	}
}

