#include <NeoPixelBus.h>
#include <LittleFS.h>
#include <Arduino.h>
#include "config.h"
#include "lua_engine.h"
#include "E131Receiver.h" 

// === AUTO-DETECT PSRAM ALLOCATOR ===
#if defined(BOARD_HAS_PSRAM)
    #define SAFE_ALLOC(sz) (psramFound() ? ps_malloc(sz) : malloc(sz))
#else
    #define SAFE_ALLOC(sz) malloc(sz)
#endif
// ===================================

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
volatile bool forceReload = true;
volatile bool wbReload = false;
volatile bool ledsDirty = false;

int* cachedScriptRef = nullptr;
char (*cachedPath)[64] = nullptr;

bool currentReversed = false;
uint16_t currentZoneStart = 0;
uint16_t currentCount = 0;

const EffectConfig* current_lua_config = nullptr;
int currentFPS = 0;

CRGB* snapshotBuffer = nullptr;
CRGB* streamBuffer = nullptr; 

EventType lastGlobalEvent = EVT_IDLE;
unsigned long fadeStartTime = 0;
bool isFading = false;

uint8_t global_ctR = 255;
uint8_t global_ctG = 255;
uint8_t global_ctB = 255;

E131Receiver streamReceiver;

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
    if (universeMap) { free(universeMap); universeMap = nullptr; }
    if (snapshotBuffer) { free(snapshotBuffer); snapshotBuffer = nullptr; }
    if (streamBuffer) { free(streamBuffer); streamBuffer = nullptr; }

    for (int i = 0; i < MAX_STRIPS; i++) {
        if (stripBuffers[i]) { free(stripBuffers[i]); stripBuffers[i] = nullptr; }
    }
    
    totalUniverseLeds = 0;
    for (int i = 0; i < config.stripCount; i++) {
        int c = config.strips[i].count;
        if (c > 0) {
            stripBuffers[i] = (CRGB*)SAFE_ALLOC(c * sizeof(CRGB));
            if (stripBuffers[i]) memset(stripBuffers[i], 0, c * sizeof(CRGB));
            totalUniverseLeds += c;
        }
    }
    
    if (totalUniverseLeds > 0) {
        universeMap = (CRGB**)SAFE_ALLOC(totalUniverseLeds * sizeof(CRGB*));
        snapshotBuffer = (CRGB*)SAFE_ALLOC(totalUniverseLeds * sizeof(CRGB));
        streamBuffer = (CRGB*)SAFE_ALLOC(totalUniverseLeds * sizeof(CRGB));
        
        if (universeMap) {
            uint16_t mapIndex = 0;
            for (int i = 0; i < config.stripCount; i++) {
                int c = config.strips[i].count;
                for (int j = 0; j < c; j++) {
                    universeMap[mapIndex++] = &stripBuffers[i][j];
                }
            }
        }
        if (snapshotBuffer) memset(snapshotBuffer, 0, totalUniverseLeds * sizeof(CRGB));
        if (streamBuffer) memset(streamBuffer, 0, totalUniverseLeds * sizeof(CRGB));
    }

    if (cachedScriptRef) free(cachedScriptRef);
    if (cachedPath) free(cachedPath);

    cachedScriptRef = (int*)SAFE_ALLOC(config.zoneCount * sizeof(int));
    cachedPath = (char(*)[64])SAFE_ALLOC(config.zoneCount * 64);

    for (int i = 0; i < config.zoneCount; i++) {
        if (cachedScriptRef) cachedScriptRef[i] = LUA_NOREF;
        if (cachedPath) memset(cachedPath[i], 0, 64);
    }
}

void handleStreamData(uint16_t universe, uint8_t* dmxData, uint16_t length) {
    if (!streamBuffer || totalUniverseLeds == 0) return;

    uint16_t startLedIndex = (universe - 1) * 170; 
    uint16_t ledsInPacket = length / 3;

    for (uint16_t i = 0; i < ledsInPacket; i++) {
        uint16_t globalIdx = startLedIndex + i;
        if (globalIdx < totalUniverseLeds) {
            streamBuffer[globalIdx].r = dmxData[i * 3];
            streamBuffer[globalIdx].g = dmxData[(i * 3) + 1];
            streamBuffer[globalIdx].b = dmxData[(i * 3) + 2];
        }
    }
}

void ledsInit() {
    allocateBuffers(); 
    getKelvinRGB(config.colorTempK, global_ctR, global_ctG, global_ctB);

    for (int i = 0; i < config.stripCount; i++) {
        int p = config.strips[i].gpio;
        int c = config.strips[i].count;
        
        if (hwStrips[i]) { delete hwStrips[i]; hwStrips[i] = nullptr; }

#if defined(CONFIG_IDF_TARGET_ESP32S3)
        switch (i) {
            case 0: hwStrips[i] = new StripImpl<NeoEsp32Rmt0800KbpsMethod>(c, p); break;
            case 1: hwStrips[i] = new StripImpl<NeoEsp32Rmt1800KbpsMethod>(c, p); break;
            case 2: hwStrips[i] = new StripImpl<NeoEsp32Rmt2800KbpsMethod>(c, p); break;
            case 3: hwStrips[i] = new StripImpl<NeoEsp32Rmt3800KbpsMethod>(c, p); break;
        }

#elif defined(CONFIG_IDF_TARGET_ESP32)
        switch (i) {
            case 0: hwStrips[i] = new StripImpl<NeoEsp32Rmt0800KbpsMethod>(c, p); break;
            case 1: hwStrips[i] = new StripImpl<NeoEsp32Rmt1800KbpsMethod>(c, p); break;
            case 2: hwStrips[i] = new StripImpl<NeoEsp32Rmt2800KbpsMethod>(c, p); break;
            case 3: hwStrips[i] = new StripImpl<NeoEsp32Rmt3800KbpsMethod>(c, p); break;
            case 4: hwStrips[i] = new StripImpl<NeoEsp32Rmt4800KbpsMethod>(c, p); break;
            case 5: hwStrips[i] = new StripImpl<NeoEsp32Rmt5800KbpsMethod>(c, p); break;
            case 6: hwStrips[i] = new StripImpl<NeoEsp32Rmt6800KbpsMethod>(c, p); break;
            case 7: hwStrips[i] = new StripImpl<NeoEsp32Rmt7800KbpsMethod>(c, p); break;
            case 8: hwStrips[i] = new StripImpl<NeoEsp32I2s0800KbpsMethod>(c, p); break;
            case 9: hwStrips[i] = new StripImpl<NeoEsp32I2s1800KbpsMethod>(c, p); break;
        }
#else
        #error "Unsupported ESP32 architecture! Please add bindings to ledsInit()."
#endif

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

    static EventType targetEvent = currentEvent;
    static bool isShadowLoading = false;
    static int loadZoneIndex = 0;

    struct PendingScript {
        char* data;
        size_t size;
        char path[64];
    };
    static PendingScript shadowBuffers[32];

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
        ledsDirty = false;

        if (currentEvent != targetEvent || forceReload) {
            targetEvent = currentEvent;
            if(wbReload == true) {
                getKelvinRGB(config.colorTempK, global_ctR, global_ctG, global_ctB);
            }

            forceReload = false; 
            
            if (isShadowLoading) {
                for (int i = 0; i < 32; i++) {
                    if (shadowBuffers[i].data) {
                        free(shadowBuffers[i].data);
                        shadowBuffers[i].data = nullptr;
                    }
                }
            }
            
            isShadowLoading = true;
            loadZoneIndex = 0;
        }

        if (isShadowLoading) {
            if (loadZoneIndex < config.zoneCount) {
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
                            shadowBuffers[i].data = (char*)SAFE_ALLOC(sz);
                            if (shadowBuffers[i].data) {
                                f.readBytes(shadowBuffers[i].data, sz);
                                shadowBuffers[i].size = sz;
                            }
                        }
                        f.close();
                    }
                }
                loadZoneIndex++;
            } else {
                if (totalUniverseLeds > 0 && universeMap && snapshotBuffer) {
                    for (uint16_t idx = 0; idx < totalUniverseLeds; idx++) {
                        snapshotBuffer[idx] = *(universeMap[idx]);
                    }
                }

                for (int i = 0; i < config.zoneCount; i++) {
                    if (L_VM) lua_settop(L_VM, 0); 
                    
                    bool loadSuccess = false;
                    int newScriptRef = LUA_NOREF;
                    
                    if (shadowBuffers[i].data) {
                        if (luaL_loadbuffer(L_VM, shadowBuffers[i].data, shadowBuffers[i].size, shadowBuffers[i].path) == LUA_OK) {
                            newScriptRef = luaL_ref(L_VM, LUA_REGISTRYINDEX); 
                            strncpy(cachedPath[i], shadowBuffers[i].path, 64);
                            loadSuccess = true;
                        } else {
                            Serial.printf("[LUA] RAM Load Error: %s\n", lua_tostring(L_VM, -1));
                            lua_pop(L_VM, 1);
                        }
                        free(shadowBuffers[i].data);
                        shadowBuffers[i].data = nullptr;
                    }

                    if (cachedScriptRef && cachedScriptRef[i] != LUA_NOREF) {
                        luaL_unref(L_VM, LUA_REGISTRYINDEX, cachedScriptRef[i]);
                    }
                    
                    if (cachedScriptRef) cachedScriptRef[i] = newScriptRef;

                    if (!loadSuccess) {
                        if (cachedPath) memset(cachedPath[i], 0, 64);
                        Zone &z = config.zones[i];
                        for (int j = 0; j < z.length; j++) {
                            int ledIdx = z.start + j;
                            if (ledIdx < totalUniverseLeds && universeMap && universeMap[ledIdx]) {
                                *universeMap[ledIdx] = CRGB{0, 0, 0};
                            }
                        }
                        showNeeded = true;
                    }
                }

                if (L_VM) { lua_gc(L_VM, LUA_GCCOLLECT, 0); }

                isShadowLoading = false;
                lastGlobalEvent = targetEvent;
                isFading = true;
                triggerFadeClock = true;
            }
        }

        for (int i = 0; i < config.zoneCount; i++) {
            Zone &z = config.zones[i];
            EffectConfig &ef = z.events[lastGlobalEvent]; 

            currentZoneStart = z.start; 
            currentCount = z.length;
            currentReversed = z.reversed;
            current_lua_config = &ef;

            if (cachedScriptRef && cachedScriptRef[i] != LUA_NOREF) {                
                executeLuaFast(cachedScriptRef[i], i);
            }
            current_lua_config = nullptr;
        }

        if (ledsDirty) {
            showNeeded = true;
        }

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