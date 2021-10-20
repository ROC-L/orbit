// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "PerfEventQueue.h"

#include <absl/meta/type_traits.h>
#include <stddef.h>

#include <algorithm>
#include <utility>

#include "OrbitBase/Logging.h"

namespace orbit_linux_tracing {

void PerfEventQueue::PushEvent(PerfEvent&& event) {
  const int origin_fd = event.ordered_in_file_descriptor;
  if (origin_fd == kNotOrderedInAnyFileDescriptor) {
    priority_queue_of_events_not_ordered_by_fd_.push(std::move(event));
  } else if (auto queue_it = queues_of_events_ordered_by_fd_.find(origin_fd);
             queue_it != queues_of_events_ordered_by_fd_.end()) {
    const std::unique_ptr<std::queue<PerfEvent>>& queue = queue_it->second;

    CHECK(!queue->empty());
    // Fundamental assumption: events from the same file descriptor come already in order.
    CHECK(event.timestamp >= queue->back().timestamp);
    queue->push(std::move(event));
  } else {
    queue_it = queues_of_events_ordered_by_fd_
                   .emplace(origin_fd, std::make_unique<std::queue<PerfEvent>>())
                   .first;
    const std::unique_ptr<std::queue<PerfEvent>>& queue = queue_it->second;

    queue->push(std::move(event));
    heap_of_queues_of_events_ordered_by_fd_.emplace_back(queue.get());
    MoveUpBackOfHeapOfQueues();
  }
}

bool PerfEventQueue::HasEvent() const {
  return !heap_of_queues_of_events_ordered_by_fd_.empty() ||
         !priority_queue_of_events_not_ordered_by_fd_.empty();
}

const PerfEvent& PerfEventQueue::TopEvent() {
  // As we effectively have two priority queues, get the older event between the two events at the
  // top of the two queues. In case those two events have the exact same timestamp, return the one
  // at the top of priority_queue_of_events_not_ordered_by_fd_ (and do the same in PopEvent).
  if (priority_queue_of_events_not_ordered_by_fd_.empty()) {
    CHECK(!heap_of_queues_of_events_ordered_by_fd_.empty());
    CHECK(!heap_of_queues_of_events_ordered_by_fd_.front()->empty());
    return heap_of_queues_of_events_ordered_by_fd_.front()->front();
  }
  if (heap_of_queues_of_events_ordered_by_fd_.empty()) {
    CHECK(!priority_queue_of_events_not_ordered_by_fd_.empty());
    return priority_queue_of_events_not_ordered_by_fd_.top();
  }
  return (heap_of_queues_of_events_ordered_by_fd_.front()->front().timestamp <
          priority_queue_of_events_not_ordered_by_fd_.top().timestamp)
             ? heap_of_queues_of_events_ordered_by_fd_.front()->front()
             : priority_queue_of_events_not_ordered_by_fd_.top();
}

void PerfEventQueue::PopEvent() {
  if (!priority_queue_of_events_not_ordered_by_fd_.empty() &&
      (heap_of_queues_of_events_ordered_by_fd_.empty() ||
       priority_queue_of_events_not_ordered_by_fd_.top().timestamp <=
           heap_of_queues_of_events_ordered_by_fd_.front()->front().timestamp)) {
    // The oldest event is at the top of the priority queue holding the events that cannot be
    // assumed sorted in any ring buffer. Note in particular that we return and pop this event even
    // if the event at the top of heap_of_queues_of_events_ordered_by_fd_ has the exact same
    // timestamp, as we need to be consistent with TopEvent.
    priority_queue_of_events_not_ordered_by_fd_.pop();
    return;
  }

  std::queue<PerfEvent>* top_queue = heap_of_queues_of_events_ordered_by_fd_.front();
  const int top_fd = top_queue->front().ordered_in_file_descriptor;
  top_queue->pop();

  if (top_queue->empty()) {
    queues_of_events_ordered_by_fd_.erase(top_fd);
    std::swap(heap_of_queues_of_events_ordered_by_fd_.front(),
              heap_of_queues_of_events_ordered_by_fd_.back());
    heap_of_queues_of_events_ordered_by_fd_.pop_back();
  }

  MoveDownFrontOfHeapOfQueues();
}

void PerfEventQueue::MoveDownFrontOfHeapOfQueues() {
  if (heap_of_queues_of_events_ordered_by_fd_.empty()) {
    return;
  }

  size_t current_index = 0;
  size_t new_index;
  while (true) {
    new_index = current_index;
    size_t left_index = current_index * 2 + 1;
    size_t right_index = current_index * 2 + 2;
    if (left_index < heap_of_queues_of_events_ordered_by_fd_.size() &&
        heap_of_queues_of_events_ordered_by_fd_[left_index]->front().timestamp <
            heap_of_queues_of_events_ordered_by_fd_[new_index]->front().timestamp) {
      new_index = left_index;
    }
    if (right_index < heap_of_queues_of_events_ordered_by_fd_.size() &&
        heap_of_queues_of_events_ordered_by_fd_[right_index]->front().timestamp <
            heap_of_queues_of_events_ordered_by_fd_[new_index]->front().timestamp) {
      new_index = right_index;
    }
    if (new_index != current_index) {
      std::swap(heap_of_queues_of_events_ordered_by_fd_[new_index],
                heap_of_queues_of_events_ordered_by_fd_[current_index]);
      current_index = new_index;
    } else {
      break;
    }
  }
}

void PerfEventQueue::MoveUpBackOfHeapOfQueues() {
  if (heap_of_queues_of_events_ordered_by_fd_.empty()) {
    return;
  }

  size_t current_index = heap_of_queues_of_events_ordered_by_fd_.size() - 1;
  while (current_index > 0) {
    size_t parent_index = (current_index - 1) / 2;
    if (heap_of_queues_of_events_ordered_by_fd_[parent_index]->front().timestamp <=
        heap_of_queues_of_events_ordered_by_fd_[current_index]->front().timestamp) {
      break;
    }
    std::swap(heap_of_queues_of_events_ordered_by_fd_[parent_index],
              heap_of_queues_of_events_ordered_by_fd_[current_index]);
    current_index = parent_index;
  }
}

}  // namespace orbit_linux_tracing
