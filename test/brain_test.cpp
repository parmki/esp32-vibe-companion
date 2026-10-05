// Host tests for brain.h -- the reply pipeline itself.
//
// The table-integrity test (personality_test.cpp) checks the DATA. This checks
// the BEHAVIOUR:
//   - a repeated keyword does not produce the identical line every time
//   - a clear keyword is not hijacked by a pending question
//   - a genuine answer to a pending question IS handled when nothing matches
//   - %last never echoes a stop word and no reply leaks an unrendered %token
//   - farewells are recognised and acknowledged from the farewell table
//   - a power cut is reported
//   - a long absence is acknowledged
//
//   cd test && g++ -std=c++17 -DVIBE_HOST_TEST -I. -I.. -o brain_test \
//       brain_test.cpp && ./brain_test

#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <vector>

#include "../brain.h"

unsigned long g_fakeMillis = 1000;

static int failures = 0;

static void check(bool ok, const char* what) {
  printf("  %s %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) failures++;
}

static void resetBrain() {
  brainPrefs.clear();
  memset(&B, 0, sizeof(B));
  brainBegin();
  B.lastMessageMillis = 0;
  B.sessionCount = 2;              // not a first boot, so greetings are normal
}

// Feed one message and return the reply text.
static std::string say(const char* text, uint32_t advanceMs = 8000) {
  g_fakeMillis += advanceMs;
  VibeReply r = brainReply(text);
  return std::string(r.text.c_str());
}

// The chosen line WITHOUT the question that may be appended. Anti-repetition
// must be asserted on this: the appended sentence makes full replies differ even
// when the core line is repeating.
static std::string sayCore(const char* text, uint32_t advanceMs = 8000) {
  g_fakeMillis += advanceMs;
  VibeReply r = brainReply(text);
  return std::string(r.coreText.c_str());
}

static VibeCtx testCtx() {
  VibeCtx c;
  c.name        = YOUR_NAME;
  c.gapSecs     = 0;
  c.uptimeSecs  = 0;
  c.msgsSession = 1;
  c.msgsEver    = 1;
  c.unplugs     = 0;
  c.lastWord    = B.lastWord;
  return c;
}

static bool isOneOf(const std::string& got, const ResponseRule* set, size_t n) {
  VibeCtx ctx = testCtx();
  for (size_t i = 0; i < n; i++) {
    std::string t = render(set[i].text, ctx).c_str();
    if (got.rfind(t, 0) == 0) return true;
  }
  return false;
}

// GapRule (used by both GAP_RULES and UNPLUG_RULES) carries the same shape.
// The template's %t/%u depend on state, so render it with the state we expect.
static bool isGapLine(const std::string& got, const GapRule* set, size_t n,
                      uint32_t gapSecs, uint32_t unplugs) {
  VibeCtx ctx = testCtx();
  ctx.gapSecs = gapSecs;
  ctx.unplugs = unplugs;
  for (size_t i = 0; i < n; i++) {
    if (gapSecs < (uint32_t)set[i].minSec || gapSecs >= (uint32_t)set[i].maxSec) continue;
    std::string t = render(set[i].text, ctx).c_str();
    if (got.rfind(t, 0) == 0) return true;
  }
  return false;
}

// Farewell lines live in a different struct (ByeRule).
static bool isFarewellLine(const std::string& got) {
  VibeCtx ctx = testCtx();
  for (size_t i = 0; i < FAREWELL_RULE_COUNT; i++) {
    std::string t = render(FAREWELL_RULES[i].text, ctx).c_str();
    if (got.rfind(t, 0) == 0) return true;
  }
  return false;
}

static bool isAnswerFor(const std::string& got, uint8_t qid) {
  VibeCtx ctx = testCtx();
  for (size_t i = 0; i < ANSWER_RULE_COUNT; i++) {
    if (ANSWER_RULES[i].qid != qid) continue;
    std::string t = render(ANSWER_RULES[i].text, ctx).c_str();
    if (got.rfind(t, 0) == 0) return true;
  }
  return false;
}

static bool contains(const std::string& hay, const char* needle) {
  return hay.find(needle) != std::string::npos;
}

int main() {
  srand(20261005);
  printf("\n== brain pipeline ==\n");

  // ---------------------------------------------------------------- BEFORE
  printf("\nanti-repetition (a repeated keyword):\n");
  {
    resetBrain();
    std::vector<std::string> seen;
    for (int i = 0; i < 12; i++) seen.push_back(sayCore("how are you"));

    std::set<std::string> distinct(seen.begin(), seen.end());
    printf("    12 x \"how are you\" -> %zu distinct CORE replies\n", distinct.size());
    check(distinct.size() >= 5, "12 repeats of one keyword give >=5 distinct replies");

    int consecutiveDupes = 0;
    for (size_t i = 1; i < seen.size(); i++) if (seen[i] == seen[i - 1]) consecutiveDupes++;
    printf("    consecutive identical CORE replies: %d\n", consecutiveDupes);
    check(consecutiveDupes == 0, "never says the identical core line twice in a row");
  }

  // ------------------------------------------------- THE PRIORITY BEHAVIOUR
  printf("\npending-question must not hijack a clear keyword:\n");
  {
    resetBrain();
    B.pendingQid = 3;                           // "do you want the time?"
    std::string got = say("what time");
    printf("    after pendingQid=3, \"what time\" -> \"%s\"\n", got.c_str());
    check(isOneOf(got, INFO_RULES, INFO_RULE_COUNT),
          "\"what time\" answers the INFO rule, not the pending question");
  }

  printf("\na genuine answer IS handled when nothing matches:\n");
  {
    resetBrain();
    B.pendingQid = 3;                           // "do you want the time?"
    std::string got = say("purple");
    printf("    pendingQid=3, \"purple\" -> \"%s\"\n", got.c_str());
    check(isAnswerFor(got, 3), "an answer with no matching keyword gets a question-specific reply");
    check(B.pendingQid == 0, "the answered question is consumed");
  }

  // ------------------------------------------------------------ THE TOKENS
  printf("\nstop-word filter for %%last:\n");
  {
    resetBrain();
    const char* inputs[] = {"what are you doing", "how are you", "what time",
                            "that was good", "why is this", "tell me about coffee"};
    for (const char* in : inputs) {
      say(in);
      printf("    \"%s\" -> lastWord=\"%s\"\n", in, B.lastWord);
    }
    check(!isStopWord(B.lastWord) || B.lastWord[0] == '\0',
          "the echoed word is never a stop word");

    resetBrain();
    bool anyToken = false;
    for (const char* in : inputs) {
      std::string got = say(in);
      if (contains(got, "%name") || contains(got, "%t ") || contains(got, "%clock"))
        anyToken = true;
    }
    check(!anyToken, "no reply leaks an unrendered %token");
  }

  // --------------------------------------------------------------- FAREWELLS
  printf("\nfarewell detection:\n");
  {
    const char* farewells[] = {"i have to go", "gotta go", "brb", "gtg",
                               "i should go", "heading out", "ttyl", "bye"};
    for (const char* f : farewells) {
      resetBrain();
      std::string got = say(f);
      printf("    \"%s\" -> \"%s\"\n", f, got.c_str());
      check(isFarewellLine(got), "farewell is acknowledged from the farewell table");
    }
  }

  // ------------------------------------------------------------- POWER CUT
  printf("\npower-cut notice:\n");
  {
    resetBrain();
    B.unplugPending = true;
    B.unplugDeadSecs = 300;
    std::string got = say("hello");
    printf("    after a 5 min outage -> \"%s\"\n", got.c_str());
    check(isGapLine(got, UNPLUG_RULES, UNPLUG_RULE_COUNT, 300, 1),
          "a power cut is reported from the unplug table");
  }

  // ----------------------------------------------------------- LONG ABSENCE
  printf("\nlong-absence acknowledgement:\n");
  {
    resetBrain();
    say("hello");
    std::string afterGap = say("hi", 45 * 60 * 1000);   // 45 minutes later
    printf("    after a 45 min gap -> \"%s\"\n", afterGap.c_str());
    check(isGapLine(afterGap, GAP_RULES, GAP_RULE_COUNT, 45 * 60, 0),
          "the gap is acknowledged from the gap table");
  }

  printf("\n%s (%d failure%s)\n\n",
         failures == 0 ? "ALL BRAIN TESTS PASSED" : "FAILURES",
         failures, failures == 1 ? "" : "s");
  return failures == 0 ? 0 : 1;
}
