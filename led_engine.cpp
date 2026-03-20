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

int cachedScriptRef[MAX_ZONES] = {LUA_NOREF}; 
char cachedPath[MAX_ZONES][64] = {0};
TickType_t xLastWakeTime;
const TickType_t xFrequency = pdMS_TO_TICKS(8);

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
    FastLED.setDither(BINARY_DITHER);
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
    xLastWakeTime = xTaskGetTickCount();

    while(true) {
        bool showNeeded = false;
        
        if (L_VM) {
            lua_gc(L_VM, LUA_GCSTEP, 2); 
        }

        for (int i = 0; i < config.zoneCount; i++) {
            Zone &z = config.zones[i];
            EffectConfig &ef = z.events[currentEvent];
            
            if (strlen(ef.scriptName) == 0) continue;

            currentLeds = &leds[z.strip][z.start];
            currentCount = z.length;
            currentReversed = z.reversed;
            
            char path[64];
            snprintf(path, sizeof(path), "/fx/%s.lua", ef.scriptName);
            
            if (strcmp(path, cachedPath[i]) != 0) {
                if (cachedScriptRef[i] != LUA_NOREF) {
                    luaL_unref(L_VM, LUA_REGISTRYINDEX, cachedScriptRef[i]);
                    cachedScriptRef[i] = LUA_NOREF;
                }

                File f = LittleFS.open(path, "r");
                if (f && f.size() > 0) {
                    String scriptStr = f.readString();
                    f.close();
                    
                    if (luaL_loadstring(L_VM, scriptStr.c_str()) == LUA_OK) {
                        cachedScriptRef[i] = luaL_ref(L_VM, LUA_REGISTRYINDEX);
                        strncpy(cachedPath[i], path, sizeof(cachedPath[i]));
                    } else {
                        Serial.printf("[LUA] Compile Error: %s\n", lua_tostring(L_VM, -1));
                        lua_pop(L_VM, 1);
                    }
                }
            }

            current_lua_config = &ef;
            if (cachedScriptRef[i] != LUA_NOREF) {
                update_lua_config(ef);
                
                if (executeLuaFast(cachedScriptRef[i], i)) {
                    showNeeded = true;
                }
            }
            current_lua_config = nullptr;
        }

        if (showNeeded) {
            FastLED.show();
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency); 
    }
}