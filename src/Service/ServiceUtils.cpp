// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ServiceUtils.h"

#include <absl/base/casts.h>
#include <absl/base/macros.h>
#include <absl/strings/match.h>
#include <absl/strings/numbers.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_join.h>
#include <absl/strings/str_split.h>
#include <sys/uio.h>

#include <algorithm>
#include <filesystem>
#include <iosfwd>
#include <map>
#include <memory>
#include <numeric>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "ObjectUtils/ObjectFile.h"
#include "ObjectUtils/SymbolsFile.h"
#include "OrbitBase/Logging.h"
#include "OrbitBase/ReadFileToString.h"
#include "OrbitBase/Result.h"
#include "OrbitBase/ThreadUtils.h"
#include "absl/strings/str_format.h"
#include "module.pb.h"

namespace orbit_service::utils {

namespace fs = std::filesystem;

using orbit_grpc_protos::TracepointInfo;
using ::orbit_object_utils::CreateObjectFile;
using ::orbit_object_utils::CreateSymbolsFile;
using ::orbit_object_utils::ObjectFile;
using ::orbit_object_utils::SymbolsFile;

static const char* kLinuxTracingEventsDirectory = "/sys/kernel/debug/tracing/events/";

const std::vector<fs::path>& kHardcodedSearchDirectories{
    "/home/cloudcast/",        "/home/cloudcast/debug_symbols/",
    "/mnt/developer/",         "/mnt/developer/debug_symbols/",
    "/srv/game/assets/",       "/srv/game/assets/debug_symbols/",
    "/home/cloudcast/symbols", "/mnt/developer/symbols",
    "/srv/game/assets/symbols"};

ErrorMessageOr<std::vector<orbit_grpc_protos::TracepointInfo>> ReadTracepoints() {
  std::vector<TracepointInfo> result;

  std::error_code error;
  auto category_directory_iterator = fs::directory_iterator(kLinuxTracingEventsDirectory, error);
  if (error) {
    return ErrorMessage{absl::StrFormat("Unable to scan \"%s\" directory: %s",
                                        kLinuxTracingEventsDirectory, error.message())};
  }

  for (auto category_it = fs::begin(category_directory_iterator),
            category_end = fs::end(category_directory_iterator);
       category_it != category_end; category_it.increment(error)) {
    if (error) {
      return ErrorMessage{absl::StrFormat("Unable to scan \"%s\" directory: %s",
                                          kLinuxTracingEventsDirectory, error.message())};
    }

    const fs::path& category_path = category_it->path();
    bool is_directory = fs::is_directory(category_path, error);
    if (error) {
      return ErrorMessage{
          absl::StrFormat("Unable to stat \"%s\": %s", category_path.string(), error.message())};
    }

    if (!is_directory) continue;

    auto name_directory_iterator = fs::directory_iterator(category_path, error);
    if (error) {
      return ErrorMessage{absl::StrFormat("Unable to scan \"%s\" directory: %s",
                                          category_path.string(), error.message())};
    }

    for (auto it = fs::begin(name_directory_iterator), end = fs::end(name_directory_iterator);
         it != end; it.increment(error)) {
      if (error) {
        return ErrorMessage{absl::StrFormat("Unable to scan \"%s\" directory: %s",
                                            category_path.string(), error.message())};
      }

      bool is_directory = it->is_directory(error);
      if (error) {
        return ErrorMessage{
            absl::StrFormat("Unable to stat \"%s\": %s", it->path().string(), error.message())};
      }

      if (!is_directory) {
        continue;
      }

      TracepointInfo tracepoint_info;
      tracepoint_info.set_name(it->path().filename());
      tracepoint_info.set_category(category_path.filename());
      result.emplace_back(tracepoint_info);
    }
  }

  return result;
}

std::optional<Jiffies> GetCumulativeCpuTimeFromProcess(pid_t pid) {
  const auto stat = std::filesystem::path{"/proc"} / std::to_string(pid) / "stat";

  // /proc/[pid]/stat looks like so (example - all in one line):
  // 1395261 (sleep) S 5273 1160 1160 0 -1 1077936128 101 0 0 0 0 0 0 0 20 0 1 0 42187401 5431296
  // 131 18446744073709551615 94702955896832 94702955911385 140735167078224 0 0 0 0 0 0 0 0 0 17 10
  // 0 0 0 0 0 94702955928880 94702955930112 94702967197696 140735167083224 140735167083235
  // 140735167083235 140735167086569 0
  //
  // This code reads field 13 (user time) and 14 (kernel time) to determine the process's cpu usage.
  // Older kernels might have less fields than in the example. Over time fields had been added to
  // the end, but field indexes stayed stable.

  std::error_code error;
  bool file_exists = std::filesystem::exists(stat, error);
  // Even if we couldn't stat we could still be able to read, continue in case of an error.
  if (!error && !file_exists) {
    return std::nullopt;
  }

  ErrorMessageOr<std::string> file_content_or_error = orbit_base::ReadFileToString(stat);
  if (file_content_or_error.has_error()) {
    ERROR("Could not read \"%s\": %s", stat.string(), file_content_or_error.error().message());
    return std::nullopt;
  }

  std::vector<std::string> lines =
      absl::StrSplit(file_content_or_error.value(), absl::MaxSplits('\n', 1));
  if (lines.empty()) {
    ERROR("\"%s\" file is empty", stat.string());
    return std::nullopt;
  }

  const std::string& first_line = lines.at(0);

  // Remove fields up to comm (process name) as this, enclosed in parentheses, could contain spaces.
  size_t last_closed_paren_index = first_line.find_last_of(')');
  if (last_closed_paren_index == std::string::npos) {
    return std::nullopt;
  }
  std::string_view first_line_excl_pid_comm =
      std::string_view{first_line}.substr(last_closed_paren_index + 1);

  std::vector<std::string_view> fields_excl_pid_comm =
      absl::StrSplit(first_line_excl_pid_comm, ' ', absl::SkipWhitespace{});

  constexpr size_t kCommIndex = 1;
  constexpr size_t kUtimeIndex = 13;
  constexpr size_t kUtimeIndexExclPidComm = kUtimeIndex - kCommIndex - 1;
  constexpr size_t kStimeIndex = 14;
  constexpr size_t kStimeIndexExclPidComm = kStimeIndex - kCommIndex - 1;

  if (fields_excl_pid_comm.size() <= std::max(kUtimeIndex, kStimeIndex)) {
    return std::nullopt;
  }

  size_t utime{};
  if (!absl::SimpleAtoi(fields_excl_pid_comm[kUtimeIndexExclPidComm], &utime)) {
    return std::nullopt;
  }

  size_t stime{};
  if (!absl::SimpleAtoi(fields_excl_pid_comm[kStimeIndexExclPidComm], &stime)) {
    return std::nullopt;
  }

  return Jiffies{utime + stime};
}

std::optional<TotalCpuTime> GetCumulativeTotalCpuTime() {
  ErrorMessageOr<std::string> stat_content_or_error = orbit_base::ReadFileToString("/proc/stat");
  if (stat_content_or_error.has_error()) {
    ERROR("%s", stat_content_or_error.error().message());
    return std::nullopt;
  }

  std::vector<std::string> lines = absl::StrSplit(stat_content_or_error.value(), '\n');
  if (lines.empty()) {
    return std::nullopt;
  }

  // /proc/stat looks like so (example):
  // cpu  2939645 2177780 3213131 495750308 128031 0 469660 0 0 0
  // cpu0 238392 136574 241698 41376123 10562 0 285529 0 0 0
  // cpu1 250552 255075 339032 41161047 10580 0 74924 0 0 0
  // cpu2 259751 189964 284201 41275484 10515 0 25803 0 0 0
  // cpu3 262709 274244 360158 41021080 11391 0 49734 0 0 0
  // cpu4 259346 334285 391229 41021734 10923 0 6862 0 0 0
  // cpu5 257605 236852 317990 41186809 11006 0 4687 0 0 0
  // cpu6 244450 197610 258522 41315244 10772 0 3679 0 0 0
  // cpu7 239533 118254 209752 41453567 10417 0 3216 0 0 0
  // cpu8 228352 104140 203956 41495612 9605 0 2898 0 0 0
  // cpu9 231930 91346 199315 41507207 10363 0 2620 0 0 0
  // cpu10 231707 130839 201517 41467968 10920 0 2616 0 0 0
  // cpu11 235314 108593 205757 41468427 10972 0 7087 0 0 0
  // intr 1137887578 7 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ...
  // ctxt 2193055270
  // btime 1599751494
  // processes 1402492
  // procs_running 3
  // procs_blocked 0
  // softirq 786377709 150 321427815 783165 48655285 46 0 1068082 323211116 5742 91226308
  //
  // This code reads the first line to determine the overall amount of Jiffies that have been
  // counted. It also reads the lines beginning with "cpu*" to determine the number of logical CPUs
  // in the system.

  const std::string& first_line = lines.at(0);

  if (!absl::StartsWith(first_line, "cpu ")) {
    return std::nullopt;
  }

  // This is counting the number of CPUs
  size_t cpus = 0;
  // Skip the first line
  for (size_t i = 1; i < lines.size(); ++i) {
    const std::string& current_line = lines.at(i);
    if (!absl::StartsWith(current_line, "cpu")) {
      break;
    }
    cpus++;
  }

  if (cpus == 0) {
    return std::nullopt;
  }

  std::vector<std::string_view> splits = absl::StrSplit(first_line, ' ', absl::SkipWhitespace{});

  const Jiffies jiffies{
      std::accumulate(splits.begin() + 1, splits.end(), 0ul, [](auto sum, const auto& str) {
        int potential_time = 0;
        if (absl::SimpleAtoi(str, &potential_time)) {
          sum += potential_time;
        }

        return sum;
      })};

  return TotalCpuTime{jiffies, cpus};
}

static ErrorMessageOr<fs::path> FindSymbolsFilePathInStructuredDebugStore(
    const std::filesystem::path& structured_debug_store, std::string_view build_id) {
  CHECK(build_id.size() >= 3);

  auto path_in_structured_debug_store = structured_debug_store / ".build-id" /
                                        build_id.substr(0, 2) /
                                        absl::StrFormat("%s.debug", build_id.substr(2));

  std::error_code error{};
  if (fs::is_regular_file(path_in_structured_debug_store, error)) {
    return path_in_structured_debug_store;
  }

  if (error.value() != 0) {
    return ErrorMessage{absl::StrFormat("Error while checking file \"%s\": %s",
                                        path_in_structured_debug_store, error.message())};
  }

  return ErrorMessage{
      absl::StrFormat("File does not exist: %s", path_in_structured_debug_store.string())};
}

ErrorMessageOr<fs::path> FindSymbolsFilePath(
    const orbit_grpc_protos::GetDebugInfoFileRequest& request) {
  const fs::path module_path{request.module_path()};

  // 1. Create object file for the module and check if it contains symbols itself.
  auto object_file_or_error = CreateObjectFile(module_path);
  if (object_file_or_error.has_error()) {
    return ErrorMessage(absl::StrFormat("Unable to create object file: %s",
                                        object_file_or_error.error().message()));
  }
  if (object_file_or_error.value()->HasDebugSymbols()) {
    return module_path;
  }
  // 2. If module does not contain a build id, no searching for separate symbol files can be done
  if (object_file_or_error.value()->GetBuildId().empty()) {
    return ErrorMessage{
        absl::StrFormat("Module \"%s\" does not contain symbols and does not contain a build id, "
                        "therefore Orbit cannot search for a separate symbols file",
                        module_path.string())};
  }
  const std::string& build_id = object_file_or_error.value()->GetBuildId();
  if (build_id.size() < 3) {
    return ErrorMessage{absl::StrFormat("The build id of \"%s\" is malformed, build id: %s",
                                        module_path.string(), build_id)};
  }
  // 3. Search in structured symbols stores.
  const fs::path structured_debug_store{"/usr/lib/debug"};
  ErrorMessageOr<fs::path> debug_store_result =
      FindSymbolsFilePathInStructuredDebugStore(structured_debug_store, build_id);
  if (debug_store_result.has_value()) return debug_store_result.value();

  // 4. Search in hardcoded directories + additional directoried (user provided) + next to the
  // module
  std::vector<fs::path> search_directories{kHardcodedSearchDirectories};
  const auto& additional_search_directories = request.additional_search_directories();
  search_directories.insert(search_directories.end(), additional_search_directories.begin(),
                            additional_search_directories.end());
  search_directories.push_back(object_file_or_error.value()->GetFilePath().parent_path());

  const fs::path& filename = module_path.filename();
  fs::path filename_dot_debug = filename;
  filename_dot_debug.replace_extension(".debug");
  fs::path filename_plus_debug = filename;
  filename_plus_debug.replace_extension(filename.extension().string() + ".debug");
  fs::path filename_dot_pdb = filename;
  filename_dot_pdb.replace_extension(".pdb");
  fs::path filename_plus_pdb = filename;
  filename_plus_debug.replace_extension(filename.extension().string() + ".pdb");

  std::set<fs::path> search_paths;
  for (const auto& directory : search_directories) {
    search_paths.insert(directory / filename_dot_debug);
    search_paths.insert(directory / filename_plus_debug);
    search_paths.insert(directory / filename);
    search_paths.insert(directory / filename_dot_pdb);
    search_paths.insert(directory / filename_plus_pdb);
  }

  std::vector<std::string> error_messages;

  for (const auto& search_path : search_paths) {
    std::error_code error;
    bool path_exists = fs::exists(search_path, error);
    if (error) {
      std::string error_message =
          absl::StrFormat("Unable to stat \"%s\": %s", search_path, error.message());
      ERROR("%s", error_message);
      error_messages.emplace_back(std::move(error_message));
      continue;
    }

    if (!path_exists) {
      // no error message when file simply does not exist
      continue;
    }

    auto symbols_file_or_error = CreateSymbolsFile(search_path);
    if (symbols_file_or_error.has_error()) {
      error_messages.push_back(symbols_file_or_error.error().message());
      continue;
    }

    const std::unique_ptr<SymbolsFile>& symbols_file{symbols_file_or_error.value()};

    if (symbols_file->GetBuildId().empty()) {
      error_messages.push_back(
          absl::StrFormat("Potential symbols file \"%s\" does not have a build id.", search_path));
      continue;
    }

    if (symbols_file->GetBuildId() != build_id) {
      error_messages.push_back(absl::StrFormat(
          "Potential symbols file \"%s\" has a different build id than the module requested by "
          "the client. \"%s\" != \"%s\"",
          search_path, symbols_file->GetBuildId(), build_id));
      continue;
    }

    return search_path;
  }

  std::string error_message_for_client{absl::StrFormat(
      "Unable to find debug symbols on the instance for module \"%s\". ", module_path)};
  if (!error_messages.empty()) {
    absl::StrAppend(&error_message_for_client, "\nDetails:\n* ",
                    absl::StrJoin(error_messages, "\n* "));
  }
  return ErrorMessage(error_message_for_client);
}

bool ReadProcessMemory(uint32_t pid, uintptr_t address, void* buffer, uint64_t size,
                       uint64_t* num_bytes_read) {
  iovec local_iov[] = {{buffer, size}};
  iovec remote_iov[] = {{absl::bit_cast<void*>(address), size}};
  pid_t native_pid = orbit_base::ToNativeProcessId(pid);
  *num_bytes_read = process_vm_readv(native_pid, local_iov, ABSL_ARRAYSIZE(local_iov), remote_iov,
                                     ABSL_ARRAYSIZE(remote_iov), 0);
  return *num_bytes_read == size;
}

}  // namespace orbit_service::utils
