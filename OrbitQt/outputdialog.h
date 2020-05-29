// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OUTPUTDIALOG_H
#define OUTPUTDIALOG_H

#include <QDialog>

namespace Ui {
class OutputDialog;
}

class OutputDialog : public QDialog {
  Q_OBJECT

 public:
  explicit OutputDialog(QWidget* parent = nullptr);
  ~OutputDialog() override;

  void Reset();
  void SetStatus(const std::string& a_Status);
  void AddLog(const std::string& a_Log);

 private:
  Ui::OutputDialog* ui;
};

#endif  // OUTPUTDIALOG_H
