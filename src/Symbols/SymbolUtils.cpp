// Copyright (c) 2022 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "Symbols/SymbolUtils.h"

#include "ObjectUtils/SymbolsFile.h"
#include "OrbitBase/File.h"
#include "OrbitBase/Logging.h"

namespace orbit_symbols {

[[nodiscard]] std::vector<std::filesystem::path> GetStandardSymbolFilenamesForModule(
    const std::filesystem::path& module_path,
    const orbit_grpc_protos::ModuleInfo::ObjectFileType& object_file_type) {
  std::string sym_ext;
  switch (object_file_type) {
    case orbit_grpc_protos::ModuleInfo::kElfFile:
      sym_ext = ".debug";
      break;
    case orbit_grpc_protos::ModuleInfo::kCoffFile:
      sym_ext = ".pdb";
      break;
    case orbit_grpc_protos::ModuleInfo::kUnknown:
      ORBIT_ERROR("Unknown object file type");
      return {module_path.filename()};
    case orbit_grpc_protos::
        ModuleInfo_ObjectFileType_ModuleInfo_ObjectFileType_INT_MIN_SENTINEL_DO_NOT_USE_:
      [[fallthrough]];
    case orbit_grpc_protos::
        ModuleInfo_ObjectFileType_ModuleInfo_ObjectFileType_INT_MAX_SENTINEL_DO_NOT_USE_:
      ORBIT_UNREACHABLE();
      break;
  }

  const std::filesystem::path& filename = module_path.filename();
  std::filesystem::path filename_dot_sym_ext = filename;
  filename_dot_sym_ext.replace_extension(sym_ext);
  std::filesystem::path filename_plus_sym_ext = filename;
  filename_plus_sym_ext.replace_extension(filename.extension().string() + sym_ext);

  return {filename_dot_sym_ext, filename_plus_sym_ext, filename};
}

ErrorMessageOr<void> VerifySymbolFile(const std::filesystem::path& symbol_file_path,
                                      const std::string& module_build_id) {
  auto symbols_file_or_error =
      CreateSymbolsFile(symbol_file_path, orbit_object_utils::ObjectFileInfo());
  if (symbols_file_or_error.has_error()) {
    return ErrorMessage(absl::StrFormat("Unable to load symbols file \"%s\": %s",
                                        symbol_file_path.string(),
                                        symbols_file_or_error.error().message()));
  }

  const std::unique_ptr<orbit_object_utils::SymbolsFile>& symbols_file{
      symbols_file_or_error.value()};

  if (symbols_file->GetBuildId().empty()) {
    return ErrorMessage(absl::StrFormat("Symbols file \"%s\" does not have a build id.",
                                        symbol_file_path.string()));
  }

  if (symbols_file->GetBuildId() != module_build_id) {
    return ErrorMessage(
        absl::StrFormat(R"(Symbols file "%s" has a different build id: "%s" != "%s")",
                        symbol_file_path.string(), module_build_id, symbols_file->GetBuildId()));
  }

  return outcome::success();
}

ErrorMessageOr<void> VerifySymbolFile(const std::filesystem::path& symbol_file_path,
                                      uint64_t expected_file_size) {
  OUTCOME_TRY(const uint64_t actual_file_size, orbit_base::FileSize(symbol_file_path));
  if (actual_file_size != expected_file_size) {
    return ErrorMessage(absl::StrFormat("Symbol file size doesn't match. Expected: %u, Actual: %u ",
                                        expected_file_size, actual_file_size));
  }
  return outcome::success();
}

}  // namespace orbit_symbols