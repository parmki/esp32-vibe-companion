// Replays a sample conversation through the CURRENT brain, so behaviour is
// visible without flashing the board. Useful for eyeballing variety: repeat a
// keyword a few times and check the replies differ.
//
//   cd test && g++ -std=c++17 -DVIBE_HOST_TEST -I. -I.. -o replay replay_transcript.cpp && ./replay

#include <cstdio>
#include <cstdlib>
#include <string>
#include "../brain.h"

unsigned long g_fakeMillis = 1000;

static const char* TRANSCRIPT[] = {
  // greetings and small talk
  "hey", "how are you", "whats up", "good morning", "thanks",
  // the anti-repetition case: four identical inputs
  "hello", "hello", "hello", "hello",
  "how are you", "what time", "status", "help",
  // the meta register and facts
  "who are you", "what are you", "are you real", "what can you do",
  "uptime", "date",
  // topics
  "tired", "coffee", "work", "music",
  // a farewell ladder
  "i have to go", "bye", "good night",
  // coming back
  "hi again",
};

int main() {
  srand(20261005);
  brainPrefs.clear();
  memset(&B, 0, sizeof(B));
  brainBegin();
  B.sessionCount = 2;

  for (const char* in : TRANSCRIPT) {
    g_fakeMillis += 9000;
    VibeReply r = brainReply(in);
    printf("user: %s\nresp: %s\n\n", in, r.coreText.c_str());
  }
  return 0;
}
