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

namespace asio = boost::asio;
namespace ip = boost::asio::ip;

TcpTransport::TcpTransport(asio::io_service& io_service) : socket_(io_service) {
  socket_.set_option(ip::tcp::no_delay(true));
  socket_.set_option(ip::tcp::socket::reuse_address(true));
}

void TcpTransport::StartRead(const boost::asio::mutable_buffers_1& buf,
                             ReadCallbackType callback) {
  boost::asio::async_read(socket_, buf, callback);
}

void TcpTransport::StartWrite(const boost::asio::const_buffers_1& buf,
                              WriteCallbackType callback) {
  boost::asio::async_write(socket_, buf, callback);
}

void TcpTransport::StartClose(CloseCallbackType callback) {
  ec_type ec;
  socket_.shutdown(ip::tcp::socket::shutdown_both, ec);
  if (ec) {
    socket_.close();
  } else {
    socket_.close(ec);
  }
  callback(ec);
}

void TcpTransportFactory::StartAccept(EndpointType endpoint,
                                      AcceptCallbackType callback) {
  auto acceptor =
      std::make_shared<ip::tcp::acceptor>(*io_service_ptr_, endpoint);
  acceptor->set_option(ip::tcp::no_delay(true));
  acceptor->set_option(ip::tcp::socket::reuse_address(true));
  DoAccept(acceptor, callback);
}

void TcpTransportFactory::StartConnect(EndpointType endpoint,
                                       ConnectCallbackType callback) {
  std::shared_ptr<TcpTransport> transport(new TcpTransport(*io_service_ptr_));
  transport->socket_.async_connect(
      endpoint, std::bind(callback, transport, std::placeholders::_1));
}

void TcpTransportFactory::DoAccept(
    const std::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
    AcceptCallbackType callback) {
  std::shared_ptr<TcpTransport> transport(new TcpTransport(*io_service_ptr_));
  acceptor->async_accept(transport->socket_, [=](const ec_type& ec) {
    if (callback(transport, ec)) {
      DoAccept(acceptor, callback);  // recursively accept more connections
    }
  });
}

}  // namespace thestral
