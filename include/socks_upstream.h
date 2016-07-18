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
/// Defines the classes for the upstream part of SOCKS protocol.
#ifndef THESTRAL_SOCKS_UPSTREAM_H_
#define THESTRAL_SOCKS_UPSTREAM_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

#include <boost/asio.hpp>

#include "base.h"
#include "common.h"
#include "socks.h"
#include "tcp_transport.h"

namespace thestral {
namespace socks {

namespace detail {

/// A wrapper around a transport pointer, overriding
/// TransportBase::GetLocalAddress() to return a specified address.
class SocksTransportWrapper : public TransportBase {
 public:
  Address GetLocalAddress() const override { return bound_address_; }

  void StartRead(const boost::asio::mutable_buffers_1& buf,
                 ReadCallbackType callback,
                 bool allow_short_read = false) override {
    wrapped_->StartRead(buf, callback, allow_short_read);
  }

  void StartWrite(const boost::asio::const_buffers_1& buf,
                  WriteCallbackType callback) override {
    wrapped_->StartWrite(buf, callback);
  }

  void StartClose(CloseCallbackType callback) override {
    wrapped_->StartClose(callback);
  }

  /// Constructs a wrapper around a pointer to a transport.
  /// @param wrapped The wrapped transport pointer.
  /// @param bound_address The address to return for GetLocalAddress().
  SocksTransportWrapper(const std::shared_ptr<TransportBase>& wrapped,
                        const Address& bound_address)
      : wrapped_(wrapped), bound_address_(bound_address) {}

 private:
  const std::shared_ptr<TransportBase> wrapped_;
  const Address bound_address_;
};

}  // namespace detail

/// Upstream factory for the TCP part of SOCKS protocol.
class SocksTcpUpstreamFactory
    : public UpstreamFactoryBase,
      public std::enable_shared_from_this<SocksTcpUpstreamFactory> {
 public:
  SocksTcpUpstreamFactory(const SocksTcpUpstreamFactory&) = delete;
  SocksTcpUpstreamFactory& operator=(const SocksTcpUpstreamFactory&) = delete;

  /// Creates a factory with a given TcpTransportFactory.
  static std::shared_ptr<SocksTcpUpstreamFactory> New(
      const std::shared_ptr<TcpTransportFactory>& transport_factory,
      const std::string& upstream_host, uint16_t upstream_port) {
    return std::shared_ptr<SocksTcpUpstreamFactory>(new SocksTcpUpstreamFactory(
        transport_factory, upstream_host, upstream_port));
  }

  /// Creates a factory with a given `io_service`.
  /// The TcpTransportFactory will be created with `io_service_ptr`.
  static std::shared_ptr<SocksTcpUpstreamFactory> New(
      const std::shared_ptr<boost::asio::io_service>& io_service_ptr,
      const std::string& upstream_host, uint16_t upstream_port) {
    return std::shared_ptr<SocksTcpUpstreamFactory>(
        new SocksTcpUpstreamFactory(TcpTransportFactory::New(io_service_ptr),
                                    upstream_host, upstream_port));
  }

  void StartRequest(const Address& endpoint,
                    RequestCallbackType callback) override;

  std::shared_ptr<boost::asio::io_service> get_io_service_ptr() const override {
    return transport_factory_->get_io_service_ptr();
  }

 private:
  /// The transport factory for creating connections to the upstream host.
  std::shared_ptr<TcpTransportFactory> transport_factory_;
  const std::string upstream_host_;
  const uint16_t upstream_port_;

  /// The cached upstream endpoint to avoid DNS query every time.
  boost::asio::ip::tcp::endpoint upstream_endpoint_;
  /// The flag indicating if @ref upstream_endpoint_ has been initialized.
  std::atomic_bool is_upstream_endpoint_init_{false};
  /// The mutex used when initializing @ref upstream_endpoint_.
  std::mutex upstream_endpoint_init_mtx_;

  SocksTcpUpstreamFactory(
      const std::shared_ptr<TcpTransportFactory>& transport_factory,
      const std::string& upstream_host, uint16_t upstream_port)
      : transport_factory_(transport_factory),
        upstream_host_(upstream_host),
        upstream_port_(upstream_port) {}

  void SendAuthRequest(const Address& endpoint,
                       std::shared_ptr<TransportBase> transport,
                       RequestCallbackType callback) const;
  void SendSocksRequest(const Address& endpoint,
                        std::shared_ptr<TransportBase> transport,
                        RequestCallbackType callback) const;
};

}  // namespace socks
}  // namespace thestral

#endif /* ifndef THESTRAL_SOCKS_UPSTREAM_H_ */