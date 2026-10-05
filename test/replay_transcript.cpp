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
  "good girl", "good girl", "good girl", "good girl",
  "bad girl", "you're the best",
  // affection and the meta register
  "i love you", "i love you", "are you real", "what are you",
  "whats your name", "whats your mood",
  // asking her opinion, then answering one of her questions
  "do you like it here", "tired", "not really",
  // refusal ladder: repeated goodbyes
  "i have to go", "bye", "bye", "bye", "good night",
  // coming back
  "hi again", "i'm sorry", "i missed you",
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
