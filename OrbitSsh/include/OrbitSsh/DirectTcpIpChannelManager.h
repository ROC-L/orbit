// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_SSH_DIRECT_TCP_IP_CHANNEL_MANAGER_H_
#define ORBIT_SSH_DIRECT_TCP_IP_CHANNEL_MANAGER_H_

#include <libssh2.h>

#include <memory>

#include "Channel.h"
#include "ResultType.h"

namespace OrbitSsh {

// This opens a direct tcp/ip via the remote host to a third party.
// In most cases the third party is a program running on the remote server and
// with a listen socket to 127.0.0.1. To establish the connection, Tick() has to
// be called periodically.
class DirectTcpIpChannelManager {
 public:
  enum class State {
    kNotInitialized,
    kRunning,
    kSentEOFToRemote,
    kRemoteSentEOFBack,
    kWaitRemoteClosed
  };

  DirectTcpIpChannelManager(Session* session_ptr, std::string third_party_host,
                            int third_party_port);
  DirectTcpIpChannelManager() = delete;
  DirectTcpIpChannelManager(const DirectTcpIpChannelManager&) = delete;
  DirectTcpIpChannelManager& operator=(const DirectTcpIpChannelManager&) =
      delete;
  DirectTcpIpChannelManager(DirectTcpIpChannelManager&& other) = default;
  DirectTcpIpChannelManager& operator=(DirectTcpIpChannelManager&& other) =
      default;

  State Tick();
  ResultType Read(std::string* result);
  ResultType WriteBlocking(const std::string& text);
  ResultType Close();

 private:
  State state_ = State::kNotInitialized;
  Session* session_ptr_;
  std::optional<Channel> channel_;
  std::string third_party_host_;
  int third_party_port_;
};

}  // namespace OrbitSsh

#endif  // ORBIT_SSH_DIRECT_TCP_IP_CHANNEL_MANAGER_H_