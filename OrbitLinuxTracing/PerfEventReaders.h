#ifndef ORBIT_LINUX_TRACING_PERF_EVENT_READERS_H_
#define ORBIT_LINUX_TRACING_PERF_EVENT_READERS_H_

#include "PerfEvent.h"
#include "PerfEventRingBuffer.h"

namespace LinuxTracing {

// Helper functions for reads from a perf_event_open ring buffer that require
// more complex operations than simply copying an entire perf_event_open record.

inline pid_t ReadMmapRecordPid(PerfEventRingBuffer* ring_buffer) {
  // Mmap records have the following layout:
  // struct {
  //   struct perf_event_header header;
  //   u32    pid, tid;
  //   u64    addr;
  //   u64    len;
  //   u64    pgoff;
  //   char   filename[];
  //   struct sample_id sample_id; /* if sample_id_all */
  // };
  // Because of filename, the layout is not fixed.

  pid_t pid;
  ring_buffer->ReadValueAtOffset(&pid, sizeof(perf_event_header));
  return pid;
}

inline pid_t ReadSampleRecordPid(PerfEventRingBuffer* ring_buffer) {
  pid_t pid;
  ring_buffer->ReadValueAtOffset(
      &pid, offsetof(perf_event_stack_sample, sample_id.pid));
  return pid;
}

inline pid_t ReadUretprobesRecordPid(PerfEventRingBuffer* ring_buffer) {
  pid_t pid;
  ring_buffer->ReadValueAtOffset(
      &pid, offsetof(perf_event_empty_sample, sample_id.pid));
  return pid;
}

uint16_t ReadTracepointCommonType(PerfEventRingBuffer* ring_buffer);

template <typename SamplePerfEventT>
inline std::unique_ptr<SamplePerfEventT> ConsumeSamplePerfEvent(
    PerfEventRingBuffer* ring_buffer, const perf_event_header& header) {
  // Data in the ring buffer has the layout of perf_event_stack_sample, but we
  // copy it into dynamically_sized_perf_event_stack_sample.
  uint64_t dyn_size;
  ring_buffer->ReadValueAtOffset(
      &dyn_size, offsetof(perf_event_stack_sample, stack.dyn_size));
  auto event = std::make_unique<SamplePerfEventT>(dyn_size);
  event->ring_buffer_record->header = header;
  ring_buffer->ReadValueAtOffset(&event->ring_buffer_record->sample_id,
                                 offsetof(perf_event_stack_sample, sample_id));
  ring_buffer->ReadValueAtOffset(&event->ring_buffer_record->regs,
                                 offsetof(perf_event_stack_sample, regs));
  ring_buffer->ReadRawAtOffset(event->ring_buffer_record->stack.data.get(),
                               offsetof(perf_event_stack_sample, stack.data),
                               dyn_size);
  ring_buffer->SkipRecord(header);
  return event;
}

}  // namespace LinuxTracing

#endif  // ORBIT_LINUX_TRACING_PERF_EVENT_READERS_H_
