#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Update.h>
#include <esp_system.h>
#include <mbedtls/md.h>
#include "config.h"

AsyncWebServer server(80);
static File uploadFile;
static File surgicalFile;
size_t cachedFsUsed = 0;
size_t cachedFsTotal = 0;

extern String lastLuaDebug;

String activeSessionToken = "";


String hashPassword(const String& password) {
    if (password.length() == 0) return "";
    
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    const size_t payloadLength = password.length();
    byte shaResult[32];
    
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, (const unsigned char *) password.c_str(), payloadLength);
    mbedtls_md_finish(&ctx, shaResult);
    mbedtls_md_free(&ctx);
    
    String hashStr = "";
    for(int i = 0; i < sizeof(shaResult); i++) {
        char str[3];
        sprintf(str, "%02x", (int)shaResult[i]);
        hashStr += str;
    }
    return hashStr;
}

String generateSecureToken() {
    char token[33];
    for (int i = 0; i < 32; i++) {
        uint32_t r = esp_random() % 16;
        token[i] = r < 10 ? '0' + r : 'a' + (r - 10);
    }
    token[32] = '\0';
    return String(token);
}

bool isAuthenticated(AsyncWebServerRequest *request) {
    if (strlen(config.webPass) == 0) return true;

    if (request->hasHeader("Cookie")) {
        String cookie = request->getHeader("Cookie")->value();
        if (activeSessionToken != "" && cookie.indexOf("LumenSession=" + activeSessionToken) != -1) {
            return true;
        }
    }
    return false;
}

void updateFsCache() {
    cachedFsUsed = LittleFS.usedBytes();
}

void handleGetConfig(AsyncWebServerRequest *request) {
    if (!isAuthenticated(request)) return request->send(401);
    JsonDocument doc;
    doc["ssid"] = config.wifiSSID;
    doc["pass"] = config.wifiPASS; 
    doc["mHost"] = config.moonrakerHost;
    doc["mPort"] = config.moonrakerPort;
    doc["br"] = config.brightness;
    doc["webUser"] = config.webUser;
    doc["mqttHost"] = config.mqttHost;
    doc["mqttPort"] = config.mqttPort;
    doc["mqttUser"] = config.mqttUser;
    doc["mqttPass"] = config.mqttPass;
    JsonArray sArr = doc["strips"].to<JsonArray>();
    for(int i=0; i<config.stripCount; i++) {
        JsonObject s = sArr.add<JsonObject>();
        s["pin"] = config.strips[i].gpio;
        s["cnt"] = config.strips[i].count;
    }

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
    if (!isAuthenticated(request)) return request->send(401);
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
            if(doc.containsKey("webUser")) strlcpy(config.webUser, doc["webUser"], sizeof(config.webUser));
            
            strlcpy(config.mqttHost, doc["mqttHost"] | "", sizeof(config.mqttHost));
            config.mqttPort = doc["mqttPort"] | 1883;
            strlcpy(config.mqttUser, doc["mqttUser"] | "", sizeof(config.mqttUser));
            strlcpy(config.mqttPass, doc["mqttPass"] | "", sizeof(config.mqttPass));
            config.stripCount = doc["strips"].size();
            for(int i=0; i<config.stripCount && i<MAX_STRIPS; i++) {
                config.strips[i].gpio = doc["strips"][i]["pin"];
                config.strips[i].count = doc["strips"][i]["cnt"];
            }

            JsonArray jsonZones = doc["zones"];
            int newZoneCount = jsonZones.size();
            
            if (config.zones) {
                delete[] config.zones;
            }
            config.zoneCount = newZoneCount;
            config.zones = new Zone[config.zoneCount];

            for(int i=0; i<config.zoneCount; i++) {
                config.zones[i].strip = jsonZones[i]["sIdx"];
                config.zones[i].start = jsonZones[i]["start"];
                config.zones[i].length = jsonZones[i]["len"];
                config.zones[i].reversed = jsonZones[i]["rev"] | false;
                
                for(int e=0; e<EVT_COUNT; e++) {
                    strlcpy(config.zones[i].events[e].scriptName, jsonZones[i]["evts"][e]["fx"] | "Solid Color", sizeof(config.zones[i].events[e].scriptName));
                    config.zones[i].events[e].r = jsonZones[i]["evts"][e]["r"] | 200;
                    config.zones[i].events[e].g = jsonZones[i]["evts"][e]["g"] | 200;
                    config.zones[i].events[e].b = jsonZones[i]["evts"][e]["b"] | 200;
                    config.zones[i].events[e].speed = jsonZones[i]["evts"][e]["sp"] | 200;
                    config.zones[i].events[e].delay = jsonZones[i]["evts"][e]["dl"] | 200;
                    config.zones[i].events[e].size = jsonZones[i]["evts"][e]["sz"] | 200;
                    config.zones[i].events[e].brightness = jsonZones[i]["evts"][e]["br"] | 200;
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
    if (!isAuthenticated(request)) return request->send(401);
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
    if (!isAuthenticated(request)) return request->send(401);
    if(!request->hasParam("name")) {
        request->send(500, "text/plain", "FileSystem Error");
        return;
    }
    String name = "/fx/" + request->getParam("name")->value() + ".lua";
    if(LittleFS.exists(name)) request->send(LittleFS, name, "text/plain");
    else request->send(404, "text/plain", "FileSystem Error");
}

void handleDeleteScript(AsyncWebServerRequest *request) {
    if (!isAuthenticated(request)) return request->send(401);
    if(!request->hasParam("name")) return;
    String name = "/fx/" + request->getParam("name")->value() + ".lua";
    if(LittleFS.remove(name)) {
        updateFsCache();
        request->send(200, "text/plain", "OK");
    } else {
        request->send(500, "text/plain", "FileSystem Error");
        return;
    }
}

void handleSurgicalWrite(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!isAuthenticated(request)) return request->send(401);
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
    if (!isAuthenticated(request)) return request->send(401);
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

void handleGetSysInfo(AsyncWebServerRequest *request) {
    if (!isAuthenticated(request)) return request->send(401);
    AsyncResponseStream *response = request->beginResponseStream("application/json");

    JsonDocument doc;

    doc["heap_free"] = ESP.getFreeHeap();
    doc["heap_total"] = ESP.getHeapSize();
    doc["heap_min"] = ESP.getMinFreeHeap();
    doc["fs_used"] = cachedFsUsed;
    doc["fs_total"] = cachedFsTotal;
    doc["uptime"] = millis() / 1000;
    doc["cpu_freq"] = ESP.getCpuFreqMHz();
    doc["chip_rev"] = ESP.getChipRevision();
    doc["sdk_ver"] = ESP.getSdkVersion();
    doc["wifi_rssi"] = WiFi.RSSI();

    serializeJson(doc, *response);
    
    request->send(response);
}

void webuiInit() {
    if(!LittleFS.begin(true)){
      Serial.println("LittleFS Mount Failed");
      return;
    }

    cachedFsTotal = LittleFS.totalBytes();
    updateFsCache();
    server.on("/api/login", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("user", true) && request->hasParam("pass", true)) {
            String user = request->getParam("user", true)->value();
            String pass = request->getParam("pass", true)->value();
            
            if (user == config.webUser && hashPassword(pass) == String(config.webPass)) {
                activeSessionToken = generateSecureToken();
                AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "{\"status\":\"ok\"}");
                response->addHeader("Set-Cookie", "LumenSession=" + activeSessionToken + "; Path=/; HttpOnly; SameSite=Strict");
                request->send(response);
                return;
            }
        }
        request->send(401, "application/json", "{\"error\":\"Invalid credentials\"}");
    });

    server.on("/api/logout", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!isAuthenticated(request)) return request->send(401);
        activeSessionToken = "";
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "{\"status\":\"ok\"}");
        response->addHeader("Set-Cookie", "LumenSession=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT");
    });

    server.on("/api/change_password", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!isAuthenticated(request)) return request->send(401);

        String oldPass = request->hasParam("old_pass", true) ? request->getParam("old_pass", true)->value() : "";
        String newPass = request->hasParam("new_pass", true) ? request->getParam("new_pass", true)->value() : "";
        String newUser = request->hasParam("new_user", true) ? request->getParam("new_user", true)->value() : "";

        if (strlen(config.webPass) > 0 && hashPassword(oldPass) != String(config.webPass)) {
            return request->send(400, "application/json", "{\"error\":\"Old password incorrect\"}");
        }

        if (newUser.length() > 0) strlcpy(config.webUser, newUser.c_str(), sizeof(config.webUser));
        
        if (newPass.length() > 0) {
            strlcpy(config.webPass, hashPassword(newPass).c_str(), sizeof(config.webPass));
        } else {
            memset(config.webPass, 0, sizeof(config.webPass)); 
        }
        
        saveConfig();

        activeSessionToken = "";
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "{\"status\":\"ok\"}");
        response->addHeader("Set-Cookie", "LumenSession=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT");
        request->send(response);
    });

    server.on("/api/sysinfo", HTTP_GET, handleGetSysInfo);
    server.on("/api/config", HTTP_GET, handleGetConfig);
    server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!isAuthenticated(request)) return request->send(401);
    }, NULL, handleSaveConfig);
    server.on("/api/debug", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!isAuthenticated(request)) return request->send(401);
        request->send(200, "text/plain", lastLuaDebug);
    });
    server.on("/api/scripts", HTTP_GET, handleListScripts);
    server.on("/api/read_script", HTTP_GET, handleReadScript);
    server.on("/api/delete_script", HTTP_DELETE, handleDeleteScript);
    server.on("/api/save_script", HTTP_POST, 
    [](AsyncWebServerRequest *request) {
        if (!isAuthenticated(request)) return request->send(401);
        request->send(200, "text/plain", "Script Saved OK");
    },
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        if (!isAuthenticated(request)) return request->send(401);
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
        if (!isAuthenticated(request)) return request->send(401);
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        response->addHeader("Connection", "close");
        request->send(response);
    }, NULL, handleFirmwareOTA);

    server.on("/api/install/file", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!isAuthenticated(request)) return request->send(401);
        request->send(200);
    }, NULL, handleSurgicalWrite);

    server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!isAuthenticated(request)) return request->send(401);
        request->send(200, "text/plain", "Rebooting");
        DefaultHeaders::Instance().addHeader("Connection", "close");
        delay(500);
        ESP.restart();
    });

    server.on("/api/version", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!isAuthenticated(request)) return request->send(401);
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "{\"version\":\"%s\"}", LUMEN_VERSION.c_str());
        request->send(200, "application/json", buffer);
    });

    server.on("/api/fps", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!isAuthenticated(request)) return request->send(401);
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "{\"fps\":%d}", currentFPS);
        request->send(200, "application/json", buffer);
    });

    server.serveStatic("/", LittleFS, "/www/").setDefaultFile("index.html");

    server.begin();
}