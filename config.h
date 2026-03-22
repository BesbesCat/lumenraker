#pragma once
#include <Arduino.h>

#define MAX_STRIPS 10
#define MAX_ZONES 8
#define MAX_LEDS 256

const String LUMEN_VERSION = "0.0.4-alpha";

enum EventType { EVT_IDLE, EVT_PRINTING, EVT_HEATING, EVT_HOMING, EVT_ERROR, EVT_SHUTDOWN, EVT_COUNT };
extern const char* const EventNames[];
struct EffectConfig {
  char scriptName[32]; 
  uint8_t r, g, b;
  uint8_t speed;   // 0-255
  uint8_t delay;   // 0-255
  uint8_t size;    // 0-255
  uint8_t brightness; // 0-255
};

extern const EffectConfig* current_lua_config;

struct Strip { int gpio; int count; };
struct Zone {
  int strip; int start; int length; bool reversed;
  EffectConfig events[EVT_COUNT]; 
};

struct Config {
  char wifiSSID[32]; char wifiPASS[64];
  char moonrakerHost[64]; int moonrakerPort;
  int brightness;
  int stripCount; Strip strips[MAX_STRIPS];
  int zoneCount; Zone zones[MAX_ZONES];
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