#include "AppHdr.h"

#include "target.h"

#include <cmath>
#include <utility> // swap

#include "cloud.h"
#include "coord.h"
#include "coordit.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "fight.h"
#include "god-abil.h"
#include "libutil.h"
#include "los-def.h"
#include "losglobal.h"
#include "mon-tentacle.h"
#include "spl-damage.h"
#include "spl-goditem.h" // player_is_debuffable
#include "spl-other.h"
#include "stringutil.h"
#include "terrain.h"

#define notify_fail(x) (why_not = (x), false)

static string _wallmsg(coord_def c)
{
    ASSERT(map_bounds(c)); // there'd be an information leak
    const char *wall = feat_type_name(grd(c));
    return "There is " + article_a(wall) + " there.";
}

bool targeter::set_aim(coord_def a)
{
    // This matches a condition in direction_chooser::move_is_ok().
    if (agent && !can_affect_unseen() &&
            !cell_see_cell(agent->pos(), a, LOS_NO_TRANS))
    {
        return false;
    }

    aim = a;
    return true;
}

bool targeter::can_affect_outside_range()
{
    return false;
}

bool targeter::can_affect_unseen()
{
    return false;
}

bool targeter::can_affect_walls()
{
    return false;
}

bool targeter::anyone_there(coord_def loc)
{
    if (!map_bounds(loc))
        return false;
    if (agent && agent->is_player())
        return env.map_knowledge(loc).monsterinfo();
    return actor_at(loc);
}

bool targeter::has_additional_sites(coord_def /*loc*/)
{
    return false;
}

bool targeter::affects_monster(const monster_info& /*mon*/)
{
    return true; //TODO: false
}

targeter_beam::targeter_beam(const actor *act, int r, zap_type zap,
                               int pow, int min_ex_rad, int max_ex_rad) :
                               min_expl_rad(min_ex_rad),
                               max_expl_rad(max_ex_rad),
                               range(r)
{
    ASSERT(act);
    ASSERT(min_ex_rad >= 0);
    ASSERT(max_ex_rad >= 0);
    ASSERT(max_ex_rad >= min_ex_rad);
    agent = act;
    beam.set_agent(act);
    origin = aim = act->pos();
    beam.attitude = ATT_FRIENDLY;
    zappy(zap, pow, false, beam);
    beam.is_tracer = true;
    beam.is_targeting = true;
    beam.range = range;
    beam.source = origin;
    beam.target = aim;
    beam.dont_stop_player = true;
    beam.friend_info.dont_stop = true;
    beam.foe_info.dont_stop = true;
    beam.ex_size = min_ex_rad;
    beam.aimed_at_spot = true;

    penetrates_targets = beam.pierce;
}

bool targeter_beam::set_aim(coord_def a)
{
    if (!targeter::set_aim(a))
        return false;

    bolt tempbeam = beam;

    tempbeam.target = aim;
    tempbeam.path_taken.clear();
    tempbeam.fire();
    path_taken = tempbeam.path_taken;

    if (max_expl_rad > 0)
        set_explosion_aim(beam);

    return true;
}

void targeter_beam::set_explosion_aim(bolt tempbeam)
{
    set_explosion_target(tempbeam);
    tempbeam.use_target_as_pos = true;
    exp_map_min.init(INT_MAX);
    tempbeam.determine_affected_cells(exp_map_min, coord_def(), 0,
                                      min_expl_rad, true, true);
    exp_map_max.init(INT_MAX);
    tempbeam.determine_affected_cells(exp_map_max, coord_def(), 0,
                                      max_expl_rad, true, true);
}

void targeter_beam::set_explosion_target(bolt &tempbeam)
{
    tempbeam.target = origin;
    for (auto c : path_taken)
    {
        if (cell_is_solid(c) && !tempbeam.can_affect_wall(c))
            break;
        tempbeam.target = c;
        if (anyone_there(c) && !tempbeam.ignores_monster(monster_at(c)))
            break;
    }
}

bool targeter_beam::valid_aim(coord_def a)
{
    if (a != origin && !can_affect_unseen() &&
            !cell_see_cell(origin, a, LOS_NO_TRANS))
    {
        if (agent->see_cell(a))
            return notify_fail("There's something in the way.");
        return notify_fail("You cannot see that place.");
    }
    if ((origin - a).rdist() > range)
        return notify_fail("Out of range.");
    return true;
}

bool targeter_beam::can_affect_outside_range()
{
    // XXX is this everything?
    return max_expl_rad > 0;
}

aff_type targeter_beam::is_affected(coord_def loc)
{
    bool on_path = false;
    int visit_count = 0;
    coord_def c;
    aff_type current = AFF_YES;
    for (auto pc : path_taken)
    {
        if (cell_is_solid(pc)
            && !beam.can_affect_wall(pc)
            && max_expl_rad > 0)
        {
            break;
        }

        c = pc;
        if (c == loc)
        {
            visit_count++;
            if (max_expl_rad > 0)
                on_path = true;
            else if (cell_is_solid(pc))
            {
                bool res = beam.can_affect_wall(pc);
                if (res)
                    return current;
                else
                    return AFF_NO;

            }
            else
                continue;
        }
        if (anyone_there(pc)
            && !penetrates_targets
            && !beam.ignores_monster(monster_at(pc)))
        {
            // We assume an exploding spell will always stop here.
            if (max_expl_rad > 0)
                break;
            current = AFF_MAYBE;
        }
    }
    if (max_expl_rad > 0)
    {
        if ((loc - c).rdist() <= 9)
        {
            bool aff_wall = beam.can_affect_wall(loc);
            if (!cell_is_solid(loc) || aff_wall)
            {
                coord_def centre(9,9);
                if (exp_map_min(loc - c + centre) < INT_MAX)
                {
                    return (!cell_is_solid(loc) || aff_wall)
                           ? AFF_YES : AFF_MAYBE;
                }
                if (exp_map_max(loc - c + centre) < INT_MAX)
                    return AFF_MAYBE;
            }
        }
        else
            return on_path ? AFF_TRACER : AFF_NO;
    }

    return visit_count == 0 ? AFF_NO :
           visit_count == 1 ? AFF_YES :
                              AFF_MULTIPLE;
}

bool targeter_beam::affects_monster(const monster_info& mon)
{
    //XXX: this is a disgusting hack that probably leaks information!
    //     bolt::is_harmless (and transitively, bolt::nasty_to) should
    //     take monster_infos instead.
    const monster* m = monster_at(mon.pos);
    if (!m)
        return false;

    if (beam.is_enchantment() && beam.has_saving_throw()
        && mon.res_magic() == MAG_IMMUNE)
    {
        return false;
    }

    // beckoning is useless against adjacent mons!
    // XXX: this should probably be somewhere else
    if (beam.flavour == BEAM_BECKONING)
        return grid_distance(mon.pos, you.pos()) > 1;

    // Inner flame doesn't affect regular summons
    if (beam.flavour == BEAM_INNER_FLAME && m->is_summoned()
        && !m->is_illusion())
    {
        return false;
    }

    return !beam.is_harmless(m) || beam.nice_to(mon)
    // Inner flame affects allies without harming or helping them.
           || beam.flavour == BEAM_INNER_FLAME && !m->is_summoned();
}

targeter_unravelling::targeter_unravelling(const actor *act, int r, int pow)
    : targeter_beam(act, r, ZAP_UNRAVELLING, pow, 1, 1)
{
}

/**
 * Will a casting of Violent Unravelling explode a target at the given loc?
 *
 * @param c     The location in question.
 * @return      Whether, to the player's knowledge, there's a valid target for
 *              Violent Unravelling at the given coordinate.
 */
static bool unravelling_explodes_at(const coord_def c)
{
    if (you.pos() == c && player_is_debuffable())
        return true;

    const monster_info* mi = env.map_knowledge(c).monsterinfo();
    return mi && mi->debuffable();
}

bool targeter_unravelling::set_aim(coord_def a)
{
    if (!targeter::set_aim(a))
        return false;

    bolt tempbeam = beam;

    tempbeam.target = aim;
    tempbeam.path_taken.clear();
    tempbeam.fire();
    path_taken = tempbeam.path_taken;

    bolt explosion_beam = beam;
    set_explosion_target(beam);
    if (unravelling_explodes_at(beam.target))
        min_expl_rad = 1;
    else
        min_expl_rad = 0;

    set_explosion_aim(beam);

    return true;
}

targeter_view::targeter_view()
{
    origin = aim = you.pos();
}

bool targeter_view::valid_aim(coord_def /*a*/)
{
    return true; // don't reveal map bounds
}

aff_type targeter_view::is_affected(coord_def loc)
{
    if (loc == aim)
        return AFF_YES;

    return AFF_NO;
}

targeter_smite::targeter_smite(const actor* act, int ran,
                                 int exp_min, int exp_max, bool wall_ok,
                                 bool (*affects_pos_func)(const coord_def &)):
    exp_range_min(exp_min), exp_range_max(exp_max), range(ran),
    affects_walls(wall_ok), affects_pos(affects_pos_func)
{
    ASSERT(act);
    ASSERT(exp_min >= 0);
    ASSERT(exp_max >= 0);
    ASSERT(exp_min <= exp_max);
    agent = act;
    origin = aim = act->pos();
}

bool targeter_smite::valid_aim(coord_def a)
{
    if (a != origin && !cell_see_cell(origin, a, LOS_NO_TRANS))
    {
        // Scrying/glass/tree/grate.
        if (agent && agent->see_cell(a))
            return notify_fail("There's something in the way.");
        return notify_fail("You cannot see that place.");
    }
    if ((origin - a).rdist() > range)
        return notify_fail("Out of range.");
    if (!affects_walls && cell_is_solid(a))
        return notify_fail(_wallmsg(a));
    return true;
}

bool targeter_smite::set_aim(coord_def a)
{
    if (!targeter::set_aim(a))
        return false;

    if (exp_range_max > 0)
    {
        bolt beam;
        beam.target = a;
        beam.use_target_as_pos = true;
        exp_map_min.init(INT_MAX);
        beam.determine_affected_cells(exp_map_min, coord_def(), 0,
                                      exp_range_min, true, true);
        exp_map_max.init(INT_MAX);
        beam.determine_affected_cells(exp_map_max, coord_def(), 0,
                                      exp_range_max, true, true);
    }
    return true;
}

bool targeter_smite::can_affect_outside_range()
{
    // XXX is this everything?
    return exp_range_max > 0;
}

bool targeter_smite::can_affect_walls()
{
    return affects_walls;
}

aff_type targeter_smite::is_affected(coord_def loc)
{
    if (!valid_aim(aim))
        return AFF_NO;

    if (affects_pos && !affects_pos(loc))
        return AFF_NO;

    if (loc == aim)
        return AFF_YES;

    if (exp_range_max <= 0)
        return AFF_NO;

    if ((loc - aim).rdist() > 9)
        return AFF_NO;
    coord_def centre(9,9);
    if (exp_map_min(loc - aim + centre) < INT_MAX)
        return AFF_YES;
    if (exp_map_max(loc - aim + centre) < INT_MAX)
        return AFF_MAYBE;

    return AFF_NO;
}

targeter_walljump::targeter_walljump() :
    targeter_smite(&you, LOS_RADIUS, 1, 1, false, nullptr)
{
}

bool targeter_walljump::valid_aim(coord_def a)
{
    return wu_jian_can_wall_jump(a, why_not);
}

aff_type targeter_walljump::is_affected(coord_def loc)
{
    if (!valid_aim(aim))
        return AFF_NO;

    auto wall_jump_direction = (you.pos() - aim).sgn();
    auto wall_jump_landing_spot = (you.pos() + wall_jump_direction
                                   + wall_jump_direction);
    if (loc == wall_jump_landing_spot)
        return AFF_YES;

    if (loc.distance_from(wall_jump_landing_spot) == 1 && monster_at(loc))
        return AFF_YES;

    return AFF_NO;
}

targeter_passwall::targeter_passwall(int max_range) :
    targeter_smite(&you, max_range, 1, 1, true, nullptr)
{
}

bool targeter_passwall::valid_aim(coord_def a)
{
    passwall_path tmp_path(you, a - you.pos(), range);
    string failmsg;
    tmp_path.is_valid(&failmsg);
    if (!tmp_path.spell_succeeds())
        return notify_fail(failmsg);
    return true;
}

bool targeter_passwall::set_aim(coord_def a)
{
    cur_path = make_unique<passwall_path>(you, a - you.pos(), range);
    return true;
}

aff_type targeter_passwall::is_affected(coord_def loc)
{
    if (!cur_path)
        return AFF_NO;
    // not very efficient...
    for (auto p : cur_path->path)
        if (p == loc)
            return AFF_YES;
    return AFF_NO;
}

bool targeter_passwall::can_affect_outside_range()
{
    return true;
}

bool targeter_passwall::can_affect_unseen()
{
    return true;
}

bool targeter_passwall::affects_monster(const monster_info& /*mon*/)
{
    return false;
}

targeter_dig::targeter_dig(int max_range) :
    targeter_beam(&you, max_range, ZAP_DIG, 0, 0, 0)
{
}

bool targeter_dig::valid_aim(coord_def a)
{
    if (a == origin)
        return notify_fail("Please select a direction to dig.");
    if ((origin - a).rdist() > range || !in_bounds(a))
        return notify_fail("Out of range.");
    int possible_squares_affected;
    if (aim_test_cache.count(a))
        possible_squares_affected = aim_test_cache[a];
    else
    {
        // TODO: maybe shouldn't use set_aim? ugly side-effect, but it does take
        // care of all the beam management.
        if (!set_aim(a))
            possible_squares_affected = -1; // can't happen?
        else
        {
            possible_squares_affected = 0;
            for (auto p : path_taken)
                if (beam.can_affect_wall(p) ||
                        in_bounds(p) && env.map_knowledge(p).feat() == DNGN_UNSEEN)
                {
                    possible_squares_affected++;
                }
        }
        aim_test_cache[a] = possible_squares_affected;
    }
    if (possible_squares_affected == 0)
        return notify_fail("Digging in that direction won't affect any walls.");
    else if (possible_squares_affected < 0)
        return false;
    else
        return true;
}

bool targeter_dig::can_affect_unseen()
{
    return true;
}

bool targeter_dig::affects_monster(const monster_info& /*mon*/)
{
    return false;
}

bool targeter_dig::can_affect_walls()
{
    return true;
}

aff_type targeter_dig::is_affected(coord_def loc)
{
    aff_type current = AFF_YES;
    bool hit_barrier = false;
    for (auto pc : path_taken)
    {
        if (hit_barrier)
            return AFF_NO; // some previous iteration hit a barrier
        current = AFF_YES;
        // uses comparison to DNGN_UNSEEN so that this works sensibly with magic
        // mapping etc. TODO: console tracers use the same symbol/color as
        // mmapped walls.
        if (in_bounds(pc) && env.map_knowledge(pc).feat() != DNGN_UNSEEN)
        {
            if (!cell_is_solid(pc))
                current = AFF_TRACER;
            else if (!beam.can_affect_wall(pc))
            {
                current = AFF_TRACER; // show tracer at the barrier cell
                hit_barrier = true;
            }
            // otherwise, default to AFF_YES
        }
        // unseen squares default to AFF_YES
        if (pc == loc)
            return current;
    }
    // path never intersected loc at all
    return AFF_NO;
}

targeter_transference::targeter_transference(const actor* act, int aoe) :
    targeter_smite(act, LOS_RADIUS, aoe, aoe, false, nullptr)
{
}

bool targeter_transference::valid_aim(coord_def a)
{
    if (!targeter_smite::valid_aim(a))
        return false;

    const actor *victim = actor_at(a);
    if (victim && you.can_see(*victim))
    {
        if (mons_is_hepliaklqana_ancestor(victim->type))
        {
            return notify_fail("You can't transfer your ancestor with "
                               "themself.");
        }
        if (mons_is_tentacle_or_tentacle_segment(victim->type)
            || victim->is_stationary())
        {
            return notify_fail("You can't transfer that.");
        }
    }
    return true;
}

targeter_fragment::targeter_fragment(const actor* act, int power, int ran) :
    targeter_smite(act, ran, 1, 1, true, nullptr),
    pow(power)
{
}

bool targeter_fragment::valid_aim(coord_def a)
{
    if (!targeter_smite::valid_aim(a))
        return false;

    bolt tempbeam;
    bool temp;
    if (!setup_fragmentation_beam(tempbeam, pow, agent, a, true, nullptr, temp))
        return notify_fail("You cannot affect that.");
    return true;
}

bool targeter_fragment::set_aim(coord_def a)
{
    if (!targeter::set_aim(a))
        return false;

    bolt tempbeam;
    bool temp;

    if (setup_fragmentation_beam(tempbeam, pow, agent, a, true, nullptr, temp))
    {
        exp_range_min = tempbeam.ex_size;
        exp_range_max = tempbeam.ex_size;
    }
    else
    {
        exp_range_min = exp_range_max = 0;
        return false;
    }

    bolt beam;
    beam.target = a;
    beam.use_target_as_pos = true;
    exp_map_min.init(INT_MAX);
    beam.determine_affected_cells(exp_map_min, coord_def(), 0,
                                  exp_range_min, false, false);
    exp_map_max.init(INT_MAX);
    beam.determine_affected_cells(exp_map_max, coord_def(), 0,
                                  exp_range_max, false, false);

    return true;
}

targeter_reach::targeter_reach(const actor* act, reach_type ran) :
    range(ran)
{
    ASSERT(act);
    agent = act;
    origin = aim = act->pos();
}

bool targeter_reach::valid_aim(coord_def a)
{
    if (!cell_see_cell(origin, a, LOS_DEFAULT))
        return notify_fail("You cannot see that place.");
    if (!agent->see_cell_no_trans(a))
        return notify_fail("You can't get through.");

    int dist = (origin - a).rdist();

    if (dist > range)
        return notify_fail("You can't reach that far!");

    return true;
}

aff_type targeter_reach::is_affected(coord_def loc)
{
    if (!valid_aim(loc))
        return AFF_NO;

    if (loc == aim)
        return AFF_YES;

    if (((loc - origin) * 2 - (aim - origin)).abs() <= 1
        && feat_is_reachable_past(grd(loc)))
    {
        return AFF_TRACER;
    }

    return AFF_NO;
}

targeter_cleave::targeter_cleave(const actor* act, coord_def target)
{
    ASSERT(act);
    agent = act;
    origin = act->pos();
    aim = target;
    list<actor*> act_targets;
    get_cleave_targets(*act, target, act_targets);
    while (!act_targets.empty())
    {
        targets.insert(act_targets.front()->pos());
        act_targets.pop_front();
    }
}

aff_type targeter_cleave::is_affected(coord_def loc)
{
    return targets.count(loc) ? AFF_YES : AFF_NO;
}

targeter_cloud::targeter_cloud(const actor* act, int r,
                                 int count_min, int count_max) :
    range(r), cnt_min(count_min), cnt_max(count_max), avoid_clouds(true)
{
    ASSERT(cnt_min > 0);
    ASSERT(cnt_max > 0);
    ASSERT(cnt_min <= cnt_max);
    agent = act;
    if (agent)
        origin = aim = act->pos();
}

static bool _cloudable(coord_def loc, bool avoid_clouds)
{
    return in_bounds(loc)
           && !cell_is_solid(loc)
           && !(avoid_clouds && cloud_at(loc)
           && !is_sanctuary(loc));
}

bool targeter_cloud::valid_aim(coord_def a)
{
    if (agent && (origin - a).rdist() > range)
        return notify_fail("Out of range.");
    if (!map_bounds(a)
        || agent
           && origin != a
           && !cell_see_cell(origin, a, LOS_NO_TRANS))
    {
        // Scrying/glass/tree/grate.
        if (agent && agent->see_cell(a))
            return notify_fail("There's something in the way.");
        return notify_fail("You cannot see that place.");
    }
    if (cell_is_solid(a))
        return notify_fail(_wallmsg(a));
    if (agent)
    {
        if (cloud_at(a) && avoid_clouds)
            return notify_fail("There's already a cloud there.");
        else if (is_sanctuary(a))
            return notify_fail("You can't place clouds in a sanctuary.");
        ASSERT(_cloudable(a, avoid_clouds));
    }
    return true;
}

bool targeter_cloud::set_aim(coord_def a)
{
    if (!targeter::set_aim(a))
        return false;

    seen.clear();
    queue.clear();
    queue.emplace_back();

    int placed = 0;
    queue[0].push_back(a);

    for (unsigned int d1 = 0; d1 < queue.size() && placed < cnt_max; d1++)
    {
        placed += queue[d1].size();

        for (coord_def c : queue[d1])
        {
            for (adjacent_iterator ai(c); ai; ++ai)
                if (_cloudable(*ai, avoid_clouds) && !seen.count(*ai))
                {
                    unsigned int d2 = d1 + 1;
                    if (d2 >= queue.size())
                        queue.resize(d2 + 1);
                    queue[d2].push_back(*ai);
                    seen[*ai] = AFF_TRACER;
                }

            seen[c] = placed <= cnt_min ? AFF_YES : AFF_MAYBE;
        }
    }

    return true;
}

bool targeter_cloud::can_affect_outside_range()
{
    return true;
}

aff_type targeter_cloud::is_affected(coord_def loc)
{
    if (!valid_aim(aim))
        return AFF_NO;

    if (aff_type *aff = map_find(seen, loc))
    {
        if (*aff > 0) // AFF_TRACER is used privately
            return *aff;
    }
    return AFF_NO;
}

targeter_splash::targeter_splash(const actor* act, int ran)
    : range(ran)
{
    ASSERT(act);
    agent = act;
    origin = aim = act->pos();
}

bool targeter_splash::valid_aim(coord_def a)
{
    if (agent && grid_distance(origin, a) > range)
        return notify_fail("Out of range.");
    return true;
}

aff_type targeter_splash::is_affected(coord_def loc)
{
    if (!valid_aim(aim) || !valid_aim(loc))
        return AFF_NO;

    if (loc == aim)
        return AFF_YES;

    // self-spit currently doesn't splash
    if (aim == origin)
        return AFF_NO;

    // it splashes around only upon hitting someone
    if (!anyone_there(aim))
        return AFF_NO;

    if (grid_distance(loc, aim) > 1)
        return AFF_NO;

    // you're safe from being splashed by own spit
    if (loc == origin)
        return AFF_NO;

    return anyone_there(loc) ? AFF_YES : AFF_MAYBE;
}

targeter_radius::targeter_radius(const actor *act, los_type _los,
                             int ran, int ran_max, int ran_min):
    range(ran), range_max(ran_max), range_min(ran_min)
{
    ASSERT(act);
    agent = act;
    origin = aim = act->pos();
    los = _los;
    if (!range_max)
        range_max = range;
    ASSERT(range_max >= range);
}

bool targeter_radius::valid_aim(coord_def a)
{
    if ((a - origin).rdist() > range_max || (a - origin).rdist() < range_min)
        return notify_fail("Out of range.");
    // If this message ever becomes used, please improve it. I did not
    // bother adding complexity just for monsters and "hit allies" prompts
    // which don't need it.
    if (!is_affected(a))
        return notify_fail("The effect is blocked.");
    return true;
}

aff_type targeter_radius::is_affected(coord_def loc)
{
    if (loc == aim)
        return AFF_YES;

    if ((loc - origin).rdist() > range_max || (loc - origin).rdist() < range_min)
        return AFF_NO;

    if (!cell_see_cell(loc, origin, los))
        return AFF_NO;

    return AFF_YES;
}

targeter_thunderbolt::targeter_thunderbolt(const actor *act, int r,
                                             coord_def _prev)
{
    ASSERT(act);
    agent = act;
    origin = act->pos();
    prev = _prev;
    aim = prev.origin() ? origin : prev;
    ASSERT_RANGE(r, 1 + 1, you.current_vision + 1);
    range = r;
}

bool targeter_thunderbolt::valid_aim(coord_def a)
{
    if (a != origin && !cell_see_cell(origin, a, LOS_NO_TRANS))
    {
        // Scrying/glass/tree/grate.
        if (agent->see_cell(a))
            return notify_fail("There's something in the way.");
        return notify_fail("You cannot see that place.");
    }
    if ((origin - a).rdist() > range)
        return notify_fail("Out of range.");
    return true;
}

static void _make_ray(ray_def &ray, coord_def a, coord_def b)
{
    // Like beams, we need to allow picking the "better" ray if one is blocked
    // by a wall.
    if (!find_ray(a, b, ray, opc_solid_see))
        ray = ray_def(geom::ray(a.x + 0.5, a.y + 0.5, b.x - a.x, b.y - a.y));
}

static bool left_of(coord_def a, coord_def b)
{
    return a.x * b.y > a.y * b.x;
}

bool targeter_thunderbolt::set_aim(coord_def a)
{
    aim = a;
    zapped.clear();

    if (a == origin)
        return false;

    arc_length.init(0);

    ray_def ray;
    coord_def p; // ray.pos() does lots of processing, cache it

    // For consistency with beams, we need to
    _make_ray(ray, origin, aim);
    while ((origin - (p = ray.pos())).rdist() <= range
           && map_bounds(p) && opc_solid_see(p) < OPC_OPAQUE)
    {
        if (p != origin && zapped[p] <= 0)
        {
            zapped[p] = AFF_YES;
            arc_length[origin.distance_from(p)]++;
        }
        ray.advance();
    }

    if (prev.origin())
        return true;

    _make_ray(ray, origin, prev);
    while ((origin - (p = ray.pos())).rdist() <= range
           && map_bounds(p) && opc_solid_see(p) < OPC_OPAQUE)
    {
        if (p != origin && zapped[p] <= 0)
        {
            zapped[p] = AFF_MAYBE; // fully affected, we just want to highlight cur
            arc_length[origin.distance_from(p)]++;
        }
        ray.advance();
    }

    coord_def a1 = prev - origin;
    coord_def a2 = aim - origin;
    if (left_of(a2, a1))
        swap(a1, a2);

    for (int x = -LOS_RADIUS; x <= LOS_RADIUS; ++x)
        for (int y = -LOS_RADIUS; y <= LOS_RADIUS; ++y)
        {
            if (max(abs(x), abs(y)) > range)
                continue;
            coord_def r(x, y);
            if (left_of(a1, r) && left_of(r, a2))
            {
                (p = r) += origin;
                if (!zapped.count(p))
                    arc_length[r.rdist()]++;
                if (zapped[p] <= 0 && cell_see_cell(origin, p, LOS_NO_TRANS))
                    zapped[p] = AFF_MAYBE;
            }
        }

    zapped[origin] = AFF_NO;

    return true;
}

aff_type targeter_thunderbolt::is_affected(coord_def loc)
{
    if (loc == aim)
        return zapped[loc] ? AFF_YES : AFF_TRACER;

    if ((loc - origin).rdist() > range)
        return AFF_NO;

    return zapped[loc];
}

targeter_shadow_step::targeter_shadow_step(const actor* act, int r) :
    range(r)
{
    ASSERT(act);
    agent = act;
    origin = act->pos();
    step_is_blocked = false;
}

bool targeter_shadow_step::valid_aim(coord_def a)
{
    ray_def ray;

    if (origin == a)
        return notify_fail("You cannot target yourself.");
    else if ((origin - a).rdist() > range)
        return notify_fail("Out of range.");
    else if (!cell_see_cell(origin, a, LOS_NO_TRANS))
    {
        if (agent->see_cell(a))
            return notify_fail("There's something in the way.");
        else
            return notify_fail("You cannot see that place.");
    }
    else if (cell_is_solid(a))
        return notify_fail("There's something in the way.");
    else if (!find_ray(agent->pos(), a, ray, opc_solid_see))
        return notify_fail("There's something in the way.");
    else if (!has_additional_sites(a))
    {
        switch (no_landing_reason)
        {
        case shadow_step_blocked::move:
        case shadow_step_blocked::occupied:
            return notify_fail("There is no safe place near that"
                               " location.");
        case shadow_step_blocked::path:
            return notify_fail("There's something in the way.");
        case shadow_step_blocked::no_target:
            return notify_fail("There isn't a shadow there.");
        case shadow_step_blocked::none:
            die("buggy no_landing_reason");
        }
    }
    return true;
}

bool targeter_shadow_step::valid_landing(coord_def a, bool check_invis)
{
    actor *act;
    ray_def ray;

    if (!agent->is_habitable(a))
    {
        blocked_landing_reason = shadow_step_blocked::move;
        return false;
    }
    if (agent->is_player())
    {
        monster* beholder = you.get_beholder(a);
        if (beholder)
        {
            blocked_landing_reason = shadow_step_blocked::move;
            return false;
        }

        monster* fearmonger = you.get_fearmonger(a);
        if (fearmonger)
        {
            blocked_landing_reason = shadow_step_blocked::move;
            return false;
        }
    }
    if (!find_ray(agent->pos(), a, ray, opc_solid_see))
    {
        blocked_landing_reason = shadow_step_blocked::path;
        return false;
    }

    // Check if a landing site is invalid due to a visible monster obstructing
    // the path.
    ray.advance();
    while (map_bounds(ray.pos()))
    {
        act = actor_at(ray.pos());
        if (ray.pos() == a)
        {
            if (act && (!check_invis || agent->can_see(*act)))
            {
                blocked_landing_reason = shadow_step_blocked::occupied;
                return false;
            }
            break;
        }
        ray.advance();
    }
    return true;
}

aff_type targeter_shadow_step::is_affected(coord_def loc)
{
    aff_type aff = AFF_NO;

    if (loc == aim)
        aff = AFF_YES;
    else if (additional_sites.count(loc))
        aff = AFF_LANDING;
    return aff;
}

// If something unseen either occupies the aim position or blocks the
// shadow_step path, indicate that with step_is_blocked, but still return true
// so long there is at least one valid landing position from the player's
// perspective.
bool targeter_shadow_step::set_aim(coord_def a)
{
    if (a == origin)
        return false;
    if (!targeter::set_aim(a))
        return false;

    step_is_blocked = false;

    // Find our set of landing sites, choose one at random to be the
    // destination and see if it's actually blocked.
    set_additional_sites(aim);
    if (additional_sites.size())
    {
        auto it = random_iterator(additional_sites);
        landing_site = *it;
        if (!valid_landing(landing_site, false))
            step_is_blocked = true;
        return true;
    }
    return false;
}

// Determine the set of valid landing sites
void targeter_shadow_step::set_additional_sites(coord_def a)
{
    get_additional_sites(a);
    additional_sites = temp_sites;
}

// Determine the set of valid landing sites for the target, putting the results
// in the private set variable temp_sites. This uses valid_aim(), so it looks
// for uninhabited squares that are habitable by the player, but doesn't check
// against e.g. harmful clouds
void targeter_shadow_step::get_additional_sites(coord_def a)
{
    bool agent_adjacent = a.distance_from(agent->pos()) == 1;
    temp_sites.clear();

    const actor *victim = actor_at(a);
    if (!victim || !victim->as_monster()
        || mons_is_firewood(*victim->as_monster())
        || victim->as_monster()->friendly()
        || !agent->can_see(*victim)
        || !victim->umbraed())
    {
        no_landing_reason = shadow_step_blocked::no_target;
        return;
    }

    no_landing_reason = shadow_step_blocked::none;
    for (adjacent_iterator ai(a, false); ai; ++ai)
    {
        // See if site is valid, record a putative reason for why no sites were
        // found.
        if (!agent_adjacent || agent->pos().distance_from(*ai) > 1)
        {
            if (valid_landing(*ai))
            {
                temp_sites.insert(*ai);
                no_landing_reason = shadow_step_blocked::none;
            }
            else
                no_landing_reason = blocked_landing_reason;
        }
    }
}

// See if we can find at least one valid landing position for the given monster.
bool targeter_shadow_step::has_additional_sites(coord_def a)
{
    get_additional_sites(a);
    return temp_sites.size();
}

targeter_cone::targeter_cone(const actor *act, int r)
{
    ASSERT(act);
    agent = act;
    origin = act->pos();
    aim = origin;
    ASSERT_RANGE(r, 1 + 1, you.current_vision + 1);
    range = r;
}

bool targeter_cone::valid_aim(coord_def a)
{
    if (a != origin && !cell_see_cell(origin, a, LOS_NO_TRANS))
    {
        // Scrying/glass/tree/grate.
        if (agent->see_cell(a))
            return notify_fail("There's something in the way.");
        return notify_fail("You cannot see that place.");
    }
    if ((origin - a).rdist() > range)
        return notify_fail("Out of range.");
    return true;
}

static bool left_of_eq(coord_def a, coord_def b)
{
    return a.x * b.y >= a.y * b.x;
}

// Ripped off from targeter_thunderbolt::set_aim.
bool targeter_cone::set_aim(coord_def a)
{
    aim = a;
    zapped.clear();
    for (int i = 0; i < LOS_RADIUS + 1; i++)
        sweep[i].clear();

    if (a == origin)
        return false;

    const coord_def delta = a - origin;
    const double arc = PI/4;
    coord_def l, r;
    l.x = origin.x + (cos(-arc) * delta.x - sin(-arc) * delta.y + 0.5);
    l.y = origin.y + (sin(-arc) * delta.x + cos(-arc) * delta.y + 0.5);
    r.x = origin.x + (cos( arc) * delta.x - sin( arc) * delta.y + 0.5);
    r.y = origin.y + (sin( arc) * delta.x + cos( arc) * delta.y + 0.5);

    coord_def p;

    coord_def a1 = l - origin;
    coord_def a2 = r - origin;
    if (left_of(a2, a1))
        swap(a1, a2);

    for (int x = -LOS_RADIUS; x <= LOS_RADIUS; ++x)
        for (int y = -LOS_RADIUS; y <= LOS_RADIUS; ++y)
        {
            if (max(abs(x), abs(y)) > range)
                continue;
            coord_def q(x, y);
            if (left_of_eq(a1, q) && left_of_eq(q, a2))
            {
                (p = q) += origin;
                if (zapped[p] <= 0
                    && map_bounds(p)
                    && opc_solid_see(p) < OPC_OPAQUE
                    && cell_see_cell(origin, p, LOS_NO_TRANS))
                {
                    zapped[p] = AFF_YES;
                    sweep[(origin - p).rdist()][p] = AFF_YES;
                }
            }
        }

    zapped[origin] = AFF_NO;
    sweep[0].clear();

    return true;
}

aff_type targeter_cone::is_affected(coord_def loc)
{
    if (loc == aim)
        return zapped[loc] ? AFF_YES : AFF_TRACER;

    if ((loc - origin).rdist() > range)
        return AFF_NO;

    return zapped[loc];
}

targeter_shotgun::targeter_shotgun(const actor* act, size_t beam_count,
                                     int r, bool clouds)
{
    ASSERT(act);
    agent = act;
    origin = act->pos();
    num_beams = beam_count;
    for (size_t i = 0; i < num_beams; i++)
        rays.emplace_back();
    range = r;
    uses_clouds = clouds;
}

bool targeter_shotgun::valid_aim(coord_def a)
{
    if (a != origin && !cell_see_cell(origin, a, LOS_NO_TRANS))
    {
        if (agent->see_cell(a))
            return notify_fail("There's something in the way.");
        return notify_fail("You cannot see that place.");
    }
    if ((origin - a).rdist() > range)
        return notify_fail("Out of range.");
    return true;
}

bool targeter_shotgun::set_aim(coord_def a)
{
    zapped.clear();

    // confused monster targeting might be fuzzed across a wall, so
    // skip the validation in the parent function and set aim directly.
    // N.B. We assume this targeter can actually handle an invalid aim
    // (not all targeters can).
    if (!agent || agent->is_monster())
        aim = a;
    // ... but for UI consistency, players should be restricted to LOS.
    else if (!targeter::set_aim(a))
        return false;

    if (a == origin)
        return false;

    ray_def orig_ray;
    _make_ray(orig_ray, origin, a);
    coord_def p;

    const double spread_range = (double)(num_beams - 1) * PI / 40.0;
    for (size_t i = 0; i < num_beams; i++)
    {
        double spread = (num_beams == 1)
                        ? 0.0
                        : -(spread_range / 2.0)
                          + (spread_range * (double)i)
                                          / (double)(num_beams - 1);
        rays[i] = ray_def();
        rays[i].r.start = orig_ray.r.start;
        rays[i].r.dir.x =
             orig_ray.r.dir.x * cos(spread) + orig_ray.r.dir.y * sin(spread);
        rays[i].r.dir.y =
            -orig_ray.r.dir.x * sin(spread) + orig_ray.r.dir.y * cos(spread);
        ray_def tempray = rays[i];
        p = tempray.pos();
        while ((origin - (p = tempray.pos())).rdist() <= range
               && map_bounds(p) && opc_solid_see(p) < OPC_OPAQUE)
        {
            if (p != origin
                && (!uses_clouds || !cloud_at(p) && !is_sanctuary(p)))
            {
                zapped[p] = zapped[p] + 1;
            }
            tempray.advance();
        }
    }

    zapped[origin] = 0;
    return true;
}

aff_type targeter_shotgun::is_affected(coord_def loc)
{
    if ((loc - origin).rdist() > range
         || uses_clouds && (cloud_at(loc) || is_sanctuary(loc)))
    {
        return AFF_NO;
    }

    return (zapped[loc] >= num_beams) ? AFF_YES :
           (zapped[loc] > 0)          ? AFF_MAYBE
                                      : AFF_NO;
}

targeter_monster_sequence::targeter_monster_sequence(const actor *act, int pow, int r) :
                          targeter_beam(act, r, ZAP_DEBUGGING_RAY, pow, 0, 0)
{
    // for `path_taken` to be set properly, the beam needs to be piercing, and
    // ZAP_DEBUGGING_RAY is not.
    beam.pierce = true;
}

bool targeter_monster_sequence::set_aim(coord_def a)
{
    if (!targeter_beam::set_aim(a))
        return false;

    bolt tempbeam = beam;
    tempbeam.target = origin;
    bool last_cell_has_mons = true;
    bool passed_through_mons = false;
    for (auto c : path_taken)
    {
        if (!last_cell_has_mons)
            return false; // we must have an uninterrupted chain of monsters

        if (cell_is_solid(c))
            break;

        tempbeam.target = c;
        if (anyone_there(c))
        {
            passed_through_mons = true;
            tempbeam.use_target_as_pos = true;
            exp_map.init(INT_MAX);
            tempbeam.determine_affected_cells(exp_map, coord_def(), 0,
                                              0, true, true);
        }
        else
            last_cell_has_mons = false;
    }

    return passed_through_mons;
}

bool targeter_monster_sequence::valid_aim(coord_def a)
{
    if (!targeter_beam::set_aim(a))
        return false;

    bool last_cell_has_mons = true;
    bool passed_through_mons = false;
    for (auto c : path_taken)
    {
        if (!last_cell_has_mons)
            return false; // we must have an uninterrupted chain of monsters

        if (cell_is_solid(c))
            return false;

        if (!anyone_there(c))
            last_cell_has_mons = false;
        else
            passed_through_mons = true;
    }

    return passed_through_mons;
}

aff_type targeter_monster_sequence::is_affected(coord_def loc)
{
    bool on_path = false;
    for (auto c : path_taken)
    {
        if (cell_is_solid(c))
            break;
        if (c == loc)
            on_path = true;
        if (anyone_there(c)
            && !beam.ignores_monster(monster_at(c))
            && (loc - c).rdist() <= 9)
        {
            coord_def centre(9,9);
            if (exp_map(loc - c + centre) < INT_MAX
                && !cell_is_solid(loc))
            {
                return AFF_YES;
            }
        }
    }

    return on_path ? AFF_TRACER : AFF_NO;
}

targeter_overgrow::targeter_overgrow()
{
    agent = &you;
    origin = you.pos();
}

bool targeter_overgrow::overgrow_affects_pos(const coord_def &p)
{
    if (!in_bounds(p))
        return false;
    if (env.markers.property_at(p, MAT_ANY, "veto_shatter") == "veto")
        return false;

    const dungeon_feature_type feat = grd(p);
    if (feat_is_open_door(feat))
    {
        const monster* const mons = monster_at(p);
        if (mons && agent && agent->can_see(*mons))
            return false;

        return true;
    }

    return feat_is_diggable(feat)
        || feat_is_closed_door(feat)
        || feat_is_tree(feat)
        || (feat_is_wall(feat) && !feat_is_permarock(feat));
}

aff_type targeter_overgrow::is_affected(coord_def loc)
{
    if (affected_positions.count(loc))
        return AFF_YES;

    return AFF_NO;
}

bool targeter_overgrow::valid_aim(coord_def a)
{
    if (a != origin && !cell_see_cell(origin, a, LOS_NO_TRANS))
    {
        if (agent && agent->see_cell(a))
            return notify_fail("There's something in the way.");
        return notify_fail("You cannot see that place.");
    }

    if (!overgrow_affects_pos(a))
        return notify_fail("You cannot grow anything here.");

    return true;
}

bool targeter_overgrow::set_aim(coord_def a)
{
    affected_positions.clear();

    if (!targeter::set_aim(a))
        return false;

    if (overgrow_affects_pos(a))
        affected_positions.insert(a);
    else
        return false;

    for (adjacent_iterator ai(a, true); ai; ++ai)
    {
        if (overgrow_affects_pos(*ai) && you.see_cell_no_trans(*ai))
            affected_positions.insert(*ai);
    }

    return affected_positions.size();
}
