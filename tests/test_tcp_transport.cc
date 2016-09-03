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
/// Tests for tcp transport related classes.
#include "tcp_transport.h"

#include <array>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <boost/test/unit_test.hpp>

#include "mocks.h"

#define TRANSPORT_CALLBACK(...)    \
  [__VA_ARGS__](const ec_type& ec, \
                const std::shared_ptr<TransportBase>& transport)
#define BYTES_CALLBACK(...) [__VA_ARGS__](const ec_type& ec, size_t n_bytes)

namespace thestral {

struct WithEchoServer {
  testing::MockServer server;
};

BOOST_AUTO_TEST_SUITE(test_tcp_transport);

BOOST_FIXTURE_TEST_CASE(test_connect, WithEchoServer) {
  auto io_service = std::make_shared<boost::asio::io_service>();
  auto factory = TcpTransportFactory::New(io_service);

  std::string data("some data some more data");
  char read_buf[64];

  bool done = false;
  factory->StartConnect(server.GetEndpoint(), TRANSPORT_CALLBACK(&) {
    BOOST_CHECK(!ec);
    transport->StartWrite(data, BYTES_CALLBACK(&, transport) {
      BOOST_CHECK(!ec);
      BOOST_CHECK_EQUAL(data.size(), n_bytes);

      // read the first 10 bytes
      transport->StartRead(read_buf, 10, BYTES_CALLBACK(&, transport) {
        BOOST_CHECK(!ec);
        BOOST_CHECK_EQUAL(10, n_bytes);

        // short read
        transport->StartRead(
            read_buf + n_bytes, sizeof(read_buf) - n_bytes,
            BYTES_CALLBACK(&, transport) {
              BOOST_CHECK(!ec);
              BOOST_CHECK_EQUAL(data.size() - 10, n_bytes);
              read_buf[data.size()] = '\0';
              BOOST_CHECK_EQUAL(data, read_buf);
              transport->StartClose(
                  [&](const ec_type& ec) {  // set done after closed
                    BOOST_CHECK(!ec);
                    done = true;
                  });
            },
            true);
      });
    });
  });

  io_service->run();
  BOOST_CHECK(done);
}

BOOST_AUTO_TEST_CASE(test_accept) {
  boost::asio::ip::tcp::endpoint endpoint(
      boost::asio::ip::address::from_string("127.0.0.1"), 47919);
  auto io_service = std::make_shared<boost::asio::io_service>();
  auto factory = TcpTransportFactory::New(io_service);

  int n_clients = 50;
  factory->StartAccept(endpoint, TRANSPORT_CALLBACK(&) {
    auto buf = std::make_shared<std::array<char, 64>>();
    transport->StartRead(
        *buf,
        BYTES_CALLBACK(transport, buf) {
          BOOST_CHECK(!ec);
          // write back the same data
          transport->StartWrite(*buf, n_bytes, BYTES_CALLBACK(transport) {
            BOOST_CHECK(!ec);
            transport->StartClose();  // now close and drop this connection
          });
        },
        true);  // allow short read
    return --n_clients > 0;
  });

  std::vector<std::thread> threads;
  for (int i = 0; i < n_clients; ++i) {
    threads.emplace_back([&io_service, &endpoint, i]() {
      // sleep for some time to wait for the server to start up
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      boost::asio::ip::tcp::socket s(*io_service);
      s.connect(endpoint);
      auto data = std::to_string(std::hash<int>()(i));
      auto len = boost::asio::write(s, boost::asio::buffer(data));

      char read_buf[64];
      boost::asio::read(s, boost::asio::buffer(read_buf, len));
      read_buf[len] = '\0';
      BOOST_CHECK_EQUAL(data, read_buf);

      boost::system::error_code ec;
      s.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
      s.close(ec);
    });
  }

  io_service->run();
  BOOST_CHECK_EQUAL(0, n_clients);

  for (auto& t : threads) {
    t.join();
  }
}

BOOST_FIXTURE_TEST_CASE(test_accept_error, testing::TestTcpTransportFactory) {
  boost::asio::ip::tcp::endpoint endpoint(
      boost::asio::ip::address::from_string("127.0.0.1"), 47999);
  auto io_service = std::make_shared<boost::asio::io_service>();
  auto factory = TcpTransportFactory::New(io_service);

  bool called = false;
  factory->StartAccept(endpoint, TRANSPORT_CALLBACK(&) {
    BOOST_CHECK_EQUAL(boost::asio::error::operation_aborted, ec.value());
    called = true;
    return true;
  });

  GetLastAcceptor(factory).lock()->close();

  io_service->run();
  BOOST_CHECK(called);
}

BOOST_AUTO_TEST_SUITE_END();

}  // namespace thestral
