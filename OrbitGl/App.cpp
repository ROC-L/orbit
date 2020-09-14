// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "App.h"

#include <absl/flags/flag.h>
#include <absl/strings/str_format.h>
#include <absl/strings/str_join.h>

#include <chrono>
#include <cmath>
#include <fstream>
#include <outcome.hpp>
#include <thread>
#include <utility>

#include "CallStackDataView.h"
#include "Callstack.h"
#include "CaptureWindow.h"
#include "Disassembler.h"
#include "DisassemblyReport.h"
#include "EventTracer.h"
#include "FunctionUtils.h"
#include "FunctionsDataView.h"
#include "GlCanvas.h"
#include "ImGuiOrbit.h"
#include "ModulesDataView.h"
#include "OrbitBase/Logging.h"
#include "OrbitBase/Result.h"
#include "OrbitBase/Tracing.h"
#include "OrbitClientModel/CaptureDeserializer.h"
#include "OrbitClientModel/CaptureSerializer.h"
#include "Path.h"
#include "Pdb.h"
#include "PresetsDataView.h"
#include "ProcessesDataView.h"
#include "SamplingProfiler.h"
#include "SamplingReport.h"
#include "ScopeTimer.h"
#include "StringManager.h"
#include "SymbolHelper.h"
#include "TimerInfosIterator.h"
#include "Utils.h"
#include "symbol.pb.h"

ABSL_DECLARE_FLAG(bool, devmode);
ABSL_DECLARE_FLAG(bool, local);
ABSL_FLAG(bool, enable_tracepoint_feature, false,
          "Enable the setting of the panel of kernel tracepoints");

using orbit_client_protos::CallstackEvent;
using orbit_client_protos::FunctionInfo;
using orbit_client_protos::LinuxAddressInfo;
using orbit_client_protos::PresetFile;
using orbit_client_protos::PresetInfo;
using orbit_client_protos::TimerInfo;

using orbit_grpc_protos::CrashOrbitServiceRequest_CrashType;
using orbit_grpc_protos::ModuleInfo;
using orbit_grpc_protos::ModuleSymbols;
using orbit_grpc_protos::ProcessInfo;
using orbit_grpc_protos::TracepointInfo;

namespace {
PresetLoadState GetPresetLoadStateForProcess(
    const std::shared_ptr<orbit_client_protos::PresetFile>& preset,
    const std::shared_ptr<Process>& process) {
  if (process == nullptr) {
    return PresetLoadState::kNotLoadable;
  }

  int modules_not_found_count = 0;
  for (const auto& pair : preset->preset_info().path_to_module()) {
    const std::string& module_path = pair.first;
    if (process->GetModuleFromPath(module_path) == nullptr) {
      modules_not_found_count++;
    }
  }

  // Empty preset is also loadable
  if (modules_not_found_count == 0) {
    return PresetLoadState::kLoadable;
  }

  if (modules_not_found_count == preset->preset_info().path_to_module_size()) {
    return PresetLoadState::kNotLoadable;
  }

  return PresetLoadState::kPartiallyLoadable;
}
}  // namespace

std::unique_ptr<OrbitApp> GOrbitApp;
bool DoZoom = false;

OrbitApp::OrbitApp(ApplicationOptions&& options,
                   std::unique_ptr<MainThreadExecutor> main_thread_executor)
    : options_(std::move(options)), main_thread_executor_(std::move(main_thread_executor)) {
  thread_pool_ = ThreadPool::Create(4 /*min_size*/, 256 /*max_size*/, absl::Seconds(1));
  main_thread_id_ = std::this_thread::get_id();
  data_manager_ = std::make_unique<DataManager>(main_thread_id_);
}

OrbitApp::~OrbitApp() {
#ifdef _WIN32
  oqpi_tk::stop_scheduler();
#endif
}

void OrbitApp::OnCaptureStarted(int32_t process_id, std::string process_name,
                                std::shared_ptr<Process> process,
                                absl::flat_hash_map<uint64_t, FunctionInfo> selected_functions,
                                TracepointInfoSet selected_tracepoints) {
  // We need to block until initialization is complete to
  // avoid races when capture thread start processing data.
  absl::Mutex mutex;
  absl::MutexLock mutex_lock(&mutex);
  bool initialization_complete = false;

  main_thread_executor_->Schedule(
      [this, &initialization_complete, &mutex, process_id, process_name = std::move(process_name),
       process = std::move(process), selected_functions = std::move(selected_functions),
       selected_tracepoints = std::move(selected_tracepoints)]() mutable {
        const bool has_selected_functions = !selected_functions.empty();

        ClearCapture();

        // It is safe to do this write on the main thread, as the capture thread is suspended until
        // this task is completely executed.
        capture_data_ = CaptureData(process_id, std::move(process_name), std::move(process),
                                    std::move(selected_functions), std::move(selected_tracepoints));

        if (capture_started_callback_) {
          capture_started_callback_();
        }

        if (has_selected_functions && select_live_tab_callback_) {
          select_live_tab_callback_();
        }

        FireRefreshCallbacks();

        absl::MutexLock lock(&mutex);
        initialization_complete = true;
      });

  bool (*IsTrue)(bool*) = [](bool* value) { return *value; };
  mutex.Await(absl::Condition(IsTrue, &initialization_complete));
}

void OrbitApp::OnCaptureComplete() {
  SamplingProfiler sampling_profiler(*capture_data_.GetCallstackData(), capture_data_);
  capture_data_.set_sampling_profiler(sampling_profiler);
  main_thread_executor_->Schedule(
      [this, sampling_profiler = std::move(sampling_profiler)]() mutable {
        RefreshCaptureView();

        SetSamplingReport(std::move(sampling_profiler),
                          GetCaptureData().GetCallstackData()->GetUniqueCallstacksCopy());
        SetTopDownView(GetCaptureData());

        if (capture_stopped_callback_) {
          capture_stopped_callback_();
        }

        FireRefreshCallbacks();
      });
}

void OrbitApp::OnTimer(const TimerInfo& timer_info) {
  if (timer_info.function_address() > 0) {
    const FunctionInfo& func =
        GetCaptureData().selected_functions().at(timer_info.function_address());
    uint64_t elapsed_nanos = timer_info.end() - timer_info.start();
    capture_data_.UpdateFunctionStats(func, elapsed_nanos);
    GCurrentTimeGraph->ProcessTimer(timer_info, &func);
  } else {
    GCurrentTimeGraph->ProcessTimer(timer_info, nullptr);
  }
}

void OrbitApp::OnKeyAndString(uint64_t key, std::string str) {
  string_manager_->AddIfNotPresent(key, std::move(str));
}

void OrbitApp::OnUniqueCallStack(CallStack callstack) {
  capture_data_.AddUniqueCallStack(std::move(callstack));
}

void OrbitApp::OnCallstackEvent(CallstackEvent callstack_event) {
  GEventTracer.GetEventBuffer().AddCallstackEvent(
      callstack_event.time(), callstack_event.callstack_hash(), callstack_event.thread_id());
  capture_data_.AddCallstackEvent(std::move(callstack_event));
}

void OrbitApp::OnThreadName(int32_t thread_id, std::string thread_name) {
  capture_data_.AddOrAssignThreadName(thread_id, std::move(thread_name));
}

void OrbitApp::OnAddressInfo(LinuxAddressInfo address_info) {
  capture_data_.InsertAddressInfo(std::move(address_info));
}

void OrbitApp::OnUniqueTracepointInfo(uint64_t key,
                                      orbit_grpc_protos::TracepointInfo tracepoint_info) {
  capture_data_.AddUniqueTracepointEventInfo(key, std::move(tracepoint_info));
}

void OrbitApp::OnTracepointEvent(orbit_client_protos::TracepointEventInfo tracepoint_event_info) {
  int32_t capture_process_id = capture_data_.process_id();
  bool is_same_pid_as_target = capture_process_id == tracepoint_event_info.pid();

  capture_data_.AddTracepointEventAndMapToThreads(
      tracepoint_event_info.time(), tracepoint_event_info.tracepoint_info_key(),
      tracepoint_event_info.pid(), tracepoint_event_info.tid(), is_same_pid_as_target);
}

void OrbitApp::OnValidateFramePointers(std::vector<std::shared_ptr<Module>> modules_to_validate) {
  thread_pool_->Schedule([modules_to_validate = std::move(modules_to_validate), this] {
    frame_pointer_validator_client_->AnalyzeModules(modules_to_validate);
  });
}

bool OrbitApp::Init(ApplicationOptions&& options,
                    std::unique_ptr<MainThreadExecutor> main_thread_executor) {
  GOrbitApp = std::make_unique<OrbitApp>(std::move(options), std::move(main_thread_executor));

#ifdef _WIN32
  oqpi_tk::start_default_scheduler();
#endif

  GOrbitApp->LoadFileMapping();

  return true;
}

void OrbitApp::PostInit() {
  if (!options_.grpc_server_address.empty()) {
    grpc::ChannelArguments channel_arguments;
    // TODO (159888769) move symbol loading to grpc stream.
    // The default receive message size is 4mb. Symbol data can easily be more
    // than this. This is set to an arbitrary size of 2gb (numeric max), which
    // seems to be enough and leaves some headroom. As an example, a 1.1gb
    // .debug symbols file results in a message size of 88mb.
    channel_arguments.SetMaxReceiveMessageSize(std::numeric_limits<int32_t>::max());
    grpc_channel_ = grpc::CreateCustomChannel(
        options_.grpc_server_address, grpc::InsecureChannelCredentials(), channel_arguments);
    if (!grpc_channel_) {
      ERROR("Unable to create GRPC channel to %s", options_.grpc_server_address);
    }

    capture_client_ = std::make_unique<CaptureClient>(grpc_channel_, this);

    // TODO: Replace refresh_timeout with config option. Let users to modify it.
    process_manager_ = ProcessManager::Create(grpc_channel_, absl::Milliseconds(1000));

    auto callback = [this](ProcessManager* process_manager) {
      main_thread_executor_->Schedule([this, process_manager]() {
        const std::vector<ProcessInfo>& process_infos = process_manager->GetProcessList();
        data_manager_->UpdateProcessInfos(process_infos);
        processes_data_view_->SetProcessList(process_infos);
        {
          // TODO: remove this part when client stops using Process class
          absl::MutexLock lock(&process_map_mutex_);
          for (const ProcessInfo& info : process_infos) {
            auto it = process_map_.find(info.pid());
            if (it != process_map_.end()) {
              continue;
            }

            std::shared_ptr<Process> process = std::make_shared<Process>();
            process->SetID(info.pid());
            process->SetName(info.name());
            process->SetFullPath(info.full_path());
            process->SetIs64Bit(info.is_64_bit());
            // The other fields do not appear to be used at the moment.

            process_map_.insert_or_assign(process->GetId(), process);
          }
        }

        if (GetSelectedProcessId() == -1 && processes_data_view_->GetFirstProcessId() != -1) {
          processes_data_view_->SelectProcess(processes_data_view_->GetFirstProcessId());
        }
        FireRefreshCallbacks(DataViewType::kProcesses);
      });
    };

    process_manager_->SetProcessListUpdateListener(callback);

    frame_pointer_validator_client_ =
        std::make_unique<FramePointerValidatorClient>(this, grpc_channel_);

    if (absl::GetFlag(FLAGS_devmode)) {
      crash_manager_ = CrashManager::Create(grpc_channel_);
    }
  }

  ListPresets();

  string_manager_ = std::make_shared<StringManager>();

  GCurrentTimeGraph->SetStringManager(string_manager_);

  if (!absl::GetFlag(FLAGS_enable_tracepoint_feature)) {
    return;
  }

  thread_pool_->Schedule([this] {
    std::unique_ptr<TracepointServiceClient> tracepoint_manager =
        TracepointServiceClient::Create(grpc_channel_);

    ErrorMessageOr<std::vector<TracepointInfo>> result = tracepoint_manager->GetTracepointList();

    if (result.has_error()) {
      ERROR("Error retrieving tracepoints: %s", result.error().message());
      SendErrorToUi("Error retrieving tracepoints", result.error().message());
      return;
    }

    main_thread_executor_->Schedule([result, this]() {
      tracepoints_data_view_->SetTracepoints(result.value());

      FireRefreshCallbacks(DataViewType::kTracepoints);
    });
  });
}

void OrbitApp::LoadFileMapping() {
  file_mapping_.clear();
  std::string file_name = Path::GetFileMappingFileName();
  if (!std::filesystem::exists(file_name)) {
    std::ofstream outfile(file_name);
    outfile << "//-------------------" << std::endl
            << "// Orbit File Mapping" << std::endl
            << "//-------------------" << std::endl
            << R"(// If the file path in the pdb is "D:\NoAccess\File.cpp")" << std::endl
            << R"(// and File.cpp is locally available in "C:\Available\")" << std::endl
            << "// then enter a file mapping on its own line like so:" << std::endl
            << R"(// "D:\NoAccess\File.cpp" "C:\Available\")" << std::endl
            << std::endl
            << R"("D:\NoAccess" "C:\Available")" << std::endl;

    outfile.close();
  }

  std::fstream infile(file_name);
  if (!infile.fail()) {
    std::string line;
    while (std::getline(infile, line)) {
      if (absl::StartsWith(line, "//")) continue;

      bool contains_quotes = absl::StrContains(line, "\"");

      std::vector<std::string> tokens = absl::StrSplit(line, ' ');

      if (tokens.size() == 2 && !contains_quotes) {
        file_mapping_[ToLower(tokens[0])] = ToLower(tokens[1]);
      } else {
        std::vector<std::string> valid_tokens;
        std::vector<std::string> subtokens = absl::StrSplit(line, '"');
        for (const std::string& subtoken : subtokens) {
          if (!IsBlank(subtoken)) {
            valid_tokens.push_back(subtoken);
          }
        }

        if (valid_tokens.size() > 1) {
          file_mapping_[ToLower(valid_tokens[0])] = ToLower(valid_tokens[1]);
        }
      }
    }
  }
}

void OrbitApp::ListPresets() {
  std::vector<std::string> preset_filenames = Path::ListFiles(Path::CreateOrGetPresetDir(), ".opr");
  std::vector<std::shared_ptr<PresetFile>> presets;
  for (std::string& filename : preset_filenames) {
    ErrorMessageOr<PresetInfo> preset_result = ReadPresetFromFile(filename);
    if (preset_result.has_error()) {
      ERROR("Loading preset from \"%s\" failed: %s", filename, preset_result.error().message());
      continue;
    }

    auto preset = std::make_shared<PresetFile>();
    preset->set_file_name(filename);
    preset->mutable_preset_info()->CopyFrom(preset_result.value());
    presets.push_back(preset);
  }

  presets_data_view_->SetPresets(presets);
}

void OrbitApp::RefreshCaptureView() {
  NeedsRedraw();
  GOrbitApp->FireRefreshCallbacks();
  DoZoom = true;  // TODO: remove global, review logic
}

void OrbitApp::Disassemble(int32_t pid, const FunctionInfo& function) {
  const bool is_64_bit = data_manager_->GetProcessByPid(pid)->is_64_bit();
  thread_pool_->Schedule([this, is_64_bit, pid, function] {
    auto result = process_manager_->LoadProcessMemory(
        pid, FunctionUtils::GetAbsoluteAddress(function), function.size());
    if (!result.has_value()) {
      SendErrorToUi("Error reading memory", absl::StrFormat("Could not read process memory: %s.",
                                                            result.error().message()));
      return;
    }

    const std::string& memory = result.value();
    Disassembler disasm;
    disasm.AddLine(absl::StrFormat("asm: /* %s */", FunctionUtils::GetDisplayName(function)));
    disasm.Disassemble(memory.data(), memory.size(), FunctionUtils::GetAbsoluteAddress(function),
                       is_64_bit);
    if (!sampling_report_) {
      DisassemblyReport empty_report(disasm);
      SendDisassemblyToUi(disasm.GetResult(), std::move(empty_report));
      return;
    }
    const CaptureData& capture_data = GetCaptureData();
    const SamplingProfiler& profiler = capture_data.sampling_profiler();

    DisassemblyReport report(disasm, FunctionUtils::GetAbsoluteAddress(function), profiler,
                             capture_data.GetCallstackData()->GetCallstackEventsSize());
    SendDisassemblyToUi(disasm.GetResult(), std::move(report));
  });
}

void OrbitApp::OnExit() {
  StopCapture();

  process_manager_->Shutdown();
  thread_pool_->ShutdownAndWait();

  GOrbitApp = nullptr;
  Orbit_ImGui_Shutdown();
}

Timer GMainTimer;

// TODO: make it non-static
void OrbitApp::MainTick() {
  GMainTimer.Reset();

  if (DoZoom) {
    GCurrentTimeGraph->SortTracks();
    GOrbitApp->capture_window_->ZoomAll();
    GOrbitApp->NeedsRedraw();
    DoZoom = false;
  }
}

void OrbitApp::RegisterCaptureWindow(CaptureWindow* capture) {
  CHECK(capture_window_ == nullptr);
  capture_window_ = capture;
}

void OrbitApp::NeedsRedraw() {
  if (capture_window_ != nullptr) {
    capture_window_->NeedsUpdate();
  }
}

void OrbitApp::SetSamplingReport(
    SamplingProfiler sampling_profiler,
    absl::flat_hash_map<CallstackID, std::shared_ptr<CallStack>> unique_callstacks) {
  auto report =
      std::make_shared<SamplingReport>(std::move(sampling_profiler), std::move(unique_callstacks));
  CHECK(sampling_reports_callback_);
  DataView* callstack_data_view = GetOrCreateDataView(DataViewType::kCallstack);
  sampling_reports_callback_(callstack_data_view, report);

  // clear old sampling report
  if (sampling_report_ != nullptr) {
    sampling_report_->ClearReport();
  }
  sampling_report_ = report;
}

void OrbitApp::SetSelectionReport(
    SamplingProfiler sampling_profiler,
    absl::flat_hash_map<CallstackID, std::shared_ptr<CallStack>> unique_callstacks,
    bool has_summary) {
  CHECK(selection_report_callback_);
  auto report = std::make_shared<SamplingReport>(std::move(sampling_profiler),
                                                 std::move(unique_callstacks), has_summary);
  DataView* callstack_data_view = GetOrCreateSelectionCallstackDataView();
  selection_report_callback_(callstack_data_view, report);

  // clear old selection report
  if (selection_report_ != nullptr) {
    selection_report_->ClearReport();
  }
  selection_report_ = report;
  FireRefreshCallbacks();
}

void OrbitApp::SetTopDownView(const CaptureData& capture_data) {
  CHECK(top_down_view_callback_);
  std::unique_ptr<TopDownView> top_down_view =
      TopDownView::CreateFromSamplingProfiler(capture_data.sampling_profiler(), capture_data);
  top_down_view_callback_(std::move(top_down_view));
}

void OrbitApp::SetSelectionTopDownView(const SamplingProfiler& selection_sampling_profiler,
                                       const CaptureData& capture_data) {
  CHECK(selection_top_down_view_callback_);
  std::unique_ptr<TopDownView> selection_top_down_view =
      TopDownView::CreateFromSamplingProfiler(selection_sampling_profiler, capture_data);
  selection_top_down_view_callback_(std::move(selection_top_down_view));
}

std::string OrbitApp::GetCaptureFileName() {
  const CaptureData& capture_data = GetCaptureData();
  time_t timestamp = std::chrono::system_clock::to_time_t(capture_data.capture_start_time());
  std::string result;
  result.append(Path::StripExtension(capture_data.process_name()));
  result.append("_");
  result.append(OrbitUtils::FormatTime(timestamp));
  result.append(".orbit");
  return result;
}

std::string OrbitApp::GetCaptureTime() {
  double time = GCurrentTimeGraph != nullptr ? GCurrentTimeGraph->GetCaptureTimeSpanUs() : 0.0;
  return GetPrettyTime(absl::Microseconds(time));
}

std::string OrbitApp::GetSaveFile(const std::string& extension) {
  if (!save_file_callback_) {
    return "";
  }
  return save_file_callback_(extension);
}

void OrbitApp::SetClipboard(const std::string& text) {
  if (clipboard_callback_) {
    clipboard_callback_(text);
  }
}

ErrorMessageOr<void> OrbitApp::OnSavePreset(const std::string& filename) {
  OUTCOME_TRY(SavePreset(filename));
  ListPresets();
  Refresh(DataViewType::kPresets);
  return outcome::success();
}

ErrorMessageOr<void> OrbitApp::SavePreset(const std::string& filename) {
  PresetInfo preset;
  const int32_t pid = GetSelectedProcessId();
  const std::shared_ptr<Process>& process = FindProcessByPid(pid);

  for (uint64_t function_address : data_manager_->selected_functions()) {
    FunctionInfo* func = process->GetFunctionFromAddress(function_address);
    // No need to store the manually instrumented functions
    if (!FunctionUtils::IsOrbitFunc(*func)) {
      uint64_t hash = FunctionUtils::GetHash(*func);
      (*preset.mutable_path_to_module())[func->loaded_module_path()].add_function_hashes(hash);
    }
  }

  std::string filename_with_ext = filename;
  if (!absl::EndsWith(filename, ".opr")) {
    filename_with_ext += ".opr";
  }

  std::ofstream file(filename_with_ext, std::ios::binary);
  if (file.fail()) {
    ERROR("Saving preset in \"%s\": %s", filename_with_ext, "file.fail()");
    return ErrorMessage(
        absl::StrFormat("Error opening the file \"%s\" for writing", filename_with_ext));
  }

  LOG("Saving preset in \"%s\"", filename_with_ext);
  preset.SerializeToOstream(&file);

  return outcome::success();
}

ErrorMessageOr<PresetInfo> OrbitApp::ReadPresetFromFile(const std::string& filename) {
  std::string file_path = filename;

  if (Path::GetDirectory(filename).empty()) {
    file_path = Path::JoinPath({Path::CreateOrGetPresetDir(), filename});
  }

  std::ifstream file(file_path, std::ios::binary);
  if (file.fail()) {
    ERROR("Loading preset from \"%s\": file.fail()", file_path);
    return ErrorMessage("Error opening the file for reading");
  }

  PresetInfo preset_info;
  if (!preset_info.ParseFromIstream(&file)) {
    ERROR("Loading preset from \"%s\" failed", file_path);
    return ErrorMessage(absl::StrFormat("Error reading the preset"));
  }
  return preset_info;
}

ErrorMessageOr<void> OrbitApp::OnLoadPreset(const std::string& filename) {
  OUTCOME_TRY(preset_info, ReadPresetFromFile(filename));

  auto preset = std::make_shared<PresetFile>();
  preset->set_file_name(filename);
  preset->mutable_preset_info()->CopyFrom(preset_info);
  LoadPreset(preset);
  return outcome::success();
}

void OrbitApp::LoadPreset(const std::shared_ptr<PresetFile>& preset) {
  const int32_t selected_process_id = GetSelectedProcessId();
  const ProcessData* selected_process = data_manager_->GetProcessByPid(selected_process_id);
  if (selected_process != nullptr) {
    LoadModulesFromPreset(GetSelectedProcess(), preset);
  } else {
    SendErrorToUi("Preset loading failed", "Process is not selected");
  }
}

PresetLoadState OrbitApp::GetPresetLoadState(
    const std::shared_ptr<orbit_client_protos::PresetFile>& preset) const {
  const std::shared_ptr<Process> selected_process = GetSelectedProcess();
  return GetPresetLoadStateForProcess(preset, selected_process);
}

ErrorMessageOr<void> OrbitApp::OnSaveCapture(const std::string& file_name) {
  const auto& key_to_string_map = GCurrentTimeGraph->GetStringManager()->GetKeyToStringMap();

  std::vector<std::shared_ptr<TimerChain>> chains = GCurrentTimeGraph->GetAllTimerChains();

  TimerInfosIterator timers_it_begin(chains.begin(), chains.end());
  TimerInfosIterator timers_it_end(chains.end(), chains.end());

  return capture_serializer::Save(file_name, GetCaptureData(), key_to_string_map, timers_it_begin,
                                  timers_it_end);
}

void OrbitApp::OnLoadCapture(const std::string& file_name) {
  if (open_capture_callback_) {
    open_capture_callback_();
  }
  ClearCapture();
  string_manager_->Clear();
  thread_pool_->Schedule([this, file_name]() mutable {
    capture_loading_cancellation_requested_ = false;
    ErrorMessageOr<void> result =
        capture_deserializer::Load(file_name, this, &capture_loading_cancellation_requested_);

    if (result.has_error()) {
      if (open_capture_failed_callback_) {
        open_capture_failed_callback_();
      }
      SendErrorToUi("Error loading capture",
                    absl::StrFormat("Could not load capture from \"%s\":\n%s", file_name,
                                    result.error().message()));
    }

    if (open_capture_finished_callback_) {
      open_capture_finished_callback_();
    }
  });

  DoZoom = true;  // TODO: remove global, review logic
}

void OrbitApp::OnLoadCatpureCanceled() {
  capture_loading_cancellation_requested_ = true;
  if (open_capture_failed_callback_) {
    open_capture_failed_callback_();
  }
  ClearCapture();
}

void OrbitApp::FireRefreshCallbacks(DataViewType type) {
  for (DataView* panel : panels_) {
    if (type == DataViewType::kAll || type == panel->GetType()) {
      panel->OnDataChanged();
    }
  }

  if (refresh_callback_) {
    refresh_callback_(type);
  }
}

bool OrbitApp::StartCapture() {
  int32_t pid = GetSelectedProcessId();
  const ProcessData* process_data = data_manager_->GetProcessByPid(pid);
  std::shared_ptr<Process> process = FindProcessByPid(pid);
  if (pid == -1 || process_data == nullptr || process == nullptr) {
    SendErrorToUi("Error starting capture",
                  "No process selected. Please choose a target process for the capture.");
    return false;
  }
  std::string process_name = process_data->name();

  absl::flat_hash_map<uint64_t, FunctionInfo> selected_functions =
      GetSelectedFunctionsAndOrbitFunctions();

  TracepointInfoSet selected_tracepoints = GetSelectedTracepoints();

  ErrorMessageOr<void> result =
      capture_client_->StartCapture(thread_pool_.get(), pid, std::move(process_name),
                                    std::move(process), selected_functions, selected_tracepoints);

  if (result.has_error()) {
    SendErrorToUi("Error starting capture", result.error().message());
    return false;
  }

  return true;
}

absl::flat_hash_map<uint64_t, FunctionInfo> OrbitApp::GetSelectedFunctionsAndOrbitFunctions()
    const {
  absl::flat_hash_map<uint64_t, FunctionInfo> selected_functions;
  for (const auto& func : GetSelectedProcess()->GetFunctions()) {
    if (IsFunctionSelected(*func) || FunctionUtils::IsOrbitFunc(*func)) {
      uint64_t address = FunctionUtils::GetAbsoluteAddress(*func);
      selected_functions[address] = *func;
    }
  }
  return selected_functions;
}

void OrbitApp::StopCapture() {
  if (!capture_client_->StopCapture()) {
    return;
  }

  if (capture_stop_requested_callback_) {
    capture_stop_requested_callback_();
  }
}

void OrbitApp::ClearCapture() {
  capture_data_ = CaptureData();
  set_selected_thread_id(-1);
  SelectTextBox(nullptr);

  UpdateAfterCaptureCleared();

  if (GCurrentTimeGraph != nullptr) {
    GCurrentTimeGraph->Clear();
  }

  if (capture_cleared_callback_) {
    capture_cleared_callback_();
  }
  FireRefreshCallbacks();
}

void OrbitApp::ToggleDrawHelp() {
  if (capture_window_ != nullptr) {
    capture_window_->ToggleDrawHelp();
  }
}

void OrbitApp::ToggleCapture() {
  if (IsCapturing()) {
    StopCapture();
  } else {
    StartCapture();
  }
}

bool OrbitApp::SelectProcess(const std::string& process) {
  if (processes_data_view_) {
    return processes_data_view_->SelectProcess(process);
  }

  return false;
}

void OrbitApp::SendDisassemblyToUi(std::string disassembly, DisassemblyReport report) {
  main_thread_executor_->Schedule(
      [this, disassembly = std::move(disassembly), report = std::move(report)]() mutable {
        if (disassembly_callback_) {
          disassembly_callback_(std::move(disassembly), std::move(report));
        }
      });
}

void OrbitApp::SendTooltipToUi(const std::string& tooltip) {
  main_thread_executor_->Schedule([this, tooltip] {
    if (tooltip_callback_) {
      tooltip_callback_(tooltip);
    }
  });
}

void OrbitApp::SendInfoToUi(const std::string& title, const std::string& text) {
  main_thread_executor_->Schedule([this, title, text] {
    if (info_message_callback_) {
      info_message_callback_(title, text);
    }
  });
}

void OrbitApp::SendWarningToUi(const std::string& title, const std::string& text) {
  main_thread_executor_->Schedule([this, title, text] {
    if (warning_message_callback_) {
      warning_message_callback_(title, text);
    }
  });
}

void OrbitApp::SendErrorToUi(const std::string& title, const std::string& text) {
  main_thread_executor_->Schedule([this, title, text] {
    if (error_message_callback_) {
      error_message_callback_(title, text);
    }
  });
}

void OrbitApp::LoadModuleOnRemote(const std::shared_ptr<Process>& process,
                                  const std::shared_ptr<Module>& module,
                                  const std::shared_ptr<PresetFile>& preset) {
  ScopedStatus scoped_status = CreateScopedStatus(absl::StrFormat(
      "Searching for symbols on remote instance (module \"%s\")...", module->m_FullName));
  thread_pool_->Schedule([this, process, module, preset,
                          scoped_status = std::move(scoped_status)]() mutable {
    const std::string& module_path = module->m_FullName;
    const auto result = process_manager_->FindDebugInfoFile(module_path);

    if (!result) {
      SendErrorToUi("Error loading symbols",
                    absl::StrFormat("Did not find symbols on remote for module \"%s\": %s",
                                    module_path, result.error().message()));
      main_thread_executor_->Schedule(
          [this, module]() { modules_currently_loading_.erase(module->m_FullName); });
      return;
    }

    const std::string& debug_file_path = result.value();

    LOG("Found symbols file on the remote: \"%s\" - loading it using scp...", debug_file_path);

    main_thread_executor_->Schedule([this, module, module_path, process, preset, debug_file_path,
                                     scoped_status = std::move(scoped_status)]() mutable {
      const std::filesystem::path local_debug_file_path =
          symbol_helper_.GenerateCachedFileName(module_path);

      {
        scoped_status.UpdateMessage(
            absl::StrFormat(R"(Copying debug info file for "%s" from remote: "%s"...)", module_path,
                            debug_file_path));
        SCOPE_TIMER_LOG(absl::StrFormat("Copying %s", debug_file_path));
        auto scp_result = secure_copy_callback_(debug_file_path, local_debug_file_path.string());
        if (!scp_result) {
          SendErrorToUi("Error loading symbols",
                        absl::StrFormat("Could not copy debug info file from the remote: %s",
                                        scp_result.error().message()));
          return;
        }
      }

      LoadSymbols(local_debug_file_path, process, module, preset);
    });
  });
}

void OrbitApp::LoadModules(const std::shared_ptr<Process>& process,
                           const std::vector<std::shared_ptr<Module>>& modules,
                           const std::shared_ptr<PresetFile>& preset) {
  // TODO(159868905) use ModuleData instead of Module
  for (const auto& module : modules) {
    if (modules_currently_loading_.contains(module->m_FullName)) {
      continue;
    }
    modules_currently_loading_.insert(module->m_FullName);

    const auto& symbols_path = FindSymbolsLocally(module->m_FullName, module->m_DebugSignature);
    if (symbols_path) {
      LoadSymbols(symbols_path.value(), process, module, preset);
      continue;
    }

    if (!absl::GetFlag(FLAGS_local)) {
      LoadModuleOnRemote(process, module, preset);
      continue;
    }

    // If no symbols are found and remote loading is not attempted.
    SendErrorToUi("Error loading symbols",
                  absl::StrFormat("Did not find symbols for module \"%s\": %s", module->m_FullName,
                                  symbols_path.error().message()));
    modules_currently_loading_.erase(module->m_FullName);
  }
}

ErrorMessageOr<std::filesystem::path> OrbitApp::FindSymbolsLocally(
    const std::filesystem::path& module_path, const std::string& build_id) {
  const auto scoped_status = CreateScopedStatus(absl::StrFormat(
      "Searching for symbols on local machine (module: \"%s\")...", module_path.string()));

  if (build_id.empty()) {
    return ErrorMessage(absl::StrFormat(
        "Unable to find local symbols for module \"%s\", build id is empty", module_path.string()));
  }

  std::string error_message;
  {
    const auto symbols_path = symbol_helper_.FindSymbolsWithSymbolsPathFile(module_path, build_id);
    if (symbols_path) {
      LOG("Found symbols for module \"%s\" in user provided symbol folder. Symbols filename: "
          "\"%s\"",
          module_path.string(), symbols_path.value().string());
      return symbols_path.value();
    }
    error_message += "\n* " + symbols_path.error().message();
  }
  {
    const auto symbols_path = symbol_helper_.FindSymbolsInCache(module_path, build_id);
    if (symbols_path) {
      LOG("Found symbols for module \"%s\" in cache. Symbols filename: \"%s\"",
          module_path.string(), symbols_path.value().string());
      return symbols_path.value();
    }
    error_message += "\n* " + symbols_path.error().message();
  }
  if (absl::GetFlag(FLAGS_local)) {
    const auto symbols_included_in_module = SymbolHelper::VerifySymbolsFile(module_path, build_id);
    if (symbols_included_in_module) {
      LOG("Found symbols included in module: \"%s\"", module_path.string());
      return module_path;
    }
    error_message += "\n* Symbols are not included in module file: " +
                     symbols_included_in_module.error().message();
  }

  error_message = absl::StrFormat("Did not find local symbols for module \"%s\": %s",
                                  module_path.string(), error_message);
  LOG("%s", error_message);
  return ErrorMessage(error_message);
}

void OrbitApp::LoadSymbols(const std::filesystem::path& symbols_path,
                           const std::shared_ptr<Process>& process,
                           const std::shared_ptr<Module>& module,
                           const std::shared_ptr<PresetFile>& preset) {
  auto scoped_status = CreateScopedStatus(absl::StrFormat(
      R"(Loading symbols for "%s" from file "%s"...)", module->m_FullName, symbols_path.string()));
  thread_pool_->Schedule([this, scoped_status = std::move(scoped_status), symbols_path, process,
                          module, preset]() mutable {
    auto symbols_result = SymbolHelper::LoadSymbolsFromFile(symbols_path);
    CHECK(symbols_result);
    main_thread_executor_->Schedule([this, symbols = std::move(symbols_result.value()),
                                     scoped_status = std::move(scoped_status), process, module,
                                     preset] {
      module->LoadSymbols(symbols);
      CHECK(process != nullptr);
      process->AddFunctions(module->m_Pdb->GetFunctions());
      LOG("Loaded %lu function symbols for module \"%s\"", symbols.symbol_infos().size(),
          module->m_FullName);

      // Applying preset
      if (preset != nullptr) {
        auto it = preset->preset_info().path_to_module().find(module->m_FullName);
        if (it != preset->preset_info().path_to_module().end()) {
          for (const FunctionInfo* func : module->m_Pdb->GetSelectedFunctionsFromPreset(*preset)) {
            SelectFunction(*func);
          }
        }
      }

      data_manager_->FindModuleByAddressStart(process->GetId(), module->m_AddressStart)
          ->set_loaded(true);

      modules_currently_loading_.erase(module->m_FullName);

      UpdateAfterSymbolLoading();
      GOrbitApp->FireRefreshCallbacks();
    });
  });
}

void OrbitApp::LoadModulesFromPreset(const std::shared_ptr<Process>& process,
                                     const std::shared_ptr<PresetFile>& preset) {
  std::vector<std::shared_ptr<Module>> modules_to_load;
  std::vector<std::string> modules_not_found;
  for (const auto& pair : preset->preset_info().path_to_module()) {
    const std::string& module_path = pair.first;
    std::shared_ptr<Module> module = process->GetModuleFromPath(module_path);
    if (module == nullptr) {
      modules_not_found.push_back(module_path);
      continue;
    }
    if (module->IsLoaded()) {
      CHECK(module->m_Pdb != nullptr);
      for (const FunctionInfo* func : module->m_Pdb->GetSelectedFunctionsFromPreset(*preset)) {
        SelectFunction(*func);
      }
      continue;
    }
    modules_to_load.emplace_back(std::move(module));
  }
  if (!modules_not_found.empty()) {
    if (static_cast<int>(modules_not_found.size()) == preset->preset_info().path_to_module_size()) {
      // no modules were loaded
      SendErrorToUi(
          "Preset loading failed",
          absl::StrFormat("None of the modules in the preset are loaded by the process \"%s\".",
                          process->GetName()));
    } else {
      SendWarningToUi(
          "Preset only partially loaded",
          absl::StrFormat("The following modules are not loaded by the process \"%s\":\n\"%s\"",
                          process->GetName(), absl::StrJoin(modules_not_found, "\"\n\"")));
    }
  }
  if (!modules_to_load.empty()) {
    LoadModules(process, modules_to_load, preset);
  }
  FireRefreshCallbacks();
}

void OrbitApp::UpdateProcessAndModuleList(int32_t pid) {
  CHECK(processes_data_view_->GetSelectedProcessId() == pid);
  thread_pool_->Schedule([pid, this] {
    ErrorMessageOr<std::vector<ModuleInfo>> result = process_manager_->LoadModuleList(pid);

    if (result.has_error()) {
      ERROR("Error retrieving modules: %s", result.error().message());
      SendErrorToUi("Error retrieving modules", result.error().message());
      return;
    }

    main_thread_executor_->Schedule([pid, result, this] {
      // Make sure that pid is actually what user has selected at
      // the moment we arrive here. If not - ignore the result.
      const std::vector<ModuleInfo>& module_infos = result.value();
      data_manager_->UpdateModuleInfos(pid, module_infos);
      if (pid != processes_data_view_->GetSelectedProcessId()) {
        return;
      }

      modules_data_view_->SetModules(pid, data_manager_->GetModules(pid));

      // TODO: remove this part when all client code is moved to
      // new data model.
      std::shared_ptr<Process> process = FindProcessByPid(pid);
      CHECK(process != nullptr);

      for (const ModuleInfo& info : module_infos) {
        // if module already exists, don't create it again.
        if (process->GetModuleFromPath(info.file_path()) != nullptr) {
          continue;
        }
        std::shared_ptr<Module> module = std::make_shared<Module>();
        module->m_Name = info.name();
        module->m_FullName = info.file_path();
        module->m_PdbSize = info.file_size();
        module->m_AddressStart = info.address_start();
        module->m_AddressEnd = info.address_end();
        module->m_DebugSignature = info.build_id();
        process->AddModule(module);
      }

      // To this point ----------------------------------

      // To this point all data is ready. We can set the Process and then
      // propagate the changes to the UI.

      if (pid != GetSelectedProcessId()) {
        data_manager_->ClearSelectedFunctions();
        data_manager_->set_selected_process(process);
      }

      FireRefreshCallbacks();
    });
  });
}

std::shared_ptr<Process> OrbitApp::FindProcessByPid(int32_t pid) {
  absl::MutexLock lock(&process_map_mutex_);
  auto it = process_map_.find(pid);
  if (it == process_map_.end()) {
    return nullptr;
  }

  return it->second;
}

void OrbitApp::SelectFunction(const orbit_client_protos::FunctionInfo& func) {
  uint64_t absolute_address = FunctionUtils::GetAbsoluteAddress(func);
  LOG("Selected %s at 0x%" PRIx64 " (address_=0x%" PRIx64 ", load_bias_= 0x%" PRIx64
      ", base_address=0x%" PRIx64 ")",
      func.pretty_name(), absolute_address, func.address(), func.load_bias(),
      func.module_base_address());
  data_manager_->SelectFunction(absolute_address);
}

void OrbitApp::DeselectFunction(const orbit_client_protos::FunctionInfo& func) {
  uint64_t absolute_address = FunctionUtils::GetAbsoluteAddress(func);
  data_manager_->DeselectFunction(absolute_address);
}

void OrbitApp::ClearSelectedFunctions() { data_manager_->ClearSelectedFunctions(); }

[[nodiscard]] bool OrbitApp::IsFunctionSelected(
    const orbit_client_protos::FunctionInfo& func) const {
  uint64_t absolute_address = FunctionUtils::GetAbsoluteAddress(func);
  return data_manager_->IsFunctionSelected(absolute_address);
}

[[nodiscard]] bool OrbitApp::IsFunctionSelected(const SampledFunction& func) const {
  return data_manager_->IsFunctionSelected(func.absolute_address);
}

[[nodiscard]] bool OrbitApp::IsFunctionSelected(uint64_t absolute_address) const {
  return data_manager_->IsFunctionSelected(absolute_address);
}

void OrbitApp::SetVisibleFunctions(absl::flat_hash_set<uint64_t> visible_functions) {
  data_manager_->set_visible_functions(std::move(visible_functions));
  NeedsRedraw();
}

[[nodiscard]] bool OrbitApp::IsFunctionVisible(uint64_t function_address) {
  return data_manager_->IsFunctionVisible(function_address);
}

ThreadID OrbitApp::selected_thread_id() const { return data_manager_->selected_thread_id(); }

void OrbitApp::set_selected_thread_id(ThreadID thread_id) {
  return data_manager_->set_selected_thread_id(thread_id);
}

const TextBox* OrbitApp::selected_text_box() const { return data_manager_->selected_text_box(); }
void OrbitApp::SelectTextBox(const TextBox* text_box) {
  data_manager_->set_selected_text_box(text_box);
}

void OrbitApp::SelectCallstackEvents(const std::vector<CallstackEvent>& selected_callstack_events,
                                     int32_t thread_id) {
  const CallstackData* callstack_data = GetCaptureData().GetCallstackData();
  std::unique_ptr<CallstackData> selection_callstack_data = std::make_unique<CallstackData>();
  for (const CallstackEvent& event : selected_callstack_events) {
    selection_callstack_data->AddCallStackFromKnownCallstackData(event, callstack_data);
  }
  // TODO: this might live on the data_manager
  capture_data_.set_selection_callstack_data(std::move(selection_callstack_data));

  // Generate selection report.
  bool generate_summary = thread_id == SamplingProfiler::kAllThreadsFakeTid;
  SamplingProfiler sampling_profiler(*capture_data_.GetSelectionCallstackData(), GetCaptureData(),
                                     generate_summary);

  SetSelectionTopDownView(sampling_profiler, GetCaptureData());

  SetSelectionReport(std::move(sampling_profiler),
                     capture_data_.GetSelectionCallstackData()->GetUniqueCallstacksCopy(),
                     generate_summary);
}

void OrbitApp::UpdateAfterSymbolLoading() {
  const CaptureData& capture_data = GetCaptureData();

  if (sampling_report_ != nullptr) {
    SamplingProfiler sampling_profiler(*capture_data.GetCallstackData(), capture_data);
    sampling_report_->UpdateReport(sampling_profiler,
                                   capture_data.GetCallstackData()->GetUniqueCallstacksCopy());
    capture_data_.set_sampling_profiler(sampling_profiler);
  }

  SetTopDownView(GetCaptureData());

  if (selection_report_ == nullptr) {
    return;
  }

  // TODO(kuebler): propagate this information
  SamplingProfiler selection_profiler(*capture_data.GetSelectionCallstackData(), capture_data,
                                      selection_report_->has_summary());

  SetSelectionTopDownView(selection_profiler, GetCaptureData());
  selection_report_->UpdateReport(
      std::move(selection_profiler),
      capture_data.GetSelectionCallstackData()->GetUniqueCallstacksCopy());
}

void OrbitApp::UpdateAfterCaptureCleared() {
  SamplingProfiler empty_profiler;
  absl::flat_hash_map<CallstackID, std::shared_ptr<CallStack>> empty_unique_callstacks;

  SetSamplingReport(empty_profiler, empty_unique_callstacks);
  SetTopDownView(GetCaptureData());
  SetSelectionTopDownView(empty_profiler, GetCaptureData());
  if (selection_report_) {
    SetSelectionReport(std::move(empty_profiler), empty_unique_callstacks, false);
  }
}

DataView* OrbitApp::GetOrCreateDataView(DataViewType type) {
  switch (type) {
    case DataViewType::kFunctions:
      if (!functions_data_view_) {
        functions_data_view_ = std::make_unique<FunctionsDataView>();
        panels_.push_back(functions_data_view_.get());
      }
      return functions_data_view_.get();

    case DataViewType::kCallstack:
      if (!callstack_data_view_) {
        callstack_data_view_ = std::make_unique<CallStackDataView>();
        panels_.push_back(callstack_data_view_.get());
      }
      return callstack_data_view_.get();

    case DataViewType::kModules:
      if (!modules_data_view_) {
        modules_data_view_ = std::make_unique<ModulesDataView>();
        panels_.push_back(modules_data_view_.get());
      }
      return modules_data_view_.get();

    case DataViewType::kProcesses:
      if (!processes_data_view_) {
        processes_data_view_ = std::make_unique<ProcessesDataView>();
        processes_data_view_->SetSelectionListener(
            [&](int32_t pid) { UpdateProcessAndModuleList(pid); });
        panels_.push_back(processes_data_view_.get());
      }
      return processes_data_view_.get();

    case DataViewType::kPresets:
      if (!presets_data_view_) {
        presets_data_view_ = std::make_unique<PresetsDataView>();
        panels_.push_back(presets_data_view_.get());
      }
      return presets_data_view_.get();

    case DataViewType::kSampling:
      FATAL(
          "DataViewType::kSampling Data View construction is not supported by"
          "the factory.");
    case DataViewType::kLiveFunctions:
      FATAL("DataViewType::kLiveFunctions should not be used with the factory.");

    case DataViewType::kAll:
      FATAL("DataViewType::kAll should not be used with the factory.");

    case DataViewType::kTracepoints:
      if (!tracepoints_data_view_) {
        tracepoints_data_view_ = std::make_unique<TracepointsDataView>();
        panels_.push_back(tracepoints_data_view_.get());
      }
      return tracepoints_data_view_.get();

    case DataViewType::kInvalid:
      FATAL("DataViewType::kInvalid should not be used with the factory.");
  }
  FATAL("Unreachable");
}

DataView* OrbitApp::GetOrCreateSelectionCallstackDataView() {
  if (selection_callstack_data_view_ == nullptr) {
    selection_callstack_data_view_ = std::make_unique<CallStackDataView>();
    panels_.push_back(selection_callstack_data_view_.get());
  }
  return selection_callstack_data_view_.get();
}

void OrbitApp::FilterTracks(const std::string& filter) {
  GCurrentTimeGraph->SetThreadFilter(filter);
}

void OrbitApp::CrashOrbitService(CrashOrbitServiceRequest_CrashType crash_type) {
  if (absl::GetFlag(FLAGS_devmode)) {
    thread_pool_->Schedule([crash_type, this] { crash_manager_->CrashOrbitService(crash_type); });
  }
}

bool OrbitApp::IsCapturing() const { return capture_client_->IsCapturing(); }

ScopedStatus OrbitApp::CreateScopedStatus(const std::string& initial_message) {
  CHECK(std::this_thread::get_id() == main_thread_id_);
  CHECK(status_listener_ != nullptr);
  return ScopedStatus{main_thread_executor_.get(), status_listener_, initial_message};
}

void OrbitApp::SelectTracepoint(const TracepointInfo& tracepoint) {
  data_manager_->SelectTracepoint(tracepoint);
}

void OrbitApp::DeselectTracepoint(const TracepointInfo& tracepoint) {
  data_manager_->DeselectTracepoint(tracepoint);
}

[[nodiscard]] bool OrbitApp::IsTracepointSelected(const TracepointInfo& info) const {
  return data_manager_->IsTracepointSelected(info);
}
