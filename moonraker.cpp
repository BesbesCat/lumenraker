#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include <LittleFS.h>

char lastMoonrakerJson[2048] = "";
extern EventType currentEvent;
extern float progress[16];
extern float currentTemp;
extern float targetTemp;

WebSocketsClient ws;

void webuiInit();

void handleStatus(JsonObject s) {
//char output[1000];
//serializeJson(s, output);
//Serial.println(output);

    if (s.containsKey("toolhead") && s["toolhead"]["position"]) {
      if(sizeof(s["toolhead"]["position"]) >= 3) {
        progress[0] = s["toolhead"]["position"][0];
        progress[1] = s["toolhead"]["position"][1];
        progress[2] = s["toolhead"]["position"][2];
        currentEvent = EVT_HOMING;
      }
    }

    if (s.containsKey("heater_bed")) {
        JsonObject bed = s["heater_bed"];
        if (bed.containsKey("temperature")) {
            currentTemp = bed["temperature"].as<float>();
        }
        if (bed.containsKey("target")) {
          targetTemp = s["heater_bed"]["target"].as<float>();
        }
        if (targetTemp > 0 && currentTemp < targetTemp - 1) {
          currentEvent = EVT_HEATING;
        }
        if(targetTemp == 0) {
          if(currentEvent == EVT_HEATING) {
            currentEvent = EVT_IDLE;
          }
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
            Serial.println("[Moonraker] Connected! Subscribing...");
            ws.sendTXT("{\"jsonrpc\":\"2.0\",\"method\":\"printer.objects.subscribe\",\"params\":{\"objects\":{\"print_stats\":null,\"idle_timeout\":null,\"heater_bed\":null,\"extruder\":null,\"pause_resume\":null,\"toolhead\":null}},\"id\":1}");
            break;
        }
        case WStype_TEXT:
        {
                snprintf(lastMoonrakerJson, sizeof(lastMoonrakerJson), "%s", (char*)payload);
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, payload);
                //Serial.printf("[Moonraker Data] %s\n", payload);
                if (error) return;
                if (doc["method"] == "notify_status_update") {
                    handleStatus(doc["params"][0].as<JsonObject>());
                } 
                else if (doc.containsKey("result")) {
                    handleStatus(doc["result"]["status"].as<JsonObject>());
                }
            }
            break;
        case WStype_DISCONNECTED:
        {
            Serial.println("[Moonraker] Disconnected. Check IP/Port.");
            break;
        }
        case WStype_ERROR:
            Serial.printf("[WS] Error: %s\n", payload != NULL ? (char*)payload : "Unknown");
            break;
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
    Serial.println("\nWiFi Failed. Starting AP: KlipperLED_Setup");
    WiFi.softAP("KoLED_Setup"); 
    Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());
  }

  webuiInit();
  if (wifiConnected) moonrakerInit();

  while(true) {
    if (wifiConnected) ws.loop();
    vTaskDelay(pdMS_TO_TICKS(10)); 
  }
}