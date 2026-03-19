#include <FastLED.h>
#include <LittleFS.h>
#include "config.h"
#include "lua_engine.h"

CRGB leds[MAX_STRIPS][MAX_LEDS];

CRGB* currentLeds = nullptr;
uint16_t currentCount = 0;

volatile EventType currentEvent = EVT_IDLE;
float progress[16];
float currentTemp = 0;
float targetTemp = 0;
String cachedScript[MAX_ZONES];
String cachedPath[MAX_ZONES];

bool currentReversed = false;

void ledsInit() {
    FastLED.clear();
    for (int i = 0; i < config.stripCount; i++) {
        int p = config.strips[i].gpio;
        int c = config.strips[i].count;
        
        if (p == 2)  FastLED.addLeds<WS2812, 2, GRB>(leds[i], c);
        else if (p == 4)  FastLED.addLeds<WS2812, 4, GRB>(leds[i], c);
        else if (p == 5)  FastLED.addLeds<WS2812, 5, GRB>(leds[i], c);
        else if (p == 12) FastLED.addLeds<WS2812, 12, GRB>(leds[i], c);
        else if (p == 13) FastLED.addLeds<WS2812, 13, GRB>(leds[i], c);
        else if (p == 14) FastLED.addLeds<WS2812, 14, GRB>(leds[i], c);
        else if (p == 15) FastLED.addLeds<WS2812, 15, GRB>(leds[i], c);
    }
    FastLED.setBrightness(config.brightness);
    initLua();
}
void dumpZoneData(int id, CRGB* leds, int count) {
    static uint32_t lastDump = 0;
    if (millis() - lastDump < 2000) return;
    if (id != 0) return;

    Serial.printf("\n--- DUMP ZONE %d (%d LEDs) ---\n", id, count);
    for (int i = 0; i < count; i++) {
        Serial.printf("#%02X%02X%02X ", leds[i].r, leds[i].g, leds[i].b);
        if ((i + 1) % 10 == 0) Serial.println();
    }
    Serial.println("\n--- END DUMP ---");
    if (id == config.zoneCount - 1) lastDump = millis(); 
}
void ledTask(void* pv) {
    while(true) {
        bool showNeeded = false;
        if (L_VM) {
            lua_settop(L_VM, 0);
        }

        for (int i = 0; i < config.zoneCount; i++) {
            Zone &z = config.zones[i];
            EffectConfig &ef = z.events[currentEvent];
            if (strlen(ef.scriptName) == 0) {
                Serial.printf("[LED] Script missing: %s\n", ef.scriptName);
                memset(&leds[z.strip][z.start], 0, z.length * sizeof(CRGB));
                showNeeded = true;
                continue;
            }
            currentLeds = &leds[z.strip][z.start];
            currentCount = z.length;
            currentReversed = z.reversed;
            update_lua_config(ef);
            char path[64];
            
            snprintf(path, sizeof(path), "/fx/%s.lua", ef.scriptName);
            if (String(path) != cachedPath[i]) {
                File f = LittleFS.open(path, "r");
                if (!f) {
                    Serial.printf("[LED] Script missing: %s\n", path);
                    memset(currentLeds, 0, currentCount * sizeof(CRGB));
                    continue;
                }
            
                if (f.size() == 0) {
                    Serial.printf("[LED] Script empty: %s\n", path);
                    f.close();
                    memset(currentLeds, 0, currentCount * sizeof(CRGB));
                    continue;
                }
                
                cachedScript[i] = f.readString();
                cachedPath[i] = path;
            }
            if (executeLuaSafe(cachedScript[i].c_str(), i)) {
                showNeeded = true;
                //dumpZoneData(i, currentLeds, currentCount);
            } else {
                Serial.printf("[LED] Script failed: %s\n",  path);
                memset(currentLeds, 0, currentCount * sizeof(CRGB));
            }
        }

        if (showNeeded) {
            FastLED.show();
        }
        
        vTaskDelay(pdMS_TO_TICKS(16));
    }
}