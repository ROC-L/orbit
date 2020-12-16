// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIVE_FUNCTIONS_H_
#define LIVE_FUNCTIONS_H_

#include <functional>

#include "LiveFunctionsDataView.h"
#include "OrbitBase/Profiling.h"
#include "TextBox.h"
#include "absl/container/flat_hash_map.h"
#include "capture_data.pb.h"

class OrbitApp;

class LiveFunctionsController {
 public:
  explicit LiveFunctionsController(OrbitApp* app)
      : live_functions_data_view_(this, app), app_{app} {}

  LiveFunctionsDataView& GetDataView() { return live_functions_data_view_; }

  bool OnAllNextButton();
  bool OnAllPreviousButton();

  void OnNextButton(uint64_t id);
  void OnPreviousButton(uint64_t id);
  void OnDeleteButton(uint64_t id);

  void Reset();
  void OnDataChanged() { live_functions_data_view_.OnDataChanged(); }

  void SetAddIteratorCallback(
      std::function<void(uint64_t, orbit_client_protos::FunctionInfo*)> callback) {
    add_iterator_callback_ = callback;
  }

  uint64_t GetCaptureMin();
  uint64_t GetCaptureMax();
  uint64_t GetStartTime(uint64_t index);

  void AddIterator(orbit_client_protos::FunctionInfo* function);

 private:
  void Move();

  LiveFunctionsDataView live_functions_data_view_;

  absl::flat_hash_map<uint64_t, const orbit_client_protos::FunctionInfo*> function_iterators_;
  absl::flat_hash_map<uint64_t, const TextBox*> current_textboxes_;

  std::function<void(uint64_t, orbit_client_protos::FunctionInfo*)> add_iterator_callback_;

  uint64_t next_iterator_id_ = 0;

  uint64_t id_to_select_ = 0;

  OrbitApp* app_ = nullptr;
};

#endif  // LIVE_FUNCTIONS_H_
