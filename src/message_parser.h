/*
 * Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef MESSAGE_PARSER_H_
#define MESSAGE_PARSER_H_

#include <cstring>

#include "defs.h"

template <class AccumulationStrategy>
class MessageParser {
protected:
    Message *m_pMsgReply;

public:
    inline MessageParser(Message *msg) : m_pMsgReply(msg)
    {
    }

    /** Process next buffer
     * @param [inout] callback object to invoke handle_message method when
     *      message is ready to be processed
     * @param [inout] recv_data receive stream context
     * @param [in] buf buffer start
     * @param [in] len buffer length
     * @return false on message parsing or processing error, otherwise true
     */
    template <class Callback>
    inline bool process_buffer(Callback &callback, SocketRecvData &recv_data, uint8_t *buf, int len)
    {
        bool direct = (recv_data.cur_offset == 0);
        int offset = 0;

        if (direct) {
            // message accumulation not started
            recv_data.cur_addr = buf;
        }

        while (len > 0) {
            int chunk_len = len;
            if (unlikely(!direct)) {
                // (1) copy data chunk to accumulation buffer
                chunk_len = (recv_data.cur_size < len) ? recv_data.cur_size : len;
                AccumulationStrategy::accumulate_more(recv_data, buf + offset, chunk_len);
            }
            int nprocessed;
            process_chunk(recv_data, chunk_len, nprocessed);
            len -= nprocessed;
            offset += nprocessed;

            if (recv_data.cur_offset == 0) {
                // message is ready to be processed
                bool ok = callback.handle_message();
                if (unlikely(!ok)) {
                    return false;
                }
                buf += offset;
                if (likely(direct)) {
                    recv_data.cur_addr += offset;
                } else {
                    // switch back from accumulation buffer to direct input buffer
                    recv_data.cur_addr = buf;
                    direct = true;
                }
                offset = 0;
            }
        }

        if (likely(recv_data.cur_offset == 0)) {
            // reset read offset to the start of the buffer
            AccumulationStrategy::reset_read_buffer(recv_data);
        } else if (likely(direct)) {
            // Pending message is in a direct buffer => copy it into accumulation buffer.
            // If message is already in accumulation buffer then copy is already performed in (1).
            AccumulationStrategy::start_accumulation(recv_data, buf, recv_data.cur_offset);
        }

        return true;
    }

    inline void process_chunk(SocketRecvData &recv_data, int nbytes, int &nprocessed)
    {
        nprocessed = 0;
        /* 1: message header is not received yet */
        if ((recv_data.cur_offset + nbytes) < MsgHeader::EFFECTIVE_SIZE) {
            recv_data.cur_size -= nbytes;
            recv_data.cur_offset += nbytes;
            nprocessed = nbytes;

            /* 4: set current buffer size to size of remained part of message header to
             *    guarantee getting full message header on next iterations
             */
            if (recv_data.cur_size < MsgHeader::EFFECTIVE_SIZE) {
                recv_data.cur_size = MsgHeader::EFFECTIVE_SIZE - recv_data.cur_offset;
            }
            return;
        } else if (recv_data.cur_offset < MsgHeader::EFFECTIVE_SIZE) {
            /* 2: message header is got, match message to cycle buffer */
            m_pMsgReply->setBuf(recv_data.cur_addr);
            m_pMsgReply->setHeaderToHost();
        } else {
            /* 2: message header is got, match message to cycle buffer */
            m_pMsgReply->setBuf(recv_data.cur_addr);
        }

        if (unlikely(!m_pMsgReply->isValidHeader())) {
            // Message received was larger than expected, message ignored. - only on stream mode.
            // Mark message as done and forward it to the handler.
            nprocessed = nbytes;
            recv_data.cur_size = recv_data.max_size;
            recv_data.cur_offset = 0;
            return;
        }

        /* 3: message is not complete */
        if ((recv_data.cur_offset + nbytes) < m_pMsgReply->getLength()) {
            recv_data.cur_size -= nbytes;
            recv_data.cur_offset += nbytes;
            nprocessed = nbytes;

            /* 4: set current buffer size to size of remained part of message to
             *    guarantee getting full message on next iterations (using extended reserved memory)
             */
            if (recv_data.cur_size < (int)m_pMsgReply->getMaxSize()) {
                recv_data.cur_size = m_pMsgReply->getLength() - recv_data.cur_offset;
            }
            return;
        }

        nprocessed = m_pMsgReply->getLength() - recv_data.cur_offset;
        nbytes -= nprocessed;
        if (nbytes != 0) {
            /* 5: message is complete. shift to process the next one */
            recv_data.cur_size -= m_pMsgReply->getLength() - recv_data.cur_offset;
            recv_data.cur_offset = 0;
        } else {
            /* 6: shift to start of cycle buffer in case receiving buffer is empty and
             * there is no uncompleted message
             */
            recv_data.cur_size = recv_data.max_size;
            recv_data.cur_offset = 0;
        }
    }
};

/**
 * This accumulation method has the following requirements:
 * * recv_data.buf is big enough (>= 2 * MAX_PAYLOAD_SIZE)
 * * incoming data is read to recv_data.cur_addr address
 * * incoming data chunk size is less or equal to recv_data.cur_size
 */
class InPlaceAccumulation {
public:
    inline static void reset_read_buffer(SocketRecvData &recv_data)
    {
        recv_data.cur_addr = recv_data.buf;
    }

    inline static void start_accumulation(SocketRecvData &recv_data, uint8_t *buf, int)
    {
        assert(recv_data.cur_addr == buf);
    }

    inline static void accumulate_more(SocketRecvData &recv_data, uint8_t *buf, int)
    {
        assert(recv_data.cur_addr + recv_data.cur_offset == buf);
    }
};

/**
 * Requirements:
 * * recv_data.buf size is enough to contain one message (>= MAX_PAYLOAD_SIZE)
 */
class BufferAccumulation {
public:
    inline static void reset_read_buffer(SocketRecvData &)
    {
    }

    inline static void start_accumulation(SocketRecvData &recv_data, uint8_t *buf, int size)
    {
        recv_data.cur_addr = recv_data.buf;
        std::memmove(recv_data.cur_addr, buf, size);
    }

    inline static void accumulate_more(SocketRecvData &recv_data, uint8_t *buf, int size)
    {
        std::memmove(recv_data.cur_addr + recv_data.cur_offset, buf, size);
    }
};

#endif // MESSAGE_PARSER_H_
