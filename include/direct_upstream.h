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
/// Defines the upstream for direct access.
#ifndef THESTRAL_DIRECT_UPSTREAM_H_
#define THESTRAL_DIRECT_UPSTREAM_H_

#include <memory>

#include <boost/asio.hpp>

#include "base.h"
#include "common.h"
#include "tcp_transport.h"

namespace thestral {

/// Upstream factory for creating direct tcp connections to target endpoints.
class DirectTcpUpstreamFactory
    : public UpstreamFactoryBase,
      public std::enable_shared_from_this<DirectTcpUpstreamFactory> {
 public:
  /// Factory method for creating a DirectTcpUpstreamFactory from a given
  /// TcpTransportFactory.
  static std::shared_ptr<DirectTcpUpstreamFactory> New(
      const std::shared_ptr<TcpTransportFactory>& transport_factory) {
    return std::shared_ptr<DirectTcpUpstreamFactory>(
        new DirectTcpUpstreamFactory(transport_factory));
  }

  std::shared_ptr<boost::asio::io_service> get_io_service_ptr() const override {
    return transport_factory_->get_io_service_ptr();
  }

  void StartRequest(const Address& address,
                    RequestCallbackType callback) override;

 private:
  DirectTcpUpstreamFactory(
      const std::shared_ptr<TcpTransportFactory>& transport_factory)
      : transport_factory_(transport_factory) {}

  const std::shared_ptr<TcpTransportFactory> transport_factory_;
};

}  // namespace thestral
#endif /* ifndef THESTRAL_DIRECT_UPSTREAM_H_ */
