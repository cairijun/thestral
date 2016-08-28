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
/// Tests for SSL transport.
#include "ssl.h"

#include <array>
#include <memory>

#include <boost/asio.hpp>
#include <boost/test/unit_test.hpp>

#include "tcp_transport.h"

#define TRANSPORT_CALLBACK(...)    \
  [__VA_ARGS__](const ec_type& ec, \
                const std::shared_ptr<TransportBase>& transport)
#define BYTES_CALLBACK(...) [__VA_ARGS__](const ec_type& ec, size_t n_bytes)

namespace thestral {
namespace ssl {

namespace {
std::shared_ptr<TcpTransportFactory> MakeServerTransportFactory(
    const std::shared_ptr<boost::asio::io_service>& io_service) {
  return SslTransportFactoryBuilder()
      .LoadCaFile("ca.pem")
      .LoadCertChain("test.server.pem")
      .LoadPrivateKey("test.server.key.pem")
      .LoadDhParams("dh2048.pem")
      .SetVerifyPeer(true)
      .Build(io_service);
}

std::shared_ptr<TcpTransportFactory> MakeClientTransportFactory(
    const std::shared_ptr<boost::asio::io_service>& io_service,
    const std::string& server_addr = "127.0.0.1") {
  return SslTransportFactoryBuilder()
      .LoadCaFile("ca.pem")
      .LoadCertChain("test.pem")
      .LoadPrivateKey("test.key.pem")
      .SetVerifyPeer(true)
      .SetVerifyHost(server_addr)
      .Build(io_service);
}

}  // anonymous namespace

BOOST_AUTO_TEST_SUITE(test_ssl_transport);

BOOST_AUTO_TEST_CASE(test_ssl_transport) {
  auto io_service = std::make_shared<boost::asio::io_service>();
  auto server_transport_factory = MakeServerTransportFactory(io_service);
  auto client_transport_factory = MakeClientTransportFactory(io_service);

  std::string data_to_client = "data to client";
  std::string data_to_server = "data to server";

  boost::asio::ip::tcp::endpoint endpoint(
      boost::asio::ip::address::from_string("127.0.0.1"), 51892);

  bool accept_done = false;
  server_transport_factory->StartAccept(endpoint, TRANSPORT_CALLBACK(&) {
    BOOST_TEST(!ec);

    auto read_buf = std::make_shared<std::array<char, 64>>();
    transport->StartRead(
        *read_buf, data_to_server.size(),
        BYTES_CALLBACK(&, read_buf, transport) {
          BOOST_TEST(!ec);
          BOOST_CHECK_EQUAL(data_to_server.size(), n_bytes);
          read_buf->at(n_bytes) = '\0';
          BOOST_CHECK_EQUAL(data_to_server, read_buf->data());

          transport->StartWrite(data_to_client, BYTES_CALLBACK(&, transport) {
            BOOST_TEST(!ec);
            BOOST_CHECK_EQUAL(data_to_client.size(), n_bytes);

            transport->StartClose(
                [&](const ec_type& ec) { accept_done = true; });
          });
        });

    return false;
  });

  bool connect_done = false;
  client_transport_factory->StartConnect(endpoint, TRANSPORT_CALLBACK(&) {
    BOOST_TEST(!ec);

    transport->StartWrite(data_to_server, BYTES_CALLBACK(&, transport) {
      BOOST_TEST(!ec);
      BOOST_CHECK_EQUAL(data_to_server.size(), n_bytes);

      auto read_buf = std::make_shared<std::array<char, 64>>();
      transport->StartRead(
          *read_buf, data_to_client.size(),
          BYTES_CALLBACK(&, read_buf, transport) {
            BOOST_TEST(!ec);
            BOOST_CHECK_EQUAL(data_to_client.size(), n_bytes);
            read_buf->at(n_bytes) = '\0';
            BOOST_CHECK_EQUAL(data_to_client, read_buf->data());

            transport->StartClose(
                [&](const ec_type& ec) { connect_done = true; });
          });
    });
  });

  io_service->run();
  BOOST_TEST(accept_done);
  BOOST_TEST(connect_done);
}

BOOST_FIXTURE_TEST_CASE(test_accept_error, testing::TestTcpTransportFactory) {
  auto io_service = std::make_shared<boost::asio::io_service>();
  auto factory = MakeServerTransportFactory(io_service);
  boost::asio::ip::tcp::endpoint endpoint(
      boost::asio::ip::address::from_string("127.0.0.1"), 51893);

  bool called = false;
  factory->StartAccept(endpoint, TRANSPORT_CALLBACK(&) {
    BOOST_CHECK_EQUAL(boost::asio::error::operation_aborted, ec.value());
    called = true;
    return true;
  });

  GetLastAcceptor(factory).lock()->close();

  io_service->run();
  BOOST_TEST(called);
}

BOOST_AUTO_TEST_CASE(test_handshake_error) {
  auto io_service = std::make_shared<boost::asio::io_service>();
  auto server_transport_factory = MakeServerTransportFactory(io_service);
  auto client_transport_factory = TcpTransportFactory::New(io_service);
  boost::asio::ip::tcp::endpoint endpoint(
      boost::asio::ip::address::from_string("127.0.0.1"), 51894);

  bool called = false;
  server_transport_factory->StartAccept(endpoint, TRANSPORT_CALLBACK(&) {
    BOOST_TEST((boost::asio::error::get_ssl_category() == ec.category()));
    called = true;
    return false;
  });

  client_transport_factory->StartConnect(
      endpoint, TRANSPORT_CALLBACK(&) { transport->StartClose(); });

  io_service->run();
  BOOST_TEST(called);
}

BOOST_AUTO_TEST_CASE(test_verify_host) {
  auto io_service = std::make_shared<boost::asio::io_service>();
  auto server_transport_factory = MakeServerTransportFactory(io_service);
  auto client_transport_factory =
      MakeClientTransportFactory(io_service, "mismatch.host");
  boost::asio::ip::tcp::endpoint endpoint(
      boost::asio::ip::address::from_string("127.0.0.1"), 51895);

  bool server_called = false;
  server_transport_factory->StartAccept(endpoint, TRANSPORT_CALLBACK(&) {
    BOOST_TEST((boost::asio::error::get_ssl_category() == ec.category()));
    server_called = true;
    return false;
  });

  bool client_called = false;
  client_transport_factory->StartConnect(endpoint, TRANSPORT_CALLBACK(&) {
    BOOST_TEST((boost::asio::error::get_ssl_category() == ec.category()));
    client_called = true;
  });

  io_service->run();
  BOOST_TEST(server_called);
  BOOST_TEST(client_called);
}

BOOST_AUTO_TEST_SUITE_END();

}  // namespace ssl
}  // namespace thestral
