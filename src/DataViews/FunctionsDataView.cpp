// Copyright (c) 2021 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "DataViews/FunctionsDataView.h"

#include <absl/flags/declare.h>
#include <absl/flags/flag.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_format.h>
#include <absl/strings/str_split.h>
#include <stddef.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <numeric>

#include "ClientData/CaptureData.h"
#include "ClientData/FunctionUtils.h"
#include "ClientData/ModuleAndFunctionLookup.h"
#include "ClientData/ModuleData.h"
#include "ClientData/ProcessData.h"
#include "CompareAscendingOrDescending.h"
#include "DataViews/AppInterface.h"
#include "DataViews/DataViewType.h"
#include "Introspection/Introspection.h"
#include "OrbitBase/Append.h"
#include "OrbitBase/Chunk.h"
#include "OrbitBase/JoinFutures.h"
#include "OrbitBase/Logging.h"
#include "OrbitBase/TaskGroup.h"
#include "OrbitBase/ThreadPool.h"

using orbit_client_data::CaptureData;
using orbit_client_data::ModuleManager;
using orbit_client_protos::FunctionInfo;

namespace orbit_data_views {

FunctionsDataView::FunctionsDataView(AppInterface* app, orbit_base::ThreadPool* thread_pool)
    : DataView(DataViewType::kFunctions, app), thread_pool_{thread_pool} {}

const std::string FunctionsDataView::kUnselectedFunctionString = "";
const std::string FunctionsDataView::kSelectedFunctionString = "✓";
const std::string FunctionsDataView::kFrameTrackString = "F";

const std::vector<DataView::Column>& FunctionsDataView::GetColumns() {
  static const std::vector<Column> columns = [] {
    std::vector<Column> columns;
    columns.resize(kNumColumns);
    columns[kColumnSelected] = {"Hooked", .0f, SortingOrder::kDescending};
    columns[kColumnName] = {"Function", .65f, SortingOrder::kAscending};
    columns[kColumnSize] = {"Size", .0f, SortingOrder::kAscending};
    columns[kColumnModule] = {"Module", .0f, SortingOrder::kAscending};
    columns[kColumnAddressInModule] = {"Address in module", .0f, SortingOrder::kAscending};
    return columns;
  }();
  return columns;
}

bool FunctionsDataView::ShouldShowSelectedFunctionIcon(AppInterface* app,
                                                       const FunctionInfo& function) {
  return app->IsFunctionSelected(function);
}

bool FunctionsDataView::ShouldShowFrameTrackIcon(AppInterface* app, const FunctionInfo& function) {
  if (app->IsFrameTrackEnabled(function)) {
    return true;
  }

  if (!app->HasCaptureData()) {
    return false;
  }

  const CaptureData& capture_data = app->GetCaptureData();
  const ModuleManager* module_manager = app->GetModuleManager();
  std::optional<uint64_t> instrumented_function_id =
      orbit_client_data::FindInstrumentedFunctionIdSlow(*module_manager, capture_data, function);

  return instrumented_function_id &&
         app->HasFrameTrackInCaptureData(instrumented_function_id.value());
}

std::string FunctionsDataView::BuildSelectedColumnsString(AppInterface* app,
                                                          const FunctionInfo& function) {
  std::string result = kUnselectedFunctionString;
  if (ShouldShowSelectedFunctionIcon(app, function)) {
    absl::StrAppend(&result, kSelectedFunctionString);
    if (ShouldShowFrameTrackIcon(app, function)) {
      absl::StrAppend(&result, " ", kFrameTrackString);
    }
  } else if (ShouldShowFrameTrackIcon(app, function)) {
    absl::StrAppend(&result, kFrameTrackString);
  }
  return result;
}

std::string FunctionsDataView::GetValue(int row, int column) {
  if (row >= static_cast<int>(GetNumElements())) {
    return "";
  }

  const FunctionInfo& function = *GetFunctionInfoFromRow(row);

  switch (column) {
    case kColumnSelected:
      return BuildSelectedColumnsString(app_, function);
    case kColumnName:
      return orbit_client_data::function_utils::GetDisplayName(function);
    case kColumnSize:
      return absl::StrFormat("%lu", function.size());
    case kColumnModule:
      return orbit_client_data::function_utils::GetLoadedModuleName(function);
    case kColumnAddressInModule:
      return absl::StrFormat("%#x", function.address());
    default:
      return "";
  }
}

#define ORBIT_FUNC_SORT(Member)                                                                   \
  [&](int a, int b) {                                                                             \
    return CompareAscendingOrDescending(functions_[a]->Member, functions_[b]->Member, ascending); \
  }

#define ORBIT_CUSTOM_FUNC_SORT(Func)                                                            \
  [&](int a, int b) {                                                                           \
    return CompareAscendingOrDescending(Func(*functions_[a]), Func(*functions_[b]), ascending); \
  }

void FunctionsDataView::DoSort() {
  // TODO(antonrohr): This sorting function can take a lot of time when a large
  // number of functions is used (several seconds). This function is currently
  // executed on the main thread and therefore freezes the UI and interrupts the
  // ssh watchdog signals that are sent to the service. Therefore this should
  // not be called on the main thread and as soon as this is done the watchdog
  // timeout should be rolled back from 25 seconds to 10 seconds in
  // OrbitService.h
  bool ascending = sorting_orders_[sorting_column_] == SortingOrder::kAscending;
  std::function<bool(int a, int b)> sorter = nullptr;

  switch (sorting_column_) {
    case kColumnSelected:
      sorter = ORBIT_CUSTOM_FUNC_SORT(app_->IsFunctionSelected);
      break;
    case kColumnName:
      sorter = ORBIT_CUSTOM_FUNC_SORT(orbit_client_data::function_utils::GetDisplayName);
      break;
    case kColumnSize:
      sorter = ORBIT_FUNC_SORT(size());
      break;
    case kColumnModule:
      sorter = ORBIT_CUSTOM_FUNC_SORT(orbit_client_data::function_utils::GetLoadedModuleName);
      break;
    case kColumnAddressInModule:
      sorter = ORBIT_FUNC_SORT(address());
      break;
    default:
      break;
  }

  if (sorter) {
    std::stable_sort(indices_.begin(), indices_.end(), sorter);
  }
}

DataView::ActionStatus FunctionsDataView::GetActionStatus(
    std::string_view action, int clicked_index, const std::vector<int>& selected_indices) {
  if (action == kMenuActionDisassembly || action == kMenuActionSourceCode) {
    return ActionStatus::kVisibleAndEnabled;
  }

  std::function<bool(const FunctionInfo&)> is_visible_action_enabled;
  if (action == kMenuActionSelect) {
    is_visible_action_enabled = [this](const FunctionInfo& function) {
      return !app_->IsFunctionSelected(function) &&
             orbit_client_data::function_utils::IsFunctionSelectable(function);
    };

  } else if (action == kMenuActionUnselect) {
    is_visible_action_enabled = [this](const FunctionInfo& function) {
      return app_->IsFunctionSelected(function);
    };

  } else if (action == kMenuActionEnableFrameTrack) {
    is_visible_action_enabled = [this](const FunctionInfo& function) {
      return !app_->IsFrameTrackEnabled(function);
    };

  } else if (action == kMenuActionDisableFrameTrack) {
    is_visible_action_enabled = [this](const FunctionInfo& function) {
      return app_->IsFrameTrackEnabled(function);
    };

  } else {
    return DataView::GetActionStatus(action, clicked_index, selected_indices);
  }

  for (int index : selected_indices) {
    const FunctionInfo& function = *GetFunctionInfoFromRow(index);
    if (is_visible_action_enabled(function)) return ActionStatus::kVisibleAndEnabled;
  }
  return ActionStatus::kVisibleButDisabled;
}

void FunctionsDataView::DoFilter() {
  ORBIT_SCOPE(absl::StrFormat("FunctionsDataView::DoFilter [%u]", functions_.size()).c_str());
  filter_tokens_ = absl::StrSplit(absl::AsciiStrToLower(filter_), ' ');

  const size_t kNumFunctionPerTask = 1024;
  std::vector<absl::Span<const FunctionInfo*>> spans =
      orbit_base::CreateChunksOfSize(functions_, kNumFunctionPerTask);
  std::vector<std::vector<uint64_t>> task_results(spans.size());
  orbit_base::TaskGroup task_group(thread_pool_);

  for (size_t i = 0; i < spans.size(); ++i) {
    task_group.AddTask([& span = spans[i], &result = task_results[i], this]() {
      ORBIT_SCOPE("FunctionsDataView::DoFilter Task");
      for (const FunctionInfo*& function : span) {
        std::string name =
            absl::AsciiStrToLower(orbit_client_data::function_utils::GetDisplayName(*function));
        std::string module = orbit_client_data::function_utils::GetLoadedModuleName(*function);

        const auto is_token_found = [&name, &module](const std::string& token) {
          return name.find(token) != std::string::npos || module.find(token) != std::string::npos;
        };

        if (std::all_of(filter_tokens_.begin(), filter_tokens_.end(), is_token_found)) {
          size_t function_index = &function - functions_.data();
          ORBIT_CHECK(function_index < functions_.size());
          result.push_back(function_index);
        }
      }
    });
  }

  task_group.Wait();
  indices_.clear();
  for (std::vector<uint64_t>& result : task_results) {
    indices_.insert(indices_.end(), result.begin(), result.end());
  }
}

void FunctionsDataView::AddFunctions(
    std::vector<const orbit_client_protos::FunctionInfo*> functions) {
  functions_.insert(functions_.end(), functions.begin(), functions.end());
  indices_.resize(functions_.size());
  for (size_t i = 0; i < indices_.size(); ++i) {
    indices_[i] = i;
  }
  OnDataChanged();
}

void FunctionsDataView::ClearFunctions() {
  functions_.clear();
  OnDataChanged();
}

}  // namespace orbit_data_views