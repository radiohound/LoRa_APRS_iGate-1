// Safe saving and loading of the settings file (/igate_conf.json).
//
// The old code opened the file for writing, which empties it at once, then wrote it without checking the result. A power loss or a failed write
// during a Save left a half file; the next boot could not read it, wrote the factory defaults over it, and the callsign and the WiFi networks were gone.
//
// Now a save goes through a temporary file and is verified before it replaces anything:
//     1. write  /igate_conf.json.tmp   and read it back, it must be complete and parse
//     2. move the current good file to  /igate_conf.json.bak
//     3. move the temporary file to     /igate_conf.json
// At every point in between, at least one complete configuration exists, and load() finds the newest one:
// the main file, else the temporary one (a save that stopped at step 3), else the .bak copy.
// A damaged file is never overwritten by a default: if nothing readable is left, the files stay as they are and the caller runs on defaults in memory only.
//
// The file system and the "is this a complete configuration" test are passed in, so the same code runs on the device (SPIFFS, ArduinoJson) and in
// the host test (an in-memory file system that can crash at any step).

#ifndef CONFIG_STORE_H_
#define CONFIG_STORE_H_

#include <string>

namespace ConfigStore {

    static const char* const CURRENT = "/igate_conf.json";
    static const char* const TMP     = "/igate_conf.json.tmp";
    static const char* const BAK     = "/igate_conf.json.bak";

    typedef bool (*Validator)(const std::string& text);         // true if the text is a complete, readable configuration

    // FS needs: bool exists(const char*), remove(const char*), rename(const char* from, const char* to) (the target must not exist),
    //           write(const char*, const std::string&) (creates or empties the file; true only if every byte was written),
    //           read(const char*, std::string&).

    template <class FS>
    bool commit(FS& fs, const std::string& json, Validator valid) {
        if (!valid(json)) return false;                         // never store something that could not be read back
        fs.remove(TMP);
        if (!fs.write(TMP, json)) { fs.remove(TMP); return false; }
        std::string back;
        if (!fs.read(TMP, back) || back != json) { fs.remove(TMP); return false; }      // what reached the flash is what we meant to write

        std::string cur;
        if (fs.exists(CURRENT)) {
            if (fs.read(CURRENT, cur) && valid(cur)) {          // keep the last good file
                fs.remove(BAK);
                if (!fs.rename(CURRENT, BAK)) { fs.remove(TMP); return false; }
            } else {
                fs.remove(CURRENT);                             // damaged: it must not replace a good .bak
            }
        }
        return fs.rename(TMP, CURRENT);                         // if this fails, TMP (new) and BAK (old) are still there, and load() finds them
    }

    enum Source { NONE, FROM_CURRENT, FROM_TMP, FROM_BAK };

    // The text of the newest complete configuration. anyFile tells whether some candidate file exists at all (even if it is unreadable).
    template <class FS>
    Source load(FS& fs, Validator valid, std::string& out, bool& anyFile) {
        const char* const names[3] = { CURRENT, TMP, BAK };
        const Source kinds[3]      = { FROM_CURRENT, FROM_TMP, FROM_BAK };
        anyFile = false;
        for (int i = 0; i < 3; i++) if (fs.exists(names[i])) anyFile = true;
        for (int i = 0; i < 3; i++) {
            std::string text;
            if (fs.exists(names[i]) && fs.read(names[i], text) && valid(text)) { out = text; return kinds[i]; }
        }
        return NONE;
    }

}

#endif
