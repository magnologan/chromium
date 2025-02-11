// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/stack_sampling/tracing_sampler_profiler.h"

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread.h"
#include "base/trace_event/trace_buffer.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "services/tracing/public/cpp/perfetto/producer_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/ext/base/utils.h"
#include "third_party/perfetto/include/perfetto/protozero/scattered_stream_null_delegate.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"

namespace tracing {
namespace {

using base::trace_event::TraceLog;

class MockTraceWriter : public perfetto::TraceWriter {
 public:
  MockTraceWriter(
      const base::RepeatingCallback<void(
          std::unique_ptr<perfetto::protos::TracePacket>)>& on_packet_callback)
      : delegate_(perfetto::base::kPageSize),
        stream_(&delegate_),
        on_packet_callback_(std::move(on_packet_callback)) {
    trace_packet_.Reset(&stream_);
  }

  void FlushPacketIfPossible() {
    // GetNewBuffer() in ScatteredStreamWriterNullDelegate doesn't
    // actually return a new buffer, but rather lets us access the buffer
    // buffer already used by protozero to write the TracePacket into.
    protozero::ContiguousMemoryRange buffer = delegate_.GetNewBuffer();

    uint32_t message_size = trace_packet_.Finalize();
    if (message_size) {
      EXPECT_GE(buffer.size(), message_size);

      auto proto = std::make_unique<perfetto::protos::TracePacket>();
      EXPECT_TRUE(proto->ParseFromArray(buffer.begin, message_size));
      on_packet_callback_.Run(std::move(proto));
    }

    stream_.Reset(buffer);
    trace_packet_.Reset(&stream_);
  }

  perfetto::TraceWriter::TracePacketHandle NewTracePacket() override {
    FlushPacketIfPossible();

    return perfetto::TraceWriter::TracePacketHandle(&trace_packet_);
  }

  void Flush(std::function<void()> callback = {}) override {}

  perfetto::WriterID writer_id() const override {
    return perfetto::WriterID(0);
  }

  uint64_t written() const override { return 0u; }

 private:
  perfetto::protos::pbzero::TracePacket trace_packet_;
  protozero::ScatteredStreamWriterNullDelegate delegate_;
  protozero::ScatteredStreamWriter stream_;

  base::RepeatingCallback<void(std::unique_ptr<perfetto::protos::TracePacket>)>
      on_packet_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockTraceWriter);
};

class MockPerfettoProducer : public ProducerClient {
 public:
  explicit MockPerfettoProducer(std::unique_ptr<PerfettoTaskRunner> task_runner)
      : ProducerClient(task_runner.get()),
        task_runner_(std::move(task_runner)) {}

  std::unique_ptr<perfetto::TraceWriter> CreateTraceWriter(
      perfetto::BufferID target_buffer) override {
    auto packet_callback = base::BindRepeating(
        [](base::WeakPtr<MockPerfettoProducer> weak_self,
           scoped_refptr<base::SequencedTaskRunner> task_runner,
           std::unique_ptr<perfetto::protos::TracePacket> packet) {
          task_runner->PostTask(
              FROM_HERE, base::BindOnce(&MockPerfettoProducer::ReceivePacket,
                                        weak_self, std::move(packet)));
        },
        weak_ptr_factory_.GetWeakPtr(), base::SequencedTaskRunnerHandle::Get());

    return std::make_unique<MockTraceWriter>(packet_callback);
  }

  void ReceivePacket(std::unique_ptr<perfetto::protos::TracePacket> packet) {
    base::AutoLock lock(lock_);
    finalized_packets_.push_back(std::move(packet));
  }

  const perfetto::protos::TracePacket* GetFinalizedPacket(
      size_t packet_index = 0) {
    base::AutoLock lock(lock_);
    EXPECT_GT(finalized_packets_.size(), packet_index);
    return finalized_packets_[packet_index].get();
  }

  const std::vector<std::unique_ptr<perfetto::protos::TracePacket>>&
  finalized_packets() const {
    return finalized_packets_;
  }

 private:
  base::Lock lock_;  // protects finalized_packets_
  std::vector<std::unique_ptr<perfetto::protos::TracePacket>>
      finalized_packets_;

  std::unique_ptr<PerfettoTaskRunner> task_runner_;
  base::WeakPtrFactory<MockPerfettoProducer> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(MockPerfettoProducer);
};

class TracingSampleProfilerTest : public testing::Test {
 public:
  TracingSampleProfilerTest() = default;
  ~TracingSampleProfilerTest() override = default;

  void SetUp() override {
    events_stack_received_count_ = 0u;
    PerfettoTracedProcess::ResetTaskRunnerForTesting();
    PerfettoTracedProcess::GetTaskRunner()->GetOrCreateTaskRunner();

    auto perfetto_wrapper = std::make_unique<PerfettoTaskRunner>(
        scoped_task_environment_.GetMainThreadTaskRunner());

    producer_ =
        std::make_unique<MockPerfettoProducer>(std::move(perfetto_wrapper));
  }

  void TearDown() override {
    // Be sure there is no pending/running tasks.
    scoped_task_environment_.RunUntilIdle();
  }

  void BeginTrace() {
    TracingSamplerProfiler::StartTracingForTesting(producer_.get());
  }

  void WaitForEvents() {
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(200));
  }

  // Returns whether of not the sampler profiling is able to unwind the stack
  // on this platform.
  bool IsStackUnwindingSupported() {
#if defined(OS_MACOSX) || defined(OS_WIN) && defined(_WIN64) ||     \
    (defined(OS_ANDROID) && BUILDFLAG(CAN_UNWIND_WITH_CFI_TABLE) && \
     defined(OFFICIAL_BUILD))
    return true;
#else
    return false;
#endif
  }

  static void TraceDataCallback(
      const base::RepeatingCallback<void()>& callback,
      std::string* output,
      const scoped_refptr<base::RefCountedString>& json_events_str,
      bool has_more_events) {
    if (output->size() > 1 && !json_events_str->data().empty()) {
      output->append(",");
    }
    output->append(json_events_str->data());
    if (!has_more_events) {
      callback.Run();
    }
  }

  void EndTracing() {
    TracingSamplerProfiler::StopTracingForTesting();
    base::RunLoop().RunUntilIdle();

    auto& packets = producer_->finalized_packets();
    for (auto& packet : packets) {
      if (packet->has_streaming_profile_packet()) {
        events_stack_received_count_++;
      }
    }
  }

  void ValidateReceivedEvents() {
    if (IsStackUnwindingSupported()) {
      EXPECT_GT(events_stack_received_count_, 0U);
    } else {
      EXPECT_EQ(events_stack_received_count_, 0U);
    }
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  // We want our singleton torn down after each test.
  base::ShadowingAtExitManager at_exit_manager_;
  base::trace_event::TraceResultBuffer trace_buffer_;
  base::trace_event::TraceResultBuffer::SimpleOutput json_output_;

  std::unique_ptr<MockPerfettoProducer> producer_;

  // Number of stack sampling events received.
  size_t events_stack_received_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TracingSampleProfilerTest);
};

// Stub module for testing.
class TestModule : public base::ModuleCache::Module {
 public:
  TestModule() = default;

  TestModule(const TestModule&) = delete;
  TestModule& operator=(const TestModule&) = delete;

  uintptr_t GetBaseAddress() const override { return 0; }
  std::string GetId() const override { return ""; }
  base::FilePath GetDebugBasename() const override { return base::FilePath(); }
  size_t GetSize() const override { return 0; }
  bool IsNative() const override { return true; }
};

}  // namespace

TEST_F(TracingSampleProfilerTest, OnSampleCompleted) {
  TracingSamplerProfiler::CreateForCurrentThread();
  BeginTrace();
  base::RunLoop().RunUntilIdle();
  WaitForEvents();
  EndTracing();
  base::RunLoop().RunUntilIdle();
  ValidateReceivedEvents();
  TracingSamplerProfiler::DeleteForCurrentThreadForTesting();
}

TEST_F(TracingSampleProfilerTest, JoinRunningTracing) {
  BeginTrace();
  TracingSamplerProfiler::CreateForCurrentThread();
  base::RunLoop().RunUntilIdle();
  WaitForEvents();
  EndTracing();
  base::RunLoop().RunUntilIdle();
  ValidateReceivedEvents();
  TracingSamplerProfiler::DeleteForCurrentThreadForTesting();
}

TEST_F(TracingSampleProfilerTest, SamplingChildThread) {
  base::Thread sampled_thread("sampling_profiler_test");
  sampled_thread.Start();
  sampled_thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&TracingSamplerProfiler::CreateForCurrentThread));
  BeginTrace();
  base::RunLoop().RunUntilIdle();
  WaitForEvents();
  EndTracing();
  ValidateReceivedEvents();
  sampled_thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &TracingSamplerProfiler::DeleteForCurrentThreadForTesting));
  base::RunLoop().RunUntilIdle();
}

TEST(TracingProfileBuilderTest, ValidModule) {
  TestModule module;
  TracingSamplerProfiler::TracingProfileBuilder profile_builder(
      base::PlatformThreadId(),
      std::make_unique<MockTraceWriter>(base::DoNothing()), false);
  profile_builder.OnSampleCompleted({base::Frame(0x1010, &module)});
}

TEST(TracingProfileBuilderTest, InvalidModule) {
  TracingSamplerProfiler::TracingProfileBuilder profile_builder(
      base::PlatformThreadId(),
      std::make_unique<MockTraceWriter>(base::DoNothing()), false);
  profile_builder.OnSampleCompleted({base::Frame(0x1010, nullptr)});
}

}  // namespace tracing
