/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 */

// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <quic/codec/Types.h>
#include <quic/state/QPRFunctions.h>
#include <quic/state/StateData.h>

namespace quic {

/*
 * Initiate a send of the given simple frame.
 */
void sendSimpleFrame(QuicConnectionStateBase& conn, QuicSimpleFrame frame);

/*
 * Update the connection state on ack of the given simple frame.
 */
void updateSimpleFrameOnAck(
    QuicConnectionStateBase& conn,
    const QuicSimpleFrame& frame);

/*
 * Update connection state and the frame on clone of the given simple frame.
 * Returns the updated simple frame.
 */
folly::Optional<QuicSimpleFrame> updateSimpleFrameOnPacketClone(
    QuicConnectionStateBase& conn,
    const QuicSimpleFrame& frame);

/*
 * Update the connection state after sending the given simple frame.
 */
void updateSimpleFrameOnPacketSent(
    QuicConnectionStateBase& conn,
    const QuicSimpleFrame& simpleFrame);

/*
 * Update the connection state after loss of a given simple frame.
 */

void updateSimpleFrameOnPacketLoss(
    QuicConnectionStateBase& conn,
    const QuicSimpleFrame& frame);

/*
 * Update the connection state on receipt of the given simple frame.
 * Returns true if the frame is not a probing frame
 */
bool updateSimpleFrameOnPacketReceived(
    QuicConnectionStateBase& conn,
    const QuicSimpleFrame& frameIn,
    PacketNum packetNum,
    bool fromChangedPeerAddress);
} // namespace quic
