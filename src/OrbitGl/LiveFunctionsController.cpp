// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "LiveFunctionsController.h"

#include <absl/container/flat_hash_map.h>
#include <absl/meta/type_traits.h>

#include <algorithm>
#include <limits>
#include <type_traits>
#include <utility>

#include "App.h"
#include "OrbitClientModel/CaptureData.h"
#include "TimeGraph.h"

using orbit_client_protos::FunctionInfo;
using orbit_client_protos::TimerInfo;

namespace {

std::pair<uint64_t, uint64_t> ComputeMinMaxTime(
    const absl::flat_hash_map<uint64_t, const TextBox*>& text_boxes) {
  uint64_t min_time = std::numeric_limits<uint64_t>::max();
  uint64_t max_time = std::numeric_limits<uint64_t>::min();
  for (auto& text_box : text_boxes) {
    min_time = std::min(min_time, text_box.second->GetTimerInfo().start());
    max_time = std::max(max_time, text_box.second->GetTimerInfo().start());
  }
  return std::make_pair(min_time, max_time);
}

uint64_t AbsDiff(uint64_t a, uint64_t b) {
  if (a > b) {
    return a - b;
  } else {
    return b - a;
  }
}

const TextBox* ClosestTo(uint64_t point, const TextBox* box_a, const TextBox* box_b) {
  uint64_t a_diff = AbsDiff(point, box_a->GetTimerInfo().start());
  uint64_t b_diff = AbsDiff(point, box_b->GetTimerInfo().start());
  if (a_diff <= b_diff) {
    return box_a;
  }
  return box_b;
}

const TextBox* SnapToClosestStart(uint64_t function_id) {
  double min_us = GCurrentTimeGraph->GetMinTimeUs();
  double max_us = GCurrentTimeGraph->GetMaxTimeUs();
  double center_us = 0.5 * max_us + 0.5 * min_us;
  uint64_t center = GCurrentTimeGraph->GetTickFromUs(center_us);

  // First, we find the next function call (text box) that has its end timestamp
  // after center - 1 (we use center - 1 to make sure that center itself is
  // included in the timerange that we search). Note that FindNextFunctionCall
  // uses the end marker of the timer as a timestamp.
  const TextBox* box = GCurrentTimeGraph->FindNextFunctionCall(function_id, center - 1);

  // If we cannot find a next function call, then the closest one is the first
  // call we find before center.
  if (!box) {
    return GCurrentTimeGraph->FindPreviousFunctionCall(function_id, center);
  }

  // We have to consider the case where center falls to the right of the start
  // marker of 'box'. In this case, the closest box can be any of two boxes:
  // 'box' or the next one. It cannot be any box before 'box' because we are
  // using the start marker to measure the distance.
  if (box->GetTimerInfo().start() <= center) {
    const TextBox* next_box =
        GCurrentTimeGraph->FindNextFunctionCall(function_id, box->GetTimerInfo().end());
    if (!next_box) {
      return box;
    }

    return ClosestTo(center, box, next_box);
  }

  // The center is to the left of 'box', so the closest box is either 'box' or
  // the next box to the left of the center.
  const TextBox* previous_box =
      GCurrentTimeGraph->FindPreviousFunctionCall(function_id, box->GetTimerInfo().start());

  if (!previous_box) {
    return box;
  }

  return ClosestTo(center, previous_box, box);
}

}  // namespace

void LiveFunctionsController::Move() {
  if (!current_textboxes_.empty()) {
    auto min_max = ComputeMinMaxTime(current_textboxes_);
    GCurrentTimeGraph->HorizontallyMoveIntoView(TimeGraph::VisibilityType::kFullyVisible,
                                                min_max.first, min_max.second, 0.5);
  }
  GCurrentTimeGraph->SetIteratorOverlayData(current_textboxes_, iterator_id_to_function_id_);
}

bool LiveFunctionsController::OnAllNextButton() {
  absl::flat_hash_map<uint64_t, const TextBox*> next_boxes;
  uint64_t id_with_min_timestamp = 0;
  uint64_t min_timestamp = std::numeric_limits<uint64_t>::max();
  for (auto it : iterator_id_to_function_id_) {
    uint64_t function_id = it.second;
    const TextBox* current_box = current_textboxes_.find(it.first)->second;
    const TextBox* box =
        GCurrentTimeGraph->FindNextFunctionCall(function_id, current_box->GetTimerInfo().end());
    if (box == nullptr) {
      return false;
    }
    if (box->GetTimerInfo().start() < min_timestamp) {
      min_timestamp = box->GetTimerInfo().start();
      id_with_min_timestamp = it.first;
    }
    next_boxes.insert(std::make_pair(it.first, box));
  }

  // We only want to commit to the new boxes when all boxes can be moved.
  current_textboxes_ = next_boxes;
  id_to_select_ = id_with_min_timestamp;
  Move();
  return true;
}

bool LiveFunctionsController::OnAllPreviousButton() {
  absl::flat_hash_map<uint64_t, const TextBox*> next_boxes;
  uint64_t id_with_min_timestamp = 0;
  uint64_t min_timestamp = std::numeric_limits<uint64_t>::max();
  for (auto it : iterator_id_to_function_id_) {
    uint64_t function_id = it.second;
    const TextBox* current_box = current_textboxes_.find(it.first)->second;
    const TextBox* box =
        GCurrentTimeGraph->FindPreviousFunctionCall(function_id, current_box->GetTimerInfo().end());
    if (box == nullptr) {
      return false;
    }
    if (box->GetTimerInfo().start() < min_timestamp) {
      min_timestamp = box->GetTimerInfo().start();
      id_with_min_timestamp = it.first;
    }
    next_boxes.insert(std::make_pair(it.first, box));
  }

  // We only want to commit to the new boxes when all boxes can be moved.
  current_textboxes_ = next_boxes;
  id_to_select_ = id_with_min_timestamp;
  Move();
  return true;
}

void LiveFunctionsController::OnNextButton(uint64_t id) {
  const TextBox* text_box = GCurrentTimeGraph->FindNextFunctionCall(
      iterator_id_to_function_id_[id], current_textboxes_[id]->GetTimerInfo().end());
  // If text_box is nullptr, then we have reached the right end of the timeline.
  if (text_box != nullptr) {
    current_textboxes_[id] = text_box;
  }
  id_to_select_ = id;
  Move();
}
void LiveFunctionsController::OnPreviousButton(uint64_t id) {
  const TextBox* text_box = GCurrentTimeGraph->FindPreviousFunctionCall(
      iterator_id_to_function_id_[id], current_textboxes_[id]->GetTimerInfo().end());
  // If text_box is nullptr, then we have reached the left end of the timeline.
  if (text_box != nullptr) {
    current_textboxes_[id] = text_box;
  }
  id_to_select_ = id;
  Move();
}

void LiveFunctionsController::OnDeleteButton(uint64_t id) {
  current_textboxes_.erase(id);
  iterator_id_to_function_id_.erase(id);
  // If we erase the iterator that was last used by the user, then
  // we need to switch last_id_pressed_ to an existing id.
  if (id == id_to_select_ && !current_textboxes_.empty()) {
    id_to_select_ = current_textboxes_.begin()->first;
  } else if (current_textboxes_.empty()) {
    id_to_select_ = orbit_grpc_protos::kInvalidFunctionId;
  }
  Move();
}

void LiveFunctionsController::AddIterator(uint64_t function_id, const FunctionInfo* function) {
  uint64_t iterator_id = next_iterator_id_++;
  const TextBox* box = app_->selected_text_box();
  // If no box is currently selected or the selected box is a different
  // function, we search for the closest box to the current center of the
  // screen.
  if (!box || box->GetTimerInfo().function_id() != function_id) {
    box = SnapToClosestStart(function_id);
  }

  iterator_id_to_function_id_.insert(std::make_pair(iterator_id, function_id));
  current_textboxes_.insert(std::make_pair(iterator_id, box));
  id_to_select_ = iterator_id;
  if (add_iterator_callback_) {
    add_iterator_callback_(iterator_id, function);
  }
  Move();
}

uint64_t LiveFunctionsController::GetStartTime(uint64_t index) {
  const auto& it = current_textboxes_.find(index);
  if (it != current_textboxes_.end()) {
    return it->second->GetTimerInfo().start();
  }
  return GetCaptureMin();
}

uint64_t LiveFunctionsController::GetCaptureMin() { return GCurrentTimeGraph->GetCaptureMin(); }
uint64_t LiveFunctionsController::GetCaptureMax() { return GCurrentTimeGraph->GetCaptureMax(); }

void LiveFunctionsController::Reset() {
  iterator_id_to_function_id_.clear();
  current_textboxes_.clear();
  GCurrentTimeGraph->SetIteratorOverlayData({}, {});
  id_to_select_ = orbit_grpc_protos::kInvalidFunctionId;
}
