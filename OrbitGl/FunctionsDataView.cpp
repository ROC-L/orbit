// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.



#include "FunctionsDataView.h"

#include "App.h"
#include "Capture.h"
#include "Core.h"
#include "Log.h"
#include "OrbitProcess.h"
#include "OrbitType.h"
#include "Pdb.h"
#include "absl/flags/flag.h"
#include "absl/strings/str_format.h"

ABSL_DECLARE_FLAG(bool, enable_stale_features);

//-----------------------------------------------------------------------------
FunctionsDataView::FunctionsDataView() : DataView(DataViewType::FUNCTIONS) {}

//-----------------------------------------------------------------------------
const std::vector<DataView::Column>& FunctionsDataView::GetColumns() {
  static const std::vector<Column> columns = [] {
    std::vector<Column> columns;
    columns.resize(COLUMN_NUM);
    columns[COLUMN_SELECTED] = {"Hooked", .0f, SortingOrder::Descending};
    columns[COLUMN_INDEX] = {"Index", .0f, SortingOrder::Ascending};
    columns[COLUMN_NAME] = {"Function", .5f, SortingOrder::Ascending};
    columns[COLUMN_SIZE] = {"Size", .0f, SortingOrder::Ascending};
    columns[COLUMN_FILE] = {"File", .0f, SortingOrder::Ascending};
    columns[COLUMN_LINE] = {"Line", .0f, SortingOrder::Ascending};
    columns[COLUMN_MODULE] = {"Module", .0f, SortingOrder::Ascending};
    columns[COLUMN_ADDRESS] = {"Address", .0f, SortingOrder::Ascending};
    columns[COLUMN_CALL_CONV] = {"Call conv", .0f, SortingOrder::Ascending};
    return columns;
  }();
  return columns;
}

//-----------------------------------------------------------------------------
std::string FunctionsDataView::GetValue(int a_Row, int a_Column) {
  ScopeLock lock(Capture::GTargetProcess->GetDataMutex());

  if (a_Row >= static_cast<int>(GetNumElements())) {
    return "";
  }

  Function& function = GetFunction(a_Row);

  switch (a_Column) {
    case COLUMN_SELECTED:
      return function.IsSelected() ? "X" : "-";
    case COLUMN_INDEX:
      return absl::StrFormat("%d", a_Row);
    case COLUMN_NAME:
      return function.PrettyName();
    case COLUMN_SIZE:
      return absl::StrFormat("%lu", function.Size());
    case COLUMN_FILE:
      return function.File();
    case COLUMN_LINE:
      return absl::StrFormat("%i", function.Line());
    case COLUMN_MODULE:
      return function.GetLoadedModuleName();
    case COLUMN_ADDRESS:
      return absl::StrFormat("0x%llx", function.GetVirtualAddress());
    case COLUMN_CALL_CONV:
      return function.GetCallingConventionString();
    default:
      return "";
  }
}

//-----------------------------------------------------------------------------
#define ORBIT_FUNC_SORT(Member)                                            \
  [&](int a, int b) {                                                      \
    return OrbitUtils::Compare(functions[a]->Member, functions[b]->Member, \
                               ascending);                                 \
  }

//-----------------------------------------------------------------------------
void FunctionsDataView::DoSort() {
  bool ascending = m_SortingOrders[m_SortingColumn] == SortingOrder::Ascending;
  std::function<bool(int a, int b)> sorter = nullptr;

  const std::vector<std::shared_ptr<Function>>& functions =
      Capture::GTargetProcess->GetFunctions();

  switch (m_SortingColumn) {
    case COLUMN_SELECTED:
      sorter = ORBIT_FUNC_SORT(IsSelected());
      break;
    case COLUMN_NAME:
      sorter = ORBIT_FUNC_SORT(PrettyName());
      break;
    case COLUMN_SIZE:
      sorter = ORBIT_FUNC_SORT(Size());
      break;
    case COLUMN_FILE:
      sorter = ORBIT_FUNC_SORT(File());
      break;
    case COLUMN_LINE:
      sorter = ORBIT_FUNC_SORT(Line());
      break;
    case COLUMN_MODULE:
      sorter = ORBIT_FUNC_SORT(GetLoadedModuleName());
      break;
    case COLUMN_ADDRESS:
      sorter = ORBIT_FUNC_SORT(Address());
      break;
    case COLUMN_CALL_CONV:
      sorter = ORBIT_FUNC_SORT(CallingConvention());
      break;
    default:
      break;
  }

  if (sorter) {
    std::stable_sort(m_Indices.begin(), m_Indices.end(), sorter);
  }
}

//-----------------------------------------------------------------------------
const std::string FunctionsDataView::MENU_ACTION_SELECT = "Hook";
const std::string FunctionsDataView::MENU_ACTION_UNSELECT = "Unhook";
const std::string FunctionsDataView::MENU_ACTION_VIEW = "Visualize";
const std::string FunctionsDataView::MENU_ACTION_DISASSEMBLY =
    "Go to Disassembly";
const std::string FunctionsDataView::MENU_ACTION_SET_AS_FRAME =
    "Set as Main Frame";

//-----------------------------------------------------------------------------
std::vector<std::string> FunctionsDataView::GetContextMenu(
    int a_ClickedIndex, const std::vector<int>& a_SelectedIndices) {
  bool enable_select = false;
  bool enable_unselect = false;
  bool enable_view = absl::GetFlag(FLAGS_enable_stale_features);
  for (int index : a_SelectedIndices) {
    const Function& function = GetFunction(index);
    enable_select |= !function.IsSelected();
    enable_unselect |= function.IsSelected();
  }

  std::vector<std::string> menu;
  if (enable_select) menu.emplace_back(MENU_ACTION_SELECT);
  if (enable_unselect) menu.emplace_back(MENU_ACTION_UNSELECT);
  if (enable_view) menu.emplace_back(MENU_ACTION_VIEW);
  menu.emplace_back(MENU_ACTION_DISASSEMBLY);
  Append(menu, DataView::GetContextMenu(a_ClickedIndex, a_SelectedIndices));
  return menu;
}

//-----------------------------------------------------------------------------
void FunctionsDataView::OnContextMenu(const std::string& a_Action,
                                      int a_MenuIndex,
                                      const std::vector<int>& a_ItemIndices) {
  if (a_Action == MENU_ACTION_SELECT) {
    for (int i : a_ItemIndices) {
      GetFunction(i).Select();
    }
  } else if (a_Action == MENU_ACTION_UNSELECT) {
    for (int i : a_ItemIndices) {
      GetFunction(i).UnSelect();
    }
  } else if (a_Action == MENU_ACTION_VIEW) {
    for (int i : a_ItemIndices) {
      GetFunction(i).Print();
    }
    GOrbitApp->SendToUi("output");
  } else if (a_Action == MENU_ACTION_DISASSEMBLY) {
    uint32_t pid = Capture::GTargetProcess->GetID();
    for (int i : a_ItemIndices) {
      GetFunction(i).GetDisassembly(pid);
    }
  } else {
    DataView::OnContextMenu(a_Action, a_MenuIndex, a_ItemIndices);
  }
}

//-----------------------------------------------------------------------------
void FunctionsDataView::DoFilter() {
  m_FilterTokens = Tokenize(ToLower(m_Filter));

#ifdef WIN32
  ParallelFilter();
#else
  // TODO: port parallel filtering
  std::vector<uint32_t> indices;
  const std::vector<std::shared_ptr<Function>>& functions =
      Capture::GTargetProcess->GetFunctions();
  for (size_t i = 0; i < functions.size(); ++i) {
    auto& function = functions[i];
    std::string name = function->Lower() + function->GetLoadedModuleName();

    bool match = true;

    for (std::string& filterToken : m_FilterTokens) {
      if (name.find(filterToken) == std::string::npos) {
        match = false;
        break;
      }
    }

    if (match) {
      indices.push_back(i);
    }
  }

  m_Indices = indices;

  OnSort(m_SortingColumn, {});
#endif
}

//-----------------------------------------------------------------------------
void FunctionsDataView::ParallelFilter() {
#ifdef _WIN32
  const std::vector<std::shared_ptr<Function>>& functions =
      Capture::GTargetProcess->GetFunctions();
  const auto prio = oqpi::task_priority::normal;
  auto numWorkers = oqpi_tk::scheduler().workersCount(prio);
  // int numWorkers = oqpi::thread::hardware_concurrency();
  std::vector<std::vector<int>> indicesArray;
  indicesArray.resize(numWorkers);

  oqpi_tk::parallel_for(
      "FunctionsDataViewParallelFor", functions.size(),
      [&](int32_t a_BlockIndex, int32_t a_ElementIndex) {
        std::vector<int>& result = indicesArray[a_BlockIndex];
        const std::string& name = functions[a_ElementIndex]->Lower();
        const std::string& file = functions[a_ElementIndex]->File();

        for (std::string& filterToken : m_FilterTokens) {
          if (name.find(filterToken) == std::string::npos &&
              file.find(filterToken) == std::string::npos) {
            return;
          }
        }

        result.push_back(a_ElementIndex);
      });

  std::set<int> indicesSet;
  for (std::vector<int>& results : indicesArray) {
    for (int index : results) {
      indicesSet.insert(index);
    }
  }

  m_Indices.clear();
  for (int i : indicesSet) {
    m_Indices.push_back(i);
  }
#endif
}

//-----------------------------------------------------------------------------
void FunctionsDataView::OnDataChanged() {
  ScopeLock lock(Capture::GTargetProcess->GetDataMutex());

  size_t numFunctions = Capture::GTargetProcess->GetFunctions().size();
  m_Indices.resize(numFunctions);
  for (size_t i = 0; i < numFunctions; ++i) {
    m_Indices[i] = i;
  }

  DataView::OnDataChanged();
}

//-----------------------------------------------------------------------------
Function& FunctionsDataView::GetFunction(int a_Row) const {
  ScopeLock lock(Capture::GTargetProcess->GetDataMutex());
  const std::vector<std::shared_ptr<Function>>& functions =
      Capture::GTargetProcess->GetFunctions();
  return *functions[m_Indices[a_Row]];
}
