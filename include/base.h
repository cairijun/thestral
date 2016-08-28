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

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include <boost/asio.hpp>

#include "common.h"

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
  typedef std::uint_fast64_t IdType;

  TransportBase() : id_(GetNextId()) {}
  virtual ~TransportBase() {}

  /// Returns the unique id of the transport.
  virtual IdType GetId() const { return id_; }

  /// Returns the bound local address of the transport.
  virtual Address GetLocalAddress() const = 0;
  /// Returns the address of the remote peer.
  virtual Address GetRemoteAddress() const = 0;
  /// Starts an asynchronous reading opeation.
  /// @param allow_short_read If true, the operation may complete before the
  /// buffer is full.
  virtual void StartRead(const boost::asio::mutable_buffers_1& buf,
                         const ReadCallbackType& callback,
                         bool allow_short_read = false) = 0;
  /// Starts an asynchronous writing opeation.
  virtual void StartWrite(const boost::asio::const_buffers_1& buf,
                          const WriteCallbackType& callback) = 0;
  /// Starts an asynchronous closing opeation.
  /// The transport should be closed by the one who "owns" it currently, i.e.
  /// * the object who created it, if the transport is in the middle of
  ///   establishment and not yet ready
  /// * the object who is using it, if the transport has been established
  /// If a transport has been closed, it should not be passed to other objects.
  virtual void StartClose(const CloseCallbackType& callback) = 0;

  /// Convenience function for constructing and reading into a buffer.
  template <
      typename T,
      typename std::enable_if<
          !(std::is_same<T, boost::asio::mutable_buffers_1&>::value ||
            std::is_convertible<T, boost::asio::mutable_buffers_1>::value),
          int>::type = 0>
  void StartRead(T&& data, const ReadCallbackType& callback,
                 bool allow_short_read = false) {
    StartRead(boost::asio::buffer(std::forward<T>(data)), callback,
              allow_short_read);
  }

  /// Convenience function for constructing and reading into a buffer of
  /// specific size
  template <typename T>
  void StartRead(T&& data, std::size_t size, const ReadCallbackType& callback,
                 bool allow_short_read = false) {
    StartRead(boost::asio::buffer(std::forward<T>(data), size), callback,
              allow_short_read);
  }

  /// Convenience function for constructing and writing from a buffer.
  template <typename T,
            typename std::enable_if<
                !(std::is_same<T, boost::asio::const_buffers_1&>::value ||
                  std::is_convertible<T, boost::asio::const_buffers_1>::value),
                int>::type = 0>
  void StartWrite(T&& data, const WriteCallbackType& callback) {
    // asio::buffer() may create a `mutable_buffers_1`, which is not implicitly
    // convertible to `const_buffers_1`, so an explicit conversion is required
    StartWrite(boost::asio::const_buffers_1(
                   boost::asio::buffer(std::forward<T>(data))),
               callback);
  }

  /// Convenience function for constructing and writing from a buffer of
  /// specific size
  template <typename T>
  void StartWrite(T&& data, std::size_t size,
                  const WriteCallbackType& callback) {
    StartWrite(boost::asio::buffer(std::forward<T>(data), size), callback);
  }

  virtual void StartClose() {
    StartClose([](const ec_type&) {});
  }

 private:
  static IdType GetNextId();

  const IdType id_;
};

/// Base class of transport factories. The transport factory creates Transport
/// objects via asynchronously accepting or connecting to remote peers.
/// @tparam Endpoint Type of endpoints when accepting or connecting.
template <typename Endpoint>
class TransportFactoryBase {
 public:
  typedef Endpoint EndpointType;
  typedef std::function<bool(const ec_type&,
                             const std::shared_ptr<TransportBase>&)>
      AcceptCallbackType;
  typedef std::function<void(const ec_type&,
                             const std::shared_ptr<TransportBase>&)>
      ConnectCallbackType;

  virtual ~TransportFactoryBase() {}

  /// Accepts connections from a specific endpoint asynchronously. The callback
  /// should return a boolean value indicating whether the factory should accept
  /// more connections.
  virtual void StartAccept(Endpoint endpoint,
                           const AcceptCallbackType& callback) = 0;
  /// Connects to a specific endpoint asynchronous.
  virtual void StartConnect(Endpoint endpoint,
                            const ConnectCallbackType& callback) = 0;

  /// Returns a pointer to the `io_service` bound with this factory.
  virtual std::shared_ptr<boost::asio::io_service> get_io_service_ptr()
      const = 0;
};

/// Base class of upstream transport factories. The upstream transport factory
/// understands the upstream protocol, and can requests the upstream to
/// establish connections to target endpoints.
class UpstreamFactoryBase {
 public:
  typedef std::function<void(const ec_type&,
                             const std::shared_ptr<TransportBase>&)>
      RequestCallbackType;

  virtual ~UpstreamFactoryBase() {}

  /// Requests the upstream to establish a connection to a specific endpoint
  /// asynchronously.
  virtual void StartRequest(const Address& endpoint,
                            const RequestCallbackType& callback) = 0;

  /// Returns a pointer to the `io_service` bound with this factory.
  virtual std::shared_ptr<boost::asio::io_service> get_io_service_ptr()
      const = 0;
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

/// Base class of transferable packet classes. As a convention, a subclass
/// should implement a static asynchronous creation function as follows.
/// ```cpp
/// typedef std::function<void(const ec_type&, PacketType)> CreateCallbackType;
/// static void StartCreateFrom(const std::shared_ptr<TransportBase>& transport,
///                             const CreateCallbackType& callback);
/// ```
class PacketBase {
 public:
  /// Writes the packet into the transport object asynchronously. The default
  /// implementation writes the bytes returned by Serialize().
  virtual void StartWriteTo(
      const std::shared_ptr<TransportBase>& transport,
      const TransportBase::WriteCallbackType& callback) const;
  /// Checks the correctness of the fields of the packet. The default
  /// implementation always returns `true`.
  virtual bool Validate() const { return true; }
  /// Returns a string representation of the packet.
  virtual std::string Serialize() const = 0;
};

/// Class template for packets with two consecutive parts, a header and a body.
template <typename Header, typename Body>
struct PacketWithHeader : PacketBase {
  static_assert(std::is_base_of<PacketBase, Header>::value,
                "Header must inherit PacketBase");
  static_assert(std::is_base_of<PacketBase, Body>::value,
                "Body must inherit PacketBase");

  typedef std::function<void(const ec_type&, PacketWithHeader<Header, Body>)>
      CreateCallbackType;

  Header header;  ///< The header part of the whole packet.
  Body body;      ///< The body part of the whole packet.

  static void StartCreateFrom(const std::shared_ptr<TransportBase>& transport,
                              const CreateCallbackType& callback);

  bool Validate() const override;
  std::string Serialize() const override;
};

template <typename Header, typename Body>
bool PacketWithHeader<Header, Body>::Validate() const {
  return header.Validate() && body.Validate();
}

template <typename Header, typename Body>
void PacketWithHeader<Header, Body>::StartCreateFrom(
    const std::shared_ptr<TransportBase>& transport,
    const CreateCallbackType& callback) {
  Header::StartCreateFrom(
      transport, [transport, callback](const ec_type& ec, Header header) {
        if (ec) {
          callback(ec, PacketWithHeader<Header, Body>());
          return;
        }
        Body::StartCreateFrom(transport, [header, transport, callback](
                                             const ec_type& ec, Body body) {
          PacketWithHeader<Header, Body> packet;
          if (!ec) {
            packet.header = header;
            packet.body = body;
          }
          callback(ec, packet);
        });
      });
}

template <typename Header, typename Body>
std::string PacketWithHeader<Header, Body>::Serialize() const {
  return header.Serialize() + body.Serialize();
}

/// Class template for fixed size packet. The subclasses only need to implement
/// ToBytes() and FromBytes().
/// @tparam PacketType The type of the subclass. It is used to generate to
/// static creation function.
/// @tparam N The size of the packet in bytes.
template <typename PacketType, size_t N>
class PacketWithSize : public PacketBase {
 public:
  typedef std::function<void(const ec_type&, PacketType)> CreateCallbackType;

  static void StartCreateFrom(std::shared_ptr<TransportBase> transport,
                              const CreateCallbackType& callback);

  std::string Serialize() const override;
  /// Writes the bytes representation to a pre-allocated memory area.
  virtual void ToBytes(char* data) const = 0;
  /// Fills the packet fields from bytes.
  virtual void FromBytes(const char* data) = 0;
};

template <typename PacketType, size_t N>
void PacketWithSize<PacketType, N>::StartCreateFrom(
    std::shared_ptr<TransportBase> transport,
    const CreateCallbackType& callback) {
  auto data = std::make_shared<std::array<char, N>>();
  transport->StartRead(*data,
                       [callback, data](const ec_type& error_code, size_t) {
                         PacketType packet;
                         if (!error_code) {
                           packet.FromBytes(data->data());
                         }
                         callback(error_code, packet);
                       });
}

template <typename PacketType, size_t N>
std::string PacketWithSize<PacketType, N>::Serialize() const {
  std::string data(N, '\0');
  ToBytes(&data[0]);
  return data;
}

}  // namespace thestral

#endif  // THESTRAL_BASE_H_
