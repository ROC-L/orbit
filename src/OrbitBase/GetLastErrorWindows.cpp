// Copyright (c) 2021 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <absl/base/casts.h>
#include <absl/strings/ascii.h>
#include <windows.h>

#include "OrbitBase/GetLastError.h"
#include "OrbitBase/Logging.h"

namespace orbit_base {

ErrorMessage GetLastError() { return ErrorMessage(GetLastErrorAsString()); }

ErrorMessage GetLastError(std::string_view prefix) {
  return ErrorMessage(absl::StrFormat("%s: %s", prefix, GetLastErrorAsString()));
}

std::string GetLastErrorAsString() {
  DWORD error = ::GetLastError();
  if (error == 0) {
    return {};
  }

  LPSTR buffer = nullptr;

  // https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-formatmessagea
  // "lpBuffer: A pointer to a buffer that receives the null-terminated string that specifies the
  // formatted message. If dwFlags includes FORMAT_MESSAGE_ALLOCATE_BUFFER, the function allocates a
  // buffer using the LocalAlloc function, and places the pointer to the buffer at the address
  // specified in lpBuffer."
  size_t size = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      /*lpSource=*/NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      /*lpBuffer=*/absl::bit_cast<LPSTR>(&buffer), /*nSize=*/0,
      /*Arguments=*/nullptr);

  if (buffer == nullptr) {
    ORBIT_ERROR("Calling FormatMessageA in GetLastErrorAsString");
    return {};
  }

  std::string error_as_string(buffer, size);
  LocalFree(buffer);
  return absl::StrFormat("%s [%u]", absl::StripAsciiWhitespace(error_as_string), error);
}

}  // namespace orbit_base
