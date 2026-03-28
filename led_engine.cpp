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

int* cachedScriptRef = nullptr;
char (*cachedPath)[64] = nullptr;

bool currentReversed = false;
uint16_t currentZoneStart = 0;
uint16_t currentCount = 0;

const EffectConfig* current_lua_config = nullptr;
int currentFPS = 0;

CRGB* snapshotBuffer = nullptr;
EventType lastGlobalEvent = EVT_IDLE;
unsigned long fadeStartTime = 0;
bool isFading = false;

uint8_t global_ctR = 255;
uint8_t global_ctG = 255;
uint8_t global_ctB = 255;

const uint8_t PROGMEM gamma8[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255
};

void getKelvinRGB(int kelvin, uint8_t &r, uint8_t &g, uint8_t &b) {
    float temp = kelvin / 100.0f;
    float red, green, blue;

    if (temp <= 66.0f) {
        red = 255.0f;
        green = temp;
        green = 99.4708025861f * log(green) - 161.1195681661f;
        if (temp <= 19.0f) { blue = 0.0f; } else {
            blue = temp - 10.0f;
            blue = 138.5177312231f * log(blue) - 305.0447927307f;
        }
    } else {
        red = temp - 60.0f;
        red = 329.698727446f * pow(red, -0.1332047592f);
        green = temp - 60.0f;
        green = 288.1221695283f * pow(green, -0.0755148492f);
        blue = 255.0f;
    }
    r = constrain(red, 0, 255);
    g = constrain(green, 0, 255);
    b = constrain(blue, 0, 255);
}

void allocateBuffers() {
    if (universeMap) { delete[] universeMap; universeMap = nullptr; }
    if (snapshotBuffer) { delete[] snapshotBuffer; snapshotBuffer = nullptr; }

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
        snapshotBuffer = new CRGB[totalUniverseLeds];
        
        uint16_t mapIndex = 0;
        for (int i = 0; i < config.stripCount; i++) {
            int c = config.strips[i].count;
            for (int j = 0; j < c; j++) {
                universeMap[mapIndex++] = &stripBuffers[i][j];
            }
        }
        memset(snapshotBuffer, 0, totalUniverseLeds * sizeof(CRGB));
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
    getKelvinRGB(config.colorTempK, global_ctR, global_ctG, global_ctB);

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
    unsigned long lastGCTime = lastFpsTime;
    int frameCount = 0;

    // --- SHADOW LOADING ARCHITECTURE ---
    static EventType targetEvent = currentEvent;
    static bool isShadowLoading = false;
    static int loadZoneIndex = 0;
    
    struct PendingScript {
        char* data;
        size_t size;
        char path[64];
    };
    static PendingScript shadowBuffers[32]; // Accommodates up to 32 zones safely

    static bool initShadow = false;
    if (!initShadow) {
        for(int i=0; i<32; i++) {
            shadowBuffers[i].data = nullptr;
            shadowBuffers[i].size = 0;
            memset(shadowBuffers[i].path, 0, 64);
        }
        initShadow = true;
    }

    while(true) {
        bool showNeeded = false;
        bool triggerFadeClock = false;

        // --- 1. DETECT EVENT CHANGE OR ABORT ---
        if (currentEvent != targetEvent) {
            targetEvent = currentEvent;
            
            // If we were already loading an event and it changed again mid-load, abort and free memory!
            if (isShadowLoading) {
                for (int i = 0; i < config.zoneCount; i++) {
                    if (shadowBuffers[i].data) {
                        free(shadowBuffers[i].data);
                        shadowBuffers[i].data = nullptr;
                    }
                }
            }
            
            isShadowLoading = true;
            loadZoneIndex = 0;
        }

        // --- 2. BACKGROUND SHADOW LOADING ---
        if (isShadowLoading) {
            if (loadZoneIndex < config.zoneCount) {
                // Load ONE file per frame into RAM so we don't block the running animation
                int i = loadZoneIndex;
                Zone &z = config.zones[i];
                EffectConfig &ef = z.events[targetEvent];
                
                shadowBuffers[i].data = nullptr;
                shadowBuffers[i].size = 0;
                memset(shadowBuffers[i].path, 0, 64);
                
                if (strlen(ef.scriptName) > 0) {
                    snprintf(shadowBuffers[i].path, 64, "/fxc/%s.luac", ef.scriptName);
                    File f = LittleFS.open(shadowBuffers[i].path, "r");
                    if (f) {
                        size_t sz = f.size();
                        if (sz > 0) {
                            shadowBuffers[i].data = (char*)malloc(sz);
                            if (shadowBuffers[i].data) {
                                f.readBytes(shadowBuffers[i].data, sz);
                                shadowBuffers[i].size = sz;
                            }
                        }
                        f.close();
                    }
                }
                loadZoneIndex++;
            } 
            else {
                // ALL FILES LOADED! EXECUTE THE HOT-SWAP.
                
                // A. Snapshot the exact final frame of the old effect for seamless crossfading
                if (totalUniverseLeds > 0 && universeMap && snapshotBuffer) {
                    for (uint16_t idx = 0; idx < totalUniverseLeds; idx++) {
                        snapshotBuffer[idx] = *(universeMap[idx]);
                    }
                }

                // B. Nuke the garbage.
                if (L_VM) { 
                    lua_gc(L_VM, LUA_GCCOLLECT, 0); 
                } 

                // C. Load scripts instantly from RAM buffers and free the memory
                for (int i = 0; i < config.zoneCount; i++) {
                    cachedScriptRef[i] = LUA_NOREF;
                    memset(cachedPath[i], 0, 64);

                    if (shadowBuffers[i].data) {
                        if (luaL_loadbuffer(L_VM, shadowBuffers[i].data, shadowBuffers[i].size, shadowBuffers[i].path) == LUA_OK) {
                            cachedScriptRef[i] = luaL_ref(L_VM, LUA_REGISTRYINDEX);
                            strncpy(cachedPath[i], shadowBuffers[i].path, 64);
                            Serial.printf("[LUA]: Swapped %s from RAM\n", shadowBuffers[i].path);
                        } else {
                            Serial.printf("[LUA] RAM Load Error: %s\n", lua_tostring(L_VM, -1));
                            lua_pop(L_VM, 1);
                        }
                        free(shadowBuffers[i].data);
                        shadowBuffers[i].data = nullptr;
                    } else {
                        // If scriptName was empty or file missing, blackout the zone
                        Zone &z = config.zones[i];
                        for (int j = 0; j < z.length; j++) {
                            int ledIdx = z.start + j;
                            if (ledIdx < totalUniverseLeds && universeMap[ledIdx]) {
                                *universeMap[ledIdx] = CRGB{0, 0, 0};
                            }
                        }
                        showNeeded = true;
                    }
                }

                // D. Finalize state swap and trigger the crossfade!
                isShadowLoading = false;
                lastGlobalEvent = targetEvent;
                isFading = true;
                triggerFadeClock = true;
            }
        }

        // --- 3. EXECUTE LUA ---
        // Notice we always use `lastGlobalEvent` here. If we are currently loading,
        // this guarantees the old animation continues playing flawlessly.
        for (int i = 0; i < config.zoneCount; i++) {
            Zone &z = config.zones[i];
            EffectConfig &ef = z.events[lastGlobalEvent]; 

            // (Manual Web UI Edit Override) - Only runs if we aren't mid-transition
            if (!isShadowLoading && strlen(ef.scriptName) > 0) {
                char path[64];
                snprintf(path, sizeof(path), "/fxc/%s.luac", ef.scriptName);
                if (strcmp(path, cachedPath[i]) != 0) {
                    if (cachedScriptRef[i] != LUA_NOREF) {
                        luaL_unref(L_VM, LUA_REGISTRYINDEX, cachedScriptRef[i]);
                        cachedScriptRef[i] = LUA_NOREF;
                    }
                    File f = LittleFS.open(path, "r");
                    if (f) {
                        size_t sz = f.size();
                        if (sz > 0) {
                            char* buf = (char*)malloc(sz);
                            if (buf) {
                                f.readBytes(buf, sz);
                                if (luaL_loadbuffer(L_VM, buf, sz, path) == LUA_OK) {
                                    cachedScriptRef[i] = luaL_ref(L_VM, LUA_REGISTRYINDEX);
                                } else { lua_pop(L_VM, 1); }
                                free(buf);
                            }
                        }
                        f.close();
                    }
                    strncpy(cachedPath[i], path, 64);
                }
            }

            currentZoneStart = z.start; 
            currentCount = z.length;
            currentReversed = z.reversed;
            current_lua_config = &ef;

            if (cachedScriptRef[i] != LUA_NOREF) {                
                if (executeLuaFast(cachedScriptRef[i], i)) {
                    showNeeded = true;
                }
            }
            current_lua_config = nullptr;
        }

        // --- 4. FADE LOGIC ---
        if (triggerFadeClock) {
            fadeStartTime = millis();
        }

        uint8_t fadeRatio = 255;
        if (isFading) {
            int currentFade = config.fadeDurationMs;
            if (currentFade <= 0) {
                isFading = false;
            } else {
                unsigned long elapsed = millis() - fadeStartTime;
                if (elapsed >= currentFade) {
                    isFading = false;
                } else {
                    fadeRatio = (elapsed * 255) / currentFade;
                }
                showNeeded = true;
            }
        }

        // --- 5. HARDWARE RENDER ---
        if (showNeeded) {
            for (int i = 0; i < config.stripCount; i++) {
                if (hwStrips[i]) {
                    while (!hwStrips[i]->CanShow()) { taskYIELD(); }
                }
            }

            uint16_t masterBrightness = config.brightness + 1;
            uint16_t globalIdx = 0;

            for (int i = 0; i < config.stripCount; i++) {
                if (hwStrips[i]) {
                    for (int j = 0; j < config.strips[i].count; j++) {
                        CRGB liveColor = stripBuffers[i][j];
                        
                        if (isFading && snapshotBuffer) {
                            CRGB snapColor = snapshotBuffer[globalIdx];
                            uint8_t invRatio = 255 - fadeRatio;
                            liveColor.r = (snapColor.r * invRatio + liveColor.r * fadeRatio) >> 8;
                            liveColor.g = (snapColor.g * invRatio + liveColor.g * fadeRatio) >> 8;
                            liveColor.b = (snapColor.b * invRatio + liveColor.b * fadeRatio) >> 8;
                        }

                        uint8_t linR = (liveColor.r * masterBrightness * global_ctR) >> 16;
                        uint8_t linG = (liveColor.g * masterBrightness * global_ctG) >> 16;
                        uint8_t linB = (liveColor.b * masterBrightness * global_ctB) >> 16;
                        
                        RgbColor hwColor(
                            pgm_read_byte(&gamma8[linR]),
                            pgm_read_byte(&gamma8[linG]),
                            pgm_read_byte(&gamma8[linB])
                        );

                        hwStrips[i]->SetPixelColor(j, hwColor);
                        globalIdx++;
                    }
                    hwStrips[i]->Show();
                } else {
                    globalIdx += config.strips[i].count;
                }
            }
            frameCount++;
        }

        // --- 6. HOUSEKEEPING ---
        unsigned long currentMillis = millis();
        if (currentMillis - lastGCTime >= 60000) {
            if (L_VM) { lua_gc(L_VM, LUA_GCCOLLECT, 0); } 
            lastGCTime = currentMillis;
        }
        if (currentMillis - lastFpsTime >= 1000) {
            currentFPS = frameCount;
            frameCount = 0;
            lastFpsTime = currentMillis;
        }
        taskYIELD();
    }
}