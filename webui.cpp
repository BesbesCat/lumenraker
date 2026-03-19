#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Update.h>
#include "config.h"


AsyncWebServer server(80);
static File uploadFile;
static File surgicalFile;

extern String lastLuaDebug;

void handleGetConfig(AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["ssid"] = config.wifiSSID;
    doc["pass"] = config.wifiPASS; 
    doc["mHost"] = config.moonrakerHost;
    doc["mPort"] = config.moonrakerPort;
    doc["br"] = config.brightness;

    // Serialize Strips
    JsonArray sArr = doc["strips"].to<JsonArray>();
    for(int i=0; i<config.stripCount; i++) {
        JsonObject s = sArr.add<JsonObject>();
        s["pin"] = config.strips[i].gpio;
        s["cnt"] = config.strips[i].count;
    }

    // Serialize Zones & Events
    JsonArray zArr = doc["zones"].to<JsonArray>();
    for(int i=0; i<config.zoneCount; i++) {
        JsonObject z = zArr.add<JsonObject>();
        z["sIdx"] = config.zones[i].strip;
        z["start"] = config.zones[i].start;
        z["len"] = config.zones[i].length;
        z["rev"] = config.zones[i].reversed;
        
        JsonArray evts = z["evts"].to<JsonArray>();
        for(int e=0; e<EVT_COUNT; e++) {
            JsonObject ev = evts.add<JsonObject>();
            ev["fx"] = config.zones[i].events[e].scriptName; 
            ev["r"] = config.zones[i].events[e].r;
            ev["g"] = config.zones[i].events[e].g;
            ev["b"] = config.zones[i].events[e].b;
            ev["sp"] = config.zones[i].events[e].speed;
            ev["dl"] = config.zones[i].events[e].delay;
            ev["sz"] = config.zones[i].events[e].size;
            ev["br"] = config.zones[i].events[e].brightness;

        }
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void handleSaveConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    static String jsonBuffer;
    if (index == 0) jsonBuffer = "";
    jsonBuffer += String((char*)data).substring(0, len);

    if (index + len == total) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, jsonBuffer);
        
        if (!error) {
            strlcpy(config.wifiSSID, doc["ssid"] | "", sizeof(config.wifiSSID));
            strlcpy(config.wifiPASS, doc["pass"] | "", sizeof(config.wifiPASS));
            strlcpy(config.moonrakerHost, doc["mHost"] | "192.168.1.100", sizeof(config.moonrakerHost));
            config.moonrakerPort = doc["mPort"] | 7125;
            config.brightness = doc["br"] | 128;

            // Save Strips
            config.stripCount = doc["strips"].size();
            for(int i=0; i<config.stripCount && i<MAX_STRIPS; i++) {
                config.strips[i].gpio = doc["strips"][i]["pin"];
                config.strips[i].count = doc["strips"][i]["cnt"];
            }

            // Save Zones
            config.zoneCount = doc["zones"].size();
            for(int i=0; i<config.zoneCount && i<MAX_ZONES; i++) {
                config.zones[i].strip = doc["zones"][i]["sIdx"];
                config.zones[i].start = doc["zones"][i]["start"];
                config.zones[i].length = doc["zones"][i]["len"];
                config.zones[i].reversed = doc["zones"][i]["rev"] | false;
                
                for(int e=0; e<EVT_COUNT; e++) {
                    // Copy the Lua script name into the struct
                    strlcpy(config.zones[i].events[e].scriptName, doc["zones"][i]["evts"][e]["fx"] | "Solid Color", sizeof(config.zones[i].events[e].scriptName));
                    config.zones[i].events[e].r = doc["zones"][i]["evts"][e]["r"] | 200;
                    config.zones[i].events[e].g = doc["zones"][i]["evts"][e]["g"] | 200;
                    config.zones[i].events[e].b = doc["zones"][i]["evts"][e]["b"] | 200;
                    config.zones[i].events[e].speed = doc["zones"][i]["evts"][e]["sp"] | 200;
                    config.zones[i].events[e].delay = doc["zones"][i]["evts"][e]["dl"] | 200;
                    config.zones[i].events[e].size = doc["zones"][i]["evts"][e]["sz"] | 200;
                    config.zones[i].events[e].brightness = doc["zones"][i]["evts"][e]["br"] | 200;
                }
            }
            saveConfig();
            request->send(200, "text/plain", "OK");
            delay(1000); ESP.restart();
        } else {
            request->send(400, "text/plain", "Invalid JSON");
        }
    }
}


void handleListScripts(AsyncWebServerRequest *request) {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    File root = LittleFS.open("/fx");
    if(root && root.isDirectory()) {
        File file = root.openNextFile();
        while(file) {
            String name = String(file.name());
            if(name.endsWith(".lua")) arr.add(name.substring(0, name.length() - 4)); 
            file = root.openNextFile();
        }
    } else {
        request->send(500, "text/plain", "FileSystem Error");
        return;
    }
    String output; serializeJson(doc, output);
    request->send(200, "application/json", output);
}

void handleReadScript(AsyncWebServerRequest *request) {
    if(!request->hasParam("name")) {
        request->send(500, "text/plain", "FileSystem Error");
        return;
    }
    String name = "/fx/" + request->getParam("name")->value() + ".lua";
    if(LittleFS.exists(name)) request->send(LittleFS, name, "text/plain");
    else request->send(404, "text/plain", "FileSystem Error");
}

void handleSaveScript(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    AsyncWebParameter* p = request->getParam("name"); 
    if(!p) {
        request->send(500, "text/plain", "FileSystem Error");
        return;
    }
    String filename = "/fx/" + p->value() + ".lua";
    File file = LittleFS.open(filename, (index == 0) ? "w" : "a");
    if(file) {
        file.write(data, len);
        file.close();
        
        if (index + len == total) {
            request->send(200, "text/plain", "OK");
        }
        delay(1000); ESP.restart();
    } else {
        if (index == 0) request->send(500, "text/plain", "FileSystem Error");
        if (index + len == total) request->send(500, "text/plain", "FileSystem Error");
    }
}

void handleDeleteScript(AsyncWebServerRequest *request) {
    if(!request->hasParam("name")) return;
    String name = "/fx/" + request->getParam("name")->value() + ".lua";
    if(LittleFS.remove(name)) {
        request->send(200, "text/plain", "OK");
    } else {
        request->send(500, "text/plain", "FileSystem Error");
        return;
    }
}

void handleSurgicalWrite(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (index == 0) {
        Serial.printf("[Surgical] Upload Start. Expected Total Size: %u\n", total);
        
        if(!request->hasHeader("X-Dest-Path")) {
            Serial.println("[Surgical] ERROR: Missing X-Dest-Path header. Aborting write.");
            return; 
        }
        
        String path = request->getHeader("X-Dest-Path")->value();
        Serial.printf("[Surgical] Target Path: %s\n", path.c_str());
        
        if (path.startsWith("/fx/") && !LittleFS.exists("/fx")) LittleFS.mkdir("/fx");
        
        surgicalFile = LittleFS.open(path, "w");
        if (!surgicalFile) Serial.println("[Surgical] ERROR: LittleFS failed to open file for writing!");
    }
    
    if (surgicalFile) {
        surgicalFile.write(data, len);
    }
    
    if (index + len == total || (total == 0 && len == 0)) {
        if (surgicalFile) {
            surgicalFile.close();
            Serial.println("[Surgical] File written and closed successfully.");
        }
    }
}

void handleFirmwareOTA(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (index == 0) {
        Serial.printf("[OTA] Firmware Upload Start. Expected Size: %u\n", total);
        
        size_t updateSize = (total == 0) ? UPDATE_SIZE_UNKNOWN : total;
        
        if (!Update.begin(updateSize, U_FLASH)) {
            Serial.println("[OTA] ERROR: Update.begin failed!");
            Update.printError(Serial);
        }
    }
    
    if (Update.write(data, len) != len) {
        Serial.println("[OTA] ERROR: Update.write failed! Stream corrupted.");
        Update.printError(Serial);
    }
    
    if (index + len == total || (total == 0 && len == 0)) {
        if (Update.end(true)) {
            Serial.println("[OTA] Firmware Update Success!");
        } else {
            Serial.println("[OTA] ERROR: Update.end failed!");
            Update.printError(Serial);
        }
    }
}

void webuiInit() {
    if(!LittleFS.begin(true)){
      Serial.println("LittleFS Mount Failed");
      return;
    }

    server.on("/api/config", HTTP_GET, handleGetConfig);
    server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, handleSaveConfig);
    server.on("/api/debug", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", lastLuaDebug);
    });
    server.on("/api/scripts", HTTP_GET, handleListScripts);
    server.on("/api/read_script", HTTP_GET, handleReadScript);
    server.on("/api/delete_script", HTTP_DELETE, handleDeleteScript);
    server.on("/api/save_script", HTTP_POST, 
    [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Script Saved OK");
    },
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        
        Serial.println("[WebUI] Started writing");
        if (index == 0) {
            if(!request->hasParam("name")) return;
            String path = "/fx/" + request->getParam("name")->value() + ".lua";
            Serial.printf("[WebUI] Starting save: %s\n", path.c_str());
            
            uploadFile = LittleFS.open(path, "w");
            if (!uploadFile) {
                Serial.println("[WebUI] Failed to open file for writing");
                return;
            }
        }

        if (uploadFile) {
            uploadFile.write(data, len);
        }

        if (index + len == total) {
            if (uploadFile) {
                    uploadFile.close();
                    delay(1000); ESP.restart();
                }
            }
        }
    );

    server.on("/api/install/firmware", HTTP_POST, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        response->addHeader("Connection", "close");
        request->send(response);
    }, NULL, handleFirmwareOTA);

    server.on("/api/install/file", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200);
    }, NULL, handleSurgicalWrite);

    server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "Rebooting");
        DefaultHeaders::Instance().addHeader("Connection", "close");
        delay(500);
        ESP.restart();
    });

    server.on("/api/version", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = "{\"version\":\"" + LUMEN_VERSION + "\"}";
        request->send(200, "application/json", json);
    });

    server.serveStatic("/", LittleFS, "/www/").setDefaultFile("index.html");

    server.begin();
}