// EdgeLLM native test runner entry point. Test cases self-register from the
// suites/ translation units; this simply runs them all and returns non-zero on
// any failure so CI fails the build.
#include "framework/edge_test.h"

int main() {
  std::printf("EdgeLLM native test suite\n\n");
  return edgetest::runAll();
}
