// EdgeLLM — minimal, dependency-free unit test framework.
// Single header. Tests self-register via the TEST() macro; test_main.cpp calls
// edgetest::runAll(). Chosen over Unity/doctest so the suite builds with nothing
// but a C++17 compiler (no package downloads), which matches CI and keeps the
// native test loop fast.
#ifndef EDGELLM_TEST_FRAMEWORK_H
#define EDGELLM_TEST_FRAMEWORK_H

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace edgetest {

struct TestCase {
  const char* name;
  std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
  static std::vector<TestCase> r;
  return r;
}
inline int& failures() {
  static int f = 0;
  return f;
}
inline int& checks() {
  static int c = 0;
  return c;
}

struct Registrar {
  Registrar(const char* name, std::function<void()> fn) {
    registry().push_back({name, std::move(fn)});
  }
};

inline void reportFail(const char* file, int line, const std::string& msg) {
  failures()++;
  std::printf("    FAIL %s:%d  %s\n", file, line, msg.c_str());
}

inline int runAll() {
  int failedTests = 0;
  for (auto& tc : registry()) {
    const int before = failures();
    tc.fn();
    if (failures() > before) {
      failedTests++;
      std::printf("[ FAIL ] %s\n", tc.name);
    } else {
      std::printf("[  ok  ] %s\n", tc.name);
    }
  }
  std::printf("\n%zu tests, %d checks, %d failures (%d test(s) failing)\n", registry().size(),
              checks(), failures(), failedTests);
  return failures() == 0 ? 0 : 1;
}

}  // namespace edgetest

#define TEST(name)                                             \
  static void name();                                          \
  static edgetest::Registrar edgetest_reg_##name(#name, name); \
  static void name()

#define CHECK(cond)                                                                \
  do {                                                                             \
    edgetest::checks()++;                                                          \
    if (!(cond)) edgetest::reportFail(__FILE__, __LINE__, "CHECK failed: " #cond); \
  } while (0)

#define CHECK_EQ(a, b)                                                                             \
  do {                                                                                             \
    edgetest::checks()++;                                                                          \
    if (!((a) == (b))) edgetest::reportFail(__FILE__, __LINE__, "CHECK_EQ failed: " #a " == " #b); \
  } while (0)

#define CHECK_STR_EQ(a, b)                                                                     \
  do {                                                                                         \
    edgetest::checks()++;                                                                      \
    std::string _va = (a);                                                                     \
    std::string _vb = (b);                                                                     \
    if (_va != _vb)                                                                            \
      edgetest::reportFail(__FILE__, __LINE__,                                                 \
                           std::string("CHECK_STR_EQ failed: " #a " == " #b "  got [") + _va + \
                               "] expected [" + _vb + "]");                                    \
  } while (0)

#endif  // EDGELLM_TEST_FRAMEWORK_H
