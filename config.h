#pragma once
#include <Arduino.h>

#define MAX_STRIPS 8
#define MAX_ZONES 8
#define MAX_LEDS 256

const String LUMEN_VERSION = "0.0.2-alpha";

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

static const EffectConfig* current_lua_config = nullptr;

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