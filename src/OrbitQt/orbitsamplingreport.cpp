// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "orbitsamplingreport.h"

#include <QColor>
#include <QGridLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QList>
#include <QModelIndex>
#include <QModelIndexList>
#include <QPushButton>
#include <QSplitter>
#include <QStaticStringData>
#include <QStringLiteral>
#include <QTabWidget>
#include <Qt>
#include <algorithm>
#include <optional>
#include <string>

#include "DataViews/SamplingReportDataView.h"
#include "OrbitBase/Logging.h"
#include "SamplingReport.h"
#include "UtilWidgets/NoticeWidget.h"
#include "orbitdataviewpanel.h"
#include "orbittablemodel.h"
#include "orbittreeview.h"
#include "types.h"
#include "ui_orbitsamplingreport.h"

const QColor kRed{255, 0, 0, 26};

OrbitSamplingReport::OrbitSamplingReport(QWidget* parent)
    : QWidget(parent), ui_(new Ui::OrbitSamplingReport) {
  ui_->setupUi(this);
  if (!sampling_report_ || !sampling_report_->HasCallstacks()) {
    ui_->NextCallstackButton->setEnabled(false);
    ui_->PreviousCallstackButton->setEnabled(false);
  }

  QList<int> sizes;
  sizes.append(5000);
  sizes.append(5000);
  ui_->splitter->setSizes(sizes);

  connect(ui_->inspectionNoticeWidget, &orbit_util_widgets::NoticeWidget::ButtonClicked, this,
          &OrbitSamplingReport::LeaveCallstackInspectionClicked);
  ui_->inspectionNoticeWidget->InitializeAsInspection();
  connect(ui_->tabWidget, &QTabWidget::currentChanged, this,
          &OrbitSamplingReport::OnCurrentThreadTabChanged);
}

OrbitSamplingReport::~OrbitSamplingReport() { delete ui_; }

void OrbitSamplingReport::Initialize(orbit_data_views::DataView* callstack_data_view,
                                     const std::shared_ptr<SamplingReport>& report) {
  ui_->CallstackTreeView->Initialize(callstack_data_view, SelectionType::kExtended,
                                     FontType::kDefault, false);
  sampling_report_ = report;

  if (!report) return;

  sampling_report_->SetUiRefreshFunc([&]() { this->RefreshCallstackView(); });

  for (orbit_data_views::SamplingReportDataView& report_data_view : report->GetThreadDataViews()) {
    ORBIT_SCOPE("SamplingReportDataView tab creation");
    auto* tab = new QWidget();
    tab->setObjectName(QStringLiteral("samplingReportThreadTab"));
    tab->setAccessibleName(QStringLiteral("SamplingReportThreadTab"));

    auto* gridLayout_2 = new QGridLayout(tab);
    gridLayout_2->setObjectName(QStringLiteral("gridLayout_2"));
    auto* treeView = new OrbitDataViewPanel(tab);
    treeView->SetDataModel(&report_data_view);

    if (!report_data_view.IsSortingAllowed()) {
      treeView->GetTreeView()->setSortingEnabled(false);
    } else {
      int column = report_data_view.GetDefaultSortingColumn();
      Qt::SortOrder order = report_data_view.GetColumns()[column].initial_order ==
                                    orbit_data_views::DataView::SortingOrder::kAscending
                                ? Qt::AscendingOrder
                                : Qt::DescendingOrder;
      treeView->GetTreeView()->sortByColumn(column, order);
    }

    treeView->setObjectName(QStringLiteral("samplingReportDataView"));
    treeView->setAccessibleName(QStringLiteral("SamplingReportDataView"));
    gridLayout_2->addWidget(treeView, 0, 0, 1, 1);
    treeView->Initialize(&report_data_view, SelectionType::kExtended, FontType::kDefault);
    {
      ORBIT_SCOPE("resizeSections");
      treeView->GetTreeView()->header()->resizeSections(QHeaderView::ResizeToContents);
    }
    treeView->GetTreeView()->SetIsMultiSelection(true);

    treeView->Link(ui_->CallstackTreeView);

    // This is hack - it is needed to update ui when data changes
    // TODO: Remove this once model is implemented properly and there
    //  is no need for manual updates.
    orbit_data_views_.push_back(treeView);

    uint32_t thread_id = report_data_view.GetThreadID();
    // Report any thread that contains more than 5% unwinding errors.
    constexpr double kUnwindErrorNoticeThreshold = 0.05;
    if (sampling_report_->ComputeUnwindErrorRatio(thread_id) >= kUnwindErrorNoticeThreshold) {
      auto* notice_box = new orbit_util_widgets::NoticeWidget(tab);
      std::string thread_tab_name = report_data_view.GetName();
      std::replace(thread_tab_name.begin(), thread_tab_name.end(), '\n', ' ');
      const double unwind_error_ratio = sampling_report_->ComputeUnwindErrorRatio(thread_id);
      const std::string label =
          absl::StrFormat("%.2f%% of callstack samples in %s are unwinding errors.",
                          unwind_error_ratio * 100, thread_tab_name);
      notice_box->Initialize(label, "Hide", kRed);
      QObject::connect(notice_box, &orbit_util_widgets::NoticeWidget::ButtonClicked, notice_box,
                       [notice_box]() { notice_box->hide(); });
      gridLayout_2->addWidget(notice_box, 1, 0);
    }

    ui_->tabWidget->addTab(tab, QString::fromStdString(report_data_view.GetName()));
  }
}

void OrbitSamplingReport::Deinitialize() {
  for (OrbitDataViewPanel* panel : orbit_data_views_) {
    panel->Deinitialize();
  }
  ui_->CallstackTreeView->Deinitialize();
  orbit_data_views_.clear();
  ui_->tabWidget->clear();
  if (sampling_report_) {
    sampling_report_->ClearReport();
  }
  sampling_report_.reset();
}

void OrbitSamplingReport::on_NextCallstackButton_clicked() {
  ORBIT_CHECK(sampling_report_ != nullptr);
  sampling_report_->IncrementCallstackIndex();
  RefreshCallstackView();
}

void OrbitSamplingReport::on_PreviousCallstackButton_clicked() {
  ORBIT_CHECK(sampling_report_ != nullptr);
  sampling_report_->DecrementCallstackIndex();
  RefreshCallstackView();
}

void OrbitSamplingReport::OnCurrentThreadTabChanged(int current_tab_index) {
  if (current_tab_index == -1 || current_tab_index >= static_cast<int>(orbit_data_views_.size())) {
    return;
  }
  OrbitDataViewPanel* data_view = orbit_data_views_[current_tab_index];
  QModelIndexList index_list = data_view->GetTreeView()->selectionModel()->selectedIndexes();
  std::vector<int> row_list;
  for (QModelIndex& index : index_list) {
    row_list.push_back(index.row());
  }
  data_view->GetTreeView()->GetModel()->OnRowsSelected(row_list);
  RefreshCallstackView();
}

void OrbitSamplingReport::RefreshCallstackView() {
  if (sampling_report_ == nullptr) {
    return;
  }

  ui_->NextCallstackButton->setEnabled(sampling_report_->HasCallstacks());
  ui_->PreviousCallstackButton->setEnabled(sampling_report_->HasCallstacks());

  std::string label = sampling_report_->GetSelectedCallstackString();
  ui_->CallstackLabel->setText(QString::fromStdString(label));

  std::string tooltip = sampling_report_->GetSelectedCallstackTooltipString();
  ui_->CallstackLabel->setToolTip(QString::fromStdString(tooltip));
  ui_->CallstackTreeView->Refresh();
}

void OrbitSamplingReport::RefreshTabs() {
  if (sampling_report_ == nullptr) {
    return;
  }

  for (OrbitDataViewPanel* panel : orbit_data_views_) {
    panel->Refresh();
  }
}

void OrbitSamplingReport::SetInspection(orbit_data_views::DataView* callstack_data_view,
                                        std::unique_ptr<SamplingReport> report) {
  Deinitialize();
  Initialize(callstack_data_view, std::move(report));
  ui_->inspectionNoticeWidget->show();
  RefreshCallstackView();
  RefreshTabs();
}