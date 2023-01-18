// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLIENT_DATA_MODULE_MANAGER_H_
#define CLIENT_DATA_MODULE_MANAGER_H_

#include <absl/base/thread_annotations.h>
#include <absl/container/flat_hash_map.h>
#include <absl/container/node_hash_map.h>
#include <absl/hash/hash.h>
#include <absl/synchronization/mutex.h>
#include <absl/types/span.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ClientData/ModuleData.h"
#include "ClientData/ModuleIdentifier.h"
#include "ClientData/ModuleIdentifierProvider.h"
#include "ClientData/ModuleInMemory.h"
#include "ClientProtos/capture_data.pb.h"
#include "GrpcProtos/module.pb.h"
#include "SymbolProvider/ModulePathAndBuildId.h"
#include "absl/container/node_hash_map.h"
#include "absl/synchronization/mutex.h"

namespace orbit_client_data {

class ModuleManager final {
 public:
  explicit ModuleManager(ModuleIdentifierProvider* module_identifier_provider)
      : module_identifier_provider_{module_identifier_provider} {};

  [[nodiscard]] const ModuleData* GetModuleByModuleInMemoryAndAbsoluteAddress(
      const ModuleInMemory& module_in_memory, uint64_t absolute_address) const;

  [[nodiscard]] ModuleData* GetMutableModuleByModuleInMemoryAndAbsoluteAddress(
      const ModuleInMemory& module_in_memory, uint64_t absolute_address);

  [[nodiscard]] const ModuleData* GetModuleByModulePathAndBuildId(
      const orbit_symbol_provider::ModulePathAndBuildId& module_path_and_build_id) const {
    std::optional<orbit_client_data::ModuleIdentifier> module_id =
        module_identifier_provider_->GetModuleIdentifier(module_path_and_build_id);
    if (!module_id.has_value()) return nullptr;
    return GetModuleByModuleIdentifier(module_id.value());
  }

  [[nodiscard]] ModuleData* GetMutableModuleByModulePathAndBuildId(
      const orbit_symbol_provider::ModulePathAndBuildId& module_path_and_build_id) {
    std::optional<orbit_client_data::ModuleIdentifier> module_id =
        module_identifier_provider_->GetModuleIdentifier(module_path_and_build_id);
    if (!module_id.has_value()) return nullptr;
    return GetMutableModuleByModuleIdentifier(module_id.value());
  }

  [[nodiscard]] const ModuleData* GetModuleByModuleIdentifier(
      const orbit_client_data::ModuleIdentifier& module_id) const;
  [[nodiscard]] ModuleData* GetMutableModuleByModuleIdentifier(
      const orbit_client_data::ModuleIdentifier& module_id);
  // Add new modules for the module_infos that do not exist yet, and update the modules that do
  // exist. If the update changed the module in a way that symbols were not valid anymore, the
  // symbols are discarded, i.e., the module is no longer loaded. This method returns the list of
  // modules that used to be loaded before the call and are no longer loaded after the call.
  [[nodiscard]] std::vector<ModuleData*> AddOrUpdateModules(
      absl::Span<const orbit_grpc_protos::ModuleInfo> module_infos);

  // Similar to AddOrUpdateModules, except that it does not update modules that already have
  // symbols. Returns the list of modules that it did not update.
  [[nodiscard]] std::vector<ModuleData*> AddOrUpdateNotLoadedModules(
      absl::Span<const orbit_grpc_protos::ModuleInfo> module_infos);

  [[nodiscard]] std::vector<const ModuleData*> GetAllModuleData() const;

  [[nodiscard]] std::vector<const ModuleData*> GetModulesByFilename(
      std::string_view filename) const;

 private:
  mutable absl::Mutex mutex_;
  ModuleIdentifierProvider* module_identifier_provider_;
  // We are sharing pointers to that entries and ensure reference stability by using node_hash_map
  // Map of ModuleIdentifier -> ModuleData (ModuleIdentifier is file_path and build_id)
  absl::flat_hash_map<orbit_client_data::ModuleIdentifier, std::unique_ptr<ModuleData>> module_map_
      ABSL_GUARDED_BY(mutex_);
  mutable absl::flat_hash_map<uint64_t, ModuleData*> absolute_address_to_module_data_cache_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace orbit_client_data

#endif  // CLIENT_DATA_MODULE_MANAGER_H_
