/* Copyright (C) 2026 Ricardo Guzman - CA2RXU
 *
 * This file is part of LoRa APRS iGate.
 *
 * LoRa APRS iGate is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LoRa APRS iGate is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LoRa APRS iGate. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef LORA_UTILS_H_
#define LORA_UTILS_H_

#include <Arduino.h>
#include "events_format.h"


namespace LoRa_Utils {

    int     validPower(int requested);
    void    setup();
    void    sendNewPacket(const String& newPacket);
    String  receivePacketFromSleep();
    String  receivePacket();
    void    changeFreqTx();
    void    changeFreqRx();
    void    sampleNoise();                          // idle-channel RSSI, call about once a second (only for the collector feed)
    bool    takeNoiseFloor(EventsFormat::Noise& n); // the last minute of samples, then starts over; false if there were none
    void    wakeRadio();
    void    sleepRadio();

    // Used by the optional over-the-air self-test (APRS_PPM_SELFTEST):
    bool    lastRxWasPrimed();              // the last received frame was a primed frame
    void    setForceCompress(int mode);     // -1 = follow lora.compressTx, 0 = plain, 1 = compressed
    bool    lastRxWasCompressed();
    bool    lastRxWasHybrid();              // the last received frame was an LACF frame (Hybrid RXT Compression)
    size_t  lastRxAirBytes();

}

#endif