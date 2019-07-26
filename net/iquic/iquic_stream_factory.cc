#include "net/iquic/iquic_stream_factory.h"

#include "base/threading/thread_task_runner_handle.h"
#include "net/dns/host_resolver.h"
#include "net/quic/quic_chromium_alarm_factory.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/datagram_client_socket.h"

#include "net/iquic/iquic_session.h"

#include <iostream>

namespace net {
namespace iquic {

class IQuicStreamFactory::Job {
 public:
  Job(IQuicStreamFactory* factory,
      HostResolver* resolver,
      const HostPortPair destination,
      QuicSessionKey session_key,
      NetLogWithSource net_log);

  ~Job() = default;

  int Run(CompletionOnceCallback callback);

  int DoLoop(int rv);

  int DoResolveHost();
  void OnResolveHostComplete(int rv);
  int DoResolveHostComplete(int rv);

  void AddRequest(IQuicStreamRequest* request);
  void RemoveRequest(IQuicStreamRequest* request);

  QuicSessionKey session_key() { return session_key_; }

  const std::set<IQuicStreamRequest*>& stream_requests() {
    return stream_requests_;
  }

 private:
  enum State { STATE_NONE, STATE_RESOLVE_HOST, STATE_RESOLVE_HOST_COMPLETE };

  State state_;
  IQuicStreamFactory* factory_;
  HostResolver* resolver_;
  const HostPortPair destination_;
  QuicSessionKey session_key_;
  NetLogWithSource net_log_;
  CompletionOnceCallback callback_;
  IQuicSession* session_;

  AddressList address_list_;
  std::unique_ptr<HostResolver::ResolveHostRequest> request_;
  std::set<IQuicStreamRequest*> stream_requests_;
};

IQuicStreamFactory::Job::Job(IQuicStreamFactory* factory,
                             HostResolver* resolver,
                             HostPortPair destination,
                             QuicSessionKey session_key,
                             NetLogWithSource net_log)
    : state_(STATE_RESOLVE_HOST),
      factory_(factory),
      resolver_(resolver),
      destination_(destination),
      session_key_(session_key),
      net_log_(net_log) {}

int IQuicStreamFactory::Job::Run(CompletionOnceCallback callback) {
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    callback_ = std::move(callback);

  return rv > 0 ? OK : rv;
}

int IQuicStreamFactory::Job::DoLoop(int rv) {
  do {
    State state = state_;
    state_ = STATE_NONE;
    switch (state) {
      case STATE_RESOLVE_HOST:
        CHECK_EQ(OK, rv);
        rv = DoResolveHost();
        break;
      case STATE_RESOLVE_HOST_COMPLETE:
        rv = DoResolveHostComplete(rv);
        break;
      default:
        NOTREACHED() << "state: " << state_;
        break;
    }
  } while (state_ != STATE_NONE && rv != ERR_IO_PENDING);
  return rv;
}

int IQuicStreamFactory::Job::DoResolveHost() {
  state_ = STATE_RESOLVE_HOST_COMPLETE;

  request_ = resolver_->CreateRequest(destination_, net_log_, {});

  int rv = request_->Start(base::Bind(
      &IQuicStreamFactory::Job::OnResolveHostComplete, base::Unretained(this)));

  return rv;
}

void IQuicStreamFactory::Job::OnResolveHostComplete(int rv) {
  rv = DoLoop(rv);

  if (callback_) {
    std::move(callback_).Run(rv);
  }
}

int IQuicStreamFactory::Job::DoResolveHostComplete(int rv) {
  if (rv != OK) {
    return rv;
  }

  address_list_ = *request_->GetAddressResults();

  rv = factory_->CreateSession(destination_, session_key_, address_list_,
                               net_log_, &session_);
  if (rv != OK) {
    return rv;
  }

  bnl::result<void> r = session_->Start();
  if (!r) {
    return ERR_FAILED;
  }

  factory_->ActivateSession(session_key_, session_);

  state_ = State::STATE_NONE;

  return OK;
}

void IQuicStreamFactory::Job::AddRequest(IQuicStreamRequest* request) {
  stream_requests_.insert(request);
}

void IQuicStreamFactory::Job::RemoveRequest(IQuicStreamRequest* request) {
  auto request_iter = stream_requests_.find(request);
  DCHECK(request_iter != stream_requests_.end());
  stream_requests_.erase(request_iter);
}

IQuicStreamRequest::IQuicStreamRequest(IQuicStreamFactory* factory)
    : factory_(factory) {}

IQuicStreamRequest::~IQuicStreamRequest() {
  if (factory_ && !callback_.is_null())
    factory_->CancelRequest(this);
}

int IQuicStreamRequest::Request(const HostPortPair& destination,
                                PrivacyMode privacy_mode,
                                const SocketTag& socket_tag,
                                const GURL& url,
                                const NetLogWithSource& net_log,
                                CompletionOnceCallback callback) {
  session_key_ =
      QuicSessionKey(HostPortPair::FromURL(url), privacy_mode, socket_tag);

  int rv = factory_->Create(session_key_, destination, net_log, this);
  if (rv == ERR_IO_PENDING) {
    callback_ = std::move(callback);
  } else {
    factory_ = nullptr;
  }

  if (rv == OK)
    DCHECK(session_);
  return rv;
}

void IQuicStreamRequest::OnRequestComplete(int rv) {
  factory_ = nullptr;
  std::move(callback_).Run(rv);
}

IQuicStreamFactory::IQuicStreamFactory(
    HostResolver* host_resolver,
    ClientSocketFactory* client_socket_factory,
    quic::QuicClock* clock)
    : host_resolver_(host_resolver),
      client_socket_factory_(client_socket_factory),
      clock_(clock),
      alarm_factory_(std::make_unique<QuicChromiumAlarmFactory>(
          base::ThreadTaskRunnerHandle::Get().get(),
          clock_)) {}

IQuicStreamFactory::~IQuicStreamFactory() {
  CloseAllSessions(ERR_ABORTED, quic::QUIC_CONNECTION_CANCELLED);
  while (!all_sessions_.empty()) {
    delete all_sessions_.begin()->first;
    all_sessions_.erase(all_sessions_.begin());
  }
  active_jobs_.clear();
}

int IQuicStreamFactory::Create(const QuicSessionKey& session_key,
                               const HostPortPair& destination,
                               const NetLogWithSource& net_log,
                               IQuicStreamRequest* request) {
  // Use active session for |session_key| if such exists.
  if (!active_sessions_.empty()) {
    auto it = active_sessions_.find(session_key);
    if (it != active_sessions_.end()) {
      IQuicSession* session = it->second;
      request->SetSession(session);
      return OK;
    }
  }

  auto it = active_jobs_.find(session_key);
  if (it != active_jobs_.end()) {
    it->second->AddRequest(request);
    return ERR_IO_PENDING;
  }

  // Pool to active session to |destination| if possible.
  if (!active_sessions_.empty()) {
    for (const auto& key_value : active_sessions_) {
      IQuicSession* session = key_value.second;
      if (destination.Equals(session->Destination())) {
        request->SetSession(session);
        return OK;
      }
    }
  }

  std::unique_ptr<Job> job = std::make_unique<Job>(
      this, host_resolver_, destination, session_key, net_log);
  int rv = job->Run(base::Bind(&IQuicStreamFactory::OnJobComplete,
                               base::Unretained(this), job.get()));

  if (rv == ERR_IO_PENDING) {
    job->AddRequest(request);
    active_jobs_[session_key] = std::move(job);
    return rv;
  }

  if (rv == OK) {
    if (active_sessions_.empty())
      return ERR_QUIC_PROTOCOL_ERROR;
    auto it = active_sessions_.find(session_key);
    DCHECK(it != active_sessions_.end());
    if (it == active_sessions_.end())
      return ERR_QUIC_PROTOCOL_ERROR;
    IQuicSession* session = it->second;
    request->SetSession(session);
  }

  return rv;
}

void IQuicStreamFactory::OnJobComplete(Job* job, int rv) {
  auto iter = active_jobs_.find(job->session_key());
  DCHECK(iter != active_jobs_.end());
  if (rv == OK) {
    auto session_it = active_sessions_.find(job->session_key());
    CHECK(session_it != active_sessions_.end());
    IQuicSession* session = session_it->second;
    for (auto* request : iter->second->stream_requests()) {
      // Do not notify |request| yet.
      request->SetSession(session);
    }
  }

  for (auto* request : iter->second->stream_requests()) {
    request->OnRequestComplete(rv);
  }

  active_jobs_.erase(iter);
}

int IQuicStreamFactory::CreateSession(const HostPortPair destination,
                                      const QuicSessionKey& key,
                                      const AddressList& address_list,
                                      const NetLogWithSource& net_log,
                                      IQuicSession** session) {
  auto it = address_list.begin();
  while (it->address().IsIPv6() && it != address_list.end()) {
    it++;
  }

  DCHECK(it != address_list.end());
  IPEndPoint addr = *it;

  *session = new IQuicSession(destination, addr, clock_, client_socket_factory_,
                              alarm_factory_.get(), this, net_log);

  all_sessions_[*session] = key;  // owning pointer

  return OK;
}

void IQuicStreamFactory::ActivateSession(const QuicSessionKey& key,
                                         IQuicSession* session) {
  active_sessions_[key] = session;
}

void IQuicStreamFactory::CancelRequest(IQuicStreamRequest* request) {
  auto job_iter = active_jobs_.find(request->session_key());
  CHECK(job_iter != active_jobs_.end());
  job_iter->second->RemoveRequest(request);
}

void IQuicStreamFactory::CloseAllSessions(int error,
                                          quic::QuicErrorCode quic_error) {}

}  // namespace iquic
}  // namespace net