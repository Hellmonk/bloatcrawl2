/**
 * @file
 * @brief Cloud creating spells.
**/

#include "AppHdr.h"

#include "spl-clouds.h"

#include <algorithm>

#include "butcher.h"
#include "cloud.h"
#include "coord.h"
#include "coordit.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "fprop.h"
#include "fight.h"
#include "god-conduct.h"
#include "items.h"
#include "level-state-type.h"
#include "losglobal.h"
#include "message.h"
#include "mon-behv.h" // ME_WHACK
#include "ouch.h"
#include "prompt.h"
#include "random-pick.h"
#include "shout.h"
#include "spl-util.h"
#include "target.h"
#include "terrain.h"
#include "viewchar.h"

spret conjure_flame(int pow, bool fail)
{
    cloud_struct* cloud = cloud_at(you.pos());

    if (cloud && !(cloud->type == CLOUD_FIRE || cloud->type == CLOUD_EMBERS))
    {
        mpr("There's already a cloud here!");
        return spret::abort;
    }

    fail_check();

    if (cloud && cloud->type == CLOUD_FIRE)
    {
        // Reinforce the cloud - but not too much.
        // It must be a fire cloud from a previous test.
        mpr("The fire blazes with new energy!");
        const int extra_dur = 2 + min(random2(pow) / 2, 20);
        cloud->decay += extra_dur * 5;
        cloud->source = you.mid ;
        cloud->set_whose(KC_YOU);
    }
    else if (cloud && cloud->type == CLOUD_EMBERS)
    {
        mpr("The fire ignites!");
        place_cloud(CLOUD_FIRE, you.pos(), min(5 + (random2(pow)/2)
                                                 + (random2(pow)/2), 23), &you);
    }
    else
    {
        you.props["cflame_dur"] = min(5 + (random2(pow)/2)
                                               + (random2(pow)/2), 23);
        place_cloud(CLOUD_EMBERS, you.pos(), 1, &you);
        // Create a cloud for the time it takes to cast plus 1 aut, so that no
        // matter what happens the flame tries to ignite after the next player
        // action.
        cloud = cloud_at(you.pos());
        cloud->decay = player_speed() + 1;
        mpr("The fire begins to smolder!");
    }
    noisy(spell_effect_noise(SPELL_CONJURE_FLAME), you.pos());

    return spret::success;
}

spret cast_poisonous_vapours(int pow, const dist &beam, bool fail)
{
    if (cell_is_solid(beam.target))
    {
        canned_msg(MSG_UNTHINKING_ACT);
        return spret::abort;
    }

    monster* mons = monster_at(beam.target);
    if (!mons || mons->submerged())
    {
        fail_check();
        canned_msg(MSG_SPELL_FIZZLES);
        return spret::success; // still losing a turn
    }

    if (actor_cloud_immune(*mons, CLOUD_POISON) && mons->observable())
    {
        mprf("But poisonous vapours would do no harm to %s!",
             mons->name(DESC_THE).c_str());
        return spret::abort;
    }

    if (stop_attack_prompt(mons, false, you.pos()))
        return spret::abort;

    cloud_struct* cloud = cloud_at(beam.target);
    if (cloud && cloud->type != CLOUD_POISON)
    {
        // XXX: consider replacing the cloud instead?
        mpr("There's already a cloud there!");
        return spret::abort;
    }

    fail_check();

    const int cloud_duration = max(random2(pow + 1) / 10, 1); // in dekaauts
    if (cloud)
    {
        // Reinforce the cloud.
        mpr("The poisonous vapours increase!");
        cloud->decay += cloud_duration * 10; // in this case, we're using auts
        cloud->set_whose(KC_YOU);
    }
    else
    {
        place_cloud(CLOUD_POISON, beam.target, cloud_duration, &you);
        mprf("Poisonous vapours surround %s!", mons->name(DESC_THE).c_str());
    }

    behaviour_event(mons, ME_WHACK, &you);

    return spret::success;
}

spret cast_big_c(int pow, spell_type spl, const actor *caster, bolt &beam,
                      bool fail)
{
    if (grid_distance(beam.target, you.pos()) > beam.range
        || !in_bounds(beam.target))
    {
        mpr("That is beyond the maximum range.");
        return spret::abort;
    }

    if (cell_is_solid(beam.target))
    {
        const char *feat = feat_type_name(grd(beam.target));
        mprf("You can't place clouds on %s.", article_a(feat).c_str());
        return spret::abort;
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
            cty = CLOUD_HOLY;
            break;
        case SPELL_FREEZING_CLOUD:
            beam.flavour = BEAM_COLD;
            beam.name = "freezing blast";
            cty = CLOUD_COLD;
            break;
        default:
            mpr("That kind of cloud doesn't exist!");
            return spret::abort;
    }

    beam.thrower           = KILL_YOU;
    beam.hit               = AUTOMATIC_HIT;
    beam.damage            = CONVENIENT_NONZERO_DAMAGE;
    beam.is_tracer         = true;
    beam.use_target_as_pos = true;
    beam.origin_spell      = spl;
    beam.affect_endpoint();
    if (beam.beam_cancelled)
        return spret::abort;

    fail_check();

    big_cloud(cty, caster, beam.target, pow, 8 + random2(3), -1);
    noisy(spell_effect_noise(spl), beam.target);
    return spret::success;
}

static int _make_a_normal_cloud(coord_def where, int pow, int spread_rate,
                                cloud_type ctype, const actor *agent,
                                int excl_rad)
{
    place_cloud(ctype, where,
                (3 + random2(pow / 4) + random2(pow / 4) + random2(pow / 4)),
                agent, spread_rate, excl_rad);

    return 1;
}

void big_cloud(cloud_type cl_type, const actor *agent,
               const coord_def& where, int pow, int size, int spread_rate)
{
    // The starting point _may_ be a place no cloud can be placed on.
    apply_area_cloud(_make_a_normal_cloud, where, pow, size,
                     cl_type, agent, spread_rate, -1);
}

spret cast_ring_of_flames(int power, bool fail)
{
    fail_check();
    you.increase_duration(DUR_FIRE_SHIELD,
                          6 + (power / 10) + (random2(power) / 5), 50,
                          "The air around you leaps into flame!");
    manage_fire_shield();
    return spret::success;
}

void manage_fire_shield()
{
    ASSERT(you.duration[DUR_FIRE_SHIELD]);

    // Melt ice armour entirely.
    maybe_melt_player_enchantments(BEAM_FIRE, 100);

    // Remove fire clouds on top of you
    if (cloud_at(you.pos()) && cloud_at(you.pos())->type == CLOUD_FIRE)
        delete_cloud(you.pos());

    // Place fire clouds all around you
    for (adjacent_iterator ai(you.pos()); ai; ++ai)
        if (!cell_is_solid(*ai) && !cloud_at(*ai))
            place_cloud(CLOUD_FIRE, *ai, 1 + random2(6), &you);
}

spret cast_corpse_rot(bool fail)
{
    fail_check();
    corpse_rot(&you);
    return spret::success;
}

void corpse_rot(actor* caster)
{
    // If there is no caster (god wrath), centre the effect on the player.
    const coord_def center = caster ? caster->pos() : you.pos();
    bool saw_rot = false;
    int did_rot = 0;

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

                    if (!saw_rot && you.see_cell(*ri))
                        saw_rot = true;

                    ++did_rot;

                    // Don't look for more corpses here.
                    break;
                }
    }

    for (fair_adjacent_iterator ai(center); ai; ++ai)
    {
        if (did_rot == 0)
            break;

        if (cell_is_solid(*ai))
            continue;

        place_cloud(CLOUD_MIASMA, *ai, 2+random2avg(8, 2),caster);
        --did_rot;
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

        place_cloud(CLOUD_HOLY, *ai, dur, caster);

        cloud_count++;
    }

    if (cloud_count)
    {
        if (defender->is_player())
            mpr("Blessed fire suddenly surrounds you!");
        else
            simple_monster_message(*defender->as_monster(),
                                   " is surrounded by blessed fire!");
    }
}

random_pick_entry<cloud_type> cloud_cone_clouds[] =
{
  { 0,   50, 200, FALL, CLOUD_MEPHITIC },
  { 0,  100, 125, PEAK, CLOUD_FIRE },
  { 0,  100, 125, PEAK, CLOUD_COLD },
  { 0,  100, 125, PEAK, CLOUD_POISON },
  { 30, 100, 125, RISE, CLOUD_NEGATIVE_ENERGY },
  { 40, 100, 135, RISE, CLOUD_STORM },
  { 50, 100, 175, RISE, CLOUD_ACID },
  { 0,0,0,FLAT,CLOUD_NONE }
};

spret cast_cloud_cone(const actor *caster, int pow, const coord_def &pos,
                           bool fail)
{
    if (env.level_state & LSTATE_STILL_WINDS)
    {
        if (caster->is_player())
            mpr("The air is too still to form clouds.");
        return spret::abort;
    }

    // For monsters:
    pow = min(100, pow);

    const int range = spell_range(SPELL_CLOUD_CONE, pow);

    targeter_shotgun hitfunc(caster, CLOUD_CONE_BEAM_COUNT, range);

    hitfunc.set_aim(pos);

    if (caster->is_player())
    {
        if (stop_attack_prompt(hitfunc, "cloud"))
            return spret::abort;
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

    return spret::success;
}
