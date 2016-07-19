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
/// Implements a transport on plain TCP protocol.
#include "tcp_transport.h"

#include <functional>

namespace thestral {
namespace impl {

namespace asio = boost::asio;
namespace ip = boost::asio::ip;

TcpTransportImpl::TcpTransportImpl(asio::io_service& io_service)
    : socket_(io_service) {
  socket_.set_option(ip::tcp::no_delay(true));
  socket_.set_option(ip::tcp::socket::reuse_address(true));
}

Address TcpTransportImpl::GetLocalAddress() const {
  ec_type ec;
  auto endpoint = socket_.local_endpoint(ec);
  Address address;

  if (ec) {
    // TODO(richardtsai): better way to indicate error?
    address.type = static_cast<AddressType>(0xff);  // invalid address
  } else {
    auto asio_addr = endpoint.address();
    if (asio_addr.is_v4()) {
      auto asio_addr_bytes = asio_addr.to_v4().to_bytes();
      address.type = AddressType::kIPv4;
      address.host.assign(asio_addr_bytes.cbegin(), asio_addr_bytes.cend());
      address.port = endpoint.port();
    } else if (asio_addr.is_v6()) {
      auto asio_addr_bytes = asio_addr.to_v6().to_bytes();
      address.type = AddressType::kIPv6;
      address.host.assign(asio_addr_bytes.cbegin(), asio_addr_bytes.cend());
      address.port = endpoint.port();
    } else {
      address.type = static_cast<AddressType>(0xff);  // invalid address
    }
  }

  return address;
}

void TcpTransportImpl::StartRead(const boost::asio::mutable_buffers_1& buf,
                                 ReadCallbackType callback,
                                 bool allow_short_read) {
  if (allow_short_read) {
    socket_.async_read_some(buf, callback);
  } else {
    boost::asio::async_read(socket_, buf, callback);
  }
}

void TcpTransportImpl::StartWrite(const boost::asio::const_buffers_1& buf,
                                  WriteCallbackType callback) {
  boost::asio::async_write(socket_, buf, callback);
}

void TcpTransportImpl::StartClose(CloseCallbackType callback) {
  ec_type ec;
  socket_.shutdown(ip::tcp::socket::shutdown_both, ec);
  if (ec) {
    socket_.close();
  } else {
    socket_.close(ec);
  }
  callback(ec);
}

void TcpTransportFactoryImpl::StartAccept(EndpointType endpoint,
                                          AcceptCallbackType callback) {
  auto acceptor =
      std::make_shared<ip::tcp::acceptor>(*io_service_ptr_, endpoint);
  acceptor->set_option(ip::tcp::no_delay(true));
  acceptor->set_option(ip::tcp::socket::reuse_address(true));
  DoAccept(acceptor, callback);
}

void TcpTransportFactoryImpl::StartConnect(EndpointType endpoint,
                                           ConnectCallbackType callback) {
  auto transport = NewTransport();
  transport->GetUnderlyingSocket().async_connect(
      endpoint, std::bind(callback, std::placeholders::_1, transport));
}

void TcpTransportFactoryImpl::DoAccept(
    const std::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
    AcceptCallbackType callback) {
  auto transport = NewTransport();
  auto self = shared_from_this();
  acceptor->async_accept(
      transport->GetUnderlyingSocket(),
      [self, acceptor, callback, transport](const ec_type& ec) {
        if (callback(ec, transport)) {
          // recursively accept more connections
          self->DoAccept(acceptor, callback);
        }
      });
}
std::shared_ptr<TcpTransport> TcpTransportFactoryImpl::TryConnect(
    boost::asio::ip::tcp::resolver::iterator& iter, ec_type& error_code) {
  auto transport = NewTransport();
  boost::asio::connect(transport->GetUnderlyingSocket(), iter, error_code);
  return transport;
}

}  // namespace impl
}  // namespace thestral
