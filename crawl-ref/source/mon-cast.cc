/**
 * @file
 * @brief Monster spell casting.
**/

#include "AppHdr.h"

#include "mon-cast.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

#include "act-iter.h"
#include "areas.h"
#include "bloodspatter.h"
#include "branch.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "database.h"
#include "delay.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "evoke.h"
#include "exclude.h"
#include "fight.h"
#include "fprop.h"
#include "godpassive.h"
#include "items.h"
#include "libutil.h"
#include "losglobal.h"
#include "mapmark.h"
#include "message.h"
#include "misc.h"
#include "mon-act.h"
#include "mon-behv.h"
#include "mon-book.h"
#include "mon-clone.h"
#include "mon-death.h"
#include "mon-gear.h"
#include "mon-pathfind.h"
#include "mon-pick.h"
#include "mon-place.h"
#include "mon-project.h"
#include "mon-speak.h"
#include "mon-tentacle.h"
#include "mutation.h"
#include "player-stats.h"
#include "random.h"
#include "religion.h"
#include "shout.h"
#include "showsymb.h"
#include "spl-clouds.h"
#include "spl-damage.h"
#include "spl-goditem.h"
#include "spl-monench.h"
#include "spl-selfench.h"
#include "spl-summoning.h"
#include "spl-transloc.h"
#include "spl-util.h"
#include "spl-zap.h" // spell_to_zap
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "target.h"
#include "teleport.h"
#include "terrain.h"
#ifdef USE_TILE
#include "tiledef-dngn.h"
#endif
#include "timed_effects.h"
#include "traps.h"
#include "travel.h"
#include "view.h"
#include "viewchar.h"
#include "xom.h"

static bool _valid_mon_spells[NUM_SPELLS];

static int  _mons_mesmerise(monster* mons, bool actual = true);
static int  _mons_cause_fear(monster* mons, bool actual = true);
static int  _mons_mass_confuse(monster* mons, bool actual = true);
static int  _mons_control_undead(monster* mons, bool actual = true);
static coord_def _mons_fragment_target(monster *mons);
static coord_def _mons_conjure_flame_pos(monster* mon, actor* foe);
static coord_def _mons_prism_pos(monster* mon, actor* foe);
static coord_def _mons_awaken_earth_target(monster& mon);
static bool _mons_consider_throwing(const monster &mons);
static bool _throw(const monster &thrower, actor &victim, int pow);
static void _maybe_throw_ally(const monster &mons);
static int _throw_site_score(const monster &thrower, const actor &victim,
                             const coord_def &site);
static void _siren_sing(monster* mons, bool avatar);
static void _doom_howl(monster &mon);
static void _mons_awaken_earth(monster &mon, const coord_def &target);
static bool _ms_waste_of_time(monster* mon, mon_spell_slot slot);
static string _god_name(god_type god);

void init_mons_spells()
{
    monster fake_mon;
    fake_mon.type       = MONS_BLACK_DRACONIAN;
    fake_mon.hit_points = 1;

    bolt pbolt;

    for (int i = 0; i < NUM_SPELLS; i++)
    {
        spell_type spell = (spell_type) i;

        _valid_mon_spells[i] = false;

        if (!is_valid_spell(spell))
            continue;

        if (
#if TAG_MAJOR_VERSION == 34
            spell == SPELL_MELEE ||
#endif
            setup_mons_cast(&fake_mon, pbolt, spell, true))
        {
            _valid_mon_spells[i] = true;
        }
    }
}

bool is_valid_mon_spell(spell_type spell)
{
    if (spell < 0 || spell >= NUM_SPELLS)
        return false;

    return _valid_mon_spells[spell];
}

static bool _flavour_benefits_monster(beam_type flavour, monster& monster)
{
    switch (flavour)
    {
    case BEAM_HASTE:
        return !monster.has_ench(ENCH_HASTE);

    case BEAM_MIGHT:
        return !monster.has_ench(ENCH_MIGHT);

    case BEAM_INVISIBILITY:
        return !monster.has_ench(ENCH_INVIS);

    case BEAM_HEALING:
        return monster.hit_points != monster.max_hit_points;

    case BEAM_AGILITY:
        return !monster.has_ench(ENCH_AGILE);

    case BEAM_RESISTANCE:
        return !monster.has_ench(ENCH_RESISTANCE);

    default:
        return false;
    }
}

// Find an allied monster to cast a beneficial beam spell at.
static bool _set_allied_target(monster* caster, bolt& pbolt, bool ignore_genus)
{
    monster* selected_target = nullptr;
    int min_distance = INT_MAX;

    const monster_type caster_genus = mons_genus(caster->type);

    for (monster_near_iterator targ(caster, LOS_NO_TRANS); targ; ++targ)
    {
        if (*targ == caster)
            continue;

        const int targ_distance = grid_distance(targ->pos(), caster->pos());

        bool got_target = false;

        if ((mons_genus(targ->type) == caster_genus
                 || mons_genus(targ->base_monster) == caster_genus
                 || caster_genus == MONS_SALAMANDER        // Let mystics haste
                    && mons_genus(targ->type) == MONS_NAGA // their brethren
                 || targ->is_holy() && caster->is_holy()
                 || mons_enslaved_soul(caster)
                 || ignore_genus)
            && mons_aligned(*targ, caster)
            && !targ->has_ench(ENCH_CHARM)
            && !targ->has_ench(ENCH_HEXED)
            && !mons_is_firewood(*targ)
            && _flavour_benefits_monster(pbolt.flavour, **targ))
        {
            got_target = true;
        }

        if (got_target && targ_distance < min_distance
            && targ_distance < pbolt.range)
        {
            // Make sure we won't hit an invalid target with this aim.
            pbolt.target = targ->pos();
            fire_tracer(caster, pbolt);
            if (!mons_should_fire(pbolt)
                || pbolt.path_taken.back() != pbolt.target)
            {
                continue;
            }

            min_distance = targ_distance;
            selected_target = *targ;
        }
    }

    if (selected_target)
    {
        pbolt.target = selected_target->pos();
        return true;
    }

    // Didn't find a target.
    return false;
}

// Find an ally of the target to cast a hex at.
// Note that this deliberately does not target the player.
static bool _set_hex_target(monster* caster, bolt& pbolt)
{
    monster* selected_target = nullptr;
    int min_distance = INT_MAX;

    if (!caster->get_foe())
        return false;

    const actor *foe = caster->get_foe();

    for (monster_near_iterator targ(caster, LOS_NO_TRANS); targ; ++targ)
    {
        if (*targ == caster)
            continue;

        const int targ_distance = grid_distance(targ->pos(), foe->pos());

        bool got_target = false;

        if (mons_aligned(*targ, foe)
            && !targ->has_ench(ENCH_CHARM)
            && !targ->has_ench(ENCH_HEXED)
            && !mons_is_firewood(*targ)
            && !_flavour_benefits_monster(pbolt.flavour, **targ))
        {
            got_target = true;
        }

        if (got_target && targ_distance < min_distance
            && targ_distance < pbolt.range)
        {
            // Make sure we won't hit an invalid target with this aim.
            pbolt.target = targ->pos();
            fire_tracer(caster, pbolt);
            if (!mons_should_fire(pbolt)
                || pbolt.path_taken.back() != pbolt.target)
            {
                continue;
            }

            min_distance = targ_distance;
            selected_target = *targ;
        }
    }

    if (selected_target)
    {
        pbolt.target = selected_target->pos();
        return true;
    }

    // Didn't find a target.
    return false;
}

/**
 * What value do monsters multiply their hd with to get spellpower, for the
 * given spell?
 *
 * XXX: everything except SPELL_CONFUSION_GAZE could be trivially exported to
 * data.
 *
 * @param spell     The spell in question.
 * @param random    Whether to randomize powers for weird spells.
 *                  If false, the average value is used.
 * @return          A multiplier to HD for spellpower.
 *                  Value may exceed 200.
 */
static int _mons_power_hd_factor(spell_type spell, bool random)
{
    switch (spell)
    {
        case SPELL_CONFUSION_GAZE:
            if (random)
                return 5 * (2 + random2(3)) * ENCH_POW_FACTOR;
            return 5 * (2 + 1) * ENCH_POW_FACTOR;

        case SPELL_CAUSE_FEAR:
            return 18 * ENCH_POW_FACTOR;

        case SPELL_SENTINEL_MARK:
            return 16 * ENCH_POW_FACTOR;

        case SPELL_SAP_MAGIC:
        case SPELL_MESMERISE:
            return 10 * ENCH_POW_FACTOR;

        case SPELL_MASS_CONFUSION:
            return 8 * ENCH_POW_FACTOR;

        case SPELL_SLEEP:
        case SPELL_AGONY:
        case SPELL_STRIP_RESISTANCE:
        case SPELL_VIRULENCE:
        case SPELL_DRAIN_MAGIC:
        case SPELL_DIMENSION_ANCHOR:
            return 6 * ENCH_POW_FACTOR;

        case SPELL_ABJURATION:
            return 20;

        case SPELL_OLGREBS_TOXIC_RADIANCE:
        case SPELL_FULMINANT_PRISM:
            return 8;

        case SPELL_LRD:
        case SPELL_MONSTROUS_MENAGERIE:
        case SPELL_BATTLESPHERE:
        case SPELL_SPECTRAL_WEAPON:
        case SPELL_IGNITE_POISON:
        case SPELL_IOOD:
        case SPELL_CONJURE_FLAME:
            return 6;

        case SPELL_OZOCUBUS_REFRIGERATION:
        case SPELL_SUMMON_DRAGON:
        case SPELL_SUMMON_HYDRA:
            return 5;

        case SPELL_CHAIN_LIGHTNING:
        case SPELL_CHAIN_OF_CHAOS:
            return 4;

        case SPELL_DRAIN_LIFE:
            return 1;

        default:
            return 12;
    }
}


/**
 * What spellpower does a monster with the given spell_hd cast the given spell
 * at?
 *
 * @param spell     The spell in question.
 * @param hd        The spell_hd of the given monster.
 * @param random    Whether to randomize powers for weird spells.
 *                  If false, the average value is used.
 * @return          A spellpower value for the spell.
 */
int mons_power_for_hd(spell_type spell, int hd, bool random)
{
    const int power = hd * _mons_power_hd_factor(spell, random);
    if (spell == SPELL_PAIN)
        return max(50 * ENCH_POW_FACTOR, power);
    return power;
}

/**
 * What power does the given monster cast the given spell with?
 *
 * @param spell     The spell in question.
 * @param mons      The monster in question.
 * @return          A spellpower value for the spell.
 *                  May vary from call to call for certain weird spells.
 */
static int _mons_spellpower(spell_type spell, const monster &mons)
{
    return mons_power_for_hd(spell, mons.spell_hd(spell));
}

/**
 * What power does the given monster cast the given enchantment with?
 *
 * @param spell     The spell in question.
 * @param mons      The monster in question.
 * @param cap       The maximum power of the spell.
 * @return          A spellpower value for the spell, with ENCH_POW_FACTOR
 *                  removed & capped at maximum spellpower.
 */
static int _ench_power(spell_type spell, const monster &mons)
{
    const int cap = 200;
    return min(cap, _mons_spellpower(spell, mons) / ENCH_POW_FACTOR);
}

static int _mons_spell_range(spell_type spell, const monster &mons)
{
    return mons_spell_range(spell, _mons_spellpower(spell, mons));
}

/**
 * How much range does a monster of the given spell HD have with the given
 * spell?
 *
 * @param spell     The spell in question.
 * @param hd        The monster's effective HD for spellcasting purposes.
 * @return          -1 if the spell has an undefined range; else its range.
 */
int mons_spell_range(spell_type spell, int hd)
{
    switch (spell)
    {
        case SPELL_SANDBLAST:
            return 2; // spell_range changes with player wielded items
        case SPELL_FLAME_TONGUE:
            // HD:1 monsters would get range 2, HD:2 -- 3, other 4, let's
            // use the mighty Throw Flame for big ranges.
            return min(2, hd);
        default:
            break;
    }

    const int power = mons_power_for_hd(spell, hd);
    return spell_range(spell, power, false);
}

bolt mons_spell_beam(monster* mons, spell_type spell_cast, int power,
                     bool check_validity)
{
    ASSERT(power > 0);

    bolt beam;

    // Initialise to some bogus values so we can catch problems.
    beam.name         = "****";
    beam.colour       = 255;
    beam.hit          = -1;
    beam.damage       = dice_def(1, 0);
    beam.ench_power   = max(1, power / ENCH_POW_FACTOR); // U G H
    beam.glyph        = 0;
    beam.flavour      = BEAM_NONE;
    beam.thrower      = KILL_MISC;
    beam.pierce       = false;
    beam.is_explosion = false;
    beam.attitude     = mons_attitude(mons);

    beam.range = _mons_spell_range(spell_cast, *mons);

    spell_type real_spell = spell_cast;

    if (spell_cast == SPELL_MAJOR_DESTRUCTION)
    {
        real_spell = random_choose(SPELL_BOLT_OF_FIRE,
                                   SPELL_FIREBALL,
                                   SPELL_LIGHTNING_BOLT,
                                   SPELL_STICKY_FLAME,
                                   SPELL_IRON_SHOT,
                                   SPELL_BOLT_OF_DRAINING,
                                   SPELL_ORB_OF_ELECTRICITY);
    }
    else if (spell_cast == SPELL_RANDOM_BOLT)
    {
        real_spell = random_choose(SPELL_VENOM_BOLT,
                                   SPELL_BOLT_OF_DRAINING,
                                   SPELL_BOLT_OF_FIRE,
                                   SPELL_LIGHTNING_BOLT,
                                   SPELL_QUICKSILVER_BOLT,
                                   SPELL_CORROSIVE_BOLT);
    }
    else if (spell_cast == SPELL_LEGENDARY_DESTRUCTION)
    {
        // ones with ranges too small are fixed in setup_mons_cast
        real_spell = random_choose_weighted(10, SPELL_ORB_OF_ELECTRICITY,
                                            10, SPELL_LEHUDIBS_CRYSTAL_SPEAR,
                                             2, SPELL_IOOD,
                                             5, SPELL_GHOSTLY_FIREBALL,
                                            10, SPELL_FIREBALL,
                                            10, SPELL_FLASH_FREEZE,
                                             0);
    }
    else if (spell_cast == SPELL_SERPENT_OF_HELL_BREATH)
    {
        // this will be fixed up later in mons_cast
        real_spell = SPELL_FIRE_BREATH;
    }
    beam.glyph = dchar_glyph(DCHAR_FIRED_ZAP); // default
    beam.thrower = KILL_MON_MISSILE;
    beam.origin_spell = real_spell;
    beam.source_id = mons->mid;
    beam.source_name = mons->name(DESC_A, true);

    // FIXME: more of these should use the zap_data[] struct from beam.cc!
    switch (real_spell)
    {
    case SPELL_ORB_OF_ELECTRICITY:
        beam.foe_ratio      = random_range(40, 55); // ...
        // fallthrough to other zaps
    case SPELL_MAGIC_DART:
    case SPELL_THROW_FLAME:
    case SPELL_THROW_FROST:
    case SPELL_FLAME_TONGUE:
    case SPELL_VENOM_BOLT:
    case SPELL_POISON_ARROW:
    case SPELL_BOLT_OF_MAGMA:
    case SPELL_BOLT_OF_FIRE:
    case SPELL_BOLT_OF_COLD:
    case SPELL_THROW_ICICLE:
    case SPELL_BOLT_OF_INACCURACY:
    case SPELL_SHOCK:
    case SPELL_LIGHTNING_BOLT:
    case SPELL_FIREBALL:
    case SPELL_LEHUDIBS_CRYSTAL_SPEAR:
    case SPELL_BOLT_OF_DRAINING:
    case SPELL_ISKENDERUNS_MYSTIC_BLAST:
    case SPELL_STICKY_FLAME:
    case SPELL_STICKY_FLAME_SPLASH:
    case SPELL_STICKY_FLAME_RANGE:
    case SPELL_STING:
    case SPELL_IRON_SHOT:
    case SPELL_STONE_ARROW:
    case SPELL_FORCE_LANCE:
    case SPELL_EXPLOSIVE_BOLT:
    case SPELL_CORROSIVE_BOLT:
    case SPELL_PARALYSE:
    case SPELL_PETRIFY:
    case SPELL_SLOW:
    case SPELL_HASTE:
    case SPELL_CORONA:
    case SPELL_CONFUSE:
    case SPELL_HIBERNATION:
    case SPELL_SLEEP:
    case SPELL_POLYMORPH:
    case SPELL_DIG:
    case SPELL_AGONY:
    case SPELL_BANISHMENT:
    case SPELL_ENSLAVEMENT:
    case SPELL_SLUG_DART:
        zappy(spell_to_zap(real_spell), power, true, beam);
        break;

    case SPELL_DAZZLING_SPRAY: // special-cased because of a spl-zap hack...
        zappy(ZAP_DAZZLING_SPRAY, power, true, beam);
        break;

    case SPELL_SANDBLAST:
        beam.colour   = BROWN;
        beam.name     = "rocky blast";
        beam.damage   = dice_def(3, 5 + (power / 40));
        beam.hit      = 20 + power / 40;
        beam.flavour  = BEAM_FRAG;
        break;

    case SPELL_DISPEL_UNDEAD:
        beam.flavour  = BEAM_DISPEL_UNDEAD;
        beam.damage   = dice_def(3, min(6 + power / 10, 40));
        break;

    case SPELL_HASTE_OTHER:
        beam.flavour  = BEAM_HASTE;
        break;

    case SPELL_MIGHT_OTHER:
    case SPELL_MIGHT:
        beam.flavour  = BEAM_MIGHT;
        break;

    case SPELL_MALMUTATE:
        beam.flavour  = BEAM_MALMUTATE;
        break;

    case SPELL_PRIMAL_WAVE:
        beam.name     = "great wave of water";
        // Water attack is weaker than the pure elemental damage
        // attacks, but also less resistible.
        beam.damage   = dice_def(3, 6 + power / 12);
        beam.colour   = LIGHTBLUE;
        beam.flavour  = BEAM_WATER;
        // Huge wave of water is hard to dodge.
        beam.hit      = 20 + power / 20;
        beam.pierce   = false;
        beam.glyph    = dchar_glyph(DCHAR_WAVY);
        break;

    case SPELL_FREEZING_CLOUD:
        beam.name     = "freezing blast";
        beam.damage   = dice_def(2, 9 + power / 11);
        beam.colour   = WHITE;
        beam.flavour  = BEAM_COLD;
        beam.hit      = 17 + power / 25;
        beam.pierce   = true;
        break;

    case SPELL_BLINKBOLT:
        beam.name     = "living lightning";
        beam.damage   = dice_def(2, 10 + power / 17);
        beam.colour   = LIGHTCYAN;
        beam.flavour  = BEAM_ELECTRICITY;
        beam.hit      = 16 + power / 40;
        beam.pierce   = true;
        break;

    case SPELL_INVISIBILITY:
    case SPELL_INVISIBILITY_OTHER:
        beam.flavour  = BEAM_INVISIBILITY;
        break;

    case SPELL_FIRE_STORM:
        setup_fire_storm(mons, power / 2, beam);
        beam.foe_ratio = random_range(40, 55);
        break;

    case SPELL_CALL_DOWN_DAMNATION:
        beam.aux_source   = "damnation";
        beam.name         = "damnation";
        beam.ex_size      = 1;
        beam.flavour      = BEAM_DAMNATION;
        beam.is_explosion = true;
        beam.colour       = LIGHTRED;
        beam.aux_source.clear();
        beam.is_tracer    = false;
        beam.hit          = 20;
        beam.damage       = dice_def(3, 15);
        break;

    case SPELL_HEAL_OTHER:
    case SPELL_MINOR_HEALING:
        beam.damage   = dice_def(2, mons->spell_hd(real_spell) / 2);
        beam.flavour  = BEAM_HEALING;
        beam.hit      = 25 + (power / 5);
        break;

    case SPELL_TELEPORT_SELF:
        beam.flavour    = BEAM_TELEPORT;
        beam.ench_power = 2000;
        break;

    case SPELL_TELEPORT_OTHER:
        beam.flavour  = BEAM_TELEPORT;
        break;

    case SPELL_STEAM_BALL:
        beam.colour   = LIGHTGREY;
        beam.name     = "ball of steam";
        beam.damage   = dice_def(3, 7 + (power / 15));
        beam.hit      = 20 + power / 20;
        beam.flavour  = BEAM_STEAM;
        break;

    case SPELL_PAIN:
        beam.flavour    = BEAM_PAIN;
        beam.damage     = dice_def(1, 7 + (power / 20));
        break;

    case SPELL_NOXIOUS_CLOUD:
        beam.name     = "noxious blast";
        beam.damage   = dice_def(1, 0);
        beam.colour   = GREEN;
        beam.flavour  = BEAM_MEPHITIC;
        beam.hit      = 18 + power / 25;
        beam.pierce   = true;
        break;

    case SPELL_POISONOUS_CLOUD:
        beam.name     = "blast of poison";
        beam.damage   = dice_def(3, 3 + power / 25);
        beam.colour   = LIGHTGREEN;
        beam.flavour  = BEAM_POISON;
        beam.hit      = 18 + power / 25;
        beam.pierce   = true;
        break;

    case SPELL_ENERGY_BOLT:        // eye of devastation
        beam.colour     = YELLOW;
        beam.name       = "bolt of energy";
        beam.short_name = "energy";
        beam.damage     = dice_def(3, 20);
        beam.hit        = 15 + power / 30;
        beam.flavour    = BEAM_DEVASTATION; // DEVASTATION is BEAM_MMISSILE
        beam.pierce     = true;             // except it also destroys walls
        break;

    case SPELL_SPIT_POISON:
        beam.colour   = GREEN;
        beam.name     = "splash of poison";
        beam.damage   = dice_def(1, 4 + power / 10);
        beam.hit      = 16 + power / 20;
        beam.flavour  = BEAM_POISON;
        break;

    case SPELL_SPIT_ACID:
        beam.colour   = YELLOW;
        beam.name     = "splash of acid";
        beam.damage   = dice_def(3, 7);

        // Natural ability, so don't use spell_hd here
        beam.hit      = 20 + (3 * mons->get_hit_dice());
        beam.flavour  = BEAM_ACID;
        break;

    case SPELL_DISINTEGRATE:
        beam.flavour    = BEAM_DISINTEGRATION;
        beam.ench_power = 50;
        beam.damage     = dice_def(1, 30 + (power / 10));
        break;

    case SPELL_MEPHITIC_CLOUD:
        beam.name     = "stinking cloud";
        beam.damage   = dice_def(1, 0);
        beam.colour   = GREEN;
        beam.flavour  = BEAM_MEPHITIC;
        beam.hit      = 14 + power / 30;
        beam.is_explosion = true;
        break;

    case SPELL_MIASMA_BREATH:      // death drake
        beam.name     = "foul vapour";
        beam.damage   = dice_def(3, 5 + power / 24);
        beam.colour   = DARKGREY;
        beam.flavour  = BEAM_MIASMA;
        beam.hit      = 17 + power / 20;
        beam.pierce   = true;
        break;

    case SPELL_QUICKSILVER_BOLT:   // Quicksilver dragon and purple draconian
        beam.colour     = random_colour();
        beam.name       = "bolt of dispelling energy";
        beam.short_name = "dispelling energy";
        beam.damage     = dice_def(3, 20);
        beam.hit        = 16 + power / 25;
        beam.flavour    = BEAM_MMISSILE;
        break;

    case SPELL_HURL_DAMNATION:           // fiend's damnation
        beam.name         = "damnation";
        beam.aux_source   = "damnation";
        beam.colour       = LIGHTRED;
        beam.damage       = dice_def(3, 20);
        beam.hit          = 24;
        beam.flavour      = BEAM_DAMNATION;
        beam.pierce       = true; // needed?
        beam.is_explosion = true;
        break;

    case SPELL_METAL_SPLINTERS:
        beam.name       = "spray of metal splinters";
        beam.short_name = "metal splinters";
        beam.damage     = dice_def(3, 20 + power / 20);
        beam.colour     = CYAN;
        beam.flavour    = BEAM_FRAG;
        beam.hit        = 19 + power / 30;
        beam.pierce     = true;
        break;

    case SPELL_BLINK_OTHER:
        beam.flavour    = BEAM_BLINK;
        break;

    case SPELL_BLINK_OTHER_CLOSE:
        beam.flavour    = BEAM_BLINK_CLOSE;
        break;

    case SPELL_FIRE_BREATH:
    case SPELL_SEARING_BREATH:
        beam.name       = "blast of flame";
        beam.aux_source = "blast of fiery breath";
        beam.short_name = "flames";
        beam.damage     = dice_def(3, (mons->get_hit_dice() * 2));
        beam.colour     = RED;
        beam.hit        = 30;
        beam.flavour    = BEAM_FIRE;
        beam.pierce     = true;
        if (real_spell == SPELL_SEARING_BREATH)
        {
            beam.name        = "searing blast";
            beam.aux_source  = "blast of searing breath";
            if (mons->type != MONS_XTAHUA)
                beam.damage.size = 65 * beam.damage.size / 100;
        }
        break;

    case SPELL_CHAOS_BREATH:
        beam.name         = "blast of chaos";
        beam.aux_source   = "blast of chaotic breath";
        beam.damage       = dice_def(1, 3 * mons->get_hit_dice() / 2);
        beam.colour       = ETC_RANDOM;
        beam.hit          = 30;
        beam.flavour      = BEAM_CHAOS;
        beam.pierce       = true;
        break;

    case SPELL_CHILLING_BREATH:
    case SPELL_COLD_BREATH:
        beam.name       = "blast of cold";
        beam.aux_source = "blast of icy breath";
        beam.short_name = "frost";
        beam.damage     = dice_def(3, (mons->get_hit_dice() * 2));
        beam.colour     = WHITE;
        beam.hit        = 30;
        beam.flavour    = BEAM_COLD;
        beam.pierce     = true;
        if (real_spell == SPELL_CHILLING_BREATH)
        {
            beam.name        = "chilling blast";
            beam.aux_source  = "blast of chilling breath";
            beam.short_name  = "frost";
            beam.damage.size = 65 * beam.damage.size / 100;
        }
        break;

    case SPELL_HOLY_BREATH:
        beam.name     = "blast of cleansing flame";
        beam.damage   = dice_def(3, (mons->get_hit_dice() * 2));
        beam.colour   = ETC_HOLY;
        beam.flavour  = BEAM_HOLY;
        beam.hit      = 18 + power / 25;
        beam.pierce   = true;
        break;

    case SPELL_PORKALATOR:
        beam.name     = "porkalator";
        beam.glyph    = 0;
        beam.flavour  = BEAM_PORKALATOR;
        beam.thrower  = KILL_MON_MISSILE;
        break;

    case SPELL_IOOD:                  // tracer only
    case SPELL_LRD:                   // for noise generation purposes
    case SPELL_PORTAL_PROJECTILE:     // ditto
    case SPELL_GLACIATE:              // ditto
    case SPELL_CLOUD_CONE:            // ditto
    case SPELL_CONJURE_FLAME:         // ditto
    case SPELL_FULMINANT_PRISM:       // ditto
    case SPELL_SCATTERSHOT:           // ditto
    case SPELL_AWAKEN_EARTH:          // ditto
        beam.flavour  = BEAM_DEVASTATION;
        beam.pierce   = true;
        // Doesn't take distance into account, but this is just a tracer so
        // we'll ignore that. We need some damage on the tracer so the monster
        // doesn't think the spell is useless against other monsters.
        beam.damage   = dice_def(42, 1);
        beam.range    = LOS_RADIUS;
        break;

    case SPELL_PETRIFYING_CLOUD:
        beam.name     = "blast of calcifying dust";
        beam.colour   = WHITE;
        beam.damage   = dice_def(2, 6);
        beam.hit      = AUTOMATIC_HIT;
        beam.flavour  = BEAM_PETRIFYING_CLOUD;
        beam.foe_ratio = 30;
        beam.pierce   = true;
        break;

#if TAG_MAJOR_VERSION == 34
    case SPELL_HOLY_LIGHT:
    case SPELL_SILVER_BLAST:
        beam.name     = "beam of golden light";
        beam.damage   = dice_def(3, 8 + power / 11);
        beam.colour   = ETC_HOLY;
        beam.flavour  = BEAM_HOLY;
        beam.hit      = 17 + power / 25;
        beam.pierce   = true;
        break;
#endif

    case SPELL_ENSNARE:
        beam.name     = "stream of webbing";
        beam.colour   = WHITE;
        beam.glyph    = dchar_glyph(DCHAR_FIRED_MISSILE);
        beam.flavour  = BEAM_ENSNARE;
        beam.hit      = 22 + power / 20;
        break;

    case SPELL_SENTINEL_MARK:
        beam.flavour    = BEAM_SENTINEL_MARK;
        break;

    case SPELL_SPECTRAL_CLOUD:
        beam.name     = "spectral mist";
        beam.damage   = dice_def(0, 1);
        beam.colour   = CYAN;
        beam.flavour  = BEAM_GHOSTLY_FLAME;
        beam.hit      = AUTOMATIC_HIT;
        beam.pierce   = true;
        break;

    case SPELL_GHOSTLY_FIREBALL:
        beam.colour   = CYAN;
        beam.name     = "ghostly fireball";
        beam.damage   = dice_def(3, 7 + power / 13);
        beam.hit      = 40;
        beam.flavour  = BEAM_GHOSTLY_FLAME;
        beam.is_explosion = true;
        break;

    case SPELL_DIMENSION_ANCHOR:
        beam.flavour    = BEAM_DIMENSION_ANCHOR;
        break;

    case SPELL_THORN_VOLLEY:
        beam.colour   = BROWN;
        beam.name     = "volley of thorns";
        beam.damage   = dice_def(3, 5 + (power / 13));
        beam.hit      = 20 + power / 15;
        beam.flavour  = BEAM_MMISSILE;
        break;

    case SPELL_STRIP_RESISTANCE:
        beam.flavour    = BEAM_VULNERABILITY;
        break;

    // XXX: This seems needed to give proper spellcasting messages, even though
    //      damage is done via another means
    case SPELL_FREEZE:
        beam.flavour    = BEAM_COLD;
        break;

    case SPELL_MALIGN_OFFERING:
        beam.flavour    = BEAM_MALIGN_OFFERING;
        beam.damage     = dice_def(2, 7 + (power / 13));
        break;

    case SPELL_VIRULENCE:
        beam.flavour    = BEAM_VIRULENCE;
        break;

    case SPELL_FLASH_FREEZE:
        beam.name     = "flash freeze";
        beam.damage   = dice_def(3, 7 + (power / 12));
        beam.colour   = WHITE;
        beam.flavour  = BEAM_ICE;
        beam.hit      = 5 + power / 3;
        break;

    case SPELL_SAP_MAGIC:
        beam.flavour    = BEAM_SAP_MAGIC;
        break;

    case SPELL_CORRUPT_BODY:
        beam.flavour    = BEAM_CORRUPT_BODY;
        break;

    case SPELL_SHADOW_BOLT:
        beam.name     = "shadow bolt";
        beam.pierce   = true;
        // deliberate fall-through
    case SPELL_SHADOW_SHARD:
        if (real_spell == SPELL_SHADOW_SHARD)
            beam.name  = "shadow shard";
        beam.damage   = dice_def(3, 8 + power / 11);
        beam.colour   = MAGENTA;
        beam.flavour  = BEAM_MMISSILE;
        beam.hit      = 17 + power / 25;
        break;

    case SPELL_CRYSTAL_BOLT:
        beam.name     = "crystal bolt";
        beam.damage   = dice_def(3, 8 + power / 11);
        beam.colour   = GREEN;
        beam.flavour  = BEAM_CRYSTAL;
        beam.hit      = 17 + power / 25;
        beam.pierce   = true;
        break;

    case SPELL_DRAIN_MAGIC:
        beam.flavour    = BEAM_DRAIN_MAGIC;
        break;

    case SPELL_SPIT_LAVA:
        beam.name        = "glob of lava";
        beam.damage      = dice_def(3, 10);
        beam.hit         = 20;
        beam.colour      = RED;
        beam.glyph       = dchar_glyph(DCHAR_FIRED_ZAP);
        beam.flavour     = BEAM_LAVA;
        break;

    case SPELL_ELECTRICAL_BOLT:
        beam.name        = "bolt of electricity";
        beam.damage      = dice_def(3, 3 + mons->get_hit_dice());
        beam.hit         = 35;
        beam.colour      = LIGHTCYAN;
        beam.glyph       = dchar_glyph(DCHAR_FIRED_ZAP);
        beam.flavour     = BEAM_ELECTRICITY;
        beam.pierce      = true;
        break;

    case SPELL_FLAMING_CLOUD:
        beam.name         = "blast of flame";
        beam.aux_source   = "blast of fiery breath";
        beam.short_name   = "flames";
        beam.damage       = dice_def(1, 3 * mons->get_hit_dice() / 2);
        beam.colour       = RED;
        beam.hit          = 30;
        beam.flavour      = BEAM_FIRE;
        beam.pierce       = true;
        break;

    case SPELL_THROW_BARBS:
        beam.name        = "volley of spikes";
        beam.aux_source  = "volley of spikes";
        beam.hit_verb    = "skewers";
        beam.hit         = 27;
        beam.damage      = dice_def(2, 13);
        beam.glyph       = dchar_glyph(DCHAR_FIRED_MISSILE);
        beam.colour      = LIGHTGREY;
        beam.flavour     = BEAM_MISSILE;
        break;

    case SPELL_DEATH_RATTLE:
        beam.name     = "vile air";
        beam.colour   = DARKGREY;
        beam.damage   = dice_def(2, 4);
        beam.hit      = AUTOMATIC_HIT;
        beam.flavour  = BEAM_DEATH_RATTLE;
        beam.foe_ratio = 30;
        beam.pierce   = true;
        break;

    default:
        if (check_validity)
        {
            beam.flavour = NUM_BEAMS;
            return beam;
        }

        if (!is_valid_spell(real_spell))
        {
            die("Invalid spell #%d cast by %s", (int) real_spell,
                     mons->name(DESC_PLAIN, true).c_str());
        }

        die("Unknown monster spell '%s' cast by %s",
                 spell_title(real_spell),
                 mons->name(DESC_PLAIN, true).c_str());
    }

    if (beam.is_enchantment())
    {
        beam.hit = AUTOMATIC_HIT;
        beam.glyph = 0;
        if (spell_cast != SPELL_AGONY)
            beam.name = "";
    }

    return beam;
}

// Set up bolt structure for monster spell casting.
bool setup_mons_cast(monster* mons, bolt &pbolt, spell_type spell_cast,
                     bool check_validity)
{
    // always set these -- used by things other than fire_beam()

    pbolt.source_id = mons->mid;

    // Convenience for the hapless innocent who assumes that this
    // damn function does all possible setup. [ds]
    if (pbolt.target.origin())
        pbolt.target = mons->target;

    // Set bolt type and range.
    if (spell_is_direct_explosion(spell_cast))
    {
        pbolt.range = 0;
        pbolt.glyph = 0;
    }

    // The below are no-ops since they don't involve fire_tracer or beams.
    switch (spell_cast)
    {
    case SPELL_STICKS_TO_SNAKES:
    case SPELL_SUMMON_SMALL_MAMMAL:
    case SPELL_VAMPIRIC_DRAINING:
    case SPELL_INJURY_MIRROR:
    case SPELL_MAJOR_HEALING:
#if TAG_MAJOR_VERSION == 34
    case SPELL_VAMPIRE_SUMMON:
#endif
    case SPELL_SHADOW_CREATURES:       // summon anything appropriate for level
    case SPELL_WEAVE_SHADOWS:
#if TAG_MAJOR_VERSION == 34
    case SPELL_FAKE_RAKSHASA_SUMMON:
#endif
    case SPELL_FAKE_MARA_SUMMON:
    case SPELL_SUMMON_ILLUSION:
    case SPELL_SUMMON_DEMON:
    case SPELL_MONSTROUS_MENAGERIE:
#if TAG_MAJOR_VERSION == 34
    case SPELL_ANIMATE_DEAD:
#endif
    case SPELL_TWISTED_RESURRECTION:
    case SPELL_CIGOTUVIS_EMBRACE:
#if TAG_MAJOR_VERSION == 34
    case SPELL_SIMULACRUM:
#endif
    case SPELL_CALL_IMP:
    case SPELL_SUMMON_MINOR_DEMON:
#if TAG_MAJOR_VERSION == 34
    case SPELL_SUMMON_SCORPIONS:
    case SPELL_SUMMON_SWARM:
#endif
    case SPELL_SUMMON_UFETUBUS:
    case SPELL_SUMMON_HELL_BEAST:  // Geryon
    case SPELL_SUMMON_UNDEAD:
    case SPELL_SUMMON_ICE_BEAST:
    case SPELL_SUMMON_MUSHROOMS:
    case SPELL_CONJURE_BALL_LIGHTNING:
    case SPELL_SUMMON_DRAKES:
    case SPELL_SUMMON_HORRIBLE_THINGS:
    case SPELL_MALIGN_GATEWAY:
    case SPELL_SYMBOL_OF_TORMENT:
    case SPELL_CAUSE_FEAR:
    case SPELL_MESMERISE:
    case SPELL_DRAIN_LIFE:
    case SPELL_SUMMON_GREATER_DEMON:
    case SPELL_CANTRIP:
    case SPELL_BROTHERS_IN_ARMS:
    case SPELL_BERSERKER_RAGE:
    case SPELL_TROGS_HAND:
    case SPELL_SPRINT:
#if TAG_MAJOR_VERSION == 34
    case SPELL_SWIFTNESS:
    case SPELL_STONESKIN:
#endif
    case SPELL_WATER_ELEMENTALS:
    case SPELL_FIRE_ELEMENTALS:
    case SPELL_AIR_ELEMENTALS:
    case SPELL_EARTH_ELEMENTALS:
#if TAG_MAJOR_VERSION == 34
    case SPELL_IRON_ELEMENTALS:
    case SPELL_SUMMON_ELEMENTAL:
#endif
    case SPELL_CREATE_TENTACLES:
    case SPELL_BLINK:
    case SPELL_CONTROLLED_BLINK:
    case SPELL_BLINK_RANGE:
    case SPELL_BLINK_AWAY:
    case SPELL_BLINK_CLOSE:
    case SPELL_TOMB_OF_DOROKLOHE:
    case SPELL_CHAIN_LIGHTNING:    // the only user is reckless
    case SPELL_SUMMON_EYEBALLS:
    case SPELL_SUMMON_BUTTERFLIES:
#if TAG_MAJOR_VERSION == 34
    case SPELL_MISLEAD:
#endif
    case SPELL_CALL_TIDE:
    case SPELL_INK_CLOUD:
    case SPELL_SILENCE:
    case SPELL_AWAKEN_FOREST:
    case SPELL_DRUIDS_CALL:
    case SPELL_SUMMON_HOLIES:
    case SPELL_REGENERATION:
    case SPELL_CORPSE_ROT:
    case SPELL_LEDAS_LIQUEFACTION:
    case SPELL_SUMMON_DRAGON:
    case SPELL_SUMMON_HYDRA:
    case SPELL_FIRE_SUMMON:
    case SPELL_DEATHS_DOOR:
    case SPELL_OZOCUBUS_ARMOUR:
    case SPELL_OZOCUBUS_REFRIGERATION:
    case SPELL_OLGREBS_TOXIC_RADIANCE:
    case SPELL_SHATTER:
#if TAG_MAJOR_VERSION == 34
    case SPELL_FRENZY:
    case SPELL_SUMMON_TWISTER:
#endif
    case SPELL_BATTLESPHERE:
    case SPELL_SPECTRAL_WEAPON:
    case SPELL_WORD_OF_RECALL:
    case SPELL_INJURY_BOND:
    case SPELL_CALL_LOST_SOUL:
    case SPELL_BLINK_ALLIES_ENCIRCLE:
    case SPELL_MASS_CONFUSION:
    case SPELL_ENGLACIATION:
#if TAG_MAJOR_VERSION == 34
    case SPELL_SHAFT_SELF:
#endif
    case SPELL_AWAKEN_VINES:
    case SPELL_CONTROL_WINDS:
    case SPELL_WALL_OF_BRAMBLES:
#if TAG_MAJOR_VERSION == 34
    case SPELL_HASTE_PLANTS:
#endif
    case SPELL_WIND_BLAST:
    case SPELL_SUMMON_VERMIN:
    case SPELL_TORNADO:
    case SPELL_DISCHARGE:
    case SPELL_IGNITE_POISON:
#if TAG_MAJOR_VERSION == 34
    case SPELL_EPHEMERAL_INFUSION:
#endif
    case SPELL_FORCEFUL_INVITATION:
    case SPELL_PLANEREND:
    case SPELL_CHAIN_OF_CHAOS:
    case SPELL_BLACK_MARK:
#if TAG_MAJOR_VERSION == 34
    case SPELL_GRAND_AVATAR:
    case SPELL_REARRANGE_PIECES:
#endif
    case SPELL_BLINK_ALLIES_AWAY:
    case SPELL_SHROUD_OF_GOLUBRIA:
    case SPELL_PHANTOM_MIRROR:
    case SPELL_SUMMON_MANA_VIPER:
    case SPELL_SUMMON_EMPEROR_SCORPIONS:
    case SPELL_BATTLECRY:
    case SPELL_WARNING_CRY:
    case SPELL_SEAL_DOORS:
    case SPELL_BERSERK_OTHER:
    case SPELL_SPELLFORGED_SERVITOR:
    case SPELL_THROW:
    case SPELL_THROW_ALLY:
    case SPELL_CORRUPTING_PULSE:
    case SPELL_SIREN_SONG:
    case SPELL_AVATAR_SONG:
    case SPELL_REPEL_MISSILES:
    case SPELL_DEFLECT_MISSILES:
    case SPELL_SUMMON_SCARABS:
#if TAG_MAJOR_VERSION == 34
    case SPELL_HUNTING_CRY:
    case SPELL_CONDENSATION_SHIELD:
#endif
    case SPELL_CONTROL_UNDEAD:
    case SPELL_CLEANSING_FLAME:
    case SPELL_PARALYSIS_GAZE:
    case SPELL_DRAINING_GAZE:
    case SPELL_CONFUSION_GAZE:
    case SPELL_HAUNT:
    case SPELL_SUMMON_SPECTRAL_ORCS:
    case SPELL_BRAIN_FEED:
    case SPELL_SMITING:
    case SPELL_HOLY_FLAMES:
    case SPELL_CALL_OF_CHAOS:
    case SPELL_AIRSTRIKE:
    case SPELL_WATERSTRIKE:
    case SPELL_FLAY:
#if TAG_MAJOR_VERSION == 34
    case SPELL_CHANT_FIRE_STORM:
#endif
    case SPELL_GRAVITAS:
    case SPELL_ENTROPIC_WEAVE:
    case SPELL_SUMMON_EXECUTIONERS:
    case SPELL_DOOM_HOWL:
    case SPELL_AURA_OF_BRILLIANCE:
        pbolt.range = 0;
        pbolt.glyph = 0;
        return true;
    default:
        if (check_validity)
        {
            bolt beam = mons_spell_beam(mons, spell_cast, 1, true);
            return beam.flavour != NUM_BEAMS;
        }
        break;
    }

    const int power = _mons_spellpower(spell_cast, *mons);

    bolt theBeam = mons_spell_beam(mons, spell_cast, power);

    if (spell_cast == SPELL_LEGENDARY_DESTRUCTION)
    {
        int range = _mons_spell_range(theBeam.origin_spell, *mons);
        while (grid_distance(mons->pos(), mons->target) > range)
        {
            theBeam = mons_spell_beam(mons, spell_cast, power);
            range = _mons_spell_range(theBeam.origin_spell, *mons);
        }
    }
    bolt_parent_init(theBeam, pbolt);
    pbolt.source = mons->pos();
    pbolt.is_tracer = false;
    if (!pbolt.is_enchantment())
        pbolt.aux_source = pbolt.name;
    else
        pbolt.aux_source.clear();

    // Your shadow can target these spells at other monsters;
    // other monsters can't.
    if (mons->mid != MID_PLAYER)
    {
        if (spell_cast == SPELL_HASTE
            || spell_cast == SPELL_MIGHT
            || spell_cast == SPELL_INVISIBILITY
            || spell_cast == SPELL_MINOR_HEALING
            || spell_cast == SPELL_TELEPORT_SELF
            || spell_cast == SPELL_SILENCE)
        {
            pbolt.target = mons->pos();
        }
        else if (spell_cast == SPELL_LRD)
        {
            pbolt.target = _mons_fragment_target(mons);
            pbolt.aimed_at_spot = true; // to get noise to work properly
        }
        else if (spell_cast == SPELL_CONJURE_FLAME)
        {
            pbolt.target = _mons_conjure_flame_pos(mons, mons->get_foe());
            pbolt.aimed_at_spot = true; // ditto
        }
        else if (spell_cast == SPELL_FULMINANT_PRISM)
        {
            pbolt.target = _mons_prism_pos(mons, mons->get_foe());
            pbolt.aimed_at_spot = true; // ditto
        }
        else if (spell_cast == SPELL_AWAKEN_EARTH)
        {
            pbolt.target = _mons_awaken_earth_target(*mons);
            pbolt.aimed_at_spot = true; // ditto
        }
     }

    return true;
}

// Function should return false if friendlies shouldn't animate any dead.
// Currently, this only happens if the player is in the middle of butchering
// a corpse (infuriating), or if they are less than satiated. Only applies
// to friendly corpse animators. {due}
static bool _animate_dead_okay(spell_type spell)
{
    // It's always okay in the arena.
    if (crawl_state.game_is_arena())
        return true;

    if (is_butchering() || is_vampire_feeding())
        return false;

    if (you.hunger_state < HS_SATIATED && you.mutation[MUT_HERBIVOROUS] < 3)
        return false;

    if (god_hates_spell(spell, you.religion))
        return false;

    // Annoying to drag around hordes of the undead as well as the living.
    if (will_have_passive(passive_t::convert_orcs))
        return false;

    return true;
}

// Returns true if the spell is something you wouldn't want done if
// you had a friendly target...  only returns a meaningful value for
// non-beam spells.
static bool _ms_direct_nasty(spell_type monspell)
{
    return !(get_spell_flags(monspell) & SPFLAG_UTILITY
             || spell_typematch(monspell, SPTYP_SUMMONING));
}

// Checks if the foe *appears* to be immune to negative energy. We
// can't just use foe->res_negative_energy(), because that'll mean
// monsters will just "know" whether a player is fully life-protected.
static bool _foe_should_res_negative_energy(const actor* foe)
{
    if (foe->is_player())
    {
        switch (you.undead_state())
        {
        case US_ALIVE:
            // Demonspawn are not demons, and statue form grants only
            // partial resistance.
            return false;
        case US_SEMI_UNDEAD:
            // Non-bloodless vampires do not appear immune.
            return you.hunger_state <= HS_STARVING;
        default:
            return true;
        }
    }

    return !(foe->holiness() & MH_NATURAL);
}

static bool _valid_blink_ally(const monster* caster, const monster* target)
{
    return mons_aligned(caster, target) && caster != target
           && !target->no_tele(true, false, true);
}

static bool _valid_encircle_ally(const monster* caster, const monster* target,
                                 const coord_def foepos)
{
    return _valid_blink_ally(caster, target)
           && target->pos().distance_from(foepos) > 1;
}

static bool _valid_blink_away_ally(const monster* caster, const monster* target,
                                   const coord_def foepos)
{
    return _valid_blink_ally(caster, target)
           && target->pos().distance_from(foepos) < 3;
}

static bool _valid_druids_call_target(const monster* caller, const monster* callee)
{
    return mons_aligned(caller, callee) && mons_is_beast(callee->type)
           && callee->get_experience_level() <= 20
           && !callee->is_shapeshifter()
           && !caller->see_cell(callee->pos())
           && mons_habitat(callee) != HT_WATER
           && mons_habitat(callee) != HT_LAVA
           && !callee->is_travelling();
}

static bool _mirrorable(const monster* agent, const monster* mon)
{
    return mon != agent
           && mons_aligned(mon, agent)
           && !mon->is_stationary()
           && !mon->is_summoned()
           && !mons_is_conjured(mon->type)
           && !mons_is_unique(mon->type);
}

static bool _valid_aura_of_brilliance_ally(const monster* caster,
                                           const monster* target)
{
    return mons_aligned(caster, target) && caster != target
           && target->is_actual_spellcaster();
}

enum battlecry_type
{
    BATTLECRY_GOBLIN,
    BATTLECRY_ORC,
    BATTLECRY_HOLY,
    BATTLECRY_NATURAL,
    NUM_BATTLECRIES
};

/**
 * Is the given monster affected by the given type of battlecry?
 *
 * @param mons   The monster to be affected.
 * @param type   What type of battlecry is being loosed.
 * @return       Whether the given monster will be buffed by the battlecry.
 */
static bool _is_battlecry_compatible(const monster& mons, battlecry_type type)
{
    switch (type)
    {
        case BATTLECRY_GOBLIN:
            return mons_genus(mons.type) == MONS_GOBLIN;
        case BATTLECRY_ORC:
            return mons_genus(mons.type) == MONS_ORC;
        case BATTLECRY_HOLY:
            return bool(mons.holiness() & MH_HOLY);
        case BATTLECRY_NATURAL:
            return bool(mons.holiness() & MH_NATURAL);
        default:
            return false;
    }
}


/**
 * What type of cry does the given monster perform?
 *
 * @param crier  The monster letting loose a mighty battlecry.
 * @return       The battlecry type, which corresponds to different sets of
 *               affected monsters, messages, and (technically) enchantments.
 */
static battlecry_type _get_cry_type(const monster& crier)
{
    if (mons_genus(crier.type) == MONS_GOBLIN)
        return BATTLECRY_GOBLIN;
    if (mons_genus(crier.type) == MONS_ORC)
        return BATTLECRY_ORC;
    if (crier.holiness() & MH_HOLY)
        return BATTLECRY_HOLY;
    return BATTLECRY_NATURAL;
}

/**
 * Print the message that the player sees after a battlecry goes off.
 *
 * @param chief             The monster letting out the battlecry.
 * @param seen_affected     The affected monsters that are visible to the
 *                          player.
 */
static void _print_battlecry_announcement(const monster& chief,
                                          vector<monster*> &seen_affected)
{
    // Columns 0 and 1 should have one instance of %s (for the monster),
    // column 2 two (for a determiner and the monsters), and column 3 none.
    enum { AFFECT_ONE, AFFECT_MANY, GENERIC_ALLIES };
    static const char * const messages[][3] =
    {
        {
            "%s goes into a battle-frenzy!",
            "%s %s go into a battle-frenzy!",
            "goblins"
        },
        {
            "%s goes into a battle-frenzy!",
            "%s %s go into a battle-frenzy!",
            "orcs"
        },
        {
            "%s is roused by the hymn!",
            "%s %s are roused to righteous anger!",
            "holy creatures"
        },
        {
            "%s is stirred to greatness!",
            "%s %s are stirred to greatness!",
            "satyr's allies"
        },
    };
    COMPILE_CHECK(ARRAYSZ(messages) == NUM_BATTLECRIES);

    const battlecry_type type = _get_cry_type(chief);
    // Disabling detailed frenzy announcement because it's so spammy.
    const msg_channel_type channel = chief.friendly() ? MSGCH_MONSTER_ENCHANT
                                                      : MSGCH_FRIEND_ENCHANT;

    string who;
    if (seen_affected.size() == 1)
    {
        who = seen_affected[0]->name(DESC_THE);
        mprf(channel, messages[type][AFFECT_ONE], who.c_str());
    }
    else
    {
        monster_type mon_type = seen_affected[0]->type;
        // if not homogeneous, use the generic term instead
        const bool generic = any_of(
                                    begin(seen_affected), end(seen_affected),
                                    [=](const monster *m)
                                    { return m->type != mon_type; });
        who = get_monster_data(mon_type)->name;

        mprf(channel, messages[type][AFFECT_MANY],
             chief.friendly() ? "Your" : "The",
             generic ? messages[type][GENERIC_ALLIES]
             : pluralise_monster(who).c_str());
    }
}

/**
 * Let loose a mighty battlecry, inspiring & strengthening nearby foes!
 *
 * @param chief         The monster letting out the battlecry.
 * @param check_only    Whether to perform a 'dry run', only checking whether
 *                      any monsters are potentially affected.
 * @return              Whether any monsters are (or would be) affected.
 */
static bool _battle_cry(const monster& chief, bool check_only = false)
{
    const actor *foe = chief.get_foe();

    // Only let loose a battlecry if you have a valid target.
    if (!foe
        || foe->is_player() && chief.friendly()
        || !chief.see_cell_no_trans(foe->pos()))
    {
        return false;
    }

    // Don't try to make noise when silent.
    if (silenced(chief.pos()) || chief.has_ench(ENCH_MUTE))
        return false;

    const battlecry_type type = _get_cry_type(chief);

    int affected = 0;

    vector<monster* > seen_affected;
    for (monster_near_iterator mi(&chief, LOS_NO_TRANS); mi; ++mi)
    {
        const monster *mons = *mi;
        // can't buff yourself
        if (mons == &chief)
            continue;

        // only buff allies
        if (!mons_aligned(&chief, mons))
            continue;

        // no buffing confused/paralysed mons
        if (mons->berserk_or_insane()
            || mons->confused()
            || mons->cannot_move())
        {
            continue;
        }

        // already buffed
        if (mons->has_ench(ENCH_MIGHT))
            continue;

        // invalid battlecry target (wrong genus/holiness, or hd too high)
        if (!_is_battlecry_compatible(*mons, type)
            || mons->get_hit_dice() >= chief.get_hit_dice()
               && type != BATTLECRY_HOLY)
        {
            continue;
        }

        if (check_only)
            return true; // just need to check

        const int dur = random_range(12, 20) * speed_to_duration(mi->speed);

        mi->add_ench(mon_enchant(ENCH_MIGHT, 1, &chief, dur));

        affected++;
        if (you.can_see(**mi))
            seen_affected.push_back(*mi);

        if (mi->asleep())
            behaviour_event(*mi, ME_DISTURB, 0, chief.pos());
    }

    if (affected == 0)
        return false;

    // The yell happens whether you happen to see it or not.
    noisy(LOS_RADIUS, chief.pos(), chief.mid);

    if (!seen_affected.empty())
        _print_battlecry_announcement(chief, seen_affected);

    return true;
}

/**
 * Call upon the powers of chaos, applying mostly positive effects to nearby
 * allies!
 *
 * @param mons          The monster carrying out the call of chaos.
 * @param check_only    Whether to perform a 'dry run', only checking whether
 *                      any monsters are potentially affected.
 * @return              Whether any monsters are (or would be) affected.
 */
static bool _mons_call_of_chaos(const monster& mon, bool check_only = false)
{
    const actor *foe = mon.get_foe();

    if (!foe
        || foe->is_player() && mon.friendly()
        || !mon.see_cell_no_trans(foe->pos()))
    {
        return false;
    }

    int affected = 0;

    vector<monster* > seen_affected;
    for (monster_near_iterator mi(&mon, LOS_NO_TRANS); mi; ++mi)
    {
        const monster *mons = *mi;
        // can't buff yourself
        if (mons == &mon)
            continue;

        // only buff allies
        if (!mons_aligned(&mon, mons))
            continue;

        if (mons_is_firewood(mons))
            continue;

        if (check_only)
            return true; // just need to check

        if (mi->asleep())
            behaviour_event(*mi, ME_DISTURB, 0, mon.pos());

        beam_type flavour = random_choose_weighted(150, BEAM_HASTE,
                                                   150, BEAM_MIGHT,
                                                   150, BEAM_BERSERK,
                                                   150, BEAM_AGILITY,
                                                   150, BEAM_RESISTANCE,
                                                   150, BEAM_BLINK_CLOSE,
                                                    75, BEAM_BLINK,
                                                    10, BEAM_SLOW,
                                                    10, BEAM_PARALYSIS,
                                                    10, BEAM_CONFUSION,
                                                    10, BEAM_DISINTEGRATION,
                                                    10, BEAM_PETRIFY,
                                                    10, BEAM_SLEEP,
                                                    10, BEAM_VULNERABILITY,
                                                    10, BEAM_UNRAVELLED_MAGIC,
                                                     2, BEAM_ENSNARE,
                                                     0);

        enchant_actor_with_flavour(*mi,
                                   flavour == BEAM_BLINK_CLOSE
                                   ? foe
                                   : &mon,
                                   flavour);

        affected++;
        if (you.can_see(**mi))
            seen_affected.push_back(*mi);
    }

    if (affected == 0)
        return false;

    return true;
}

static void _set_door(set<coord_def> door, dungeon_feature_type feat)
{
    for (const auto &dc : door)
    {
        grd(dc) = feat;
        set_terrain_changed(dc);
    }
}

static bool _can_force_door_shut(const coord_def& door)
{
    if (grd(door) != DNGN_OPEN_DOOR)
        return false;

    set<coord_def> all_door;
    vector<coord_def> veto_spots;
    find_connected_identical(door, all_door);
    copy(all_door.begin(), all_door.end(), back_inserter(veto_spots));

    for (const auto &dc : all_door)
    {
        // Only attempt to push players and non-hostile monsters out of
        // doorways
        actor* act = actor_at(dc);
        if (act)
        {
            if (act->is_player()
                || act->is_monster()
                    && act->as_monster()->attitude != ATT_HOSTILE)
            {
                coord_def newpos;
                if (!get_push_space(dc, newpos, act, true, &veto_spots))
                    return false;
                else
                    veto_spots.push_back(newpos);
            }
            else
                return false;
        }
        // If there are items in the way, see if there's room to push them
        // out of the way
        else if (igrd(dc) != NON_ITEM)
        {
            if (!has_push_space(dc, 0))
                return false;
        }
    }

    // Didn't find any items we couldn't displace
    return true;
}

static bool _should_force_door_shut(const coord_def& door)
{
    if (grd(door) != DNGN_OPEN_DOOR)
        return false;

    dungeon_feature_type old_feat = grd(door);
    int cur_tension = get_tension(GOD_NO_GOD);

    set<coord_def> all_door;
    vector<coord_def> veto_spots;
    find_connected_identical(door, all_door);
    copy(all_door.begin(), all_door.end(), back_inserter(veto_spots));

    bool player_in_door = false;
    for (const auto &dc : all_door)
    {
        if (you.pos() == dc)
        {
            player_in_door = true;
            break;
        }
    }

    int new_tension;
    if (player_in_door)
    {
        coord_def newpos;
        coord_def oldpos = you.pos();
        get_push_space(oldpos, newpos, &you, false, &veto_spots);
        you.move_to_pos(newpos);
        _set_door(all_door, DNGN_CLOSED_DOOR);
        new_tension = get_tension(GOD_NO_GOD);
        _set_door(all_door, old_feat);
        you.move_to_pos(oldpos);
    }
    else
    {
        _set_door(all_door, DNGN_CLOSED_DOOR);
        new_tension = get_tension(GOD_NO_GOD);
        _set_door(all_door, old_feat);
    }

    // If closing the door would reduce player tension by too much, probably
    // it is scarier for the player to leave it open and thus it should be left
    // open

    // Currently won't allow tension to be lowered by more than 33%
    return ((cur_tension - new_tension) * 3) <= cur_tension;
}

// Find an adjacent space to displace a stack of items or a creature
// (If act is null, we are just moving items and not an actor)
bool get_push_space(const coord_def& pos, coord_def& newpos, actor* act,
                    bool ignore_tension, const vector<coord_def>* excluded)
{
    if (act && act->is_stationary())
        return false;

    int max_tension = -1;
    coord_def best_spot(-1, -1);
    bool can_push = false;
    for (adjacent_iterator ai(pos); ai; ++ai)
    {
        dungeon_feature_type feat = grd(*ai);
        if (feat_has_solid_floor(feat))
        {
            // Extra checks if we're moving a monster instead of an item
            if (act)
            {
                if (actor_at(*ai)
                    || !act->can_pass_through(*ai)
                    || !act->is_habitable(*ai))
                {
                    continue;
                }

                // Make sure the spot wasn't vetoed.
                if (excluded && find(begin(*excluded), end(*excluded), *ai)
                                    != end(*excluded))
                {
                    continue;
                }

                // If we don't care about tension, first valid spot is acceptable
                if (ignore_tension)
                {
                    newpos = *ai;
                    return true;
                }
                else // Calculate tension with monster at new location
                {
                    set<coord_def> all_door;
                    find_connected_identical(pos, all_door);
                    dungeon_feature_type old_feat = grd(pos);

                    act->move_to_pos(*ai);
                    _set_door(all_door, DNGN_CLOSED_DOOR);
                    int new_tension = get_tension(GOD_NO_GOD);
                    _set_door(all_door, old_feat);
                    act->move_to_pos(pos);

                    if (new_tension > max_tension)
                    {
                        max_tension = new_tension;
                        best_spot = *ai;
                        can_push = true;
                    }
                }
            }
            else //If we're not moving a creature, the first open spot is enough
            {
                newpos = *ai;
                return true;
            }
        }
    }

    if (can_push)
        newpos = best_spot;
    return can_push;
}

bool has_push_space(const coord_def& pos, actor* act,
                    const vector<coord_def>* excluded)
{
    coord_def dummy(-1, -1);
    return get_push_space(pos, dummy, act, true, excluded);
}

static bool _seal_doors_and_stairs(const monster* warden,
                                   bool check_only = false)
{
    ASSERT(warden);

    int num_closed = 0;
    int seal_duration = 80 + random2(80);
    bool player_pushed = false;
    bool had_effect = false;

    // Friendly wardens are already excluded by _ms_waste_of_time()
    if (!warden->can_see(you) || warden->foe != MHITYOU)
        return false;

    for (radius_iterator ri(you.pos(), LOS_RADIUS, C_SQUARE);
                 ri; ++ri)
    {
        if (grd(*ri) == DNGN_OPEN_DOOR)
        {
            if (!_can_force_door_shut(*ri))
                continue;

            // If it's scarier to leave this door open, do so
            if (!_should_force_door_shut(*ri))
                continue;

            if (check_only)
                return true;

            set<coord_def> all_door;
            vector<coord_def> veto_spots;
            find_connected_identical(*ri, all_door);
            copy(all_door.begin(), all_door.end(), back_inserter(veto_spots));
            for (const auto &dc : all_door)
            {
                // If there are things in the way, push them aside
                actor* act = actor_at(dc);
                if (igrd(dc) != NON_ITEM || act)
                {
                    coord_def newpos;
                    // If we don't find a spot, try again ignoring tension.
                    const bool success =
                        get_push_space(dc, newpos, act, false, &veto_spots)
                        || get_push_space(dc, newpos, act, true, &veto_spots);
                    ASSERTM(success, "No push space from (%d,%d)", dc.x, dc.y);

                    move_items(dc, newpos);
                    if (act)
                    {
                        actor_at(dc)->move_to_pos(newpos);
                        if (act->is_player())
                            player_pushed = true;
                        veto_spots.push_back(newpos);
                    }
                }
            }

            // Close the door
            bool seen = false;
            vector<coord_def> excludes;
            for (const auto &dc : all_door)
            {
                grd(dc) = DNGN_CLOSED_DOOR;
                set_terrain_changed(dc);
                dungeon_events.fire_position_event(DET_DOOR_CLOSED, dc);

                if (is_excluded(dc))
                    excludes.push_back(dc);

                if (you.see_cell(dc))
                    seen = true;

                had_effect = true;
            }

            if (seen)
            {
                for (const auto &dc : all_door)
                {
                    if (env.map_knowledge(dc).seen())
                    {
                        env.map_knowledge(dc).set_feature(DNGN_CLOSED_DOOR);
#ifdef USE_TILE
                        env.tile_bk_bg(dc) = TILE_DNGN_CLOSED_DOOR;
#endif
                    }
                }

                update_exclusion_los(excludes);
                ++num_closed;
            }
        }

        // Try to seal the door
        if (grd(*ri) == DNGN_CLOSED_DOOR || grd(*ri) == DNGN_RUNED_DOOR)
        {
            if (check_only)
                return true;

            set<coord_def> all_door;
            find_connected_identical(*ri, all_door);
            for (const auto &dc : all_door)
            {
                temp_change_terrain(dc, DNGN_SEALED_DOOR, seal_duration,
                                    TERRAIN_CHANGE_DOOR_SEAL, warden);
                had_effect = true;
            }
        }
        else if (feat_is_travelable_stair(grd(*ri)))
        {
            if (check_only)
                return true;

            dungeon_feature_type stype;
            if (feat_stair_direction(grd(*ri)) == CMD_GO_UPSTAIRS)
                stype = DNGN_SEALED_STAIRS_UP;
            else
                stype = DNGN_SEALED_STAIRS_DOWN;

            temp_change_terrain(*ri, stype, seal_duration,
                                TERRAIN_CHANGE_DOOR_SEAL, warden);
            had_effect = true;
        }
    }

    if (had_effect)
    {
        ASSERT(!check_only);
        mprf(MSGCH_MONSTER_SPELL, "%s activates a sealing rune.",
                (warden->visible_to(&you) ? warden->name(DESC_THE, true).c_str()
                                          : "Someone"));
        if (num_closed > 1)
            mpr("The doors slam shut!");
        else if (num_closed == 1)
            mpr("A door slams shut!");

        if (player_pushed)
            mpr("You are pushed out of the doorway!");

        return true;
    }

    return false;
}

static bool _make_monster_angry(const monster* mon, monster* targ, bool actual)
{
    ASSERT(mon); // XXX: change to const monster &mon
    ASSERT(targ); // XXX: change to monster &targ
    if (mon->friendly() != targ->friendly())
        return false;

    // targ is guaranteed to have a foe (needs_berserk checks this).
    // Now targ needs to be closer to *its* foe than mon is (otherwise
    // mon might be in the way).

    coord_def victim;
    if (targ->foe == MHITYOU)
        victim = you.pos();
    else if (targ->foe != MHITNOT)
    {
        const monster* vmons = &menv[targ->foe];
        if (!vmons->alive())
            return false;
        victim = vmons->pos();
    }
    else
    {
        // Should be impossible. needs_berserk should find this case.
        die("angered by no foe");
    }

    // If mon may be blocking targ from its victim, don't try.
    if (victim.distance_from(targ->pos()) > victim.distance_from(mon->pos()))
        return false;

    if (!actual)
        return true;

    if (you.can_see(*mon))
    {
        if (mon->type == MONS_QUEEN_BEE && targ->type == MONS_KILLER_BEE)
        {
            mprf("%s calls on %s to defend %s!",
                mon->name(DESC_THE).c_str(),
                targ->name(DESC_THE).c_str(),
                mon->pronoun(PRONOUN_OBJECTIVE).c_str());
        }
        else
            mprf("%s goads %s on!", mon->name(DESC_THE).c_str(),
                 targ->name(DESC_THE).c_str());
    }

    targ->go_berserk(false);

    return true;
}

static bool _incite_monsters(const monster* mon, bool actual)
{
    if (is_sanctuary(you.pos()) || is_sanctuary(mon->pos()))
        return false;

    // Only things both in LOS of the inciter and within radius 3.
    const int radius = 3;
    int goaded = 0;
    for (monster_near_iterator mi(mon, LOS_NO_TRANS); mi; ++mi)
    {
        if (*mi == mon || !mi->needs_berserk())
            continue;

        if (is_sanctuary(mi->pos()))
            continue;

        // Cannot goad other moths of wrath!
        if (mon->type == MONS_MOTH_OF_WRATH
            && mi->type == MONS_MOTH_OF_WRATH
        // Queen bees can only incite killer bees.
            || mon->type == MONS_QUEEN_BEE
               && mi->type != MONS_KILLER_BEE)
        {
            continue;
        }

        if (grid_distance(mon->pos(), mi->pos()) > radius)
            continue;

        const bool worked = _make_monster_angry(mon, *mi, actual);
        if (worked && (!actual || !one_chance_in(3 * ++goaded)))
            return true;
    }

    return goaded > 0;
}

static bool _ms_low_hitpoint_cast(monster* mon, mon_spell_slot slot)
{
    spell_type monspell = slot.spell;
    if (_ms_waste_of_time(mon, slot))
        return false;

    bool targ_adj       = false;
    bool targ_sanct     = false;
    bool targ_friendly  = false;
    bool targ_living    = false;

    if (mon->foe == MHITYOU || mon->foe == MHITNOT)
    {
        if (adjacent(you.pos(), mon->pos()))
            targ_adj = true;
        if (is_sanctuary(you.pos()))
            targ_sanct = true;
        if (you.holiness() & MH_NATURAL)
            targ_living = true;
    }
    else
    {
        if (adjacent(menv[mon->foe].pos(), mon->pos()))
            targ_adj = true;
        if (is_sanctuary(menv[mon->foe].pos()))
            targ_sanct = true;
        if (menv[mon->foe].holiness() & MH_NATURAL)
            targ_living = true;
    }

    targ_friendly = (mon->foe == MHITYOU
                     ? mon->wont_attack()
                     : mons_aligned(mon, mon->get_foe()));

    switch (monspell)
    {
    case SPELL_HEAL_OTHER:
        return targ_friendly && mon->foe != MHITYOU;
    case SPELL_TELEPORT_OTHER:
        return !targ_sanct && !targ_friendly;
    case SPELL_MINOR_HEALING:
    case SPELL_MAJOR_HEALING:
    case SPELL_INVISIBILITY:
    case SPELL_TELEPORT_SELF:
    case SPELL_HASTE:
    case SPELL_DEATHS_DOOR:
    case SPELL_BERSERKER_RAGE:
    case SPELL_MIGHT:
    case SPELL_WIND_BLAST:
        return true;
    case SPELL_VAMPIRIC_DRAINING:
        return !targ_sanct && targ_adj && !targ_friendly && targ_living;
    case SPELL_BLINK_AWAY:
    case SPELL_BLINK_RANGE:
    case SPELL_INJURY_MIRROR:
        return !targ_friendly;
    case SPELL_BLINK_OTHER:
        return !targ_sanct && targ_adj && !targ_friendly;
    case SPELL_CONFUSE:
    case SPELL_DRAIN_LIFE:
    case SPELL_BANISHMENT:
    case SPELL_CALL_DOWN_DAMNATION:
    case SPELL_FIREBALL:
    case SPELL_AIRSTRIKE:
    case SPELL_IOOD:
    case SPELL_ENSNARE:
    case SPELL_THROW_FLAME:
    case SPELL_MEPHITIC_CLOUD:
    case SPELL_DEATH_RATTLE:
        return !targ_friendly && !targ_sanct;
    case SPELL_BLINK:
    case SPELL_CONTROLLED_BLINK:
        return targ_adj;
    case SPELL_TOMB_OF_DOROKLOHE:
        return true;
    case SPELL_NO_SPELL:
        return false;
    case SPELL_INK_CLOUD:
        if (mon->type == MONS_KRAKEN)
            return true;
    default:
        return !targ_adj && spell_typematch(monspell, SPTYP_SUMMONING);
    }
}

// Spells for a quick get-away.
// Currently only used to get out of a net.
static bool _ms_quick_get_away(const monster* mon, spell_type monspell)
{
    switch (monspell)
    {
    case SPELL_TELEPORT_SELF:
        // Don't cast again if already about to teleport.
        if (mon->has_ench(ENCH_TP))
            return false;
        // intentional fall-through
    case SPELL_BLINK:
        return true;
    default:
        return false;
    }
}

// Is it worth bothering to invoke recall? (Currently defined by there being at
// least 3 things we could actually recall, and then with a probability inversely
// proportional to how many HD of allies are current nearby)
static bool _should_recall(monster* caller)
{
    ASSERT(caller); // XXX: change to monster &caller
    // It's a long recitation - if we're winded, we can't use it.
    if (caller->has_ench(ENCH_BREATH_WEAPON))
        return false;

    int num = 0;
    for (monster_iterator mi; mi; ++mi)
    {
        if (mons_is_recallable(caller, *mi)
            && !caller->see_cell_no_trans((*mi)->pos()))
        {
            ++num;
        }
    }

    // Since there are reinforcements we could recall, do we think we need them?
    if (num > 2)
    {
        int ally_hd = 0;
        for (monster_near_iterator mi(caller->pos(), LOS_NO_TRANS); mi; ++mi)
        {
            if (*mi != caller && caller->can_see(**mi)
                && mons_aligned(caller, *mi)
                && !mons_is_firewood(*mi))
            {
                ally_hd += mi->get_experience_level();
            }
        }
        return 25 + roll_dice(2, 22) > ally_hd;
    }
    else
        return false;
}

/**
 * Recall a bunch of monsters!
 *
 * @param mons[in] the monster doing the recall
 * @param recall_target the max number of monsters to recall.
 * @returns whether anything was recalled.
 */
bool mons_word_of_recall(monster* mons, int recall_target)
{
    unsigned short num_recalled = 0;
    vector<monster* > mon_list;

    // Build the list of recallable monsters and randomize
    for (monster_iterator mi; mi; ++mi)
    {
        // Don't recall ourselves
        if (*mi == mons)
            continue;

        if (!mons_is_recallable(mons, *mi))
            continue;

        // Don't recall things that are already close to us
        if ((mons && mons->see_cell_no_trans((*mi)->pos()))
            || (!mons && you.see_cell_no_trans((*mi)->pos())))
        {
            continue;
        }

        mon_list.push_back(*mi);
    }
    shuffle_array(mon_list);

    const coord_def target   = (mons) ? mons->pos() : you.pos();
    const unsigned short foe = (mons) ? mons->foe   : MHITYOU;

    // Now actually recall things
    for (monster *mon : mon_list)
    {
        coord_def empty;
        if (find_habitable_spot_near(target, mons_base_type(mon),
                                     3, false, empty)
            && mon->move_to_pos(empty))
        {
            mon->behaviour = BEH_SEEK;
            mon->foe = foe;
            ++num_recalled;
            simple_monster_message(mon, " is recalled.");
        }
        // Can only recall a couple things at once
        if (num_recalled == recall_target)
            break;
    }
    return num_recalled;
}

static bool _valid_vine_spot(coord_def p)
{
    if (actor_at(p) || !monster_habitable_grid(MONS_PLANT, grd(p)))
        return false;

    int num_trees = 0;
    bool valid_trees = false;
    for (adjacent_iterator ai(p); ai; ++ai)
    {
        if (feat_is_tree(grd(*ai)))
        {
            // Make sure this spot is not on a diagonal to its only adjacent
            // tree (so that the vines can pull back against the tree properly)
            if (num_trees || !((*ai-p).sgn().x != 0 && (*ai-p).sgn().y != 0))
            {
                valid_trees = true;
                break;
            }
            else
                ++num_trees;
        }
    }

    if (!valid_trees)
        return false;

    // Now the connectivity check
    return !plant_forbidden_at(p, true);
}

static bool _awaken_vines(monster* mon, bool test_only = false)
{
    vector<coord_def> spots;
    for (radius_iterator ri(mon->pos(), LOS_NO_TRANS); ri; ++ri)
    {
        if (_valid_vine_spot(*ri))
            spots.push_back(*ri);
    }

    shuffle_array(spots);

    actor* foe = mon->get_foe();

    int num_vines = 1 + random2(3);
    if (mon->props.exists("vines_awakened"))
        num_vines = min(num_vines, 3 - mon->props["vines_awakened"].get_int());
    bool seen = false;

    for (coord_def spot : spots)
    {
        // Don't place vines where they can't see our target
        if (!cell_see_cell(spot, foe->pos(), LOS_NO_TRANS))
            continue;

        // Don't place a vine too near to another existing one
        bool too_close = false;
        for (distance_iterator di(spot, false, true, 3); di; ++di)
        {
            monster* m = monster_at(*di);
            if (m && m->type == MONS_SNAPLASHER_VINE)
            {
                too_close = true;
                break;
            }
        }
        if (too_close)
            continue;

        // We've found at least one valid spot, so the spell should be castable
        if (test_only)
            return true;

        // Actually place the vine and update properties
        if (monster* vine = create_monster(
            mgen_data(MONS_SNAPLASHER_VINE, SAME_ATTITUDE(mon), mon,
                        0, SPELL_AWAKEN_VINES, spot, mon->foe,
                        MG_FORCE_PLACE, mon->god, MONS_NO_MONSTER)))
        {
            vine->props["vine_awakener"].get_int() = mon->mid;
            mon->props["vines_awakened"].get_int()++;
            mon->add_ench(mon_enchant(ENCH_AWAKEN_VINES, 1, nullptr, 200));
            --num_vines;
            if (you.can_see(*vine))
                seen = true;
        }

        // We've finished placing all our vines
        if (num_vines == 0)
            break;
    }

    if (test_only)
        return false;
    else
    {
        if (seen)
            mpr("Vines fly forth from the trees!");
        return true;
    }
}

static bool _place_druids_call_beast(const monster* druid, monster* beast,
                                     const actor* target)
{
    for (int t = 0; t < 20; ++t)
    {
        // Attempt to find some random spot out of the target's los to place
        // the beast (but not too far away).
        coord_def area = clamp_in_bounds(target->pos() + coord_def(random_range(-11, 11),
                                                                   random_range(-11, 11)));
        if (cell_see_cell(target->pos(), area, LOS_DEFAULT))
            continue;

        coord_def base_spot;
        int tries = 0;
        while (tries < 10 && base_spot.origin())
        {
            find_habitable_spot_near(area, mons_base_type(beast), 3, false, base_spot);
            if (cell_see_cell(target->pos(), base_spot, LOS_DEFAULT))
                base_spot.reset();
            ++tries;
        }

        if (base_spot.origin())
            continue;

        beast->move_to_pos(base_spot);

        // Wake the beast up and calculate a path to the druid's target.
        // (Note that both BEH_WANDER and MTRAV_PATROL are necessary for it
        // to follow the given path and also not randomly wander off instead)
        beast->behaviour = BEH_WANDER;
        beast->foe = druid->foe;

        monster_pathfind mp;
        if (mp.init_pathfind(beast, target->pos()))
        {
            beast->travel_path = mp.calc_waypoints();
            if (!beast->travel_path.empty())
            {
                beast->target = beast->travel_path[0];
                beast->travel_target = MTRAV_PATROL;
            }
        }

        // Assign blame (for statistical purposes, mostly)
        mons_add_blame(beast, "called by " + druid->name(DESC_A, true));

        return true;
    }

    return false;
}

static void _cast_druids_call(const monster* mon)
{
    vector<monster*> mon_list;
    for (monster_iterator mi; mi; ++mi)
    {
        if (_valid_druids_call_target(mon, *mi))
            mon_list.push_back(*mi);
    }

    shuffle_array(mon_list);

    const actor* target = mon->get_foe();
    const int num = min((int)mon_list.size(),
                        mon->get_experience_level() > 10 ? random_range(2, 3)
                                                      : random_range(1, 2));

    for (int i = 0; i < num; ++i)
        _place_druids_call_beast(mon, mon_list[i], target);
}

static double _angle_between(coord_def origin, coord_def p1, coord_def p2)
{
    double ang0 = atan2(p1.x - origin.x, p1.y - origin.y);
    double ang  = atan2(p2.x - origin.x, p2.y - origin.y);
    return min(fabs(ang - ang0), fabs(ang - ang0 + 2 * PI));
}

// Does there already appear to be a bramble wall in this direction?
// We approximate this by seeing if there are at least two briar patches in
// a ray between us and our target, which turns out to be a pretty decent
// metric in practice.
static bool _already_bramble_wall(const monster* mons, coord_def targ)
{
    bolt tracer;
    tracer.source    = mons->pos();
    tracer.target    = targ;
    tracer.range     = 12;
    tracer.is_tracer = true;
    tracer.pierce    = true;
    tracer.fire();

    int briar_count = 0;
    bool targ_reached = false;
    for (coord_def p : tracer.path_taken)
    {
        if (!targ_reached && p == targ)
            targ_reached = true;
        else if (!targ_reached)
            continue;

        if (monster_at(p) && monster_at(p)->type == MONS_BRIAR_PATCH)
            ++briar_count;
    }

    return briar_count > 1;
}

static bool _wall_of_brambles(monster* mons)
{
    mgen_data briar_mg = mgen_data(MONS_BRIAR_PATCH, SAME_ATTITUDE(mons),
                                   mons, 0, 0, coord_def(-1, -1), MHITNOT,
                                   MG_FORCE_PLACE);

    // We want to raise a defensive wall if we think our foe is moving to attack
    // us, and otherwise raise a wall further away to block off their escape.
    // (Each wall type uses different parameters)
    bool defensive = mons->props["foe_approaching"].get_bool();

    coord_def aim_pos = you.pos();
    coord_def targ_pos = mons->pos();

    // A defensive wall cannot provide any cover if our target is already
    // adjacent, so don't bother creating one.
    if (defensive && mons->pos().distance_from(aim_pos) == 1)
        return false;

    // Don't raise a non-defensive wall if it looks like there's an existing one
    // in the same direction already (this looks rather silly to see walls
    // springing up in the distance behind already-closed paths, and probably
    // is more likely to aid the player than the monster)
    if (!defensive)
    {
        if (_already_bramble_wall(mons, aim_pos))
            return false;
    }

    // Select a random radius for the circle used draw an arc from (affects
    // both shape and distance of the resulting wall)
    int rad = (defensive ? random_range(3, 5)
                         : min(11, mons->pos().distance_from(you.pos()) + 6));

    // Adjust the center of the circle used to draw the arc of the wall if
    // we're raising one defensively, based on both its radius and foe distance.
    // (The idea is the ensure that our foe will end up on the other side of it
    // without always raising the wall in exactly the same shape and position)
    if (defensive)
    {
        coord_def adjust = (targ_pos - aim_pos).sgn();

        targ_pos += adjust;
        if (rad == 5)
            targ_pos += adjust;
        if (mons->pos().distance_from(aim_pos) == 2)
            targ_pos += adjust;
    }

    // XXX: There is almost certainly a better way to calculate the points
    //      along the desired arcs, though this code produces the proper look.
    vector<coord_def> points;
    for (distance_iterator di(targ_pos, false, false, rad); di; ++di)
    {
        if (di.radius() == rad || di.radius() == rad - 1)
        {
            if (!actor_at(*di) && !cell_is_solid(*di))
            {
                if (defensive && _angle_between(targ_pos, aim_pos, *di) <= PI/4.0
                    || (!defensive
                        && _angle_between(targ_pos, aim_pos, *di) <= PI/(4.2 + rad/6.0)))
                {
                    points.push_back(*di);
                }
            }
        }
    }

    bool seen = false;
    for (coord_def point : points)
    {
        briar_mg.pos = point;
        monster* briar = create_monster(briar_mg, false);
        if (briar)
        {
            briar->add_ench(mon_enchant(ENCH_SHORT_LIVED, 1, nullptr, 80 + random2(100)));
            if (you.can_see(*briar))
                seen = true;
        }
    }

    if (seen)
        mpr("Thorny briars emerge from the ground!");

    return true;
}

/**
 * Make the given monster cast the spell "Corrupting Pulse", corrupting
 * (temporarily malmutating) all creatures in LOS.
 *
 * @param mons  The monster in question.
 */
static void _corrupting_pulse(monster *mons)
{
    if (cell_see_cell(you.pos(), mons->pos(), LOS_DEFAULT))
    {
        targetter_los hitfunc(mons, LOS_SOLID);
        flash_view_delay(UA_MONSTER, MAGENTA, 300, &hitfunc);

        if (!is_sanctuary(you.pos())
            && cell_see_cell(you.pos(), mons->pos(), LOS_SOLID))
        {
            int num_mutations = one_chance_in(4) ? 2 : 1;
            for (int i = 0; i < num_mutations; ++i)
                temp_mutate(RANDOM_CORRUPT_MUTATION, "wretched star");
        }
    }

    for (radius_iterator ri(mons->pos(), LOS_RADIUS, C_SQUARE); ri; ++ri)
    {
        monster *m = monster_at(*ri);
        if (m && cell_see_cell(mons->pos(), *ri, LOS_SOLID_SEE))
            m->corrupt();
    }
}

// Returns the clone just created (null otherwise)
monster* cast_phantom_mirror(monster* mons, monster* targ, int hp_perc, int summ_type)
{
    // Create clone.
    monster *mirror = clone_mons(targ, true);

    // Abort if we failed to place the monster for some reason.
    if (!mirror)
        return nullptr;

    // Unentangle the real monster.
    if (mons->is_constricted())
        mons->stop_being_constricted();

    mons_clear_trapping_net(mons);

    // Don't leak the real one with the targeting interface.
    if (you.prev_targ == mons->mindex())
    {
        you.prev_targ = MHITNOT;
        crawl_state.cancel_cmd_repeat();
    }
    mons->reset_client_id();

    mirror->mark_summoned(5, true, summ_type);
    mirror->add_ench(ENCH_PHANTOM_MIRROR);
    mirror->summoner = mons->mid;
    mirror->hit_points = mirror->hit_points * 100 / hp_perc;
    mirror->max_hit_points = mirror->max_hit_points * 100 / hp_perc;

    // Sometimes swap the two monsters, so as to disguise the original and the
    // copy.
    if (coinflip())
        targ->swap_with(mirror);

    return mirror;
}

static bool _trace_los(monster* agent, bool (*vulnerable)(actor*))
{
    bolt tracer;
    tracer.foe_ratio = 0;
    for (actor_near_iterator ai(agent, LOS_NO_TRANS); ai; ++ai)
    {
        if (agent == *ai || !agent->can_see(**ai) || !vulnerable(*ai))
            continue;

        if (mons_aligned(agent, *ai))
        {
            tracer.friend_info.count++;
            tracer.friend_info.power +=
                    ai->is_player() ? effective_xl()
                                    : ai->as_monster()->get_experience_level();
        }
        else
        {
            tracer.foe_info.count++;
            tracer.foe_info.power +=
                    ai->is_player() ? effective_xl()
                                    : ai->as_monster()->get_experience_level();
        }
    }
    return mons_should_fire(tracer);
}

static bool _tornado_vulnerable(actor* victim)
{
    return !victim->res_wind();
}

static bool _torment_vulnerable(actor* victim)
{
    if (victim->is_player())
        return !player_res_torment(false);

    return !victim->res_torment();
}

static bool _elec_vulnerable(actor* victim)
{
    return victim->res_elec() < 3;
}

static bool _mutation_vulnerable(actor* victim)
{
    return victim->can_mutate();
}

static bool _dummy_vulnerable(actor* victim)
{
    return true;
}

static void _cast_black_mark(monster* agent)
{
    for (actor_near_iterator ai(agent, LOS_NO_TRANS); ai; ++ai)
    {
        if (!ai->visible_to(agent)
            || ai->is_player()
            || !mons_aligned(*ai, agent))
        {
            continue;
        }
        monster* mon = ai->as_monster();
        if (!mon->has_ench(ENCH_BLACK_MARK) && !mons_is_firewood(mon))
        {
            mon->add_ench(ENCH_BLACK_MARK);
            simple_monster_message(mon, " begins absorbing vital energies!");
        }
    }
}

void aura_of_brilliance(monster* agent)
{
    bool did_something = false;
    for (actor_near_iterator ai(agent, LOS_NO_TRANS); ai; ++ai)
    {
        if (!ai->visible_to(agent)
            || ai->is_player()
            || !mons_aligned(*ai, agent))
        {
            continue;
        }
        monster* mon = ai->as_monster();
        if (_valid_aura_of_brilliance_ally(agent, mon))
        {
            if (!mon->has_ench(ENCH_EMPOWERED_SPELLS) && you.can_see(*mon))
            {
               mprf("%s is empowered by %s aura!",
                    mon->name(DESC_THE).c_str(),
                    apostrophise(agent->name(DESC_THE)).c_str());
            }
            mon_enchant ench = mon->get_ench(ENCH_EMPOWERED_SPELLS);
            if (ench.ench != ENCH_NONE)
            {
                ench.duration = 2 * BASELINE_DELAY;
                mon->update_ench(ench);
            }
            else
            {
                mon->add_ench(mon_enchant(ENCH_EMPOWERED_SPELLS, 1,
                                          agent, 2 * BASELINE_DELAY));
            }
            did_something = true;
        }
    }

    if (!did_something)
        agent->del_ench(ENCH_BRILLIANCE_AURA);
}

static bool _glaciate_tracer(monster *caster, int pow, coord_def aim)
{
    targetter_cone hitfunc(caster, spell_range(SPELL_GLACIATE, pow));
    hitfunc.set_aim(aim);

    mon_attitude_type castatt = caster->temp_attitude();
    int friendly = 0, enemy = 0;

    for (const auto &entry : hitfunc.zapped)
    {
        if (entry.second <= 0)
            continue;

        const actor *victim = actor_at(entry.first);
        if (!victim)
            continue;

        if (mons_atts_aligned(castatt, victim->temp_attitude()))
        {
            if (victim->is_player() && !(caster->holiness() & MH_DEMONIC))
                return false; // never glaciate the player! except demons
            friendly += victim->get_experience_level();
        }
        else
            enemy += victim->get_experience_level();
    }

    return enemy > friendly;
}

bool mons_should_cloud_cone(monster* agent, int power, const coord_def pos)
{
    targetter_shotgun hitfunc(agent, CLOUD_CONE_BEAM_COUNT,
                              spell_range(SPELL_CLOUD_CONE, power));

    hitfunc.set_aim(pos);

    bolt tracer;
    tracer.foe_ratio = 80;
    tracer.source_id = agent->mid;
    tracer.target = pos;
    for (actor_near_iterator ai(agent, LOS_NO_TRANS); ai; ++ai)
    {
        if (hitfunc.is_affected(ai->pos()) == AFF_NO || !agent->can_see(**ai))
            continue;

        if (mons_aligned(agent, *ai))
        {
            tracer.friend_info.count++;
            tracer.friend_info.power += ai->get_experience_level();
        }
        else
        {
            tracer.foe_info.count++;
            tracer.foe_info.power += ai->get_experience_level();
        }
    }

    return mons_should_fire(tracer);
}

static bool _spray_tracer(monster *caster, int pow, bolt parent_beam, spell_type spell)
{
    vector<bolt> beams = get_spray_rays(caster, parent_beam.target,
                                        spell_range(spell, pow), 3);
    if (beams.size() == 0)
        return false;

    bolt beam;

    for (bolt &child : beams)
    {
        bolt_parent_init(parent_beam, child);
        fire_tracer(caster, child);
        beam.friend_info += child.friend_info;
        beam.foe_info    += child.foe_info;
    }

    return mons_should_fire(beam);
}

/**
 * Pick a target for conjuring a flame.
 *
 * @param[in] mon The monster casting this.
 * @param[in] foe The victim whose movement we are trying to impede.
 * @return A position for conjuring a cloud.
 */
static coord_def _mons_conjure_flame_pos(monster* mon, actor* foe)
{
    // Don't bother if our target is sufficiently fire-resistant,
    // or doesn't exist.
    if (!foe || foe->res_fire() >= 3)
        return coord_def();

    const coord_def foe_pos = foe->pos();
    const coord_def a = foe_pos - mon->pos();
    vector<coord_def> targets;

    const int range = _mons_spell_range(SPELL_CONJURE_FLAME, *mon);
    for (distance_iterator di(mon->pos(), true, true, range); di; ++di)
    {
        // Our target needs to be in LOS, and we can't have a creature or
        // cloud there. Technically we can target flame clouds, but that's
        // usually not very constructive where monsters are concerned.
        if (!cell_see_cell(mon->pos(), *di, LOS_SOLID)
            || cell_is_solid(*di)
            || (actor_at(*di) && mon->can_see(*actor_at(*di))))
        {
            continue;
        }

        if (cloud_at(*di))
            continue;

        // Conjure flames *behind* the target; blocking the target from
        // accessing us just lets them run away, which is bad if our target
        // is usually the player.
        coord_def b = *di - foe_pos;
        int dot = (a.x * b.x + a.y * b.y);
        if (dot < 0)
            continue;

        // Try to block off a hallway.
        int floor_count = 0;
        for (adjacent_iterator ai(*di); ai; ++ai)
        {
            if (feat_is_traversable(grd(*ai))
                && is_damaging_cloud(cloud_type_at(*ai)))
            {
                floor_count++;
            }
        }

        if (floor_count != 2)
            continue;

        targets.push_back(*di);
    }

    // If we found something, pick a square at random to block.
    const int count = targets.size();
    if (!count)
        return coord_def();

    return targets[random2(count)];
}

/**
 * Pick a target for conjuring a fulminant prism.
 *
 * @param[in] mon The monster casting this.
 * @param[in] foe The victim we're trying to kill.
 * @return A position for conjuring a prism.
 */
static coord_def _mons_prism_pos(monster* mon, actor* foe)
{
    // Don't bother if our target doesn't exist.
    if (!foe)
        return coord_def();

    const int foe_speed =
        foe->is_player() ? player_movement_speed()
                         : foe->as_monster()->action_energy(EUT_MOVE)
                           * BASELINE_DELAY / foe->as_monster()->speed;

    // The % bit is effectively a ceil(); it captures the partial move the
    // target gets with their leftover energy.
    const int rad = 3 * BASELINE_DELAY / foe_speed
                    + (((3 * BASELINE_DELAY) % foe_speed) > 0 ? 1 : 0);

    // XXX: make this use coord_def when we have a hash<> for it
    unordered_set<int> possible_places;

    for (radius_iterator ri(foe->pos(), rad, C_SQUARE, LOS_NO_TRANS); ri; ++ri)
    {
        if (cell_is_solid(*ri))
            continue;

        possible_places.insert(ri->y * GYM + ri->x);
    }

    coord_def target;
    int max_coverage = 0;
    int hits = 1;

    const int range = _mons_spell_range(SPELL_FULMINANT_PRISM, *mon);

    // TODO: try to avoid hurting allies and oneself here.
    for (distance_iterator di(mon->pos(), true, true, range); di; ++di)
    {
        // Our target needs to be in LOS, and we can't have a creature there.
        if (!cell_see_cell(mon->pos(), *di, LOS_NO_TRANS)
            || cell_is_solid(*di)
            || (actor_at(*di) && mon->can_see(*actor_at(*di))))
        {
            continue;
        }
        int coverage = 0;
        for (radius_iterator ri(*di, 2, C_SQUARE, LOS_NO_TRANS); ri; ++ri)
        {
            if (possible_places.find(ri->y * GYM + ri->x)
                != possible_places.end())
            {
                coverage++;
            }
        }
        if (coverage > max_coverage)
        {
            target = *di;
            max_coverage = coverage;
            hits = 1;
        }
        else if (coverage == max_coverage && !random2(++hits))
            target = *di;
    }

    return target;
}

/**
 * Is this a feature that we can Awaken?
 *
 * @param   feat The feature type.
 * @returns If the feature is a valid feature that we can Awaken Earth on.
 */
static bool _feat_is_awakenable(dungeon_feature_type feat)
{
    return feat == DNGN_ROCK_WALL || feat == DNGN_CLEAR_ROCK_WALL;
}

/**
 * Pick a target for Awaken Earth.
 *
 *  @param  mon       The monster casting
 *  @return The target square - out of bounds if no target was found.
 */
static coord_def _mons_awaken_earth_target(monster &mon)
{
    coord_def pos = mon.target;

    // First up, see if we can see our target, and if they're in a good spot
    // to pick on them. If so, do that.
    if (in_bounds(pos) && mon.see_cell(pos)
        && count_neighbours_with_func(pos, &_feat_is_awakenable) > 0)
    {
        return pos;
    }

    // Either we can't see our target, or they're not adjacent to walls.
    // Step back towards the caster from the target, and see if we can find
    // a better wall for this.
    const coord_def start_pos = mon.pos();

    ray_def ray;
    fallback_ray(pos, start_pos, ray); // straight line from them to mon

    // XXX: make this use coord_def when we have a hash<> for it
    unordered_set<int> candidates;

    // Candidates: everything on or adjacent to a straight line to the target.
    // Strongly prefer cells where we can get lots of elementals.
    while (in_bounds(pos) && pos != start_pos)
    {
        for (adjacent_iterator ai(pos, false); ai; ++ai)
        {
            if (!mon.see_cell(pos))
                continue;
            candidates.insert(ai->y * GYM + ai->x);
        }

        ray.advance();
        pos = ray.pos();
    }

    vector<coord_weight> targets;
    for (auto candidate : candidates)
    {
        coord_def target(candidate % GYM, candidate / GYM);
        int neighbours = count_neighbours_with_func(target,
                                                    &_feat_is_awakenable);

        // We can target solid cells, which themselves will awaken, so count
        // those as well.
        if (_feat_is_awakenable(grd(target)))
            neighbours++;

        if (neighbours > 0)
            targets.emplace_back(target, neighbours * neighbours);
    }

    coord_def* choice = random_choose_weighted(targets);
    return choice ? *choice : coord_def(0, 0);
}

bool scattershot_tracer(monster *caster, int pow, coord_def aim)
{
    targetter_shotgun hitfunc(caster, shotgun_beam_count(pow),
                              spell_range(SPELL_SCATTERSHOT, pow));
    hitfunc.set_aim(aim);

    mon_attitude_type castatt = caster->temp_attitude();
    int friendly = 0, enemy = 0;

    for (const auto &entry : hitfunc.zapped)
    {
        if (entry.second <= 0)
            continue;

        const actor *victim = actor_at(entry.first);
        if (!victim)
            continue;

        if (mons_atts_aligned(castatt, victim->temp_attitude()))
            friendly += victim->get_experience_level();
        else
            enemy += victim->get_experience_level();
    }

    return enemy > friendly;
}

/** Chooses a matching spell from this spell list, based on frequency.
 *
 *  @param[in]  spells     the monster spell list to search
 *  @param[in]  flag       what SPFLAG_ the spell should match
 *  @param[out] slot_flags the flags of the slot the spell was found in.
 *
 *  @return The spell chosen, or SPELL_NO_SPELL if no matching spell was
 *          found. In the latter case, slot_flags is not assigned to.
 */
static spell_type _pick_spell_from_list(const monster_spells &spells,
                                        int flag,
                                        mon_spell_slot_flags &slot_flags)
{
    spell_type spell_cast = SPELL_NO_SPELL;
    int weight = 0;
    for (const mon_spell_slot &slot : spells)
    {
        int flags = get_spell_flags(slot.spell);
        if (!(flags & flag))
            continue;

        weight += slot.freq;
        if (x_chance_in_y(slot.freq, weight))
        {
            spell_cast = slot.spell;
            slot_flags = slot.flags;
        }
    }

    return spell_cast;
}

/**
 * Are we a short distance from our target?
 *
 * @param  mons The monster checking distance from its target.
 * @return true if we have a target and are within LOS_RADIUS / 2 of that
 *         target, or false otherwise.
 */
static bool _short_target_range(const monster *mons)
{
    return mons->get_foe()
           && mons->pos().distance_from(mons->get_foe()->pos())
              < LOS_RADIUS / 2;
}

/**
 * Are we a long distance from our target?
 *
 * @param  mons The monster checking distance from its target.
 * @return true if we have a target and are outside LOS_RADIUS / 2 of that
 *         target, or false otherwise.
 */
static bool _long_target_range(const monster *mons)
{
    return mons->get_foe()
           && mons->pos().distance_from(mons->get_foe()->pos())
              > LOS_RADIUS / 2;
}

/**
 * Give a monster a chance to cast a spell.
 *
 * @param mons the monster that might cast.
 * @param beem[in,out] Contains some basic information like who the monster
 *                     is attacking; filled out with more information about
 *                     the attack.
 * @param return whether a spell was cast.
 */
bool handle_mon_spell(monster* mons, bolt &beem)
{
    bool finalAnswer   = false;   // as in: "Is that your...?" {dlb}
    bool reroll        = mons->has_ench(ENCH_EMPOWERED_SPELLS);
    const actor *foe = mons->get_foe();

    if (is_sanctuary(mons->pos()) && !mons->wont_attack())
        return false;

    // Yes, there is a logic to this ordering {dlb}:
    // .. berserk check is necessary for out-of-sequence actions like emergency
    // slot spells {blue}
    if (mons->asleep()
        || mons->submerged()
        || mons->berserk_or_insane()
        || mons_is_confused(mons, false)
        || !mons->has_spells())
    {
        return false;
    }

    mon_spell_slot_flags flags = MON_SPELL_NO_FLAGS;
    spell_type spell_cast = SPELL_NO_SPELL;

    monster_spells hspell_pass(mons->spells);

    if (mons->is_silenced() || mons->is_shapeshifter())
    {
        erase_if(hspell_pass, [](const mon_spell_slot &t) {
            return t.flags & MON_SPELL_SILENCE_MASK;
        });
    }

    if (!hspell_pass.size())
        return false;

    // Monsters caught in a net try to get away.
    // This is only urgent if enemies are around.
    if (!finalAnswer && mon_enemies_around(mons)
        && mons->caught() && one_chance_in(15))
    {
        for (const mon_spell_slot &slot : hspell_pass)
        {
            if (_ms_quick_get_away(mons, slot.spell))
            {
                spell_cast = slot.spell;
                flags = slot.flags;
                finalAnswer = true;
                break;
            }
        }
    }

    const bool emergency = mons->hit_points < mons->max_hit_points / 3;

    // Promote the casting of useful spells for low-HP monsters.
    // (kraken should always cast their escape spell of inky).
    if (!finalAnswer && emergency
        && one_chance_in(mons->type == MONS_KRAKEN ? 4 : 8))
    {
        // Note: There should always be at least some chance we don't
        // get here... even if the monster is on its last HP. That
        // way we don't have to worry about monsters infinitely casting
        // Healing on themselves (e.g. orc high priests).
        int found_spell = 0;
        for (const mon_spell_slot &slot : hspell_pass)
        {
            if (_ms_low_hitpoint_cast(mons, slot)
                && one_chance_in(++found_spell))
            {
                spell_cast = slot.spell;
                flags = slot.flags;
                finalAnswer = true;
            }
        }
    }

    bool ignore_good_idea = false;
    if (does_ru_wanna_redirect(mons))
    {
        ru_interference interference = get_ru_attack_interference_level();
        if (interference == DO_BLOCK_ATTACK)
        {
            if (mons->is_actual_spellcaster())
            {
                simple_monster_message(mons,
                    " begins to cast a spell, but is stunned by your will!",
                    MSGCH_GOD);
            }
            else if (mons->is_priest())
            {
                simple_monster_message(mons,
                    " begins to pray, but is stunned by your will!",
                    MSGCH_GOD);
            }
            else
            {
                simple_monster_message(mons,
                    " begins to attack, but is stunned by your will!",
                    MSGCH_GOD);
            }
            mons->lose_energy(EUT_SPELL);
            return true;
        }
        else if (interference == DO_REDIRECT_ATTACK)
        {
            mprf(MSGCH_GOD, "You redirect %s's attack!",
                    mons->name(DESC_THE).c_str());
            int pfound = 0;
            for (radius_iterator ri(you.pos(),
                LOS_DEFAULT); ri; ++ri)
            {
                monster* new_target = monster_at(*ri);

                if (new_target == nullptr
                    || mons_is_projectile(new_target->type)
                    || mons_is_firewood(new_target)
                    || new_target->friendly())
                {
                    continue;
                }

                ASSERT(new_target);

                if (one_chance_in(++pfound))
                {
                    mons->target = new_target->pos();
                    mons->foe = new_target->mindex();
                    beem.target = mons->target;
                    ignore_good_idea = true;
                }
            }
        }
    }

    if (!finalAnswer)
    {
        // If nothing found by now, safe friendlies and good
        // neutrals will rarely cast.
        if (mons->wont_attack() && !mon_enemies_around(mons)
            && !one_chance_in(10))
        {
            return false;
        }

        // Remove healing/invis/haste if we don't need them.
        erase_if(hspell_pass, [&](const mon_spell_slot &t) {
            return _ms_waste_of_time(mons, t)
                // Should monster not have selected dig by now,
                // it never will.
                || t.spell == SPELL_DIG;
        });

        // If no useful spells... cast no spell.
        if (!hspell_pass.size())
            return false;

        const bolt orig_beem = beem;

        for (int attempt = 0; attempt < 2; attempt++)
        {
            beem = orig_beem;

            bool spellOK = false;

            // Setup spell.
            // If we didn't find a spell on the first pass, try a
            // self-enchantment.
            if (attempt > 0 && coinflip())
            {
                spell_cast = _pick_spell_from_list(hspell_pass,
                                                   SPFLAG_SELFENCH,
                                                   flags);
            }
            // Monsters that are fleeing or pacified and leaving the
            // level will always try to choose an emergency spell.
            else if (mons_is_fleeing(mons) || mons->pacified())
            {
                spell_cast = _pick_spell_from_list(hspell_pass,
                                                   SPFLAG_EMERGENCY,
                                                   flags);

                // Pacified monsters leaving the level will only
                // try and cast escape spells.
                if (spell_cast != SPELL_NO_SPELL
                    && mons->pacified()
                    && !testbits(get_spell_flags(spell_cast), SPFLAG_ESCAPE))
                {
                    spell_cast = SPELL_NO_SPELL;
                }
            }
            else
            {
                spell_cast = SPELL_NO_SPELL;

                unsigned what = random2(200);
                unsigned int i = 0;
                for (; i < hspell_pass.size(); i++)
                {
                    if ((hspell_pass[i].flags & MON_SPELL_EMERGENCY
                         && !emergency)
                        || (hspell_pass[i].flags & MON_SPELL_SHORT_RANGE
                            && !_short_target_range(mons))
                        || (hspell_pass[i].flags & MON_SPELL_LONG_RANGE
                            && !_long_target_range(mons)))
                    {
                        continue;
                    }

                    if (hspell_pass[i].freq >= what)
                        break;
                    what -= hspell_pass[i].freq;
                }

                // If we roll above the weight of the spell list,
                // don't cast a spell at all.
                if (i == hspell_pass.size())
                {
                    // Aura of Brilliance forces a reroll if the monster
                    // otherwise would not have cast a spell.
                    if (reroll)
                    {
                        reroll = false;
                        attempt--;
                        continue;
                    }
                    return false;
                }

                spell_cast = hspell_pass[i].spell;
                flags = hspell_pass[i].flags;
            }

            // Setup the spell.
            if (spell_cast != SPELL_NO_SPELL)
                setup_mons_cast(mons, beem, spell_cast);

            // Try to find a nearby ally to haste, heal, might,
            // or make invisible.
            if ((spell_cast == SPELL_HASTE_OTHER
                 || spell_cast == SPELL_HEAL_OTHER
                 || spell_cast == SPELL_MIGHT_OTHER
                 || spell_cast == SPELL_INVISIBILITY_OTHER)
                    && !_set_allied_target(mons, beem,
                           mons->type == MONS_IRONBRAND_CONVOKER))
            {
                spell_cast = SPELL_NO_SPELL;
                continue;
            }

            // Try to find an ally of the player to hex if we are
            // hexing the player.
            if (spell_cast == SPELL_ENSLAVEMENT
                && mons->foe == MHITYOU
                && !_set_hex_target(mons, beem))
            {
                spell_cast = SPELL_NO_SPELL;
                continue;
            }

            // Don't knockback something we're trying to constrict.
            const actor *victim = actor_at(beem.target);
            if (victim &&
                beem.can_knockback(victim)
                && mons->is_constricting()
                && mons->constricting->count(victim->mid))
            {
                spell_cast = SPELL_NO_SPELL;
                continue;
            }

            if ((spell_cast == SPELL_LRD
                 || spell_cast == SPELL_CONJURE_FLAME
                 || spell_cast == SPELL_FULMINANT_PRISM
                 || spell_cast == SPELL_AWAKEN_EARTH)
                && !in_bounds(beem.target))
            {
                spell_cast = SPELL_NO_SPELL;
                continue;
            }

            if (spell_cast == SPELL_DAZZLING_SPRAY
                && (!foe
                    || !_spray_tracer(mons, _mons_spellpower(spell_cast, *mons),
                                      beem, spell_cast)))
            {
                spell_cast = SPELL_NO_SPELL;
                continue;
            }

            // beam-type spells requiring tracers
            if (get_spell_flags(spell_cast) & SPFLAG_NEEDS_TRACER)
            {
                const bool explode =
                    spell_is_direct_explosion(spell_cast);
                fire_tracer(mons, beem, explode);
                // Good idea?
                if (mons_should_fire(beem, ignore_good_idea))
                    spellOK = true;
            }
            else
            {
                // All direct-effect/summoning/self-enchantments/etc.
                spellOK = true;

                if (_ms_direct_nasty(spell_cast)
                    && mons_aligned(mons, (mons->foe == MHITYOU) ?
                       &you : foe)) // foe=get_foe() is nullptr for friendlies
                {                   // targeting you, which is bad here.
                    spellOK = false;
                }
                else if (mons->foe == MHITYOU || mons->foe == MHITNOT)
                {
                    // XXX: Note the crude hack so that monsters can
                    // use ME_ALERT to target (we should really have
                    // a measure of time instead of peeking to see
                    // if the player is still there). -- bwr
                    if (!you.visible_to(mons)
                        && (mons->target != you.pos() || coinflip()))
                    {
                        spellOK = false;
                    }
                }
                else if (!mons->can_see(*foe))
                    spellOK = false;
                else if (mons->type == MONS_DAEVA
                         && mons->god == GOD_SHINING_ONE)
                {
                    // Don't allow TSO-worshipping daevas to make
                    // unchivalric magic attacks, except against
                    // appropriate monsters.
                    if (find_stab_type(mons, foe) != STAB_NO_STAB
                        && !tso_unchivalric_attack_safe_monster(foe->as_monster()))
                    {
                        spellOK = false;
                    }
                }
            }

            if (!spellOK)
                spell_cast = SPELL_NO_SPELL;

            if (spell_cast != SPELL_NO_SPELL)
                break;
        }
    }

    // Should the monster *still* not have a spell, well, too bad {dlb}:
    if (spell_cast == SPELL_NO_SPELL)
        return false;

    // Check for antimagic if casting a spell spell.
    if (mons->has_ench(ENCH_ANTIMAGIC) && flags & MON_SPELL_ANTIMAGIC_MASK
        && !x_chance_in_y(4 * BASELINE_DELAY,
                          4 * BASELINE_DELAY
                          + mons->get_ench(ENCH_ANTIMAGIC).duration))
    {
        // This may be a bad idea -- if we decide monsters shouldn't
        // lose a turn like players do not, please make this just return.
        simple_monster_message(mons, " falters for a moment.");
        mons->lose_energy(EUT_SPELL);
        return true;
    }

    // Dragons now have a time-out on their breath weapons, draconians too!
    if (flags & MON_SPELL_BREATH)
        setup_breath_timeout(mons);

    // FINALLY! determine primary spell effects {dlb}:
    if (spell_cast == SPELL_BLINK || spell_cast == SPELL_CONTROLLED_BLINK)
    {
        // Why only cast blink if nearby? {dlb}
        if (mons->can_see(you))
        {
            mons_cast_noise(mons, beem, spell_cast, flags);
            monster_blink(mons);
        }
        else
            return false;
    }
    else if (spell_cast == SPELL_BLINK_RANGE)
        blink_range(mons);
    else if (spell_cast == SPELL_BLINK_AWAY)
        blink_away(mons, true);
    else if (spell_cast == SPELL_BLINK_CLOSE)
        blink_close(mons);
    else
    {
        const bool battlesphere = mons->props.exists("battlesphere");
        if (!(get_spell_flags(spell_cast) & SPFLAG_UTILITY))
            make_mons_stop_fleeing(mons);

        if (battlesphere)
            aim_battlesphere(mons, spell_cast, beem.ench_power, beem);
        mons_cast(mons, beem, spell_cast, flags);
        if (battlesphere)
            trigger_battlesphere(mons, beem);
        if (flags & MON_SPELL_WIZARD && mons->has_ench(ENCH_SAP_MAGIC))
        {
            mons->add_ench(mon_enchant(ENCH_ANTIMAGIC, 0,
                                       mons->get_ench(ENCH_SAP_MAGIC).agent(),
                                       6 * BASELINE_DELAY));
        }
        // Wellsprings "cast" from their own hp.
        if (spell_cast == SPELL_PRIMAL_WAVE
            && mons->type == MONS_ELEMENTAL_WELLSPRING)
        {
            mons->hurt(mons, 5 + random2(15));
            if (mons->alive())
            {
                create_monster(
                    mgen_data(MONS_WATER_ELEMENTAL, SAME_ATTITUDE(mons), mons,
                    3, spell_cast, mons->pos(), mons->foe));
            }
        }
    }

    // Reflection, fireballs, wellspring self-damage, etc.
    if (!mons->alive())
        return true;

    if (!(flags & MON_SPELL_INSTANT))
    {
        mons->lose_energy(EUT_SPELL);
        return true;
    }

    return false; // to let them do something else
}

static int _monster_abjure_target(monster* target, int pow, bool actual)
{
    int duration;

    if (!target->is_summoned(&duration))
        return 0;

    pow = max(20, fuzz_value(pow, 40, 25));

    if (!actual)
        return pow > 40 || pow >= duration;

    // TSO and Trog's abjuration protection.
    bool shielded = false;
    if (have_passive(passive_t::abjuration_protection_hd))
    {
        pow = pow * (30 - target->get_hit_dice()) / 30;
        if (pow < duration)
        {
            simple_god_message(" protects your fellow warrior from evil "
                                   "magic!");
            shielded = true;
        }
    }
    else if (have_passive(passive_t::abjuration_protection))
    {
        pow = pow / 2;
        if (pow < duration)
        {
            simple_god_message(" shields your ally from puny magic!");
            shielded = true;
        }
    }
    else if (is_sanctuary(target->pos()))
    {
        pow = 0;
        mprf(MSGCH_GOD, "Zin's power protects your fellow warrior from evil magic!");
        shielded = true;
    }

    dprf("Abj: dur: %d, pow: %d, ndur: %d", duration, pow, duration - pow);

    mon_enchant abj = target->get_ench(ENCH_ABJ);
    if (!target->lose_ench_duration(abj, pow))
    {
        if (!shielded)
            simple_monster_message(target, " shudders.");
        return 1;
    }

    return 0;
}

static int _monster_abjuration(const monster* caster, bool actual)
{
    int maffected = 0;

    if (actual)
        mpr("Send 'em back where they came from!");

    const int pow = _mons_spellpower(SPELL_ABJURATION, *caster);

    for (monster_near_iterator mi(caster->pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (!mons_aligned(caster, *mi))
            maffected += _monster_abjure_target(*mi, pow, actual);
    }

    return maffected;
}

static bool _mons_will_abjure(monster* mons, spell_type spell)
{
    if (get_spell_flags(spell) & SPFLAG_MONS_ABJURE
        && _monster_abjuration(mons, false) > 0
        && one_chance_in(3))
    {
        return true;
    }

    return false;
}

static void _haunt_fixup(monster* summon, coord_def pos)
{
    actor* victim = actor_at(pos);
    if (victim && victim != summon)
    {
        summon->add_ench(mon_enchant(ENCH_HAUNTING, 1, victim,
                                     INFINITE_DURATION));
        summon->foe = victim->mindex();
    }
}

static monster_type _pick_horrible_thing()
{
    return one_chance_in(4) ? MONS_TENTACLED_MONSTROSITY
                            : MONS_ABOMINATION_LARGE;
}

static monster_type _pick_undead_summon()
{
    static monster_type undead[] =
    {
        MONS_NECROPHAGE, MONS_JIANGSHI, MONS_HUNGRY_GHOST, MONS_FLAYED_GHOST,
        MONS_ZOMBIE, MONS_SKELETON, MONS_SIMULACRUM, MONS_SPECTRAL_THING,
        MONS_FLYING_SKULL, MONS_MUMMY, MONS_VAMPIRE, MONS_WIGHT, MONS_WRAITH,
        MONS_SHADOW_WRAITH, MONS_FREEZING_WRAITH, MONS_PHANTASMAL_WARRIOR, MONS_SHADOW
    };

    return RANDOM_ELEMENT(undead);
}

static monster_type _pick_vermin()
{
    return random_choose_weighted(8, MONS_HELL_RAT,
                                  5, MONS_REDBACK,
                                  2, MONS_TARANTELLA,
                                  2, MONS_JUMPING_SPIDER,
                                  3, MONS_DEMONIC_CRAWLER,
                                  0);
}

static monster_type _pick_drake()
{
    return random_choose_weighted(5, MONS_SWAMP_DRAKE,
                                  5, MONS_KOMODO_DRAGON,
                                  5, MONS_WIND_DRAKE,
                                  6, MONS_RIME_DRAKE,
                                  6, MONS_DEATH_DRAKE,
                                  3, MONS_LINDWURM,
                                  0);
}

static void _do_high_level_summon(monster* mons, spell_type spell_cast,
                                  monster_type (*mpicker)(), int nsummons,
                                  god_type god, const coord_def *target = nullptr,
                                  void (*post_hook)(monster*, coord_def)
                                      = nullptr)
{
    const int duration = min(2 + mons->spell_hd(spell_cast) / 5, 6);

    for (int i = 0; i < nsummons; ++i)
    {
        monster_type which_mons = mpicker();

        if (which_mons == MONS_NO_MONSTER)
            continue;

        monster* summon = create_monster(
            mgen_data(which_mons, SAME_ATTITUDE(mons), mons,
                      duration, spell_cast, target ? *target : mons->pos(),
                      mons->foe, MG_NONE, god));
        if (summon && post_hook)
            post_hook(summon, target ? *target : mons->pos());
    }
}

static void _mons_summon_elemental(monster* mons,
                                   spell_type spell_cast,
                                   monster_type elemental,
                                   god_type god)
{
    const int count = 1 + (mons->spell_hd(spell_cast) > 15)
                      + random2(mons->spell_hd(spell_cast) / 7 + 1);

    for (int i = 0; i < count; i++)
    {
        create_monster(
            mgen_data(elemental, SAME_ATTITUDE(mons), mons,
                      3, spell_cast, mons->pos(), mons->foe, MG_NONE, god));
    }
    return;
}

// Returns true if a message referring to the player's legs makes sense.
static bool _legs_msg_applicable()
{
    return you.species != SP_NAGA && !you.fishtail;
}

static void _mons_cast_haunt(monster* mons)
{
    ASSERT(mons->get_foe());
    const coord_def fpos = mons->get_foe()->pos();

    _do_high_level_summon(mons, SPELL_HAUNT, pick_random_wraith,
                          random_range(2, 3), GOD_NO_GOD, &fpos, _haunt_fixup);
}

static void _mons_cast_summon_illusion(monster* mons, spell_type spell)
{
    actor *foe = mons->get_foe();
    if (!foe || !actor_is_illusion_cloneable(foe))
        return;

    mons_summon_illusion_from(mons, foe, spell);
}

static void _mons_cast_spectral_orcs(monster* mons)
{
    ASSERT(mons->get_foe());
    const coord_def fpos = mons->get_foe()->pos();

    const int abj = 3;

    for (int i = random2(3) + 1; i > 0; --i)
    {
        monster_type mon = MONS_ORC;
        if (coinflip())
            mon = MONS_ORC_WARRIOR;
        else if (one_chance_in(3))
            mon = MONS_ORC_KNIGHT;
        else if (one_chance_in(10))
            mon = MONS_ORC_WARLORD;

        // Use the original monster type as the zombified type here, to
        // get the proper stats from it.
        if (monster *orc = create_monster(
                  mgen_data(MONS_SPECTRAL_THING, SAME_ATTITUDE(mons), mons,
                          abj, SPELL_SUMMON_SPECTRAL_ORCS, fpos, mons->foe,
                          MG_NONE, mons->god, mon)))
        {
            // set which base type this orc is pretending to be for gear
            // purposes
            if (mon != MONS_ORC)
            {
                orc->mname = mons_type_name(mon, DESC_PLAIN);
                orc->flags |= MF_NAME_REPLACE | MF_NAME_DESCRIPTOR;
            }

            // give gear using the base type
            give_item(orc, env.absdepth0, true, true);

            // set gear as summoned
            orc->mark_summoned(abj, true, SPELL_SUMMON_SPECTRAL_ORCS);
        }
    }
}

static bool _mons_vampiric_drain(monster *mons)
{
    actor *target = mons->get_foe();
    if (!target)
        return false;
    if (grid_distance(mons->pos(), target->pos()) > 1)
        return false;

    const int pow = _mons_spellpower(SPELL_VAMPIRIC_DRAINING, *mons);
    int hp_cost = 3 + random2avg(9, 2) + 1 + random2(pow) / 7;

    hp_cost = min(hp_cost, target->stat_hp());
    hp_cost = min(hp_cost, mons->max_hit_points - mons->hit_points);
    if (target->res_negative_energy())
        hp_cost = 0;

    if (!hp_cost)
    {
        simple_monster_message(mons,
                               " is infused with unholy energy, but nothing happens.",
                               MSGCH_MONSTER_SPELL);
        return false;
    }

    dprf("vamp draining: %d damage, %d healing", hp_cost, hp_cost/2);

    if (you.can_see(*mons))
    {
        simple_monster_message(mons,
                               " is infused with unholy energy.",
                               MSGCH_MONSTER_SPELL);
    }
    else
        mpr("Unholy energy fills the air.");

    if (target->is_player())
    {
        ouch(hp_cost, KILLED_BY_BEAM, mons->mid, "by vampiric draining");
        if (mons->heal(hp_cost * 2 / 3))
        {
            simple_monster_message(mons,
                " draws life force from you and is healed!");
        }
    }
    else
    {
        monster* mtarget = target->as_monster();
        const string targname = mtarget->name(DESC_THE);
        mtarget->hurt(mons, hp_cost);
        if (mtarget->is_summoned())
        {
            simple_monster_message(mons,
                                   make_stringf(" draws life force from %s!",
                                                targname.c_str()).c_str());
        }
        else if (mons->heal(hp_cost * 2 / 3))
        {
            simple_monster_message(mons,
                make_stringf(" draws life force from %s and is healed!",
                targname.c_str()).c_str());
        }
        if (mtarget->alive())
            print_wounds(mtarget);
    }

    return true;
}

static bool _mons_cast_freeze(monster* mons)
{
    actor *target = mons->get_foe();
    if (!target)
        return false;
    if (grid_distance(mons->pos(), target->pos()) > 1)
        return false;

    const int pow = _mons_spellpower(SPELL_FREEZE, *mons);

    const int base_damage = roll_dice(1, 3 + pow / 6);
    int damage = 0;

    if (target->is_player())
        damage = resist_adjust_damage(&you, BEAM_COLD, base_damage);
    else
    {
        bolt beam;
        beam.flavour = BEAM_COLD;
        damage = mons_adjust_flavoured(target->as_monster(), beam, base_damage);
    }

    if (you.can_see(*target))
    {
        mprf("%s %s frozen.", target->name(DESC_THE).c_str(),
                              target->conj_verb("are").c_str());
    }

    target->hurt(mons, damage, BEAM_COLD, KILLED_BY_BEAM, "", "by Freeze");

    if (target->alive())
    {
        target->expose_to_element(BEAM_COLD, damage);

        if (target->is_monster() && target->res_cold() <= 0)
        {
            const int stun = (1 - target->res_cold())
                             * random2(min(7, 2 + pow/24));
            target->as_monster()->speed_increment -= stun;
        }
    }

    return true;
}

void setup_breath_timeout(monster* mons)
{
    if (mons->has_ench(ENCH_BREATH_WEAPON))
        return;

    int timeout = roll_dice(1, 5);

    dprf("breath timeout: %d", timeout);

    mon_enchant breath_timeout = mon_enchant(ENCH_BREATH_WEAPON, 1, mons, timeout*10);
    mons->add_ench(breath_timeout);
}

/**
 * Maybe mesmerise the player.
 *
 * This function decides whether or not it is possible for the player to become
 * mesmerised by mons. It will return a variety of values depending on whether
 * or not this can succeed or has succeeded; finally, it will add mons to the
 * player's list of beholders.
 *
 * @param mons      The monster doing the mesmerisation.
 * @param actual    Whether or not we are actually casting the spell. If false,
 *                  no messages are emitted.
 * @return          0 if the player could be mesmerised but wasn't, 1 if the
 *                  player was mesmerised, -1 if the player couldn't be
 *                  mesmerised.
**/
static int _mons_mesmerise(monster* mons, bool actual)
{
    ASSERT(mons); // XXX: change to monster &mons
    bool already_mesmerised = you.beheld_by(mons);

    if (!you.visible_to(mons)             // Don't mesmerise while invisible.
        || (!you.can_see(*mons)           // Or if we are, and you're aren't
            && !already_mesmerised)       // already mesmerised by us.
        || !player_can_hear(mons->pos())  // Or if you're silenced, or we are.
        || you.berserk()                  // Or if you're berserk.
        || mons->has_ench(ENCH_CONFUSION) // Or we're confused,
        || mons_is_fleeing(mons)          // fleeing,
        || mons->pacified()               // pacified,
        || mons->friendly())              // or friendly!
    {
        return -1;
    }

    if (actual)
    {
        if (!already_mesmerised)
        {
            simple_monster_message(mons, " attempts to bespell you!");
            flash_view(UA_MONSTER, LIGHTMAGENTA);
        }
        else
        {
            mprf("%s draws you further into %s thrall.",
                    mons->name(DESC_THE).c_str(),
                    mons->pronoun(PRONOUN_POSSESSIVE).c_str());
        }
    }

    const int pow = _ench_power(SPELL_MESMERISE, *mons);
    const int res_magic = you.check_res_magic(pow);

    // Don't mesmerise if you pass an MR check or have clarity.
    // If you're already mesmerised, you cannot resist further.
    if ((res_magic > 0 || you.clarity()
         || you.duration[DUR_MESMERISE_IMMUNE]) && !already_mesmerised)
    {
        if (actual)
        {
            if (you.clarity())
                canned_msg(MSG_YOU_UNAFFECTED);
            else if (you.duration[DUR_MESMERISE_IMMUNE] && !already_mesmerised)
                canned_msg(MSG_YOU_RESIST);
            else
                mprf("You%s", you.resist_margin_phrase(res_magic).c_str());
        }

        return 0;
    }

    you.add_beholder(mons);

    return 1;
}

// Check whether targets might be scared.
// Returns 0, if targets can be scared but the attempt failed or wasn't made.
// Returns 1, if targets are scared.
// Returns -1, if targets can never be scared.
static int _mons_cause_fear(monster* mons, bool actual)
{
    if (actual)
    {
        if (you.can_see(*mons))
            simple_monster_message(mons, " radiates an aura of fear!");
        else if (you.see_cell(mons->pos()))
            mpr("An aura of fear fills the air!");
    }

    int retval = -1;

    const int pow = _ench_power(SPELL_CAUSE_FEAR, *mons);

    if (mons->see_cell_no_trans(you.pos())
        && mons->can_see(you)
        && !mons->wont_attack()
        && !you.afraid_of(mons))
    {
        if (!(you.holiness() & MH_NATURAL))
        {
            if (actual)
                canned_msg(MSG_YOU_UNAFFECTED);
        }
        else if (!actual)
            retval = 0;
        else
        {
            const int res_margin = you.check_res_magic(pow);
            if (res_margin > 0)
                mprf("You%s", you.resist_margin_phrase(res_margin).c_str());
            else if (you.add_fearmonger(mons))
            {
                retval = 1;

                you.increase_duration(DUR_AFRAID, 10 + random2avg(pow / 10, 4));

                if (!mons->has_ench(ENCH_FEAR_INSPIRING))
                    mons->add_ench(ENCH_FEAR_INSPIRING);
            }
        }
    }

    for (monster_near_iterator mi(mons->pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (*mi == mons)
            continue;

        // Magic-immune, unnatural and "firewood" monsters are
        // immune to being scared. Same-aligned monsters are
        // never affected, even though they aren't immune.
        // Will not further scare a monster that is already afraid.
        if (mons_immune_magic(*mi)
            || !(mi->holiness() & MH_NATURAL)
            || mons_is_firewood(*mi)
            || mons_atts_aligned(mi->attitude, mons->attitude)
            || mi->has_ench(ENCH_FEAR))
        {
            continue;
        }

        retval = max(retval, 0);

        if (!actual)
            continue;

        // It's possible to scare this monster. If its magic
        // resistance fails, do so.
        int res_margin = mi->check_res_magic(pow);
        if (res_margin > 0)
        {
            simple_monster_message(*mi,
                mi->resist_margin_phrase(res_margin).c_str());
            continue;
        }

        if (mi->add_ench(mon_enchant(ENCH_FEAR, 0, mons)))
        {
            retval = 1;

            if (you.can_see(**mi))
                simple_monster_message(*mi, " looks frightened!");

            behaviour_event(*mi, ME_SCARE, mons);

            if (!mons->has_ench(ENCH_FEAR_INSPIRING))
                mons->add_ench(ENCH_FEAR_INSPIRING);
        }
    }

    if (actual && retval == 1 && you.see_cell(mons->pos()))
        flash_view_delay(UA_MONSTER, DARKGREY, 300);

    return retval;
}

static int _mons_mass_confuse(monster* mons, bool actual)
{
    int retval = -1;

    const int pow = _ench_power(SPELL_MASS_CONFUSION, *mons);

    if (mons->see_cell_no_trans(you.pos())
        && mons->can_see(you)
        && !mons->wont_attack())
    {
        retval = 0;

        if (actual)
        {
            const int res_magic = you.check_res_magic(pow);
            if (res_magic > 0)
                mprf("You%s", you.resist_margin_phrase(res_magic).c_str());
            else
            {
                you.confuse(mons, 5 + random2(3));
                retval = 1;
            }
        }
    }

    for (monster_near_iterator mi(mons->pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (*mi == mons)
            continue;

        if (mons_immune_magic(*mi)
            || mons_is_firewood(*mi)
            || mons_atts_aligned(mi->attitude, mons->attitude)
            || mons->has_ench(ENCH_HEXED))
        {
            continue;
        }

        retval = max(retval, 0);

        int res_margin = mi->check_res_magic(pow);
        if (res_margin > 0)
        {
            if (actual)
            {
                simple_monster_message(*mi,
                    mi->resist_margin_phrase(res_margin).c_str());
            }
            continue;
        }
        if (actual)
        {
            retval = 1;
            mi->confuse(mons, 5 + random2(3));
        }
    }

    return retval;
}

static int _mons_control_undead(monster* mons, bool actual)
{
    int retval = -1;

    const int pow = _ench_power(SPELL_CONTROL_UNDEAD, *mons);

    if (mons->see_cell_no_trans(you.pos())
        && mons->can_see(you)
        && !mons->wont_attack()
        && you.holiness() & MH_UNDEAD)
    {
        retval = 0;

        if (actual)
        {
            int res_margin = you.check_res_magic(pow);
            if (res_margin > 0)
                mprf("You%s", you.resist_margin_phrase(res_margin).c_str());
            else
            {
                enchant_actor_with_flavour(&you, mons, BEAM_ENSLAVE);
                retval = 1;
            }
        }
    }

    enchant_type good = (mons->wont_attack()) ? ENCH_CHARM
                                              : ENCH_HEXED;
    enchant_type bad  = (mons->wont_attack()) ? ENCH_HEXED
                                              : ENCH_CHARM;
    for (monster_near_iterator mi(mons->pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (*mi == mons)
            continue;

        if (mons_immune_magic(*mi)
            || mons_is_firewood(*mi)
            || (mons_atts_aligned(mi->attitude, mons->attitude)
                && !mi->has_ench(bad))
            || !(mi->holiness() & MH_UNDEAD))
        {
            continue;
        }

        retval = max(retval, 0);

        int res_margin = mi->check_res_magic(pow);
        if (res_margin > 0)
        {
            if (actual)
            {
                simple_monster_message(*mi,
                    mi->resist_margin_phrase(res_margin).c_str());
            }
            continue;
        }
        if (actual)
        {
            retval = 1;
            if (you.can_see(**mi))
            {
                mprf("%s submits to %s will!",
                     mi->name(DESC_YOUR).c_str(),
                     apostrophise(mons->name(DESC_THE)).c_str());
            }
            if (mi->has_ench(bad))
                mi->del_ench(bad);
            else
                mi->add_ench(mon_enchant(good, 0, mons));
        }
    }

    return retval;
}

static coord_def _mons_fragment_target(monster *mons)
{
    coord_def target(GXM+1, GYM+1);
    const int pow = _mons_spellpower(SPELL_LRD, *mons);

    // Shadow casting should try to affect the same tile as the player.
    if (mons_is_player_shadow(mons))
    {
        bool temp;
        bolt beam;
        if (!setup_fragmentation_beam(beam, pow, mons, mons->target, false,
                                      true, true, nullptr, temp, temp))
        {
            return target;
        }
        return mons->target;
    }

    const int range = _mons_spell_range(SPELL_LRD, *mons);
    int maxpower = 0;
    for (distance_iterator di(mons->pos(), true, true, range); di; ++di)
    {
        bool temp;

        if (!cell_see_cell(mons->pos(), *di, LOS_SOLID))
            continue;

        bolt beam;
        if (!setup_fragmentation_beam(beam, pow, mons, *di, false, true, true,
                                      nullptr, temp, temp))
        {
            continue;
        }

        beam.range = range;
        fire_tracer(mons, beam, true);
        if (!mons_should_fire(beam))
            continue;

        bolt beam2;
        if (!setup_fragmentation_beam(beam2, pow, mons, *di, false, false, true,
                                      nullptr, temp, temp))
        {
            continue;
        }

        beam2.range = range;
        fire_tracer(mons, beam2, true);

        if (beam2.foe_info.count > 0
            && beam2.foe_info.power > maxpower)
        {
            maxpower = beam2.foe_info.power;
            target = *di;
        }
    }

    return target;
}

static void _blink_allies_encircle(const monster* mon)
{
    vector<monster*> allies;
    const coord_def foepos = mon->get_foe()->pos();

    for (monster_near_iterator mi(mon, LOS_NO_TRANS); mi; ++mi)
    {
        if (_valid_encircle_ally(mon, *mi, foepos))
            allies.push_back(*mi);
    }
    shuffle_array(allies);

    int count = max(1, mon->spell_hd(SPELL_BLINK_ALLIES_ENCIRCLE) / 8
                       + random2(mon->spell_hd(SPELL_BLINK_ALLIES_ENCIRCLE) / 4));

    for (monster *ally : allies)
    {
        coord_def empty;
        if (find_habitable_spot_near(foepos, mons_base_type(ally), 1, false, empty))
        {
            if (ally->blink_to(empty))
            {
                // XXX: This seems an awkward way to give a message for something
                // blinking from out of sight into sight. Probably could use a
                // more general solution.
                if (!(ally->flags & MF_WAS_IN_VIEW)
                    && ally->flags & MF_SEEN)
                {
                    simple_monster_message(ally, " blinks into view!");
                }
                ally->behaviour = BEH_SEEK;
                ally->foe = mon->foe;
                count--;
            }
        }
    }
}

static void _blink_allies_away(const monster* mon)
{
    vector<monster*> allies;
    const coord_def foepos = mon->get_foe()->pos();

    for (monster_near_iterator mi(mon, LOS_NO_TRANS); mi; ++mi)
    {
        if (_valid_blink_away_ally(mon, *mi, foepos))
            allies.push_back(*mi);
    }
    shuffle_array(allies);

    int count = max(1, mon->spell_hd(SPELL_BLINK_ALLIES_AWAY) / 8
                       + random2(mon->spell_hd(SPELL_BLINK_ALLIES_AWAY) / 4));

    for (unsigned int i = 0; i < allies.size() && count; ++i)
    {
        if (blink_away(allies[i], &you, false))
            count--;
    }
}

struct branch_summon_pair
{
    branch_type     origin;
    const pop_entry *pop;
};

static const pop_entry _invitation_lair[] =
{ // Lair enemies
  {  1,   1,   80, FLAT, MONS_YAK },
  {  1,   1,   60, FLAT, MONS_BLINK_FROG },
  {  1,   1,   40, FLAT, MONS_ELEPHANT },
  {  1,   1,   20, FLAT, MONS_SPINY_FROG },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _invitation_snake[] =
{ // Snake enemies
  {  1,   1,   90, FLAT, MONS_NAGA },
  {  1,   1,   70, FLAT, MONS_WATER_MOCCASIN },
  {  1,   1,   30, FLAT, MONS_BLACK_MAMBA },
  {  1,   1,   10, FLAT, MONS_GUARDIAN_SERPENT },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _invitation_spider[] =
{ // Spider enemies
  {  1,   1,   60, FLAT, MONS_TARANTELLA },
  {  1,   1,   80, FLAT, MONS_JUMPING_SPIDER },
  {  1,   1,   20, FLAT, MONS_REDBACK },
  {  1,   1,   20, FLAT, MONS_ORB_SPIDER },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _invitation_swamp[] =
{ // Swamp enemies
  {  1,   1,   80, FLAT, MONS_VAMPIRE_MOSQUITO },
  {  1,   1,   60, FLAT, MONS_INSUBSTANTIAL_WISP },
  {  1,   1,   60, FLAT, MONS_SWAMP_DRAKE },
  {  1,   1,   10, FLAT, MONS_ALLIGATOR },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _invitation_shoals[] =
{ // Swamp enemies
  {  1,   1,   80, FLAT, MONS_MERFOLK },
  {  1,   1,   60, FLAT, MONS_SIREN },
  {  1,   1,   40, FLAT, MONS_MANTICORE },
  {  1,   1,   20, FLAT, MONS_SNAPPING_TURTLE },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _invitation_orc[] =
{ // Orc enemies
  {  1,   1,   90, FLAT, MONS_ORC_PRIEST },
  {  1,   1,   70, FLAT, MONS_ORC_WARRIOR },
  {  1,   1,   30, FLAT, MONS_WARG },
  {  1,   1,   10, FLAT, MONS_TROLL },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _invitation_elf[] =
{ // Elf enemies
  {  1,   1,  100, FLAT, MONS_DEEP_ELF_MAGE },
  {  1,   1,   50, FLAT, MONS_DEEP_ELF_KNIGHT },
  {  1,   1,   50, FLAT, MONS_DEEP_ELF_ARCHER },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _invitation_vaults[] =
{ // Vaults enemies
  {  1,   1,   80, FLAT, MONS_HUMAN },
  {  1,   1,   60, FLAT, MONS_YAKTAUR },
  {  1,   1,   40, FLAT, MONS_IRONHEART_PRESERVER },
  {  1,   1,   20, FLAT, MONS_VAULT_SENTINEL },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _invitation_crypt[] =
{ // Crypt enemies
  {  1,   1,   80, FLAT, MONS_WRAITH },
  {  1,   1,   60, FLAT, MONS_SHADOW },
  {  1,   1,   40, FLAT, MONS_VAMPIRE },
  {  1,   1,   20, FLAT, MONS_SHADOW_WRAITH },
  { 0,0,0,FLAT,MONS_0 }
};

static branch_summon_pair _invitation_summons[] =
{
  { BRANCH_LAIR,   _invitation_lair },
  { BRANCH_SNAKE,  _invitation_snake },
  { BRANCH_SPIDER, _invitation_spider },
  { BRANCH_SWAMP,  _invitation_swamp },
  { BRANCH_SHOALS, _invitation_shoals },
  { BRANCH_ORC,    _invitation_orc },
  { BRANCH_ELF,    _invitation_elf },
  { BRANCH_VAULTS, _invitation_vaults },
  { BRANCH_CRYPT,  _invitation_crypt }
};

static const pop_entry _planerend_lair[] =
{ // Lair enemies
  {  1,   1,  100, FLAT, MONS_CATOBLEPAS },
  {  1,   1,  100, FLAT, MONS_DIRE_ELEPHANT },
  {  1,   1,   60, FLAT, MONS_TORPOR_SNAIL },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _planerend_snake[] =
{ // Snake enemies
  {  1,   1,   40, FLAT, MONS_ANACONDA },
  {  1,   1,   60, FLAT, MONS_SALAMANDER_STORMCALLER },
  {  1,   1,  100, FLAT, MONS_GUARDIAN_SERPENT },
  {  1,   1,  100, FLAT, MONS_GREATER_NAGA },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _planerend_spider[] =
{ // Spider enemies
  {  1,   1,  100, FLAT, MONS_GHOST_MOTH },
  {  1,   1,  100, FLAT, MONS_EMPEROR_SCORPION },
  {  1,   1,   20, FLAT, MONS_HORNET },
  {  1,   1,   60, FLAT, MONS_ORB_SPIDER },
  {  1,   1,   20, FLAT, MONS_TARANTELLA },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _planerend_swamp[] =
{ // Swamp enemies
  {  1,   1,   80, FLAT, MONS_SWAMP_DRAGON },
  {  1,   1,   80, FLAT, MONS_HYDRA },
  {  1,   1,  100, FLAT, MONS_SHAMBLING_MANGROVE },
  {  1,   1,   40, FLAT, MONS_THORN_HUNTER },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _planerend_shoals[] =
{ // Shoals enemies
  {  1,   1,   50, FLAT, MONS_WATER_NYMPH },
  {  1,   1,  100, FLAT, MONS_MERFOLK_JAVELINEER },
  {  1,   1,  100, FLAT, MONS_MERFOLK_AQUAMANCER },
  {  1,   1,   80, FLAT, MONS_ALLIGATOR_SNAPPING_TURTLE },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _planerend_slime[] =
{ // Slime enemies
  {  1,   1,  100, FLAT, MONS_ACID_BLOB },
  {  1,   1,  100, FLAT, MONS_AZURE_JELLY },
  {  1,   1,  100, FLAT, MONS_SLIME_CREATURE }, // changed to titanic below
  {  1,   1,   80, FLAT, MONS_GIANT_ORANGE_BRAIN },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _planerend_orc[] =
{ // Orc enemies
  {  1,   1,  100, FLAT, MONS_ORC_WARLORD },
  {  1,   1,   80, FLAT, MONS_ORC_SORCERER },
  {  1,   1,   80, FLAT, MONS_ORC_HIGH_PRIEST },
  {  1,   1,   40, FLAT, MONS_STONE_GIANT },
  {  1,   1,   60, FLAT, MONS_OGRE_MAGE },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _planerend_elf[] =
{ // Elf enemies
  {  1,   1,  100, FLAT, MONS_DEEP_ELF_SORCERER },
  {  1,   1,  100, FLAT, MONS_DEEP_ELF_HIGH_PRIEST },
  {  1,   1,   60, FLAT, MONS_DEEP_ELF_MASTER_ARCHER },
  {  1,   1,   60, FLAT, MONS_DEEP_ELF_BLADEMASTER },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _planerend_vaults[] =
{ // Vaults enemies
  {  1,   1,   80, FLAT, MONS_VAULT_SENTINEL },
  {  1,   1,   80, FLAT, MONS_DANCING_WEAPON },
  {  1,   1,   60, FLAT, MONS_IRONBRAND_CONVOKER },
  {  1,   1,  100, FLAT, MONS_WAR_GARGOYLE },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _planerend_crypt[] =
{ // Crypt enemies
  {  1,   1,  100, FLAT, MONS_VAMPIRE_KNIGHT },
  {  1,   1,  100, FLAT, MONS_FLAYED_GHOST },
  {  1,   1,   80, FLAT, MONS_REVENANT },
  {  1,   1,   80, FLAT, MONS_DEEP_ELF_DEATH_MAGE },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _planerend_tomb[] =
{ // Tomb enemies
  {  1,   1,   60, FLAT, MONS_ANCIENT_CHAMPION },
  {  1,   1,  100, FLAT, MONS_SPHINX },
  {  1,   1,  100, FLAT, MONS_MUMMY_PRIEST },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _planerend_abyss[] =
{ // Abyss enemies
  {  1,   1,  100, FLAT, MONS_STARCURSED_MASS },
  {  1,   1,   80, FLAT, MONS_APOCALYPSE_CRAB },
  {  1,   1,   50, FLAT, MONS_THRASHING_HORROR },
  {  1,   1,   50, FLAT, MONS_VERY_UGLY_THING },
  { 0,0,0,FLAT,MONS_0 }
};

static const pop_entry _planerend_zot[] =
{ // Zot enemies
  {  1,   1,   80, FLAT, MONS_DRACONIAN_SHIFTER },
  {  1,   1,   80, FLAT, MONS_DRACONIAN_SCORCHER },
  {  1,   1,   80, FLAT, MONS_DRACONIAN_ZEALOT },
  {  1,   1,  100, FLAT, MONS_GOLDEN_DRAGON },
  {  1,   1,  100, FLAT, MONS_MOTH_OF_WRATH },
  { 0,0,0,FLAT,MONS_0 }
};

static branch_summon_pair _planerend_summons[] =
{
  { BRANCH_LAIR,   _planerend_lair },
  { BRANCH_SNAKE,  _planerend_snake },
  { BRANCH_SPIDER, _planerend_spider },
  { BRANCH_SWAMP,  _planerend_swamp },
  { BRANCH_SHOALS, _planerend_shoals },
  { BRANCH_SLIME,  _planerend_slime },
  { BRANCH_ORC,    _planerend_orc },
  { BRANCH_ELF,    _planerend_elf },
  { BRANCH_VAULTS, _planerend_vaults },
  { BRANCH_CRYPT,  _planerend_crypt },
  { BRANCH_TOMB,   _planerend_tomb },
  { BRANCH_ABYSS,  _planerend_abyss },
  { BRANCH_ZOT,    _planerend_zot }
};



static void _branch_summon_helper(monster* mons, spell_type spell_cast,
                                  branch_summon_pair *summon_list,
                                  const size_t list_size, int count)
{
    int which = 0;
    // XXX: should this not depend on the specific spell?
    if (spell_cast == SPELL_FORCEFUL_INVITATION
        && mons->props.exists("invitation_branch"))
    {
        which = mons->props["invitation_branch"].get_byte();
    }
    else
    {
        which = random2(list_size);
        if (spell_cast == SPELL_FORCEFUL_INVITATION)
            mons->props["invitation_branch"].get_byte() = which;
    }

    if (you.see_cell(mons->pos()))
    {
        string msg = getSpeakString("branch summon cast prefix");
        if (!msg.empty())
        {
            msg  = replace_all(msg, "@The_monster@", mons->name(DESC_THE));
            msg += " ";
            msg += branches[summon_list[which].origin].longname;
            msg += "!";
            mprf(mons->wont_attack() ? MSGCH_FRIEND_ENCHANT
                 : MSGCH_MONSTER_ENCHANT, "%s", msg.c_str());
        }
    }

    for (int i = 0; i < count; i++)
    {
        monster_type type = pick_monster_from(summon_list[which].pop, 1);
        if (type == MONS_NO_MONSTER)
            continue;

        mgen_data mg(type, SAME_ATTITUDE(mons), mons,
                     1, spell_cast, mons->pos(), mons->foe);
        if (type == MONS_SLIME_CREATURE)
            mg.props[MGEN_BLOB_SIZE] = 5;
        create_monster(mg);
    }
}

static void _cast_flay(monster* source, actor *defender)
{
    bool was_flayed = false;
    int damage_taken = 0;
    if (defender->is_player())
    {
        damage_taken = (6 + (get_hp() * 18 / get_hp_max())) * get_hp_max() / 100;
        damage_taken = min(damage_taken,
                           max(0, get_hp() - 25 - random2(15)));
        if (damage_taken < 10)
            return;

        if (you.duration[DUR_FLAYED])
            was_flayed = true;

        you.duration[DUR_FLAYED] = max(you.duration[DUR_FLAYED],
                                       55 + random2(66));
    }
    else
    {
        monster* mon = defender->as_monster();

        damage_taken = (6 + (mon->hit_points * 18 / mon->max_hit_points))
                       * mon->max_hit_points / 100;
        damage_taken = min(damage_taken,
                           max(0, mon->hit_points - 25 - random2(15)));
        if (damage_taken < 10)
            return;

        if (mon->has_ench(ENCH_FLAYED))
        {
            was_flayed = true;
            mon_enchant flayed = mon->get_ench(ENCH_FLAYED);
            flayed.duration = min(flayed.duration + 30 + random2(50), 150);
            mon->update_ench(flayed);
        }
        else
        {
            mon_enchant flayed(ENCH_FLAYED, 1, source, 30 + random2(50));
            mon->add_ench(flayed);
        }
    }

    if (you.can_see(*defender))
    {
        if (was_flayed)
        {
            mprf("Terrible wounds spread across more of %s body!",
                 defender->name(DESC_ITS).c_str());
        }
        else
        {
            mprf("Terrible wounds open up all over %s body!",
                 defender->name(DESC_ITS).c_str());
        }
    }

    // Due to Deep Dwarf damage shaving, the player may take less than the intended
    // amount of damage. Keep track of the actual amount of damage done by comparing
    // hp before and after the player is hurt; use this as the actual value for
    // flay damage to prevent the player from regaining extra hp when it wears off

    const int orig_hp = defender->stat_hp();

    defender->hurt(source, damage_taken, BEAM_NONE,
                   KILLED_BY_MONSTER, "", "flay_damage", true);
    defender->props["flay_damage"].get_int() += orig_hp - defender->stat_hp();

    vector<coord_def> old_blood;
    CrawlVector &new_blood = defender->props["flay_blood"].get_vector();

    // Find current blood spatters
    for (radius_iterator ri(defender->pos(), LOS_SOLID); ri; ++ri)
    {
        if (env.pgrid(*ri) & FPROP_BLOODY)
            old_blood.push_back(*ri);
    }

    blood_spray(defender->pos(), defender->type, 20);

    // Compute and store new blood spatters
    unsigned int i = 0;
    for (radius_iterator ri(defender->pos(), LOS_SOLID); ri; ++ri)
    {
        if (env.pgrid(*ri) & FPROP_BLOODY)
        {
            if (i < old_blood.size() && old_blood[i] == *ri)
                ++i;
            else
                new_blood.push_back(*ri);
        }
    }
}

// marking this extern (const defaults to static) so that monster can link to it
extern const spell_type serpent_of_hell_breaths[][3] =
{
    // Geh
    {
        SPELL_FIRE_BREATH,
        SPELL_FLAMING_CLOUD,
        SPELL_FIREBALL
    },
    // Coc
    {
        SPELL_COLD_BREATH,
        SPELL_FREEZING_CLOUD,
        SPELL_FLASH_FREEZE
    },
    // Dis
    {
        SPELL_METAL_SPLINTERS,
        SPELL_QUICKSILVER_BOLT,
        SPELL_LEHUDIBS_CRYSTAL_SPEAR
    },
    // Tar
    {
        SPELL_BOLT_OF_DRAINING,
        SPELL_MIASMA_BREATH,
        SPELL_CORROSIVE_BOLT
    }
};

static bool _spell_charged(monster *mons)
{
    mon_enchant ench = mons->get_ench(ENCH_SPELL_CHARGED);
    if (ench.ench == ENCH_NONE || ench.degree < max_mons_charge(mons->type))
    {
        if (ench.ench == ENCH_NONE)
        {
            mons->add_ench(mon_enchant(ENCH_SPELL_CHARGED, 1, mons,
                                       INFINITE_DURATION));
        }
        else
        {
            ench.degree++;
            mons->update_ench(ench);
        }

        if (!you.can_see(*mons))
            return false;
        string msg =
            getSpeakString(make_stringf("%s charge",
                                        mons->name(DESC_PLAIN, true).c_str())
                           .c_str());
        if (!msg.empty())
        {
            msg = do_mon_str_replacements(msg, mons);
            mprf(mons->wont_attack() ? MSGCH_FRIEND_ENCHANT
                 : MSGCH_MONSTER_ENCHANT, "%s", msg.c_str());
        }
        return false;
    }
    mons->del_ench(ENCH_SPELL_CHARGED);
    return true;
}

/**
 *  Make this monster cast a spell
 *
 *  @param mons       The monster casting
 *  @param pbolt      The beam, possibly containing pre-done setup, to use
 *                    for the spell. Not a reference because this function
                      shouldn't affect the original copy.
 *  @param spell_cast The spell to be cast.
 *  @param slot_flags The spell slot flags in mons->spells (is it an
 *                    invocation, natural, shouty, etc.?)
 *  @param do_noise   Whether to make noise (including casting messages).
 */
void mons_cast(monster* mons, bolt pbolt, spell_type spell_cast,
               mon_spell_slot_flags slot_flags, bool do_noise)
{
    if (spell_cast == SPELL_SERPENT_OF_HELL_BREATH)
    {
        monster_type serpent_type = mons_is_zombified(mons)
            ? mons->base_monster
            : mons->type;
        ASSERT(serpent_type >= MONS_SERPENT_OF_HELL);
        ASSERT(serpent_type <= MONS_SERPENT_OF_HELL_TARTARUS);

#if TAG_MAJOR_VERSION > 34
        const int idx = serpent_type - MONS_SERPENT_OF_HELL;
#else
        const int idx =
            serpent_type == MONS_SERPENT_OF_HELL          ? 0
          : serpent_type == MONS_SERPENT_OF_HELL_COCYTUS  ? 1
          : serpent_type == MONS_SERPENT_OF_HELL_DIS      ? 2
          : serpent_type == MONS_SERPENT_OF_HELL_TARTARUS ? 3
          :                                                 -1;
#endif
        ASSERT(idx < (int)ARRAYSZ(serpent_of_hell_breaths));
        ASSERT(idx >= 0);
        ASSERT(mons->heads() == ARRAYSZ(serpent_of_hell_breaths[idx]));

        for (int i = 0; i < mons->heads(); ++i)
        {
            if (!mons->get_foe())
                return;
            spell_type head_spell = serpent_of_hell_breaths[idx][i];
            setup_mons_cast(mons, pbolt, head_spell);
            mons_cast(mons, pbolt, head_spell, slot_flags, do_noise);
        }

        return;
    }
    // Always do setup. It might be done already, but it doesn't hurt
    // to do it again (cheap).
    setup_mons_cast(mons, pbolt, spell_cast);

    // single calculation permissible {dlb}
    const unsigned int flags = get_spell_flags(spell_cast);
    const bool orig_noise = do_noise;
    actor* const foe = mons->get_foe();

    int sumcount = 0;
    int sumcount2;
    int duration = 0;

    dprf("Mon #%d casts %s (#%d)",
         mons->mindex(), spell_title(spell_cast), spell_cast);
    ASSERT(!(flags & SPFLAG_TESTING));
    // Targeted spells need a valid target.
    // Wizard-mode cast monster spells may target the boundary (shift-dir).
    ASSERT(map_bounds(pbolt.target) || !(flags & SPFLAG_TARGETING_MASK));

    // Maybe cast abjuration instead of certain summoning spells.
    if (mons->can_see(you) && _mons_will_abjure(mons, spell_cast))
    {
        if (do_noise)
        {
            pbolt.range = 0;
            pbolt.glyph = 0;
            mons_cast_noise(mons, pbolt, SPELL_ABJURATION, MON_SPELL_NO_FLAGS);
        }
        _monster_abjuration(mons, true);
        return;
    }

    if (spell_cast == SPELL_CANTRIP
        || spell_cast == SPELL_VAMPIRIC_DRAINING
        || spell_cast == SPELL_IOOD
        || spell_cast == SPELL_INJURY_MIRROR
        || spell_cast == SPELL_DRAIN_LIFE
        || spell_cast == SPELL_TROGS_HAND
        || spell_cast == SPELL_LEDAS_LIQUEFACTION
        || spell_cast == SPELL_PORTAL_PROJECTILE
        || spell_cast == SPELL_FORCEFUL_INVITATION
        || spell_cast == SPELL_PLANEREND
        || spell_cast == SPELL_FIRE_STORM
        || spell_cast == SPELL_PARALYSIS_GAZE)
    {
        do_noise = false;       // Spell itself does the messaging.
    }

    if (do_noise)
        mons_cast_noise(mons, pbolt, spell_cast, slot_flags);

    // If this is a wizard spell, summons won't necessarily have the
    // same god. But intrinsic/priestly summons should.
    god_type god = slot_flags & MON_SPELL_WIZARD ? GOD_NO_GOD : mons->god;

    // Permanent wizard summons of Yred should have the same god even
    // though they aren't priests. This is so that e.g. the zombies of
    // Yred's enslaved souls will properly turn on you if you abandon
    // Yred.
    if (mons->god == GOD_YREDELEMNUL)
        god = mons->god;

    const int splpow = _mons_spellpower(spell_cast, *mons);

    switch (spell_cast)
    {
    default:
        break;

    case SPELL_FLAY:
        _cast_flay(mons, foe);
        return;

    case SPELL_WATERSTRIKE:
    {
        if (you.can_see(*foe))
        {
            if (foe->airborne())
                mprf("The water rises up and strikes %s!", foe->name(DESC_THE).c_str());
            else
                mprf("The water swirls and strikes %s!", foe->name(DESC_THE).c_str());
        }

        pbolt.flavour    = BEAM_WATER;

        int damage_taken = roll_dice(3, 7 + mons->spell_hd(spell_cast));
        damage_taken = foe->beam_resists(pbolt, damage_taken, false);
        damage_taken = foe->apply_ac(damage_taken);

        foe->hurt(mons, damage_taken, BEAM_MISSILE, KILLED_BY_BEAM,
                      "", "by the raging water");
        return;
    }

    case SPELL_AIRSTRIKE:
    {
        int damage_taken = 10 + 2 * mons->get_hit_dice();
        damage_taken = foe->beam_resists(pbolt, damage_taken, false);

        // Previous method of damage calculation (in line with player
        // airstrike) had absurd variance.
        damage_taken = foe->apply_ac(random2avg(damage_taken, 3));

        // Damage averages 14 for 5HD, 18 for 10HD, 28 for 20HD, +50% if flying.
        if (foe->is_player())
        {
            if (you.airborne())
                mprf("The air twists around and violently strikes you in flight! (%d)", damage_taken);
            else
                mprf("The air twists around and strikes you! (%d)", damage_taken);
        }
        else
        {
            monster_message(foe->as_monster(),
                                   " is struck by the twisting air! (%d)", damage_taken);
        }

        pbolt.flavour = BEAM_AIR;

        foe->hurt(mons, damage_taken, BEAM_MISSILE, KILLED_BY_BEAM,
                  "", "by the air");
        return;
    }

    case SPELL_HOLY_FLAMES:
        holy_flames(mons, foe);
        return;

    case SPELL_SMITING:
    {
		const int damage = 7 + random2avg(11, 2);
		foe->hurt(mons, damage, BEAM_MISSILE, KILLED_BY_BEAM,
                       "", "by divine providence");
        if (foe->is_player())
            mprf("%s smites you! (%d)", _god_name(god).c_str(), damage);
        else
            monster_message(foe->as_monster(), " is smitten. (%d)", damage);
        return;
    }

    case SPELL_BRAIN_FEED:
        if (one_chance_in(3)
            && lose_stat(STAT_INT, 1 + random2(3)))
        {
            mpr("Something feeds on your intellect!");
            xom_is_stimulated(50);
        }
        else
            mpr("Something tries to feed on your intellect!");
        return;

    case SPELL_SUMMON_SPECTRAL_ORCS:
        if (foe->is_player())
            mpr("Orcish apparitions take form around you.");
        else
            simple_monster_message(foe->as_monster(), " is surrounded by Orcish apparitions.");
        _mons_cast_spectral_orcs(mons);
        return;

    case SPELL_HAUNT:
        if (foe->is_player())
            mpr("You feel haunted.");
        else
            mpr("You sense an evil presence.");
        _mons_cast_haunt(mons);
        return;

    case SPELL_CONFUSION_GAZE:
    {
        const int res_margin = foe->check_res_magic(splpow / ENCH_POW_FACTOR);
        if (res_margin > 0)
        {
            if (you.can_see(*foe))
            {
                mprf("%s%s",
                     foe->name(DESC_THE).c_str(),
                     foe->resist_margin_phrase(res_margin).c_str());
            }
            return;
        }

        foe->confuse(mons, 5 + random2(3));
        return;
    }

    case SPELL_DRAINING_GAZE:
        enchant_actor_with_flavour(foe, mons, BEAM_DRAIN_MAGIC,
                                   mons->get_experience_level() * 12);
        return;

    case SPELL_PARALYSIS_GAZE:
        if (mons->type == MONS_GIANT_EYEBALL && !_spell_charged(mons))
            return;
        if (orig_noise)
            mons_cast_noise(mons, pbolt, spell_cast, slot_flags);
        foe->paralyse(mons, 2 + random2(3));
        return;

    case SPELL_MAJOR_HEALING:
        if (mons->heal(50 + random2avg(mons->spell_hd(spell_cast) * 10, 2)))
            simple_monster_message(mons, " is healed.");
        return;

    case SPELL_INJURY_MIRROR:
        simple_monster_message(mons,
                               make_stringf(" offers %s to %s, and fills with unholy energy.",
                                   mons->pronoun(PRONOUN_REFLEXIVE).c_str(),
                                   god_name(mons->god).c_str()).c_str(),
                               MSGCH_MONSTER_SPELL);
        mons->add_ench(mon_enchant(ENCH_MIRROR_DAMAGE, 0, mons, random_range(7, 9) * BASELINE_DELAY));
        mons->props["mirror_recast_time"].get_int() = you.elapsed_time + 150 + random2(60);
        return;

    case SPELL_VAMPIRIC_DRAINING:
        _mons_vampiric_drain(mons);
        return;

    case SPELL_BERSERKER_RAGE:
        mons->props.erase("brothers_count");
        mons->go_berserk(true);
        return;

    case SPELL_TROGS_HAND:
    {
        simple_monster_message(mons,
                               make_stringf(" invokes %s protection!",
                                   apostrophise(god_name(mons->god)).c_str()).c_str(),
                               MSGCH_MONSTER_SPELL);
        // Not spell_hd(spell_cast); this is an invocation
        const int dur = BASELINE_DELAY
            * min(5 + roll_dice(2, (mons->get_hit_dice() * 10) / 3 + 1), 100);
        mons->add_ench(mon_enchant(ENCH_RAISED_MR, 0, mons, dur));
        mons->add_ench(mon_enchant(ENCH_REGENERATION, 0, mons, dur));
        dprf("Trog's Hand cast (dur: %d aut)", dur);
        return;
    }

#if TAG_MAJOR_VERSION == 34
    // Replaced with monster-specific version.
    case SPELL_SWIFTNESS:
#endif
    case SPELL_SPRINT:
        mons->add_ench(ENCH_SWIFT);
        simple_monster_message(mons, " puts on a burst of speed!");
        return;

    case SPELL_SILENCE:
        mons->add_ench(ENCH_SILENCE);
        invalidate_agrid(true);
        simple_monster_message(mons, "'s surroundings become eerily quiet.");
        return;

    case SPELL_CALL_TIDE:
        if (player_in_branch(BRANCH_SHOALS))
        {
            const int tide_duration = BASELINE_DELAY
                * random_range(80, 200, 2);
            mons->add_ench(mon_enchant(ENCH_TIDE, 0, mons,
                                       tide_duration));
            mons->props[TIDE_CALL_TURN].get_int() = you.num_turns;
            if (simple_monster_message(
                    mons,
                    " sings a water chant to call the tide!"))
            {
                flash_view_delay(UA_MONSTER, ETC_WATER, 300);
            }
        }
        return;

    case SPELL_INK_CLOUD:
        if (!feat_is_watery(grd(mons->pos())))
            return;

        big_cloud(CLOUD_INK, mons, mons->pos(), 30, 30);

        simple_monster_message(
            mons,
            " squirts a massive cloud of ink into the water!");
        return;

#if TAG_MAJOR_VERSION == 34
    case SPELL_VAMPIRE_SUMMON:
#endif
    case SPELL_SUMMON_SMALL_MAMMAL:
        sumcount2 = 1 + random2(3);

        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            monster_type rats[] = { MONS_QUOKKA, MONS_RIVER_RAT, MONS_RAT };

            const monster_type mon = (one_chance_in(3) ? MONS_BAT
                                                       : RANDOM_ELEMENT(rats));
            create_monster(
                mgen_data(mon, SAME_ATTITUDE(mons), mons,
                          5, spell_cast, mons->pos(), mons->foe, MG_NONE, god));
        }
        return;

    case SPELL_STICKS_TO_SNAKES:
    {
        const int pow = (mons->spell_hd(spell_cast) * 15) / 10;
        int cnt = 1 + random2(1 + pow / 4);
        monster_type sum;
        for (int i = 0; i < cnt; i++)
        {
            if (random2(mons->spell_hd(spell_cast)) > 27
                || one_chance_in(5 - min(4, div_rand_round(pow * 2, 25))))
            {
                sum = x_chance_in_y(pow / 3, 100) ? MONS_WATER_MOCCASIN
                                                  : MONS_ADDER;
            }
            else
                sum = MONS_BALL_PYTHON;

            if (create_monster(
                    mgen_data(sum, SAME_ATTITUDE(mons), mons,
                              5, spell_cast, mons->pos(), mons->foe,
                              MG_NONE, god)))
            {
                i++;
            }
        }
        return;
    }

    case SPELL_SHADOW_CREATURES:       // summon anything appropriate for level
    case SPELL_WEAVE_SHADOWS:
    {
        level_id place = (spell_cast == SPELL_SHADOW_CREATURES)
                         ? level_id::current()
                         : level_id(BRANCH_DUNGEON,
                                    min(MAX_BRANCH_DEPTH, max(1, mons->spell_hd(spell_cast))));

        sumcount2 = 1 + random2(mons->spell_hd(spell_cast) / 5 + 1);

        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            create_monster(
                mgen_data(RANDOM_MOBILE_MONSTER, SAME_ATTITUDE(mons), mons,
                          5, spell_cast, mons->pos(), mons->foe, MG_NONE, god,
                          MONS_NO_MONSTER, COLOUR_INHERIT, PROX_ANYWHERE,
                          place));
        }
        return;
    }

    case SPELL_WATER_ELEMENTALS:
        _mons_summon_elemental(mons, spell_cast, MONS_WATER_ELEMENTAL, god);
        return;

    case SPELL_EARTH_ELEMENTALS:
        _mons_summon_elemental(mons, spell_cast, MONS_EARTH_ELEMENTAL, god);
        return;

#if TAG_MAJOR_VERSION == 34
    case SPELL_IRON_ELEMENTALS:
        _mons_summon_elemental(mons, spell_cast, MONS_IRON_ELEMENTAL, god);
        return;
#endif

    case SPELL_AIR_ELEMENTALS:
        _mons_summon_elemental(mons, spell_cast, MONS_AIR_ELEMENTAL, god);
        return;

    case SPELL_FIRE_ELEMENTALS:
        _mons_summon_elemental(mons, spell_cast, MONS_FIRE_ELEMENTAL, god);
        return;

    case SPELL_SUMMON_ILLUSION:
        _mons_cast_summon_illusion(mons, spell_cast);
        return;

    case SPELL_CREATE_TENTACLES:
        mons_create_tentacles(mons);
        return;

    case SPELL_FAKE_MARA_SUMMON:
        // We only want there to be two fakes, which, plus Mara, means
        // a total of three Maras; if we already have two, give up, otherwise
        // we want to summon either one more or two more.
        sumcount2 = 2 - count_summons(mons, SPELL_FAKE_MARA_SUMMON);
        if (sumcount2 <= 0)
            return;

        for (sumcount = 0; sumcount < sumcount2; sumcount++)
            cast_phantom_mirror(mons, mons, 50, SPELL_FAKE_MARA_SUMMON);

        if (you.can_see(*mons))
        {
            mprf("%s shimmers and seems to become %s!", mons->name(DESC_THE).c_str(),
                                                        sumcount2 == 1 ? "two"
                                                                       : "three");
        }

        return;

    case SPELL_SUMMON_DEMON: // class 2-4 demons
        // if you change this, please update art-func.h:_DEMON_AXE_melee_effects
        sumcount2 = 1 + random2(mons->spell_hd(spell_cast) / 10 + 1);

        duration  = min(2 + mons->spell_hd(spell_cast) / 10, 6);
        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
            create_monster(
                mgen_data(summon_any_demon(RANDOM_DEMON_COMMON, true),
                          SAME_ATTITUDE(mons), mons, duration, spell_cast,
                          mons->pos(), mons->foe, MG_NONE, god));
        }
        return;

    case SPELL_MONSTROUS_MENAGERIE:
        cast_monstrous_menagerie(mons, splpow, mons->god);
        return;

    case SPELL_TWISTED_RESURRECTION:
        // Double efficiency compared to maxed out player spell: one
        // elf corpse gives 4.5 HD.
        twisted_resurrection(mons, 500, SAME_ATTITUDE(mons),
                             mons->foe, god);
        return;

    case SPELL_CIGOTUVIS_EMBRACE:
        harvest_corpses(*mons);
        if (crawl_state.game_is_arena() || you.can_see(*mons))
        {
            mprf("The bodies of the dead form a shell around %s.",
                 mons->name(DESC_THE).c_str());
        }
        mons->add_ench(ENCH_BONE_ARMOUR);
        return;

    case SPELL_CALL_IMP:
        duration  = min(2 + mons->spell_hd(spell_cast) / 5, 6);
        create_monster(
            mgen_data(random_choose_weighted(
                        1, MONS_IRON_IMP,
                        2, MONS_SHADOW_IMP,
                        2, MONS_WHITE_IMP,
                        4, MONS_CRIMSON_IMP,
                        0),
                      SAME_ATTITUDE(mons), mons,
                      duration, spell_cast, mons->pos(), mons->foe, MG_NONE,
                      god));
        return;

    case SPELL_SUMMON_MINOR_DEMON: // class 5 demons
        sumcount2 = 1 + random2(3);

        duration  = min(2 + mons->spell_hd(spell_cast) / 5, 6);
        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            create_monster(
                mgen_data(summon_any_demon(RANDOM_DEMON_LESSER, true),
                          SAME_ATTITUDE(mons), mons,
                          duration, spell_cast, mons->pos(), mons->foe, MG_NONE,
                          god));
        }
        return;

    case SPELL_SUMMON_UFETUBUS:
        sumcount2 = 2 + random2(2);

        duration  = min(2 + mons->spell_hd(spell_cast) / 5, 6);

        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            create_monster(
                mgen_data(MONS_UFETUBUS, SAME_ATTITUDE(mons), mons,
                          duration, spell_cast, mons->pos(), mons->foe, MG_NONE,
                          god));
        }
        return;

    case SPELL_SUMMON_HELL_BEAST:  // Geryon
        create_monster(
            mgen_data(MONS_HELL_BEAST, SAME_ATTITUDE(mons), mons,
                      4, spell_cast, mons->pos(), mons->foe, MG_NONE, god));
        return;

    case SPELL_SUMMON_ICE_BEAST:
        create_monster(
            mgen_data(MONS_ICE_BEAST, SAME_ATTITUDE(mons), mons,
                      5, spell_cast, mons->pos(), mons->foe, MG_NONE, god));
        return;

    case SPELL_SUMMON_MUSHROOMS:   // Summon a ring of icky crawling fungi.
        sumcount2 = 2 + random2(mons->spell_hd(spell_cast) / 4 + 1);
        duration  = min(2 + mons->spell_hd(spell_cast) / 5, 6);
        for (int i = 0; i < sumcount2; ++i)
        {
            // Attempt to place adjacent to target first, and only at a wider
            // radius if no adjacent spots can be found
            coord_def empty;
            find_habitable_spot_near(mons->get_foe()->pos(),
                                     MONS_WANDERING_MUSHROOM, 1, false, empty);
            if (empty.origin())
            {
                find_habitable_spot_near(mons->get_foe()->pos(),
                                         MONS_WANDERING_MUSHROOM, 2, false, empty);
            }

            // Can't find any room, so stop trying
            if (empty.origin())
                return;

            create_monster(
                mgen_data(one_chance_in(3) ? MONS_DEATHCAP
                                           : MONS_WANDERING_MUSHROOM,
                          SAME_ATTITUDE(mons), mons, duration, spell_cast,
                          empty, mons->foe, MG_FORCE_PLACE, god));
        }
        return;

    case SPELL_SUMMON_HORRIBLE_THINGS:
        _do_high_level_summon(mons, spell_cast, _pick_horrible_thing,
                              random_range(3, 5), god);
        return;

    case SPELL_MALIGN_GATEWAY:
        if (!can_cast_malign_gateway())
        {
            dprf("ERROR: %s can't cast malign gateway, but is casting anyway! "
                 "Counted %d gateways.", mons->name(DESC_THE).c_str(),
                 count_malign_gateways());
        }
        cast_malign_gateway(mons, 200);
        return;

    case SPELL_CONJURE_BALL_LIGHTNING:
    {
        const int n = min(8, 2 + random2avg(mons->spell_hd(spell_cast) / 4, 2));
        for (int i = 0; i < n; ++i)
        {
            if (monster *ball = create_monster(
                    mgen_data(MONS_BALL_LIGHTNING, SAME_ATTITUDE(mons),
                              mons, 0, spell_cast, mons->pos(), mons->foe,
                              MG_NONE, god)))
            {
                ball->add_ench(ENCH_SHORT_LIVED);
            }
        }
        return;
    }

    case SPELL_SUMMON_UNDEAD:
        _do_high_level_summon(mons, spell_cast, _pick_undead_summon,
                              2 + random2(mons->spell_hd(spell_cast) / 5 + 1),
                              god);
        return;

    case SPELL_BROTHERS_IN_ARMS:
    {
        // Invocation; don't use spell_hd
        const int power = (mons->get_hit_dice() * 20)
                          + random2(mons->get_hit_dice() * 5)
                          - random2(mons->get_hit_dice() * 5);
        monster_type to_summon;

        if (mons->type == MONS_SPRIGGAN_BERSERKER)
        {
            monster_type berserkers[] = { MONS_POLAR_BEAR, MONS_ELEPHANT,
                                          MONS_DEATH_YAK };
            to_summon = RANDOM_ELEMENT(berserkers);
        }
        else
        {
            monster_type berserkers[] = { MONS_BLACK_BEAR, MONS_OGRE, MONS_TROLL,
                                           MONS_HILL_GIANT, MONS_DEEP_TROLL,
                                           MONS_TWO_HEADED_OGRE};
            to_summon = RANDOM_ELEMENT(berserkers);
        }

        summon_berserker(power, mons, to_summon);
        mons->props["brothers_count"].get_int()++;
        return;
    }

    case SPELL_SYMBOL_OF_TORMENT:
        torment(mons, TORMENT_SPELL, mons->pos());
        return;

    case SPELL_MESMERISE:
        _mons_mesmerise(mons);
        return;

    case SPELL_CAUSE_FEAR:
        _mons_cause_fear(mons);
        return;

    case SPELL_DRAIN_LIFE:
        cast_los_attack_spell(spell_cast, splpow, mons, true);
        return;

    case SPELL_OZOCUBUS_REFRIGERATION:
        cast_los_attack_spell(spell_cast, splpow, mons, true);
        return;

    case SPELL_OLGREBS_TOXIC_RADIANCE:
        cast_toxic_radiance(mons, splpow);
        return;

    case SPELL_LRD:
    {
        if (in_bounds(pbolt.target))
            cast_fragmentation(splpow, mons, pbolt.target, false);
        else if (you.can_see(*mons))
            canned_msg(MSG_NOTHING_HAPPENS);

        return;
    }

    case SPELL_SHATTER:
        mons_shatter(mons);
        return;

    case SPELL_LEDAS_LIQUEFACTION:
        if (!mons->has_ench(ENCH_LIQUEFYING) && you.can_see(*mons))
        {
            mprf("%s liquefies the ground around %s!", mons->name(DESC_THE).c_str(),
                 mons->pronoun(PRONOUN_REFLEXIVE).c_str());
            flash_view_delay(UA_MONSTER, BROWN, 80);
        }

        mons->add_ench(ENCH_LIQUEFYING);
        invalidate_agrid(true);
        return;

    case SPELL_CORPSE_ROT:
        corpse_rot(mons);
        return;

    case SPELL_SUMMON_GREATER_DEMON:
        duration  = min(2 + mons->spell_hd(spell_cast) / 10, 6);

        create_monster(
            mgen_data(summon_any_demon(RANDOM_DEMON_GREATER, true),
                      SAME_ATTITUDE(mons), mons,
                      duration, spell_cast,
                      mons->pos(), mons->foe, MG_NONE, god));
        return;

    // Journey -- Added in Summon Lizards
    case SPELL_SUMMON_DRAKES:
        sumcount2 = 1 + random2(mons->spell_hd(spell_cast) / 5 + 1);

        duration  = min(2 + mons->spell_hd(spell_cast) / 10, 6);

        {
            vector<monster_type> monsters;

            for (sumcount = 0; sumcount < sumcount2; ++sumcount)
            {
                monster_type mon = _pick_drake();
                monsters.push_back(mon);
            }

            for (monster_type type : monsters)
            {
                create_monster(
                    mgen_data(type, SAME_ATTITUDE(mons), mons,
                              duration, spell_cast,
                              mons->pos(), mons->foe, MG_NONE, god));
            }
        }
        return;

    case SPELL_DRUIDS_CALL:
        _cast_druids_call(mons);
        return;

    case SPELL_BATTLESPHERE:
        cast_battlesphere(mons, min(splpow, 200), mons->god, false);
        return;

    case SPELL_SPECTRAL_WEAPON:
        cast_spectral_weapon(mons, min(splpow, 200), mons->god, false);
        return;

    case SPELL_TORNADO:
    {
        int dur = 60;
        if (you.can_see(*mons))
        {
            bool flying = mons->airborne();
            mprf("A great vortex of raging winds appears %s%s%s!",
                 flying ? "around " : "and lifts ",
                 mons->name(DESC_THE).c_str(),
                 flying ? "" : " up!");
        }
        else if (you.see_cell(mons->pos()))
            mpr("A great vortex of raging winds appears out of thin air!");
        mons->props["tornado_since"].get_int() = you.elapsed_time;
        mon_enchant me(ENCH_TORNADO, 0, mons, dur);
        mons->add_ench(me);
        if (mons->has_ench(ENCH_FLIGHT))
        {
            mon_enchant me2 = mons->get_ench(ENCH_FLIGHT);
            me2.duration = me.duration;
            mons->update_ench(me2);
        }
        else
            mons->add_ench(mon_enchant(ENCH_FLIGHT, 0, mons, dur));
        return;
    }

    case SPELL_SUMMON_HOLIES: // Holy monsters.
        sumcount2 = 1 + random2(2)
                      + random2(mons->spell_hd(spell_cast) / 4 + 1);

        duration  = min(2 + mons->spell_hd(spell_cast) / 5, 6);
        for (int i = 0; i < sumcount2; ++i)
        {
            create_monster(
                mgen_data(random_choose_weighted(
                            100, MONS_ANGEL,     80,  MONS_CHERUB,
                            50,  MONS_DAEVA,      1,  MONS_OPHAN,
                             0),
                          SAME_ATTITUDE(mons),
                          mons, duration, spell_cast, mons->pos(),
                          mons->foe, MG_NONE, god));
        }
        return;

    case SPELL_CANTRIP:
    {
        // Monster spell of uselessness, just prints a message.
        // This spell exists so that some monsters with really strong
        // spells (ie orc priest) can be toned down a bit. -- bwr

        // Don't give any message if the monster isn't nearby.
        // (Otherwise you could get them from halfway across the level.)
        if (!you.see_cell(mons->pos()))
            return;

        const bool friendly  = mons->friendly();
        const bool buff_only = !friendly && is_sanctuary(you.pos());
        const msg_channel_type channel = (friendly) ? MSGCH_FRIEND_ENCHANT
                                                    : MSGCH_MONSTER_ENCHANT;

        if (mons->type == MONS_GASTRONOK)
        {
            bool has_mon_foe = !invalid_monster_index(mons->foe);
            string slugform = "";
            if (buff_only || crawl_state.game_is_arena() && !has_mon_foe
                || friendly && !has_mon_foe || coinflip())
            {
                slugform = getSpeakString("gastronok_self_buff");
                if (!slugform.empty())
                {
                    slugform = replace_all(slugform, "@The_monster@",
                                           mons->name(DESC_THE));
                    mprf(channel, "%s", slugform.c_str());
                }
            }
            else if (!friendly && !has_mon_foe)
            {
                mons_cast_noise(mons, pbolt, spell_cast, slot_flags);

                // "Enchant" the player.
                slugform = getSpeakString("gastronok_debuff");
                if (!slugform.empty()
                    && (slugform.find("legs") == string::npos
                        || _legs_msg_applicable()))
                {
                    mpr(slugform);
                }
            }
            else
            {
                // "Enchant" another monster.
                slugform = getSpeakString("gastronok_other_buff");
                if (!slugform.empty())
                {
                    slugform = replace_all(slugform, "@The_monster@",
                                           foe->name(DESC_THE));
                    mprf(channel, "%s", slugform.c_str());
                }
            }
        }
        else
        {
            const char* msgs[] =
            {
                " casts a cantrip, but nothing happens.",
                " miscasts a cantrip.",
                " looks braver for a moment.",
            };

            simple_monster_message(mons, RANDOM_ELEMENT(msgs), channel);
        }
        return;
    }

    case SPELL_BLINK_OTHER:
    {
        // Allow the caster to comment on moving the foe.
        string msg = getSpeakString(mons->name(DESC_PLAIN) + " blink_other");
        if (!msg.empty() && msg != "__NONE")
        {
            mons_speaks_msg(mons, msg, MSGCH_TALK,
                            silenced(you.pos()) || silenced(mons->pos()));
        }
        break;
    }

    case SPELL_BLINK_OTHER_CLOSE:
    {
        // Allow the caster to comment on moving the foe.
        string msg = getSpeakString(mons->name(DESC_PLAIN)
                                    + " blink_other_close");
        if (!msg.empty() && msg != "__NONE")
        {
            mons_speaks_msg(mons, msg, MSGCH_TALK,
                            silenced(you.pos()) || silenced(mons->pos()));
        }
        break;
    }

    case SPELL_TOMB_OF_DOROKLOHE:
    {
        sumcount = 0;

        const int hp_lost = mons->max_hit_points - mons->hit_points;

        if (!hp_lost)
            sumcount++;

        static const set<dungeon_feature_type> safe_tiles =
        {
            DNGN_SHALLOW_WATER, DNGN_FLOOR, DNGN_OPEN_DOOR,
        };

        for (adjacent_iterator ai(mons->pos()); ai; ++ai)
        {
            const actor* act = actor_at(*ai);

            // We can blink away the crowd, but only our allies.
            if (act
                && (act->is_player()
                    || (act->is_monster()
                        && act->as_monster()->attitude != mons->attitude)))
            {
                sumcount++;
            }

            // Make sure we have a legitimate tile.
            if (!safe_tiles.count(grd(*ai)) && !feat_is_trap(grd(*ai))
                && feat_is_reachable_past(grd(*ai)))
            {
                sumcount++;
            }
        }

        if (sumcount)
        {
            mons->blink();
            return;
        }

        sumcount = 0;
        for (adjacent_iterator ai(mons->pos()); ai; ++ai)
        {
            if (monster_at(*ai))
            {
                monster_at(*ai)->blink();
                if (monster_at(*ai))
                {
                    monster_at(*ai)->teleport(true);
                    if (monster_at(*ai))
                        continue;
                }
            }

            // Make sure we have a legitimate tile.
            if (safe_tiles.count(grd(*ai)) || feat_is_trap(grd(*ai)))
            {
                // All items are moved inside.
                if (igrd(*ai) != NON_ITEM)
                    move_items(*ai, mons->pos());

                // All clouds are destroyed.
                delete_cloud(*ai);

                // All traps are destroyed.
                if (trap_def *ptrap = trap_at(*ai))
                    ptrap->destroy();

                // Actually place the wall.
                temp_change_terrain(*ai, DNGN_ROCK_WALL, INFINITE_DURATION,
                                    TERRAIN_CHANGE_TOMB, mons);
                sumcount++;
            }
        }

        if (sumcount)
        {
            mpr("Walls emerge from the floor!");

            // XXX: Assume that the entombed monster can regenerate.
            // Also, base the regeneration rate on HD to avoid
            // randomness.
            const int tomb_duration = BASELINE_DELAY
                * hp_lost * max(1, mons->spell_hd(spell_cast) / 3);
            int mon_index = mons->mindex();
            env.markers.add(new map_tomb_marker(mons->pos(),
                                                tomb_duration,
                                                mon_index,
                                                mon_index));
            env.markers.clear_need_activate(); // doesn't need activation
        }
        return;
    }

    case SPELL_CHAIN_LIGHTNING:
    case SPELL_CHAIN_OF_CHAOS:
        cast_chain_spell(spell_cast, splpow, mons);
        return;

    case SPELL_SUMMON_EYEBALLS:
        sumcount2 = 1 + random2(mons->spell_hd(spell_cast) / 7 + 1);

        duration = min(2 + mons->spell_hd(spell_cast) / 10, 6);

        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
            const monster_type mon = random_choose_weighted(
                                       100, MONS_GIANT_EYEBALL,
                                        80, MONS_EYE_OF_DRAINING,
                                        60, MONS_GOLDEN_EYE,
                                        40, MONS_SHINING_EYE,
                                        20, MONS_GREAT_ORB_OF_EYES,
                                        10, MONS_EYE_OF_DEVASTATION,
                                         0);

            create_monster(
                mgen_data(mon, SAME_ATTITUDE(mons), mons, duration,
                          spell_cast, mons->pos(), mons->foe, MG_NONE, god));
        }
        return;

    case SPELL_SUMMON_BUTTERFLIES:
        duration = min(2 + mons->spell_hd(spell_cast) / 5, 6);
        for (int i = 0; i < 10; ++i)
        {
            create_monster(
                mgen_data(MONS_BUTTERFLY, SAME_ATTITUDE(mons),
                          mons, duration, spell_cast, mons->pos(),
                          mons->foe, MG_NONE, god));
        }
        return;

    case SPELL_IOOD:
        if (mons->type == MONS_ORB_SPIDER && !_spell_charged(mons))
            return;
        if (orig_noise)
            mons_cast_noise(mons, pbolt, spell_cast, slot_flags);
        cast_iood(mons, 6 * mons->spell_hd(spell_cast), &pbolt);
        return;

    case SPELL_AWAKEN_FOREST:
        if (have_passive(passive_t::friendly_plants))
        {
            if (you.can_see(*mons))
            {
                mprf("%s commands the forest to attack, but nothing happens.",
                     mons->name(DESC_THE).c_str());
            }
            return;
        }

        duration = 50 + random2(mons->spell_hd(spell_cast) * 20);

        mons->add_ench(mon_enchant(ENCH_AWAKEN_FOREST, 0, mons, duration));
        // Actually, it's a boolean marker... save for a sanity check.
        env.forest_awoken_until = you.elapsed_time + duration;

        // You may be unable to see the monster, but notice an affected tree.
        forest_message(mons->pos(), "The forest starts to sway and rumble!");
        return;

    case SPELL_SUMMON_DRAGON:
        cast_summon_dragon(mons, splpow, god);
        return;

    case SPELL_SUMMON_HYDRA:
        cast_summon_hydra(mons, splpow, god);
        return;

    case SPELL_FIRE_SUMMON:
        sumcount2 = 1 + random2(mons->spell_hd(spell_cast) / 5 + 1);

        duration = min(2 + mons->spell_hd(spell_cast) / 10, 6);

        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
            const monster_type mon = random_choose_weighted(
                                       3, MONS_EFREET,
                                       3, MONS_SUN_DEMON,
                                       3, MONS_BALRUG,
                                       2, MONS_HELLION,
                                       1, MONS_BRIMSTONE_FIEND,
                                       0);

            create_monster(
                mgen_data(mon, SAME_ATTITUDE(mons), mons, duration,
                          spell_cast, mons->pos(), mons->foe, MG_NONE, god));
        }
        return;

    case SPELL_DEATHS_DOOR:
        if (!mons->has_ench(ENCH_DEATHS_DOOR))
        {
            const int dur = BASELINE_DELAY * 2 * mons->skill(SK_NECROMANCY);
            simple_monster_message(mons,
                                   " stands defiantly in death's doorway!");
            mons->hit_points = max(min(mons->hit_points,
                                       mons->skill(SK_NECROMANCY)), 1);
            mons->add_ench(mon_enchant(ENCH_DEATHS_DOOR, 0, mons, dur));
        }
        return;

    case SPELL_REGENERATION:
    {
        simple_monster_message(mons,
                               "'s wounds begin to heal before your eyes!");
        const int dur = BASELINE_DELAY
            * min(5 + roll_dice(2, (mons->spell_hd(spell_cast) * 10) / 3 + 1), 100);
        mons->add_ench(mon_enchant(ENCH_REGENERATION, 0, mons, dur));
        return;
    }

    case SPELL_OZOCUBUS_ARMOUR:
    {
        if (you.can_see(*mons))
        {
            mprf("A film of ice covers %s body!",
                 apostrophise(mons->name(DESC_THE)).c_str());
        }
        const int power = (mons->spell_hd(spell_cast) * 15) / 10;
        mons->add_ench(mon_enchant(ENCH_OZOCUBUS_ARMOUR,
                                   20 + random2(power) + random2(power),
                                   mons));

        return;
    }

    case SPELL_WORD_OF_RECALL:
    {
        mon_enchant chant_timer = mon_enchant(ENCH_WORD_OF_RECALL, 1, mons, 30);
        mons->add_ench(chant_timer);
        mons->speed_increment -= 30;
        return;
    }

    case SPELL_INJURY_BOND:
    {
        simple_monster_message(mons,
            make_stringf(" begins to accept %s allies' injuries.",
                         mons->pronoun(PRONOUN_POSSESSIVE).c_str()).c_str());
        // FIXME: allies preservers vs the player
        for (monster_near_iterator mi(mons, LOS_NO_TRANS); mi; ++mi)
        {
            if (mons_aligned(mons, *mi) && !mi->has_ench(ENCH_CHARM)
                && !mi->has_ench(ENCH_HEXED) && *mi != mons)
            {
                mon_enchant bond = mon_enchant(ENCH_INJURY_BOND, 1, mons,
                                               40 + random2(80));
                mi->add_ench(bond);
            }
        }

        return;
    }

    case SPELL_CALL_LOST_SOUL:
        create_monster(mgen_data(MONS_LOST_SOUL, SAME_ATTITUDE(mons),
                                 mons, 2, spell_cast, mons->pos(),
                                 mons->foe, MG_NONE, god));
        return;

    case SPELL_BLINK_ALLIES_ENCIRCLE:
        _blink_allies_encircle(mons);
        return;

    case SPELL_MASS_CONFUSION:
        _mons_mass_confuse(mons);
        return;

    case SPELL_ENGLACIATION:
        if (you.can_see(*mons))
            simple_monster_message(mons, " radiates an aura of cold.");
        else if (mons->see_cell_no_trans(you.pos()))
            mpr("A wave of cold passes over you.");
        apply_area_visible([splpow, mons] (coord_def where) {
            return englaciate(where, min(splpow, 200), mons);
        }, mons->pos());
        return;

    case SPELL_AWAKEN_VINES:
        _awaken_vines(mons);
        return;

    case SPELL_CONTROL_WINDS:
        if (you.can_see(*mons))
            mprf("The winds start to flow at %s will.", mons->name(DESC_ITS).c_str());
        mons->add_ench(mon_enchant(ENCH_CONTROL_WINDS, 1, mons, 200 + random2(150)));
        return;

    case SPELL_WALL_OF_BRAMBLES:
        // If we can't cast this for some reason (can be expensive to determine
        // at every call to _ms_waste_of_time), refund the energy for it so that
        // the caster can do something else
        if (!_wall_of_brambles(mons))
        {
            mons->speed_increment +=
                get_monster_data(mons->type)->energy_usage.spell;
        }
        return;

    case SPELL_WIND_BLAST:
    {
        // Wind blast is stopped by FFT_SOLID features.
        if (foe && cell_see_cell(mons->pos(), foe->pos(), LOS_SOLID))
            wind_blast(mons, splpow, foe->pos());
        return;
    }

    case SPELL_FREEZE:
        _mons_cast_freeze(mons);
        return;

    case SPELL_SUMMON_VERMIN:
        _do_high_level_summon(mons, spell_cast, _pick_vermin,
                              one_chance_in(4) ? 3 : 2 , god);
        return;

    case SPELL_DISCHARGE:
    {
        const int power = min(200, splpow);
        const int num_targs = 1 + random2(random_range(1, 3) + power / 20);
        const int dam =
            apply_random_around_square([power, mons] (coord_def where) {
                return discharge_monsters(where, power, mons);
            }, mons->pos(), true, num_targs);

        if (dam > 0)
            scaled_delay(100);
        else
        {
            if (!you.can_see(*mons))
                mpr("You hear crackling.");
            else if (coinflip())
            {
                mprf("The air around %s crackles with electrical energy.",
                     mons->name(DESC_THE).c_str());
            }
            else
            {
                const bool plural = coinflip();
                mprf("%s blue arc%s ground%s harmlessly %s %s.",
                     plural ? "Some" : "A",
                     plural ? "s" : "",
                     plural ? " themselves" : "s itself",
                     plural ? "around" : (coinflip() ? "beside" :
                                          coinflip() ? "behind" : "before"),
                     mons->name(DESC_THE).c_str());
            }
        }
        return;
    }

    case SPELL_PORTAL_PROJECTILE:
    {
        // Swap weapons if necessary so that that happens before the spell
        // casting message.
        item_def *launcher = nullptr;
        mons_usable_missile(mons, &launcher);
        const item_def *weapon = mons->mslot_item(MSLOT_WEAPON);
        if (launcher && launcher != weapon)
            mons->swap_weapons();
        mons_cast_noise(mons, pbolt, spell_cast, slot_flags);
        handle_throw(mons, pbolt, true, false);
        return;
    }

    case SPELL_IGNITE_POISON:
        cast_ignite_poison(mons, splpow, false);
        return;

    case SPELL_LEGENDARY_DESTRUCTION:
    {
        if (pbolt.origin_spell == SPELL_IOOD)
        {
            cast_iood(mons, _mons_spellpower(SPELL_IOOD, *mons), &pbolt);
            return;
        }
        // Don't return yet, we want to actually fire the random beam later
        break;
    }

    case SPELL_FORCEFUL_INVITATION:
        _branch_summon_helper(mons, spell_cast, _invitation_summons,
                              ARRAYSZ(_invitation_summons), 1 + random2(3));
        return;

    case SPELL_PLANEREND:
        _branch_summon_helper(mons, spell_cast, _planerend_summons,
                              ARRAYSZ(_planerend_summons), 1 + random2(3));
        return;

    case SPELL_BLACK_MARK:
        _cast_black_mark(mons);
        return;

#if TAG_MAJOR_VERSION == 34
    case SPELL_REARRANGE_PIECES:
    {
        bool did_message = false;
        vector<actor* > victims;
        for (actor_near_iterator ai(mons, LOS_NO_TRANS); ai; ++ai)
            victims.push_back(*ai);
        shuffle_array(victims);
        for (auto it = victims.begin(); it != victims.end(); it++)
        {
            actor* victim1 = *it;
            it++;
            if (it == victims.end())
                break;
            actor* victim2 = *it;
            if (victim1->is_player())
                swap_with_monster(victim2->as_monster());
            else if (victim2->is_player())
                swap_with_monster(victim1->as_monster());
            else
            {
                if (!did_message
                    && (you.can_see(*victim1)
                        || you.can_see(*victim2)))
                {
                    mpr("Some monsters swap places.");
                    did_message = true;
                }

                swap_monsters(victim1->as_monster(), victim2->as_monster());
            }
        }
        return;
    }
#endif

    case SPELL_BLINK_ALLIES_AWAY:
        _blink_allies_away(mons);
        return;

    case SPELL_SHROUD_OF_GOLUBRIA:
        if (you.can_see(*mons))
        {
            mprf("Space distorts along a thin shroud covering %s %s.",
                 apostrophise(mons->name(DESC_THE)).c_str(),
                 mons->is_insubstantial() ? "form" : "body");
        }
        mons->add_ench(mon_enchant(ENCH_SHROUD));
        return;

    case SPELL_DAZZLING_SPRAY:
    {
        vector<bolt> beams = get_spray_rays(mons, pbolt.target, pbolt.range, 3,
                                            ZAP_DAZZLING_SPRAY);
        for (bolt &child : beams)
        {
            bolt_parent_init(pbolt, child);
            child.fire();
        }
        return;
    }

    case SPELL_GLACIATE:
    {
        ASSERT(foe);
        cast_glaciate(mons, splpow, foe->pos());
        return;
    }

    case SPELL_CLOUD_CONE:
    {
        ASSERT(mons->get_foe());
        cast_cloud_cone(mons, splpow, mons->get_foe()->pos());
        return;
    }

    case SPELL_PHANTOM_MIRROR:
    {
        // Find appropriate ally to clone.
        vector<monster*> targets;
        for (monster_near_iterator mi(mons); mi; ++mi)
        {
            if (_mirrorable(mons, *mi))
                targets.push_back(*mi);
        }

        // If we've found something, mirror it.
        if (targets.size())
        {
            monster* targ = targets[random2(targets.size())];
            if (cast_phantom_mirror(mons, targ))
                simple_monster_message(targ, " shimmers and seems to become two!");
        }
        return;
    }

    case SPELL_SUMMON_MANA_VIPER:
        sumcount2 = 1 + random2(mons->spell_hd(spell_cast) / 5 + 1);

        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
        create_monster(mgen_data(MONS_MANA_VIPER, SAME_ATTITUDE(mons),
                                 mons, 2, spell_cast, mons->pos(),
                                 mons->foe, MG_NONE, god));
        }
        return;

    case SPELL_SUMMON_EMPEROR_SCORPIONS:
        sumcount2 = 1 + random2(mons->spell_hd(spell_cast) / 5 + 1);

        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
            create_monster(mgen_data(MONS_EMPEROR_SCORPION, SAME_ATTITUDE(mons),
                                     mons, 5, spell_cast, mons->pos(),
                                     mons->foe, MG_NONE, god));
        }
        return;

    case SPELL_BATTLECRY:
        _battle_cry(*mons);
        return;

    case SPELL_WARNING_CRY:
        return; // the entire point is the noise, handled elsewhere

    case SPELL_SEAL_DOORS:
        _seal_doors_and_stairs(mons);
        return;

    case SPELL_BERSERK_OTHER:
        _incite_monsters(mons, true);
        return;

    case SPELL_SPELLFORGED_SERVITOR:
    {
        monster* servitor = create_monster(
            mgen_data(MONS_SPELLFORGED_SERVITOR, SAME_ATTITUDE(mons),
                      mons, 4, spell_cast, mons->pos(), mons->foe, MG_NONE, god));
        if (servitor)
            init_servitor(servitor, mons);
        else if (you.can_see(*mons))
            canned_msg(MSG_NOTHING_HAPPENS);
        return;
    }

    case SPELL_CORRUPTING_PULSE:
        _corrupting_pulse(mons);
        return;

    case SPELL_THROW:
        _mons_consider_throwing(*mons);
        return;

    case SPELL_THROW_ALLY:
        _maybe_throw_ally(*mons);
        return;

    case SPELL_SIREN_SONG:
        _siren_sing(mons, false);
        return;

    case SPELL_AVATAR_SONG:
        _siren_sing(mons, true);
        return;

    case SPELL_REPEL_MISSILES:
        simple_monster_message(mons, " begins repelling missiles!");
        mons->add_ench(mon_enchant(ENCH_REPEL_MISSILES));
        return;

    case SPELL_DEFLECT_MISSILES:
        simple_monster_message(mons, " begins deflecting missiles!");
        mons->add_ench(mon_enchant(ENCH_DEFLECT_MISSILES));
        return;

    case SPELL_SUMMON_SCARABS:
        sumcount2 = 1 + random2(mons->spell_hd(spell_cast) / 5 + 1);

        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
            create_monster(mgen_data(MONS_DEATH_SCARAB, SAME_ATTITUDE(mons),
                                     mons, 2, spell_cast, mons->pos(),
                                     mons->foe, MG_NONE, god));
        }
        return;

    case SPELL_SCATTERSHOT:
        ASSERT(foe);
        cast_scattershot(mons, splpow, foe->pos());
        return;

    case SPELL_CONJURE_FLAME:
        if (in_bounds(pbolt.target))
        {
            if (conjure_flame(mons, splpow, pbolt.target, false)
                != SPRET_SUCCESS)
            {
                canned_msg(MSG_NOTHING_HAPPENS);
            }
        }
        else if (you.can_see(*mons))
            canned_msg(MSG_NOTHING_HAPPENS);

        return;

    case SPELL_CONTROL_UNDEAD:
        _mons_control_undead(mons);
        return;

    case SPELL_FULMINANT_PRISM:
        if (in_bounds(pbolt.target))
           cast_fulminating_prism(mons, splpow, pbolt.target, false);
        else if (you.can_see(*mons))
            canned_msg(MSG_NOTHING_HAPPENS);

        return;

    case SPELL_CLEANSING_FLAME:
        simple_monster_message(mons, " channels a blast of cleansing flame!");
        cleansing_flame(5 + (7 * mons->spell_hd(spell_cast) / 12),
                        CLEANSING_FLAME_SPELL, mons->pos(), mons);
        return;

    case SPELL_GRAVITAS:
        fatal_attraction(foe->pos(), mons, splpow);
        return;

    case SPELL_ENTROPIC_WEAVE:
        foe->corrode_equipment("the entropic weave");
        return;

    case SPELL_FIRE_STORM:
        if (mons->type == MONS_SALAMANDER_STORMCALLER && !_spell_charged(mons))
            return;
        if (orig_noise)
            mons_cast_noise(mons, pbolt, spell_cast, slot_flags);
        break;

    case SPELL_SUMMON_EXECUTIONERS:
        sumcount2 = 1 + random2(mons->spell_hd(spell_cast) / 5 + 1);

        duration  = min(2 + mons->spell_hd(spell_cast) / 10, 6);

        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            create_monster(
                mgen_data(MONS_EXECUTIONER, SAME_ATTITUDE(mons), mons,
                          duration, spell_cast, mons->pos(), mons->foe, MG_NONE,
                          god));
        }
        return;

    case SPELL_DOOM_HOWL:
        _doom_howl(*mons);
        break;

    case SPELL_AWAKEN_EARTH:
        _mons_awaken_earth(*mons, pbolt.target);
        return;

    case SPELL_CALL_OF_CHAOS:
        _mons_call_of_chaos(*mons);
        return;

    case SPELL_AURA_OF_BRILLIANCE:
        simple_monster_message(mons, " begins emitting a brilliant aura!");
        mons->add_ench(ENCH_BRILLIANCE_AURA);
        aura_of_brilliance(mons);
        return;
    }

    // If a monster just came into view and immediately cast a spell,
    // we need to refresh the screen before drawing the beam.
    viewwindow();
    if (spell_is_direct_explosion(spell_cast))
    {
        const bool need_more = foe && (foe->is_player() || you.see_cell(foe->pos()));
        pbolt.in_explosion_phase = false;
        pbolt.refine_for_explosion();
        pbolt.explode(need_more);
    }
    else
        pbolt.fire();
}

static int _noise_level(const monster* mons, spell_type spell,
                        bool silent, mon_spell_slot_flags slot_flags)
{
    const unsigned int flags = get_spell_flags(spell);

    int noise;

    if (silent
        || (slot_flags & MON_SPELL_INNATE_MASK
            && !(slot_flags & MON_SPELL_NOISY)
            && !(flags & SPFLAG_NOISY)))
    {
        noise = 0;
    }
    else if (mons_genus(mons->type) == MONS_DRAGON)
        noise = get_shout_noise_level(S_ROAR);
    else
        noise = spell_noise(spell);

    return noise;
}

static void _speech_keys(vector<string>& key_list,
                         const monster* mons, const bolt& pbolt,
                         spell_type spell, mon_spell_slot_flags slot_flags,
                         bool targeted)
{
    const string cast_str = " cast";

    // Can't use copy-initialization 'wizard = slot_flags & ...' here,
    // because the bitfield-to-bool conversion is not implicit.
    const bool wizard  {slot_flags & MON_SPELL_WIZARD};
    const bool priest  {slot_flags & MON_SPELL_PRIEST};
    const bool natural {slot_flags & MON_SPELL_NATURAL};
    const bool magical {slot_flags & MON_SPELL_MAGICAL};

    const mon_body_shape shape = get_mon_shape(mons);
    const string    spell_name = spell_title(spell);
    const bool      real_spell = priest || wizard;

    // Before just using generic per-spell and per-monster casts, try
    // per-monster, per-spell, with the monster type name, then the
    // species name, then the genus name, then wizard/priest/magical/natural.
    // We don't include "real" or "gestures" here since that can be
    // be determined from the monster type; or "targeted" since that
    // can be determined from the spell.
    key_list.push_back(spell_name + " "
                       + mons_type_name(mons->type, DESC_PLAIN) + cast_str);
    key_list.push_back(spell_name + " "
                       + mons_type_name(mons_species(mons->type), DESC_PLAIN)
                       + cast_str);
    key_list.push_back(spell_name + " "
                       + mons_type_name(mons_genus(mons->type), DESC_PLAIN)
                       + cast_str);
    if (wizard)
    {
        key_list.push_back(make_stringf("%s %swizard%s",
                               spell_name.c_str(),
                               mon_shape_is_humanoid(shape) ? ""
                                                            : "non-humanoid ",
                               cast_str.c_str()));
    }
    else if (priest)
        key_list.push_back(spell_name + " priest" + cast_str);
    else if (magical)
        key_list.push_back(spell_name + " magical" + cast_str);
    else if (natural)
        key_list.push_back(spell_name + " natural" + cast_str);


    // Now try just the spell's name.
    if (mon_shape_is_humanoid(shape))
    {
        if (real_spell)
            key_list.push_back(spell_name + cast_str + " real");
        if (mons_intel(mons) >= I_HUMAN)
            key_list.push_back(spell_name + cast_str + " gestures");
    }

    key_list.push_back(spell_name + cast_str);

    // Only postfix "targeted" after this point.
    const unsigned int num_spell_keys = key_list.size();

    // Next the monster type name, then species name, then genus name.
    key_list.push_back(mons_type_name(mons->type, DESC_PLAIN) + cast_str);
    key_list.push_back(mons_type_name(mons_species(mons->type), DESC_PLAIN)
                       + cast_str);
    key_list.push_back(mons_type_name(mons_genus(mons->type), DESC_PLAIN)
                       + cast_str);

    // Last, generic wizard, priest or magical.
    if (wizard)
    {
        key_list.push_back(make_stringf("%swizard%s",
                               mon_shape_is_humanoid(shape) ? ""
                                                            : "non-humanoid ",
                               cast_str.c_str()));
    }
    else if (priest)
        key_list.push_back("priest" + cast_str);
    else if (magical)
        key_list.push_back("magical" + cast_str);

    if (targeted)
    {
        // For targeted spells, try with the targeted suffix first.
        for (unsigned int i = key_list.size() - 1; i >= num_spell_keys; i--)
        {
            string str = key_list[i] + " targeted";
            key_list.insert(key_list.begin() + i, str);
        }

        // Generic beam messages.
        if (pbolt.visible())
        {
            key_list.push_back(pbolt.get_short_name() + " beam " + cast_str);
            key_list.emplace_back("beam catchall cast");
        }
    }
}

static string _speech_message(const vector<string>& key_list,
                              bool silent, bool unseen)
{
    string prefix;
    if (silent)
        prefix = "silent ";
    else if (unseen)
        prefix = "unseen ";

    string msg;
    for (const string &key : key_list)
    {
#ifdef DEBUG_MONSPEAK
        dprf(DIAG_SPEECH, "monster casting lookup: %s%s",
             prefix.c_str(), key.c_str());
#endif

        msg = getSpeakString(prefix + key);
        if (msg == "__NONE")
        {
            msg = "";
            break;
        }
        else if (!msg.empty())
            break;

        // If we got no message and we're using the silent prefix, then
        // try again without the prefix.
        if (prefix != "silent ")
            continue;

        msg = getSpeakString(key);
        if (msg == "__NONE")
        {
            msg = "";
            break;
        }
        else if (!msg.empty())
            break;
    }

    return msg;
}

static void _speech_fill_target(string& targ_prep, string& target,
                                const monster* mons, const bolt& pbolt,
                                bool gestured)
{
    targ_prep = "at";
    target    = "nothing";

    bolt tracer = pbolt;
    // For a targeted but rangeless spell make the range positive so that
    // fire_tracer() will fill out path_taken.
    if (pbolt.range == 0 && pbolt.target != mons->pos())
        tracer.range = ENV_SHOW_DIAMETER;
    fire_tracer(mons, tracer);

    if (pbolt.target == you.pos())
        target = "you";
    else if (pbolt.target == mons->pos())
        target = mons->pronoun(PRONOUN_REFLEXIVE);
    // Monsters should only use targeted spells while foe == MHITNOT
    // if they're targeting themselves.
    else if (mons->foe == MHITNOT && !mons_is_confused(mons, true))
        target = "NONEXISTENT FOE";
    else if (!invalid_monster_index(mons->foe)
             && menv[mons->foe].type == MONS_NO_MONSTER)
    {
        target = "DEAD FOE";
    }
    else if (in_bounds(pbolt.target) && you.see_cell(pbolt.target))
    {
        if (const monster* mtarg = monster_at(pbolt.target))
        {
            if (you.can_see(*mtarg))
                target = mtarg->name(DESC_THE);
        }
    }

    const bool visible_path      = pbolt.visible() || gestured;

    // Monster might be aiming past the real target, or maybe some fuzz has
    // been applied because the target is invisible.
    if (target == "nothing")
    {
        if (pbolt.aimed_at_spot || pbolt.origin_spell == SPELL_DIG)
        {
            int count = 0;
            for (adjacent_iterator ai(pbolt.target); ai; ++ai)
            {
                const actor* act = actor_at(*ai);
                if (act && act != mons && you.can_see(*act))
                {
                    targ_prep = "next to";

                    if (act->is_player() || one_chance_in(++count))
                        target = act->name(DESC_THE);

                    if (act->is_player())
                        break;
                }
            }

            if (targ_prep == "at")
            {
                if (grd(pbolt.target) != DNGN_FLOOR)
                {
                    target = feature_description(grd(pbolt.target),
                                                 NUM_TRAPS, "", DESC_THE,
                                                 false);
                }
                else
                    target = "thin air";
            }

            return;
        }

        bool mons_targ_aligned = false;

        for (const coord_def pos : tracer.path_taken)
        {
            if (pos == mons->pos())
                continue;

            const monster* m = monster_at(pos);
            if (pos == you.pos())
            {
                // Be egotistical and assume that the monster is aiming at
                // the player, rather than the player being in the path of
                // a beam aimed at an ally.
                if (!mons->wont_attack())
                {
                    targ_prep = "at";
                    target    = "you";
                    break;
                }
                // If the ally is confused or aiming at an invisible enemy,
                // with the player in the path, act like it's targeted at
                // the player if there isn't any visible target earlier
                // in the path.
                else if (target == "nothing")
                {
                    targ_prep         = "at";
                    target            = "you";
                    mons_targ_aligned = true;
                }
            }
            else if (visible_path && m && you.can_see(*m))
            {
                bool is_aligned  = mons_aligned(m, mons);
                string name = m->name(DESC_THE);

                if (target == "nothing")
                {
                    mons_targ_aligned = is_aligned;
                    target            = name;
                }
                // If the first target was aligned with the beam source then
                // the first subsequent non-aligned monster in the path will
                // take it's place.
                else if (mons_targ_aligned && !is_aligned)
                {
                    mons_targ_aligned = false;
                    target            = name;
                }
                targ_prep = "at";
            }
            else if (visible_path && target == "nothing")
            {
                int count = 0;
                for (adjacent_iterator ai(pbolt.target); ai; ++ai)
                {
                    const actor* act = monster_at(*ai);
                    if (act && act != mons && you.can_see(*act))
                    {
                        targ_prep = "past";
                        if (act->is_player()
                            || one_chance_in(++count))
                        {
                            target = act->name(DESC_THE);
                        }

                        if (act->is_player())
                            break;
                    }
                }
            }
        } // for (const coord_def pos : path)
    } // if (target == "nothing" && targeted)

    const actor* foe = mons->get_foe();

    // If we still can't find what appears to be the target, and the
    // monster isn't just throwing the spell in a random direction,
    // we should be able to tell what the monster was aiming for if
    // we can see the monster's foe and the beam (or the beam path
    // implied by gesturing). But only if the beam didn't actually hit
    // anything (but if it did hit something, why didn't that monster
    // show up in the beam's path?)
    if (target == "nothing"
        && (tracer.foe_info.count + tracer.friend_info.count) == 0
        && foe != nullptr
        && you.can_see(*foe)
        && !mons->confused()
        && visible_path)
    {
        target = foe->name(DESC_THE);
        targ_prep = (pbolt.aimed_at_spot ? "next to" : "past");
    }

    // If the monster gestures to create an invisible beam then
    // assume that anything close to the beam is the intended target.
    // Also, if the monster gestures to create a visible beam but it
    // misses still say that the monster gestured "at" the target,
    // rather than "past".
    if (gestured || target == "nothing")
        targ_prep = "at";

    // "throws whatever at something" is better than "at nothing"
    if (target == "nothing")
        target = "something";
}

void mons_cast_noise(monster* mons, const bolt &pbolt,
                     spell_type spell_cast, mon_spell_slot_flags slot_flags)
{
    bool force_silent = false;
    noise_flag_type noise_flags = NF_NONE;

    if (mons->type == MONS_SHADOW_DRAGON)
        // Draining breath is silent.
        force_silent = true;

    const bool unseen    = !you.can_see(*mons);
    const bool silent    = silenced(mons->pos()) || force_silent;

    if (unseen && silent)
        return;

    int noise = _noise_level(mons, spell_cast, silent, slot_flags);

    const unsigned int spell_flags = get_spell_flags(spell_cast);
    const bool targeted = (spell_flags & SPFLAG_TARGETING_MASK)
                           && (pbolt.target != mons->pos()
                               || pbolt.visible());

    vector<string> key_list;
    _speech_keys(key_list, mons, pbolt, spell_cast, slot_flags, targeted);

    string msg = _speech_message(key_list, silent, unseen);

    if (msg.empty())
    {
        if (silent)
            return;

        noisy(noise, mons->pos(), mons->mid, noise_flags);
        return;
    }

    // FIXME: we should not need to look at the message text.
    const bool gestured = msg.find("Gesture") != string::npos
                          || msg.find(" gesture") != string::npos
                          || msg.find("Point") != string::npos
                          || msg.find(" point") != string::npos;

    string targ_prep = "at";
    string target    = "NO_TARGET";

    if (targeted)
        _speech_fill_target(targ_prep, target, mons, pbolt, gestured);

    msg = replace_all(msg, "@at@",     targ_prep);
    msg = replace_all(msg, "@target@", target);

    string beam_name;
    if (!targeted)
        beam_name = "NON TARGETED BEAM";
    else if (pbolt.name.empty())
        beam_name = "INVALID BEAM";
    else
        beam_name = pbolt.get_short_name();

    msg = replace_all(msg, "@beam@", beam_name);

    const msg_channel_type chan =
        (unseen              ? MSGCH_SOUND :
         mons->friendly()    ? MSGCH_FRIEND_SPELL
                             : MSGCH_MONSTER_SPELL);

    if (silent || noise == 0)
        mons_speaks_msg(mons, msg, chan, true);
    else if (noisy(noise, mons->pos(), mons->mid, noise_flags) || !unseen)
    {
        // noisy() returns true if the player heard the noise.
        mons_speaks_msg(mons, msg, chan);
    }
}

static const int MIN_THROW_DIST = 2;

static bool _valid_throw_dest(const actor &thrower, const actor &victim,
                              const coord_def pos)
{
    return thrower.pos().distance_from(pos) >= MIN_THROW_DIST
           && !actor_at(pos)
           && victim.is_habitable(pos)
           && thrower.see_cell(pos);
}

/**
 * Choose a landing site for a monster that is throwing someone.
 *
 * @param thrower      The monster performing the toss.
 * @param victim       The actor being thrown.
 * @param rater        A function that takes thrower, victim, and an arbitrary
 *                     coord_def and determines how good the throw is; a higher
 *                     number is better.
 * @return             The coord_def of one of the best (as determined by rater)
 *                     possible landing sites for a toss.
 *                     If no valid site is found, returns the origin (0,0).
 */
static coord_def _choose_throwing_target(const monster &thrower,
                            const actor &victim,
                            function<int (const monster&, const actor&,
                                          coord_def)> rater)
{
    int best_site_score = -1;
    vector<coord_def> best_sites;

    for (distance_iterator di(thrower.pos(), true, true, LOS_RADIUS); di; ++di)
    {
        ray_def ray;
        // Unusable landing sites.
        if (!_valid_throw_dest(thrower, victim, *di)
            || !find_ray(thrower.pos(), *di, ray, opc_solid_see))
        {
            continue;
        }

        const int site_score = rater(thrower, victim, *di);
        if (site_score > best_site_score)
        {
            best_site_score = site_score;
            best_sites.clear();
        }
        if (site_score == best_site_score)
            best_sites.push_back(*di);
    }

    // No valid landing site found.
    if (!best_sites.size())
        return coord_def(0,0);

    const coord_def best_site = best_sites[random2(best_sites.size())];
    return best_site;
}

static bool _will_throw_ally(const monster& thrower, const monster& throwee)
{
    return thrower.type == MONS_ROBIN && throwee.mons_species() == MONS_GOBLIN
           || thrower.type == MONS_POLYPHEMUS
              && mons_genus(throwee.type) == MONS_YAK;
}

static monster* _find_ally_to_throw(const monster &mons)
{
    const actor *foe = mons.get_foe();
    if (!foe)
        return nullptr;

    int furthest_dist = -1;

    monster* best = nullptr;
    for (fair_adjacent_iterator ai(mons.pos(), true); ai; ++ai)
    {
        monster* throwee = monster_at(*ai);

        if (!throwee || !throwee->alive()
            || !_will_throw_ally(mons, *throwee))
        {
            continue;
        }

        // Don't try to throw anything constricted.
        if (throwee->is_constricted())
            continue;

        // otherwise throw whoever's furthest from our target.
        const int dist = grid_distance(throwee->pos(), foe->pos());
        if (dist > furthest_dist)
        {
            best = throwee;
            furthest_dist = dist;
        }
    }

    dprf("found a monster to toss");
    return best;
}

/**
 * Toss an ally at the monster's foe, landing them in the given square after
 * maybe dealing a pittance of damage.
 *
 * XXX: some duplication with tentacle toss code
 *
 * @param thrower       The monster doing the throwing.
 * @param throwee       The monster being tossed.
 * @param chosen_dest   The location of the square throwee should land on.
 */
static void _throw_ally_to(const monster &thrower, monster &throwee,
                           const coord_def chosen_dest)
{
    ASSERT_IN_BOUNDS(chosen_dest);
    ASSERT(!throwee.is_constricted());

    actor* foe = thrower.get_foe();
    ASSERT(foe);

    const coord_def old_pos = throwee.pos();
    const bool thrower_seen = you.can_see(thrower);
    const bool throwee_was_seen = you.can_see(throwee);
    const bool throwee_will_be_seen = throwee.visible_to(&you)
                                      && you.see_cell(chosen_dest);
    const bool throwee_seen = throwee_was_seen || throwee_will_be_seen;

    if (!(throwee.flags & MF_WAS_IN_VIEW))
        throwee.seen_context = SC_THROWN_IN;

    if (thrower_seen || throwee_seen)
    {
        const string destination = you.can_see(*foe) ?
                                   make_stringf("at %s",
                                                foe->name(DESC_THE).c_str()) :
                                   "out of sight";

        mprf("%s throws %s %s!",
             (thrower_seen ? thrower.name(DESC_THE).c_str() : "Something"),
             (throwee_seen ? throwee.name(DESC_THE, true).c_str() : "something"),
             destination.c_str());

        bolt beam;
        beam.range   = INFINITE_DISTANCE;
        beam.hit     = AUTOMATIC_HIT;
        beam.flavour = BEAM_VISUAL;
        beam.source  = thrower.pos();
        beam.target  = chosen_dest;
        beam.glyph   = mons_char(throwee.type);
        const monster_info mi(&throwee);
        beam.colour  = mi.colour();

        beam.draw_delay = 30; // Make beam animation somewhat slower than normal.
        beam.aimed_at_spot = true;
        beam.fire();
    }

    throwee.move_to_pos(chosen_dest);
    throwee.apply_location_effects(old_pos);
    throwee.check_redraw(old_pos);

    const string killed_by = make_stringf("Hit by %s thrown by %s",
                                          throwee.name(DESC_A, true).c_str(),
                                          thrower.name(DESC_PLAIN, true).c_str());
    const int dam = foe->apply_ac(random2(thrower.get_hit_dice() * 2));
    foe->hurt(&thrower, dam, BEAM_NONE, KILLED_BY_BEAM, "", killed_by, true);

    // wake sleepy goblins
    behaviour_event(&throwee, ME_DISTURB, &thrower, throwee.pos());
}

static int _throw_ally_site_score(const monster& thrower, const actor& throwee,
                                  coord_def pos)
{
    const actor *foe = thrower.get_foe();
    if (!foe || !adjacent(foe->pos(), pos))
        return -2;
    return grid_distance(thrower.pos(), pos);
}

static void _maybe_throw_ally(const monster &mons)
{
    monster* throwee = _find_ally_to_throw(mons);
    if (!throwee)
        return;

    const coord_def toss_target =
        _choose_throwing_target(mons, *static_cast<actor*>(throwee),
                                _throw_ally_site_score);

    if (toss_target.origin())
        return;

    _throw_ally_to(mons, *throwee, toss_target);
}

/**
 * Find the best creature for the given monster to toss.
 *
 * @param mons      The monster thinking about throwing.
 * @return          The victim for the monster to throw. Can be null.
 */
static actor *_get_throw_victim(const monster &mons)
{
    // Always throw the player first, if possible.
    if (!mons.wont_attack() && adjacent(you.pos(), mons.pos())
        && !(mons.is_constricted() && mons.constricted_by == MID_PLAYER)
        && mons.can_constrict(&you))
    {
        return &you;
    }

    for (fair_adjacent_iterator ai(mons.pos()); ai; ++ai)
    {
        actor* victim = actor_at(*ai);
        // Only throw real, living victims.
        if (!victim || !victim->alive())
            continue;

        // Don't throw allies.
        if (mons_aligned(&mons, victim))
            continue;

        // Don't try to throw anything constricting the thrower.
        if (mons.is_constricted() && mons.constricted_by == victim->mid)
            continue;

        // Don't throw statues or tentacles.
        if (mons_class_is_stationary(victim->type))
            continue;

        // See if we *could* execute a grab attack, and if so, they're
        // a valid target.
        if (mons.can_constrict(victim))
            return victim;
    }

    // Nope.
    return nullptr;
}

/**
 * Make the given monster try to throw a constricted victim, if possible.
 *
 * @param mons       The monster doing the throwing.
 * @return           Whether a throw attempt was made.
 */
static bool _mons_consider_throwing(const monster &mons)
{
    actor *victim = _get_throw_victim(mons);
    if (!victim)
        return false;

    return _throw(mons, *victim, mons.get_hit_dice() * 4);
}

/**
 * Find the actual landing place for a throw.
 *
 * @param thrower       The monster performing the toss.
 * @param victim        The actor being thrown.
 * @param target_site   The intended destination.
 * @return              The actual destination, somewhere along a ray between
 *                      the victim's initial position and the intended
 *                      destination.
 */
static coord_def _choose_throw_dest(const monster &thrower,
                                            const actor &victim,
                                            const coord_def &target_site)
{
    ray_def ray;
    vector<coord_weight> dests;
    const bool found_ray
        = find_ray(thrower.pos(), target_site, ray, opc_solid_see);
    // Should have already been rejected by _choose_throwing_target.
    ASSERT(found_ray);
    while (ray.advance())
    {
        if (_valid_throw_dest(thrower, victim, ray.pos()))
        {
            const int dist = thrower.pos().distance_from(ray.pos());
            const int weight = sqr(LOS_RADIUS - dist + 1);
            dests.emplace_back(ray.pos(), weight);
        }
        if (ray.pos() == target_site)
            break;
    }

    ASSERT(dests.size());
    const coord_def* const choice = random_choose_weighted(dests);
    ASSERT(choice);
    ASSERT(in_bounds(*choice));
    return *choice;
}

/**
 * Actually perform a throw to the specified destination.
 *
 * @param thrower       The monster doing the tossing.
 * @param victim        The tossee.
 * @param pow           The power of the throw; determines damage.
 * @param chosen_dest   The final destination of the victim.
 */
static void _throw_to(const monster &thrower, actor &victim,
                              int pow, const coord_def &chosen_dest)
{
    ASSERT(in_bounds(chosen_dest));

    const bool thrower_seen = you.can_see(thrower);
    const bool victim_was_seen = you.can_see(victim);
    const string thrower_name = thrower.name(DESC_THE);

    const int dam = victim.apply_ac(random2(pow));
    victim.stop_being_constricted(true);
    if (victim.is_player())
    {
        mprf("%s throws you!",
             (thrower_seen ? thrower_name.c_str() : "Something"));
        move_player_to_grid(chosen_dest, false);
    }
    else
    {
        monster * const vmon = victim.as_monster();
        const string victim_name = victim.name(DESC_THE);
        const coord_def old_pos = victim.pos();

        if (!(vmon->flags & MF_WAS_IN_VIEW))
            vmon->seen_context = SC_THROWN_IN;
        vmon->move_to_pos(chosen_dest);
        vmon->apply_location_effects(old_pos);
        vmon->check_redraw(old_pos);
        if (thrower_seen || victim_was_seen)
        {
            mprf("%s throws %s%s!",
                 (thrower_seen ? thrower_name.c_str() : "Something"),
                 (victim_was_seen ? victim_name.c_str() : "something"),
                 (you.can_see(*vmon) ? "" : " out of view"));
        }
    }
    victim.hurt(&thrower, dam, BEAM_MISSILE, KILLED_BY_BEING_THROWN, "", "",
                true);
}

/**
 * The actor throws the victim to a habitable square within LOS of the victim
 * and at least as far as a distance of 2 from the thrower, which deals
 * AC-checking damage. A hostile monster prefers to throw the player into a
 * dangerous spot, and a monster throwing another monster prefers to throw far
 * from the player, regardless of alignment.
 * @param thrower  The thrower.
 * @param victim   The victim.
 * @param pow      The throw power, which is the die size for damage.
 * @return         True if the victim was thrown, False otherwise.
 */
static bool _throw(const monster &thrower, actor &victim, int pow)
{
    const coord_def throw_target = _choose_throwing_target(thrower, victim,
                                                           _throw_site_score);
    if (throw_target.origin())
        return false;

    const coord_def chosen_dest = _choose_throw_dest(thrower, victim,
                                                             throw_target);
    _throw_to(thrower, victim, pow, chosen_dest);
    return true;
}

/**
 * Score a landing site for purposes of throwing the victim. This uses monster
 * difficulty and number of open (habitable) squares as a score if the victim
 * is the player, or distance from player otherwise.
 * @param   thrower  The thrower.
 * @param   victim   The victim.
 * @param   site     The site to score.
 * @return           An integer score >= 0
 */
static int _throw_site_score(const monster &thrower, const actor &victim,
                             const coord_def &site)
{
    const int open_site_score = 1;
    const monster * const vmons = victim.as_monster();

    // Initial score is just as far away from player as possible, and
    // we stop there if the thrower or victim is friendly.
    int score = you.pos().distance_from(site);
    if (thrower.friendly() || (vmons && vmons->friendly()))
        return score;

    for (adjacent_iterator ai(site); ai; ++ai)
    {
        if (!thrower.see_cell(*ai))
            continue;

        // Being next to open space is bad for players, who thrive in crannies
        // and nooks, like the vermin they are.
        if (victim.is_habitable(*ai))
            score += open_site_score;

        // Being next to dangerous monsters is also bad.
        const monster * const mons = monster_at(*ai);
        if (mons && !mons->friendly()
            && mons != &thrower
            && !mons_is_firewood(mons))
        {
            score += sqr(mons_threat_level(mons) + 2);
        }
    }

    return score;
}

/**
 * Check if a siren or merfolk avatar should sing its song.
 *
 * @param mons   The singing monster.
 * @param avatar Whether to use the more powerful "avatar song".
 * @return       Whether the song should be sung.
 */
static bool _should_siren_sing(monster* mons, bool avatar)
{
    // Don't behold observer in the arena.
    if (crawl_state.game_is_arena())
        return false;

    // Don't behold player already half down or up the stairs.
    if (!you.delay_queue.empty())
    {
        const delay_queue_item delay = you.delay_queue.front();

        if (delay.type == DELAY_ASCENDING_STAIRS
            || delay.type == DELAY_DESCENDING_STAIRS)
        {
            dprf("Taking stairs, don't mesmerise.");
            return false;
        }
    }

    // Won't sing if either of you silenced, or it's friendly,
    // confused, fleeing, or leaving the level.
    if (mons->has_ench(ENCH_CONFUSION)
        || mons_is_fleeing(mons)
        || mons->pacified()
        || mons->friendly()
        || !player_can_hear(mons->pos()))
    {
        return false;
    }

    // Don't even try on berserkers. Sirens know their limits.
    // (merfolk avatars should still sing since their song has other effects)
    if (!avatar && you.berserk())
        return false;

    // If the mer is trying to mesmerise you anew, only sing half as often.
    if (!you.beheld_by(mons) && mons->foe == MHITYOU && you.can_see(*mons)
        && coinflip())
    {
        return false;
    }

    // We can do it!
    return true;
}

/**
 * Have a monster attempt to cast Doom Howl.
 *
 * @param mon   The howling monster.
 */
static void _doom_howl(monster &mon)
{
    mprf("%s unleashes a %s howl, and it begins to echo in your mind!",
         mon.name(DESC_THE).c_str(),
         silenced(mon.pos()) ? "silent" : "terrible");
    you.duration[DUR_DOOM_HOWL] = random_range(120, 180);
    mon.props[DOOM_HOUND_HOWLED_KEY] = true;
}

/**
 * Have a monster cast Awaken Earth.
 *
 * @param mon    The monster casting the spell.
 * @param target The target cell.
 */
static void _mons_awaken_earth(monster &mon, const coord_def &target)
{
    if (!in_bounds(target))
    {
        if (you.can_see(mon))
            canned_msg(MSG_NOTHING_HAPPENS);
        return;
    }

    bool seen = false;
    int count = 0;
    const int max = 1 + (mon.spell_hd(SPELL_AWAKEN_EARTH) > 15)
                      + random2(mon.spell_hd(SPELL_AWAKEN_EARTH) / 7 + 1);

    for (fair_adjacent_iterator ai(target, false); ai; ++ai)
    {
        if (!_feat_is_awakenable(grd(*ai))
            || env.markers.property_at(*ai, MAT_ANY, "veto_disintegrate")
               == "veto")
        {
            continue;
        }

        destroy_wall(*ai);
        if (you.see_cell(*ai))
            seen = true;

        if (create_monster(mgen_data(
                MONS_EARTH_ELEMENTAL, SAME_ATTITUDE((&mon)), &mon,
                2, SPELL_AWAKEN_EARTH, *ai, mon.foe, MG_NONE, mon.god)))
        {
            count++;
        }

        if (count >= max)
            break;
    }

    if (seen)
    {
        noisy(20, target);
        mprf("Some walls %s!",
             count > 0 ? "begin to move on their own"
                       : "crumble away");
    }
    else
        noisy(20, target, "You hear rumbling.");

    if (!seen && !count && you.can_see(mon))
        canned_msg(MSG_NOTHING_HAPPENS);
}

/**
 * Have a siren or merfolk avatar attempt to mesmerize the player.
 *
 * @param mons   The singing monster.
 * @param avatar Whether to use the more powerful "avatar song".
 */
static void _siren_sing(monster* mons, bool avatar)
{
    const msg_channel_type spl = (mons->friendly() ? MSGCH_FRIEND_SPELL
                                                       : MSGCH_MONSTER_SPELL);
    const bool already_mesmerised = you.beheld_by(mons);

    noisy(LOS_RADIUS, mons->pos(), mons->mid, NF_SIREN);

    if (avatar && !mons->has_ench(ENCH_MERFOLK_AVATAR_SONG))
        mons->add_ench(mon_enchant(ENCH_MERFOLK_AVATAR_SONG, 0, mons, 70));

    if (you.can_see(*mons))
    {
        const char * const song_adj = already_mesmerised ? "its luring"
                                                         : "a haunting";
        const string song_desc = make_stringf(" chants %s song.", song_adj);
        simple_monster_message(mons, song_desc.c_str(), spl);
    }
    else
    {
        mprf(MSGCH_SOUND, "You hear %s.",
                          already_mesmerised ? "a luring song" :
                          coinflip()         ? "a haunting song"
                                             : "an eerie melody");

        // If you're already mesmerised by an invisible siren, it
        // can still prolong the enchantment.
        if (!already_mesmerised)
            return;
    }

    // Once mesmerised by a particular monster, you cannot resist anymore.
    const int res_magic =
        you.check_res_magic(mons->get_hit_dice() * 22 / 3 + 15);

    if (you.duration[DUR_MESMERISE_IMMUNE]
        || !already_mesmerised
           && (res_magic > 0 || you.clarity()))
    {
        if (you.clarity())
            canned_msg(MSG_YOU_UNAFFECTED);
        else if (you.duration[DUR_MESMERISE_IMMUNE] && !already_mesmerised)
            canned_msg(MSG_YOU_RESIST);
        else
            mprf("You%s", you.resist_margin_phrase(res_magic).c_str());
        return;
    }

    you.add_beholder(mons);
}

// Checks to see if a particular spell is worth casting in the first place.
static bool _ms_waste_of_time(monster* mon, mon_spell_slot slot)
{
    spell_type monspell = slot.spell;
    actor *foe = mon->get_foe();
    const bool friendly = mon->friendly();

    // Keep friendly summoners from spamming summons constantly.
    if (friendly
        && !foe
        && spell_typematch(monspell, SPTYP_SUMMONING))
    {
        return true;
    }

    if (!mon->wont_attack())
    {
        if (spell_harms_area(monspell) && env.sanctuary_time > 0)
            return true;

        if (spell_harms_target(monspell) && is_sanctuary(mon->target))
            return true;
    }

    if (slot.flags & MON_SPELL_BREATH && mon->has_ench(ENCH_BREATH_WEAPON))
        return true;

    // Don't bother casting a summon spell if we're already at its cap
    if (summons_are_capped(monspell)
        && count_summons(mon, monspell) >= summons_limit(monspell))
    {
        return true;
    }

    // Eventually, we'll probably want to be able to have monsters
    // learn which of their elemental bolts were resisted and have those
    // handled here as well. - bwr
    switch (monspell)
    {
    case SPELL_CALL_TIDE:
        return !player_in_branch(BRANCH_SHOALS)
               || mon->has_ench(ENCH_TIDE)
               || !foe
               || (grd(mon->pos()) == DNGN_DEEP_WATER
                   && grd(foe->pos()) == DNGN_DEEP_WATER);

    case SPELL_BRAIN_FEED:
        return !foe || !foe->is_player();

    case SPELL_VAMPIRIC_DRAINING:
        if (!foe
            || !adjacent(mon->pos(), foe->pos())
            || !(foe->holiness() & MH_NATURAL) // dumb enough to cast on rN,
                                              // but not undead/demons/etc
            || x_chance_in_y(mon->hit_points - (mon->max_hit_points / 3),
                             mon->max_hit_points * 2 / 3))
        {
            return true;
        }
    // fall through
    case SPELL_BOLT_OF_DRAINING:
    case SPELL_AGONY:
    case SPELL_MALIGN_OFFERING:
        return !foe || _foe_should_res_negative_energy(foe);

    case SPELL_MIASMA_BREATH:
        return !foe || foe->res_rotting();

    case SPELL_DISPEL_UNDEAD:
        // [ds] How is dispel undead intended to interact with vampires?
        // Currently if the vampire's undead state returns MH_UNDEAD it
        // affects the player.
        return !foe || !(foe->holiness() & MH_UNDEAD);

    case SPELL_CORONA:
        return !foe || foe->backlit() || foe->glows_naturally();

    case SPELL_BERSERKER_RAGE:
        // Snorg does not go berserk as often until wounded.
        if (mon->type == MONS_SNORG
            && mon->hit_points == mon->max_hit_points
            && !one_chance_in(4))
        {
            return true;
        }
        return !mon->needs_berserk(false);

    case SPELL_HASTE:
        return mon->has_ench(ENCH_HASTE);

    case SPELL_MIGHT:
        return mon->has_ench(ENCH_MIGHT);

#if TAG_MAJOR_VERSION == 34
    case SPELL_SWIFTNESS:
#endif
    case SPELL_SPRINT:
        return mon->has_ench(ENCH_SWIFT);

    case SPELL_REGENERATION:
        return mon->has_ench(ENCH_REGENERATION)
               || mon->has_ench(ENCH_DEATHS_DOOR);

    case SPELL_INJURY_MIRROR:
        return mon->has_ench(ENCH_MIRROR_DAMAGE)
               || mon->props.exists("mirror_recast_time")
                  && you.elapsed_time < mon->props["mirror_recast_time"].get_int();

    case SPELL_TROGS_HAND:
        return mon->has_ench(ENCH_RAISED_MR)
               || mon->has_ench(ENCH_REGENERATION);

    case SPELL_INVISIBILITY:
        return mon->has_ench(ENCH_INVIS)
               || mon->glows_naturally();

    case SPELL_MINOR_HEALING:
    case SPELL_MAJOR_HEALING:
        return mon->hit_points > mon->max_hit_points / 2;

    case SPELL_TELEPORT_SELF:
        // Monsters aren't smart enough to know when to cancel teleport.
        return mon->has_ench(ENCH_TP) || mon->no_tele(true, false);

    case SPELL_BLINK_CLOSE:
        if (!foe || adjacent(mon->pos(), foe->pos()))
            return true;
        // intentional fall-through
    case SPELL_BLINK:
    case SPELL_CONTROLLED_BLINK:
    case SPELL_BLINK_RANGE:
    case SPELL_BLINK_AWAY:
        // Prefer to keep a tornado going rather than blink.
        return mon->no_tele(true, false) || mon->has_ench(ENCH_TORNADO);

    case SPELL_BLINK_OTHER:
    case SPELL_BLINK_OTHER_CLOSE:
        return !foe
                || foe->is_monster()
                    && foe->as_monster()->has_ench(ENCH_DIMENSION_ANCHOR)
                || foe->is_player()
                    && you.duration[DUR_DIMENSION_ANCHOR];

    case SPELL_TELEPORT_OTHER:
        // Monsters aren't smart enough to know when to cancel teleport.
        if (!foe
            || foe->is_player() && you.duration[DUR_TELEPORT]
            || foe->is_monster() && foe->as_monster()->has_ench(ENCH_TP))
        {
            return true;
        }
        // intentional fall-through
    case SPELL_SLOW:
    case SPELL_CONFUSE:
    case SPELL_PAIN:
    case SPELL_BANISHMENT:
    case SPELL_DISINTEGRATE:
    case SPELL_PARALYSE:
    case SPELL_SLEEP:
    case SPELL_HIBERNATION:
    case SPELL_DIMENSION_ANCHOR:
    {
        if ((monspell == SPELL_HIBERNATION || monspell == SPELL_SLEEP)
            && (!foe || foe->asleep() || !foe->can_sleep()))
        {
            return true;
        }

        // Occasionally we don't estimate... just fire and see.
        if (one_chance_in(5))
            return false;

        // Only intelligent monsters estimate.
        if (mons_intel(mon) < I_HUMAN)
            return false;

        // We'll estimate the target's resistance to magic, by first getting
        // the actual value and then randomising it.
        int est_magic_resist = 10000;

        if (foe != nullptr)
        {
            est_magic_resist = foe->res_magic();

            // now randomise
            est_magic_resist += random2(60) - 30; // +-30
        }

        const int power = ench_power_stepdown(_mons_spellpower(monspell, *mon));

        // Determine the amount of chance allowed by the benefit from
        // the spell. The estimated difficulty is the probability
        // of rolling over 100 + diff on 2d100. -- bwr
        int diff = (monspell == SPELL_PAIN
                    || monspell == SPELL_SLOW
                    || monspell == SPELL_CONFUSE) ? 0 : 50;

        return est_magic_resist - power > diff;
    }

    // Mara shouldn't cast player ghost if he can't see the player
    case SPELL_SUMMON_ILLUSION:
        return !foe
               || !mon->see_cell_no_trans(foe->pos())
               || !mon->can_see(*foe)
               || !actor_is_illusion_cloneable(foe);

    case SPELL_AWAKEN_FOREST:
        return mon->has_ench(ENCH_AWAKEN_FOREST)
               || env.forest_awoken_until > you.elapsed_time
               || !forest_near_enemy(mon);

    case SPELL_DEATHS_DOOR:
        // The caster may be an (undead) enslaved soul.
        return mon->holiness() & MH_UNDEAD
               || mon->has_ench(ENCH_DEATHS_DOOR)
               || mon->has_ench(ENCH_FATIGUE)
               || !foe || !mon->see_cell_no_trans(foe->pos())
               || !mon->can_see(*foe);

    case SPELL_OZOCUBUS_ARMOUR:
        return mon->is_insubstantial() || mon->has_ench(ENCH_OZOCUBUS_ARMOUR);

    case SPELL_BATTLESPHERE:
        return find_battlesphere(mon);

    case SPELL_SPECTRAL_WEAPON:
        return find_spectral_weapon(mon)
            || !weapon_can_be_spectral(mon->weapon())
            || !foe
            // Don't cast unless the caster is at or close to melee range for
            // its target. Casting spectral weapon at distance is bad since it
            // generally helps the caster's target maintain distance, also
            // letting the target exploit the spectral's damage sharing.
            || grid_distance(mon->pos(), foe->pos()) > 2;

    case SPELL_INJURY_BOND:
        for (monster_iterator mi; mi; ++mi)
        {
            if (mons_aligned(mon, *mi) && !mi->has_ench(ENCH_CHARM)
                && !mi->has_ench(ENCH_HEXED)
                && *mi != mon && mon->see_cell_no_trans(mi->pos())
                && !mi->has_ench(ENCH_INJURY_BOND))
            {
                return false; // We found at least one target; that's enough.
            }
        }
        return true;

    case SPELL_GHOSTLY_FIREBALL:
        return !foe || foe->holiness() & MH_UNDEAD;

    case SPELL_BLINK_ALLIES_ENCIRCLE:
        if (!foe || !mon->see_cell_no_trans(foe->pos()) || !mon->can_see(*foe))
            return true;

        for (monster_near_iterator mi(mon, LOS_NO_TRANS); mi; ++mi)
            if (_valid_encircle_ally(mon, *mi, foe->pos()))
                return false; // We found at least one valid ally; that's enough.
        return true;

    case SPELL_AWAKEN_VINES:
        return !foe
               || mon->has_ench(ENCH_AWAKEN_VINES)
                   && mon->props["vines_awakened"].get_int() >= 3
               || !_awaken_vines(mon, true);

    case SPELL_CONTROL_WINDS:
        return mon->has_ench(ENCH_CONTROL_WINDS);

    case SPELL_WATERSTRIKE:
        return !foe || !feat_is_watery(grd(foe->pos()));

    // Don't use unless our foe is close to us and there are no allies already
    // between the two of us
    case SPELL_WIND_BLAST:
        if (foe && foe->pos().distance_from(mon->pos()) < 4)
        {
            bolt tracer;
            tracer.target = foe->pos();
            tracer.range  = LOS_RADIUS;
            tracer.hit    = AUTOMATIC_HIT;
            fire_tracer(mon, tracer);

            actor* act = actor_at(tracer.path_taken.back());
            if (act && mons_aligned(mon, act))
                return true;
            else
                return false;
        }
        else
            return true;

    case SPELL_STRIP_RESISTANCE:
        return !foe
               || foe->is_monster()
                  && foe->as_monster()->has_ench(ENCH_LOWERED_MR)
               || foe->is_player() && you.duration[DUR_LOWERED_MR];

    case SPELL_BROTHERS_IN_ARMS:
        return mon->props.exists("brothers_count")
               && mon->props["brothers_count"].get_int() >= 2;

    case SPELL_HAUNT:
    case SPELL_SUMMON_SPECTRAL_ORCS:
    case SPELL_SMITING:
    case SPELL_HOLY_FLAMES:
    case SPELL_SUMMON_MUSHROOMS:
    case SPELL_ENTROPIC_WEAVE:
        return !foe;

    case SPELL_AIRSTRIKE:
        return !foe || foe->res_wind();

    case SPELL_FREEZE:
        return !foe || !adjacent(mon->pos(), foe->pos());

    case SPELL_DRUIDS_CALL:
        // Don't cast unless there's at least one valid target
        for (monster_iterator mi; mi; ++mi)
            if (_valid_druids_call_target(mon, *mi))
                return false;
        return true;

    // Don't spam mesmerisation if you're already mesmerised
    case SPELL_MESMERISE:
        return you.beheld_by(mon) && coinflip();

    case SPELL_DISCHARGE:
        // TODO: possibly check for friendlies nearby?
        // Perhaps it will be used recklessly like chain lightning...
        return !foe || !adjacent(foe->pos(), mon->pos());

    case SPELL_PORTAL_PROJECTILE:
    {
        bolt beam;
        beam.source    = mon->pos();
        beam.target    = mon->target;
        beam.source_id = mon->mid;
        return !handle_throw(mon, beam, true, true);
    }

    case SPELL_VIRULENCE:
        return !foe || foe->res_poison(false) >= 3;

    case SPELL_FLASH_FREEZE:
        return !foe
               || foe->is_player() && you.duration[DUR_FROZEN]
               || foe->is_monster()
                  && foe->as_monster()->has_ench(ENCH_FROZEN);

    case SPELL_LEGENDARY_DESTRUCTION:
        return !foe;

    case SPELL_BLACK_MARK:
        return mon->has_ench(ENCH_BLACK_MARK);

    // No need to spam cantrips if we're just travelling around
    case SPELL_CANTRIP:
        return !foe;

    case SPELL_BLINK_ALLIES_AWAY:
        if (!foe || !mon->see_cell_no_trans(foe->pos()) && !mon->can_see(*foe))
            return true;

        for (monster_near_iterator mi(mon, LOS_NO_TRANS); mi; ++mi)
            if (_valid_blink_away_ally(mon, *mi, foe->pos()))
                return false;
        return true;

    case SPELL_SHROUD_OF_GOLUBRIA:
        return mon->has_ench(ENCH_SHROUD);

    // Don't let clones duplicate anything, to prevent exponential explosion
    case SPELL_FAKE_MARA_SUMMON:
        return mon->has_ench(ENCH_PHANTOM_MIRROR);

    case SPELL_PHANTOM_MIRROR:
        if (!mon->has_ench(ENCH_PHANTOM_MIRROR))
        {
            for (monster_near_iterator mi(mon); mi; ++mi)
            {
                // A single valid target is enough.
                if (_mirrorable(mon, *mi))
                    return false;
            }
        }
        return true;

    case SPELL_THROW_BARBS:
        // Don't fire barbs in melee range.
        return !foe || adjacent(mon->pos(), foe->pos());

    case SPELL_BATTLECRY:
        return !_battle_cry(*mon, true);

    case SPELL_WARNING_CRY:
        return friendly;

    case SPELL_CONJURE_BALL_LIGHTNING:
        return friendly
               && (you.res_elec() <= 0 || get_hp() <= 50)
               && !(mon->holiness() & MH_DEMONIC); // rude demons

    case SPELL_SEAL_DOORS:
        return friendly || !_seal_doors_and_stairs(mon, true);

    case SPELL_FLAY:
        return !foe || !(foe->holiness() & MH_NATURAL);

    case SPELL_TWISTED_RESURRECTION:
        if (friendly && !_animate_dead_okay(monspell))
            return true;

        if (mon->is_summoned())
            return true;

        return !twisted_resurrection(mon, 500, SAME_ATTITUDE(mon), mon->foe,
                                     mon->god, false);

    case SPELL_CIGOTUVIS_EMBRACE:
        if (friendly && !_animate_dead_okay(monspell))
            return true;
        if (mon->has_ench(ENCH_BONE_ARMOUR))
            return true;
        return !harvest_corpses(*mon, true);

    //XXX: unify with the other SPELL_FOO_OTHER spells?
    case SPELL_BERSERK_OTHER:
        return !_incite_monsters(mon, false);

    case SPELL_CAUSE_FEAR:
        return _mons_cause_fear(mon, false) < 0;

    case SPELL_MASS_CONFUSION:
        return _mons_mass_confuse(mon, false) < 0;

    case SPELL_THROW:
    {
        const actor* victim = _get_throw_victim(*mon);
        return !victim || _choose_throwing_target(*mon, *victim,
                                                  _throw_site_score).origin();
    }

    case SPELL_THROW_ALLY:
        return !_find_ally_to_throw(*mon);

    case SPELL_CREATE_TENTACLES:
        return !mons_available_tentacles(mon);

    case SPELL_WORD_OF_RECALL:
        return !_should_recall(mon);

    case SPELL_SHATTER:
        return friendly || !mons_shatter(mon, false);

    case SPELL_SYMBOL_OF_TORMENT:
        return !_trace_los(mon, _torment_vulnerable)
               || you.visible_to(mon)
                  && friendly
                  && !player_res_torment(false)
                  && !player_kiku_res_torment();
    case SPELL_CHAIN_LIGHTNING:
        return !_trace_los(mon, _elec_vulnerable)
                || you.visible_to(mon) && friendly; // don't zap player
    case SPELL_CHAIN_OF_CHAOS:
        return !_trace_los(mon, _dummy_vulnerable);
    case SPELL_CORRUPTING_PULSE:
        return !_trace_los(mon, _mutation_vulnerable)
               || you.visible_to(mon)
                  && friendly;
    case SPELL_TORNADO:
        return mon->has_ench(ENCH_TORNADO)
               || mon->has_ench(ENCH_TORNADO_COOLDOWN)
               || !_trace_los(mon, _tornado_vulnerable)
               || you.visible_to(mon) && friendly // don't cast near the player
                  && !(mon->holiness() & MH_DEMONIC); // demons are rude

    case SPELL_ENGLACIATION:
        return !foe
               || !mon->see_cell_no_trans(foe->pos())
               || foe->res_cold() > 0;

    case SPELL_OLGREBS_TOXIC_RADIANCE:
        return mon->has_ench(ENCH_TOXIC_RADIANCE)
               || cast_toxic_radiance(mon, 100, false, true) != SPRET_SUCCESS;
    case SPELL_IGNITE_POISON:
        return cast_ignite_poison(mon, 0, false, true) != SPRET_SUCCESS;

    case SPELL_DRAIN_LIFE:
    case SPELL_OZOCUBUS_REFRIGERATION:
        return cast_los_attack_spell(monspell, _mons_spellpower(monspell, *mon),
                                     mon, false) != SPRET_SUCCESS
               || you.visible_to(mon)
                  && friendly
                  && (monspell == SPELL_OZOCUBUS_REFRIGERATION
                      && player_res_cold(false) < 1
                      || monspell == SPELL_DRAIN_LIFE
                         && player_prot_life(false) < 1);

    case SPELL_GLACIATE:
        return !foe
               || !_glaciate_tracer(mon, _mons_spellpower(monspell, *mon),
                                    foe->pos());

    case SPELL_CLOUD_CONE:
        return !foe
               || !mons_should_cloud_cone(mon, _mons_spellpower(monspell, *mon),
                                          foe->pos());

    // Friendly monsters don't use polymorph, as it's likely to cause
    // runaway growth of an enemy.
    case SPELL_POLYMORPH:
        return friendly;

    case SPELL_MALIGN_GATEWAY:
        return !can_cast_malign_gateway();

    case SPELL_LEDAS_LIQUEFACTION:
        return !mon->stand_on_solid_ground()
                || liquefied(mon->pos());

    case SPELL_SIREN_SONG:
        return !_should_siren_sing(mon, false);

    case SPELL_AVATAR_SONG:
        return !_should_siren_sing(mon, true);

    case SPELL_REPEL_MISSILES:
        return mon->has_ench(ENCH_REPEL_MISSILES);

    case SPELL_DEFLECT_MISSILES:
        return mon->has_ench(ENCH_DEFLECT_MISSILES);

    case SPELL_PARALYSIS_GAZE:
        return !foe || foe->paralysed() || !mon->can_see(*foe);

    case SPELL_CONFUSION_GAZE:
    case SPELL_DRAINING_GAZE:
        return !foe || !mon->can_see(*foe);

    case SPELL_CONTROL_UNDEAD:
        return _mons_control_undead(mon, false) < 0;

    case SPELL_SCATTERSHOT:
        return !foe
               || !scattershot_tracer(mon, _mons_spellpower(monspell, *mon),
                                      foe->pos());

    case SPELL_CLEANSING_FLAME:
    {
        bolt tracer;
        setup_cleansing_flame_beam(tracer,
                                   5 + (7 * mon->spell_hd(monspell)) / 12,
                                   CLEANSING_FLAME_SPELL, mon->pos(), mon);
        fire_tracer(mon, tracer, true);
        return !mons_should_fire(tracer);
    }

    case SPELL_GRAVITAS:
        if (!foe)
            return true;

        for (actor_near_iterator ai(foe, LOS_SOLID); ai; ++ai)
            if (*ai != mon && *ai != foe && !ai->is_stationary()
                && mon->can_see(**ai))
            {
                return false;
            }

        return true;

    case SPELL_DOOM_HOWL:
        return !foe || !foe->is_player() || you.duration[DUR_DOOM_HOWL]
                || mon->props[DOOM_HOUND_HOWLED_KEY]
                || mon->is_summoned();

    case SPELL_CALL_OF_CHAOS:
        return !_mons_call_of_chaos(*mon, true);

    case SPELL_AURA_OF_BRILLIANCE:
        if (mon->has_ench(ENCH_BRILLIANCE_AURA) || !foe || !mon->can_see(*foe))
            return true;

        for (monster_near_iterator mi(mon, LOS_NO_TRANS); mi; ++mi)
            if (_valid_aura_of_brilliance_ally(mon, *mi))
                return false;
        return true;

#if TAG_MAJOR_VERSION == 34
    case SPELL_SUMMON_TWISTER:
    case SPELL_SHAFT_SELF:
    case SPELL_MISLEAD:
    case SPELL_SUMMON_SCORPIONS:
    case SPELL_SUMMON_SWARM:
    case SPELL_SUMMON_ELEMENTAL:
    case SPELL_EPHEMERAL_INFUSION:
    case SPELL_SINGULARITY:
    case SPELL_GRAND_AVATAR:
    case SPELL_INNER_FLAME:
    case SPELL_ANIMATE_DEAD:
    case SPELL_SIMULACRUM:
    case SPELL_CHANT_FIRE_STORM:
    case SPELL_IGNITE_POISON_SINGLE:
    case SPELL_CONDENSATION_SHIELD:
    case SPELL_STONESKIN:
    case SPELL_HUNTING_CRY:
#endif
    case SPELL_NO_SPELL:
        return true;

    default:
        return false;
    }
}

static string _god_name(god_type god)
{
    return god_has_name(god) ? god_name(god) : "Something";
}
