/**
 * @file
 * @brief Collects all calls to skills.cc:exercise for
 *            easier changes to the training model.
**/

#include "AppHdr.h"

#include "exercise.h"

#include <algorithm>

#include "ability.h"
#include "fight.h"
#include "godabil.h" // for USKAYAW_DID_DANCE_ACTION
#include "itemprop.h"
#include "skills.h"
#include "spl-util.h"

/// Skill training for when the player casts or miscasts a spell.
void practise_casting(spell_type spell, bool success)
{
    if (success) // ???
        you.props[USKAYAW_DID_DANCE_ACTION] = true;

    // (!success) reduces skill increase for miscast spells
    int workout = 0;

    spschools_type disciplines = get_spell_disciplines(spell);

    int skillcount = count_bits(disciplines);

    if (!success)
        skillcount += 4 + random2(10);

    const int diff = spell_difficulty(spell);

    // Fill all disciplines into a vector, then shuffle the vector, and
    // exercise skills in that random order. That way, first skill don't
    // stay in the queue for a shorter time.
    bool conj = false;
    vector<skill_type> disc;
    for (const auto bit : spschools_type::range())
    {
        if (!spell_typematch(spell, bit))
            continue;

        skill_type skill = spell_type2skill(bit);
        if (skill == SK_CONJURATIONS)
            conj = true;

        disc.push_back(skill);
    }

    // We slow down the training of spells with conjurations.
    if (conj && !x_chance_in_y(skillcount, 4))
        return;

    shuffle_array(disc);

    for (const skill_type skill : disc)
    {
        workout = (random2(1 + diff) / skillcount);

        if (!one_chance_in(5))
            workout++;       // most recently, this was an automatic add {dlb}

        exercise(skill, workout);
    }

    /* ******************************************************************
       Other recent formulae for the above:

       * workout = random2(spell_difficulty(spell_ex)
       * (10 + (spell_difficulty(spell_ex) * 2)) / 10 / spellsy + 1);

       * workout = spell_difficulty(spell_ex)
       * (15 + spell_difficulty(spell_ex)) / 15 / spellsy;

       spellcasting had also been generally exercised at the same time
       ****************************************************************** */
}

/**
 * A best-fit linear approximation of the old item mass equation for armour.
 *
 * @return 42 * EVP - 13, if the player is wearing body armour; else 0.
 */
static int _armour_mass()
{
    const int evp = you.unadjusted_body_armour_penalty();
    return max(0, 42 * evp - 13);
}

static bool _check_train_armour(int amount)
{
    const int mass_base = 100; // old animal skin mass
    if (x_chance_in_y(_armour_mass() - mass_base,
                      you.skill(SK_ARMOUR, 50)))
    {
        exercise(SK_ARMOUR, amount);
        return true;
    }
    return false;
}

static bool _check_train_dodging(int amount)
{
    if (!x_chance_in_y(_armour_mass(), 800))
    {
        exercise(SK_DODGING, amount);
        return true;
    }
    return false;
}

/// Skill training while not being noticed by a monster.
void practise_sneaking(bool invis)
{
    if (!x_chance_in_y(_armour_mass(), 1000)
        && !you.attribute[ATTR_SHADOWS]
            // If invisible, training happens much more rarely.
        && (!invis && one_chance_in(25) || one_chance_in(100)))
    {
        exercise(SK_STEALTH, 1);
    }
}

/// Skill training while doing nothing in particular.
void practise_waiting()
{
    if (one_chance_in(6) && _check_train_armour(1))
        return; // Armour trained in check_train_armour

    if (you.berserk() || you.attribute[ATTR_SHADOWS])
        return;

    // Exercise stealth skill:
    if (!x_chance_in_y(_armour_mass(), 1000)
        // Diminishing returns for stealth training by waiting.
        && you.skills[SK_STEALTH] <= 2 + random2(3)
        && one_chance_in(15))
    {
        exercise(SK_STEALTH, 1);
    }
}

/// Skill training when the player uses the given weapon.
static void _practise_weapon_use(const item_def &weapon)
{
    const skill_type weapon_skill = item_attack_skill(weapon);
    const int mindelay_skill = weapon_min_delay_skill(weapon);
    const int your_skill = you.skills[weapon_skill];
    if (your_skill >= mindelay_skill)
        exercise(weapon_skill, coinflip()); //1/2 past mindelay
    else
    {
        // 3 at 0 skill, 2 at 1/2 mindelay skill, 1 at mindelay
        const int degree
            = 1 + div_rand_round(2 * (mindelay_skill - your_skill),
                                 mindelay_skill);
        exercise(weapon_skill, degree);
    }
}

/// Skill training when the player hits a monster in melee combat.
void practise_hitting(const item_def *weapon)
{
    you.props[USKAYAW_DID_DANCE_ACTION] = true;

    if (!weapon)
    {
        exercise(SK_UNARMED_COMBAT, 1);
        return;
    }
    if (!is_melee_weapon(*weapon))
        return;

    // Slow down the practice of low-damage weapons. XXX: use speed instead?
    const int damage = property(*weapon, PWPN_DAMAGE);
    if (x_chance_in_y(damage, 20))
        _practise_weapon_use(*weapon);
}

/// Skill training when the player shoots at a monster with a ranged weapon.
void practise_launching(const item_def &weapon)
{
    if (coinflip()) // XXX: arbitrary; test and revise
        _practise_weapon_use(weapon);
    you.props[USKAYAW_DID_DANCE_ACTION] = true;
}

/// Skill training when the player throws a projectile at a monster.
void practise_throwing(missile_type mi_type)
{
    if (mi_type == MI_STONE && coinflip())
        return;
    exercise(SK_THROWING, 1);
    you.props[USKAYAW_DID_DANCE_ACTION] = true;
}

/// Skill training when the player stabs an vulnerable monster for extra dam.
void practise_stabbing() { exercise(SK_STEALTH, 1 + random2avg(5, 4)); }

/// Skill training when a monster attacks the player in melee.
void practise_being_attacked()
{
    if (one_chance_in(6))
        _check_train_dodging(1);
}

/// Skill training when a monster hits the player with a melee attack.
void practise_being_hit()
{
    _check_train_armour(coinflip() ? 2 : 1);
}

/// Skill training when the player uses a special ability.
void practise_using_ability(ability_type abil)
{
    const skill_type sk = abil_skill(abil);
    if (sk != SK_NONE)
        exercise(sk, abil_skill_weight(abil));
    you.props[USKAYAW_DID_DANCE_ACTION] = true;
}

/// Skill training when blocking or failing to block attacks with a shield.
void practise_shield_block(bool successful)
{
    const int odds_denom = successful ? 2 : 6;
    if (one_chance_in(odds_denom))
        exercise(SK_SHIELDS, 1);
}

/// Skill training when being hit by a spell or other ranged attack.
void practise_being_shot()
{
    if (one_chance_in(4))
        _check_train_armour(1);
}

/// Skill training when being attacked with a spell or other ranged attack.
void practise_being_shot_at()
{
    if (one_chance_in(4))
        _check_train_dodging(1);
}

/// Skill training when using an evocable item such as a wand.
void practise_evoking(int amount)
{
    // XXX: degree determination is just passed in but should be done here.
    exercise(SK_EVOCATIONS, amount);
    you.props[USKAYAW_DID_DANCE_ACTION] = true;
}

/// Skill training while using one of Nemelex's decks.
void practise_using_deck()
{
    exercise(SK_INVOCATIONS, 1);
    you.props[USKAYAW_DID_DANCE_ACTION] = true;
}
