/**
 * @file
 * @brief Weapon enchantment spells.
**/

#include "AppHdr.h"

#include "spl-wpnench.h"

#include "areas.h"
#include "artefact.h"
#include "god-item.h"
#include "god-passive.h"
#include "item-prop.h"
#include "makeitem.h"
#include "message.h"
#include "player-equip.h"
#include "prompt.h"
#include "religion.h"
#include "shout.h"
#include "spl-miscast.h"

/** End your weapon branding spell.
 *
 * Returns the weapon to the previous brand, and ends DUR_EXCRUCIATING_WOUNDS.
 * @param weapon The item in question (which may have just been unwielded).
 * @param verbose whether to print a message about expiration.
 */
void end_weapon_brand(item_def &weapon, bool verbose)
{
    // The assertion is removed here because I'm not sure enough about this to
    // want to die rather than get a bug report
    if (!you.props.exists(ORIGINAL_BRAND_KEY)) {
        you.props[ORIGINAL_BRAND_KEY] = SPWPN_NORMAL;
        mprf(MSGCH_ERROR, "BUG: tried to end pain brand but you have no original weapon brand");
    }

    set_item_ego_type(weapon, OBJ_WEAPONS, you.props[ORIGINAL_BRAND_KEY]);
    you.props.erase(ORIGINAL_BRAND_KEY);

    if (verbose)
    {
        mprf(MSGCH_DURATION, "%s seems less pained.",
             weapon.name(DESC_YOUR).c_str());
    }

    you.wield_change = true;
    const brand_type real_brand = get_weapon_brand(weapon);
    if (real_brand == SPWPN_ANTIMAGIC)
        calc_mp();
    if (you.weapon() && is_holy_item(weapon) && you.form == transformation::lich)
    {
        mprf(MSGCH_DURATION, "%s falls away!", weapon.name(DESC_YOUR).c_str());
        unequip_item(EQ_WEAPON);
    }
}

void start_weapon_brand(item_def &weapon, bool verbose) {
    noisy(spell_effect_noise(SPELL_EXCRUCIATING_WOUNDS), you.pos());
    if (verbose) {
        mprf("%s %s in agony.", weapon.name(DESC_YOUR).c_str(),
             silenced(you.pos()) ? "writhes" : "shrieks");
    }
    you.props[ORIGINAL_BRAND_KEY] = get_weapon_brand(weapon);
    set_item_ego_type(weapon, OBJ_WEAPONS, SPWPN_PAIN);
    you.wield_change = true;
    if (you.duration[DUR_SPWPN_PROTECTION])
    {
        you.duration[DUR_SPWPN_PROTECTION] = 0;
        you.redraw_armour_class = true;
    }
}

/**
 * Temporarily brand a weapon with pain.
 *
 * @param[in] power         Spellpower.
 * @param[in] fail          Whether you've already failed to cast.
 * @return                  Success, fail, or abort.
 */
spret_type cast_excruciating_wounds(int power, bool fail)
{
    if (you.permabuff[PERMA_EXCRU]) {
        mpr(you.permabuff_could(PERMA_EXCRU) ?
            "You stop infusing your attacks with pain." :
            "You stop attempting to infuse your attacks with pain.");
        you.pb_off(PERMA_EXCRU); return SPRET_PERMACANCEL;
    } else {
        fail_check();
// Suitable weapon checks moved to spl-util.cc
        item_def& weapon = *you.weapon();
        // No other message - start_weapon_brand will produce one anyway
        if (you.duration[DUR_EXCRUCIATING_WOUNDS]) {
            mpr ("You will soon be infusing your attacks with pain.");
        }
        start_weapon_brand(weapon);
        you.pb_on(PERMA_EXCRU); return SPRET_SUCCESS;
    }
}

spret_type cast_confusing_touch(int power, bool fail)
{
    fail_check();
    msg::stream << you.hands_act("begin", "to glow ")
                << (you.duration[DUR_CONFUSING_TOUCH] ? "brighter" : "red")
                << "." << endl;

    you.set_duration(DUR_CONFUSING_TOUCH,
                     max(10 + random2(power) / 5,
                         you.duration[DUR_CONFUSING_TOUCH]),
                     20, nullptr);

    return SPRET_SUCCESS;
}
