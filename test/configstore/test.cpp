// ConfigStore: a save that is cut short at ANY step must leave a complete configuration that load() can find.
#include <stdio.h>
#include <map>
#include <string>
#include <ArduinoJson.h>
#include "config_store.h"

struct Crash {};

// An in-memory file system that can lose power at a chosen step.
struct FakeFS {
    std::map<std::string, std::string> f;
    int ops = 0, crashAt = -1;
    int tear = 0;            // for a write that crashes: 0 = nothing written, 1 = half, 2 = all but the last byte
    bool doneBeforeCrash = false;   // for a rename/remove that crashes: was it carried out first?
    bool diskFull = false;
    bool exists(const char* p) { return f.count(p) > 0; }
    bool read(const char* p, std::string& o) { auto i = f.find(p); if (i == f.end()) return false; o = i->second; return true; }
    bool remove(const char* p) {
        if (ops++ == crashAt) { if (doneBeforeCrash) f.erase(p); throw Crash(); }
        return f.erase(p) > 0;
    }
    bool rename(const char* a, const char* b) {
        if (ops++ == crashAt) { if (doneBeforeCrash && f.count(a) && !f.count(b)) { f[b] = f[a]; f.erase(a); } throw Crash(); }
        if (!f.count(a) || f.count(b)) return false;      // like SPIFFS: the target must not exist
        f[b] = f[a]; f.erase(a); return true;
    }
    bool write(const char* p, const std::string& d) {
        if (ops++ == crashAt) { size_t n = tear == 0 ? 0 : tear == 1 ? d.size() / 2 : d.size() - 1; f[p] = d.substr(0, n); throw Crash(); }
        if (diskFull) { f[p] = d.substr(0, d.size() / 3); return false; }
        f[p] = d; return true;
    }
};

static bool valid(const std::string& s) {                  // the same test the firmware uses
    JsonDocument d;
    if (deserializeJson(d, s)) return false;
    return d["callsign"].is<const char*>() && d["wifi"].is<JsonObject>();
}
static std::string cfg(const char* call) { return std::string("{\"callsign\":\"") + call + "\",\"wifi\":{\"AP\":[{\"ssid\":\"net\",\"password\":\"pw\"}],\"autoAP\":{\"enabled\":true}}}"; }

static int bad = 0, cases = 0;
#define CHECK(c, msg) do { cases++; if (!(c)) { bad++; printf("FAIL: %s\n", msg); } } while (0)

int main() {
    const std::string OLD = cfg("OLD-1"), OLDER = cfg("OLDER-0"), NEWC = cfg("NEW-2");
    std::string trunc = OLD.substr(0, OLD.size() / 2);                       // a half-written file, as the field failure left it

    struct Start { const char* name; std::map<std::string, std::string> files; bool hadGood; std::string good; } starts[] = {
        { "main file only",                    { {ConfigStore::CURRENT, OLD} },                                              true,  OLD },
        { "main file and a .bak",              { {ConfigStore::CURRENT, OLD}, {ConfigStore::BAK, OLDER} },                   true,  OLD },
        { "main file damaged, good .bak",      { {ConfigStore::CURRENT, trunc}, {ConfigStore::BAK, OLD} },                   true,  OLD },
        { "first ever save (no files)",        { },                                                                          false, "" },
    };

    for (auto& st : starts) {
        // 1. a save that completes
        { FakeFS fs; fs.f = st.files; bool ok = ConfigStore::commit(fs, NEWC, valid); std::string out; bool any;
          CHECK(ok, "a normal save succeeds");
          CHECK(ConfigStore::load(fs, valid, out, any) == ConfigStore::FROM_CURRENT && out == NEWC, "after a normal save the main file is the new one");
          if (st.hadGood) { std::string b; CHECK(fs.read(ConfigStore::BAK, b) && b == st.good, "the previous good file is kept as .bak"); }
          CHECK(!fs.exists(ConfigStore::TMP), "no temporary file is left behind"); }
        // 2. a save that stops at every possible step, in every way a step can be torn
        int steps = 0; { FakeFS fs; fs.f = st.files; ConfigStore::commit(fs, NEWC, valid); steps = fs.ops; }
        for (int k = 0; k < steps; k++) for (int variant = 0; variant < 3; variant++) {
            FakeFS fs; fs.f = st.files; fs.crashAt = k; fs.tear = variant; fs.doneBeforeCrash = (variant != 0);
            try { ConfigStore::commit(fs, NEWC, valid); } catch (Crash&) {}
            fs.crashAt = -1;                                                    // power is back: reboot and load
            std::string out; bool any; ConfigStore::Source src = ConfigStore::load(fs, valid, out, any);
            char msg[160];
            snprintf(msg, sizeof msg, "[%s] crash at step %d/%d, variant %d", st.name, k, steps, variant);
            if (st.hadGood) { CHECK(src != ConfigStore::NONE && (out == st.good || out == NEWC), msg); }
            else            { CHECK(src == ConfigStore::NONE || out == NEWC, msg); }
            if (src != ConfigStore::NONE) CHECK(valid(out), "what is loaded is always a complete configuration");
        }
        // 3. the disk is full: the save fails, and the existing files are untouched
        { FakeFS fs; fs.f = st.files; fs.diskFull = true; auto before = fs.f; bool ok = ConfigStore::commit(fs, NEWC, valid);
          CHECK(!ok, "a save on a full disk reports failure");
          std::string out; bool any; ConfigStore::Source src = ConfigStore::load(fs, valid, out, any);
          if (st.hadGood) CHECK(src != ConfigStore::NONE && out == st.good, "a failed save leaves the good configuration in place");
          (void)before; }
    }

    // 4. nothing readable at all: load says so, and the files are NOT touched (the old code replaced them with defaults)
    { FakeFS fs; fs.f = { {ConfigStore::CURRENT, trunc}, {ConfigStore::BAK, trunc} }; auto before = fs.f; std::string out; bool any;
      CHECK(ConfigStore::load(fs, valid, out, any) == ConfigStore::NONE && any, "damaged files: nothing loaded, but they are reported as present");
      CHECK(fs.f == before, "damaged files are left exactly as they were"); }
    { FakeFS fs; std::string out; bool any = true; CHECK(ConfigStore::load(fs, valid, out, any) == ConfigStore::NONE && !any, "no files at all: reported as absent"); }
    // 5. preference order
    { FakeFS fs; fs.f = { {ConfigStore::TMP, NEWC}, {ConfigStore::BAK, OLD} }; std::string out; bool any;
      CHECK(ConfigStore::load(fs, valid, out, any) == ConfigStore::FROM_TMP && out == NEWC, "a finished temporary file beats the .bak (a save that stopped before the last rename)"); }
    // 6. something that is not a configuration is never stored
    { FakeFS fs; fs.f = { {ConfigStore::CURRENT, OLD} }; CHECK(!ConfigStore::commit(fs, "{\"callsign\":", valid) && fs.f[ConfigStore::CURRENT] == OLD, "an unreadable document is refused and the old file is kept"); }

    printf("%d checks, %d failed\n", cases, bad);
    return bad ? 1 : 0;
}
