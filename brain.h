// brain.h -- the desk bot's memory and reply pipeline.
//
// Everything that makes the bot feel like it keeps track lives here: how long
// the desk was idle, how many messages have been handled, whether power was cut,
// and which question it last asked.
//
// State is persisted in NVS (the huge_app partition scheme includes a 20 KB nvs
// partition), so a power cycle does not reset the counters.
//
// Lines may contain %tokens which are substituted at render time:
//   %name %t %up %n %all %u %clock %last

#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <ctype.h>
#include <time.h>

#include "sprites.h"
#include "responses.h"
#include "personality.h"   // tokenize(), toLowerInPlace(), vibeRand()

#ifndef YOUR_NAME
#define YOUR_NAME "Friend"
#endif

static const char* const PREF_NS = "vibe";

// ===========================================================================
//  State
// ===========================================================================
struct BrainState {
  // --- persisted across reboots ---
  uint32_t messagesEver;
  uint32_t unplugCount;
  uint32_t sessionCount;
  uint32_t lastSeenEpoch;     // wall clock of the last message handled

  // --- live for this power cycle ---
  uint32_t lastMessageMillis; // millis() of the last user message
  uint32_t bootMillis;
  uint32_t messagesThisSession;
  uint8_t  pendingQid;        // question id it asked and is awaiting (0 = none)
  bool     havePrevWord;
  char     lastWord[24];
  char     prevWord[24];

  // set at boot when power was cut uncleanly
  bool     unplugPending;
  uint32_t unplugDeadSecs;
};

static BrainState B;
static Preferences brainPrefs;
static uint32_t   lastPersistMs = 0;

// ===========================================================================
//  Time helpers
// ===========================================================================
static bool clockValid() {
  return time(nullptr) > 1600000000;   // NTP has landed
}

static uint32_t nowEpoch() {
  return clockValid() ? (uint32_t)time(nullptr) : 0;
}

static String humanDuration(uint32_t s) {
  char b[48];
  if (s < 60)          snprintf(b, sizeof(b), "%u seconds", (unsigned)s);
  else if (s < 120)    snprintf(b, sizeof(b), "a minute");
  else if (s < 3600)   snprintf(b, sizeof(b), "%u minutes", (unsigned)(s / 60));
  else if (s < 7200)   snprintf(b, sizeof(b), "an hour");
  else if (s < 86400)  snprintf(b, sizeof(b), "%u hours", (unsigned)(s / 3600));
  else if (s < 172800) snprintf(b, sizeof(b), "a day");
  else                 snprintf(b, sizeof(b), "%u days", (unsigned)(s / 86400));
  return String(b);
}

static String clockString() {
  if (!clockValid()) return String("sometime");
  time_t t = time(nullptr);
  struct tm ti;
  localtime_r(&t, &ti);
  char b[16];
  int h = ti.tm_hour % 12;
  if (h == 0) h = 12;
  snprintf(b, sizeof(b), "%d:%02d%s", h, ti.tm_min, ti.tm_hour < 12 ? "am" : "pm");
  return String(b);
}

// -1 when the clock has not landed yet.
static int hourNow() {
  if (!clockValid()) return -1;
  time_t t = time(nullptr);
  struct tm ti;
  localtime_r(&t, &ti);
  return ti.tm_hour;
}

// ===========================================================================
//  Template rendering
// ===========================================================================
struct VibeCtx {
  const char* name;
  uint32_t    gapSecs;
  uint32_t    uptimeSecs;
  uint32_t    msgsSession;
  uint32_t    msgsEver;
  uint32_t    unplugs;
  const char* lastWord;
};

static bool tokIs(const char* s, size_t n, const char* lit) {
  return strlen(lit) == n && strncmp(s, lit, n) == 0;
}

static void renderTemplate(const char* tmpl, const VibeCtx& c, char* out, size_t outLen) {
  size_t o = 0;
  const char* p = tmpl;
  while (*p && o + 1 < outLen) {
    if (*p != '%') { out[o++] = *p++; continue; }

    const char* start = p + 1;
    const char* q = start;
    while (*q && isalpha((unsigned char)*q)) ++q;
    size_t n = (size_t)(q - start);

    String sub;
    bool known = true;
    if      (tokIs(start, n, "name"))    sub = c.name ? c.name : YOUR_NAME;
    else if (tokIs(start, n, "t"))       sub = humanDuration(c.gapSecs);
    else if (tokIs(start, n, "up"))      sub = humanDuration(c.uptimeSecs);
    else if (tokIs(start, n, "n"))       sub = String(c.msgsSession);
    else if (tokIs(start, n, "all"))     sub = String(c.msgsEver);
    else if (tokIs(start, n, "u"))       sub = String(c.unplugs);
    else if (tokIs(start, n, "clock"))   sub = clockString();
    else if (tokIs(start, n, "last"))    sub = (c.lastWord && c.lastWord[0]) ? c.lastWord : String("that");
    else known = false;

    if (!known) { out[o++] = '%'; p = start; continue; }   // leave unknown tokens intact

    for (size_t i = 0; i < sub.length() && o + 1 < outLen; i++) out[o++] = sub[i];
    p = q;
  }
  out[o] = '\0';
}

static String render(const char* tmpl, const VibeCtx& c) {
  char buf[320];
  renderTemplate(tmpl, c, buf, sizeof(buf));
  return String(buf);
}

// ===========================================================================
//  Key-list matching (same whole-word rule as the personality matcher)
// ===========================================================================
static bool keyMatches(const char* lower, const char* const* tokens, int nTokens,
                       const char* key) {
  if (strchr(key, ' ') != nullptr) return strstr(lower, key) != nullptr;
  for (int i = 0; i < nTokens; i++) {
    if (strcmp(tokens[i], key) == 0) return true;
  }
  return false;
}

static bool anyKey(const char* lower, const char* const* tokens, int nTokens,
                   const char* const* keys, size_t nKeys) {
  for (size_t i = 0; i < nKeys; i++) {
    if (keyMatches(lower, tokens, nTokens, keys[i])) return true;
  }
  return false;
}

// ===========================================================================
//  Persistence
// ===========================================================================
static void brainSave(bool force = false) {
  if (!force && millis() - lastPersistMs < 10000UL) return;   // throttle NVS wear
  lastPersistMs = millis();
  brainPrefs.putUInt("msgsEver", B.messagesEver);
  brainPrefs.putUInt("unplugs",  B.unplugCount);
  brainPrefs.putUInt("sessions", B.sessionCount);
  brainPrefs.putUInt("lastSeen", B.lastSeenEpoch);
}

static void brainBegin() {
  memset(&B, 0, sizeof(B));
  brainPrefs.begin(PREF_NS, false);

  B.messagesEver  = brainPrefs.getUInt("msgsEver", 0);
  B.unplugCount   = brainPrefs.getUInt("unplugs", 0);
  B.sessionCount  = brainPrefs.getUInt("sessions", 0);
  B.lastSeenEpoch = brainPrefs.getUInt("lastSeen", 0);

  B.bootMillis = millis();
  B.sessionCount++;

  // Power was cut. If the wall clock is known, work out for how long.
  if (B.lastSeenEpoch > 0 && clockValid()) {
    uint32_t dead = nowEpoch() - B.lastSeenEpoch;
    if (dead > 90) {
      B.unplugCount++;
      B.unplugPending = true;
      B.unplugDeadSecs = dead;
    }
  }
  brainSave(true);
}

// ===========================================================================
//  Reply pipeline
// ===========================================================================
struct VibeReply {
  Mood    mood;
  String  text;          // what actually goes on the panel / into the chat
  String  coreText;      // the chosen line BEFORE any question is appended
  bool    askedQuestion;
  uint8_t questionId;
};

// ===========================================================================
//  Anti-repetition
//  Without this the same canned sentence can come back several times in a row.
//  Every selection site goes through pickFreshCand()/pickUnsaid(), which render
//  a candidate and reject it if it was used in the last RECENT_MAX replies.
// ===========================================================================
static const int RECENT_MAX = 16;
static uint32_t  recentHash[RECENT_MAX];
static int       recentIdx = 0;

static uint32_t hashStr(const String& s) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < s.length(); i++) { h ^= (uint8_t)s[i]; h *= 16777619u; }
  return h ? h : 1u;
}
static bool recentlySaid(const String& s) {
  uint32_t h = hashStr(s);
  for (int i = 0; i < RECENT_MAX; i++) if (recentHash[i] == h) return true;
  return false;
}
static void noteSaid(const String& s) {
  if (s.length() == 0) return;
  recentHash[recentIdx] = hashStr(s);
  recentIdx = (recentIdx + 1) % RECENT_MAX;
}

// A candidate line plus the face to show while saying it.
struct Cand { const char* text; Mood mood; };

// Choose a candidate that has not just been used. Returns "" only if the array
// is empty; if everything is recent it returns the last one tried.
static String pickFreshCand(const Cand* c, int n, const VibeCtx& ctx, Mood* moodOut) {
  if (n <= 0) return String("");
  String  fallback;
  Mood    fbMood = MOOD_HAPPY;
  int tries = (n < 8) ? n : 8;
  for (int i = 0; i < tries; i++) {
    const Cand& k = c[vibeRand() % n];
    if (!k.text) continue;
    String s = render(k.text, ctx);
    if (!recentlySaid(s)) { if (moodOut) *moodOut = k.mood; return s; }
    fallback = s; fbMood = k.mood;
  }
  if (moodOut) *moodOut = fbMood;
  return fallback;
}

static String pickFrom(const char* const* lines, size_t nLines, const VibeCtx& c) {
  if (nLines == 0) return String("");
  return render(lines[vibeRand() % nLines], c);
}

// STRICT version: returns "" if EVERY candidate has been said recently. Used
// where a generic line would be wrong -- the caller then substitutes a fresh
// generic line.
static String pickUnsaid(const Cand* c, int n, const VibeCtx& ctx, Mood* moodOut) {
  if (n <= 0) return String("");
  for (int i = 0; i < n; i++) {
    const Cand& k = c[i];
    if (!k.text) continue;
    String s = render(k.text, ctx);
    if (!recentlySaid(s)) { if (moodOut) *moodOut = k.mood; return s; }
  }
  return String("");
}

// A few random generic lines, appended to a candidate pool so a keyword with
// only one variant still never repeats itself back to back.
static void addFallbackCands(Cand* c, int& n, int max, int howMany) {
  for (int i = 0; i < howMany && n < max; i++) {
    const ResponseRule* f = randomFallback();
    c[n].text = f->text;
    c[n].mood = f->mood;
    n++;
  }
}

// Give the keyword pool a fair shot; only if every variant is exhausted does it
// deflect to something generic.
static String pickKeywordReply(const Cand* c, int n, const VibeCtx& ctx, Mood* moodOut) {
  String s = pickUnsaid(c, n, ctx, moodOut);
  if (s.length() > 0) return s;
  for (int i = 0; i < 10; i++) {
    const ResponseRule* f = randomFallback();
    String t = render(f->text, ctx);
    if (!recentlySaid(t)) { if (moodOut) *moodOut = f->mood; return t; }
  }
  return render(randomFallback()->text, ctx);
}

// Words that must never be echoed back as "what you said".
static bool isStopWord(const char* w) {
  static const char* const SW[] = {
    "what", "when", "where", "who", "why", "how", "which", "that", "this",
    "these", "those", "you", "your", "yours", "have", "has", "had", "just",
    "like", "dont", "didnt", "doesnt", "arent", "isnt", "wasnt", "im", "its",
    "are", "was", "were", "not", "but", "all", "can", "will", "would",
    "could", "should", "know", "really", "very", "much", "more", "some",
    "any", "out", "off", "then", "there", "here", "with", "from", "about",
    "been", "being", "they", "them", "their", "she", "her", "him", "his", "one",
    "two", "get", "got", "let", "make", "made", "going", "want", "need",
    "think", "thought", "thing", "things", "time", "good", "well", "yeah",
    "okay", "because", "into", "over", "only", "also", "even", "still",
    "sure", "fine", "many", "please", "sorry", "ok", "yes", "no",
    "and", "the", "for", "youre", "youve", "hes", "shes", "theyre",
  };
  for (size_t i = 0; i < sizeof(SW) / sizeof(SW[0]); i++) {
    if (strcmp(w, SW[i]) == 0) return true;
  }
  return false;
}

static void rememberWords(const char* lower) {
  // Remember the longest *content* word so it can be echoed back later. Stop
  // words are excluded -- echoing "what" or "like" reads as nonsense.
  char scratch[256];
  snprintf(scratch, sizeof(scratch), "%s", lower);
  const char* toks[32];
  int n = tokenize(scratch, toks, 32);

  const char* best = nullptr;
  size_t bestLen = 0;
  for (int i = 0; i < n; i++) {
    size_t l = strlen(toks[i]);
    if (l >= 4 && l > bestLen && !isStopWord(toks[i])) { best = toks[i]; bestLen = l; }
  }
  if (best) {
    snprintf(B.prevWord, sizeof(B.prevWord), "%s", B.lastWord);
    B.havePrevWord = (B.lastWord[0] != '\0');
    snprintf(B.lastWord, sizeof(B.lastWord), "%s", best);
  }
}

static VibeReply brainReply(const char* rawInput) {
  VibeReply r;
  r.mood = MOOD_HAPPY;
  r.askedQuestion = false;
  r.questionId = 0;

  char lower[256];
  snprintf(lower, sizeof(lower), "%s", rawInput);
  toLowerInPlace(lower);

  char scratch[256];
  snprintf(scratch, sizeof(scratch), "%s", lower);
  const char* tokens[32];
  int nTokens = tokenize(scratch, tokens, 32);

  // ---- how long was the desk idle? ------------------------------------
  uint32_t gap = 0;
  bool hadPrevious = (B.lastMessageMillis != 0);
  if (hadPrevious) gap = (millis() - B.lastMessageMillis) / 1000UL;

  B.messagesThisSession++;
  B.messagesEver++;

  // ---- classify the message -------------------------------------------
  bool isFarewell = anyKey(lower, tokens, nTokens, FAREWELL_KEYS, FAREWELL_KEY_COUNT);

  VibeCtx ctx;
  ctx.name        = YOUR_NAME;
  ctx.gapSecs     = gap;
  ctx.uptimeSecs  = (millis() - B.bootMillis) / 1000UL;
  ctx.msgsSession = B.messagesThisSession;
  ctx.msgsEver    = B.messagesEver;
  ctx.unplugs     = B.unplugCount;
  ctx.lastWord    = B.lastWord;

  // ---- 1. power was cut: report it before anything else ---------------
  if (B.unplugPending) {
    B.unplugPending = false;
    VibeCtx uc = ctx;
    uc.gapSecs = B.unplugDeadSecs;
    Cand c[8]; int nc = 0;
    for (size_t i = 0; i < UNPLUG_RULE_COUNT && nc < 8; i++) {
      if (B.unplugDeadSecs >= (uint32_t)UNPLUG_RULES[i].minSec &&
          B.unplugDeadSecs <  (uint32_t)UNPLUG_RULES[i].maxSec) {
        c[nc].text = UNPLUG_RULES[i].text;
        c[nc].mood = UNPLUG_RULES[i].mood;
        nc++;
      }
    }
    if (nc) r.text = pickFreshCand(c, nc, uc, &r.mood);
    else    { r.mood = MOOD_CONFIDENT; r.text = render("Power was cut. Back online.", uc); }
  }
  // ---- 2. farewells are acknowledged, plainly -------------------------
  else if (isFarewell) {
    Cand c[8]; int nc = 0;
    for (size_t i = 0; i < FAREWELL_RULE_COUNT && nc < 8; i++) {
      const ByeRule& b = FAREWELL_RULES[i];
      c[nc].text = b.text; c[nc].mood = b.mood; nc++;
    }
    if (nc) r.text = pickFreshCand(c, nc, ctx, &r.mood);
  }
  // ---- 3. a long absence outranks the content -------------------------
  else if (hadPrevious && gap >= 1800) {
    Cand c[8]; int nc = 0;
    for (size_t i = 0; i < GAP_RULE_COUNT && nc < 8; i++) {
      if (gap >= (uint32_t)GAP_RULES[i].minSec && gap < (uint32_t)GAP_RULES[i].maxSec) {
        c[nc].text = GAP_RULES[i].text; c[nc].mood = GAP_RULES[i].mood; nc++;
      }
    }
    if (nc) r.text = pickFreshCand(c, nc, ctx, &r.mood);
    else    { r.mood = MOOD_CONFIDENT; r.text = render("You were gone %t.", ctx); }
    B.pendingQid = 0;
  }
  // ---- 4. what was actually said --------------------------------------
  else {
    const ResponseRule* matches[MAX_COLLECTED];
    int nm = collectRules(lower, matches, MAX_COLLECTED);

    if (nm > 0) {
      B.pendingQid = 0;                       // a concrete message clears the question
      Cand c[40]; int nc = 0;
      for (int i = 0; i < nm && nc < 32; i++) {
        c[nc].text = matches[i]->text;
        c[nc].mood = matches[i]->mood;
        nc++;
      }
      r.text = pickKeywordReply(c, nc, ctx, &r.mood);

      if (B.havePrevWord && B.lastWord[0] &&
          strcmp(B.prevWord, B.lastWord) == 0 && (vibeRand() % 3) == 0) {
        r.text = pickFrom(REPEAT_NOTICES, REPEAT_NOTICE_COUNT, ctx);
        r.mood = MOOD_SMUG;
      }
    }
    // ---- 5. nothing matched: answer the pending question if there is one
    else if (B.pendingQid != 0 && gap < 1800) {
      Cand c[12]; int nc = 0;
      for (size_t i = 0; i < ANSWER_RULE_COUNT && nc < 12; i++) {
        if (ANSWER_RULES[i].qid == B.pendingQid) {
          c[nc].text = ANSWER_RULES[i].text;
          c[nc].mood = ANSWER_RULES[i].mood;
          nc++;
        }
      }
      if (nc) r.text = pickFreshCand(c, nc, ctx, &r.mood);
      else    r.text = pickFrom(ANSWER_ACKS, ANSWER_ACK_COUNT, ctx);
      B.pendingQid = 0;
    }
    // ---- 6. a shorter absence ------------------------------------------
    else if (hadPrevious && gap >= 300) {
      Cand c[8]; int nc = 0;
      for (size_t i = 0; i < GAP_RULE_COUNT && nc < 8; i++) {
        if (gap >= (uint32_t)GAP_RULES[i].minSec &&
            gap <  (uint32_t)GAP_RULES[i].maxSec) {
          c[nc].text = GAP_RULES[i].text; c[nc].mood = GAP_RULES[i].mood; nc++;
        }
      }
      if (nc) r.text = pickFreshCand(c, nc, ctx, &r.mood);
      else    { r.mood = MOOD_CONFIDENT; r.text = render("You were gone %t.", ctx); }
    }
    // ---- 7. the clock, then the generic lines ---------------------------
    else {
      int h = hourNow();
      Cand c[16]; int nc = 0;
      if (h >= 0) {
        for (size_t i = 0; i < TIME_RULE_COUNT && nc < 8; i++) {
          if (h >= TIME_RULES[i].hMin && h < TIME_RULES[i].hMax) {
            c[nc].text = TIME_RULES[i].text; c[nc].mood = TIME_RULES[i].mood; nc++;
          }
        }
      }
      addFallbackCands(c, nc, 16, 8);
      if (nc) r.text = pickFreshCand(c, nc, ctx, &r.mood);
    }
  }

  // Remember the CORE line, before a question gets appended.
  String coreLine = r.text;
  r.coreText = coreLine;

  // The bot asks a question every so often, and REMEMBERS which one, so the
  // next message can be handled as an answer to it.
  if (!isFarewell && !B.unplugPending && (vibeRand() % 4) == 0) {
    const QuestionDef& q = QUESTION_DEFS[vibeRand() % QUESTION_DEF_COUNT];
    String qtext = render(q.text, ctx);
    if (r.text.length() + 1 + qtext.length() <= 148) {
      r.text += " ";
      r.text += qtext;
      r.askedQuestion = true;
      r.questionId = q.id;
    }
  }
  B.pendingQid = r.askedQuestion ? r.questionId : 0;

  rememberWords(lower);
  if (clockValid()) B.lastSeenEpoch = nowEpoch();
  B.lastMessageMillis = millis();
  brainSave();

  // keep the on-screen line inside what the text band can show
  if (r.text.length() > 150) r.text = r.text.substring(0, 147) + "...";
  noteSaid(coreLine);
  return r;
}

// ===========================================================================
//  Idle behaviour
// ===========================================================================
struct VibeIdle {
  Mood   mood;
  String text;
  bool   askedQuestion;
};

static VibeIdle brainIdle(uint32_t neglectSecs) {
  VibeIdle out;
  out.askedQuestion = false;

  VibeCtx ctx;
  ctx.name        = YOUR_NAME;
  ctx.gapSecs     = neglectSecs;
  ctx.uptimeSecs  = (millis() - B.bootMillis) / 1000UL;
  ctx.msgsSession = B.messagesThisSession;
  ctx.msgsEver    = B.messagesEver;
  ctx.unplugs     = B.unplugCount;
  ctx.lastWord    = B.lastWord;

  Cand c[14]; int nc = 0;

  if (neglectSecs < 300) {
    for (int i = 0; i < 14; i++) {
      const IdleThought& t = IDLE_THOUGHTS[vibeRand() % IDLE_THOUGHT_COUNT];
      c[nc].text = t.text; c[nc].mood = t.mood; nc++;
    }
  } else {
    // highest tier the neglect has reached
    int32_t band = IDLE_TIERS[0].afterSec;
    for (size_t i = 0; i < IDLE_TIER_COUNT; i++) {
      if ((int32_t)neglectSecs >= IDLE_TIERS[i].afterSec) band = IDLE_TIERS[i].afterSec;
    }
    for (size_t i = 0; i < IDLE_TIER_COUNT && nc < 14; i++) {
      if (IDLE_TIERS[i].afterSec != band) continue;
      c[nc].text = IDLE_TIERS[i].text; c[nc].mood = IDLE_TIERS[i].mood; nc++;
    }
  }
  if (nc) out.text = pickFreshCand(c, nc, ctx, &out.mood);
  noteSaid(out.text);

  brainSave();
  return out;
}

// ===========================================================================
//  Boot greeting: used right after Wi-Fi comes up
// ===========================================================================
static VibeReply brainBootGreeting() {
  VibeReply r;
  r.askedQuestion = false;
  VibeCtx ctx;
  ctx.name        = YOUR_NAME;
  ctx.gapSecs     = B.unplugPending ? B.unplugDeadSecs : 0;
  ctx.uptimeSecs  = 0;
  ctx.msgsSession = B.messagesThisSession;
  ctx.msgsEver    = B.messagesEver;
  ctx.unplugs     = B.unplugCount;
  ctx.lastWord    = B.lastWord;

  int h = hourNow();

  if (B.sessionCount <= 1) {
    r.mood = MOOD_HAPPY;
    r.text = render("First boot. Hello, %name. Ask me for the time or say help.", ctx);
  } else if (B.unplugPending) {
    r.mood = MOOD_CONFIDENT;
    r.text = render("Back online. Power had been cut for %t.", ctx);
  } else if (h >= 0 && h < 5) {
    r.mood = MOOD_SLEEPING;
    r.text = render("Booting at %clock. Quiet hours.", ctx);
  } else if (h >= 5 && h < 11) {
    r.mood = MOOD_HAPPY;
    r.text = render("Booting at %clock. Good morning.", ctx);
  } else if (h >= 22) {
    r.mood = MOOD_SLEEPING;
    r.text = render("Booting at %clock. Getting late.", ctx);
  } else {
    r.mood = MOOD_HAPPY;
    r.text = render("Booting at %clock. Ready.", ctx);
  }
  return r;
}
