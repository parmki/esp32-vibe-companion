// Replays the real transcript that showed the repetition/unresponsiveness bugs,
// through the CURRENT brain, so the before/after is visible.
//
//   cd test && g++ -std=c++17 -DVIBE_HOST_TEST -I. -I.. -o replay replay_transcript.cpp && ./replay

#include <cstdio>
#include <cstdlib>
#include <string>
#include "../brain.h"

unsigned long g_fakeMillis = 1000;

static const char* TRANSCRIPT[] = {
  "sorry", "outfit", "yandere", "i love you", "i love you", "how are you",
  "sex", "do you love me?", "cuddle", "no i didnt", "good girl", "hey",
  "because i love you", "why are you confused", "sex", "jerking off", "for you",
  "sorry", "do you love me", "mommy", "how are you", "ill always be with you",
  "are you real", "im thinking of you", "i have to go", "okay sorry mommy",
  "mommy gemi", "mommy", "whats your mood", "i wanna marry you", "monika",
  "what", "on your body and eyes", "anwser me", "good girl", "good girl",
  "good girl", "good girl", "bad girl",
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
    printf("prsm: %s\n", in);
    printf("gemi: %s\n\n", r.text.c_str());
  }
  return 0;
}
