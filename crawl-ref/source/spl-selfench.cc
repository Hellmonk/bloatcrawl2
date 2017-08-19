/**
 * @file
 * @brief Self-enchantment spells.
**/

#include "AppHdr.h"

#include "spl-selfench.h"

#include <cmath>

#include "areas.h"
#include "art-enum.h"
#include "butcher.h" // butcher_corpse
#include "coordit.h" // radius_iterator
#include "godconduct.h"
#include "godpassive.h"
#include "hints.h"
#include "items.h" // stack_iterator
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "output.h"
#include "religion.h"
#include "showsymb.h"
#include "spl-transloc.h"
#include "spl-util.h"
#include "spl-wpnench.h"
#include "transform.h"
#include "tilepick.h"
#include "view.h"
#include "viewchar.h"

int allowed_deaths_door_hp()
{
    int hp = calc_spell_power(SPELL_DEATHS_DOOR, true) / 10;

    return max(hp, 1);
}

spret_type cast_deaths_door(int pow, bool fail)
{
    fail_check();
    mpr("You stand defiantly in death's doorway!");
    mprf(MSGCH_SOUND, "You seem to hear sand running through an hourglass...");

    set_hp(allowed_deaths_door_hp());
    deflate_hp(you.hp_max, false);

    you.set_duration(DUR_DEATHS_DOOR, 10 + random2avg(13, 3)
                                       + (random2(pow) / 10));

    if (you.duration[DUR_DEATHS_DOOR] > 25 * BASELINE_DELAY)
        you.duration[DUR_DEATHS_DOOR] = (23 + random2(5)) * BASELINE_DELAY;
    return SPRET_SUCCESS;
}

void remove_ice_armour()
{
    mprf(MSGCH_DURATION, "Your icy armour melts away.");
    you.redraw_armour_class = true;
    you.attribute[ATTR_OZO_ARMOUR] = 0;
}

spret_type ice_armour(int pow, bool fail)
{
    fail_check();

    if (you.form == TRAN_ICE_BEAST)
        mpr("Your icy body feels more resilient.");
    else
        mpr("A film of ice covers your body!");

    if (you.attribute[ATTR_BONE_ARMOUR] > 0)
    {
        you.attribute[ATTR_BONE_ARMOUR] = 0;
        mpr("Your corpse armour falls away.");
    }

    you.attribute[ATTR_OZO_ARMOUR] = 1;
    you.redraw_armour_class = true;

    return SPRET_SUCCESS;
}

/**
 * Iterate over all corpses in LOS and harvest them (unless it's just a test
 * run)
 *
 * @param harvester   The entity planning to do the harvesting.
 * @param dry_run     Whether this is a test run & no corpses should be
 *                    actually destroyed.
 * @return            The total number of corpses (available to be) destroyed.
 */
int harvest_corpses(const actor &harvester, bool dry_run)
{
    int harvested = 0;

    for (radius_iterator ri(harvester.pos(), LOS_NO_TRANS); ri; ++ri)
    {
        for (stack_iterator si(*ri, true); si; ++si)
        {
            item_def &item = *si;
            if (item.base_type != OBJ_CORPSES)
                continue;

            ++harvested;

            if (dry_run)
                continue;

            // don't spam animations
            if (harvested <= 5)
            {
                bolt beam;
                beam.source = *ri;
                beam.target = harvester.pos();
                beam.glyph = get_item_glyph(item).ch;
                beam.colour = item.get_colour();
                beam.range = LOS_RADIUS;
                beam.aimed_at_spot = true;
                beam.item = &item;
                beam.flavour = BEAM_VISUAL;
                beam.draw_delay = 3;
                beam.fire();
                viewwindow();
            }

            destroy_item(item.index());
        }
    }

    return harvested;
}


/**
 * Casts the player spell "Cigotuvi's Embrace", pulling all corpses into LOS
 * around the caster to serve as armour.
 *
 * @param pow   The spellpower at which the spell is being cast.
 * @param fail  Whether the casting failed.
 * @return      SPRET_ABORT if you already have an incompatible buff running,
 *              SPRET_FAIL if fail is true, and SPRET_SUCCESS otherwise.
 */
spret_type corpse_armour(int pow, bool fail)
{
    fail_check();
    you.attribute[ATTR_BONE_ARMOUR] = 1;
    mpr("The dead rush to embrace you!");
    you.redraw_armour_class = true;

    return SPRET_SUCCESS;
}


spret_type missile_prot(int pow, bool fail)
{
    fail_check();
    you.attribute[ATTR_REPEL_MISSILES] = 1;
    mpr("You feel protected from missiles.");
    return SPRET_SUCCESS;
}

spret_type deflection(int pow, bool fail)
{
    fail_check();
    you.attribute[ATTR_DEFLECT_MISSILES] = 1;
    mpr("You feel very safe from missiles.");
    // Replace RMsl, if active.
    if (you.attribute[ATTR_REPEL_MISSILES])
        you.attribute[ATTR_REPEL_MISSILES] = 0;

    return SPRET_SUCCESS;
}

spret_type cast_regen(int pow, bool fail)
{
    fail_check();
    you.attribute[ATTR_SPELL_REGEN] = 1;
    mpr("Your skin crawls.");
    return SPRET_SUCCESS;
}

spret_type cast_revivification(int pow, bool fail)
{
    fail_check();
    mpr("Your body is healed in an amazingly painful way.");

    const int loss = 6 + binomial(9, 8, pow);
    dec_max_hp(loss * you.hp_max / 100);
    set_hp(you.hp_max);

    if (you.duration[DUR_DEATHS_DOOR])
    {
        mprf(MSGCH_DURATION, "Your life is in your own hands once again.");
        // XXX: better cause name?
        paralyse_player("Death's Door abortion", 5 + random2(5));
        confuse_player(10 + random2(10));
        you.duration[DUR_DEATHS_DOOR] = 0;
        you.increase_duration(DUR_EXHAUSTED, roll_dice(1,3));
    }
    return SPRET_SUCCESS;
}

spret_type cast_swiftness(int power, bool fail)
{
    fail_check();

    if (you.in_liquid())
    {
        // Hint that the player won't be faster until they leave the liquid.
        mprf("The %s foams!", you.in_water() ? "water"
                            : you.in_lava()  ? "lava"
                                             : "liquid ground");
    }

    you.set_duration(DUR_SWIFTNESS, 12 + random2(power)/2, 30,
                     "You feel quick.");
    you.attribute[ATTR_SWIFTNESS] = you.duration[DUR_SWIFTNESS];

    return SPRET_SUCCESS;
}

int cast_selective_amnesia(const string &pre_msg)
{
    if (you.spell_no == 0)
    {
        canned_msg(MSG_NO_SPELLS);
        return 0;
    }

    int keyin = 0;
    spell_type spell;
    int slot;

    // Pick a spell to forget.
    mprf(MSGCH_PROMPT, "Forget which spell ([?*] list [ESC] exit)? ");
    keyin = list_spells(false, false, false, "Forget which spell?");
    redraw_screen();

    while (true)
    {
        if (key_is_escape(keyin))
        {
            canned_msg(MSG_OK);
            return -1;
        }

        if (keyin == '?' || keyin == '*')
        {
            keyin = list_spells(false, false, false, "Forget which spell?");
            redraw_screen();
        }

        if (!isaalpha(keyin))
        {
            clear_messages();
            mprf(MSGCH_PROMPT, "Forget which spell ([?*] list [ESC] exit)? ");
            keyin = get_ch();
            continue;
        }

        spell = get_spell_by_letter(keyin);
        slot = get_spell_slot_by_letter(keyin);

        if (spell == SPELL_NO_SPELL)
        {
            mpr("You don't know that spell.");
            mprf(MSGCH_PROMPT, "Forget which spell ([?*] list [ESC] exit)? ");
            keyin = get_ch();
        }
        else
            break;
    }

    if (!pre_msg.empty())
        mpr(pre_msg);

    del_spell_from_memory_by_slot(slot);

    return 1;
}

spret_type cast_infusion(int pow, bool fail)
{
    fail_check();
    you.attribute[ATTR_INFUSION] = 1;
    mpr("You begin infusing your attacks with magical energy");
    return SPRET_SUCCESS;
}

spret_type cast_song_of_slaying(int pow, bool fail)
{
    fail_check();
    you.attribute[ATTR_SONG_OF_SLAYING] = 1;
    mpr("You start singing a song of slaying.");
    return SPRET_SUCCESS;
}

spret_type cast_silence(int pow, bool fail)
{
    fail_check();
    mpr("A profound silence engulfs you.");

    you.increase_duration(DUR_SILENCE, 10 + pow/4 + random2avg(pow/2, 2), 100);
    invalidate_agrid(true);

    if (you.beheld())
        you.update_beholders();

    learned_something_new(HINT_YOU_SILENCE);
    return SPRET_SUCCESS;
}

spret_type cast_liquefaction(int pow, bool fail)
{
    fail_check();
    flash_view_delay(UA_PLAYER, BROWN, 80);
    flash_view_delay(UA_PLAYER, YELLOW, 80);
    flash_view_delay(UA_PLAYER, BROWN, 140);

    mpr("The ground around you becomes liquefied!");

    you.increase_duration(DUR_LIQUEFYING, 10 + random2avg(pow, 2), 100);
    invalidate_agrid(true);
    return SPRET_SUCCESS;
}

spret_type cast_shroud_of_golubria(int pow, bool fail)
{
    fail_check();
    if (you.duration[DUR_SHROUD_OF_GOLUBRIA])
        mpr("You renew your shroud.");
    else
        mpr("Space distorts slightly along a thin shroud covering your body.");

    you.increase_duration(DUR_SHROUD_OF_GOLUBRIA, 7 + roll_dice(2, pow), 50);
    return SPRET_SUCCESS;
}

spret_type cast_transform(int pow, transformation_type which_trans, bool fail)
{
    if (!transform(pow, which_trans, false, true)
        || !check_form_stat_safety(which_trans))
    {
        return SPRET_ABORT;
    }

    fail_check();
    transform(pow, which_trans);
    return SPRET_SUCCESS;
}

spret_type cast_haste(int pow, bool fail)
{
    fail_check();
    you.attribute[ATTR_PERMAHASTE] = 1;
    if(you.duration[DUR_HASTE])
        mpr("You feel as though you will remain fast.");
    else
        mpr("You feel yourself speed up.");
    return SPRET_SUCCESS;
}

spret_type cast_invisibility(int pow, bool fail)
{
    fail_check();
    you.attribute[ATTR_PERMAINVIS] = 1;
    if(you.duration[DUR_INVIS] || you.form == TRAN_SHADOW)
        mpr("You feel more invisible than before");
    else
        mpr("You fade into invisibility.");
    return SPRET_SUCCESS;
}

//return the total amount of mp the player has frozen using a really ugly 
//chain of ifs, probably should, like, use a real data structure
int calculate_frozen_mp()
{
    double frozen_mp = 0;
	if (you.attribute[ATTR_OZO_ARMOUR] > 0)
    {
		frozen_mp += spell_mp_freeze(SPELL_OZOCUBUS_ARMOUR);
	}
	if (you.attribute[ATTR_SPELL_REGEN] > 0)
    {
		frozen_mp += spell_mp_freeze(SPELL_REGENERATION);
	}
	if (you.attribute[ATTR_SONG_OF_SLAYING] > 0)
    {
		frozen_mp += spell_mp_freeze(SPELL_SONG_OF_SLAYING);
	}
	if (you.attribute[ATTR_DEATH_CHANNEL] > 0)
    {
		frozen_mp += spell_mp_freeze(SPELL_DEATH_CHANNEL);
	}
    if (you.attribute[ATTR_DARKNESS] > 0)
    {
		frozen_mp += spell_mp_freeze(SPELL_DARKNESS);
	}
    if (you.attribute[ATTR_ABJURATION_AURA] > 0)
    {
		frozen_mp += spell_mp_freeze(SPELL_AURA_OF_ABJURATION);
	}
    if (you.attribute[ATTR_DEFLECT_MISSILES] > 0)
    {
		frozen_mp += spell_mp_freeze(SPELL_DEFLECT_MISSILES);
	}
    if (you.attribute[ATTR_REPEL_MISSILES] > 0)
    {
		frozen_mp += spell_mp_freeze(SPELL_REPEL_MISSILES);
	}
    if (you.attribute[ATTR_PERMAHASTE] > 0)
    {
		frozen_mp += spell_mp_freeze(SPELL_HASTE);
	}
    if (you.attribute[ATTR_PERMAINVIS] > 0)
    {
		frozen_mp += spell_mp_freeze(SPELL_INVISIBILITY);
	}
    if (you.attribute[ATTR_BONE_ARMOUR] > 0)
    {
		frozen_mp += spell_mp_freeze(SPELL_CIGOTUVIS_EMBRACE);
	}
    if (you.attribute[ATTR_FIRE_SHIELD] > 0)
    {
		frozen_mp += spell_mp_freeze(SPELL_RING_OF_FLAMES);
	}
    if (you.attribute[ATTR_INFUSION] > 0)
    {
		frozen_mp += spell_mp_freeze(SPELL_INFUSION);
	}
    if (you.attribute[ATTR_EXCRUCIATING_WOUNDS] > 0)
    {
		frozen_mp += spell_mp_freeze(SPELL_EXCRUCIATING_WOUNDS);
	}
    if (you.attribute[ATTR_ANIMATE_DEAD] > 0)
    {
		frozen_mp += spell_mp_freeze(SPELL_ANIMATE_DEAD);
	}
    // Forms. Only check for cancelable forms here; uncancellable goodforms shouldn't reserve mp.
    if (you.form && !you.transform_uncancellable)
    {
        switch (you.form)
        {
            case TRAN_SPIDER:
                frozen_mp += spell_mp_freeze(SPELL_SPIDER_FORM);
                break;
            case TRAN_ICE_BEAST:
                frozen_mp += spell_mp_freeze(SPELL_ICE_FORM);
                break;
            case TRAN_BLADE_HANDS:
                frozen_mp += spell_mp_freeze(SPELL_BLADE_HANDS);
                break;
            case TRAN_STATUE:
                frozen_mp += spell_mp_freeze(SPELL_STATUE_FORM);
                break;
            case TRAN_LICH:
                frozen_mp += spell_mp_freeze(SPELL_NECROMUTATION);
                break;
            case TRAN_HYDRA:
                frozen_mp += spell_mp_freeze(SPELL_HYDRA_FORM);
                break;
            case TRAN_DRAGON:
                frozen_mp += spell_mp_freeze(SPELL_DRAGON_FORM);
                break;
            case TRAN_APPENDAGE:
                frozen_mp += spell_mp_freeze(SPELL_BEASTLY_APPENDAGE);
                break;
            default:
                break;
        }
    }
    return (int) frozen_mp;
}

void dispel_permanent_buffs()
{
    bool dispelled = false;
    if(you.attribute[ATTR_OZO_ARMOUR])
    {
	    you.attribute[ATTR_OZO_ARMOUR] = 0;
        dispelled = true;
    }
    if(you.attribute[ATTR_SPELL_REGEN])
    {
	    you.attribute[ATTR_SPELL_REGEN] = 0;
        dispelled = true;
    }
    if(you.attribute[ATTR_SONG_OF_SLAYING])
    {
        you.attribute[ATTR_SONG_OF_SLAYING] = 0;
        dispelled = true;
    }
    if(you.attribute[ATTR_DEATH_CHANNEL])
    {
        you.attribute[ATTR_DEATH_CHANNEL] = 0;
        dispelled = true;
    }
    if(you.attribute[ATTR_DARKNESS])
    {
        you.attribute[ATTR_DARKNESS] = 0;
        update_vision_range();
        dispelled = true;
    }
    if(you.attribute[ATTR_DEFLECT_MISSILES])
    {
        you.attribute[ATTR_DEFLECT_MISSILES] = 0;
        dispelled = true;
    }
    if(you.attribute[ATTR_REPEL_MISSILES])
    {
        you.attribute[ATTR_REPEL_MISSILES] = 0;
        dispelled = true;
    }
    if(you.attribute[ATTR_ABJURATION_AURA])
    {
        you.attribute[ATTR_ABJURATION_AURA] = 0;
        dispelled = true;
    }
    if(you.attribute[ATTR_PERMAHASTE])
    {
        you.attribute[ATTR_PERMAHASTE] = 0;
        dispelled = true;
    }
    if(you.attribute[ATTR_PERMAINVIS])
    {
        you.attribute[ATTR_PERMAINVIS] = 0;
        dispelled = true;
    }
    if(you.attribute[ATTR_BONE_ARMOUR])
    {
        you.attribute[ATTR_BONE_ARMOUR] = 0;
        dispelled = true;
    }
    if(you.attribute[ATTR_FIRE_SHIELD])
    {
        you.attribute[ATTR_FIRE_SHIELD] = 0;
        dispelled = true;
    }
    if(you.attribute[ATTR_INFUSION])
    {
        you.attribute[ATTR_INFUSION] = 0;
        dispelled = true;
    }
    if(you.attribute[ATTR_ANIMATE_DEAD])
    {
        you.attribute[ATTR_ANIMATE_DEAD] = 0;
        dispelled = true;
    }
    if(you.attribute[ATTR_EXCRUCIATING_WOUNDS])
    {
        item_def *wpn = you.weapon();
        end_weapon_brand(*wpn,true);
        dispelled = true;
    }
    you.redraw_armour_class = true;
    unfreeze_mp();
    if (dispelled)
        mpr("Your buffs unravel.");
}
