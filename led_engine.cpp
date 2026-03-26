#include <NeoPixelBus.h>
#include <LittleFS.h>
#include "config.h"
#include "lua_engine.h"

class StripWrapper {
public:
    virtual ~StripWrapper() {};
    virtual void Begin() = 0;
    virtual void Show() = 0;
    virtual bool CanShow() = 0;
    virtual void SetPixelColor(uint16_t indexPixel, RgbColor color) = 0;
};

template<typename T_METHOD>
class StripImpl : public StripWrapper {
    NeoPixelBus<NeoGrbFeature, T_METHOD> strip;
public:
    StripImpl(uint16_t count, uint8_t pin) : strip(count, pin) {}
    void Begin() override { strip.Begin(); }
    void Show() override { strip.Show(); }
    bool CanShow() override { return strip.CanShow(); }
    void SetPixelColor(uint16_t indexPixel, RgbColor color) override { strip.SetPixelColor(indexPixel, color); }
};

StripWrapper* hwStrips[MAX_STRIPS] = {nullptr};

CRGB* stripBuffers[MAX_STRIPS] = {nullptr};
CRGB** universeMap = nullptr; 
uint16_t totalUniverseLeds = 0;

volatile EventType currentEvent = EVT_IDLE;
float progress[16];
float currentTemp = 0;
float targetTemp = 0;

int* cachedScriptRef = nullptr;
char (*cachedPath)[64] = nullptr;

bool currentReversed = false;
uint16_t currentZoneStart = 0;
uint16_t currentCount = 0;

const EffectConfig* current_lua_config = nullptr;
int currentFPS = 0;

void allocateBuffers() {
    if (universeMap) { delete[] universeMap; universeMap = nullptr; }
    for (int i = 0; i < MAX_STRIPS; i++) {
        if (stripBuffers[i]) { delete[] stripBuffers[i]; stripBuffers[i] = nullptr; }
    }
    
    totalUniverseLeds = 0;
    for (int i = 0; i < config.stripCount; i++) {
        int c = config.strips[i].count;
        if (c > 0) {
            stripBuffers[i] = new CRGB[c];
            totalUniverseLeds += c;
        }
    }
    
    if (totalUniverseLeds > 0) {
        universeMap = new CRGB*[totalUniverseLeds];
        uint16_t mapIndex = 0;
        for (int i = 0; i < config.stripCount; i++) {
            int c = config.strips[i].count;
            for (int j = 0; j < c; j++) {
                universeMap[mapIndex++] = &stripBuffers[i][j];
            }
        }
    }

    if (cachedScriptRef) delete[] cachedScriptRef;
    if (cachedPath) delete[] cachedPath;
    
    cachedScriptRef = new int[config.zoneCount];
    cachedPath = new char[config.zoneCount][64];
    
    for (int i = 0; i < config.zoneCount; i++) {
        cachedScriptRef[i] = LUA_NOREF;
        memset(cachedPath[i], 0, 64);
    }
}

void ledsInit() {
    allocateBuffers(); 

    for (int i = 0; i < config.stripCount; i++) {
        int p = config.strips[i].gpio;
        int c = config.strips[i].count;
        
        if (hwStrips[i]) { delete hwStrips[i]; hwStrips[i] = nullptr; }

        switch (i) {
            case 0: hwStrips[i] = new StripImpl<NeoEsp32I2s0800KbpsMethod>(c, p); break;
            case 1: hwStrips[i] = new StripImpl<NeoEsp32I2s1800KbpsMethod>(c, p); break;
            case 2: hwStrips[i] = new StripImpl<NeoEsp32Rmt0800KbpsMethod>(c, p); break;
            case 3: hwStrips[i] = new StripImpl<NeoEsp32Rmt1800KbpsMethod>(c, p); break;
            case 4: hwStrips[i] = new StripImpl<NeoEsp32Rmt2800KbpsMethod>(c, p); break;
            case 5: hwStrips[i] = new StripImpl<NeoEsp32Rmt3800KbpsMethod>(c, p); break;
            case 6: hwStrips[i] = new StripImpl<NeoEsp32Rmt4800KbpsMethod>(c, p); break;
            case 7: hwStrips[i] = new StripImpl<NeoEsp32Rmt5800KbpsMethod>(c, p); break;
            case 8: hwStrips[i] = new StripImpl<NeoEsp32Rmt6800KbpsMethod>(c, p); break;
            case 9: hwStrips[i] = new StripImpl<NeoEsp32Rmt7800KbpsMethod>(c, p); break;
        }

        if (hwStrips[i]) {
            hwStrips[i]->Begin();
            hwStrips[i]->Show();
        }
    }
    initLua();
}

void ledTask(void* pv) {
    unsigned long lastFpsTime = millis();
    int frameCount = 0;

    while(true) {
        bool showNeeded = false;

        for (int i = 0; i < config.zoneCount; i++) {
            Zone &z = config.zones[i];
            EffectConfig &ef = z.events[currentEvent];
            
            if (strlen(ef.scriptName) == 0) continue;

            currentZoneStart = z.start; 
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
                if (executeLuaFast(cachedScriptRef[i], i)) {
                    showNeeded = true;
                }
            }
            current_lua_config = nullptr;
        }

        if (showNeeded) {
            for (int i = 0; i < config.stripCount; i++) {
                if (hwStrips[i]) {
                    while (!hwStrips[i]->CanShow()) { taskYIELD(); }
                }
            }

            uint16_t masterBrightness = config.brightness + 1;
            
            for (int i = 0; i < config.stripCount; i++) {
                if (hwStrips[i]) {
                    for (int j = 0; j < config.strips[i].count; j++) {
                        RgbColor hwColor(
                            (stripBuffers[i][j].r * masterBrightness) >> 8,
                            (stripBuffers[i][j].g * masterBrightness) >> 8,
                            (stripBuffers[i][j].b * masterBrightness) >> 8
                        );
                        hwStrips[i]->SetPixelColor(j, hwColor);
                    }
                    hwStrips[i]->Show();
                }
            }
            frameCount++;
        }
        
        unsigned long currentMillis = millis();
        if (currentMillis - lastFpsTime >= 1000) {
            currentFPS = frameCount;
            frameCount = 0;
            lastFpsTime = currentMillis;
            if (L_VM) { lua_gc(L_VM, LUA_GCSTEP, 10); }
        }
        taskYIELD();
    }
}