// Host-side test for the response data + sprite headers.
//
//   cd test
//   g++ -std=c++17 -DVIBE_HOST_TEST -I. -I.. -o personality_test personality_test.cpp && ./personality_test
//
// Compiles the REAL responses.h and REAL generated sprites.h, so a syntax error
// in either, a duplicate keyword, or a broken mood mapping fails here instead
// of after a 60-second arduino-cli build.

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <map>
#include <set>
#include <vector>
#include <string>

#include "../personality.h"

static int failures = 0;

static void check(bool ok, const char* what) {
  printf("  %s %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) failures++;
}

// Ask the engine, ignoring the random pick among equally-valid matches.
static std::vector<std::pair<Mood, std::string>> answers(const char* input) {
  std::set<std::string> seen;
  std::vector<std::pair<Mood, std::string>> out;
  for (int i = 0; i < 200; i++) {
    const char* line = nullptr;
    Mood m = respondWith(input, &line);
    std::string key = std::string(moodName(m)) + "|" + line;
    if (seen.insert(key).second) out.push_back({m, line});
  }
  return out;
}

static bool everMood(const char* input, Mood want) {
  for (auto& p : answers(input)) if (p.first == want) return true;
  return false;
}

static bool lineIsFallback(const std::string& line) {
  for (size_t i = 0; i < FALLBACK_RULE_COUNT; i++)
    if (line == FALLBACK_RULES[i].text) return true;
  return false;
}

// Did the engine answer from a specific rule set (not the fallback bank)?
static bool answersFrom(const char* input, const ResponseRule* set, size_t n) {
  auto a = answers(input);
  for (auto& p : a) {
    bool fromSet = false;
    for (size_t i = 0; i < n; i++) if (p.second == set[i].text) fromSet = true;
    if (!fromSet && !lineIsFallback(p.second)) return false;
  }
  return true;
}

int main() {
  srand(1234);

  printf("\n== personality engine ==\n");

  // --- sprite header sanity (real generated sprites.h) --------------------
  printf("\nmood reachability:\n");
  bool allMoods = true;
  for (int m = 0; m < MOOD_COUNT; m++) {
    if (getSprite((Mood)m) == nullptr) allMoods = false;
  }
  check(allMoods, "getSprite() returns a non-null array for every mood");
  {
    std::set<std::string> names;
    bool namesOk = true;
    for (int m = 0; m < MOOD_COUNT; m++) {
      const char* n = moodName((Mood)m);
      if (n == nullptr || n[0] == '\0') namesOk = false;
      else names.insert(n);
    }
    printf("  MOOD_COUNT = %d\n", (int)MOOD_COUNT);
    check(MOOD_COUNT >= 8, "at least 8 moods defined");
    check(namesOk && names.size() == (size_t)MOOD_COUNT,
          "every mood has a distinct, non-empty name");
  }
  check(std::string(moodName(MOOD_NO_INTERNET)) == "NO_INTERNET",
        "moodName(NO_INTERNET) == \"NO_INTERNET\"");

  // --- category routing ---------------------------------------------------
  printf("\ncategory routing:\n");
  check(everMood("hi", MOOD_HAPPY),                 "\"hi\" -> HAPPY");
  check(everMood("good morning", MOOD_HAPPY),       "\"good morning\" -> HAPPY");
  check(everMood("who are you", MOOD_PROUD),        "\"who are you\" -> PROUD (phrase)");
  check(everMood("what are you", MOOD_PROUD),       "\"what are you\" -> PROUD (phrase)");
  check(everMood("help", MOOD_CONFIDENT) || everMood("help", MOOD_PROUD),
        "\"help\" -> CONFIDENT/PROUD");
  check(everMood("time", MOOD_CONFIDENT) || everMood("time", MOOD_HAPPY),
        "\"time\" -> CONFIDENT/HAPPY");
  check(everMood("status", MOOD_PROUD) || everMood("status", MOOD_CONFIDENT),
        "\"status\" -> PROUD/CONFIDENT");
  check(everMood("coffee", MOOD_HAPPY),            "\"coffee\" -> HAPPY");
  check(everMood("work", MOOD_CONFIDENT),          "\"work\" -> CONFIDENT");
  check(everMood("tired", MOOD_BLANKET),           "\"tired\" -> BLANKET");
  check(everMood("lol", MOOD_HAPPY),               "\"lol\" -> HAPPY");

  // --- whole-word matching: substrings must NOT fire ----------------------
  printf("\nwhole-word matching (substring traps):\n");
  {
    auto a = answers("there is something here");
    bool allFb = true;
    for (auto& p : a) if (!lineIsFallback(p.second)) allFb = false;
    check(allFb, "\"there/here\" does NOT trigger any keyword rule (falls through)");
  }
  {
    auto a = answers("qqq xyzzy plugh frobnicate wibble");
    bool allFb = true;
    for (auto& p : a) if (!lineIsFallback(p.second)) allFb = false;
    check(allFb, "unmatched input -> fallback one-liners");
  }
  check(answersFrom("what time", INFO_RULES, INFO_RULE_COUNT),
        "\"what time\" is answered from the INFO table (phrase, not stray)");

  // --- table integrity ----------------------------------------------------
  // NOTE: duplicate keywords are INTENTIONAL -- several rules sharing a keyword
  // are the variants that keep a repeated phrase from repeating its answer.
  // What must still never happen is a duplicated *line*.
  printf("\ntable integrity:\n");
  std::map<std::string, int> keywords;
  std::set<std::string> lines;
  struct { const ResponseRule* r; size_t n; const char* name; } sets[] = {
    { GREETING_RULES,     GREETING_RULE_COUNT,     "GREETING"     },
    { INFO_RULES,         INFO_RULE_COUNT,         "INFO"         },
    { META_RULES,         META_RULE_COUNT,         "META"         },
    { TOPIC_RULES,        TOPIC_RULE_COUNT,        "TOPIC"        },
    { CONVERSATION_RULES, CONVERSATION_RULE_COUNT, "CONVERSATION" },
  };
  bool dupLine = false, missingText = false;
  size_t totalRules = 0;
  for (auto& s : sets) {
    for (size_t i = 0; i < s.n; i++) {
      const ResponseRule& r = s.r[i];
      totalRules++;
      if (r.keyword == nullptr) { printf("  NULL keyword in %s\n", s.name); missingText = true; continue; }
      keywords[r.keyword]++;
      if (!lines.insert(r.text).second) {
        printf("  duplicate line: \"%s\"\n", r.text);
        dupLine = true;
      }
      if (r.text[0] == '\0') missingText = true;
    }
  }
  check(!dupLine,    "no duplicate response lines anywhere");
  check(!missingText, "every rule has a keyword and non-empty text");

  size_t maxVariants = 0; std::string maxKw;
  for (auto& kv : keywords) {
    if ((size_t)kv.second > maxVariants) { maxVariants = kv.second; maxKw = kv.first; }
  }
  printf("  distinct keywords        : %zu\n", keywords.size());
  printf("  most variants for one    : %zu  (\"%s\")\n", maxVariants, maxKw.c_str());
  check(maxVariants >= 3, "some keyword has >=3 variants (anti-repeat needs variety)");

  // --- question/answer integrity ------------------------------------------
  // A question with no answer is a dead end: it asks, you reply, nothing lands.
  // A dangling qid is worse -- it can never be reached at all.
  printf("\nquestion/answer bank:\n");
  int orphanAnswers = 0, unansweredQuestions = 0;
  for (size_t i = 0; i < ANSWER_RULE_COUNT; i++) {
    bool found = false;
    for (size_t q = 0; q < QUESTION_DEF_COUNT; q++) {
      if (QUESTION_DEFS[q].id == ANSWER_RULES[i].qid) { found = true; break; }
    }
    if (!found) {
      printf("  answer rule with unknown qid %u: \"%s\"\n", ANSWER_RULES[i].qid, ANSWER_RULES[i].text);
      orphanAnswers++;
    }
  }
  for (size_t q = 0; q < QUESTION_DEF_COUNT; q++) {
    int n = 0;
    for (size_t i = 0; i < ANSWER_RULE_COUNT; i++) if (ANSWER_RULES[i].qid == QUESTION_DEFS[q].id) n++;
    if (n == 0) {
      printf("  question %u has no answers:\n    \"%s\"\n", QUESTION_DEFS[q].id, QUESTION_DEFS[q].text);
      unansweredQuestions++;
    }
  }
  printf("  questions: %zu   answer rules: %zu\n", (size_t)QUESTION_DEF_COUNT, (size_t)ANSWER_RULE_COUNT);
  check(orphanAnswers == 0,     "no answer rule references a question that doesn't exist");
  check(unansweredQuestions == 0, "every question the bot can ask has at least one answer");
  check(QUESTION_DEF_COUNT >= 15, "a decent question bank (>=15 questions it can ask)");

  // --- distinct line count ------------------------------------------------
  size_t totalFallbacks = 0;
  std::set<std::string> idleLines;
  for (size_t i = 0; i < FALLBACK_RULE_COUNT; i++) {
    totalFallbacks++;
    if (!lines.insert(FALLBACK_RULES[i].text).second) {
      printf("  duplicate fallback line: \"%s\"\n", FALLBACK_RULES[i].text);
      dupLine = true;
    }
  }
  // context tables (not plain ResponseRules) -- count their lines too.
  for (size_t i = 0; i < IDLE_THOUGHT_COUNT; i++) {
    idleLines.insert(IDLE_THOUGHTS[i].text);
    lines.insert(IDLE_THOUGHTS[i].text);
  }
  for (size_t i = 0; i < GAP_RULE_COUNT; i++)      lines.insert(GAP_RULES[i].text);
  for (size_t i = 0; i < FAREWELL_RULE_COUNT; i++) lines.insert(FAREWELL_RULES[i].text);
  for (size_t i = 0; i < UNPLUG_RULE_COUNT; i++)   lines.insert(UNPLUG_RULES[i].text);
  for (size_t i = 0; i < IDLE_TIER_COUNT; i++)     lines.insert(IDLE_TIERS[i].text);
  for (size_t i = 0; i < ANSWER_ACK_COUNT; i++)    lines.insert(ANSWER_ACKS[i]);
  for (size_t i = 0; i < REPEAT_NOTICE_COUNT; i++) lines.insert(REPEAT_NOTICES[i]);
  for (size_t i = 0; i < TIME_RULE_COUNT; i++)     lines.insert(TIME_RULES[i].text);
  for (size_t i = 0; i < QUESTION_DEF_COUNT; i++)  lines.insert(QUESTION_DEFS[i].text);
  for (size_t i = 0; i < ANSWER_RULE_COUNT; i++)   lines.insert(ANSWER_RULES[i].text);

  size_t distinct = lines.size();
  printf("\n  keyword rules            : %zu\n", totalRules);
  printf("  fallback one-liners      : %zu\n", totalFallbacks);
  printf("  DISTINCT RESPONSE LINES  : %zu\n", distinct);
  printf("  idle thoughts            : %zu\n", idleLines.size());

  check(distinct >= 100, "at least 100 distinct canned responses");
  check(IDLE_THOUGHT_COUNT >= 8, "at least 8 autonomous idle thoughts");
  check(!dupLine, "all response lines unique across rules + fallbacks");

  // moods referenced by the tables (informational)
  std::set<int> moodsUsed;
  for (auto& s : sets) for (size_t i = 0; i < s.n; i++) moodsUsed.insert((int)s.r[i].mood);
  for (size_t i = 0; i < FALLBACK_RULE_COUNT; i++) moodsUsed.insert((int)FALLBACK_RULES[i].mood);
  for (size_t i = 0; i < IDLE_THOUGHT_COUNT; i++) moodsUsed.insert((int)IDLE_THOUGHTS[i].mood);
  for (size_t i = 0; i < GAP_RULE_COUNT; i++)      moodsUsed.insert((int)GAP_RULES[i].mood);
  for (size_t i = 0; i < FAREWELL_RULE_COUNT; i++) moodsUsed.insert((int)FAREWELL_RULES[i].mood);
  for (size_t i = 0; i < UNPLUG_RULE_COUNT; i++)   moodsUsed.insert((int)UNPLUG_RULES[i].mood);
  for (size_t i = 0; i < IDLE_TIER_COUNT; i++)     moodsUsed.insert((int)IDLE_TIERS[i].mood);
  for (size_t i = 0; i < TIME_RULE_COUNT; i++)     moodsUsed.insert((int)TIME_RULES[i].mood);
  for (size_t i = 0; i < QUESTION_DEF_COUNT; i++)  moodsUsed.insert((int)QUESTION_DEFS[i].mood);
  for (size_t i = 0; i < ANSWER_RULE_COUNT; i++)   moodsUsed.insert((int)ANSWER_RULES[i].mood);
  printf("  moods used by tables     : %zu / %d\n", moodsUsed.size(), (int)MOOD_COUNT);
  check(moodsUsed.size() >= 5, "the data exercises at least 5 moods");

  printf("\n%s (%d failure%s)\n\n", failures == 0 ? "ALL TESTS PASSED" : "FAILURES",
         failures, failures == 1 ? "" : "s");
  return failures == 0 ? 0 : 1;
}
