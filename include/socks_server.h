// Copyright 2016 Richard Tsai
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/// @file
/// Defines the SOCKS server.
#ifndef THESTRAL_SOCKS_SERVER_H_
#define THESTRAL_SOCKS_SERVER_H_

#include <memory>
#include <string>

#include <boost/asio.hpp>

#include "base.h"
#include "logging.h"
#include "socks.h"
#include "socks_upstream.h"
#include "tcp_transport.h"

namespace thestral {
namespace socks {

/// A server implementing the TCP part of SOCKS protocol.
class SocksTcpServer : public ServerBase,
                       public std::enable_shared_from_this<SocksTcpServer> {
 public:
  SocksTcpServer(const SocksTcpServer&) = delete;
  SocksTcpServer& operator=(const SocksTcpServer&) = delete;

  /// Factory method to create a SocksTcpServer from a given tcp transport
  /// factory and a upstream transport, listening on a given tcp endpoint.
  static std::shared_ptr<SocksTcpServer> New(
      const std::string& bind_address, uint16_t bind_port,
      const std::shared_ptr<TcpTransportFactory>& server_transport_factory,
      const std::shared_ptr<UpstreamFactoryBase>& upstream_factory) {
    return std::shared_ptr<SocksTcpServer>(new SocksTcpServer(
        bind_address, bind_port, server_transport_factory, upstream_factory));
  }

  void Start() override;

 private:
  /// Buffer size when relaying data. A large buffer would result in memory
  /// inefficiency when there are lots of connection, while a small buffer might
  /// lead to increase in delay.
  constexpr static size_t kRelayBufferSize = 0x4000;

  static logging::Logger LOG;

  SocksTcpServer(
      const std::string& bind_address, uint16_t bind_port,
      const std::shared_ptr<TcpTransportFactory>& server_transport_factory,
      const std::shared_ptr<UpstreamFactoryBase>& upstream_factory)
      : bind_address_(bind_address),
        bind_port_(bind_port),
        server_transport_factory_(server_transport_factory),
        upstream_factory_(upstream_factory) {}

  /// Receives and replies auth packet on a new connection. Returns `false` when
  /// there is no need to accept more connections.
  bool HandleNewConnection(const ec_type& ec,
                           const std::shared_ptr<TransportBase>& transport);
  /// Receives the request packet from the client and performs some checks.
  void ReceiveRequestPacket(const ec_type& ec,
                            const std::shared_ptr<TransportBase>& transport);
  /// Establishes the upstream connection from a given request.
  void HandleRequest(RequestPacket request,
                     const std::shared_ptr<TransportBase>& transport);
  /// Sends a ResponsePacket with a response code to the client than close the
  /// transport.
  void ResponseError(ResponseCode response_code,
                     const std::shared_ptr<TransportBase>& transport);
  /// Relays data from a transport to another transport in a single direction.
  void StartRelay(const std::shared_ptr<TransportBase>& from,
                  const std::shared_ptr<TransportBase>& to);

  const std::string& bind_address_;
  const uint16_t bind_port_;
  const std::shared_ptr<TcpTransportFactory> server_transport_factory_;
  const std::shared_ptr<UpstreamFactoryBase> upstream_factory_;
};

}  // namespace socks
}  // namespace thestral
#endif /* ifndef THESTRAL_SOCKS_SERVER_H_ */
