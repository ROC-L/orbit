// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_GL_TIME_GRAPH_H_
#define ORBIT_GL_TIME_GRAPH_H_

#include <unordered_map>
#include <utility>

#include "../Orbit.h"
#include "AsyncTrack.h"
#include "Batcher.h"
#include "BlockChain.h"
#include "Geometry.h"
#include "GpuTrack.h"
#include "GraphTrack.h"
#include "ManualInstrumentationManager.h"
#include "OrbitBase/Profiling.h"
#include "SchedulerTrack.h"
#include "ScopeTimer.h"
#include "StringManager.h"
#include "TextBox.h"
#include "TextRenderer.h"
#include "ThreadTrack.h"
#include "TimeGraphLayout.h"
#include "TimerChain.h"
#include "absl/container/flat_hash_map.h"
#include "capture_data.pb.h"

class TimeGraph {
 public:
  TimeGraph();
  ~TimeGraph();

  void Draw(GlCanvas* canvas, PickingMode picking_mode = PickingMode::kNone);
  void DrawTracks(GlCanvas* canvas, PickingMode picking_mode = PickingMode::kNone);
  void DrawOverlay(GlCanvas* canvas, PickingMode picking_mode);
  void DrawText(GlCanvas* canvas);

  void NeedsUpdate();
  void UpdatePrimitives(PickingMode picking_mode);
  void SortTracks();
  std::vector<orbit_client_protos::CallstackEvent> SelectEvents(float world_start, float world_end,
                                                                int32_t thread_id);
  const std::vector<orbit_client_protos::CallstackEvent>& GetSelectedCallstackEvents(int32_t tid);

  void ProcessTimer(const orbit_client_protos::TimerInfo& timer_info,
                    const orbit_client_protos::FunctionInfo* function);
  void UpdateMaxTimeStamp(uint64_t time);

  float GetThreadTotalHeight();
  float GetTextBoxHeight() const { return layout_.GetTextBoxHeight(); }
  float GetWorldFromTick(uint64_t time) const;
  float GetWorldFromUs(double micros) const;
  uint64_t GetTickFromWorld(float world_x) const;
  uint64_t GetTickFromUs(double micros) const;
  double GetUsFromTick(uint64_t time) const;
  double GetTimeWindowUs() const { return time_window_us_; }
  void GetWorldMinMax(float* min, float* max) const;
  bool UpdateCaptureMinMaxTimestamps();

  void Clear();
  void ZoomAll();
  void Zoom(const TextBox* text_box);
  void Zoom(uint64_t min, uint64_t max);
  void ZoomTime(float zoom_value, double mouse_ratio);
  void VerticalZoom(float zoom_value, float mouse_ratio);
  void SetMinMax(double min_time_us, double max_time_us);
  void PanTime(int initial_x, int current_x, int width, double initial_time);
  enum class VisibilityType {
    kPartlyVisible,
    kFullyVisible,
  };
  void HorizontallyMoveIntoView(VisibilityType vis_type, uint64_t min, uint64_t max,
                                double distance = 0.3);
  void HorizontallyMoveIntoView(VisibilityType vis_type, const TextBox* text_box,
                                double distance = 0.3);
  void VerticallyMoveIntoView(const TextBox* text_box);
  double GetTime(double ratio) const;
  void Select(const TextBox* text_box);
  enum class JumpScope { kGlobal, kSameDepth, kSameThread, kSameFunction, kSameThreadSameFunction };
  enum class JumpDirection { kPrevious, kNext, kTop, kDown };
  void JumpToNeighborBox(const TextBox* from, JumpDirection jump_direction, JumpScope jump_scope);
  const TextBox* FindPreviousFunctionCall(uint64_t function_address, uint64_t current_time,
                                          std::optional<int32_t> thread_id = std::nullopt) const;
  const TextBox* FindNextFunctionCall(uint64_t function_address, uint64_t current_time,
                                      std::optional<int32_t> thread_id = std::nullopt) const;
  void SelectAndZoom(const TextBox* text_box);
  double GetCaptureTimeSpanUs();
  double GetCurrentTimeSpanUs();
  void NeedsRedraw() { needs_redraw_ = true; }
  bool IsRedrawNeeded() const { return needs_redraw_; }
  void SetThreadFilter(const std::string& filter);

  bool IsFullyVisible(uint64_t min, uint64_t max) const;
  bool IsPartlyVisible(uint64_t min, uint64_t max) const;
  bool IsVisible(VisibilityType vis_type, uint64_t min, uint64_t max) const;

  int GetNumDrawnTextBoxes() { return num_drawn_text_boxes_; }
  void SetTextRenderer(TextRenderer* text_renderer) { text_renderer_ = text_renderer; }
  TextRenderer* GetTextRenderer() { return &text_renderer_static_; }
  void SetStringManager(std::shared_ptr<StringManager> str_manager);
  StringManager* GetStringManager() { return string_manager_.get(); }
  void SetCanvas(GlCanvas* canvas);
  GlCanvas* GetCanvas() { return canvas_; }
  void SetFontSize(int font_size);
  int GetFontSize() { return GetTextRenderer()->GetFontSize(); }
  Batcher& GetBatcher() { return batcher_; }
  uint32_t GetNumTimers() const;
  uint32_t GetNumCores() const;
  std::vector<std::shared_ptr<TimerChain>> GetAllTimerChains() const;
  std::vector<std::shared_ptr<TimerChain>> GetAllThreadTrackTimerChains() const;

  void OnDrag(float ratio);
  double GetMinTimeUs() const { return min_time_us_; }
  double GetMaxTimeUs() const { return max_time_us_; }
  const TimeGraphLayout& GetLayout() const { return layout_; }
  TimeGraphLayout& GetLayout() { return layout_; }
  float GetRightMargin() const { return right_margin_; }
  void SetRightMargin(float margin) { right_margin_ = margin; }

  const TextBox* FindPrevious(const TextBox* from);
  const TextBox* FindNext(const TextBox* from);
  const TextBox* FindTop(const TextBox* from);
  const TextBox* FindDown(const TextBox* from);

  [[nodiscard]] static Color GetColor(uint32_t id) {
    constexpr unsigned char kAlpha = 255;
    static std::vector<Color> colors{
        Color(231, 68, 53, kAlpha),    // red
        Color(43, 145, 175, kAlpha),   // blue
        Color(185, 117, 181, kAlpha),  // purple
        Color(87, 166, 74, kAlpha),    // green
        Color(215, 171, 105, kAlpha),  // beige
        Color(248, 101, 22, kAlpha)    // orange
    };
    return colors[id % colors.size()];
  }
  [[nodiscard]] static Color GetColor(uint64_t id) { return GetColor(static_cast<uint32_t>(id)); }
  [[nodiscard]] static Color GetColor(const std::string& str) { return GetColor(StringHash(str)); }
  [[nodiscard]] static Color GetThreadColor(int32_t tid) {
    return GetColor(static_cast<uint32_t>(tid));
  }

  void SetIteratorOverlayData(
      const absl::flat_hash_map<uint64_t, const TextBox*>& iterator_text_boxes,
      const absl::flat_hash_map<uint64_t, const orbit_client_protos::FunctionInfo*>&
          iterator_functions) {
    iterator_text_boxes_ = iterator_text_boxes;
    iterator_functions_ = iterator_functions;
    NeedsRedraw();
  }

  uint64_t GetCaptureMin() const { return capture_min_timestamp_; }
  uint64_t GetCaptureMax() const { return capture_max_timestamp_; }
  uint64_t GetCurrentMouseTimeNs() const { return current_mouse_time_ns_; }

 protected:
  std::shared_ptr<SchedulerTrack> GetOrCreateSchedulerTrack();
  std::shared_ptr<ThreadTrack> GetOrCreateThreadTrack(int32_t tid);
  std::shared_ptr<GpuTrack> GetOrCreateGpuTrack(uint64_t timeline_hash);
  GraphTrack* GetOrCreateGraphTrack(const std::string& name);
  AsyncTrack* GetOrCreateAsyncTrack(const std::string& name);

  void ProcessOrbitFunctionTimer(orbit_client_protos::FunctionInfo::OrbitType type,
                                 const orbit_client_protos::TimerInfo& timer_info);
  void ProcessValueTrackingTimer(const orbit_client_protos::TimerInfo& timer_info);
  void ProcessAsyncTimer(const std::string& track_name,
                         const orbit_client_protos::TimerInfo& timer_info);

 private:
  TextRenderer text_renderer_static_;
  TextRenderer* text_renderer_ = nullptr;
  GlCanvas* canvas_ = nullptr;
  int num_drawn_text_boxes_ = 0;

  // First member is id.
  absl::flat_hash_map<uint64_t, const TextBox*> iterator_text_boxes_;
  absl::flat_hash_map<uint64_t, const orbit_client_protos::FunctionInfo*> iterator_functions_;

  double ref_time_us_ = 0;
  double min_time_us_ = 0;
  double max_time_us_ = 0;
  uint64_t capture_min_timestamp_ = 0;
  uint64_t capture_max_timestamp_ = 0;
  uint64_t current_mouse_time_ns_ = 0;
  std::map<int32_t, uint32_t> event_count_;
  double time_window_us_ = 0;
  float world_start_x_ = 0;
  float world_width_ = 0;
  float min_y_ = 0;
  float right_margin_ = 0;

  TimeGraphLayout layout_;

  std::map<int32_t, uint32_t> thread_count_map_;

  // Be careful when directly changing these members without using the
  // methods NeedsRedraw() or NeedsUpdate():
  // needs_update_primitives_ should always imply needs_redraw_, that is
  // needs_update_primitives_ => needs_redraw_ is an invariant of this
  // class. When updating the primitives, which computes the primitives
  // to be drawn and their coordinates, we always have to redraw the
  // timeline.
  bool needs_update_primitives_ = false;
  bool needs_redraw_ = false;

  bool draw_text_ = true;

  Batcher batcher_;
  Timer last_thread_reorder_;

  mutable Mutex mutex_;
  std::vector<std::shared_ptr<Track>> tracks_;
  std::unordered_map<int32_t, std::shared_ptr<ThreadTrack>> thread_tracks_;
  std::map<std::string, std::shared_ptr<AsyncTrack>> async_tracks_;
  std::map<std::string, std::shared_ptr<GraphTrack>> graph_tracks_;
  // Mapping from timeline hash to GPU tracks.
  std::unordered_map<uint64_t, std::shared_ptr<GpuTrack>> gpu_tracks_;
  std::vector<std::shared_ptr<Track>> sorted_tracks_;
  std::string thread_filter_;

  std::set<uint32_t> cores_seen_;
  std::shared_ptr<SchedulerTrack> scheduler_track_;
  std::shared_ptr<ThreadTrack> process_track_;

  absl::flat_hash_map<int32_t, std::vector<orbit_client_protos::CallstackEvent>>
      selected_callstack_events_per_thread_;

  std::shared_ptr<StringManager> string_manager_;
  ManualInstrumentationManager* manual_instrumentation_manager_;
  std::unique_ptr<ManualInstrumentationManager::AsyncTimerInfoListener> async_timer_info_listener_;
};

extern TimeGraph* GCurrentTimeGraph;

#endif  // ORBIT_GL_TIME_GRAPH_H_
