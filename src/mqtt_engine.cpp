#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "config.h"

extern const char* const EventNames[];
extern void saveConfig();

#define NUM_EVENTS 7

WiFiClient espClient;
PubSubClient mqttClient(espClient);

unsigned long lastMqttReconnectAttempt = 0;
String macID = "";

String getEffectList() {
    String effects = "[\"Solid\"";
    File dir = LittleFS.open("/fx");
    if (dir && dir.isDirectory()) {
        File file = dir.openNextFile();
        while (file) {
            String name = file.name();
            if (name.endsWith(".lua") && name != "Solid.lua") {
                name.replace(".lua", "");
                effects += ",\"" + name + "\"";
            }
            file = dir.openNextFile();
        }
    }
    effects += "]";
    return effects;
}

void publishAutoDiscovery() {
    String effectList = getEffectList();
    String devId = "lumenraker_" + macID;

    {
        JsonDocument doc;
        doc["name"] = "Save Configuration";
        doc["unique_id"] = devId + "_save";
        doc["cmd_t"] = "lumenraker/sys/save";
        doc["icon"] = "mdi:content-save";
        
        JsonObject device = doc["device"].to<JsonObject>();
        device["identifiers"].add(devId);
        device["name"] = "LumenRaker LED Controller";
        device["manufacturer"] = "LumenRaker";
        device["model"] = "ESP32";

        String payload;
        serializeJson(doc, payload);
        mqttClient.publish(String("homeassistant/button/" + devId + "/save/config").c_str(), payload.c_str(), true);
    }

    for (int z = 0; z < config.zoneCount; z++) {
        for (int e = 0; e < NUM_EVENTS; e++) {
            String topicPrefix = "lumenraker/light/z" + String(z) + "_e" + String(e);
            String configTopic = "homeassistant/light/" + devId + "/z" + String(z) + "_e" + String(e) + "/config";
            
            JsonDocument doc;
            doc["name"] = String("Zone ") + z + " - " + EventNames[e];
            doc["unique_id"] = devId + "_z" + z + "_e" + e;
            doc["stat_t"] = topicPrefix + "/state";
            doc["cmd_t"] = topicPrefix + "/set";
            doc["schema"] = "json";
            doc["brightness"] = true;
            doc["color_mode"] = true;
            doc["supported_color_modes"][0] = "rgb";
            doc["effect"] = true;
          
            JsonObject device = doc["device"].to<JsonObject>();
            device["identifiers"].add(devId);
            device["name"] = "LumenRaker LED Controller";

            String payload;
            serializeJson(doc, payload);
            
            payload.replace("\"effect\":true", "\"effect\":true,\"effect_list\":" + effectList);
            
            mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
            
            mqttClient.subscribe((topicPrefix + "/set").c_str());
        }
    }
    
    mqttClient.subscribe("lumenraker/sys/save");
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    int z, e;

    if (strcmp(topic, "lumenraker/sys/save") == 0) {
        Serial.println("[MQTT] Received Save Command. Writing RAM to Flash...");
        saveConfig();
        return;
    }

    if (sscanf(topic, "lumenraker/light/z%d_e%d/set", &z, &e) == 2) {
        if (z < 0 || z >= config.zoneCount || e < 0 || e >= NUM_EVENTS) return;

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload, length);
        if (error) return;

        EffectConfig& targetFx = config.zones[z].events[e]; 

        if (doc.containsKey("state")) {
            String state = doc["state"];
            if (state == "OFF") {
                targetFx.brightness = 0;
            } else if (targetFx.brightness == 0) {
                targetFx.brightness = config.brightness;
            }
        }
        if (doc.containsKey("brightness")) targetFx.brightness = doc["brightness"];
        if (doc.containsKey("color")) {
            targetFx.r = doc["color"]["r"];
            targetFx.g = doc["color"]["g"];
            targetFx.b = doc["color"]["b"];
        }
        if (doc.containsKey("effect")) {
            String fx = doc["effect"];
            strlcpy(targetFx.scriptName, fx.c_str(), sizeof(targetFx.scriptName));
        }

        if (doc.containsKey("speed")) targetFx.speed = doc["speed"];
        if (doc.containsKey("size")) targetFx.size = doc["size"];
        if (doc.containsKey("delay")) targetFx.delay = doc["delay"];

        String stateTopic = "lumenraker/light/z" + String(z) + "_e" + String(e) + "/state";
        JsonDocument stateDoc;
        stateDoc["state"] = targetFx.brightness > 0 ? "ON" : "OFF";
        stateDoc["brightness"] = targetFx.brightness;
        JsonObject color = stateDoc["color"].to<JsonObject>();
        color["r"] = targetFx.r;
        color["g"] = targetFx.g;
        color["b"] = targetFx.b;
        stateDoc["effect"] = targetFx.scriptName;
        
        char statePayload[256];
        serializeJson(stateDoc, statePayload);
        mqttClient.publish(stateTopic.c_str(), statePayload);
    }
}

void mqttTask(void* pv) {
    if (strlen(config.mqttHost) == 0) {
        Serial.println("[MQTT] Disabled in config. Terminating module task.");
        vTaskDelete(NULL); 
        return;
    }

    macID = WiFi.macAddress();
    macID.replace(":", "");
    
    mqttClient.setServer(config.mqttHost, config.mqttPort);
    mqttClient.setCallback(mqttCallback);
    
    mqttClient.setBufferSize(4096); 

    while (true) {
        if (WiFi.status() == WL_CONNECTED) {
            if (!mqttClient.connected()) {
                unsigned long now = millis();
                if (now - lastMqttReconnectAttempt > 5000) {
                    lastMqttReconnectAttempt = now;
                    
                    String clientId = "LumenRaker-" + macID;
                    bool connected = (strlen(config.mqttUser) > 0) ? 
                        mqttClient.connect(clientId.c_str(), config.mqttUser, config.mqttPass) : 
                        mqttClient.connect(clientId.c_str());

                    if (connected) {
                        Serial.println("[MQTT] Connected! Broadcasting Auto-Discovery...");
                        publishAutoDiscovery();
                    }
                }
            } else {
                mqttClient.loop();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}