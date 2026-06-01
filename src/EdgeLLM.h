// EdgeLLM — umbrella header.
//
// Include <EdgeLLM.h> to pull in the public API. Two independent features build
// on a shared transport/HAL foundation:
//   * LLM client  — talk to Claude, OpenAI, Gemini, Ollama (added in Phase 2+).
//   * MCP server  — expose device data/functions to LLM hosts (added in Phase 4).
//
// Phase 1 ships the foundation: capabilities, logging, secrets, secure HTTP
// transport (request building, SSE + chunked parsing), and the per-board HAL.
#ifndef EDGELLM_H
#define EDGELLM_H

// --- Core (Arduino-independent) ---
#include "core/Version.h"
#include "core/Errors.h"
#include "core/Result.h"
#include "core/Logger.h"
#include "core/Capabilities.h"

// --- Transport (Arduino-independent) ---
#include "transport/IConnection.h"
#include "transport/HttpTypes.h"
#include "transport/HttpRequestBuilder.h"
#include "transport/SseParser.h"
#include "transport/ChunkedDecoder.h"
#include "transport/CACertStore.h"
#include "transport/HttpClient.h"

// --- HAL: secrets + time (interfaces + portable impls) ---
#include "hal/Platform.h"
#include "hal/ISecretStore.h"
#include "hal/MemorySecretStore.h"
#include "hal/ITimeSource.h"

// --- HAL: on-device only (guarded internally) ---
#include "hal/PreferencesSecretStore.h"
#include "hal/EepromCodec.h"
#include "hal/EepromSecretStore.h"
#include "hal/ArduinoConnection.h"
#include "hal/NtpTimeSource.h"
#include "hal/SerialLogSink.h"
#include "hal/ArduinoClock.h"

// --- Shared tools (Phase 3): registry used by the agent loop and, later, MCP ---
#include "tools/ToolTypes.h"
#include "tools/ToolCallArgs.h"
#include "tools/SchemaUtil.h"
#include "tools/Tool.h"
#include "tools/ToolRegistry.h"
#include "tools/UrlGuard.h"

// --- Feature A: LLM client (structured output only). Requires ArduinoJson. ---
#include "llm/Message.h"
#include "llm/ChatTypes.h"
#include "llm/ResponseSchema.h"
#include "llm/StructuredResult.h"
#include "llm/Conversation.h"
#include "llm/Provider.h"
#include "llm/LLMClient.h"
#include "llm/providers/AnthropicProvider.h"
#include "llm/providers/OpenAIChatProvider.h"
#include "llm/providers/GeminiProvider.h"
#include "llm/providers/OllamaProvider.h"

// --- Feature B: MCP server (Phase 4). Requires ArduinoJson. ---
#include "mcp/EdgeStore.h"
#include "mcp/ResourceRegistry.h"
#include "mcp/PromptRegistry.h"
#include "mcp/McpServer.h"
#include "mcp/McpHttpServer.h"  // on-device HTTP glue (guarded internally)

// --- Optional provisioning (Phase 5), off by default ---
#include "provisioning/ProvisioningService.h"
#include "provisioning/SerialProvisioner.h"  // device-only (guarded internally)

#endif  // EDGELLM_H
