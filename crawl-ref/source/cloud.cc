/**
 * @file
 * @brief Functions related to clouds.
 *
 * Creating a cloud module so all the cloud stuff can be isolated.
**/

#include "AppHdr.h"

#include "cloud.h"

#include <algorithm>

#include "areas.h"
#include "colour.h"
#include "coordit.h"
#include "dungeon.h"
#include "godconduct.h"
#include "godpassive.h"
#include "libutil.h" // testbits
#include "los.h"
#include "mapmark.h"
#include "melee_attack.h"
#include "message.h"
#include "mon-behv.h"
#include "mon-death.h"
#include "mon-place.h"
#include "religion.h"
#include "shout.h"
#include "spl-util.h"
#include "state.h"
#include "stringutil.h"
#include "terrain.h"
#include "tiledef-main.h"
#include "unwind.h"

cloud_struct* cloud_at(coord_def pos)
{
    return map_find(env.cloud, pos);
}

/// A portrait of a cloud_type.
struct cloud_data
{
    /// A (relatively) short name for the cloud. May be referenced from lua.
    const char* terse_name;
    /// Another name for the cloud. If nullptr, defaults to terse name.
    const char* verbose_name;
    /// The colour of the cloud in console.
    colour_t colour;
    /// The associated "beam" (effect) for this cloud type.
    beam_type beam_effect;
    /// How much damage a cloud is expected to do in one turn, minimum.
    int expected_base_damage;
    /// The amount of additional random damage a cloud is expected to maybe do.
    int expected_random_damage;
};

/// A map from cloud_type to cloud_data.
static const cloud_data clouds[] = {
    // CLOUD_NONE,
    { "?", "?",                                 // terse, verbose name
    },
    // CLOUD_FIRE,
    { "flame", "roaring flames",                // terse, verbose name
       COLOUR_UNDEF,                            // colour
       BEAM_FIRE,                               // beam_effect
       15, 46,                                  // base, expected random damage
    },
    // CLOUD_MEPHITIC,
    { "noxious fumes", nullptr,                 // terse, verbose name
      GREEN,                                    // colour
      BEAM_MEPHITIC,                            // beam_effect
      0, 19,                                    // base, expected random damage
    },
    // CLOUD_COLD,
    { "freezing vapour", "freezing vapours",    // terse, verbose name
      COLOUR_UNDEF,                             // colour
      BEAM_COLD,                                // beam_effect
      15, 46,                                   // base, expected random damage
    },
    // CLOUD_POISON,
    { "poison gas", nullptr,                    // terse, verbose name
      LIGHTGREEN,                               // colour
      BEAM_POISON,                              // beam_effect
      0, 37,                                    // base, expected random damage
    },
    // CLOUD_BLACK_SMOKE,
    { "black smoke",  nullptr,                  // terse, verbose name
      DARKGREY,                                 // colour
    },
    // CLOUD_GREY_SMOKE,
    { "grey smoke",  nullptr,                   // terse, verbose name
      LIGHTGREY,                                // colour
    },
    // CLOUD_BLUE_SMOKE,
    { "blue smoke",  nullptr,                   // terse, verbose name
      LIGHTBLUE,                                // colour
    },
    // CLOUD_PURPLE_SMOKE,
    { "purple smoke",  nullptr,                 // terse, verbose name
      MAGENTA,                                  // colour
    },
    // CLOUD_TLOC_ENERGY,
    { "translocational energy",  nullptr,       // terse, verbose name
      MAGENTA,                                  // colour
    },
    // CLOUD_FOREST_FIRE,
    { "spreading flames", "a forest fire",      // terse, verbose name
      COLOUR_UNDEF,                             // colour
      BEAM_FIRE,                                // beam_effect
      15, 46                                    // base, expected random damage
    },
    // CLOUD_STEAM,
    { "steam", "a cloud of scalding steam",     // terse, verbose name
      LIGHTGREY,                                // colour
      BEAM_STEAM,                               // beam_effect
      0, 25,
    },
#if TAG_MAJOR_VERSION == 34
    // CLOUD_GLOOM,
    { "gloom", "thick gloom",                   // terse, verbose name
      MAGENTA,                                  // colour
    },
#endif
    // CLOUD_INK,
    { "ink",  nullptr,                          // terse, verbose name
      DARKGREY,                                 // colour
      BEAM_INK,                                 // beam_effect
    },
    // CLOUD_PETRIFY,
    { "calcifying dust",  nullptr,              // terse, verbose name
      WHITE,                                    // colour
      BEAM_PETRIFYING_CLOUD,                    // beam_effect
    },
    // CLOUD_HOLY_FLAMES,
    { "blessed fire", nullptr,                  // terse, verbose name
      ETC_HOLY,                                 // colour
      BEAM_HOLY_FLAME,                          // beam_effect
      15, 46,                                   // base, expected random damage
    },
    // CLOUD_MIASMA,
    { "foul pestilence", "dark miasma",         // terse, verbose name
      DARKGREY,                                 // colour
      BEAM_MIASMA,                              // beam_effect
    },
    // CLOUD_MIST,
    { "thin mist", nullptr,                     // terse, verbose name
      ETC_MIST,                                 // colour
    },
    // CLOUD_CHAOS,
    { "seething chaos", nullptr,                // terse, verbose name
      ETC_RANDOM,                               // colour
      BEAM_CHAOS,                               // beam_effect
    },
    // CLOUD_RAIN,
    { "rain", "the rain",                       // terse, verbose name
      ETC_MIST,                                 // colour
    },
    // CLOUD_MUTAGENIC,
    { "mutagenic fog",  nullptr,                // terse, verbose name
      ETC_MUTAGENIC,                            // colour
    },
    // CLOUD_MAGIC_TRAIL,
    { "magical condensation", nullptr,          // terse, verbose name
      ETC_MAGIC,                                // colour
    },
    // CLOUD_TORNADO,
    { "raging winds", nullptr,                  // terse, verbose name
      ETC_TORNADO,                              // colour
    },
    // CLOUD_DUST_TRAIL,
    { "sparse dust",  nullptr,                  // terse, verbose name
      ETC_EARTH,                                // colour
    },
    // CLOUD_SPECTRAL,
    { "spectral mist", nullptr,                 // terse, verbose name
      ETC_ELECTRICITY,                          // colour
      BEAM_NONE,                                // beam_effect
      0, 25,                                    // base, expected random damage
    },
    // CLOUD_ACID,
    { "acidic fog", nullptr,                    // terse, verbose name
      YELLOW,                                   // colour
      BEAM_ACID,                                // beam_effect
      15, 46,                                   // base, random expected damage
    },
    // CLOUD_STORM,
    { "thunder", "a thunderstorm",              // terse, verbose name
      ETC_DARK,                                 // colour
      BEAM_ELECTRICITY,                         // beam_effect
      60, 46,                                   // base, random expected damage
    },
    // CLOUD_NEGATIVE_ENERGY,
    { "negative energy", nullptr,               // terse, verbose name
      ETC_INCARNADINE,                          // colour
      BEAM_NEG,                                 // beam_effect
      15, 46,                                   // base, random expected damage
    },
};
COMPILE_CHECK(ARRAYSZ(clouds) == NUM_CLOUD_TYPES);

static int _actor_cloud_damage(const actor *act, const cloud_struct &cloud,
                               bool maximum_damage);

static int _actual_spread_rate(cloud_type type, int spread_rate)
{
    if (spread_rate >= 0)
        return spread_rate;

    switch (type)
    {
#if TAG_MAJOR_VERSION == 34
    case CLOUD_GLOOM:
        return 50;
#endif
    case CLOUD_STEAM:
    case CLOUD_GREY_SMOKE:
    case CLOUD_BLACK_SMOKE:
        return 22;
    case CLOUD_RAIN:
    case CLOUD_INK:
        return 11;
    default:
        return 0;
    }
}

static beam_type _cloud2beam(cloud_type flavour)
{
    if (flavour == CLOUD_RANDOM)
        return BEAM_RANDOM;
    return clouds[flavour].beam_effect;
}

#ifdef ASSERTS
static bool _killer_whose_match(kill_category whose, killer_type killer)
{
    switch (whose)
    {
        case KC_YOU:
            return killer == KILL_YOU_MISSILE || killer == KILL_YOU_CONF;

        case KC_FRIENDLY:
            return killer == KILL_MON_MISSILE || killer == KILL_YOU_CONF
                   || killer == KILL_MON;

        case KC_OTHER:
            return killer == KILL_MON_MISSILE || killer == KILL_MISCAST
                   || killer == KILL_MISC || killer == KILL_MON;

        case KC_NCATEGORIES:
            die("kill category not matching killer type");
    }
    return false;
}
#endif

static void _los_cloud_changed(const coord_def& p, cloud_type t)
{
    if (is_opaque_cloud(t))
        los_terrain_changed(p);
}

cloud_struct::cloud_struct(coord_def p, cloud_type c, int d, int spread,
                           kill_category kc, killer_type kt, mid_t src, int clr,
                           string name_, string tile_, int excl)
    : pos(p), type(c), decay(d), spread_rate(spread), whose(kc), killer(kt),
      source(src), colour(clr), name(name_), tile(tile_), excl_rad(excl)
{
    ASSERT(_killer_whose_match(whose, killer));

    if (type == CLOUD_RANDOM_SMOKE)
        type = random_smoke_type();
    if (!tile.empty())
    {
        tileidx_t index;
        if (!tile_main_index(tile.c_str(), &index))
        {
            mprf(MSGCH_ERROR, "Invalid tile requested for cloud: '%s'.", tile.c_str());
            tile = "";
        }
    }
    _los_cloud_changed(pos, type);
}

static int _spread_cloud(const cloud_struct &cloud)
{
    const int spreadch = cloud.decay > 30 ? 80 :
                         cloud.decay > 20 ? 50 :
                                            30;
    int extra_decay = 0;
    for (adjacent_iterator ai(cloud.pos); ai; ++ai)
    {
        if (random2(100) >= spreadch)
            continue;

        if (!in_bounds(*ai)
            || cloud_at(*ai)
            || cell_is_solid(*ai)
            || is_sanctuary(*ai) && !is_harmless_cloud(cloud.type))
        {
            continue;
        }

        if (cloud.type == CLOUD_INK && !feat_is_watery(grd(*ai)))
            continue;

        int newdecay = cloud.decay / 2 + 1;
        if (newdecay >= cloud.decay)
            newdecay = cloud.decay - 1;

        env.cloud[*ai] = cloud;
        env.cloud[*ai].pos = *ai;
        env.cloud[*ai].decay = newdecay;

        extra_decay += 8;
    }

    return extra_decay;
}

static void _spread_fire(const cloud_struct &cloud)
{
    int make_flames = one_chance_in(5);

    for (adjacent_iterator ai(cloud.pos); ai; ++ai)
    {
        if (!in_bounds(*ai)
            || cloud_at(*ai)
            || is_sanctuary(*ai))
        {
            continue;
        }

        // burning trees produce flames all around
        if (!cell_is_solid(*ai) && make_flames)
        {
            env.cloud[*ai] = cloud;
            env.cloud[*ai].type = CLOUD_FIRE;
            env.cloud[*ai].pos = *ai;
            env.cloud[*ai].decay = cloud.decay / 2 + 1;
        }

        // forest fire doesn't spread in all directions at once,
        // every neighbouring square gets a separate roll
        if (!feat_is_tree(grd(*ai)) || x_chance_in_y(19, 20))
            continue;

        if (env.markers.property_at(*ai, MAT_ANY, "veto_fire") == "veto")
            continue;

        if (you.see_cell(*ai))
            mpr("The forest fire spreads!");
        destroy_wall(*ai);
        env.cloud[*ai] = cloud;
        env.cloud[*ai].pos = *ai;
        env.cloud[*ai].decay = random2(30) + 25;
        if (cloud.whose == KC_YOU)
        {
            did_god_conduct(DID_KILL_PLANT, 1);
            did_god_conduct(DID_FIRE, 6);
        }
        else if (cloud.whose == KC_FRIENDLY && !crawl_state.game_is_arena())
            did_god_conduct(DID_KILL_PLANT, 1);

    }
}

static void _cloud_interacts_with_terrain(const cloud_struct &cloud)
{
    if (cloud.type != CLOUD_FIRE && cloud.type != CLOUD_FOREST_FIRE)
        return;

    for (adjacent_iterator ai(cloud.pos); ai; ++ai)
    {
        const coord_def p(*ai);
        if (in_bounds(p)
            && feat_is_watery(grd(p))
            && !cell_is_solid(p)
            && !cloud_at(p)
            && one_chance_in(7))
        {
            env.cloud[p] = cloud_struct(p, CLOUD_STEAM, cloud.decay / 2 + 1,
                                        22, cloud.whose, cloud.killer,
                                        cloud.source, -1, "", "", -1);
        }
    }
}

/**
 * How fast should a given cloud fade away this turn?
 *
 * @param cloud_idx     The cloud in question.
 * @return              The rate at which the cloud's "decay" should decrease
 *                      this turn.
 */
static int _cloud_dissipation_rate(const cloud_struct &cloud)
{
    int dissipate = you.time_taken;

    // If a player-created cloud is out of LOS, it dissipates much faster.
    if (cloud.source == MID_PLAYER && !you.see_cell_no_trans(cloud.pos))
        dissipate *= 4;

    switch (cloud.type)
    {
        // Fire clouds dissipate faster over water.
        case CLOUD_FIRE:
        case CLOUD_FOREST_FIRE:
            if (feat_is_watery(grd(cloud.pos)))
                return dissipate * 4;
            break;
        // rain and cold clouds dissipate faster over lava.
        case CLOUD_COLD:
        case CLOUD_RAIN:
        case CLOUD_STORM:
            if (grd(cloud.pos) == DNGN_LAVA)
                return dissipate * 4;
            break;
        // Ink cloud shouldn't appear outside of water.
        case CLOUD_INK:
            if (!feat_is_watery(grd(cloud.pos)))
                return dissipate * 40;
            break;
        default:
            break;
    }

    return dissipate;
}

static void _dissipate_cloud(cloud_struct& cloud)
{
    // Apply calculated rate to the actual cloud.
    cloud.decay -= _cloud_dissipation_rate(cloud);

    if (cloud.type == CLOUD_FOREST_FIRE)
        _spread_fire(cloud);
    else if (x_chance_in_y(cloud.spread_rate, 100))
    {
        cloud.spread_rate -= div_rand_round(cloud.spread_rate, 10);
        cloud.decay       -= _spread_cloud(cloud);
    }

    // Check for total dissipation and handle accordingly.
    if (cloud.decay < 1)
        delete_cloud(cloud.pos);
}

static void _handle_spectral_cloud(const cloud_struct& cloud)
{
    if (actor_at(cloud.pos) || !actor_by_mid(cloud.source))
        return;

    int countn = 0;
    for (distance_iterator di(cloud.pos, false, false, 2); di; ++di)
    {
        if (monster_at(*di) && monster_at(*di)->type == MONS_SPECTRAL_THING)
            countn++;
    }

    int rate[5] = {650, 175, 45, 20, 0};
    int chance = rate[(min(4, countn))];

    if (!x_chance_in_y(chance, you.time_taken * 600))
        return;

    monster_type basetype =
        random_choose_weighted(4,   MONS_ANACONDA,
                               6,   MONS_HYDRA,
                               3,   MONS_SNAPPING_TURTLE,
                               2,   MONS_ALLIGATOR_SNAPPING_TURTLE,
                               100, RANDOM_MONSTER,
                               0);

    monster* agent = monster_by_mid(cloud.source);
    create_monster(mgen_data(MONS_SPECTRAL_THING,
                             (cloud.whose == KC_OTHER ?
                                BEH_HOSTILE :
                                BEH_FRIENDLY),
                             actor_by_mid(cloud.source), 1,
                             SPELL_SPECTRAL_CLOUD, cloud.pos,
                             (agent ? agent->foe : MHITYOU), MG_FORCE_PLACE,
                             GOD_NO_GOD, basetype));
}

void manage_clouds()
{
    // We can't iterate over env.cloud directly because _dissipate_cloud
    // will remove this cloud and invalidate our iterator.
    vector<cloud_struct *> cloud_ptrs;
    for (auto& entry : env.cloud)
        cloud_ptrs.push_back(&entry.second);

    for (auto ptr : cloud_ptrs)
    {
        cloud_struct& cloud = *ptr;

#ifdef ASSERTS
        if (cell_is_solid(cloud.pos))
        {
            die("cloud %s in %s at (%d,%d)", cloud_type_name(cloud.type).c_str(),
                dungeon_feature_name(grd(cloud.pos)), cloud.pos.x, cloud.pos.y);
        }
#endif

        // This was initially 40, but that was far too spammy.
        if (cloud.type == CLOUD_STORM
            && x_chance_in_y(you.time_taken, 400) && !actor_at(cloud.pos))
        {
            const bool you_see = you.see_cell(cloud.pos);
            if (you_see && !you_worship(GOD_QAZLAL))
                mpr("Lightning arcs down from a storm cloud!");
            noisy(spell_effect_noise(SPELL_LIGHTNING_BOLT), cloud.pos,
                  you_see || you_worship(GOD_QAZLAL) ? nullptr
                  : "You hear a mighty clap of thunder!");
        }
        else if (cloud.type == CLOUD_SPECTRAL)
            _handle_spectral_cloud(cloud);

        _cloud_interacts_with_terrain(cloud);

        _dissipate_cloud(cloud);
    }
}

static void _maybe_leave_water(const coord_def pos)
{
    ASSERT_IN_BOUNDS(pos);

    // Rain clouds can occasionally leave shallow water or deepen it:
    // If we're near lava, chance of leaving water is lower;
    // if we're near deep water already, chance of leaving water
    // is slightly higher.
    if (!one_chance_in((5 + count_neighbours(pos, DNGN_LAVA)) -
                            count_neighbours(pos, DNGN_DEEP_WATER)))
    {
        return;
    }

    dungeon_feature_type feat = grd(pos);

    if (grd(pos) == DNGN_FLOOR)
        feat = DNGN_SHALLOW_WATER;
    else if (grd(pos) == DNGN_SHALLOW_WATER && you.pos() != pos
             && one_chance_in(3) && !crawl_state.game_is_sprint())
    {
        // Don't drown the player!
        feat = DNGN_DEEP_WATER;
    }

    if (grd(pos) != feat)
    {
        if (you.pos() == pos && you.ground_level())
            mpr("The rain has left you waist-deep in water!");
        temp_change_terrain(pos, feat, random_range(500, 1000),
                            TERRAIN_CHANGE_FLOOD);
    }
}

void delete_cloud(coord_def p)
{
    if (!cloud_at(p))
        return;
    const cloud_type type = cloud_at(p)->type;
    env.cloud.erase(p);
    if (type == CLOUD_RAIN)
        _maybe_leave_water(p);
    _los_cloud_changed(p, type);
}

void delete_all_clouds()
{
    // We can't iterate over env.cloud directly because delete_cloud
    // will remove this cloud and invalidate our iterator.
    vector<coord_def> cloud_locs;
    for (auto& entry : env.cloud)
        cloud_locs.push_back(entry.first);

    for (auto pos : cloud_locs)
        delete_cloud(pos);
}

// The current use of this function is for shifting in the abyss, so
// that clouds get moved along with the rest of the map.
void move_cloud(coord_def src, coord_def newpos)
{
    if (!cloud_at(src))
        return;
    ASSERT(!cell_is_solid(newpos));

    env.cloud[newpos] = env.cloud[src];
    env.cloud.erase(src);
    env.cloud[newpos].pos = newpos;
    _los_cloud_changed(src, env.cloud[newpos].type);
    _los_cloud_changed(newpos, env.cloud[newpos].type);
}

void swap_clouds(coord_def p1, coord_def p2)
{
    if (p1 == p2)
        return;
    if (!cloud_at(p1))
    {
        move_cloud(p2, p1);
        return;
    }
    else if (!cloud_at(p2))
    {
        move_cloud(p1, p2);
        return;
    }

    cloud_struct temp = env.cloud[p1];
    env.cloud[p1] = env.cloud[p2];
    env.cloud[p2] = temp;
    env.cloud[p1].pos = p1;
    env.cloud[p2].pos = p2;
    if (is_opaque_cloud(cloud_type_at(p1))
        || is_opaque_cloud(cloud_type_at(p2)))
    {
        los_terrain_changed(p1);
        los_terrain_changed(p2);
    }
}

// Places a cloud with the given stats assuming one doesn't already
// exist at that point.
void check_place_cloud(cloud_type cl_type, const coord_def& p, int lifetime,
                       const actor *agent, int spread_rate, int colour,
                       string name, string tile, int excl_rad)
{
    if (!in_bounds(p) || cloud_at(p))
        return;

    place_cloud(cl_type, p, lifetime, agent, spread_rate, colour, name, tile,
                excl_rad);
}

static bool _cloud_is_stronger(cloud_type ct, const cloud_struct& cloud)
{
    return is_harmless_cloud(cloud.type)
           || cloud.type == CLOUD_STEAM
           || ct == CLOUD_TORNADO
           || ct == CLOUD_POISON && cloud.type == CLOUD_MEPHITIC
           || cloud.decay <= 20; // soon gone
}

//   Places a cloud with the given stats. Will overwrite an old
//   cloud under some circumstances.
void place_cloud(cloud_type cl_type, const coord_def& ctarget, int cl_range,
                 const actor *agent, int _spread_rate, int colour,
                 string name, string tile, int excl_rad)
{
    if (is_sanctuary(ctarget) && !is_harmless_cloud(cl_type))
        return;

    if (cl_type == CLOUD_INK && !feat_is_watery(grd(ctarget)))
        return;

    ASSERT(!cell_is_solid(ctarget));

    kill_category whose = KC_OTHER;
    killer_type killer  = KILL_MISC;
    mid_t source        = MID_NOBODY;
    if (agent && agent->is_player())
        whose = KC_YOU, killer = KILL_YOU_MISSILE, source = MID_PLAYER;
    else if (agent && agent->is_monster())
    {
        if (agent->as_monster()->friendly())
            whose = KC_FRIENDLY;
        else
            whose = KC_OTHER;
        killer = KILL_MON_MISSILE;
        source = agent->mid;
    }


    // There's already a cloud here. See if we can overwrite it.
    if (cloud_at(ctarget) && !_cloud_is_stronger(cl_type, *cloud_at(ctarget)))
        return;

    const int spread_rate = _actual_spread_rate(cl_type, _spread_rate);

    env.cloud[ctarget] = cloud_struct(ctarget, cl_type, cl_range * 10,
                                      spread_rate, whose, killer, source,
                                      colour, name, tile, excl_rad);
}

bool is_opaque_cloud(cloud_type ctype)
{
    return ctype >= CLOUD_OPAQUE_FIRST && ctype <= CLOUD_OPAQUE_LAST;
}

cloud_type cloud_type_at(const coord_def &c)
{
    return cloud_at(c) ? cloud_at(c)->type : CLOUD_NONE;
}

bool cloud_is_yours_at(const coord_def &c)
{
    return cloud_at(c) ? YOU_KILL(cloud_at(c)->killer) : false;
}

cloud_type random_smoke_type()
{
    return random_choose(CLOUD_GREY_SMOKE, CLOUD_BLUE_SMOKE,
                         CLOUD_BLACK_SMOKE, CLOUD_PURPLE_SMOKE);
}
int max_cloud_damage(cloud_type cl_type, int power)
{
    cloud_struct cloud;
    cloud.type = cl_type;
    cloud.decay = power * 10;
    return _actor_cloud_damage(&you, cloud, true);
}

// Returns true if the cloud type has negative side effects beyond
// plain damage and inventory destruction effects.
static bool _cloud_has_negative_side_effects(cloud_type cloud)
{
    switch (cloud)
    {
    case CLOUD_MEPHITIC:
    case CLOUD_MIASMA:
    case CLOUD_MUTAGENIC:
    case CLOUD_CHAOS:
    case CLOUD_PETRIFY:
    case CLOUD_ACID:
    case CLOUD_NEGATIVE_ENERGY:
        return true;
    default:
        return false;
    }
}

static int _cloud_damage_calc(int size, int n_average, int extra,
                              bool maximum_damage)
{
    return maximum_damage?
           extra + size - 1
           : random2avg(size, n_average) + extra;
}

// Calculates the base damage that the cloud does to an actor without
// considering resistances and time spent in the cloud.
static int _cloud_base_damage(const actor *act,
                              const cloud_struct &cloud,
                              bool maximum_damage)
{
    switch (cloud.type)
    {
    case CLOUD_RAIN:
        // Only applies to fiery actors: see _actor_cloud_resist.
        return _cloud_damage_calc(9, 1, 0, maximum_damage);
    case CLOUD_FIRE:
    case CLOUD_FOREST_FIRE:
    case CLOUD_COLD:
    case CLOUD_HOLY_FLAMES:
    case CLOUD_ACID:
    case CLOUD_NEGATIVE_ENERGY:
        // Yes, we really hate players, damn their guts.
        //
        // XXX: Some superior way of linking cloud damage to cloud
        // power would be nice, so we can dispense with these hacky
        // special cases.
        if (act->is_player())
            return _cloud_damage_calc(23, 3, 10, maximum_damage);
        else
            return _cloud_damage_calc(16, 3, 6, maximum_damage);

    case CLOUD_STORM:
        // Four times the damage, because it's a quarter as often.
        if (act->is_player())
            return _cloud_damage_calc(92, 3, 40, maximum_damage);
        else
            return _cloud_damage_calc(64, 3, 24, maximum_damage);

    case CLOUD_MEPHITIC:
        return _cloud_damage_calc(3, 1, 0, maximum_damage);
    case CLOUD_POISON:
        return _cloud_damage_calc(10, 1, 0, maximum_damage);
    case CLOUD_MIASMA:
        return _cloud_damage_calc(12, 3, 0, maximum_damage);
    case CLOUD_STEAM:
        return _cloud_damage_calc(16, 2, 0, maximum_damage);
    case CLOUD_SPECTRAL:
        return _cloud_damage_calc(15, 3, 4, maximum_damage);
    default:
        return 0;
    }
}

// Returns true if the actor is immune to cloud damage, inventory item
// destruction, and all other cloud-type-specific side effects (i.e.
// apart from cloud interaction with invisibility).
//
// Note that actor_cloud_immune may be false even if the actor will
// not be harmed by the cloud. The cloud may have positive
// side-effects on the actor.
bool actor_cloud_immune(const actor *act, const cloud_struct &cloud)
{
    if (is_harmless_cloud(cloud.type))
        return true;

    const bool player = act->is_player();

    if (!player
        && (you_worship(GOD_FEDHAS)
            && fedhas_protects(act->as_monster())
            || testbits(act->as_monster()->flags, MF_DEMONIC_GUARDIAN))
        && (cloud.whose == KC_YOU || cloud.whose == KC_FRIENDLY)
        && (act->as_monster()->friendly() || act->as_monster()->neutral()))
    {
        return true;
    }

    // Qazlalites get immunity to their own clouds.
    if (player && YOU_KILL(cloud.killer) && have_passive(passive_t::resist_own_clouds))
        return true;

    switch (cloud.type)
    {
    case CLOUD_FIRE:
    case CLOUD_FOREST_FIRE:
        if (!player)
            return act->res_fire() >= 3;
        return you.duration[DUR_FIRE_SHIELD]
               || you.mutation[MUT_FLAME_CLOUD_IMMUNITY];
    case CLOUD_HOLY_FLAMES:
        return act->res_holy_energy(cloud.agent()) > 0;
    case CLOUD_COLD:
        if (!player)
            return act->res_cold() >= 3;
        return you.mutation[MUT_FREEZING_CLOUD_IMMUNITY];
    case CLOUD_MEPHITIC:
        return act->res_poison() > 0 || act->is_unbreathing();
    case CLOUD_POISON:
        return act->res_poison() > 0;
    case CLOUD_STEAM:
        return act->res_steam() > 0;
    case CLOUD_MIASMA:
        return act->res_rotting() > 0;
    case CLOUD_PETRIFY:
        return act->res_petrify();
    case CLOUD_SPECTRAL:
        return bool(act->holiness() & MH_UNDEAD);
    case CLOUD_ACID:
        return act->res_acid() > 0;
    case CLOUD_STORM:
        return act->res_elec() >= 3;
    case CLOUD_NEGATIVE_ENERGY:
        return act->res_negative_energy() >= 3;
    case CLOUD_TORNADO:
        return act->res_wind();
    default:
        return false;
    }
}

// Returns a numeric resistance value for the actor's resistance to
// the cloud's effects. If the actor is immune to the cloud's damage,
// returns MAG_IMMUNE.
static int _actor_cloud_resist(const actor *act, const cloud_struct &cloud)
{
    if (actor_cloud_immune(act, cloud))
        return MAG_IMMUNE;
    switch (cloud.type)
    {
    case CLOUD_RAIN:
        return act->is_fiery()? 0 : MAG_IMMUNE;
    case CLOUD_FIRE:
    case CLOUD_FOREST_FIRE:
        return act->res_fire();
    case CLOUD_HOLY_FLAMES:
        return act->res_holy_energy(cloud.agent());
    case CLOUD_COLD:
        return act->res_cold();
    case CLOUD_PETRIFY:
        return act->res_petrify();
    case CLOUD_ACID:
        return act->res_acid();
    case CLOUD_STORM:
        return act->res_elec();
    case CLOUD_NEGATIVE_ENERGY:
        return act->res_negative_energy();

    default:
        return 0;
    }
}

static bool _mephitic_cloud_roll(const monster* mons)
{
    const int meph_hd_cap = 21;
    return mons->get_hit_dice() >= meph_hd_cap? one_chance_in(50)
           : !x_chance_in_y(mons->get_hit_dice(), meph_hd_cap);
}

// Applies cloud messages and side-effects and returns true if the
// cloud had a side-effect. This function does not check for cloud immunity.
// ... but it's only called if the actor isn't immune
static bool _actor_apply_cloud_side_effects(actor *act,
                                            const cloud_struct &cloud,
                                            int final_damage)
{
    ASSERT(act); // XXX: change to actor &act
    const bool player = act->is_player();
    monster *mons = !player? act->as_monster() : nullptr;
    switch (cloud.type)
    {
    case CLOUD_FIRE:
    case CLOUD_STEAM:
        if (player)
            maybe_melt_player_enchantments(BEAM_FIRE, final_damage);
    case CLOUD_RAIN:
    case CLOUD_STORM:
        if (act->is_fiery() && final_damage > 0)
        {
            if (you.can_see(*act))
            {
                mprf("%s %s in the rain.",
                     act->name(DESC_THE).c_str(),
                     act->conj_verb(silenced(act->pos())?
                                    "steam" : "sizzle").c_str());
            }
        }
        if (player)
        {
            if (you.duration[DUR_FIRE_SHIELD] > 1)
            {
                you.duration[DUR_FIRE_SHIELD] = 1;
                return true;
            }
        }
        break;

    case CLOUD_MEPHITIC:
    {
        if (player)
        {
            if (1 + random2(27) >= effective_xl())
            {
                mpr("You choke on the stench!");
                // effectively one or two turns, since it will be
                // decremented right away
                confuse_player(coinflip() ? 3 : 2);
                return true;
            }
        }
        else
        {
            bolt beam;
            beam.flavour = BEAM_CONFUSION;
            beam.thrower = cloud.killer;

            if (cloud.whose == KC_FRIENDLY)
                beam.source_id = MID_ANON_FRIEND;

            if (_mephitic_cloud_roll(mons))
            {
                beam.apply_enchantment_to_monster(mons);
                return true;
            }
        }
        break;
    }

    case CLOUD_PETRIFY:
    {
        if (player)
        {
            if (random2(55) - 13 >= effective_xl())
            {
                you.petrify(act);
                return true;
            }
        }
        else
        {
            bolt beam;
            beam.flavour = BEAM_PETRIFY;
            beam.thrower = cloud.killer;

            if (cloud.whose == KC_FRIENDLY)
                beam.source_id = MID_ANON_FRIEND;

            beam.apply_enchantment_to_monster(mons);
            return true;
        }
        break;
    }

    case CLOUD_POISON:
        if (player)
        {
            const actor* agent = cloud.agent();
            poison_player(5 + roll_dice(3, 8), agent ? agent->name(DESC_A) : "",
                          cloud.cloud_name());
        }
        else
            poison_monster(mons, cloud.agent());
        return true;

    case CLOUD_MIASMA:
        if (player)
            miasma_player(cloud.agent(), cloud.cloud_name());
        else
            miasma_monster(mons, cloud.agent());
        break;

    case CLOUD_MUTAGENIC:
        if (player)
        {
            mpr("The mutagenic energy flows into you.");
            // It's possible that you got trampled into the mutagenic cloud
            // and it's not your fault... so we'll say it's not intentional.
            // (it's quite bad in any case, so players won't scum, probably.)
            contaminate_player(1300 + random2(1250), false);
            // min 2 turns to yellow, max 4
            return true;
        }
        else if (coinflip() && mons->malmutate("mutagenic cloud"))
        {
            if (you_worship(GOD_ZIN) && cloud.whose == KC_YOU)
                did_god_conduct(DID_DELIBERATE_MUTATING, 5 + random2(3));
            return true;
        }
        return false;

    case CLOUD_CHAOS:
        if (coinflip())
        {
            // TODO: Not have this in melee_attack
            melee_attack::chaos_affect_actor(act);
            return true;
        }
        break;

    case CLOUD_ACID:
    {
        const actor* agent = cloud.agent();
        act->splash_with_acid(agent, 5, true);
        return true;
    }

    case CLOUD_NEGATIVE_ENERGY:
    {
        actor* agent = cloud.agent();
        if (act->drain_exp(agent))
        {
            if (cloud.whose == KC_YOU)
                did_god_conduct(DID_NECROMANCY, 5 + random2(3));
            return true;
        }
        break;
    }

    default:
        break;
    }
    return false;
}

static int _actor_cloud_base_damage(const actor *act,
                                    const cloud_struct &cloud,
                                    int resist,
                                    bool maximum_damage)
{
    if (actor_cloud_immune(act, cloud))
        return 0;

    const int cloud_raw_base_damage =
        _cloud_base_damage(act, cloud, maximum_damage);
    const int cloud_base_damage = (resist == MAG_IMMUNE ?
                                   0 : cloud_raw_base_damage);
    return cloud_base_damage;
}

static int _cloud_damage_output(const actor *actor,
                                beam_type flavour,
                                int base_damage,
                                bool maximum_damage = false)
{
    if (maximum_damage)
        return resist_adjust_damage(actor, flavour, base_damage);

    int dam = actor->apply_ac(base_damage);
    dam = resist_adjust_damage(actor, flavour, dam);
    return dam;
//    return max(0, dam);
}

/**
 * How much damage will this cloud do to the given actor?
 *
 * @param act               The actor in question.
 * @param cloud             The cloud in question.
 * @param maximum_damage    Whether to return the maximum possible damage.
 */
static int _actor_cloud_damage(const actor *act,
                               const cloud_struct &cloud,
                               bool maximum_damage)
{
    const int resist = _actor_cloud_resist(act, cloud);
    const int cloud_base_damage = _actor_cloud_base_damage(act, cloud,
                                                           resist,
                                                           maximum_damage);
    int final_damage = cloud_base_damage;

    switch (cloud.type)
    {
    case CLOUD_FIRE:
    case CLOUD_FOREST_FIRE:
    case CLOUD_HOLY_FLAMES:
    case CLOUD_COLD:
    case CLOUD_STEAM:
    case CLOUD_SPECTRAL:
    case CLOUD_ACID:
    case CLOUD_NEGATIVE_ENERGY:
        final_damage =
            _cloud_damage_output(act, _cloud2beam(cloud.type),
                                 cloud_base_damage,
                                 maximum_damage);
        break;
    case CLOUD_STORM:
    {

        // if we don't have thunder, there's always rain
        cloud_struct raincloud = cloud;
        raincloud.type = CLOUD_RAIN;
        const int rain_damage = _actor_cloud_damage(act, raincloud,
                                                    maximum_damage);

        // if this isn't just a test run, and no time passed, don't trigger
        // lightning. (just rain.)
        if (!maximum_damage && !(you.turn_is_over && you.time_taken > 0))
            return rain_damage;

        // only announce ourselves if this isn't a test run.
        if (!maximum_damage)
            cloud.announce_actor_engulfed(act);

        const int turns_per_lightning = 4;
        const int aut_per_lightning = turns_per_lightning * BASELINE_DELAY;

        // if we fail our lightning roll, again, just rain.
        if (!maximum_damage && !x_chance_in_y(you.time_taken,
                                              aut_per_lightning))
        {
            return rain_damage;
        }

        const int lightning_dam = _cloud_damage_output(act,
                                                       _cloud2beam(cloud.type),
                                                       cloud_base_damage,
                                                       maximum_damage);

        if (maximum_damage)
        {
            // Average maximum damage over time.
            const int avg_dam = lightning_dam / turns_per_lightning;
            if (avg_dam > 0)
                return avg_dam;
            return rain_damage; // vs relec+++ or w/e
        }

        if (act->is_player())
            mprf("You are struck by lightning! (%d)", lightning_dam);
        else if (you.can_see(*act))
        {
            mprf("%s is struck by lightning. (%d)", act->as_monster()->name(DESC_THE).c_str(), lightning_dam);
        }
        else if (you.see_cell(act->pos()))
        {
            mprf("Lightning from the thunderstorm strikes something you cannot "
                "see. (%d)", lightning_dam);
        }
        noisy(spell_effect_noise(SPELL_LIGHTNING_BOLT), act->pos(),
              act->is_player() || you.see_cell(act->pos())
              || you_worship(GOD_QAZLAL)
                ? nullptr
                : "You hear a clap of thunder!");

        return lightning_dam;

    }
    default:
        break;
    }

    return timescale_damage(act, final_damage);
}

// Applies damage and side effects for an actor in a cloud and returns
// the damage dealt.
int actor_apply_cloud(actor *act)
{
    cloud_struct* cl = cloud_at(act->pos());
    if (!cl)
        return 0;

    const cloud_struct &cloud(*cl);
    const bool player = act->is_player();
    monster *mons = !player? act->as_monster() : nullptr;
    const beam_type cloud_flavour = _cloud2beam(cloud.type);

    if (actor_cloud_immune(act, cloud))
        return 0;

    const int resist = _actor_cloud_resist(act, cloud);
    const int cloud_max_base_damage =
        _actor_cloud_base_damage(act, cloud, resist, true);
    const int final_damage = _actor_cloud_damage(act, cloud, false);

    if ((player || final_damage > 0
         || _cloud_has_negative_side_effects(cloud.type))
        && cloud.type != CLOUD_STORM
		&& final_damage > 0) // handled elsewhere
    {
        cloud.announce_actor_engulfed(act, final_damage < 0);
    }
    if (player && cloud_max_base_damage > 0 && resist > 0
        && (cloud.type != CLOUD_STORM || final_damage > 0)
		&& you.species != SP_DJINNI)
    {
        canned_msg(MSG_YOU_RESIST);
    }

    if (cloud_flavour != BEAM_NONE)
        act->expose_to_element(cloud_flavour, 7);

    const bool side_effects =
        _actor_apply_cloud_side_effects(act, cloud, final_damage);

    if (!player && (side_effects || final_damage > 0))
        behaviour_event(mons, ME_DISTURB, 0, act->pos());

    if (final_damage)
    {
        dprf("%s %s %d damage from cloud: %s.",
             act->name(DESC_THE).c_str(),
             act->conj_verb("take").c_str(),
             final_damage,
             cloud.cloud_name().c_str());

        actor *oppressor = cloud.agent();
        act->hurt(oppressor, final_damage, BEAM_MISSILE,
                  KILLED_BY_CLOUD, "", cloud.cloud_name(true));
    }

    /*
    if (final_damage < 0) {
    	// cloud dissipates twice as fast if we are healing from it
    	cl->decay -= _cloud_dissipation_rate(cloud);
    }
     */

    return final_damage;
}

static bool _cloud_is_harmful(actor *act, cloud_struct &cloud,
                              int maximum_negligible_damage)
{
    return !actor_cloud_immune(act, cloud)
           && (_cloud_has_negative_side_effects(cloud.type)
               || (_actor_cloud_damage(act, cloud, true) >
                   maximum_negligible_damage));
}

/**
 * Is this cloud type dangerous to you?
 *
 * @param type the type of cloud to look at.
 * @param accept_temp_resistances whether to look at resistances from your form
 *        or durations; items and gods are used regardless of this parameter's value.
 * @param yours whether to treat this cloud as being made by you.
 */
bool is_damaging_cloud(cloud_type type, bool accept_temp_resistances, bool yours)
{
    // A nasty hack; map_knowledge doesn't preserve whom the cloud belongs to.
    if (type == CLOUD_TORNADO)
        return !you.duration[DUR_TORNADO] && !you.duration[DUR_TORNADO_COOLDOWN];

    if (accept_temp_resistances)
    {
        cloud_struct cloud;
        cloud.type = type;
        cloud.decay = 100;
        if (yours)
            cloud.set_killer(KILL_YOU);
        return _cloud_is_harmful(&you, cloud, 0);
    }
    else
    {
        // [ds] Yes, this is an ugly kludge: temporarily hide
        // durations and transforms.
        unwind_var<durations_t> old_durations(you.duration);
        unwind_var<transformation_type> old_form(you.form, TRAN_NONE);
        you.duration.init(0);
        return is_damaging_cloud(type, true, yours);
    }
}

/**
 * Will the given monster refuse to walk into the given cloud?
 *
 * @param mons              The monster in question.
 * @param cloud             The cloud in question.
 * @param extra_careful     Whether the monster could suffer any harm from the
 *                          cloud at all, even if it would normally be brave
 *                          enough (based on e.g. hp) to enter the cloud.
 * @return                  Whether the monster is NOT ok to enter the cloud.
 */
static bool _mons_avoids_cloud(const monster* mons, const cloud_struct& cloud,
                               bool extra_careful)
{
    // clouds you're immune to are inherently safe.
    if (actor_cloud_immune(mons, cloud))
        return false;

    // harmless clouds, likewise.
    if (is_harmless_cloud(cloud.type))
        return false;

    // Berserk monsters are less careful and will blindly plow through any
    // dangerous cloud, just to kill you. {due}
    if (!extra_careful && mons->berserk_or_insane())
        return false;

    const int resistance = _actor_cloud_resist(mons, cloud);

    // Thinking things avoid things they are vulnerable to (-resists)
    if (mons_intel(mons) >= I_ANIMAL && resistance < 0)
        return true;

    switch (cloud.type)
    {
    case CLOUD_MIASMA:
        // Even the dumbest monsters will avoid miasma if they can.
        return true;

    case CLOUD_RAIN:
        // Fiery monsters dislike the rain.
        if (mons->is_fiery() && extra_careful)
            return true;

        // We don't care about what's underneath the rain cloud if we can fly.
        if (mons->airborne())
            return false;

        // These don't care about deep water.
        if (monster_habitable_grid(mons, DNGN_DEEP_WATER))
            return false;

        // This position could become deep water, and they might drown.
        if (grd(cloud.pos) == DNGN_SHALLOW_WATER
            && mons_intel(mons) > I_BRAINLESS)
        {
            return true;
        }
        break;

    default:
    {
        if (extra_careful)
            return true;

        const int rand_dam = clouds[cloud.type].expected_random_damage;
        const int trials = max(1, rand_dam/9);
        const int hp_threshold = clouds[cloud.type].expected_base_damage +
                                 random2avg(rand_dam, trials);

        // intelligent monsters want a larger margin of safety
        int safety_mult = (mons_intel(mons) > I_ANIMAL) ? 2 : 1;
        // dare we risk the damage?
        const bool hp_ok = mons->hit_points > safety_mult * hp_threshold;
        // dare we risk the status effects?
        const bool sfx_ok = cloud.type != CLOUD_MEPHITIC
                            || x_chance_in_y(mons->get_hit_dice() - 1, 5);
        if (hp_ok && sfx_ok)
            return false;
        break;
    }
    }

    // Exceedingly dumb creatures will wander into harmful clouds.
    if (mons_intel(mons) == I_BRAINLESS && !extra_careful)
        return false;

    // If we get here, the cloud is potentially harmful.
    return true;
}

// Like the above, but allow a monster to move from one damaging cloud
// to another, even if they're of different types.
bool mons_avoids_cloud(const monster* mons, coord_def pos, bool placement)
{
    if (!cloud_at(pos))
        return false;

    // Is the target cloud okay?
    if (!_mons_avoids_cloud(mons, *cloud_at(pos), placement))
        return false;

    // If we're already in a cloud that we'd want to avoid then moving
    // from one to the other is okay.
    if (!in_bounds(mons->pos()) || mons->pos() == pos)
        return true;

    if (!cloud_at(mons->pos()))
        return true;

    return !_mons_avoids_cloud(mons, *cloud_at(mons->pos()), true);
}
// Is the cloud purely cosmetic with no gameplay effect? If so, <foo>
// is engulfed in <cloud> messages will be suppressed.
static bool _cloud_is_cosmetic(cloud_type type)
{
    switch (type)
    {
    case CLOUD_BLACK_SMOKE:
    case CLOUD_GREY_SMOKE:
    case CLOUD_BLUE_SMOKE:
    case CLOUD_PURPLE_SMOKE:
    case CLOUD_MIST:
        return true;
    default:
        return false;
    }
}

bool is_harmless_cloud(cloud_type type)
{
    switch (type)
    {
    case CLOUD_NONE:
    case CLOUD_TLOC_ENERGY:
    case CLOUD_MAGIC_TRAIL:
    case CLOUD_DUST_TRAIL:
#if TAG_MAJOR_VERSION == 34
    case CLOUD_GLOOM:
#endif
    case CLOUD_INK:
    case CLOUD_DEBUGGING:
        return true;
    default:
        return _cloud_is_cosmetic(type);
    }
}

string cloud_type_name(cloud_type type, bool terse)
{
    if (type <= CLOUD_NONE || type >= NUM_CLOUD_TYPES)
        return "buggy goodness";

    ASSERT(clouds[type].terse_name);
    if (terse || clouds[type].verbose_name == nullptr)
        return clouds[type].terse_name;
    return clouds[type].verbose_name;
}

cloud_type cloud_name_to_type(const string &name)
{
    const string lower_name = lowercase_string(name);

    if (lower_name == "random")
        return CLOUD_RANDOM;
    else if (lower_name == "debugging")
        return CLOUD_DEBUGGING;

    for (int i = CLOUD_NONE; i < CLOUD_RANDOM; i++)
        if (cloud_type_name(static_cast<cloud_type>(i)) == lower_name)
            return static_cast<cloud_type>(i);

    return CLOUD_NONE;
}

////////////////////////////////////////////////////////////////////////
// cloud_struct

kill_category cloud_struct::killer_to_whose(killer_type _killer)
{
    switch (_killer)
    {
        case KILL_YOU:
        case KILL_YOU_MISSILE:
        case KILL_YOU_CONF:
            return KC_YOU;

        case KILL_MON:
        case KILL_MON_MISSILE:
        case KILL_MISC:
            return KC_OTHER;

        default:
            die("invalid killer type");
    }
    return KC_OTHER;
}

killer_type cloud_struct::whose_to_killer(kill_category _whose)
{
    switch (_whose)
    {
        case KC_YOU:         return KILL_YOU_MISSILE;
        case KC_FRIENDLY:    return KILL_MON_MISSILE;
        case KC_OTHER:       return KILL_MISC;
        case KC_NCATEGORIES: die("invalid kill category");
    }
    return KILL_NONE;
}

void cloud_struct::set_whose(kill_category _whose)
{
    whose  = _whose;
    killer = whose_to_killer(whose);
}

void cloud_struct::set_killer(killer_type _killer)
{
    killer = _killer;
    whose  = killer_to_whose(killer);

    switch (killer)
    {
    case KILL_YOU:
        killer = KILL_YOU_MISSILE;
        break;

    case KILL_MON:
        killer = KILL_MON_MISSILE;
        break;

    default:
        break;
    }
}

actor *cloud_struct::agent() const
{
    return find_agent(source, whose);
}

string cloud_struct::cloud_name(bool terse) const
{
    return !name.empty() ? name : cloud_type_name(type, terse);
}

void cloud_struct::announce_actor_engulfed(const actor *act,
                                           bool beneficial) const
{
    ASSERT(act); // XXX: change to const actor &act
    if (_cloud_is_cosmetic(type))
        return;

    if (!you.can_see(*act))
        return;

    // Normal clouds. (Unmodified rain clouds have a different message.)
    const bool raincloud = type == CLOUD_RAIN || type == CLOUD_STORM;
    const bool unmodified = cloud_name() == cloud_type_name(type, false);
    if (!raincloud || !unmodified)
    {
        mprf("%s %s in %s.",
             act->name(DESC_THE).c_str(),
             beneficial ? act->conj_verb("bask").c_str()
                        : (act->conj_verb("are") + " engulfed").c_str(),
             cloud_name().c_str());
        return;
    }

    // Don't produce monster-in-rain messages in the interests
    // of spam reduction.
    if (act->is_player())
    {
        mprf("%s %s standing in %s.",
             act->name(DESC_THE).c_str(),
             act->conj_verb("are").c_str(),
             type == CLOUD_STORM ? "a thunderstorm" : "the rain");
    }
}

/**
 * What colour is the given cloud?
 *
 * @param cloudno       The cloud in question.
 * @return              An appropriate colour for the cloud.
 *                      May vary from call to call (randomized for some cloud
 *                      types).
 */
colour_t get_cloud_colour(const cloud_struct &cloud)
{
    // if the cloud has a set custom colour, use that.
    if (cloud.colour != -1)
        return cloud.colour;

    // if we have the colour in data, use that.
    if (clouds[cloud.type].colour)
        return clouds[cloud.type].colour;

    // weird clouds
    switch (cloud.type)
    {
    case CLOUD_FIRE:
    case CLOUD_FOREST_FIRE:
        if (cloud.decay <= 20)
            return RED;
        if (cloud.decay <= 40)
            return LIGHTRED;

        // total weight 16
        return random_choose_weighted(9, YELLOW,
                                      4, RED,
                                      3, LIGHTRED,
                                      0);

    case CLOUD_COLD:
        if (cloud.decay <= 20)
            return BLUE;
        if (cloud.decay <= 40)
            return LIGHTBLUE;

        // total weight 16
        return random_choose_weighted(9, WHITE,
                                      4, BLUE,
                                      3, LIGHTBLUE,
                                      0);
        break;

    default:
        return LIGHTGREY;
    }
}

coord_def get_cloud_originator(const coord_def& pos)
{
    const cloud_struct* cloud = cloud_at(pos);
    if (!cloud)
        return coord_def();
    const actor *agent = actor_by_mid(cloud->source);
    if (!agent)
        return coord_def();
    return agent->pos();
}

void remove_tornado_clouds(mid_t whose)
{
    // Needed to clean up after the end of tornado cooldown, so we can again
    // assume all "raging winds" clouds are harmful. This is needed only
    // because map_knowledge doesn't preserve the knowledge about whom the
    // cloud belongs to. If this changes, please remove this function. For
    // example, this approach doesn't work if we ever make Tornado a monster
    // spell (excluding immobile and mindless casters).

    for (auto& entry : env.cloud)
        if (entry.second.type == CLOUD_TORNADO && entry.second.source == whose)
            delete_cloud(entry.first);
}

static void _spread_cloud(coord_def pos, cloud_type type, int radius, int pow,
                          int &remaining, int ratio = 10,
                          mid_t agent_mid = 0, kill_category kcat = KC_OTHER)
{
    bolt beam;
    beam.target = pos;
    beam.use_target_as_pos = true;
    explosion_map exp_map;
    exp_map.init(INT_MAX);
    beam.determine_affected_cells(exp_map, coord_def(), 0,
                                  radius, true, true);

    coord_def centre(9,9);
    for (distance_iterator di(pos, true, false); di; ++di)
    {
        if (di.radius() > radius)
            return;

        if ((exp_map(*di - pos + centre) < INT_MAX) && !cloud_at(*di)
            && (di.radius() < radius || x_chance_in_y(ratio, 100)))
        {
            place_cloud(type, *di, pow + random2(pow), nullptr);
            --remaining;

            // Setting this way since the agent of the cloud may be dead before
            // cloud is placed, so no agent exists to pass to place_cloud (though
            // proper blame should still be assigned)
            if (cloud_struct* cloud = cloud_at(*di))
            {
                cloud->source = agent_mid;
                cloud->whose = kcat;
            }
        }

        // Placed all clouds for this spreader
        if (remaining == 0)
            return;
    }
}

void run_cloud_spreaders(int dur)
{
    if (!dur)
        return;

    for (map_marker *marker : env.markers.get_all(MAT_CLOUD_SPREADER))
    {
        map_cloud_spreader_marker * const mark
            = dynamic_cast<map_cloud_spreader_marker*>(marker);

        mark->speed_increment += dur;
        int rad = min(mark->speed_increment / mark->speed, mark->max_rad - 1) + 1;
        int ratio = (mark->speed_increment - ((rad - 1) * mark->speed))
                    * 100 / mark->speed;

        if (ratio == 0)
        {
            rad--;
            ratio = 100;
        }

        _spread_cloud(mark->pos, mark->ctype, rad, mark->duration,
                        mark->remaining, ratio, mark->agent_mid, mark->kcat);
        if ((rad >= mark->max_rad && ratio >= 100) || mark->remaining == 0)
        {
            env.markers.remove(mark);
            break;
        }
    }
}
