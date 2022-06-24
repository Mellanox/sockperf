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

#include <cstring>
#include <vector>

#include "googletest/include/gtest/gtest.h"

#include "message_parser.h"

template <class AccumulationStrategy>
class TestMessageParser : public MessageParser<AccumulationStrategy> {
private:
    size_t m_numReceived;
    std::vector<Message *> m_expectedMessages;
    Message m_lastMsg;

public:
    TestMessageParser() : MessageParser<AccumulationStrategy>(&m_lastMsg),
        m_numReceived(0)
    {
        m_lastMsg.setLength(MAX_PAYLOAD_SIZE);
    }

    ~TestMessageParser()
    {
        for (size_t i = 0; i < m_expectedMessages.size(); ++i) {
            delete m_expectedMessages[i];
        }
    }

    void addExpectedMessage(const Message &msg)
    {
        m_expectedMessages.push_back(new Message());
        Message *m = m_expectedMessages.back();
        m->setBuf();
        std::memcpy(m->getBuf(), msg.getBuf(), msg.getLength());
    }

    bool handle_message()
    {
        EXPECT_TRUE(m_numReceived < m_expectedMessages.size()) << "too many messages";
        Message *expectedMsg = m_expectedMessages[m_numReceived];
        EXPECT_EQ(expectedMsg->isValidHeader(), m_lastMsg.isValidHeader());
        if (!expectedMsg->isValidHeader()) {
            ++m_numReceived;
            return false;
        }
        EXPECT_EQ(expectedMsg->getLength(), m_lastMsg.getLength())
            << "message " << m_numReceived << " has unexpected size";
        EXPECT_TRUE(std::memcmp(m_lastMsg.getBuf(), expectedMsg->getBuf(), expectedMsg->getLength()) == 0)
            << "message " << m_numReceived << " has unexpected content";
        ++m_numReceived;

        return true;
    }

    void checkReceivedMessages()
    {
        ASSERT_EQ(m_expectedMessages.size(), m_numReceived) << "invalid number of received messages";
    }
};

template <class AccumulationStrategy>
class MessageParserTestTmpl : public ::testing::Test {
private:
    std::vector<uint8_t> m_accumulateBuf;
    std::vector<uint8_t> m_dataBuf;

protected:
    SocketRecvData recv_data;
    TestMessageParser<AccumulationStrategy> handler;
    uint8_t *buf;

    MessageParserTestTmpl()
    {
        m_accumulateBuf.resize(2 * MAX_PAYLOAD_SIZE);
        m_dataBuf.resize(65536, 0); // big enough
        buf = m_dataBuf.data();

        recv_data.buf = m_accumulateBuf.data();
        recv_data.max_size = MAX_PAYLOAD_SIZE;
        recv_data.cur_addr = recv_data.buf;
        recv_data.cur_offset = 0;
        recv_data.cur_size = recv_data.max_size;
    }

    void setChunkSize(size_t size)
    {
        recv_data.max_size = size;
        recv_data.cur_size = size;
    }

    void check_process_buffer(SocketRecvData &recv_data, uint8_t *buf, size_t size, bool expected = true)
    {
        bool ok = handler.process_buffer(handler, recv_data, buf, size);
        EXPECT_EQ(expected, ok) << "parse error is not expected";
    }
};

typedef MessageParserTestTmpl<InPlaceAccumulation> MessageParserInplaceTest;
typedef MessageParserTestTmpl<BufferAccumulation> MessageParserBufferedTest;

TEST_F(MessageParserInplaceTest, SingleMessageInSingleBuffer)
{
    Message msg;
    msg.setBuf(buf);
    msg.setLength(MsgHeader::EFFECTIVE_SIZE);
    msg.resetWarmupMessage();
    msg.setClient();
    msg.setSequenceCounter(0);

    handler.addExpectedMessage(msg);
    msg.setHeaderToNetwork();

    check_process_buffer(recv_data, buf, MsgHeader::EFFECTIVE_SIZE);
    ASSERT_EQ(recv_data.cur_offset, 0) << "message accumulation is not expected";
    ASSERT_EQ(recv_data.cur_size, recv_data.max_size) << "expected max buffer size";
    ASSERT_EQ(recv_data.cur_addr, recv_data.buf) << "accumulation pointer should point to the start of the buffer";

    handler.checkReceivedMessages();
}

TEST_F(MessageParserInplaceTest, HandleBadMessage)
{
    Message msg;
    msg.setBuf(buf);
    msg.setLength(MAX_PAYLOAD_SIZE + 1);
    msg.resetWarmupMessage();
    msg.setClient();
    msg.setSequenceCounter(0);

    handler.addExpectedMessage(msg);
    msg.setHeaderToNetwork();

    check_process_buffer(recv_data, buf, MsgHeader::EFFECTIVE_SIZE, false);
    ASSERT_EQ(recv_data.cur_offset, 0) << "message accumulation is not expected";
    ASSERT_EQ(recv_data.cur_size, recv_data.max_size) << "expected max buffer size";

    handler.checkReceivedMessages();
}

TEST_F(MessageParserInplaceTest, ThreeMessagesInSingleBuffer)
{
    // 1st
    Message msg1;
    msg1.setBuf(buf);
    msg1.setLength(MsgHeader::EFFECTIVE_SIZE);
    msg1.resetWarmupMessage();
    msg1.setClient();
    msg1.setSequenceCounter(0);
    handler.addExpectedMessage(msg1);
    msg1.setHeaderToNetwork();
    // 2nd
    Message msg2;
    msg2.setBuf(buf + MsgHeader::EFFECTIVE_SIZE);
    msg2.setLength(MsgHeader::EFFECTIVE_SIZE);
    msg2.resetWarmupMessage();
    msg2.setClient();
    msg2.setSequenceCounter(1);
    handler.addExpectedMessage(msg2);
    msg2.setHeaderToNetwork();
    // 3rd
    Message msg3;
    msg3.setBuf(buf + 2 * MsgHeader::EFFECTIVE_SIZE);
    msg3.setLength(MsgHeader::EFFECTIVE_SIZE);
    msg3.resetWarmupMessage();
    msg3.setClient();
    msg3.setSequenceCounter(2);
    handler.addExpectedMessage(msg3);
    msg3.setHeaderToNetwork();

    check_process_buffer(recv_data, buf, 3 * MsgHeader::EFFECTIVE_SIZE);
    ASSERT_EQ(recv_data.cur_offset, 0) << "message accumulation is not expected";
    ASSERT_EQ(recv_data.cur_size, recv_data.max_size) << "expected max buffer size";
    ASSERT_EQ(recv_data.cur_addr, recv_data.buf) << "accumulation pointer should point to the start of the buffer";

    handler.checkReceivedMessages();
}

TEST_F(MessageParserInplaceTest, MessageIsSplittedAcrossTwoBuffers)
{
    setChunkSize(14 + 7);

    // 1st
    Message msg1;
    msg1.setBuf(buf);
    msg1.setLength(MsgHeader::EFFECTIVE_SIZE);
    msg1.resetWarmupMessage();
    msg1.setClient();
    msg1.setSequenceCounter(0);
    handler.addExpectedMessage(msg1);
    msg1.setHeaderToNetwork();
    // 2nd - splitted
    Message msg2;
    msg2.setBuf(buf + MsgHeader::EFFECTIVE_SIZE);
    msg2.setLength(MsgHeader::EFFECTIVE_SIZE);
    msg2.resetWarmupMessage();
    msg2.setClient();
    msg2.setSequenceCounter(1);
    handler.addExpectedMessage(msg2);
    msg2.setHeaderToNetwork();
    // 3rd - contiguous, so can be directly accessed
    Message msg3;
    msg3.setBuf(buf + 2 * MsgHeader::EFFECTIVE_SIZE);
    msg3.setLength(MsgHeader::EFFECTIVE_SIZE);
    msg3.resetWarmupMessage();
    msg3.setClient();
    msg3.setSequenceCounter(2);
    handler.addExpectedMessage(msg3);
    msg3.setHeaderToNetwork();

    check_process_buffer(recv_data, buf, 14 + 7);
    ASSERT_EQ(recv_data.cur_size, 7) << "expected 7 more bytes";
    ASSERT_EQ(recv_data.cur_offset, 7) << "message accumulation is expected";

    check_process_buffer(recv_data, buf + 14 + 7, 14 + 7);
    ASSERT_EQ(recv_data.cur_offset, 0) << "message accumulation is not expected";
    ASSERT_EQ(recv_data.cur_size, recv_data.max_size) << "expected max buffer size";
    ASSERT_EQ(recv_data.cur_addr, recv_data.buf) << "accumulation pointer should point to the start of the buffer";

    handler.checkReceivedMessages();
}

TEST_F(MessageParserBufferedTest, MessageIsSplittedAcrossTwoBuffers)
{
    setChunkSize(14 + 7);

    // 1st
    Message msg1;
    msg1.setBuf(buf);
    msg1.setLength(MsgHeader::EFFECTIVE_SIZE);
    msg1.resetWarmupMessage();
    msg1.setClient();
    msg1.setSequenceCounter(0);
    handler.addExpectedMessage(msg1);
    msg1.setHeaderToNetwork();
    // 2nd - splitted
    Message msg2;
    msg2.setBuf(buf + MsgHeader::EFFECTIVE_SIZE);
    msg2.setLength(MsgHeader::EFFECTIVE_SIZE);
    msg2.resetWarmupMessage();
    msg2.setClient();
    msg2.setSequenceCounter(1);
    handler.addExpectedMessage(msg2);
    msg2.setHeaderToNetwork();
    // 3rd - contiguous, so can be directly accessed
    Message msg3;
    msg3.setBuf(buf + 2 * MsgHeader::EFFECTIVE_SIZE);
    msg3.setLength(MsgHeader::EFFECTIVE_SIZE);
    msg3.resetWarmupMessage();
    msg3.setClient();
    msg3.setSequenceCounter(2);
    handler.addExpectedMessage(msg3);
    msg3.setHeaderToNetwork();

    check_process_buffer(recv_data, buf, 14 + 7);
    ASSERT_EQ(recv_data.cur_size, 7) << "expected 7 more bytes";
    ASSERT_EQ(recv_data.cur_offset, 7) << "message accumulation is expected";

    std::memset(buf, 0xff, 14);
    check_process_buffer(recv_data, buf + 14 + 7, 14 + 7);
    ASSERT_EQ(recv_data.cur_offset, 0) << "message accumulation is not expected";
    ASSERT_EQ(recv_data.cur_size, recv_data.max_size) << "expected max buffer size";

    handler.checkReceivedMessages();
}

TEST_F(MessageParserInplaceTest, MessageIsSplittedAcrossThreeBuffers)
{
    setChunkSize(20);

    // 1st
    Message msg1;
    msg1.setBuf(buf);
    msg1.setLength(MsgHeader::EFFECTIVE_SIZE);
    msg1.resetWarmupMessage();
    msg1.setClient();
    msg1.setSequenceCounter(0);
    handler.addExpectedMessage(msg1);
    msg1.setHeaderToNetwork();
    // 2nd
    Message msg2;
    msg2.setBuf(buf + MsgHeader::EFFECTIVE_SIZE);
    msg2.setLength(30);
    msg2.resetWarmupMessage();
    msg2.setClient();
    msg2.setSequenceCounter(1);
    handler.addExpectedMessage(msg2);
    msg2.setHeaderToNetwork();

    check_process_buffer(recv_data, buf, 20);
    ASSERT_EQ(recv_data.cur_size, 8) << "expected 6 more bytes (remaining header)";
    ASSERT_EQ(recv_data.cur_offset, 6) << "message accumulation is expected";

    check_process_buffer(recv_data, buf + 20, 20);
    ASSERT_EQ(recv_data.cur_size, 4) << "expected 4 more bytes";
    ASSERT_EQ(recv_data.cur_offset, 26) << "message accumulation is expected";

    check_process_buffer(recv_data, buf + 40, 4);
    ASSERT_EQ(recv_data.cur_offset, 0) << "message accumulation is not expected";
    ASSERT_EQ(recv_data.cur_size, recv_data.max_size) << "expected max buffer size";
    ASSERT_EQ(recv_data.cur_addr, recv_data.buf) << "accumulation pointer should point to the start of the buffer";

    handler.checkReceivedMessages();
}

TEST_F(MessageParserBufferedTest, MessageIsSplittedAcrossThreeBuffers)
{
    setChunkSize(20);

    // 1st
    Message msg1;
    msg1.setBuf(buf);
    msg1.setLength(MsgHeader::EFFECTIVE_SIZE);
    msg1.resetWarmupMessage();
    msg1.setClient();
    msg1.setSequenceCounter(0);
    handler.addExpectedMessage(msg1);
    msg1.setHeaderToNetwork();
    // 2nd
    Message msg2;
    msg2.setBuf(buf + MsgHeader::EFFECTIVE_SIZE);
    msg2.setLength(30);
    msg2.resetWarmupMessage();
    msg2.setClient();
    msg2.setSequenceCounter(1);
    handler.addExpectedMessage(msg2);
    msg2.setHeaderToNetwork();

    check_process_buffer(recv_data, buf, 20);
    ASSERT_EQ(recv_data.cur_size, 8) << "expected 6 more bytes (remaining header)";
    ASSERT_EQ(recv_data.cur_offset, 6) << "message accumulation is expected";

    std::memset(buf, 0xff, 20);
    check_process_buffer(recv_data, buf + 20, 20);
    ASSERT_EQ(recv_data.cur_size, 4) << "expected 4 more bytes";
    ASSERT_EQ(recv_data.cur_offset, 26) << "message accumulation is expected";

    std::memset(buf + 20, 0xff, 20);
    check_process_buffer(recv_data, buf + 40, 4);
    ASSERT_EQ(recv_data.cur_offset, 0) << "message accumulation is not expected";
    ASSERT_EQ(recv_data.cur_size, recv_data.max_size) << "expected max buffer size";

    handler.checkReceivedMessages();
}
