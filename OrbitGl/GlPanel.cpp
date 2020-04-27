//-----------------------------------
// Copyright Pierric Gimmig 2013-2017
//-----------------------------------

#include "GlPanel.h"

#include "BlackBoard.h"
#include "CaptureWindow.h"
#include "GlCanvas.h"
#include "HomeWindow.h"
#include "ImmediateWindow.h"
#include "PluginCanvas.h"
#include "RuleEditor.h"

//-----------------------------------------------------------------------------
GlPanel* GlPanel::Create(Type a_Type, void* a_UserData) {
  GlPanel* panel = nullptr;

  switch (a_Type) {
    case CAPTURE:
      panel = new CaptureWindow();
      break;
    case IMMEDIATE:
      panel = new ImmediateWindow();
      break;
    case VISUALIZE:
      panel = new BlackBoard();
      break;
    case RULE_EDITOR:
      panel = new RuleEditor();
      break;
    case DEBUG:
      panel = new HomeWindow();
      break;
    case PLUGIN:
      panel = new PluginCanvas(static_cast<Orbit::Plugin*>(a_UserData));

      break;
  }

  panel->m_Type = a_Type;

  // Todo: fix leak...
  return panel;
}

//-----------------------------------------------------------------------------
GlPanel::GlPanel() {
  m_WindowOffset[0] = 0;
  m_WindowOffset[1] = 0;
  m_MainWindowWidth = 0;
  m_MainWindowHeight = 0;
  m_NeedsRedraw = true;
}

//-----------------------------------------------------------------------------
GlPanel::~GlPanel() {}

//-----------------------------------------------------------------------------
void GlPanel::Initialize() {}

//-----------------------------------------------------------------------------
void GlPanel::Resize(int /*a_Width*/, int /*a_Height*/) {}

//-----------------------------------------------------------------------------
void GlPanel::Render(int /*a_Width*/, int /*a_Height*/) {}
