// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "CodeViewer/Dialog.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QSyntaxHighlighter>

#include "ui_Dialog.h"

namespace orbit_code_viewer {
Dialog::Dialog(QWidget* parent) : QDialog{parent}, ui_{std::make_unique<Ui::CodeViewerDialog>()} {
  ui_->setupUi(this);

  QObject::connect(ui_->buttonBox->button(QDialogButtonBox::StandardButton::Close),
                   &QPushButton::clicked, this, &QDialog::close);
}

Dialog::~Dialog() noexcept = default;

void Dialog::SetSourceCode(const QString& code) {
  ui_->viewer->setPlainText(code);
  syntax_highlighter_.reset();
}

void Dialog::SetSourceCode(const QString& code,
                           std::unique_ptr<QSyntaxHighlighter> syntax_highlighter) {
  SetSourceCode(code);
  syntax_highlighter_ = std::move(syntax_highlighter);
  syntax_highlighter_->setDocument(ui_->viewer->document());
}

void Dialog::SetHeatmap(FontSizeInEm heatmap_bar_width, Viewer::HeatmapSource heatmap_source) {
  ui_->viewer->SetHeatmapBarWidth(heatmap_bar_width);
  ui_->viewer->SetHeatmapSource(std::move(heatmap_source));
}

void Dialog::ClearHeatmap() {
  ui_->viewer->SetHeatmapBarWidth(FontSizeInEm{0.0});
  ui_->viewer->ClearHeatmapSource();
}

void Dialog::SetLineNumberMargins(FontSizeInEm left, FontSizeInEm right) {
  ui_->viewer->SetLineNumberMargins(left, right);
}

void Dialog::SetEnableLineNumbers(bool enabled) { ui_->viewer->SetEnableLineNumbers(enabled); }

}  // namespace orbit_code_viewer
