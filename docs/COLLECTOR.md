# Development frame info (lora-aprs-collector)

The MQTT section of the web page comes pre-filled for the LoRa APRS collector: server `mqtt.lora-aprs.org`, port 1884, topic `lora-aprs`.
Switch MQTT on and save: the user `igate` and password `lora-aprs` are public and already filled in.
(A unit that already has a saved configuration keeps its old MQTT values.)

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
* **rx with an outcome** for a frame that was heard but not decoded, with `packet: null`: `crc_error` (`crc_valid` false, the bytes as `raw_hex`, at most 30 a minute),
  `dropped_no_reference` (a primed frame with no earlier full one) or `parse_failed`. The same shape as Dire Wolf's `LORAEVENTS`.
* **tx** for every frame sent: class, bytes on air, whether the radio reported success.
* **status** once a minute: uptime, frames received per class, dropped / unparsable / CRC-failed counts, and the **noise floor** (power-averaged idle-channel RSSI,
  one sample a second, min and max). The noise floor is skipped while transmitting, with CAD on, or in an eco mode; `rx_gain` is not sent (the SX127x has no switchable gain).

