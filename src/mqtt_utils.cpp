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
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <sys/time.h>
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


// The root certificate that signs the Let's Encrypt chain of mqtt.lora-aprs.org (ISRG Root X1, valid until 2035-06-04).
static const char COLLECTOR_ROOT_CA[] = R"EOF(-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";


namespace MQTT_Utils {

    // With the server set to the collector, this iGate sends event lines (rx, tx, status) under lora-aprs/<callsign>/ over TLS,
    // for development statistics. With any other server the old behaviour stays: the received TNC2 text goes out as it is.
    static const char*      COLLECTOR_HOST      = "mqtt.lora-aprs.org";
    static const uint32_t   STATUS_EVERY_MS     = 60000;
    static const uint32_t   RETRY_EVERY_MS      = 15000;

    static WiFiClientSecure secureClient;
    static String           bootId;
    static uint32_t         sequence            = 0;
    static uint32_t         lastStatus          = 0;
    static uint32_t         lastTry             = 0;
    static EventsFormat::Counts counts          = {};

    bool collectorMode() {
        return Config.mqtt.server.equalsIgnoreCase(COLLECTOR_HOST);
    }

    bool isConnected() {
        return pubSub.connected();
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
        if (!pubSub.publish(topic.c_str(), json.c_str())) Serial.println("Event not sent to MQTT (check connection or size)");
    }

    static int rxFrameClass() {
        if (LoRa_Utils::lastRxWasHybrid())      return EventsFormat::HYBRID;
        if (LoRa_Utils::lastRxWasCompressed())  return LoRa_Utils::lastRxWasPrimed() ? EventsFormat::PPM_PRIMED : EventsFormat::PPM_FULL;
        return EventsFormat::PLAIN;
    }

    static void publishRx(const String& packet) {
        const int cls = rxFrameClass();
        counts.received[cls]++;
        if (!pubSub.connected()) return;
        EventsFormat::Head h;
        if (!makeHead(h)) return;
        const String tnc2 = packet.substring(3);
        publishEvent("rx", EventsFormat::rx(h, radioNow(), rssi, snr, freqError, cls, (int)LoRa_Utils::lastRxAirBytes(), (int)packet.length(),
                                            std::string(tnc2.c_str(), tnc2.length())));
    }

    void publishTx(int frameClass, int airBytes, bool ok) {
        if (!collectorMode() || !pubSub.connected()) return;
        EventsFormat::Head h;
        if (!makeHead(h)) return;
        publishEvent("tx", EventsFormat::tx(h, radioNow(), frameClass, airBytes, ok));
    }

    void countCrcError() {
        counts.crcErrors++;
    }

    static void publishStatus() {
        EventsFormat::Head h;
        if (!makeHead(h)) return;
        publishEvent("status", EventsFormat::status(h, (long)(millis() / 1000), counts));
    }

    void sendToMqtt(const String& packet) {
        if (collectorMode()) {
            publishRx(packet);
            return;
        }
        if (!pubSub.connected()) {
            Serial.println("Can not send to MQTT because it is not connected");
            return;
        }
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

    void connect() {
        if (pubSub.connected()) return;
        if (Config.mqtt.server.isEmpty() || Config.mqtt.port <= 0) {
            Serial.println("Connect to MQTT server KO because no host or port given");
            return;
        }
        const bool collector = collectorMode();
        if (collector) {                                    // a TLS connect takes seconds: not on every pass of the main loop
            if (lastTry != 0 && millis() - lastTry < RETRY_EVERY_MS) return;
            lastTry = millis();
        }
        pubSub.setServer(Config.mqtt.server.c_str(), Config.mqtt.port);
        Serial.print("Trying to connect with MQTT Server: " + String(Config.mqtt.server) + " MqttServerPort: " + String(Config.mqtt.port));

        bool connected = false;
        if (collector) {                                    // the broker lets a station write only under its own name, so the user name is the callsign
            const String user = Config.mqtt.username.isEmpty() ? Config.callsign : Config.mqtt.username;
            connected = pubSub.connect(Config.callsign.c_str(), user.c_str(), Config.mqtt.password.c_str());
        } else if (!Config.mqtt.username.isEmpty()) {
            connected = pubSub.connect(Config.callsign.c_str(), Config.mqtt.username.c_str(), Config.mqtt.password.c_str());
        } else {
            connected = pubSub.connect(Config.callsign.c_str());
        }

        if (connected) {
            Serial.println(" -> Connected !");
            if (collector) {
                lastStatus = millis() - STATUS_EVERY_MS + 5000;     // first status line 5 seconds after connecting
                return;                                             // nothing to subscribe to: stations may only write
            }
            const String subscribedTopic = Config.mqtt.topic + "/" + Config.callsign + "/#";
            if (!pubSub.subscribe(subscribedTopic.c_str())) {
                Serial.println("Subscribed to MQTT Failed");
            }
            Serial.print("Subscribed to MQTT topic : ");
            Serial.println(subscribedTopic);
        } else {
            Serial.println(" -> Not Connected (Retry in a few secs)");
        }
    }

    void loop() {
        if (!Config.mqtt.active) return;
        if (!pubSub.connected()) return;
        pubSub.loop();
        if (collectorMode() && millis() - lastStatus >= STATUS_EVERY_MS) {
            lastStatus = millis();
            publishStatus();
        }
    }

    void setup() {
        if (!Config.mqtt.active) return;
        if (collectorMode()) {
            char id[9];
            snprintf(id, sizeof id, "%08x", (unsigned)esp_random());
            bootId = id;
            secureClient.setCACert(COLLECTOR_ROOT_CA);      // the server is checked: the password goes over this connection
            secureClient.setTimeout(8);
            pubSub.setClient(secureClient);
            pubSub.setBufferSize(1536);                     // an rx line is about 600 bytes; the library default is 256
            return;
        }
        pubSub.setClient(mqttClient);
        pubSub.setCallback(receivedFromMqtt);
    }

}