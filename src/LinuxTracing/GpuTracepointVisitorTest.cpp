// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#include <memory>
#include <string>
#include <utility>

#include "GpuTracepointVisitor.h"
#include "KernelTracepoints.h"
#include "OrbitBase/Logging.h"
#include "PerfEvent.h"
#include "PerfEventRecords.h"
#include "TracingInterface/TracerListener.h"
#include "capture.pb.h"

namespace orbit_linux_tracing {

namespace {

class MockTracerListener : public orbit_tracing_interface::TracerListener {
 public:
  MOCK_METHOD(void, OnSchedulingSlice, (orbit_grpc_protos::SchedulingSlice), (override));
  MOCK_METHOD(void, OnCallstackSample, (orbit_grpc_protos::FullCallstackSample), (override));
  MOCK_METHOD(void, OnFunctionCall, (orbit_grpc_protos::FunctionCall), (override));
  MOCK_METHOD(void, OnGpuJob, (orbit_grpc_protos::FullGpuJob full_gpu_job), (override));
  MOCK_METHOD(void, OnThreadName, (orbit_grpc_protos::ThreadName), (override));
  MOCK_METHOD(void, OnThreadNamesSnapshot, (orbit_grpc_protos::ThreadNamesSnapshot), (override));
  MOCK_METHOD(void, OnThreadStateSlice, (orbit_grpc_protos::ThreadStateSlice), (override));
  MOCK_METHOD(void, OnAddressInfo, (orbit_grpc_protos::FullAddressInfo), (override));
  MOCK_METHOD(void, OnTracepointEvent, (orbit_grpc_protos::FullTracepointEvent), (override));
  MOCK_METHOD(void, OnModuleUpdate, (orbit_grpc_protos::ModuleUpdateEvent), (override));
  MOCK_METHOD(void, OnModulesSnapshot, (orbit_grpc_protos::ModulesSnapshot), (override));
  MOCK_METHOD(void, OnErrorsWithPerfEventOpenEvent,
              (orbit_grpc_protos::ErrorsWithPerfEventOpenEvent), (override));
  MOCK_METHOD(void, OnLostPerfRecordsEvent, (orbit_grpc_protos::LostPerfRecordsEvent), (override));
  MOCK_METHOD(void, OnOutOfOrderEventsDiscardedEvent,
              (orbit_grpc_protos::OutOfOrderEventsDiscardedEvent), (override));
};

class GpuTracepointVisitorTest : public ::testing::Test {
 protected:
  MockTracerListener mock_listener_;
  GpuTracepointVisitor visitor_{&mock_listener_};
};

std::unique_ptr<AmdgpuCsIoctlPerfEvent> MakeFakeAmdgpuCsIoctlPerfEvent(
    pid_t pid, pid_t tid, uint64_t timestamp_ns, uint32_t context, uint32_t seqno,
    const std::string& timeline) {
  auto event = std::make_unique<AmdgpuCsIoctlPerfEvent>(
      static_cast<uint32_t>(sizeof(amdgpu_cs_ioctl_tracepoint) + timeline.length() + 1));
  event->ring_buffer_record.sample_id.pid = pid;
  CHECK(event->GetPid() == pid);
  event->ring_buffer_record.sample_id.tid = tid;
  CHECK(event->GetTid() == tid);
  event->ring_buffer_record.sample_id.time = timestamp_ns;
  CHECK(event->GetTimestamp() == timestamp_ns);
  reinterpret_cast<amdgpu_cs_ioctl_tracepoint*>(event->tracepoint_data.get())->context = context;
  CHECK(event->GetContext() == context);
  reinterpret_cast<amdgpu_cs_ioctl_tracepoint*>(event->tracepoint_data.get())->seqno = seqno;
  CHECK(event->GetSeqno() == seqno);
  // This logic is the reverse of GpuPerfEvent::ExtractTimelineString.
  reinterpret_cast<amdgpu_cs_ioctl_tracepoint*>(event->tracepoint_data.get())->timeline =
      ((timeline.length() + 1) << 16) | sizeof(amdgpu_cs_ioctl_tracepoint);
  memcpy(event->tracepoint_data.get() + sizeof(amdgpu_cs_ioctl_tracepoint), timeline.c_str(),
         timeline.length() + 1);
  CHECK(event->ExtractTimelineString() == timeline);
  return event;
}

std::unique_ptr<AmdgpuSchedRunJobPerfEvent> MakeFakeAmdgpuSchedRunJobPerfEvent(
    uint64_t timestamp_ns, uint32_t context, uint32_t seqno, const std::string& timeline) {
  auto event = std::make_unique<AmdgpuSchedRunJobPerfEvent>(
      static_cast<uint32_t>(sizeof(amdgpu_sched_run_job_tracepoint) + timeline.length() + 1));
  event->ring_buffer_record.sample_id.time = timestamp_ns;
  CHECK(event->GetTimestamp() == timestamp_ns);
  reinterpret_cast<amdgpu_sched_run_job_tracepoint*>(event->tracepoint_data.get())->context =
      context;
  CHECK(event->GetContext() == context);
  reinterpret_cast<amdgpu_sched_run_job_tracepoint*>(event->tracepoint_data.get())->seqno = seqno;
  CHECK(event->GetSeqno() == seqno);
  reinterpret_cast<amdgpu_sched_run_job_tracepoint*>(event->tracepoint_data.get())->timeline =
      ((timeline.length() + 1) << 16) | sizeof(amdgpu_sched_run_job_tracepoint);
  memcpy(event->tracepoint_data.get() + sizeof(amdgpu_sched_run_job_tracepoint), timeline.c_str(),
         timeline.length() + 1);
  CHECK(event->ExtractTimelineString() == timeline);
  return event;
}

std::unique_ptr<DmaFenceSignaledPerfEvent> MakeFakeDmaFenceSignaledPerfEvent(
    uint64_t timestamp_ns, uint32_t context, uint32_t seqno, const std::string& timeline) {
  auto event = std::make_unique<DmaFenceSignaledPerfEvent>(
      static_cast<uint32_t>(sizeof(dma_fence_signaled_tracepoint) + timeline.length() + 1));
  event->ring_buffer_record.sample_id.time = timestamp_ns;
  CHECK(event->GetTimestamp() == timestamp_ns);
  reinterpret_cast<dma_fence_signaled_tracepoint*>(event->tracepoint_data.get())->context = context;
  CHECK(event->GetContext() == context);
  reinterpret_cast<dma_fence_signaled_tracepoint*>(event->tracepoint_data.get())->seqno = seqno;
  CHECK(event->GetSeqno() == seqno);
  reinterpret_cast<dma_fence_signaled_tracepoint*>(event->tracepoint_data.get())->timeline =
      ((timeline.length() + 1) << 16) | sizeof(dma_fence_signaled_tracepoint);
  memcpy(event->tracepoint_data.get() + sizeof(dma_fence_signaled_tracepoint), timeline.c_str(),
         timeline.length() + 1);
  CHECK(event->ExtractTimelineString() == timeline);
  return event;
}

orbit_grpc_protos::FullGpuJob MakeGpuJob(uint32_t pid, uint32_t tid, uint32_t context,
                                         uint32_t seqno, std::string timeline, int32_t depth,
                                         uint64_t amdgpu_cs_ioctl_time_ns,
                                         uint64_t amdgpu_sched_run_job_time_ns,
                                         uint64_t gpu_hardware_start_time_ns,
                                         uint64_t dma_fence_signaled_time_ns) {
  orbit_grpc_protos::FullGpuJob expected_gpu_job;
  expected_gpu_job.set_pid(pid);
  expected_gpu_job.set_tid(tid);
  expected_gpu_job.set_context(context);
  expected_gpu_job.set_seqno(seqno);
  expected_gpu_job.set_timeline(std::move(timeline));
  expected_gpu_job.set_depth(depth);
  expected_gpu_job.set_amdgpu_cs_ioctl_time_ns(amdgpu_cs_ioctl_time_ns);
  expected_gpu_job.set_amdgpu_sched_run_job_time_ns(amdgpu_sched_run_job_time_ns);
  expected_gpu_job.set_gpu_hardware_start_time_ns(gpu_hardware_start_time_ns);
  expected_gpu_job.set_dma_fence_signaled_time_ns(dma_fence_signaled_time_ns);
  return expected_gpu_job;
}

::testing::Matcher<orbit_grpc_protos::FullGpuJob> GpuJobEq(
    const orbit_grpc_protos::FullGpuJob& expected) {
  return ::testing::AllOf(
      ::testing::Property("pid", &orbit_grpc_protos::FullGpuJob::pid, expected.pid()),
      ::testing::Property("tid", &orbit_grpc_protos::FullGpuJob::tid, expected.tid()),
      ::testing::Property("context", &orbit_grpc_protos::FullGpuJob::context, expected.context()),
      ::testing::Property("seqno", &orbit_grpc_protos::FullGpuJob::seqno, expected.seqno()),
      ::testing::Property("timeline", &orbit_grpc_protos::FullGpuJob::timeline,
                          expected.timeline()),
      ::testing::Property("depth", &orbit_grpc_protos::FullGpuJob::depth, expected.depth()),
      ::testing::Property("amdgpu_cs_ioctl_time_ns",
                          &orbit_grpc_protos::FullGpuJob::amdgpu_cs_ioctl_time_ns,
                          expected.amdgpu_cs_ioctl_time_ns()),
      ::testing::Property("amdgpu_sched_run_job_time_ns",
                          &orbit_grpc_protos::FullGpuJob::amdgpu_sched_run_job_time_ns,
                          expected.amdgpu_sched_run_job_time_ns()),
      ::testing::Property("gpu_hardware_start_time_ns",
                          &orbit_grpc_protos::FullGpuJob::gpu_hardware_start_time_ns,
                          expected.gpu_hardware_start_time_ns()),
      ::testing::Property("dma_fence_signaled_time_ns",
                          &orbit_grpc_protos::FullGpuJob::dma_fence_signaled_time_ns,
                          expected.dma_fence_signaled_time_ns()));
}

}  // namespace

TEST(GpuTracepointVisitor, NeedsListener) {
  EXPECT_DEATH(GpuTracepointVisitor{nullptr}, "listener_ != nullptr");
}

TEST_F(GpuTracepointVisitorTest, JobCreatedWithAllThreePerfEvents) {
  static constexpr pid_t kPid = 41;
  static constexpr pid_t kTid = 42;
  static constexpr uint32_t kContext = 1;
  static constexpr uint32_t kSeqno = 10;
  static const std::string kTimeline = "timeline";
  static constexpr uint64_t kTimestampA = 100;
  static constexpr uint64_t kTimestampB = 200;
  static constexpr uint64_t kTimestampC = kTimestampB;
  static constexpr uint64_t kTimestampD = 300;

  orbit_grpc_protos::FullGpuJob expected_gpu_job =
      MakeGpuJob(kPid, kTid, kContext, kSeqno, kTimeline, 0, kTimestampA, kTimestampB, kTimestampC,
                 kTimestampD);
  orbit_grpc_protos::FullGpuJob actual_gpu_job;
  EXPECT_CALL(mock_listener_, OnGpuJob).Times(1).WillOnce(::testing::SaveArg<0>(&actual_gpu_job));
  MakeFakeAmdgpuCsIoctlPerfEvent(kPid, kTid, kTimestampA, kContext, kSeqno, kTimeline)
      ->Accept(&visitor_);
  MakeFakeAmdgpuSchedRunJobPerfEvent(kTimestampB, kContext, kSeqno, kTimeline)->Accept(&visitor_);
  MakeFakeDmaFenceSignaledPerfEvent(kTimestampD, kContext, kSeqno, kTimeline)->Accept(&visitor_);
  EXPECT_THAT(actual_gpu_job, GpuJobEq(expected_gpu_job));
}

TEST_F(GpuTracepointVisitorTest, JobCreatedEvenWithOutOfOrderPerfEvents1) {
  static constexpr pid_t kPid = 41;
  static constexpr pid_t kTid = 42;
  static constexpr uint32_t kContext = 1;
  static constexpr uint32_t kSeqno = 10;
  static const std::string kTimeline = "timeline";
  static constexpr uint64_t kTimestampA = 100;
  static constexpr uint64_t kTimestampB = 200;
  static constexpr uint64_t kTimestampC = kTimestampB;
  static constexpr uint64_t kTimestampD = 300;

  orbit_grpc_protos::FullGpuJob expected_gpu_job =
      MakeGpuJob(kPid, kTid, kContext, kSeqno, kTimeline, 0, kTimestampA, kTimestampB, kTimestampC,
                 kTimestampD);
  orbit_grpc_protos::FullGpuJob actual_gpu_job;
  EXPECT_CALL(mock_listener_, OnGpuJob).Times(1).WillOnce(::testing::SaveArg<0>(&actual_gpu_job));
  MakeFakeDmaFenceSignaledPerfEvent(kTimestampD, kContext, kSeqno, kTimeline)->Accept(&visitor_);
  MakeFakeAmdgpuSchedRunJobPerfEvent(kTimestampB, kContext, kSeqno, kTimeline)->Accept(&visitor_);

  MakeFakeAmdgpuCsIoctlPerfEvent(kPid, kTid, kTimestampA, kContext, kSeqno, kTimeline)
      ->Accept(&visitor_);
  EXPECT_THAT(actual_gpu_job, GpuJobEq(expected_gpu_job));
}

TEST_F(GpuTracepointVisitorTest, JobCreatedEvenWithOutOfOrderPerfEvents2) {
  static constexpr pid_t kPid = 41;
  static constexpr pid_t kTid = 42;
  static constexpr uint32_t kContext = 1;
  static constexpr uint32_t kSeqno = 10;
  static const std::string kTimeline = "timeline";
  static constexpr uint64_t kTimestampA = 100;
  static constexpr uint64_t kTimestampB = 200;
  static constexpr uint64_t kTimestampC = kTimestampB;
  static constexpr uint64_t kTimestampD = 300;

  orbit_grpc_protos::FullGpuJob expected_gpu_job =
      MakeGpuJob(kPid, kTid, kContext, kSeqno, kTimeline, 0, kTimestampA, kTimestampB, kTimestampC,
                 kTimestampD);
  orbit_grpc_protos::FullGpuJob actual_gpu_job;
  EXPECT_CALL(mock_listener_, OnGpuJob).Times(1).WillOnce(::testing::SaveArg<0>(&actual_gpu_job));
  MakeFakeAmdgpuSchedRunJobPerfEvent(kTimestampB, kContext, kSeqno, kTimeline)->Accept(&visitor_);
  MakeFakeAmdgpuCsIoctlPerfEvent(kPid, kTid, kTimestampA, kContext, kSeqno, kTimeline)
      ->Accept(&visitor_);
  MakeFakeDmaFenceSignaledPerfEvent(kTimestampD, kContext, kSeqno, kTimeline)->Accept(&visitor_);
  EXPECT_THAT(actual_gpu_job, GpuJobEq(expected_gpu_job));
}

TEST_F(GpuTracepointVisitorTest, NoJobBecauseOfMismatchingContext) {
  static constexpr pid_t kPid = 41;
  static constexpr pid_t kTid = 42;
  static constexpr uint32_t kContext = 1;
  static constexpr uint32_t kSeqno = 10;
  static const std::string kTimeline = "timeline";
  static constexpr uint64_t kTimestampA = 100;
  static constexpr uint64_t kTimestampB = 200;
  static constexpr uint64_t kTimestampD = 300;

  EXPECT_CALL(mock_listener_, OnGpuJob).Times(0);

  MakeFakeAmdgpuCsIoctlPerfEvent(kPid, kTid, kTimestampA, kContext + 1, kSeqno, kTimeline)
      ->Accept(&visitor_);
  MakeFakeAmdgpuSchedRunJobPerfEvent(kTimestampB, kContext, kSeqno, kTimeline)->Accept(&visitor_);
  MakeFakeDmaFenceSignaledPerfEvent(kTimestampD, kContext, kSeqno, kTimeline)->Accept(&visitor_);
}

TEST_F(GpuTracepointVisitorTest, NoJobBecauseOfMismatchingSeqno) {
  static constexpr pid_t kPid = 41;
  static constexpr pid_t kTid = 42;
  static constexpr uint32_t kContext = 1;
  static constexpr uint32_t kSeqno = 10;
  static const std::string kTimeline = "timeline";
  static constexpr uint64_t kTimestampA = 100;
  static constexpr uint64_t kTimestampB = 200;
  static constexpr uint64_t kTimestampD = 300;

  EXPECT_CALL(mock_listener_, OnGpuJob).Times(0);
  MakeFakeAmdgpuCsIoctlPerfEvent(kPid, kTid, kTimestampA, kContext, kSeqno, kTimeline)
      ->Accept(&visitor_);
  MakeFakeAmdgpuSchedRunJobPerfEvent(kTimestampB, kContext, kSeqno + 1, kTimeline)
      ->Accept(&visitor_);
  MakeFakeDmaFenceSignaledPerfEvent(kTimestampD, kContext, kSeqno, kTimeline)->Accept(&visitor_);
}

TEST_F(GpuTracepointVisitorTest, NoJobBecauseOfMismatchingTimeline) {
  static constexpr pid_t kPid = 41;
  static constexpr pid_t kTid = 42;
  static constexpr uint32_t kContext = 1;
  static constexpr uint32_t kSeqno = 10;
  static const std::string kTimeline = "timeline";
  static constexpr uint64_t kTimestampA = 100;
  static constexpr uint64_t kTimestampB = 200;
  static constexpr uint64_t kTimestampD = 300;

  EXPECT_CALL(mock_listener_, OnGpuJob).Times(0);
  MakeFakeAmdgpuCsIoctlPerfEvent(kPid, kTid, kTimestampA, kContext, kSeqno, kTimeline)
      ->Accept(&visitor_);
  MakeFakeAmdgpuSchedRunJobPerfEvent(kTimestampB, kContext, kSeqno, kTimeline)->Accept(&visitor_);
  MakeFakeDmaFenceSignaledPerfEvent(kTimestampD, kContext, kSeqno, kTimeline + "1")
      ->Accept(&visitor_);
}

TEST_F(GpuTracepointVisitorTest, TwoNonOverlappingJobsWithSameDepthDifferingByContext) {
  static constexpr pid_t kPid = 41;
  static constexpr pid_t kTid = 42;
  static constexpr uint32_t kContext1 = 1;
  static constexpr uint32_t kContext2 = 2;
  static constexpr uint32_t kSeqno = 10;
  static const std::string kTimeline = "timeline";
  static constexpr uint64_t kTimestampA1 = 100;
  static constexpr uint64_t kTimestampB1 = 200;
  static constexpr uint64_t kTimestampC1 = kTimestampB1;
  static constexpr uint64_t kTimestampD1 = 300;
  static constexpr uint64_t kNsDistanceForSameDepth = 1'000'000;
  static constexpr uint64_t kTimestampA2 = kNsDistanceForSameDepth + 300;
  static constexpr uint64_t kTimestampB2 = kNsDistanceForSameDepth + 400;
  static constexpr uint64_t kTimestampC2 = kTimestampB2;
  static constexpr uint64_t kTimestampD2 = kNsDistanceForSameDepth + 500;

  orbit_grpc_protos::FullGpuJob expected_gpu_job1 =
      MakeGpuJob(kPid, kTid, kContext1, kSeqno, kTimeline, 0, kTimestampA1, kTimestampB1,
                 kTimestampC1, kTimestampD1);
  orbit_grpc_protos::FullGpuJob expected_gpu_job2 =
      MakeGpuJob(kPid, kTid, kContext2, kSeqno, kTimeline, 0, kTimestampA2, kTimestampB2,
                 kTimestampC2, kTimestampD2);
  orbit_grpc_protos::FullGpuJob actual_gpu_job1;
  orbit_grpc_protos::FullGpuJob actual_gpu_job2;
  EXPECT_CALL(mock_listener_, OnGpuJob)
      .Times(2)
      .WillOnce(::testing::SaveArg<0>(&actual_gpu_job1))
      .WillOnce(::testing::SaveArg<0>(&actual_gpu_job2));

  MakeFakeAmdgpuCsIoctlPerfEvent(kPid, kTid, kTimestampA1, kContext1, kSeqno, kTimeline)
      ->Accept(&visitor_);
  MakeFakeAmdgpuSchedRunJobPerfEvent(kTimestampB1, kContext1, kSeqno, kTimeline)->Accept(&visitor_);
  MakeFakeDmaFenceSignaledPerfEvent(kTimestampD1, kContext1, kSeqno, kTimeline)->Accept(&visitor_);

  MakeFakeAmdgpuCsIoctlPerfEvent(kPid, kTid, kTimestampA2, kContext2, kSeqno, kTimeline)
      ->Accept(&visitor_);
  MakeFakeAmdgpuSchedRunJobPerfEvent(kTimestampB2, kContext2, kSeqno, kTimeline)->Accept(&visitor_);
  MakeFakeDmaFenceSignaledPerfEvent(kTimestampD2, kContext2, kSeqno, kTimeline)->Accept(&visitor_);

  EXPECT_THAT(actual_gpu_job1, GpuJobEq(expected_gpu_job1));
  EXPECT_THAT(actual_gpu_job2, GpuJobEq(expected_gpu_job2));
}

TEST_F(GpuTracepointVisitorTest, TwoNonOverlappingJobsWithSameDepthDifferingBySeqno) {
  static constexpr pid_t kPid = 41;
  static constexpr pid_t kTid = 42;
  static constexpr uint32_t kContext = 1;
  static constexpr uint32_t kSeqno1 = 10;
  static constexpr uint32_t kSeqno2 = 20;
  static const std::string kTimeline = "timeline";
  static constexpr uint64_t kTimestampA1 = 100;
  static constexpr uint64_t kTimestampB1 = 200;
  static constexpr uint64_t kTimestampC1 = kTimestampB1;
  static constexpr uint64_t kTimestampD1 = 300;
  static constexpr uint64_t kNsDistanceForSameDepth = 1'000'000;
  static constexpr uint64_t kTimestampA2 = kNsDistanceForSameDepth + 300;
  static constexpr uint64_t kTimestampB2 = kNsDistanceForSameDepth + 400;
  static constexpr uint64_t kTimestampC2 = kTimestampB2;
  static constexpr uint64_t kTimestampD2 = kNsDistanceForSameDepth + 500;

  orbit_grpc_protos::FullGpuJob expected_gpu_job1 =
      MakeGpuJob(kPid, kTid, kContext, kSeqno1, kTimeline, 0, kTimestampA1, kTimestampB1,
                 kTimestampC1, kTimestampD1);
  orbit_grpc_protos::FullGpuJob expected_gpu_job2 =
      MakeGpuJob(kPid, kTid, kContext, kSeqno2, kTimeline, 0, kTimestampA2, kTimestampB2,
                 kTimestampC2, kTimestampD2);
  orbit_grpc_protos::FullGpuJob actual_gpu_job1;
  orbit_grpc_protos::FullGpuJob actual_gpu_job2;
  EXPECT_CALL(mock_listener_, OnGpuJob)
      .Times(2)
      .WillOnce(::testing::SaveArg<0>(&actual_gpu_job1))
      .WillOnce(::testing::SaveArg<0>(&actual_gpu_job2));

  MakeFakeAmdgpuCsIoctlPerfEvent(kPid, kTid, kTimestampA1, kContext, kSeqno1, kTimeline)
      ->Accept(&visitor_);
  MakeFakeAmdgpuSchedRunJobPerfEvent(kTimestampB1, kContext, kSeqno1, kTimeline)->Accept(&visitor_);
  MakeFakeDmaFenceSignaledPerfEvent(kTimestampD1, kContext, kSeqno1, kTimeline)->Accept(&visitor_);

  MakeFakeAmdgpuCsIoctlPerfEvent(kPid, kTid, kTimestampA2, kContext, kSeqno2, kTimeline)
      ->Accept(&visitor_);
  MakeFakeAmdgpuSchedRunJobPerfEvent(kTimestampB2, kContext, kSeqno2, kTimeline)->Accept(&visitor_);
  MakeFakeDmaFenceSignaledPerfEvent(kTimestampD2, kContext, kSeqno2, kTimeline)->Accept(&visitor_);

  EXPECT_THAT(actual_gpu_job1, GpuJobEq(expected_gpu_job1));
  EXPECT_THAT(actual_gpu_job2, GpuJobEq(expected_gpu_job2));
}

TEST_F(GpuTracepointVisitorTest, TwoOverlappingJobsButOnDifferentTimelines) {
  static constexpr pid_t kPid = 41;
  static constexpr pid_t kTid = 42;
  static constexpr uint32_t kContext = 1;
  static constexpr uint32_t kSeqno = 10;
  static const std::string kTimeline1 = "timeline1";
  static const std::string kTimeline2 = "timeline2";
  static constexpr uint64_t kTimestampA = 100;
  static constexpr uint64_t kTimestampB = 200;
  static constexpr uint64_t kTimestampC = kTimestampB;
  static constexpr uint64_t kTimestampD = 300;

  orbit_grpc_protos::FullGpuJob expected_gpu_job1 =
      MakeGpuJob(kPid, kTid, kContext, kSeqno, kTimeline1, 0, kTimestampA, kTimestampB, kTimestampC,
                 kTimestampD);
  orbit_grpc_protos::FullGpuJob expected_gpu_job2 =
      MakeGpuJob(kPid, kTid, kContext, kSeqno, kTimeline2, 0, kTimestampA, kTimestampB, kTimestampC,
                 kTimestampD);
  orbit_grpc_protos::FullGpuJob actual_gpu_job1;
  orbit_grpc_protos::FullGpuJob actual_gpu_job2;
  EXPECT_CALL(mock_listener_, OnGpuJob)
      .Times(2)
      .WillOnce(::testing::SaveArg<0>(&actual_gpu_job1))
      .WillOnce(::testing::SaveArg<0>(&actual_gpu_job2));

  MakeFakeAmdgpuCsIoctlPerfEvent(kPid, kTid, kTimestampA, kContext, kSeqno, kTimeline1)
      ->Accept(&visitor_);
  MakeFakeAmdgpuSchedRunJobPerfEvent(kTimestampB, kContext, kSeqno, kTimeline1)->Accept(&visitor_);
  MakeFakeDmaFenceSignaledPerfEvent(kTimestampD, kContext, kSeqno, kTimeline1)->Accept(&visitor_);

  MakeFakeAmdgpuCsIoctlPerfEvent(kPid, kTid, kTimestampA, kContext, kSeqno, kTimeline2)
      ->Accept(&visitor_);
  MakeFakeAmdgpuSchedRunJobPerfEvent(kTimestampB, kContext, kSeqno, kTimeline2)->Accept(&visitor_);
  MakeFakeDmaFenceSignaledPerfEvent(kTimestampD, kContext, kSeqno, kTimeline2)->Accept(&visitor_);

  EXPECT_THAT(actual_gpu_job1, GpuJobEq(expected_gpu_job1));
  EXPECT_THAT(actual_gpu_job2, GpuJobEq(expected_gpu_job2));
}

TEST_F(GpuTracepointVisitorTest, TwoNonOverlappingJobsWithDifferentDepthsBecauseOfSlack) {
  static constexpr pid_t kPid = 41;
  static constexpr pid_t kTid = 42;
  static constexpr uint32_t kContext = 1;
  static constexpr uint32_t kSeqno1 = 10;
  static constexpr uint32_t kSeqno2 = 20;
  static const std::string kTimeline = "timeline";
  static constexpr uint64_t kTimestampA1 = 100;
  static constexpr uint64_t kTimestampB1 = 200;
  static constexpr uint64_t kTimestampC1 = kTimestampB1;
  static constexpr uint64_t kTimestampD1 = 300;
  static constexpr uint64_t kTimestampA2 = 400;
  static constexpr uint64_t kTimestampB2 = 500;
  static constexpr uint64_t kTimestampC2 = kTimestampB2;
  static constexpr uint64_t kTimestampD2 = 600;

  orbit_grpc_protos::FullGpuJob expected_gpu_job1 =
      MakeGpuJob(kPid, kTid, kContext, kSeqno1, kTimeline, 0, kTimestampA1, kTimestampB1,
                 kTimestampC1, kTimestampD1);
  orbit_grpc_protos::FullGpuJob expected_gpu_job2 =
      MakeGpuJob(kPid, kTid, kContext, kSeqno2, kTimeline, 1, kTimestampA2, kTimestampB2,
                 kTimestampC2, kTimestampD2);
  orbit_grpc_protos::FullGpuJob actual_gpu_job1;
  orbit_grpc_protos::FullGpuJob actual_gpu_job2;
  EXPECT_CALL(mock_listener_, OnGpuJob)
      .Times(2)
      .WillOnce(::testing::SaveArg<0>(&actual_gpu_job1))
      .WillOnce(::testing::SaveArg<0>(&actual_gpu_job2));

  MakeFakeAmdgpuCsIoctlPerfEvent(kPid, kTid, kTimestampA1, kContext, kSeqno1, kTimeline)
      ->Accept(&visitor_);
  MakeFakeAmdgpuSchedRunJobPerfEvent(kTimestampB1, kContext, kSeqno1, kTimeline)->Accept(&visitor_);
  MakeFakeDmaFenceSignaledPerfEvent(kTimestampD1, kContext, kSeqno1, kTimeline)->Accept(&visitor_);

  MakeFakeAmdgpuCsIoctlPerfEvent(kPid, kTid, kTimestampA2, kContext, kSeqno2, kTimeline)
      ->Accept(&visitor_);
  MakeFakeAmdgpuSchedRunJobPerfEvent(kTimestampB2, kContext, kSeqno2, kTimeline)->Accept(&visitor_);
  MakeFakeDmaFenceSignaledPerfEvent(kTimestampD2, kContext, kSeqno2, kTimeline)->Accept(&visitor_);

  EXPECT_THAT(actual_gpu_job1, GpuJobEq(expected_gpu_job1));
  EXPECT_THAT(actual_gpu_job2, GpuJobEq(expected_gpu_job2));
}

TEST_F(GpuTracepointVisitorTest, TwoOverlappingJobsWithImmediateHwExecution) {
  static constexpr pid_t kPid = 41;
  static constexpr pid_t kTid = 42;
  static constexpr uint32_t kContext = 1;
  static constexpr uint32_t kSeqno1 = 10;
  static constexpr uint32_t kSeqno2 = 20;
  static const std::string kTimeline = "timeline";
  static constexpr uint64_t kTimestampA1 = 100;
  static constexpr uint64_t kTimestampB1 = 200;
  static constexpr uint64_t kTimestampC1 = kTimestampB1;
  static constexpr uint64_t kTimestampD1 = 300;
  static constexpr uint64_t kTimestampA2 = 110;
  static constexpr uint64_t kTimestampB2 = 310;
  static constexpr uint64_t kTimestampC2 = kTimestampB2;
  static constexpr uint64_t kTimestampD2 = 410;

  orbit_grpc_protos::FullGpuJob expected_gpu_job1 =
      MakeGpuJob(kPid, kTid, kContext, kSeqno1, kTimeline, 0, kTimestampA1, kTimestampB1,
                 kTimestampC1, kTimestampD1);
  orbit_grpc_protos::FullGpuJob expected_gpu_job2 =
      MakeGpuJob(kPid, kTid, kContext, kSeqno2, kTimeline, 1, kTimestampA2, kTimestampB2,
                 kTimestampC2, kTimestampD2);
  orbit_grpc_protos::FullGpuJob actual_gpu_job1;
  orbit_grpc_protos::FullGpuJob actual_gpu_job2;
  EXPECT_CALL(mock_listener_, OnGpuJob)
      .Times(2)
      .WillOnce(::testing::SaveArg<0>(&actual_gpu_job1))
      .WillOnce(::testing::SaveArg<0>(&actual_gpu_job2));

  MakeFakeAmdgpuCsIoctlPerfEvent(kPid, kTid, kTimestampA1, kContext, kSeqno1, kTimeline)
      ->Accept(&visitor_);
  MakeFakeAmdgpuSchedRunJobPerfEvent(kTimestampB1, kContext, kSeqno1, kTimeline)->Accept(&visitor_);
  MakeFakeDmaFenceSignaledPerfEvent(kTimestampD1, kContext, kSeqno1, kTimeline)->Accept(&visitor_);

  MakeFakeAmdgpuCsIoctlPerfEvent(kPid, kTid, kTimestampA2, kContext, kSeqno2, kTimeline)
      ->Accept(&visitor_);
  MakeFakeAmdgpuSchedRunJobPerfEvent(kTimestampB2, kContext, kSeqno2, kTimeline)->Accept(&visitor_);
  MakeFakeDmaFenceSignaledPerfEvent(kTimestampD2, kContext, kSeqno2, kTimeline)->Accept(&visitor_);

  EXPECT_THAT(actual_gpu_job1, GpuJobEq(expected_gpu_job1));
  EXPECT_THAT(actual_gpu_job2, GpuJobEq(expected_gpu_job2));
}

TEST_F(GpuTracepointVisitorTest, TwoOverlappingJobsWithDelayedHwExecution) {
  static constexpr pid_t kPid = 41;
  static constexpr pid_t kTid = 42;
  static constexpr uint32_t kContext = 1;
  static constexpr uint32_t kSeqno1 = 10;
  static constexpr uint32_t kSeqno2 = 20;
  static const std::string kTimeline = "timeline";
  static constexpr uint64_t kTimestampA1 = 100;
  static constexpr uint64_t kTimestampB1 = 200;
  static constexpr uint64_t kTimestampC1 = kTimestampB1;
  static constexpr uint64_t kTimestampD1 = 300;
  static constexpr uint64_t kTimestampA2 = 110;
  static constexpr uint64_t kTimestampB2 = 210;
  static constexpr uint64_t kTimestampC2 = kTimestampD1;
  static constexpr uint64_t kTimestampD2 = 400;

  orbit_grpc_protos::FullGpuJob expected_gpu_job1 =
      MakeGpuJob(kPid, kTid, kContext, kSeqno1, kTimeline, 0, kTimestampA1, kTimestampB1,
                 kTimestampC1, kTimestampD1);
  orbit_grpc_protos::FullGpuJob expected_gpu_job2 =
      MakeGpuJob(kPid, kTid, kContext, kSeqno2, kTimeline, 1, kTimestampA2, kTimestampB2,
                 kTimestampC2, kTimestampD2);
  orbit_grpc_protos::FullGpuJob actual_gpu_job1;
  orbit_grpc_protos::FullGpuJob actual_gpu_job2;
  EXPECT_CALL(mock_listener_, OnGpuJob)
      .Times(2)
      .WillOnce(::testing::SaveArg<0>(&actual_gpu_job1))
      .WillOnce(::testing::SaveArg<0>(&actual_gpu_job2));

  MakeFakeAmdgpuCsIoctlPerfEvent(kPid, kTid, kTimestampA1, kContext, kSeqno1, kTimeline)
      ->Accept(&visitor_);
  MakeFakeAmdgpuSchedRunJobPerfEvent(kTimestampB1, kContext, kSeqno1, kTimeline)->Accept(&visitor_);
  MakeFakeDmaFenceSignaledPerfEvent(kTimestampD1, kContext, kSeqno1, kTimeline)->Accept(&visitor_);

  MakeFakeAmdgpuCsIoctlPerfEvent(kPid, kTid, kTimestampA2, kContext, kSeqno2, kTimeline)
      ->Accept(&visitor_);
  MakeFakeAmdgpuSchedRunJobPerfEvent(kTimestampB2, kContext, kSeqno2, kTimeline)->Accept(&visitor_);
  MakeFakeDmaFenceSignaledPerfEvent(kTimestampD2, kContext, kSeqno2, kTimeline)->Accept(&visitor_);

  EXPECT_THAT(actual_gpu_job1, GpuJobEq(expected_gpu_job1));
  EXPECT_THAT(actual_gpu_job2, GpuJobEq(expected_gpu_job2));
}

TEST_F(GpuTracepointVisitorTest,
       TwoNonOverlappingJobsWithWrongDepthsAndHardwareStartsBecauseReceivedOutOfOrder) {
  static constexpr pid_t kPid = 41;
  static constexpr pid_t kTid = 42;
  static constexpr uint32_t kContext = 1;
  static constexpr uint32_t kSeqno1 = 10;
  static constexpr uint32_t kSeqno2 = 20;
  static const std::string kTimeline = "timeline";
  static constexpr uint64_t kTimestampA1 = 100;
  static constexpr uint64_t kTimestampB1 = 200;
  static constexpr uint64_t kTimestampD1 = 300;
  static constexpr uint64_t kNsDistanceForSameDepth = 1'000'000;
  static constexpr uint64_t kTimestampA2 = kNsDistanceForSameDepth + 300;
  static constexpr uint64_t kTimestampB2 = kNsDistanceForSameDepth + 400;
  static constexpr uint64_t kTimestampC2 = kTimestampB2;
  static constexpr uint64_t kTimestampD2 = kNsDistanceForSameDepth + 500;
  // This is the timestamp that ends up being wrong when the assumption that "dma_fence_signaled"
  // events are processed reasonably in order doesn't hold.
  static constexpr uint64_t kTimestampC1 = kTimestampD2;

  orbit_grpc_protos::FullGpuJob expected_gpu_job1 =
      MakeGpuJob(kPid, kTid, kContext, kSeqno1, kTimeline, 1, kTimestampA1, kTimestampB1,
                 kTimestampC1, kTimestampD1);
  orbit_grpc_protos::FullGpuJob expected_gpu_job2 =
      MakeGpuJob(kPid, kTid, kContext, kSeqno2, kTimeline, 0, kTimestampA2, kTimestampB2,
                 kTimestampC2, kTimestampD2);
  orbit_grpc_protos::FullGpuJob actual_gpu_job1;
  orbit_grpc_protos::FullGpuJob actual_gpu_job2;
  EXPECT_CALL(mock_listener_, OnGpuJob)
      .Times(2)
      // Save actual_gpu_job2 first as it's created first (its last PerfEvent is processed first).
      .WillOnce(::testing::SaveArg<0>(&actual_gpu_job2))
      .WillOnce(::testing::SaveArg<0>(&actual_gpu_job1));

  MakeFakeAmdgpuCsIoctlPerfEvent(kPid, kTid, kTimestampA1, kContext, kSeqno1, kTimeline)
      ->Accept(&visitor_);
  MakeFakeAmdgpuSchedRunJobPerfEvent(kTimestampB1, kContext, kSeqno1, kTimeline)->Accept(&visitor_);
  MakeFakeAmdgpuCsIoctlPerfEvent(kPid, kTid, kTimestampA2, kContext, kSeqno2, kTimeline)
      ->Accept(&visitor_);
  MakeFakeAmdgpuSchedRunJobPerfEvent(kTimestampB2, kContext, kSeqno2, kTimeline)->Accept(&visitor_);
  MakeFakeDmaFenceSignaledPerfEvent(kTimestampD2, kContext, kSeqno2, kTimeline)->Accept(&visitor_);
  MakeFakeDmaFenceSignaledPerfEvent(kTimestampD1, kContext, kSeqno1, kTimeline)->Accept(&visitor_);

  EXPECT_THAT(actual_gpu_job1, GpuJobEq(expected_gpu_job1));
  EXPECT_THAT(actual_gpu_job2, GpuJobEq(expected_gpu_job2));
}

}  // namespace orbit_linux_tracing
