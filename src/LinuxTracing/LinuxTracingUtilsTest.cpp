// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <absl/strings/str_format.h>
#include <absl/synchronization/mutex.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <pthread.h>
#include <sys/types.h>
#include <syscall.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "LinuxTracingUtils.h"
#include "OrbitBase/ThreadUtils.h"

namespace orbit_linux_tracing {

TEST(ReadFile, ProcPidCommOfLinuxTracingTests) {
  std::string filename = absl::StrFormat("/proc/%d/comm", getpid());
  std::optional<std::string> returned_comm = ReadFile(filename);
  // Comm values have a size limit of 15 characters.
  std::string expected_comm = std::string{"LinuxTracingTests"}.substr(0, 15).append("\n");
  ASSERT_TRUE(returned_comm.has_value());
  EXPECT_EQ(returned_comm.value(), expected_comm);
}

TEST(GetThreadName, LinuxTracingTests) {
  // Thread names have a length limit of 15 characters.
  std::string expected_name = std::string{"LinuxTracingTests"}.substr(0, 15);
  std::string returned_name = orbit_base::GetThreadName(getpid());
  EXPECT_EQ(returned_name, expected_name);
}

TEST(GetThreadState, LinuxTracingTestsMainAndAnother) {
  pid_t main_tid = syscall(SYS_gettid);

  std::optional<char> main_state_initial = GetThreadState(main_tid);
  ASSERT_TRUE(main_state_initial.has_value());
  EXPECT_EQ('R', main_state_initial.value());

  absl::Mutex mutex;
  pid_t thread_tid = -1;
  std::optional<char> thread_state_holding_mutex;
  std::optional<char> main_state_waiting_mutex;
  std::thread thread{[&] {
    // Make sure /proc/<pid>/stat is parsed correctly
    // even when the thread name contains spaces and parentheses.
    pthread_setname_np(pthread_self(), ") )  )()( )(  )");
    {
      absl::MutexLock lock{&mutex};
      thread_tid = syscall(SYS_gettid);
      thread_state_holding_mutex = GetThreadState(thread_tid);
      main_state_waiting_mutex = GetThreadState(main_tid);
    }
    // Give the main thread the time to read this thread's state before exiting.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }};

  {
    absl::MutexLock lock{&mutex};
    mutex.Await(absl::Condition(
        +[](pid_t* tid) { return *tid != -1; }, &thread_tid));
  }
  ASSERT_TRUE(thread_state_holding_mutex.has_value());
  EXPECT_EQ('R', thread_state_holding_mutex.value());
  ASSERT_TRUE(main_state_waiting_mutex.has_value());
  EXPECT_EQ('S', main_state_waiting_mutex.value());  // Interruptible sleep

  // Make sure `thread` has had the time to call sleep_for.
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
  std::optional<char> thread_state_sleeping = GetThreadState(thread_tid);
  ASSERT_TRUE(thread_state_sleeping.has_value());
  EXPECT_EQ('S', thread_state_sleeping.value());

  thread.join();
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
  std::optional<char> thread_state_exited = GetThreadState(thread_tid);
  EXPECT_FALSE(thread_state_exited.has_value());
}

TEST(ExecuteCommand, EchoHelloWorld) {
  std::string string_to_echo = "Hello, World!";
  std::optional<std::string> returned_result =
      ExecuteCommand(absl::StrFormat("echo %s", string_to_echo));
  std::string expected_result = string_to_echo + "\n";
  ASSERT_TRUE(returned_result.has_value());
  EXPECT_EQ(returned_result.value(), expected_result);
}

TEST(ExtractCpusetFromCgroup, NoCpuset) {
  std::string cgroup_content =
      "11:memory:/groupname/foo\n"
      "6:cpu,cpuacct:/groupname/foo";
  std::optional<std::string> returned_cpuset = ExtractCpusetFromCgroup(cgroup_content);
  ASSERT_FALSE(returned_cpuset.has_value());
}

TEST(ExtractCpusetFromCgroup, OnlyCpusetInLine) {
  std::string cgroup_content =
      "11:memory:/groupname/foo\n"
      "8:cpuset:/groupname/foo\n"
      "6:cpu,cpuacct:/groupname/foo";
  std::optional<std::string> returned_cpuset = ExtractCpusetFromCgroup(cgroup_content);
  std::string expected_cpuset = "/groupname/foo";
  ASSERT_TRUE(returned_cpuset.has_value());
  EXPECT_EQ(returned_cpuset.value(), expected_cpuset);
}

TEST(ExtractCpusetFromCgroup, CpusetLastInLine) {
  std::string cgroup_content =
      "11:memory:/groupname/foo\n"
      "6:cpu,cpuacct,cpuset:/groupname/foo";
  std::optional<std::string> returned_cpuset = ExtractCpusetFromCgroup(cgroup_content);
  std::string expected_cpuset = "/groupname/foo";
  ASSERT_TRUE(returned_cpuset.has_value());
  EXPECT_EQ(returned_cpuset.value(), expected_cpuset);
}

TEST(ExtractCpusetFromCgroup, CpusetMiddleInLine) {
  std::string cgroup_content =
      "11:memory:/groupname/foo\n"
      "6:cpu,cpuset,cpuacct:/groupname/foo";
  std::optional<std::string> returned_cpuset = ExtractCpusetFromCgroup(cgroup_content);
  std::string expected_cpuset = "/groupname/foo";
  ASSERT_TRUE(returned_cpuset.has_value());
  EXPECT_EQ(returned_cpuset.value(), expected_cpuset);
}

TEST(ParseCpusetCpus, Empty) {
  std::string cpuset_cpus_content = "";
  std::vector<int> returned_cpus = ParseCpusetCpus(cpuset_cpus_content);
  EXPECT_TRUE(returned_cpus.empty());
}

TEST(ParseCpusetCpus, SingleValuesAndRanges) {
  std::string cpuset_cpus_content = "0-2,4,7,12-14";
  std::vector<int> returned_cpus = ParseCpusetCpus(cpuset_cpus_content);
  EXPECT_THAT(returned_cpus, ::testing::ElementsAre(0, 1, 2, 4, 7, 12, 13, 14));
}

}  // namespace orbit_linux_tracing
