// Host tests for brain.h -- the reply pipeline itself.
//
// The table-integrity test (personality_test.cpp) checks the DATA. This checks
// the BEHAVIOUR, using inputs lifted from a real transcript that exposed four
// bugs:
//
//   1. "good girl" five times in a row produced the identical sentence five
//      times (one rule per keyword, no anti-repetition).
//   2. "are you real" was answered as if it were a reply to one of her own
//      questions ("always. Good. I'll approve."), because the pending-question
//      branch outranked the keyword matcher.
//   3. %last echoed stop words, producing "what. It always ends up being what."
//   4. "i have to go" never reached the farewell path -- the key list only had
//      literal "bye"-type words.
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

// Feed one message and return her reply.
static std::string say(const char* text, uint32_t advanceMs = 8000) {
  g_fakeMillis += advanceMs;
  VibeReply r = brainReply(text);
  return std::string(r.text.c_str());
}

// The chosen line WITHOUT the barb/question she may append. Anti-repetition must
// be asserted on this: the appended sentence makes full replies differ even when
// the core line is repeating, which is how a real bug hid from this very test.
static std::string sayCore(const char* text, uint32_t advanceMs = 8000) {
  g_fakeMillis += advanceMs;
  VibeReply r = brainReply(text);
  return std::string(r.coreText.c_str());
}

// Templates contain %tokens, so comparisons must be against the RENDERED form.
static VibeCtx testCtx() {
  VibeCtx c;
  c.name        = YOUR_NAME;
  c.gapSecs     = 0;
  c.uptimeSecs  = 0;
  c.msgsSession = 1;
  c.msgsEver    = 1;
  c.unplugs     = 0;
  c.lastWord    = B.lastWord;
  c.promise     = B.promiseTopic;
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

static bool contains(const std::string& hay, const char* needle) {
  return hay.find(needle) != std::string::npos;
}

// Farewell lines live in a different struct (ByeRule), so a second helper.
static bool isFarewellLine(const std::string& got) {
  VibeCtx ctx = testCtx();
  for (size_t i = 0; i < FAREWELL_RULE_COUNT; i++) {
    std::string t = render(FAREWELL_RULES[i].text, ctx).c_str();
    if (got.rfind(t, 0) == 0) return true;
  }
  return false;
}

int main() {
  srand(20261004);
  printf("\n== brain pipeline ==\n");

  // ---------------------------------------------------------------- BEFORE
  printf("\nanti-repetition (the 'good girl' x5 bug):\n");
  {
    resetBrain();
    std::vector<std::string> seen;
    for (int i = 0; i < 12; i++) seen.push_back(sayCore("good girl"));

    std::set<std::string> distinct(seen.begin(), seen.end());
    printf("    12 x \"good girl\" -> %zu distinct CORE replies\n", distinct.size());
    check(distinct.size() >= 6, "12 repeats of one keyword give >=6 distinct replies");

    int consecutiveDupes = 0;
    for (size_t i = 1; i < seen.size(); i++) if (seen[i] == seen[i - 1]) consecutiveDupes++;
    printf("    consecutive identical CORE replies: %d\n", consecutiveDupes);
    check(consecutiveDupes == 0, "never says the identical core line twice in a row");
  }

  printf("\nanti-repetition (the 'i love you' x8 bug):\n");
  {
    resetBrain();
    std::set<std::string> distinct;
    for (int i = 0; i < 10; i++) distinct.insert(sayCore("i love you"));
    printf("    10 x \"i love you\" -> %zu distinct replies\n", distinct.size());
    check(distinct.size() >= 6, "10 repeats of 'i love you' give >=6 distinct replies");
  }

  printf("\nheart-eyes routing (positive love -> MOOD_HEART):\n");
  {
    resetBrain();
    int heart = 0;
    for (int i = 0; i < 10; i++) {
      g_fakeMillis += 8000;
      VibeReply r = brainReply("i love you");
      if (r.mood == MOOD_HEART) heart++;
    }
    printf("    MOOD_HEART replies: %d/10\n", heart);
    check(heart >= 4, "most loving replies use the heart-eyes sprite");
  }

  // ----------------------------------------------------------- THE PRIORITY BUG
  printf("\npending-question must not hijack a clear keyword:\n");
  {
    resetBrain();
    say("hi");                                  // may or may not ask a question
    // force a pending question, then say something unmistakably META
    B.pendingQid = 3;                           // "what are you working on?"
    std::string got = say("are you real");
    printf("    after pendingQid=3, \"are you real\" -> \"%s\"\n", got.c_str());
    check(isOneOf(got, META_RULES, META_RULE_COUNT),
          "\"are you real\" answers the META rule, not her own question");

    B.pendingQid = 13;
    std::string got2 = say("on your body and eyes");
    printf("    after pendingQid=13, \"on your body and eyes\" -> \"%s\"\n", got2.c_str());
    check(!contains(got2, "It always ends up being"), "no stop-word echo from an answer rule");
  }

  printf("\nbut a genuine answer IS handled when nothing matches:\n");
  {
    resetBrain();
    B.pendingQid = 1;                            // "how long have you been awake?"
    g_fakeMillis += 8000;
    VibeReply r = brainReply("hours");
    std::string got = r.text.c_str();
    printf("    pendingQid=1, \"hours\" -> \"%s\"\n", got.c_str());
    bool fromAnswers = false;
    for (size_t i = 0; i < ANSWER_RULE_COUNT; i++) {
      if (got.rfind(ANSWER_RULES[i].text, 0) == 0 && ANSWER_RULES[i].qid == 1) fromAnswers = true;
    }
    check(fromAnswers, "an answer with no matching keyword gets a question-specific reply");
    // She may immediately ask something new -- that's fine. What must not happen
    // is question 1 staying pending as though it were never answered.
    check(B.pendingQid == 0 || r.askedQuestion,
          "the answered question is consumed (a new pending one was just asked)");
  }

  // ------------------------------------------------------------ THE STOP WORDS
  printf("\nstop-word filter for %%last:\n");
  {
    resetBrain();
    const char* inputs[] = {"what are you doing", "how are you", "i like you",
                            "that was good", "why is this", "you are very nice"};
    for (const char* in : inputs) {
      say(in);
      printf("    \"%s\" -> lastWord=\"%s\"\n", in, B.lastWord);
    }
    check(!isStopWord(B.lastWord) || B.lastWord[0] == '\0',
          "the echoed word is never a stop word");

    // and no reply should ever contain an unsubstituted template token
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
  printf("\nfarewell detection (the 'i have to go' bug):\n");
  {
    const char* farewells[] = {"i have to go", "gotta go", "brb", "gtg",
                               "i should go", "heading out", "ttyl", "bye"};
    for (const char* f : farewells) {
      resetBrain();
      std::string got = say(f);
      bool refused = isFarewellLine(got);
      printf("    \"%s\" -> \"%s\"\n", f, got.c_str());
      check(refused, "farewell is refused rather than keyword-matched");
    }
  }

  printf("\nfarewell escalation:\n");
  {
    resetBrain();
    std::vector<std::string> got;
    for (int i = 0; i < 5; i++) got.push_back(say("bye"));
    printf("    byeCount after 5 tries = %u\n", (unsigned)B.byeCount);
    for (auto& g : got) printf("      %s\n", g.c_str());
    check(B.byeCount == 5, "goodbyes are counted");
    check(got[0] != got[4], "her answer to the 5th goodbye differs from the 1st");
  }

  // ------------------------------------------------------------------ GRUDGE
  printf("\ngap + grudge + reconciliation:\n");
  {
    resetBrain();
    say("hello");
    std::string afterGap = say("hi", 45 * 60 * 1000);   // 45 minutes later
    printf("    after a 45 min gap: \"%s\"  (grudge=%d)\n", afterGap.c_str(), B.grudge);
    check(B.grudge > 0, "an absence raises the grudge");
    check(contains(afterGap, "minutes") || contains(afterGap, "hour") ||
          contains(afterGap, "kept") || contains(afterGap, "list"),
          "the gap is acknowledged in the reply");

    int before = B.grudge;
    say("i'm sorry");
    printf("    after an apology: grudge=%d (was %d)\n", B.grudge, before);
    check(B.grudge < before, "an apology actually reduces the grudge");

    // A high grudge must be fully recoverable -- but it should COST something,
    // so test that it clears within a handful of sincere messages rather than
    // assuming one apology is enough.
    B.grudge = 70;
    B.reliefArmed = true;
    const char* makeup[] = {"sorry", "i love you", "i'm sorry", "i love you",
                            "i'm sorry", "i love you", "sorry", "i love you"};
    int need = 0;
    bool cleared = false;
    for (int i = 0; i < 8 && !cleared; i++) {
      say(makeup[i]);
      need = i + 1;
      if (B.grudge <= 5) cleared = true;
    }
    printf("    from grudge 70: cleared after %d messages (grudge=%d)\n", need, B.grudge);
    check(cleared, "a high grudge is fully recoverable by making up");
    check(need >= 3, "but not for free -- it takes more than one apology");
  }

  printf("\n%s (%d failure%s)\n\n",
         failures == 0 ? "ALL BRAIN TESTS PASSED" : "FAILURES",
         failures, failures == 1 ? "" : "s");
  return failures == 0 ? 0 : 1;
}
