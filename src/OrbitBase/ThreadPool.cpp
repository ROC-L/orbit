// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "OrbitBase/ThreadPool.h"

#include <absl/container/flat_hash_map.h>
#include <absl/synchronization/mutex.h>
#include <absl/time/time.h>

#include <algorithm>
#include <list>
#include <thread>
#include <utility>
#include <vector>

#include "OrbitBase/Logging.h"

namespace orbit_base {
namespace {

class ThreadPoolImpl : public ThreadPool {
 public:
  explicit ThreadPoolImpl(size_t thread_pool_min_size, size_t thread_pool_max_size,
                          absl::Duration thread_ttl,
                          std::function<void(const std::unique_ptr<Action>&)> run_action);

  size_t GetPoolSize() override;
  size_t GetNumberOfBusyThreads() override;
  void Shutdown() override;
  void Wait() override;

 private:
  void ScheduleImpl(std::unique_ptr<Action> action) override;
  bool ActionsAvailableOrShutdownInitiated();
  // Blocking call - returns nullptr if the worker thread needs to exit.
  std::unique_ptr<Action> TakeAction();
  void CleanupFinishedThreads();
  void CreateWorker();
  void WorkerFunction();

  absl::Mutex mutex_;
  std::list<std::unique_ptr<Action>> scheduled_actions_;
  absl::flat_hash_map<std::thread::id, std::thread> worker_threads_;
  std::vector<std::thread> finished_threads_;
  size_t thread_pool_min_size_;
  size_t thread_pool_max_size_;
  absl::Duration thread_ttl_;
  size_t idle_threads_;
  bool shutdown_initiated_;
  std::function<void(const std::unique_ptr<Action>&)> run_action_ = nullptr;
};

ThreadPoolImpl::ThreadPoolImpl(size_t thread_pool_min_size, size_t thread_pool_max_size,
                               absl::Duration thread_ttl,
                               std::function<void(const std::unique_ptr<Action>&)> run_action)
    : thread_pool_min_size_(thread_pool_min_size),
      thread_pool_max_size_(thread_pool_max_size),
      thread_ttl_(thread_ttl),
      idle_threads_(0),
      shutdown_initiated_(false),
      run_action_(std::move(run_action)) {
  ORBIT_CHECK(thread_pool_min_size > 0);
  ORBIT_CHECK(thread_pool_max_size >= thread_pool_min_size);
  // Ttl should not be too small
  ORBIT_CHECK(thread_ttl / absl::Nanoseconds(1) >= 1000);

  absl::MutexLock lock(&mutex_);
  for (size_t i = 0; i < thread_pool_min_size; ++i) {
    CreateWorker();
  }
}

void ThreadPoolImpl::CreateWorker() {
  ORBIT_CHECK(!shutdown_initiated_);
  idle_threads_++;
  std::thread thread([this] { WorkerFunction(); });
  std::thread::id thread_id = thread.get_id();
  ORBIT_CHECK(!worker_threads_.contains(thread_id));
  worker_threads_.insert_or_assign(thread_id, std::move(thread));
}

void ThreadPoolImpl::ScheduleImpl(std::unique_ptr<Action> action) {
  std::unique_ptr<Action> wrapped_action =
      run_action_
          ? CreateAction([this, action = std::move(action)]() mutable { run_action_(action); })
          : std::move(action);

  absl::MutexLock lock(&mutex_);
  ORBIT_CHECK(!shutdown_initiated_);

  scheduled_actions_.push_back(std::move(wrapped_action));
  if (idle_threads_ < scheduled_actions_.size() && worker_threads_.size() < thread_pool_max_size_) {
    CreateWorker();
  }

  CleanupFinishedThreads();
}

void ThreadPoolImpl::CleanupFinishedThreads() {
  for (std::thread& thread : finished_threads_) {
    thread.join();
  }

  finished_threads_.clear();
}

size_t ThreadPoolImpl::GetPoolSize() {
  absl::MutexLock lock(&mutex_);
  return worker_threads_.size();
}

size_t ThreadPoolImpl::GetNumberOfBusyThreads() {
  absl::MutexLock lock(&mutex_);
  return worker_threads_.size() - idle_threads_;
}

void ThreadPoolImpl::Shutdown() {
  absl::MutexLock lock(&mutex_);
  shutdown_initiated_ = true;
}

void ThreadPoolImpl::Wait() {
  absl::MutexLock lock(&mutex_);
  ORBIT_CHECK(shutdown_initiated_);
  // First wait until all worker threads finished their work
  // and moved to finished_threads_ list.
  mutex_.Await(
      absl::Condition(&worker_threads_, &absl::flat_hash_map<std::thread::id, std::thread>::empty));

  CleanupFinishedThreads();
}

bool ThreadPoolImpl::ActionsAvailableOrShutdownInitiated() {
  return !scheduled_actions_.empty() || shutdown_initiated_;
}

std::unique_ptr<Action> ThreadPoolImpl::TakeAction() {
  while (true) {
    if (mutex_.AwaitWithTimeout(
            absl::Condition(
                +[](ThreadPoolImpl* self) { return self->ActionsAvailableOrShutdownInitiated(); },
                this),
            thread_ttl_)) {
      break;
    }

    // Timed out - check if we need to reduce thread pool.
    if (worker_threads_.size() > thread_pool_min_size_) {
      return nullptr;
    }
  }

  if (scheduled_actions_.empty()) {
    return nullptr;
  }

  std::unique_ptr<Action> action = std::move(scheduled_actions_.front());
  scheduled_actions_.pop_front();

  return action;
}

void ThreadPoolImpl::WorkerFunction() {
  while (true) {
    absl::MutexLock lock(&mutex_);
    std::unique_ptr<Action> action = TakeAction();

    ORBIT_CHECK(idle_threads_ > 0);  // Sanity check
    --idle_threads_;

    if (!action) {
      // Move this thread from the worker_threads_ to finished_threads_.
      std::thread::id thread_id = std::this_thread::get_id();
      auto it = worker_threads_.find(thread_id);
      ORBIT_CHECK(it != worker_threads_.end());
      finished_threads_.push_back(std::move(it->second));
      worker_threads_.erase(it);
      break;
    }

    mutex_.Unlock();
    action->Execute();
    mutex_.Lock();
    ++idle_threads_;
  }
}

}  // namespace

std::shared_ptr<ThreadPool> ThreadPool::Create(
    size_t thread_pool_min_size, size_t thread_pool_max_size, absl::Duration thread_ttl,
    std::function<void(const std::unique_ptr<Action>&)> run_action) {
  // The base class `Executor` uses `std::enable_shared_from_this` and requires `ThreadPool` to be
  // created as a `shared_ptr`.
  return std::make_shared<ThreadPoolImpl>(thread_pool_min_size, thread_pool_max_size, thread_ttl,
                                          std::move(run_action));
}

}  // namespace orbit_base