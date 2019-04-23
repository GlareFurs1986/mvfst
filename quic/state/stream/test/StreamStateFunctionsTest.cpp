/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 */

// Copyright 2004-present Facebook. All Rights Reserved.

#include <quic/state/stream/StreamStateFunctions.h>
#include <gtest/gtest.h>
#include <quic/server/state/ServerStateMachine.h>
#include <quic/state/QuicStreamFunctions.h>
#include <quic/state/stream/StreamStateMachine.h>

using namespace ::testing;

namespace quic {
namespace test {

class StreamStateFunctionsTests : public Test {};

TEST_F(StreamStateFunctionsTests, SanityTest) {
  QuicServerConnectionState conn;
  StreamId streamId = 0xbaad;
  QuicStreamState stream(streamId, conn);
  appendDataToReadBuffer(
      stream, StreamBuffer(folly::IOBuf::copyBuffer("It is a hotdog!"), 0));
  appendDataToReadBuffer(
      stream,
      StreamBuffer(folly::IOBuf::copyBuffer(" It is not a hotdog."), 15));
  writeDataToQuicStream(
      stream, folly::IOBuf::copyBuffer("What is it then?"), false);
  stream.retransmissionBuffer.emplace_back(
      folly::IOBuf::copyBuffer("How would I know?"), 34);
  auto currentWriteOffset = stream.currentWriteOffset;
  auto currentReadOffset = stream.currentReadOffset;
  EXPECT_TRUE(stream.writable());

  invokeHandler<StreamStateMachine>(
      stream, StreamEvents::SendReset(ApplicationErrorCode::STOPPING));
  // Something are cleared:
  EXPECT_TRUE(stream.writeBuffer.empty());
  EXPECT_TRUE(stream.retransmissionBuffer.empty());
  EXPECT_TRUE(stream.readBuffer.empty());
  EXPECT_TRUE(stream.streamReadError.hasValue());

  // The rest are untouched:
  EXPECT_EQ(stream.id, streamId);
  EXPECT_EQ(currentReadOffset, stream.currentReadOffset);
  EXPECT_EQ(currentWriteOffset, stream.currentWriteOffset);
  EXPECT_FALSE(stream.writable());
}

TEST_F(StreamStateFunctionsTests, IsAllDataReceivedEmptyStream) {
  QuicServerConnectionState conn;
  StreamId id = 3;
  QuicStreamState stream(id, conn);
  EXPECT_FALSE(isAllDataReceived(stream));
}

TEST_F(StreamStateFunctionsTests, IsAllDataReceivedReadBufferHasHole) {
  QuicServerConnectionState conn;
  StreamId id = 3;
  QuicStreamState stream(id, conn);
  stream.currentReadOffset = 100;
  appendDataToReadBuffer(
      stream,
      StreamBuffer(
          folly::IOBuf::copyBuffer("Your read buffer has a hole"), 150, true));
  EXPECT_FALSE(isAllDataReceived(stream));
}

TEST_F(StreamStateFunctionsTests, IsAllDataReceivedReadBufferNoHoleNoFin) {
  QuicServerConnectionState conn;
  StreamId id = 3;
  QuicStreamState stream(id, conn);
  stream.currentReadOffset = 100;
  appendDataToReadBuffer(
      stream,
      StreamBuffer(folly::IOBuf::copyBuffer("Your haven't seen FIN yet"), 100));
  EXPECT_FALSE(isAllDataReceived(stream));
}

TEST_F(StreamStateFunctionsTests, IsAllDataReceivedReadBufferEmptyBufferFin) {
  QuicServerConnectionState conn;
  StreamId id = 3;
  QuicStreamState stream(id, conn);
  stream.currentReadOffset = 100;
  appendDataToReadBuffer(
      stream, StreamBuffer(folly::IOBuf::create(0), 100, true));
  EXPECT_TRUE(isAllDataReceived(stream));
}

TEST_F(StreamStateFunctionsTests, IsAllDataReceivedReadBufferBufferFin) {
  QuicServerConnectionState conn;
  StreamId id = 3;
  QuicStreamState stream(id, conn);
  stream.currentReadOffset = 100;
  appendDataToReadBuffer(
      stream,
      StreamBuffer(
          folly::IOBuf::copyBuffer("you may say im a dreamer"), 100, true));
  EXPECT_TRUE(isAllDataReceived(stream));
}

TEST_F(StreamStateFunctionsTests, IsAllDataReceivedMultipleStreamDataNoHole) {
  QuicServerConnectionState conn;
  StreamId id = 3;
  QuicStreamState stream(id, conn);
  stream.currentReadOffset = 100;
  appendDataToReadBuffer(
      stream, StreamBuffer(folly::IOBuf::copyBuffer("0123456789"), 100));
  appendDataToReadBuffer(
      stream,
      StreamBuffer(folly::IOBuf::copyBuffer("01234567890123456789"), 110));
  appendDataToReadBuffer(
      stream,
      StreamBuffer(folly::IOBuf::copyBuffer("Counting is hard"), 130, true));
  EXPECT_TRUE(isAllDataReceived(stream));
}

TEST_F(StreamStateFunctionsTests, IsAllDataReceivedMultipleStreamDataHasHole) {
  QuicServerConnectionState conn;
  StreamId id = 3;
  QuicStreamState stream(id, conn);
  stream.currentReadOffset = 100;
  appendDataToReadBuffer(
      stream, StreamBuffer(folly::IOBuf::copyBuffer("0123456789"), 100));
  appendDataToReadBuffer(
      stream,
      StreamBuffer(folly::IOBuf::copyBuffer("01234567890123456789"), 115));
  appendDataToReadBuffer(
      stream,
      StreamBuffer(folly::IOBuf::copyBuffer("Counting is hard"), 130, true));
  EXPECT_FALSE(isAllDataReceived(stream));
}

TEST_F(StreamStateFunctionsTests, IsAllDataReceivedAllDataRead) {
  QuicServerConnectionState conn;
  StreamId id = 3;
  QuicStreamState stream(id, conn);
  stream.currentReadOffset = 101;
  stream.finalReadOffset = 100;
  EXPECT_TRUE(isAllDataReceived(stream));
}

TEST_F(StreamStateFunctionsTests, SendReset) {
  QuicServerConnectionState conn;
  StreamId id = 1;
  QuicStreamState stream(id, conn);
  writeDataToQuicStream(stream, folly::IOBuf::copyBuffer("hello"), true);
  EXPECT_GT(conn.flowControlState.sumCurStreamBufferLen, 0);
  appendDataToReadBuffer(
      stream, StreamBuffer(folly::IOBuf::copyBuffer("hi"), 0));
  EXPECT_FALSE(stream.writeBuffer.empty());
  EXPECT_FALSE(stream.readBuffer.empty());
  resetQuicStream(stream, ApplicationErrorCode::STOPPING);

  EXPECT_TRUE(stream.writeBuffer.empty());
  EXPECT_TRUE(stream.readBuffer.empty());
  EXPECT_EQ(conn.flowControlState.sumCurStreamBufferLen, 0);
}

TEST_F(StreamStateFunctionsTests, ResetNoFlowControlGenerated) {
  QuicServerConnectionState conn;
  StreamId id = 1;
  QuicStreamState stream(id, conn);
  writeDataToQuicStream(stream, folly::IOBuf::copyBuffer("hello"), true);
  EXPECT_GT(conn.flowControlState.sumCurStreamBufferLen, 0);
  RstStreamFrame rst(id, ApplicationErrorCode::STOPPING, 90);

  stream.currentReadOffset = 80;
  stream.maxOffsetObserved = 90;
  stream.flowControlState.advertisedMaxOffset = 100;

  conn.flowControlState.advertisedMaxOffset = 10000;
  conn.flowControlState.sumMaxObservedOffset = 90;
  conn.flowControlState.sumCurReadOffset = 80;
  conn.flowControlState.windowSize = 10000;

  onResetQuicStream(stream, std::move(rst));
  EXPECT_EQ(stream.currentReadOffset, 90);
  EXPECT_EQ(conn.flowControlState.sumCurReadOffset, 90);
  EXPECT_FALSE(conn.pendingEvents.connWindowUpdate);
  EXPECT_EQ(conn.flowControlState.sumCurStreamBufferLen, 0);
}

TEST_F(StreamStateFunctionsTests, ResetFlowControlGenerated) {
  QuicServerConnectionState conn;
  StreamId id = 1;
  QuicStreamState stream(id, conn);
  writeDataToQuicStream(stream, folly::IOBuf::copyBuffer("hello"), true);
  EXPECT_GT(conn.flowControlState.sumCurStreamBufferLen, 0);
  RstStreamFrame rst(id, ApplicationErrorCode::STOPPING, 100);
  stream.currentReadOffset = 80;
  stream.maxOffsetObserved = 90;
  stream.flowControlState.advertisedMaxOffset = 100;

  conn.flowControlState.advertisedMaxOffset = 100;
  conn.flowControlState.sumMaxObservedOffset = 90;
  conn.flowControlState.sumCurReadOffset = 80;
  conn.flowControlState.windowSize = 100;

  onResetQuicStream(stream, std::move(rst));
  EXPECT_EQ(stream.currentReadOffset, 100);
  EXPECT_EQ(conn.flowControlState.sumCurReadOffset, 100);
  EXPECT_TRUE(conn.pendingEvents.connWindowUpdate);
  EXPECT_EQ(conn.flowControlState.sumCurStreamBufferLen, 0);
}

TEST_F(StreamStateFunctionsTests, ResetOffsetNotMatch) {
  QuicServerConnectionState conn;
  StreamId id = 1;
  QuicStreamState stream(id, conn);
  RstStreamFrame rst(id, ApplicationErrorCode::STOPPING, 10);
  stream.currentReadOffset = 20;
  stream.maxOffsetObserved = 100;
  stream.finalReadOffset = 100;
  stream.flowControlState.advertisedMaxOffset = 300;
  EXPECT_THROW(
      onResetQuicStream(stream, std::move(rst)), QuicTransportException);
}

TEST_F(StreamStateFunctionsTests, ResetOffsetLessThanMaxObserved) {
  QuicServerConnectionState conn;
  StreamId id = 1;
  QuicStreamState stream(id, conn);
  RstStreamFrame rst(id, ApplicationErrorCode::STOPPING, 30);
  stream.currentReadOffset = 20;
  stream.maxOffsetObserved = 100;
  stream.flowControlState.advertisedMaxOffset = 300;
  EXPECT_THROW(
      onResetQuicStream(stream, std::move(rst)), QuicTransportException);
}

TEST_F(StreamStateFunctionsTests, ResetOffsetGreaterThanStreamFlowControl) {
  QuicServerConnectionState conn;
  StreamId id = 1;
  QuicStreamState stream(id, conn);
  RstStreamFrame rst(id, ApplicationErrorCode::STOPPING, 200);
  stream.currentReadOffset = 20;
  stream.maxOffsetObserved = 30;
  stream.flowControlState.advertisedMaxOffset = 100;
  EXPECT_THROW(
      onResetQuicStream(stream, std::move(rst)), QuicTransportException);
}

TEST_F(StreamStateFunctionsTests, ResetOffsetGreaterThanConnFlowControl) {
  QuicServerConnectionState conn;
  StreamId id = 1;
  QuicStreamState stream(id, conn);
  RstStreamFrame rst(id, ApplicationErrorCode::STOPPING, 200);

  stream.currentReadOffset = 20;
  stream.maxOffsetObserved = 30;
  stream.flowControlState.advertisedMaxOffset = 300;
  stream.flowControlState.windowSize = 100;

  conn.flowControlState.sumCurReadOffset = 20;
  conn.flowControlState.sumMaxObservedOffset = 30;
  conn.flowControlState.advertisedMaxOffset = 100;
  conn.flowControlState.windowSize = 100;
  EXPECT_THROW(
      onResetQuicStream(stream, std::move(rst)), QuicTransportException);
}

TEST_F(StreamStateFunctionsTests, ResetAfterReadingAllBytesTillFin) {
  QuicServerConnectionState conn;
  StreamId id = 1;
  QuicStreamState stream(id, conn);
  RstStreamFrame rst(id, ApplicationErrorCode::STOPPING, 100);
  stream.currentReadOffset = 101;
  stream.finalReadOffset = 100;
  stream.maxOffsetObserved = 100;
  stream.flowControlState.advertisedMaxOffset = 300;
  onResetQuicStream(stream, std::move(rst));
  EXPECT_EQ(stream.currentReadOffset, 101);
  EXPECT_FALSE(conn.streamManager->hasWindowUpdates());
  EXPECT_FALSE(conn.pendingEvents.connWindowUpdate);
}
} // namespace test
} // namespace quic
