// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "OrbitClientData/ModuleManager.h"

#include <vector>

#include "OrbitBase/Logging.h"
#include "absl/synchronization/mutex.h"
#include "capture_data.pb.h"

using orbit_client_protos::FunctionInfo;

namespace OrbitClientData {

void ModuleManager::AddNewModules(const std::vector<orbit_grpc_protos::ModuleInfo>& module_infos) {
  absl::MutexLock lock(&mutex_);

  for (const auto& module_info : module_infos) {
    auto module_it = module_map_.find(module_info.file_path());
    if (module_it == module_map_.end()) {
      const auto [inserted_it, success] =
          module_map_.try_emplace(module_info.file_path(), module_info);
      CHECK(success);
    }
  }
}

const ModuleData* ModuleManager::GetModuleByPath(const std::string& path) const {
  absl::MutexLock lock(&mutex_);

  auto it = module_map_.find(path);
  if (it == module_map_.end()) return nullptr;

  return &it->second;
}

ModuleData* ModuleManager::GetMutableModuleByPath(const std::string& path) {
  absl::MutexLock lock(&mutex_);

  auto it = module_map_.find(path);
  if (it == module_map_.end()) return nullptr;

  return &it->second;
}

std::vector<FunctionInfo> ModuleManager::GetOrbitFunctionsOfProcess(
    const ProcessData& process) const {
  absl::MutexLock lock(&mutex_);

  std::vector<FunctionInfo> result;

  for (const auto& [module_path, _] : process.GetMemoryMap()) {
    auto it = module_map_.find(module_path);
    CHECK(it != module_map_.end());
    const ModuleData* module = &it->second;
    CHECK(module != nullptr);
    if (!module->is_loaded()) continue;

    const std::vector<FunctionInfo>& orbit_functions = module->GetOrbitFunctions();
    result.insert(result.end(), orbit_functions.begin(), orbit_functions.end());
  }
  return result;
}

}  // namespace OrbitClientData