// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include "PrintVar.h"
#include "Threading.h"

//#define TRACE_VAR( var ) TraceVar( __FUNCTION__ ##"."#var, var );
#define TRACE_VAR(var) TraceVar(#var, var)

class VariableTracing {
 public:
  static VariableTracing& Get();

  typedef std::function<void(std::vector<std::string>&)> TraceCallback;
  static void AddCallback(TraceCallback a_Callback);

  static void Trace(const char* a_Msg);
  static void ProcessCallbacks();

 protected:
  VariableTracing() {}

 protected:
  Mutex m_Mutex;
  std::vector<std::string> m_Entries;
  std::vector<TraceCallback> m_Callbacks;
};

template <class T>
inline void TraceVar(const char* a_VarName, const T& a_Value) {
  std::stringstream l_StringStream;
  l_StringStream << a_VarName << " = " << a_Value;
  VariableTracing::Trace(l_StringStream.str().c_str());
}

inline void TraceVar(const char* a_VarName, float a_Value) {
  std::stringstream l_StringStream;
  static int precision = 20;
  l_StringStream << a_VarName << " = " << std::setprecision(precision)
                 << a_Value;
  VariableTracing::Trace(l_StringStream.str().c_str());
}
