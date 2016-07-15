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

namespace thestral {

/// Transport on plain TCP protocol.
class TcpTransport : public TransportBase {
  friend class TcpTransportFactory;

 public:
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
    : public TransportFactoryBase<boost::asio::ip::tcp::endpoint> {
 public:
  explicit TcpTransportFactory(
      const std::shared_ptr<boost::asio::io_service>& io_service_ptr)
      : io_service_ptr_(io_service_ptr) {}

  void StartAccept(EndpointType endpoint, AcceptCallbackType callback) override;
  void StartConnect(EndpointType endpoint,
                    ConnectCallbackType callback) override;

 private:
  const std::shared_ptr<boost::asio::io_service> io_service_ptr_;

  void DoAccept(const std::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
                AcceptCallbackType callback);
};

}  // namespace thestral

#endif /* ifndef THESTRAL_TCP_TRANSPORT_H_ */
