// EdgeLLM — shared JSON-Schema writer + validator over a field list.
// Arduino-independent (uses ArduinoJson). One implementation backs both tool
// input schemas (Phase 3) and structured-output response schemas (Phase 6), so
// the schema dialect and validation rules stay identical across the library.
#ifndef EDGELLM_TOOLS_SCHEMAUTIL_H
#define EDGELLM_TOOLS_SCHEMAUTIL_H

#include <ArduinoJson.h>

#include <string>
#include <vector>

#include "../core/Result.h"
#include "ToolTypes.h"

namespace edge {

// Writes an object JSON Schema for `fields` into `out`:
// {"type":"object","properties":{...},"required":[...],"additionalProperties":false}
// Set `additionalPropertiesFalse` to false for dialects that reject the
// additionalProperties keyword (e.g. Gemini's OpenAPI-subset responseSchema).
void writeObjectSchema(const std::vector<ToolParam>& fields, JsonObject out,
                       bool additionalPropertiesFalse = true);

// True if every field — including nested object/array-item fields — is
// required. OpenAI's strict json_schema mode only accepts such schemas, so the
// provider downgrades to non-strict (with local validation still applied) when
// this returns false.
bool allFieldsRequired(const std::vector<ToolParam>& fields);

// Validates a JSON object string against `fields` (required present, types
// match, string enums respected — recursively through nested objects and array
// items). Returns SchemaValidationFailed on any mismatch, or on malformed JSON.
Status validateObject(const std::vector<ToolParam>& fields, const std::string& json);

}  // namespace edge

#endif  // EDGELLM_TOOLS_SCHEMAUTIL_H
