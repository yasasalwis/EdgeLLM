# EdgeLLM — CMake build for the NATIVE (host) unit tests.
#
# This is a convenience build for IDEs (e.g. CLion) and an alternative to the
# Makefile. It compiles only the Arduino-independent sources plus the test
# suites — it does NOT build firmware. Build firmware for real boards with
# PlatformIO (see platformio.ini).
#
#   cmake -S . -B build && cmake --build build && ./build/edgellm_tests
cmake_minimum_required(VERSION 3.16)
project(EdgeLLM_NativeTests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

set(EDGELLM_PURE_SOURCES
    src/core/Logger.cpp
    src/core/Capabilities.cpp
    src/transport/HttpTypes.cpp
    src/transport/HttpRequestBuilder.cpp
    src/transport/SseParser.cpp
    src/transport/ChunkedDecoder.cpp
    src/transport/HttpClient.cpp
    src/hal/MemorySecretStore.cpp
    src/tools/ToolCallArgs.cpp
    src/tools/ToolRegistry.cpp
    src/tools/UrlGuard.cpp
    src/mcp/EdgeStore.cpp
    src/mcp/ResourceRegistry.cpp
    src/mcp/PromptRegistry.cpp
    src/mcp/McpServer.cpp
    src/llm/Conversation.cpp
    src/llm/LLMClient.cpp
    src/llm/providers/AnthropicProvider.cpp
    src/llm/providers/OpenAIChatProvider.cpp
    src/llm/providers/GeminiProvider.cpp
    src/llm/providers/OllamaProvider.cpp)

set(EDGELLM_TEST_SOURCES
    test/test_main.cpp
    test/suites/test_core.cpp
    test/suites/test_http.cpp
    test/suites/test_sse.cpp
    test/suites/test_chunked.cpp
    test/suites/test_httpclient.cpp
    test/suites/test_secretstore.cpp
    test/suites/test_conversation.cpp
    test/suites/test_providers.cpp
    test/suites/test_llmclient.cpp
    test/suites/test_tools.cpp
    test/suites/test_provider_tools.cpp
    test/suites/test_agentloop.cpp
    test/suites/test_urlguard.cpp
    test/suites/test_edgestore.cpp
    test/suites/test_mcp_server.cpp
    test/suites/test_mcp_resources.cpp)

add_executable(edgellm_tests ${EDGELLM_PURE_SOURCES} ${EDGELLM_TEST_SOURCES})
# ArduinoJson is vendored for the host test build only (on-device it comes from
# the Library Manager / PlatformIO lib_deps).
target_include_directories(edgellm_tests PRIVATE test/third_party/ArduinoJson)

enable_testing()
add_test(NAME edgellm_tests COMMAND edgellm_tests)
