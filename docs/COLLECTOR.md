# Development frame info (lora-aprs-collector)

The MQTT section of the web page comes pre-filled for the LoRa APRS collector: server `mqtt.lora-aprs.org`, port 1884, topic `lora-aprs`.
Switch MQTT on and save: the user `igate` and password `lora-aprs` are public and already filled in.
The pre-fill also reaches a unit that already has a saved configuration, as long as MQTT was **never set up** there: MQTT off and the server, user name and
password all empty. The first boot of this firmware fills in the values above, saves, and restarts once. MQTT stays **off** until you switch it on.
A unit where anyone entered a server (their own broker, for instance), a user name or a password keeps its settings untouched.

* With that server the iGate sends **event lines** not the received packets: `lora-aprs/<callsign>/rx` for every frame heard, `/tx` for every
  frame sent, `/status` once a minute (frame counts per format, CRC errors, uptime). The fields are those of Dire Wolf's `LORAEVENTS`
  (`doc/LoRa-Events.md` in the `feature/lora-events` branch), with `frame.class` = `plain`, `ppm_full`, `ppm_primed` or `hybrid` (LACF).
* **No TLS.** The firmware has no room for it (it costs about 136 KB of flash). Port 1884 is an unencrypted listener with one shared, public, write-only login
  (it can write only under the client id, which is the callsign, and can read nothing). Anyone can therefore publish fake events under any callsign; that is accepted for
  a test network. The iGate does not verify the server.
* Nothing is subscribed to: the broker lets a station write only under its own name.
* With any other server the old behaviour is unchanged: the received TNC2 text goes to `<topic>/<sender>`, and the iGate subscribes to `<topic>/<callsign>/#`.
* Nothing is sent before the clock is set by NTP. A failed connection is retried every 15 seconds.

`sh test/events/run.sh` builds the formatter on the host and feeds its lines to the collector's own parser (`collector.add_event`).

Cost: a few KB of flash for the event code (about 91% of the slot on ttgo-lora32-v21).

## What is sent

* **rx** for every frame heard: RSSI, SNR, frequency error, frame class (`plain`, `ppm_full`, `ppm_primed`, `hybrid`), bytes on air and as plain text, and the decoded packet.
* **rx with an outcome** for a frame that was heard but not decoded, with `packet: null`: `crc_error` (`crc_valid` false, at most 30 a minute; the damaged bytes themselves are **not** sent, because a private packet whose RFONLY or NOGATE got damaged could no longer be recognised and would be published readable),
  `dropped_no_reference` (a primed frame with no earlier full one) or `parse_failed`. The same shape as Dire Wolf's `LORAEVENTS`.
* **tx** for every frame sent: class, bytes on air, whether the radio reported success.
* **status** once a minute: uptime, frames received per class, dropped / unparsable / CRC-failed counts, and the **noise floor** (power-averaged idle-channel RSSI,
  one sample a second, min and max). The noise floor is skipped while transmitting or in an eco mode (CAD does not matter); `rx_gain` is not sent (the SX127x has no switchable gain).

**Private packets.** A received packet is never sent if the words RFONLY or NOGATE appear anywhere in its header (before the first `:`), in any case, so `RFONLY-1` counts too: the same rule the iGate uses before it uploads to APRS-IS. A packet whose header cannot be read is not sent either. Such a packet is still counted in the status line.
