// personality.h -- keyword matching engine for the desk bot.
//
// Deliberately free of TFT/serial/WiFi dependencies so it compiles and runs on
// the host too (see test/personality_test.cpp). Included by the .ino.

#pragma once
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "responses.h"

#ifdef VIBE_HOST_TEST
  static uint32_t vibeRand() { return (uint32_t)rand(); }
#else
  static uint32_t vibeRand() { return esp_random(); }
#endif

static void toLowerInPlace(char* s) {
  for (; *s; ++s) *s = tolower((unsigned char)*s);
}

// Tokenize into lowercase whole words so "her" cannot fire on "there".
static int tokenize(char* scratch, const char* tokens[], int maxTokens) {
  int n = 0;
  char* p = scratch;
  while (*p && n < maxTokens) {
    while (*p && !isalnum((unsigned char)*p)) *p++ = '\0';
    if (!*p) break;
    tokens[n++] = p;
    while (*p && isalnum((unsigned char)*p)) ++p;
  }
  return n;
}

static bool ruleMatches(const ResponseRule& r, const char* lower,
                        const char* const* tokens, int nTokens, bool phrasesPass) {
  if (r.keyword == nullptr) return false;
  bool isPhrase = strchr(r.keyword, ' ') != nullptr;
  if (isPhrase != phrasesPass) return false;

  if (isPhrase) return strstr(lower, r.keyword) != nullptr;

  for (int i = 0; i < nTokens; i++) {
    if (strcmp(tokens[i], r.keyword) == 0) return true;
  }
  return false;
}

struct RuleSet { const ResponseRule* rules; size_t count; };

static const RuleSet RULE_SETS[] = {
  { GREETING_RULES,     GREETING_RULE_COUNT     },   // hello, hi, good morning
  { INFO_RULES,         INFO_RULE_COUNT         },   // time, date, status, help
  { META_RULES,         META_RULE_COUNT         },   // what the device is
  { TOPIC_RULES,        TOPIC_RULE_COUNT        },   // work, coffee, music, ...
  { CONVERSATION_RULES, CONVERSATION_RULE_COUNT },   // ok, yes, lol, ...
};
static const int RULE_SET_COUNT = (int)(sizeof(RULE_SETS) / sizeof(RULE_SETS[0]));

// Two passes: exact multi-word phrases win over single words, so a phrase like
// "what time" resolves before the single word "time".
static const ResponseRule* pickRule(const char* input) {
  char lower[256];
  snprintf(lower, sizeof(lower), "%s", input);
  toLowerInPlace(lower);

  char tokenBuf[256];
  snprintf(tokenBuf, sizeof(tokenBuf), "%s", lower);
  const char* tokens[32];
  int nTokens = tokenize(tokenBuf, tokens, 32);

  const ResponseRule* matches[16];
  int nMatches = 0;

  for (int pass = 0; pass < 2; pass++) {
    bool phrasesPass = (pass == 0);
    for (int s = 0; s < RULE_SET_COUNT; s++) {
      for (size_t i = 0; i < RULE_SETS[s].count && nMatches < 16; i++) {
        const ResponseRule* r = &RULE_SETS[s].rules[i];
        if (ruleMatches(*r, lower, tokens, nTokens, phrasesPass)) {
          matches[nMatches++] = r;
        }
      }
    }
    if (nMatches > 0) break;   // phrase hits short-circuit the single-word pass
  }

  if (nMatches == 0) return nullptr;
  return matches[vibeRand() % nMatches];
}

// Same matching, but hands back EVERY match so the caller can choose among the
// variants of one keyword and reject one it just used. This is what stops the
// same sentence coming back twice in a row.
static const int MAX_COLLECTED = 32;
static int collectRules(const char* input, const ResponseRule** out, int maxOut) {
  char lower[256];
  snprintf(lower, sizeof(lower), "%s", input);
  toLowerInPlace(lower);

  char tokenBuf[256];
  snprintf(tokenBuf, sizeof(tokenBuf), "%s", lower);
  const char* tokens[32];
  int nTokens = tokenize(tokenBuf, tokens, 32);

  int n = 0;
  for (int pass = 0; pass < 2; pass++) {
    bool phrasesPass = (pass == 0);
    for (int s = 0; s < RULE_SET_COUNT; s++) {
      for (size_t i = 0; i < RULE_SETS[s].count && n < maxOut; i++) {
        const ResponseRule* r = &RULE_SETS[s].rules[i];
        if (ruleMatches(*r, lower, tokens, nTokens, phrasesPass)) out[n++] = r;
      }
    }
    if (n > 0) break;   // phrase hits short-circuit the single-word pass
  }
  return n;
}

static const ResponseRule* randomFallback() {
  return &FALLBACK_RULES[vibeRand() % FALLBACK_RULE_COUNT];
}

// Returns the mood it answers with; writes the line into *outLine.
static Mood respondWith(const char* input, const char** outLine) {
  const ResponseRule* r = pickRule(input);
  if (r == nullptr) r = randomFallback();
  *outLine = r->text;
  return r->mood;
}
