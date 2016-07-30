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

#include <boost/asio/ssl/error.hpp>

namespace thestral {
namespace socks {

namespace ip = boost::asio::ip;
using namespace std::placeholders;

logging::Logger SocksTcpServer::LOG("SocksTcpServer");

void SocksTcpServer::Start() {
  ip::tcp::resolver resolver(*server_transport_factory_->get_io_service_ptr());
  auto query_flags = ip::tcp::resolver::query::address_configured |
                     ip::tcp::resolver::query::numeric_service |
                     ip::tcp::resolver::query::passive;
  LOG.Info("start listening on %s, port: %u", bind_address_.c_str(),
           bind_port_);
  ip::tcp::resolver::query query(bind_address_, std::to_string(bind_port_),
                                 query_flags);
  ec_type error_code;
  auto iter = resolver.resolve(query, error_code);
  if (error_code) {
    // TODO(richardtsai): error handling
    LOG.Error("failed to resolve address %s, port: %u, reason: %s",
              bind_address_.c_str(), bind_port_, error_code.message().c_str());
    return;
  }

  server_transport_factory_->StartAccept(
      *iter, std::bind(&SocksTcpServer::HandleNewConnection, shared_from_this(),
                       _1, _2));
}

bool SocksTcpServer::HandleNewConnection(
    const ec_type& ec, const std::shared_ptr<TransportBase>& transport) {
  if (ec) {
    LOG.Error("failed to accept a new connection, reason: %s",
              ec.message().c_str());
    if (transport) {
      transport->StartClose();
    }
    // if it is an ssl error, than the network is ok, and we may proceed
    return ec.category() == boost::asio::error::get_ssl_category();
  }

  auto self = shared_from_this();
  auto remote_address = transport->GetRemoteAddress();
  LOG.Info("new incomming connection from %s, port: %u",
           remote_address.host.c_str(), remote_address.port);
  LOG.Debug("receiving auth request packet");
  AuthMethodList::StartCreateFrom(
      transport, [self, transport](const ec_type& ec, AuthMethodList packet) {
        if (ec) {
          LOG.Error("failed to receive auth request packet, reason: %s",
                    ec.message().c_str());
          transport->StartClose();
          return;
        }
        AuthMethodSelectPacket response;
        if (std::find(packet.methods.cbegin(), packet.methods.cend(),
                      AuthMethod::kNoAuth) == packet.methods.cend()) {
          // no supported auth method provided
          LOG.Error("no supported auth method provided by the downstream");
          response.method = AuthMethod::kNotSupported;
          response.StartWriteTo(transport, [transport](const ec_type&, size_t) {
            transport->StartClose();
          });
        } else {
          LOG.Debug("sending auth acknowledgment packet");
          response.method = AuthMethod::kNoAuth;
          response.StartWriteTo(
              transport, std::bind(&SocksTcpServer::ReceiveRequestPacket, self,
                                   _1, transport));
        }
      });

  return true;
}

void SocksTcpServer::ReceiveRequestPacket(
    const ec_type& ec, const std::shared_ptr<TransportBase>& transport) {
  if (ec) {
    LOG.Error("failed to send auth acknowledgment packet, reason: %s",
              ec.message().c_str());
    transport->StartClose();
    return;
  }

  auto self = shared_from_this();
  LOG.Debug("receiving SOCKS request packet");
  RequestPacket::StartCreateFrom(
      transport, [self, transport](const ec_type& ec, RequestPacket packet) {
        if (ec) {
          LOG.Error("failed to receive SOCKS request packet, reason: %s",
                    ec.message().c_str());
          transport->StartClose();
        } else if (packet.header.command != Command::kConnect) {
          LOG.Error("downstream requested an unsupported command %s",
                    to_string(packet.header.command).c_str());
          self->ResponseError(ResponseCode::kCommandNotSupported, transport);
        } else {
          // TODO(richardtsai): ACL on request
          self->HandleRequest(packet, transport);
        }
      });
}

void SocksTcpServer::HandleRequest(
    RequestPacket request, const std::shared_ptr<TransportBase>& downstream) {
  Address downstream_address = downstream->GetRemoteAddress();
  LOG.Info(
      "establishing connection to %s, port %u, "
      "on behalf of downstream %s, port %u",
      request.body.host.c_str(), request.body.port,
      downstream_address.host.c_str(), downstream_address.port);

  auto self = shared_from_this();
  upstream_factory_->StartRequest(
      request.body,
      [self, request, downstream](
          const ec_type& ec, const std::shared_ptr<TransportBase>& upstream) {
        if (ec) {
          // TODO(richardtsai): handle more kinds of errors
          LOG.Error("failed to establish connection, reason: %s",
                    ec.message().c_str());
          self->ResponseError(ResponseCode::kConnectionRefused, downstream);
        } else {
          ResponsePacket response;
          response.header.response_code = ResponseCode::kSuccess;
          response.body = upstream->GetLocalAddress();
          LOG.Debug("sending SOCKS response to downstream");
          response.StartWriteTo(
              downstream,
              [self, request, downstream, upstream](const ec_type& ec, size_t) {
                if (ec) {
                  LOG.Error("failed to send SOCKS response, reason: %s",
                            ec.message().c_str());
                  downstream->StartClose();
                  upstream->StartClose();
                } else {
                  // start relay in both direction
                  Address downstream_address = downstream->GetRemoteAddress();
                  LOG.Info(
                      "connection established to %s, port: %u, "
                      "on behalf of downstream %s, port: %u, start relaying",
                      request.body.host.c_str(), request.body.port,
                      downstream_address.host.c_str(), downstream_address.port);
                  self->StartRelay(downstream, upstream);
                  self->StartRelay(upstream, downstream);
                }
              });
        }
      });
}

void SocksTcpServer::ResponseError(
    ResponseCode response_code,
    const std::shared_ptr<TransportBase>& transport) {
  ResponsePacket response;
  response.header.response_code = response_code;
  response.StartWriteTo(transport, [transport](const ec_type&, size_t) {
    transport->StartClose();
  });
}

void SocksTcpServer::StartRelay(const std::shared_ptr<TransportBase>& from,
                                const std::shared_ptr<TransportBase>& to) {
  auto self = shared_from_this();
  auto buf = std::make_shared<std::array<char, kRelayBufferSize>>();
  from->StartRead(
      *buf,
      [self, buf, from, to](const ec_type& error_code, size_t bytes_read) {
        if (bytes_read == 0 || error_code) {
          // TODO(richardtsai): finer control on shutdown
          // TODO(richardtsai): log this event
          from->StartClose();
          to->StartClose();
        } else {
          to->StartWrite(
              buf->data(), bytes_read,
              std::bind(&SocksTcpServer::StartRelay, self, from, to));
        }
      },
      true /* allow short read */);
}

}  // namespace socks
}  // namespace thestral
