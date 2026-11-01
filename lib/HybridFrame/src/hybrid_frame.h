// LACF, the LoRa APRS Compact Frame (spec 11 of the APRS-PPM proposal): the native frame 3C FF 02 with kind 2 or 3 and a PPM-coded content field.
// The frame code (hb.c and the two table files) is the C reference of that spec, unchanged. This iGate build has no RXT: a received RXT block is
// counted and dropped.
#ifndef HYBRID_FRAME_H_
#define HYBRID_FRAME_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace HYBRID_FRAME {

    // True for 3C FF 02 with information kind 2 or 3.
    bool isHybrid(const uint8_t* data, size_t size);

    // TNC2 packet ("SRC>DST,PATH:info", no 3C FF 01 prefix) to an LACF frame. False when the packet cannot be encoded (the caller then sends it as text)
    // or its path holds a non-compliant WIDEn-N (the caller must then DROP the packet, see pathOk()).
    bool encode(const std::string& tnc2, std::vector<uint8_t>& out);

    // LACF frame to TNC2. False when the receiver rules drop the frame. rxtTuples, if given, gets the number of RXT tuples the frame carried.
    bool decode(const uint8_t* data, size_t size, std::string& tnc2, int* rxtTuples = nullptr);

    // False if the path holds a WIDEn-N that breaks the APRS rule (n 1 to 7, N 1 to n). Such a packet is dropped and not repeated on RF in any form.
    bool pathOk(const std::string& tnc2);

    // Gives back the 16 KB stack of the worker task until the next call needs it again.
    void shutdown();

}

#endif
