// Minimal Arduino shim so responses.h / sprites.h / brain.h compile on the host.
//
// brain.h additionally uses String, millis() and Preferences (see Preferences.h
// in this directory), none of which exist in the host stdlib, so this supplies
// just enough of each to exercise the whole reply pipeline in a test.
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <string>
#include <cctype>
#define PROGMEM

// --------------------------------------------------------------------- String
// Enough of Arduino's String to run the pipeline, backed by std::string.
class String {
public:
  String() {}
  String(const char* s) : s_(s ? s : "") {}
  String(const std::string& s) : s_(s) {}
  String(const String& o) : s_(o.s_) {}
  explicit String(int v)           { char b[24]; snprintf(b, sizeof(b), "%d", v); s_ = b; }
  explicit String(unsigned v)      { char b[24]; snprintf(b, sizeof(b), "%u", v); s_ = b; }
  explicit String(unsigned long v) { char b[24]; snprintf(b, sizeof(b), "%lu", v); s_ = b; }

  String& operator=(const char* s)   { s_ = s ? s : ""; return *this; }
  String& operator=(const String& o) { s_ = o.s_; return *this; }

  String& operator+=(const String& o) { s_ += o.s_; return *this; }
  String& operator+=(const char* s)   { if (s) s_ += s; return *this; }
  String& operator+=(char c)          { s_ += c; return *this; }

  size_t      length()  const { return s_.size(); }
  const char* c_str()   const { return s_.c_str(); }
  bool        isEmpty() const { return s_.empty(); }
  char        operator[](size_t i) const { return s_[i]; }

  String substring(size_t from) const {
    return from >= s_.size() ? String() : String(s_.substr(from));
  }
  String substring(size_t from, size_t to) const {
    if (from >= s_.size()) return String();
    if (to > s_.size()) to = s_.size();
    return String(s_.substr(from, to - from));
  }
  int indexOf(const char* needle) const {
    size_t p = s_.find(needle ? needle : "");
    return p == std::string::npos ? -1 : (int)p;
  }

  bool operator==(const String& o) const { return s_ == o.s_; }
  bool operator!=(const String& o) const { return s_ != o.s_; }
  bool operator==(const char* o)   const { return s_ == (o ? o : ""); }
  bool operator!=(const char* o)   const { return s_ != (o ? o : ""); }

  const std::string& std_str() const { return s_; }
private:
  std::string s_;
};

inline String operator+(const String& a, const String& b) { String r(a); r += b; return r; }
inline String operator+(const String& a, const char* b)   { String r(a); r += b; return r; }
inline String operator+(const char* a, const String& b)   { String r(a); r += b; return r; }

// ------------------------------------------------------------------- millis()
// The test drives this directly, so gap-based behaviour is reproducible.
extern unsigned long g_fakeMillis;
inline unsigned long millis() { return g_fakeMillis; }

