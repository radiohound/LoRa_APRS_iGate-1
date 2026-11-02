# LACF, the LoRa APRS Compact Frame (experimental)

LACF is the native frame `3C FF 02` with a PPM-coded content field, as written down in spec 11 of the APRS-PPM proposal
(`radiohound/aprs-compression-proposal`, a draft in early development and still being reviewed for change). The frame code in
`lib/HybridFrame` is the C reference of that spec, unchanged; `hybrid_frame.cpp` is the glue.

The same format runs on the T-Deck tracker (branch `hybrid` of the tracker fork) and Dire Wolf (branch `feature/lora-hybrid`).

**Credit.** LACF is developed together with Fabrice Morel, F4MLV (moricef): it combines his native LoRa APRS frame and code with K6ATV's PPM compression, and the two are working on it together (issue 7 of moricef/LoRa_APRS_Tracker). RXT was created by Jon, N7UV. This firmware is a fork of Ricardo Guzman's (CA2RXU) LoRa APRS iGate.

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

The published build for the TTGO LoRa32 V2.1 (commit 88964d7) is 1,804,336 bytes of the 1,966,080-byte app slot (91.8%). Ricardo's `main` is 1,602,737 bytes (81.5%),
so LACF, the MQTT feed and the other additions of this branch cost about 330 KB. That fits because `btStop()` was removed from `wifi_utils.cpp`: nothing here uses
Bluetooth, and the call alone makes the Arduino core link about 128 KB of Bluetooth support (found by F4MLV; measured again on 1 November 2026 on `main`: 1,602,737
bytes (81.5%) became 1,474,561 (75.0%), and RAM 17.2% became 16.0%). The frame code runs in a task of its own with a 16 KB stack, created at the first use.

## Tests

Host tests (no board needed):

* `sh test/lacf/run.sh`: the wrapper and the frame code against 40 spec vectors, 35 frames with RXT tuples, 28 rejection vectors and the WIDE rule (277 checks).
* `sh test/events/run.sh`: the collector event lines, including the RFONLY/NOGATE privacy rule.
* `sh test/configstore/run.sh`: safe saving of the settings file (123 checks).
* `sh test/mqttdefaults/run.sh`: the pre-filled collector settings (10 cases).

Both `ttgo-lora32-v21` and `ESP32C3_DIY_1W_LoRa_915_E22_900M30S` build.

## Status

Flashed to a LoRa32 V2.1 and used on the air: the iGate sends and receives LACF frames next to ordinary LoRa APRS frames, and the T-Deck tracker and the Dire Wolf
station on the hill (both on the `hybrid` code) decode its frames and it decodes theirs. The browser **Update** on the published firmware page was tested on this board
and keeps the settings. LACF is still an experimental draft in early development, being worked out together with F4MLV; see the firmware page for how to try it.

## Not done

* No RXT: the iGate does not add its own measurement when it digipeats an LACF frame, and an RXT block it receives is counted and not carried on.
* Frames of kind 0 and 1 (plain text frames of the earlier drafts) are not handled; they are dropped.
