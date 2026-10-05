// Minimal Preferences (NVS) shim for host tests.
//
// Backed by an in-memory map, which is exactly what the brain needs: state that
// survives a "reboot" within one test run. The real thing is NVS on the ESP32.
#pragma once
#include <stdint.h>
#include <string>
#include <map>

class Preferences {
public:
  bool begin(const char* ns, bool readOnly = false) {
    (void)readOnly;
    ns_ = ns ? ns : "";
    return true;
  }
  void end() {}

  int32_t  getInt(const char* k, int32_t d = 0)   { return get(k, d); }
  uint32_t getUInt(const char* k, uint32_t d = 0) { return (uint32_t)get(k, (int32_t)d); }
  size_t   putInt(const char* k, int32_t v)   { store_[key(k)] = (int32_t)v; return 4; }
  size_t   putUInt(const char* k, uint32_t v) { store_[key(k)] = (int32_t)v; return 4; }

  String getString(const char* k, const String& d = String()) {
    auto it = strings_.find(key(k));
    return it == strings_.end() ? d : String(it->second);
  }
  size_t putString(const char* k, const String& v) {
    strings_[key(k)] = v.c_str();
    return v.length();
  }

  void clear() { store_.clear(); strings_.clear(); }

private:
  std::string key(const char* k) const { return ns_ + "/" + (k ? k : ""); }
  int32_t get(const char* k, int32_t d) {
    auto it = store_.find(key(k));
    return it == store_.end() ? d : it->second;
  }
  std::string ns_;
  std::map<std::string, int32_t>     store_;
  std::map<std::string, std::string> strings_;
};
