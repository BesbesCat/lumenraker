#include "lua_engine.h"
#include <lualib.h>
#include <lauxlib.h>
#include "config.h"
#include <LittleFS.h>

extern char lastMoonrakerJson[1024];
extern volatile EventType currentEvent;
extern float progress[16];
extern float currentTemp;
extern float targetTemp;

extern CRGB* currentLeds;
extern uint16_t currentCount;
lua_State* L_VM = nullptr;
extern String lastLuaDebug = "";

static int last_bound_script[MAX_ZONES];
static EventType last_event[MAX_ZONES];

int l_get_count(lua_State* L) {
    lua_pushinteger(L, currentLeds ? currentCount : 0);
    return 1;
}

int l_clear(lua_State* L) {
    if (!currentLeds || currentCount == 0) return 0;
    for (int i = 0; i < currentCount; i++) {
        currentLeds[i] = CRGB(0, 0, 0);
    }
    return 0;
}

int l_set_hsv(lua_State* L) {
    if (!currentLeds) return 0;
    int i = luaL_checkinteger(L, 1);
    float h = luaL_checkinteger(L, 2) / 255.0f;
    float s = luaL_checkinteger(L, 3) / 255.0f;
    float v = luaL_checkinteger(L, 4) / 255.0f;

    if (currentReversed) i = (currentCount - 1) - i;
    
    if (i >= 0 && i < currentCount) {
        HsbColor hsb(h, s, v);
        RgbColor rgb(hsb);
        currentLeds[i] = CRGB(rgb.R, rgb.G, rgb.B);
    }
    return 0;
}

int l_set_rgb(lua_State* L) {
    if (!currentLeds) return 0;
    int i = luaL_checkinteger(L, 1);
    if (currentReversed) i = (currentCount - 1) - i;
    if (i >= 0 && i < currentCount) {
        currentLeds[i] = CRGB(
            luaL_checkinteger(L, 2),
            luaL_checkinteger(L, 3),
            luaL_checkinteger(L, 4)
        );
    } 
    return 0;
}

int l_fade(lua_State* L) {
    if (!currentLeds || currentCount == 0) return 0;
    uint16_t fadeAmount = luaL_checkinteger(L, 1); 
    for (int i = 0; i < currentCount; i++) {
        currentLeds[i].r = (currentLeds[i].r * fadeAmount) >> 8;
        currentLeds[i].g = (currentLeds[i].g * fadeAmount) >> 8;
        currentLeds[i].b = (currentLeds[i].b * fadeAmount) >> 8;
    }
    return 0;
}

int l_debug_log(lua_State* L) {
    const char* msg = luaL_checkstring(L, 1);
    lastLuaDebug = String(msg);
    Serial.println(msg);
    return 0;
}

int l_millis(lua_State* L) {
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

int l_klipper_pos(lua_State* L) {
    int idx = luaL_checkinteger(L, 1);
    if (idx >= 1 && idx <= 16) lua_pushnumber(L, progress[idx - 1]);
    else lua_pushnumber(L, 0.0);
    return 1;
}

int l_get_json(lua_State* L) {
    lua_pushstring(L, lastMoonrakerJson);
    return 1;
}

bool executeLuaFast(int scriptRef, int zoneIndex) {
    if (!L_VM || zoneIndex >= MAX_ZONES) return false;

    // 1. Set context
    lua_pushinteger(L_VM, zoneIndex);
    lua_setglobal(L_VM, "id");
    lua_pushinteger(L_VM, (zoneIndex % 2 == 0) ? 2 : 1);
    lua_setglobal(L_VM, "axis");

    lua_getglobal(L_VM, "zone_updates");
    
    bool needs_rebind = (last_bound_script[zoneIndex] != scriptRef) || 
                        (last_event[zoneIndex] != currentEvent);

    if (needs_rebind) {
        lua_pushnil(L_VM);
        lua_setglobal(L_VM, "update");

        lua_rawgeti(L_VM, LUA_REGISTRYINDEX, scriptRef);
        if (lua_pcall(L_VM, 0, 0, 0) != LUA_OK) {
            Serial.printf("[LUA] Init Error (Zone %d): %s\n", zoneIndex, lua_tostring(L_VM, -1));
            lua_pop(L_VM, 2);
            return false;
        }

        lua_getglobal(L_VM, "update");
        if (lua_isfunction(L_VM, -1)) {
            lua_rawseti(L_VM, -2, zoneIndex); 
            last_bound_script[zoneIndex] = scriptRef;
            last_event[zoneIndex] = currentEvent; 
        } else {
            lua_pop(L_VM, 1);
            lua_pushnil(L_VM);
            lua_rawseti(L_VM, -2, zoneIndex);
            last_bound_script[zoneIndex] = scriptRef;
            last_event[zoneIndex] = currentEvent;
        }
        
        lua_rawgeti(L_VM, -1, zoneIndex);
    } else {
        lua_rawgeti(L_VM, -1, zoneIndex);
    }

    if (lua_isfunction(L_VM, -1)) {
        if (lua_pcall(L_VM, 0, 0, 0) != LUA_OK) {
            Serial.printf("[LUA] Update Error (Zone %d): %s\n", zoneIndex, lua_tostring(L_VM, -1));
            lua_pop(L_VM, 2);
            return false;
        }
    } else {
        lua_pop(L_VM, 1);
    }

    lua_pop(L_VM, 1);
    return true;
}

void initLua() {
    if (L_VM) lua_close(L_VM);
    L_VM = luaL_newstate();
    luaL_openlibs(L_VM);

    for(int i=0; i<MAX_ZONES; i++) {
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