/**
 * @file
 * @brief Traps related functions.
**/

#include "AppHdr.h"

#include "traps.h"
#include "trap_def.h"

#include <algorithm>
#include <cmath>

#include "act-iter.h"
#include "areas.h"
#include "bloodspatter.h"
#include "branch.h"
#include "cloud.h"
#include "coordit.h"
#include "delay.h"
#include "describe.h"
#include "directn.h"
#include "dungeon.h"
#include "english.h"
#include "exercise.h"
#include "godpassive.h" // passive_t::search_traps
#include "hints.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "mapmark.h"
#include "mon-enum.h"
#include "mon-tentacle.h"
#include "mgen_enum.h"
#include "message.h"
#include "mon-place.h"
#include "mon-transit.h"
#include "nearby-danger.h"
#include "output.h"
#include "prompt.h"
#include "random.h"
#include "religion.h"
#include "shout.h"
#include "spl-miscast.h"
#include "stash.h"
#include "state.h"
#include "stringutil.h"
#include "terrain.h"
#include "travel.h"
#include "view.h"
#include "xom.h"

bool trap_def::active() const
{
    return type != TRAP_UNASSIGNED;
}

bool trap_def::type_has_ammo() const
{
    switch (type)
    {
#if TAG_MAJOR_VERSION == 34
    case TRAP_DART:
#endif
    case TRAP_ARROW:  case TRAP_BOLT:
    case TRAP_NEEDLE: case TRAP_SPEAR:
        return true;
    default:
        break;
    }
    return false;
}

void trap_def::destroy(bool known)
{
    if (!in_bounds(pos))
        die("Trap position out of bounds!");

    grd(pos) = DNGN_FLOOR;
    if (known)
    {
        env.map_knowledge(pos).set_feature(DNGN_FLOOR);
        StashTrack.update_stash(pos);
    }
    env.trap.erase(pos);
}

void trap_def::hide()
{
    grd(pos) = DNGN_UNDISCOVERED_TRAP;
}

void trap_def::prepare_ammo(int charges)
{
    skill_rnd = random2(256);

    if (charges)
    {
        ammo_qty = charges;
        return;
    }
    switch (type)
    {
    case TRAP_ARROW:
    case TRAP_BOLT:
    case TRAP_NEEDLE:
        ammo_qty = 3 + random2avg(9, 3);
        break;
    case TRAP_SPEAR:
        ammo_qty = 2 + random2avg(6, 3);
        break;
    case TRAP_GOLUBRIA:
        // really, turns until it vanishes
        ammo_qty = 30 + random2(20);
        break;
    case TRAP_TELEPORT:
        ammo_qty = 1;
        break;
    default:
        ammo_qty = 0;
        break;
    }
}

void trap_def::reveal()
{
    grd(pos) = category();
}

string trap_def::name(description_level_type desc) const
{
    if (type >= NUM_TRAPS)
        return "buggy";

    string basename = full_trap_name(type);
    if (desc == DESC_A)
    {
        string prefix = "a";
        if (is_vowel(basename[0]))
            prefix += 'n';
        prefix += ' ';
        return prefix + basename;
    }
    else if (desc == DESC_THE)
        return string("the ") + basename;
    else                        // everything else
        return basename;
}

bool trap_def::is_known(const actor* act) const
{
    const bool player_knows = (grd(pos) != DNGN_UNDISCOVERED_TRAP);

    if (act == nullptr || act->is_player())
        return player_knows;
    else if (act->is_monster())
    {
        const monster* mons = act->as_monster();
        const int intel = mons_intel(*mons);

        // Smarter trap handling for intelligent monsters
        // * monsters native to a branch can be assumed to know the trap
        //   locations and thus be able to avoid them
        // * friendlies and good neutrals can be assumed to have been warned
        //   by the player about all traps s/he knows about
        // * very intelligent monsters can be assumed to have a high T&D
        //   skill (or have memorised part of the dungeon layout ;))

        if (type == TRAP_SHAFT)
        {
            // Slightly different rules for shafts:
            // * Lower intelligence requirement for native monsters.
            // * Allied zombies won't fall through shafts. (No herding!)
            return intel > I_BRAINLESS && mons_is_native_in_branch(*mons)
                   || player_knows && mons->wont_attack();
        }
        else
        {
            if (intel < I_HUMAN)
                return false;
            if (player_knows && mons->wont_attack())
                return true;

            return mons_is_native_in_branch(*mons);
        }
    }
    die("invalid actor type");
}

bool trap_def::is_safe(actor* act) const
{
    if (!act)
        act = &you;

    // TODO: For now, just assume they're safe; they don't damage outright,
    // and the messages get old very quickly
    if (category() == DNGN_TRAP_WEB) // && act->is_web_immune()
        return true;

#if TAG_MAJOR_VERSION == 34
    if (type == TRAP_SHADOW_DORMANT || type == TRAP_SHADOW)
        return true;
#endif

    if (!act->is_player())
        return false;

    // No prompt (teleport traps are ineffective if wearing a -Tele item)
    if ((type == TRAP_TELEPORT || type == TRAP_TELEPORT_PERMANENT)
        && you.no_tele(false))
    {
        return true;
    }

    if (!is_known(act))
        return false;

    if (type == TRAP_GOLUBRIA || type == TRAP_SHAFT)
        return true;

#ifdef CLUA_BINDINGS
    // Let players specify traps as safe via lua.
    if (clua.callbooleanfn(false, "c_trap_is_safe", "s", trap_name(type).c_str()))
        return true;
#endif

    if (type == TRAP_NEEDLE)
        return you.hp > 15;
    else if (type == TRAP_ARROW)
        return you.hp > 35;
    else if (type == TRAP_BOLT)
        return you.hp > 45;
    else if (type == TRAP_SPEAR)
        return you.hp > 40;
    else if (type == TRAP_BLADE)
        return you.hp > 95;

    return false;
}

/**
 * Get the item index of the first net on the square.
 *
 * @param where The location.
 * @param trapped If true, the index of the stationary net (trapping a victim)
 *                is returned.
 * @return  The item index of the net.
*/
int get_trapping_net(const coord_def& where, bool trapped)
{
    return NON_ITEM;
}

/**
 * Return a string describing the reason a given actor is ensnared. (Since nets
 * & webs use the same status.
 *
 * @param actor     The ensnared actor.
 * @return          Either 'held in a net' or 'caught in a web'.
 */
const char* held_status(actor *act)
{
    act = act ? act : &you;

    if (get_trapping_net(act->pos(), true) != NON_ITEM)
        return "held in a net";
    else
        return "caught in a web";
}

/**
 * Attempt to trap a monster in a net.
 *
 * @param mon       The monster being trapped.
 * @param agent     The entity doing the trapping.
 * @return          Whether the monster was successfully trapped.
 */
bool monster_caught_in_net(monster* mon, actor* agent)
{
    if (mon->body_size(PSIZE_BODY) >= SIZE_GIANT)
    {
        if (you.see_cell(mon->pos()))
        {
            if (!mon->visible_to(&you))
                mpr("The net bounces off something gigantic!");
            else
                simple_monster_message(*mon, " is too large for the net to hold!");
        }
        return false;
    }

    if (mons_class_is_stationary(mon->type))
    {
        if (you.see_cell(mon->pos()))
        {
            if (mon->visible_to(&you))
            {
                mprf("The net is caught on %s!",
                     mon->name(DESC_THE).c_str());
            }
            else
                mpr("The net is caught on something unseen!");
        }
        return false;
    }

    if (mon->is_insubstantial())
    {
        if (you.can_see(*mon))
        {
            mprf("The net passes right through %s!",
                 mon->name(DESC_THE).c_str());
        }
        return false;
    }

    if (!mon->caught() && mon->add_ench(ENCH_HELD))
    {
        if (you.see_cell(mon->pos()))
        {
            if (!mon->visible_to(&you))
                mpr("Something gets caught in the net!");
            else
                simple_monster_message(*mon, " is caught in the net!");
        }
        return true;
    }

    return false;
}

bool player_caught_in_net()
{
    if (you.body_size(PSIZE_BODY) >= SIZE_GIANT)
        return false;

    if (!you.attribute[ATTR_HELD])
    {
        mpr("You become entangled in the net!");
        stop_running();

        // Set the attribute after the mpr, otherwise the screen updates
        // and we get a glimpse of a web because there isn't a trapping net
        // item yet
        you.attribute[ATTR_HELD] = 1;

        stop_delay(true); // even stair delays
        return true;
    }
    return false;
}

void check_net_will_hold_monster(monster* mons)
{
    if (mons->body_size(PSIZE_BODY) >= SIZE_GIANT)
    {
        int net = get_trapping_net(mons->pos());
        if (net != NON_ITEM)
            destroy_item(net);

        if (you.see_cell(mons->pos()))
        {
            if (mons->visible_to(&you))
            {
                mprf("The net rips apart, and %s comes free!",
                     mons->name(DESC_THE).c_str());
            }
            else
                mpr("All of a sudden the net rips apart!");
        }
    }
    else if (mons->is_insubstantial())
    {
        const int net = get_trapping_net(mons->pos());
        if (net != NON_ITEM)
            free_stationary_net(net);

        simple_monster_message(*mons,
                               " drifts right through the net!");
    }
    else
        mons->add_ench(ENCH_HELD);
}

static bool _player_caught_in_web()
{
    if (you.attribute[ATTR_HELD])
        return false;

    you.attribute[ATTR_HELD] = 1;

    you.redraw_armour_class = true;
    you.redraw_evasion      = true;
    you.redraw_quiver       = true;

    // No longer stop_running() and stop_delay().
    return true;
}

vector<coord_def> find_golubria_on_level()
{
    vector<coord_def> ret;
    for (rectangle_iterator ri(coord_def(0, 0), coord_def(GXM-1, GYM-1)); ri; ++ri)
    {
        trap_def *trap = trap_at(*ri);
        if (trap && trap->type == TRAP_GOLUBRIA)
            ret.push_back(*ri);
    }
    return ret;
}

static bool _find_other_passage_side(coord_def& to)
{
    vector<coord_def> clear_passages;
    for (coord_def passage : find_golubria_on_level())
    {
        if (passage != to && !actor_at(passage))
            clear_passages.push_back(passage);
    }
    const int choices = clear_passages.size();
    if (choices < 1)
        return false;
    to = clear_passages[random2(choices)];
    return true;
}

// Returns a direction string from you.pos to the
// specified position. If fuzz is true, may be wrong.
// Returns an empty string if no direction could be
// determined (if fuzz if false, this is only if
// you.pos==pos).
static string _direction_string(coord_def pos, bool fuzz)
{
    int dx = you.pos().x - pos.x;
    if (fuzz)
        dx += random2avg(41,2) - 20;
    int dy = you.pos().y - pos.y;
    if (fuzz)
        dy += random2avg(41,2) - 20;
    const char *ew=((dx > 0) ? "west" : ((dx < 0) ? "east" : ""));
    const char *ns=((dy < 0) ? "south" : ((dy > 0) ? "north" : ""));
    if (abs(dy) > 2 * abs(dx))
        ew="";
    if (abs(dx) > 2 * abs(dy))
        ns="";
    return string(ns) + ew;
}

void trap_def::trigger(actor& triggerer)
{
    const bool you_know = is_known();
    const bool trig_knows = is_known(&triggerer);

    const bool you_trigger = triggerer.is_player();
    const bool in_sight = you.see_cell(pos);

    // If set, the trap will be removed at the end of the
    // triggering process.
    bool trap_destroyed = false, know_trap_destroyed = false;;

    monster* m = triggerer.as_monster();

    // Smarter monsters and those native to the level will simply
    // side-step known shafts. Unless they are already looking for
    // an exit, of course.
    if (type == TRAP_SHAFT
        && m
        && (!m->will_trigger_shaft()
            || trig_knows && !mons_is_fleeing(*m) && !m->pacified()))
    {
        return;
    }

    // Tentacles aren't real monsters, and shouldn't invoke magic traps.
    if (m && mons_is_tentacle_or_tentacle_segment(m->type)
        && category() != DNGN_TRAP_MECHANICAL)
    {
        return;
    }

    // Anything stepping onto a trap almost always reveals it.
    // (We can rehide it later for the exceptions.)
    if (in_sight)
        reveal();

    // Store the position now in case it gets cleared in between.
    const coord_def p(pos);

    if (type_has_ammo())
        shoot_ammo(triggerer, trig_knows);
    else switch (type)
    {
    case TRAP_GOLUBRIA:
    {
        coord_def to = p;
        if (_find_other_passage_side(to))
        {
            if (you_trigger)
                mpr("You enter the passage of Golubria.");
            else
                simple_monster_message(*m, " enters the passage of Golubria.");

            if (triggerer.move_to_pos(to))
            {
                if (you_trigger)
                    place_cloud(CLOUD_TLOC_ENERGY, p, 1 + random2(3), &you);
                else
                    place_cloud(CLOUD_TLOC_ENERGY, p, 1 + random2(3), m);
                trap_destroyed = true;
                know_trap_destroyed = you_trigger;
            }
            else
                mpr("But it is blocked!");
        }
        break;
    }
    case TRAP_TELEPORT:
    case TRAP_TELEPORT_PERMANENT:
        // Never revealed by monsters.
        // except when it's in sight, it's pretty obvious what happened. -doy
        if (!you_trigger && !you_know && !in_sight)
            hide();
        if (you_trigger)
            mprf("You enter %s!", name(DESC_A).c_str());
        if (ammo_qty > 0 && !--ammo_qty)
        {
            // can't use trap_destroyed, as we might recurse into a shaft
            // or be banished by a Zot trap
            if (in_sight)
            {
                env.map_knowledge(pos).set_feature(DNGN_FLOOR);
                mprf("%s disappears.", name(DESC_THE).c_str());
            }
            destroy();
        }
        if (!triggerer.no_tele(true, you_know || you_trigger))
            triggerer.teleport(true);
        break;

    case TRAP_ALARM:
        trap_destroyed = true;

        if (silenced(pos))
        {
            if (you_know && in_sight)
            {
                mprf("%s vibrates slightly, failing to make a sound.",
                     name(DESC_THE).c_str());
            }
        }
        else
        {
            string msg;
            if (you_trigger)
            {
                msg = make_stringf("%s emits a blaring wail!",
                                   name(DESC_THE).c_str());
            }
            else
            {
                string dir = _direction_string(pos, !in_sight);
                msg = string("You hear a ") + ((in_sight) ? "" : "distant ")
                      + "blaring wail " + (!dir.empty()? ("to the " + dir + ".")
                                                       : "behind you!");
            }

            // XXX: this is very goofy and probably should be replaced with
            // const mid_t source = triggerer.mid;
            mid_t source = !m ? MID_PLAYER :
                            mons_intel(*m) >= I_HUMAN ? m->mid : MID_NOBODY;

            noisy(40, pos, msg.c_str(), source);
        }

        if (you_trigger)
            you.sentinel_mark(true);
        break;

    case TRAP_BLADE:
        if (you_trigger)
        {
            if (trig_knows && one_chance_in(3))
                mpr("You avoid triggering a blade trap.");
            else if (random2limit(you.evasion(), 40)
                     + random2(6) + (trig_knows ? 3 : 0) > 8)
            {
                mpr("A huge blade swings just past you!");
            }
            else
            {
                mpr("A huge blade swings out and slices into you!");
                const int damage = you.apply_ac(48 + random2avg(29, 2));
                string n = name(DESC_A);
                ouch(damage, KILLED_BY_TRAP, MID_NOBODY, n.c_str());
                bleed_onto_floor(you.pos(), MONS_PLAYER, damage, true);
            }
        }
        else if (m)
        {
            if (one_chance_in(5) || (trig_knows && coinflip()))
            {
                // Trap doesn't trigger. Don't reveal it.
                if (you_know)
                {
                    simple_monster_message(*m,
                                           " fails to trigger a blade trap.");
                }
                else
                    hide();
            }
            else if (random2(m->evasion()) > 8
                     || (trig_knows && random2(m->evasion()) > 8))
            {
                if (in_sight
                    && !simple_monster_message(*m,
                                            " avoids a huge, swinging blade."))
                {
                    mpr("A huge blade swings out!");
                }
            }
            else
            {
                if (in_sight)
                {
                    string msg = "A huge blade swings out";
                    if (m->visible_to(&you))
                    {
                        msg += " and slices into ";
                        msg += m->name(DESC_THE);
                    }
                    msg += "!";
                    mpr(msg);
                }

                int damage_taken = m->apply_ac(10 + random2avg(29, 2));

                if (!m->is_summoned())
                    bleed_onto_floor(m->pos(), m->type, damage_taken, true);

                m->hurt(nullptr, damage_taken);
                if (in_sight && m->alive())
                    print_wounds(*m);
            }
        }
        break;
		
#if TAG_MAJOR_VERSION == 34
    case TRAP_NET:
        if (you_trigger)
        {
			mpr("A net swings high above you.");
            trap_destroyed = true;
        }
        else if (m)
        {
            if (you_know)
                simple_monster_message(*m, " fails to trigger a net trap.");
            else
                hide();
		}
        break;
#endif

    case TRAP_WEB:
        if (triggerer.body_size(PSIZE_BODY) >= SIZE_GIANT)
        {
            trap_destroyed = true;
            if (you_trigger)
                mprf("You tear through %s web.", you_know ? "the" : "a");
            else if (m)
                simple_monster_message(*m, " tears through a web.");
            break;
        }

        if (triggerer.is_web_immune())
        {
            if (m)
            {
                if (m->is_insubstantial())
                    simple_monster_message(*m, " passes through a web.");
                else if (mons_genus(m->type) == MONS_JELLY)
                    simple_monster_message(*m, " oozes through a web.");
                // too spammy for spiders, and expected
            }
            break;
        }

        if (you_trigger)
        {
            if (trig_knows && one_chance_in(3))
                mpr("You pick your way through the web.");
            else
            {
                mpr("You are caught in the web!");

                if (_player_caught_in_web())
                {
                    check_monsters_sense(SENSE_WEB_VIBRATION, 9, you.pos());
                    if (player_in_a_dangerous_place())
                        xom_is_stimulated(50);
                }
            }
        }
        else if (m)
        {
            if (one_chance_in(3) || (trig_knows && coinflip()))
            {
                // Not triggered, trap stays.
                if (you_know)
                    simple_monster_message(*m, " evades a web.");
                else
                    hide();
            }
            else
            {
                // Triggered and hit.
                if (in_sight)
                {
                    if (m->visible_to(&you))
                        simple_monster_message(*m, " is caught in a web!");
                    else
                        mpr("A web moves frantically as something is caught in it!");
                }

                // If somehow already caught, make it worse.
                m->add_ench(ENCH_HELD);

                // Don't try to escape the web in the same turn
                m->props[NEWLY_TRAPPED_KEY] = true;

                // Alert monsters.
                check_monsters_sense(SENSE_WEB_VIBRATION, 9, triggerer.position);
            }
        }
        break;

    case TRAP_ZOT:
        if (you_trigger)
        {
            mpr((trig_knows) ? "You enter the Zot trap."
                             : "Oh no! You have blundered into a Zot trap!");
            if (!trig_knows)
                xom_is_stimulated(25);

            MiscastEffect(&you, nullptr, ZOT_TRAP_MISCAST, SPTYP_RANDOM,
                           3, name(DESC_A));
        }
        else if (m)
        {
            // Zot traps are out to get *the player*! Hostile monsters
            // benefit and friendly monsters suffer. Such is life.

            // The old code rehid the trap, but that's pure interface screw
            // in 99% of cases - a player can just watch who stepped where
            // and mark the trap on an external paper map. Not good.

            actor* targ = nullptr;
            if (you.see_cell_no_trans(pos))
            {
                if (m->wont_attack() || crawl_state.game_is_arena())
                    targ = m;
                else if (one_chance_in(5))
                    targ = &you;
            }

            // Give the player a chance to figure out what happened
            // to their friend.
            if (player_can_hear(pos) && (!targ || !in_sight))
            {
                mprf(MSGCH_SOUND, "You hear a %s \"Zot\"!",
                     in_sight ? "loud" : "distant");
            }

            if (targ)
            {
                if (in_sight)
                {
                    mprf("The power of Zot is invoked against %s!",
                         targ->name(DESC_THE).c_str());
                }
                MiscastEffect(targ, nullptr, ZOT_TRAP_MISCAST, SPTYP_RANDOM,
                              3, "the power of Zot");
            }
        }
        break;

    case TRAP_SHAFT:
        // Unknown shafts are traps triggered by walking onto them.
        // Known shafts are used as escape hatches

        // Paranoia
        if (!is_valid_shaft_level())
        {
            if (you_know && in_sight)
                mpr("The shaft disappears in a puff of logic!");

            trap_destroyed = true;
            break;
        }

        // If the shaft isn't known, don't reveal it.
        // The shafting code in downstairs() needs to know
        // whether it's undiscovered.
        if (!you_know)
            hide();

        // Known shafts don't trigger as traps.
        if (trig_knows)
            break;

        // A chance to escape.
        if (one_chance_in(4))
            break;

        {
        // keep this for messaging purposes
        const bool triggerer_seen = you.can_see(triggerer);

        // Fire away!
        triggerer.do_shaft();

        // Player-used shafts are destroyed
        // after one use in down_stairs(), misc.cc
        if (!you_trigger)
        {
            if (in_sight)
            {
                mprf("%s shaft crumbles and collapses.",
                     triggerer_seen ? "The" : "A");
                know_trap_destroyed = true;
            }
            trap_destroyed = true;
        }
        }
        break;

#if TAG_MAJOR_VERSION == 34
    case TRAP_GAS:
        if (in_sight && you_know)
            mpr("The gas trap seems to be inoperative.");
        trap_destroyed = true;
        break;
#endif

    case TRAP_PLATE:
        dungeon_events.fire_position_event(DET_PRESSURE_PLATE, pos);
        break;

#if TAG_MAJOR_VERSION == 34
    case TRAP_SHADOW:
    case TRAP_SHADOW_DORMANT:
#endif
    default:
        break;
    }

    if (you_trigger)
        learned_something_new(HINT_SEEN_TRAP, p);

    if (trap_destroyed)
        destroy(know_trap_destroyed);
}

int trap_def::max_damage(const actor& act)
{
    // Trap damage to monsters is a lot smaller, because they are fairly
    // stupid and tend to have fewer hp than players -- this choice prevents
    // traps from easily killing large monsters.
    bool mon = act.is_monster();

    switch (type)
    {
        case TRAP_NEEDLE: return 0;
        case TRAP_ARROW:  return mon ?  7 : 15;
        case TRAP_SPEAR:  return mon ? 10 : 26;
        case TRAP_BOLT:   return mon ? 18 : 40;
        case TRAP_BLADE:  return mon ? 38 : 76;
        default:          return 0;
    }

    return 0;
}

int trap_def::shot_damage(actor& act)
{
    const int dam = max_damage(act);

    if (!dam)
        return 0;
    return random2(dam) + 1;
}

int trap_def::difficulty()
{
    switch (type)
    {
    // To-hit:
    case TRAP_ARROW:
        return 7;
    case TRAP_SPEAR:
        return 10;
    case TRAP_BOLT:
        return 15;
    case TRAP_NEEDLE:
        return 8;
    // Irrelevant:
    default:
        return 0;
    }
}

int reveal_traps(const int range)
{
    int traps_found = 0;

    for (auto& entry : env.trap)
    {
        trap_def& trap = entry.second;
        if (!trap.active())
            continue;
        if (grid_distance(you.pos(), trap.pos) < range && !trap.is_known())
        {
            traps_found++;
            trap.reveal();
            env.map_knowledge(trap.pos).set_feature(grd(trap.pos), 0, trap.type);
            set_terrain_mapped(trap.pos);
        }
    }

    return traps_found;
}

void destroy_trap(const coord_def& pos)
{
    if (trap_def* ptrap = trap_at(pos))
        ptrap->destroy();
}

trap_def* trap_at(const coord_def& pos)
{
    if (!feat_is_trap(grd(pos), true))
        return nullptr;

    auto it = env.trap.find(pos);
    ASSERT(it != env.trap.end());
    ASSERT(it->second.pos == pos);
    ASSERT(it->second.type != TRAP_UNASSIGNED);

    return &it->second;
}

trap_type get_trap_type(const coord_def& pos)
{
    if (trap_def* ptrap = trap_at(pos))
        return ptrap->type;

    return TRAP_UNASSIGNED;
}

void search_around()
{
    ASSERT(!crawl_state.game_is_arena());
	
    int max_dist = 5;

    for (radius_iterator ri(you.pos(), max_dist, C_SQUARE, LOS_NO_TRANS); ri; ++ri)
    {
        if (grd(*ri) != DNGN_UNDISCOVERED_TRAP)
            continue;

        trap_def* ptrap = trap_at(*ri);
        if (!ptrap)
        {
            // Maybe we shouldn't kill the trap for debugging
            // purposes - oh well.
            grd(*ri) = DNGN_FLOOR;
            dprf("You found a buggy trap! It vanishes!");
            continue;
        }
        ptrap->reveal();
        mprf("You found %s!",
            ptrap->name(DESC_A).c_str());
        learned_something_new(HINT_SEEN_TRAP, *ri);
        
    }
}

/**
 * End the ATTR_HELD state & redraw appropriate UI.
 *
 * Do NOT call without clearing up nets, webs, etc first!
 */
void stop_being_held()
{
    you.attribute[ATTR_HELD] = 0;
    you.redraw_quiver = true;
    you.redraw_evasion = true;
}

/**
 * Exit a web that's currently holding you.
 *
 * @param quiet     Whether to squash messages.
 */
void leave_web(bool quiet)
{
    const trap_def *trap = trap_at(you.pos());
    if (!trap || trap->type != TRAP_WEB)
        return;

    if (trap->ammo_qty == 1) // temp web from e.g. jumpspider/spidersack
    {
        if (!quiet)
            mpr("The web tears apart.");
        destroy_trap(you.pos());
    }
    else if (!quiet)
        mpr("You disentangle yourself.");

    stop_being_held();
}

/**
 * Let the player attempt to unstick themself from a web.
 */
static void _free_self_from_web()
{
    // Check if there's actually a web trap in your tile.
    trap_def *trap = trap_at(you.pos());
    if (trap && trap->type == TRAP_WEB)
    {
        // if so, roll a chance to escape the web (based on str).
        if (x_chance_in_y(40 - you.stat(STAT_STR), 66))
        {
            mpr("You struggle to detach yourself from the web.");
            // but you actually accomplished nothing!
            return;
        }

        leave_web();
    }

    // whether or not there was a web trap there, you're free now.
    stop_being_held();
}

void free_self_from_net()
{
    const int net = get_trapping_net(you.pos());

    if (net == NON_ITEM)
    {
        // If there's no net, it must be a web.
        _free_self_from_web();
        return;
    }

    int hold = mitm[net].net_durability;
    dprf("net.net_durability: %d", hold);

    const int damage = 1 + random2(4);

    hold -= damage;
    mitm[net].net_durability = hold;

    if (hold < NET_MIN_DURABILITY)
    {
        mprf("You %s the net and break free!", damage > 3 ? "shred" : "rip");

        destroy_item(net);
        stop_being_held();
        return;
    }

    if (damage > 3)
        mpr("You tear a large gash into the net.");
    else
        mpr("You struggle against the net.");
}

/**
 * Deals with messaging & cleanup for temporary web traps. Does not actually
 * delete ENCH_HELD!
 *
 * @param mons      The monster leaving a web.
 * @param quiet     Whether to suppress messages.
 */
void monster_web_cleanup(const monster &mons, bool quiet)
{
    trap_def *trap = trap_at(mons.pos());
    if (trap && trap->type == TRAP_WEB)
    {
        if (trap->ammo_qty == 1)
        {
            // temp web from e.g. jumpspider/spidersack
            if (!quiet)
                simple_monster_message(mons, " tears the web.");
            destroy_trap(mons.pos());
        }
        else if (!quiet)
            simple_monster_message(mons, " pulls away from the web.");
    }
}

void mons_clear_trapping_net(monster* mon)
{
    if (!mon->caught())
        return;

    const int net = get_trapping_net(mon->pos());
    if (net != NON_ITEM)
        free_stationary_net(net);

    mon->del_ench(ENCH_HELD, true);
}

void free_stationary_net(int item_index)
{
    return;
}

void clear_trapping_net()
{
    if (!you.attribute[ATTR_HELD])
        return;

    if (!in_bounds(you.pos()))
        return;

    const int net = get_trapping_net(you.pos());
    if (net == NON_ITEM)
        leave_web(true);
    else
        free_stationary_net(net);

    stop_being_held();
}

item_def trap_def::generate_trap_item()
{
    item_def item;
    object_class_type base;
    int sub;

    switch (type)
    {
#if TAG_MAJOR_VERSION == 34
    case TRAP_DART:   base = OBJ_MISSILES; sub = MI_DART;          break;
#endif
    case TRAP_ARROW:  base = OBJ_MISSILES; sub = MI_ARROW;         break;
    case TRAP_BOLT:   base = OBJ_MISSILES; sub = MI_BOLT;          break;
    case TRAP_SPEAR:  base = OBJ_WEAPONS;  sub = WPN_SPEAR;        break;
    case TRAP_NEEDLE: base = OBJ_MISSILES; sub = MI_DART_POISONED; break;
#if TAG_MAJOR_VERSION == 34
    case TRAP_NET:    base = OBJ_MISSILES; sub = MI_THROWING_NET;  break;
#endif
    default:          return item;
    }

    item.base_type = base;
    item.sub_type  = sub;
    item.quantity  = 1;

    if (base != OBJ_MISSILES)
        set_item_ego_type(item, base, SPWPN_NORMAL);

    item_colour(item);
    return item;
}

// Shoot a single piece of ammo at the relevant actor.
void trap_def::shoot_ammo(actor& act, bool was_known)
{
    if (ammo_qty <= 0)
    {
        if (was_known && act.is_player())
            mpr("The trap is out of ammunition!");
        else if (player_can_hear(pos) && you.see_cell(pos))
            mpr("You hear a soft click.");

        destroy();
        return;
    }

    if (act.is_player())
    {
        if (one_chance_in(5) || was_known && !one_chance_in(4))
        {
            mprf("You avoid triggering %s.", name(DESC_A).c_str());
            return;
        }
    }
    else if (one_chance_in(5))
    {
        if (was_known && you.see_cell(pos) && you.can_see(act))
        {
            mprf("%s avoids triggering %s.", act.name(DESC_THE).c_str(),
                 name(DESC_A).c_str());
        }
        return;
    }

    item_def shot = generate_trap_item();

    int trap_hit = (20 + (difficulty()*2)) * random2(200) / 100;
    if (int defl = act.missile_deflection())
        trap_hit = random2(trap_hit / defl);

    const int con_block = random2(20 + act.shield_block_penalty());
    const int pro_block = act.shield_bonus();
    dprf("%s: hit %d EV %d, shield hit %d block %d", name(DESC_PLAIN).c_str(),
         trap_hit, act.evasion(), con_block, pro_block);

    // Determine whether projectile hits.
    if (trap_hit < act.evasion())
    {
        if (act.is_player())
            mprf("%s shoots out and misses you.", shot.name(DESC_A).c_str());
        else if (you.see_cell(act.pos()))
        {
            mprf("%s misses %s!", shot.name(DESC_A).c_str(),
                 act.name(DESC_THE).c_str());
        }
    }
    else if (pro_block >= con_block
             && you.see_cell(act.pos()))
    {
        string owner;
        if (act.is_player())
            owner = "your";
        else if (you.can_see(act))
            owner = apostrophise(act.name(DESC_THE));
        else // "its" sounds abysmal; animals don't use shields
            owner = "someone's";
        mprf("%s shoots out and hits %s shield.", shot.name(DESC_A).c_str(),
             owner.c_str());

        act.shield_block_succeeded(0);
    }
    else // OK, we've been hit.
    {
        bool force_poison = (env.markers.property_at(pos, MAT_ANY,
                                "poisoned_needle_trap") == "true");

        bool poison = (type == TRAP_NEEDLE
                       && (x_chance_in_y(50 - (3*act.armour_class()) / 2, 100)
                            || force_poison));

        int damage_taken = act.apply_ac(shot_damage(act));

        if (act.is_player())
        {
            mprf("%s shoots out and hits you!", shot.name(DESC_A).c_str());

            string n = name(DESC_A);

            // Needle traps can poison.
            if (poison)
                poison_player(1 + roll_dice(2, 9), "", n);

            ouch(damage_taken, KILLED_BY_TRAP, MID_NOBODY, n.c_str());
        }
        else
        {
            if (you.see_cell(act.pos()))
            {
                mprf("%s hits %s%s!",
                     shot.name(DESC_A).c_str(),
                     act.name(DESC_THE).c_str(),
                     (damage_taken == 0 && !poison) ?
                         ", but does no damage" : "");
            }

            if (poison)
                act.poison(nullptr, 3 + roll_dice(2, 5));
            act.hurt(nullptr, damage_taken);
        }
    }
    ammo_qty--;
}

// returns appropriate trap symbol
dungeon_feature_type trap_def::category() const
{
    return trap_category(type);
}

dungeon_feature_type trap_category(trap_type type)
{
    switch (type)
    {
    case TRAP_WEB:
        return DNGN_TRAP_WEB;
    case TRAP_SHAFT:
        return DNGN_TRAP_SHAFT;

    case TRAP_TELEPORT:
    case TRAP_TELEPORT_PERMANENT:
        return DNGN_TRAP_TELEPORT;
    case TRAP_ALARM:
        return DNGN_TRAP_ALARM;
    case TRAP_ZOT:
        return DNGN_TRAP_ZOT;
    case TRAP_GOLUBRIA:
        return DNGN_PASSAGE_OF_GOLUBRIA;
#if TAG_MAJOR_VERSION == 34
    case TRAP_SHADOW:
        return DNGN_TRAP_SHADOW;
    case TRAP_SHADOW_DORMANT:
        return DNGN_TRAP_SHADOW_DORMANT;
#endif

    case TRAP_ARROW:
    case TRAP_SPEAR:
    case TRAP_BLADE:
    case TRAP_BOLT:
    case TRAP_NEEDLE:
#if TAG_MAJOR_VERSION == 34
    case TRAP_NET:
    case TRAP_GAS:
    case TRAP_DART:
#endif
    case TRAP_PLATE:
        return DNGN_TRAP_MECHANICAL;

    default:
        die("placeholder trap type %d used", type);
    }
}

/***
 * Can a shaft be placed on the current level?
 *
 * @param known Whether the potential shaft will be known to the player
 *              (default false). During level-generation, unknown shafts
 *              are not permitted on certain levels; at any other time,
 *              this argument is ignored.
 * @returns true if such a shaft can be placed.
 */
bool is_valid_shaft_level(bool known)
{
    // Important: We are sometimes called before the level has been loaded
    // or generated, so should not depend on properties of the level itself,
    // but only on its level_id.
    const level_id place = level_id::current();
    if (crawl_state.test
        || crawl_state.game_is_sprint())
    {
        return false;
    }

    if (!is_connected_branch(place))
        return false;

    const Branch &branch = branches[place.branch];

    if (env.turns_on_level == -1
        && branch.branch_flags & BFLAG_NO_SHAFTS)
    {
        return false;
    }

    // When generating levels, don't place an unknown shaft on the level
    // immediately above the bottom of a branch if that branch is
    // significantly more dangerous than normal.
    int min_delta = 1;
    if (!known && env.turns_on_level == -1
        && branch.branch_flags & BFLAG_DANGEROUS_END)
    {
        min_delta = 2;
    }

    return (brdepth[place.branch] - place.depth) >= min_delta;
}

static level_id _generic_shaft_dest(level_pos lpos, bool known = false)
{
    level_id lid = lpos.id;

    if (!is_connected_branch(lid))
        return lid;

    int curr_depth = lid.depth;
    int max_depth = brdepth[lid.branch];

	//shaft traps always drop you 1 level only
    lid.depth += 1;

    if (lid.depth > max_depth)
        lid.depth = max_depth;

    if (lid.depth == curr_depth)
        return lid;

    // Only shafts on the level immediately above a dangerous branch
    // bottom will take you to that dangerous bottom, and shafts can't
    // be created during level generation time.
    if (branches[lid.branch].branch_flags & BFLAG_DANGEROUS_END
        && lid.depth == max_depth
        && (max_depth - curr_depth) > 1)
    {
        lid.depth--;
    }

    return lid;
}

level_id generic_shaft_dest(coord_def pos, bool known = false)
{
    return _generic_shaft_dest(level_pos(level_id::current(), pos));
}

/**
 * Get a number of traps to place on the current level.
 *
 * No traps are placed in either Temple or disconnected branches other than
 * Pandemonium. For other branches, we place 0-2 traps per level, averaged over
 * two dice. This value is increased for deeper levels; roughly one additional
 * trap for every 10 levels of absdepth, capping out at max 9 traps in a level.
 *
 * @return  A number of traps to be placed.
*/
int num_traps_for_place()
{
    return 0;
}

/**
 * Choose a weighted random trap type for the currently-generated level.
 *
 * Odds of generating zot traps vary by depth (and are depth-limited). Alarm
 * traps also can't be placed before D:4. All other traps are depth-agnostic.
 *
 * @return                    A random trap type.
 *                            May be NUM_TRAPS, if no traps were valid.
 */

trap_type random_trap_for_place()
{
    // zot traps are Very Special.
    // very common in zot...
    if (player_in_branch(BRANCH_ZOT) && coinflip())
        return TRAP_ZOT;

    // and elsewhere, increasingly common with depth
    // possible starting at depth 15 (end of D, late lair, lair branches)
    // XXX: is there a better way to express this?
    if (random2(1 + env.absdepth0) > 14 && one_chance_in(3))
        return TRAP_ZOT;

    const bool shaft_ok = is_valid_shaft_level();
    const bool tele_ok = !crawl_state.game_is_sprint();
    const bool alarm_ok = env.absdepth0 > 3;

    const pair<trap_type, int> trap_weights[] =
    {
        { TRAP_TELEPORT, tele_ok  ? 2 : 0},
        { TRAP_SHAFT,   shaft_ok  ? 1 : 0},
        { TRAP_ALARM,   alarm_ok  ? 1 : 0},
    };

    const trap_type *trap = random_choose_weighted(trap_weights);
    return trap ? *trap : NUM_TRAPS;
}

/**
 * Oldstyle trap algorithm, used for vaults. Very bad. Please remove ASAP.
 */
trap_type random_vault_trap()
{
    const int level_number = env.absdepth0;
    trap_type type = TRAP_ARROW;

    if ((random2(1 + level_number) > 1) && one_chance_in(4))
        type = TRAP_NEEDLE;
    if (random2(1 + level_number) > 3)
        type = TRAP_SPEAR;

    if (random2(1 + level_number) > 7)
        type = TRAP_BOLT;
    if (random2(1 + level_number) > 14)
        type = TRAP_BLADE;

    if (random2(1 + level_number) > 14 && one_chance_in(3)
        || (player_in_branch(BRANCH_ZOT) && coinflip()))
    {
        type = TRAP_ZOT;
    }

    if (one_chance_in(20) && is_valid_shaft_level())
        type = TRAP_SHAFT;
    if (one_chance_in(20) && !crawl_state.game_is_sprint())
        type = TRAP_TELEPORT;
    if (one_chance_in(40) && level_number > 3)
        type = TRAP_ALARM;

    return type;
}

int count_traps(trap_type ttyp)
{
    int num = 0;
    for (const auto& entry : env.trap)
        if (entry.second.type == ttyp)
            num++;
    return num;
}

void place_webs(int num)
{
    trap_def ts;
    for (int j = 0; j < num; j++)
    {
        int tries;
        // this is hardly ever enough to place many webs, most of the time
        // it will fail prematurely. Which is fine.
        for (tries = 0; tries < 200; ++tries)
        {
            ts.pos.x = random2(GXM);
            ts.pos.y = random2(GYM);
            if (in_bounds(ts.pos)
                && grd(ts.pos) == DNGN_FLOOR
                && !map_masked(ts.pos, MMT_NO_TRAP))
            {
                // Calculate weight.
                int weight = 0;
                for (adjacent_iterator ai(ts.pos); ai; ++ai)
                {
                    // Solid wall?
                    int solid_weight = 0;
                    // Orthogonals weight three, diagonals 1.
                    if (cell_is_solid(*ai))
                    {
                        solid_weight = (ai->x == ts.pos.x || ai->y == ts.pos.y)
                                        ? 3 : 1;
                    }
                    weight += solid_weight;
                }

                // Maximum weight is 4*3+4*1 = 16
                // *But* that would imply completely surrounded by rock (no point there)
                if (weight <= 16 && x_chance_in_y(weight + 2, 34))
                    break;
            }
        }

        if (tries >= 200)
            break;

        ts.type = TRAP_WEB;
        grd(ts.pos) = DNGN_UNDISCOVERED_TRAP;
        ts.prepare_ammo();
        // Reveal some webs
        if (coinflip())
            ts.reveal();
        env.trap[ts.pos] = ts;
    }
}

bool ensnare(actor *fly)
{
    ASSERT(fly); // XXX: change to actor &fly
    if (fly->is_web_immune())
        return false;

    if (fly->caught())
    {
        // currently webs are stateless so except for flavour it's a no-op
        if (fly->is_player())
            mpr("You are even more entangled.");
        return false;
    }

    if (fly->body_size() >= SIZE_GIANT)
    {
        if (you.can_see(*fly))
            mprf("A web harmlessly splats on %s.", fly->name(DESC_THE).c_str());
        return false;
    }

    // If we're over water, an open door, shop, portal, etc, the web will
    // fail to attach and you'll be released after a single turn.
    if (grd(fly->pos()) == DNGN_FLOOR)
    {
        place_specific_trap(fly->pos(), TRAP_WEB, 1); // 1 ammo = destroyed on exit (hackish)
        if (grd(fly->pos()) == DNGN_UNDISCOVERED_TRAP
            && you.see_cell(fly->pos()))
        {
            grd(fly->pos()) = DNGN_TRAP_WEB;
        }
    }

    if (fly->is_player())
    {
        if (_player_caught_in_web()) // no fail, returns false if already held
            mpr("You are caught in a web!");
    }
    else
    {
        simple_monster_message(*fly->as_monster(), " is caught in a web!");
        fly->as_monster()->add_ench(ENCH_HELD);
    }

    // Drowned?
    if (!fly->alive())
        return true;

    check_monsters_sense(SENSE_WEB_VIBRATION, 9, fly->pos());
    return true;
}
