//-----------------------------------
// Copyright Pierric Gimmig 2013-2017
//-----------------------------------

//-----------------------------------
// Author: Florian Kuebler
//-----------------------------------
#pragma once

#include "Callstack.h"
#include "SerializationMacros.h"

#include <string>

//-----------------------------------------------------------------------------
class LinuxPerfData
{
public:
    std::string m_header = "";
    uint32_t m_tid = 0; // TODO: This is actually redundant
    uint64_t m_time = 0;
    uint64_t m_numCallstacks = 0;

    CallStack m_CS;

    void Clear();

    ORBIT_SERIALIZABLE;
};

//-----------------------------------------------------------------------------
struct HashedLinuxPerfData
{
public:
    uint64_t m_time = 0;

    HashedCallStack m_CS;

    ORBIT_SERIALIZABLE;
};