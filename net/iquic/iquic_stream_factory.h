#include "net/base/address_list.h"
#include "net/base/net_export.h"
#include "net/iquic/iquic_http_stream.h"
#include "net/log/net_log_with_source.h"

#include "net/quic/quic_session_key.h"

#pragma once

namespace quic {
class QuicAlarmFactory;
class QuicClock;
}  // namespace quic

namespace net {

class HostResolver;
class ClientSocketFactory;

namespace iquic {

class IQuicStreamFactory;

class NET_EXPORT_PRIVATE IQuicStreamRequest {
 public:
  IQuicStreamRequest(IQuicStreamFactory* factory);
  ~IQuicStreamRequest();

  int Request(const HostPortPair& destination,
              PrivacyMode privacy_mode,
              const SocketTag& socket_tag,
              const GURL& url,
              const NetLogWithSource& net_log,
              CompletionOnceCallback callback);

  IQuicSession* ReleaseSession() { return session_; }

  void SetSession(IQuicSession* session) { session_ = session; }

  void OnRequestComplete(int rv);

  const QuicSessionKey& session_key() { return session_key_; }

 private:
  IQuicStreamFactory* factory_;

  QuicSessionKey session_key_;
  IQuicSession* session_;
  CompletionOnceCallback callback_;
};

class NET_EXPORT_PRIVATE IQuicStreamFactory {
 public:
  IQuicStreamFactory(HostResolver* host_resolver,
                     ClientSocketFactory* client_socket_factory,
                     quic::QuicClock* clock);
  ~IQuicStreamFactory();

  int Create(const QuicSessionKey& session_key,
             const HostPortPair& destination,
             const NetLogWithSource& net_log,
             IQuicStreamRequest* request);

  int CreateSession(const HostPortPair destination, const QuicSessionKey& key,
                    const AddressList& address_list,
                    const NetLogWithSource& net_log,
                    IQuicSession** session);

  void ActivateSession(const QuicSessionKey& key, IQuicSession* session);

  void CancelRequest(IQuicStreamRequest* request);

  void CloseAllSessions(int error, quic::QuicErrorCode quic_error);

 private:
  class Job;
  typedef std::map<QuicSessionKey, std::unique_ptr<Job>> JobMap;
  typedef std::map<QuicSessionKey, IQuicSession*> SessionMap;
  typedef std::map<IQuicSession*, QuicSessionKey> SessionIdMap;

  void OnJobComplete(Job* job, int rv);

  HostResolver* host_resolver_;
  ClientSocketFactory* client_socket_factory_;
  quic::QuicClock* clock_;
  const NetLogWithSource net_log_;
  std::unique_ptr<quic::QuicAlarmFactory> alarm_factory_;

  JobMap active_jobs_;
  SessionMap active_sessions_;
  SessionIdMap all_sessions_;
};

}  // namespace iquic
}  // namespace net
