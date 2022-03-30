// Copyright (c) 2022 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ClientData/FunctionInfo.h"

#include <absl/strings/match.h>
#include <absl/strings/str_join.h>
#include <xxhash.h>

#include "ClientData/ModuleData.h"
#include "ClientData/ProcessData.h"
#include "ObjectUtils/Address.h"

namespace orbit_client_data {

namespace {
uint64_t StringHash(const std::string& string) {
  return XXH64(string.data(), string.size(), 0xBADDCAFEDEAD10CC);
}
}  // namespace

std::string FunctionInfo::GetLoadedModuleName() const {
  return ModuleData::GetLoadedModuleNameByPath(module_path());
}

uint64_t FunctionInfo::GetHash() const { return StringHash(pretty_name()); }

uint64_t FunctionInfo::Offset(const ModuleData& module) const {
  return address() - module.load_bias();
}

std::optional<uint64_t> FunctionInfo::GetAbsoluteAddress(const ProcessData& process,
                                                         const ModuleData& module) const {
  std::vector<uint64_t> page_aligned_base_addresses =
      process.GetModuleBaseAddresses(module.file_path(), module.build_id());

  if (page_aligned_base_addresses.empty()) {
    return std::nullopt;
  }

  if (page_aligned_base_addresses.size() > 1) {
    ORBIT_ERROR(
        "Found multiple mappings for \"%s\" with build_id=%s [%s]: "
        "will use the first one as a base address",
        module.file_path(), module.build_id(),
        absl::StrJoin(page_aligned_base_addresses, ",", [](std::string* out, uint64_t address) {
          return out->append(absl::StrFormat("%#x", address));
        }));
  }

  ORBIT_CHECK(!page_aligned_base_addresses.empty());

  return orbit_object_utils::SymbolVirtualAddressToAbsoluteAddress(
      address(), page_aligned_base_addresses.at(0), module.load_bias(),
      module.executable_segment_offset());
}

bool FunctionInfo::IsFunctionSelectable() const {
  constexpr const char* kLibOrbitUserSpaceInstrumentation = "liborbituserspaceinstrumentation.so";
  return module_path().find(kLibOrbitUserSpaceInstrumentation) == std::string::npos;
}

}  // namespace orbit_client_data
