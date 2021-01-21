// Copyright (c) 2021 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_BASE_GET_PROCESS_IDS_H_
#define ORBIT_BASE_GET_PROCESS_IDS_H_

#include <sys/types.h>

#include <vector>

namespace orbit_base {

#if defined(__linux)
// Get the process ids of all currently running processes.
std::vector<pid_t> GetAllPids();

// Get the thread ids of all the threads belonging to process 'pid'.
std::vector<pid_t> GetTidsOfProcess(pid_t pid);

// Get all thread ids of all the threads in all currently running processes.
std::vector<pid_t> GetAllTids();
#endif

}  // namespace orbit_base

#endif  // ORBIT_BASE_GET_PROCESS_IDS_H_