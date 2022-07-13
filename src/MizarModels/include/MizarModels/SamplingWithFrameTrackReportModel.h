// Copyright (c) 2022 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIZAR_WIDGETS_SAMPLING_WITH_FRAME_TRACK_REPORT_MODEL_H_
#define MIZAR_WIDGETS_SAMPLING_WITH_FRAME_TRACK_REPORT_MODEL_H_

#include <absl/container/flat_hash_map.h>
#include <stdint.h>

#include <QAbstractItemModel>
#include <QVariant>
#include <Qt>
#include <algorithm>
#include <iterator>
#include <vector>

#include "MizarData/SamplingWithFrameTrackComparisonReport.h"
#include "OrbitBase/Logging.h"
#include "OrbitBase/Typedef.h"

namespace orbit_mizar_widgets {

// The class implements the model for the instance of `QTableView` owned by
// `SamplingWithFrameTrackOutputWidget`. It represents the results of comparison based on sampling
// data with frame track.
template <typename Report, typename Counts, typename FrameTrackStats>
class SamplingWithFrameTrackReportModelTmpl : public QAbstractTableModel {
  template <typename T>
  using Baseline = ::orbit_mizar_base::Baseline<T>;
  template <typename T>
  using Comparison = ::orbit_mizar_base::Comparison<T>;
  using SFID = ::orbit_mizar_base::SFID;

 public:
  enum class Column {
    kFunctionName,
    kBaselineExclusivePercent,
    kBaselineExclusiveTimePerFrame,
    kComparisonExclusivePercent,
    kComparisonExclusiveTimePerFrame,
    kPvalue,
    kIsSignificant,
    kSlowdownPercent,
    kPercentOfSlowdown
  };

  static constexpr int kColumnsCount = 9;

  explicit SamplingWithFrameTrackReportModelTmpl(Report report,
                                                 bool is_multiplicity_correction_enabled,
                                                 double significance_level,
                                                 QObject* parent = nullptr)
      : QAbstractTableModel(parent),
        report_(std::move(report)),
        is_multiplicity_correction_enabled_(is_multiplicity_correction_enabled),
        significance_level_(significance_level) {
    for (const auto& [sfid, unused_name] : report_.GetSfidToNames()) {
      if (*BaselineExclusiveCount(sfid) > 0 || *ComparisonExclusiveCount(sfid) > 0) {
        sfids_.push_back(sfid);
      }
    }
  }

  void SetMultiplicityCorrectionEnabled(bool is_enabled) {
    is_multiplicity_correction_enabled_ = is_enabled;
    EmitDataChanged(Column::kPvalue);
  }

  void SetSignificanceLevel(double significance_level) {
    significance_level_ = significance_level;
    EmitDataChanged(Column::kIsSignificant);
  }

  [[nodiscard]] int rowCount(const QModelIndex& /*parent*/ = {}) const override {
    return static_cast<int>(sfids_.size());
  };
  [[nodiscard]] int columnCount(const QModelIndex& /*parent*/) const override {
    return kColumnsCount;
  };
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override {
    if (index.model() != this || role != Qt::DisplayRole) return {};
    return QString::fromStdString(MakeDisplayedString(index));
  }

  [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                    int role = Qt::DisplayRole) const override {
    static const absl::flat_hash_map<Column, QString> kColumnNames = {
        {Column::kFunctionName, "Function"},
        {Column::kBaselineExclusivePercent, "Baseline, %"},
        {Column::kBaselineExclusiveTimePerFrame, "Baseline (per frame), us"},
        {Column::kComparisonExclusivePercent, "Comparison, %"},
        {Column::kComparisonExclusiveTimePerFrame, "Comparison (per frame), us"},
        {Column::kPvalue, "P-value"},
        {Column::kIsSignificant, "Significant?"},
        {Column::kSlowdownPercent, "Slowdown, %"},
        {Column::kPercentOfSlowdown, "\% of frametime slowdown"}};

    if (orientation == Qt::Vertical || role != Qt::DisplayRole) {
      return {};
    }

    return kColumnNames.at(static_cast<Column>(section));
  }

 private:
  void EmitDataChanged(Column column) {
    const int column_int = static_cast<int>(column);
    emit dataChanged(index(0, column_int), index(rowCount() - 1, column_int));
  }

  [[nodiscard]] std::string MakeDisplayedString(const QModelIndex& index) const {
    const SFID sfid = sfids_[index.row()];
    const auto column = static_cast<Column>(index.column());
    switch (column) {
      case Column::kFunctionName:
        return report_.GetSfidToNames().at(sfid);
      case Column::kBaselineExclusivePercent:
      case Column::kBaselineExclusiveTimePerFrame:
      case Column::kComparisonExclusivePercent:
      case Column::kComparisonExclusiveTimePerFrame:
      case Column::kPvalue:
      case Column::kSlowdownPercent:
      case Column::kPercentOfSlowdown:
        return absl::StrFormat("%.3f", MakeNumericEntry(sfid, column));
      case Column::kIsSignificant:
        return GetPvalue(sfid) < significance_level_ ? "Yes" : "No";
      default:
        ORBIT_UNREACHABLE();
    }
  }
  [[nodiscard]] Baseline<double> BaselineExclusiveRate(SFID sfid) const {
    return LiftAndApply(&Counts::GetExclusiveRate, report_.GetBaselineSamplingCounts(),
                        Baseline<SFID>(sfid));
  }
  [[nodiscard]] Comparison<double> ComparisonExclusiveRate(SFID sfid) const {
    return LiftAndApply(&Counts::GetExclusiveRate, report_.GetComparisonSamplingCounts(),
                        Comparison<SFID>(sfid));
  }

  [[nodiscard]] Baseline<uint64_t> BaselineExclusiveCount(SFID sfid) const {
    return LiftAndApply(&Counts::GetExclusiveCount, report_.GetBaselineSamplingCounts(),
                        Baseline<SFID>(sfid));
  }
  [[nodiscard]] Comparison<uint64_t> ComparisonExclusiveCount(SFID sfid) const {
    return LiftAndApply(&Counts::GetExclusiveCount, report_.GetComparisonSamplingCounts(),
                        Comparison<SFID>(sfid));
  }

  inline static constexpr uint64_t kNsInUs = 1'000;

  [[nodiscard]] static double TimePerFrameUs(double rate,
                                             const FrameTrackStats& frame_track_stats) {
    return rate * frame_track_stats.ComputeAverageTimeNs() / kNsInUs;
  }

  [[nodiscard]] static double AverageFrameTime(const FrameTrackStats& stats) {
    return static_cast<double>(stats.ComputeAverageTimeNs() / kNsInUs);
  }

  [[nodiscard]] Baseline<double> BaselineExclusiveTimePerFrameUs(SFID sfid) const {
    return LiftAndApply(&TimePerFrameUs, BaselineExclusiveRate(sfid),
                        report_.GetBaselineFrameTrackStats());
  }
  [[nodiscard]] Comparison<double> ComparisonExclusiveTimePerFrameUs(SFID sfid) const {
    return LiftAndApply(&TimePerFrameUs, ComparisonExclusiveRate(sfid),
                        report_.GetComparisonFrameTrackStats());
  }

  [[nodiscard]] double GetPvalue(SFID sfid) const {
    const orbit_mizar_data::CorrectedComparisonResult& result = report_.GetComparisonResult(sfid);
    return is_multiplicity_correction_enabled_ ? result.corrected_pvalue : result.pvalue;
  }

  [[nodiscard]] static double Slowdown(Baseline<double> baseline_time,
                                       Comparison<double> comparison_time) {
    return *comparison_time - *baseline_time;
  }

  [[nodiscard]] double SlowdownPercent(SFID sfid) const {
    const Baseline<double> baseline_time = BaselineExclusiveTimePerFrameUs(sfid);
    const Comparison<double> comparison_time = ComparisonExclusiveTimePerFrameUs(sfid);
    return Slowdown(baseline_time, comparison_time) / *baseline_time * 100;
  }

  [[nodiscard]] double PercentOfFrameSlowdown(SFID sfid) const {
    const double function_slowdown_per_frame =
        Slowdown(BaselineExclusiveTimePerFrameUs(sfid), ComparisonExclusiveTimePerFrameUs(sfid));

    const Baseline<double> baseline_frame_time =
        orbit_base::LiftAndApply(&AverageFrameTime, report_.GetBaselineFrameTrackStats());
    const Comparison<double> comparison_frame_time =
        orbit_base::LiftAndApply(&AverageFrameTime, report_.GetComparisonFrameTrackStats());

    const double frame_slowdown = Slowdown(baseline_frame_time, comparison_frame_time);

    return function_slowdown_per_frame / std::abs(frame_slowdown) * 100;
  }

  [[nodiscard]] double MakeNumericEntry(SFID sfid, Column column) const {
    switch (column) {
      case Column::kBaselineExclusivePercent:
        return *BaselineExclusiveRate(sfid) * 100;
      case Column::kBaselineExclusiveTimePerFrame:
        return *BaselineExclusiveTimePerFrameUs(sfid);
      case Column::kComparisonExclusivePercent:
        return *ComparisonExclusiveRate(sfid) * 100;
      case Column::kComparisonExclusiveTimePerFrame:
        return *ComparisonExclusiveTimePerFrameUs(sfid);
      case Column::kPvalue:
        return GetPvalue(sfid);
      case Column::kSlowdownPercent:
        return SlowdownPercent(sfid);
      case Column::kPercentOfSlowdown:
        return PercentOfFrameSlowdown(sfid);
      default:
        ORBIT_UNREACHABLE();
    }
  }

  Report report_;
  std::vector<SFID> sfids_;
  bool is_multiplicity_correction_enabled_;
  double significance_level_;
};

using SamplingWithFrameTrackReportModel =
    SamplingWithFrameTrackReportModelTmpl<orbit_mizar_data::SamplingWithFrameTrackComparisonReport,
                                          orbit_mizar_data::SamplingCounts,
                                          orbit_client_data::ScopeStats>;

}  // namespace orbit_mizar_widgets

#endif  // MIZAR_WIDGETS_SAMPLING_WITH_FRAME_TRACK_REPORT_MODEL_H_