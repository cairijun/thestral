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
/// Implements types and utilities related to SOCKS protocol.
#include "socks.h"

namespace thestral {
namespace socks {

void AuthMethodList::StartCreateFrom(std::shared_ptr<TransportBase> transport,
                                     CreateCallbackType callback) {
  auto header = std::make_shared<uint16_t>();
  transport->StartRead(header.get(), 2, [header, transport, callback](
                                            const ec_type& ec, size_t) {
    if (ec) {
      callback(ec, AuthMethodList());
      return;
    }
    auto packet = std::make_shared<AuthMethodList>();
    packet->version = (*header) >> 8;
    packet->methods.resize((*header) & 0xff);
    transport->StartRead(packet->methods,
                         [packet, callback](const ec_type& ec, size_t) {
                           callback(ec, *packet);
                         });
  });
}

std::string AuthMethodList::ToString() const {
  // TODO: check methods.size()
  std::string str{static_cast<char>(version),
                  static_cast<char>(methods.size())};
  for (auto m : methods) {
    str.push_back(static_cast<char>(m));
  }
  return str;
}

SocksAddress::SocksAddress(const Address& address) : Address(address) {}

SocksAddress& SocksAddress::operator=(const Address& address) {
  if (this != &address) {
    this->type = address.type;
    this->host = address.host;
    this->port = address.port;
  }
  return *this;
}

void SocksAddress::StartCreateFrom(std::shared_ptr<TransportBase> transport,
                                   CreateCallbackType callback) {
  auto packet = std::make_shared<SocksAddress>();

  transport->StartRead(
      &packet->type, 1,
      [transport, callback, packet](const ec_type& ec, size_t bytes_read) {
        if (ec) {
          callback(ec, *packet);
          return;
        }
        switch (packet->type) {
          // TODO: use some cleaner and safer way to read into string
          // the extra 2 bytes specify the port number
          case AddressType::kIPv4:
            packet->host.resize(4 + 2);
            transport->StartRead(
                &packet->host[0], packet->host.size(),
                [packet, callback](const ec_type& error_code, size_t) {
                  if (!error_code) {
                    packet->ExtractPortFromHost();
                  }
                  callback(error_code, std::move(*packet));
                });
            break;
          case AddressType::kIPv6:
            packet->host.resize(16 + 2);
            transport->StartRead(
                &packet->host[0], packet->host.size(),
                [packet, callback](const ec_type& error_code, size_t) {
                  if (!error_code) {
                    packet->ExtractPortFromHost();
                  }
                  callback(error_code, std::move(*packet));
                });
            break;
          case AddressType::kDomainName:
            StartReadDomain(std::move(packet), transport, callback);
            break;
          default:  // Unknown address type, which should be handled outside.
            break;
        }
      });
}

void SocksAddress::StartReadDomain(std::shared_ptr<SocksAddress> packet,
                                   std::shared_ptr<TransportBase> transport,
                                   CreateCallbackType callback) {
  // use the first byte of this->host to temporarily store the length
  packet->host.resize(1);
  transport->StartRead(&packet->host[0], 1, [packet, transport, callback](
                                                const ec_type& ec, size_t) {
    if (ec) {
      callback(ec, *packet);
      return;
    }
    // the extra 2 bytes specify the port number
    uint8_t len = static_cast<uint8_t>(packet->host[0]) + 2;
    packet->host.resize(len);
    transport->StartRead(&packet->host[0], len,
                         [packet, callback](const ec_type& ec, size_t) {
                           if (!ec) {
                             packet->ExtractPortFromHost();
                           }
                           callback(ec, *packet);
                         });
  });
}

void SocksAddress::ExtractPortFromHost() {
  port = static_cast<uint8_t>(host[host.size() - 2]) << 8;
  port |= static_cast<uint8_t>(host.back());
  host.resize(host.size() - 2);
}

std::string SocksAddress::ToString() const {
  std::string str{static_cast<char>(type)};
  if (type == AddressType::kDomainName) {
    // TODO: check host length
    str.push_back(static_cast<char>(host.size()));
  }
  str.append(host);
  str.append({static_cast<char>(port >> 8), static_cast<char>(port & 0xff)});
  return str;
}

}  // namespace socks
}  // namespace thestral
