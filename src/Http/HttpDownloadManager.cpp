// Copyright (c) 2022 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "Http/HttpDownloadManager.h"

namespace orbit_http {
using orbit_base::CanceledOr;
using orbit_base::Future;
using orbit_base::Promise;
using orbit_base::StopToken;
using orbit_http_internal::HttpDownloadOperation;

HttpDownloadManager::~HttpDownloadManager() {
  waiting_download_operations_ = std::queue<HttpDownloadOperationMetadata>();
  if (current_download_operation_ != nullptr) current_download_operation_->Abort();
}

Future<ErrorMessageOr<CanceledOr<void>>> HttpDownloadManager::Download(
    const std::string& url, const std::filesystem::path& save_file_path, StopToken stop_token) {
  Promise<ErrorMessageOr<CanceledOr<void>>> promise;
  auto future = promise.GetFuture();
  DoDownload({url, save_file_path, std::move(stop_token), std::move(promise)});
  return future;
}

void HttpDownloadManager::DoDownload(HttpDownloadOperationMetadata metadata) {
  if (current_download_operation_ != nullptr) {
    waiting_download_operations_.push(std::move(metadata));
    return;
  }

  current_download_operation_ = new HttpDownloadOperation{
      metadata.url, metadata.save_file_path, std::move(metadata.stop_token), &manager_};

  auto finish_handler =
      [this, url = std::move(metadata.url), save_file_path = std::move(metadata.save_file_path),
       promise = std::move(metadata.promise)](HttpDownloadOperation::State state,
                                              std::optional<std::string> maybe_error_msg) mutable {
        if (promise.HasResult()) return;

        current_download_operation_->deleteLater();
        current_download_operation_ = nullptr;

        if (!waiting_download_operations_.empty()) {
          QMetaObject::invokeMethod(
              this,
              [this]() {
                DoDownload(std::move(waiting_download_operations_.front()));
                waiting_download_operations_.pop();
              },
              Qt::QueuedConnection);
        }

        switch (state) {
          case HttpDownloadOperation::State::kCancelled:
            promise.SetResult(orbit_base::Canceled{});
            break;
          case HttpDownloadOperation::State::kDone:
            promise.SetResult(outcome::success());
            break;
          case HttpDownloadOperation::State::kError:
            promise.SetResult(ErrorMessage{std::move(maybe_error_msg.value())});
            break;
          case HttpDownloadOperation::State::kStarted:
          case HttpDownloadOperation::State::kInitial:
            ORBIT_UNREACHABLE();
        }
      };

  QObject::connect(current_download_operation_, &HttpDownloadOperation::finished,
                   std::move(finish_handler));

  current_download_operation_->Start();
}

}  // namespace orbit_http