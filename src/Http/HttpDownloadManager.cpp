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

Future<ErrorMessageOr<CanceledOr<void>>> HttpDownloadManager::Download(
    std::string url, std::filesystem::path save_file_path, StopToken stop_token) {
  Promise<ErrorMessageOr<CanceledOr<void>>> promise;
  auto future = promise.GetFuture();

  auto current_download_operation = new HttpDownloadOperation{
      std::move(url), std::move(save_file_path), std::move(stop_token), &manager_};

  auto finish_handler = [current_download_operation, promise = std::move(promise)](
                            HttpDownloadOperation::State state,
                            std::optional<std::string> maybe_error_msg) mutable {
    if (promise.HasResult()) return;

    current_download_operation->deleteLater();
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

  QObject::connect(current_download_operation, &HttpDownloadOperation::finished,
                   std::move(finish_handler));

  current_download_operation->Start();

  download_operations_.push_back(current_download_operation);

  return future;
}

}  // namespace orbit_http