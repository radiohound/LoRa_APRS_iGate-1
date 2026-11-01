# Development frame info (lora-aprs-collector)

The MQTT section of the web page comes pre-filled for the LoRa APRS collector: server `mqtt.lora-aprs.org`, port 8883, topic `lora-aprs`.
Switch MQTT on, type the password you were given and save. The user name is the callsign when it is left empty.
(A unit that already has a saved configuration keeps its old MQTT values.)

* With that server the iGate sends **event lines** over TLS, not the received packets: `lora-aprs/<callsign>/rx` for every frame heard, `/tx` for every
  frame sent, `/status` once a minute (frame counts per format, CRC errors, uptime). The fields are those of Dire Wolf's `LORAEVENTS`
  (`doc/LoRa-Events.md` in the `feature/lora-events` branch), with `frame.class` = `plain`, `ppm_full`, `ppm_primed` or `hybrid` (LACF).
* The server's certificate is checked against ISRG Root X1 (built in, valid until 2035-06-04).
* Nothing is subscribed to: the broker lets a station write only under its own name.
* With any other server the old behaviour is unchanged: the received TNC2 text goes to `<topic>/<sender>`, and the iGate subscribes to `<topic>/<callsign>/#`.
* Nothing is sent before the clock is set by NTP. A failed connection is retried every 15 seconds.

`sh test/events/run.sh` builds the formatter on the host and feeds its lines to the collector's own parser (`collector.add_event`).

Cost: the TLS code takes about 136 KB of flash (ttgo-lora32-v21: 97.6% of the slot).
