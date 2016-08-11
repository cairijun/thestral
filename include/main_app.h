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
/// Defines the main application class which loads and runs various components
/// of the program from a given config.
#ifndef THESTRAL_MAIN_APP_H_
#define THESTRAL_MAIN_APP_H_

#include <boost/property_tree/ptree.hpp>

namespace thestral {
class MainApp {
 public:
  explicit MainApp(const boost::property_tree::ptree& config)
      : config_(config) {}

  void Run() const;

 private:
  void SetUpLoggingOrDie() const;

  boost::property_tree::ptree config_;
};
}  // namespace thestral

#endif  // ifndef THESTRAL_MAIN_APP_H_
