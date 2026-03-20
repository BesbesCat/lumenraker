#pragma once
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <NeoPixelBus.h>
#include "config.h"

extern lua_State* L_VM;

void initLua();
void update_lua_config(EffectConfig &cfg);
bool executeLuaFast(int scriptRef, int zoneIndex);

extern CRGB* currentLeds;
extern uint16_t currentCount;
extern bool currentReversed;

void register_lua_hooks(lua_State* L);
