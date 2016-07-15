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
/// This file defines some abstract base classes used across the program.
#ifndef THESTRAL_BASE_H_
#define THESTRAL_BASE_H_

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#include <boost/asio.hpp>

namespace thestral {

typedef boost::system::error_code ec_type;

/// Base class of all transport types. A transport object represents an
/// established connection and provides interfaces to asynchronously talk to the
/// remote peer.
class TransportBase {
 public:
  typedef std::function<void(const ec_type&, std::size_t)> ReadCallbackType;
  typedef std::function<void(const ec_type&, std::size_t)> WriteCallbackType;
  typedef std::function<void(const ec_type&)> CloseCallbackType;

  virtual ~TransportBase() {}

  /// Starts an asynchronous reading opeation.
  /// @param allow_short_read If true, the operation may complete before the
  /// buffer is full.
  virtual void StartRead(const boost::asio::mutable_buffers_1& buf,
                         ReadCallbackType callback,
                         bool allow_short_read = false) = 0;
  /// Starts an asynchronous writing opeation.
  virtual void StartWrite(const boost::asio::const_buffers_1& buf,
                          WriteCallbackType callback) = 0;
  /// Starts an asynchronous closing opeation.
  virtual void StartClose(CloseCallbackType callback) = 0;

  /// Convenience function for constructing and reading into a buffer.
  template <typename T>
  void StartRead(T&& data, ReadCallbackType callback,
                 bool allow_short_read = false) {
    StartRead(boost::asio::buffer(std::forward<T>(data)), callback,
              allow_short_read);
  }

  /// Convenience function for constructing and reading into a buffer of
  /// specific size
  template <typename T>
  void StartRead(T&& data, std::size_t size, ReadCallbackType callback,
                 bool allow_short_read = false) {
    StartRead(boost::asio::buffer(std::forward<T>(data), size), callback,
              allow_short_read);
  }

  /// Convenience function for constructing and writing from a buffer.
  template <typename T>
  void StartWrite(T&& data, WriteCallbackType callback) {
    StartWrite(boost::asio::buffer(std::forward<T>(data)), callback);
  }

  /// Convenience function for constructing and writing from a buffer of
  /// specific size
  template <typename T>
  void StartWrite(T&& data, std::size_t size, WriteCallbackType callback) {
    StartWrite(boost::asio::buffer(std::forward<T>(data), size), callback);
  }
};

/// Base class of transport factories. The transport factory creates Transport
/// objects via asynchronously accepting or connecting to remote peers.
/// @tparam Endpoint Type of endpoints when accepting or connecting.
template <typename Endpoint>
class TransportFactoryBase {
 public:
  typedef Endpoint EndpointType;
  typedef std::function<bool(std::shared_ptr<TransportBase>, const ec_type&)>
      AcceptCallbackType;
  typedef std::function<void(std::shared_ptr<TransportBase>, const ec_type&)>
      ConnectCallbackType;

  virtual ~TransportFactoryBase() {}

  /// Accepts connections from a specific endpoint asynchronously. The callback
  /// should return a boolean value indicating whether the factory should accept
  /// more connections.
  virtual void StartAccept(Endpoint endpoint, AcceptCallbackType callback) = 0;
  /// Connects to a specific endpoint asynchronous.
  virtual void StartConnect(Endpoint endpoint,
                            ConnectCallbackType callback) = 0;
};

/// Base class of upstream transport factories. The upstream transport factory
/// understands the upstream protocol, and can requests the upstream to
/// establish connections to target endpoints.
/// @tparam Endpoint Type of endpoints when requesting.
template <typename Endpoint>
class UpstreamFactoryBase {
 public:
  typedef Endpoint EndpointType;
  typedef std::function<void(std::shared_ptr<TransportBase>, const ec_type&)>
      RequestCallbackType;

  virtual ~UpstreamFactoryBase() {}

  /// Requests the upstream to establish a connection to a specific endpoint
  /// asynchronously.
  virtual void StartRequest(Endpoint endpoint,
                            RequestCallbackType callback) = 0;
};

/// Base class of servers. The server understands the downstream protocol.
/// It accepts connection requests from downstream hosts and serves them via
/// upstream.
class ServerBase {
 public:
  virtual ~ServerBase() {}
  /// Starts the server.
  virtual void Start() = 0;
};

}  // namespace thestral

#endif  // THESTRAL_BASE_H_
