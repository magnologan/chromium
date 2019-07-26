#include "net/iquic/iquic_session.h"

#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/datagram_client_socket.h"
#include "net/third_party/quiche/src/quic/core/quic_alarm_factory.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_clock.h"

#include "net/iquic/iquic_error.h"
#include "net/iquic/iquic_stream_factory.h"

#include <bnl/base/error.hpp>
#include <iostream>

#include <fmt/ostream.h>

#define THROW(...)                                                        \
  BNL_LOG_ERROR(&logger_, "{}", make_status_code(__VA_ARGS__).message()); \
  return __VA_ARGS__;                                                     \
  (void)0

constexpr unsigned long long operator"" _KiB(unsigned long long k)  // NOLINT
{
  return k * 1024;
}

constexpr unsigned long long operator"" _MiB(unsigned long long m)  // NOLINT
{
  return m * 1024 * 1024;
}

constexpr unsigned long long operator"" _GiB(unsigned long long g)  // NOLINT
{
  return g * 1024 * 1024 * 1024;
}

namespace net {
namespace iquic {

static bnl::ip::endpoint MakeEndpoint(const IPEndPoint& endpoint) {
  bnl::base::buffer_view bytes = {endpoint.address().bytes().data(),
                                  endpoint.address().bytes().size()};
  return {bytes, endpoint.port()};
}

static bnl::quic::path MakePath(const Socket& socket) {
  return {MakeEndpoint(socket.Local()), MakeEndpoint(socket.Peer())};
}

static bnl::quic::clock MakeClock(quic::QuicClock* clock) {
  return [clock]() -> bnl::result<bnl::quic::duration> {
    return bnl::quic::duration(clock->WallNow().ToUNIXMicroseconds());
  };
}

static bnl::quic::params DefaultParams() {
  bnl::quic::params params;

  params.max_stream_data_bidi_local = 256_KiB;
  params.max_stream_data_bidi_remote = 256_KiB;
  params.max_stream_data_uni = 256_KiB;
  params.max_data = 1_MiB;
  params.max_streams_bidi = 1;
  params.max_streams_uni = 3;
  params.idle_timeout = bnl::quic::milliseconds(30000);

  return params;
}

static quic::QuicTime MakeTime(quic::QuicClock* clock,
                               bnl::quic::duration usec) {
  return clock->ApproximateNow() +
         quic::QuicTime::Delta::FromMicroseconds(usec.count());
}

class RetransmitAlarmDelegate : public quic::QuicAlarm::Delegate {
 public:
  explicit RetransmitAlarmDelegate(IQuicSession* session) : session_(session) {}

  RetransmitAlarmDelegate(const RetransmitAlarmDelegate&) = delete;
  RetransmitAlarmDelegate operator=(const RetransmitAlarmDelegate&) = delete;

  void OnAlarm() override { session_->Retransmit(); }

 private:
  IQuicSession* session_;
};

class TimeoutAlarmDelegate : public quic::QuicAlarm::Delegate {
 public:
  explicit TimeoutAlarmDelegate(IQuicSession* session) : session_(session) {}

  TimeoutAlarmDelegate(const TimeoutAlarmDelegate&) = delete;
  TimeoutAlarmDelegate& operator=(const TimeoutAlarmDelegate&) = delete;

  void OnAlarm() override { session_->Timeout(); }

 private:
  IQuicSession* session_;
};

IQuicSession::IQuicSession(const HostPortPair& destination,
                           const IPEndPoint& peer,
                           quic::QuicClock* clock,
                           ClientSocketFactory* socket_factory,
                           quic::QuicAlarmFactory* alarm_factory,
                           IQuicStreamFactory *stream_factory,
                           const NetLogWithSource& net_log)
    : logger_(new bnl::log::console(true)),
      socket_(peer, socket_factory, net_log, &logger_),
      retransmit_alarm_(
          alarm_factory->CreateAlarm(new TimeoutAlarmDelegate(this))),
      timeout_alarm_(
          alarm_factory->CreateAlarm(new RetransmitAlarmDelegate(this))),
      http3_(&logger_),
      quic_(destination.host(),
            MakePath(socket_),
            DefaultParams(),
            MakeClock(clock),
            nullptr),
      destination_(destination),
      clock_(clock),
      stream_factory_(stream_factory),
      weak_factory_(this) {
  socket_.SetOnSend(
      base::BindRepeating(&IQuicSession::OnSend, weak_factory_.GetWeakPtr()));
  socket_.SetOnRecv(
      base::BindRepeating(&IQuicSession::OnRecv, weak_factory_.GetWeakPtr()));

  BNL_LOG_INFO(&logger_, "Establishing IQUIC connection with {}:{}",
               destination.host(), destination.port());
  BNL_LOG_INFO(&logger_, "At address {}", peer.ToString());
}

IPEndPoint IQuicSession::Peer() {
  return socket_.Peer();
}

const HostPortPair& IQuicSession::Destination()
{
  return destination_;
}

bnl::result<bnl::http3::request::handle> IQuicSession::Request(
    handler handler) {
  bnl::http3::request::handle handle = BNL_TRY(http3_.request());
  streams_[handle.id()] = std::move(handler);
  return handle;
}

void IQuicSession::Remove(bnl::http3::request::handle request) {
  BNL_LOG_INFO(&logger_, "Erasing id {} from handler", request.id());
  streams_.erase(request.id());
}

bnl::result<void> IQuicSession::Start() {
  retransmit_alarm_->Set(MakeTime(clock_, quic_.expiry()));
  timeout_alarm_->Set(MakeTime(clock_, quic_.timeout()));

  BNL_TRY(Send());
  BNL_TRY(Recv());
  return bnl::success();
}

bnl::result<void> IQuicSession::Wake() {
  return Send();
}

IQuicSession::~IQuicSession() = default;

bnl::result<void> IQuicSession::Send() {
  bnl::result<void> r = bnl::success();

  do {
    r = SendOnce();
  } while (r);

  auto time = MakeTime(clock_, quic_.timeout());

  BNL_LOG_INFO(&logger_, "{}", time.ToDebuggingValue());

  retransmit_alarm_->Update(MakeTime(clock_, quic_.expiry()),
                            quic::QuicTime::Delta::Zero());
  timeout_alarm_->Update(MakeTime(clock_, quic_.timeout()),
                         quic::QuicTime::Delta::Zero());

  if (r.error() == bnl::base::error::idle ||
      r.error() == bnl::errc::resource_unavailable_try_again) {
    return bnl::success();
  }

  BNL_LOG_ERROR(&logger_, "{}", r.error().value());

  return std::move(r).error();
}

bnl::result<void> IQuicSession::SendOnce() {
  {
    bnl::result<void> r = socket_.Send();
    if (r) {
      return bnl::success();
    }

    if (r.error() != bnl::base::error::idle) {
      return std::move(r).error();
    }
  }

  {
    bnl::result<bnl::base::buffer> r = quic_.send();
    if (r) {
      BNL_LOG_TRACE(&logger_, "QUIC -> UDP: {}", r.value().size());
      socket_.Add(std::move(r).value());
      return bnl::success();
    }

    if (r.error() != bnl::base::error::idle) {
      return std::move(r).error();
    }
  }

  {
    bnl::result<bnl::quic::event> r = http3_.send();
    if (r) {
      BNL_LOG_TRACE(&logger_, "HTTP/3 -> QUIC: {}", r.value());
      return quic_.add(std::move(r).value());
    }

    if (r.error() != bnl::base::error::idle) {
      return std::move(r).error();
    }
  }

  return bnl::base::error::idle;
}

void IQuicSession::OnSend(bnl::result<void> r) {
  if (!r) {
    BNL_LOG_ERROR(&logger_, "OnSend got error: ", r.error().message());
    return;
  }

  r = Send();
  if (!r) {
    BNL_LOG_ERROR(&logger_, "{}", r.error().message());
  }
}

bnl::result<void> IQuicSession::Recv() {
  bnl::result<void> r = bnl::success();

  do {
    r = RecvOnce();
  } while (r);

  timeout_alarm_->Update(MakeTime(clock_, quic_.timeout()),
                         quic::QuicTime::Delta::Zero());

  if (r.error() == bnl::errc::resource_unavailable_try_again) {
    return Send();
  }

  return std::move(r).error();
}

bnl::result<void> IQuicSession::RecvOnce() {
  bnl::base::buffer packet = BNL_TRY(socket_.Recv());

  auto on_quic = [this](bnl::quic::event event) -> bnl::result<void> {
    auto run = [this](bnl::http3::event event) -> bnl::result<void> {
      uint64_t id = 0;

      switch (event) {
        case bnl::http3::event::type::settings:
          return bnl::success();
        case bnl::http3::event::type::header:
          id = event.header.id;
          break;
        case bnl::http3::event::type::body:
          id = event.body.id;
      }

      auto match = streams_.find(id);
      if (match == streams_.end()) {
        BNL_LOG_WARNING(&logger_, "stream {} not found in handlers", id);
        THROW(bnl::quic::connection::error::internal);
      }

      auto handler = match->second;
      handler.Run(std::move(event));

      return bnl::success();
    };

    return http3_.recv(std::move(event), run);
  };

  BNL_TRY(quic_.recv(std::move(packet), on_quic));

  return bnl::success();
}

void IQuicSession::OnRecv(bnl::result<void> r) {
  if (!r) {
    BNL_LOG_ERROR(&logger_, "OnRecv got error: ", r.error().message());
    return;
  }

  r = Recv();
  if (!r) {
    BNL_LOG_ERROR(&logger_, "{}", r.error().message());
  }
}

void IQuicSession::Retransmit() {
  bnl::result<void> r = quic_.expire();
  if (!r) {
    BNL_LOG_ERROR(&logger_, "{}", r.error().message());
  }
}

void IQuicSession::Timeout() {
  BNL_LOG_INFO(&logger_, "Timeout");
}

}  // namespace iquic
}  // namespace net
