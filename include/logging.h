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
/// Defines logging facility.
#ifndef THESTRAL_LOG_H_
#define THESTRAL_LOG_H_

#include <cstdarg>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace thestral {
namespace logging {

class Logger;

enum class Level { DEBUG, INFO, WARN, ERROR, FATAL };

namespace impl {
/// A record of logging. A LogRecord is comprised of a Level and a collection of
/// attributes with string keys.
class LogRecord {
 public:
  LogRecord(Level level, const std::map<std::string, std::string>& attributes)
      : level_(level), attributes_(attributes) {}

  /// Sets an attribute of the log record.
  void SetAttribute(const std::string& name, const std::string& value) {
    attributes_[name] = value;
  }

  /// Returns the level of the log record.
  Level GetLevel() const { return level_; }

  /// Returns a constant reference to the attribute map.
  const std::map<std::string, std::string>& GetAttributes() const {
    return attributes_;
  }

 private:
  Level level_;
  std::map<std::string, std::string> attributes_;
};

/// Base class of log sinks. A log sink accepts LogRecord objects, and stores
/// them into some place if the levels of the records are *not lower than* the
/// level of the sink.
class LogSinkBase {
 public:
  LogSinkBase() { SetFormat("[{time}][{level}][{logger_name}]: {message}"); }

  LogSinkBase(const LogSinkBase&) = delete;

  /// Returns a reference to the mutex of the sink. The mutex should be locked
  /// before logging operations.
  std::mutex& GetMutex() { return mtx_; }
  /// Returns the level of the sink.
  Level GetLevel() const { return level_; }
  /// Sets the level of the sink.
  void SetLevel(Level level) { level_ = level; }

  /// Sets the format string of the sink. Patterns like `"{attr_name}"` in the
  /// format will be replaced with the attribute values of key `attr_name` in
  /// the log records when storing. If a required attribute is missing in the
  /// record, an empty string will be used. The format string will be compiled
  /// in a bid to improve performance.
  void SetFormat(const std::string& format);
  /// Pushes a log record into the sink. If the level of record is not lower
  /// than the level of the sink, the record will be stored using StoreRecord().
  void PushRecord(const LogRecord& record);

 protected:
  /// Component of the compiled format string. A component can generate a string
  /// given an attribute map.
  class CompiledFormatPart {
   public:
    CompiledFormatPart(const std::string& constant_or_key, bool is_constant)
        : constant_or_key_(constant_or_key), is_constant_(is_constant) {}

    std::string GetStringFromMap(
        const std::map<std::string, std::string>& map) const;

   private:
    const std::string constant_or_key_;
    bool is_constant_;
  };

  std::vector<CompiledFormatPart> compiled_format_;

  /// Actually stores a record.
  virtual void StoreRecord(const LogRecord& record) = 0;

 private:
  std::mutex mtx_;
  Level level_ = Level::WARN;
};

}  // namespace impl

/// Adds a log sink storing to a given file.
impl::LogSinkBase& add_file_log_sink(const std::string& filename,
                                     bool truncate = false);
/// Adds a log sink writing to standard error file.
impl::LogSinkBase& add_stderr_log_sink();
/// Adds a customized log sink.
impl::LogSinkBase& add_log_sink(std::unique_ptr<impl::LogSinkBase> sink);

class FatalEvent : public std::runtime_error {
 public:
  FatalEvent() : std::runtime_error("a fatal event is logged") {}
};

/// Logger class used to collect logs.
class Logger {
 public:
  /// A proxy for setting attributes of created log record. The log record will
  /// be pushed to log sinks during the destruction of the proxy object. The
  /// users MUST NOT hold this proxy object.
  class LogRecordProxy {
   public:
    ~LogRecordProxy() throw(FatalEvent);
    LogRecordProxy& SetAttribute(const std::string& name,
                                 const std::string& value) {
      record_.SetAttribute(name, value);
      return *this;
    }

   private:
    friend class Logger;
    impl::LogRecord record_;

    explicit LogRecordProxy(impl::LogRecord&& record)
        : record_(std::move(record)) {}
  };

  explicit Logger(const std::string& name);

  /// Sets the name of the logger.
  void SetName(const std::string& name);
  /// Returns the name of the logger.
  std::string GetName() const;
  /// The an attribute of the logger. Log records created from this logger will
  /// inherit this attribute.
  void SetAttribute(const std::string& name, const std::string& value);

  /// Create a log with the given level and message. The message will be
  /// formated using the standard C-style formating syntax. When a log record is
  /// created, some attributes will be added automatically: `logger_name` is the
  /// name of this logger, `time` is the string representation of the local
  /// creation time of the log, `level` is a symbol representing the level of
  /// the log, and `message` is the log message. If a log of level FATAL is
  /// created, a FatalEvent exception will be thrown.
  LogRecordProxy Log(Level level, const char* format, ...) const;
  LogRecordProxy Debug(const char* format, ...) const;
  LogRecordProxy Info(const char* format, ...) const;
  LogRecordProxy Warn(const char* format, ...) const;
  LogRecordProxy Error(const char* format, ...) const;
  LogRecordProxy Fatal(const char* format, ...) const;

 private:
  std::map<std::string, std::string> attributes_;

  LogRecordProxy Log(Level level, const char* format, va_list args) const;
};

}  // namespace logging
}  // namespace thestral
#endif  // THESTRAL_LOG_H_
