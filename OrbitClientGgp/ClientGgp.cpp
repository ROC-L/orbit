// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ClientGgp.h"

#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "FunctionUtils.h"
#include "OrbitBase/Logging.h"
#include "OrbitBase/Result.h"
#include "OrbitCaptureClient/CaptureClient.h"
#include "OrbitClientModel/CaptureSerializer.h"
#include "OrbitClientServices/ProcessManager.h"
#include "SamplingProfiler.h"
#include "StringManager.h"
#include "SymbolHelper.h"
#include "absl/container/flat_hash_set.h"
#include "capture_data.pb.h"

using orbit_client_protos::CallstackEvent;
using orbit_client_protos::FunctionInfo;
using orbit_client_protos::LinuxAddressInfo;
using orbit_client_protos::TimerInfo;
using orbit_grpc_protos::ModuleInfo;
using orbit_grpc_protos::ProcessInfo;

ClientGgp::ClientGgp(ClientGgpOptions&& options) : options_(std::move(options)) {}

bool ClientGgp::InitClient() {
  if (options_.grpc_server_address.empty()) {
    ERROR("gRPC server address not provided");
    return false;
  }

  // Create channel
  grpc::ChannelArguments channel_arguments;
  channel_arguments.SetMaxReceiveMessageSize(std::numeric_limits<int32_t>::max());

  grpc_channel_ = grpc::CreateCustomChannel(options_.grpc_server_address,
                                            grpc::InsecureChannelCredentials(), channel_arguments);
  if (!grpc_channel_) {
    ERROR("Unable to create GRPC channel to %s", options_.grpc_server_address);
    return false;
  }
  LOG("Created GRPC channel to %s", options_.grpc_server_address);

  process_client_ = std::make_unique<ProcessClient>(grpc_channel_);

  // Initialisations needed for capture to work
  if (!InitCapture()) {
    return false;
  }
  capture_client_ = std::make_unique<CaptureClient>(grpc_channel_, this);
  string_manager_ = std::make_shared<StringManager>();

  return true;
}

// Client requests to start the capture
bool ClientGgp::RequestStartCapture(ThreadPool* thread_pool) {
  int32_t pid = target_process_->GetId();
  if (pid == -1) {
    ERROR(
        "Error starting capture: "
        "No process selected. Please choose a target process for the capture.");
    return false;
  }

  // Load selected functions if provided
  absl::flat_hash_map<uint64_t, FunctionInfo> selected_functions;
  if (!options_.capture_functions.empty()) {
    LOG("Loading selected functions");
    selected_functions = GetSelectedFunctions();
    if (!selected_functions.empty()) {
      LOG("List of selected functions to hook in the capture:");
      for (auto const& [address, selected_function] : selected_functions) {
        LOG("%d %s", address, selected_function.pretty_name());
      }
    }
  } else {
    LOG("No functions provided; no functions hooked in the capture");
  }

  // Start capture
  LOG("Capture pid %d", pid);
  TracepointInfoSet selected_tracepoints;

  ErrorMessageOr<void> result =
      capture_client_->StartCapture(thread_pool, pid, target_process_->GetName(), target_process_,
                                    selected_functions, selected_tracepoints);

  if (result.has_error()) {
    ERROR("Error starting capture: %s", result.error().message());
    return false;
  }
  return true;
}

bool ClientGgp::StopCapture() {
  LOG("Request to stop capture");
  return capture_client_->StopCapture();
}

bool ClientGgp::SaveCapture() {
  LOG("Saving capture");
  const auto& key_to_string_map = string_manager_->GetKeyToStringMap();
  std::string file_name = options_.capture_file_name;
  if (file_name.empty()) {
    file_name = capture_serializer::GetCaptureFileName(capture_data_);
  } else {
    // Make sure the file is saved with orbit extension
    capture_serializer::IncludeOrbitExtensionInFile(file_name);
  }

  ErrorMessageOr<void> result = capture_serializer::Save(
      file_name, capture_data_, key_to_string_map, timer_infos_.begin(), timer_infos_.end());
  if (result.has_error()) {
    ERROR("Could not save the capture: %s", result.error().message());
    return false;
  }
  return true;
}

ErrorMessageOr<std::shared_ptr<Process>> ClientGgp::GetOrbitProcessByPid(int32_t pid) {
  // We retrieve the information of the process to later get the module corresponding to its binary
  OUTCOME_TRY(process_infos, process_client_->GetProcessList());
  LOG("List of processes:");
  for (const ProcessInfo& info : process_infos) {
    LOG("pid:%d, name:%s, path:%s, is64:%d", info.pid(), info.name(), info.full_path(),
        info.is_64_bit());
  }
  auto process_it = find_if(process_infos.begin(), process_infos.end(),
                            [&pid](const ProcessInfo& info) { return info.pid() == pid; });
  if (process_it == process_infos.end()) {
    return ErrorMessage(absl::StrFormat("Error: Process with pid %d not found", pid));
  }
  LOG("Found process by pid, set target process");
  std::shared_ptr<Process> process = std::make_shared<Process>();
  process->SetID(process_it->pid());
  process->SetName(process_it->name());
  process->SetFullPath(process_it->full_path());
  process->SetIs64Bit(process_it->is_64_bit());
  LOG("Process info: pid:%d, name:%s, path:%s, is64:%d", process->GetId(), process->GetName(),
      process->GetFullPath(), process->GetIs64Bit());
  return process;
}

ErrorMessageOr<void> ClientGgp::LoadModuleAndSymbols() {
  // Load modules for target_process_
  OUTCOME_TRY(module_infos, process_client_->LoadModuleList(target_process_->GetId()));
  LOG("List of modules");
  for (const ModuleInfo& info : module_infos) {
    LOG("name:%s, path:%s, size:%d, address_start:%d. address_end:%d, build_id:%s", info.name(),
        info.file_path(), info.file_size(), info.address_start(), info.address_end(),
        info.build_id());
  }
  // Process name can be arbitrary so we use the path to find the module corresponding to the binary
  // of target_process_
  std::string_view main_executable_path = target_process_->GetFullPath();
  auto module_it = find_if(module_infos.begin(), module_infos.end(),
                           [&main_executable_path](const ModuleInfo& info) {
                             return info.file_path() == main_executable_path;
                           });
  if (module_it == module_infos.end()) {
    return ErrorMessage("Error: Module corresponding to process binary not found");
  }
  LOG("Found module correspondent to process binary");
  std::shared_ptr<Module> module = std::make_shared<Module>();
  module->m_Name = module_it->name();
  module->m_FullName = module_it->file_path();
  module->m_PdbSize = module_it->file_size();
  module->m_AddressStart = module_it->address_start();
  module->m_AddressEnd = module_it->address_end();
  module->m_DebugSignature = module_it->build_id();
  target_process_->AddModule(module);
  LOG("Module info: name:%s, path:%s, size:%d, address_start:%d. address_end:%d, build_id:%s",
      module->m_Name, module->m_FullName, module->m_PdbSize, module->m_AddressStart,
      module->m_AddressEnd, module->m_DebugSignature);

  // Load symbols for the module
  const std::string& module_path = module->m_FullName;
  LOG("Looking for debug info file for %s", module_path);
  OUTCOME_TRY(main_executable_debug_file, process_client_->FindDebugInfoFile(module_path));
  LOG("Found file: %s", main_executable_debug_file);
  LOG("Loading symbols");
  OUTCOME_TRY(symbols, SymbolHelper::LoadSymbolsFromFile(main_executable_debug_file));
  module->LoadSymbols(symbols);
  target_process_->AddFunctions(module->m_Pdb->GetFunctions());
  return outcome::success();
}

bool ClientGgp::InitCapture() {
  ErrorMessageOr<std::shared_ptr<Process>> target_process_result =
      GetOrbitProcessByPid(options_.capture_pid);
  if (target_process_result.has_error()) {
    ERROR("Not able to set target process: %s", target_process_result.error().message());
    return false;
  }
  target_process_ = target_process_result.value();
  // Load the module and symbols
  ErrorMessageOr<void> result = LoadModuleAndSymbols();
  if (result.has_error()) {
    ERROR("Not possible to finish loading the module and symbols: %s", result.error().message());
    return false;
  }
  return true;
}

void ClientGgp::InformUsedSelectedCaptureFunctions(
    const absl::flat_hash_set<std::string>& capture_functions_used) {
  if (capture_functions_used.size() != options_.capture_functions.size()) {
    for (const std::string& selected_function : options_.capture_functions) {
      if (!capture_functions_used.contains(selected_function)) {
        ERROR("Function matching %s not found; will not be hooked in the capture",
              selected_function);
      }
    }
  } else {
    LOG("All functions provided had at least a match");
  }
}

std::string ClientGgp::SelectedFunctionMatch(const FunctionInfo& func) {
  for (const std::string& selected_function : options_.capture_functions) {
    if (func.pretty_name().find(selected_function) != std::string::npos) {
      return selected_function;
    }
  }
  return {};
}

absl::flat_hash_map<uint64_t, FunctionInfo> ClientGgp::GetSelectedFunctions() {
  absl::flat_hash_map<uint64_t, FunctionInfo> selected_functions;
  absl::flat_hash_set<std::string> capture_functions_used;
  for (const auto& func : target_process_->GetFunctions()) {
    const std::string& selected_function_match = SelectedFunctionMatch(*func);
    if (!selected_function_match.empty()) {
      uint64_t address = FunctionUtils::GetAbsoluteAddress(*func);
      selected_functions[address] = *func;
      if (!capture_functions_used.contains(selected_function_match)) {
        capture_functions_used.insert(selected_function_match);
      }
    }
  }
  InformUsedSelectedCaptureFunctions(capture_functions_used);
  return selected_functions;
}

void ClientGgp::ProcessTimer(const TimerInfo& timer_info) { timer_infos_.push_back(timer_info); }

// CaptureListener implementation
void ClientGgp::OnCaptureStarted(
    int32_t process_id, std::string process_name, std::shared_ptr<Process> process,
    absl::flat_hash_map<uint64_t, orbit_client_protos::FunctionInfo> selected_functions,
    TracepointInfoSet selected_tracepoints) {
  capture_data_ =
      CaptureData(process_id, process_name, process, selected_functions, selected_tracepoints);
  LOG("Capture started");
}

void ClientGgp::OnCaptureComplete() {
  LOG("Capture completed");
  SamplingProfiler sampling_profiler(*capture_data_.GetCallstackData(), capture_data_);
  capture_data_.set_sampling_profiler(sampling_profiler);
}

void ClientGgp::OnTimer(const orbit_client_protos::TimerInfo& timer_info) {
  if (timer_info.function_address() > 0) {
    FunctionInfo* func =
        capture_data_.process()->GetFunctionFromAddress(timer_info.function_address());
    // For timers, the function must be present in the process
    CHECK(func != nullptr);
    uint64_t elapsed_nanos = timer_info.end() - timer_info.start();
    capture_data_.UpdateFunctionStats(*func, elapsed_nanos);
  }
  ProcessTimer(timer_info);
}

void ClientGgp::OnKeyAndString(uint64_t key, std::string str) {
  string_manager_->AddIfNotPresent(key, std::move(str));
}

void ClientGgp::OnUniqueCallStack(CallStack callstack) {
  capture_data_.AddUniqueCallStack(std::move(callstack));
}

void ClientGgp::OnCallstackEvent(CallstackEvent callstack_event) {
  capture_data_.AddCallstackEvent(std::move(callstack_event));
}

void ClientGgp::OnThreadName(int32_t thread_id, std::string thread_name) {
  capture_data_.AddOrAssignThreadName(thread_id, std::move(thread_name));
}

void ClientGgp::OnAddressInfo(LinuxAddressInfo address_info) {
  capture_data_.InsertAddressInfo(std::move(address_info));
}

void ClientGgp::OnUniqueTracepointInfo(uint64_t key,
                                       orbit_grpc_protos::TracepointInfo tracepoint_info) {
  capture_data_.AddUniqueTracepointEventInfo(key, std::move(tracepoint_info));
}

void ClientGgp::OnTracepointEvent(orbit_client_protos::TracepointEventInfo tracepoint_event_info) {
  int32_t capture_process_id = capture_data_.process_id();
  bool is_same_pid_as_target = capture_process_id == tracepoint_event_info.pid();

  capture_data_.AddTracepointEventAndMapToThreads(
      tracepoint_event_info.time(), tracepoint_event_info.tracepoint_info_key(),
      tracepoint_event_info.pid(), tracepoint_event_info.tid(), is_same_pid_as_target);
}