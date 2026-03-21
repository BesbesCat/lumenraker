#include "config.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

Preferences prefs;
Config config;

const char* const EventNames[] = {
  "Idle", 
  "Printing", 
  "Heating", 
  "Homing", 
  "Error", 
  "Shutdown"
};

const char* const EffectNames[] = {
  "Solid"
};

void defaultConfig() {
  memset(&config, 0, sizeof(Config));
  
  strcpy(config.moonrakerHost, "192.168.1.100");
  config.moonrakerPort = 7125;
  config.brightness = 128;
  
  config.stripCount = 1;
  config.strips[0].gpio = 5;
  config.strips[0].count = 60;

  config.zoneCount = 1;
  config.zones[0].strip = 0;
  config.zones[0].start = 0;
  config.zones[0].length = 60;
  config.zones[0].reversed = false;

  for(int i=0; i<EVT_COUNT; i++) {
    strlcpy(config.zones[0].events[i].scriptName, "Solid", 32);
    config.zones[0].events[i].r = 255; 
    config.zones[0].events[i].g = 255;
    config.zones[0].events[i].b = 255;
    config.zones[0].events[i].speed = 0; 
    config.zones[0].events[i].delay = 0;
    config.zones[0].events[i].brightness = 255;
    config.zones[0].events[i].size = 255; 
  }
}

void loadConfig() {
  prefs.begin("klpro", false);
  
  uint32_t version = prefs.getUInt("cfgVer", 0);

  if(version != 101) { 
    prefs.end();
    Serial.println("[Config] No valid config found. Loading Defaults...");
    defaultConfig();
    saveConfig();
    return; 
  }

  prefs.getBytes("wifiSSID", config.wifiSSID, 32);
  prefs.getBytes("wifiPASS", config.wifiPASS, 64);
  prefs.getBytes("mHost", config.moonrakerHost, 64);
  config.moonrakerPort = prefs.getInt("mPort", 7125);
  config.brightness = prefs.getInt("br", 128);
  config.stripCount = prefs.getInt("sCnt", 1);
  config.zoneCount = prefs.getInt("zCnt", 1);

  prefs.getBytes("strips", config.strips, sizeof(config.strips));
  prefs.getBytes("zones", config.zones, sizeof(config.zones));
  
  prefs.end();

  if(config.stripCount == 0 || config.stripCount > MAX_STRIPS) {
    defaultConfig();
  }
}

void saveConfig() {
  prefs.begin("klpro", false);
  prefs.putUInt("cfgVer", 101);
  prefs.putBytes("wifiSSID", config.wifiSSID, 32);
  prefs.putBytes("wifiPASS", config.wifiPASS, 64);
  prefs.putBytes("mHost", config.moonrakerHost, 64);
  prefs.putInt("mPort", config.moonrakerPort);
  prefs.putInt("br", config.brightness);

  prefs.putInt("sCnt", config.stripCount);
  prefs.putInt("zCnt", config.zoneCount);

  prefs.putBytes("strips", config.strips, sizeof(config.strips));
  prefs.putBytes("zones", config.zones, sizeof(config.zones));
  
  prefs.end();
  Serial.println("[Config] Saved to NVM");
}

void setupFS() {
    if(!LittleFS.begin(true)){
      Serial.println("LittleFS Mount Failed");
      return;
    }

    if (!LittleFS.exists("/fx")) {
        Serial.println("Creating /fx directory...");
        if (LittleFS.mkdir("/fx")) {
            Serial.println("Directory created successfully");
        } else {
            Serial.println("Failed to create directory");
        }
    }
}