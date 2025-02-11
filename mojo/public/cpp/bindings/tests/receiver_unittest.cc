// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/bindings/tests/bindings_test_base.h"
#include "mojo/public/interfaces/bindings/tests/ping_service.mojom.h"
#include "mojo/public/interfaces/bindings/tests/sample_interfaces.mojom.h"
#include "mojo/public/interfaces/bindings/tests/sample_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

class ServiceImpl : public sample::Service {
 public:
  ServiceImpl() = default;
  explicit ServiceImpl(bool* was_deleted)
      : destruction_callback_(base::BindLambdaForTesting(
            [was_deleted] { *was_deleted = true; })) {}
  explicit ServiceImpl(base::OnceClosure destruction_callback)
      : destruction_callback_(std::move(destruction_callback)) {}

  ~ServiceImpl() override {
    if (destruction_callback_)
      std::move(destruction_callback_).Run();
  }

 private:
  // sample::Service implementation
  void Frobinate(sample::FooPtr foo,
                 BazOptions options,
                 PendingRemote<sample::Port> port,
                 FrobinateCallback callback) override {
    std::move(callback).Run(1);
  }
  void GetPort(PendingReceiver<sample::Port> port) override {}

  base::OnceClosure destruction_callback_;

  DISALLOW_COPY_AND_ASSIGN(ServiceImpl);
};

using ReceiverTest = BindingsTestBase;

TEST_P(ReceiverTest, Reset) {
  bool called = false;
  Remote<sample::Service> remote;

  ServiceImpl impl;
  Receiver<sample::Service> receiver(&impl,
                                     remote.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop;
  remote.set_disconnect_handler(base::BindLambdaForTesting([&] {
    run_loop.Quit();
    called = true;
  }));

  receiver.reset();
  EXPECT_FALSE(called);
  run_loop.Run();
  EXPECT_TRUE(called);
}

// Tests that destroying a mojo::Binding closes the bound message pipe handle.
TEST_P(ReceiverTest, DestroyClosesMessagePipe) {
  bool encountered_error = false;
  ServiceImpl impl;
  Remote<sample::Service> remote;
  auto pending_receiver = remote.BindNewPipeAndPassReceiver();
  base::RunLoop run_loop;
  remote.set_disconnect_handler(base::BindLambdaForTesting([&] {
    run_loop.Quit();
    encountered_error = true;
  }));

  bool called = false;
  base::RunLoop run_loop2;
  {
    Receiver<sample::Service> binding(&impl, std::move(pending_receiver));
    remote->Frobinate(nullptr, sample::Service::BazOptions::REGULAR,
                      NullRemote(), base::BindLambdaForTesting([&](int32_t) {
                        run_loop2.Quit();
                        called = true;
                      }));
    run_loop2.Run();
    EXPECT_TRUE(called);
    EXPECT_FALSE(encountered_error);
  }
  // Now that the Binding is out of scope we should detect an error on the other
  // end of the pipe.
  run_loop.Run();
  EXPECT_TRUE(encountered_error);

  // And calls should fail.
  called = false;
  remote->Frobinate(
      nullptr, sample::Service::BazOptions::REGULAR, NullRemote(),
      base::BindLambdaForTesting([&](int32_t) { called = true; }));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(called);
}

// Tests that the binding's connection error handler gets called when the other
// end is closed.
TEST_P(ReceiverTest, Disconnect) {
  bool called = false;
  {
    ServiceImpl impl;
    Remote<sample::Service> remote;
    Receiver<sample::Service> receiver(&impl,
                                       remote.BindNewPipeAndPassReceiver());
    base::RunLoop run_loop;
    receiver.set_disconnect_handler(base::BindLambdaForTesting([&] {
      called = true;
      run_loop.Quit();
    }));
    remote.reset();
    EXPECT_FALSE(called);
    run_loop.Run();
    EXPECT_TRUE(called);
    // We want to make sure that it isn't called again during destruction.
    called = false;
  }
  EXPECT_FALSE(called);
}

// Tests that calling Close doesn't result in the connection error handler being
// called.
TEST_P(ReceiverTest, ResetDoesntCallDisconnectHandler) {
  ServiceImpl impl;
  Remote<sample::Service> remote;
  Receiver<sample::Service> receiver(&impl,
                                     remote.BindNewPipeAndPassReceiver());
  bool called = false;
  receiver.set_disconnect_handler(
      base::BindLambdaForTesting([&] { called = true; }));
  receiver.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(called);

  // We can also close the other end, and the error handler still won't be
  // called.
  remote.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(called);
}

class ServiceImplWithReceiver : public ServiceImpl {
 public:
  ServiceImplWithReceiver(bool* was_deleted,
                          base::OnceClosure closure,
                          PendingReceiver<sample::Service> receiver)
      : ServiceImpl(was_deleted),
        receiver_(this, std::move(receiver)),
        closure_(std::move(closure)) {
    receiver_.set_disconnect_handler(base::BindOnce(
        &ServiceImplWithReceiver::OnDisconnect, base::Unretained(this)));
  }

 private:
  ~ServiceImplWithReceiver() override { std::move(closure_).Run(); }

  void OnDisconnect() { delete this; }

  Receiver<sample::Service> receiver_;
  base::OnceClosure closure_;

  DISALLOW_COPY_AND_ASSIGN(ServiceImplWithReceiver);
};

// Tests that the receiver may be deleted in its disconnect handler.
TEST_P(ReceiverTest, SelfDeleteOnDisconnect) {
  bool was_deleted = false;
  Remote<sample::Service> remote;
  base::RunLoop run_loop;
  // This will delete itself on disconnect.
  new ServiceImplWithReceiver(&was_deleted, run_loop.QuitClosure(),
                              remote.BindNewPipeAndPassReceiver());
  remote.reset();
  EXPECT_FALSE(was_deleted);
  run_loop.Run();
  EXPECT_TRUE(was_deleted);
}

// Tests that explicitly calling Unbind followed by rebinding works.
TEST_P(ReceiverTest, Unbind) {
  ServiceImpl impl;
  Remote<sample::Service> remote;
  Receiver<sample::Service> receiver(&impl,
                                     remote.BindNewPipeAndPassReceiver());

  bool called = false;
  base::RunLoop run_loop;
  remote->Frobinate(nullptr, sample::Service::BazOptions::REGULAR, NullRemote(),
                    base::BindLambdaForTesting([&](int32_t) {
                      called = true;
                      run_loop.Quit();
                    }));
  run_loop.Run();
  EXPECT_TRUE(called);

  called = false;
  auto pending_receiver = receiver.Unbind();
  EXPECT_FALSE(receiver.is_bound());
  EXPECT_TRUE(pending_receiver);

  // All calls should be withheld when the receiver is not bound...
  remote->Frobinate(nullptr, sample::Service::BazOptions::REGULAR, NullRemote(),
                    base::BindLambdaForTesting([&](int32_t) {
                      called = true;
                      run_loop.Quit();
                    }));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(called);

  called = false;
  receiver.Bind(std::move(pending_receiver));
  EXPECT_TRUE(receiver.is_bound());
  EXPECT_FALSE(pending_receiver);

  // ...and calls should resume again when the receiver is
  // rebound.
  base::RunLoop run_loop2;
  remote->Frobinate(nullptr, sample::Service::BazOptions::REGULAR, NullRemote(),
                    base::BindLambdaForTesting([&](int32_t) {
                      called = true;
                      run_loop2.Quit();
                    }));
  run_loop2.Run();
  EXPECT_TRUE(called);
}

class IntegerAccessorImpl : public sample::IntegerAccessor {
 public:
  IntegerAccessorImpl() = default;
  ~IntegerAccessorImpl() override = default;

 private:
  // sample::IntegerAccessor implementation.
  void GetInteger(GetIntegerCallback callback) override {
    std::move(callback).Run(1, sample::Enum::VALUE);
  }
  void SetInteger(int64_t data, sample::Enum type) override {}

  DISALLOW_COPY_AND_ASSIGN(IntegerAccessorImpl);
};

TEST_P(ReceiverTest, PauseResume) {
  bool called = false;
  base::RunLoop run_loop;
  Remote<sample::Service> remote;
  ServiceImpl impl;
  Receiver<sample::Service> receiver(&impl,
                                     remote.BindNewPipeAndPassReceiver());
  receiver.Pause();
  remote->Frobinate(nullptr, sample::Service::BazOptions::REGULAR, NullRemote(),
                    base::BindLambdaForTesting([&](int32_t) {
                      called = true;
                      run_loop.Quit();
                    }));

  EXPECT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  // Frobinate() should not be called as the receiver is paused.
  EXPECT_FALSE(called);

  // Resume the receiver, which should trigger processing.
  receiver.Resume();
  run_loop.Run();
  EXPECT_TRUE(called);
}

// Verifies the disconnect handler is not run while a receiver is paused.
TEST_P(ReceiverTest, ErrorHandleNotRunWhilePaused) {
  bool called = false;
  base::RunLoop run_loop;
  Remote<sample::Service> remote;
  ServiceImpl impl;
  Receiver<sample::Service> receiver(&impl,
                                     remote.BindNewPipeAndPassReceiver());
  receiver.set_disconnect_handler(base::BindLambdaForTesting([&] {
    called = true;
    run_loop.Quit();
  }));
  receiver.Pause();

  remote.reset();
  base::RunLoop().RunUntilIdle();
  // The disconnect handler should not be called as the receiver is paused.
  EXPECT_FALSE(called);

  // Resume the receiver, which should trigger the disconnect handler.
  receiver.Resume();
  run_loop.Run();
  EXPECT_TRUE(called);
}

class PingServiceImpl : public test::PingService {
 public:
  PingServiceImpl() = default;
  ~PingServiceImpl() override = default;

  // test::PingService:
  void Ping(PingCallback callback) override {
    if (ping_handler_)
      ping_handler_.Run();
    std::move(callback).Run();
  }

  void set_ping_handler(base::RepeatingClosure handler) {
    ping_handler_ = std::move(handler);
  }

 private:
  base::RepeatingClosure ping_handler_;

  DISALLOW_COPY_AND_ASSIGN(PingServiceImpl);
};

class CallbackFilter : public MessageReceiver {
 public:
  explicit CallbackFilter(const base::RepeatingClosure& callback)
      : callback_(callback) {}
  ~CallbackFilter() override {}

  static std::unique_ptr<CallbackFilter> Wrap(
      const base::RepeatingClosure& callback) {
    return std::make_unique<CallbackFilter>(callback);
  }

  // MessageReceiver:
  bool Accept(Message* message) override {
    callback_.Run();
    return true;
  }

 private:
  const base::RepeatingClosure callback_;
};

// Verifies that message filters are notified in the order they were added and
// are always notified before a message is dispatched.
TEST_P(ReceiverTest, MessageFilter) {
  Remote<test::PingService> remote;
  PingServiceImpl impl;
  Receiver<test::PingService> receiver(&impl,
                                       remote.BindNewPipeAndPassReceiver());

  int status = 0;
  receiver.AddFilter(CallbackFilter::Wrap(base::BindLambdaForTesting([&] {
    EXPECT_EQ(0, status);
    status = 1;
  })));

  receiver.AddFilter(CallbackFilter::Wrap(base::BindLambdaForTesting([&] {
    EXPECT_EQ(1, status);
    status = 2;
  })));

  impl.set_ping_handler(base::BindLambdaForTesting([&] {
    EXPECT_EQ(2, status);
    status = 3;
  }));

  for (int i = 0; i < 10; ++i) {
    status = 0;
    base::RunLoop loop;
    remote->Ping(loop.QuitClosure());
    loop.Run();
    EXPECT_EQ(3, status);
  }
}

void Fail() {
  FAIL() << "Unexpected connection error";
}

TEST_P(ReceiverTest, FlushForTesting) {
  bool called = false;
  Remote<sample::Service> remote;
  ServiceImpl impl;
  Receiver<sample::Service> receiver(&impl,
                                     remote.BindNewPipeAndPassReceiver());
  receiver.set_disconnect_handler(base::BindOnce(&Fail));

  remote->Frobinate(
      nullptr, sample::Service::BazOptions::REGULAR, NullRemote(),
      base::BindLambdaForTesting([&](int32_t) { called = true; }));
  EXPECT_FALSE(called);
  // Because the flush is sent from the receiver, it only guarantees that the
  // request has been received, not the response. The second flush waits for
  // the response to be received.
  receiver.FlushForTesting();
  receiver.FlushForTesting();
  EXPECT_TRUE(called);
}

TEST_P(ReceiverTest, FlushForTestingWithClosedPeer) {
  bool called = false;
  Remote<sample::Service> remote;
  ServiceImpl impl;
  Receiver<sample::Service> receiver(&impl,
                                     remote.BindNewPipeAndPassReceiver());
  receiver.set_disconnect_handler(
      base::BindLambdaForTesting([&] { called = true; }));
  remote.reset();

  EXPECT_FALSE(called);
  receiver.FlushForTesting();
  EXPECT_TRUE(called);
  receiver.FlushForTesting();
}

TEST_P(ReceiverTest, DisconnectWithReason) {
  Remote<sample::Service> remote;
  ServiceImpl impl;
  Receiver<sample::Service> receiver(&impl,
                                     remote.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop;
  receiver.set_disconnect_with_reason_handler(base::BindLambdaForTesting(
      [&](uint32_t custom_reason, const std::string& description) {
        EXPECT_EQ(1234u, custom_reason);
        EXPECT_EQ("hello", description);
        run_loop.Quit();
      }));

  remote.ResetWithReason(1234u, "hello");
  run_loop.Run();
}

template <typename T>
struct WeakPtrImplRefTraits {
  using PointerType = base::WeakPtr<T>;

  static bool IsNull(const base::WeakPtr<T>& ptr) { return !ptr; }
  static T* GetRawPointer(base::WeakPtr<T>* ptr) { return ptr->get(); }
};

template <typename T>
using WeakReceiver = Receiver<T, WeakPtrImplRefTraits<T>>;

TEST_P(ReceiverTest, CustomImplPointerType) {
  PingServiceImpl impl;
  base::WeakPtrFactory<test::PingService> weak_factory(&impl);

  Remote<test::PingService> remote;
  WeakReceiver<test::PingService> receiver(weak_factory.GetWeakPtr(),
                                           remote.BindNewPipeAndPassReceiver());

  {
    // Ensure the receiver is functioning.
    base::RunLoop run_loop;
    remote->Ping(run_loop.QuitClosure());
    run_loop.Run();
  }

  {
    // Attempt to dispatch another message after the WeakPtr is invalidated.
    impl.set_ping_handler(base::BindRepeating([] { NOTREACHED(); }));
    remote->Ping(base::BindOnce([] { NOTREACHED(); }));

    // The receiver will close its end of the pipe which will trigger a
    // disconnect on |remote|.
    base::RunLoop run_loop;
    remote.set_disconnect_handler(run_loop.QuitClosure());
    weak_factory.InvalidateWeakPtrs();
    run_loop.Run();
  }
}

TEST_P(ReceiverTest, ReportBadMessage) {
  bool called = false;
  Remote<test::PingService> remote;
  auto pending_receiver = remote.BindNewPipeAndPassReceiver();
  base::RunLoop run_loop;
  remote.set_disconnect_handler(base::BindLambdaForTesting([&] {
    called = true;
    run_loop.Quit();
  }));
  PingServiceImpl impl;
  Receiver<test::PingService> receiver(&impl, std::move(pending_receiver));
  impl.set_ping_handler(base::BindLambdaForTesting(
      [&] { receiver.ReportBadMessage("received bad message"); }));

  std::string received_error;
  core::SetDefaultProcessErrorCallback(base::BindLambdaForTesting(
      [&](const std::string& error) { received_error = error; }));

  remote->Ping(base::DoNothing());
  EXPECT_FALSE(called);
  run_loop.Run();
  EXPECT_TRUE(called);
  EXPECT_EQ("received bad message", received_error);

  core::SetDefaultProcessErrorCallback(base::NullCallback());
}

TEST_P(ReceiverTest, GetBadMessageCallback) {
  Remote<test::PingService> remote;
  base::RunLoop run_loop;
  PingServiceImpl impl;
  ReportBadMessageCallback bad_message_callback;

  std::string received_error;
  core::SetDefaultProcessErrorCallback(base::BindLambdaForTesting(
      [&](const std::string& error) { received_error = error; }));

  {
    Receiver<test::PingService> receiver(&impl,
                                         remote.BindNewPipeAndPassReceiver());
    impl.set_ping_handler(base::BindLambdaForTesting(
        [&] { bad_message_callback = receiver.GetBadMessageCallback(); }));
    remote->Ping(run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_TRUE(received_error.empty());
    EXPECT_TRUE(bad_message_callback);
  }

  std::move(bad_message_callback).Run("delayed bad message");
  EXPECT_EQ("delayed bad message", received_error);

  core::SetDefaultProcessErrorCallback(base::NullCallback());
}

TEST_P(ReceiverTest, InvalidPendingReceivers) {
  PendingReceiver<sample::Service> uninitialized_pending;
  EXPECT_FALSE(uninitialized_pending);

  // A "null" receiver is just a generic helper for an uninitialized
  // PendingReceiver. Verify that it's equivalent to above.
  PendingReceiver<sample::Service> null_pending{NullReceiver()};
  EXPECT_FALSE(null_pending);
}

TEST_P(ReceiverTest, GenericPendingReceiver) {
  Remote<sample::Service> remote;
  GenericPendingReceiver receiver;
  EXPECT_FALSE(receiver.is_valid());
  EXPECT_FALSE(receiver.interface_name().has_value());

  receiver = GenericPendingReceiver(remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(receiver.is_valid());
  EXPECT_EQ(sample::Service::Name_, receiver.interface_name());

  auto ping_receiver = receiver.As<test::PingService>();
  EXPECT_FALSE(ping_receiver.is_valid());
  EXPECT_TRUE(receiver.is_valid());

  auto sample_receiver = receiver.As<sample::Service>();
  EXPECT_TRUE(sample_receiver.is_valid());
  EXPECT_FALSE(receiver.is_valid());
}

using StrongBindingTest = BindingsTestBase;

TEST_P(StrongBindingTest, CloseDestroysImplAndPipe) {
  base::RunLoop run_loop;
  bool disconnected = false;
  bool was_deleted = false;
  Remote<sample::Service> remote;
  auto receiver = remote.BindNewPipeAndPassReceiver();
  remote.set_disconnect_handler(base::BindLambdaForTesting([&] {
    disconnected = true;
    run_loop.Quit();
  }));
  bool called = false;
  base::RunLoop run_loop2;

  auto binding = MakeStrongBinding<sample::Service>(
      std::make_unique<ServiceImpl>(&was_deleted), std::move(receiver));
  remote->Frobinate(nullptr, sample::Service::BazOptions::REGULAR, NullRemote(),
                    base::BindLambdaForTesting([&](int32_t) {
                      called = true;
                      run_loop2.Quit();
                    }));
  run_loop2.Run();
  EXPECT_TRUE(called);
  EXPECT_FALSE(disconnected);
  binding->Close();

  // Now that the StrongBinding is closed we should detect an error on the other
  // end of the pipe.
  run_loop.Run();
  EXPECT_TRUE(disconnected);

  // Destroying the StrongBinding also destroys the impl.
  ASSERT_TRUE(was_deleted);
}

TEST_P(StrongBindingTest, DisconnectDestroysImplAndPipe) {
  Remote<sample::Service> remote;
  bool was_deleted = false;
  base::RunLoop run_loop;

  MakeStrongBinding<sample::Service>(
      std::make_unique<ServiceImpl>(base::BindLambdaForTesting([&] {
        was_deleted = true;
        run_loop.Quit();
      })),
      remote.BindNewPipeAndPassReceiver());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(was_deleted);

  remote.reset();
  EXPECT_FALSE(was_deleted);
  run_loop.Run();
  EXPECT_TRUE(was_deleted);
}

INSTANTIATE_MOJO_BINDINGS_TEST_SUITE_P(ReceiverTest);
INSTANTIATE_MOJO_BINDINGS_TEST_SUITE_P(StrongBindingTest);

}  // namespace
}  // namespace mojo
