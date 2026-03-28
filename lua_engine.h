#pragma once
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <NeoPixelBus.h>
#include "config.h"

extern lua_State* L_VM;

void initLua();
bool executeLuaFast(int scriptRef, int zoneIndex);

extern CRGB* currentLeds;
extern uint16_t currentCount;
extern bool currentReversed;

void register_lua_hooks(lua_State* L);
bool compileLuaScript(const char* srcPath, const char* dstPath);
void checkAndCompileAllScripts();