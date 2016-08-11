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
/// Implements some methods in base.h.
#include "base.h"

namespace thestral {

void PacketBase::StartWriteTo(
    const std::shared_ptr<TransportBase>& transport,
    const TransportBase::WriteCallbackType& callback) const {
  auto data = std::make_shared<std::string>(Serialize());
  transport->StartWrite(  // capture `data` to ensure it outlives this function
      *data, [callback, data](const ec_type& ec, size_t bytes_written) {
        callback(ec, bytes_written);
      });
}

}  // namespace thestral
