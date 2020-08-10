// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "Disassembler.h"

#include <capstone/capstone.h>
#include <capstone/platform.h>

#define LOGF(format, ...)                                   \
  {                                                         \
    std::string log = absl::StrFormat(format, __VA_ARGS__); \
    result_ += log;                                         \
  }

#define LOG(str) \
  { result_ += str; }

void Disassembler::LogHex(const uint8_t* str, size_t len) {
  const unsigned char* c;

  LOGF("%s", "Code: ");
  for (c = str; c < str + len; c++) {
    LOGF("0x%02x ", *c & 0xff);
  }
  LOGF("%s", "\n");
}

void Disassembler::Disassemble(const uint8_t* machine_code, size_t size,
                               uint64_t address, bool is_64bit) {
  csh handle = 0;
  cs_arch arch = CS_ARCH_X86;
  cs_insn* insn = nullptr;
  size_t count = 0;
  cs_err err;
  cs_mode mode = is_64bit ? CS_MODE_64 : CS_MODE_32;

  LOG("\n");
  line_to_address_.push_back(0);
  LOGF("Platform: %s\n",
       is_64bit ? "X86 64 (Intel syntax)" : "X86 32 (Intel syntax)");
  line_to_address_.push_back(0);
  err = cs_open(arch, mode, &handle);
  if (err) {
    LOGF("Failed on cs_open() with error returned: %u\n", err);
    return;
  }

  count = cs_disasm(handle, machine_code, size, address, 0, &insn);

  if (count) {
    size_t j;

    for (j = 0; j < count; j++) {
      LOGF("0x%" PRIx64 ":\t%-12s %s\n", insn[j].address, insn[j].mnemonic,
           insn[j].op_str);
      line_to_address_.push_back(insn[j].address);
    }

    // print out the next offset, after the last insn
    LOGF("0x%" PRIx64 ":\n", insn[j - 1].address + insn[j - 1].size);

    // free memory allocated by cs_disasm()
    cs_free(insn, count);
  } else {
    LOG("****************\n");
    LOG("ERROR: Failed to disasm given code!\n");
  }

  LOG("\n");

  cs_close(&handle);
}

uint64_t Disassembler::GetAddressAtLine(size_t line) const {
  if (line >= line_to_address_.size()) return 0;
  return line_to_address_[line];
}