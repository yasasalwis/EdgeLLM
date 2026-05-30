// EdgeLLM — value-based error handling (no exceptions).
// Arduino-independent. `Status` carries just an Error; `Result<T>` carries a
// value on success or an Error on failure. This keeps every fallible API
// honest about failure without relying on C++ exceptions, which are disabled or
// costly on most microcontroller cores.
#ifndef EDGELLM_CORE_RESULT_H
#define EDGELLM_CORE_RESULT_H

#include <utility>

#include "Errors.h"

namespace edge {

// Lightweight success/failure with no payload.
class Status {
 public:
  Status() : err_(Error::Ok) {}
  Status(Error e) : err_(e) {}  // NOLINT(runtime/explicit) — intentional implicit

  static Status ok() { return Status(Error::Ok); }
  static Status fail(Error e) { return Status(e); }

  bool isOk() const { return err_ == Error::Ok; }
  explicit operator bool() const { return isOk(); }
  Error error() const { return err_; }
  const char* message() const { return errorString(err_); }

 private:
  Error err_;
};

// Success carries a value of type T; failure carries an Error. T is value-stored
// (not heap) so this is allocation-free for trivial/small types.
template <typename T>
class Result {
 public:
  static Result ok(const T& value) { return Result(value); }
  static Result ok(T&& value) { return Result(std::move(value)); }
  static Result fail(Error e) { return Result(e); }

  bool isOk() const { return err_ == Error::Ok; }
  explicit operator bool() const { return isOk(); }
  Error error() const { return err_; }
  const char* message() const { return errorString(err_); }

  // Precondition: isOk(). Accessing the value of a failed Result is a caller bug.
  const T& value() const { return value_; }
  T& value() { return value_; }

  // Returns the contained value, or `fallback` if this Result is an error.
  T valueOr(const T& fallback) const { return isOk() ? value_ : fallback; }

 private:
  explicit Result(const T& v) : err_(Error::Ok), value_(v) {}
  explicit Result(T&& v) : err_(Error::Ok), value_(std::move(v)) {}
  explicit Result(Error e) : err_(e), value_() {}

  Error err_;
  T value_;
};

}  // namespace edge

#endif  // EDGELLM_CORE_RESULT_H
