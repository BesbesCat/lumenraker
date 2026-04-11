#include <Preferences.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "config.h"

Preferences prefs;
Config config;

const char* const EventNames[] = { "Idle", "Printing", "Heating Bed", "Heating Extruder", "Moving", "Error", "Shutdown", "Streaming" };

void defaultConfig() {
  if (config.zones) { 
      delete[] config.zones; 
      config.zones = nullptr; 
  }
  memset(&config, 0, sizeof(Config));
  
  strcpy(config.hostname, "lumenraker");
  strcpy(config.moonrakerHost, "192.168.1.100");
  config.moonrakerPort = 7125;
  config.brightness = 128;
  config.fadeDurationMs = 750;
  config.colorTempK = 6500;

  strcpy(config.webUser, "admin");
  memset(config.webPass, 0, sizeof(config.webPass));
  
  strcpy(config.mqttHost, "");
  config.mqttPort = 1883;
  strcpy(config.mqttUser, "");
  strcpy(config.mqttPass, "");
  
  strcpy(config.uiBg, "#0f111a");
  strcpy(config.uiPanel, "#1a1c25");
  strcpy(config.uiCard, "#242734");
  strcpy(config.uiText, "#e0e0e0");
  strcpy(config.uiDim, "#9499b0");
  strcpy(config.uiAccent, "#3b82f6");
  strcpy(config.uiBorder, "#30364d");
  strcpy(config.uiDanger, "#ff4444");
  strcpy(config.uiSuccess, "#00c853");

  config.stripCount = 1;
  config.strips[0].gpio = 5;
  config.strips[0].count = 60;

  config.zoneCount = 1;
  config.zones = new Zone[1];
  
  config.zones[0].strip = 0;
  config.zones[0].start = 0;
  config.zones[0].length = 60;
  config.zones[0].reversed = false;

  for(int i=0; i<EVT_COUNT; i++) {
    strlcpy(config.zones[0].events[i].scriptName, "Solid", 32);
    config.zones[0].events[i].r = 255; 
    config.zones[0].events[i].g = 255;
    config.zones[0].events[i].b = 255;
    config.zones[0].events[i].speed = 128; 
    config.zones[0].events[i].delay = 0;
    config.zones[0].events[i].brightness = 255;
    config.zones[0].events[i].size = 128; 
  }
}

void loadConfig() {
  prefs.begin("klpro", false);
  uint32_t version = prefs.getUInt("cfgVer", 0);
  
  if(version != 102) { 
    prefs.end();
    Serial.println("[Config] No valid config found or version mismatch. Loading Defaults...");
    defaultConfig();
    saveConfig();
    return;
  }

  if (prefs.getBytes("hostname", config.hostname, 32) == 0) {
      strcpy(config.hostname, "lumenraker");
  }

  prefs.getBytes("wifiSSID", config.wifiSSID, 32);
  prefs.getBytes("wifiPASS", config.wifiPASS, 64);
  prefs.getBytes("mHost", config.moonrakerHost, 64);
  config.moonrakerPort = prefs.getInt("mPort", 7125);

  prefs.getBytes("webUser", config.webUser, 32);
  prefs.getBytes("webPass", config.webPass, 65);
  if(strlen(config.webUser) == 0) strcpy(config.webUser, "admin");

  prefs.getBytes("mqttHost", config.mqttHost, 64);
  config.mqttPort = prefs.getInt("mqttPort", 1883);
  prefs.getBytes("mqttUser", config.mqttUser, 32);
  prefs.getBytes("mqttPass", config.mqttPass, 64);

  prefs.getBytes("uiBg", config.uiBg, 16);
  prefs.getBytes("uiPan", config.uiPanel, 16);
  prefs.getBytes("uiCrd", config.uiCard, 16);
  prefs.getBytes("uiTxt", config.uiText, 16);
  prefs.getBytes("uiDim", config.uiDim, 16);
  prefs.getBytes("uiAcc", config.uiAccent, 16);
  prefs.getBytes("uiBdr", config.uiBorder, 16);
  prefs.getBytes("uiDng", config.uiDanger, 16);
  prefs.getBytes("uiSuc", config.uiSuccess, 16);

  config.brightness = prefs.getInt("br", 128);
  config.fadeDurationMs = prefs.getInt("fadeDur", 750);
  config.colorTempK = prefs.getInt("colTemp", 6500);
  config.stripCount = prefs.getInt("sCnt", 1);
  config.zoneCount = prefs.getInt("zCnt", 1);

  prefs.getBytes("strips", config.strips, sizeof(config.strips));
  
  config.zones = new Zone[config.zoneCount];
  for (int i = 0; i < config.zoneCount; i++) {
      char key[16]; 
      snprintf(key, sizeof(key), "z%d", i);
      prefs.getBytes(key, &config.zones[i], sizeof(Zone));
  }
  
  prefs.end();
  
  if(config.stripCount == 0 || config.stripCount > MAX_STRIPS) {
    defaultConfig();
  }
}

void saveConfig() {
  prefs.begin("klpro", false);
  prefs.putUInt("cfgVer", 102);
  prefs.putBytes("hostname", config.hostname, 32);
  prefs.putBytes("wifiSSID", config.wifiSSID, 32);
  prefs.putBytes("wifiPASS", config.wifiPASS, 64);
  prefs.putBytes("mHost", config.moonrakerHost, 64);
  prefs.putInt("mPort", config.moonrakerPort);

  prefs.putBytes("uiBg", config.uiBg, 16);
  prefs.putBytes("uiPan", config.uiPanel, 16);
  prefs.putBytes("uiCrd", config.uiCard, 16);
  prefs.putBytes("uiTxt", config.uiText, 16);
  prefs.putBytes("uiDim", config.uiDim, 16);
  prefs.putBytes("uiAcc", config.uiAccent, 16);
  prefs.putBytes("uiBdr", config.uiBorder, 16);
  prefs.putBytes("uiDng", config.uiDanger, 16);
  prefs.putBytes("uiSuc", config.uiSuccess, 16);

  prefs.putInt("br", config.brightness);
  prefs.putInt("fadeDur", config.fadeDurationMs);
  prefs.putInt("colTemp", config.colorTempK);
  prefs.putBytes("webUser", config.webUser, 32);
  prefs.putBytes("webPass", config.webPass, 65);
  
  prefs.putBytes("mqttHost", config.mqttHost, 64);
  prefs.putInt("mqttPort", config.mqttPort);
  prefs.putBytes("mqttUser", config.mqttUser, 32);
  prefs.putBytes("mqttPass", config.mqttPass, 64);
  prefs.putInt("sCnt", config.stripCount);
  prefs.putInt("zCnt", config.zoneCount);

  prefs.putBytes("strips", config.strips, sizeof(config.strips));
  
  for (int i = 0; i < config.zoneCount; i++) {
      char key[16]; 
      snprintf(key, sizeof(key), "z%d", i);
      prefs.putBytes(key, &config.zones[i], sizeof(Zone));
  }
  
  prefs.end();
  Serial.println("[Config] Saved to NVM");
}

void setupFS() {
    if(!LittleFS.begin(true)){
        Serial.println("LittleFS Mount Failed");
        return;
    }
}