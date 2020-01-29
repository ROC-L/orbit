//-----------------------------------
// Copyright Pierric Gimmig 2013-2017
//-----------------------------------
#pragma once

#include <memory>

#include "BaseTypes.h"
#include "SerializationMacros.h"

//-----------------------------------------------------------------------------
struct FunctionStats {
  FunctionStats() { Reset(); }
  void Reset() { memset(this, 0, sizeof(*this)); }
  void Update(const class Timer& a_Timer);

  uint64_t m_Address;
  uint64_t m_Count;
  double m_TotalTimeMs;
  double m_AverageTimeMs;
  double m_MinMs;
  double m_MaxMs;

  ORBIT_SERIALIZABLE;
};