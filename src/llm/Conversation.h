// EdgeLLM — optional managed conversation history.
// Arduino-independent. Holds multi-turn history with a byte budget; when the
// budget is exceeded the oldest turns are trimmed (the system prompt is kept
// separately and never trimmed). Callers who want stateless single-shot chat
// simply don't use this class — that's the hybrid model chosen in discovery.
//
// The budget is a coarse RAM guard, not an exact token count: it bounds how much
// history we keep so a long-running chat can't grow without limit on a device
// with tens of kilobytes of heap.
#ifndef EDGELLM_LLM_CONVERSATION_H
#define EDGELLM_LLM_CONVERSATION_H

#include <string>

#include "Message.h"

namespace edge {

class Conversation {
 public:
  explicit Conversation(size_t budgetBytes = 4096) : budget_(budgetBytes) {}

  void setBudget(size_t bytes) {
    budget_ = bytes;
    trim();
  }
  size_t budget() const { return budget_; }

  void setSystem(const std::string& prompt) { system_ = prompt; }
  const std::string& system() const { return system_; }

  void addUser(const std::string& text) { add(Message::user(text)); }
  void addAssistant(const std::string& text) { add(Message::assistant(text)); }
  void add(const Message& msg);

  const MessageList& messages() const { return messages_; }
  void clear() { messages_.clear(); }
  bool empty() const { return messages_.empty(); }
  size_t size() const { return messages_.size(); }

  // Approximate retained size in bytes (content + small per-message overhead).
  size_t approxBytes() const;

 private:
  void trim();

  std::string system_;
  MessageList messages_;
  size_t budget_;
};

}  // namespace edge

#endif  // EDGELLM_LLM_CONVERSATION_H
