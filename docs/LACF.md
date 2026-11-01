# LACF, the LoRa APRS Compact Frame (experimental)

LACF is the native frame `3C FF 02` with a PPM-coded content field, as written down in spec 11 of the APRS-PPM proposal
(`radiohound/aprs-compression-proposal`, a draft, not agreed with the people who maintain the native frame). The frame code in
`lib/HybridFrame` is the C reference of that spec, unchanged; `hybrid_frame.cpp` is the glue.

The same format runs on the T-Deck tracker (branch `hybrid` of the tracker fork) and Dire Wolf (branch `feature/lora-hybrid`).

## The setting

On the web page, **Digipeating**, last row: **Tx frame format**, with the same two choices as on the T-Deck:

* **Text (standard)**: what the station sends today (or the older compressed text, if `compressTx` is on in the config file).
* **Hybrid RXT Compression (LACF, test only)**: every frame the station sends, beacons, digipeated packets and the rest, goes out as an LACF frame
  when the packet can be encoded, otherwise as text. Other firmware ignores these frames.

Stored as `lora.txFormat` in the config (0 or 3; any other value reads as 0).

## Receiving

LACF frames are always decoded, whatever the setting, and expanded to the text packet the rest of the firmware already handles (so they are gated to
APRS-IS and digipeated like any other). The serial log says `LACF frame: N B on air, M B expanded`. This firmware has no RXT: a received RXT block
is counted and dropped. A frame that fails the receiver rules of the spec is dropped.

## Non-compliant WIDE paths

A path element shaped like `WIDEn-N` must follow the APRS rule (n 1 to 7, N 1 to n). A packet whose path holds one that does not (`WIDE1-2`,
`WIDE2-3`) is dropped and **not repeated on RF in any form**, in any Tx frame format. See spec 11, "Non-compliant WIDE paths", for the reasons.

## Cost

On a TTGO LoRa32 V2.1 the build uses 1,783,457 bytes of the 1,966,080-byte slot (90.7%). That fits because `btStop()` was removed from
`wifi_utils.cpp`: nothing here uses Bluetooth, and the call alone makes the Arduino core link about 128 KB of Bluetooth support (measured by F4MLV, and
here: 88.3% before, 81.8% after). The frame code runs in a task of its own with a 16 KB stack, created at the first use.

## Tests

`sh test/lacf/run.sh` builds the wrapper and the frame code on the host and checks them against 40 spec vectors, 35 frames with RXT tuples, 28 rejection
vectors and the WIDE rule (277 checks). Both `ttgo-lora32-v21` and `ESP32C3_DIY_1W_LoRa_915_E22_900M30S` build.

## Not done

* Not flashed to a board, and not tried on the air.
* No RXT: the iGate does not add its own measurement when it digipeats an LACF frame.
* No feed of frame information to a collector (planned as a separate, optional MQTT setting).
