#include "Conversation.h"

namespace edge {

namespace {
// Per-message bookkeeping overhead added to the content length when estimating
// retained size (role tag, JSON envelope, etc.).
constexpr size_t kPerMessageOverhead = 16;
}  // namespace

size_t Conversation::approxBytes() const {
  size_t total = 0;
  for (const auto& m : messages_)
    total += m.content.size() + kPerMessageOverhead;
  return total;
}

void Conversation::add(const Message& msg) {
  messages_.push_back(msg);
  trim();
}

void Conversation::trim() {
  // Drop oldest turns until within budget, but always keep at least the most
  // recent message so a single oversized turn still gets sent.
  while (messages_.size() > 1 && approxBytes() > budget_) {
    messages_.erase(messages_.begin());
  }
}

}  // namespace edge
