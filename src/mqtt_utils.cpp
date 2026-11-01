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

#include <WiFiClient.h>
#include <PubSubClient.h>
#include <sys/time.h>
#include <atomic>
#include "configuration.h"
#include "station_utils.h"
#include "events_format.h"
#include "lora_utils.h"
#include "mqtt_utils.h"


extern          Configuration   Config;
extern          WiFiClient      mqttClient;
extern          int             rssi, freqError;        // last received frame, set in lora_utils.cpp
extern          float           snr;

PubSubClient    pubSub;



namespace MQTT_Utils {

    // With the server set to the collector, this iGate sends event lines (rx, tx, status) under lora-aprs/<callsign>/ (port 1884, no TLS:
    // the firmware has no room for it; that broker port has one shared, public, write-only login), for development statistics. With any other server the old behaviour stays: the received TNC2 text goes out as it is.
    static const char*      COLLECTOR_HOST      = "mqtt.lora-aprs.org";
    static const uint32_t   STATUS_EVERY_MS     = 60000;
    static const uint32_t   RETRY_FIRST_MS      = 15000;       // after a failed connect: wait this long, then twice as long each time, up to RETRY_MAX_MS
    static const uint32_t   RETRY_MAX_MS        = 300000;
    static const int        BROKEN_PER_MIN      = 30;

    static String           bootId;
    static uint32_t         sequence            = 0;
    static uint32_t         lastStatus          = 0;
    static uint32_t         nextTry             = 0;           // earliest time of the next connect attempt
    static uint32_t         retryDelayMs        = RETRY_FIRST_MS;
    static std::atomic<bool> connecting(false);                    // the connect task is running: the main loop must not touch the MQTT client
    static uint32_t         lastPass            = 0;
    static uint32_t         loopMaxMs           = 0;               // longest main-loop pass in the current minute
    static uint32_t         reportedLoopMaxMs   = 0;               // ... and in the last full minute (sent in the status line)
    static uint32_t         lastLoopReport      = 0;
    static EventsFormat::Counts counts          = {};

    bool collectorMode() {
        return Config.mqtt.server.equalsIgnoreCase(COLLECTOR_HOST);
    }

    // The client may be used only while no connect is in progress (the connect task owns it then), and only when it is connected.
    static bool usable() {
        return !connecting.load() && pubSub.connected();
    }

    bool isConnected() {
        return usable();
    }

    static bool makeHead(EventsFormat::Head& h) {
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        if (tv.tv_sec < 1700000000) return false;           // the clock is not set yet (NTP)
        h.bootId    = bootId.c_str();
        h.sequence  = ++sequence;
        h.timeMs    = (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
        h.station   = Config.callsign.c_str();
        return true;
    }

    static EventsFormat::Radio radioNow() {
        return { Config.loramodule.rxFreq, Config.loramodule.rxSignalBandwidth, Config.loramodule.rxSpreadingFactor, Config.loramodule.rxCodingRate4 };
    }

    static void publishEvent(const char* kind, const std::string& json) {
        const String topic = Config.mqtt.topic + "/" + Config.callsign + "/" + kind;
        // A failed publish on a dead connection makes PubSubClient drop it (connected() turns false), and the main loop starts a new connect with back-off.
        if (!pubSub.publish(topic.c_str(), json.c_str())) Serial.println(pubSub.connected() ? "Event not sent to MQTT (too big for the buffer)" : "Event not sent to MQTT (connection lost)");
    }

    static int rxFrameClass() {
        if (LoRa_Utils::lastRxWasHybrid())      return EventsFormat::HYBRID;
        if (LoRa_Utils::lastRxWasCompressed())  return LoRa_Utils::lastRxWasPrimed() ? EventsFormat::PPM_PRIMED : EventsFormat::PPM_FULL;
        return EventsFormat::PLAIN;
    }

    static void publishRx(const String& packet) {
        const int cls = rxFrameClass();
        counts.received[cls]++;
        if (!usable()) return;
        EventsFormat::Head h;
        if (!makeHead(h)) return;
        const String tnc2 = packet.substring(3);
        if (EventsFormat::privatePath(std::string(tnc2.c_str(), tnc2.length()))) return;     // RFONLY / NOGATE: counted above, never sent
        publishEvent("rx", EventsFormat::rx(h, radioNow(), rssi, snr, freqError, cls, (int)LoRa_Utils::lastRxAirBytes(), (int)packet.length(),
                                            std::string(tnc2.c_str(), tnc2.length())));
    }

    void publishTx(int frameClass, int airBytes, bool ok) {
        if (!collectorMode() || !usable()) return;
        EventsFormat::Head h;
        if (!makeHead(h)) return;
        publishEvent("tx", EventsFormat::tx(h, radioNow(), frameClass, airBytes, ok));
    }

    void publishUndecoded(int outcome, int frameClass, const uint8_t* raw, size_t rawLen, int rssiDbm, float snrDb, int freqErrorHz) {
        if (outcome == EventsFormat::CRC_ERROR)                 counts.crcErrors++;
        else if (outcome == EventsFormat::DROPPED_NO_REFERENCE) counts.droppedNoReference++;
        else                                                    counts.parseFailed++;
        if (!Config.mqtt.active || !collectorMode() || !usable()) return;
        if (outcome == EventsFormat::CRC_ERROR) {                       // a burst of collisions must not flood the broker
            static uint32_t windowStart = 0;
            static int      inWindow    = 0;
            if (millis() - windowStart >= 60000) { windowStart = millis(); inWindow = 0; }
            if (inWindow >= BROKEN_PER_MIN) return;
            inWindow++;
        }
        EventsFormat::Head h;
        if (!makeHead(h)) return;
        // The bytes of a frame that failed its CRC are NOT sent: they are damaged, so a private packet (RFONLY or NOGATE in its path) can no longer be recognised
        // as one, and would be published in readable form. What is sent: the class guessed from the first bytes, the length, RSSI, SNR and the frequency error.
        (void)raw;
        publishEvent("rx", EventsFormat::rxUndecoded(h, radioNow(), rssiDbm, snrDb, freqErrorHz, frameClass, outcome, (int)rawLen, nullptr, 0));
    }

    static void publishStatus() {
        EventsFormat::Head h;
        if (!makeHead(h)) return;
        EventsFormat::Noise noise = {};
        LoRa_Utils::takeNoiseFloor(noise);
        counts.loopMaxMs = (long)reportedLoopMaxMs;
        publishEvent("status", EventsFormat::status(h, (long)(millis() / 1000), counts, noise));
    }

    void sendToMqtt(const String& packet) {
        if (collectorMode()) {
            publishRx(packet);
            return;
        }
        if (!usable()) return;                              // not connected (yet): the packet is simply not sent over MQTT
        const String cleanPacket    = packet.substring(3);
        const String sender         = cleanPacket.substring(0, cleanPacket.indexOf(">"));
        const String topic          = String(Config.mqtt.topic + "/" + sender);

        const bool result           = pubSub.publish(topic.c_str(), cleanPacket.c_str());
        if (result) {
            Serial.print("Packet sent to MQTT topic "); Serial.println(topic);
        } else {
            Serial.println("Packet not sent to MQTT (check connection)");
        }
    }

    void receivedFromMqtt(char* topic, byte* payload, unsigned int length) {
        Serial.print("Received from MQTT topic "); Serial.print(topic); Serial.print(": ");
        for (int i = 0; i < length; i++) {
            Serial.print((char)payload[i]);
        }
        Serial.println();
        STATION_Utils::addToOutputPacketBuffer(String(payload, length));
    }

    // Connecting is slow when something is wrong: a DNS lookup, a TCP connect that times out, and up to 15 s waiting for a broker that accepts the
    // connection but never answers. It runs in its own task so the radio loop never waits for it; while the task runs, the main loop leaves
    // the MQTT client alone (usable() is false). After a failure the next attempt waits 15 s, then 30, 60 ... up to 5 minutes.
    static void connectTask(void*) {
        const bool collector = collectorMode();
        const String server  = Config.mqtt.server;
        const int    port    = Config.mqtt.port;
        const String call    = Config.callsign;
        const String user    = Config.mqtt.username;
        const String pass    = Config.mqtt.password;
        const String topic   = Config.mqtt.topic;

        pubSub.setServer(server.c_str(), port);
        Serial.println("MQTT: connecting to " + server + ":" + String(port) + " in the background");
        bool connected = false;
        if (collector) {                                    // the broker lets a station write only under its own name, so the user name is the callsign
            connected = pubSub.connect(call.c_str(), (user.isEmpty() ? String("igate") : user).c_str(), pass.c_str());     // client id = callsign: it may write under lora-aprs/<client id>/
        } else if (!user.isEmpty()) {
            connected = pubSub.connect(call.c_str(), user.c_str(), pass.c_str());
        } else {
            connected = pubSub.connect(call.c_str());
        }

        if (connected) {
            mqttClient.setTimeout(2);                       // seconds: a write on a dead socket must not hold the main loop for long
            retryDelayMs = RETRY_FIRST_MS;
            Serial.println("Trying to connect with MQTT Server: " + server + " MqttServerPort: " + String(port) + " -> Connected !");
            if (collector) {
                lastStatus = millis() - STATUS_EVERY_MS + 5000;     // first status line 5 seconds after connecting
            } else {
                const String subscribedTopic = topic + "/" + call + "/#";
                if (!pubSub.subscribe(subscribedTopic.c_str())) Serial.println("Subscribed to MQTT Failed");
                Serial.println("Subscribed to MQTT topic : " + subscribedTopic);
            }
        } else {
            nextTry = millis() + retryDelayMs;
            Serial.println("Trying to connect with MQTT Server: " + server + " MqttServerPort: " + String(port) + " -> Not Connected (retry in " + String(retryDelayMs / 1000) + " s)");
            retryDelayMs = retryDelayMs * 2 > RETRY_MAX_MS ? RETRY_MAX_MS : retryDelayMs * 2;
        }
        connecting.store(false);
        vTaskDelete(nullptr);
    }

    void connect() {
        if (connecting.load() || pubSub.connected()) return;
        if (Config.mqtt.server.isEmpty() || Config.mqtt.port <= 0) return;     // nothing to connect to
        if ((int32_t)(millis() - nextTry) < 0) return;                          // still waiting after a failure
        connecting.store(true);
        if (xTaskCreatePinnedToCore(connectTask, "mqttconn", 8192, nullptr, 1, nullptr, 0) != pdPASS) {
            connecting.store(false);
            nextTry = millis() + retryDelayMs;                                  // no memory for the task right now: try again later
            Serial.println("MQTT: could not start the connect task, will retry");
        }
    }

    void loop() {
        const uint32_t now = millis();
        if (lastPass != 0 && now - lastPass > loopMaxMs) loopMaxMs = now - lastPass;     // the longest pass of the whole main loop, for the status line
        lastPass = now;
        if (!Config.mqtt.active) return;
        if (collectorMode() && now - lastLoopReport >= STATUS_EVERY_MS) {
            lastLoopReport = now;
            reportedLoopMaxMs = loopMaxMs;
            loopMaxMs = 0;
            Serial.println("Main loop: longest pass in the last minute " + String(reportedLoopMaxMs) + " ms");
        }
        if (!usable()) return;
        pubSub.loop();
        if (collectorMode() && now - lastStatus >= STATUS_EVERY_MS) {
            lastStatus = now;
            publishStatus();
        }
    }

    void setup() {
        if (!Config.mqtt.active) return;
        if (collectorMode()) {
            char id[9];
            snprintf(id, sizeof id, "%08x", (unsigned)esp_random());
            bootId = id;
            pubSub.setClient(mqttClient);
            pubSub.setBufferSize(3072);                     // an rx line is about 600 bytes, up to 2 KB for a long non-ASCII packet (every byte of 0x7F or more becomes 6); the library default is 256
            return;
        }
        pubSub.setClient(mqttClient);
        pubSub.setCallback(receivedFromMqtt);
    }

}