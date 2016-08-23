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
/// Defines classes for ssl.
#ifndef THESTRAL_SSL_H_
#define THESTRAL_SSL_H_

#include <memory>
#include <string>
#include <utility>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include "base.h"
#include "logging.h"
#include "tcp_transport.h"

namespace thestral {
namespace ssl {

class SslTransportFactoryBuilder;
class SslTransportFactoryImpl;

namespace impl {
/// A TcpTransport wrapping SSL protocol.
class SslTransportImpl : public TcpTransport,
                         public std::enable_shared_from_this<SslTransportImpl> {
 public:
  void StartRead(const boost::asio::mutable_buffers_1& buf,
                 const ReadCallbackType& callback,
                 bool allow_short_read = false) override;
  void StartWrite(const boost::asio::const_buffers_1& buf,
                  const WriteCallbackType& callback) override;
  void StartClose(const CloseCallbackType& callback) override;
  using TransportBase::StartClose;

  Address GetLocalAddress() const override {
    return Address::FromAsioEndpoint(ssl_sock_.next_layer().local_endpoint());
  }

  Address GetRemoteAddress() const override {
    return Address::FromAsioEndpoint(ssl_sock_.next_layer().remote_endpoint());
  }

  boost::asio::ip::tcp::socket& GetUnderlyingSocket() override {
    return ssl_sock_.next_layer();
  }

 private:
  friend class SslTransportFactoryImpl;

  SslTransportImpl(boost::asio::io_service& io_service,
                   boost::asio::ssl::context& ssl_ctx);

  boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_sock_;
};

/// Factory for creating TcpTransport with SSL support.
class SslTransportFactoryImpl
    : public TcpTransportFactory,
      public std::enable_shared_from_this<SslTransportFactoryImpl> {
 public:
  SslTransportFactoryImpl(const SslTransportFactoryImpl&) = delete;
  SslTransportFactoryImpl& operator=(const SslTransportFactoryImpl&) = delete;

  void StartAccept(EndpointType endpoint,
                   const AcceptCallbackType& callback) override;
  void StartConnect(EndpointType endpoint,
                    const ConnectCallbackType& callback) override;
  std::shared_ptr<TransportBase> TryConnect(
      boost::asio::ip::tcp::resolver::iterator& iter,
      ec_type& error_code) override;

  std::shared_ptr<boost::asio::io_service> get_io_service_ptr() const override {
    return io_service_ptr_;
  }

 private:
  friend class ::thestral::ssl::SslTransportFactoryBuilder;

  SslTransportFactoryImpl(
      const std::shared_ptr<boost::asio::io_service>& io_service_ptr,
      boost::asio::ssl::context&& ssl_ctx)
      : io_service_ptr_(io_service_ptr), ssl_ctx_(std::move(ssl_ctx)) {}

  void DoAccept(const std::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
                const AcceptCallbackType& callback);

  const std::shared_ptr<boost::asio::io_service> io_service_ptr_;
  boost::asio::ssl::context ssl_ctx_;
  static logging::Logger LOG;
};
}  // namespace impl

/// Builder of TcpTransportFactory with SSL support. A builder can only create
/// one instance.
class SslTransportFactoryBuilder {
 public:
  SslTransportFactoryBuilder();

  std::shared_ptr<TcpTransportFactory> Build(
      const std::shared_ptr<boost::asio::io_service>& io_service_ptr);

  SslTransportFactoryBuilder& AddCaPath(const std::string& path);
  SslTransportFactoryBuilder& LoadCaFile(const std::string& pem_file);
  SslTransportFactoryBuilder& LoadCert(const std::string& pem_file);
  SslTransportFactoryBuilder& LoadCertChain(const std::string& pem_file);
  SslTransportFactoryBuilder& LoadPrivateKey(const std::string& pem_file);
  SslTransportFactoryBuilder& LoadDhParams(const std::string& file);
  SslTransportFactoryBuilder& SetVerifyDepth(int depth);
  /// Sets if peers should be verified. If peer verification is enabled,
  /// `SSL_VERIFY_FAIL_IF_NO_PEER_CERT` and `SSL_VERIFY_CLIENT_ONCE` are set as
  /// well.
  SslTransportFactoryBuilder& SetVerifyPeer(bool verify);

 private:
  boost::asio::ssl::context ssl_ctx_;
  bool used_;
};

}  // namespace ssl
}  // namespace thestral

#endif  // THESTRAL_SSL_H_
