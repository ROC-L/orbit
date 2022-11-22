// Copyright (c) 2021 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "SessionSetup/SessionSetupUtils.h"

#include <absl/strings/str_format.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/support/channel_arguments.h>

#include <QUrl>
#include <algorithm>
#include <filesystem>

#include "OrbitBase/Logging.h"
#include "OrbitSsh/AddrAndPort.h"

namespace orbit_session_setup {

orbit_ssh::Credentials CredentialsFromSshInfo(const orbit_ggp::SshInfo& ssh_info) {
  orbit_ssh::Credentials credentials;
  credentials.addr_and_port = {ssh_info.host.toStdString(), ssh_info.port};
  credentials.key_path = ssh_info.key_path.toStdString();
  credentials.known_hosts_path = ssh_info.known_hosts_path.toStdString();
  credentials.user = ssh_info.user.toStdString();

  return credentials;
}

std::shared_ptr<grpc::Channel> CreateGrpcChannel(uint16_t port) {
  std::string grpc_server_address = absl::StrFormat("127.0.0.1:%d", port);
  ORBIT_LOG("Starting gRPC channel to: %s", grpc_server_address);
  std::shared_ptr<grpc::Channel> result = grpc::CreateCustomChannel(
      grpc_server_address, grpc::InsecureChannelCredentials(), grpc::ChannelArguments());
  ORBIT_CHECK(result != nullptr);
  return result;
}

std::unique_ptr<orbit_client_data::ProcessData> TryToFindProcessData(
    std::vector<orbit_grpc_protos::ProcessInfo> process_list,
    const std::string& process_name_or_path) {
  std::string shortened_process_name = process_name_or_path.substr(0, kMaxProcessNameLength);

  std::sort(
      process_list.begin(), process_list.end(),
      [](const orbit_grpc_protos::ProcessInfo& lhs,
         const orbit_grpc_protos::ProcessInfo& rhs) -> bool { return lhs.pid() > rhs.pid(); });
  for (auto& process : process_list) {
    if (process.full_path() == process_name_or_path || process.name() == shortened_process_name) {
      return std::make_unique<orbit_client_data::ProcessData>(process);
    }
  }

  return nullptr;
}

std::optional<ConnectionTarget> SplitTargetUri(const QString& target_uri) {
  QUrl url(target_uri);

  if (!url.isValid()) return std::nullopt;
  if (url.scheme() != kCustomProtocol) return std::nullopt;
  if (url.authority().isEmpty()) return std::nullopt;
  if (url.query().isEmpty()) return std::nullopt;

  const QString instance = url.authority() + url.path();
  const QString process = url.query(QUrl::FullyDecoded);

  return ConnectionTarget(process, instance);
}

}  // namespace orbit_session_setup