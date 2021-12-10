#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "nrf_log.h"

void * sbrk(intptr_t );
static void memused(const char *loc) {
  NRF_LOG_INFO("heap at %s: %x", loc, sbrk(0));
}

static const luaL_Reg loadedlibs[] = {
  {"_G", luaopen_base},
  {LUA_LOADLIBNAME, luaopen_package},
  //   {LUA_COLIBNAME, luaopen_coroutine},
  {LUA_TABLIBNAME, luaopen_table},
  //   {LUA_IOLIBNAME, luaopen_io},
  //   {LUA_OSLIBNAME, luaopen_os},
  {LUA_STRLIBNAME, luaopen_string},
  {LUA_MATHLIBNAME, luaopen_math},
  //   {LUA_UTF8LIBNAME, luaopen_utf8},
  //   {LUA_DBLIBNAME, luaopen_debug},
  // #if defined(LUA_COMPAT_BITLIB)
  //   {LUA_BITLIBNAME, luaopen_bit32},
  // #endif
  {NULL, NULL}
};


LUALIB_API void luaP_opensomelibs (lua_State *L) {
  const luaL_Reg *lib;
  /* "require" functions from 'loadedlibs' and set results to global table */
  for (lib = loadedlibs; lib->func; lib++) {
    memused(lib->name);
    luaL_requiref(L, lib->name, lib->func, 1);
    lua_pop(L, 1);  /* remove lib */
  }
}

static lua_State *L;

int open_lua() {
     memused("before");

     L = luaL_newstate();
     memused("newstate");

     luaP_opensomelibs(L);
     memused("opened libs");

     int ret = luaL_dostring(L, "return tostring(6*7);");
     const char *  str = lua_tostring(L, -1);
     memused("after");
     if (ret) {
       NRF_LOG_INFO("Error: %s", str);
     } else {
       NRF_LOG_INFO("open_lua: %s", str);
     }

     return 0;
}
