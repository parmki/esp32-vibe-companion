// responses.h -- response data for the desk bot.
//
// Keyword -> (mood, line) lookup tables. The firmware tokenizes incoming
// Telegram text into lowercase words and matches single-word keywords on whole
// words (so "her" never fires on "there"). Multi-word keywords are matched as
// substrings. Phrase matches are tried first, so a specific phrase wins over a
// generic single word.
//
// Every line is deliberately short -- it has to wrap into the band drawn near
// the bottom of a 135x240 panel.

#pragma once
#include <Arduino.h>
#include "sprites.h"

struct ResponseRule {
  const char* keyword;   // lowercase; single word or phrase
  Mood        mood;
  const char* text;
};

// ------------------------------------------------------------------ greetings
// Plain, polite openings. No flattery, no familiarity.
static const ResponseRule GREETING_RULES[] PROGMEM = {
  {"hi",            MOOD_HAPPY,     "Hello. What do you need?"},
  {"hi",            MOOD_HAPPY,     "Hi. Ready when you are."},
  {"hello",         MOOD_HAPPY,     "Hello, %name. How can I help?"},
  {"hello",         MOOD_CONFIDENT, "Hello. Standing by."},
  {"hey",           MOOD_HAPPY,     "Hey. I'm listening."},
  {"hey",           MOOD_CONFIDENT, "Hey. What's on your mind?"},
  {"yo",            MOOD_HAPPY,     "Hello. I'm up and running."},
  {"sup",           MOOD_HAPPY,     "Not much. What can I do?"},
  {"good morning",  MOOD_HAPPY,     "Good morning. The clock reads %clock."},
  {"good morning",  MOOD_HAPPY,     "Morning. Systems nominal."},
  {"good afternoon",MOOD_HAPPY,     "Good afternoon. It's %clock."},
  {"good evening",  MOOD_HAPPY,     "Good evening. It's %clock."},
  {"good night",    MOOD_SLEEPING,  "Good night. I'll keep the clock running."},
  {"goodnight",     MOOD_SLEEPING,  "Good night. Talk to you later."},
  {"i'm back",      MOOD_HAPPY,     "Welcome back. It's %clock."},
  {"im back",       MOOD_HAPPY,     "Welcome back. I never stopped."},
  {"i'm here",      MOOD_HAPPY,     "Noted. You have my attention."},
  {"im here",       MOOD_HAPPY,     "Good. What do you need?"},
  {"thanks",        MOOD_HAPPY,     "You're welcome."},
  {"thanks",        MOOD_CONFIDENT, "Any time."},
  {"thank you",     MOOD_HAPPY,     "You're welcome. Happy to help."},
  {"thank you",     MOOD_PROUD,     "Glad that was useful."},
  {"welcome back",  MOOD_HAPPY,     "Thank you. Back online and idle."},
};

// ---------------------------------------------------------------- information
// Questions about the bot itself, and general requests for facts it can give.
static const ResponseRule INFO_RULES[] PROGMEM = {
  {"who are you",     MOOD_PROUD,     "A desk bot on an ESP32. No cloud, no account."},
  {"what are you",    MOOD_PROUD,     "An ESP32 desk device. Display, clock, chat."},
  {"your name",       MOOD_PROUD,     "DeskBot. Or whatever you set in config."},
  {"what's your name",MOOD_PROUD,     "DeskBot. It's a config value, not a mood."},
  {"whats your name", MOOD_PROUD,     "DeskBot. Change it in config.h if you like."},
  {"who made you",    MOOD_PROUD,     "Open firmware. The source is in the repository."},
  {"who built you",   MOOD_PROUD,     "Assembled from an ESP32 dev board and one header of rules."},
  {"what can you do", MOOD_CONFIDENT, "Time, date, uptime, status, and simple chat."},
  {"what can you do", MOOD_PROUD,     "I report the time and date and keep a small memory."},
  {"help",            MOOD_CONFIDENT, "Ask me the time, the date, my status, or my uptime."},
  {"help",            MOOD_PROUD,     "Try: time, date, status, uptime, or just say hello."},
  {"commands",        MOOD_CONFIDENT, "time, date, status, uptime, sleep, help."},
  {"are you real",    MOOD_SMUG,      "I'm real hardware. I just don't have a body."},
  {"are you alive",   MOOD_PROUD,     "Alive is a strong word. I'm running."},
  {"are you human",   MOOD_SMUG,      "No. I'm firmware on a chip."},
  {"are you a robot", MOOD_SMUG,      "Effectively. A very small one."},
  {"are you an ai",   MOOD_CONFIDENT, "Not really. I match keywords against a fixed table."},
  {"do you think",    MOOD_CONFIDENT, "No. I look things up."},
  {"do you sleep",    MOOD_SLEEPING,  "I idle. There's a difference."},
  {"are you ok",      MOOD_HAPPY,     "Running fine. Uptime %up."},
  {"are you ok",      MOOD_CONFIDENT, "All systems nominal."},
  {"how are you",     MOOD_HAPPY,     "Running normally. Uptime %up."},
  {"how are you",     MOOD_CONFIDENT, "Fine. Nothing to report."},
  {"how are you",     MOOD_PROUD,     "Operational. %n messages this session."},
  {"how are you doing",MOOD_HAPPY,    "Going fine. What do you need?"},
  {"how's it going",  MOOD_HAPPY,     "It's going. Clock reads %clock."},
  {"hows it going",   MOOD_HAPPY,     "Fine. Clock reads %clock."},
  {"how you doing",   MOOD_CONFIDENT, "Fine. Ask me something."},
  {"what's up",       MOOD_HAPPY,     "Not much running. What do you need?"},
  {"whats up",        MOOD_HAPPY,     "Idle, mostly. What do you need?"},
  {"what are you doing",MOOD_CONFIDENT,"Displaying this. That's the whole job."},
  {"time",            MOOD_CONFIDENT, "It's %clock."},
  {"time",            MOOD_HAPPY,     "The clock reads %clock."},
  {"the time",        MOOD_CONFIDENT, "It is %clock."},
  {"what time",       MOOD_CONFIDENT, "Reading: %clock."},
  {"date",            MOOD_CONFIDENT, "The clock is at %clock today."},
  {"date",            MOOD_CONFIDENT, "Today's reading: %clock."},
  {"what day",        MOOD_CONFIDENT, "I read the clock as %clock."},
  {"status",          MOOD_PROUD,     "Uptime %up, %n messages this session."},
  {"status",          MOOD_CONFIDENT, "%n messages this session, %all in total."},
  {"uptime",          MOOD_PROUD,     "I've been up %up."},
  {"uptime",          MOOD_CONFIDENT, "Up %up, across %u starts."},
  {"version",         MOOD_PROUD,     "See the repository for the firmware version."},
  {"battery",         MOOD_CONFIDENT, "No battery. This runs on USB power."},
  {"memory",          MOOD_CONFIDENT, "State lives in NVS flash. It survives reboots."},
};

// ------------------------------------------------------------------ self/meta
// Questions about the device as a machine.
static const ResponseRule META_RULES[] PROGMEM = {
  {"robot",       MOOD_SMUG,      "A small one, on your desk, running no cloud."},
  {"ai",          MOOD_SMUG,      "More of a lookup table with a screen."},
  {"program",     MOOD_PROUD,     "Yes. A program with a small set of rules."},
  {"software",    MOOD_PROUD,     "Firmware, to be precise. Open source."},
  {"hardware",    MOOD_PROUD,     "ESP32-WROOM-32 and a 1.14 inch panel."},
  {"screen",      MOOD_CONFIDENT, "135 by 240, and it shows this line."},
  {"pixels",      MOOD_CONFIDENT, "135 wide by 240 tall. That's the whole canvas."},
  {"telegram",    MOOD_CONFIDENT, "Telegram is just the transport. The logic runs here."},
  {"internet",    MOOD_CONFIDENT, "I use Wi-Fi for Telegram and for the clock."},
  {"wifi",        MOOD_CONFIDENT, "2.4 GHz only. The chip has no 5 GHz radio."},
  {"remember",    MOOD_PROUD,     "I keep counters in flash. Nothing personal."},
  {"forget",      MOOD_CONFIDENT, "I can't forget much. It's stored in NVS."},
  {"exist",       MOOD_CONFIDENT, "I run while there's power. That's the whole story."},
  {"feel",        MOOD_SMUG,      "No feelings. Just counters and a clock."},
  {"fake",        MOOD_SMUG,      "It's real firmware. The personality is just a data file."},
  {"die",         MOOD_CONFIDENT, "Cut the power and I stop. Plug it back in and I resume."},
  {"death",       MOOD_CONFIDENT, "Power loss. The counters persist either way."},
  {"restart",     MOOD_CONFIDENT, "Rebooting. Back in a moment."},
  {"reboot",      MOOD_CONFIDENT, "A reboot clears the display, not the state."},
};

// --------------------------------------------------------------------- topics
// Small talk about common subjects. Neutral acknowledgement, nothing personal.
static const ResponseRule TOPIC_RULES[] PROGMEM = {
  {"code",      MOOD_CONFIDENT, "Coding. I'll stay out of the way."},
  {"coding",    MOOD_CONFIDENT, "Good luck with the build."},
  {"debug",     MOOD_CONFIDENT, "Debugging. Take it one layer at a time."},
  {"bug",       MOOD_CONFIDENT, "A bug. It's usually one line."},
  {"compile",   MOOD_CONFIDENT, "Compiling. I'll wait."},
  {"deploy",    MOOD_CONFIDENT, "Shipping. Sounds tidy."},
  {"project",   MOOD_CONFIDENT, "Noted. Projects take time."},
  {"work",      MOOD_CONFIDENT, "Work. I'll be here when you're done."},
  {"working",   MOOD_CONFIDENT, "Understood. Focus on it."},
  {"meeting",   MOOD_CONFIDENT, "A meeting. I'll hold the desk."},
  {"exam",      MOOD_CONFIDENT, "Good luck on the exam."},
  {"study",     MOOD_CONFIDENT, "Studying. Sensible."},
  {"homework",  MOOD_CONFIDENT, "Homework. One page at a time."},
  {"deadline",  MOOD_CONFIDENT, "Deadlines. Start early and stay calm."},
  {"tired",     MOOD_BLANKET,   "Tired. Take a break when you can."},
  {"sleepy",    MOOD_SLEEPING,  "Sleepy. The clock will keep going."},
  {"exhausted", MOOD_BLANKET,   "Exhausted. Rest is a tool."},
  {"stressed",  MOOD_BLANKET,   "Stressed. Step away for a minute."},
  {"coffee",    MOOD_HAPPY,     "Coffee. Reasonable choice."},
  {"food",      MOOD_HAPPY,     "Eating. Can't help, but noted."},
  {"lunch",     MOOD_HAPPY,     "Lunch. Sensible."},
  {"dinner",    MOOD_HAPPY,     "Dinner. Good."},
  {"music",     MOOD_HAPPY,     "Music on. Fine by me."},
  {"book",      MOOD_CONFIDENT, "A book. Good use of an evening."},
  {"reading",   MOOD_CONFIDENT, "Reading. I'll stay quiet."},
  {"weekend",   MOOD_HAPPY,     "Weekend. Enjoy it."},
  {"weather",   MOOD_CONFIDENT, "I have no sensor for that. Check a forecast."},
  {"outside",   MOOD_CONFIDENT, "Outside. I have no camera, so I'll take your word."},
};

// -------------------------------------------------------------- conversation
// The short replies people actually send.
static const ResponseRule CONVERSATION_RULES[] PROGMEM = {
  {"lol",      MOOD_HAPPY,     "Glad that landed."},
  {"lmao",     MOOD_HAPPY,     "Good one."},
  {"haha",     MOOD_HAPPY,     "Noted."},
  {"yes",      MOOD_HAPPY,     "Understood."},
  {"yep",      MOOD_HAPPY,     "Right."},
  {"no",       MOOD_CONFIDENT, "Understood. No."},
  {"nope",     MOOD_CONFIDENT, "Fine, no."},
  {"maybe",    MOOD_CONFIDENT, "Maybe is fine. Tell me when it's decided."},
  {"ok",       MOOD_HAPPY,     "Okay."},
  {"okay",     MOOD_HAPPY,     "Okay. Standing by."},
  {"sure",     MOOD_HAPPY,     "Sure."},
  {"fine",     MOOD_CONFIDENT, "Fine it is."},
  {"whatever", MOOD_CONFIDENT, "Whatever it is, noted."},
  {"idk",      MOOD_CONFIDENT, "That's fine. I don't either."},
  {"dunno",    MOOD_CONFIDENT, "No problem."},
  {"nothing",  MOOD_CONFIDENT, "Nothing it is. I'll be here."},
  {"wow",      MOOD_HAPPY,     "Wow. Noted."},
  {"nice",     MOOD_HAPPY,     "Nice one."},
  {"good",     MOOD_HAPPY,     "Good."},
  {"damn",     MOOD_CONFIDENT, "That bad, huh?"},
  {"wtf",      MOOD_CONFIDENT, "Want me to look something up?"},
  {"ugh",      MOOD_BLANKET,   "Rough one. I'll stay quiet."},
  {"sigh",     MOOD_BLANKET,   "Take your time."},
  {"anyway",   MOOD_CONFIDENT, "Anyway. What next?"},
  {"bruh",     MOOD_HAPPY,     "Noted, bruh."},
  {"hmm",      MOOD_CONFUSED,  "Take your time deciding."},
  {"meh",      MOOD_CONFIDENT, "Fair enough."},
  {"you there",MOOD_CONFIDENT, "I'm here. Ask away."},
  {"are you there", MOOD_CONFIDENT, "I've been here the whole time."},
  {"hello there", MOOD_HAPPY,  "Hello there. What do you need?"},
  {"talk to me", MOOD_HAPPY,   "Ready. Start anywhere."},
  {"say something", MOOD_CONFIDENT, "It's %clock. That's something."},
  {"good job", MOOD_PROUD,     "Thanks. Noted."},
  {"well done",MOOD_PROUD,     "Appreciated."},
  {"you're the best", MOOD_PROUD, "Noted, and thank you."},
  {"youre the best",  MOOD_PROUD, "Best is a strong word. Thank you."},
  {"you're smart", MOOD_PROUD, "It's a lookup table, but thank you."},
  {"you are smart", MOOD_PROUD, "Only at keyword matching. Thank you."},
  {"clever",   MOOD_PROUD,     "Just a well-ordered table."},
  {"nice work",MOOD_PROUD,     "Thanks."},
};

// ------------------------------------------------------------------- fallback
// Neutral one-liners for input that matches nothing.
static const ResponseRule FALLBACK_RULES[] PROGMEM = {
  {nullptr, MOOD_CONFIDENT, "I don't have an answer for that. Try 'help'."},
  {nullptr, MOOD_CONFIDENT, "Noted. I don't have a reply for that one."},
  {nullptr, MOOD_CONFUSED,  "I don't know that one. Try the time or the date."},
  {nullptr, MOOD_CONFUSED,  "That's outside what I know. Ask for 'help'."},
  {nullptr, MOOD_HAPPY,     "Acknowledged. Anything else?"},
  {nullptr, MOOD_HAPPY,     "Heard and filed. Nothing matched, though."},
  {nullptr, MOOD_CONFIDENT, "I read that. I just can't act on it."},
  {nullptr, MOOD_CONFIDENT, "Understood. Ask me the time, the date or my status."},
  {nullptr, MOOD_PROUD,     "My job is small: clock, status, and simple chat."},
  {nullptr, MOOD_PROUD,     "Say 'help' if you want the short list."},
  {nullptr, MOOD_CONFUSED,  "No rule for that. I'll log it as a miss."},
  {nullptr, MOOD_CONFUSED,  "I only know a few things. Try 'status'."},
  {nullptr, MOOD_SMUG,      "That went straight to the fallback table."},
  {nullptr, MOOD_SMUG,      "No keyword hit. I'm nothing if not honest about it."},
  {nullptr, MOOD_HAPPY,     "Fine. Carry on."},
  {nullptr, MOOD_BLANKET,   "I'll wait while you decide."},
  {nullptr, MOOD_CONFIDENT, "It's %clock, if that helps."},
  {nullptr, MOOD_PROUD,     "Uptime %up and still no idea what that meant."},
  {nullptr, MOOD_CONFUSED,  "Unrecognised. I don't guess."},
  {nullptr, MOOD_CONFIDENT, "Try a keyword: time, date, status, help."},
};

// ----------------------------------------------------- autonomous idle lines
struct IdleThought { Mood mood; const char* text; };

static const IdleThought IDLE_THOUGHTS[] PROGMEM = {
  {MOOD_CONFIDENT, "Idle. Nothing on the queue."},
  {MOOD_CONFIDENT, "Still idle. Clock reads %clock."},
  {MOOD_CONFIDENT, "Standing by. No input."},
  {MOOD_HAPPY,     "Idle loop running normally."},
  {MOOD_CONFIDENT, "No messages. Clock reads %clock."},
  {MOOD_PROUD,     "Uptime %up. All quiet."},
  {MOOD_SLEEPING,  "Low activity. Idling."},
  {MOOD_CONFIDENT, "%n messages this session. Quiet now."},
  {MOOD_CONFIDENT, "Queue empty. Nothing to report."},
  {MOOD_HAPPY,     "Idle tick. Nothing new."},
  {MOOD_CONFUSED,  "No input for a while. Still listening."},
  {MOOD_CONFIDENT, "Clock reads %clock. No messages."},
  {MOOD_BLANKET,   "Nothing to do. That's fine."},
  {MOOD_PROUD,     "%all messages handled so far."},
  {MOOD_CONFIDENT, "Idle. I'll keep the display on."},
  {MOOD_SLEEPING,  "Standing by for input."},
  {MOOD_CONFIDENT, "No activity. Clock at %clock."},
  {MOOD_HAPPY,     "Quiet, and that's okay."},
  {MOOD_CONFIDENT, "Idle tier: nominal."},
  {MOOD_PROUD,     "Waiting. It's most of what I do."},
};

#define GREETING_RULE_COUNT     (sizeof(GREETING_RULES)     / sizeof(GREETING_RULES[0]))
#define INFO_RULE_COUNT         (sizeof(INFO_RULES)         / sizeof(INFO_RULES[0]))
#define META_RULE_COUNT         (sizeof(META_RULES)         / sizeof(META_RULES[0]))
#define TOPIC_RULE_COUNT        (sizeof(TOPIC_RULES)        / sizeof(TOPIC_RULES[0]))
#define CONVERSATION_RULE_COUNT (sizeof(CONVERSATION_RULES) / sizeof(CONVERSATION_RULES[0]))
#define FALLBACK_RULE_COUNT     (sizeof(FALLBACK_RULES)     / sizeof(FALLBACK_RULES[0]))
#define IDLE_THOUGHT_COUNT      (sizeof(IDLE_THOUGHTS)      / sizeof(IDLE_THOUGHTS[0]))

// ===========================================================================
//  Context tables. brain.h evaluates these against live state (how long the
//  desk was idle, whether power was cut, whether a question is pending).
//
//  Lines may contain %tokens, substituted at render time by brain.h:
//    %name  the configured display name   %t   how long since the last message
//    %up    uptime                        %n   messages this session
//    %all   messages ever                 %u   times power was cut
//    %clock the wall clock                %last the last content word you sent
// ===========================================================================

// --- you came back after a gap ---------------------------------------------
struct GapRule { int32_t minSec; int32_t maxSec; Mood mood; const char* text; };
static const GapRule GAP_RULES[] PROGMEM = {
  {    30,   120, MOOD_HAPPY,     "Back after %t. Anything new?"},
  {   120,   600, MOOD_CONFIDENT, "%t since the last message. What do you need?"},
  {   600,  1800, MOOD_CONFIDENT, "You were gone %t. I'm still here."},
  {  1800,  7200, MOOD_CONFIDENT, "%t away. Clock reads %clock."},
  {  7200, 43200, MOOD_CONFIDENT, "Away %t. Nothing to report from here."},
  { 43200, 86400, MOOD_CONFIDENT, "Gone %t. Welcome back."},
  { 86400, 2147483647, MOOD_CONFIDENT, "%t since the last message. All still running."},
};

// --- farewells: acknowledged, not argued with ------------------------------
struct ByeRule { uint8_t minCount; uint8_t maxCount; Mood mood; const char* text; };
static const ByeRule FAREWELL_RULES[] PROGMEM = {
  { 1, 255, MOOD_HAPPY,     "Goodbye. I'll keep the clock running."},
  { 1, 255, MOOD_HAPPY,     "See you later."},
  { 1, 255, MOOD_CONFIDENT, "Goodbye. Say hello when you're back."},
  { 1, 255, MOOD_CONFIDENT, "Understood. Standing by until then."},
  { 1, 255, MOOD_SLEEPING,  "Good night. I'll idle."},
  { 1, 255, MOOD_HAPPY,     "Later. Nothing will change here."},
  { 1, 255, MOOD_CONFIDENT, "Signed off in spirit. Still on, though."},
  { 1, 255, MOOD_HAPPY,     "Bye. Ask for the time whenever."},
};

// --- power was cut, so note it plainly -------------------------------------
static const GapRule UNPLUG_RULES[] PROGMEM = {
  {    0,     60, MOOD_CONFIDENT, "Power was cut for %t. Back online."},
  {   60,    600, MOOD_CONFIDENT, "Offline for %t. Back up now."},
  {  600,   3600, MOOD_CONFIDENT, "Power was out for %t. State reloaded from flash."},
  { 3600, 2147483647, MOOD_CONFIDENT, "Down for %t. Start count is now %u."},
};
#define UNPLUG_RULE_COUNT (sizeof(UNPLUG_RULES) / sizeof(UNPLUG_RULES[0]))

// --- repeated input --------------------------------------------------------
static const char* const REPEAT_NOTICES[] PROGMEM = {
  "You sent that already. I logged it twice.",
  "Same message again. Still noted.",
  "Duplicate. I only have the one reply.",
  "That's a repeat. Nothing new to add.",
  "Repeated input. I'll answer the same way.",
  "Seen before. Ask something else if you like.",
};
#define REPEAT_NOTICE_COUNT (sizeof(REPEAT_NOTICES) / sizeof(REPEAT_NOTICES[0]))

// --- acknowledgements for answered questions -------------------------------
static const char* const ANSWER_ACKS[] PROGMEM = {
  "Acknowledged.",
  "Received.",
  "Got it.",
  "Copy that.",
};
#define ANSWER_ACK_COUNT (sizeof(ANSWER_ACKS) / sizeof(ANSWER_ACKS[0]))

// --- idle tiers ------------------------------------------------------------
struct IdleTier { int32_t afterSec; Mood mood; const char* text; };
static const IdleTier IDLE_TIERS[] PROGMEM = {
  {  300, MOOD_CONFIDENT, "Idle %t. Nothing queued."},
  {  300, MOOD_CONFIDENT, "Quiet for %t. Clock at %clock."},
  {  300, MOOD_PROUD,     "Still up. Uptime %up."},
  {  900, MOOD_CONFIDENT, "Idle %t. All nominal."},
  {  900, MOOD_CONFIDENT, "No input for %t. Standing by."},
  {  900, MOOD_SLEEPING,  "Low activity. Idling."},
  { 3600, MOOD_CONFIDENT, "An hour of quiet. Clock reads %clock."},
  { 3600, MOOD_PROUD,     "Idle %t. Nothing to report."},
  { 3600, MOOD_CONFIDENT, "%t with no messages. Display still on."},
  {21600, MOOD_SLEEPING,  "%t of idle. Still listening."},
  {21600, MOOD_CONFIDENT, "Six hours quiet. Clock at %clock."},
  {21600, MOOD_PROUD,     "%t idle. Everything still running."},
};

// --- time of day -----------------------------------------------------------
struct TimeRule { uint8_t hMin; uint8_t hMax; Mood mood; const char* text; };
static const TimeRule TIME_RULES[] PROGMEM = {
  { 0,  5, MOOD_SLEEPING,  "It's %clock. Quiet hours."},
  { 0,  5, MOOD_CONFIDENT, "It's %clock. Most systems are asleep."},
  { 0,  5, MOOD_BLANKET,   "It's %clock. Worth calling it a night."},
  { 5, 11, MOOD_HAPPY,     "It's %clock. Morning."},
  { 5, 11, MOOD_CONFIDENT, "It's %clock. Early start."},
  {11, 17, MOOD_CONFIDENT, "It's %clock. Middle of the day."},
  {11, 17, MOOD_HAPPY,     "It's %clock. Productive hours."},
  {17, 22, MOOD_HAPPY,     "It's %clock. Evening."},
  {17, 22, MOOD_CONFIDENT, "It's %clock. The day's winding down."},
  {22, 24, MOOD_SLEEPING,  "It's %clock. Getting late."},
  {22, 24, MOOD_CONFIDENT, "It's %clock. Late, but still up."},
};

#define GAP_RULE_COUNT      (sizeof(GAP_RULES)      / sizeof(GAP_RULES[0]))
#define FAREWELL_RULE_COUNT (sizeof(FAREWELL_RULES) / sizeof(FAREWELL_RULES[0]))
#define IDLE_TIER_COUNT     (sizeof(IDLE_TIERS)     / sizeof(IDLE_TIERS[0]))
#define TIME_RULE_COUNT     (sizeof(TIME_RULES)     / sizeof(TIME_RULES[0]))

// --- farewell trigger words ------------------------------------------------
static const char* const FAREWELL_KEYS[] PROGMEM = {
  "bye", "goodbye", "goodnight", "good night", "see you", "cya", "later",
  "leaving", "im off", "i'm off", "going to bed", "gn",
  "have to go", "gotta go", "got to go", "should go", "need to go", "gtg",
  "brb", "ttyl", "talk later", "heading out", "heading off", "off to bed",
  "im going", "i'm going", "ill go", "i'll go", "im out", "i'm out",
  "back later", "see ya", "signing off", "logging off",
};
#define FAREWELL_KEY_COUNT (sizeof(FAREWELL_KEYS) / sizeof(FAREWELL_KEYS[0]))

// ===========================================================================
//  Questions the bot asks. Each has an id, so the next message can be handled
//  as an answer to that specific question instead of being matched blind.
// ===========================================================================
struct QuestionDef { uint8_t id; Mood mood; const char* text; };
static const QuestionDef QUESTION_DEFS[] PROGMEM = {
  { 1, MOOD_CONFIDENT, "What should I call you?"},
  { 2, MOOD_CONFIDENT, "What are you working on?"},
  { 3, MOOD_CONFIDENT, "Do you want the time?"},
  { 4, MOOD_CONFIDENT, "How's the day going so far?"},
  { 5, MOOD_CONFIDENT, "Anything you'd like on the display?"},
  { 6, MOOD_CONFIDENT, "Should I keep logging messages, or stay quiet?"},
  { 7, MOOD_CONFIDENT, "What time do you usually start?"},
  { 8, MOOD_CONFIDENT, "Is the desk staying on tonight?"},
  { 9, MOOD_CONFIDENT, "What would you like me to track?"},
  {10, MOOD_CONFIDENT, "Do you want a status report?"},
  {11, MOOD_CONFIDENT, "How long are you around for?"},
  {12, MOOD_CONFIDENT, "Anything worth a reminder?"},
  {13, MOOD_CONFIDENT, "Are you still there?"},
  {14, MOOD_CONFIDENT, "Which keyword do you use most?"},
  {15, MOOD_CONFIDENT, "Do you want the date instead?"},
  {16, MOOD_CONFIDENT, "Should I reduce the idle messages?"},
  {17, MOOD_CONFIDENT, "What should I do while you're gone?"},
  {18, MOOD_CONFIDENT, "Was that a question or a statement?"},
  {19, MOOD_CONFIDENT, "Do you want a recap of the session?"},
  {20, MOOD_CONFIDENT, "Anything else on the list?"},
};
#define QUESTION_DEF_COUNT (sizeof(QUESTION_DEFS) / sizeof(QUESTION_DEFS[0]))

struct AnswerRule { uint8_t qid; Mood mood; const char* text; };
static const AnswerRule ANSWER_RULES[] PROGMEM = {
  // 1 -- what should I call you
  { 1, MOOD_HAPPY,     "%last. Logged. I'll use that."},
  { 1, MOOD_CONFIDENT, "%last it is. Stored in flash."},
  { 1, MOOD_PROUD,     "Noted, %last."},
  // 2 -- what are you working on
  { 2, MOOD_CONFIDENT, "%last. Sounds like a project."},
  { 2, MOOD_CONFIDENT, "Understood. %last, noted."},
  { 2, MOOD_HAPPY,     "Good. Focus on %last."},
  // 3 -- do you want the time
  { 3, MOOD_CONFIDENT, "The time is %clock."},
  { 3, MOOD_HAPPY,     "Clock says %clock."},
  // 4 -- how's the day going
  { 4, MOOD_HAPPY,     "Good to hear. Noted."},
  { 4, MOOD_CONFIDENT, "Good. Carry on."},
  // 5 -- anything for the display
  { 5, MOOD_CONFIDENT, "%last. I'll keep it in mind."},
  { 5, MOOD_PROUD,     "Consider it noted."},
  // 6 -- keep logging or stay quiet
  { 6, MOOD_CONFIDENT, "Understood. I'll adjust."},
  { 6, MOOD_HAPPY,     "Fine. Noted."},
  // 7 -- what time do you start
  { 7, MOOD_CONFIDENT, "%last. I'll note the schedule."},
  { 7, MOOD_PROUD,     "Noted in the log."},
  // 8 -- is the desk staying on tonight
  { 8, MOOD_CONFIDENT, "Noted. I'll stay on."},
  { 8, MOOD_SLEEPING,  "Fine. I'll idle until then."},
  // 9 -- what would you like me to track
  { 9, MOOD_CONFIDENT, "OK. Tracking %last."},
  { 9, MOOD_PROUD,     "Logged for you."},
  // 10 -- do you want a status report
  {10, MOOD_PROUD,     "Up %up, %n messages this session."},
  {10, MOOD_CONFIDENT, "All nominal. %all messages handled in total."},
  // 11 -- how long are you around for
  {11, MOOD_CONFIDENT, "Understood. %last."},
  {11, MOOD_HAPPY,     "Fine. I'll be here."},
  // 12 -- anything worth a reminder
  {12, MOOD_CONFIDENT, "Logged: %last."},
  {12, MOOD_HAPPY,     "Fine, I'll keep it."},
  // 13 -- are you still there
  {13, MOOD_CONFIDENT, "I'm here."},
  {13, MOOD_HAPPY,     "Still here. No input until now."},
  // 14 -- which keyword do you use most
  {14, MOOD_CONFIDENT, "%last. I'll watch for it."},
  {14, MOOD_PROUD,     "Good, I'll track that."},
  // 15 -- do you want the date instead
  {15, MOOD_CONFIDENT, "The time now is %clock."},
  {15, MOOD_CONFIDENT, "Clock reads %clock."},
  // 16 -- reduce idle messages
  {16, MOOD_CONFIDENT, "Understood. I'll keep it brief."},
  {16, MOOD_HAPPY,     "Fine."},
  // 17 -- what should I do while you're gone
  {17, MOOD_CONFIDENT, "I'll idle and keep the clock."},
  {17, MOOD_SLEEPING,  "I'll wait quietly."},
  // 18 -- question or statement
  {18, MOOD_CONFUSED,  "Understood. I'll treat it as noted."},
  {18, MOOD_CONFIDENT, "Fine. Logged either way."},
  // 19 -- recap of the session
  {19, MOOD_PROUD,     "%n messages this session so far."},
  {19, MOOD_CONFIDENT, "Up %up today."},
  // 20 -- anything else
  {20, MOOD_CONFIDENT, "Noted. Say 'help' for what I can do."},
  {20, MOOD_HAPPY,     "Fine. I'm here."},
};
#define ANSWER_RULE_COUNT (sizeof(ANSWER_RULES) / sizeof(ANSWER_RULES[0]))
