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
/// Implements mock classes for testing.
#include "mocks.h"

#include <algorithm>

#include <boost/asio.hpp>

namespace thestral {
namespace testing {

namespace asio = boost::asio;

std::shared_ptr<MockTransport> MockTransport::New(const std::string& read_buf) {
  auto p = std::make_shared<MockTransport>();
  p->read_buf = read_buf;
  return p;
}

void MockTransport::StartRead(const asio::mutable_buffers_1& buf,
                              ReadCallbackType callback,
                              bool allow_short_read) {
  auto len = asio::buffer_copy(buf, asio::buffer(read_buf));
  read_buf = read_buf.substr(len);
  if (!allow_short_read && len < asio::buffer_size(buf)) {
    ec = asio::error::make_error_code(
        asio::error::basic_errors::connection_reset);
  }
  callback(ec, len);
}

void MockTransport::StartWrite(const asio::const_buffers_1& buf,
                               WriteCallbackType callback) {
  auto p = asio::buffer_cast<const char*>(buf);
  write_buf.append(p, p + asio::buffer_size(buf));
  callback(ec, asio::buffer_size(buf));
}

void MockTransport::StartClose(CloseCallbackType callback) {
  if (closed) {
    callback(ec);
  } else {
    ec = boost::asio::error::make_error_code(
        boost::asio::error::basic_errors::bad_descriptor);
    closed = true;
    callback(ec_type());
  }
}

}  // namespace testing
}  // namespace thestral
