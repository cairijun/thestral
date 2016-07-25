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
/// Tests for SOCKS upstream.
#include "socks_upstream.h"

#include <array>
#include <memory>
#include <vector>

#include <boost/asio.hpp>
#include <boost/test/unit_test.hpp>

#include "mocks.h"
#include "socks.h"

namespace thestral {
namespace socks {

BOOST_AUTO_TEST_SUITE(test_socks_upstream);

BOOST_AUTO_TEST_CASE(test_request) {
  AuthMethodList p1;
  p1.methods.push_back(AuthMethod::kNoAuth);

  AuthMethodSelectPacket p2;
  p2.method = AuthMethod::kNoAuth;

  RequestPacket p3;
  p3.header.command = Command::kConnect;
  p3.body.type = AddressType::kDomainName;
  p3.body.host = "richardtsai.me";
  p3.body.port = 54321;

  ResponsePacket p4;
  p4.header.response_code = ResponseCode::kSuccess;
  p4.body.type = AddressType::kIPv4;
  p4.body.host = "\xab\xcd\xef\x12";
  p4.body.port = 12345;

  std::string data_to_upstream = "some data some more data to upstream";
  std::string data_to_downstream = "some data some more data to downstream";

  auto io_service = std::make_shared<boost::asio::io_service>();
  auto transport_factory =
      std::make_shared<testing::MockTcpTransportFactory>(io_service);
  auto upstream =
      SocksTcpUpstreamFactory::New(transport_factory, "127.0.0.1", 57819);

  int n_clients = 10;
  std::vector<std::shared_ptr<testing::MockTransport>> transports;

  for (int i = 0; i < n_clients; ++i) {
    transports.push_back(transport_factory->NewMockTransport(
        p2.ToString() + p4.ToString() + data_to_downstream));

    upstream->StartRequest(
        p3.body, [&](const ec_type& ec,
                     const std::shared_ptr<TransportBase>& transport) {
          BOOST_TEST(!ec);
          auto local_addr = transport->GetLocalAddress();
          BOOST_CHECK_EQUAL(Address(p4.body), local_addr);

          transport->StartWrite(data_to_upstream, [&, transport](
                                                      const ec_type& ec,
                                                      size_t n_bytes) {
            BOOST_TEST(!ec);
            BOOST_CHECK_EQUAL(data_to_upstream.size(), n_bytes);

            auto read_buf = std::make_shared<std::array<char, 64>>();
            transport->StartRead(
                *read_buf, data_to_downstream.size(),
                [&, transport, read_buf](const ec_type& ec, size_t n_bytes) {
                  BOOST_TEST(!ec);
                  BOOST_CHECK_EQUAL(data_to_downstream.size(), n_bytes);
                  read_buf->at(n_bytes) = '\0';
                  BOOST_CHECK_EQUAL(data_to_downstream, read_buf->data());

                  transport->StartClose([&](const ec_type& ec) {
                    BOOST_TEST(!ec);
                    --n_clients;
                  });
                });
          });
        });
  }

  io_service->run();

  BOOST_TEST(n_clients == 0);
  auto listened_on = transport_factory->PopEndpoint();
  BOOST_CHECK_EQUAL("127.0.0.1", listened_on.address().to_string());
  BOOST_CHECK_EQUAL(57819, listened_on.port());

  for (auto& transport : transports) {
    BOOST_CHECK_EQUAL(p1.ToString() + p3.ToString() + data_to_upstream,
                      transport->write_buf);
  }
}

BOOST_AUTO_TEST_SUITE_END();

}  // namespace socks
}  // namespace thestral
