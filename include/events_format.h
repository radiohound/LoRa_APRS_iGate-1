// Formats the LoRa APRS event lines (protocol "lora-aprs-events") that the iGate sends to lora-aprs-collector.
// Same fields as Dire Wolf's LORAEVENTS (feature/lora-events, doc/LoRa-Events.md), so one collector reads both.
// Plain C++ with no Arduino types, so the host test can run it.

#ifndef EVENTS_FORMAT_H_
#define EVENTS_FORMAT_H_

#include <string>
#include <stdint.h>

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

    struct Counts {
        long received[NCLASSES];
        long droppedNoReference;
        long parseFailed;
        long crcErrors;
    };

    const char* className(int frameClass);
    std::string rx(const Head& h, const Radio& r, int rssiDbm, float snrDb, int freqErrorHz,
                   int frameClass, int airBytes, int plainBytes, const std::string& tnc2);
    std::string tx(const Head& h, const Radio& r, int frameClass, int airBytes, bool ok);
    std::string status(const Head& h, long uptimeS, const Counts& c);

}

#endif
