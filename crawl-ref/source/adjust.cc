/**
 * @file
 * @brief Functions for letting the player adjust letter assignments.
 **/

#include "AppHdr.h"

#include "adjust.h"

#include "ability.h"
#include "libutil.h"
#include "invent.h"
#include "items.h"
#include "macro.h"
#include "message.h"
#include "prompt.h"
#include "spl-util.h"

static void _adjust_spell();
static void _adjust_ability();

void adjust()
{
    mprf(MSGCH_PROMPT, "Adjust (i)tems, (s)pells, or (a)bilities? ");

    const int keyin = toalower(get_ch());

    if (keyin == 'i')
        adjust_item();
    else if (keyin == 's')
        _adjust_spell();
    else if (keyin == 'a')
        _adjust_ability();
    else if (key_is_escape(keyin))
        canned_msg(MSG_OK);
    else
        canned_msg(MSG_HUH);
}

void adjust_item(int from_slot)
{
    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return;
    }

    if (from_slot == -1)
    {
        from_slot = prompt_invent_item("Adjust which item?", MT_INVLIST, -1);
        if (prompt_failed(from_slot))
            return;

        mprf_nocap("%s", you.inv[from_slot].name(DESC_INVENTORY_EQUIP).c_str());
    }

    const int to_slot = prompt_invent_item("Adjust to which letter? ",
                                           MT_INVLIST,
                                           -1,
                                           false,
                                           false);
    if (to_slot == PROMPT_ABORT
        || from_slot == to_slot)
    {
        canned_msg(MSG_OK);
        return;
    }

    swap_inv_slots(from_slot, to_slot, true);
    you.wield_change = true;
    you.redraw_quiver = true;
}

static void _adjust_spell()
{
    if (!you.spell_no)
    {
        canned_msg(MSG_NO_SPELLS);
        return;
    }

    // Select starting slot
    mprf(MSGCH_PROMPT, "Adjust which spell? ");
    int keyin = list_spells(false, false, false, "Adjust which spell?");

    if (!isaalpha(keyin))
    {
        canned_msg(MSG_OK);
        return;
    }

    const int input_1 = keyin;
    const int index_1 = letter_to_index(input_1);
    spell_type spell  = get_spell_by_letter(input_1);

    if (spell == SPELL_NO_SPELL)
    {
        mpr("You don't know that spell.");
        return;
    }

    // Print targeted spell.
    mprf_nocap("%c - %s", keyin, spell_title(spell));

    // Select target slot.
    keyin = 0;
    while (!isaalpha(keyin))
    {
        mprf(MSGCH_PROMPT, "Adjust to which letter? ");
        keyin = get_ch();
        if (key_is_escape(keyin))
        {
            canned_msg(MSG_OK);
            return;
        }
        // FIXME: It would be nice if the user really could select letters
        // without spells from this menu.
        if (keyin == '?' || keyin == '*')
            keyin = list_spells(true, false, false, "Adjust to which letter?");
    }

    const int input_2 = keyin;
    const int index_2 = letter_to_index(keyin);

    if (index_1 == index_2)
    {
        canned_msg(MSG_OK);
        return;
    }

    // swap references in the letter table:
    const int tmp = you.spell_letter_table[index_2];
    you.spell_letter_table[index_2] = you.spell_letter_table[index_1];
    you.spell_letter_table[index_1] = tmp;

    // print out spell in new slot
    mprf_nocap("%c - %s", input_2, spell_title(get_spell_by_letter(input_2)));

    // print out other spell if one was involved (now at input_1)
    spell = get_spell_by_letter(input_1);

    if (spell != SPELL_NO_SPELL)
        mprf_nocap("%c - %s", input_1, spell_title(spell));
}

static void _adjust_ability()
{
    const vector<talent> talents = your_talents(false);

    if (talents.empty())
    {
        mpr("You don't currently have any abilities.");
        return;
    }

    mprf(MSGCH_PROMPT, "Adjust which ability? ");
    int selected = choose_ability_menu(talents);

    // If we couldn't find anything, cancel out.
    if (selected == -1)
    {
        mpr("No such ability.");
        return;
    }

    char old_key = static_cast<char>(talents[selected].hotkey);

    mprf_nocap("%c - %s", old_key, ability_name(talents[selected].which));

    const int index1 = letter_to_index(old_key);

    mprf(MSGCH_PROMPT, "Adjust to which letter?");

    const int keyin = get_ch();

    if (!isaalpha(keyin))
    {
        canned_msg(MSG_HUH);
        return;
    }

    swap_ability_slots(index1, letter_to_index(keyin));
}

void swap_inv_slots(int from_slot, int to_slot, bool verbose)
{
    // Swap items.
    item_def tmp = you.inv[to_slot];
    you.inv[to_slot]   = you.inv[from_slot];
    you.inv[from_slot] = tmp;

    // Slot switching.
    tmp.slot = you.inv[to_slot].slot;
    you.inv[to_slot].slot  = index_to_letter(to_slot);//you.inv[from_slot].slot is 0 when 'from_slot' contains no item.
    you.inv[from_slot].slot = tmp.slot;

    you.inv[from_slot].link = from_slot;
    you.inv[to_slot].link  = to_slot;

    for (int i = 0; i < NUM_EQUIP; i++)
    {
        if (you.equip[i] == from_slot)
            you.equip[i] = to_slot;
        else if (you.equip[i] == to_slot)
            you.equip[i] = from_slot;
    }

    if (verbose)
    {
        mprf_nocap("%s", you.inv[to_slot].name(DESC_INVENTORY_EQUIP).c_str());

        if (you.inv[from_slot].defined())
            mprf_nocap("%s", you.inv[from_slot].name(DESC_INVENTORY_EQUIP).c_str());
    }

    if (to_slot == you.equip[EQ_WEAPON] || from_slot == you.equip[EQ_WEAPON])
    {
        you.wield_change = true;
        you.m_quiver.on_weapon_changed();
    }
    else // just to make sure
        you.redraw_quiver = true;

    // Swap the moved items in last_pickup if they're there.
    if (!you.last_pickup.empty())
    {
        auto &last_pickup = you.last_pickup;
        int to_count = lookup(last_pickup, to_slot, 0);
        int from_count = lookup(last_pickup, from_slot, 0);
        last_pickup.erase(to_slot);
        last_pickup.erase(from_slot);
        if (from_count > 0)
            last_pickup[to_slot] = from_count;
        if (to_count > 0)
            last_pickup[from_slot] = to_count;
    }
}
