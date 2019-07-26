#include "net/base/ip_endpoint.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/task/thread_pool/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/host_port_pair.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/platform/impl/quic_chromium_clock.h"
#include "net/quic/quic_chromium_alarm_factory.h"
#include "net/socket/client_socket_factory.h"

#include "base/test/bind_test_util.h"

#include "net/iquic/iquic_session.h"

#include <bnl/base/error.hpp>
#include <bnl/ip/address.hpp>
#include <bnl/log.hpp>

#include <fmt/ostream.h>
#include <iostream>

#define THROW_SYSTEM(function, errno)                            \
  {                                                              \
    bnl::posix_code code(errno);                                 \
    BNL_LOG_ERROR(logger_, "{}: {}", #function, code.message()); \
                                                                 \
    return code;                                                 \
  }                                                              \
  (void)0

namespace bnl {
namespace log {
class api;
}
}  // namespace bnl

static bnl::ip::address make_address(sockaddr* sockaddr) {
  switch (sockaddr->sa_family) {
    case AF_INET: {
      sockaddr_in* ipv4 = reinterpret_cast<sockaddr_in*>(sockaddr);
      return bnl::ipv4::address(ipv4->sin_addr.s_addr);
    }

    case AF_INET6: {
      const sockaddr_in6* ipv6 =
          reinterpret_cast<const sockaddr_in6*>(sockaddr);

      const uint8_t* bytes =
          reinterpret_cast<const uint8_t*>(ipv6->sin6_addr.s6_addr);

      return bnl::ipv6::address({bytes, bnl::ipv6::address::size});
    }
  }

  assert(false);
  return {};
}

class dns {
 public:
  dns(const bnl::log::api* logger) : logger_(logger) {}

  bnl::result<std::vector<bnl::ip::address>> resolve(bnl::ip::host host) {
    bnl::base::string name(host.name().data(), host.name().size());

    addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;

    addrinfo* results = nullptr;

    int rv = getaddrinfo(name.c_str(), nullptr, &hints, &results);
    if (rv != 0) {
      THROW_SYSTEM(getaddrinfo, rv == EAI_SYSTEM ? errno : rv);
    }

    addrinfo* info = nullptr;

    std::vector<bnl::ip::address> addresses;

    for (info = results; info != nullptr; info = info->ai_next) {
      bnl::ip::address address = make_address(info->ai_addr);
      if (std::find(addresses.begin(), addresses.end(), address) ==
          addresses.end()) {
        addresses.push_back(address);
      }
    }

    freeaddrinfo(results);

    return addresses;
  }

 private:
  const bnl::log::api* logger_;
};

bnl::result<void> run(int argc, char* argv[]) {
  bnl::log::console logger;
  dns dns(&logger);

  bnl::ip::host host(argv[1]);

  std::vector<bnl::ip::address> resolved = BNL_TRY(dns.resolve(host));

  if (resolved.empty()) {
    BNL_LOG_ERROR(&logger, "Failed to resolve {}", host);
    return bnl::errc::unknown;
  }

  BNL_LOG_INFO(&logger,
               "Host {} resolved to the following IP addresses: ", host);

  for (const bnl::ip::address& address : resolved) {
    BNL_LOG_INFO(&logger, "{}", address);
  }

  net::IPAddress address(resolved[0].bytes().data(),
                         resolved[0].bytes().size());

  uint16_t port = static_cast<uint16_t>(std::stoul(argv[2]));
  net::IPEndPoint peer(address, port);

  BNL_LOG_INFO(&logger, "Peer: {}:{}", peer.address().ToString(), peer.port());

  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("iquic_client");
  base::CommandLine::Init(argc, argv);
  base::AtExitManager exit_manager;
  base::MessageLoopForIO message_loop;

  logging::LoggingSettings settings;
  DCHECK(logging::InitLogging(settings));

  net::NetLogWithSource log;

  quic::QuicChromiumClock clock;
  net::QuicChromiumAlarmFactory alarm_factory(
      base::ThreadTaskRunnerHandle::Get().get(), &clock);

  net::ClientSocketFactory* socket_factory =
      net::ClientSocketFactory::GetDefaultFactory();

  net::HostPortPair pair(std::string(host.name().data(), host.name().size()),
                         port);

  BNL_LOG_INFO(&logger, "Pair: {}:{}", pair.host(), pair.port());
  net::iquic::IQuicSession client(pair, peer, &clock, socket_factory,
                                  &alarm_factory, log);

  std::vector<bnl::http3::header> headers;
  bnl::base::buffer body;

  auto exit = base::RunLoop().QuitClosure();

  auto handler = base::BindLambdaForTesting(
      [&](bnl::http3::event event) -> void {
        switch (event) {
          case bnl::http3::event::type::settings:
            break;

          case bnl::http3::event::type::header:
            std::cout << event.header.header << std::endl;
            headers.emplace_back(std::move(event.header.header));
            break;

          case bnl::http3::event::type::body:
            std::cout.write(reinterpret_cast<char*>(event.body.buffer.data()),
                            event.body.buffer.size());
            body = bnl::base::buffer::concat(body, event.body.buffer);

            if (event.body.fin) {
              exit.Run();
            }

            break;
        }
      });

  auto request = BNL_TRY(client.Request(std::move(handler)));

  BNL_TRY(request.header({":method", "GET"}));
  BNL_TRY(request.header({":scheme", "https"}));
  BNL_TRY(request.header({":authority", host.name()}));
  BNL_TRY(request.header({":path", "/index.html"}));

  BNL_TRY(request.start());
  BNL_TRY(request.fin());

  BNL_TRY(client.Start());

  base::RunLoop().Run();

  for (const auto& header : headers) {
    std::cout << header << std::endl;
  }

  std::cout << std::endl;

  std::cout.write(reinterpret_cast<char*>(body.data()),
                  static_cast<std::streamsize>(body.size()));

  return bnl::success();
}

int main(int argc, char* argv[]) {
  bnl::result<void> r = run(argc, argv);

  if (!r) {
    return 1;
  }

  return 0;
}