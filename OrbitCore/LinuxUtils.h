// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "BaseTypes.h"

struct Module;

//-----------------------------------------------------------------------------
namespace LinuxUtils {
std::string ExecuteCommand(const char* a_Cmd);
void StreamCommandOutput(const char* a_Cmd,
                         std::function<void(const std::string&)> a_Callback,
                         bool* a_ExitRequested);
std::vector<std::string> ListModules(pid_t a_PID);
uint64_t GetTracePointID(const char* a_Group, const char* a_Event);
void ListModules(pid_t pid,
                 std::map<uint64_t, std::shared_ptr<Module> >* module_map);
std::unordered_map<uint32_t, float> GetCpuUtilization();
bool Is64Bit(pid_t a_PID);
void DumpClocks();
std::string GetKernelVersionStr();
uint32_t GetKernelVersion();
bool IsKernelOlderThan(const char* a_Version);
std::string GetProcessDir(pid_t process_id);
std::map<uint32_t, std::string> GetThreadNames(pid_t process_id);
}  // namespace LinuxUtils
