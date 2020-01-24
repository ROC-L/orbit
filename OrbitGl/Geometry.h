//-----------------------------------
// Copyright Pierric Gimmig 2013-2017
//-----------------------------------
#pragma once
#include "BlockChain.h"
#include "CoreMath.h"

class TextBox;

//-----------------------------------------------------------------------------
struct Line {
  Vec3 m_Beg;
  Vec3 m_End;
};

//-----------------------------------------------------------------------------
struct Box {
  Vec3 m_Vertices[4];
};