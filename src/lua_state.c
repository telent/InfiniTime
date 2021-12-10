#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "nrf_log.h"

#define SCREEN_WIDTH_PIXELS (240)

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
    // memused(lib->name);
    luaL_requiref(L, lib->name, lib->func, 1);
    lua_pop(L, 1);  /* remove lib */
  }
}




// metatable method for handling "array[index]"
static int array_index (lua_State* L) {
  uint16_t ** parray = luaL_checkudata(L, 1, "i16_buffer");
  int index = luaL_checkinteger(L, 2);
  NRF_LOG_INFO("get index %d", index);
  lua_pushnumber(L, (*parray)[index-1]);
  return 1;
}

// metatable method for handle "array[index] = value"
static int array_newindex (lua_State* L) {
  uint16_t ** parray = luaL_checkudata(L, 1, "i16_buffer");
  int index = luaL_checkinteger(L, 2);
  int value = luaL_checkinteger(L, 3);
  NRF_LOG_INFO("set index %d %d", index, value);
  // XXX probably should check 0 < value < 65536
  (*parray)[index-1] = value;

  return 0;
}

// create a metatable for our array type
static void create_buffer_type(lua_State* L) {
   static const struct luaL_Reg funcs[] = {
      { "__index",  array_index  },
      { "__newindex",  array_newindex  },
      { NULL, NULL }
   };
   luaL_newmetatable(L, "i16_buffer");
   luaL_setfuncs(L, funcs, 0);
}

// expose an array to lua, by storing it in a userdata with the array metatable
static int expose_buffer(lua_State* L, uint16_t array[]) {
  uint16_t ** parray = lua_newuserdata(L, sizeof(uint16_t **));
  *parray = array;
  luaL_getmetatable(L, "i16_buffer");
  lua_setmetatable(L, -2);
  return 1;
}

uint16_t lcd_buffer[SCREEN_WIDTH_PIXELS];

static lua_State *L;


#define lua_ok(c) ((c)==LUA_OK)

static int runstring(lua_State *L, char * str) {
  int ret = luaL_dostring(L, str);
  NRF_LOG_INFO("runstring ret %d", ret);
  if(ret != LUA_OK) {
    NRF_LOG_INFO("runstring error %s", lua_tostring(L, -1));
    lua_pop(L, 1);
  }
  return ret;
}

extern void draw_lcd_buffer(int x, int y, int w, int h, int length);

static int lua_draw_lcd_buffer(lua_State *L){
  // x y w h buffer_length
  int x = lua_tonumber(L, 1);
  int y = lua_tonumber(L, 2);
  int w = lua_tonumber(L, 3);
  int h = lua_tonumber(L, 4);
  int length = lua_tonumber(L, 5);
  draw_lcd_buffer(x, y, w, h, length);
  return 0;
}

int open_lua() {
     memused("before");
     memset(lcd_buffer, 0, SCREEN_WIDTH_PIXELS * sizeof(uint16_t));

     L = luaL_newstate();
     memused("newstate");

     luaP_opensomelibs(L);
     memused("opened libs");

     create_buffer_type(L);
     expose_buffer(L, lcd_buffer);
     lua_setglobal(L, "lcd_buffer");
     memused("alloc lcd buffer");

     lua_register(L, "draw_lcd_buffer", lua_draw_lcd_buffer);

     memused("again");
     if(lua_ok(runstring(L, "return lcd_buffer[5];"))) {
       NRF_LOG_INFO("lcd buf index %d",  lua_tonumber(L, -1));
     }
     memused("buf done");

     return 0;
}

extern char hello_lua[];

void lua_say_hello(){
  runstring(L, (char *) hello_lua);
}
