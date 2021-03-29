#include <string.h>

#include "vmsystem.h"
#include "vmlog.h"
#include "vmfs.h"
#include "vmchset.h"
#include "vmaudio_play.h"

#include "lua.h"
#include "lauxlib.h"
#include "GATTServer.h"

extern vm_bt_gatt_attribute_value_t gatt_attribute_value_t;

int gatt_send_indication(lua_State *L)
{
    char *data = lua_tostring(L, -1);
    
    sprintf(gatt_attribute_value_t.data, data);
    gatt_attribute_value_t.length = strlen(gatt_attribute_value_t.data);
    int result = app_send_indication(VM_FALSE, &gatt_attribute_value_t);

    lua_pushnumber(L, result);

	return 1;
}

#undef MIN_OPT_LEVEL
#define MIN_OPT_LEVEL 0
#include "lrodefs.h"

const LUA_REG_TYPE gatt_map[] =
{
    {LSTRKEY("send"), LFUNCVAL(gatt_send_indication)},
    {LNILKEY, LNILVAL}
};

LUALIB_API int luaopen_gatt(lua_State *L)
{
    luaL_register(L, "gatt", gatt_map);
    return 1;
}
