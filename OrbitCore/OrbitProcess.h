// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_CORE_ORBIT_PROCESS_H_
#define ORBIT_CORE_ORBIT_PROCESS_H_

#include <map>
#include <memory>
#include <set>
#include <unordered_set>
#include <utility>
#include <vector>

#include "BaseTypes.h"
#include "OrbitModule.h"
#include "ScopeTimer.h"
#include "Threading.h"
#include "absl/container/flat_hash_map.h"
#include "capture_data.pb.h"

class Process {
 public:
  Process();

  void AddModule(std::shared_ptr<Module>& a_Module);

  std::map<std::string, std::shared_ptr<Module>>& GetNameToModulesMap() {
    return m_NameToModuleMap;
  }

  void SetName(std::string_view name) { m_Name = name; }
  const std::string& GetName() const { return m_Name; }
  void SetFullPath(std::string_view full_path) { m_FullPath = full_path; }
  const std::string& GetFullPath() const { return m_FullPath; }
  void SetID(int32_t id) { m_ID = id; }
  int32_t GetID() const { return m_ID; }
  void SetIs64Bit(bool value) { m_Is64Bit = value; }
  bool GetIs64Bit() const { return m_Is64Bit; }

  orbit_client_protos::FunctionInfo* GetFunctionFromAddress(
      uint64_t address, bool a_IsExact = true);
  std::shared_ptr<Module> GetModuleFromAddress(uint64_t a_Address);
  std::shared_ptr<Module> GetModuleFromName(const std::string& a_Name);
  std::shared_ptr<Module> GetModuleFromPath(const std::string& module_path);

  void AddFunction(
      const std::shared_ptr<orbit_client_protos::FunctionInfo>& function) {
    m_Functions.push_back(function);
  }

  const std::vector<std::shared_ptr<orbit_client_protos::FunctionInfo>>&
  GetFunctions() const {
    return m_Functions;
  }

  Mutex& GetDataMutex() { return m_DataMutex; }

 private:
  int32_t m_ID;

  std::string m_Name;
  std::string m_FullPath;

  bool m_Is64Bit;
  bool m_IsRemote;
  Mutex m_DataMutex;

  std::map<uint64_t, std::shared_ptr<Module>> m_Modules;
  // TODO(antonrohr) change the usage of m_NameToModuleMap to
  // path_to_module_map_, since the name of a module is not unique
  // (/usr/lib/libbase.so and /opt/somedir/libbase.so)
  std::map<std::string, std::shared_ptr<Module>> m_NameToModuleMap;
  std::map<std::string, std::shared_ptr<Module>> path_to_module_map_;

  // Transients
  std::vector<std::shared_ptr<orbit_client_protos::FunctionInfo>> m_Functions;
};

#endif  // ORBIT_CORE_ORBIT_PROCESS_H_
