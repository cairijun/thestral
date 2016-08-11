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
/// Tests for SOCKS server.
#include "socks_server.h"

#include <boost/test/unit_test.hpp>

#include "mocks.h"
#include "socks.h"

namespace thestral {
namespace socks {

BOOST_AUTO_TEST_SUITE(test_socks_server);

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

  auto downstream_transport_factory =
      std::make_shared<testing::MockTcpTransportFactory>(io_service);
  auto downstream_transport = downstream_transport_factory->NewMockTransport(
      p1.Serialize() + p3.Serialize() + data_to_upstream);

  auto upstream_factory =
      std::make_shared<testing::MockUpstreamFactory>(io_service);
  auto upstream_transport =
      upstream_factory->NewMockTransport(data_to_downstream);
  upstream_transport->local_address = p4.body;

  auto socks_server = SocksTcpServer::New(
      "127.0.0.1", 19278, downstream_transport_factory, upstream_factory);
  socks_server->Start();
  io_service->run();

  BOOST_CHECK_EQUAL(p2.Serialize() + p4.Serialize() + data_to_downstream,
                    downstream_transport->write_buf);
  BOOST_CHECK_EQUAL(data_to_upstream, upstream_transport->write_buf);
  BOOST_CHECK_EQUAL(p3.body, upstream_factory->PopAddress());
  auto listened_addr = downstream_transport_factory->PopEndpoint();
  BOOST_CHECK_EQUAL("127.0.0.1", listened_addr.address().to_string());
  BOOST_CHECK_EQUAL(19278, listened_addr.port());
}

BOOST_AUTO_TEST_SUITE_END();

}  // namespace socks
}  // namespace thestral
