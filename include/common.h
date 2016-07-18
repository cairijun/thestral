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
/// Defines some common types and utilities.
#ifndef THESTRAL_COMMON_
#define THESTRAL_COMMON_

#include <cstdint>
#include <string>

namespace thestral {

/// Types of addresses. Its values follow the definition of SOCKS protocol.
enum class AddressType : uint8_t {
  kIPv4 = 0x1,
  kDomainName = 0x3,
  kIPv6 = 0x4,
};

/// Address type used across the program.
struct Address {
  AddressType type = AddressType::kIPv4;  ///< Type of the address
  std::string host{0, 0, 0, 0};           ///< Host string of the address
  uint16_t port = 0;                      ///< Port number of the address
};

}  // namespace thestral
#endif /* ifndef THESTRAL_COMMON_ */
