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
/// Tests for direct upstream.
#include "direct_upstream.h"

#include <memory>

#include <boost/asio.hpp>
#include <boost/test/unit_test.hpp>

#include "base.h"
#include "common.h"
#include "mocks.h"

namespace thestral {

BOOST_AUTO_TEST_SUITE(test_direct_upstream);

BOOST_AUTO_TEST_CASE(test_request) {
  auto io_service = std::make_shared<boost::asio::io_service>();
  auto transport_factory =
      std::make_shared<testing::MockTcpTransportFactory>(io_service);
  auto transport_1 = transport_factory->NewMockTransport();
  auto transport_2 = transport_factory->NewMockTransport();
  auto transport_3 = transport_factory->NewMockTransport();
  auto upstream = DirectTcpUpstreamFactory::New(transport_factory);

  Address ipv4;
  ipv4.type = AddressType::kIPv4;
  ipv4.host = "\xab\xcd\xef\x12";
  ipv4.port = 12345;

  Address ipv6;
  ipv6.type = AddressType::kIPv6;
  ipv6.host = "01234567abcdefgh";
  ipv6.port = 54321;

  Address domain;
  domain.type = AddressType::kDomainName;
  domain.host = "localhost";
  domain.port = 11111;

  bool called_ipv4 = false;
  upstream->StartRequest(
      ipv4,
      [&](const ec_type& ec, const std::shared_ptr<TransportBase>& transport) {
        called_ipv4 = true;
        BOOST_TEST(!ec);
        BOOST_CHECK_EQUAL(transport_1, transport);
      });

  bool called_ipv6 = false;
  upstream->StartRequest(
      ipv6, [&](const ec_type& ec, std::shared_ptr<TransportBase> transport) {
        called_ipv6 = true;
        BOOST_TEST(!ec);
        BOOST_CHECK_EQUAL(transport_2, transport);
      });

  bool called_domain = false;
  upstream->StartRequest(
      domain, [&](const ec_type& ec, std::shared_ptr<TransportBase> transport) {
        called_domain = true;
        BOOST_TEST(!ec);
        BOOST_CHECK_EQUAL(transport_3, transport);
      });

  io_service->run();

  BOOST_TEST(called_ipv4);
  BOOST_TEST(called_ipv6);
  BOOST_TEST(called_domain);

  auto endpoint_1 = transport_factory->PopEndpoint();
  auto endpoint_2 = transport_factory->PopEndpoint();
  auto endpoint_3 = transport_factory->PopEndpoint();
  BOOST_CHECK_EQUAL(ipv4, Address::FromAsioEndpoint(endpoint_1));
  BOOST_CHECK_EQUAL(ipv6, Address::FromAsioEndpoint(endpoint_2));
  if (endpoint_3.address().is_v4()) {
    BOOST_CHECK_EQUAL("127.0.0.1", endpoint_3.address().to_string());
  } else if (endpoint_3.address().is_v6()) {
    BOOST_CHECK_EQUAL("::1", endpoint_3.address().to_string());
  } else {
    BOOST_ERROR("invalid endpoint: " + endpoint_3.address().to_string());
  }
  BOOST_CHECK_EQUAL(domain.port, endpoint_3.port());
}

BOOST_AUTO_TEST_SUITE_END();

}  // namespace thestral
