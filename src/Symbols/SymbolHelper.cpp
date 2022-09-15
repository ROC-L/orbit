// Copyright (c) 2021 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "Symbols/SymbolHelper.h"

#include <absl/strings/ascii.h>
#include <absl/strings/match.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_format.h>
#include <absl/strings/str_replace.h>
#include <absl/strings/str_split.h>
#include <absl/types/span.h>
#include <llvm/Object/Binary.h>
#include <llvm/Object/ObjectFile.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <set>
#include <string_view>
#include <system_error>
#include <vector>

#include "Introspection/Introspection.h"
#include "ObjectUtils/ElfFile.h"
#include "ObjectUtils/SymbolsFile.h"
#include "OrbitBase/ExecutablePath.h"
#include "OrbitBase/File.h"
#include "OrbitBase/Logging.h"
#include "OrbitBase/ReadFileToString.h"
#include "OrbitBase/Result.h"
#include "OrbitBase/StopSource.h"
#include "OrbitBase/WriteStringToFile.h"
#include "SymbolProvider/ModuleIdentifier.h"
#include "SymbolProvider/StructuredDebugDirectorySymbolProvider.h"
#include "SymbolProvider/SymbolLoadingOutcome.h"
#include "SymbolProvider/SymbolProvider.h"
#include "Symbols/SymbolUtils.h"

using orbit_grpc_protos::ModuleSymbols;

namespace fs = std::filesystem;
using orbit_grpc_protos::ModuleInfo;
using orbit_object_utils::CreateSymbolsFile;
using orbit_object_utils::ElfFile;
using orbit_object_utils::ObjectFileInfo;
using orbit_object_utils::SymbolsFile;
using orbit_symbol_provider::ModuleIdentifier;
using orbit_symbol_provider::StructuredDebugDirectorySymbolProvider;
using orbit_symbol_provider::SymbolLoadingOutcome;
using orbit_symbol_provider::SymbolProvider;
using SymbolSource = orbit_symbol_provider::SymbolLoadingSuccessResult::SymbolSource;

constexpr const char* kDeprecationNote =
    "// !!! Do not remove this comment !!!\n// This file has been migrated in Orbit 1.68. Please "
    "use: Menu > Settings > Symbol Locations...\n// This file can still used by Orbit versions "
    "prior to 1.68. If that is relevant to you, do not delete this file.\n";

namespace orbit_symbols {

std::vector<fs::path> ReadSymbolsFile(const fs::path& file_name) {
  std::error_code error;
  bool file_exists = fs::exists(file_name, error);
  if (error) {
    ORBIT_ERROR("Unable to stat \"%s\":%s", file_name.string(), error.message());
    return {};
  }

  if (!file_exists) {
    ErrorMessageOr<void> result = orbit_base::WriteStringToFile(
        file_name,
        "//-------------------\n"
        "// Orbit Symbol Locations\n"
        "//-------------------\n"
        "// Orbit will scan the specified directories for symbol files.\n"
        "// Enter one directory per line, like so:\n"
#ifdef _WIN32
        "// C:\\MyApp\\Release\\\n"
        "// D:\\MySymbolServer\\\n"
#else
        "// /home/git/project/build/\n"
        "// /home/symbol_server/\n"
#endif
    );

    if (result.has_error()) {
      ORBIT_ERROR("Unable to create symbols file: %s", result.error().message());
    }
    // Since file is empty - return empty list
    return {};
  }

  std::vector<fs::path> directories;
  ErrorMessageOr<std::string> file_content_or_error = orbit_base::ReadFileToString(file_name);
  if (file_content_or_error.has_error()) {
    ORBIT_ERROR("%s", file_content_or_error.error().message());
    return {};
  }

  std::vector<std::string> lines =
      absl::StrSplit(file_content_or_error.value(), absl::ByAnyChar("\r\n"));
  for (std::string_view line : lines) {
    line = absl::StripAsciiWhitespace(line);

    if (absl::StartsWith(line, "//") || line.empty()) continue;

    if (absl::StartsWith(line, "\"") && absl::EndsWith(line, "\"")) {
      line = line.substr(1, line.size() - 2);
    }

    const fs::path dir = line;
    bool is_directory = fs::is_directory(dir, error);
    if (error) {
      ORBIT_ERROR("Unable to stat \"%s\": %s (skipping)", dir.string(), error.message());
      continue;
    }

    if (!is_directory) {
      ORBIT_ERROR("\"%s\" is not a directory (skipping)", dir.string());
      continue;
    }

    directories.push_back(dir);
  }
  return directories;
}

[[nodiscard]] static std::vector<StructuredDebugDirectorySymbolProvider>
FindStructuredDebugDirectorySymbolProviders() {
  std::vector<StructuredDebugDirectorySymbolProvider> providers;

  const char* const ggp_sdk_path = std::getenv("GGP_SDK_PATH");
  if (ggp_sdk_path != nullptr) {
    auto path = std::filesystem::path{ggp_sdk_path} / "sysroot" / "usr" / "lib" / "debug";
    std::error_code error{};
    if (std::filesystem::is_directory(path, error)) {
      providers.emplace_back(path, SymbolSource::kLocalStadiaSdk);
    }
  }

  {  // Other way of finding the Stadia SDK, via parent path
    auto path = orbit_base::GetExecutableDir().parent_path().parent_path() / "sysroot" / "usr" /
                "lib" / "debug";
    std::error_code error{};
    if (std::filesystem::is_directory(path, error)) {
      providers.emplace_back(path, SymbolSource::kLocalStadiaSdk);
    }
  }

#ifndef _WIN32
  {
    std::filesystem::path path{"/usr/lib/debug"};
    std::error_code error{};
    if (std::filesystem::is_directory(path, error)) {
      providers.emplace_back(path, SymbolSource::kUsrLibDebugDirectory);
    }
  }
#endif

  return providers;
}

// TODO(b/246743231): Remove this function when not needed anymore.
[[nodiscard]] static std::vector<StructuredDebugDirectorySymbolProvider>
CreateStructuredDebugDirectorySymbolProviders(std::vector<std::filesystem::path> paths) {
  std::vector<StructuredDebugDirectorySymbolProvider> result;
  result.reserve(paths.size());
  for (const auto& path : paths) {
    result.emplace_back(path, SymbolSource::kLocalStadiaSdk);
  }
  return result;
}

ErrorMessageOr<void> SymbolHelper::VerifySymbolsFile(const fs::path& symbols_path,
                                                     const std::string& build_id) {
  ORBIT_SCOPE_FUNCTION;
  auto symbols_file_or_error = CreateSymbolsFile(symbols_path, ObjectFileInfo());
  if (symbols_file_or_error.has_error()) {
    return ErrorMessage(absl::StrFormat("Unable to load symbols file \"%s\": %s",
                                        symbols_path.string(),
                                        symbols_file_or_error.error().message()));
  }

  const std::unique_ptr<SymbolsFile>& symbols_file{symbols_file_or_error.value()};

  if (symbols_file->GetBuildId().empty()) {
    return ErrorMessage(
        absl::StrFormat("Symbols file \"%s\" does not have a build id.", symbols_path.string()));
  }

  if (symbols_file->GetBuildId() != build_id) {
    return ErrorMessage(
        absl::StrFormat(R"(Symbols file "%s" has a different build id: "%s" != "%s")",
                        symbols_path.string(), build_id, symbols_file->GetBuildId()));
  }

  return outcome::success();
}

SymbolHelper::SymbolHelper(fs::path cache_directory)
    : cache_directory_(std::move(cache_directory)),
      structured_debug_directory_providers_(FindStructuredDebugDirectorySymbolProviders()) {}

SymbolHelper::SymbolHelper(std::filesystem::path cache_directory,
                           std::vector<std::filesystem::path> structured_debug_directories)
    : cache_directory_(std::move(cache_directory)),
      structured_debug_directory_providers_(
          CreateStructuredDebugDirectorySymbolProviders(structured_debug_directories)) {}

ErrorMessageOr<fs::path> SymbolHelper::FindSymbolsFileLocally(
    const fs::path& module_path, const std::string& build_id,
    const ModuleInfo::ObjectFileType& object_file_type, absl::Span<const fs::path> paths) const {
  ORBIT_SCOPE_FUNCTION;
  if (build_id.empty()) {
    return ErrorMessage(absl::StrFormat(
        "Could not find symbols file for module \"%s\", because it does not contain a build id.",
        module_path.string()));
  }

  // structured debug directories is only supported for elf files
  if (object_file_type == ModuleInfo::kElfFile) {
    for (const auto& provider : structured_debug_directory_providers_) {
      const ModuleIdentifier module_id{module_path.string(), build_id};
      const orbit_base::StopSource stop_source;
      orbit_base::Future<SymbolLoadingOutcome> future =
          provider.RetrieveSymbols(module_id, stop_source.GetStopToken());

      // TODO(antonrohr): This `.Get()` makes this asynchronous future operation a syncronous
      // operation. This is okay for now.
      const SymbolLoadingOutcome& outcome = future.Get();
      if (orbit_symbol_provider::IsSuccessResult(outcome)) {
        return orbit_symbol_provider::GetSuccessResult(outcome).path;
      }
      if (outcome.has_error()) {
        ORBIT_ERROR("Error while searching in structured debug directories: %s",
                    outcome.error().message());
      }
    }
  }

  // Search in all directories for all the allowed symbol filenames
  std::set<fs::path> search_paths;
  for (const auto& path : paths) {
    if (!fs::is_directory(path)) {
      search_paths.insert(path);
      continue;
    }
    for (const auto& filename :
         orbit_symbols::GetStandardSymbolFilenamesForModule(module_path, object_file_type)) {
      search_paths.insert(path / filename);
    }
  }

  ORBIT_LOG("Trying to find symbols for module: \"%s\"", module_path.string());
  for (const auto& symbols_path : search_paths) {
    std::error_code error;
    bool exists = fs::exists(symbols_path, error);
    if (error) {
      ORBIT_ERROR("Unable to stat \"%s\": %s", symbols_path.string(), error.message());
      continue;
    }

    if (!exists) continue;

    const auto verification_result = VerifySymbolsFile(symbols_path, build_id);
    if (verification_result.has_error()) {
      ORBIT_LOG("Existing file \"%s\" is not the symbols file for module \"%s\": %s",
                symbols_path.string(), module_path.string(), verification_result.error().message());
      continue;
    }

    ORBIT_LOG("Found debug info for module \"%s\" -> \"%s\"", module_path.string(),
              symbols_path.string());
    return symbols_path;
  }

  return ErrorMessage(absl::StrFormat(
      "Could not find a file with debug symbols on the local machine for module \"%s\"",
      module_path.string()));
}

ErrorMessageOr<fs::path> SymbolHelper::FindSymbolsInCache(const fs::path& module_path,
                                                          const std::string& build_id) const {
  return FindSymbolsInCacheImpl(module_path,
                                [&build_id](const std::filesystem::path& cache_file_path) {
                                  return VerifySymbolsFile(cache_file_path, build_id);
                                });
}

ErrorMessageOr<void> SymbolHelper::VerifySymbolsFile(const std::filesystem::path& symbols_path,
                                                     uint64_t expected_file_size) {
  OUTCOME_TRY(const uint64_t actual_file_size, orbit_base::FileSize(symbols_path));
  if (actual_file_size != expected_file_size) {
    return ErrorMessage(absl::StrFormat("Symbol file size doesn't match. Expected: %u, Actual: %u ",
                                        expected_file_size, actual_file_size));
  }
  return outcome::success();
}

ErrorMessageOr<fs::path> SymbolHelper::FindSymbolsInCache(const fs::path& module_path,
                                                          uint64_t expected_file_size) const {
  return FindSymbolsInCacheImpl(
      module_path, [&expected_file_size](const std::filesystem::path& cache_file_path) {
        return VerifySymbolsFile(cache_file_path, expected_file_size);
      });
}

ErrorMessageOr<ModuleSymbols> SymbolHelper::LoadSymbolsFromFile(
    const fs::path& file_path, const ObjectFileInfo& object_file_info) {
  ORBIT_SCOPE_FUNCTION;
  ORBIT_SCOPED_TIMED_LOG("LoadSymbolsFromFile: %s", file_path.string());

  OUTCOME_TRY(auto symbols_file, CreateSymbolsFile(file_path, object_file_info));
  return symbols_file->LoadDebugSymbols();
}

fs::path SymbolHelper::GenerateCachedFilePath(const fs::path& file_path) const {
  auto file_name = absl::StrReplaceAll(file_path.string(), {{"/", "_"}});
  return cache_directory_ / file_name;
}

[[nodiscard]] bool SymbolHelper::IsMatchingDebugInfoFile(
    const std::filesystem::path& debuginfo_file_path, uint32_t checksum) {
  std::error_code error;
  bool exists = fs::exists(debuginfo_file_path, error);
  if (error) {
    ORBIT_ERROR("Unable to stat \"%s\": %s", debuginfo_file_path.string(), error.message());
    return false;
  }

  if (!exists) return false;

  const auto checksum_or_error = ElfFile::CalculateDebuglinkChecksum(debuginfo_file_path);
  if (checksum_or_error.has_error()) {
    ORBIT_LOG("Unable to calculate checksum of \"%s\": \"%s\"",
              debuginfo_file_path.filename().string(), checksum_or_error.error().message());
    return false;
  }

  if (checksum_or_error.value() != checksum) {
    ORBIT_LOG(
        "Found file with matching name \"%s\", but the checksums do not match. Expected: %#x. "
        "Actual: %#x",
        debuginfo_file_path.string(), checksum, checksum_or_error.value());
    return false;
  }

  ORBIT_LOG("Found debug info in file \"%s\"", debuginfo_file_path.string());
  return true;
}

ErrorMessageOr<fs::path> SymbolHelper::FindDebugInfoFileLocally(
    std::string_view filename, uint32_t checksum, absl::Span<const fs::path> directories) const {
  std::set<fs::path> search_paths;
  for (const auto& directory : directories) {
    search_paths.insert(directory / filename);
  }

  ORBIT_LOG("Trying to find debuginfo file with filename \"%s\"", filename);
  for (const auto& debuginfo_file_path : search_paths) {
    if (IsMatchingDebugInfoFile(debuginfo_file_path, checksum)) return debuginfo_file_path;
  }

  return ErrorMessage{
      absl::StrFormat("Could not find a file with debug info with filename \"%s\" and checksum %#x",
                      filename, checksum)};
}

template <typename Verifier>
ErrorMessageOr<fs::path> SymbolHelper::FindSymbolsInCacheImpl(const fs::path& module_path,
                                                              Verifier&& verify) const {
  ORBIT_SCOPE_FUNCTION;
  fs::path cache_file_path = GenerateCachedFilePath(module_path);
  OUTCOME_TRY(const bool exists, orbit_base::FileExists(cache_file_path));
  if (!exists) {
    return ErrorMessage(
        absl::StrFormat("Unable to find symbols in cache for module \"%s\"", module_path.string()));
  }
  OUTCOME_TRY(verify(cache_file_path));
  return cache_file_path;
}

ErrorMessageOr<bool> FileStartsWithDeprecationNote(const std::filesystem::path& file_name) {
  OUTCOME_TRY(auto&& file_content, orbit_base::ReadFileToString(file_name));

  return absl::StartsWith(file_content, kDeprecationNote);
}

ErrorMessageOr<void> AddDeprecationNoteToFile(const std::filesystem::path& file_name) {
  OUTCOME_TRY(auto&& already_contains_note, FileStartsWithDeprecationNote(file_name));

  if (already_contains_note) return ErrorMessage("File already contains a deprecation note.");

  OUTCOME_TRY(auto&& file_content, orbit_base::ReadFileToString(file_name));

  OUTCOME_TRY(
      orbit_base::WriteStringToFile(file_name, absl::StrCat(kDeprecationNote, file_content)));

  return outcome::success();
}

}  // namespace orbit_symbols