#include "LiveFunctions.h"

#include <utility>

#include "TimeGraph.h"

std::pair<uint64_t, uint64_t> ComputeMinMaxTime(
    const absl::flat_hash_map<uint64_t, TextBox*>& text_boxes) {
  uint64_t min_time = std::numeric_limits<uint64_t>::max();
  uint64_t max_time = std::numeric_limits<uint64_t>::min();
  for (auto& text_box : text_boxes) {
    min_time = std::min(min_time, text_box.second->GetTimer().m_Start);
    max_time = std::max(max_time, text_box.second->GetTimer().m_End);
  }
  return std::make_pair(min_time, max_time);
}

void LiveFunctions::Move() {
  if (!current_textboxes_.empty()) {
    auto min_max = ComputeMinMaxTime(current_textboxes_);
    GCurrentTimeGraph->Zoom(min_max.first, min_max.second);
    if (current_textboxes_.find(id_to_select_) != current_textboxes_.end()) {
      GCurrentTimeGraph->Select(current_textboxes_[id_to_select_]);
    } else {
      CHECK(false);
    }
  } else {
    GCurrentTimeGraph->ZoomAll();
  }
  GCurrentTimeGraph->SetCurrentTextBoxes(current_textboxes_);
}

bool LiveFunctions::OnAllNextButton() {
  absl::flat_hash_map<uint64_t, TextBox*> next_boxes;
  uint64_t id_with_min_timestamp = 0;
  uint64_t min_timestamp = std::numeric_limits<uint64_t>::max();
  for (auto it : function_iterators_) {
    Function* function = it.second;
    TextBox* current_box = current_textboxes_.find(it.first)->second;
    TextBox* box = live_functions_data_view_.FindNext(
        *function, current_box->GetTimer().m_End);
    if (box == nullptr) {
      return false;
    }
    if (box->GetTimer().m_Start < min_timestamp) {
      min_timestamp = box->GetTimer().m_Start;
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

bool LiveFunctions::OnAllPreviousButton() {
  absl::flat_hash_map<uint64_t, TextBox*> next_boxes;
  uint64_t id_with_min_timestamp = 0;
  uint64_t min_timestamp = std::numeric_limits<uint64_t>::max();
  for (auto it : function_iterators_) {
    Function* function = it.second;
    TextBox* current_box = current_textboxes_.find(it.first)->second;
    TextBox* box = live_functions_data_view_.FindPrevious(
        *function, current_box->GetTimer().m_End);
    if (box == nullptr) {
      return false;
    }
    if (box->GetTimer().m_Start < min_timestamp) {
      min_timestamp = box->GetTimer().m_Start;
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

void LiveFunctions::OnNextButton(uint64_t id) {
  TextBox* text_box = live_functions_data_view_.FindNext(
      *(function_iterators_[id]), current_textboxes_[id]->GetTimer().m_End);
  // If text_box is nullptr, then we have reached the right end of the timeline.
  if (text_box != nullptr) {
    current_textboxes_[id] = text_box;
  }
  id_to_select_ = id;
  Move();
}
void LiveFunctions::OnPreviousButton(uint64_t id) {
  TextBox* text_box = live_functions_data_view_.FindPrevious(
      *(function_iterators_[id]), current_textboxes_[id]->GetTimer().m_Start);
  // If text_box is nullptr, then we have reached the left end of the timeline.
  if (text_box != nullptr) {
    current_textboxes_[id] = text_box;
  }
  id_to_select_ = id;
  Move();
}

void LiveFunctions::OnDeleteButton(uint64_t id) {
  current_textboxes_.erase(id);
  function_iterators_.erase(id);
  // If we erase the iterator that was last used by the user, then
  // we need to switch last_id_pressed_ to an existing id.
  if (id == id_to_select_ && !current_textboxes_.empty()) {
    id_to_select_ = current_textboxes_.begin()->first;
  } else if (current_textboxes_.empty()) {
    // TODO: Not sure this is a good idea...
    id_to_select_ = 0;
  }
  Move();
}

void LiveFunctions::AddIterator(Function* function, TextBox* current_textbox) {
  uint64_t id = next_iterator_id_;
  ++next_iterator_id_;

  function_iterators_.insert(std::make_pair(id, function));
  current_textboxes_.insert(std::make_pair(id, current_textbox));
  id_to_select_ = id;
  if (add_iterator_callback_) {
    add_iterator_callback_(id, function);
  }
  Move();
}