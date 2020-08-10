// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "Pdb.h"

#include "Capture.h"
#include "FunctionUtils.h"
#include "OrbitProcess.h"
#include "ScopeTimer.h"

using orbit_client_protos::FunctionInfo;
using orbit_client_protos::PresetFile;
using orbit_client_protos::PresetModule;

Pdb::Pdb(uint64_t module_address, uint64_t load_bias, std::string file_name,
         std::string module_file_name)
    : m_MainModule(module_address),
      load_bias_(load_bias),
      m_FileName(std::move(file_name)),
      m_LoadedModuleName(std::move(module_file_name)) {
  m_Name = Path::GetFileName(m_FileName);
}

void Pdb::AddFunction(const std::shared_ptr<FunctionInfo>& function) {
  functions_.push_back(function);
}

void Pdb::ProcessData() {
  std::shared_ptr<Process> process = Capture::GTargetProcess;
  if (process == nullptr) return;

  SCOPE_TIMER_LOG("ProcessData");
  ScopeLock lock(process->GetDataMutex());

  for (auto& func : functions_) {
    process->AddFunction(func);
  }

  PopulateFunctionMap();
  PopulateStringFunctionMap();
}

void Pdb::PopulateFunctionMap() {
  SCOPE_TIMER_LOG("Pdb::PopulateFunctionMap");
  for (auto& function : functions_) {
    m_FunctionMap.insert(std::make_pair(function->address(), function.get()));
  }
}

void Pdb::PopulateStringFunctionMap() {
  {
    // SCOPE_TIMER_LOG("Reserving map");
    m_StringFunctionMap.reserve(static_cast<size_t>(1.5f * functions_.size()));
  }

  {
    // SCOPE_TIMER_LOG("Map inserts");
    for (auto& function : functions_) {
      m_StringFunctionMap[FunctionUtils::GetHash(*function)] = function.get();
    }
  }
}

FunctionInfo* Pdb::GetFunctionFromExactAddress(uint64_t a_Address) {
  uint64_t function_address = a_Address - GetHModule() + load_bias_;
  auto it = m_FunctionMap.find(function_address);
  return (it != m_FunctionMap.end()) ? it->second : nullptr;
}

FunctionInfo* Pdb::GetFunctionFromProgramCounter(uint64_t a_Address) {
  if (m_FunctionMap.empty()) {
    return nullptr;
  }

  uint64_t relative_address = a_Address - GetHModule() + load_bias_;
  auto it = m_FunctionMap.upper_bound(relative_address);

  if (it == m_FunctionMap.begin()) {
    return nullptr;
  }

  --it;
  return it->second;
}

void Pdb::ApplyPreset(const PresetFile& preset) {
  SCOPE_TIMER_LOG(absl::StrFormat("Pdb::ApplyPreset - %s", m_Name.c_str()));

  std::string module_name = m_LoadedModuleName;
  auto it = preset.preset_info().path_to_module().find(module_name);
  if (it != preset.preset_info().path_to_module().end()) {
    const PresetModule& preset_module = it->second;

    for (uint64_t hash : preset_module.function_hashes()) {
      auto fit = m_StringFunctionMap.find(hash);
      if (fit != m_StringFunctionMap.end()) {
        FunctionInfo* function = fit->second;
        FunctionUtils::Select(function);
      }
    }
  }
}
