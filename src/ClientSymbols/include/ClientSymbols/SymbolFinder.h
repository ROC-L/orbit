// Copyright (c) 2022 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLIENT_SYMBOLS_SYMBOL_FINDER_H_
#define CLIENT_SYMBOLS_SYMBOL_FINDER_H_

#include <absl/container/flat_hash_map.h>

#include <thread>

#include "OrbitBase/CanceledOr.h"
#include "OrbitBase/Future.h"
#include "OrbitBase/Result.h"
#include "OrbitBase/StopSource.h"

namespace orbit_client_symbols {

class SymbolFinder {
 public:
  explicit SymbolFinder(std::thread::id thread_id) : main_thread_id_(thread_id) {}

  using SymbolFindingResult =
      orbit_base::Future<ErrorMessageOr<orbit_base::CanceledOr<std::filesystem::path>>>;

  struct ModuleDownloadOperation {
    orbit_base::StopSource stop_source;
    SymbolFindingResult future;
  };

  // The following methods access symbol_files_currently_downloading_ and verify whether the call is
  // from main thread.
  [[nodiscard]] std::optional<SymbolFindingResult> GetDownloadingResultByModulePath(
      const std::string& module_file_path) const;
  [[nodiscard]] bool IsModuleDownloading(const std::string& module_file_path) const;
  // Stop the downloading operation if the module is currently being downloaded;
  // otherwise, do nothing.
  void StopModuleDownloading(const std::string& module_file_path);
  void AddToCurrentlyDownloading(std::string module_file_path,
                                 ModuleDownloadOperation download_operation);
  void RemoveFromCurrentlyDownloading(const std::string& module_file_path);

 private:
  const std::thread::id main_thread_id_;

  // Map of module file path to download operation future, that holds all symbol downloads that
  // are currently in progress.
  // ONLY access this from the main thread.
  absl::flat_hash_map<std::string, ModuleDownloadOperation> symbol_files_currently_downloading_;
};

}  // namespace orbit_client_symbols

#endif  // CLIENT_SYMBOLS_SYMBOL_FINDER_H_
