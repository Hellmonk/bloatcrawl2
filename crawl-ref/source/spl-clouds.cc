/**
 * @file
 * @brief Cloud creating spells.
**/

#include "AppHdr.h"

#include "spl-clouds.h"

#include <algorithm>

#include "butcher.h"
#include "cloud.h"
#include "coordit.h"
#include "english.h"
#include "env.h"
#include "fprop.h"
#include "godconduct.h"
#include "items.h"
#include "losglobal.h"
#include "message.h"
#include "misc.h"
#include "ouch.h"
#include "prompt.h"
#include "random-pick.h"
#include "shout.h"
#include "spl-util.h"
#include "target.h"
#include "terrain.h"
#include "viewchar.h"

spret_type conjure_flame(const actor *agent, int pow, const coord_def& where,
                         bool fail)
{
    // FIXME: This would be better handled by a flag to enforce max range.
    if (grid_distance(where, agent->pos()) > spell_range(SPELL_CONJURE_FLAME, pow)
        || !in_bounds(where))
    {
        if (agent->is_player())
            mpr("That's too far away.");
        return SPRET_ABORT;
    }

    if (cell_is_solid(where))
    {
        if (agent->is_player())
        {
            const char *feat = feat_type_name(grd(where));
            mprf("You can't place the cloud on %s.", article_a(feat).c_str());
        }
        return SPRET_ABORT;
    }

    cloud_struct* cloud = cloud_at(where);

    if (cloud && cloud->type != CLOUD_FIRE)
    {
        if (agent->is_player())
            mpr("There's already a cloud there!");
        return SPRET_ABORT;
    }

    actor* victim = actor_at(where);
    if (victim)
    {
        if (agent->can_see(*victim))
        {
            if (agent->is_player())
                mpr("You can't place the cloud on a creature.");
            return SPRET_ABORT;
        }

        fail_check();

        // FIXME: maybe should do _paranoid_option_disable() here?
        if (agent->is_player())
            canned_msg(MSG_GHOSTLY_OUTLINE);
        return SPRET_SUCCESS;      // Don't give free detection!
    }

    fail_check();

    if (agent->is_player())
        did_god_conduct(DID_FIRE, min(5 + pow/2, 23));

    if (cloud)
    {
        // Reinforce the cloud - but not too much.
        // It must be a fire cloud from a previous test.
        if (you.see_cell(where))
            mpr("The fire roars with new energy!");
        const int extra_dur = 2 + min(random2(pow) / 2, 20);
        cloud->decay += extra_dur * 5;
        cloud->source = agent->mid;
        if (agent->is_player())
            cloud->set_whose(KC_YOU);
        else
            cloud->set_killer(KILL_MON_MISSILE);
    }
    else
    {
        const int durat = min(5 + (random2(pow)/2) + (random2(pow)/2), 23);
        place_cloud(CLOUD_FIRE, where, durat, agent);
        if (you.see_cell(where))
        {
            if (agent->is_player())
                mpr("The fire roars!");
            else
                mpr("A cloud of flames roars to life!");
        }
    }
    noisy(spell_effect_noise(SPELL_CONJURE_FLAME), where);

    return SPRET_SUCCESS;
}

spret_type cast_big_c(int pow, spell_type spl, const actor *caster, bolt &beam,
                      bool fail)
{
    if (grid_distance(beam.target, you.pos()) > beam.range
        || !in_bounds(beam.target))
    {
        mpr("That is beyond the maximum range.");
        return SPRET_ABORT;
    }

    if (cell_is_solid(beam.target))
    {
        const char *feat = feat_type_name(grd(beam.target));
        mprf("You can't place clouds on %s.", article_a(feat).c_str());
        return SPRET_ABORT;
    }

    cloud_type cty = CLOUD_NONE;
    //XXX: there should be a better way to specify beam cloud types
    switch (spl)
    {
        case SPELL_POISONOUS_CLOUD:
            beam.flavour = BEAM_POISON;
            beam.name = "blast of poison";
            cty = CLOUD_POISON;
            break;
        case SPELL_HOLY_BREATH:
            beam.flavour = BEAM_HOLY;
            beam.origin_spell = SPELL_HOLY_BREATH;
            cty = CLOUD_HOLY_FLAMES;
            break;
        case SPELL_FREEZING_CLOUD:
            beam.flavour = BEAM_COLD;
            beam.name = "freezing blast";
            cty = CLOUD_COLD;
            break;
        default:
            mpr("That kind of cloud doesn't exist!");
            return SPRET_ABORT;
    }

    beam.thrower           = KILL_YOU;
    beam.hit               = AUTOMATIC_HIT;
    beam.damage            = dice_def(42, 1); // just a convenient non-zero
    beam.is_tracer         = true;
    beam.use_target_as_pos = true;
    beam.origin_spell      = spl;
    beam.affect_endpoint();
    if (beam.beam_cancelled)
        return SPRET_ABORT;

    fail_check();

    big_cloud(cty, caster, beam.target, pow, 8 + random2(3), -1);
    noisy(spell_effect_noise(spl), beam.target);
    return SPRET_SUCCESS;
}

static int _make_a_normal_cloud(coord_def where, int pow, int spread_rate,
                                cloud_type ctype, const actor *agent, int colour,
                                string name, string tile, int excl_rad)
{
    place_cloud(ctype, where,
                (3 + random2(pow / 4) + random2(pow / 4) + random2(pow / 4)),
                agent, spread_rate, colour, name, tile, excl_rad);

    return 1;
}

void big_cloud(cloud_type cl_type, const actor *agent,
               const coord_def& where, int pow, int size, int spread_rate,
               int colour, string name, string tile)
{
    // The starting point _may_ be a place no cloud can be placed on.
    apply_area_cloud(_make_a_normal_cloud, where, pow, size,
                     cl_type, agent, spread_rate, colour, name, tile,
                     -1);
}

spret_type cast_ring_of_flames(int power, bool fail)
{
    fail_check();
    did_god_conduct(DID_FIRE, min(5 + power/5, 50));
    you.increase_duration(DUR_FIRE_SHIELD,
                          5 + (power / 10) + (random2(power) / 5), 50,
                          "The air around you leaps into flame!");
    manage_fire_shield(1);
    return SPRET_SUCCESS;
}

void manage_fire_shield(int delay)
{
    ASSERT(you.duration[DUR_FIRE_SHIELD]);

    int old_dur = you.duration[DUR_FIRE_SHIELD];

    you.duration[DUR_FIRE_SHIELD]-= delay;
    if (you.duration[DUR_FIRE_SHIELD] < 0)
        you.duration[DUR_FIRE_SHIELD] = 0;

    // Melt ice armour entirely.
    maybe_melt_player_enchantments(BEAM_FIRE, 100);

    // Remove fire clouds on top of you
    if (cloud_at(you.pos()) && cloud_at(you.pos())->type == CLOUD_FIRE)
        delete_cloud(you.pos());

    if (!you.duration[DUR_FIRE_SHIELD])
    {
        mprf(MSGCH_DURATION, "Your ring of flames gutters out.");
        return;
    }

    int threshold = get_expiration_threshold(DUR_FIRE_SHIELD);

    if (old_dur > threshold && you.duration[DUR_FIRE_SHIELD] < threshold)
        mprf(MSGCH_WARN, "Your ring of flames is guttering out.");

    // Place fire clouds all around you
    for (adjacent_iterator ai(you.pos()); ai; ++ai)
        if (!cell_is_solid(*ai) && !cloud_at(*ai))
            place_cloud(CLOUD_FIRE, *ai, 1 + random2(6), &you);
}

spret_type cast_corpse_rot(bool fail)
{
    if (!you.res_rotting())
    {
        for (stack_iterator si(you.pos()); si; ++si)
        {
            if (si->is_type(OBJ_CORPSES, CORPSE_BODY))
            {
                if (!yesno(("Really cast Corpse Rot while standing on " + si->name(DESC_A) + "?").c_str(), false, 'n'))
                {
                    canned_msg(MSG_OK);
                    return SPRET_ABORT;
                }
                break;
            }
        }
    }
    fail_check();
    corpse_rot(&you);
    return SPRET_SUCCESS;
}

void corpse_rot(actor* caster)
{
    // If there is no caster (god wrath), centre the effect on the player.
    const coord_def center = caster ? caster->pos() : you.pos();
    bool saw_rot = false;

    for (radius_iterator ri(center, LOS_NO_TRANS); ri; ++ri)
    {
        if (!is_sanctuary(*ri) && !cloud_at(*ri))
            for (stack_iterator si(*ri); si; ++si)
                if (si->is_type(OBJ_CORPSES, CORPSE_BODY))
                {
                    // Found a corpse. Skeletonise it if possible.
                    if (!mons_skeleton(si->mon_type))
                    {
                        item_was_destroyed(*si);
                        destroy_item(si->index());
                    }
                    else
                        turn_corpse_into_skeleton(*si);

                    place_cloud(CLOUD_MIASMA, *ri, 4+random2avg(16, 3),caster);

                    if (!saw_rot && you.see_cell(*ri))
                        saw_rot = true;

                    // Don't look for more corpses here.
                    break;
                }
    }

    if (saw_rot)
        mprf("You %s decay.", you.can_smell() ? "smell" : "sense");
    else
        canned_msg(MSG_NOTHING_HAPPENS);
}

void holy_flames(monster* caster, actor* defender)
{
    const coord_def pos = defender->pos();
    int cloud_count = 0;
    const int dur = 8 + random2avg(caster->get_hit_dice() * 3, 2);

    for (adjacent_iterator ai(pos); ai; ++ai)
    {
        if (!in_bounds(*ai)
            || cloud_at(*ai)
            || cell_is_solid(*ai)
            || is_sanctuary(*ai)
            || monster_at(*ai))
        {
            continue;
        }

        place_cloud(CLOUD_HOLY_FLAMES, *ai, dur, caster);

        cloud_count++;
    }

    if (cloud_count)
    {
        if (defender->is_player())
            mpr("Blessed fire suddenly surrounds you!");
        else
            simple_monster_message(defender->as_monster(),
                                   " is surrounded by blessed fire!");
    }
}

static bool _safe_cloud_spot(const monster* mon, coord_def p)
{
    if (cell_is_solid(p) || cloud_at(p))
        return false;

    if (actor_at(p) && mons_aligned(mon, actor_at(p)))
        return false;

    return true;
}

void apply_control_winds(const monster* mon)
{
    vector<coord_def> cloud_list;
    for (distance_iterator di(mon->pos(), true, false, LOS_RADIUS); di; ++di)
    {
        if (cloud_at(*di)
            && cell_see_cell(mon->pos(), *di, LOS_SOLID)
            && (di.radius() < 6 || cloud_at(*di)->type == CLOUD_FOREST_FIRE
                                || actor_at(*di) && mons_aligned(mon, actor_at(*di))))
        {
            cloud_list.push_back(*di);
        }
    }

    bolt wind_beam;
    wind_beam.hit = AUTOMATIC_HIT;
    wind_beam.pierce  = true;
    wind_beam.affects_nothing = true;
    wind_beam.source = mon->pos();
    wind_beam.range = LOS_RADIUS;
    wind_beam.is_tracer = true;

    for (int i = cloud_list.size() - 1; i >= 0; --i)
    {
        const coord_def p = cloud_list[i];
        // Leave clouds engulfing hostiles alone
        if (actor_at(p) && !mons_aligned(actor_at(p), mon))
            continue;

        bool pushed = false;

        coord_def newpos;
        if (p != mon->pos())
        {
            wind_beam.target = p;
            wind_beam.fire();
            for (unsigned int j = 0; j < wind_beam.path_taken.size() - 1; ++j)
            {
                if (wind_beam.path_taken[j] == p)
                {
                    newpos = wind_beam.path_taken[j+1];
                    if (_safe_cloud_spot(mon, newpos))
                    {
                        swap_clouds(p, newpos);
                        pushed = true;
                        break;
                    }
                }
            }
        }

        if (!pushed)
        {
            for (distance_iterator di(p, true, true, 1); di; ++di)
            {
                if ((newpos.origin() || adjacent(*di, newpos))
                    && di->distance_from(mon->pos())
                        == (p.distance_from(mon->pos()) + 1)
                    && _safe_cloud_spot(mon, *di))
                {
                    swap_clouds(*di, p);
                    pushed = true;
                    break;
                }
            }

            if (!pushed && actor_at(p) && mons_aligned(mon, actor_at(p)))
                cloud_at(p)->decay = cloud_at(p)->decay / 2 - 20;
        }
    }
}

random_pick_entry<cloud_type> cloud_cone_clouds[] =
{
  { 0,   50,  80, FALL, CLOUD_RAIN },
  { 0,   50, 100, FALL, CLOUD_MIST },
  { 0,   50, 150, FALL, CLOUD_MEPHITIC },
  { 0,  100, 100, PEAK, CLOUD_FIRE },
  { 0,  100, 100, PEAK, CLOUD_COLD },
  { 0,  100, 100, PEAK, CLOUD_POISON },
  { 30, 100, 125, RISE, CLOUD_NEGATIVE_ENERGY },
  { 40, 100, 135, RISE, CLOUD_STORM },
  { 50, 100, 175, RISE, CLOUD_ACID },
  { 0,0,0,FLAT,CLOUD_NONE }
};

spret_type cast_cloud_cone(const actor *caster, int pow, const coord_def &pos,
                           bool fail)
{
    // For monsters:
    pow = min(100, pow);

    const int range = spell_range(SPELL_CLOUD_CONE, pow);

    targetter_shotgun hitfunc(caster, CLOUD_CONE_BEAM_COUNT, range);

    hitfunc.set_aim(pos);

    if (caster->is_player())
    {
        if (stop_attack_prompt(hitfunc, "cloud"))
            return SPRET_ABORT;
    }

    fail_check();

    random_picker<cloud_type, NUM_CLOUD_TYPES> cloud_picker;
    cloud_type cloud = cloud_picker.pick(cloud_cone_clouds, pow, CLOUD_NONE);

    for (const auto &entry : hitfunc.zapped)
    {
        if (entry.second <= 0)
            continue;
        place_cloud(cloud, entry.first,
                    5 + random2avg(12 + div_rand_round(pow * 3, 4), 3),
                    caster);
    }
    mprf("%s %s a blast of %s!",
         caster->name(DESC_THE).c_str(),
         caster->conj_verb("create").c_str(),
         cloud_type_name(cloud).c_str());

    if (cloud == CLOUD_FIRE && caster->is_player())
        did_god_conduct(DID_FIRE, min(5 + pow/2, 23));

    return SPRET_SUCCESS;
}
