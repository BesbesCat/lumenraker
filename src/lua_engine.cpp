#include <lualib.h>
#include <lauxlib.h>
#include <LittleFS.h>
#include "config.h"
#include "lua_engine.h"

extern char lastMoonrakerJson[1024];
extern volatile EventType currentEvent;
extern EventType lastGlobalEvent;
extern int cachedScriptRef[];

float progress[16];
float currentTemp;
float targetTemp;

extern CRGB** universeMap;
extern uint16_t totalUniverseLeds;
extern uint16_t currentZoneStart;
extern uint16_t currentCount;
extern bool ledsDirty;

lua_State* L_VM = nullptr;
extern String lastLuaDebug = "";

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
    char* buf = (char*)malloc(size);
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
        if (global_i < totalUniverseLeds) {
            *(universeMap[global_i]) = CRGB(0, 0, 0);
        }
    }
    ledsDirty = true;
    return 0;
}

int l_set_hsv(lua_State* L) {
    if (!universeMap) return 0;
    int i = lua_tointeger(L, 1);
    if (currentReversed) i = (currentCount - 1) - i;
    
    uint16_t global_i = currentZoneStart + i;
    if (global_i < totalUniverseLeds) {
        
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
    if (global_i < totalUniverseLeds) {
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
        if (global_i < totalUniverseLeds) {
            CRGB* pixel = universeMap[global_i];
            pixel->r = (pixel->r * fadeAmount) >> 8;
            pixel->g = (pixel->g * fadeAmount) >> 8;
            pixel->b = (pixel->b * fadeAmount) >> 8;
        }
    }
    ledsDirty = true;
    return 0;
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

    // 1. Wipe the stack clean so previous errors don't cascade
    lua_settop(L_VM, 0); 

    lua_getglobal(L_VM, "zone_updates"); // This table is now safely at Stack Index 1
    
    // 2. Sync against lastGlobalEvent, NOT currentEvent, to prevent shadow-loader desync
    bool needs_rebind = (last_bound_script[zoneIndex] != scriptRef) || 
                        (last_event[zoneIndex] != lastGlobalEvent);

    if (needs_rebind) {
        lua_pushnil(L_VM);
        lua_setglobal(L_VM, "update");

        lua_rawgeti(L_VM, LUA_REGISTRYINDEX, scriptRef);
        if (lua_pcall(L_VM, 0, 0, 0) != LUA_OK) {
            Serial.printf("[LUA] Init Error (Zone %d): %s\n", zoneIndex, lua_tostring(L_VM, -1));
            lua_settop(L_VM, 0); // Clean exit
            return false;
        }

        lua_getglobal(L_VM, "update"); // Result is at Stack Index 2
        
        if (lua_isfunction(L_VM, -1)) {
            lua_pushvalue(L_VM, -1); // Copy the function
            lua_rawseti(L_VM, 1, zoneIndex); // Save to zone_updates table at index 1
            
            last_bound_script[zoneIndex] = scriptRef;
            last_event[zoneIndex] = lastGlobalEvent; 
        } else {
            // If script has no update(), cleanly erase old functions to prevent stack panics
            lua_pushnil(L_VM);
            lua_rawseti(L_VM, 1, zoneIndex); 
        }
        
        lua_pop(L_VM, 1); // Remove the 'update' global result to keep stack balanced
    }

    // 3. Fetch the bound function
    lua_rawgeti(L_VM, 1, zoneIndex); 

    if (lua_isfunction(L_VM, -1)) {
        // Push arguments onto the stack AFTER the function
        lua_pushinteger(L_VM, zoneIndex);                     // Argument 1: id
        lua_pushinteger(L_VM, (zoneIndex % 2 == 0) ? 2 : 1);  // Argument 2: axis
        
        // Tell pcall to expect 2 arguments instead of 0
        if (lua_pcall(L_VM, 2, 0, 0) != LUA_OK) {
            Serial.printf("[LUA] Update Error (Zone %d): %s\n", zoneIndex, lua_tostring(L_VM, -1));
            lua_settop(L_VM, 0);
            return false;
        }
    } 

    lua_settop(L_VM, 0); // Always exit with 0 stack
    return true;
}

void initLua() {
    if (L_VM) {
        lua_close(L_VM);
        for (int i = 0; i < config.zoneCount; i++) {
            cachedScriptRef[i] = LUA_NOREF;
        }
    }

    L_VM = luaL_newstate();
    luaL_openlibs(L_VM);

    checkAndCompileAllScripts();

    if (last_bound_script) delete[] last_bound_script;
    if (last_event) delete[] last_event;

    last_bound_script = new int[config.zoneCount];
    last_event = new EventType[config.zoneCount];

    for(int i=0; i < config.zoneCount; i++) {
        last_bound_script[i] = -1;
        last_event[i] = (EventType)-1;
    }

    lua_newtable(L_VM);
    lua_setglobal(L_VM, "zone_updates");

    lua_register(L_VM, "log", l_debug_log);
    lua_register(L_VM, "millis", l_millis);

    lua_newtable(L_VM);
    lua_pushcfunction(L_VM, l_set_rgb);   lua_setfield(L_VM, -2, "set_rgb");
    lua_pushcfunction(L_VM, l_set_hsv);   lua_setfield(L_VM, -2, "set_hsv");
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