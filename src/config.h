#pragma once
#include <Arduino.h>

#define MAX_STRIPS 10

const String LUMEN_VERSION = "0.0.8-alpha7";

enum EventType { EVT_IDLE, EVT_PRINTING, EVT_HEATING, EVT_HEATING_EXTRUDER, EVT_HOMING, EVT_ERROR, EVT_SHUTDOWN, EVT_STREAMING, EVT_COUNT };
extern const char* const EventNames[];

struct EffectConfig {
  char scriptName[32]; 
  uint8_t r, g, b;
  uint8_t speed;
  uint8_t delay;
  uint8_t size;
  uint8_t brightness;
};

extern const EffectConfig* current_lua_config;

struct Strip { 
  int gpio; 
  int count; 
};

struct Zone {
  int strip; // [LEGACY]
  int start;
  int length; 
  bool reversed;
  EffectConfig events[EVT_COUNT]; 
};

struct Config {
  char hostname[32];
  char wifiSSID[32]; 
  char wifiPASS[64];
  char moonrakerHost[64];
  int moonrakerPort;
  int brightness;
  int fadeDurationMs;
  int colorTempK;
  char webUser[32];
  char webPass[65];
  
  char mqttHost[64];
  int mqttPort;
  char mqttUser[32];
  char mqttPass[64];
  
  char uiBg[16];
  char uiPanel[16];
  char uiCard[16];
  char uiText[16];
  char uiDim[16];
  char uiAccent[16];
  char uiBorder[16];
  char uiDanger[16];
  char uiSuccess[16];

  int stripCount;
  Strip strips[MAX_STRIPS];
  
  int zoneCount; 
  Zone* zones;
};

extern Config config;

void saveConfig();
void loadConfig();
void setupFS();

struct CRGB {
    union {
        struct {
            union { uint8_t r; uint8_t red; };
            union { uint8_t g; uint8_t green; };
            union { uint8_t b; uint8_t blue; };
        };
        uint8_t raw[3];
    };
    inline CRGB() __attribute__((always_inline)) : r(0), g(0), b(0) {}
    inline CRGB( uint8_t ir, uint8_t ig, uint8_t ib)  __attribute__((always_inline)) : r(ir), g(ig), b(ib) {}
};

extern int currentFPS;