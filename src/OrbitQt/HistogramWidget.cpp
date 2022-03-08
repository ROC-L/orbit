// Copyright (c) 2022 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "HistogramWidget.h"

#include <absl/strings/str_cat.h>
#include <absl/strings/str_format.h>
#include <absl/strings/str_replace.h>

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPoint>
#include <QWidget>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <utility>

#include "ApiInterface/Orbit.h"
#include "DisplayFormats/DisplayFormats.h"
#include "Introspection/Introspection.h"
#include "OrbitBase/Logging.h"
#include "Statistics/Histogram.h"

constexpr uint32_t kVerticalTickCount = 3;
constexpr uint32_t kHorizontalTickCount = 3;
constexpr int kVerticalAxisTickLength = 4;
constexpr int kHorizontalAxisTickLength = 8;
constexpr int kTickLabelGap = 3;

const QColor kAxisColor = Qt::white;
constexpr int kLineWidth = 2;

constexpr int kTopMargin = 50;
constexpr int kBottomMargin = 40;
constexpr int kLeftMargin = 50;
constexpr int kRightMargin = 50;

const QString kDefaultTitle =
    QStringLiteral("Select a function with Count>0 to plot a histogram of its runtime");

const QColor kSelectionColor = QColor(128, 128, 255, 128);

[[nodiscard]] static int RoundToClosestInt(double x) { return static_cast<int>(std::round(x)); }

// if `length > 0`, the line will be plot to the right from `start` and to the left otherwise
static void DrawHorizontalLine(QPainter& painter, const QPoint& start, int length) {
  painter.drawLine(start, {start.x() + length, start.y()});
}

// if `length > 0`, the line will be plot downwards from `start` and upwards otherwise
static void DrawVerticalLine(QPainter& painter, const QPoint& start, int length) {
  painter.drawLine(start, {start.x(), start.y() + length});
}

static void DrawHorizontalAxis(QPainter& painter, const QPoint& axes_intersection,
                               const orbit_statistics::Histogram& histogram, int length,
                               uint64_t min_value) {
  DrawHorizontalLine(painter, axes_intersection, length);

  const auto tick_spacing_as_value = (histogram.max - min_value) / kHorizontalTickCount;
  const int tick_spacing_pixels =
      RoundToClosestInt(static_cast<double>(length) / kHorizontalTickCount);

  int current_tick_location = axes_intersection.x();
  uint64_t current_tick_value = min_value;

  const QFontMetrics font_metrics(painter.font());

  for (uint32_t i = 0; i <= kHorizontalTickCount; ++i) {
    DrawVerticalLine(painter, {current_tick_location, axes_intersection.y()},
                     kHorizontalAxisTickLength);

    const QString tick_label = QString::fromStdString(
        orbit_display_formats::GetDisplayTime(absl::Nanoseconds(current_tick_value)));
    const QRect tick_label_bounding_rect = font_metrics.tightBoundingRect(tick_label);

    painter.drawText(current_tick_location - tick_label_bounding_rect.width() / 2,
                     axes_intersection.y() + kHorizontalAxisTickLength +
                         tick_label_bounding_rect.height() + kTickLabelGap + kLineWidth / 2,
                     tick_label);

    current_tick_location += tick_spacing_pixels;
    current_tick_value += tick_spacing_as_value;
  }
}

static void DrawVerticalAxis(QPainter& painter, const QPoint& axes_intersection, int length,
                             double max_freq) {
  DrawVerticalLine(painter, axes_intersection, -length);

  const double tick_spacing_as_value = max_freq / kVerticalTickCount;
  const int tick_spacing_pixels =
      RoundToClosestInt(static_cast<double>(length) / kVerticalTickCount);

  double current_tick_value = tick_spacing_as_value;
  int current_tick_location = axes_intersection.y() - tick_spacing_pixels;

  const QFontMetrics font_metrics(painter.font());

  for (uint32_t i = 1; i <= kVerticalTickCount; ++i) {
    DrawHorizontalLine(painter, {axes_intersection.x(), current_tick_location},
                       -kVerticalAxisTickLength);

    QString tick_label = QString::fromStdString(absl::StrFormat("%.0f", current_tick_value * 100));

    QRect tick_label_bounding_rect = font_metrics.tightBoundingRect(tick_label);
    painter.drawText(axes_intersection.x() - tick_label_bounding_rect.width() -
                         kVerticalAxisTickLength - kTickLabelGap,
                     current_tick_location + (tick_label_bounding_rect.height()) / 2, tick_label);

    current_tick_location -= tick_spacing_pixels;
    current_tick_value += tick_spacing_as_value;
  }
}

[[nodiscard]] static int ValueToAxisLocation(double value, int axis_length, double min_value,
                                             double max_value) {
  if (min_value == max_value) max_value++;
  return RoundToClosestInt(((value - min_value) / (max_value - min_value)) * axis_length);
}

[[nodiscard]] static double GetFreq(const orbit_statistics::Histogram& histogram, size_t i) {
  return static_cast<double>(histogram.counts[i]) / static_cast<double>(histogram.data_set_size);
}

static void DrawHistogram(QPainter& painter, const QPoint& axes_intersection,
                          const orbit_statistics::Histogram& histogram, int horizontal_axis_length,
                          int vertical_axis_length, double max_freq, uint64_t min_value) {
  for (size_t i = 0; i < histogram.counts.size(); ++i) {
    const uint64_t bin_from = histogram.min + i * histogram.bin_width;
    const uint64_t bin_to = bin_from + histogram.bin_width;

    double freq = GetFreq(histogram, i);
    if (freq > 0) {
      const QPoint top_left(
          axes_intersection.x() +
              ValueToAxisLocation(bin_from, horizontal_axis_length, min_value, histogram.max),
          axes_intersection.y() - kLineWidth -
              ValueToAxisLocation(freq, vertical_axis_length, 0, max_freq));
      const QPoint lower_right(
          axes_intersection.x() +
              ValueToAxisLocation(bin_to, horizontal_axis_length, min_value, histogram.max),
          axes_intersection.y() - kLineWidth);
      const QRect bar(top_left, lower_right);
      painter.fillRect(bar, Qt::cyan);
    }
  }
}

void HistogramWidget::UpdateData(const std::vector<uint64_t>* data, std::string function_name,
                                 uint64_t function_id) {
  ORBIT_SCOPE_FUNCTION;
  if (function_data_.has_value() && function_data_->id == function_id) return;

  histogram_stack_ = {};
  ranges_stack_ = {};
  EmitSignalSelectionRangeChange();

  function_data_.emplace(data, std::move(function_name), function_id);

  if (data != nullptr) {
    std::optional<orbit_statistics::Histogram> histogram =
        orbit_statistics::BuildHistogram(*function_data_->data);
    if (histogram) {
      histogram_stack_.push(std::move(*histogram));
    }
  }

  selected_area_.reset();

  EmitSignalTitleChange();
  update();
}

static void DrawSelection(QPainter& painter, int start_x, int end_x,
                          const QPoint& axes_intersection, int vertical_axis_length) {
  if (start_x == end_x) return;
  if (start_x > end_x) std::swap(start_x, end_x);

  const QPoint top_left = {start_x, axes_intersection.y() - vertical_axis_length};
  const QPoint bottom_right = {end_x, axes_intersection.y()};
  const QRect selection(top_left, bottom_right);
  painter.fillRect(selection, kSelectionColor);
}

void HistogramWidget::paintEvent(QPaintEvent* /*event*/) {
  if (histogram_stack_.empty()) {
    return;
  }

  QPainter painter(this);

  const orbit_statistics::Histogram& histogram = histogram_stack_.top();

  QPoint axes_intersection(kLeftMargin, Height() - kBottomMargin);

  const int vertical_axis_length = Height() - kTopMargin - kBottomMargin;
  const int horizontal_axis_length = Width() - kLeftMargin - kRightMargin;

  const uint64_t max_count =
      *std::max_element(std::begin(histogram.counts), std::end(histogram.counts));
  const double max_freq = static_cast<double>(max_count) / histogram.data_set_size;

  DrawHistogram(painter, axes_intersection, histogram, horizontal_axis_length, vertical_axis_length,
                max_freq, MinValue());

  painter.setPen(QPen(kAxisColor, kLineWidth));
  DrawHorizontalAxis(painter, axes_intersection, histogram, horizontal_axis_length, MinValue());
  DrawVerticalAxis(painter, axes_intersection, vertical_axis_length, max_freq);
  painter.setPen(QPen(Qt::white, 1));

  if (selected_area_) {
    DrawSelection(painter, selected_area_->selection_start_pixel,
                  selected_area_->selection_current_pixel, axes_intersection, vertical_axis_length);
  }
}

void HistogramWidget::mousePressEvent(QMouseEvent* event) {
  if (histogram_stack_.empty()) return;

  const int pixel_x = event->x();
  selected_area_ = {pixel_x, pixel_x};

  update();
}

[[nodiscard]] static uint64_t LocationToValue(int pos_x, int width, uint64_t min_value,
                                              uint64_t max_value) {
  if (pos_x <= kLeftMargin) return 0;
  if (pos_x > width - kRightMargin) return max_value + 1;

  const int location = pos_x - kLeftMargin;
  const int histogram_width = width - kLeftMargin - kRightMargin;
  const uint64_t value_range = max_value - min_value;
  return min_value +
         static_cast<uint64_t>(static_cast<double>(location) / histogram_width * value_range);
}

void HistogramWidget::mouseReleaseEvent(QMouseEvent* /* event*/) {
  if (histogram_stack_.empty()) return;

  ORBIT_SCOPE("Histogram zooming in");
  if (selected_area_) {
    // if it wasn't a drag, but just a click, go one level of selections up
    if (selected_area_->selection_start_pixel == selected_area_->selection_current_pixel) {
      if (IsSelectionActive()) {
        histogram_stack_.pop();
        ranges_stack_.pop();
      }
      selected_area_.reset();
      UpdateAndNotify();
      return;
    }

    uint64_t min =
        LocationToValue(selected_area_->selection_start_pixel, Width(), MinValue(), MaxValue());
    uint64_t max =
        LocationToValue(selected_area_->selection_current_pixel, Width(), MinValue(), MaxValue());
    if (min > max) {
      std::swap(min, max);
    }

    const auto data_begin = function_data_->data->begin();
    const auto data_end = function_data_->data->end();

    const auto min_it = std::lower_bound(data_begin, data_end, min);
    if (min_it != function_data_->data->end()) {
      const auto max_it = std::upper_bound(data_begin, data_end, max);
      const auto selection = absl::Span<const uint64_t>(&*min_it, std::distance(min_it, max_it));

      auto histogram = orbit_statistics::BuildHistogram(selection);
      if (histogram) {
        histogram_stack_.push(std::move(*histogram));
        ranges_stack_.push({min, max});
      }
    }
    selected_area_.reset();
  }

  UpdateAndNotify();
}

void HistogramWidget::mouseMoveEvent(QMouseEvent* event) {
  if (!selected_area_) return;

  selected_area_->selection_current_pixel = event->x();

  update();
}

bool HistogramWidget::IsSelectionActive() const { return histogram_stack_.size() > 1; }

uint64_t HistogramWidget::MinValue() const {
  return IsSelectionActive() ? histogram_stack_.top().min : 0;
}

uint64_t HistogramWidget::MaxValue() const { return histogram_stack_.top().max; }

int HistogramWidget::Width() const { return size().width(); }

int HistogramWidget::Height() const { return size().height(); }

[[nodiscard]] std::optional<orbit_statistics::HistogramSelectionRange>
HistogramWidget::GetSelectionRange() const {
  if (ranges_stack_.empty()) {
    return std::nullopt;
  }
  return ranges_stack_.top();
}

void HistogramWidget::EmitSignalSelectionRangeChange() const {
  emit SignalSelectionRangeChange(GetSelectionRange());
}

void HistogramWidget::EmitSignalTitleChange() const { emit SignalTitleChange(GetTitle()); }

void HistogramWidget::UpdateAndNotify() {
  EmitSignalSelectionRangeChange();
  EmitSignalTitleChange();
  update();
}

constexpr size_t kMaxFunctionNameLengthForTitle = 80;

[[nodiscard]] QString HistogramWidget::GetTitle() const {
  if (!function_data_.has_value() || histogram_stack_.empty()) {
    return kDefaultTitle;
  }
  std::string function_name = function_data_->name;
  if (function_name.size() > kMaxFunctionNameLengthForTitle) {
    function_name = absl::StrCat(function_name.substr(0, kMaxFunctionNameLengthForTitle), "...");
  }

  function_name =
      absl::StrReplaceAll(function_name, {{"&", "&amp;"}, {"<", "&lt;"}, {">", "&gt;"}});

  std::string title =
      absl::StrFormat("<b>%s</b> (%d of %d samples)", function_name,
                      histogram_stack_.top().data_set_size, function_data_->data->size());

  return QString::fromStdString(title);
}
