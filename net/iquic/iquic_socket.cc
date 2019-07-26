#include "net/iquic/iquic_socket.h"

#include "net/iquic/iquic_error.h"

#include "net/base/io_buffer.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/datagram_client_socket.h"

#include <bnl/base/error.hpp>
#include <bnl/log.hpp>

#include <fmt/ostream.h>

namespace net {
namespace iquic {

class RecvStorage : public net::IOBuffer {
 public:
  RecvStorage(size_t size) : net::IOBuffer(nullptr), buffer_(size) {
    data_ = reinterpret_cast<char*>(buffer_.data());
  }

  const uint8_t* begin() const { return buffer_.data(); }

 private:
  ~RecvStorage() override { data_ = nullptr; }

  bnl::base::buffer buffer_;
};

class SendStorage : public net::IOBuffer {
 public:
  SendStorage() = default;

  void Set(bnl::base::buffer buffer) {
    buffer_ = std::move(buffer);
    data_ = reinterpret_cast<char*>(buffer_.data());
  }

  size_t Size() { return buffer_.size(); }

  bool Empty() const { return buffer_.empty(); }

  void Consume(size_t size) { buffer_.consume(size); }

 private:
  ~SendStorage() override { data_ = nullptr; }

  bnl::base::buffer buffer_;
};

static std::unique_ptr<DatagramClientSocket> MakeSocket(
    const IPEndPoint& peer,
    ClientSocketFactory* factory,
    const NetLogWithSource& net_log) {
  std::unique_ptr<DatagramClientSocket> socket =
      factory->CreateDatagramClientSocket(DatagramSocket::DEFAULT_BIND,
                                          net_log.net_log(), net_log.source());

  socket->UseNonBlockingIO();

  int rv = socket->Connect(peer);
  DCHECK(!rv);

  return socket;
}

static constexpr int UDP_MAX_PACKET_SIZE = 65507;

Socket::Socket(const IPEndPoint& peer,
               ClientSocketFactory* socket_factory,
               const NetLogWithSource& net_log,
               const bnl::log::api* logger)
    : send_storage_(base::MakeRefCounted<SendStorage>()),
      recv_storage_(base::MakeRefCounted<RecvStorage>(UDP_MAX_PACKET_SIZE)),
      socket_(MakeSocket(peer, socket_factory, net_log)),
      logger_(logger),
      weak_factory_(this) {}

Socket::~Socket() = default;

IPEndPoint Socket::Local() const {
  IPEndPoint local;

  int rv = socket_->GetLocalAddress(&local);
  DCHECK(!rv);

  return local;
}

IPEndPoint Socket::Peer() const {
  IPEndPoint peer;

  int rv = socket_->GetPeerAddress(&peer);
  DCHECK(!rv);

  return peer;
}

void Socket::SetOnSend(SendHandler handler) {
  on_send_ = std::move(handler);
}

void Socket::SetOnRecv(RecvHandler handler) {
  on_recv_ = std::move(handler);
}

bnl::result<void> Socket::Send() {
  if (send_buffers_.empty()) {
    return bnl::base::error::idle;
  }

  if (send_in_progress_) {
    return bnl::errc::resource_unavailable_try_again;
  }

  if (send_storage_->Empty()) {
    send_storage_->Set(send_buffers_.pop());
  }

  BNL_LOG_TRACE(logger_, "Sending: {}", send_storage_->Size());

  int rv =
      socket_->Write(send_storage_.get(), send_storage_->Size(),
                     base::Bind(&Socket::OnSend, weak_factory_.GetWeakPtr()),
                     MISSING_TRAFFIC_ANNOTATION);

  if (static_cast<error>(rv) == error::io_pending) {
    send_in_progress_ = true;
    return bnl::errc::resource_unavailable_try_again;
  }

  if (rv < 0) {
    return static_cast<error>(rv);
  }

  send_storage_->Consume(static_cast<size_t>(rv));

  return bnl::success();
}

void Socket::OnSend(int rv) {
  send_in_progress_ = false;

  if (rv < 0) {
    code code = static_cast<net::error>(rv);
    BNL_LOG_ERROR(logger_, "{}", code.message());
    on_send_.Run(std::move(code));
    return;
  }

  send_storage_->Consume(static_cast<size_t>(rv));

  on_send_.Run(bnl::success());
}

void Socket::Add(bnl::base::buffer buffer) {
  send_buffers_.push(std::move(buffer));
}

bnl::result<bnl::base::buffer> Socket::Recv() {
  if (!recv_buffers_.empty()) {
    return recv_buffers_.pop();
  }

  if (recv_in_progress_) {
    return bnl::errc::resource_unavailable_try_again;
  }

  int rv =
      socket_->Read(recv_storage_.get(), UDP_MAX_PACKET_SIZE,
                    base::Bind(&Socket::OnRecv, weak_factory_.GetWeakPtr()));

  if (static_cast<error>(rv) == error::io_pending) {
    recv_in_progress_ = true;
    return bnl::errc::resource_unavailable_try_again;
  }

  if (rv < 0) {
    return static_cast<error>(rv);
  }

  return bnl::base::buffer(recv_storage_->begin(), static_cast<size_t>(rv));
}

void Socket::OnRecv(int rv) {
  recv_in_progress_ = false;

  if (rv < 0) {
    code code = static_cast<net::error>(rv);
    BNL_LOG_ERROR(logger_, "{}", code.message());
    on_recv_.Run(std::move(code));
    return;
  }

  bnl::base::buffer packet(recv_storage_->begin(), static_cast<size_t>(rv));
  recv_buffers_.push(std::move(packet));

  on_recv_.Run(bnl::success());
}

}  // namespace iquic
}  // namespace net