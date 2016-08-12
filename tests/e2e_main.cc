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
/// Entry point for e2e tests.
#define BOOST_TEST_MODULE thestral e2e tests
#include <boost/test/unit_test.hpp>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace ut = boost::unit_test;

namespace thestral {
namespace testing {

static constexpr const char* kConfigFiles[] = {"example1.conf",
                                               "example2.conf"};

template <int CONFIG_FILE_ID>
struct WithThestralServer {
  WithThestralServer() {
    std::string thestral_bin;
    auto& master_test_suite = boost::unit_test::framework::master_test_suite();
    if (master_test_suite.argc < 2) {
      thestral_bin = "thestral";
    } else {
      thestral_bin = master_test_suite.argv[1];
    }

    if ((server_pid_ = fork())) {
      // parent process
      if (server_pid_ == -1) {
        std::perror("Error occured when creating subprocess");
        std::exit(EXIT_FAILURE);
      }
    } else {
      // child process
      execl(thestral_bin.c_str(), "thestral", "-c",
            kConfigFiles[CONFIG_FILE_ID], nullptr);
      // should not reach here if execl succeeds
      std::perror("Error occured when starting thestral bin");
      std::exit(EXIT_FAILURE);
    }
  }

  ~WithThestralServer() {
    kill(server_pid_, SIGTERM);
    int status;
    waitpid(server_pid_, &status, 0);
  }

 private:
  pid_t server_pid_;
};

// typedefs workaround the limitation of BOOST_GLOBAL_FIXTURE
typedef WithThestralServer<0> WithThestralServer1;
typedef WithThestralServer<1> WithThestralServer2;

template <int MS>
struct WaitFor {
  WaitFor() { std::this_thread::sleep_for(std::chrono::milliseconds(MS)); }
};

typedef WaitFor<500> WaitFor500Ms;

BOOST_GLOBAL_FIXTURE(WithThestralServer1);
BOOST_GLOBAL_FIXTURE(WithThestralServer2);
BOOST_GLOBAL_FIXTURE(WaitFor500Ms);  // wait for the server to startup

}  // namespace testing
}  // namespace thestral
