// Copyright (c) 2021 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <QString>
#include <filesystem>

#include "GrpcProtos/process.pb.h"
#include "OrbitGgp/SshInfo.h"
#include "OrbitSsh/Credentials.h"
#include "SessionSetup/SessionSetupUtils.h"

namespace orbit_session_setup {

TEST(SessionSetupUtils, CredentialsFromSshInfoWorksCorrectly) {
  orbit_ggp::SshInfo info;
  info.host = "127.0.0.1";
  info.key_path = "invalid/key/path";
  info.known_hosts_path = "invalid/known/hosts/path";
  info.port = 123;
  info.user = "some_user";

  orbit_ssh::Credentials credentials = CredentialsFromSshInfo(info);

  EXPECT_EQ(info.host.toStdString(), credentials.addr_and_port.addr);
  EXPECT_EQ(info.port, credentials.addr_and_port.port);
  EXPECT_EQ(std::filesystem::path{info.key_path.toStdString()}, credentials.key_path);
  EXPECT_EQ(std::filesystem::path{info.known_hosts_path.toStdString()},
            credentials.known_hosts_path);
  EXPECT_EQ(info.user.toStdString(), credentials.user);
}

// Tests below need to be adjusted if the name changes, they are conveniently set up
// for process names that are exactly at the length limit
static_assert(kMaxProcessNameLength == 15);

const uint32_t kPid = 100;
const char* kFullProcessName = "ok_process_name_long";
const char* kShortProcessName = "ok_process_name";
const char* kProcessPath = "/path/to/ok_process_name_long";

std::vector<orbit_grpc_protos::ProcessInfo> SetupTestProcessList() {
  using orbit_grpc_protos::ProcessInfo;

  ProcessInfo expected_target_process;
  expected_target_process.set_pid(kPid);
  expected_target_process.set_name(kShortProcessName);
  expected_target_process.set_full_path(kProcessPath);

  ProcessInfo lower_pid_process1;
  lower_pid_process1.set_pid(kPid - 1);
  lower_pid_process1.set_name(kShortProcessName);
  lower_pid_process1.set_full_path(kProcessPath);

  ProcessInfo lower_pid_process2;
  lower_pid_process2.set_pid(kPid - 2);
  lower_pid_process2.set_name(kShortProcessName);
  lower_pid_process2.set_full_path(kProcessPath);

  ProcessInfo different_process1;
  different_process1.set_pid(kPid + 1);
  different_process1.set_name("some_other_process");
  different_process1.set_full_path("/path/to/some_other_process");

  ProcessInfo different_process2;
  different_process1.set_pid(kPid + 2);
  different_process1.set_name("some_other_process");
  different_process1.set_full_path("/path/to/some_other_process");

  // Try to add different combinations of PID sorting order and different process names before and
  // after the expected target process
  return {different_process1, lower_pid_process1, expected_target_process, different_process2,
          lower_pid_process2};
}

TEST(SessionSetupUtils, TryToFindProcessDataFindsProcessByShortName) {
  std::vector<orbit_grpc_protos::ProcessInfo> processes = SetupTestProcessList();

  EXPECT_EQ(kPid, TryToFindProcessData(processes, kShortProcessName)->pid());
}

TEST(SessionSetupUtils, TryToFindProcessDataFindsProcessByLongName) {
  std::vector<orbit_grpc_protos::ProcessInfo> processes = SetupTestProcessList();

  EXPECT_EQ(kPid, TryToFindProcessData(processes, kFullProcessName)->pid());
}

TEST(SessionSetupUtils, TryToFindProcessDataFindsProcessByPath) {
  std::vector<orbit_grpc_protos::ProcessInfo> processes = SetupTestProcessList();

  EXPECT_EQ(kPid, TryToFindProcessData(processes, kProcessPath)->pid());
}

TEST(SessionSetupUtils, TryToFindProcessDataReturnsNullOnFailure) {
  std::vector<orbit_grpc_protos::ProcessInfo> processes = SetupTestProcessList();

  EXPECT_EQ(nullptr, TryToFindProcessData(processes, "nonexisting_process"));
}

}  // namespace orbit_session_setup