// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_TRACEPOINTSERVICECLIENT_H
#define ORBIT_TRACEPOINTSERVICECLIENT_H

#include <memory>
#include <vector>

#include "OrbitBase/Result.h"
#include "grpcpp/channel.h"
#include "services.grpc.pb.h"
#include "tracepoint.pb.h"

using orbit_grpc_protos::TracepointInfo;

class TracepointServiceClient {
 public:
  static std::unique_ptr<TracepointServiceClient> Create(
      const std::shared_ptr<grpc::Channel>& channel);

  ErrorMessageOr<std::vector<TracepointInfo>> GetTracepointList() const;

 private:
  TracepointServiceClient() = default;

  TracepointServiceClient(const std::shared_ptr<grpc::Channel>& channel);

  std::unique_ptr<orbit_grpc_protos::TracepointService::Stub> tracepoint_service_;
};

#endif  // ORBIT_TRACEPOINTSERVICECLIENT_H
