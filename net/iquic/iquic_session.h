#pragma once

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/net_export.h"
#include "net/third_party/quiche/src/quic/core/quic_versions.h"
#include "net/base/host_port_pair.h"

#include "net/iquic/iquic_socket.h"

#include <bnl/http3/client/connection.hpp>
#include <bnl/log/console.hpp>
#include <bnl/quic/client/connection.hpp>

#include <memory>
#include <unordered_map>

namespace quic {
class QuicAlarmFactory;
class QuicAlarm;
class QuicClock;
}  // namespace quic

namespace net {

class IPEndPoint;
class DatagramClientSocket;
class ClientSocketFactory;
class NetLogWithSource;
class IOBufferWithSize;

namespace iquic {

class IQuicStreamFactory;
class RetransmitAlarmDelegate;
class TimeoutAlarmDelegate;

class NET_EXPORT_PRIVATE IQuicSession {
 public:
  using handler = base::RepeatingCallback<void(bnl::http3::event)>;

  IQuicSession(const HostPortPair &destination,
               const IPEndPoint& peer,
               quic::QuicClock* clock,
               ClientSocketFactory* socket_factory,
               quic::QuicAlarmFactory* alarm_factory,
               IQuicStreamFactory *stream_factory,
               const NetLogWithSource& net_log);
  ~IQuicSession();

  IPEndPoint Peer();
  const HostPortPair &Destination();

  bnl::result<bnl::http3::request::handle> Request(handler handler);
  void Remove(bnl::http3::request::handle request);

  bnl::result<void> Start();
  bnl::result<void> Wake();

 private:
  friend class RetransmitAlarmDelegate;
  friend class TimeoutAlarmDelegate;

  bnl::result<void> Send();
  bnl::result<void> SendOnce();
  void OnSend(bnl::result<void> r);

  bnl::result<void> Recv();
  bnl::result<void> RecvOnce();
  void OnRecv(bnl::result<void> r);

  void Retransmit();
  void Timeout();

  bnl::http3::request::handle Request();

 private:
  bnl::log::console logger_;
  std::unordered_map<uint64_t, handler> streams_;

  Socket socket_;
  std::unique_ptr<quic::QuicAlarm> retransmit_alarm_;
  std::unique_ptr<quic::QuicAlarm> timeout_alarm_;

  bnl::http3::client::connection http3_;
  bnl::quic::client::connection quic_;

  HostPortPair destination_;
  quic::QuicClock* clock_;

  IQuicStreamFactory *stream_factory_;
  base::WeakPtrFactory<IQuicSession> weak_factory_;
};

}  // namespace iquic
}  // namespace net