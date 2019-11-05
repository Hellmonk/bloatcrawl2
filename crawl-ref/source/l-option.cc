/***
 * @module Globals
 */
/*** Named lua options.
 * To set options with the same processing as `.crawlrc` or `init.txt`, use
 * @{crawl.setopt}.
 *
 * This table provides access to the following crawl options: `autoswitch,
 * travel_open_doors, easy_armour, easy_unequip, note_skill_max,
 * clear_messages, blink_brightens_background, bold_brightens_foreground,
 * best_effort_brighten_background, best_effort_brighten_foreground,
 * allow_extended_colours, pickup_thrown, easy_exit_menu,
 * dos_use_background_intensity, autopickup_on`; documented in
 * `docs/options_guide.txt`.
 *
 * It can also be used for global configuration of clua extensions.
 * @table options
 */
#include "AppHdr.h"

#include "l-libs.h"

#include "clua.h"
#include "libutil.h" // map_find
#include "options.h"

typedef int (*ohandler)(lua_State *ls, const char *name, void *data, bool get);
struct option_handler
{
    const char *option;
    void       *data;
    ohandler    handler;
};

static int option_hboolean(lua_State *ls, const char */*name*/, void *data,
                           bool get)
{
    if (get)
    {
        lua_pushboolean(ls, *static_cast<bool*>(data));
        return 1;
    }
    else
    {
        if (lua_isboolean(ls, 3))
            *static_cast<bool*>(data) = lua_toboolean(ls, 3);
        return 0;
    }
}

static int option_autopick(lua_State *ls, const char */*name*/, void */*data*/,
                           bool /*get*/)
{
    lua_pushboolean(ls, Options.autopickup_on>0);
    return 1;
}

static option_handler handlers[] =
{
    // Boolean options come first
    { "autoswitch",    &Options.auto_switch, option_hboolean },
    { "travel_open_doors",    &Options.travel_open_doors, option_hboolean },
    { "easy_armour",   &Options.easy_unequip, option_hboolean },
    { "easy_unequip",  &Options.easy_unequip, option_hboolean },
    { "note_skill_max",       &Options.note_skill_max, option_hboolean },
    { "clear_messages",  &Options.clear_messages, option_hboolean },
    { "blink_brightens_background", &Options.blink_brightens_background,
                                   option_hboolean },
    { "bold_brightens_foreground", &Options.bold_brightens_foreground,
                                   option_hboolean },
    { "best_effort_brighten_background",
        &Options.best_effort_brighten_background, option_hboolean },
    { "best_effort_brighten_foreground",
        &Options.best_effort_brighten_foreground, option_hboolean },
    { "allow_extended_colours", &Options.allow_extended_colours,
                               option_hboolean },
    { "pickup_thrown",   &Options.pickup_thrown, option_hboolean },
    { "dos_use_background_intensity", &Options.dos_use_background_intensity,
                                      option_hboolean },
    { "autopick_on", nullptr, option_autopick }
};

static const option_handler *get_handler(const char *optname)
{
    if (optname)
    {
        for (const option_handler &handler : handlers)
        {
            if (!strcmp(handler.option, optname))
                return &handler;
        }
    }
    return nullptr;
}

static int option_get(lua_State *ls)
{
    const char *opt = luaL_checkstring(ls, 2);
    if (!opt)
        return 0;

    // Is this a Lua named option?
    if (string *value = map_find(Options.named_options, opt))
    {
        lua_pushstring(ls, value->c_str());
        return 1;
    }

    const option_handler *oh = get_handler(opt);
    if (oh)
#ifdef DEBUG_GLOBALS
        return oh->handler(ls, opt, (char*)real_Options+(intptr_t)oh->data, true);
#else
        return oh->handler(ls, opt, oh->data, true);
#endif

    return 0;
}

static int option_set(lua_State *ls)
{
    const char *opt = luaL_checkstring(ls, 2);
    if (!opt)
        return 0;

    const option_handler *oh = get_handler(opt);
    if (oh)
        oh->handler(ls, opt, oh->data, false);

    return 0;
}

#define OPT_METATABLE "clua_metatable_optaccess"

void cluaopen_options(lua_State *ls)
{
    int top = lua_gettop(ls);

    luaL_newmetatable(ls, OPT_METATABLE);
    lua_pushstring(ls, "__index");
    lua_pushcfunction(ls, option_get);
    lua_settable(ls, -3);

    luaL_getmetatable(ls, OPT_METATABLE);
    lua_pushstring(ls, "__newindex");
    lua_pushcfunction(ls, option_set);
    lua_settable(ls, -3);

    lua_settop(ls, top);

    // Create dummy userdata to front for our metatable
    int *dummy = static_cast<int *>(lua_newuserdata(ls, sizeof(int)));
    // Mystic number
    *dummy = 42;

    luaL_getmetatable(ls, OPT_METATABLE);
    lua_setmetatable(ls, -2);
    lua_setglobal(ls, "options");
}
