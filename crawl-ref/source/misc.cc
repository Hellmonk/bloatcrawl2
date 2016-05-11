/**
 * @file
 * @brief Misc functions.
**/

#include "AppHdr.h"

#include "misc.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if !defined(__IBMCPP__) && !defined(TARGET_COMPILER_VC)
#include <unistd.h>
#endif

#include "abyss.h"
#include "act-iter.h"
#include "areas.h"
#include "cloud.h"
#include "coordit.h"
#include "database.h"
#include "delay.h"
#include "dgn-shoals.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "fight.h"
#include "food.h"
#include "fprop.h"
#include "godpassive.h"
#include "items.h"
#include "item_use.h"
#include "libutil.h"
#include "mapmark.h"
#include "message.h"
#include "mon-pathfind.h"
#include "mon-place.h"
#include "mon-tentacle.h"
#include "ng-setup.h"
#include "player-stats.h"
#include "prompt.h"
#include "religion.h"
#include "spl-clouds.h"
#include "state.h"
#include "stringutil.h"
#include "terrain.h"
#include "tileview.h"
#include "transform.h"
#include "traps.h"
#include "travel.h"
#include "view.h"
#include "xom.h"

string weird_glowing_colour()
{
    return getMiscString("glowing_colour_name");
}

string weird_writing()
{
    return getMiscString("writing_name");
}

string weird_smell()
{
    return getMiscString("smell_name");
}

string weird_sound()
{
    return getMiscString("sound_name");
}

// HACK ALERT: In the following several functions, want_move is true if the
// player is travelling. If that is the case, things must be considered one
// square closer to the player, since we don't yet know where the player will
// be next turn.

// Returns true if the monster has a path to the player, or it has to be
// assumed that this is the case.
static bool _mons_has_path_to_player(const monster* mon, bool want_move = false)
{
    if (mon->is_stationary() && !mons_is_tentacle(mon->type))
    {
        int dist = grid_distance(you.pos(), mon->pos());
        if (want_move)
            dist--;
        if (dist >= 2)
            return false;
    }

    // Short-cut if it's already adjacent.
    if (grid_distance(mon->pos(), you.pos()) <= 1)
        return true;

    // If the monster is awake and knows a path towards the player
    // (even though the player cannot know this) treat it as unsafe.
    if (mon->travel_target == MTRAV_FOE)
        return true;

    if (mon->travel_target == MTRAV_KNOWN_UNREACHABLE)
        return false;

    // Try to find a path from monster to player, using the map as it's
    // known to the player and assuming unknown terrain to be traversable.
    monster_pathfind mp;
    const int range = mons_tracking_range(mon);
    // At the very least, we shouldn't consider a visible monster with a
    // direct path to you "safe" just because it would be too stupid to
    // track you that far out-of-sight. Use a factor of 2 for smarter
    // creatures as a safety margin.
    if (range > 0)
        mp.set_range(max(LOS_RADIUS, range * 2));

    if (mp.init_pathfind(mon, you.pos(), true, false, true))
        return true;

    // Now we know the monster cannot possibly reach the player.
    mon->travel_target = MTRAV_KNOWN_UNREACHABLE;

    return false;
}

bool mons_can_hurt_player(const monster* mon, const bool want_move)
{
    // FIXME: This takes into account whether the player knows the map!
    //        It should, for the purposes of i_feel_safe. [rob]
    // It also always returns true for sleeping monsters, but that's okay
    // for its current purposes. (Travel interruptions and tension.)
    if (_mons_has_path_to_player(mon, want_move))
        return true;

    // Even if the monster can not actually reach the player it might
    // still use some ranged form of attack.
    if (you.see_cell_no_trans(mon->pos())
        && mons_has_known_ranged_attack(mon))
    {
        return true;
    }

    return false;
}

// Returns true if a monster can be considered safe regardless
// of distance.
static bool _mons_is_always_safe(const monster *mon)
{
    return mon->wont_attack()
        || mon->type == MONS_BUTTERFLY
        || mon->withdrawn()
        || (mon->type == MONS_BALLISTOMYCETE && !mons_is_active_ballisto(mon));
}

bool mons_is_safe(const monster* mon, const bool want_move,
                  const bool consider_user_options, bool check_dist)
{
    // Short-circuit plants, some vaults have tons of those. Except for both
    // active and inactive ballistos, players may still want these.
    if (mons_is_firewood(mon) && mon->type != MONS_BALLISTOMYCETE)
        return true;

    int  dist    = grid_distance(you.pos(), mon->pos());

    bool is_safe = (_mons_is_always_safe(mon)
                    || check_dist
                       && (mon->pacified() && dist > 1
                           || crawl_state.disables[DIS_MON_SIGHT] && dist > 2
                           // Only seen through glass walls or within water?
                           // Assuming that there are no water-only/lava-only
                           // monsters capable of throwing or zapping wands.
                           || !mons_can_hurt_player(mon, want_move)));

#ifdef CLUA_BINDINGS
    if (consider_user_options)
    {
        bool moving = (!you.delay_queue.empty()
                          && delay_is_run(you.delay_queue.front().type)
                          && you.delay_queue.front().type != DELAY_REST
                       || you.running < RMODE_NOT_RUNNING
                       || want_move);

        bool result = is_safe;

        monster_info mi(mon, MILEV_SKIP_SAFE);
        if (clua.callfn("ch_mon_is_safe", "Ibbd>b",
                        &mi, is_safe, moving, dist,
                        &result))
        {
            is_safe = result;
        }
    }
#endif

    return is_safe;
}

// Return all nearby monsters in range (default: LOS) that the player
// is able to recognise as being monsters (i.e. no submerged creatures.)
//
// want_move       (??) Somehow affects what monsters are considered dangerous
// just_check      Return zero or one monsters only
// dangerous_only  Return only "dangerous" monsters
// require_visible Require that monsters be visible to the player
// range           search radius (defaults: LOS)
//
vector<monster* > get_nearby_monsters(bool want_move,
                                      bool just_check,
                                      bool dangerous_only,
                                      bool consider_user_options,
                                      bool require_visible,
                                      bool check_dist,
                                      int range)
{
    ASSERT(!crawl_state.game_is_arena());

    if (range == -1)
        range = LOS_RADIUS;

    vector<monster* > mons;

    // Sweep every visible square within range.
    for (radius_iterator ri(you.pos(), range, C_SQUARE, LOS_DEFAULT); ri; ++ri)
    {
        if (monster* mon = monster_at(*ri))
        {
            if (mon->alive()
                && (!require_visible || mon->visible_to(&you))
                && !mon->submerged()
                && (!dangerous_only || !mons_is_safe(mon, want_move,
                                                     consider_user_options,
                                                     check_dist)))
            {
                mons.push_back(mon);
                if (just_check) // stop once you find one
                    break;
            }
        }
    }
    return mons;
}

static bool _exposed_monsters_nearby(bool want_move)
{
    const int radius = want_move ? 2 : 1;
    for (radius_iterator ri(you.pos(), radius, C_SQUARE, LOS_DEFAULT); ri; ++ri)
        if (env.map_knowledge(*ri).flags & MAP_INVISIBLE_MONSTER)
            return true;
    return false;
}

bool i_feel_safe(bool announce, bool want_move, bool just_monsters,
                 bool check_dist, int range)
{
    if (!just_monsters)
    {
        // check clouds
        if (in_bounds(you.pos()))
        {
            const cloud_type type = cloud_type_at(you.pos());

            // Temporary immunity allows travelling through a cloud but not
            // resting in it.
            // Qazlal immunity will allow for it, however.
            if (is_damaging_cloud(type, want_move, cloud_is_yours_at(you.pos())))
            {
                if (announce)
                {
                    mprf(MSGCH_WARN, "You're standing in a cloud of %s!",
                         cloud_type_name(type).c_str());
                }
                return false;
            }
        }

        // No monster will attack you inside a sanctuary,
        // so presence of monsters won't matter -- until it starts shrinking...
        if (is_sanctuary(you.pos()) && env.sanctuary_time >= 5)
            return true;

        if (poison_is_lethal())
        {
            if (announce)
                mprf(MSGCH_WARN, "There is a lethal amount of poison in your body!");

            return false;
        }
    }

    // Monster check.
    vector<monster* > visible =
        get_nearby_monsters(want_move, !announce, true, true, true,
                            check_dist, range);

    // Announce the presence of monsters (Eidolos).
    string msg;
    if (visible.size() == 1)
    {
        const monster& m = *visible[0];
        msg = make_stringf("%s is nearby!", m.name(DESC_A).c_str());
    }
    else if (visible.size() > 1)
        msg = "There are monsters nearby!";
    else if (_exposed_monsters_nearby(want_move))
        msg = "There is a strange disturbance nearby!";
    else
        return true;

    if (announce)
    {
        mprf(MSGCH_WARN, "%s", msg.c_str());

        if (Options.use_animations & UA_MONSTER_IN_SIGHT)
        {
            static bool tried = false;

            if (visible.size() && tried)
            {
                monster_view_annotator flasher(&visible);
                delay(100);
            }
            else if (visible.size())
                tried = true;
            else
                tried = false;
        }
    }

    return false;
}

bool there_are_monsters_nearby(bool dangerous_only, bool require_visible,
                               bool consider_user_options)
{
    return !get_nearby_monsters(false, true, dangerous_only,
                                consider_user_options,
                                require_visible).empty();
}

// General threat = sum_of_logexpervalues_of_nearby_unfriendly_monsters.
// Highest threat = highest_logexpervalue_of_nearby_unfriendly_monsters.
static void _monster_threat_values(double *general, double *highest,
                                   bool *invis, coord_def pos = you.pos())
{
    *invis = false;

    double sum = 0;
    int highest_xp = -1;

    for (monster_near_iterator mi(pos); mi; ++mi)
    {
        if (mi->friendly())
            continue;

        const int xp = exper_value(*mi);
        const double log_xp = log((double)xp);
        sum += log_xp;
        if (xp > highest_xp)
        {
            highest_xp = xp;
            *highest   = log_xp;
        }
        if (!mi->visible_to(&you))
            *invis = true;
    }

    *general = sum;
}

bool player_in_a_dangerous_place(bool *invis)
{
    bool junk;
    if (invis == nullptr)
        invis = &junk;

    const double logexp = log((double)you.experience + 1);
    double gen_threat = 0.0, hi_threat = 0.0;
    _monster_threat_values(&gen_threat, &hi_threat, invis);

    return gen_threat > logexp * 1.3 || hi_threat > logexp / 2;
}

void bring_to_safety()
{
    if (player_in_branch(BRANCH_ABYSS))
        return abyss_teleport();



    coord_def best_pos, pos;
    double min_threat = DBL_MAX;
    int tries = 0;

    // Up to 100 valid spots, but don't lock up when there's none. This can happen
    // on tiny Zig/portal rooms with a bad summon storm and you in cloud / over water.
    while (tries < 100000 && min_threat > 0)
    {
        pos.x = random2(GXM);
        pos.y = random2(GYM);
        if (!in_bounds(pos)
            || grd(pos) != DNGN_FLOOR
            || cloud_at(pos)
            || monster_at(pos)
            || env.pgrid(pos) & FPROP_NO_TELE_INTO
            || crawl_state.game_is_sprint()
               && grid_distance(pos, you.pos()) > 8)
        {
            tries++;
            continue;
        }

        for (adjacent_iterator ai(pos); ai; ++ai)
            if (grd(*ai) == DNGN_SLIMY_WALL)
            {
                tries++;
                continue;
            }

        bool junk;
        double gen_threat = 0.0, hi_threat = 0.0;
        _monster_threat_values(&gen_threat, &hi_threat, &junk, pos);
        const double threat = gen_threat * hi_threat;

        if (threat < min_threat)
        {
            best_pos = pos;
            min_threat = threat;
        }
        tries += 1000;
    }

    if (min_threat < DBL_MAX)
        you.moveto(best_pos);
}

// This includes ALL afflictions, unlike wizard/Xom revive.
void revive()
{
    // adjust_level(-1);

    // Allow a spare after two levels (we just lost one); the exact value
    // doesn't matter here.
    you.attribute[ATTR_LIFE_GAINED] = 0;

    you.disease = 0;
    you.magic_contamination = 0;
    set_hunger(HUNGER_DEFAULT, true);
    restore_stat(STAT_ALL, 0, true);

    you.attribute[ATTR_DELAYED_FIREBALL] = 0;
    clear_trapping_net();
    you.attribute[ATTR_DIVINE_VIGOUR] = 0;
    you.attribute[ATTR_DIVINE_STAMINA] = 0;
    you.attribute[ATTR_DIVINE_SHIELD] = 0;
    if (you.form)
        untransform();
    you.clear_beholders();
    you.clear_fearmongers();
    you.attribute[ATTR_DIVINE_DEATH_CHANNEL] = 0;
    you.attribute[ATTR_INVIS_UNCANCELLABLE] = 0;
    you.attribute[ATTR_FLIGHT_UNCANCELLABLE] = 0;
    you.attribute[ATTR_XP_DRAIN] = 0;
    you.attribute[ATTR_INSIGHT] = 0;
    if (you.duration[DUR_SCRYING])
        you.xray_vision = false;

    for (int dur = 0; dur < NUM_DURATIONS; dur++)
        if (dur != DUR_GOURMAND && dur != DUR_PIETY_POOL)
            you.duration[dur] = 0;

    unrot_hp(9999);
    set_hp(9999);
    set_mp(9999);
    you.dead = false;

    // Remove silence.
    invalidate_agrid();

    if (you.hp_max <= 0)
    {
        you.lives = 0;
        mpr("You are too frail to live.");
        // possible only with an extreme abuse of Borgnjor's
        ouch(INSTANT_DEATH, KILLED_BY_DRAINING);
    }

    mpr("You rejoin the land of the living...");
    // included in default force_more_message
}

bool bad_attack(const monster *mon, string& adj, string& suffix,
                bool& would_cause_penance, coord_def attack_pos,
                bool check_landing_only)
{
    ASSERT(mon); // XXX: change to const monster &mon
    ASSERT(!crawl_state.game_is_arena());
    bool bad_landing = false;

    if (!you.can_see(*mon))
        return false;

    if (attack_pos == coord_def(0, 0))
        attack_pos = you.pos();

    adj.clear();
    suffix.clear();
    would_cause_penance = false;

    if (!check_landing_only
        && (is_sanctuary(mon->pos()) || is_sanctuary(attack_pos)))
    {
        suffix = ", despite your sanctuary";
    }
    else if (check_landing_only && is_sanctuary(attack_pos))
    {
        suffix = ", when you might land in your sanctuary";
        bad_landing = true;
    }
    if (check_landing_only)
        return bad_landing;

    if (you_worship(GOD_JIYVA) && mons_is_slime(mon)
        && !(mon->is_shapeshifter() && (mon->flags & MF_KNOWN_SHIFTER)))
    {
        would_cause_penance = true;
        return true;
    }

    if (mon->friendly())
    {
        if (god_hates_attacking_friend(you.religion, mon))
        {
            adj = "your ally ";

            monster_info mi(mon, MILEV_NAME);
            if (!mi.is(MB_NAME_UNQUALIFIED))
                adj += "the ";

            would_cause_penance = true;

        }
        else
        {
            adj = "your ";

            monster_info mi(mon, MILEV_NAME);
            if (mi.is(MB_NAME_UNQUALIFIED))
                adj += "ally ";
        }

        return true;
    }

    if (find_stab_type(&you, mon) != STAB_NO_STAB
        && you_worship(GOD_SHINING_ONE)
        && !tso_unchivalric_attack_safe_monster(mon))
    {
        adj += "helpless ";
        would_cause_penance = true;
    }

    if (mon->neutral() && is_good_god(you.religion))
    {
        adj += "neutral ";
        if (you_worship(GOD_SHINING_ONE) || you_worship(GOD_ELYVILON))
            would_cause_penance = true;
    }
    else if (mon->wont_attack())
    {
        adj += "non-hostile ";
        if (you_worship(GOD_SHINING_ONE) || you_worship(GOD_ELYVILON))
            would_cause_penance = true;
    }

    return !adj.empty() || !suffix.empty();
}

bool stop_attack_prompt(const monster* mon, bool beam_attack,
                        coord_def beam_target, bool *prompted,
                        coord_def attack_pos, bool check_landing_only)
{
    ASSERT(mon); // XXX: change to const monster &mon
    bool penance = false;

    if (prompted)
        *prompted = false;

    if (crawl_state.disables[DIS_CONFIRMATIONS])
        return false;

    if (you.confused() || !you.can_see(*mon))
        return false;

    string adj, suffix;
    if (!bad_attack(mon, adj, suffix, penance, attack_pos, check_landing_only))
        return false;

    // Listed in the form: "your rat", "Blork the orc".
    string mon_name = mon->name(DESC_PLAIN);
    if (starts_with(mon_name, "the ")) // no "your the Royal Jelly" nor "the the RJ"
        mon_name = mon_name.substr(4); // strlen("the ")
    if (!starts_with(adj, "your"))
        adj = "the " + adj;
    mon_name = adj + mon_name;
    string verb;
    if (beam_attack)
    {
        verb = "fire ";
        if (beam_target == mon->pos())
            verb += "at ";
        else
        {
            verb += "in " + apostrophise(mon_name) + " direction";
            mon_name = "";
        }
    }
    else
        verb = "attack ";

    const string prompt = make_stringf("Really %s%s%s?%s",
             verb.c_str(), mon_name.c_str(), suffix.c_str(),
             penance ? " This attack would place you under penance!" : "");

    if (prompted)
        *prompted = true;

    if (yesno(prompt.c_str(), false, 'n'))
        return false;
    else
    {
        canned_msg(MSG_OK);
        return true;
    }
}

bool stop_attack_prompt(targetter &hitfunc, const char* verb,
                        bool (*affects)(const actor *victim), bool *prompted)
{
    if (crawl_state.disables[DIS_CONFIRMATIONS])
        return false;

    if (crawl_state.which_god_acting() == GOD_XOM)
        return false;

    if (you.confused())
        return false;

    string adj, suffix;
    bool penance = false;
    counted_monster_list victims;
    for (distance_iterator di(hitfunc.origin, false, true, LOS_RADIUS); di; ++di)
    {
        if (hitfunc.is_affected(*di) <= AFF_NO)
            continue;
        const monster* mon = monster_at(*di);
        if (!mon || !you.can_see(*mon))
            continue;
        if (affects && !affects(mon))
            continue;
        string adjn, suffixn;
        bool penancen = false;
        if (bad_attack(mon, adjn, suffixn, penancen))
        {
            // record the adjectives for the first listed, or
            // first that would cause penance
            if (victims.empty() || penancen && !penance)
                adj = adjn, suffix = suffixn, penance = penancen;
            victims.add(mon);
        }
    }

    if (victims.empty())
        return false;

    // Listed in the form: "your rat", "Blork the orc".
    string mon_name = victims.describe(DESC_PLAIN);
    if (starts_with(mon_name, "the ")) // no "your the Royal Jelly" nor "the the RJ"
        mon_name = mon_name.substr(4); // strlen("the ")
    if (!starts_with(adj, "your"))
        adj = "the " + adj;
    mon_name = adj + mon_name;

    const string prompt = make_stringf("Really %s %s%s?%s",
             verb, mon_name.c_str(), suffix.c_str(),
             penance ? " This attack would place you under penance!" : "");

    if (prompted)
        *prompted = true;

    if (yesno(prompt.c_str(), false, 'n'))
        return false;
    else
    {
        canned_msg(MSG_OK);
        return true;
    }
}

// Make the player swap positions with a given monster.
void swap_with_monster(monster* mon_to_swap)
{
    monster& mon(*mon_to_swap);
    ASSERT(mon.alive());
    const coord_def newpos = mon.pos();

    if (check_stasis())
        return;

    // Be nice: no swapping into uninhabitable environments.
    if (!you.is_habitable(newpos) || !mon.is_habitable(you.pos()))
    {
        mpr("You spin around.");
        return;
    }

    const bool mon_caught = mon.caught();
    const bool you_caught = you.attribute[ATTR_HELD];

    // If it was submerged, it surfaces first.
    mon.del_ench(ENCH_SUBMERGED);

    mprf("You swap places with %s.", mon.name(DESC_THE).c_str());

    mon.move_to_pos(you.pos(), true, true);

    if (you_caught)
    {
        check_net_will_hold_monster(&mon);
        if (!mon_caught)
        {
            you.attribute[ATTR_HELD] = 0;
            you.redraw_quiver = true;
            you.redraw_evasion = true;
        }
    }

    // Move you to its previous location.
    move_player_to_grid(newpos, false);

    if (mon_caught)
    {
        if (you.body_size(PSIZE_BODY) >= SIZE_GIANT)
        {
            int net = get_trapping_net(you.pos());
            if (net != NON_ITEM)
                destroy_item(net);
            mprf("The %s rips apart!", (net == NON_ITEM) ? "web" : "net");
            you.attribute[ATTR_HELD] = 0;
            you.redraw_quiver = true;
            you.redraw_evasion = true;
        }
        else
        {
            you.attribute[ATTR_HELD] = 10;
            if (get_trapping_net(you.pos()) != NON_ITEM)
                mpr("You become entangled in the net!");
            else
                mpr("You get stuck in the web!");
            you.redraw_quiver = true; // Account for being in a net.
            // Xom thinks this is hilarious if you trap yourself this way.
            if (you_caught)
                xom_is_stimulated(12);
            else
                xom_is_stimulated(200);
        }

        if (!you_caught)
            mon.del_ench(ENCH_HELD, true);
    }
}

// Reduce damage by AC.
// In most cases, we want AC to mostly stop weak attacks completely but affect
// strong ones less, but the regular formula is too hard to apply well to cases
// when damage is spread into many small chunks.
//
// Every point of damage is processed independently. Every point of AC has
// an independent 1/81 chance of blocking that damage.
//
// AC 20 stops 22% of damage, AC 40 -- 39%, AC 80 -- 63%.
int apply_chunked_AC(int dam, int ac)
{
    double chance = pow(80.0/81, ac);
    uint64_t cr = chance * (((uint64_t)1) << 32);

    int hurt = 0;
    for (int i = 0; i < dam; i++)
        if (get_uint32() < cr)
            hurt++;

    return hurt;
}

void handle_real_time(chrono::time_point<chrono::system_clock> now)
{
    const chrono::milliseconds elapsed =
     chrono::duration_cast<chrono::milliseconds>(now - you.last_keypress_time);
    you.real_time_delta = min<chrono::milliseconds>(
      elapsed,
      (chrono::milliseconds)(IDLE_TIME_CLAMP * 1000));
    you.real_time_ms += you.real_time_delta;
    you.last_keypress_time = now;
}

unsigned int breakpoint_rank(int val, const int breakpoints[],
                             unsigned int num_breakpoints)
{
    unsigned int result = 0;
    while (result < num_breakpoints && val >= breakpoints[result])
        ++result;

    return result;
}

void counted_monster_list::add(const monster* mons)
{
    const string name = mons->name(DESC_PLAIN);
    for (auto &entry : list)
    {
        if (entry.first->name(DESC_PLAIN) == name)
        {
            entry.second++;
            return;
        }
    }
    list.emplace_back(mons, 1);
}

int counted_monster_list::count()
{
    int nmons = 0;
    for (const auto &entry : list)
        nmons += entry.second;
    return nmons;
}

string counted_monster_list::describe(description_level_type desc,
                                      bool force_article)
{
    string out;

    for (auto i = list.begin(); i != list.end();)
    {
        const counted_monster &cm(*i);
        if (i != list.begin())
        {
            ++i;
            out += (i == list.end() ? " and " : ", ");
        }
        else
            ++i;

        out += cm.second > 1
               ? pluralise_monster(cm.first->name(desc, false, true))
               : cm.first->name(desc);
    }
    return out;
}

/**
 * Halloween or Hallowe'en (/ˌhæləˈwiːn, -oʊˈiːn, ˌhɑːl-/; a contraction of
 * "All Hallows' Evening"),[6] also known as Allhalloween,[7] All Hallows' Eve,
 * [8] or All Saints' Eve,[9] is a yearly celebration observed in a number of
 * countries on 31 October, the eve of the Western Christian feast of All
 * Hallows' Day... Within Allhallowtide, the traditional focus of All Hallows'
 * Eve revolves around the theme of using "humor and ridicule to confront the
 * power of death."[12]
 *
 * Typical festive Halloween activities include trick-or-treating (or the
 * related "guising"), attending costume parties, decorating, carving pumpkins
 * into jack-o'-lanterns, lighting bonfires, apple bobbing, visiting haunted
 * house attractions, playing pranks, telling scary stories, and watching
 * horror films.
 *
 * @return  Whether the current day is Halloween. (Cunning players may reset
 *          their system clocks to manipulate this. That's fine.)
 */
bool today_is_halloween()
{
    const time_t curr_time = time(nullptr);
    const struct tm *date = TIME_FN(&curr_time);
    // tm_mon is zero-based in case you are wondering
    return date->tm_mon == 9 && date->tm_mday == 31;
}

bool tobool(maybe_bool mb, bool def)
{
    switch (mb)
    {
    case MB_TRUE:
        return true;
    case MB_FALSE:
        return false;
    case MB_MAYBE:
    default:
        return def;
    }
}

maybe_bool frombool(bool b)
{
    return b ? MB_TRUE : MB_FALSE;
}
