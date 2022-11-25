// Copyright (c) 2021 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SESSION_SETUP_LOAD_CAPTURE_WIDGET_H_
#define SESSION_SETUP_LOAD_CAPTURE_WIDGET_H_

#include <QEvent>
#include <QObject>
#include <QRadioButton>
#include <QShowEvent>
#include <QSortFilterProxyModel>
#include <QString>
#include <QWidget>
#include <filesystem>
#include <memory>

#include "CaptureFileInfo/ItemModel.h"
#include "SessionSetup/HasRadioButtonInterface.h"

namespace Ui {
class LoadCaptureWidget;  // IWYU pragma: keep
}

namespace orbit_session_setup {

class LoadCaptureWidget : public QWidget, HasRadioButtonInterface {
  Q_OBJECT
  Q_PROPERTY(bool active READ IsActive WRITE SetActive)

 public:
  explicit LoadCaptureWidget(QWidget* parent = nullptr);
  ~LoadCaptureWidget() override;

  [[nodiscard]] bool IsActive() const;

  QRadioButton* GetRadioButton() override;

 public slots:
  void SetActive(bool value);

 signals:
  void Activated();
  void FileSelected(std::filesystem::path file_path);
  void SelectionConfirmed();

 private:
  std::unique_ptr<Ui::LoadCaptureWidget> ui_;
  orbit_capture_file_info::ItemModel item_model_;
  QSortFilterProxyModel proxy_item_model_;

  void showEvent(QShowEvent* event) override;
  void DetachRadioButton();
  void SelectViaFilePicker();
};

}  // namespace orbit_session_setup

#endif  // SESSION_SETUP_LOAD_CAPTURE_WIDGET_H_