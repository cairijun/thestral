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
/// E2e tests for the basic functions of thestral.
#include <array>
#include <iterator>
#include <memory>
#include <random>

#include <boost/asio.hpp>
#include <boost/test/unit_test.hpp>

#include "mocks.h"
#include "socks_upstream.h"
#include "tcp_transport.h"

namespace thestral {

namespace {
template <typename T>
void FillWithRandomBytes(T& buf) {
  std::random_device rd;
  std::default_random_engine eng(rd());
  // cann't use <char> here due to a possible defect in the standard
  std::uniform_int_distribution<unsigned short> dist(0, 255);

  auto begin = std::begin(buf);
  auto end = std::end(buf);
  while (begin != end) {
    *begin++ = dist(eng);
  }
}
}  // anonymous namespace

BOOST_FIXTURE_TEST_SUITE(e2e_basic, testing::MockServer);

BOOST_AUTO_TEST_CASE(test_relay) {
  auto io_service_ptr = std::make_shared<boost::asio::io_service>();
  auto client = socks::SocksTcpUpstreamFactory::New(
      TcpTransportFactory::New(io_service_ptr), "::1", 1081);

  typedef std::array<unsigned char, 1024 * 1024> BufferType;

  int n_pending = 30;
  for (int i = 0; i < n_pending; ++i) {
    auto write_buf = std::make_shared<BufferType>();
    FillWithRandomBytes(*write_buf);

    client->StartRequest(
        Address::FromAsioEndpoint(GetEndpoint()),
        [=, &n_pending](const ec_type& ec,
                        const std::shared_ptr<TransportBase>& transport) {
          BOOST_TEST(!ec);

          auto read_buf = std::make_shared<BufferType>();
          transport->StartRead(
              *read_buf, [=, &n_pending](const ec_type& ec, size_t n_bytes) {
                BOOST_TEST(!ec);
                BOOST_CHECK_EQUAL(read_buf->size(), n_bytes);
                BOOST_TEST((*write_buf == *read_buf));

                transport->StartClose(
                    [&n_pending](const ec_type& ec) { --n_pending; });
              });

          transport->StartWrite(
              *write_buf, [=, &n_pending](const ec_type& ec, size_t n_bytes) {
                BOOST_TEST(!ec);
                BOOST_CHECK_EQUAL(write_buf->size(), n_bytes);
              });
        });
  }

  io_service_ptr->run();
  BOOST_CHECK_EQUAL(0, n_pending);
}

BOOST_AUTO_TEST_SUITE_END();

}  // namespace thestral
