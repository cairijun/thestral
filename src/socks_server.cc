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
/// Implements the SOCKS server.
#include "socks_server.h"

#include <algorithm>
#include <array>
#include <functional>

namespace thestral {
namespace socks {

namespace ip = boost::asio::ip;
using namespace std::placeholders;

void SocksTcpServer::Start() {
  ip::tcp::resolver resolver(*server_transport_factory_->get_io_service_ptr());
  auto query_flags = ip::tcp::resolver::query::address_configured |
                     ip::tcp::resolver::query::numeric_service |
                     ip::tcp::resolver::query::passive;
  ip::tcp::resolver::query query(bind_address_, std::to_string(bind_port_),
                                 query_flags);
  ec_type error_code;
  auto iter = resolver.resolve(query, error_code);
  if (error_code) {
    // TODO(richardtsai): error handling
    return;
  }

  server_transport_factory_->StartAccept(
      *iter, std::bind(&SocksTcpServer::HandleNewConnection, shared_from_this(),
                       _1, _2));
}

bool SocksTcpServer::HandleNewConnection(
    const ec_type& ec, std::shared_ptr<TransportBase> transport) {
  if (ec) {
    transport->StartClose();
    return false;
  }

  auto self = shared_from_this();
  AuthMethodList::StartCreateFrom(
      transport, [self, transport](const ec_type& ec, AuthMethodList packet) {
        if (ec) {
          transport->StartClose();
          return;
        }
        AuthMethodSelectPacket response;
        if (std::find(packet.methods.cbegin(), packet.methods.cend(),
                      AuthMethod::kNoAuth) == packet.methods.cend()) {
          // no supported auth method provided
          response.method = AuthMethod::kNotSupported;
          response.StartWriteTo(transport, [transport](const ec_type&, size_t) {
            transport->StartClose();
          });
        } else {
          response.method = AuthMethod::kNoAuth;
          response.StartWriteTo(
              transport, std::bind(&SocksTcpServer::ReceiveRequestPacket, self,
                                   _1, transport));
        }
      });

  return true;
}

void SocksTcpServer::ReceiveRequestPacket(
    const ec_type& ec, std::shared_ptr<TransportBase> transport) {
  if (ec) {
    transport->StartClose();
    return;
  }

  auto self = shared_from_this();
  RequestPacket::StartCreateFrom(
      transport, [self, transport](const ec_type& ec, RequestPacket packet) {
        if (ec) {
          transport->StartClose();
        } else if (packet.header.command != Command::kConnect) {
          self->ResponseError(ResponseCode::kCommandNotSupported, transport);
        } else {
          // TODO(richardtsai): ACL on request
          self->HandleRequest(packet, transport);
        }
      });
}

void SocksTcpServer::HandleRequest(RequestPacket request,
                                   std::shared_ptr<TransportBase> downstream) {
  auto self = shared_from_this();
  upstream_factory_->StartRequest(
      request.body,
      [self, downstream](const ec_type& ec,
                         std::shared_ptr<TransportBase> upstream) {
        if (ec) {
          // TODO(richardtsai): handle more kinds of errors
          self->ResponseError(ResponseCode::kConnectionRefused, downstream);
        } else {
          ResponsePacket response;
          response.header.response_code = ResponseCode::kSuccess;
          response.body = upstream->GetLocalAddress();
          response.StartWriteTo(downstream, [self, downstream, upstream](
                                                const ec_type& ec, size_t) {
            if (ec) {
              downstream->StartClose();
              upstream->StartClose();
            } else {
              // start relay in both direction
              self->StartRelay(downstream, upstream);
              self->StartRelay(upstream, downstream);
            }
          });
        }
      });
}

void SocksTcpServer::ResponseError(ResponseCode response_code,
                                   std::shared_ptr<TransportBase> transport) {
  ResponsePacket response;
  response.header.response_code = response_code;
  response.StartWriteTo(transport, [transport](const ec_type&, size_t) {
    transport->StartClose();
  });
}

void SocksTcpServer::StartRelay(std::shared_ptr<TransportBase> from,
                                std::shared_ptr<TransportBase> to) {
  auto self = shared_from_this();
  auto buf = std::make_shared<std::array<char, kRelayBufferSize>>();
  from->StartRead(
      *buf,
      [self, buf, from, to](const ec_type& error_code, size_t bytes_read) {
        if (bytes_read == 0 || error_code) {
          from->StartClose();
          to->StartClose();
        } else {
          to->StartWrite(
              buf->data(), bytes_read,
              std::bind(&SocksTcpServer::StartRelay, self, from, to));
        }
      },
      true  /* allow short read */);
}

}  // namespace socks
}  // namespace thestral
