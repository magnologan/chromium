// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_prompt_channel_win.h"

#include <windows.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_task_environment.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_prompt_actions_win.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::StrictMock;

namespace safe_browsing {
namespace {

static constexpr uint8_t kVersion = 1U;

// Get the error category from a histogram sample.
ChromePromptChannelProtobuf::ErrorCategory SampleToCategory(
    int histogram_sample) {
  return static_cast<ChromePromptChannelProtobuf::ErrorCategory>(
      histogram_sample >> (sizeof(uint16_t) * CHAR_BIT));
}

class MockCleanerProcessDelegate
    : public ChromePromptChannel::CleanerProcessDelegate {
 public:
  MOCK_CONST_METHOD0(Handle, base::ProcessHandle());
  MOCK_CONST_METHOD0(TerminateOnError, void());
};

class ChromePromptChannelProtobufTest : public ::testing::Test {
 public:
  using ChromePromptChannelPtr =
      std::unique_ptr<ChromePromptChannelProtobuf, base::OnTaskRunnerDeleter>;

  ChromePromptChannelProtobufTest() = default;

  ~ChromePromptChannelProtobufTest() override = default;

  void SetUp() override {
    auto task_runner = base::CreateSequencedTaskRunner({base::MayBlock()});
    channel_ = ChromePromptChannelPtr(
        new ChromePromptChannelProtobuf(
            /*on_connection_closed=*/run_loop_.QuitClosure(),
            std::make_unique<ChromePromptActions>(
                /*extension_service=*/nullptr,
                /*on_prompt_user=*/base::DoNothing()),
            task_runner),
        base::OnTaskRunnerDeleter(task_runner));

    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    base::HandlesToInheritVector handles_to_inherit;
    ASSERT_TRUE(
        channel_->PrepareForCleaner(&command_line, &handles_to_inherit));

    // Instead of spawning a cleaner process, extract the prompt handles from
    // the command-line. Duplicate them so that we retain ownership if
    // ChromePromptChannelProtobuf closes them.
    response_read_handle_ = DuplicateHandleFromCommandLine(
        command_line, chrome_cleaner::kChromeReadHandleSwitch);
    ASSERT_TRUE(response_read_handle_.IsValid());
    request_write_handle_ = DuplicateHandleFromCommandLine(
        command_line, chrome_cleaner::kChromeWriteHandleSwitch);
    ASSERT_TRUE(request_write_handle_.IsValid());

    mock_cleaner_process_ =
        std::make_unique<StrictMock<MockCleanerProcessDelegate>>();
  }

  // Used to wait for the disconnect. This ensures that work done on other
  // sequences is complete.
  void WaitForDisconnect() {
    // When channel_ closes the quit closure of run_loop_ is invoked.
    run_loop_.Run();
  }

  void SetupCommunicationFailure() {
    // Expect a call to TerminateOnError since the execution will fail.
    // Have that call trigger a closing of cleaner handles which ensures reads
    // start failing.
    EXPECT_CALL(*mock_cleaner_process_, TerminateOnError)
        .WillOnce(InvokeWithoutArgs(
            this, &ChromePromptChannelProtobufTest::CloseCleanerHandles));
  }

  template <typename T>
  void ExpectUniqueSample(ChromePromptChannelProtobuf::ErrorCategory category,
                          T error,
                          int count = 1) {
    histogram_tester_.ExpectUniqueSample(
        ChromePromptChannelProtobuf::kErrorHistogramName,
        ChromePromptChannelProtobuf::GetErrorCodeInt(category, error), count);
  }

  void ExpectHistogramEmpty() {
    histogram_tester_.ExpectTotalCount(
        ChromePromptChannelProtobuf::kErrorHistogramName, 0);
  }

  // This is used when we want to validate that an operation failed a certain
  // number of times without needing to know the specific error code. Not to be
  // used with CustomErrors as those are well-known.
  void ExpectCategoryErrorCount(
      ChromePromptChannelProtobuf::ErrorCategory category,
      uint32_t expected_count) {
    std::vector<base::Bucket> buckets = histogram_tester_.GetAllSamples(
        ChromePromptChannelProtobuf::kErrorHistogramName);

    uint32_t samples_found = 0;
    for (const base::Bucket& bucket : buckets) {
      if (SampleToCategory(bucket.min) == category) {
        samples_found += bucket.count;
      }
    }

    EXPECT_EQ(expected_count, samples_found);
  }

  // Closes the cleaner process pipe handles to simulate the cleaner process
  // exiting.
  void CloseCleanerHandles() {
    response_read_handle_.Close();
    request_write_handle_.Close();
  }

  void PostCloseCleanerHandles() {
    channel_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ChromePromptChannelProtobufTest::CloseCleanerHandles,
                       base::Unretained(this)));
  }

  void WriteVersion(uint8_t version) {
    DWORD bytes_written = 0;
    ASSERT_TRUE(::WriteFile(request_write_handle_.Get(), &version,
                            sizeof(version), &bytes_written, nullptr));
    ASSERT_EQ(bytes_written, sizeof(version));
  }

  // Writes the version to the pipe without blocking the main test thread.
  void PostWriteVersion(uint8_t version) {
    channel_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ChromePromptChannelProtobufTest::WriteVersion,
                       base::Unretained(this), version));
  }

  void ExpectReadFails() {
    DWORD bytes_read = 0;
    char c = 0;
    EXPECT_FALSE(
        ::ReadFile(response_read_handle_.Get(), &c, 1, &bytes_read, nullptr));
  }

 protected:
  base::win::ScopedHandle DuplicateHandleFromCommandLine(
      const base::CommandLine& command_line,
      const std::string& handle_switch) {
    uint32_t handle_value = 0;
    if (!base::StringToUint(command_line.GetSwitchValueNative(handle_switch),
                            &handle_value)) {
      LOG(ERROR) << handle_switch << " not found on commandline";
      return base::win::ScopedHandle();
    }
    HANDLE handle = base::win::Uint32ToHandle(handle_value);
    HANDLE duplicate_handle;
    if (!::DuplicateHandle(::GetCurrentProcess(), handle, ::GetCurrentProcess(),
                           &duplicate_handle, 0, FALSE,
                           DUPLICATE_SAME_ACCESS)) {
      PLOG(ERROR) << "Failed to duplicate handle from " << handle_switch;
      return base::win::ScopedHandle();
    }
    return base::win::ScopedHandle(duplicate_handle);
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::RunLoop run_loop_;
  ChromePromptChannelPtr channel_ =
      ChromePromptChannelPtr(nullptr, base::OnTaskRunnerDeleter(nullptr));
  base::win::ScopedHandle response_read_handle_;
  base::win::ScopedHandle request_write_handle_;

  std::unique_ptr<StrictMock<MockCleanerProcessDelegate>> mock_cleaner_process_;
  base::HistogramTester histogram_tester_;
};

TEST_F(ChromePromptChannelProtobufTest, PipeInfo) {
  DWORD read_pipe_flags = 0;
  DWORD read_pipe_max_instances = 0;
  ASSERT_TRUE(::GetNamedPipeInfo(response_read_handle_.Get(), &read_pipe_flags,
                                 nullptr, nullptr, &read_pipe_max_instances));
  EXPECT_TRUE(read_pipe_flags & PIPE_TYPE_MESSAGE);
  EXPECT_EQ(read_pipe_max_instances, 1UL);

  DWORD write_pipe_flags = 0;
  DWORD write_pipe_max_instances = 0;
  ASSERT_TRUE(::GetNamedPipeInfo(request_write_handle_.Get(), &write_pipe_flags,
                                 nullptr, nullptr, &write_pipe_max_instances));
  EXPECT_TRUE(write_pipe_flags & PIPE_TYPE_MESSAGE);
  EXPECT_EQ(write_pipe_max_instances, 1UL);
}

TEST_F(ChromePromptChannelProtobufTest, ImmediateExit) {
  EXPECT_CALL(*mock_cleaner_process_, TerminateOnError).Times(1);
  channel_->ConnectToCleaner(std::move(mock_cleaner_process_));

  // Simulate the cleaner exiting without writing anything.
  PostCloseCleanerHandles();

  WaitForDisconnect();
  ExpectReadFails();
}

TEST_F(ChromePromptChannelProtobufTest, VersionIsTooLarge) {
  SetupCommunicationFailure();
  channel_->ConnectToCleaner(std::move(mock_cleaner_process_));

  // Invalid version
  PostWriteVersion(128);
  WaitForDisconnect();

  // We expect the the handshake to have failed because of the version.
  ExpectUniqueSample(
      ChromePromptChannelProtobuf::ErrorCategory::kCustomError,
      ChromePromptChannelProtobuf::CustomErrors::kWrongHandshakeVersion);

  ExpectReadFails();
}

TEST_F(ChromePromptChannelProtobufTest, VersionIsZero) {
  SetupCommunicationFailure();
  channel_->ConnectToCleaner(std::move(mock_cleaner_process_));

  // Invalid version
  PostWriteVersion(0U);
  WaitForDisconnect();

  // We expect the the handshake to have failed because of the version.
  ExpectUniqueSample(
      ChromePromptChannelProtobuf::ErrorCategory::kCustomError,
      ChromePromptChannelProtobuf::CustomErrors::kWrongHandshakeVersion);
  ExpectReadFails();
}

TEST_F(ChromePromptChannelProtobufTest, ExitAfterVersion) {
  SetupCommunicationFailure();
  channel_->ConnectToCleaner(std::move(mock_cleaner_process_));

  // Write version 1.
  PostWriteVersion(kVersion);

  // Simulate the cleaner exiting after writing the version.
  PostCloseCleanerHandles();

  WaitForDisconnect();

  // There were no errors so the histogram should be empty
  // TODO(crbug.com/969139): We should be getting a kReadRequestLengthWinError
  // here.
  ExpectHistogramEmpty();

  ExpectReadFails();
}

TEST_F(ChromePromptChannelProtobufTest, ExitBeforeVersion) {
  SetupCommunicationFailure();
  channel_->ConnectToCleaner(std::move(mock_cleaner_process_));

  // Simulate the cleaner exiting before writing the version.
  PostCloseCleanerHandles();
  WaitForDisconnect();

  ExpectCategoryErrorCount(
      ChromePromptChannelProtobuf::ErrorCategory::kReadVersionWinError, 1);
  ExpectReadFails();
}

}  // namespace
}  // namespace safe_browsing
