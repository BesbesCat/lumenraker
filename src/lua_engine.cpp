#include <lualib.h>
#include <lauxlib.h>
#include <LittleFS.h>
#include <Arduino.h>
#include "config.h"
#include "lua_engine.h"

// === AUTO-DETECT PSRAM ALLOCATOR ===
#if defined(BOARD_HAS_PSRAM)
    #define SAFE_ALLOC(sz) (psramFound() ? ps_malloc(sz) : malloc(sz))
#else
    #define SAFE_ALLOC(sz) malloc(sz)
#endif
// ===================================

#define MAX_PALETTE_COLORS 16
#define PREC_MUL 16777216.0f // 24-bit Precision Multiplier
#define PREC_MAX 0xFFFFFF    // 24-bit Bitmask

extern char lastMoonrakerJson[1024];
extern volatile EventType currentEvent;
extern EventType lastGlobalEvent;
extern int* cachedScriptRef;

float progress[16];
float currentTemp;
float targetTemp;

extern CRGB** universeMap;
extern CRGB* streamBuffer; 
extern uint16_t totalUniverseLeds;
extern uint16_t currentZoneStart;
extern uint16_t currentCount;
extern bool ledsDirty;

lua_State* L_VM = nullptr;
String lastLuaDebug = "";

static int* last_bound_script = nullptr;
static EventType* last_event = nullptr;

static int luaBytecodeWriter(lua_State *L, const void* p, size_t size, void* u) {
    File* f = (File*)u;
    return (f->write((const uint8_t*)p, size) != size) ? 1 : 0;
}

bool compileLuaScript(const char* srcPath, const char* dstPath) {
    File f = LittleFS.open(srcPath, "r");
    if (!f) return false;
    
    size_t size = f.size();
    char* buf = (char*)SAFE_ALLOC(size);
    if (!buf) { f.close(); return false; }
    f.readBytes(buf, size);
    f.close();

    lua_State *C_VM = luaL_newstate(); 
    if (!C_VM) { 
        free(buf); 
        return false; 
    }
    
    if (luaL_loadbuffer(C_VM, buf, size, srcPath) != LUA_OK) {
        Serial.printf("[LUA] Compile Error in %s: %s\n", srcPath, lua_tostring(C_VM, -1));
        lua_close(C_VM);
        free(buf);
        return false;
    }
    free(buf);

    File df = LittleFS.open(dstPath, "w");
    if (!df) {
        lua_close(C_VM);
        return false;
    }
    
    #if LUA_VERSION_NUM >= 503
        lua_dump(C_VM, luaBytecodeWriter, &df, 1); 
    #else
        lua_dump(C_VM, luaBytecodeWriter, &df); 
    #endif
    
    df.close();
    
    lua_close(C_VM); 
    return true;
}

void checkAndCompileAllScripts() {
    if (!LittleFS.exists("/fxc")) {
        LittleFS.mkdir("/fxc");
    }
    
    File root = LittleFS.open("/fx");
    if (!root || !root.isDirectory()) return;

    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String fileName = String(file.name());
            String srcPath = fileName.startsWith("/") ? fileName : "/fx/" + fileName;
            
            if (srcPath.endsWith(".lua")) {
                int slashIdx = srcPath.lastIndexOf('/');
                int dotIdx = srcPath.lastIndexOf('.');
                String baseName = srcPath.substring(slashIdx + 1, dotIdx);
                String dstPath = "/fxc/" + baseName + ".luac";
                compileLuaScript(srcPath.c_str(), dstPath.c_str());
            }
        }
        file = root.openNextFile();
    }
}

int IRAM_ATTR l_get_count(lua_State* L) {
    lua_pushinteger(L, universeMap ? currentCount : 0);
    return 1;
}

int IRAM_ATTR l_clear(lua_State* L) {
    if (!universeMap || currentCount == 0) return 0;
    for (int i = 0; i < currentCount; i++) {
        uint16_t global_i = currentZoneStart + i;
        if (global_i < totalUniverseLeds && universeMap[global_i]) {
            *(universeMap[global_i]) = CRGB(0, 0, 0);
        }
    }
    ledsDirty = true;
    return 0;
}

void hsv2rgb(uint8_t h, uint8_t s, uint8_t v, uint8_t &r, uint8_t &g, uint8_t &b) {
    if (s == 0) { r = g = b = v; return; }
    uint8_t region = h / 43;
    uint8_t remainder = (h - (region * 43)) * 6; 
    uint8_t p = (v * (255 - s)) >> 8;
    uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
    switch (region) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
}

inline uint8_t blend8(uint8_t c1, uint8_t c2, uint16_t fract) {
    int32_t val = (int32_t)c1 * (256 - fract) + (int32_t)c2 * fract;
    return (uint8_t)(val / 256);
}

inline uint8_t ease8InOutQuad(uint8_t f) {
    if (f < 128) {
        return (uint16_t)(f * f) >> 7; 
    } else {
        uint8_t inv = 255 - f;
        return 255 - ((uint16_t)(inv * inv) >> 7);
    }
}

int IRAM_ATTR l_fill_palette(lua_State* L) {
    if (!universeMap || currentCount == 0) return 0;
    if (!lua_istable(L, 1)) return 0; 
    
    float offset = luaL_checknumber(L, 2);
    float size_step = luaL_checknumber(L, 3);
    uint8_t br = luaL_checkinteger(L, 4) & 255;

    float start_pixel_f = luaL_optnumber(L, 5, 0.0f);
    float end_pixel_f = luaL_optnumber(L, 6, (float)(currentCount)); 
    uint8_t smooth_val = luaL_optinteger(L, 7, 0); 
    uint8_t dither_amount = luaL_optinteger(L, 8, 0); 
    
    bool is_cyclic = true;
    if (lua_gettop(L) >= 9 && !lua_isnil(L, 9)) {
        is_cyclic = lua_toboolean(L, 9);
    }
    
    float feather = luaL_optnumber(L, 10, 0.0f);
    int fade_speed = luaL_optinteger(L, 11, 0);

    if (start_pixel_f < 0.0f) start_pixel_f = 0.0f;
    if (start_pixel_f > currentCount) start_pixel_f = (float)(currentCount);
    if (end_pixel_f < 0.0f) end_pixel_f = 0.0f;
    if (end_pixel_f > currentCount) end_pixel_f = (float)(currentCount);

    int start_pixel = (int)start_pixel_f;
    int end_pixel = (int)end_pixel_f;

    uint8_t start_fade = 255;
    uint8_t end_fade = 255;
    if (start_pixel <= end_pixel) {
        start_fade = (uint8_t)((1.0f - (start_pixel_f - start_pixel)) * 255.0f);
        end_fade = (uint8_t)((end_pixel_f - end_pixel) * 255.0f);
    } else {
        start_fade = (uint8_t)((start_pixel_f - start_pixel) * 255.0f);
        end_fade = (uint8_t)((1.0f - (end_pixel_f - end_pixel)) * 255.0f);
    }

    uint8_t r[MAX_PALETTE_COLORS] = {0};
    uint8_t g[MAX_PALETTE_COLORS] = {0};
    uint8_t b[MAX_PALETTE_COLORS] = {0};
    float weights[MAX_PALETTE_COLORS] = {0.0f};
    
    int num_colors = 0;

    lua_pushnil(L);
    while (lua_next(L, 1) != 0 && num_colors < MAX_PALETTE_COLORS) {
        if (lua_istable(L, -1)) {
            lua_rawgeti(L, -1, 1); r[num_colors] = lua_tointeger(L, -1); lua_pop(L, 1);
            lua_rawgeti(L, -1, 2); g[num_colors] = lua_tointeger(L, -1); lua_pop(L, 1);
            lua_rawgeti(L, -1, 3); b[num_colors] = lua_tointeger(L, -1); lua_pop(L, 1);
            lua_rawgeti(L, -1, 4); weights[num_colors] = lua_tonumber(L, -1); lua_pop(L, 1);
            num_colors++;
        }
        lua_pop(L, 1);
    }

    if (num_colors == 0) return 0;

    float total_weight = 0;
    int active_weights = is_cyclic ? num_colors : (num_colors > 1 ? num_colors - 1 : 1);
    for (int i = 0; i < active_weights; i++) {
        total_weight += weights[i];
    }
    if (total_weight <= 0.0f) total_weight = 1.0f;

    uint32_t start_pos[MAX_PALETTE_COLORS];
    float current_acc = 0;
    for (int i = 0; i < num_colors; i++) {
        if (!is_cyclic && i == num_colors - 1) {
            start_pos[i] = PREC_MAX;
        } else {
            start_pos[i] = (uint32_t)((current_acc / total_weight) * PREC_MUL);
            current_acc += weights[i];
        }
    }

    int next_idx_lut[MAX_PALETTE_COLORS];
    uint32_t seg_start_lut[MAX_PALETTE_COLORS];
    uint32_t seg_len_lut[MAX_PALETTE_COLORS];

    for (int i = 0; i < num_colors; i++) {
        int next_idx = (is_cyclic) ? ((i + 1) % num_colors) : ((i + 1 < num_colors) ? i + 1 : i);
        next_idx_lut[i] = next_idx;
        
        uint32_t seg_start = start_pos[i];
        uint32_t seg_end = (is_cyclic && next_idx == 0) ? 0 : start_pos[next_idx]; 
        
        uint32_t seg_len = (is_cyclic && next_idx == 0) ? ((PREC_MAX + 1) - seg_start) : ((seg_end >= seg_start) ? (seg_end - seg_start) : 1);
        if (seg_len == 0) seg_len = 1; 

        seg_start_lut[i] = seg_start;
        seg_len_lut[i] = seg_len;
    }

    float safe_offset = offset - (int32_t)offset;
    if (safe_offset < 0.0f) safe_offset += 1.0f;

    int step = (start_pixel <= end_pixel) ? 1 : -1;
    int loop_count = abs(end_pixel - start_pixel) + 1;
    int current_p = start_pixel;
    
    uint32_t fast_t_ms = millis() >> 4; 

    float dist = (step > 0) ? ((float)start_pixel - start_pixel_f) : (start_pixel_f - (float)start_pixel);
    float dist_from_end = (step > 0) ? (end_pixel_f - (float)start_pixel) : ((float)start_pixel - end_pixel_f);
    float current_pos_f = safe_offset + (dist * size_step);
    
    bool do_dither = dither_amount > 0;
    bool do_smooth = smooth_val > 0;
    bool do_feather = feather > 0.001f;
    bool do_fade = fade_speed > 0;
    
    float inv_feather = do_feather ? (1.0f / feather) : 0.0f;

    for (int i = 0; i < loop_count; i++) {
        int mapped_i = currentReversed ? (currentCount - 1) - current_p : current_p;
        
        if (mapped_i >= 0 && mapped_i < currentCount) {
            uint16_t global_i = currentZoneStart + mapped_i;
            
            if (global_i < totalUniverseLeds && universeMap[global_i]) {
                CRGB* targetPixel = universeMap[global_i]; 

                uint32_t current_pos;
                if (is_cyclic) {
                    float wrapped = current_pos_f - (int32_t)current_pos_f;
                    if (wrapped < 0.0f) wrapped += 1.0f;
                    current_pos = (uint32_t)(wrapped * PREC_MUL);
                } else {
                    if (current_pos_f <= 0.0f) current_pos = 0;
                    else if (current_pos_f >= 1.0f) current_pos = PREC_MAX;
                    else current_pos = (uint32_t)(current_pos_f * PREC_MUL);
                }

                int idx = 0;
                for (int j = 1; j < num_colors; j++) {
                    if (current_pos >= start_pos[j]) idx = j;
                    else break;
                }
                
                int next_idx = next_idx_lut[idx];
                uint32_t seg_start = seg_start_lut[idx];
                uint32_t seg_len = seg_len_lut[idx];

                uint32_t offset_in_seg = (current_pos >= seg_start) ? (current_pos - seg_start) : 0;
                uint16_t fract = ((offset_in_seg * 255) + (seg_len >> 1)) / seg_len; 
                if (fract > 255) fract = 255;

                if (do_dither) {
                    uint8_t noise = (global_i * 73 + fast_t_ms) & 255; 
                    int16_t dither_val = ((noise * dither_amount) >> 8) - (dither_amount >> 1);
                    int new_fract = fract + dither_val;
                    fract = (new_fract < 0) ? 0 : ((new_fract > 255) ? 255 : new_fract);
                }

                if (do_smooth) {
                    uint8_t eased = ease8InOutQuad((uint8_t)fract);
                    fract = fract + (((int32_t)eased - fract) * smooth_val) / 255;
                }

                uint8_t r_blend = blend8(r[idx], r[next_idx], fract);
                uint8_t g_blend = blend8(g[idx], g[next_idx], fract);
                uint8_t b_blend = blend8(b[idx], b[next_idx], fract);

                uint16_t shape_alpha = 255; 

                if (do_feather) {
                    if (dist >= feather && dist_from_end >= feather) {
                        shape_alpha = 255;
                    } else {
                        float start_mult = (dist + 0.5f) * inv_feather; 
                        float end_mult = (dist_from_end + 0.5f) * inv_feather;
                        
                        // Ternary clamps compile to efficient branchless MIN/MAX instructions
                        start_mult = (start_mult < 0.0f) ? 0.0f : ((start_mult > 1.0f) ? 1.0f : start_mult);
                        end_mult = (end_mult < 0.0f) ? 0.0f : ((end_mult > 1.0f) ? 1.0f : end_mult);
                        
                        start_mult = start_mult * start_mult * (3.0f - 2.0f * start_mult);
                        end_mult = end_mult * end_mult * (3.0f - 2.0f * end_mult);
                        
                        shape_alpha = (uint16_t)(255.0f * start_mult * end_mult);
                    }
                } else {
                    if (loop_count == 1) {
                        shape_alpha = (start_fade * end_fade) >> 8;
                    } else {
                        if (i == 0) shape_alpha = start_fade;
                        else if (i == loop_count - 1) shape_alpha = end_fade;
                    }
                }

                uint8_t fg_r = (r_blend * br) >> 8;
                uint8_t fg_g = (g_blend * br) >> 8;
                uint8_t fg_b = (b_blend * br) >> 8;

                if (do_fade) {
                    CRGB current = *targetPixel;
                    
                    int active_speed = (fade_speed * shape_alpha) >> 8;
                    if (active_speed == 0 && shape_alpha > 0) active_speed = 1;

                    if (shape_alpha > 0) {
                        if (current.r < fg_r) { int n = current.r + active_speed; current.r = (n > fg_r) ? fg_r : n; }
                        else if (current.r > fg_r) { int n = current.r - active_speed; current.r = (n < fg_r) ? fg_r : n; }

                        if (current.g < fg_g) { int n = current.g + active_speed; current.g = (n > fg_g) ? fg_g : n; }
                        else if (current.g > fg_g) { int n = current.g - active_speed; current.g = (n < fg_g) ? fg_g : n; }

                        if (current.b < fg_b) { int n = current.b + active_speed; current.b = (n > fg_b) ? fg_b : n; }
                        else if (current.b > fg_b) { int n = current.b - active_speed; current.b = (n < fg_b) ? fg_b : n; }
                    }
                    *targetPixel = current;
                } else {
                    if (shape_alpha < 255) {
                        uint8_t inv_alpha = 255 - shape_alpha;
                        CRGB bg = *targetPixel;
                        *targetPixel = CRGB(
                            ((fg_r * shape_alpha) + (bg.r * inv_alpha)) >> 8,
                            ((fg_g * shape_alpha) + (bg.g * inv_alpha)) >> 8,
                            ((fg_b * shape_alpha) + (bg.b * inv_alpha)) >> 8
                        );
                    } else {
                        *targetPixel = CRGB(fg_r, fg_g, fg_b);
                    }
                }
            }
        }
        
        current_p += step; 
        current_pos_f += size_step;
        if (do_feather) {
            dist += 1.0f;
            dist_from_end -= 1.0f;
        }
    }
    
    ledsDirty = true;
    return 0;
}

int IRAM_ATTR l_scale_pixel(lua_State* L) {
    if (!universeMap || currentCount == 0) return 0;
    
    int i = luaL_checkinteger(L, 1);
    uint8_t target_lum = luaL_checkinteger(L, 2) & 255;
    
    uint8_t dither = luaL_optinteger(L, 3, 0); 

    uint16_t global_i = currentZoneStart + (currentReversed ? (currentCount - 1) - i : i);

    if (global_i < totalUniverseLeds && universeMap[global_i]) {
        uint16_t scale = target_lum;

        if (dither > 0 && target_lum > 0 && target_lum < 255) {
            uint8_t noise = (global_i * 73 + (millis() / 16)) & 255;
            int16_t dither_val = ((noise * dither) >> 8) - (dither >> 1);
            int new_scale = target_lum + dither_val;
            scale = (new_scale < 0) ? 0 : ((new_scale > 255) ? 255 : new_scale);
        }

        universeMap[global_i]->r = ((uint16_t)universeMap[global_i]->r * scale + 127) / 255;
        universeMap[global_i]->g = ((uint16_t)universeMap[global_i]->g * scale + 127) / 255;
        universeMap[global_i]->b = ((uint16_t)universeMap[global_i]->b * scale + 127) / 255;
    }
    return 0;
}

int l_set_hsv(lua_State* L) {
    if (!universeMap) return 0;
    int i = lua_tointeger(L, 1);
    if (currentReversed) i = (currentCount - 1) - i;
    
    uint16_t global_i = currentZoneStart + i;
    if (global_i < totalUniverseLeds && universeMap[global_i]) {
        
        uint8_t h = lua_tointeger(L, 2) & 255; 
        uint8_t s = lua_tointeger(L, 3) & 255;
        uint8_t v = lua_tointeger(L, 4) & 255;

        uint8_t r = 0, g = 0, b = 0;

        if (s == 0) {
            r = g = b = v;
        } else {
            uint8_t region = h / 43;
            uint8_t remainder = (h - (region * 43)) * 6; 
            
            uint8_t p = (v * (255 - s)) >> 8;
            uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
            uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

            switch (region) {
                case 0: r = v; g = t; b = p; break;
                case 1: r = q; g = v; b = p; break;
                case 2: r = p; g = v; b = t; break;
                case 3: r = p; g = q; b = v; break;
                case 4: r = t; g = p; b = v; break;
                default: r = v; g = p; b = q; break;
            }
        }
        *(universeMap[global_i]) = CRGB(r, g, b);
        ledsDirty = true;
    } 
    return 0;
}

int IRAM_ATTR l_set_rgb(lua_State* L) {
    if (!universeMap) return 0;
    int i = luaL_checkinteger(L, 1);
    if (currentReversed) i = (currentCount - 1) - i;
    
    uint16_t global_i = currentZoneStart + i;
    if (global_i < totalUniverseLeds && universeMap[global_i]) {
        *(universeMap[global_i]) = CRGB(
            luaL_checkinteger(L, 2),
            luaL_checkinteger(L, 3),
            luaL_checkinteger(L, 4)
        );
        ledsDirty = true;
    } 
    return 0;
}

int IRAM_ATTR l_fade(lua_State* L) {
    if (!universeMap || currentCount == 0) return 0;
    uint16_t fadeAmount = luaL_checkinteger(L, 1); 
    
    for (int i = 0; i < currentCount; i++) {
        uint16_t global_i = currentZoneStart + i;
        if (global_i < totalUniverseLeds && universeMap[global_i]) {
            CRGB* pixel = universeMap[global_i];
            pixel->r = (pixel->r * fadeAmount) >> 8;
            pixel->g = (pixel->g * fadeAmount) >> 8;
            pixel->b = (pixel->b * fadeAmount) >> 8;
        }
    }
    ledsDirty = true;
    return 0;
}

int IRAM_ATTR l_get_stream_pixel(lua_State* L) {
    int i = luaL_checkinteger(L, 1);
    
    if (!streamBuffer || currentCount == 0 || totalUniverseLeds == 0 || i < 0 || i >= currentCount) {
        lua_pushinteger(L, 0); lua_pushinteger(L, 0); lua_pushinteger(L, 0);
        return 3;
    }

    if (currentReversed) i = (currentCount - 1) - i;
    
    uint16_t global_i = 0;
    
    if (currentCount > 1) {
        global_i = (i * (totalUniverseLeds - 1)) / (currentCount - 1);
    }
    
    if (global_i < totalUniverseLeds) {
        lua_pushinteger(L, streamBuffer[global_i].r);
        lua_pushinteger(L, streamBuffer[global_i].g);
        lua_pushinteger(L, streamBuffer[global_i].b);
        return 3; 
    }
    
    lua_pushinteger(L, 0); lua_pushinteger(L, 0); lua_pushinteger(L, 0);
    return 3;
}

int l_debug_log(lua_State* L) {
    const char* msg = luaL_checkstring(L, 1);
    lastLuaDebug = String(msg);
    Serial.println(msg);
    return 0;
}

int IRAM_ATTR l_millis(lua_State* L) {
    lua_pushinteger(L, millis());
    return 1;
}

int l_config_index(lua_State* L) {
    if (!current_lua_config) { lua_pushnil(L); return 1; }
    const char* key = lua_tostring(L, 2); 
    if (!key) { lua_pushnil(L); return 1; }

    if (key[0] == 'r' && key[1] == '\0') lua_pushinteger(L, current_lua_config->r);
    else if (key[0] == 'g' && key[1] == '\0') lua_pushinteger(L, current_lua_config->g);
    else if (key[0] == 'b' && key[1] == '\0') lua_pushinteger(L, current_lua_config->b);
    else if (strcmp(key, "speed") == 0) lua_pushinteger(L, current_lua_config->speed);
    else if (strcmp(key, "size") == 0) lua_pushinteger(L, current_lua_config->size);
    else if (strcmp(key, "delay") == 0) lua_pushinteger(L, current_lua_config->delay);
    else if (strcmp(key, "brightness") == 0) lua_pushinteger(L, current_lua_config->brightness);
    else lua_pushnil(L); 
    return 1;
}

int l_klipper_index(lua_State* L) {
    const char* key = luaL_checkstring(L, 2); 
    if (strcmp(key, "event") == 0) lua_pushstring(L, EventNames[currentEvent]);
    else if (strcmp(key, "temp") == 0) lua_pushnumber(L, currentTemp);
    else if (strcmp(key, "target") == 0) lua_pushnumber(L, targetTemp);
    else lua_pushnil(L); 
    return 1;
}

int IRAM_ATTR l_klipper_pos(lua_State* L) {
    int idx = luaL_checkinteger(L, 1);
    if (idx >= 1 && idx <= 16) lua_pushnumber(L, progress[idx - 1]);
    else lua_pushnumber(L, 0.0);
    return 1;
}

int IRAM_ATTR l_get_json(lua_State* L) {
    lua_pushstring(L, lastMoonrakerJson);
    return 1;
}

bool executeLuaFast(int scriptRef, int zoneIndex) {
    if (!L_VM || zoneIndex >= config.zoneCount) return false;

    lua_settop(L_VM, 0); 
    lua_getglobal(L_VM, "zone_updates"); 
    
    bool needs_rebind = false;
    if (last_bound_script && last_event) {
        needs_rebind = (last_bound_script[zoneIndex] != scriptRef) || (last_event[zoneIndex] != lastGlobalEvent);
    } else {
        needs_rebind = true;
    }

    if (needs_rebind) {
        lua_pushnil(L_VM);
        lua_setglobal(L_VM, "update");

        lua_rawgeti(L_VM, LUA_REGISTRYINDEX, scriptRef);
        if (lua_pcall(L_VM, 0, 0, 0) != LUA_OK) {
            Serial.printf("[LUA] Init Error (Zone %d): %s\n", zoneIndex, lua_tostring(L_VM, -1));
            lua_settop(L_VM, 0); 
            return false;
        }

        lua_getglobal(L_VM, "update"); 
        
        if (lua_isfunction(L_VM, -1)) {
            lua_pushvalue(L_VM, -1); 
            lua_rawseti(L_VM, 1, zoneIndex); 
            
            if (last_bound_script) last_bound_script[zoneIndex] = scriptRef;
            if (last_event) last_event[zoneIndex] = lastGlobalEvent; 
        } else {
            lua_pushnil(L_VM);
            lua_rawseti(L_VM, 1, zoneIndex); 
        }
        
        lua_pop(L_VM, 1); 
    }

    lua_rawgeti(L_VM, 1, zoneIndex); 

    if (lua_isfunction(L_VM, -1)) {
        lua_pushinteger(L_VM, zoneIndex);                     
        lua_pushinteger(L_VM, (zoneIndex % 2 == 0) ? 2 : 1);  
        
        if (lua_pcall(L_VM, 2, 0, 0) != LUA_OK) {
            Serial.printf("[LUA] Update Error (Zone %d): %s\n", zoneIndex, lua_tostring(L_VM, -1));
            lua_settop(L_VM, 0);
            return false;
        }
    } 

    lua_settop(L_VM, 0); 
    return true;
}

void initLua() {
    if (L_VM) {
        lua_close(L_VM);
        if (cachedScriptRef) {
            for (int i = 0; i < config.zoneCount; i++) {
                cachedScriptRef[i] = LUA_NOREF;
            }
        }
    }

    L_VM = luaL_newstate();
    luaL_openlibs(L_VM);

    checkAndCompileAllScripts();

    if (last_bound_script) free(last_bound_script);
    if (last_event) free(last_event);

    last_bound_script = (int*)SAFE_ALLOC(config.zoneCount * sizeof(int));
    last_event = (EventType*)SAFE_ALLOC(config.zoneCount * sizeof(EventType));

    for(int i=0; i < config.zoneCount; i++) {
        if (last_bound_script) last_bound_script[i] = -1;
        if (last_event) last_event[i] = (EventType)-1;
    }

    lua_newtable(L_VM);
    lua_setglobal(L_VM, "zone_updates");

    lua_register(L_VM, "log", l_debug_log);
    lua_register(L_VM, "millis", l_millis);
    
    lua_pushcfunction(L_VM, l_get_stream_pixel); 
    lua_setglobal(L_VM, "get_stream_pixel");

    lua_newtable(L_VM);
    lua_pushcfunction(L_VM, l_set_rgb);   lua_setfield(L_VM, -2, "set_rgb");
    lua_pushcfunction(L_VM, l_set_hsv);   lua_setfield(L_VM, -2, "set_hsv");
    lua_pushcfunction(L_VM, l_fill_palette);  lua_setfield(L_VM, -2, "fill_palette");
    lua_pushcfunction(L_VM, l_scale_pixel);  lua_setfield(L_VM, -2, "scale_pixel");
    lua_pushcfunction(L_VM, l_get_count); lua_setfield(L_VM, -2, "get_count");
    lua_pushcfunction(L_VM, l_clear);     lua_setfield(L_VM, -2, "clear");
    lua_pushcfunction(L_VM, l_fade);      lua_setfield(L_VM, -2, "fade");
    lua_setglobal(L_VM, "led");

    lua_newtable(L_VM);
    lua_newtable(L_VM);
    lua_pushcfunction(L_VM, l_config_index);
    lua_setfield(L_VM, -2, "__index");
    lua_setmetatable(L_VM, -2);
    lua_setglobal(L_VM, "config");

    lua_newtable(L_VM);
    lua_pushcfunction(L_VM, l_get_json);    lua_setfield(L_VM, -2, "get_json");
    lua_pushcfunction(L_VM, l_klipper_pos); lua_setfield(L_VM, -2, "get_pos");
    
    lua_newtable(L_VM);
    lua_pushcfunction(L_VM, l_klipper_index);
    lua_setfield(L_VM, -2, "__index");
    lua_setmetatable(L_VM, -2);
    lua_setglobal(L_VM, "klipper");
}