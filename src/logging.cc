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
/// Implements logging facility.
#include "logging.h"

#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

#if defined(THESTRAL_USE_BOOST_REGEX)
#include <boost/regex.hpp>
namespace re = boost;
#else
#include <regex>
namespace re = std;
#endif

namespace thestral {
namespace logging {

namespace impl {

std::string level_to_string(Level level) {
  switch (level) {
    case Level::DEBUG:
      return "D";
    case Level::INFO:
      return "I";
    case Level::WARN:
      return "W";
    case Level::ERROR:
      return "E";
    case Level::FATAL:
      return "F";
    default:
      return "?";
  }
}

std::string LogSinkBase::CompiledFormatPart::GetStringFromMap(
    const std::map<std::string, std::string>& map) const {
  if (is_constant_) {
    return constant_or_key_;
  } else {
    auto iter = map.find(constant_or_key_);
    if (iter != map.end()) {
      return iter->second;
    } else {
      return "";
    }
  }
}

class FileLogSink : public LogSinkBase {
 public:
  FileLogSink(const std::string& filename,
              std::ios_base::openmode mode = std::ios_base::out |
                                             std::ios_base::app)
      : ofs_(filename, mode) {}

  void StoreRecord(const LogRecord& record) override {
    for (auto& p : compiled_format_) {
      ofs_ << p.GetStringFromMap(record.GetAttributes());
    }
    ofs_ << std::endl;
  }

 private:
  std::ofstream ofs_;
};

class StdErrLogSink : public LogSinkBase {
 public:
  void StoreRecord(const LogRecord& record) override {
    for (auto& p : compiled_format_) {
      std::cerr << p.GetStringFromMap(record.GetAttributes());
    }
    std::cerr << std::endl;
  }
};

void LogSinkBase::SetFormat(const std::string& format) {
  re::regex re("\\{([_[:alnum:]]+?)\\}");
  re::sregex_iterator search_iter(format.begin(), format.end(), re);
  re::sregex_iterator search_end;

  compiled_format_.clear();
  auto last_end = format.cbegin();
  while (search_iter != search_end) {
    auto m = *search_iter;
    if (last_end != m[0].first) {  // constant string before current match
      compiled_format_.emplace_back(std::string(last_end, m[0].first), true);
    }
    compiled_format_.emplace_back(m[1], false);  // m[1] is the current attr key
    last_end = m[0].second;
    ++search_iter;
  }

  if (last_end != format.cend()) {
    compiled_format_.emplace_back(std::string(last_end, format.cend()), true);
  }
}

void LogSinkBase::PushRecord(const LogRecord& record) {
  if (record.GetLevel() >= level_) {
    StoreRecord(record);
  }
}

std::vector<std::unique_ptr<LogSinkBase>> g_log_sinks;

}  // namespace impl

impl::LogSinkBase& add_file_log_sink(const std::string& filename,
                                     bool truncate) {
  impl::g_log_sinks.emplace_back(new impl::FileLogSink(
      filename, std::ios_base::out |
                    (truncate ? std::ios_base::trunc : std::ios_base::app)));
  return *impl::g_log_sinks.back();
}

impl::LogSinkBase& add_stderr_log_sink() {
  impl::g_log_sinks.emplace_back(new impl::StdErrLogSink());
  return *impl::g_log_sinks.back();
}

impl::LogSinkBase& add_log_sink(std::unique_ptr<impl::LogSinkBase> sink) {
  impl::g_log_sinks.emplace_back(std::move(sink));
  return *impl::g_log_sinks.back();
}

Logger::Logger(const std::string& name) { SetName(name); }

void Logger::SetName(const std::string& name) {
  attributes_["logger_name"] = name;
}

void Logger::SetAttribute(const std::string& name, const std::string& value) {
  attributes_[name] = value;
}

std::string Logger::GetName() const { return attributes_.at("logger_name"); }

Logger::LogRecordProxy Logger::Log(Level level, const char* format,
                                   va_list args) const {
  // we may need to call vsnprintf twice, but a va_list can be used only once
  va_list args_clone;
  va_copy(args_clone, args);
  // only unique_ptr can manage array in C++11, DO NOT use share_ptr here
  // 128 bytes should be sufficient in most cases
  std::unique_ptr<char[]> buf(new char[128]);
  auto msg_len = std::vsnprintf(buf.get(), 128, format, args);

  if (msg_len > 127) {  // buffer is not large enough
    buf.reset(new char[msg_len + 1]);
    std::vsnprintf(buf.get(), msg_len + 1, format, args_clone);
  }
  va_end(args_clone);

  auto attributes = attributes_;
  attributes["message"] = buf.get();

  char time_buf[64];
  std::time_t t = std::time(nullptr);
  std::strftime(time_buf, sizeof(time_buf), "%x %X", std::localtime(&t));
  attributes["time"] = time_buf;

  attributes["level"] = impl::level_to_string(level);

  return LogRecordProxy(impl::LogRecord(level, attributes));
}

Logger::LogRecordProxy::~LogRecordProxy() throw(FatalEvent) {
  // the incremental locking here ensures records are in the same order in all
  // sinks. But be cautious about the order of locking to avoid deadlock!
  std::vector<std::unique_lock<std::mutex>> lock_guards;
  for (auto& sink : impl::g_log_sinks) {
    lock_guards.emplace_back(sink->GetMutex());
    sink->PushRecord(record_);
  }

  // Throwing in dtor is normally bad. But we required the users not to hold the
  // proxy object. So it should be okay here.
  if (record_.GetLevel() == Level::FATAL) {
    throw FatalEvent();
  }
}

Logger::LogRecordProxy Logger::Log(Level level, const char* format, ...) const {
  va_list args;
  va_start(args, format);
  auto record = Log(level, format, args);
  va_end(args);
  return record;
}

Logger::LogRecordProxy Logger::Debug(const char* format, ...) const {
  va_list args;
  va_start(args, format);
  auto record = Log(Level::DEBUG, format, args);
  va_end(args);
  return record;
}

Logger::LogRecordProxy Logger::Info(const char* format, ...) const {
  va_list args;
  va_start(args, format);
  auto record = Log(Level::INFO, format, args);
  va_end(args);
  return record;
}

Logger::LogRecordProxy Logger::Warn(const char* format, ...) const {
  va_list args;
  va_start(args, format);
  auto record = Log(Level::WARN, format, args);
  va_end(args);
  return record;
}

Logger::LogRecordProxy Logger::Error(const char* format, ...) const {
  va_list args;
  va_start(args, format);
  auto record = Log(Level::ERROR, format, args);
  va_end(args);
  return record;
}

Logger::LogRecordProxy Logger::Fatal(const char* format, ...) const {
  va_list args;
  va_start(args, format);
  auto record = Log(Level::FATAL, format, args);
  va_end(args);
  return record;
}

}  // namespace logging
}  // namespace thestral
