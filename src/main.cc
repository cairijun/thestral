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

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>

#include <boost/property_tree/info_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include "main_app.h"

struct CommandLineArguments {
  std::string config_file_name;
};

CommandLineArguments ParseCommandLineOrDie(int argc, char* argv[]) {
  CommandLineArguments args;

  int state = 0;  // 0: START, 1: READY, 2: -c
  for (int i = 1; i < argc; ++i) {
    std::string arg(argv[i]);
    switch (state) {
      case 0:
      case 1:
        if (arg == "-c" || arg == "--config") {
          state = 2;
        } else {
          std::cerr << "unexpected argument: " << arg << std::endl;
          goto print_usage_and_exit;
        }
        break;
      case 2:
        args.config_file_name = arg;
        state = 1;
        break;
      default:
        assert(false);
    }
  }

  switch (state) {
    case 0:
      goto print_usage_and_exit;
    case 1:
      return args;
    case 2:
      std::cerr << "expects an argument after -c or --config" << std::endl;
      goto print_usage_and_exit;
    default:
      assert(false);
  }

print_usage_and_exit:
  std::cout << "usage: " << argv[0] << " -c CONFIG_FILE_NAME" << std::endl;
  std::exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {
  auto args = ParseCommandLineOrDie(argc, argv);

  boost::property_tree::ptree config;
  boost::property_tree::read_info(args.config_file_name, config);

  thestral::MainApp app(config);
  app.Run();

  return EXIT_SUCCESS;
}
