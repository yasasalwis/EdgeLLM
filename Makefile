# EdgeLLM — native (host) unit test build.
#
# Compiles only the Arduino-independent sources together with the test suites.
# On-device sources (ArduinoConnection, NtpTimeSource, PreferencesSecretStore)
# are guarded and excluded here; they are compile-tested for real boards via
# PlatformIO in CI.
#
#   make test    build and run the native unit tests
#   make clean   remove build artifacts

CXX      ?= c++
# ArduinoJson (vendored for host builds only) is needed by the provider sources.
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -O2 -g
INCLUDES := -Itest/third_party/ArduinoJson

PURE_SRC := \
  src/core/Logger.cpp \
  src/core/Capabilities.cpp \
  src/transport/HttpTypes.cpp \
  src/transport/HttpRequestBuilder.cpp \
  src/transport/SseParser.cpp \
  src/transport/ChunkedDecoder.cpp \
  src/transport/HttpClient.cpp \
  src/hal/MemorySecretStore.cpp \
  src/hal/EepromCodec.cpp \
  src/tools/ToolCallArgs.cpp \
  src/tools/ToolRegistry.cpp \
  src/tools/SchemaUtil.cpp \
  src/tools/UrlGuard.cpp \
  src/mcp/EdgeStore.cpp \
  src/mcp/ResourceRegistry.cpp \
  src/mcp/PromptRegistry.cpp \
  src/mcp/McpServer.cpp \
  src/provisioning/ProvisioningService.cpp \
  src/llm/Conversation.cpp \
  src/llm/LLMClient.cpp \
  src/llm/providers/AnthropicProvider.cpp \
  src/llm/providers/OpenAIChatProvider.cpp \
  src/llm/providers/GeminiProvider.cpp \
  src/llm/providers/OllamaProvider.cpp

TEST_SRC := \
  test/test_main.cpp \
  test/suites/test_core.cpp \
  test/suites/test_http.cpp \
  test/suites/test_sse.cpp \
  test/suites/test_chunked.cpp \
  test/suites/test_httpclient.cpp \
  test/suites/test_keepalive.cpp \
  test/suites/test_retry.cpp \
  test/suites/test_images.cpp \
  test/suites/test_usage_metrics.cpp \
  test/suites/test_secretstore.cpp \
  test/suites/test_eepromcodec.cpp \
  test/suites/test_conversation.cpp \
  test/suites/test_structured.cpp \
  test/suites/test_schema_nested.cpp \
  test/suites/test_providers.cpp \
  test/suites/test_llmclient.cpp \
  test/suites/test_tools.cpp \
  test/suites/test_provider_tools.cpp \
  test/suites/test_agentloop.cpp \
  test/suites/test_urlguard.cpp \
  test/suites/test_edgestore.cpp \
  test/suites/test_mcp_server.cpp \
  test/suites/test_mcp_resources.cpp \
  test/suites/test_provisioning.cpp

BIN := test/bin/edgellm_tests

.PHONY: test clean
test: $(BIN)
	@./$(BIN)

$(BIN): $(PURE_SRC) $(TEST_SRC)
	@mkdir -p test/bin
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(PURE_SRC) $(TEST_SRC) -o $(BIN)

clean:
	rm -rf test/bin
