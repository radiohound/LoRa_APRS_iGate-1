# Development frame info (lora-aprs-collector)

The MQTT section of the web page comes pre-filled for the LoRa APRS collector: server `mqtt.lora-aprs.org`, port 1884, topic `lora-aprs`.
Switch MQTT on, type the password you were given (a login made with `add-station.sh NAME --plain` on the broker) and save. The user name is the callsign when it is left empty.
(A unit that already has a saved configuration keeps its old MQTT values.)

* With that server the iGate sends **event lines** not the received packets: `lora-aprs/<callsign>/rx` for every frame heard, `/tx` for every
  frame sent, `/status` once a minute (frame counts per format, CRC errors, uptime). The fields are those of Dire Wolf's `LORAEVENTS`
  (`doc/LoRa-Events.md` in the `feature/lora-events` branch), with `frame.class` = `plain`, `ppm_full`, `ppm_primed` or `hybrid` (LACF).
* **No TLS.** The firmware has no room for it (it costs about 136 KB of flash). Port 1884 is an unencrypted listener with its own write-only logins: a sniffed
  password lets someone publish fake events under that one callsign, nothing more. Use a password you use nowhere else. The iGate does not verify the server.
* Nothing is subscribed to: the broker lets a station write only under its own name.
* With any other server the old behaviour is unchanged: the received TNC2 text goes to `<topic>/<sender>`, and the iGate subscribes to `<topic>/<callsign>/#`.
* Nothing is sent before the clock is set by NTP. A failed connection is retried every 15 seconds.

`sh test/events/run.sh` builds the formatter on the host and feeds its lines to the collector's own parser (`collector.add_event`).

Cost: a few KB of flash for the event code (about 91% of the slot on ttgo-lora32-v21).
