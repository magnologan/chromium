#pragma once

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/callback.h"
#include "net/base/net_export.h"

#include <bnl/base/buffers.hpp>
#include <bnl/result.hpp>

#include <memory>

namespace bnl {
namespace log {
class api;
}
}

namespace net {

class IPEndPoint;
class DatagramClientSocket;
class ClientSocketFactory;
class NetLogWithSource;
class IOBufferWithSize;

namespace iquic {

class SendStorage;
class RecvStorage;

class NET_EXPORT_PRIVATE Socket {
 public:
  Socket(const IPEndPoint& peer,
         ClientSocketFactory* socket_factory,
         const NetLogWithSource& net_log, const bnl::log::api *logger);
  ~Socket();     

  bnl::result<void> Send();
  bnl::result<bnl::base::buffer> Recv();

  void Add(bnl::base::buffer buffer);

  using SendHandler = base::RepeatingCallback<void(bnl::result<void>)>;
  using RecvHandler = base::RepeatingCallback<void(bnl::result<void>)>;

  void SetOnSend(SendHandler handler);
  void SetOnRecv(RecvHandler handler);
  
  IPEndPoint Local() const;
  IPEndPoint Peer() const;

  void OnSend(int rv);
  void OnRecv(int rv);

 private:

 private:
  bnl::base::buffers send_buffers_;
  bnl::base::buffers recv_buffers_;

  SendHandler on_send_;
  RecvHandler on_recv_;

  bool send_in_progress_ = false;
  bool recv_in_progress_ = false;

  scoped_refptr<SendStorage> send_storage_;
  scoped_refptr<RecvStorage> recv_storage_;

  std::unique_ptr<DatagramClientSocket> socket_;

  const bnl::log::api *logger_;
  base::WeakPtrFactory<Socket> weak_factory_;
};

}  // namespace iquic
}  // namespace net