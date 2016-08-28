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

namespace thestral {

std::shared_ptr<TcpTransportFactory> TcpTransportFactory::New(
    const std::shared_ptr<boost::asio::io_service>& io_service_ptr) {
  return impl::TcpTransportFactoryImpl::New(io_service_ptr);
}

namespace impl {

namespace asio = boost::asio;
namespace ip = boost::asio::ip;

TcpTransportImpl::TcpTransportImpl(asio::io_service& io_service)
    : socket_(io_service) {}

Address TcpTransportImpl::GetLocalAddress() const {
  return Address::FromAsioEndpoint(socket_.local_endpoint());
}

Address TcpTransportImpl::GetRemoteAddress() const {
  return Address::FromAsioEndpoint(socket_.remote_endpoint());
}

void TcpTransportImpl::StartRead(const boost::asio::mutable_buffers_1& buf,
                                 const ReadCallbackType& callback,
                                 bool allow_short_read) {
  if (allow_short_read) {
    socket_.async_read_some(buf, callback);
  } else {
    boost::asio::async_read(socket_, buf, callback);
  }
}

void TcpTransportImpl::StartWrite(const boost::asio::const_buffers_1& buf,
                                  const WriteCallbackType& callback) {
  boost::asio::async_write(socket_, buf, callback);
}

void TcpTransportImpl::StartClose(const CloseCallbackType& callback) {
  ec_type ec;
  socket_.shutdown(ip::tcp::socket::shutdown_both, ec);
  if (ec) {
    socket_.close();
  } else {
    socket_.close(ec);
  }
  callback(ec);
}

logging::Logger TcpTransportFactoryImpl::LOG("TcpTransportFactoryImpl");

void TcpTransportFactoryImpl::StartAccept(EndpointType endpoint,
                                          const AcceptCallbackType& callback) {
  LOG.Debug("start accepting");
  auto acceptor =
      std::make_shared<ip::tcp::acceptor>(*io_service_ptr_, endpoint);
  acceptor->set_option(ip::tcp::no_delay(true));
  acceptor->set_option(ip::tcp::socket::reuse_address(true));
  last_acceptor_ = acceptor;
  DoAccept(acceptor, callback);
}

void TcpTransportFactoryImpl::StartConnect(
    EndpointType endpoint, const ConnectCallbackType& callback) {
  auto transport = NewTransport();
  auto self = shared_from_this();
  LOG.Debug("start connecting");
  transport->GetUnderlyingSocket().async_connect(
      endpoint, [transport, self, callback](const ec_type& ec) {
        if (ec) {
          self->LOG.Debug("transport returning an error: %s",
                          ec.message().c_str());
          transport->StartClose();
          callback(ec, nullptr);
        } else {
          self->LOG.Debug("connection established");
          transport->GetUnderlyingSocket().set_option(ip::tcp::no_delay(true));
          callback(ec, transport);
        }
      });
}

void TcpTransportFactoryImpl::DoAccept(
    const std::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
    const AcceptCallbackType& callback) {
  auto transport = NewTransport();
  auto self = shared_from_this();
  LOG.Debug("waiting for one connection");
  acceptor->async_accept(
      transport->GetUnderlyingSocket(),
      [self, acceptor, callback, transport](const ec_type& ec) {
        bool should_stop = false;
        if (ec) {
          self->LOG.Debug("acceptor returning an error: %s, stop accepting",
                          ec.message().c_str());
          should_stop = true;
          transport->StartClose();
        } else {
          self->LOG.Debug("one connection accepted");
        }
        // even if `should_stop` is true, we still need to report the error
        // to the upper layer(s)
        if (callback(ec, ec ? nullptr : transport) && !should_stop) {
          // recursively accept more connections
          self->DoAccept(acceptor, callback);
        } else if (should_stop) {
          self->LOG.Debug("give up accepting more connections");
        } else {
          self->LOG.Debug("upper layer gave up accepting more connections");
        }
      });
}

std::shared_ptr<TransportBase> TcpTransportFactoryImpl::TryConnect(
    boost::asio::ip::tcp::resolver::iterator& iter, ec_type& error_code) {
  auto transport = NewTransport();
  LOG.Debug("start connecting");
  boost::asio::connect(transport->GetUnderlyingSocket(), iter, error_code);
  if (error_code) {
    LOG.Debug("transport returning an error: %s", error_code.message().c_str());
    transport->StartClose();
    return nullptr;
  } else {
    LOG.Debug("connection established");
    return transport;
  }
}

}  // namespace impl
}  // namespace thestral
