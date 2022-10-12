// Copyright (c) 2021 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <absl/base/casts.h>
#include <gmock/gmock.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <memory>

#include "CaptureFile/CaptureFile.h"
#include "CaptureFile/CaptureFileOutputStream.h"
#include "CaptureFile/CaptureFileSection.h"
#include "GrpcProtos/capture.pb.h"
#include "OrbitBase/File.h"
#include "OrbitBase/MakeUniqueForOverwrite.h"
#include "OrbitBase/Result.h"
#include "OrbitBase/TemporaryFile.h"
#include "TestUtils/TestUtils.h"

namespace orbit_capture_file {

using orbit_base::TemporaryFile;
using orbit_test_utils::HasError;
using orbit_test_utils::HasNoError;
using orbit_test_utils::HasValue;

static constexpr const char* kAnswerString =
    "Answer to the Ultimate Question of Life, The Universe, and Everything";
static constexpr const char* kNotAnAnswerString = "Some odd number, not the answer.";
static constexpr uint64_t kAnswerKey = 42;
static constexpr uint64_t kNotAnAnswerKey = 43;

using orbit_grpc_protos::ClientCaptureEvent;

static ClientCaptureEvent CreateInternedStringCaptureEvent(uint64_t key, const std::string& str) {
  ClientCaptureEvent event;
  orbit_grpc_protos::InternedString* interned_string = event.mutable_interned_string();
  interned_string->set_key(key);
  interned_string->set_intern(str);
  return event;
}

class CaptureFileHeaderTest : public testing::Test {
 public:
  void SetUp() override {
    // ASSERT_THAT cannot be used in the constructor, hence this work is done in SetUp.
    auto tmp_file_or_error = TemporaryFile::Create();
    ASSERT_THAT(tmp_file_or_error, HasNoError());
    temporary_file_ = std::make_unique<TemporaryFile>(std::move(tmp_file_or_error.value()));
  }

 protected:
  std::unique_ptr<TemporaryFile> temporary_file_;
};

class CaptureFileTest : public CaptureFileHeaderTest {
 public:
  explicit CaptureFileTest()
      : test_event_1(CreateInternedStringCaptureEvent(kAnswerKey, kAnswerString)),
        test_event_2(CreateInternedStringCaptureEvent(kNotAnAnswerKey, kNotAnAnswerString)) {}

  void SetUp() override {
    CaptureFileHeaderTest::SetUp();
    temporary_file_->CloseAndRemove();

    AddCaptureSection();
    OpenTemporayFileAsCaptureFile();
  }

  void AddCaptureSection() {
    auto output_stream_or_error =
        CaptureFileOutputStream::Create(temporary_file_->file_path().string());
    ASSERT_THAT(output_stream_or_error, HasNoError());
    auto output_stream = std::move(output_stream_or_error.value());

    ASSERT_TRUE(output_stream->IsOpen());

    ASSERT_THAT(output_stream->WriteCaptureEvent(test_event_1), HasNoError());
    ASSERT_THAT(output_stream->WriteCaptureEvent(test_event_2), HasNoError());
    ASSERT_THAT(output_stream->Close(), HasNoError());
  }

  static void VerifyEventEquals(const ClientCaptureEvent& lhs, const ClientCaptureEvent& rhs) {
    ASSERT_EQ(lhs.event_case(), ClientCaptureEvent::kInternedString);
    ASSERT_EQ(rhs.event_case(), ClientCaptureEvent::kInternedString);
    EXPECT_EQ(lhs.interned_string().key(), rhs.interned_string().key());
    EXPECT_EQ(lhs.interned_string().intern(), rhs.interned_string().intern());
  }

  void VerifyCaptureSectionContent(
      const std::unique_ptr<ProtoSectionInputStream>& capture_section) {
    {
      ClientCaptureEvent event;
      ASSERT_THAT(capture_section->ReadMessage(&event), HasNoError());
      VerifyEventEquals(event, test_event_1);
    }

    {
      ClientCaptureEvent event;
      ASSERT_THAT(capture_section->ReadMessage(&event), HasNoError());
      VerifyEventEquals(event, test_event_2);
    }
  }

  void OpenTemporayFileAsCaptureFile() {
    auto capture_file_or_error = CaptureFile::OpenForReadWrite(temporary_file_->file_path());
    ASSERT_THAT(capture_file_or_error, HasNoError());
    capture_file_ = std::move(capture_file_or_error.value());
  }

  void VerifySectionIsReadable(uint64_t section_index, size_t section_size) {
    EXPECT_EQ(capture_file_->GetSectionList()[section_index].size, section_size);
    auto data = make_unique_for_overwrite<uint8_t[]>(section_size);
    const ErrorMessageOr<void> read_result =
        capture_file_->ReadFromSection(section_index, 0, data.get(), section_size);
    EXPECT_THAT(read_result, HasNoError());
  }

  void VerifySingleUserDataSectionExistsAtEnd(size_t user_data_section_size) {
    ASSERT_GE(capture_file_->GetSectionList().size(), 1);

    const uint64_t last_section_index = capture_file_->GetSectionList().size() - 1;

    ASSERT_EQ(capture_file_->FindAllSectionsByType(kSectionTypeUserData).size(), 1);
    EXPECT_EQ(capture_file_->FindAllSectionsByType(kSectionTypeUserData).front(),
              last_section_index);

    VerifySectionIsReadable(last_section_index, user_data_section_size);
  }

 protected:
  std::unique_ptr<CaptureFile> capture_file_;

 private:
  const ClientCaptureEvent test_event_1;
  const ClientCaptureEvent test_event_2;
};

TEST_F(CaptureFileTest, CreateCaptureFileAndReadMainSection) {
  auto capture_section = capture_file_->CreateCaptureSectionInputStream();

  VerifyCaptureSectionContent(capture_section);
}

TEST_F(CaptureFileTest, CreateCaptureFileWriteAdditionalSectionAndReadMainSection) {
  constexpr size_t kUserDataSectionSize = 333;
  auto section_number_or_error = capture_file_->AddUserDataSection(kUserDataSectionSize);
  ASSERT_THAT(section_number_or_error, HasValue(0));
  ASSERT_EQ(capture_file_->GetSectionList().size(), 1);

  VerifySingleUserDataSectionExistsAtEnd(kUserDataSectionSize);

  // Reopen the file to make sure this information was saved
  OpenTemporayFileAsCaptureFile();

  VerifySingleUserDataSectionExistsAtEnd(kUserDataSectionSize);

  auto capture_section = capture_file_->CreateCaptureSectionInputStream();

  VerifyCaptureSectionContent(capture_section);

  // Read beyond the last message to see if we are just reading zeros as
  // empty messages and then correctly return an end of section error.
  // We should not accidentally read into the next section or the section
  // list. Since section alignment is 8, we can expect at most 7 empty
  // messages, and on the 8th time we go through the loop, we must
  // encounter the end of the section.
  constexpr int kSectionAlignment = 8;
  for (int i = 0; i < kSectionAlignment; ++i) {
    ClientCaptureEvent event;
    ErrorMessageOr<void> error = capture_section->ReadMessage(&event);
    if (error.has_error()) {
      ASSERT_THAT(error, HasError("Unexpected end of section"));
      return;
    }
    ASSERT_EQ(0, event.ByteSizeLong());
  }
  FAIL() << "More empty messages at end of section than expected.";
}

TEST_F(CaptureFileTest, CreateCaptureFileAndAddUserDataSection) {
  EXPECT_EQ(capture_file_->GetSectionList().size(), 0);

  uint64_t buf_size;
  {
    ClientCaptureEvent event;
    event.mutable_capture_finished()->set_status(orbit_grpc_protos::CaptureFinished::kFailed);
    event.mutable_capture_finished()->set_error_message("some error");

    buf_size = event.ByteSizeLong() +
               google::protobuf::io::CodedOutputStream::VarintSize32(event.ByteSizeLong());
    auto buf = make_unique_for_overwrite<uint8_t[]>(buf_size);
    google::protobuf::io::ArrayOutputStream array_output_stream(buf.get(), buf_size);
    google::protobuf::io::CodedOutputStream coded_output_stream(&array_output_stream);
    coded_output_stream.WriteVarint32(event.ByteSizeLong());
    ASSERT_TRUE(event.SerializeToCodedStream(&coded_output_stream));
    auto section_number_or_error = capture_file_->AddUserDataSection(buf_size);
    ASSERT_THAT(section_number_or_error, HasValue(0));
    ASSERT_EQ(capture_file_->GetSectionList().size(), 1);

    VerifySingleUserDataSectionExistsAtEnd(buf_size);

    // Write something to the section
    const std::string something{"something"};
    constexpr uint64_t kOffsetInSection = 5;
    auto write_to_section_result =
        capture_file_->WriteToSection(0, kOffsetInSection, something.c_str(), something.size());
    ASSERT_THAT(write_to_section_result, HasNoError());

    {
      std::string content;
      content.resize(something.size());
      auto read_result =
          capture_file_->ReadFromSection(0, kOffsetInSection, content.data(), something.size());
      ASSERT_THAT(read_result, HasNoError());
      EXPECT_EQ(content, something);
    }

    ASSERT_THAT(
        capture_file_->WriteToSection(section_number_or_error.value(), 0, buf.get(), buf_size),
        HasNoError());
  }

  {
    const auto& capture_file_section = capture_file_->GetSectionList()[0];
    EXPECT_EQ(capture_file_section.size, buf_size);
    EXPECT_GT(capture_file_section.offset, 0);
    EXPECT_EQ(capture_file_section.type, kSectionTypeUserData);
  }

  VerifySingleUserDataSectionExistsAtEnd(buf_size);

  // Reopen the file to make sure this information was saved
  OpenTemporayFileAsCaptureFile();

  EXPECT_EQ(capture_file_->GetSectionList().size(), 1);
  {
    const auto& capture_file_section = capture_file_->GetSectionList()[0];
    EXPECT_EQ(capture_file_section.type, kSectionTypeUserData);
    EXPECT_GT(capture_file_section.offset, 0);
    EXPECT_EQ(capture_file_section.size, buf_size);
  }

  VerifySingleUserDataSectionExistsAtEnd(buf_size);

  {
    auto section_input_stream = capture_file_->CreateProtoSectionInputStream(0);
    ASSERT_NE(section_input_stream.get(), nullptr);
    ClientCaptureEvent event_from_file;
    ASSERT_THAT(section_input_stream->ReadMessage(&event_from_file), HasNoError());
    EXPECT_EQ(event_from_file.capture_finished().status(),
              orbit_grpc_protos::CaptureFinished::kFailed);
    EXPECT_EQ(event_from_file.capture_finished().error_message(), "some error");
  }
}

TEST_F(CaptureFileHeaderTest, OpenCaptureFileInvalidSignature) {
  auto write_result =
      orbit_base::WriteFully(temporary_file_->fd(), "This is not an Orbit Capture File");

  auto capture_file_or_error = CaptureFile::OpenForReadWrite(temporary_file_->file_path());
  EXPECT_THAT(capture_file_or_error, HasError("Invalid file signature"));
}

TEST_F(CaptureFileHeaderTest, OpenCaptureFileTooSmall) {
  auto write_result = orbit_base::WriteFully(temporary_file_->fd(), "ups");

  auto capture_file_or_error = CaptureFile::OpenForReadWrite(temporary_file_->file_path());
  EXPECT_THAT(capture_file_or_error, HasError("Failed to read the file signature"));
}

static std::string CreateHeader(uint32_t version, uint64_t capture_section_offset,
                                uint64_t section_list_offset) {
  std::string header{"ORBT", 4};
  header.append(std::string_view{absl::bit_cast<const char*>(&version), sizeof(version)});
  header.append(std::string_view{absl::bit_cast<const char*>(&capture_section_offset),
                                 sizeof(capture_section_offset)});
  header.append(std::string_view{absl::bit_cast<const char*>(&section_list_offset),
                                 sizeof(section_list_offset)});

  return header;
}

TEST_F(CaptureFileHeaderTest, OpenCaptureFileInvalidVersion) {
  std::string header = CreateHeader(0, 0, 0);

  auto write_result = orbit_base::WriteFully(temporary_file_->fd(), header);

  auto capture_file_or_error = CaptureFile::OpenForReadWrite(temporary_file_->file_path());
  EXPECT_THAT(capture_file_or_error, HasError("Incompatible version 0, expected 1"));
}

TEST_F(CaptureFileHeaderTest, OpenCaptureFileInvalidSectionListSize) {
  std::string header = CreateHeader(1, 24, 32);
  header.append(std::string_view{"12345678", 8});
  constexpr uint64_t kSectionListSize = 10;
  header.append(
      std::string_view{absl::bit_cast<const char*>(&kSectionListSize), sizeof(kSectionListSize)});

  auto write_result = orbit_base::WriteFully(temporary_file_->fd(), header);

  auto capture_file_or_error = CaptureFile::OpenForReadWrite(temporary_file_->file_path());
  EXPECT_THAT(capture_file_or_error, HasError("Unexpected EOF while reading section list"));
}

TEST_F(CaptureFileHeaderTest, OpenCaptureFileInvalidSectionListSizeTooLarge) {
  std::string header = CreateHeader(1, 24, 32);
  header.append(std::string_view{"12345678", 8});
  constexpr uint64_t kSectionListSize = 65'536;
  header.append(
      std::string_view{absl::bit_cast<const char*>(&kSectionListSize), sizeof(kSectionListSize)});

  auto write_result = orbit_base::WriteFully(temporary_file_->fd(), header);

  auto capture_file_or_error = CaptureFile::OpenForReadWrite(temporary_file_->file_path());
  EXPECT_THAT(capture_file_or_error, HasError("The section list is too large"));
}

}  // namespace orbit_capture_file