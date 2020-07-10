// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "Capture.h"

#include <fstream>
#include <ostream>

#include "Core.h"
#include "EventBuffer.h"
#include "Injection.h"
#include "Log.h"
#include "OrbitBase/Logging.h"
#include "OrbitSession.h"
#include "Params.h"
#include "Path.h"
#include "Pdb.h"
#include "SamplingProfiler.h"
#include "Serialization.h"
#include "absl/strings/str_format.h"

#ifndef _WIN32
std::shared_ptr<Pdb> GPdbDbg;
#endif

Capture::State Capture::GState = Capture::State::kEmpty;
bool Capture::GInjected = false;
std::string Capture::GInjectedProcess;
bool Capture::GIsSampling = false;
bool Capture::GIsTesting = false;
uint32_t Capture::GFunctionIndex = -1;
uint32_t Capture::GNumInstalledHooks;
bool Capture::GHasContextSwitches;
Timer Capture::GTestTimer;
uint64_t Capture::GNumContextSwitches;
ULONG64 Capture::GNumLinuxEvents;
ULONG64 Capture::GNumProfileEvents;
std::string Capture::GPresetToLoad;
std::string Capture::GProcessToInject;
std::string Capture::GFunctionFilter;

std::vector<std::shared_ptr<Function>> Capture::GSelectedFunctions;
std::map<uint64_t, Function*> Capture::GSelectedFunctionsMap;
std::map<uint64_t, Function*> Capture::GVisibleFunctionsMap;
std::unordered_map<uint64_t, uint64_t> Capture::GFunctionCountMap;
std::shared_ptr<CallStack> Capture::GSelectedCallstack;
std::unordered_map<uint64_t, std::shared_ptr<CallStack>> Capture::GCallstacks;
std::unordered_map<int32_t, std::string> Capture::GThreadNames;
std::unordered_map<uint64_t, LinuxAddressInfo> Capture::GAddressInfos;
std::unordered_map<uint64_t, std::string> Capture::GAddressToFunctionName;
Mutex Capture::GCallstackMutex;
std::unordered_map<uint64_t, std::string> Capture::GZoneNames;
TextBox* Capture::GSelectedTextBox;
ThreadID Capture::GSelectedThreadId;
Timer Capture::GCaptureTimer;
std::chrono::system_clock::time_point Capture::GCaptureTimePoint;

std::shared_ptr<SamplingProfiler> Capture::GSamplingProfiler = nullptr;
std::shared_ptr<Process> Capture::GTargetProcess = nullptr;
std::shared_ptr<Preset> Capture::GSessionPresets = nullptr;

void (*Capture::GClearCaptureDataFunc)();
std::vector<std::shared_ptr<SamplingProfiler>> GOldSamplingProfilers;
bool Capture::GUnrealSupported = false;

//-----------------------------------------------------------------------------
void Capture::Init() { GTargetProcess = std::make_shared<Process>(); }

//-----------------------------------------------------------------------------
void Capture::SetTargetProcess(const std::shared_ptr<Process>& a_Process) {
  if (a_Process != GTargetProcess) {
    GInjected = false;
    GInjectedProcess = "";

    GTargetProcess = a_Process;
    GSamplingProfiler = std::make_shared<SamplingProfiler>(a_Process);
    GSelectedFunctionsMap.clear();
    GFunctionCountMap.clear();
  }
}

//-----------------------------------------------------------------------------
outcome::result<void, std::string> Capture::StartCapture() {
  if (GTargetProcess->GetID() == 0) {
    return outcome::failure(
        "No process selected. Please choose a target process for the capture.");
  }

  GCaptureTimer.Start();
  GCaptureTimePoint = std::chrono::system_clock::now();

  GInjected = true;

  ClearCaptureData();
  PreFunctionHooks();

  Capture::NewSamplingProfiler();
  Capture::GSamplingProfiler->StartCapture();

  GState = State::kStarted;

  return outcome::success();
}

//-----------------------------------------------------------------------------
void Capture::StopCapture() {
  if (!GInjected) {
    return;
  }

  GState = State::kStopping;
}

//-----------------------------------------------------------------------------
void Capture::FinalizeCapture() {
  if (Capture::GSamplingProfiler != nullptr) {
    Capture::GSamplingProfiler->StopCapture();
    Capture::GSamplingProfiler->ProcessSamples();
  }

  GState = State::kDone;
}

//-----------------------------------------------------------------------------
void Capture::ClearCaptureData() {
  GFunctionCountMap.clear();
  GCallstacks.clear();
  GThreadNames.clear();
  GAddressInfos.clear();
  GAddressToFunctionName.clear();
  GZoneNames.clear();
  GSelectedTextBox = nullptr;
  GSelectedThreadId = 0;
  GNumProfileEvents = 0;
  GHasContextSwitches = false;
  GNumLinuxEvents = 0;
  GNumContextSwitches = 0;
  GState = State::kEmpty;
}

//-----------------------------------------------------------------------------
void Capture::PreFunctionHooks() {
  GSelectedFunctions = GetSelectedFunctions();

  for (auto& func : GSelectedFunctions) {
    uint64_t address = func->GetVirtualAddress();
    GSelectedFunctionsMap[address] = func.get();
    func->ResetStats();
    GFunctionCountMap[address] = 0;
  }

  GVisibleFunctionsMap = GSelectedFunctionsMap;

  if (GClearCaptureDataFunc) {
    GClearCaptureDataFunc();
  }
}

std::vector<std::shared_ptr<Function>> Capture::GetSelectedFunctions() {
  std::vector<std::shared_ptr<Function>> selected_functions;
  for (auto& func : GTargetProcess->GetFunctions()) {
    if (func->IsSelected() || func->IsOrbitFunc()) {
      selected_functions.push_back(func);
    }
  }
  return selected_functions;
}

//-----------------------------------------------------------------------------
void Capture::TestHooks() {
  if (!GIsTesting) {
    GIsTesting = true;
    GFunctionIndex = 0;
    GTestTimer.Start();
  } else {
    GIsTesting = false;
  }
}

//-----------------------------------------------------------------------------
bool Capture::IsCapturing() {
  return GState == State::kStarted || GState == State::kStopping;
}

//-----------------------------------------------------------------------------
void Capture::DisplayStats() {
  if (GSamplingProfiler) {
    TRACE_VAR(GSamplingProfiler->GetNumSamples());
  }
}

//-----------------------------------------------------------------------------
outcome::result<void, std::string> Capture::SavePreset(
    const std::string& filename) {
  Preset preset;
  preset.m_ProcessFullPath = GTargetProcess->GetFullPath();

  for (auto& func : GTargetProcess->GetFunctions()) {
    if (func->IsSelected()) {
      preset.m_Modules[func->GetLoadedModulePath()].m_FunctionHashes.push_back(
          func->Hash());
    }
  }

  std::string filename_with_ext = filename;
  if (!absl::EndsWith(filename, ".opr")) {
    filename_with_ext += ".opr";
  }

  std::ofstream file(filename_with_ext, std::ios::binary);
  if (file.fail()) {
    ERROR("Saving preset in \"%s\": %s", filename_with_ext, "file.fail()");
    return outcome::failure("Error opening the file for writing");
  }

  try {
    SCOPE_TIMER_LOG(
        absl::StrFormat("Saving preset in \"%s\"", filename_with_ext));
    cereal::BinaryOutputArchive archive(file);
    // "Session" is use for backwards compatibility.
    archive(cereal::make_nvp("Session", preset));
    return outcome::success();
  } catch (std::exception& e) {
    ERROR("Saving preset in \"%s\": %s", filename_with_ext, e.what());
    return outcome::failure("Error serializing the preset");
  }
}

//-----------------------------------------------------------------------------
void Capture::NewSamplingProfiler() {
  if (GSamplingProfiler) {
    // To prevent destruction while processing data...
    GOldSamplingProfilers.push_back(GSamplingProfiler);
  }

  Capture::GSamplingProfiler =
      std::make_shared<SamplingProfiler>(Capture::GTargetProcess);
}

//-----------------------------------------------------------------------------
bool Capture::IsRemote() {
  return GTargetProcess && GTargetProcess->GetIsRemote();
}

//-----------------------------------------------------------------------------
bool Capture::IsLinuxData() {
  bool isLinux = false;
#if __linux__
  isLinux = true;
#endif
  return IsRemote() || isLinux;
}

//-----------------------------------------------------------------------------
void Capture::RegisterZoneName(uint64_t a_ID, const char* a_Name) {
  GZoneNames[a_ID] = a_Name;
}

//-----------------------------------------------------------------------------
void Capture::AddCallstack(CallStack& a_CallStack) {
  ScopeLock lock(GCallstackMutex);
  Capture::GCallstacks[a_CallStack.m_Hash] =
      std::make_shared<CallStack>(a_CallStack);
}

//-----------------------------------------------------------------------------
std::shared_ptr<CallStack> Capture::GetCallstack(CallstackID a_ID) {
  ScopeLock lock(GCallstackMutex);

  auto it = Capture::GCallstacks.find(a_ID);
  if (it != Capture::GCallstacks.end()) {
    return it->second;
  }

  return nullptr;
}

//-----------------------------------------------------------------------------
LinuxAddressInfo* Capture::GetAddressInfo(uint64_t address) {
  auto address_info_it = GAddressInfos.find(address);
  if (address_info_it == GAddressInfos.end()) {
    return nullptr;
  }
  return &address_info_it->second;
}

//-----------------------------------------------------------------------------
void Capture::PreSave() {
  // Add selected functions' exact address to sampling profiler
  for (auto& pair : GSelectedFunctionsMap) {
    GSamplingProfiler->UpdateAddressInfo(pair.first);
  }
}
