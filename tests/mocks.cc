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
/// Implements mock classes for testing.
#include "mocks.h"

#include <algorithm>
#include <array>
#include <functional>

#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>

namespace thestral {
namespace testing {

namespace asio = boost::asio;

std::shared_ptr<MockTransport> MockTransport::New(
    const std::shared_ptr<boost::asio::io_service>& io_service_ptr,
    const std::string& read_buf) {
  auto p = std::make_shared<MockTransport>();
  p->read_buf = read_buf;
  p->io_service_ptr = io_service_ptr;
  return p;
}

void MockTransport::StartRead(const asio::mutable_buffers_1& buf,
                              const ReadCallbackType& callback,
                              bool allow_short_read) {
  auto len = asio::buffer_copy(buf, asio::buffer(read_buf));
  read_buf = read_buf.substr(len);
  if (!allow_short_read && len < asio::buffer_size(buf)) {
    ec = asio::error::make_error_code(asio::error::misc_errors::eof);
  }
  io_service_ptr->post(std::bind(callback, ec, len));
}

void MockTransport::StartWrite(const asio::const_buffers_1& buf,
                               const WriteCallbackType& callback) {
  auto p = asio::buffer_cast<const char*>(buf);
  write_buf.append(p, p + asio::buffer_size(buf));
  io_service_ptr->post(std::bind(callback, ec, asio::buffer_size(buf)));
}

void MockTransport::StartClose(const CloseCallbackType& callback) {
  if (closed) {
    io_service_ptr->post(std::bind(callback, ec));
  } else {
    ec = boost::asio::error::make_error_code(
        boost::asio::error::basic_errors::bad_descriptor);
    closed = true;
    io_service_ptr->post(std::bind(callback, ec_type()));
  }
}

std::shared_ptr<MockTransport> MockTcpTransportFactory::NewMockTransport(
    const std::string& read_buf) {
  auto transport = MockTransport::New(io_service_ptr_, read_buf);
  transports_.push(transport);
  return transport;
}

MockTcpTransportFactory::EndpointType MockTcpTransportFactory::PopEndpoint() {
  auto endpoint = endpoints_.front();
  endpoints_.pop();
  return endpoint;
}

void MockTcpTransportFactory::StartAccept(EndpointType endpoint,
                                          const AcceptCallbackType& callback) {
  endpoints_.push(endpoint);
  io_service_ptr_->post(
      std::bind(&MockTcpTransportFactory::AcceptOne, this, callback));
}

void MockTcpTransportFactory::AcceptOne(const AcceptCallbackType& callback) {
  if (transports_.empty()) {
    callback(
        boost::asio::error::make_error_code(boost::asio::error::bad_descriptor),
        nullptr);
  } else {
    auto transport = transports_.front();
    transports_.pop();
    if (callback(ec_type(), transport)) {
      io_service_ptr_->post(
          std::bind(&MockTcpTransportFactory::AcceptOne, this, callback));
    }
  }
}

void MockTcpTransportFactory::StartConnect(
    EndpointType endpoint, const ConnectCallbackType& callback) {
  endpoints_.push(endpoint);
  auto transport = transports_.front();
  transports_.pop();
  io_service_ptr_->post(std::bind(callback, ec_type(), transport));
}

std::shared_ptr<TransportBase> MockTcpTransportFactory::TryConnect(
    boost::asio::ip::tcp::resolver::iterator& iter, ec_type& error_code) {
  endpoints_.push(*iter);
  auto transport = transports_.front();
  transports_.pop();
  return transport;
}

std::shared_ptr<MockTransport> MockUpstreamFactory::NewMockTransport(
    const std::string& read_buf) {
  auto transport = MockTransport::New(io_service_ptr_, read_buf);
  transports_.push(transport);
  return transport;
}

Address MockUpstreamFactory::PopAddress() {
  auto address = addresses_.front();
  addresses_.pop();
  return address;
}

void MockUpstreamFactory::StartRequest(const Address& endpoint,
                                       const RequestCallbackType& callback) {
  addresses_.push(endpoint);
  auto transport = transports_.front();
  transports_.pop();
  io_service_ptr_->post(std::bind(callback, ec_type(), transport));
}

MockServer::MockServer()
    : acceptor_(io_service_,
                asio::ip::tcp::endpoint(asio::ip::tcp::v4(), kPort)) {
  t_.reset(new std::thread(std::bind(&MockServer::Run, this)));
}

MockServer::~MockServer() {
  acceptor_.close();
  io_service_.stop();
  t_->join();
}

void MockServer::Run() {
  AcceptOne();
  io_service_.run();
}

void MockServer::AcceptOne() {
  auto s = std::make_shared<asio::ip::tcp::socket>(io_service_);
  acceptor_.async_accept(*s, [=](const ec_type& ec) {
    if (!ec) {
      this->ServeOne(s);
      this->Run();
    }
  });
}

void MockServer::ServeOne(const std::shared_ptr<asio::ip::tcp::socket>& s) {
  auto buf = std::make_shared<std::array<unsigned char, 8192>>();
  s->async_read_some(asio::buffer(*buf), [=](const ec_type& ec,
                                             size_t n_bytes) {
    if (ec) {
      s->shutdown(s->shutdown_send);
      s->close();
    } else {
      s->async_write_some(asio::buffer(*buf, n_bytes),
                          [this, s, buf](const ec_type& ec, size_t n_bytes) {
                            if (ec) {
                              s->shutdown(s->shutdown_receive);
                              s->close();
                            } else {
                              ServeOne(s);
                            }
                          });
    }
  });
}

}  // namespace testing
}  // namespace thestral
