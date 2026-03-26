#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "config.h"

char lastMoonrakerJson[1024] = "";
extern EventType currentEvent;
extern float progress[16];
extern float currentTemp;
extern float targetTemp;
bool isMoonrakerConnected = false;
unsigned long lastMoonrakerCheck = 0;

WebSocketsClient ws;

void webuiInit();

void handleStatus(JsonObject s) {

    if (s.containsKey("toolhead") && s["toolhead"]["position"]) {
      JsonArray pos = s["toolhead"]["position"].as<JsonArray>();
      if(pos.size() >= 3) {
        progress[0] = pos[0];
        progress[1] = pos[1];
        progress[2] = pos[2];
        currentEvent = EVT_HOMING;
      }
    }

    static float bedTemp = 0;
    static float bedTarget = 0;

    if (s.containsKey("heater_bed")) {
        JsonObject bed = s["heater_bed"];
        
        if (bed.containsKey("temperature")) {
          bedTemp = bed["temperature"].as<float>();
        }
        if (bed.containsKey("target")) {
          bedTarget = bed["target"].as<float>();
        }

        if (bedTarget > 0 && bedTemp < bedTarget - 1) {
          currentTemp = bedTemp;
          targetTemp = bedTarget;
          currentEvent = EVT_HEATING;
          return;
        }
        
        if (bedTarget == 0 && currentEvent == EVT_HEATING) {
          currentEvent = EVT_IDLE;
          return;
        }
    }

    static float extTemp = 0;
    static float extTarget = 0;

    if (s.containsKey("extruder")) {
        JsonObject extruder = s["extruder"];
        
        if (extruder.containsKey("temperature")) {
          extTemp = extruder["temperature"].as<float>();
        }
        if (extruder.containsKey("target")) {
          extTarget = extruder["target"].as<float>();
        }

        if (extTarget > 0 && extTemp < extTarget - 1) {
          currentTemp = extTemp;
          targetTemp = extTarget;
          currentEvent = EVT_HEATING_EXTRUDER;
          return;
        }
        
        if (extTarget == 0 && currentEvent == EVT_HEATING_EXTRUDER) {
          currentEvent = EVT_IDLE;
          return;
        }
    }

    if (s.containsKey("print_stats")) {
        String state = s["print_stats"]["state"] | "";
        if (state == "printing") currentEvent = EVT_PRINTING;
        else if (state == "complete") currentEvent = EVT_IDLE;
        else if (state == "standby") currentEvent = EVT_IDLE;
        else if (state == "error") currentEvent = EVT_ERROR;
    }
}

void wsEvent(WStype_t t, uint8_t* payload, size_t len) {
    switch (t) {
        case WStype_CONNECTED:
        {
            isMoonrakerConnected = true;
            Serial.println("[Moonraker] Connected! Subscribing...");
            ws.sendTXT("{\"jsonrpc\":\"2.0\",\"method\":\"printer.objects.subscribe\",\"params\":{\"objects\":{\"print_stats\":null,\"idle_timeout\":null,\"heater_bed\":null,\"extruder\":null,\"pause_resume\":null,\"toolhead\":null}},\"id\":1}");
            break;
        }
        case WStype_TEXT:
        {
            snprintf(lastMoonrakerJson, sizeof(lastMoonrakerJson), "%s", (char*)payload);
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);
            if (error) return;
            if (doc["method"] == "notify_status_update") {
                handleStatus(doc["params"][0].as<JsonObject>());
            } 
            else if (doc.containsKey("result")) {
                handleStatus(doc["result"]["status"].as<JsonObject>());
            }
            break;
        }
        case WStype_DISCONNECTED:
        {
            isMoonrakerConnected = false;
            currentEvent = EVT_SHUTDOWN;
            Serial.println("[Moonraker] Disconnected. Check IP/Port.");
            break;
        }
        case WStype_ERROR:
        {
            isMoonrakerConnected = false;
            currentEvent = EVT_SHUTDOWN;
            Serial.printf("[WS] Error: %s\n", payload != NULL ? (char*)payload : "Unknown");
            break;
        }
    }
}

void moonrakerInit() {
    ws.setExtraHeaders("Origin: http://localhost.local\r\nUser-Agent: ESP32-KlipperLED");
    ws.begin(config.moonrakerHost, config.moonrakerPort, "/websocket");
    ws.onEvent(wsEvent);
    ws.setReconnectInterval(5000);
}

void netTask(void* pv) {
    bool wifiConnected = false;
    
    if (strlen(config.wifiSSID) > 0) {
        Serial.printf("Connecting to WiFi: %s\n", config.wifiSSID);
        WiFi.begin(config.wifiSSID, config.wifiPASS);
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
            vTaskDelay(pdMS_TO_TICKS(500));
            Serial.print(".");
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nWiFi Connected!");
            Serial.print("IP: "); Serial.println(WiFi.localIP());
            wifiConnected = true;
        }
    }

    if (!wifiConnected) {
        Serial.println("\nWiFi Failed. Starting AP: LUMENRAKER_SETUP");
        WiFi.softAP("LUMENRAKER_SETUP"); 
        Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());
    }

    webuiInit();
    
    if (wifiConnected) {
        moonrakerInit();
    }

    while(true) {
        if (strlen(config.wifiSSID) > 0 && WiFi.status() != WL_CONNECTED) {
            isMoonrakerConnected = false;
            currentEvent = EVT_SHUTDOWN;
            Serial.println("WiFi dropped. Attempting to reconnect...");
            WiFi.disconnect();
            WiFi.reconnect();
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        if (WiFi.status() == WL_CONNECTED && !isMoonrakerConnected) {
            if (millis() - lastMoonrakerCheck > 5000) {
                lastMoonrakerCheck = millis();
                Serial.println("[NetTask] Forcing Moonraker socket reset...");
                ws.disconnect();
                moonrakerInit();
            }
        }

        if (WiFi.status() == WL_CONNECTED) {
            ws.loop();
        }
        
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}