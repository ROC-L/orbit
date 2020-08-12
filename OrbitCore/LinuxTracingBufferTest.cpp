// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <utility>

#include "LinuxTracingBuffer.h"

using orbit_client_protos::CallstackEvent;
using orbit_client_protos::LinuxAddressInfo;

TEST(LinuxTracingBuffer, Empty) {
  LinuxTracingBuffer buffer;

  std::vector<Timer> timers;
  EXPECT_FALSE(buffer.ReadAllTimers(&timers));
  EXPECT_TRUE(timers.empty());

  std::vector<LinuxCallstackEvent> callstacks;
  EXPECT_FALSE(buffer.ReadAllCallstacks(&callstacks));
  EXPECT_TRUE(callstacks.empty());

  std::vector<CallstackEvent> hashed_callstacks;
  EXPECT_FALSE(buffer.ReadAllHashedCallstacks(&hashed_callstacks));
  EXPECT_TRUE(hashed_callstacks.empty());
}

TEST(LinuxTracingBuffer, Timers) {
  LinuxTracingBuffer buffer;

  {
    Timer timer;
    timer.m_PID = 1;
    timer.m_TID = 1;
    timer.m_Depth = 0;
    timer.m_Type = Timer::CORE_ACTIVITY;
    timer.m_Processor = 1;
    timer.m_CallstackHash = 2;
    timer.m_FunctionAddress = 3;
    timer.m_UserData[0] = 7;
    timer.m_UserData[1] = 77;
    timer.m_Start = 800;
    timer.m_End = 900;

    buffer.RecordTimer(std::move(timer));
  }

  {
    Timer timer;
    timer.m_PID = 1;
    timer.m_TID = 2;
    timer.m_Depth = 0;
    timer.m_Type = Timer::CORE_ACTIVITY;
    timer.m_Processor = 3;
    timer.m_CallstackHash = 4;
    timer.m_FunctionAddress = 1;
    timer.m_UserData[0] = 17;
    timer.m_UserData[1] = 177;
    timer.m_Start = 1800;
    timer.m_End = 1900;

    buffer.RecordTimer(std::move(timer));
  }

  std::vector<Timer> timers;
  EXPECT_TRUE(buffer.ReadAllTimers(&timers));
  EXPECT_FALSE(buffer.ReadAllTimers(&timers));

  EXPECT_EQ(timers.size(), 2);

  EXPECT_EQ(timers[0].m_PID, 1);
  EXPECT_EQ(timers[0].m_TID, 1);
  EXPECT_EQ(timers[0].m_Depth, 0);
  EXPECT_EQ(timers[0].m_Type, Timer::CORE_ACTIVITY);
  EXPECT_EQ(timers[0].m_Processor, 1);
  EXPECT_EQ(timers[0].m_CallstackHash, 2);
  EXPECT_EQ(timers[0].m_FunctionAddress, 3);
  EXPECT_THAT(timers[0].m_UserData, testing::ElementsAre(7, 77));
  EXPECT_EQ(timers[0].m_Start, 800);
  EXPECT_EQ(timers[0].m_End, 900);

  EXPECT_EQ(timers[1].m_PID, 1);
  EXPECT_EQ(timers[1].m_TID, 2);
  EXPECT_EQ(timers[1].m_Depth, 0);
  EXPECT_EQ(timers[1].m_Type, Timer::CORE_ACTIVITY);
  EXPECT_EQ(timers[1].m_Processor, 3);
  EXPECT_EQ(timers[1].m_CallstackHash, 4);
  EXPECT_EQ(timers[1].m_FunctionAddress, 1);
  EXPECT_THAT(timers[1].m_UserData, testing::ElementsAre(17, 177));
  EXPECT_EQ(timers[1].m_Start, 1800);
  EXPECT_EQ(timers[1].m_End, 1900);

  // Check that the vector is reset, even if it was not empty.
  {
    Timer timer;
    timer.m_PID = 11;
    timer.m_TID = 12;
    timer.m_Depth = 10;
    timer.m_Type = Timer::CORE_ACTIVITY;
    timer.m_Processor = 3;
    timer.m_CallstackHash = 4;
    timer.m_FunctionAddress = 1;
    timer.m_UserData[0] = 7;
    timer.m_UserData[1] = 77;
    timer.m_Start = 1800;
    timer.m_End = 1900;

    buffer.RecordTimer(std::move(timer));
  }

  EXPECT_TRUE(buffer.ReadAllTimers(&timers));
  EXPECT_EQ(timers.size(), 1);

  EXPECT_FALSE(buffer.ReadAllTimers(&timers));
  EXPECT_EQ(timers.size(), 1);

  EXPECT_EQ(timers[0].m_PID, 11);
  EXPECT_EQ(timers[0].m_TID, 12);
  EXPECT_EQ(timers[0].m_Depth, 10);
  EXPECT_EQ(timers[0].m_Type, Timer::CORE_ACTIVITY);
  EXPECT_EQ(timers[0].m_Processor, 3);
  EXPECT_EQ(timers[0].m_CallstackHash, 4);
  EXPECT_EQ(timers[0].m_FunctionAddress, 1);
  EXPECT_THAT(timers[0].m_UserData, testing::ElementsAre(7, 77));
  EXPECT_EQ(timers[0].m_Start, 1800);
  EXPECT_EQ(timers[0].m_End, 1900);
}

TEST(LinuxTracingBuffer, Callstacks) {
  LinuxTracingBuffer buffer;

  {
    CallStack callstack({21, 22});
    LinuxCallstackEvent event(1, callstack);
    buffer.RecordCallstack(std::move(event));
  }

  {
    CallStack callstack({121, 122});
    LinuxCallstackEvent event(2, callstack);
    buffer.RecordCallstack(std::move(event));
  }

  std::vector<LinuxCallstackEvent> callstacks;
  EXPECT_TRUE(buffer.ReadAllCallstacks(&callstacks));
  EXPECT_FALSE(buffer.ReadAllCallstacks(&callstacks));

  EXPECT_EQ(callstacks.size(), 2);

  EXPECT_EQ(callstacks[0].time_, 1);
  EXPECT_EQ(callstacks[0].callstack_.GetFramesCount(), 2);
  EXPECT_THAT(callstacks[0].callstack_.GetFrames(),
              testing::ElementsAre(21, 22));
  EXPECT_THAT(
      callstacks[0].callstack_.GetHash(),
      XXH64(callstacks[0].callstack_.GetFrames().data(),
            callstacks[0].callstack_.GetFramesCount() * sizeof(uint64_t),
            0xca1157ac));

  EXPECT_EQ(callstacks[1].time_, 2);
  EXPECT_EQ(callstacks[1].callstack_.GetFramesCount(), 2);
  EXPECT_THAT(callstacks[1].callstack_.GetFrames(),
              testing::ElementsAre(121, 122));
  EXPECT_THAT(
      callstacks[1].callstack_.GetHash(),
      XXH64(callstacks[1].callstack_.GetFrames().data(),
            callstacks[1].callstack_.GetFramesCount() * sizeof(uint64_t),
            0xca1157ac));

  {
    CallStack callstack({221, 222});
    LinuxCallstackEvent event(3, callstack);
    buffer.RecordCallstack(std::move(event));
  }

  EXPECT_TRUE(buffer.ReadAllCallstacks(&callstacks));
  EXPECT_EQ(callstacks.size(), 1);

  EXPECT_FALSE(buffer.ReadAllCallstacks(&callstacks));
  EXPECT_EQ(callstacks.size(), 1);

  EXPECT_EQ(callstacks[0].time_, 3);
  EXPECT_EQ(callstacks[0].callstack_.GetFramesCount(), 2);
  EXPECT_THAT(callstacks[0].callstack_.GetFrames(),
              testing::ElementsAre(221, 222));
  EXPECT_THAT(
      callstacks[0].callstack_.GetHash(),
      XXH64(callstacks[0].callstack_.GetFrames().data(),
            callstacks[0].callstack_.GetFramesCount() * sizeof(uint64_t),
            0xca1157ac));
}

TEST(LinuxTracingBuffer, HashedCallstacks) {
  LinuxTracingBuffer buffer;

  {
    CallstackEvent event;
    event.set_time(11);
    event.set_callstack_hash(12);
    event.set_thread_id(13);
    buffer.RecordHashedCallstack(std::move(event));
  }

  {
    CallstackEvent event;
    event.set_time(21);
    event.set_callstack_hash(22);
    event.set_thread_id(23);
    buffer.RecordHashedCallstack(std::move(event));
  }

  std::vector<CallstackEvent> callstacks;
  EXPECT_TRUE(buffer.ReadAllHashedCallstacks(&callstacks));
  EXPECT_FALSE(buffer.ReadAllHashedCallstacks(&callstacks));

  EXPECT_EQ(callstacks.size(), 2);

  EXPECT_EQ(callstacks[0].time(), 11);
  EXPECT_EQ(callstacks[0].callstack_hash(), 12);
  EXPECT_EQ(callstacks[0].thread_id(), 13);

  EXPECT_EQ(callstacks[1].time(), 21);
  EXPECT_EQ(callstacks[1].callstack_hash(), 22);
  EXPECT_EQ(callstacks[1].thread_id(), 23);

  {
    CallstackEvent event;
    event.set_time(31);
    event.set_callstack_hash(32);
    event.set_thread_id(33);
    buffer.RecordHashedCallstack(std::move(event));
  }

  EXPECT_TRUE(buffer.ReadAllHashedCallstacks(&callstacks));
  EXPECT_EQ(callstacks.size(), 1);

  EXPECT_FALSE(buffer.ReadAllHashedCallstacks(&callstacks));
  EXPECT_EQ(callstacks.size(), 1);

  EXPECT_EQ(callstacks[0].time(), 31);
  EXPECT_EQ(callstacks[0].callstack_hash(), 32);
  EXPECT_EQ(callstacks[0].thread_id(), 33);
}

TEST(LinuxTracingBuffer, AddressInfos) {
  LinuxTracingBuffer buffer;

  {
    LinuxAddressInfo address_info;
    address_info.set_absolute_address(0x11);
    address_info.set_module_name("module1");
    address_info.set_function_name("function1");
    address_info.set_offset_in_function(0x1);
    buffer.RecordAddressInfo(std::move(address_info));
  }

  {
    LinuxAddressInfo address_info;
    address_info.set_absolute_address(0x22);
    address_info.set_module_name("module2");
    address_info.set_function_name("function2");
    address_info.set_offset_in_function(0x2);
    buffer.RecordAddressInfo(std::move(address_info));
  }

  std::vector<LinuxAddressInfo> address_infos;
  EXPECT_TRUE(buffer.ReadAllAddressInfos(&address_infos));
  EXPECT_FALSE(buffer.ReadAllAddressInfos(&address_infos));

  EXPECT_EQ(address_infos.size(), 2);

  EXPECT_EQ(address_infos[0].absolute_address(), 0x11);
  EXPECT_EQ(address_infos[0].module_name(), "module1");
  EXPECT_EQ(address_infos[0].function_name(), "function1");
  EXPECT_EQ(address_infos[0].offset_in_function(), 0x1);

  EXPECT_EQ(address_infos[1].absolute_address(), 0x22);
  EXPECT_EQ(address_infos[1].module_name(), "module2");
  EXPECT_EQ(address_infos[1].function_name(), "function2");
  EXPECT_EQ(address_infos[1].offset_in_function(), 0x2);

  {
    LinuxAddressInfo address_info;
    address_info.set_absolute_address(0x33);
    address_info.set_module_name("module3");
    address_info.set_function_name("function3");
    address_info.set_offset_in_function(0x3);
    buffer.RecordAddressInfo(std::move(address_info));
  }

  EXPECT_TRUE(buffer.ReadAllAddressInfos(&address_infos));
  EXPECT_EQ(address_infos.size(), 1);

  EXPECT_FALSE(buffer.ReadAllAddressInfos(&address_infos));
  EXPECT_EQ(address_infos.size(), 1);

  EXPECT_EQ(address_infos[0].absolute_address(), 0x33);
  EXPECT_EQ(address_infos[0].module_name(), "module3");
  EXPECT_EQ(address_infos[0].function_name(), "function3");
  EXPECT_EQ(address_infos[0].offset_in_function(), 0x3);
}

TEST(LinuxTracingBuffer, KeysAndStrings) {
  LinuxTracingBuffer buffer;

  {
    KeyAndString key_and_string{0, "str0"};
    buffer.RecordKeyAndString(std::move(key_and_string));
  }

  buffer.RecordKeyAndString(1, "str1");

  std::vector<KeyAndString> keys_and_strings;
  EXPECT_TRUE(buffer.ReadAllKeysAndStrings(&keys_and_strings));
  EXPECT_FALSE(buffer.ReadAllKeysAndStrings(&keys_and_strings));

  EXPECT_EQ(keys_and_strings.size(), 2);

  EXPECT_EQ(keys_and_strings[0].key, 0);
  EXPECT_EQ(keys_and_strings[0].str, "str0");

  EXPECT_EQ(keys_and_strings[1].key, 1);
  EXPECT_EQ(keys_and_strings[1].str, "str1");

  buffer.RecordKeyAndString(2, "str2");

  EXPECT_TRUE(buffer.ReadAllKeysAndStrings(&keys_and_strings));
  EXPECT_EQ(keys_and_strings.size(), 1);

  EXPECT_FALSE(buffer.ReadAllKeysAndStrings(&keys_and_strings));
  EXPECT_EQ(keys_and_strings.size(), 1);

  EXPECT_EQ(keys_and_strings[0].key, 2);
  EXPECT_EQ(keys_and_strings[0].str, "str2");
}

TEST(LinuxTracingBuffer, ThreadNames) {
  LinuxTracingBuffer buffer;

  buffer.RecordThreadName(1, "thread1");

  {
    TidAndThreadName tid_and_name{2, "thread2"};
    buffer.RecordThreadName(std::move(tid_and_name));
  }

  std::vector<TidAndThreadName> thread_names;
  EXPECT_TRUE(buffer.ReadAllThreadNames(&thread_names));
  EXPECT_FALSE(buffer.ReadAllThreadNames(&thread_names));

  EXPECT_EQ(thread_names.size(), 2);

  EXPECT_EQ(thread_names[0].tid, 1);
  EXPECT_EQ(thread_names[0].thread_name, "thread1");

  EXPECT_EQ(thread_names[1].tid, 2);
  EXPECT_EQ(thread_names[1].thread_name, "thread2");

  buffer.RecordThreadName(3, "thread3");

  EXPECT_TRUE(buffer.ReadAllThreadNames(&thread_names));
  EXPECT_EQ(thread_names.size(), 1);

  EXPECT_FALSE(buffer.ReadAllThreadNames(&thread_names));
  EXPECT_EQ(thread_names.size(), 1);

  EXPECT_EQ(thread_names[0].tid, 3);
  EXPECT_EQ(thread_names[0].thread_name, "thread3");
}

TEST(LinuxTracingBuffer, Reset) {
  LinuxTracingBuffer buffer;

  {
    Timer timer;
    timer.m_PID = 1;
    timer.m_TID = 1;
    timer.m_Depth = 0;
    timer.m_Type = Timer::CORE_ACTIVITY;
    timer.m_Processor = 1;
    timer.m_CallstackHash = 2;
    timer.m_FunctionAddress = 3;
    timer.m_UserData[0] = 7;
    timer.m_UserData[1] = 77;
    timer.m_Start = 800;
    timer.m_End = 900;

    buffer.RecordTimer(std::move(timer));
  }

  {
    CallStack callstack({221, 222});
    LinuxCallstackEvent event(3, callstack);
    buffer.RecordCallstack(std::move(event));
  }

  {
    CallstackEvent event;
    event.set_time(11);
    event.set_callstack_hash(12);
    event.set_thread_id(13);
    buffer.RecordHashedCallstack(std::move(event));
  }

  buffer.RecordKeyAndString(42, "str42");

  buffer.RecordThreadName(42, "thread42");

  buffer.Reset();

  std::vector<Timer> timers;
  EXPECT_FALSE(buffer.ReadAllTimers(&timers));

  std::vector<LinuxCallstackEvent> callstacks;
  EXPECT_FALSE(buffer.ReadAllCallstacks(&callstacks));

  std::vector<CallstackEvent> hashed_callstacks;
  EXPECT_FALSE(buffer.ReadAllHashedCallstacks(&hashed_callstacks));

  std::vector<KeyAndString> keys_and_strings;
  EXPECT_FALSE(buffer.ReadAllKeysAndStrings(&keys_and_strings));

  std::vector<TidAndThreadName> thread_names;
  EXPECT_FALSE(buffer.ReadAllThreadNames(&thread_names));
}
