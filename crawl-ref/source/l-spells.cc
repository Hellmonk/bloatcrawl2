/*** Information about spells.
 * @module spells
 */
#include "AppHdr.h"

#include "l-libs.h"

#include "cluautil.h"
#include "food.h"
#include "religion.h"
#include "spl-book.h"
#include "spl-damage.h"
#include "spl-util.h"

/*** Is this spell memorised?
 * @tparam string spellname
 * @treturn boolean
 * @function memorised
 */
LUAFN(l_spells_memorised)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    PLUARET(boolean, you.has_spell(spell));
}

/*** What letter is this spell assigned to?
 * @tparam string name
 * @treturn string|nil the spell letter or nil if not memorised
 * @function letter
 */
LUAFN(l_spells_letter)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    if (!you.has_spell(spell))
    {
        lua_pushnil(ls);
        return 1;
    }
    char buf[2];
    buf[0] = get_spell_letter(spell);
    buf[1] = 0;
    PLUARET(string, buf);
}

/*** The level of the named spell.
 * @tparam string name
 * @treturn int
 * @function level
 */
LUAFN(l_spells_level)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    PLUARET(number, spell_difficulty(spell));
}

/*** The MP cost of the spell.
 * @tparam string name
 * @treturn int
 * @function mana_cost
 */
LUAFN(l_spells_mana_cost)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    PLUARET(number, spell_mana(spell));
}

/*** The current range of the spell.
 * @tparam string name
 * @treturn int
 * @function range
 */
LUAFN(l_spells_range)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    PLUARET(number, spell_range(spell, calc_spell_power(spell, true)));
}

/*** The maximum range of the spell.
 * @tparam string name
 * @treturn int
 * @function max_range
 */
LUAFN(l_spells_max_range)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    PLUARET(number, spell_range(spell, spell_power_cap(spell)));
}

/*** The minimum range of the spell.
 * @tparam string name
 * @treturn int
 * @function min_range
 */
LUAFN(l_spells_min_range)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    PLUARET(number, spell_range(spell, 0));
}

/*** The failure rate of the spell as a number in [0,100].
 * @tparam string name
 * @treturn int
 * @function fail
 */
LUAFN(l_spells_fail)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    PLUARET(number, failure_rate_to_int(raw_spell_fail(spell)));
}

/*** The failure severity of the spell.
 * TODO: Document these numbers
 * @tparam string name
 * @treturn int
 * @function fail_severity
 */
LUAFN(l_spells_fail_severity)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    PLUARET(number, fail_severity(spell));
}

/*** The current hunger of the spell.
 * @tparam string name
 * @treturn int number of hunger bars
 * @function hunger
 */
LUAFN(l_spells_hunger)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    PLUARET(number, hunger_bars(spell_hunger(spell)));
}

/*** The current spellpower (in bars).
 * @tparam string name
 * @treturn int
 * @function power
 */
LUAFN(l_spells_power)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    PLUARET(number, power_to_barcount(calc_spell_power(spell, true)));
}

/*** The maximum spellpower (in bars).
 * @tparam string name
 * @treturn int
 * @function max_power
 */
LUAFN(l_spells_max_power)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    PLUARET(number, power_to_barcount(spell_power_cap(spell)));
}

/*** Does this spell take a direction or target?
 * @tparam string name
 * @treturn boolean
 * @function dir_or_target
 */
LUAFN(l_spells_dir_or_target)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    const unsigned int flags = get_spell_flags(spell);
    PLUARET(boolean, flags & SPFLAG_DIR_OR_TARGET);
}

/*** Is this spell targetable?
 * @tparam string name
 * @treturn boolean
 * @function target
 */
LUAFN(l_spells_target)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    const unsigned int flags = get_spell_flags(spell);
    PLUARET(boolean, flags & SPFLAG_TARGET);
}

/*** Is this spell castable in a direction?
 * @tparam string name
 * @treturn boolean
 * @function dir
 */
LUAFN(l_spells_dir)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    const unsigned int flags = get_spell_flags(spell);
    PLUARET(boolean, flags & SPFLAG_DIR);
}

/*** Can this spell target objects?
 * @tparam string name
 * @treturn boolean
 * @function targ_obj
 */
LUAFN(l_spells_targ_obj)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    const unsigned int flags = get_spell_flags(spell);
    PLUARET(boolean, flags & SPFLAG_OBJ);
}

/*** Does our god like this spell?
 * @tparam string name
 * @treturn boolean
 * @function god_likes
 */
LUAFN(l_spells_god_likes)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    god_type god = you.religion;
    if (lua_gettop(ls) > 1)
    {
        const char *godname = luaL_checkstring(ls, 2);
        god = str_to_god(godname);
    }
    PLUARET(boolean, god_likes_spell(spell, god));
}

/*** Does our god hate this spell?
 * Casting this will result in pennance or excommunication.
 * @tparam string name
 * @treturn boolean
 * @function god_hates
 */
LUAFN(l_spells_god_hates)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    god_type god = you.religion;
    if (lua_gettop(ls) > 1)
    {
        const char *godname = luaL_checkstring(ls, 2);
        god = str_to_god(godname);
    }
    PLUARET(boolean, god_hates_spell(spell, god));
}

/*** Does our god loathe this spell?
 * Casting this will result in excommunication.
 * @tparam string name
 * @treturn boolean
 * @function god_loathes
 */
LUAFN(l_spells_god_loathes)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    god_type god = you.religion;
    if (lua_gettop(ls) > 1)
    {
        const char *godname = luaL_checkstring(ls, 2);
        god = str_to_god(godname);
    }
    PLUARET(boolean, god_loathes_spell(spell, god));
}

static const struct luaL_reg spells_clib[] =
{
    { "memorised"     , l_spells_memorised },
    { "letter"        , l_spells_letter },
    { "level"         , l_spells_level },
    { "mana_cost"     , l_spells_mana_cost },
    { "range"         , l_spells_range },
    { "max_range"     , l_spells_max_range },
    { "min_range"     , l_spells_min_range },
    { "fail"          , l_spells_fail },
    { "fail_severity" , l_spells_fail_severity },
    { "hunger"        , l_spells_hunger },
    { "power"         , l_spells_power },
    { "max_power"     , l_spells_max_power },
    { "dir_or_target" , l_spells_dir_or_target },
    { "target"        , l_spells_target },
    { "dir"           , l_spells_dir },
    { "targ_obj"      , l_spells_targ_obj },
    { "god_likes"     , l_spells_god_likes },
    { "god_hates"     , l_spells_god_hates },
    { "god_loathes"   , l_spells_god_loathes },
    { nullptr, nullptr }
};

void cluaopen_spells(lua_State *ls)
{
    luaL_openlib(ls, "spells", spells_clib, 0);
}
