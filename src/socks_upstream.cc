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
/// Implements the classes for the upstream part of SOCKS protocol.
#include "socks_upstream.h"

namespace thestral {
namespace socks {

namespace ip = boost::asio::ip;

void SocksTcpUpstreamFactory::StartRequest(const Address& endpoint,
                                           RequestCallbackType callback) {
  ec_type ec;
  std::shared_ptr<TransportBase> transport;

  // if upstream_endpoint_ has not been initialized, we have to resolve the
  // upstream host address first
  if (!is_upstream_endpoint_init_) {
    std::lock_guard<std::mutex> lock(upstream_endpoint_init_mtx_);

    if (!is_upstream_endpoint_init_) {
      // need to resolve the upstream host
      // synchronous operations are used to simplify locking control
      ip::tcp::resolver resolver(*transport_factory_->get_io_service_ptr());
      ip::tcp::resolver::query query(
          upstream_host_, std::to_string(upstream_port_),
          ip::tcp::resolver::query::address_configured |
              ip::tcp::resolver::query::numeric_service);

      auto iter = resolver.resolve(query, ec);
      if (ec) {
        callback(ec, nullptr);
        return;
      }

      transport = transport_factory_->TryConnect(iter, ec);
      if (ec) {
        transport->StartClose();
        callback(ec, nullptr);
        return;
      }

      upstream_endpoint_ = *iter;
      is_upstream_endpoint_init_ = true;
    }
  }

  if (transport) {  // connection to upstream established already
    SendAuthRequest(endpoint, transport, callback);
  } else {  // not connected yet
    auto self = shared_from_this();
    transport_factory_->StartConnect(
        upstream_endpoint_,
        [self, endpoint, callback](const ec_type& ec,
                                   std::shared_ptr<TransportBase> transport) {
          if (ec) {
            transport->StartClose();
            callback(ec, nullptr);  // don't care about the closing result
          } else {
            self->SendAuthRequest(endpoint, transport, callback);
          }
        });
  }
}

void SocksTcpUpstreamFactory::SendAuthRequest(
    const Address& endpoint, std::shared_ptr<TransportBase> transport,
    RequestCallbackType callback) const {
  AuthMethodList packet;
  packet.methods.push_back(AuthMethod::kNoAuth);
  auto self = shared_from_this();
  packet.StartWriteTo(transport, [self, endpoint, transport, callback](
                                     const ec_type& ec, size_t) {
    if (ec) {
      transport->StartClose();
      callback(ec, nullptr);
      return;
    }
    AuthMethodSelectPacket::StartCreateFrom(
        transport, [self, endpoint, transport, callback](
                       const ec_type& ec, AuthMethodSelectPacket packet) {
          if (ec || packet.method != AuthMethod::kNoAuth) {
            transport->StartClose();
            callback(ec, nullptr);
          } else {
            self->SendSocksRequest(endpoint, transport, callback);
          }
        });
  });
}

void SocksTcpUpstreamFactory::SendSocksRequest(
    const Address& endpoint, std::shared_ptr<TransportBase> transport,
    RequestCallbackType callback) const {
  RequestPacket packet;
  packet.header.command = Command::kConnect;
  packet.body = endpoint;

  auto self = shared_from_this();
  packet.StartWriteTo(
      transport, [self, transport, callback](const ec_type& ec, size_t) {
        if (ec) {
          transport->StartClose();
          callback(ec, nullptr);
          return;
        }
        ResponsePacket::StartCreateFrom(
            transport, [self, transport, callback](const ec_type& ec,
                                                   ResponsePacket packet) {
              if (ec || packet.header.response_code != ResponseCode::kSuccess) {
                transport->StartClose();
                callback(ec, nullptr);
              } else {
                // the bound address of the resulting transport should be the
                // one reported by the server rather than the one of the
                // underlying transport
                auto wrapped_transport =
                    std::make_shared<detail::SocksTransportWrapper>(
                        transport, packet.body);
                callback(ec, wrapped_transport);  // finally, success!
              }
            });
      });
}

}  // namespace socks
}  // namespace thestral
