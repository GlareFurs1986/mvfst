/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 */

#include <quic/server/handshake/StatelessResetGenerator.h>

#include <folly/Range.h>

namespace {
constexpr folly::StringPiece kSalt{"Stateless reset"};
}

namespace quic {

StatelessResetGenerator::StatelessResetGenerator(
    StatelessResetSecret secret,
    const std::string& addressStr)
    : secret_(std::move(secret)), addressStr_(std::move(addressStr)) {
  extractedSecret_ = hdkf_.extract(kSalt, folly::range(secret));
}

StatelessResetToken StatelessResetGenerator::generateToken(
    const ConnectionId& connId) const {
  StatelessResetToken token;
  auto info = toData(connId);
  info.prependChain(
      folly::IOBuf::wrapBuffer(addressStr_.data(), addressStr_.size()));
  auto out = hdkf_.expand(folly::range(extractedSecret_), info, token.size());
  out->coalesce();
  memcpy(token.data(), out->data(), out->length());
  return token;
}

} // namespace quic
