// Copyright (c) 2021 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USER_SPACE_INSTRUMENTATION_REGISTER_STATE_H_
#define USER_SPACE_INSTRUMENTATION_REGISTER_STATE_H_

#include <stdint.h>

#include <array>
#include <vector>

#include "OrbitBase/Result.h"

namespace orbit_user_space_instrumentation {

// The structs below are defined to match the memory layout of the binary blobs containing the
// register state of the cpu (as returned by the kernel). They should not used directly - see
// comment on class "RegisterState" below.
#define PACKED_START _Pragma("pack(push, 1)")
#define PACKED_END _Pragma("pack(pop)")

PACKED_START
struct GeneralPurposeRegisters32 {
  uint32_t ebx;
  uint32_t ecx;
  uint32_t edx;
  uint32_t esi;
  uint32_t edi;
  uint32_t ebp;
  uint32_t eax;
  uint32_t xds;
  uint32_t xes;
  uint32_t xfs;
  uint32_t xgs;
  uint32_t orig_eax;
  uint32_t eip;
  uint32_t xcs;
  uint32_t eflags;
  uint32_t esp;
  uint32_t xss;
};

struct GeneralPurposeRegisters64 {
  uint64_t r15;
  uint64_t r14;
  uint64_t r13;
  uint64_t r12;
  uint64_t rbp;
  uint64_t rbx;
  uint64_t r11;
  uint64_t r10;
  uint64_t r9;
  uint64_t r8;
  uint64_t rax;
  uint64_t rcx;
  uint64_t rdx;
  uint64_t rsi;
  uint64_t rdi;
  uint64_t orig_rax;
  uint64_t rip;
  uint64_t cs;
  uint64_t rflags;
  uint64_t rsp;
  uint64_t ss;
  uint64_t fs_base;
  uint64_t gs_base;
  uint64_t ds;
  uint64_t es;
  uint64_t fs;
  uint64_t gs;
};

union GeneralPurposeRegisters {
  GeneralPurposeRegisters32 x86_32;
  GeneralPurposeRegisters64 x86_64;
};

struct MmsAs80BitFloat {
  uint64_t mantissa;
  uint16_t sign_exp;
};
static_assert(sizeof(MmsAs80BitFloat) == 10, "MmsAsFloat is not 10 bytes of size");

struct MmsRegister {
  union {
    std::array<uint8_t, 10> bytes;
    MmsAs80BitFloat as_float;
  };
  std::array<uint8_t, 6> reserved;
};
static_assert(sizeof(MmsRegister) == 16, "MmsRegister is not 16 bytes of size");

struct XmmRegister {
  uint8_t bytes[16];
};

// Legacy Region of the XSave area stores the Fpu, Mmx, Sse state of the Cpu.
// Compare "Intel 64 and IA-32 Architectures Software Developer’s Manual, Volume 1" section 13.4.1.
struct FxSave {
  uint16_t fcw;
  uint16_t fsw;
  uint16_t ftw;
  uint16_t fop;
  uint64_t fip;
  uint64_t fdp;
  uint32_t mxcsr;
  uint32_t mxcsr_mask;
  std::array<MmsRegister, 8> stmm;
  std::array<XmmRegister, 16> xmm;
  std::array<uint8_t, 48> padding1;
  uint64_t xcr0;
  std::array<uint8_t, 40> padding2;
};
static_assert(sizeof(FxSave) == 512, "FxSave is not 512 bytes of size");

// XSave header contains information about what is present in the extended region of an XSAVE area.
// Compare "Intel 64 and IA-32 Architectures Software Developer’s Manual, Volume 1" section 13.4.2.
// and 13.4.3..
struct XSaveHeader {
  enum class StateComponents : uint64_t {
    kX87 = 1,
    kSse = 2,
    kAvx = 4,
    kBndRegs = 8,
    kBndCsr = 16,
    kOpMask = 32,
    kZmmHi256 = 64,
    kHi16Zmm = 128,
    kPt = 256,
    kPkru = 512,
  };
  uint64_t xstate_bv;
  uint64_t xcomp_bv;
  std::array<uint64_t, 6> reserved;
};
static_assert(sizeof(XSaveHeader) == 64, "XSaveHeader layout incorrect");

// The upper 128 bit of a single YMMx register.
struct YmmHiRegister {
  std::array<uint8_t, 16> bytes;
};

// The upper 128 bit of the YMM0..15 registers. The lower bits are shared with XMM registers above.
struct YmmHi {
  std::array<YmmHiRegister, 16> ymm;
};
PACKED_END

// Backup, modify and restore register state of a halted thread.
//
// Requires the XSave extension (added around 2008) to be supported by the Cpu otherwise
// "BackupRegisters" will return an error. RegisterState requires to be initialized by a call to
// "BackupRegisters" before anything else can be called on the object. RegisterState stores the
// general purpose registers as well as all floating point and vector registers.
//
// Access to the stored data is provided by somewhat forcefully casting the memory blocks returned
// by the kernel to the structs above.
//
// For the general purpose registers this is somewhat straightforward with the only complication
// of bitness of the process. Users of this class should query the bitness of the process using
// "GetBitness" and only access the respective member of the union returned by
// "GetGeneralPurposeRegisters".
//
// For fpu / vector data the situation is slightly more complicated. A comprehensive write-up can be
// found in [1]. A good, readable introduction can be found here: [2].
// Different registers may or may not be present on a specific Cpu (e.g. the Cpu may or may not
// support Avx). Even if the Cpu has the respective registers they might not have been saved by
// "BackupRegisters" since they are in their initial state. Hence every meaningful access to this
// data needs to call the "Has*DataStored" functions first.
//
// The fpu, mmx, sse registers can be accessed via the "GetFxSave" call using the structs defined
// above. Note that the fpu and the mmx registers share the same memory since their usage is
// mutually exclusive. The upper half of the avx registers can be accessed via "GetAvxHiRegisters"
// (low half is shared with sse registers).
//
// There are more additional registers that get backed up and restored by this class (if the cpu
// supports them and backup is necessary) but no accessors are defined to alter that data (e.g.
// Avx-512, Mpx registers, ...). It should be straightforward to add support for these if that
// should become necessary. Details again can be found in [1].
//
// [1]: "Intel 64 and IA-32 Architectures Software Developer’s Manual, Volume 1", section 13
// [2]: https://www.moritz.systems/blog/how-debuggers-work-getting-and-setting-x86-registers-part-2/
//
// Example usage:
//
//    // Wait for some thread to be halted.
//    waitpid(pid, &status, 0);
//
//    // Alter Avx state if applicable.
//    RegisterState s;
//    auto result_backup = s.BackupRegisters(pid);
//    if (s.HasAvxDataStored()){
//      s.GetAvxHiRegisters()->ymm[0].bytes[0] = 42;
//    }
//    auto result_restore = s.RestoreRegisters();
//
//    // Continue thread with altered state.
//    ptrace(PT_CONTINUE, pid, 1, 0);
class RegisterState {
 public:
  [[nodiscard]] ErrorMessageOr<void> BackupRegisters(pid_t tid);
  [[nodiscard]] ErrorMessageOr<void> RestoreRegisters();

  enum class Bitness {
    k32Bit,
    k64Bit,
  };
  Bitness GetBitness() const { return bitness_; }

  // Returns a pointer to a union of 32 and 64 bit general purpose registers. Call "GetBitness" to
  // determine which member of the union is valid.
  GeneralPurposeRegisters* GetGeneralPurposeRegisters() { return &general_purpose_registers_; }

  // Some registers do not get stored in RegisterState. The Cpu might not support them or they might
  // be in their initial state. So before accessing this data one needs to call the "Has*DataStored"
  // methods first.
  bool Hasx87DataStored();
  bool HasSseDataStored();
  bool HasAvxDataStored();

  // Structure access to the different parts of the XSave area. Detail, again, can be found at
  // "Intel 64 and IA-32 Architectures Software Developer’s Manual, Volume 1", section 13.
  // "GetFxSave" can be used to access fpu, mmx, sse registers. "GetAvxHiRegisters" give access to
  // the upper half of the avx registers (lower half in store in the sse registers).
  FxSave* GetFxSave() { return reinterpret_cast<FxSave*>(xsave_area_.data()); }
  XSaveHeader* GetXSaveHeader() { return reinterpret_cast<XSaveHeader*>(xsave_area_.data() + 512); }
  YmmHi* GetAvxHiRegisters() { return reinterpret_cast<YmmHi*>(xsave_area_.data() + avx_offset_); }

 private:
  pid_t tid_ = -1;
  GeneralPurposeRegisters general_purpose_registers_;
  std::vector<uint8_t> xsave_area_;
  Bitness bitness_;
  size_t avx_offset_ = 0;
};

}  // namespace orbit_user_space_instrumentation

#endif