// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_QT_ORBIT_DISASSEMBLY_DIALOG_H_
#define ORBIT_QT_ORBIT_DISASSEMBLY_DIALOG_H_

#include <QDialog>
#include <QObject>
#include <QString>
#include <QWidget>
#include <functional>
#include <string>

#include "DisassemblyReport.h"

namespace Ui {
class OrbitDisassemblyDialog;
}

class OrbitDisassemblyDialog : public QDialog {
  Q_OBJECT

 public:
  explicit OrbitDisassemblyDialog(QWidget* parent = nullptr);
  ~OrbitDisassemblyDialog() override;

  void SetText(std::string a_Text);
  void SetDisassemblyReport(DisassemblyReport report);

 private:
  Ui::OrbitDisassemblyDialog* ui;
};

#endif  // ORBIT_QT_ORBIT_DISASSEMBLY_DIALOG_H_
