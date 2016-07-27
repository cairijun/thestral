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
/// Tests for logging facility.
#include "logging.h"

#include <iostream>
#include <memory>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>

namespace thestral {
namespace logging {

template <typename C, typename T>
std::basic_ostream<C, T>& operator<<(std::basic_ostream<C, T>& os,
                                     Level level) {
  switch (level) {
    case Level::INFO:
      os << "Level::INFO";
      break;
    case Level::WARN:
      os << "Level::WARN";
      break;
    case Level::ERROR:
      os << "Level::ERROR";
      break;
    case Level::FATAL:
      os << "Level::FATAL";
      break;
    default:
      os << "Level::UNKNOWN";
  }
  return os;
}

namespace testing {
class TestingLogSink : public impl::LogSinkBase {
 public:
  void StoreRecord(const impl::LogRecord& record) override {
    std::ostringstream buf;
    for (auto& p : compiled_format_) {
      buf << p.GetStringFromMap(record.GetAttributes());
    }
    logs_.push_back(buf.str());
  }

  const std::vector<std::string>& GetLogs() const { return logs_; }

 private:
  std::vector<std::string> logs_;
};

class TestLoggingFixture {
 public:
  TestLoggingFixture() {
    log1.SetAttribute("message", "msg of I");
    log2.SetAttribute("message", "msg of W");
    log3.SetAttribute("message", "msg of E");
    log4.SetAttribute("message", "msg of W 2");
    log5.SetAttribute("message", "msg of E 2");
    log6.SetAttribute("message", "msg of F");
  }

 protected:
  std::map<std::string, std::string> attributes{{"time", "LOG TIME"},
                                                {"level", "LOG LEVEL"},
                                                {"logger_name", "LOGGER NAME"},
                                                {"attr", "VALUE"}};

  impl::LogRecord log1{Level::INFO, attributes};
  impl::LogRecord log2{Level::WARN, attributes};
  impl::LogRecord log3{Level::ERROR, attributes};
  impl::LogRecord log4{Level::WARN, attributes};
  impl::LogRecord log5{Level::ERROR, attributes};
  impl::LogRecord log6{Level::FATAL, attributes};
};
}

BOOST_AUTO_TEST_SUITE(test_logging);

BOOST_FIXTURE_TEST_CASE(test_log_sink_base, testing::TestLoggingFixture) {
  testing::TestingLogSink sink;
  BOOST_CHECK_EQUAL(Level::WARN, sink.GetLevel());

  sink.PushRecord(log1);
  sink.PushRecord(log2);
  sink.SetFormat("{message} suffix");
  sink.PushRecord(log3);
  sink.SetFormat("{message}");
  sink.SetLevel(Level::ERROR);
  sink.SetFormat("prefix {message} {not_exist}suffix");
  sink.PushRecord(log4);
  sink.PushRecord(log5);
  sink.SetFormat("{message}");
  sink.PushRecord(log6);

  BOOST_CHECK_EQUAL(Level::ERROR, sink.GetLevel());
  auto& logs = sink.GetLogs();
  BOOST_CHECK_EQUAL(4, logs.size());
  BOOST_CHECK_EQUAL("[LOG TIME][LOG LEVEL][LOGGER NAME]: msg of W", logs[0]);
  BOOST_CHECK_EQUAL("msg of E suffix", logs[1]);
  BOOST_CHECK_EQUAL("prefix msg of E 2 suffix", logs[2]);
  BOOST_CHECK_EQUAL("msg of F", logs[3]);
}

BOOST_AUTO_TEST_CASE(test_logger) {
  std::unique_ptr<testing::TestingLogSink> sink(new testing::TestingLogSink);
  sink->SetFormat("[{level}][{logger_name}][{message}][{attr}]");
  auto& logs = sink->GetLogs();  // hold a reference to the log list first
  add_log_sink(std::move(sink));

  Logger logger("test");
  BOOST_CHECK_EQUAL("test", logger.GetName());
  logger.SetAttribute("attr", "value");
  logger.Info("will not output");
  logger.Warn("msg %d (%s)", 42, "forty-two");
  logger.Error("change attr").SetAttribute("attr", "another value");

  BOOST_CHECK_EQUAL(2, logs.size());
  BOOST_CHECK_EQUAL("[W][test][msg 42 (forty-two)][value]", logs[0]);
  BOOST_CHECK_EQUAL("[E][test][change attr][another value]", logs[1]);

  std::string super_long(1000, 'a');
  logger.Log(Level::ERROR, super_long.c_str());
  BOOST_CHECK_EQUAL(3, logs.size());
  BOOST_CHECK_EQUAL("[E][test][" + super_long + "][value]", logs[2]);

  logger.SetName("test2");
  BOOST_CHECK_EQUAL("test2", logger.GetName());
  BOOST_CHECK_THROW(logger.Fatal("fatal"), FatalEvent);
  BOOST_CHECK_EQUAL(4, logs.size());
  BOOST_CHECK_EQUAL("[F][test2][fatal][value]", logs[3]);
}

BOOST_AUTO_TEST_SUITE_END();

}  // namespace logging
}  // namespace thestral
