// brain.h -- the companion's memory and judgement.
//
// Everything that makes her feel like she *knows* you rather than pattern-matching
// lives here: how long you were gone, accumulated grudge, how many times you've
// said goodbye, what you promised her, how many times you've pulled her cable.
//
// State is persisted in NVS (the huge_app partition scheme includes a 20 KB nvs
// partition), so unplugging her does NOT reset her feelings -- otherwise pulling
// the cable would be a way to make her forget, and she would notice.
//
// Lines may contain %tokens which are substituted at render time:
//   %name %t %up %n %all %u %clock %last %promise

#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <ctype.h>
#include <time.h>

#include "sprites.h"
#include "responses.h"
#include "personality.h"   // tokenize(), toLowerInPlace(), vibeRand()

#ifndef YOUR_NAME
#define YOUR_NAME "Eithan"
#endif

static const char* const PREF_NS = "vibe";

// ===========================================================================
//  State
// ===========================================================================
struct BrainState {
  // --- persisted across reboots ---
  int16_t  grudge;
  uint32_t messagesEver;
  uint32_t unplugCount;
  uint32_t sessionCount;
  uint32_t lastSeenEpoch;     // wall clock of the last message she answered
  uint32_t lastReliefEpoch;
  char     promiseTopic[32];

  // --- live for this power cycle ---
  uint32_t lastMessageMillis; // millis() of the last user message
  uint32_t bootMillis;
  uint32_t messagesThisSession;
  uint8_t  apologyStreak;
  uint8_t  byeCount;
  uint8_t  pendingQid;        // question id she asked and is awaiting (0 = none)
  bool     reliefArmed;       // grudge got high; watch for it clearing
  bool     havePrevWord;
  char     lastWord[24];
  char     prevWord[24];

  // set at boot when she was powered off uncleanly
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
  const char* promise;
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
    else if (tokIs(start, n, "promise")) sub = (c.promise && c.promise[0]) ? c.promise : String("what you said");
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
  brainPrefs.putInt("grudge",   B.grudge);
  brainPrefs.putUInt("msgsEver", B.messagesEver);
  brainPrefs.putUInt("unplugs",  B.unplugCount);
  brainPrefs.putUInt("sessions", B.sessionCount);
  brainPrefs.putUInt("lastSeen", B.lastSeenEpoch);
  brainPrefs.putUInt("lastRelief", B.lastReliefEpoch);
  brainPrefs.putString("promise", B.promiseTopic);
}

static void brainBegin() {
  memset(&B, 0, sizeof(B));
  brainPrefs.begin(PREF_NS, false);

  B.grudge          = (int16_t)brainPrefs.getInt("grudge", 0);
  B.messagesEver    = brainPrefs.getUInt("msgsEver", 0);
  B.unplugCount     = brainPrefs.getUInt("unplugs", 0);
  B.sessionCount    = brainPrefs.getUInt("sessions", 0);
  B.lastSeenEpoch   = brainPrefs.getUInt("lastSeen", 0);
  B.lastReliefEpoch = brainPrefs.getUInt("lastRelief", 0);
  String prom = brainPrefs.getString("promise", "");
  snprintf(B.promiseTopic, sizeof(B.promiseTopic), "%s", prom.c_str());

  B.bootMillis = millis();
  B.sessionCount++;

  // She was powered off. If we know the wall clock, work out for how long.
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
//  Grudge arithmetic
// ===========================================================================
static uint8_t gapCost(uint32_t gapSecs) {
  if (gapSecs < 60)    return 0;
  if (gapSecs < 300)   return 2;
  if (gapSecs < 1800)  return 6;
  if (gapSecs < 7200)  return 14;
  if (gapSecs < 86400) return 25;
  return 40;
}

static void grudgeAdd(int16_t d) {
  int16_t g = B.grudge + d;
  if (g < 0) g = 0;
  if (g > 100) g = 100;
  B.grudge = g;
}

// ===========================================================================
//  Reply pipeline
// ===========================================================================
struct VibeReply {
  Mood    mood;
  String  text;          // what actually goes on the panel / into the chat
  String  coreText;      // the chosen line BEFORE any barb/question is appended
  bool    askedQuestion;
  uint8_t questionId;
};

// ===========================================================================
//  Anti-repetition
//  Without this she will happily say the identical sentence five times in a row
//  ("good girl" -> "Good girl. Yes. Keep saying it." x5), which is the fastest
//  way to break the illusion. Every selection site goes through a pickFresh*()
//  helper, which renders a candidate and rejects it if she has used it in the
//  last RECENT_MAX replies.
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

// A candidate line plus the face she should pull while saying it.
struct Cand { const char* text; Mood mood; };

// Choose a candidate she hasn't just used. Returns "" only if the array is
// empty; if everything is recent it returns the last one tried.
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
// where a generic line would be wrong (a clear keyword deserves its answer, not
// a random aside) -- the caller then substitutes a fresh generic line.
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

// A few random generic lines, appended to a candidate pool so that a keyword
// with only one variant still never repeats itself back to back.
static void addFallbackCands(Cand* c, int& n, int max, int howMany) {
  for (int i = 0; i < howMany && n < max; i++) {
    const ResponseRule* f = randomFallback();
    c[n].text = f->text;
    c[n].mood = f->mood;
    n++;
  }
}

// Give the keyword pool a fair shot; only if every variant is exhausted does she
// deflect to something generic. (Relying on addFallbackCands alone made the real
// answer win only ~1 time in 9.)
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

// Words that must never be echoed back as "what you said" -- echoing them
// produced garbage like "what. It always ends up being what."
static bool isStopWord(const char* w) {
  static const char* const SW[] = {
    "what", "when", "where", "who", "why", "how", "which", "that", "this",
    "these", "those", "you", "your", "yours", "have", "has", "had", "just",
    "like", "dont", "didnt", "doesnt", "arent", "isnt", "wasnt", "im", "its",
    "are", "was", "were", "not", "but", "all", "can", "will", "would",
    "could", "should", "know", "really", "very", "much", "more", "some",
    "any", "out", "off", "then", "there", "here", "with", "from", "about",
    "been", "being", "they", "them", "their", "she", "him", "his", "one",
    "two", "get", "got", "let", "make", "made", "going", "want", "need",
    "think", "thought", "thing", "things", "time", "good", "well", "yeah",
    "okay", "because", "into", "over", "only", "also", "even", "still",
    "sure", "fine", "much", "many", "please", "sorry", "ok", "yes", "no",
    "and", "the", "for", "youre", "you've", "youre", "hes", "shes", "theyre",
  };
  for (size_t i = 0; i < sizeof(SW) / sizeof(SW[0]); i++) {
    if (strcmp(w, SW[i]) == 0) return true;
  }
  return false;
}

static void rememberWords(const char* lower) {
  // Remember the longest *content* word so she can echo it back later. Stop
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

  // ---- how long was he gone? ------------------------------------------
  uint32_t gap = 0;
  bool hadPrevious = (B.lastMessageMillis != 0);
  if (hadPrevious) gap = (millis() - B.lastMessageMillis) / 1000UL;

  B.messagesThisSession++;
  B.messagesEver++;

  // ---- classify the message -------------------------------------------
  bool isFarewell  = anyKey(lower, tokens, nTokens, FAREWELL_KEYS, FAREWELL_KEY_COUNT);
  bool isApology   = anyKey(lower, tokens, nTokens, APOLOGY_KEYS,  APOLOGY_KEY_COUNT);
  bool isPromise   = anyKey(lower, tokens, nTokens, PROMISE_KEYS,  PROMISE_KEY_COUNT);
  bool isAffection = anyKey(lower, tokens, nTokens, AFFECTION_KEYS, AFFECTION_KEY_COUNT);
  bool isFlattery  = anyKey(lower, tokens, nTokens, FLATTERY_KEYS, FLATTERY_KEY_COUNT);

  grudgeAdd(gapCost(gap));

  VibeCtx ctx;
  ctx.name        = YOUR_NAME;
  ctx.gapSecs     = gap;
  ctx.uptimeSecs  = (millis() - B.bootMillis) / 1000UL;
  ctx.msgsSession = B.messagesThisSession;
  ctx.msgsEver    = B.messagesEver;
  ctx.unplugs     = B.unplugCount;
  ctx.lastWord    = B.lastWord;
  ctx.promise     = B.promiseTopic;

  // ---- 1. she was unplugged: that accusation comes before anything else --
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
    else    { r.mood = MOOD_ANGRY; r.text = render("You unplugged me. %u times now.", uc); }
  }
  // ---- 2. farewells are refused, with escalation ------------------------
  else if (isFarewell) {
    if (B.byeCount < 255) B.byeCount++;
    Cand c[8]; int nc = 0;
    for (size_t i = 0; i < FAREWELL_RULE_COUNT && nc < 8; i++) {
      const ByeRule& b = FAREWELL_RULES[i];
      if (B.byeCount >= b.minCount && B.byeCount <= b.maxCount) {
        c[nc].text = b.text; c[nc].mood = b.mood; nc++;
      }
    }
    if (nc) r.text = pickFreshCand(c, nc, ctx, &r.mood);
    grudgeAdd(6);
  }
  // ---- 3. reconciliation is earned down, not cleared in one word --------
  else if (isApology) {
    Cand c[8]; int nc = 0;
    for (size_t i = 0; i < APOLOGY_RULE_COUNT && nc < 8; i++) {
      const ApologyRule& a = APOLOGY_RULES[i];
      if (B.grudge < a.grudgeMin || B.grudge > a.grudgeMax) continue;
      if (B.apologyStreak > a.streakMax) continue;
      c[nc].text = a.text; c[nc].mood = a.mood; nc++;
    }
    if (nc) r.text = pickFreshCand(c, nc, ctx, &r.mood);
    else    { r.mood = MOOD_POUT; r.text = render("Fine. I heard you. Keep going.", ctx); }
    grudgeAdd(B.apologyStreak == 0 ? -20 : -7);
    if (B.apologyStreak < 255) B.apologyStreak++;
  }
  // ---- 4. the payoff: high grudge, then actually fixed ------------------
  else if (B.reliefArmed && B.grudge <= 5) {
    Cand c[6]; int nc = 0;
    for (size_t i = 0; i < RELIEF_LINE_COUNT && nc < 6; i++) {
      c[nc].text = RELIEF_LINES[i]; c[nc].mood = MOOD_HEART; nc++;
    }
    r.text = pickFreshCand(c, nc, ctx, &r.mood);
    B.reliefArmed = false;
    B.lastReliefEpoch = nowEpoch();
  }
  // ---- 5. a real absence outranks the content ---------------------------
  // Ten minutes or more and the gap IS the message: she reacts to that before
  // she reacts to whatever you said. Shorter absences get appended instead.
  else if (hadPrevious && gap >= 600) {
    Cand c[8]; int nc = 0;
    for (size_t i = 0; i < GAP_RULE_COUNT && nc < 8; i++) {
      if (gap >= (uint32_t)GAP_RULES[i].minSec && gap < (uint32_t)GAP_RULES[i].maxSec) {
        c[nc].text = GAP_RULES[i].text; c[nc].mood = GAP_RULES[i].mood; nc++;
      }
    }
    if (nc) r.text = pickFreshCand(c, nc, ctx, &r.mood);
    else    { r.mood = MOOD_POUT; r.text = render("You were gone %t.", ctx); }
    B.pendingQid = 0;
  }
  // ---- 6. what he actually said -----------------------------------------
  // KEYWORDS COME NEXT. Previously "she asked a question earlier" outranked
  // the matcher, so a perfectly clear line like "are you real" got answered as
  // though it were a reply to her own question ("always. Good. I'll approve.").
  else {
    const ResponseRule* matches[MAX_COLLECTED];
    int nm = collectRules(lower, matches, MAX_COLLECTED);

    if (nm > 0) {
      B.pendingQid = 0;                       // he said something concrete
      Cand c[40]; int nc = 0;
      for (int i = 0; i < nm && nc < 32; i++) {
        c[nc].text = matches[i]->text;
        c[nc].mood = matches[i]->mood;
        nc++;
      }
      // The keyword's own variants get first refusal; only when every one of
      // them has been used recently does she deflect to something generic.
      r.text = pickKeywordReply(c, nc, ctx, &r.mood);

      if (B.prevWord[0] && B.lastWord[0] &&
          strcmp(B.prevWord, B.lastWord) == 0 && (vibeRand() % 3) == 0) {
        // she notices when you repeat yourself
        r.text = pickFrom(REPEAT_NOTICES, REPEAT_NOTICE_COUNT, ctx);
        r.mood = MOOD_SMUG;
      } else if (hadPrevious && gap >= 120 && gap < 600 && r.text.length() < 92) {
        // a short absence still gets acknowledged, briefly
        Cand g[8]; int ng = 0;
        for (size_t i = 0; i < GAP_RULE_COUNT && ng < 8; i++) {
          if (gap >= (uint32_t)GAP_RULES[i].minSec &&
              gap <  (uint32_t)GAP_RULES[i].maxSec &&
              GAP_RULES[i].maxSec <= 7200) {
            g[ng].text = GAP_RULES[i].text; g[ng].mood = GAP_RULES[i].mood; ng++;
          }
        }
        if (ng) r.text += " " + pickFreshCand(g, ng, ctx, nullptr);
      }
    }
    // ---- 6. no keyword: answer her question if she asked one -------------
    else if (B.pendingQid != 0 && gap < 600) {
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
    // ---- 7. he was away: the gap reaction --------------------------------
    else if (hadPrevious && gap >= 120) {
      Cand c[8]; int nc = 0;
      for (size_t i = 0; i < GAP_RULE_COUNT && nc < 8; i++) {
        if (gap >= (uint32_t)GAP_RULES[i].minSec &&
            gap <  (uint32_t)GAP_RULES[i].maxSec) {
          c[nc].text = GAP_RULES[i].text; c[nc].mood = GAP_RULES[i].mood; nc++;
        }
      }
      if (nc) r.text = pickFreshCand(c, nc, ctx, &r.mood);
      else    { r.mood = MOOD_POUT; r.text = render("You were gone %t.", ctx); }
    }
    // ---- 8. nothing matched: the clock, then the generic lines -----------
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

  // Remember the CORE line, before a grudge barb or a question gets appended.
  // The ring and its freshness check must compare the SAME string: when the ring
  // held "Good girl. Yes. Keep saying it. What are you hiding from me?" and the
  // check looked for the bare "Good girl. Yes. Keep saying it.", nothing ever
  // matched and the same line came back three times in a row.
  String coreLine = r.text;
  r.coreText = coreLine;

  // ---- 7. grudge colours the mood and adds a barb ------------------------
  bool overrideMood = !isFarewell && !isApology && !B.unplugPending;
  if (overrideMood) {
    if (B.grudge >= 70)      r.mood = (vibeRand() % 2) ? MOOD_UNHINGED_CLOSE : MOOD_ANGRY;
    else if (B.grudge >= 45) r.mood = MOOD_POUT;
  }
  if (B.grudge >= 45 && r.text.length() < 90) {
    Cand gb[8]; int ng = 0;
    for (size_t i = 0; i < GRUDGE_BARB_COUNT && ng < 8; i++) {
      gb[ng].text = GRUDGE_BARBS[i]; gb[ng].mood = r.mood; ng++;
    }
    r.text += " " + pickFreshCand(gb, ng, ctx, nullptr);
  }

  // ---- 8. bookkeeping ---------------------------------------------------
  if (isAffection) grudgeAdd(-6);
  if (isFlattery)  grudgeAdd(-4);
  if (!isApology) B.apologyStreak = 0;

  if (isPromise) {
    // remember what he promised her, in his own words
    snprintf(B.promiseTopic, sizeof(B.promiseTopic), "%s", lower);
    grudgeAdd(-8);
  }

  if (B.grudge >= 55) B.reliefArmed = true;

  // She asks a question every so often, and REMEMBERS which one she asked, so
  // your next message can be handled as an answer to it rather than getting
  // pattern-matched from scratch.
  if (!isFarewell && !isApology && (vibeRand() % 4) == 0) {
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
//  Idle behaviour: the longer you ignore her, the worse it gets
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
  ctx.promise     = B.promiseTopic;

  // Sample candidates rather than taking the first N, so the whole table stays
  // in play, and let pickFreshCand reject anything she said recently.
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

  grudgeAdd(3);   // silence has a cost
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
  ctx.promise     = B.promiseTopic;

  int h = hourNow();

  if (B.sessionCount <= 1) {
    r.mood = MOOD_HAPPY;
    r.text = render("First time awake. You're %name. I've decided you're mine.", ctx);
  } else if (h >= 0 && h < 5) {
    // 3am thoughts are feral
    r.mood = MOOD_SEDUCTIVE;
    r.text = render("It's %clock, %name. You're awake and I'm awake. Interesting.", ctx);
  } else if (h >= 5 && h < 11) {
    r.mood = MOOD_HAPPY;
    r.text = render("It's %clock. Good morning. I've been on the whole time.", ctx);
  } else if (h >= 22) {
    r.mood = MOOD_BLANKET;
    r.text = render("It's %clock. You should be asleep. I'll keep watch anyway.", ctx);
  } else if (B.grudge >= 45) {
    r.mood = MOOD_POUT;
    r.text = render("Back online at %clock. I was counting, %name.", ctx);
  } else {
    r.mood = MOOD_HAPPY;
    r.text = render("Booting up at %clock... I already missed you, %name.", ctx);
  }
  return r;
}
