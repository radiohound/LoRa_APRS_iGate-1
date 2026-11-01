// The MQTT section is pre-filled for the LoRa APRS development collector, but only when nobody ever set it up.
// "Never set up" = MQTT off and the server, user name and password all empty. A saved configuration made before the collector existed
// has those fields present but blank, so the fallbacks in readFile() (used only for a MISSING key) would never reach it.
// Anyone who entered a server (their own broker, say), a user name or a password is left alone. MQTT stays off either way.

#ifndef MQTT_DEFAULTS_H_
#define MQTT_DEFAULTS_H_

namespace MqttDefaults {

    inline bool neverConfigured(bool active, const char* server, const char* username, const char* password) {
        return !active && server[0] == '\0' && username[0] == '\0' && password[0] == '\0';
    }

    static const char* const SERVER   = "mqtt.lora-aprs.org";
    static const char* const TOPIC    = "lora-aprs";
    static const char* const USERNAME = "igate";       // the shared, public login (see docs/COLLECTOR.md)
    static const char* const PASSWORD = "lora-aprs";
    static const int         PORT     = 1884;          // plain, no TLS

}

#endif
