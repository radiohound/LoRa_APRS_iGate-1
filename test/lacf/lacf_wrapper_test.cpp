// Host test of the LACF wrapper (lib/HybridFrame) against the spec 11 vectors.   sh test/lacf/run.sh
#include "hybrid_frame.h"
#include "vectors.h"
#include <cstdio>
#include <string>
#include <vector>

static int fails = 0, checks = 0;
#define EXPECT(name, cond) do { checks++; if (!(cond)) { fails++; printf("FAIL: %s\n", name); } } while (0)

static std::vector<uint8_t> hexBytes(const char* h) {
    std::vector<uint8_t> b;
    for (size_t i = 0; h[i] && h[i + 1]; i += 2) b.push_back((uint8_t)std::stoi(std::string(h + i, 2), nullptr, 16));
    return b;
}
static std::string hexText(const char* h) { auto b = hexBytes(h); return std::string(b.begin(), b.end()); }

int main() {
    for (const HbVec& v : HB_VECTORS) {
        std::string tnc2 = hexText(v.tnc2), back;
        std::vector<uint8_t> frame;
        EXPECT("encode", HYBRID_FRAME::encode(tnc2, frame));
        EXPECT("encode bytes", frame == hexBytes(v.frame));
        EXPECT("decode", HYBRID_FRAME::decode(frame.data(), frame.size(), back) && back == tnc2);
        EXPECT("is hybrid", HYBRID_FRAME::isHybrid(frame.data(), frame.size()));
    }
    for (const HbVec& v : HB_RELAYED) {                                // frames that carry RXT tuples: decoded, tuples counted and dropped
        std::vector<uint8_t> frame = hexBytes(v.frame);
        std::string back; int tuples = 0;
        EXPECT("relayed decode", HYBRID_FRAME::decode(frame.data(), frame.size(), back, &tuples) && back == hexText(v.tnc2));
        EXPECT("relayed carries tuples", tuples >= 1);
    }
    for (const char* h : HB_REJECT) {
        std::vector<uint8_t> frame = hexBytes(h); std::string back;
        EXPECT("reject", !HYBRID_FRAME::decode(frame.data(), frame.size(), back));
    }
    for (const char* h : HB_NOENC) {                                   // packets that must not encode (non-compliant WIDE path, ':' in the source, no hybrid form)
        std::string tnc2 = hexText(h); std::vector<uint8_t> f;
        EXPECT("not encodable", !HYBRID_FRAME::encode(tnc2, f));
    }
    // non-compliant WIDE paths are dropped: not encoded, and pathOk says no
    const char* bad[] = { "N0CALL>APLRG1,WIDE1-2:>x", "N0CALL>APLRG1,WIDE2-3:>x", "N0CALL>APLRG1,WIDE8-1:>x", "N0CALL>APLRG1,WIDE1-0:>x" };
    for (const char* p : bad) { std::vector<uint8_t> f; EXPECT("bad path not encoded", !HYBRID_FRAME::encode(p, f)); EXPECT("bad path", !HYBRID_FRAME::pathOk(p)); }
    const char* good[] = { "N0CALL>APLRG1,WIDE1-1:>x", "N0CALL>APLRG1,WIDE2:>x", "N0CALL>APLRG1,WIDE3-3*:>x", "N0CALL>APLRG1:>x" };
    for (const char* p : good) { std::vector<uint8_t> f; EXPECT("good path encodes", HYBRID_FRAME::encode(p, f)); EXPECT("good path", HYBRID_FRAME::pathOk(p)); }
    uint8_t k1[6] = { 0x3C, 0xFF, 0x02, 0x01, 0, 0 }, k3[6] = { 0x3C, 0xFF, 0x02, 0x03, 0, 0 }, t[6] = { 0x3C, 0xFF, 0x01, 0x02, 0, 0 };
    EXPECT("kind 1 is not LACF", !HYBRID_FRAME::isHybrid(k1, 6));
    EXPECT("kind 3 is LACF", HYBRID_FRAME::isHybrid(k3, 6));
    EXPECT("plain is not LACF", !HYBRID_FRAME::isHybrid(t, 6));
    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
