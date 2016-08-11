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
/// Defines types and utilities related to SOCKS protocol.
#ifndef THESTRAL_SOCKS_H_
#define THESTRAL_SOCKS_H_

#include <cstdint>
#include <string>
#include <vector>

#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>

#include "base.h"
#include "common.h"

namespace thestral {
namespace socks {

THESTRAL_DEFINE_ENUM(AuthMethod, uint8_t, (kNoAuth, 0x0),
                     (kNotSupported, 0xff));

THESTRAL_DEFINE_ENUM(Command, uint8_t, (kConnect, 0x1), (kBind, 0x2),
                     (kUdpAssociate, 0x3));

THESTRAL_DEFINE_ENUM(ResponseCode, uint8_t, (kSuccess, 0x0),
                     (kSocksServerFailure, 0x1), (kConnectionNotAllow, 0x2),
                     (kNetworkUnreachable, 0x3), (kHostUnreachable, 0x4),
                     (kConnectionRefused, 0x5), (kTtlExpired, 0x6),
                     (kCommandNotSupported, 0x7),
                     (kAddressTypeNotSupported, 0x8));

struct AuthMethodList : PacketBase {
  typedef std::function<void(const ec_type&, AuthMethodList)>
      CreateCallbackType;

  uint8_t version = 0x5;
  std::vector<AuthMethod> methods;

  static void StartCreateFrom(const std::shared_ptr<TransportBase>& transport,
                              const CreateCallbackType& callback);

  std::string ToString() const override;
};

struct AuthMethodSelectPacket : PacketWithSize<AuthMethodSelectPacket, 2> {
  uint8_t version = 0x5;
  AuthMethod method = AuthMethod::kNoAuth;

  void FromBytes(const char* data) override {
    version = static_cast<uint8_t>(data[0]);
    method = static_cast<AuthMethod>(data[1]);
  }

  void ToBytes(char* data) const override {
    data[0] = static_cast<char>(version);
    data[1] = static_cast<char>(method);
  }
};

struct RequestHeader : PacketWithSize<RequestHeader, 3> {
  uint8_t version = 0x5;
  Command command = Command::kConnect;

  void FromBytes(const char* data) override {
    version = static_cast<uint8_t>(data[0]);
    command = static_cast<Command>(data[1]);
    // data[2] is reserved
  }

  void ToBytes(char* data) const override {
    data[0] = static_cast<char>(version);
    data[1] = static_cast<char>(command);
    data[2] = 0;
  }
};

struct ResponseHeader : PacketWithSize<ResponseHeader, 3> {
  uint8_t version = 0x5;
  ResponseCode response_code = ResponseCode::kSuccess;

  void FromBytes(const char* data) override {
    version = static_cast<uint8_t>(data[0]);
    response_code = static_cast<ResponseCode>(data[1]);
    // data[2] is reserved
  }

  void ToBytes(char* data) const override {
    data[0] = static_cast<char>(version);
    data[1] = static_cast<char>(response_code);
    data[2] = 0;
  }
};

/// Request address supported by SOCKS protocol.
struct SocksAddress : Address, PacketBase {
  typedef std::function<void(const ec_type&, SocksAddress)> CreateCallbackType;

  SocksAddress() = default;
  explicit SocksAddress(const Address& address);
  SocksAddress& operator=(const Address& address);

  static void StartCreateFrom(const std::shared_ptr<TransportBase>& transport,
                              const CreateCallbackType& callback);

  std::string ToString() const override;

 private:
  static void StartReadDomain(const std::shared_ptr<SocksAddress>& packet,
                              const std::shared_ptr<TransportBase>& transport,
                              const CreateCallbackType& callback);
  void ExtractPortFromHost();
};

typedef PacketWithHeader<RequestHeader, SocksAddress> RequestPacket;
typedef PacketWithHeader<ResponseHeader, SocksAddress> ResponsePacket;

namespace error {

class SocksCategory : public boost::system::error_category {
 public:
  const char* name() const noexcept override { return "thestral.socks"; }

  std::string message(int value) const override {
    return "thestral.socks." + to_string(static_cast<ResponseCode>(value));
  }
};

static inline const SocksCategory& GetSocksCategory() {
  static SocksCategory category;
  return category;
}

static inline boost::system::error_code make_error_code(ResponseCode code) {
  return boost::system::error_code(static_cast<int>(code), GetSocksCategory());
}

}  // namespace error

}  // namespace socks
}  // namespace thestral

namespace boost {
namespace system {
template <>
struct is_error_code_enum<::thestral::socks::ResponseCode> {
  static const bool value = true;
};
}  // namespace system
}  // namespace boost
#endif  // THESTRAL_SOCKS_H_
