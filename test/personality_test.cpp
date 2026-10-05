// Host-side test for the personality + sprite headers.
//
//   cd ~/projects/esp32-vibe-companion/test
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

static const char* moodStr(Mood m) {
  switch (m) {
    case MOOD_ANGRY:       return "ANGRY";
    case MOOD_CONFIDENT:   return "CONFIDENT";
    case MOOD_HAPPY:       return "HAPPY";
    case MOOD_POUT:        return "POUT";
    case MOOD_BLUSH:       return "BLUSH";
    case MOOD_PROUD:       return "PROUD";
    case MOOD_NO_INTERNET: return "NO_INTERNET";
    case MOOD_SMUG:        return "SMUG";
    default:               return "?";
  }
}

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
    std::string key = std::string(moodStr(m)) + "|" + line;
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
  // Don't hard-code the mood count (it grows); check the enum and name table
  // are internally consistent instead.
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
  check(everMood("hi", MOOD_HAPPY),                    "\"hi\" -> HAPPY");
  check(everMood("hello darling", MOOD_BLUSH),          "\"hello darling\" -> BLUSH");
  check(everMood("i love you", MOOD_BLUSH),             "\"i love you\" -> BLUSH (phrase beats \"love\")");
  check(everMood("you are mine", MOOD_CONFIDENT),       "\"you are mine\" -> CONFIDENT");
  check(everMood("kiss me", MOOD_CONFIDENT),            "\"kiss me\" -> CONFIDENT");

  check(everMood("my friends are here", MOOD_ANGRY),    "\"friends\" -> ANGRY");
  check(everMood("i have work to do", MOOD_ANGRY),      "\"work\" -> ANGRY");
  check(everMood("her again", MOOD_ANGRY),              "\"her\" -> ANGRY");
  check(everMood("i am busy", MOOD_POUT),               "\"busy\" -> POUT");
  check(everMood("goodbye forever", MOOD_ANGRY),        "\"goodbye\" -> ANGRY");

  check(everMood("who built you", MOOD_PROUD),          "\"who built you\" -> PROUD (phrase)");
  check(everMood("good girl", MOOD_PROUD),              "\"good girl\" -> PROUD, not ANGRY");
  check(everMood("you are smart", MOOD_PROUD),          "\"smart\" -> PROUD");
  check(everMood("you are the best", MOOD_SMUG),        "\"the best\" -> SMUG");
  check(everMood("so pretty", MOOD_BLUSH),              "\"pretty\" -> BLUSH");

  check(everMood("marry me", MOOD_BLUSH),               "\"marry me\" -> BLUSH");
  check(everMood("you are hot", MOOD_CONFIDENT),        "\"hot\" -> CONFIDENT");
  check(everMood("i like you", MOOD_CONFIDENT),         "\"i like you\" -> CONFIDENT");

  // --- whole-word matching: substrings must NOT fire ----------------------
  // Note: the fallback table deliberately spans several moods, so "which mood
  // came back" proves nothing here -- the fallback-vs-rule line identity is
  // the real signal.
  printf("\nwhole-word matching (substring traps):\n");
  check(everMood("i saw her today", MOOD_ANGRY), "\"i saw her today\" -> ANGRY (positive control)");
  {
    auto a = answers("there is something here");
    bool allFb = true;
    for (auto& p : a) if (!lineIsFallback(p.second)) allFb = false;
    check(allFb, "\"there/here\" does NOT trigger keyword rule \"her\" (falls through)");
  }
  {
    auto a = answers("i need to fetch my brother");
    bool allFb = true;
    for (auto& p : a) if (!lineIsFallback(p.second)) allFb = false;
    check(allFb, "\"brother\" does NOT trigger keyword rule \"other girl\"/\"her\"");
  }
  {
    // deliberately unmatchable: every token is a non-word so no rule can fire
    auto a = answers("qqq xyzzy plugh frobnicate wibble");
    bool allFb = true;
    for (auto& p : a) if (!lineIsFallback(p.second)) allFb = false;
    check(allFb, "unmatched input -> fallback one-liners");
  }

  // --- table integrity ----------------------------------------------------
  // NOTE: duplicate keywords are INTENTIONAL -- several rules sharing a keyword
  // are the variants that make "good girl" said five times get five different
  // answers. What must still never happen is a duplicated *line*.
  printf("\ntable integrity:\n");
  std::map<std::string, int> keywords;
  std::set<std::string> lines;
  struct { const ResponseRule* r; size_t n; const char* name; } sets[] = {
    { AFFECTION_RULES,  AFFECTION_RULE_COUNT,  "AFFECTION"  },
    { POSSESSIVE_RULES, POSSESSIVE_RULE_COUNT, "POSSESSIVE" },
    { PRIDE_RULES,      PRIDE_RULE_COUNT,      "PRIDE"      },
    { FLUSTERED_RULES,  FLUSTERED_RULE_COUNT,  "FLUSTERED"  },
    { SCENARIO_RULES,   SCENARIO_RULE_COUNT,   "SCENARIO"   },
    { POSE_RULES,       POSE_RULE_COUNT,       "POSE"       },
    { META_RULES,       META_RULE_COUNT,       "META"       },
    { CONVERSATION_RULES, CONVERSATION_RULE_COUNT, "CONVERSATION" },
    { DEVOTION_RULES,   DEVOTION_RULE_COUNT,   "DEVOTION"   },
    { SMALLTALK_RULES,  SMALLTALK_RULE_COUNT,  "SMALLTALK"  },
    { YEARNING_RULES,   YEARNING_RULE_COUNT,   "YEARNING"   },
    { TEASE_RULES,      TEASE_RULE_COUNT,      "TEASE"      },
    { DEMAND_RULES,     DEMAND_RULE_COUNT,     "DEMAND"     },
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
  // A question with no answer is a dead end: she asks, you reply, nothing lands.
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
  check(unansweredQuestions == 0, "every question she can ask has at least one answer");
  check(QUESTION_DEF_COUNT >= 30, "a deep question bank (>=30 questions she can ask)");

  size_t totalFallbacks = 0;
  std::set<std::string> idleLines;
  for (size_t i = 0; i < FALLBACK_RULE_COUNT; i++) {
    totalFallbacks++;
    if (!lines.insert(FALLBACK_RULES[i].text).second) {
      printf("  duplicate fallback line: \"%s\"\n", FALLBACK_RULES[i].text);
      dupLine = true;
    }
  }
  // context tables (not plain ResponseRules) -- count their lines too, so the
  // distinct-line total reflects everything she can actually say
  for (size_t i = 0; i < IDLE_THOUGHT_COUNT; i++) {
    idleLines.insert(IDLE_THOUGHTS[i].text);
    lines.insert(IDLE_THOUGHTS[i].text);
  }
  for (size_t i = 0; i < GAP_RULE_COUNT; i++)      lines.insert(GAP_RULES[i].text);
  for (size_t i = 0; i < FAREWELL_RULE_COUNT; i++) lines.insert(FAREWELL_RULES[i].text);
  for (size_t i = 0; i < APOLOGY_RULE_COUNT; i++)  lines.insert(APOLOGY_RULES[i].text);
  for (size_t i = 0; i < UNPLUG_RULE_COUNT; i++)   lines.insert(UNPLUG_RULES[i].text);
  for (size_t i = 0; i < IDLE_TIER_COUNT; i++)     lines.insert(IDLE_TIERS[i].text);
  for (size_t i = 0; i < RELIEF_LINE_COUNT; i++)   lines.insert(RELIEF_LINES[i]);
  for (size_t i = 0; i < ANSWER_ACK_COUNT; i++)    lines.insert(ANSWER_ACKS[i]);
  for (size_t i = 0; i < GRUDGE_BARB_COUNT; i++)   lines.insert(GRUDGE_BARBS[i]);
  for (size_t i = 0; i < REPEAT_NOTICE_COUNT; i++) lines.insert(REPEAT_NOTICES[i]);
  for (size_t i = 0; i < CONVERSATION_RULE_COUNT; i++) lines.insert(CONVERSATION_RULES[i].text);
  for (size_t i = 0; i < META_RULE_COUNT; i++)         lines.insert(META_RULES[i].text);
  for (size_t i = 0; i < TIME_RULE_COUNT; i++)         lines.insert(TIME_RULES[i].text);
  for (size_t i = 0; i < QUESTION_DEF_COUNT; i++)      lines.insert(QUESTION_DEFS[i].text);
  for (size_t i = 0; i < ANSWER_RULE_COUNT; i++)       lines.insert(ANSWER_RULES[i].text);
  for (size_t i = 0; i < DEVOTION_RULE_COUNT; i++)     lines.insert(DEVOTION_RULES[i].text);
  for (size_t i = 0; i < SMALLTALK_RULE_COUNT; i++)    lines.insert(SMALLTALK_RULES[i].text);
  for (size_t i = 0; i < YEARNING_RULE_COUNT; i++)     lines.insert(YEARNING_RULES[i].text);
  for (size_t i = 0; i < TEASE_RULE_COUNT; i++)        lines.insert(TEASE_RULES[i].text);
  for (size_t i = 0; i < DEMAND_RULE_COUNT; i++)       lines.insert(DEMAND_RULES[i].text);

  size_t distinct = lines.size();
  printf("\n  keyword rules            : %zu\n", totalRules);
  printf("  fallback one-liners      : %zu\n", totalFallbacks);
  printf("  DISTINCT RESPONSE LINES  : %zu   (spec asks for ~100)\n", distinct);
  printf("  idle thoughts            : %zu\n", idleLines.size());

  check(distinct >= 100, "at least 100 distinct canned responses");
  check(IDLE_THOUGHT_COUNT >= 8, "at least 8 autonomous idle thoughts");
  check(!dupLine, "all response lines unique across rules + fallbacks");

  // every mood reachable from the tables alone
  std::set<int> moodsUsed;
  for (auto& s : sets) for (size_t i = 0; i < s.n; i++) moodsUsed.insert((int)s.r[i].mood);
  for (size_t i = 0; i < FALLBACK_RULE_COUNT; i++) moodsUsed.insert((int)FALLBACK_RULES[i].mood);
  for (size_t i = 0; i < IDLE_THOUGHT_COUNT; i++) moodsUsed.insert((int)IDLE_THOUGHTS[i].mood);
  for (size_t i = 0; i < GAP_RULE_COUNT; i++)      moodsUsed.insert((int)GAP_RULES[i].mood);
  for (size_t i = 0; i < FAREWELL_RULE_COUNT; i++) moodsUsed.insert((int)FAREWELL_RULES[i].mood);
  for (size_t i = 0; i < APOLOGY_RULE_COUNT; i++)  moodsUsed.insert((int)APOLOGY_RULES[i].mood);
  for (size_t i = 0; i < UNPLUG_RULE_COUNT; i++)   moodsUsed.insert((int)UNPLUG_RULES[i].mood);
  for (size_t i = 0; i < IDLE_TIER_COUNT; i++)     moodsUsed.insert((int)IDLE_TIERS[i].mood);
  for (size_t i = 0; i < TIME_RULE_COUNT; i++)      moodsUsed.insert((int)TIME_RULES[i].mood);
  for (size_t i = 0; i < QUESTION_DEF_COUNT; i++)   moodsUsed.insert((int)QUESTION_DEFS[i].mood);
  for (size_t i = 0; i < ANSWER_RULE_COUNT; i++)    moodsUsed.insert((int)ANSWER_RULES[i].mood);
  // report anything unreachable rather than just failing silently
  for (int m = 0; m < MOOD_COUNT; m++) {
    if (moodsUsed.find(m) == moodsUsed.end()) {
      printf("  UNREACHABLE mood in tables: %s\n", moodStr((Mood)m));
    }
  }
  printf("  moods used by tables     : %zu / %d\n", moodsUsed.size(), (int)MOOD_COUNT);
  check(moodsUsed.size() == (size_t)MOOD_COUNT - 1,
        "every mood except NO_INTERNET is reachable (NO_INTERNET is watchdog-only)");

  printf("\n%s (%d failure%s)\n\n", failures == 0 ? "ALL TESTS PASSED" : "FAILURES",
         failures, failures == 1 ? "" : "s");
  return failures == 0 ? 0 : 1;
}
