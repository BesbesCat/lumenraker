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

CRGB leds[MAX_STRIPS][MAX_LEDS];

CRGB* currentLeds = nullptr;
uint16_t currentCount = 0;

volatile EventType currentEvent = EVT_IDLE;
float progress[16];
float currentTemp = 0;
float targetTemp = 0;

int cachedScriptRef[MAX_ZONES] = {LUA_NOREF}; 
char cachedPath[MAX_ZONES][64] = {0};

bool currentReversed = false;
const EffectConfig* current_lua_config = nullptr;
int currentFPS = 0;

void ledsInit() {
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
    unsigned long lastFpsTime = millis();
    int frameCount = 0;

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
                if (executeLuaFast(cachedScriptRef[i], i)) {
                    showNeeded = true;
                }
            }
            current_lua_config = nullptr;
        }

        if (showNeeded) {
            for (int i = 0; i < config.stripCount; i++) {
                if (hwStrips[i]) {
                    while (!hwStrips[i]->CanShow()) {
                        taskYIELD();
                    }
                }
            }

            float masterBrightness = config.brightness / 255.0f;
            
            for (int i = 0; i < config.stripCount; i++) {
                if (hwStrips[i]) {
                    for (int j = 0; j < config.strips[i].count; j++) {
                        RgbColor hwColor(
                            leds[i][j].r * masterBrightness,
                            leds[i][j].g * masterBrightness,
                            leds[i][j].b * masterBrightness
                        );
                        hwStrips[i]->SetPixelColor(j, hwColor);
                    }
                    
                    hwStrips[i]->Show();
                    frameCount++;
                }
            }
        }
        unsigned long currentMillis = millis();
        if (currentMillis - lastFpsTime >= 1000) {
            currentFPS = frameCount;
            frameCount = 0;
            lastFpsTime = currentMillis;
        }
        taskYIELD();
    }
}