#include <stdarg.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <time.h>
#include <vector>
#include "events_format.h"


namespace EventsFormat {

    static const char* const CLASS_NAMES[NCLASSES] = { "plain", "ppm_full", "ppm_primed", "native", "unknown", "hybrid" };

    const char* className(int frameClass) {
        return (frameClass >= 0 && frameClass < NCLASSES) ? CLASS_NAMES[frameClass] : "unknown";
    }

    static const char* const OUTCOME_NAMES[] = { "ok", "dropped_no_reference", "parse_failed", "crc_error" };

    int classify(const uint8_t* f, size_t len) {
        if (len >= 3 && f[0] == 0x3C && f[1] == 0xFF) {
            switch (f[2]) {
                case 0x01: return PLAIN;
                case 0x81: return PPM_FULL;
                case 0xC1: return PPM_PRIMED;
                case 0x02: return (len >= 4 && (f[3] & 3) >= 2) ? HYBRID : NATIVE;
                default:   break;
            }
        }
        return UNKNOWN;
    }

    static void putf(std::string& out, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
    static void putf(std::string& out, const char* fmt, ...) {
        char tmp[160];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(tmp, sizeof tmp, fmt, ap);
        va_end(ap);
        out += tmp;
    }

    // JSON string; control bytes and bytes of 0x7F and above are written as \u00XX, like Dire Wolf does.
    static void putString(std::string& out, const std::string& s) {
        out += '"';
        for (unsigned char c : s) {
            if (c == '"')                   out += "\\\"";
            else if (c == '\\')             out += "\\\\";
            else if (c < 0x20 || c >= 0x7F) putf(out, "\\u%04x", c);
            else                            out += (char)c;
        }
        out += '"';
    }

    static void putBase64(std::string& out, const std::string& s) {
        static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        out += '"';
        size_t i = 0;
        for (; i + 2 < s.size(); i += 3) {
            uint32_t v = ((uint8_t)s[i] << 16) | ((uint8_t)s[i + 1] << 8) | (uint8_t)s[i + 2];
            out += T[v >> 18]; out += T[(v >> 12) & 63]; out += T[(v >> 6) & 63]; out += T[v & 63];
        }
        if (i + 1 == s.size()) {
            uint32_t v = (uint8_t)s[i] << 16;
            out += T[v >> 18]; out += T[(v >> 12) & 63]; out += "==";
        } else if (i + 2 == s.size()) {
            uint32_t v = ((uint8_t)s[i] << 16) | ((uint8_t)s[i + 1] << 8);
            out += T[v >> 18]; out += T[(v >> 12) & 63]; out += T[(v >> 6) & 63]; out += '=';
        }
        out += '"';
    }

    static void putIsoTime(std::string& out, int64_t ms) {
        time_t secs = (time_t)(ms / 1000);
        int msec = (int)(ms % 1000);
        struct tm t;
        gmtime_r(&secs, &t);
        putf(out, "\"%04d-%02d-%02dT%02d:%02d:%02d.%03dZ\"", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, msec);
    }

    static void putHead(std::string& out, const char* event, const Head& h) {
        out += "{\"protocol\":\"lora-aprs-events\",\"schema_version\":\"0.1\",\"event\":\"";
        out += event;
        out += "\",\"boot_id\":";
        putString(out, h.bootId);
        putf(out, ",\"sequence\":%lu,\"time_ms\":%lld,\"time\":", (unsigned long)h.sequence, (long long)h.timeMs);
        putIsoTime(out, h.timeMs);
        out += ",\"receiver\":{\"station\":";
        putString(out, h.station);
        out += ",\"interface\":\"igate-lora\"}";
    }

    static void putRadio(std::string& out, const Radio& r) {
        putf(out, "\"radio\":{\"frequency_hz\":%ld,\"bandwidth_hz\":%ld,\"spreading_factor\":%d,\"coding_rate\":\"4/%d\"}", r.freqHz, r.bwHz, r.sf, r.cr);
    }

    // ---- reading SRC>DEST,PATH1,PATH2*:info ----

    struct Header {
        std::string src, dst;
        std::vector<std::string> path;
    };

    static bool parseHeader(const std::string& t, Header& h) {
        size_t colon = t.find(':');
        size_t gt = t.find('>');
        if (colon == std::string::npos || gt == std::string::npos || gt > colon || gt == 0) return false;
        h.src = t.substr(0, gt);
        std::string rest = t.substr(gt + 1, colon - gt - 1);
        size_t pos = 0;
        bool first = true;
        while (pos <= rest.size()) {
            size_t comma = rest.find(',', pos);
            std::string el = rest.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
            if (first) { h.dst = el; first = false; } else if (!el.empty()) h.path.push_back(el);
            if (comma == std::string::npos) break;
            pos = comma + 1;
        }
        return !h.dst.empty();
    }

    static bool startsWithCi(const std::string& s, const char* p) {
        size_t n = strlen(p);
        return s.size() >= n && strncasecmp(s.c_str(), p, n) == 0;
    }

    static std::string stripStar(const std::string& s) {
        return (!s.empty() && s.back() == '*') ? s.substr(0, s.size() - 1) : s;
    }

    static bool isAlias(const std::string& s) {
        return startsWithCi(s, "WIDE") || startsWithCi(s, "RELAY") || startsWithCi(s, "TRACE") || startsWithCi(s, "ECHO") || startsWithCi(s, "TCPIP");
    }

    // The same rule the iGate applies before it uploads to APRS-IS (aprs_is_utils.cpp): the words RFONLY or NOGATE anywhere in the header, in any case,
    // so RFONLY-1 or a lower-case nogate also count. The header is everything before the first ':'. A packet with no ':' or no '>' cannot be read,
    // so it is treated as private too: when in doubt, nothing is published.
    bool privatePath(const std::string& tnc2) {
        size_t colon = tnc2.find(':');
        if (colon == std::string::npos || tnc2.find('>') == std::string::npos || tnc2.find('>') > colon) return true;
        std::string head = tnc2.substr(0, colon);
        for (char& c : head) if (c >= 'a' && c <= 'z') c -= 32;
        return head.find("RFONLY") != std::string::npos || head.find("NOGATE") != std::string::npos;
    }

    std::string rx(const Head& h, const Radio& r, int rssiDbm, float snrDb, int freqErrorHz,
                   int frameClass, int airBytes, int plainBytes, const std::string& tnc2) {
        std::string o;
        o.reserve(700 + 2 * tnc2.size());
        putHead(o, "rx", h);
        o += ",\"reception\":{\"crc_valid\":true,";
        putRadio(o, r);
        putf(o, ",\"local\":{\"rssi_dbm\":%d,\"snr_db\":%.1f,\"freq_error_hz\":%d}}", rssiDbm, snrDb, freqErrorHz);
        putf(o, ",\"frame\":{\"class\":\"%s\",\"air_bytes\":%d,\"plain_bytes\":%d,\"outcome\":\"ok\"}", className(frameClass), airBytes, plainBytes);

        o += ",\"packet\":{\"raw_tnc2_base64\":";
        putBase64(o, tnc2);
        o += ",\"tnc2\":";
        putString(o, tnc2);
        o += ",\"parse_status\":\"ok\"";
        Header p;
        if (parseHeader(tnc2, p)) {
            o += ",\"source\":";       putString(o, p.src);
            o += ",\"destination\":";  putString(o, p.dst);
            o += ",\"path\":[";
            const bool priv = privatePath(tnc2);
            int last = -1;
            for (size_t i = 0; i < p.path.size(); i++) {
                if (i) o += ',';
                putString(o, p.path[i]);
                if (!p.path[i].empty() && p.path[i].back() == '*') last = (int)i;
            }
            o += ']';
            std::string who = p.src;
            bool known = true, direct = false;
            int hops = 0;
            if (last < 0) {
                direct = true;
            } else {
                hops = last + 1;
                who = stripStar(p.path[last]);
                if (isAlias(who)) {                 // the starred element is an alias: look for the real digipeater before it
                    known = false;
                    for (int j = last - 1; j >= 0; j--) {
                        std::string nn = stripStar(p.path[j]);
                        if (!isAlias(nn)) { who = nn; known = true; break; }
                    }
                }
            }
            o += ",\"heard_from\":";
            if (known) putString(o, who); else o += "null";
            putf(o, ",\"direct\":%s,\"hops_used\":%d,\"private_path\":%s", direct ? "true" : "false", hops, priv ? "true" : "false");
        }
        o += "}}";
        return o;
    }

    std::string rxUndecoded(const Head& h, const Radio& r, int rssiDbm, float snrDb, int freqErrorHz,
                            int frameClass, int outcome, int airBytes, const uint8_t* raw, size_t rawLen) {
        std::string o;
        o.reserve(600 + 2 * rawLen);
        putHead(o, "rx", h);
        putf(o, ",\"reception\":{\"crc_valid\":%s,", outcome == CRC_ERROR ? "false" : "true");
        putRadio(o, r);
        putf(o, ",\"local\":{\"rssi_dbm\":%d,\"snr_db\":%.1f,\"freq_error_hz\":%d}}", rssiDbm, snrDb, freqErrorHz);
        putf(o, ",\"frame\":{\"class\":\"%s\",\"air_bytes\":%d,\"plain_bytes\":0,\"outcome\":\"%s\"",
             className(frameClass), airBytes, (outcome >= 0 && outcome < 4) ? OUTCOME_NAMES[outcome] : "parse_failed");
        if (outcome == CRC_ERROR && raw && rawLen > 0) {
            o += ",\"raw_hex\":\"";
            for (size_t i = 0; i < rawLen; i++) putf(o, "%02x", raw[i]);
            o += '"';
        }
        o += "},\"packet\":null}";
        return o;
    }

    std::string tx(const Head& h, const Radio& r, int frameClass, int airBytes, bool ok) {
        std::string o;
        putHead(o, "tx", h);
        o += ',';
        putRadio(o, r);
        putf(o, ",\"frame\":{\"class\":\"%s\",\"air_bytes\":%d,\"ok\":%s}}", className(frameClass), airBytes, ok ? "true" : "false");
        return o;
    }

    std::string status(const Head& h, long uptimeS, const Counts& c, const Noise& n) {
        std::string o;
        putHead(o, "status", h);
        putf(o, ",\"uptime_s\":%ld", uptimeS);
        if (n.samples > 0) putf(o, ",\"noise_floor\":{\"avg_dbm\":%.1f,\"min_dbm\":%d,\"max_dbm\":%d,\"samples\":%d}", n.avgDbm, n.minDbm, n.maxDbm, n.samples);
        else               o += ",\"noise_floor\":null";
        o += ",\"received\":{";
        for (int i = 0; i < NCLASSES; i++) putf(o, "%s\"%s\":%ld", i ? "," : "", className(i), c.received[i]);
        putf(o, ",\"dropped_no_reference\":%ld,\"parse_failed\":%ld},\"crc_errors\":%ld}", c.droppedNoReference, c.parseFailed, c.crcErrors);
        return o;
    }

}
