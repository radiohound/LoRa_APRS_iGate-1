// The rule for pre-filling MQTT: only when it was never set up.
#include <stdio.h>
#include <string.h>
#include "mqtt_defaults.h"

int main() {
    struct { const char* what; bool active; const char* server; const char* user; const char* pass; bool want; } c[] = {
        { "never set up (the 31 October backup)",       false, "", "", "", true  },
        { "stock firmware defaults: off, blank",         false, "", "", "", true  },
        { "own broker, off",                             false, "broker.example.org", "", "", false },
        { "own broker, on",                              true,  "broker.example.org", "u", "p", false },
        { "collector already entered, off",              false, "mqtt.lora-aprs.org", "igate", "lora-aprs", false },
        { "only a user name typed",                      false, "", "me", "", false },
        { "only a password typed",                       false, "", "", "secret", false },
        { "switched on but server blank (user error)",   true,  "", "", "", false },
        { "server cleared on purpose but user kept",     false, "", "igate", "", false },
    };
    int bad = 0, n = 0;
    for (auto& k : c) {
        n++;
        if (MqttDefaults::neverConfigured(k.active, k.server, k.user, k.pass) != k.want) { printf("WRONG: %s\n", k.what); bad++; }
    }
    // the values themselves: what a tester would see
    if (strcmp(MqttDefaults::SERVER, "mqtt.lora-aprs.org") || MqttDefaults::PORT != 1884 || strcmp(MqttDefaults::TOPIC, "lora-aprs") ||
        strcmp(MqttDefaults::USERNAME, "igate") || strcmp(MqttDefaults::PASSWORD, "lora-aprs")) { printf("WRONG: default values\n"); bad++; }
    printf("%d cases, %d wrong\n", n + 1, bad);
    return bad ? 1 : 0;
}
