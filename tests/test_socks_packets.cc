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
/// Tests for some packet classes defined in socks.h.
#include "socks.h"

#include <iterator>

#include <boost/test/unit_test.hpp>

#include "mocks.h"

#define PACKET_CALLBACK(type) [&](const ec_type& ec, type data)
#define CHECK_SEQUENCES_EQUAL(seq1, seq2)                        \
  do {                                                           \
    auto s1 = seq1;                                              \
    auto s2 = seq2;                                              \
    BOOST_CHECK_EQUAL_COLLECTIONS(std::begin(s1), std::end(s1),  \
                                  std::begin(s2), std::end(s2)); \
  } while (0)

namespace thestral {
namespace socks {

BOOST_AUTO_TEST_SUITE(test_socks_packets);

BOOST_AUTO_TEST_CASE(test_auth_method_list_create) {
  auto transport = testing::MockTransport::New({'\x05', '\x01', '\x00'});
  bool called = false;
  AuthMethodList::StartCreateFrom(transport, PACKET_CALLBACK(AuthMethodList) {
    called = true;
    BOOST_TEST(!ec);
    BOOST_CHECK_EQUAL(0x5, data.version);
    CHECK_SEQUENCES_EQUAL({AuthMethod::kNoAuth}, data.methods);
  });
  BOOST_TEST(called);
}

BOOST_AUTO_TEST_CASE(test_auth_method_list_write) {
  AuthMethodList packet;
  packet.methods.push_back(AuthMethod::kNoAuth);
  std::string s{'\x05', '\x01', '\x00'};
  BOOST_CHECK_EQUAL(s, packet.ToString());

  auto transport = testing::MockTransport::New();
  bool called = false;
  packet.StartWriteTo(transport, PACKET_CALLBACK(size_t) {
    called = true;
    BOOST_TEST(!ec);
    BOOST_CHECK_EQUAL(3, data);
  });
  BOOST_TEST(called);
  BOOST_CHECK_EQUAL(s, transport->write_buf);
}

BOOST_AUTO_TEST_CASE(test_socks_address_create) {
  std::string ipv4 = "\xab\xcd\xef\x12";
  std::string ipv6 =
      "\x01\x23\x45\x67\x89\xab\xcd\xef"
      "abcdefgh";
  std::string domain = "richardtsai.me";  // len = 14

  auto transport = testing::MockTransport::New(
      '\x01' + ipv4 + "\x34\x56" + '\x04' + ipv6 + "\x34\x56" + '\x03' +
      '\x0e' + domain + "\x34\x56");  // port = 13398

  bool called = false;
  SocksAddress::StartCreateFrom(transport, PACKET_CALLBACK(SocksAddress) {
    called = true;
    BOOST_TEST(!ec);
    BOOST_CHECK_EQUAL(AddressType::kIPv4, data.type);
    BOOST_CHECK_EQUAL(ipv4, data.host);
    BOOST_CHECK_EQUAL(13398, data.port);
  });
  BOOST_TEST(called);

  called = false;
  SocksAddress::StartCreateFrom(transport, PACKET_CALLBACK(SocksAddress) {
    called = true;
    BOOST_TEST(!ec);
    BOOST_CHECK_EQUAL(AddressType::kIPv6, data.type);
    BOOST_CHECK_EQUAL(ipv6, data.host);
    BOOST_CHECK_EQUAL(13398, data.port);
  });
  BOOST_TEST(called);

  called = false;
  SocksAddress::StartCreateFrom(transport, PACKET_CALLBACK(SocksAddress) {
    called = true;
    BOOST_TEST(!ec);
    BOOST_CHECK_EQUAL(AddressType::kDomainName, data.type);
    BOOST_CHECK_EQUAL(domain, data.host);
    BOOST_CHECK_EQUAL(13398, data.port);
  });
  BOOST_TEST(called);
}

BOOST_AUTO_TEST_CASE(test_socks_address_write) {
  std::string ipv4 = "\xab\xcd\xef\x12";
  std::string ipv6 =
      "\x01\x23\x45\x67\x89\xab\xcd\xef"
      "abcdefgh";
  std::string domain = "richardtsai.me";  // len = 14
  uint16_t port = 13398;

  auto transport = testing::MockTransport::New();

  SocksAddress packet;
  packet.type = AddressType::kIPv4;
  packet.host = ipv4;
  packet.port = port;
  bool called = false;
  packet.StartWriteTo(transport, PACKET_CALLBACK(size_t) {
    called = true;
    BOOST_TEST(!ec);
    BOOST_CHECK_EQUAL(7, data);
  });
  BOOST_TEST(called);

  packet.type = AddressType::kIPv6;
  packet.host = ipv6;
  packet.port = port;
  called = false;
  packet.StartWriteTo(transport, PACKET_CALLBACK(size_t) {
    called = true;
    BOOST_TEST(!ec);
    BOOST_CHECK_EQUAL(19, data);
  });
  BOOST_TEST(called);

  packet.type = AddressType::kDomainName;
  packet.host = domain;
  packet.port = port;
  called = false;
  packet.StartWriteTo(transport, PACKET_CALLBACK(size_t) {
    called = true;
    BOOST_TEST(!ec);
    BOOST_CHECK_EQUAL(domain.size() + 4, data);
  });
  BOOST_TEST(called);

  BOOST_CHECK_EQUAL('\x01' + ipv4 + "\x34\x56" + '\x04' + ipv6 + "\x34\x56" +
                        '\x03' + '\x0e' + domain + "\x34\x56",
                    transport->write_buf);
}

}  // namespace socks
}  // namespace thestral

BOOST_AUTO_TEST_SUITE_END();
