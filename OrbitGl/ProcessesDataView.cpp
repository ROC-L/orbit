// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ProcessesDataView.h"

#include <utility>

#include "App.h"
#include "Callstack.h"
#include "Capture.h"
#include "ModulesDataView.h"
#include "Params.h"
#include "absl/strings/str_format.h"

ProcessesDataView::ProcessesDataView()
    : DataView(DataViewType::PROCESSES), selected_process_id_(-1) {}

const std::vector<DataView::Column>& ProcessesDataView::GetColumns() {
  static const std::vector<Column> columns = [] {
    std::vector<Column> columns;
    columns.resize(COLUMN_NUM);
    columns[COLUMN_PID] = {"PID", .2f, SortingOrder::Ascending};
    columns[COLUMN_NAME] = {"Name", .6f, SortingOrder::Ascending};
    columns[COLUMN_CPU] = {"CPU", .0f, SortingOrder::Descending};
    return columns;
  }();
  return columns;
}

std::string ProcessesDataView::GetValue(int row, int col) {
  const ProcessInfo& process = GetProcess(row);

  switch (col) {
    case COLUMN_PID:
      return std::to_string(process.pid());
    case COLUMN_NAME:
      return process.name();
    case COLUMN_CPU:
      return absl::StrFormat("%.1f", process.cpu_usage());
    default:
      return "";
  }
}

std::string ProcessesDataView::GetToolTip(int row, int /*column*/) {
  return GetProcess(row).command_line();
}

#define ORBIT_PROC_SORT(Member)                                          \
  [&](int a, int b) {                                                    \
    return OrbitUtils::Compare(processes[a].Member, processes[b].Member, \
                               ascending);                               \
  }

void ProcessesDataView::DoSort() {
  bool ascending = m_SortingOrders[m_SortingColumn] == SortingOrder::Ascending;
  std::function<bool(int a, int b)> sorter = nullptr;

  const std::vector<ProcessInfo>& processes = process_list_;

  switch (m_SortingColumn) {
    case COLUMN_PID:
      sorter = ORBIT_PROC_SORT(pid());
      break;
    case COLUMN_NAME:
      sorter = ORBIT_PROC_SORT(name());
      break;
    case COLUMN_CPU:
      sorter = ORBIT_PROC_SORT(cpu_usage());
      break;
    default:
      break;
  }

  if (sorter) {
    std::stable_sort(indices_.begin(), indices_.end(), sorter);
  }

  SetSelectedItem();
}

#undef ORBIT_PROC_SORT

void ProcessesDataView::OnSelect(int index) {
  const ProcessInfo& selected_process = GetProcess(index);
  selected_process_id_ = selected_process.pid();

  SetSelectedItem();

  if (selection_listener_) {
    selection_listener_(selected_process_id_);
  }
}

int32_t ProcessesDataView::GetSelectedProcessId() const {
  return selected_process_id_;
}

int32_t ProcessesDataView::GetFirstProcessId() const {
  if (indices_.empty()) {
    return -1;
  }
  return process_list_[indices_[0]].pid();
}

void ProcessesDataView::SetSelectedItem() {
  for (size_t i = 0; i < GetNumElements(); ++i) {
    if (GetProcess(i).pid() == selected_process_id_) {
      m_SelectedIndex = i;
      return;
    }
  }

  // This happens when selected process disappears from the list.
  m_SelectedIndex = -1;
}

bool ProcessesDataView::SelectProcess(const std::string& process_name) {
  for (size_t i = 0; i < GetNumElements(); ++i) {
    const ProcessInfo& process = GetProcess(i);
    // TODO: What if there are multiple processes with the same substring?
    if (process.full_path().find(process_name) != std::string::npos) {
      OnSelect(i);
      return true;
    }
  }

  return false;
}

bool ProcessesDataView::SelectProcess(int32_t process_id) {
  for (size_t i = 0; i < GetNumElements(); ++i) {
    const ProcessInfo& process = GetProcess(i);
    if (process.pid() == process_id) {
      OnSelect(i);
      return true;
    }
  }

  return false;
}

void ProcessesDataView::DoFilter() {
  std::vector<uint32_t> indices;
  const std::vector<ProcessInfo>& processes = process_list_;

  std::vector<std::string> tokens = absl::StrSplit(ToLower(m_Filter), ' ');

  for (size_t i = 0; i < processes.size(); ++i) {
    const ProcessInfo& process = processes[i];
    std::string name = ToLower(process.name());
    std::string type = process.is_64_bit() ? "64" : "32";

    bool match = true;

    for (std::string& filterToken : tokens) {
      if (!(name.find(filterToken) != std::string::npos ||
            type.find(filterToken) != std::string::npos)) {
        match = false;
        break;
      }
    }

    if (match) {
      indices.push_back(i);
    }
  }

  indices_ = indices;

  OnSort(m_SortingColumn, {});
}

void ProcessesDataView::UpdateProcessList() {
  size_t numProcesses = process_list_.size();
  indices_.resize(numProcesses);
  for (size_t i = 0; i < numProcesses; ++i) {
    indices_[i] = i;
  }
}

void ProcessesDataView::SetProcessList(
    const std::vector<ProcessInfo>& process_list) {
  process_list_ = process_list;
  UpdateProcessList();
  OnSort(m_SortingColumn, {});
  OnFilter(m_Filter);
  SetSelectedItem();
}

const ProcessInfo& ProcessesDataView::GetProcess(uint32_t row) const {
  return process_list_[indices_[row]];
}

void ProcessesDataView::SetSelectionListener(
    const std::function<void(int32_t)>& selection_listener) {
  selection_listener_ = selection_listener;
}
