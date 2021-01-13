// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "OrbitBase/SafeStrerror.h"

#include <cstring>

const char* SafeStrerror(int errnum) {
  constexpr size_t BUFLEN = 256;
  thread_local char buf[BUFLEN];
#ifdef _MSC_VER
  strerror_s(buf, BUFLEN, errnum);
  return buf;
#else
  return strerror_r(errnum, buf, BUFLEN);
#endif
}
