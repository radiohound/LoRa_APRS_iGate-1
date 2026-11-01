// Formats the LoRa APRS event lines (protocol "lora-aprs-events") that the iGate sends to lora-aprs-collector.
// Same fields as Dire Wolf's LORAEVENTS (feature/lora-events, doc/LoRa-Events.md), so one collector reads both.
// Plain C++ with no Arduino types, so the host test can run it.

#ifndef EVENTS_FORMAT_H_
#define EVENTS_FORMAT_H_

#include <string>
#include <stdint.h>
#include <stddef.h>

namespace EventsFormat {

    enum FrameClass { PLAIN = 0, PPM_FULL, PPM_PRIMED, NATIVE, UNKNOWN, HYBRID, NCLASSES };

    struct Head {
        std::string bootId;
        uint32_t    sequence;
        int64_t     timeMs;         // UTC, ms since 1970
        std::string station;        // this iGate's callsign
    };

    struct Radio {
        long freqHz;
        long bwHz;
        int  sf;
        int  cr;                    // coding rate denominator, 5..8
    };

    enum Outcome { OK = 0, DROPPED_NO_REFERENCE, PARSE_FAILED, CRC_ERROR };

    struct Noise {                  // the idle-channel RSSI of the last minute; samples == 0: not measured
        int    samples;
        double avgDbm;
        int    minDbm;
        int    maxDbm;
    };

    struct Counts {
        long received[NCLASSES];
        long droppedNoReference;
        long parseFailed;
        long crcErrors;
    };

    const char* className(int frameClass);
    int classify(const uint8_t* frame, size_t len);             // the frame class from the first bytes on the air, like Dire Wolf's lorae_classify
    std::string rx(const Head& h, const Radio& r, int rssiDbm, float snrDb, int freqErrorHz,
                   int frameClass, int airBytes, int plainBytes, const std::string& tnc2);
    // A frame that was heard but not decoded: outcome DROPPED_NO_REFERENCE, PARSE_FAILED or CRC_ERROR (then the raw bytes go along as hex).
    std::string rxUndecoded(const Head& h, const Radio& r, int rssiDbm, float snrDb, int freqErrorHz,
                            int frameClass, int outcome, int airBytes, const uint8_t* raw, size_t rawLen);
    std::string tx(const Head& h, const Radio& r, int frameClass, int airBytes, bool ok);
    std::string status(const Head& h, long uptimeS, const Counts& c, const Noise& n);

}

#endif
