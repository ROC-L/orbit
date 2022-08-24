// Copyright (c) 2022 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYMBOLS_SYMBOL_LOADING_OUTCOME_H_
#define SYMBOLS_SYMBOL_LOADING_OUTCOME_H_

#include <filesystem>
#include <variant>

#include "OrbitBase/CanceledOr.h"
#include "OrbitBase/Logging.h"
#include "OrbitBase/NotFoundOr.h"
#include "OrbitBase/Result.h"

namespace orbit_symbols {

struct SuccessOutcome {
  enum class SymbolSource {
    kUnknown,
    kOrbitCache,
    kLocalStadiaSdk,
    kStadiaInstance,
    kSymbolLocationsDialog,
    kAdditionalSymbolPathsFlag
  };
  enum class SymbolFileSeparation { kDifferentFile, kModuleFile };

  explicit SuccessOutcome(std::filesystem::path path, SymbolSource symbol_source,
                          SymbolFileSeparation symbol_file_separation)
      : path(std::move(path)),
        symbol_source(symbol_source),
        symbol_file_separation(symbol_file_separation) {}
  std::filesystem::path path;
  SymbolSource symbol_source;
  SymbolFileSeparation symbol_file_separation;
};

using SymbolLoadingOutcome =
    ErrorMessageOr<orbit_base::CanceledOr<orbit_base::NotFoundOr<SuccessOutcome>>>;

[[nodiscard]] bool IsCanceled(const SymbolLoadingOutcome& outcome);
[[nodiscard]] bool IsNotFound(const SymbolLoadingOutcome& outcome);
[[nodiscard]] std::string GetNotFoundMessage(const SymbolLoadingOutcome& outcome);
[[nodiscard]] bool IsSuccessOutcome(const SymbolLoadingOutcome& outcome);
[[nodiscard]] SuccessOutcome GetSuccessOutcome(const SymbolLoadingOutcome& outcome);

}  // namespace orbit_symbols

#endif  // SYMBOLS_SYMBOL_LOADING_OUTCOME_H_