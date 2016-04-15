/**
 * @file
 * @brief Monster level-up code.
**/

#include "AppHdr.h"

#include "mon-grow.h"

#include "message.h"
#include "mon-place.h"
#include "monster.h"
#include "mon-util.h"

// Base experience required by a monster to reach HD 1.
const int monster_xp_base       = 15;
// Experience multiplier to determine the experience needed to gain levels.
const int monster_xp_multiplier = 150;
const mons_experience_levels mexplevs;

// Monster growing-up sequences. You can specify a chance to indicate that
// the monster only has a chance of changing type, otherwise the monster
// will grow up when it reaches the HD of the target form.
//
// No special cases are in place: make sure source and target forms are similar.
// If the target form requires special handling of some sort, add the handling
// to level_up_change().

static const monster_level_up mon_grow[] =
{
    monster_level_up(MONS_ORC, MONS_ORC_WARRIOR),
    monster_level_up(MONS_ORC_WARRIOR, MONS_ORC_KNIGHT),
    monster_level_up(MONS_ORC_KNIGHT, MONS_ORC_WARLORD),
    monster_level_up(MONS_ORC_PRIEST, MONS_ORC_HIGH_PRIEST),
    monster_level_up(MONS_ORC_WIZARD, MONS_ORC_SORCERER),

    monster_level_up(MONS_KOBOLD, MONS_BIG_KOBOLD),

    monster_level_up(MONS_UGLY_THING, MONS_VERY_UGLY_THING),

    monster_level_up(MONS_CENTAUR, MONS_CENTAUR_WARRIOR),
    monster_level_up(MONS_YAKTAUR, MONS_YAKTAUR_CAPTAIN),

    monster_level_up(MONS_NAGA, MONS_NAGA_MAGE, 500),
    monster_level_up(MONS_NAGA, MONS_NAGA_WARRIOR),
    monster_level_up(MONS_NAGA_MAGE, MONS_GREATER_NAGA),
    monster_level_up(MONS_NAGA_WARRIOR, MONS_GREATER_NAGA),

    monster_level_up(MONS_DEEP_ELF_MAGE, MONS_DEEP_ELF_DEATH_MAGE, 250),
    monster_level_up(MONS_DEEP_ELF_MAGE, MONS_DEEP_ELF_DEMONOLOGIST, 333),
    monster_level_up(MONS_DEEP_ELF_MAGE, MONS_DEEP_ELF_ANNIHILATOR, 500),
    monster_level_up(MONS_DEEP_ELF_MAGE, MONS_DEEP_ELF_SORCERER),

    monster_level_up(MONS_GNOLL, MONS_GNOLL_SERGEANT),

    monster_level_up(MONS_MERFOLK, MONS_MERFOLK_IMPALER),
    monster_level_up(MONS_SIREN, MONS_MERFOLK_AVATAR),

    // Spriggan -> rider is no good (no mount), -> defender would be an insane
    // power jump, -> druid or -> air mage would require magic training,
    // -> berserker an altar.

    monster_level_up(MONS_DEEP_DWARF, MONS_DEEP_DWARF_SCION),

    monster_level_up(MONS_FAUN, MONS_SATYR),
    monster_level_up(MONS_TENGU, MONS_TENGU_CONJURER, 500),
    monster_level_up(MONS_TENGU, MONS_TENGU_WARRIOR),
    monster_level_up(MONS_TENGU_CONJURER, MONS_TENGU_REAVER),
    monster_level_up(MONS_TENGU_WARRIOR, MONS_TENGU_REAVER),
};

mons_experience_levels::mons_experience_levels()
{
    int experience = monster_xp_base;
    for (int i = 1; i <= MAX_MONS_LEVEL; ++i)
    {
        mexp[i] = experience;

        int delta =
            (monster_xp_base + experience) * 2 * monster_xp_multiplier
            / 500;
        delta =
            min(max(delta, monster_xp_base * monster_xp_multiplier / 100),
                40000);
        experience += delta;
    }
}

static const monster_level_up *_monster_level_up_target(monster_type type,
                                                        int hit_dice)
{
    for (const monster_level_up &mlup : mon_grow)
    {
        if (mlup.before == type)
        {
            const monsterentry *me = get_monster_data(mlup.after);
            if (me->HD == hit_dice && x_chance_in_y(mlup.chance, 1000))
                return &mlup;
        }
    }
    return nullptr;
}

void monster::upgrade_type(monster_type after, bool adjust_hd,
                            bool adjust_hp)
{
    // Ta-da!
    type   = after;

    // Initialise a dummy monster to save work.
    monster dummy;
    dummy.type         = after;
    dummy.base_monster = base_monster;
    dummy.number       = number;
    define_monster(&dummy); // assumes after is not a zombie

    colour = dummy.colour;
    speed  = dummy.speed;
    spells = dummy.spells;
    calc_speed();

    if (adjust_hd)
        set_hit_dice(max(get_experience_level(), dummy.get_hit_dice()));

    if (adjust_hp)
    {
        const int minhp = dummy.max_hit_points;
        if (max_hit_points < minhp)
        {
            hit_points    += minhp - max_hit_points;
            max_hit_points = minhp;
            hit_points     = min(hit_points, max_hit_points);
        }
    }

    // An ugly thing is the only ghost demon monster that can level up.
    // If one has leveled up to a very ugly thing, upgrade it properly.
    if (type == MONS_VERY_UGLY_THING)
        uglything_upgrade();
}

bool monster::level_up_change()
{
    const monster_level_up *lup =
        _monster_level_up_target(type, get_experience_level());
    if (lup)
    {
        upgrade_type(lup->after, false, lup->adjust_hp);
        return true;
    }
    else if (mons_is_base_draconian(type))
    {
        base_monster = type;
        monster_type upgrade = resolve_monster_type(RANDOM_NONBASE_DRACONIAN,
                                                    type);
        if (get_monster_data(upgrade)->HD == get_experience_level())
            upgrade_type(upgrade, false, true);
    }
    return false;
}

bool monster::level_up()
{
    if (get_experience_level() >= MAX_MONS_LEVEL)
        return false;

    set_hit_dice(get_experience_level() + 1);

    // A little maxhp boost.
    if (max_hit_points < 1000)
    {
        int hpboost =
            (get_experience_level() > 3? max_hit_points / 8 : max_hit_points / 4)
            + random2(5);

        // Not less than 3 hp, not more than 25.
        hpboost = min(max(hpboost, 3), 25);

        dprf("%s: HD: %d, maxhp: %d, boost: %d",
             name(DESC_PLAIN).c_str(), get_experience_level(),
             max_hit_points, hpboost);

        max_hit_points += hpboost;
        hit_points     += hpboost;
        hit_points      = min(hit_points, max_hit_points);
    }

    level_up_change();

    return true;
}

void monster::init_experience()
{
    if (experience || !alive())
        return;
    set_hit_dice(max(get_experience_level(), 1));
    experience = mexplevs[min(get_experience_level(), MAX_MONS_LEVEL)];
}

bool monster::gain_exp(int exp, int max_levels_to_gain)
{
    if (!alive())
        return false;

    init_experience();
    if (get_experience_level() >= MAX_MONS_LEVEL)
        return false;

    // Only natural monsters can level-up.
    if (!(holiness() & MH_NATURAL))
        return false;

    // Only monsters that you can gain XP from can level-up.
    if (!mons_class_gives_xp(type) || is_summoned())
        return false;

    // Avoid wrap-around.
    if (experience + exp < experience)
        return false;

    experience += exp;

    const monster mcopy(*this);
    int levels_gained = 0;
    // Monsters can normally gain a maximum of two levels from one kill.
    while (get_experience_level() < MAX_MONS_LEVEL
           && experience >= mexplevs[get_experience_level() + 1]
           && level_up()
           && ++levels_gained < max_levels_to_gain);

    if (levels_gained)
    {
        if (mons_intel(this) >= I_HUMAN)
            simple_monster_message(&mcopy, " looks more experienced.");
        else
            simple_monster_message(&mcopy, " looks stronger.");
    }

    if (get_experience_level() < MAX_MONS_LEVEL
        && experience >= mexplevs[get_experience_level() + 1])
    {
        experience = (mexplevs[get_experience_level()]
                      + mexplevs[get_experience_level() + 1]) / 2;
    }

    // If the monster has leveled up to a monster that will be angered
    // by the player, handle it properly.
    player_angers_monster(this);

    return levels_gained > 0;
}
