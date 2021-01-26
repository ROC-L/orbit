// Copyright (c) 2021 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <chrono>
#include <thread>

#include "OrbitBase/Promise.h"
#include "OrbitBase/ThreadPool.h"
#include "QtUtils/FutureWatcher.h"

namespace orbit_qt_utils {

static int argc = 0;

TEST(FutureWatcher, WaitFor) {
  QCoreApplication app{argc, nullptr};

  orbit_base::Promise<void> promise{};
  orbit_base::Future<void> future = promise.GetFuture();

  QTimer completion_timer{};
  QObject::connect(&completion_timer, &QTimer::timeout, [&]() { promise.MarkFinished(); });
  completion_timer.start(std::chrono::milliseconds{0});

  FutureWatcher watcher{};
  EXPECT_EQ(watcher.WaitFor(std::move(future), std::chrono::milliseconds{20}),
            FutureWatcher::Reason::kFutureCompleted);
}

TEST(FutureWatcher, WaitForWithTimeout) {
  QCoreApplication app{argc, nullptr};

  orbit_base::Promise<void> promise{};
  orbit_base::Future<void> future = promise.GetFuture();

  QTimer completion_timer{};
  QObject::connect(&completion_timer, &QTimer::timeout, [&]() { promise.MarkFinished(); });
  completion_timer.start(std::chrono::milliseconds{40});

  FutureWatcher watcher{};
  EXPECT_EQ(watcher.WaitFor(std::move(future), std::chrono::milliseconds{20}),
            FutureWatcher::Reason::kTimeout);
}

TEST(FutureWatcher, WaitForWithAbort) {
  QCoreApplication app{argc, nullptr};

  orbit_base::Promise<void> promise{};
  orbit_base::Future<void> future = promise.GetFuture();

  QTimer completion_timer{};
  QObject::connect(&completion_timer, &QTimer::timeout, [&]() { promise.MarkFinished(); });
  completion_timer.start(std::chrono::milliseconds{40});

  FutureWatcher watcher{};

  QTimer abort_timer{};
  QObject::connect(&abort_timer, &QTimer::timeout, &watcher, &FutureWatcher::Abort);
  abort_timer.start(std::chrono::milliseconds{10});

  EXPECT_EQ(watcher.WaitFor(std::move(future), std::chrono::milliseconds{20}),
            FutureWatcher::Reason::kAbortRequested);
}

TEST(FutureWatcher, WaitForWithThreadPool) {
  // One background job which is supposed to succeed. (No timeout, no abort)

  QCoreApplication app{argc, nullptr};

  constexpr size_t kThreadPoolMinSize = 1;
  constexpr size_t kThreadPoolMaxSize = 2;
  constexpr absl::Duration kThreadTtl = absl::Milliseconds(5);
  std::unique_ptr<ThreadPool> thread_pool =
      ThreadPool::Create(kThreadPoolMinSize, kThreadPoolMaxSize, kThreadTtl);

  absl::Mutex mutex;

  mutex.Lock();
  orbit_base::Future<void> future = thread_pool->Schedule([&]() { absl::MutexLock lock(&mutex); });

  EXPECT_TRUE(future.IsValid());
  EXPECT_FALSE(future.IsFinished());

  // The lambda will be executed by the event loop, running inside of watcher.WaitFor.
  QTimer::singleShot(std::chrono::milliseconds{5}, [&]() { mutex.Unlock(); });

  FutureWatcher watcher{};
  const auto reason = watcher.WaitFor(std::move(future), std::nullopt);

  EXPECT_EQ(reason, FutureWatcher::Reason::kFutureCompleted);

  thread_pool->ShutdownAndWait();
}

TEST(FutureWatcher, WaitForWithThreadPoolAndTimeout) {
  // One background job which is supposed to time out.

  QCoreApplication app{argc, nullptr};

  constexpr size_t kThreadPoolMinSize = 1;
  constexpr size_t kThreadPoolMaxSize = 2;
  constexpr absl::Duration kThreadTtl = absl::Milliseconds(5);
  std::unique_ptr<ThreadPool> thread_pool =
      ThreadPool::Create(kThreadPoolMinSize, kThreadPoolMaxSize, kThreadTtl);

  absl::Mutex mutex;

  mutex.Lock();
  orbit_base::Future<void> future = thread_pool->Schedule([&]() { absl::MutexLock lock(&mutex); });

  EXPECT_TRUE(future.IsValid());
  EXPECT_FALSE(future.IsFinished());

  // The lambda will be executed by the event loop, running inside of watcher.WaitFor.
  QTimer::singleShot(std::chrono::milliseconds{20}, [&]() { mutex.Unlock(); });

  FutureWatcher watcher{};
  const auto reason = watcher.WaitFor(std::move(future), std::chrono::milliseconds{5});

  EXPECT_EQ(reason, FutureWatcher::Reason::kTimeout);

  mutex.Unlock();
  thread_pool->ShutdownAndWait();
}

TEST(FutureWatcher, WaitForAllWithThreadPool) {
  // Multiple background jobs which are all supposed to succeed.

  QCoreApplication app{argc, nullptr};

  constexpr size_t kThreadPoolMinSize = 1;
  constexpr size_t kThreadPoolMaxSize = 2;
  constexpr absl::Duration kThreadTtl = absl::Milliseconds(5);
  std::unique_ptr<ThreadPool> thread_pool =
      ThreadPool::Create(kThreadPoolMinSize, kThreadPoolMaxSize, kThreadTtl);

  absl::Mutex mutex;
  mutex.Lock();

  std::vector<orbit_base::Future<void>> futures;
  for (int i = 0; i < 10; ++i) {
    futures.emplace_back(thread_pool->Schedule([&]() { absl::MutexLock lock(&mutex); }));

    orbit_base::Future<void>& future = futures.back();
    EXPECT_TRUE(future.IsValid());
    EXPECT_FALSE(future.IsFinished());
  }

  // The lambda will be executed by the event loop, running inside of watcher.WaitFor.
  QTimer::singleShot(std::chrono::milliseconds{5}, [&]() { mutex.Unlock(); });

  FutureWatcher watcher{};
  const auto reason = watcher.WaitForAll(absl::MakeSpan(futures), std::nullopt);

  EXPECT_EQ(reason, FutureWatcher::Reason::kFutureCompleted);

  thread_pool->ShutdownAndWait();
}

TEST(FutureWatcher, WaitForAllWithThreadPoolAndTimeout) {
  // Multiple background jobs which are all supposed to time out.

  QCoreApplication app{argc, nullptr};

  constexpr size_t kThreadPoolMinSize = 1;
  constexpr size_t kThreadPoolMaxSize = 2;
  constexpr absl::Duration kThreadTtl = absl::Milliseconds(5);
  std::unique_ptr<ThreadPool> thread_pool =
      ThreadPool::Create(kThreadPoolMinSize, kThreadPoolMaxSize, kThreadTtl);

  absl::Mutex mutex;
  mutex.Lock();

  std::vector<orbit_base::Future<void>> futures;
  for (int i = 0; i < 10; ++i) {
    futures.emplace_back(thread_pool->Schedule([&]() {
      absl::MutexLock lock(&mutex);
      std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }));

    orbit_base::Future<void>& future = futures.back();
    EXPECT_TRUE(future.IsValid());
    EXPECT_FALSE(future.IsFinished());
  }

  // The lambda will be executed by the event loop, running inside of watcher.WaitFor.
  QTimer::singleShot(std::chrono::milliseconds{5}, [&]() { mutex.Unlock(); });

  // The timer starts execution of background jobs after 5ms. Every background job takes 1ms and
  // they can't run in parallel due to the mutex. That means in total 15ms of execution time is
  // needed, but the watcher will time out after 10ms.

  FutureWatcher watcher{};
  const auto reason = watcher.WaitForAll(absl::MakeSpan(futures), std::chrono::milliseconds{10});

  EXPECT_EQ(reason, FutureWatcher::Reason::kTimeout);

  // This function will wait until all jobs are finished. That means the whole test will need a
  // little longer than 15ms.
  thread_pool->ShutdownAndWait();
}

TEST(FutureWatcher, WaitForAllWithEmptyList) {
  QCoreApplication app{argc, nullptr};

  FutureWatcher watcher{};
  const auto reason = watcher.WaitForAll({}, std::nullopt);

  EXPECT_EQ(reason, FutureWatcher::Reason::kFutureCompleted);
}

}  // namespace orbit_qt_utils
