/*** Information about the player.
 * @module you
 */
#include "AppHdr.h"

#include <cmath>

#include "l-libs.h"

#include "ability.h"
#include "abyss.h"
#include "areas.h"
#include "artefact.h"
#include "branch.h"
#include "chardump.h"
#include "cluautil.h"
#include "delay.h"
#include "english.h"
#include "env.h"
#include "food.h"
#include "initfile.h"
#include "item-name.h"
#include "item-prop.h"
#include "items.h"
#include "jobs.h"
#include "losglobal.h"
#include "mutation.h"
#include "nearby-danger.h"
#include "newgame-def.h"
#include "ng-setup.h"
#include "ouch.h"
#include "output.h"
#include "place.h"
#include "player.h"
#include "prompt.h"
#include "religion.h"
#include "shopping.h"
#include "skills.h"
#include "spl-book.h"
#include "spl-transloc.h"
#include "spl-util.h"
#include "status.h"
#include "stringutil.h"
#include "transform.h"
#include "traps.h"
#include "travel.h"
#include "wiz-you.h"

//
// Bindings to get information on the player (clua).
//

/*** Has player done something that takes time?
 * @treturn boolean
 * @function turn_is_over
 */
LUARET1(you_turn_is_over, boolean, you.turn_is_over)
/*** Get player's name.
 * @treturn string
 * @function name
 */
LUARET1(you_name, string, you.your_name.c_str())
/*** Get name of player's race.
 * @treturn string
 * @function race
 */
LUARET1(you_race, string, species_name(you.species).c_str())
/*** Get name of player's background.
 * @treturn string
 * @function class
 */
LUARET1(you_class, string, get_job_name(you.char_class))
/*** Is player in wizard mode?
 * @treturn boolean
 * @function wizard
 */
LUARET1(you_wizard, boolean, you.wizard)
/*** Get name of player's god
 * @treturn string
 * @function god
 */
LUARET1(you_god, string, god_name(you.religion).c_str())
/*** Is this [player's] god good?
 * @tparam[opt=you.god()] string god
 * @treturn boolean
 * @function good_god
 */
LUARET1(you_good_god, boolean,
        lua_isstring(ls, 1) ? is_good_god(str_to_god(lua_tostring(ls, 1)))
        : is_good_god(you.religion))
/*** Is this [player's] god evil?
 * @tparam[opt=you.god()] string god
 * @treturn boolean
 * @function evil_god
 */
LUARET1(you_evil_god, boolean,
        lua_isstring(ls, 1) ? is_evil_god(str_to_god(lua_tostring(ls, 1)))
        : is_evil_god(you.religion))
/*** Has the [player's] current god's one-time ability been used? (if any).
 * @treturn boolean
 * @function one_time_ability_used
 */
LUARET1(you_one_time_ability_used, boolean,
        you.one_time_ability_used[you.religion])
/*** Hit points.
 * @treturn int current hp
 * @treturn int current max hp
 * @function hp
 */
LUARET2(you_hp, number, you.hp, you.hp_max)
/*** Magic points.
 * @treturn int current mp
 * @treturn int current max mp
 * @function mp
 */
LUARET2(you_mp, number, you.magic_points, you.max_magic_points)
/*** Base max mp.
 * @treturn int
 * @function base_mp
 */
LUARET1(you_base_mp, number, get_real_mp(false))
/*** How much rot.
 * @treturn int
 * @function rot
 */
LUARET1(you_rot, number, player_rotted())
/*** Minimum hp after poison wears off.
 * @treturn int
 * @function poison_survival
 */
LUARET1(you_poison_survival, number, poison_survival())
/*** Corrosion amount.
 * @treturn int
 * @function corrosion
 */
LUARET1(you_corrosion, number, you.props["corrosion_amount"].get_int())
/*** Hunger state number.
 * @treturn int
 * @function hunger
 */
LUARET1(you_hunger, number, you.hunger_state - 1)
/*** Hunger state string.
 * @treturn string
 * @function hunger_name
 */
LUARET1(you_hunger_name, string, hunger_level())
/*** Strength.
 * @treturn int current strength
 * @treturn int max strength
 * @function strength
 */
LUARET2(you_strength, number, you.strength(false), you.max_strength())
/*** Intelligence.
 * @treturn int current intelligence
 * @treturn int max intelligence
 * @function intelligence
 */
LUARET2(you_intelligence, number, you.intel(false), you.max_intel())
/*** Dexterity.
 * @treturn int current dexterity
 * @treturn int max dexterity
 * @function dexterity
 */
LUARET2(you_dexterity, number, you.dex(false), you.max_dex())
/*** XL.
 * @treturn int xl
 * @tfunction xl
 */
LUARET1(you_xl, number, you.experience_level)
/*** XL progress.
 * @treturn number percentage of the way to the next xl [0,100]
 * @function xl_progress
 */
LUARET1(you_xl_progress, number, get_exp_progress())

/*** Skill progress.
 * @tparam string name skill name
 * @treturn number percentage of the way to the next skill level
 * @function skill_progress
 */
LUAFN(you_skill_progress)
{
    string sk_name = luaL_checkstring(ls, 1);
    skill_type sk = str_to_skill(lua_tostring(ls, 1));
    if (sk > NUM_SKILLS)
    {
        string err = make_stringf("Unknown skill name `%s`.", sk_name.c_str());
        return luaL_argerror(ls, 1, err.c_str());
    }
    PLUARET(number, get_skill_percentage(str_to_skill(lua_tostring(ls, 1))));
}

/*** Can a skill be trained?
 * @tparam string name skill name
 * @treturn boolean
 * @function can_train_skill
 */
LUAFN(you_can_train_skill)
{
    string sk_name = luaL_checkstring(ls, 1);
    skill_type sk = str_to_skill(lua_tostring(ls, 1));
    if (sk > NUM_SKILLS)
    {
        string err = make_stringf("Unknown skill name `%s`.", sk_name.c_str());
        return luaL_argerror(ls, 1, err.c_str());
    }
    PLUARET(boolean, you.can_currently_train[sk]);
}

/*** Best skill.
 * @treturn string
 * @function best_skill
 */
LUARET1(you_best_skill, string,
        skill_name(best_skill(SK_FIRST_SKILL, SK_LAST_SKILL)))
/*** Poison resistance (rPois).
 * @treturn int resistance level
 * @function res_poison
 */
LUARET1(you_res_poison, number, player_res_poison(false))
/*** Fire resistance (rF).
 * @treturn int resistance level
 * @function res_fire
 */
LUARET1(you_res_fire, number, player_res_fire(false))
/*** Cold resistance (rC).
 * @treturn int resistance level
 * @function res_cold
 */
LUARET1(you_res_cold, number, player_res_cold(false))
/*** Negative energy resistance (rN).
 * @treturn int resistance level
 * @function res_draining
 */
LUARET1(you_res_draining, number, player_prot_life(false))
/*** Electric resistance (rElec).
 * @treturn int resistance level
 * @function res_shock
 */
LUARET1(you_res_shock, number, player_res_electricity(false))
/*** Stealth pips.
 * @treturn int number of stealth pips
 * @function stealth_pips
 */
LUARET1(you_stealth_pips, number, stealth_breakpoint(player_stealth()))
/*** Magic resistance (MR).
 * @treturn int number of MR pips
 * @function res_magic
 */
LUARET1(you_res_magic, number, player_res_magic(false) / MR_PIP)
/*** Drowning resistance (rDrown).
 * @treturn int resistance level
 * @function res_drowning
 */
LUARET1(you_res_drowning, boolean, you.res_water_drowning())
/*** Mutation resistance (rMut).
 * @treturn int resistance level
 * @function res_mutation
 */
LUARET1(you_res_mutation, number, you.rmut_from_item(false) ? 1 : 0)
/*** See invisible (sInv).
 * @treturn boolean
 * @function see_invisible
 */
LUARET1(you_see_invisible, boolean, you.can_see_invisible(false))
/*** Guardian spirit.
 * Returns a number for backwards compatibility.
 * @treturn int
 * @function spirit_shield
 */
LUARET1(you_spirit_shield, number, you.spirit_shield(false) ? 1 : 0)
/*** Gourmand.
 * @treturn boolean
 * @function gourmond
 */
LUARET1(you_gourmand, boolean, you.gourmand(false))
/*** Corrosion resistance (rCorr).
 * @treturn int resistance level
 * @function res_corr
 */
LUARET1(you_res_corr, boolean, you.res_corr(false))
/*** Do you like to eat chunks?
 * Returns a number so as not to break existing scripts.
 * @treturn int
 * @function like_chunks
 */
LUARET1(you_like_chunks, number, player_likes_chunks(true) ? 3 : 0)
/*** Are you flying?
 * @treturn boolean
 * @function flying
 */
LUARET1(you_flying, boolean, you.airborne())
/*** Current transformation, if any.
 * @treturn string transformation name
 * @function transform
 */
LUARET1(you_transform, string, you.form == transformation::none
                               ? "" : transform_name())
/*** Are you berserk?
 * @treturn boolean
 * @function berserk
 */
LUARET1(you_berserk, boolean, you.berserk())
/*** Are you confused?
 * @treturn boolean
 * @function confused
 */
LUARET1(you_confused, boolean, you.confused())
/*** Do you have a Shroud of Golubria?
 * @treturn boolean
 * @function shrouded
 */
LUARET1(you_shrouded, boolean, you.duration[DUR_SHROUD_OF_GOLUBRIA])
/*** Are you currently +Swift or -Swift?
 * If you have neither, returns 0. If you are +Swift, +1, and -Swift, -1.
 * @treturn int Swift level
 * @function swift
 */
LUARET1(you_swift, number, you.duration[DUR_SWIFTNESS] ? ((you.attribute[ATTR_SWIFTNESS] >= 0) ? 1 : -1) : 0)
/*** Are you paralysed?
 * @treturn boolean
 * @function paralysed
 */
LUARET1(you_paralysed, boolean, you.paralysed())
/*** Are you asleep?
 * @treturn boolean
 * @function asleep
 */
LUARET1(you_asleep, boolean, you.asleep())
/*** Are you hasted?
 * @treturn boolean
 * @function hasted
 */
LUARET1(you_hasted, boolean, you.duration[DUR_HASTE])
/*** Are you slowed?
 * @treturn boolean
 * @function slowed
 */
LUARET1(you_slowed, boolean, you.duration[DUR_SLOW])
/*** Are you exhausted?
 * @treturn boolean
 * @function exhausted
 */
LUARET1(you_exhausted, boolean, you.duration[DUR_EXHAUSTED])
/*** Are you teleporting?
 * @treturn boolean
 * @function teleporting
 */
LUARET1(you_teleporting, boolean, you.duration[DUR_TELEPORT])
/*** Are you dimensionally anchored?
 * @treturn boolean
 * @function anchored
 */
LUARET1(you_anchored, boolean, you.duration[DUR_DIMENSION_ANCHOR])
/*** Are you rooted?
 * @treturn boolean
 * @function rooted
 */
LUARET1(you_rooted, boolean, you.duration[DUR_GRASPING_ROOTS])
/*** Are you poisoned?
 * @treturn boolean
 * @function poisoned
 */
LUARET1(you_poisoned, boolean, you.duration[DUR_POISONING])
/*** Are you invisible?
 * @treturn boolean
 * @function invisible
 */
LUARET1(you_invisible, boolean, you.duration[DUR_INVIS])
/*** Are you mesmerised?
 * @treturn boolean
 * @function mesmerised
 */
LUARET1(you_mesmerised, boolean, you.duration[DUR_MESMERISED])
/*** Are you on fire?
 * @treturn boolean
 * @function on_fire
 */
LUARET1(you_on_fire, boolean, you.duration[DUR_LIQUID_FLAMES])
/*** Are you petrifying?
 * @treturn boolean
 * @function petrifying
 */
LUARET1(you_petrifying, boolean, you.duration[DUR_PETRIFYING])
/*** Are you silencing the world around you?
 * @treturn boolean
 * @function silencing
 */
LUARET1(you_silencing, boolean, you.duration[DUR_SILENCE])
/*** Are you regenerating?
 * @treturn boolean
 * @function regenerating
 */
LUARET1(you_regenerating, boolean, you.duration[DUR_TROGS_HAND])
/*** Are you out of breath?
 * @treturn boolean
 * @function breath_timeout
 */
LUARET1(you_breath_timeout, boolean, you.duration[DUR_BREATH_WEAPON])
/*** Are you extra resistant?
 * @treturn boolean
 * @function extra_resistant
 */
LUARET1(you_extra_resistant, boolean, you.duration[DUR_RESISTANCE])
/*** Are you mighty?
 * @treturn boolean
 * @function mighty
 */
LUARET1(you_mighty, boolean, you.duration[DUR_MIGHT])
/*** Are you agile?
 * @treturn boolean
 * @function agile
 */
LUARET1(you_agile, boolean, you.duration[DUR_AGILITY])
/*** Are you brilliant?
 * @treturn boolean
 * @function brilliant
 */
LUARET1(you_brilliant, boolean, you.duration[DUR_BRILLIANCE])
/*** Are you silenced?
 * @treturn boolean
 * @function silenced
 */
LUARET1(you_silenced, boolean, silenced(you.pos()))
/*** Are you sick?
 * @treturn boolean
 * @function sick
 */
LUARET1(you_sick, boolean, you.disease)
/*** Are you contaminated?
 * @treturn number
 * @function contaminated
 */
LUARET1(you_contaminated, number, get_contamination_level())
/*** Do you feel safe?
 * @treturn boolean
 * @function feel_safe
 */
LUARET1(you_feel_safe, boolean, i_feel_safe())
/*** How many times have you died?
 * @treturn int
 * @function deaths
 */
LUARET1(you_deaths, number, you.deaths)
/*** How many extra lives do you have?
 * @treturn int
 * @function lives
 */
LUARET1(you_lives, number, you.lives)
#if TAG_MAJOR_VERSION == 34
LUARET1(you_antimagic, boolean, you.duration[DUR_ANTIMAGIC])
#endif

/*** Where are you?
 * @treturn string
 * @function where
 */
LUARET1(you_where, string, level_id::current().describe().c_str())
/*** What branch are you in?
 * @treturn string
 * @function branch
 */
LUARET1(you_branch, string, level_id::current().describe(false, false).c_str())
/*** Your current depth in that branch.
 * @treturn int
 * @function depth
 */
LUARET1(you_depth, number, you.depth)
/*** What fraction of the branch you've gone into.
 * @treturn number
 * @function depth_fraction
 */
LUARET1(you_depth_fraction, number,
        (brdepth[you.where_are_you] <= 1) ? 1
        : ((float)(you.depth - 1) / (brdepth[you.where_are_you] - 1)))
/*** The current "absolute depth".
 * Absolute depth of the current location.
 * @treturn number
 * @function absdepth
 */
// [ds] Absolute depth is 1-based for Lua to match things like DEPTH:
// which are also 1-based. Yes, this is confusing. FIXME: eventually
// change you.absdepth0 to be 1-based as well.
// [1KB] FIXME: eventually eliminate the notion of absolute depth at all.
LUARET1(you_absdepth, number, env.absdepth0 + 1)
/*** How long has the player been on the current level?
 * @treturn number
 * @function turns_on_level
 */
LUARET1(you_turns_on_level, number, env.turns_on_level)
/*** Interrupt the current multi-turn activity or macro sequence.
 * @function stop_activity
 */
LUAWRAP(you_stop_activity, interrupt_activity(activity_interrupt::force))
/*** Are you taking the stairs?
 * @treturn boolean
 * @function taking_stairs
 */
LUARET1(you_taking_stairs, boolean, player_stair_delay())
/*** How many turns have been taken.
 * @treturn int
 * @function turns
 */
LUARET1(you_turns, number, you.num_turns)
/*** Total elapsed time in auts.
 * @treturn int
 * @function time
 */
LUARET1(you_time, number, you.elapsed_time)
/*** How many spell levels are currently available.
 * @treturn int
 * @function spell_levels
 */
LUARET1(you_spell_levels, number, player_spell_levels())
/*** Can you smell?
 * @treturn boolean
 * @function can_smell
 */
LUARET1(you_can_smell, boolean, you.can_smell())
/*** Do you have claws?
 * @treturn int claws level
 * @function has_claws
 */
LUARET1(you_has_claws, number, you.has_claws(false))
/*** How many temporary mutations do you have?
 * @treturn int
 * @function temp_mutations
 */
LUARET1(you_temp_mutations, number, you.attribute[ATTR_TEMP_MUTATIONS])
/*** Mutation overview string.
 * @treturn string
 * @function mutation_overview
 */
LUARET1(you_mutation_overview, string, mutation_overview().c_str())

/*** LOS Radius.
 * @treturn int
 * @function los
 */
LUARET1(you_los, number, get_los_radius())
/*** Can you see a cell?
 * Uses player-centered coordinates
 * @tparam int x
 * @tparam int y
 * @treturn boolean
 * @function see_cell
 */
LUARET1(you_see_cell_rel, boolean,
        you.see_cell(coord_def(luaL_safe_checkint(ls, 1), luaL_safe_checkint(ls, 2)) + you.pos()))
/*** Can you see this cell without looking through a window?
 * Checks line of sight treating transparent rock and stone as opaque.
 * Uses player-centered coordinates.
 * @tparam int x
 * @tparam int y
 * @treturn boolean
 * @function see_cell_no_trans
 */
LUARET1(you_see_cell_no_trans_rel, boolean,
        you.see_cell_no_trans(coord_def(luaL_safe_checkint(ls, 1), luaL_safe_checkint(ls, 2)) + you.pos()))
/*** Can you see this cell without something solid in the way?
 * Checks line of sight treating all solid features as opaque.
 * Uses player-centered coordinates.
 * @tparam int x
 * @tparam int y
 * @treturn boolean
 * @function see_cell_solid
 */
LUARET1(you_see_cell_solid_rel, boolean,
        cell_see_cell(you.pos(),
                      (coord_def(luaL_safe_checkint(ls, 1),
                                 luaL_safe_checkint(ls, 2)) + you.pos()),
                      LOS_SOLID))
/*** Can you see this cell with nothing in the way?
 * Checks line of sight treating all solid features as opaque and properly
 * checking bushes and clouds.
 * Uses player-centered coordinates.
 * @tparam int x
 * @tparam int y
 * @treturn boolean
 * @function see_cell_solid_see
 */
LUARET1(you_see_cell_solid_see_rel, boolean,
        cell_see_cell(you.pos(),
                      (coord_def(luaL_safe_checkint(ls, 1),
                                 luaL_safe_checkint(ls, 2)) + you.pos()),
                      LOS_SOLID_SEE))
/*** Stars of piety.
 * @treturn int
 * @function piety_rank
 */
LUARET1(you_piety_rank, number, piety_rank())
/*** Are you under penance?
 * @treturn boolean
 * @function under_penance
 */
LUARET1(you_under_penance, boolean,
        lua_isstring(ls, 1) ? player_under_penance(str_to_god(lua_tostring(ls, 1)))
                            : player_under_penance())
/*** Are you currently constricted?
 * @treturn boolean
 * @function constricted
 */
LUARET1(you_constricted, boolean, you.is_constricted())
/*** Are you currently constricting anything?
 * @treturn boolean
 * @function constricting
 */
LUARET1(you_constricting, boolean, you.is_constricting())

/*** The name of your monster type.
 * @treturn string
 * @function monster
 */
static int l_you_monster(lua_State *ls)
{
    const monster_type mons = player_species_to_mons_species(you.species);

    string name = mons_type_name(mons, DESC_PLAIN);
    lowercase(name);

    lua_pushstring(ls, name.c_str());
    return 1;
}

/*** Your genus.
 * As a lower-case plural string.
 * @treturn string
 * @function genus
 */
static int l_you_genus(lua_State *ls)
{
    bool plural = lua_toboolean(ls, 1);
    string genus = species_name(you.species, SPNAME_GENUS);
    lowercase(genus);
    if (plural)
        genus = pluralise(genus);
    lua_pushstring(ls, genus.c_str());
    return 1;
}

/*** The items on your cell on the floor.
 * @treturn array an Array of @{items.Item} objects
 * @function floor_items
 */
static int you_floor_items(lua_State *ls)
{
    lua_push_floor_items(ls, you.visible_igrd(you.pos()));
    return 1;
}

/*** List your spells
 * @treturn array An array of spell names
 * @see spells
 * @function spells
 */
static int l_you_spells(lua_State *ls)
{
    lua_newtable(ls);
    int index = 0;
    for (int i = 0; i < 52; ++i)
    {
        const spell_type spell = get_spell_by_letter(index_to_letter(i));
        if (spell == SPELL_NO_SPELL)
            continue;

        lua_pushstring(ls, spell_title(spell));
        lua_rawseti(ls, -2, ++index);
    }
    return 1;
}

/*** Currently used spell letters
 * @treturn array An array of letters
 * @see spells
 * @function spell_letters
 */
static int l_you_spell_letters(lua_State *ls)
{
    lua_newtable(ls);
    int index = 0;

    char buf[2];
    buf[1] = 0;

    for (int i = 0; i < 52; ++i)
    {
        buf[0] = index_to_letter(i);
        const spell_type spell = get_spell_by_letter(buf[0]);
        if (spell == SPELL_NO_SPELL)
            continue;

        lua_pushstring(ls, buf);
        lua_rawseti(ls, -2, ++index);
    }
    return 1;
}

/*** Get the spell table.
 * @treturn table A table matching spell letters to spell names
 * @see spells
 * @function spell_table
 */
static int l_you_spell_table(lua_State *ls)
{
    lua_newtable(ls);

    char buf[2];
    buf[1] = 0;

    for (int i = 0; i < 52; ++i)
    {
        buf[0] = index_to_letter(i);
        const spell_type spell = get_spell_by_letter(buf[0]);
        if (spell == SPELL_NO_SPELL)
            continue;

        lua_pushstring(ls, buf);
        lua_pushstring(ls, spell_title(spell));
        lua_rawset(ls, -3);
    }
    return 1;
}

/*** Get memorisable spells.
 * This lists the spells available in the spell library.
 * @treturn array An array of spell names.
 * @function mem_spells
 */
static int l_you_mem_spells(lua_State *ls)
{
    lua_newtable(ls);

    char buf[2];
    buf[1] = 0;

    vector<spell_type> mem_spells = get_sorted_spell_list(true);

    for (size_t i = 0; i < mem_spells.size(); ++i)
    {
        buf[0] = index_to_letter(i);

        lua_pushstring(ls, buf);
        lua_pushstring(ls, spell_title(mem_spells[i]));
        lua_rawset(ls, -3);
    }
    return 1;
}

/*** Available abilities
 * @treturn array An array of ability names.
 * @function abils
 */
static int l_you_abils(lua_State *ls)
{
    lua_newtable(ls);

    vector<const char *>abils = get_ability_names();
    for (int i = 0, size = abils.size(); i < size; ++i)
    {
        lua_pushstring(ls, abils[i]);
        lua_rawseti(ls, -2, i + 1);
    }
    return 1;
}

/*** Ability letters in use.
 * @treturn array An array of ability letters
 * @function abil_letters
 */
static int l_you_abil_letters(lua_State *ls)
{
    lua_newtable(ls);

    char buf[2];
    buf[1] = 0;

    vector<talent> talents = your_talents(false);
    for (int i = 0, size = talents.size(); i < size; ++i)
    {
        buf[0] = talents[i].hotkey;
        lua_pushstring(ls, buf);
        lua_rawseti(ls, -2, i + 1);
    }
    return 1;
}

/*** Ability table.
 * @treturn table A map of letters to ability names
 * @function abil_table
 */
static int l_you_abil_table(lua_State *ls)
{
    lua_newtable(ls);

    char buf[2];
    buf[1] = 0;

    for (const talent &tal : your_talents(false))
    {
        buf[0] = tal.hotkey;
        lua_pushstring(ls, buf);
        lua_pushstring(ls, ability_name(tal.which));
        lua_rawset(ls, -3);
    }
    return 1;
}

/*** How much gold do you have?
 * @treturn int
 * @function gold
 */
static int you_gold(lua_State *ls)
{
    if (lua_gettop(ls) >= 1 && !CLua::get_vm(ls).managed_vm)
    {
        const int new_gold = luaL_safe_checkint(ls, 1);
        const int old_gold = you.gold;
        you.set_gold(max(new_gold, 0));
        if (new_gold > old_gold)
            you.attribute[ATTR_GOLD_FOUND] += new_gold - old_gold;
        else if (old_gold > new_gold)
            you.attribute[ATTR_MISC_SPENDING] += old_gold - new_gold;
    }
    PLUARET(number, you.gold);
}

/*** Can you eat chunks?
 * @treturn boolean
 * @function you_can_consume_corpses
 */
static int you_can_consume_corpses(lua_State *ls)
{
    lua_pushboolean(ls, you.get_mutation_level(MUT_HERBIVOROUS) == 0
                        && !you_foodless());
    return 1;
}

static int _you_have_rune(lua_State *ls)
{
    int which_rune = NUM_RUNE_TYPES;
    if (lua_gettop(ls) >= 1 && lua_isnumber(ls, 1))
        which_rune = luaL_safe_checkint(ls, 1);
    else if (lua_gettop(ls) >= 1 && lua_isstring(ls, 1))
    {
        const char *spec = lua_tostring(ls, 1);
        for (which_rune = 0; which_rune < NUM_RUNE_TYPES; which_rune++)
            if (!strcasecmp(spec, rune_type_name(which_rune)))
                break;
    }
    bool have_rune = false;
    if (which_rune >= 0 && which_rune < NUM_RUNE_TYPES)
        have_rune = you.runes[which_rune];
    lua_pushboolean(ls, have_rune);
    return 1;
}

/*** Are you intrinsically immune to this particular hex spell?
 * @tparam string spell name
 * @treturn boolean
 * @function you_immune_to_hex
 */
static int you_immune_to_hex(lua_State *ls)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    lua_pushboolean(ls, you.immune_to_hex(spell));
    return 1;
}

/*** How many runes do you have?
 * @treturn int
 * @function num_runes
 */
LUARET1(you_num_runes, number, runes_in_pack())

/*** Do you have the orb?
 * @treturn boolean
 * @function have_orb
 */
LUARET1(you_have_orb, boolean, player_has_orb())

/*** Are you caught in something?
 * @treturn string|nil what's got you
 * @function caught
 */
LUAFN(you_caught)
{
    if (you.caught())
        lua_pushstring(ls, held_status(&you));
    else
        lua_pushnil(ls);

    return 1;
}

/*** Get the mutation level of a mutation.
 * If all optional parameters are false this returns zero.
 * @tparam string mutationname
 * @tparam[opt=true] boolean innate include innate mutations
 * @tparam[optchain=true] boolean temp include temporary mutations
 * @tparam[optchain=true] boolean normal include normal mutations
 * @treturn int level
 * @function get_base_mutation_level
 */
LUAFN(you_get_base_mutation_level)
{
    string mutname = luaL_checkstring(ls, 1);
    bool innate = true;
    bool temp = true;
    bool normal = true;
    if (!lua_isnoneornil(ls, 2))
        innate = lua_toboolean(ls, 2);
    if (!lua_isnoneornil(ls, 3))
        temp = lua_toboolean(ls, 3);
    if (!lua_isnoneornil(ls, 4))
        normal = lua_toboolean(ls, 4);
    mutation_type mut = mutation_from_name(mutname, false);
    if (mut != NUM_MUTATIONS)
        PLUARET(integer, you.get_base_mutation_level(mut, innate, temp, normal)); // includes innate mutations

    string err = make_stringf("No such mutation: '%s'.", mutname.c_str());
    return luaL_argerror(ls, 1, err.c_str());
}

/*** How mutated are you?
 * Adds up the total number (including levels if requested) of mutations.
 * @tparam boolean innate include innate mutations
 * @tparam boolean levels count levels
 * @tparam boolean temp include temporary mutations
 * @treturn int
 * @function how_mutated
 */
LUAFN(you_how_mutated)
{
    bool innate = lua_toboolean(ls, 1); // whether to include innate mutations
    bool levels = lua_toboolean(ls, 2); // whether to count levels
    bool temp = lua_toboolean(ls, 3); // whether to include temporary mutations
    int result = you.how_mutated(innate, levels, temp);
    PLUARET(number, result);
}

/*** Deprecated: use @{you.get_base_mutation_level}.
 * Equivalent to `you.get_base_mutation_level(name)`
 * @tparam string mutation name
 * @treturn int num
 * @function mutation
 */
LUAFN(you_mutation)
{
    string mutname = luaL_checkstring(ls, 1);
    mutation_type mut = mutation_from_name(mutname, false);
    if (mut != NUM_MUTATIONS)
        PLUARET(integer, you.get_base_mutation_level(mut, true, true, true)); // includes innate, temp mutations. I'm not sure if this is what was intended but this was the old behaviour.

    string err = make_stringf("No such mutation: '%s'.", mutname.c_str());
    return luaL_argerror(ls, 1, err.c_str());
}


/*** Deprecated: use @{you.get_base_mutation_level}.
 * Equivalent to `you.get_base_mutation_level(name, false, true, false)`
 * @tparam string mutation name
 * @treturn int num
 * @function temp_mutation
 */
LUAFN(you_temp_mutation)
{
    string mutname = luaL_checkstring(ls, 1);
    mutation_type mut = mutation_from_name(mutname, false);
    if (mut != NUM_MUTATIONS)
        PLUARET(integer, you.get_base_mutation_level(mut, false, true, false));

    string err = make_stringf("No such mutation: '%s'.", mutname.c_str());
    return luaL_argerror(ls, 1, err.c_str());
}

/*** Check the level stack.
 * When entering portal branches or the abyss crawl tracks
 * where we came from with a level stack, which we can query.
 * Can take a Branch or Branch:depth string.
 * @tparam string levelname
 * @treturn boolean
 * @function is_level_on_stack
 */
LUAFN(you_is_level_on_stack)
{
    string levname = luaL_checkstring(ls, 1);
    level_id lev;
    try
    {
        lev = level_id::parse_level_id(levname);
    }
    catch (const bad_level_id &err)
    {
        return luaL_argerror(ls, 1, err.what());
    }

    PLUARET(boolean, is_level_on_stack(lev));
}

/*** Current skill level.
 * @tparam string name
 * @treturn number
 * @function skill
 */
LUAFN(you_skill)
{
    skill_type sk = str_to_skill_safe(luaL_checkstring(ls, 1));

    PLUARET(number, you.skill(sk, 10) * 0.1);
}

/*** Base skill level.
 * Ignores drain, crosstraining, &c.
 * @tparam string name
 * @function base_skill
 */
LUAFN(you_base_skill)
{
    skill_type sk = str_to_skill_safe(luaL_checkstring(ls, 1));

    PLUARET(number, you.skill(sk, 10, true) * 0.1);
}

/*** Train a skill.
 * Training levels:
 *
 * - 0 is disable
 * - 1 is enable
 * - 2 is focus
 *
 * If no level is passed, does not change the current training.
 * @tparam string name
 * @tparam[opt] int level set training level
 * @treturn int training level
 * @function train_skill
 */
LUAFN(you_train_skill)
{
    skill_type sk = str_to_skill_safe(luaL_checkstring(ls, 1));
    if (lua_gettop(ls) >= 2 && can_enable_skill(sk))
    {
        you.train[sk] = min(max((training_status)luaL_safe_checkint(ls, 2),
                                                 TRAINING_DISABLED),
                                             TRAINING_FOCUSED);
        reset_training();
    }

    PLUARET(number, you.train[sk]);
}

/*** Get a training target.
 * @tparam string name
 * @treturn number
 * @function get_training_target
 */
LUAFN(you_get_training_target)
{
    string sk_name = luaL_checkstring(ls, 1);
    skill_type sk = str_to_skill(sk_name);
    if (sk == SK_NONE)
    {
        string err = make_stringf("Unknown skill name `%s`", sk_name.c_str());
        return luaL_argerror(ls, 1, err.c_str());
    }

    PLUARET(number, (double) you.get_training_target(sk) * 0.1);
}

/*** Set a training target.
 * @tparam string name
 * @tparam number target
 * @treturn number|nil if successfully set the new target
 * @function set_training_target
 */
LUAFN(you_set_training_target)
{
    string sk_name = luaL_checkstring(ls, 1);
    skill_type sk = str_to_skill(sk_name);
    if (sk == SK_NONE)
    {
        string err = make_stringf("Unknown skill name `%s`", sk_name.c_str());
        return luaL_argerror(ls, 1, err.c_str());
    }
    else if (!you.set_training_target(sk, luaL_checknumber(ls, 2), true))
        return 0; // not a full-on error
    return 1;
}

/*** Cost of training this skill.
 * @tparam string name
 * @treturn number
 * @function skill_cost
 */
LUAFN(you_skill_cost)
{
    skill_type sk = str_to_skill_safe(luaL_checkstring(ls, 1));
    float cost = scaled_skill_cost(sk);
    if (cost == 0)
    {
        lua_pushnil(ls);
        return 1;
    }
    PLUARET(number, max(1, (int)(10.0 * cost + 0.5)) * 0.1);
}

/*** Check status effects.
 * Given a specific status name, checks for that status.
 * Otherwise, lists all statuses.
 * @tparam[opt] string stat
 * @treturn string|boolean
 * @function status
 */
LUAFN(you_status)
{
    const char* which = nullptr;
    if (lua_gettop(ls) >= 1)
        which = luaL_checkstring(ls, 1);

    string status_effects = "";
    status_info inf;
    for (unsigned i = 0; i <= STATUS_LAST_STATUS; ++i)
    {
        if (fill_status_info(i, inf) && !inf.short_text.empty())
        {
            if (which)
            {
                if (inf.short_text == which)
                    PLUARET(boolean, true);
            }
            else
            {
                if (!status_effects.empty())
                    status_effects += ",";
                status_effects += inf.short_text;
            }
        }
    }
    if (which)
        PLUARET(boolean, false);
    PLUARET(string, status_effects.c_str());
}

static const struct luaL_reg you_clib[] =
{
    { "turn_is_over", you_turn_is_over },
    { "turns"       , you_turns },
    { "time"        , you_time },
    { "spells"      , l_you_spells },
    { "spell_letters", l_you_spell_letters },
    { "spell_table" , l_you_spell_table },
    { "spell_levels", you_spell_levels },
    { "mem_spells", l_you_mem_spells },
    { "abilities"   , l_you_abils },
    { "ability_letters", l_you_abil_letters },
    { "ability_table", l_you_abil_table },
    { "name"        , you_name },
    { "race"        , you_race },
    { "class"       , you_class },
    { "genus"       , l_you_genus },
    { "monster"     , l_you_monster },
    { "wizard"      , you_wizard },
    { "god"         , you_god },
    { "gold"        , you_gold },
    { "good_god"    , you_good_god },
    { "evil_god"    , you_evil_god },
    { "one_time_ability_used" , you_one_time_ability_used },
    { "hp"          , you_hp },
    { "mp"          , you_mp },
    { "base_mp"     , you_base_mp },
    { "rot"         , you_rot },
    { "hunger"      , you_hunger },
    { "hunger_name" , you_hunger_name },
    { "strength"    , you_strength },
    { "intelligence", you_intelligence },
    { "dexterity"   , you_dexterity },
    { "skill"       , you_skill },
    { "base_skill"  , you_base_skill },
    { "skill_progress", you_skill_progress },
    { "can_train_skill", you_can_train_skill },
    { "best_skill",   you_best_skill },
    { "train_skill",  you_train_skill },
    { "skill_cost"  , you_skill_cost },
    { "get_training_target", you_get_training_target },
    { "set_training_target", you_set_training_target },
    { "xl"          , you_xl },
    { "xl_progress" , you_xl_progress },
    { "res_poison"  , you_res_poison },
    { "res_fire"    , you_res_fire   },
    { "res_cold"    , you_res_cold   },
    { "res_draining", you_res_draining },
    { "res_shock"   , you_res_shock },
    { "stealth_pips", you_stealth_pips },
    { "res_magic"   , you_res_magic },
    { "res_drowning", you_res_drowning },
    { "res_mutation", you_res_mutation },
    { "see_invisible", you_see_invisible },
    { "spirit_shield", you_spirit_shield },
    { "like_chunks",  you_like_chunks },
    { "gourmand",     you_gourmand },
    { "res_corr",     you_res_corr },
    { "flying",       you_flying },
    { "transform",    you_transform },
    { "berserk",      you_berserk },
    { "confused",     you_confused },
    { "paralysed",    you_paralysed },
    { "shrouded",     you_shrouded },
    { "swift",        you_swift },
    { "caught",       you_caught },
    { "asleep",       you_asleep },
    { "hasted",       you_hasted },
    { "slowed",       you_slowed },
    { "exhausted",    you_exhausted },
    { "teleporting",  you_teleporting },
    { "anchored",     you_anchored },
    { "rooted",       you_rooted },
    { "poisoned",     you_poisoned },
    { "poison_survival", you_poison_survival },
    { "corrosion",    you_corrosion },
    { "invisible",    you_invisible },
    { "mesmerised",   you_mesmerised },
    { "on_fire",      you_on_fire },
    { "petrifying",   you_petrifying },
    { "silencing",    you_silencing },
    { "regenerating", you_regenerating },
    { "breath_timeout", you_breath_timeout },
    { "extra_resistant", you_extra_resistant },
    { "mighty",       you_mighty },
    { "agile",        you_agile },
    { "brilliant",    you_brilliant },
    { "silenced",     you_silenced },
    { "sick",         you_sick },
    { "contaminated", you_contaminated },
    { "feel_safe",    you_feel_safe },
    { "deaths",       you_deaths },
    { "lives",        you_lives },
    { "piety_rank",   you_piety_rank },
    { "under_penance", you_under_penance },
    { "constricted",  you_constricted },
    { "constricting", you_constricting },
#if TAG_MAJOR_VERSION == 34
    { "antimagic",    you_antimagic },
#endif
    { "status",       you_status },
    { "immune_to_hex", you_immune_to_hex },

    { "can_consume_corpses",      you_can_consume_corpses },

    { "stop_activity", you_stop_activity },
    { "taking_stairs", you_taking_stairs },

    { "floor_items",  you_floor_items },

    { "where",        you_where },
    { "branch",       you_branch },
    { "depth",        you_depth },
    { "depth_fraction", you_depth_fraction },
    { "absdepth",     you_absdepth },
    { "turns_on_level", you_turns_on_level },
    { "is_level_on_stack", you_is_level_on_stack },

    { "can_smell",         you_can_smell },
    { "has_claws",         you_has_claws },

    { "los",               you_los },
    { "see_cell",          you_see_cell_rel },
    { "see_cell_no_trans", you_see_cell_no_trans_rel },
    { "see_cell_solid",    you_see_cell_solid_rel },
    { "see_cell_solid_see",you_see_cell_solid_see_rel },

    { "get_base_mutation_level", you_get_base_mutation_level },
    { "mutation",                you_mutation }, // still here for compatibility
    { "temp_mutation",           you_temp_mutation }, // still here for compatibility
    { "how_mutated",             you_how_mutated },
    { "temp_mutations",          you_temp_mutations },
    { "mutation_overview",       you_mutation_overview},

    { "num_runes",          you_num_runes },
    { "have_rune",          _you_have_rune },
    { "have_orb",           you_have_orb},

    { nullptr, nullptr },
};

void cluaopen_you(lua_State *ls)
{
    luaL_openlib(ls, "you", you_clib, 0);
}

//
// Player information (dlua). Grid coordinates etc.
//

LUARET1(you_can_hear_pos, boolean,
        player_can_hear(coord_def(luaL_safe_checkint(ls,1), luaL_safe_checkint(ls, 2))))
LUARET1(you_x_pos, number, you.pos().x)
LUARET1(you_y_pos, number, you.pos().y)
LUARET2(you_pos, number, you.pos().x, you.pos().y)

LUARET1(you_see_cell, boolean,
        you.see_cell(coord_def(luaL_safe_checkint(ls, 1), luaL_safe_checkint(ls, 2))))
LUARET1(you_see_cell_no_trans, boolean,
        you.see_cell_no_trans(coord_def(luaL_safe_checkint(ls, 1), luaL_safe_checkint(ls, 2))))

LUAWRAP(you_stop_running, stop_running())

LUAFN(you_moveto)
{
    const coord_def place(luaL_safe_checkint(ls, 1), luaL_safe_checkint(ls, 2));
    ASSERT(map_bounds(place));
    you.moveto(place);
    return 0;
}

LUAFN(you_teleport_to)
{
    const coord_def place(luaL_safe_checkint(ls, 1), luaL_safe_checkint(ls, 2));
    bool move_monsters = false;
    if (lua_gettop(ls) == 3)
        move_monsters = lua_toboolean(ls, 3);

    lua_pushboolean(ls, you_teleport_to(place, move_monsters));
    if (player_in_branch(BRANCH_ABYSS))
        maybe_shift_abyss_around_player();

    return 1;
}

LUAWRAP(you_random_teleport, you_teleport_now())

static int _you_uniques(lua_State *ls)
{
    bool unique_found = false;

    if (lua_gettop(ls) >= 1 && lua_isstring(ls, 1))
    {
        unique_found =
            you.unique_creatures[get_monster_by_name(lua_tostring(ls, 1))];
    }

    lua_pushboolean(ls, unique_found);
    return 1;
}

static int _you_unrands(lua_State *ls)
{
    bool unrand_found = false;

    if (lua_gettop(ls) >= 1 && lua_isstring(ls, 1))
    {
        int unrand_num = get_unrandart_num(lua_tostring(ls, 1));
        if (unrand_num != SPWPN_NORMAL)
            unrand_found = get_unique_item_status(unrand_num);
    }

    lua_pushboolean(ls, unrand_found);
    return 1;
}

LUAWRAP(_you_die,ouch(INSTANT_DEATH, KILLED_BY_SOMETHING))

static int _you_piety(lua_State *ls)
{
    if (lua_gettop(ls) >= 1)
    {
        const int new_piety = min(max(luaL_safe_checkint(ls, 1), 0), MAX_PIETY);
        set_piety(new_piety);
    }
    PLUARET(number, you.piety);
}

static int you_dock_piety(lua_State *ls)
{
    const int piety_loss = luaL_safe_checkint(ls, 1);
    const int penance = luaL_safe_checkint(ls, 2);
    dock_piety(piety_loss, penance);
    return 0;
}

static int you_lose_piety(lua_State *ls)
{
    const int piety_loss = luaL_safe_checkint(ls, 1);
    lose_piety(piety_loss);
    return 0;
}

LUAFN(you_in_branch)
{
    const char* name = luaL_checkstring(ls, 1);

    int br = NUM_BRANCHES;

    for (branch_iterator it; it; ++it)
    {
        if (strcasecmp(name, it->shortname) == 0
            || strcasecmp(name, it->longname) == 0
            || strcasecmp(name, it->abbrevname) == 0)
        {
            if (br != NUM_BRANCHES)
            {
                string err = make_stringf(
                    "'%s' matches both branch '%s' and '%s'",
                    name, branches[br].abbrevname,
                    it->abbrevname);
                return luaL_argerror(ls, 1, err.c_str());
            }
            br = it->id;
        }
    }

    if (br == NUM_BRANCHES)
    {
        string err = make_stringf("'%s' matches no branches.", name);
        return luaL_argerror(ls, 1, err.c_str());
    }

    bool in_branch = (br == you.where_are_you);
    PLUARET(boolean, in_branch);
}

LUAFN(_you_at_branch_bottom)
{
    PLUARET(boolean, at_branch_bottom());
}

LUAWRAP(you_gain_exp, gain_exp(luaL_safe_checkint(ls, 1)))

LUAFN(you_mutate)
{
    string mutname = luaL_checkstring(ls, 1);
    string reason = luaL_checkstring(ls, 2);
    bool temp = false;
    bool force = true;
    if (!lua_isnoneornil(ls, 3))
        temp = lua_toboolean(ls, 3);
    if (!lua_isnoneornil(ls, 4))
        force = lua_toboolean(ls, 4);
    const mutation_permanence_class mutclass = temp ? MUTCLASS_TEMPORARY : MUTCLASS_NORMAL;
    mutation_type mut = mutation_from_name(mutname, true); // requires exact match
    if (mut != NUM_MUTATIONS)
        PLUARET(boolean, mutate(mut, reason, true, force, false, false, mutclass));

    string err = make_stringf("No such mutation: '%s'.", mutname.c_str());
    return luaL_argerror(ls, 1, err.c_str());
}

LUAFN(you_delete_mutation)
{
    string mutname = luaL_checkstring(ls, 1);
    string reason = luaL_checkstring(ls, 2);
    mutation_type mut = mutation_from_name(mutname, true); // requires exact match
    if (mut != NUM_MUTATIONS)
        PLUARET(boolean, delete_mutation(mut, reason, false));

    string err = make_stringf("No such mutation: '%s'.", mutname.c_str());
    return luaL_argerror(ls, 1, err.c_str());
}


// clear one or all temporary mutations.
LUAFN(you_delete_temp_mutations)
{
    bool result;
    bool clear_all = lua_toboolean(ls, 1);
    string reason = luaL_checkstring(ls, 2);
    if (clear_all)
        result = delete_all_temp_mutations(reason);
    else
        result = temp_mutation_wanes();
    PLUARET(boolean, result);
}

// clear one or all temporary mutations.
LUAFN(you_delete_all_mutations)
{
    bool result;
    string reason = luaL_checkstring(ls, 1);
    result = delete_all_mutations(reason);
    PLUARET(boolean, result);
}

LUAFN(you_change_species)
{
    string species = luaL_checkstring(ls, 1);
    const species_type sp = find_species_from_string(species);

    if (sp == SP_UNKNOWN)
    {
        mpr("That species isn't available.");
        PLUARET(boolean, false);
    }

    change_species_to(sp);
    PLUARET(boolean, true);
}

#ifdef WIZARD
LUAFN(you_set_xl)
{
    const int newxl = luaL_safe_checkint(ls, 1);
    bool train = lua_toboolean(ls, 2); // whether to train skills
    if (newxl < 1 || newxl > you.get_max_xl())
    {
        mprf("Can't change to invalid xl %d", newxl);
        PLUARET(boolean, false);
    }
    if (newxl == you.experience_level)
        PLUARET(boolean, true);
    set_xl(newxl, train, newxl < you.experience_level); // most useful for testing if it's not silent on levelup
    PLUARET(boolean, true);
}
#endif

/*
 * Init the player class.
 *
 * @param combo a string with the species and background abbreviations to use.
 * @param weapon optional string with the weapon to give.
 * @return a string with the weapon skill name.
 */
LUAFN(you_init)
{
    const string combo = luaL_checkstring(ls, 1);
    newgame_def ng;
    ng.type = GAME_TYPE_NORMAL;
    ng.species = get_species_by_abbrev(combo.substr(0, 2).c_str());
    ng.job = get_job_by_abbrev(combo.substr(2, 2).c_str());
    ng.weapon = str_to_weapon(luaL_checkstring(ls, 2));
    setup_game(ng);
    you.save->unlink();
    you.save = nullptr;
    PLUARET(string, skill_name(item_attack_skill(OBJ_WEAPONS, ng.weapon)));
}

LUAWRAP(you_enter_wizard_mode, you.wizard = true)

LUARET1(you_exp_needed, number, exp_needed(luaL_safe_checkint(ls, 1)))
LUAWRAP(you_exercise, exercise(str_to_skill(luaL_checkstring(ls, 1)), 1))
LUARET1(you_skill_cost_level, number, you.skill_cost_level)
LUARET1(you_skill_points, number,
        you.skill_points[str_to_skill(luaL_checkstring(ls, 1))])
LUARET1(you_zigs_completed, number, you.zigs_completed)

static const struct luaL_reg you_dlib[] =
{
{ "hear_pos",           you_can_hear_pos },
{ "silenced",           you_silenced },
{ "x_pos",              you_x_pos },
{ "y_pos",              you_y_pos },
{ "pos",                you_pos },
{ "moveto",             you_moveto },
{ "see_cell",           you_see_cell },
{ "see_cell_no_trans",  you_see_cell_no_trans },
{ "random_teleport",    you_random_teleport },
{ "teleport_to",        you_teleport_to },
{ "uniques",            _you_uniques },
{ "unrands",            _you_unrands },
{ "die",                _you_die },
{ "piety",              _you_piety },
{ "dock_piety",         you_dock_piety },
{ "lose_piety",         you_lose_piety },
{ "in_branch",          you_in_branch },
{ "stop_running",       you_stop_running },
{ "at_branch_bottom",   _you_at_branch_bottom },
{ "gain_exp",           you_gain_exp },
{ "init",               you_init },
{ "exp_needed",         you_exp_needed },
{ "exercise",           you_exercise },
{ "skill_cost_level",   you_skill_cost_level },
{ "skill_points",       you_skill_points },
{ "zigs_completed",     you_zigs_completed },
{ "mutate",             you_mutate },
{ "delete_mutation",    you_delete_mutation },
{ "delete_temp_mutations", you_delete_temp_mutations },
{ "delete_all_mutations", you_delete_all_mutations },
{ "change_species",     you_change_species },
#ifdef WIZARD
{ "enter_wizard_mode",  you_enter_wizard_mode },
{ "set_xl",             you_set_xl },
#endif

{ nullptr, nullptr }
};

void dluaopen_you(lua_State *ls)
{
    luaL_openlib(ls, "you", you_dlib, 0);
}
