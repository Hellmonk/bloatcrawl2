/**
 * @file
 * @brief Spell miscast class.
**/

#include "AppHdr.h"

#include "spl-miscast.h"

#include <sstream>

#include "areas.h"
#include "branch.h"
#include "cloud.h"
#include "colour.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "food.h"
#include "godpassive.h"
#include "godwrath.h"
#include "item_use.h"
#include "itemprop.h"
#include "mapmark.h"
#include "message.h"
#include "misc.h"
#include "mon-cast.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mutation.h"
#include "player-stats.h"
#include "potion.h"
#include "religion.h"
#include "shout.h"
#include "spl-clouds.h"
#include "spl-goditem.h"
#include "spl-summoning.h"
#include "state.h"
#include "stringutil.h"
#include "terrain.h"
#include "transform.h"
#include "unwind.h"
#include "viewchar.h"
#include "view.h"
#include "xom.h"

// This determines how likely it is that more powerful wild magic
// effects will occur. Set to 100 for the old probabilities (although
// the individual effects have been made much nastier since then).
#define WILD_MAGIC_NASTINESS 150

#define MAX_RECURSE 100

MiscastEffect::MiscastEffect(actor* _target, actor* _act_source,
                             int _source, spell_type _spell,
                             int _pow, int _fail, string _cause,
                             nothing_happens_when_type _nothing_happens,
                             int _lethality_margin, string _hand_str,
                             bool _can_plural) :
    target(_target), act_source(_act_source),
    special_source(_source), cause(_cause), spell(_spell),
    school(SPTYP_NONE), pow(_pow), fail(_fail), level(-1),
    nothing_happens_when(_nothing_happens),
    lethality_margin(_lethality_margin), hand_str(_hand_str),
    can_plural_hand(_can_plural)
{
    ASSERT(is_valid_spell(_spell));
    ASSERT(get_spell_disciplines(_spell) != SPTYP_NONE);

    init();
    do_miscast();
}

MiscastEffect::MiscastEffect(actor* _target, actor* _act_source, int _source,
                             spschool_flag_type _school, int _level,
                             string _cause,
                             nothing_happens_when_type _nothing_happens,
                             int _lethality_margin, string _hand_str,
                             bool _can_plural) :
    target(_target), act_source(_act_source),
    special_source(_source), cause(_cause),
    spell(SPELL_NO_SPELL), school(_school), pow(-1), fail(-1), level(_level),
    nothing_happens_when(_nothing_happens),
    lethality_margin(_lethality_margin), hand_str(_hand_str),
    can_plural_hand(_can_plural)
{
    ASSERT(!_cause.empty());
    ASSERT(count_bits(_school) == 1);
    ASSERT(_school <= SPTYP_LAST_SCHOOL || _school == SPTYP_RANDOM);
    ASSERT_RANGE(level, 0, 3 + 1);

    init();
    do_miscast();
}

MiscastEffect::MiscastEffect(actor* _target, actor* _act_source, int _source,
                             spschool_flag_type _school, int _pow, int _fail,
                             string _cause,
                             nothing_happens_when_type _nothing_happens,
                             int _lethality_margin, string _hand_str,
                             bool _can_plural) :
    target(_target), act_source(_act_source),
    special_source(_source), cause(_cause),
    spell(SPELL_NO_SPELL), school(_school), pow(_pow), fail(_fail), level(-1),
    nothing_happens_when(_nothing_happens),
    lethality_margin(_lethality_margin), hand_str(_hand_str),
    can_plural_hand(_can_plural)
{
    ASSERT(!_cause.empty());
    ASSERT(count_bits(_school) == 1);
    ASSERT(_school <= SPTYP_LAST_SCHOOL || _school == SPTYP_RANDOM);

    init();
    do_miscast();
}

MiscastEffect::~MiscastEffect()
{
    ASSERT(recursion_depth == 0);
}

void MiscastEffect::init()
{
    ASSERT(spell != SPELL_NO_SPELL && school == SPTYP_NONE
           || spell == SPELL_NO_SPELL && school != SPTYP_NONE);
    ASSERT(pow != -1 && fail != -1 && level == -1
           || pow == -1 && fail == -1 && level >= 0 && level <= 3);

    ASSERT(target != nullptr);
    ASSERT(target->alive());

    ASSERT(lethality_margin == 0 || target->is_player());

    recursion_depth = 0;

    source_known = target_known = false;

    if (target->is_monster())
        target_known = you.can_see(*target);
    else
        target_known = true;

    if (act_source && act_source->is_player())
    {
        kt           = KILL_YOU_MISSILE;
        source_known = true;
    }
    else if (act_source && act_source->is_monster())
    {
        if (act_source->as_monster()->confused_by_you()
            && !act_source->as_monster()->friendly())
        {
            kt = KILL_YOU_CONF;
        }
        else
            kt = KILL_MON_MISSILE;

        source_known = you.can_see(*act_source);

        if (target_known && special_source == MUMMY_MISCAST)
            source_known = true;
    }
    else
    {
        ASSERT(special_source != MELEE_MISCAST);

        kt = KILL_MISCAST;

        if (special_source == ZOT_TRAP_MISCAST)
        {
            source_known = target_known;

            if (target->is_monster()
                && target->as_monster()->confused_by_you())
            {
                kt = KILL_YOU_CONF;
            }
        }
        else
            source_known = true;
    }

    ASSERT(kt != KILL_NONE);

    // source_known = false for MELEE_MISCAST so that melee miscasts
    // won't give a "nothing happens" message.
    if (special_source == MELEE_MISCAST)
        source_known = false;

    if (hand_str.empty())
        target->hand_name(true, &can_plural_hand);

    // Explosion stuff.
    beam.is_explosion = true;

    // [ds] Don't attribute the beam's cause to the actor, because the
    // death message will name the actor anyway.
    if (cause.empty())
        cause = get_default_cause(false);
    beam.aux_source  = cause;
    if (act_source)
        beam.source_id = act_source->mid;
    else
        beam.source_id = MID_NOBODY;
    beam.thrower     = kt;
}

string MiscastEffect::get_default_cause(bool attribute_to_user) const
{
    // This is only for true miscasts, which means both a spell and that
    // the source of the miscast is the same as the target of the miscast.
    ASSERT(special_source == SPELL_MISCAST);
    ASSERT(spell != SPELL_NO_SPELL);
    ASSERT(school == SPTYP_NONE);
    ASSERT(target->is_player());

    return string("miscasting ") + spell_title(spell);
}

bool MiscastEffect::neither_end_silenced()
{
    return !silenced(you.pos()) && !silenced(target->pos());
}

void MiscastEffect::do_miscast()
{
    ASSERT_RANGE(recursion_depth, 0, MAX_RECURSE);

    if (recursion_depth == 0)
        did_msg = false;

    unwind_var<int> unwind_depth(recursion_depth);
    recursion_depth++;

    // Repeated calls to do_miscast() on a single object instance have
    // killed a target which was alive when the object was created.
    if (!target->alive())
    {
        dprf("Miscast target '%s' already dead",
             target->name(DESC_PLAIN, true).c_str());
        return;
    }

    spschool_flag_type sp_type;
    int                severity;

    if (spell != SPELL_NO_SPELL)
    {
        vector<spschool_flag_type> school_list;
        for (const auto bit : spschools_type::range())
            if (spell_typematch(spell, bit))
                school_list.push_back(bit);

        sp_type = school_list[random2(school_list.size())];
    }
    else
    {
        sp_type = school;
        if (sp_type == SPTYP_RANDOM)
            sp_type = spschools_type::exponent(random2(SPTYP_LAST_EXPONENT + 1));
    }

    if (level != -1)
        severity = level;
    else
    {
        severity = (pow * fail * (10 + pow) / 7 * WILD_MAGIC_NASTINESS) / 100;

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_MISCAST)
        mprf(MSGCH_DIAGNOSTICS, "'%s' miscast power: %d",
             spell != SPELL_NO_SPELL ? spell_title(spell)
                                     : spelltype_short_name(sp_type),
             severity);
#endif

        if (random2(40) > severity && random2(40) > severity)
        {
            if (target->is_player())
                canned_msg(MSG_NOTHING_HAPPENS);
            return;
        }

        severity /= 100;
        severity = random2(severity);
        if (severity > 3)
            severity = 3;
        else if (severity < 0)
            severity = 0;
    }

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_MISCAST)
    mprf(MSGCH_DIAGNOSTICS, "Sptype: %s, severity: %d",
         spelltype_short_name(sp_type), severity);
#endif

    beam.ex_size            = 1;
    beam.name               = "";
    beam.damage             = dice_def(0, 0);
    beam.flavour            = BEAM_NONE;
    beam.msg_generated      = false;
    beam.in_explosion_phase = false;

    // Do this here since multiple miscasts (wizmode testing) might move
    // the target around.
    beam.source = target->pos();
    beam.target = target->pos();
    beam.use_target_as_pos = true;

    all_msg        = you_msg = mon_msg = mon_msg_seen = mon_msg_unseen = "";
    msg_ch         = MSGCH_PLAIN;
    sound_loudness = 0;

    if (special_source == ZOT_TRAP_MISCAST)
    {
        _zot();
        return;
    }

    switch (sp_type)
    {
    case SPTYP_CONJURATION:    _conjuration(severity);    break;
    case SPTYP_HEXES:          _hexes(severity);          break;
    case SPTYP_CHARMS:         _charms(severity);         break;
    case SPTYP_TRANSLOCATION:  _translocation(severity);  break;
    case SPTYP_SUMMONING:      _summoning(severity);      break;
    case SPTYP_NECROMANCY:     _necromancy(severity);     break;
    case SPTYP_TRANSMUTATION:  _transmutation(severity);  break;
    case SPTYP_FIRE:           _fire(severity);           break;
    case SPTYP_ICE:            _ice(severity);            break;
    case SPTYP_EARTH:          _earth(severity);          break;
    case SPTYP_AIR:            _air(severity);            break;
    case SPTYP_POISON:         _poison(severity);         break;

    default:
        die("Invalid miscast spell discipline.");
    }
}

void MiscastEffect::do_msg(bool suppress_nothing_happens)
{
    ASSERT(!did_msg);

    if (!you.see_cell(target->pos()))
        return;

    did_msg = true;

    string msg;

    if (!all_msg.empty())
        msg = all_msg;
    else if (target->is_player())
        msg = you_msg;
    else if (!mon_msg.empty())
    {
        msg = mon_msg;
        // Monster might be unseen with hands that can't be seen.
        ASSERT(msg.find("@hand") == string::npos);
    }
    else
    {
        if (you.can_see(*target))
            msg = mon_msg_seen;
        else
        {
            msg = mon_msg_unseen;
            // Can't see the hands of invisible monsters.
            ASSERT(msg.find("@hand") == string::npos);
        }
    }

    if (msg.empty())
    {
        if (!suppress_nothing_happens
            && (nothing_happens_when == NH_ALWAYS
                || (nothing_happens_when == NH_DEFAULT && source_known
                    && target_known)))
        {
            canned_msg(MSG_NOTHING_HAPPENS);
        }

        return;
    }

    bool plural;

    if (hand_str.empty())
    {
        msg = replace_all(msg, "@hand@",  target->hand_name(false, &plural));
        msg = replace_all(msg, "@hands@", target->hand_name(true));
    }
    else
    {
        plural = can_plural_hand;
        msg = replace_all(msg, "@hand@",  hand_str);
        if (can_plural_hand)
            msg = replace_all(msg, "@hands@", pluralise(hand_str));
        else
            msg = replace_all(msg, "@hands@", hand_str);
    }

    if (plural)
        msg = replace_all(msg, "@hand_conj@", "");
    else
        msg = replace_all(msg, "@hand_conj@", "s");

    if (target->is_monster())
    {
        msg = do_mon_str_replacements(msg, *target->as_monster(), S_SILENT);
        if (!mons_has_body(*target->as_monster()))
            msg = replace_all(msg, "'s body", "");
    }

    mprf(msg_ch, "%s", msg.c_str());

    if (msg_ch == MSGCH_SOUND)
    {
        // XXX: can this just be target->mid?
        mid_t src = target->is_player() ? MID_PLAYER : target->mid;
        noisy(sound_loudness, target->pos(), src);
    }
}

bool MiscastEffect::_ouch(int dam, beam_type flavour)
{
    // Delay do_msg() until after avoid_lethal().
    if (target->is_monster())
    {
        monster* mon_target = target->as_monster();

        do_msg(true);

        bolt beem;

        beem.flavour = flavour;
        dam = mons_adjust_flavoured(mon_target, beem, dam, true);
        mon_target->hurt(act_source, dam, BEAM_MISSILE, KILLED_BY_BEAM,
                         "", "", false);

        if (!mon_target->alive())
            monster_die(mon_target, kt, actor_to_death_source(act_source));
    }
    else
    {
        dam = check_your_resists(dam, flavour, cause);

        if (avoid_lethal(dam))
            return false;

        do_msg(true);

        kill_method_type method;

        if (special_source == SPELL_MISCAST && spell != SPELL_NO_SPELL)
            method = KILLED_BY_WILD_MAGIC;
        else if (special_source == ZOT_TRAP_MISCAST)
            method = KILLED_BY_TRAP;
        else if (special_source >= GOD_MISCAST)
        {
            god_type god = static_cast<god_type>(special_source - GOD_MISCAST);

            if (god == GOD_XOM && !player_under_penance(GOD_XOM))
                method = KILLED_BY_XOM;
            else
                method = KILLED_BY_DIVINE_WRATH;
        }
        else
            method = KILLED_BY_SOMETHING;

        bool see_source = act_source && you.can_see(*act_source);
        ouch(dam, method, act_source ? act_source->mid : MID_NOBODY,
             cause.c_str(), see_source,
             act_source ? act_source->name(DESC_A, true).c_str() : nullptr);
    }

    return true;
}

bool MiscastEffect::_explosion()
{
    ASSERT(!beam.name.empty());
    ASSERT(beam.damage.num != 0);
    ASSERT(beam.damage.size != 0);
    ASSERT(beam.flavour != BEAM_NONE);

    // wild magic card
    if (special_source == DECK_MISCAST)
        beam.thrower = KILL_MISCAST;

    int max_dam = beam.damage.num * beam.damage.size;
    max_dam = check_your_resists(max_dam, beam.flavour, cause);
    if (avoid_lethal(max_dam))
        return false;

    do_msg(true);
    beam.explode();

    return true;
}

bool MiscastEffect::_big_cloud(cloud_type cl_type, int cloud_pow, int size,
                               int spread_rate)
{
    if (avoid_lethal(2 * max_cloud_damage(cl_type, cloud_pow)))
        return false;

    do_msg(true);
    big_cloud(cl_type, act_source, target->pos(), cloud_pow, size, spread_rate);

    return true;
}

bool MiscastEffect::_paralyse(int dur)
{
    if (special_source != HELL_EFFECT_MISCAST)
    {
        target->paralyse(act_source, dur, cause);
        return true;
    }
    else
        return false;
}

bool MiscastEffect::_sleep(int dur)
{
    if (!target->can_sleep() || special_source == HELL_EFFECT_MISCAST)
        return false;

    if (target->is_player())
        you.put_to_sleep(act_source, dur);
    else
        target->put_to_sleep(act_source, dur);
    return true;
}

// XXX: Mostly duplicated from cast_malign_gateway.
bool MiscastEffect::_malign_gateway(bool hostile)
{
    coord_def point = find_gateway_location(target);
    bool success = (point != coord_def(0, 0));

    if (success)
    {
        const int malign_gateway_duration = BASELINE_DELAY * (random2(3) + 2);
        env.markers.add(new map_malign_gateway_marker(point,
                                malign_gateway_duration,
                                false,
                                cause,
                                hostile ? BEH_HOSTILE : BEH_FRIENDLY,
                                GOD_NO_GOD,
                                200));
        env.markers.clear_need_activate();
        env.grid(point) = DNGN_MALIGN_GATEWAY;
        set_terrain_changed(point);

        noisy(spell_effect_noise(SPELL_MALIGN_GATEWAY), point);
        all_msg = "The dungeon shakes, a horrible noise fills the air, and a "
                  "portal to some otherworldly place is opened!";
        msg_ch = MSGCH_WARN;
        do_msg();
    }

    return success;
}

bool MiscastEffect::avoid_lethal(int dam)
{
    if (lethality_margin <= 0 || (you.hp - dam) > lethality_margin)
        return false;

    if (recursion_depth == MAX_RECURSE)
    {
        // Any possible miscast would kill you, now that's interesting.
        if (you_worship(GOD_XOM))
            simple_god_message(" watches you with interest.");
        return true;
    }

    if (did_msg)
    {
#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_MISCAST)
        mprf(MSGCH_ERROR, "Couldn't avoid lethal miscast: already printed "
                          "message for this miscast.");
#endif
        return false;
    }

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_MISCAST)
    mprf(MSGCH_DIAGNOSTICS, "Avoided lethal miscast.");
#endif

    do_miscast();

    return true;
}

bool MiscastEffect::_create_monster(monster_type what, int abj_deg,
                                    bool alert)
{
    const god_type god =
        (crawl_state.is_god_acting()) ? crawl_state.which_god_acting()
                                      : GOD_NO_GOD;

    if (cause.empty())
        cause = get_default_cause(true);
    mgen_data data = mgen_data::hostile_at(what, alert, target->pos());
    data.set_summoned(nullptr, abj_deg, SPELL_NO_SPELL, god);
    data.set_non_actor_summoner(cause);

    if (special_source != HELL_EFFECT_MISCAST)
        data.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);

    // hostile_at() assumes the monster is hostile to the player,
    // but should be hostile to the target monster unless the miscast
    // is a result of either divine wrath or a Zot trap.
    if (target->is_monster() && !player_under_penance(god)
        && special_source != ZOT_TRAP_MISCAST)
    {
        monster* mon_target = target->as_monster();

        switch (mon_target->temp_attitude())
        {
            case ATT_FRIENDLY:     data.behaviour = BEH_HOSTILE; break;
            case ATT_HOSTILE:      data.behaviour = BEH_FRIENDLY; break;
            case ATT_GOOD_NEUTRAL:
            case ATT_NEUTRAL:
            case ATT_STRICT_NEUTRAL:
                data.behaviour = BEH_NEUTRAL;
            break;
        }

        if (alert)
            data.foe = mon_target->mindex();

        // No permanent allies from miscasts.
        if (data.behaviour == BEH_FRIENDLY && abj_deg == 0)
            data.abjuration_duration = 6;
    }

    // If data.abjuration_duration == 0, then data.summon_type will
    // simply be ignored.
    if (data.abjuration_duration != 0)
    {
        if (what == RANDOM_MOBILE_MONSTER)
            data.summon_type = SPELL_SHADOW_CREATURES;
        else if (player_under_penance(god))
            data.summon_type = MON_SUMM_WRATH;
        else if (special_source == ZOT_TRAP_MISCAST)
            data.summon_type = MON_SUMM_ZOT;
        else
            data.summon_type = MON_SUMM_MISCAST;
    }

    return create_monster(data);
}

/**
 * Malmutate the player.
 *
 * Significantly worse than the spell. (Gives 1-2 muts, never pure-random.)
 */
void MiscastEffect::_malmutate()
{
    you_msg = "Your body is distorted in a weirdly horrible way!";
    // We don't need messages when the mutation fails, because we give our
    // own (which is justified anyway as you take damage).
    mutate(RANDOM_BAD_MUTATION, cause, false, false);
    if (coinflip())
        mutate(RANDOM_BAD_MUTATION, cause, false, false);
}

void MiscastEffect::_conjuration(int severity)
{
    switch (severity)
    {
    case 0:         // just a harmless message but for some reason we need 11 of them
        you_msg      = "Strange energies run through your body.";
        mon_msg_seen = "@The_monster@ glows " + weird_glowing_colour() +
                       " for a moment.";
        do_msg();
    break;

    case 1:
        you_msg      = "A wave of violent energy washes through your body!";
        mon_msg_seen = "@The_monster@ lurches violently!";
        _ouch(6 + random2avg(7, 2));
        break;

    case 2:
        you_msg      = "Energy rips through your body!";
        mon_msg_seen = "@The_monster@ jerks violently!";
        _ouch(9 + random2avg(17, 2));
        break;

    case 3:
        you_msg      = "You are blasted with magical energy!";
        mon_msg_seen = "@The_monster@ is blasted with magical energy!";
        // No message for invis monsters?
        _ouch(12 + random2avg(29, 2));
        break;
    }
}

void MiscastEffect::_hexes(int severity)
{
    switch (severity)
    {
    case 0:         // harmless messages only
        you_msg        = "Multicoloured lights dance before your eyes!";
        mon_msg_seen   = "Multicoloured lights dance around @the_monster@!";
        mon_msg_unseen = "Multicoloured lights dance in the air!";
        do_msg();
        break;

    case 1:         // slightly annoying
        if (target->is_player())
        {
            contaminate_player(random2avg(2000, 3), spell != SPELL_NO_SPELL);
            you.backlight();
        }
        else
            target->as_monster()->add_ench(mon_enchant(ENCH_CORONA, 20,
                                                           act_source));
        break;

    case 2:         // much more annoying
        if (target->is_player())
        {
            contaminate_player(random2avg(6000, 3), spell != SPELL_NO_SPELL);
            you.backlight();
        }
        else
            target->as_monster()->add_ench(mon_enchant(ENCH_CORONA, 20,
                                                           act_source));
        break;

    case 3:
        target->confuse(act_source, 5 + random2(3));
        if (target->is_player())
        {
            contaminate_player(random2avg(18000, 3), spell != SPELL_NO_SPELL);
            you.backlight();
        }
        else
            target->as_monster()->add_ench(mon_enchant(ENCH_CORONA, 20,
                                                           act_source));
        break;
    }
}

void MiscastEffect::_charms(int severity)
{
    switch (severity)
    {
    case 0:         // harmless messages only
        you_msg      = "Your skin glows momentarily.";
        mon_msg_seen = "@The_monster@'s body glows momentarily.";
        do_msg();
        break;

    case 1:         // slightly annoying
        target->slow_down(act_source, 6);
        break;
        
    case 2:         // much more annoying
        target->slow_down(act_source, 12);
        break;

    case 3:         // potentially lethal
        target->slow_down(act_source, 18);
        if (special_source == HELL_EFFECT_MISCAST)
            all_msg = "Magic is drained from your body!";
        you_msg        = "Magic surges out from your body!";
        mon_msg_seen   = "Magic surges out from @the_monster@!";
        mon_msg_unseen = "Magic surges out from thin air!";
        if (target->is_player())
        {
            debuff_player();
            if (you.magic_points > 0
#if TAG_MAJOR_VERSION == 34
                || you.species == SP_DJINNI
#endif
                )
                {
                    drain_mp(4 + random2(3));
                }
        }
        else if (target->is_monster())
        {
            debuff_monster(*target->as_monster());
            enchant_actor_with_flavour(target->as_monster(), nullptr,
                                       BEAM_DRAIN_MAGIC, 50 + random2avg(51, 2));
        }
        do_msg();
        break;
    }
}

void MiscastEffect::_translocation(int severity)
{
    switch (severity)
    {
    case 0:         // harmless messages only
        you_msg      = "You feel a wrenching sensation.";
        mon_msg_seen = "@The_monster@ jerks violently for a moment.";
        do_msg();
        break;

    case 1:         // mostly harmless
        you_msg        = "Space bends around you!";
        mon_msg_seen   = "Space bends around @the_monster@!";
        mon_msg_unseen = "A piece of empty space twists and distorts.";
        if (_ouch(4 + random2avg(7, 2)) && target->alive() && !target->no_tele())
            target->blink();
        break;

    case 2:         // less harmless
        you_msg        = "Space warps around you!";
        mon_msg_seen   = "Space warps around @the_monster@!";
        mon_msg_unseen = "A piece of empty space twists and writhes.";
        _ouch(5 + random2avg(9, 2));
        if (target->alive())
        {
            // Same message as a harmless miscast, thus no permit_id.
            if (!target->no_tele(true, false))
                    target->blink();
            if (target->is_player())
                you.increase_duration(DUR_DIMENSION_ANCHOR, 5 + random2(6), 50);
            else if (target->is_monster())
                     target->as_monster()->add_ench(mon_enchant(ENCH_DIMENSION_ANCHOR,
                     0, act_source, 5 + random2(6) * BASELINE_DELAY));
        }
        break;

    case 3:         // much less harmless
        you_msg        = "Space warps crazily around you!";
        mon_msg_seen   = "Space warps crazily around @the_monster@!";
        mon_msg_unseen = "A rift temporarily opens in the fabric of space!";
        if (_ouch(9 + random2avg(17, 2)) && target->alive())
        {
            if (!target->no_tele())
                target->teleport(true);
            if (target->alive())
            {
                if (target->is_player())
                    you.increase_duration(DUR_DIMENSION_ANCHOR, 20 + random2(11), 50);
                else if (target->is_monster())
                     target->as_monster()->add_ench(mon_enchant(ENCH_DIMENSION_ANCHOR,
                     0, act_source, 20 + random2(11) * BASELINE_DELAY));
            }
        }
        break;
    }
}

void MiscastEffect::_summoning(int severity)
{
    switch (severity)
    {
    case 0:         // harmless messages only
        you_msg      = "Shadowy shapes form in the air around you, "
                           "then vanish.";
        mon_msg_seen = "Shadowy shapes form in the air around "
                           "@the_monster@, then vanish.";
        do_msg();
        break;

    case 1:         // a little bad
        if (_create_monster(summon_any_demon(RANDOM_DEMON_LESSER), 5, true))
                all_msg = "Something appears in a flash of light!";
        do_msg();
        break;

    case 2:         // more bad
        if (_create_monster(summon_any_demon(RANDOM_DEMON_COMMON), 5, true))
            all_msg = "Something forms from out of thin air!";
        do_msg();
        break;

    case 3:         // most bad
        if (_create_monster(summon_any_demon(RANDOM_DEMON_GREATER), 0, true))
            all_msg = "You sense a hostile presence.";
        do_msg();
        break;
    }
}

void MiscastEffect::_necromancy(int severity)
{
    if (target->is_player()
        && have_passive(passive_t::miscast_protection_necromancy))
    {
        if (spell != SPELL_NO_SPELL)
        {
            // An actual necromancy miscast.
            if (x_chance_in_y(you.piety, piety_breakpoint(5)))
            {
                simple_god_message(" protects you from your miscast "
                                   "necromantic spell!");
                return;
            }
        }
        else if (special_source == MUMMY_MISCAST)
        {
            if (coinflip())
            {
                simple_god_message(" averts the curse.");
                return;
            }
            else
            {
                simple_god_message(" partially averts the curse.");
                severity = max(severity - 1, 0);
            }
        }
    }

    switch (severity)
    {
    case 0:
        you_msg      = "The world around you seems to dim momentarily.";
        mon_msg_seen = "@The_monster@ seems to dim momentarily.";
        do_msg();
        break;

    case 1:         // a bit nasty
        you_msg      = "You are partially engulfed in negative energy!";
        mon_msg_seen = "@The_monster@ is partially engulfed in negative energy!";
        do_msg();
        target->drain_exp(act_source, false, 10);
        break;

    case 2:         // much nastier
        you_msg      = "You are engulfed in negative energy!";
        mon_msg_seen = "@The_monster@ is engulfed in negative energy!";
        do_msg();
        target->drain_exp(act_source, false, 30);
        if (!target->res_rotting())
            target->rot(act_source, 1, true);
        break;

    case 3:         // even nastier
        you_msg      = "You are completely engulfed in negative energy!";
        mon_msg_seen = "@The_monster@ is completely engulfed in negative energy!";

        do_msg();
        target->drain_exp(act_source, false, 90);

        if (target->is_player())
            lose_stat(STAT_RANDOM, 1 + random2(3));
        break;
    }
}

void MiscastEffect::_transmutation(int severity)
{
    switch (severity)
    {
    case 0:         // just a harmless message
        you_msg      = "Your skin glows momentarily.";
        mon_msg_seen = "@The_monster@'s body glows momentarily.";
        do_msg();
        break;

    case 1:         // slightly annoying
        you_msg        = "Your limbs ache.";
        mon_msg_seen   = "@The_monster@ looks weaker.";
        do_msg();

        if (target->is_player())
            you.increase_duration(DUR_WEAK, 8 + random2(4), 50);
        else if (target->is_monster())
        {
             target->as_monster()->add_ench(mon_enchant(ENCH_WEAK,
             0, act_source, 8 + random2(4) * BASELINE_DELAY));
        }
        break;

    case 2:         // much more annoying
        you_msg        = "Your limbs ache and wobble like jelly!";
        mon_msg_seen   = "@The_monster@ looks weaker.";
        do_msg();

        if (target->is_player())
            you.increase_duration(DUR_WEAK, 12 + random2(6), 75);
        else if (target->is_monster())
        {
             target->as_monster()->add_ench(mon_enchant(ENCH_WEAK,
             0, act_source, 10 + random2(6) * BASELINE_DELAY));
        }
        target->polymorph(0);
        break;

    case 3:         // even nastier
        you_msg        = "Your limbs ache terribly!";
        mon_msg_seen   = "@The_monster@ looks weaker.";
        do_msg();

        if (target->is_player())
            you.increase_duration(DUR_WEAK, 18 + random2(9), 100);
        else if (target->is_monster())
        {
             target->as_monster()->add_ench(mon_enchant(ENCH_WEAK,
             0, act_source, 10 + random2(6) * BASELINE_DELAY));
        }
        target->polymorph(200);

        break;
    }
}

void MiscastEffect::_fire(int severity)
{
    switch (severity)
    {
    case 0:         // just a harmless message
        you_msg      = "The air around you burns with energy!";
        mon_msg_seen = "The air around @the_monster@ burns with energy!";
        do_msg();
        break;

    case 1:         // a bit less harmless stuff
        you_msg      = "Flames sear your flesh.";
        mon_msg_seen = "Flames sear @the_monster@.";
        if (target->res_fire() < 0)
        {
            if (!_ouch(2 + random2avg(13, 2)))
                return;
        }
        else
            do_msg();
        if (target->alive())
            target->expose_to_element(BEAM_FIRE, 3);
        break;

    case 2:         // rather less harmless stuff
        you_msg        = "You are blasted with fire.";
        mon_msg_seen   = "@The_monster@ is blasted with fire.";
        mon_msg_unseen = "A flame briefly burns in thin air.";
        if (_ouch(6 + random2avg(26, 2), BEAM_FIRE) && target->alive())
            target->expose_to_element(BEAM_FIRE, 5);
        break;

    case 3:         // considerably less harmless stuff
        you_msg        = "You are blasted with searing flames! "
                             "The searing flames burn away your fire resistance!";
        mon_msg_seen   = "@The_monster@ is blasted with searing flames! "
                             "The searing flames burn away @possessive@ fire resistance!";
        mon_msg_unseen = "A large flame burns hotly for a moment in the "
                             "thin air.";

        if (_ouch(10 + random2avg(39, 2), BEAM_FIRE) && target->alive())
            target->expose_to_element(BEAM_FIRE, 10);
        if (!target->alive())
            break;

        if (target->is_player())
            you.increase_duration(DUR_FIRE_VULN, 15 + random2(11), 50);
        else if (target->is_monster())
                 target->as_monster()->add_ench(mon_enchant(ENCH_FIRE_VULN,
                 0, act_source, 15 + random2(11) * BASELINE_DELAY));
        break;
    }
}

void MiscastEffect::_ice(int severity)
{
    const dungeon_feature_type feat = grd(target->pos());
	
    const string feat_name = (feat == DNGN_FLOOR ? "the " : "") +
        feature_description_at(target->pos(), false, DESC_THE);

    switch (severity)
    {
    case 0:         // just a harmless message
        you_msg      = "You shiver with cold.";
        mon_msg_seen = "@The_monster@ shivers with cold.";
        do_msg();
        break;

    case 1:         // a bit less harmless stuff
        you_msg      = "You are covered in a thin layer of ice.";
        mon_msg_seen = "@The_monster@ is covered in a thin layer of ice.";
        if (target->res_cold() < 0)
        {
            if (!_ouch(4 + random2avg(5, 2)))
                return;
        }
        else
            do_msg();
        if (target->alive())
            target->expose_to_element(BEAM_COLD, 2, false);
        break;

    case 2:         // rather less harmless stuff
        you_msg = "Heat is drained from your body!";
        mon_msg = "Heat is drained from @the_monster@.";
        if (_ouch(5 + random2avg(13, 2), BEAM_COLD) && target->alive())
            target->expose_to_element(BEAM_COLD, 4);
        if(!target->alive())
            break;
        if (target->is_player())
            you.increase_duration(DUR_FROZEN, 4 + random2(7), 50);
        else if (target->is_monster())
        {
            target->as_monster()->add_ench(mon_enchant(ENCH_FROZEN,
            0, act_source, 4 + random2(7) * BASELINE_DELAY));
        }
        break;

    case 3:         // less harmless stuff
        you_msg        = "You are encased in ice!";
        mon_msg_seen   = "@The_monster@ is encased in ice!";
        mon_msg_unseen = "An unseen figure is encased in ice!";

        if (_ouch(9 + random2avg(23, 2), BEAM_ICE) && target->alive())
            target->expose_to_element(BEAM_COLD, 9);
        if (!target->alive())
            break;

        if (target->is_player())
            you.increase_duration(DUR_FROZEN, 8 + random2(15), 50);
        else if (target->is_monster())
        {
            target->as_monster()->add_ench(mon_enchant(ENCH_FROZEN,
            0, act_source, 8 + random2(15) * BASELINE_DELAY));
        }
        break;
    }
}


void MiscastEffect::_earth(int severity)
{
    switch (severity)
    {
    case 0:         // just a harmless message
	    you_msg        = "Motes of dust swirl before your eyes.";
        mon_msg_seen   = "Motes of dust swirl around @the_monster@.";
        mon_msg_unseen = "Motes of dust swirl around in the air.";
        do_msg();
        break;
    case 1:
        you_msg        = "You are blasted with sand!";
        mon_msg_seen   = "@the_monster@ is blasted with sand!";
        mon_msg_unseen = "Sand pours all over something!";
        _ouch(target->apply_ac(random2avg(6, 2) + 6));
        break;


    case 2:         // slightly less harmless stuff
        you_msg        = "Rocks fall onto you out of nowhere!";
        mon_msg_seen   = "Rocks fall onto @the_monster@ out of "
                                 "nowhere!";
        mon_msg_unseen = "Rocks fall out of nowhere!";
        _ouch(target->apply_ac(random2avg(13, 2) + 12));
        break;

    case 3:         // less harmless stuff
        you_msg        = "You are caught in an explosion of flying "
                             "shrapnel!";
        mon_msg_seen   = "@The_monster@ is caught in an explosion of "
                             "flying shrapnel!";
        mon_msg_unseen = "Flying shrapnel explodes from thin air!";

        beam.flavour = BEAM_FRAG;
        beam.glyph   = dchar_glyph(DCHAR_FIRED_BURST);
        beam.damage  = dice_def(3, 18);
        beam.name    = "explosion";
        beam.colour  = CYAN;

        if (one_chance_in(5))
            beam.colour = BROWN;
        if (one_chance_in(5))
            beam.colour = LIGHTCYAN;

        _explosion();
        break;
    }
}

void MiscastEffect::_air(int severity)
{
    switch (severity)
    {
    case 0:         // just a harmless message
        you_msg      = "You feel momentarily weightless.";
        mon_msg_seen = "@The_monster@ bobs in the air for a moment.";
        do_msg();
        break;

    case 1:         // rather less harmless stuff
        you_msg        = "Electricity courses through your body.";
        mon_msg_seen   = "@The_monster@ is jolted!";
        mon_msg_unseen = "Something invisible sparkles with electricity.";
        _ouch(4 + random2avg(9, 2), BEAM_ELECTRICITY);
        break;

    case 2:         // much less harmless stuff
        you_msg        = "You are caught in an explosion of electrical "
                             "discharges!";
        mon_msg_seen   = "@The_monster@ is caught in an explosion of "
                             "electrical discharges!";
        mon_msg_unseen = "Electrical discharges explode from out of "
                             "thin air!";

        beam.flavour = BEAM_ELECTRICITY;
        beam.glyph   = dchar_glyph(DCHAR_FIRED_BURST);
        beam.damage  = dice_def(3, 8);
        beam.name    = "explosion";
        beam.colour  = LIGHTBLUE;
        beam.ex_size = one_chance_in(4) ? 1 : 2;

        _explosion();
        break;

    case 3:         // even less harmless stuff
        if (_create_monster(MONS_BALL_LIGHTNING, 3))
            all_msg = "A ball of electricity appears!";
        do_msg();
        break;
    }
}

void MiscastEffect::_poison(int severity)
{
    switch (severity)
    {
    case 0:         // just a harmless message
        you_msg      = "You feel slightly ill.";
        mon_msg_seen = "@The_monster@ briefly looks sick.";
        do_msg();
        break;

    case 1:         // a bit less harmless stuff
        if (target->res_poison() <= 0)
        {
            you_msg      = "You feel sick.";
            mon_msg_seen = "@The_monster@ looks sick.";
            _do_poison(7 + random2(9));
        }
        do_msg();
            break;

    case 2:         // rather less harmless stuff
        if (target->res_poison() <= 0)
        {
            you_msg      = "You feel very sick.";
            mon_msg_seen = "@The_monster@ looks very sick.";
            _do_poison(14 + random2avg(17, 2));
        }
        do_msg();
        break;

    case 3:         // less harmless stuff
        if (target->res_poison() <= 0)
        {
                you_msg      = "You feel incredibly sick.";
                mon_msg_seen = "@The_monster@ looks incredibly sick.";
                _do_poison(20 + random2avg(35, 2));
        }
        do_msg();
        break;
    }
}

void MiscastEffect::_do_poison(int amount)
{
    if (target->is_player())
        poison_player(amount, cause, "residual poison");
    else
        target->poison(act_source, amount);
}

void MiscastEffect::_zot()
{
    switch (random2(5))
    {
    case 0:
        target->paralyse(act_source, 2 + random2(4), cause);
        break;
    case 1:
        target->petrify(act_source);
        break;
    case 2:
        target->confuse(act_source, 2 + random2(4));
        break;  
    case 3:
        _sleep(2 + random2(4));	
        break;
    case 4:
        target->slow_down(act_source, 10);
        break;
    }
}
