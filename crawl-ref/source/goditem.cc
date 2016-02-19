/**
 * @file
 * @brief Gods' attitude towards items.
**/

#include "AppHdr.h"

#include "goditem.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include "artefact.h"
#include "art-enum.h"
#include "food.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "religion.h"
#include "skills.h"
#include "spl-book.h"
#include "spl-util.h"

static bool _is_bookrod_type(const item_def& item,
                             bool (*matches)(spell_type spell))
{
    if (!item.defined())
        return false;

    // Return false for item_infos of unknown subtype
    // (== NUM_{BOOKS,RODS} in most cases, OBJ_RANDOM for acquirement)
    if (item.sub_type == get_max_subtype(item.base_type)
        || item.sub_type == OBJ_RANDOM)
    {
        return false;
    }

    if (item.base_type == OBJ_RODS)
        return matches(spell_in_rod(static_cast<rod_type>(item.sub_type)));

    if (!item_is_spellbook(item))
        return false;

    // Book matches only if all the spells match
    for (spell_type spell : spells_in_book(item))
    {
        if (!matches(spell))
            return false;
    }
    return true;
}

bool is_holy_item(const item_def& item)
{
    bool retval = false;

    if (is_unrandom_artefact(item))
    {
        const unrandart_entry* entry = get_unrand_entry(item.unrand_idx);

        if (entry->flags & UNRAND_FLAG_HOLY)
            return true;
    }

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        retval = (is_blessed(item)
                  || get_weapon_brand(item) == SPWPN_HOLY_WRATH);
        break;
    case OBJ_SCROLLS:
        retval = (item.sub_type == SCR_HOLY_WORD);
        break;
    default:
        break;
    }

    return retval;
}

bool is_unholy_item(const item_def& item)
{
    bool retval = false;

    if (is_unrandom_artefact(item))
    {
        const unrandart_entry* entry = get_unrand_entry(item.unrand_idx);

        if (entry->flags & UNRAND_FLAG_UNHOLY)
            return true;
    }

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        retval = is_demonic(item);
        break;
    case OBJ_BOOKS:
    case OBJ_RODS:
        retval = _is_bookrod_type(item, is_unholy_spell);
        break;
    case OBJ_MISCELLANY:
        retval = item.sub_type == MISC_HORN_OF_GERYON;
        break;
    default:
        break;
    }

    return retval;
}

bool is_potentially_evil_item(const item_def& item)
{
    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        {
        const int item_brand = get_weapon_brand(item);
        if (item_brand == SPWPN_CHAOS)
            return true;
        }
        break;
    case OBJ_MISSILES:
        {
        const int item_brand = get_ammo_brand(item);
        if (item_brand == SPMSL_CHAOS)
            return true;
        }
        break;
    case OBJ_WANDS:
        if (item.sub_type == WAND_RANDOM_EFFECTS)
            return true;
        break;
    case OBJ_RODS:
        if (item.sub_type == ROD_CLOUDS)
            return true;
        break;
    default:
        break;
    }

    return false;
}

// This is a subset of is_evil_item().
bool is_corpse_violating_item(const item_def& item)
{
    bool retval = false;

    if (is_unrandom_artefact(item))
    {
        const unrandart_entry* entry = get_unrand_entry(item.unrand_idx);

        if (entry->flags & UNRAND_FLAG_CORPSE_VIOLATING)
            return true;
    }

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
    {
        const int item_brand = get_weapon_brand(item);
        retval = (item_brand == SPWPN_REAPING);
        break;
    }
    case OBJ_BOOKS:
    case OBJ_RODS:
        retval = _is_bookrod_type(item, is_corpse_violating_spell);
        break;
    default:
        break;
    }

    return retval;
}

bool is_evil_item(const item_def& item)
{
    if (is_unrandom_artefact(item))
    {
        const unrandart_entry* entry = get_unrand_entry(item.unrand_idx);

        if (entry->flags & UNRAND_FLAG_EVIL)
            return true;
    }

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        {
        const int item_brand = get_weapon_brand(item);
        return item_brand == SPWPN_DRAINING
               || item_brand == SPWPN_PAIN
               || item_brand == SPWPN_VAMPIRISM
               || item_brand == SPWPN_REAPING;
        }
        break;
    case OBJ_POTIONS:
        return is_blood_potion(item);
    case OBJ_SCROLLS:
        return item.sub_type == SCR_TORMENT;
    case OBJ_STAVES:
        return item.sub_type == STAFF_DEATH;
    case OBJ_BOOKS:
    case OBJ_RODS:
        return _is_bookrod_type(item, is_evil_spell);
    case OBJ_MISCELLANY:
        return item.sub_type == MISC_LANTERN_OF_SHADOWS;
    case OBJ_JEWELLERY:
        return item.sub_type == AMU_HARM;
    default:
        return false;
    }
}

bool is_unclean_item(const item_def& item)
{
    if (is_unrandom_artefact(item))
    {
        const unrandart_entry* entry = get_unrand_entry(item.unrand_idx);

        if (entry->flags & UNRAND_FLAG_UNCLEAN)
            return true;
    }

    if (item.has_spells())
        return _is_bookrod_type(item, is_unclean_spell);

    return false;
}

bool is_chaotic_item(const item_def& item)
{
    bool retval = false;

    if (is_unrandom_artefact(item))
    {
        const unrandart_entry* entry = get_unrand_entry(item.unrand_idx);

        if (entry->flags & UNRAND_FLAG_CHAOTIC)
            return true;
    }

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        {
        const int item_brand = get_weapon_brand(item);
        retval = (item_brand == SPWPN_CHAOS);
        }
        break;
    case OBJ_MISSILES:
        {
        const int item_brand = get_ammo_brand(item);
        retval = (item_brand == SPMSL_CHAOS);
        }
        break;
    case OBJ_WANDS:
        retval = (item.sub_type == WAND_POLYMORPH);
        break;
    case OBJ_POTIONS:
        retval = item.sub_type == POT_MUTATION
                 || item.sub_type == POT_BENEFICIAL_MUTATION
                 || item.sub_type == POT_LIGNIFY;
        break;
    case OBJ_BOOKS:
    case OBJ_RODS:
        retval = _is_bookrod_type(item, is_chaotic_spell);
        break;
    case OBJ_MISCELLANY:
        retval = (item.sub_type == MISC_BOX_OF_BEASTS);
        break;
    default:
        break;
    }

    return retval;
}

static bool _is_potentially_hasty_item(const item_def& item)
{
    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        {
        const int item_brand = get_weapon_brand(item);
        if (item_brand == SPWPN_CHAOS)
            return true;
        }
        break;
    case OBJ_MISSILES:
        {
        const int item_brand = get_ammo_brand(item);
        if (item_brand == SPMSL_CHAOS || item_brand == SPMSL_FRENZY)
            return true;
        }
        break;
    case OBJ_WANDS:
        if (item.sub_type == WAND_RANDOM_EFFECTS)
            return true;
        break;
    default:
        break;
    }

    return false;
}

bool is_hasty_item(const item_def& item)
{
    bool retval = false;

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        {
        const int item_brand = get_weapon_brand(item);
        retval = (item_brand == SPWPN_SPEED
                  || item.sub_type == WPN_QUICK_BLADE);
        }
        break;
    case OBJ_ARMOUR:
        {
        const int item_brand = get_armour_ego_type(item);
        retval = (item_brand == SPARM_RUNNING);
        }
        break;
    case OBJ_WANDS:
        retval = (item.sub_type == WAND_HASTING);
        break;
    case OBJ_JEWELLERY:
        retval = (item.sub_type == AMU_RAGE && !is_artefact(item));
        break;
    case OBJ_POTIONS:
        retval = (item.sub_type == POT_HASTE
                  || item.sub_type == POT_BERSERK_RAGE);
        break;
    case OBJ_BOOKS:
    case OBJ_RODS:
        retval = _is_bookrod_type(item, is_hasty_spell);
        break;
    default:
        break;
    }

    return retval;
}

bool is_poisoned_item(const item_def& item)
{
    if (is_unrandom_artefact(item, UNRAND_OLGREB))
        return true;

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        {
        const int item_brand = get_weapon_brand(item);
        if (item_brand == SPWPN_VENOM)
            return true;
        }
        break;
    case OBJ_MISSILES:
        {
        const int item_brand = get_ammo_brand(item);
        if (item_brand == SPMSL_POISONED || item_brand == SPMSL_CURARE)
            return true;
        }
        break;
    case OBJ_STAVES:
        if (item.sub_type == STAFF_POISON)
            return true;
        break;
    default:
        break;
    }

    return false;
}

static bool _is_potentially_fiery_item(const item_def& item)
{
    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        {
        const int item_brand = get_weapon_brand(item);
        if (item_brand == SPWPN_CHAOS)
            return true;
        }
        break;
    case OBJ_WANDS:
        if (item.sub_type == WAND_RANDOM_EFFECTS)
            return true;
        break;
    case OBJ_RODS:
        if (item.sub_type == ROD_CLOUDS)
            return true;
        break;
    default:
        break;
    }

    return false;
}

bool is_fiery_item(const item_def& item)
{
    // Flaming Death is handled through its fire brand.
    if (is_unrandom_artefact(item, UNRAND_HELLFIRE))
        return true;

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        {
        const int item_brand = get_weapon_brand(item);
        if (item_brand == SPWPN_FLAMING)
            return true;
        }
        break;
    case OBJ_MISSILES:
        {
        const int item_brand = get_ammo_brand(item);
        if (item_brand == SPMSL_FLAME)
            return true;
        }
        break;
    case OBJ_WANDS:
        if (item.sub_type == WAND_FLAME)
            return true;
        break;
    case OBJ_SCROLLS:
        if (item.sub_type == SCR_IMMOLATION)
            return true;
        break;
    case OBJ_BOOKS:
    case OBJ_RODS:
        return _is_bookrod_type(item, is_fiery_spell);
        break;
    case OBJ_STAVES:
        if (item.sub_type == STAFF_FIRE)
            return true;
        break;
    case OBJ_MISCELLANY:
        if (item.sub_type == MISC_LAMP_OF_FIRE)
            return true;
        break;
    default:
        break;
    }

    return false;
}

bool is_channeling_item(const item_def& item)
{
    if (is_unrandom_artefact(item, UNRAND_WUCAD_MU))
        return true;

    return item.base_type == OBJ_STAVES && item.sub_type == STAFF_ENERGY
           || item.base_type == OBJ_MISCELLANY
              && item.sub_type == MISC_CRYSTAL_BALL_OF_ENERGY;
}

bool is_unholy_spell(spell_type spell)
{
    unsigned int flags = get_spell_flags(spell);

    return flags & SPFLAG_UNHOLY;
}

bool is_corpse_violating_spell(spell_type spell)
{
    unsigned int flags = get_spell_flags(spell);

    return flags & SPFLAG_CORPSE_VIOLATING;
}

bool is_evil_spell(spell_type spell)
{
    const spschools_type disciplines = get_spell_disciplines(spell);
    unsigned int flags = get_spell_flags(spell);

    return bool(disciplines & SPTYP_NECROMANCY)
           && !bool(flags & SPFLAG_NOT_EVIL);
}

bool is_unclean_spell(spell_type spell)
{
    unsigned int flags = get_spell_flags(spell);

    return bool(flags & SPFLAG_UNCLEAN);
}

bool is_chaotic_spell(spell_type spell)
{
    unsigned int flags = get_spell_flags(spell);

    return bool(flags & SPFLAG_CHAOTIC);
}

bool is_hasty_spell(spell_type spell)
{
    unsigned int flags = get_spell_flags(spell);

    return bool(flags & SPFLAG_HASTY);
}

bool is_fiery_spell(spell_type spell)
{
    const spschools_type disciplines = get_spell_disciplines(spell);

    return bool(disciplines & SPTYP_FIRE);
}

static bool _your_god_hates_spell(spell_type spell)
{
    return god_hates_spell(spell, you.religion);
}

static bool _your_god_hates_rod_spell(spell_type spell)
{
    return god_hates_spell(spell, you.religion, true);
}

static conduct_type good_god_hates_item_handling(const item_def &item)
{
    if (!is_good_god(you.religion)
        || (!is_unholy_item(item) && !is_evil_item(item)))
    {
        return DID_NOTHING;
    }

    if (item_type_known(item) || is_unrandom_artefact(item))
    {
        if (is_evil_item(item))
            return DID_NECROMANCY;
        else
            return DID_UNHOLY;
    }

    if (is_demonic(item))
        return DID_UNHOLY;

    return DID_NOTHING;
}

conduct_type god_hates_item_handling(const item_def &item)
{
    if (good_god_hates_item_handling(item) != DID_NOTHING)
        return good_god_hates_item_handling(item);

    switch (you.religion)
    {
    case GOD_ZIN:
        // Handled here rather than is_forbidden_food() because you can
        // butcher or otherwise desecrate the corpses all you want, just as
        // long as you don't eat the chunks. This check must come before the
        // item_type_known() check because the latter returns false for food
        // (and other item types without identification).
        if (item.is_type(OBJ_FOOD, FOOD_CHUNK) && is_mutagenic(item))
            return DID_DELIBERATE_MUTATING;

        if (!item_type_known(item))
            return DID_NOTHING;

        if (is_unclean_item(item))
            return DID_UNCLEAN;

        if (is_chaotic_item(item))
            return DID_CHAOS;
        break;

    case GOD_SHINING_ONE:
        if (item_type_known(item) && is_poisoned_item(item))
            return DID_POISON;
        if (is_unrandom_artefact(item, UNRAND_CAPTAIN))
            return DID_UNCHIVALRIC_ATTACK;
        break;

    case GOD_YREDELEMNUL:
        if (item_type_known(item) && is_holy_item(item))
            return DID_HOLY;
        break;

    case GOD_TROG:
        if (item_is_spellbook(item))
            return DID_SPELL_MEMORISE;
        if (item.sub_type == BOOK_MANUAL && item_type_known(item)
            && is_harmful_skill((skill_type)item.plus))
        {
            return DID_SPELL_PRACTISE;
        }
        // Only Trog cares about spellbooks vs rods.
        if (item.base_type == OBJ_RODS)
            return DID_NOTHING;
        break;

    case GOD_FEDHAS:
        if (item_type_known(item) && is_corpse_violating_item(item))
            return DID_CORPSE_VIOLATION;
        break;

    case GOD_CHEIBRIADOS:
        if (item_type_known(item) && (_is_potentially_hasty_item(item)
                                      || is_hasty_item(item))
            // Don't need item_type_known for quick blades.
            || item.is_type(OBJ_WEAPONS, WPN_QUICK_BLADE))
        {
            return DID_HASTY;
        }
        break;

    case GOD_DITHMENOS:
        if (item_type_known(item)
            && (_is_potentially_fiery_item(item) || is_fiery_item(item)))
        {
            return DID_FIRE;
        }
        break;

    case GOD_PAKELLAS:
        if (item_type_known(item) && is_channeling_item(item))
            return DID_CHANNEL;
        break;

    default:
        break;
    }

    if (item_type_known(item) && is_potentially_evil_item(item)
        && is_good_god(you.religion))
    {
        return DID_NECROMANCY;
    }

    if (item_type_known(item) && _is_bookrod_type(item,
                                                  item.base_type == OBJ_RODS
                                                  ? _your_god_hates_rod_spell
                                                  : _your_god_hates_spell))
    {
        return NUM_CONDUCTS; // FIXME: Get the specific reason, if it
    }                          // will ever be needed for spellbooks.

    return DID_NOTHING;
}

bool god_hates_item(const item_def &item)
{
    return god_hates_item_handling(item) != DID_NOTHING;
}

bool god_dislikes_spell_type(spell_type spell, god_type god)
{
    if (god_hates_spell(spell, god))
        return true;

    unsigned int flags       = get_spell_flags(spell);
    spschools_type disciplines = get_spell_disciplines(spell);

    switch (god)
    {
    case GOD_SHINING_ONE:
        // TSO probably wouldn't like spells which would put enemies
        // into a state where attacking them would be unchivalrous.
        if (spell == SPELL_CAUSE_FEAR || spell == SPELL_PARALYSE
            || spell == SPELL_CONFUSE || spell == SPELL_MASS_CONFUSION
            || spell == SPELL_HIBERNATION)
        {
            return true;
        }
        break;

    case GOD_XOM:
        // Ideally, Xom would only like spells which have a random
        // effect, are risky to use, or would otherwise amuse him, but
        // that would be a really small number of spells.

        // Neutral, but in an amusing way.
        if (spell == SPELL_INNER_FLAME)
            return false;

        // Xom would probably find these extra boring.
        if (flags & (SPFLAG_HELPFUL | SPFLAG_NEUTRAL | SPFLAG_ESCAPE
                     | SPFLAG_RECOVERY))
        {
            return true;
        }

        // Things are more fun for Xom the less the player knows in
        // advance.
        if (disciplines & SPTYP_DIVINATION)
            return true;
        break;

    case GOD_ELYVILON:
        // A peaceful god of healing wouldn't like combat spells.
        if (disciplines & SPTYP_CONJURATION)
            return true;

        // Also doesn't like battle spells of the non-conjuration type.
        if (flags & SPFLAG_BATTLE)
            return true;
        break;

    default:
        break;
    }

    return false;
}

bool god_dislikes_spell_discipline(spschools_type discipline, god_type god)
{
    if (is_good_god(god) && (discipline & SPTYP_NECROMANCY))
        return true;

    switch (god)
    {
    case GOD_SHINING_ONE:
        return bool(discipline & SPTYP_POISON);

    case GOD_ELYVILON:
        return bool(discipline & (SPTYP_CONJURATION | SPTYP_SUMMONING));

    case GOD_DITHMENOS:
        return bool(discipline & SPTYP_FIRE);

    default:
        break;
    }

    return false;
}
