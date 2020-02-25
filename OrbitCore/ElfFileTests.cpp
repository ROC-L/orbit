#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <utility>

#include "ElfFile.h"
#include "Path.h"

TEST(ElfFile, GetFunctions) {
  std::string executable_path = Path::GetExecutablePath();
  std::string test_elf_file = executable_path + "/testdata/hello_world_elf";

  auto elf_file = ElfFile::Create(test_elf_file.c_str());
  ASSERT_NE(elf_file, nullptr);

  Pdb pdb;
  std::vector<Function> functions;
  ASSERT_TRUE(elf_file->GetFunctions(&pdb, &functions));
  EXPECT_EQ(functions.size(), 10);
  const Function* function = &functions[0];

  EXPECT_EQ(function->Name(), "deregister_tm_clones");
  EXPECT_EQ(function->PrettyName(), "deregister_tm_clones");
  EXPECT_EQ(function->Address(), 0x1080);
  EXPECT_EQ(function->Size(), 0);
  EXPECT_EQ(function->GetPdb(), &pdb);

  function = &functions[9];
  EXPECT_EQ(function->Name(), "main");
  EXPECT_EQ(function->PrettyName(), "main");
  EXPECT_EQ(function->Address(), 0x1135);
  EXPECT_EQ(function->Size(), 35);
  EXPECT_EQ(function->GetPdb(), &pdb);
}

TEST(ElfFile, IsAddressInTextSection) {
  std::string executable_path = Path::GetExecutablePath();
  std::string test_elf_file = executable_path + "/testdata/hello_world_elf";

  auto elf_file = ElfFile::Create(test_elf_file.c_str());
  ASSERT_NE(elf_file, nullptr);

  EXPECT_FALSE(elf_file->IsAddressInTextSection(0x104F));
  EXPECT_TRUE(elf_file->IsAddressInTextSection(0x1050));
  EXPECT_TRUE(elf_file->IsAddressInTextSection(0x11C0));
  EXPECT_FALSE(elf_file->IsAddressInTextSection(0x11C1));
}

TEST(ElfFile, CalculateLoadBias) {
  const std::string executable_path = Path::GetExecutablePath();
  const std::string test_elf_file_dynamic =
      executable_path + "/testdata/hello_world_elf";

  auto elf_file_dynamic = ElfFile::Create(test_elf_file_dynamic.c_str());
  ASSERT_NE(elf_file_dynamic, nullptr);
  EXPECT_EQ(elf_file_dynamic->GetLoadBias(), 0x0);

  const std::string test_elf_file_static =
      executable_path + "/testdata/hello_world_static_elf";
  auto elf_file_static = ElfFile::Create(test_elf_file_static.c_str());
  ASSERT_NE(elf_file_static, nullptr);
  EXPECT_EQ(elf_file_static->GetLoadBias(), 0x400000);
}

TEST(ElfFile, CalculateLoadBiasNoProgramHeaders) {
  const std::string executable_path = Path::GetExecutablePath();
  const std::string test_elf_file =
      executable_path + "/testdata/hello_world_elf_no_program_headers";
  auto elf_file = ElfFile::Create(test_elf_file.c_str());

  ASSERT_NE(elf_file, nullptr);
  EXPECT_TRUE(!elf_file->GetLoadBias().has_value());
}
