// Tests for the optional managed Conversation (history + byte-budget trimming).
#include "../../src/llm/Conversation.h"
#include "../framework/edge_test.h"

using namespace edge;

TEST(conversation_accumulates_turns) {
  Conversation c;
  c.setSystem("you are terse");
  c.addUser("hello");
  c.addAssistant("hi");
  c.addUser("how are you?");
  CHECK_EQ(c.size(), static_cast<size_t>(3));
  CHECK_STR_EQ(c.system(), "you are terse");
  CHECK_EQ(c.messages().front().role, Role::User);
  CHECK_STR_EQ(c.messages().front().content, "hello");
}

TEST(conversation_trims_oldest_over_budget) {
  Conversation c(60);  // tiny budget forces trimming
  c.addUser("aaaaaaaaaaaaaaaaaaaa");        // 20 + overhead
  c.addAssistant("bbbbbbbbbbbbbbbbbbbb");    // 20 + overhead
  c.addUser("cccccccccccccccccccc");        // 20 + overhead -> over budget
  // Oldest message(s) dropped; most recent retained.
  CHECK(c.approxBytes() <= 60 || c.size() == 1);
  CHECK_STR_EQ(c.messages().back().content, "cccccccccccccccccccc");
  CHECK(c.size() < 3);
}

TEST(conversation_keeps_at_least_latest_message) {
  Conversation c(4);  // smaller than a single message
  c.addUser("this single message exceeds the whole budget on its own");
  CHECK_EQ(c.size(), static_cast<size_t>(1));  // never trimmed below 1
}

TEST(conversation_clear_resets_history_not_system) {
  Conversation c;
  c.setSystem("sys");
  c.addUser("x");
  c.clear();
  CHECK(c.empty());
  CHECK_STR_EQ(c.system(), "sys");  // system survives clear()
}
