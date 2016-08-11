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
/// Implements \ref thestral::MainApp.
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <boost/asio.hpp>

#include "direct_upstream.h"
#include "logging.h"
#include "main_app.h"
#include "socks_server.h"
#include "socks_upstream.h"
#include "ssl.h"
#include "tcp_transport.h"

namespace thestral {
namespace pt = boost::property_tree;

namespace {

template <typename T>
[[noreturn]] void DieOf(const T& reason) {
  std::cerr << reason << std::endl;
  std::exit(EXIT_FAILURE);
}

template <typename T0, typename T1, typename... T>
[[noreturn]] void DieOf(const T0& reason0, const T1& reason1,
                        const T&... reasons) {
  std::cerr << reason0;
  DieOf(reason1, reasons...);
}

logging::Level ParseLevelOrDie(const std::string& level_str) {
  if (level_str == "debug") {
    return logging::Level::DEBUG;
  }
  if (level_str == "info") {
    return logging::Level::INFO;
  }
  if (level_str == "warn") {
    return logging::Level::WARN;
  }
  if (level_str == "error") {
    return logging::Level::ERROR;
  }
  if (level_str == "fatal") {
    return logging::Level::FATAL;
  }
  DieOf("unknown logging level in config file: ", level_str);
}

std::shared_ptr<TcpTransportFactory> MakeTcpTransportFactoryOrDie(
    pt::ptree config,
    const std::shared_ptr<boost::asio::io_service>& io_service_ptr) {
  auto ssl_iter = config.find("ssl");
  if (ssl_iter == config.not_found()) {
    return TcpTransportFactory::New(io_service_ptr);

  } else {
    auto ssl_config = ssl_iter->second;
    ssl::SslTransportFactoryBuilder builder;
    if (auto val = ssl_config.get_optional<std::string>("ca")) {
      builder.LoadCaFile(*val);
    }
    if (auto val = ssl_config.get_optional<std::string>("cert")) {
      builder.LoadCert(*val);
    }
    if (auto val = ssl_config.get_optional<std::string>("cert_chain")) {
      builder.LoadCertChain(*val);
    }
    if (auto val = ssl_config.get_optional<std::string>("private_key")) {
      builder.LoadPrivateKey(*val);
    }
    if (auto val = ssl_config.get_optional<std::string>("dh_param")) {
      builder.LoadDhParams(*val);
    }
    if (auto val = ssl_config.get_optional<int>("verify_depth")) {
      builder.SetVerifyDepth(*val);
    }
    if (auto val = ssl_config.get_optional<std::string>("verify_peer")) {
      if (*val == "true") {
        builder.SetVerifyPeer(true);
      } else if (*val == "false") {
        builder.SetVerifyPeer(false);
      } else {
        DieOf("unknown value of option \"verify_peer\": ", *val);
      }
    }
    return builder.Build(io_service_ptr);
  }
}

std::shared_ptr<UpstreamFactoryBase> MakeUpstreamFactoryOrDie(
    pt::ptree config,
    const std::shared_ptr<boost::asio::io_service>& io_service_ptr) {
  auto transport_factory = MakeTcpTransportFactoryOrDie(config, io_service_ptr);
  if (config.data() == "direct") {
    return DirectTcpUpstreamFactory::New(transport_factory);

  } else if (config.data() == "socks") {
    auto upstream_host = config.get<std::string>("address");
    auto upstream_port = config.get<uint16_t>("port");
    return socks::SocksTcpUpstreamFactory::New(transport_factory, upstream_host,
                                               upstream_port);

  } else {
    DieOf("unknown upstream type: ", config.data());
  }
}

}  // anonymous namespace

void MainApp::Run() const {
  SetUpLoggingOrDie();

  auto server_iter = config_.equal_range("server");
  if (server_iter.first == server_iter.second) {
    DieOf("no server configuration provided in the config file");
  }

  auto io_service_ptr = std::make_shared<boost::asio::io_service>();

  // It shouldn't be necessary to maintain the server objects here as
  // ServerBase.Start() will manage the lifetime of themselves. But we still
  // hold pointers to them since we may need to operate on them (to support, for
  // example, more graceful shutdown) in the future.
  std::vector<std::shared_ptr<ServerBase>> servers;
  for (auto i = server_iter.first; i != server_iter.second; ++i) {
    if (i->second.data() != "socks") {
      DieOf("unknown server type: ", i->second.data());
    }

    auto address = i->second.get<std::string>("address");
    auto port = i->second.get<uint16_t>("port");
    auto transport_factory =
        MakeTcpTransportFactoryOrDie(i->second, io_service_ptr);
    auto upstream_config = i->second.get_child("upstream");
    auto upstream = MakeUpstreamFactoryOrDie(upstream_config, io_service_ptr);

    servers.emplace_back(
        socks::SocksTcpServer::New(address, port, transport_factory, upstream));
    servers.back()->Start();
  }

  io_service_ptr->run();
}

void MainApp::SetUpLoggingOrDie() const {
  auto iter = config_.find("log");
  if (iter == config_.not_found()) {
    return;
  }

  for (auto& sink : iter->second) {
    if (sink.first == "stderr") {
      logging::add_stderr_log_sink().SetLevel(
          ParseLevelOrDie(sink.second.data()));

    } else if (sink.first == "file") {
      auto path_iter = sink.second.find("path");
      if (path_iter == sink.second.not_found()) {
        DieOf("log file path must be supplied for file log sink");
      }
      std::string path = path_iter->second.data();

      auto mode_iter = sink.second.find("mode");
      bool truncate;
      if (mode_iter == sink.second.not_found()) {
        truncate = false;
      } else if (mode_iter->second.data() == "append") {
        truncate = false;
      } else if (mode_iter->second.data() == "truncate") {
        truncate = true;
      } else {
        DieOf("unknown file open mode for file log sink: ",
              mode_iter->second.data());
      }

      logging::add_file_log_sink(path, truncate)
          .SetLevel(ParseLevelOrDie(sink.second.data()));

    } else {
      DieOf("unknown log sink: ", sink.first);
    }
  }
}

}  // namespace thestral
