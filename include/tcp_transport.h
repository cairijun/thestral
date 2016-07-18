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
/// Defines a transport on plain TCP protocol.
#ifndef THESTRAL_TCP_TRANSPORT_H_
#define THESTRAL_TCP_TRANSPORT_H_

#include <memory>

#include <boost/asio.hpp>

#include "base.h"
#include "common.h"

namespace thestral {

/// Transport on plain TCP protocol.
class TcpTransport : public TransportBase {
  friend class TcpTransportFactory;

 public:
  Address GetLocalAddress() const override;
  void StartRead(const boost::asio::mutable_buffers_1& buf,
                 ReadCallbackType callback,
                 bool allow_short_read = false) override;
  void StartWrite(const boost::asio::const_buffers_1& buf,
                  WriteCallbackType callback) override;
  void StartClose(CloseCallbackType callback) override;

 private:
  boost::asio::ip::tcp::socket socket_;

  explicit TcpTransport(boost::asio::io_service& io_service);
};

/// Transport factory for TcpTransport.
class TcpTransportFactory
    : public TransportFactoryBase<boost::asio::ip::tcp::endpoint>,
      public std::enable_shared_from_this<TcpTransportFactory> {
 public:
  TcpTransportFactory(const TcpTransportFactory&) = delete;
  TcpTransportFactory& operator=(const TcpTransportFactory&) = delete;

  static std::shared_ptr<TcpTransportFactory> New(
      const std::shared_ptr<boost::asio::io_service>& io_service_ptr) {
    return std::shared_ptr<TcpTransportFactory>(
        new TcpTransportFactory(io_service_ptr));
  }

  void StartAccept(EndpointType endpoint, AcceptCallbackType callback) override;
  void StartConnect(EndpointType endpoint,
                    ConnectCallbackType callback) override;

  /// Synchronously tries connecting to a remote peer with a set of endpoints.
  template <typename Iter>
  std::shared_ptr<TransportBase> TryConnect(Iter& iter, ec_type& error_code) {
    std::shared_ptr<TcpTransport> transport(new TcpTransport(*io_service_ptr_));
    iter = boost::asio::connect(transport->socket_, iter, error_code);
    return transport;
  }

  std::shared_ptr<boost::asio::io_service> get_io_service_ptr() const override {
    return io_service_ptr_;
  }

 private:
  const std::shared_ptr<boost::asio::io_service> io_service_ptr_;

  explicit TcpTransportFactory(
      const std::shared_ptr<boost::asio::io_service>& io_service_ptr)
      : io_service_ptr_(io_service_ptr) {}

  void DoAccept(const std::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
                AcceptCallbackType callback);
};

}  // namespace thestral

#endif /* ifndef THESTRAL_TCP_TRANSPORT_H_ */
