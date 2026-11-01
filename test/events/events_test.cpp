// Prints a few event lines (NDJSON) made by EventsFormat; check.py feeds them to the collector.
#include <stdio.h>
#include "events_format.h"

int main() {
    EventsFormat::Head h = { "a1b2c3d4", 1, 1793545200123LL, "K6ATV-14" };
    EventsFormat::Radio r = { 433775000, 125000, 12, 5 };
    EventsFormat::Counts c = { { 5, 2, 1, 0, 0, 3 }, 0, 0, 4 };

    printf("%s\n", EventsFormat::rx(h, r, -107, -19.75f, -4331, EventsFormat::PLAIN, 57, 57, "K6ATV-10>APDW19:!3701.16NL12137.08W&K6ATV-10 LoRa APRS").c_str());
    h.sequence = 2;
    printf("%s\n", EventsFormat::rx(h, r, -90, 5.5f, 120, EventsFormat::HYBRID, 39, 57, "K6ATV-14>APLRT1,N0RLY-1*,WIDE2-1:=3701.16N/12137.08W-quote\" back\\ \x01 \xe9").c_str());
    h.sequence = 3;
    printf("%s\n", EventsFormat::rx(h, r, -100, 0.0f, 0, EventsFormat::HYBRID, 30, 45, "K6ATV-4>APLRT1,WIDE1-1:=hello").c_str());
    h.sequence = 4;
    printf("%s\n", EventsFormat::tx(h, r, EventsFormat::HYBRID, 58, true).c_str());
    h.sequence = 5;
    printf("%s\n", EventsFormat::status(h, 3600, c).c_str());
    return 0;
}
