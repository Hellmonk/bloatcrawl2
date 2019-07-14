#pragma once

#include "AppHdr.h"

#include "ability.h"
#include "env.h"
#include "equipment-type.h"
#include "invent.h"
#include "makeitem.h"
#include "mutation.h"
#include "ng-setup.h"
#include "player.h"
#include "potion.h"
#include "skills.h"
#include "star-sign.h"
#include "view.h"

struct star_sign_def
{
    /**
     * "You were born under the sign [name], which gives you [effect]."
     */
    const char* name;
    /**
     * "You were born under the sign [name], which gives you [effect]."
     */
    const char* effect;
    /**
     * Checks if the star sign is valid for this char. This runs during newgame
     * init and almost none of the character is set up, so don't try reading
     * any external game state.
     */
    function<bool (species_type sp, job_type job)> valid;
    function<void ()> apply;
};

static const map<star_sign, star_sign_def> star_signs =
{
    {
        star_sign::conscript,
        {
            "the Conscript",
            "+1 Fighting skill",
            [](species_type sp, job_type job) {
                return true;
            },
            []() {
                const int sklev = you.skill(SK_FIGHTING, 1, true, false, false);
                set_skill_level(SK_FIGHTING, sklev + 1);
                you.redraw_title = true;
                calc_hp(true, false);
            },
        }
    },
    {
        star_sign::thief,
        {
            "the Thief",
            "a Ring of Stealth",
            [](species_type sp, job_type job) {
                return true;
            },
            []() {
                newgame_make_item(OBJ_JEWELLERY, RING_STEALTH);
            },
        }
    },
    {
        star_sign::pilgrim,
        {
            "the Pilgrim",
            "+1 Invocations skill",
            [](species_type sp, job_type job) {
                return job != JOB_BERSERKER && sp != SP_DEMIGOD;
            },
            []() {
                const int sklev = you.skill(SK_INVOCATIONS, 1, true, false, false);
                set_skill_level(SK_INVOCATIONS, sklev + 1);
                you.redraw_title = true;
                calc_hp(true, false);
            },
        }
    },
    {
        star_sign::scout,
        {
            "the Scout",
            "a map of the dungeon",
            [](species_type sp, job_type job) {
                return job != JOB_ABYSSAL_KNIGHT;
            },
            []() {
                if (you.char_class != JOB_ABYSSAL_KNIGHT)
                    magic_mapping(500, 100, true);
            },
        }
    },
    {
        star_sign::apothecary,
        {
            "the Apothecary",
            "a potion of curing",
            [](species_type sp, job_type job) {
                return true;
            },
            []() {
                newgame_make_item(OBJ_POTIONS, POT_CURING);
            },
        }
    },
    {
        star_sign::scholar,
        {
            "the Scholar",
            "a scroll of identify",
            [](species_type sp, job_type job) {
                return true;
            },
            []() {
                newgame_make_item(OBJ_SCROLLS, SCR_IDENTIFY);
            },
        }
    },
    {
        star_sign::smith_weapon,
        {
            "the Smith",
            "+1 weapon enchantment",
            [](species_type sp, job_type job) {
                return you.equip[EQ_WEAPON];
            },
            []() {
                you.inv[you.equip[EQ_WEAPON]].plus++;
            },
        }
    },
    {
        star_sign::traveller,
        {
            "the Traveller",
            "a cloak",
            [](species_type sp, job_type job) {
                return you_can_wear(EQ_CLOAK) == MB_TRUE;
            },
            []() {
                newgame_make_item(OBJ_ARMOUR, ARM_CLOAK);
            },
        }
    },
    {
        star_sign::dj,
        {
            "the DJ",
            "an air horn",
            [](species_type sp, job_type job) {
                return true;
            },
            []() {
                newgame_make_item(OBJ_MISCELLANY, MISC_AIR_HORN);
            },
        }
    },
    {
        star_sign::alex_jones,
        {
            "Alex Jones",
            "berserkitis",
            [](species_type sp, job_type job) {
                return you.undead_modifier == US_ALIVE;
            },
            []() {
                perma_mutate(MUT_BERSERK, 1, "Astrological forces");
            },
        }
    },
};
// ASSERT(star_signs.size() == NUM_STAR_SIGNS); // XXX need a way to make this work, preferably compile-time
