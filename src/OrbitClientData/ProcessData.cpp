// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "OrbitClientData/ProcessData.h"

#include <absl/container/flat_hash_map.h>

#include <cstdint>
#include <vector>

#include "OrbitBase/Result.h"
#include "absl/strings/str_format.h"
#include "process.pb.h"
#include "symbol.pb.h"

using orbit_grpc_protos::ModuleSymbols;
using orbit_grpc_protos::ProcessInfo;

ProcessData::ProcessData() { process_info_.set_pid(-1); }

void ProcessData::UpdateModuleInfos(absl::Span<const orbit_grpc_protos::ModuleInfo> module_infos) {
  module_memory_map_.clear();
  start_addresses_.clear();

  for (const auto& module_info : module_infos) {
    {
      const auto [it, success] = module_memory_map_.try_emplace(
          module_info.file_path(),
          ModuleInMemory{module_info.address_start(), module_info.address_end(),
                         module_info.file_path(), module_info.build_id()});
      CHECK(success);
    }
    {
      const auto [it, success] =
          start_addresses_.try_emplace(module_info.address_start(), module_info.file_path());
      CHECK(success);
    }
  }
}
const ModuleInMemory* ProcessData::FindModuleByPath(const std::string& module_path) const {
  auto it = module_memory_map_.find(module_path);
  if (it == module_memory_map_.end()) {
    return nullptr;
  }

  return &it->second;
}

ErrorMessageOr<ModuleInMemory> ProcessData::FindModuleByAddress(uint64_t absolute_address) const {
  if (start_addresses_.empty()) {
    return ErrorMessage(absl::StrFormat("Unable to find module for address %016" PRIx64
                                        ": No modules loaded by process %s",
                                        absolute_address, name()));
  }

  ErrorMessage not_found_error =
      ErrorMessage(absl::StrFormat("Unable to find module for address %016" PRIx64
                                   ": No module loaded at this address by process %s",
                                   absolute_address, name()));

  auto it = start_addresses_.upper_bound(absolute_address);
  if (it == start_addresses_.begin()) return not_found_error;

  --it;
  const std::string& module_path = it->second;
  const ModuleInMemory& module_in_memory = module_memory_map_.at(module_path);
  CHECK(absolute_address >= module_in_memory.start());
  if (absolute_address > module_in_memory.end()) return not_found_error;

  return module_in_memory;
}
