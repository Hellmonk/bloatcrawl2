/**
 * @file
 * @brief Functions for corpse butchering & bottling.
 **/

#include "AppHdr.h"

#include "butcher.h"

#include "bloodspatter.h"
#include "command.h"
#include "delay.h"
#include "env.h"
#include "food.h"
#include "godconduct.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "macro.h"
#include "makeitem.h"
#include "message.h"
#include "options.h"
#include "output.h"
#include "prompt.h"
#include "rot.h"
#include "stash.h"
#include "stepdown.h"
#include "stringutil.h"

#ifdef TOUCH_UI
#include "invent.h"
#include "menu.h"
#endif

static bool _should_butcher(const item_def& corpse)
{
    if (is_forbidden_food(corpse)
        && (Options.confirm_butcher == CONFIRM_NEVER
            || !yesno("Desecrating this corpse would be a sin. Continue anyway?",
                      false, 'n', true, false)))
    {
        if (Options.confirm_butcher != CONFIRM_NEVER)
            canned_msg(MSG_OK);
        return false;
    }

    return true;
}

/**
 * Start butchering a corpse.
 *
 * @param corpse       Which corpse to butcher.
 * @returns whether the player decided to actually butcher the corpse after all.
 */
static bool _start_butchering(item_def& corpse)
{
    if (!_should_butcher(corpse))
        return false;

    const bool bottle_blood =
        you.species == SP_VAMPIRE
        && can_bottle_blood_from_corpse(corpse.mon_type);

    const delay_type dtype = bottle_blood ? DELAY_BOTTLE_BLOOD : DELAY_BUTCHER;

    // Yes, 0 is correct (no "continue butchering" stage).
    start_delay(dtype, 0, corpse.index());

    you.turn_is_over = true;
    return true;
}

void finish_butchering(item_def& corpse, bool bottling)
{
    ASSERT(corpse.base_type == OBJ_CORPSES);
    ASSERT(corpse.sub_type == CORPSE_BODY);
    const bool was_holy = bool(mons_class_holiness(corpse.mon_type) & MH_HOLY);
    const bool was_intelligent = corpse_intelligence(corpse) >= I_HUMAN;
    const bool was_same_genus = is_player_same_genus(corpse.mon_type);

    if (bottling)
    {
        mpr("You bottle the corpse's blood.");

        if (mons_skeleton(corpse.mon_type) && one_chance_in(3))
            turn_corpse_into_skeleton_and_blood_potions(corpse);
        else
            turn_corpse_into_blood_potions(corpse);
    }
    else
    {
        mprf("You butcher %s.",
             corpse.name(DESC_THE).c_str());

        butcher_corpse(corpse);

        if (you.berserk()
            && you.berserk_penalty != NO_BERSERK_PENALTY)
        {
            mpr("You enjoyed that.");
            you.berserk_penalty = 0;
        }
    }

    if (was_same_genus)
        did_god_conduct(DID_CANNIBALISM, 2);
    else if (was_holy)
        did_god_conduct(DID_DESECRATE_HOLY_REMAINS, 4);
    else if (was_intelligent)
        did_god_conduct(DID_DESECRATE_SOULED_BEING, 1);

    StashTrack.update_stash(you.pos()); // Stash-track the generated items.
}

#ifdef TOUCH_UI
static string _butcher_menu_title(const Menu *menu, const string &oldt)
{
    return oldt;
}
#endif

static int _corpse_quality(const item_def &item, bool bottle_blood)
{
    const corpse_effect_type ce = determine_chunk_effect(item);
    // Being almost rotten away has 480 badness.
    int badness = 3 * item.freshness;
    if (ce == CE_MUTAGEN)
        badness += 1000;
    else if (ce == CE_NOXIOUS)
        badness += 1000;

    // Bottleable corpses first, unless forbidden
    if (bottle_blood && !can_bottle_blood_from_corpse(item.mon_type))
        badness += 4000;

    if (is_forbidden_food(item))
        badness += 10000;
    return -badness;
}

/**
 * Attempt to butcher a corpse.
 *
 * @param specific_corpse A pointer to the corpse. null means that the player
 *                        chooses what to butcher (unless confirm_butcher =
 *                        never).
 */
void butchery(item_def* specific_corpse)
{
    if (you.visible_igrd(you.pos()) == NON_ITEM)
    {
        mpr("There isn't anything here!");
        return;
    }

    const bool bottle_blood = you.species == SP_VAMPIRE;
    typedef pair<item_def *, int> corpse_quality;
    vector<corpse_quality> corpses;

    // First determine which things there are to butcher.
    for (stack_iterator si(you.pos(), true); si; ++si)
    {
        if (!si->is_type(OBJ_CORPSES, CORPSE_BODY))
            continue;

        corpses.emplace_back(&*si, _corpse_quality(*si, bottle_blood));
    }

    // If the pre-chosen corpse exists, pretend it was the only one.
    if (specific_corpse)
        corpses = { { specific_corpse, 0 } };

    if (corpses.empty())
    {
        mprf("There isn't anything to %sbutcher here.",
             bottle_blood ? "bottle or " : "");
        return;
    }

    stable_sort(begin(corpses), end(corpses), greater_second<corpse_quality>());

    // Butcher pre-chosen corpse, if found, or if there is only one corpse.
    if (specific_corpse
        || corpses.size() == 1 && Options.confirm_butcher != CONFIRM_ALWAYS
        || Options.confirm_butcher == CONFIRM_NEVER)
    {
        if (Options.confirm_butcher == CONFIRM_NEVER
            && !_should_butcher(*corpses[0].first))
        {
            mprf("It would be a sin to %sbutcher this!",
                 bottle_blood ? "bottle or " : "");
            return;
        }

        //XXX: this assumes that we're not being called from a delay ourselves.
        if (_start_butchering(*corpses[0].first))
            handle_delay();
        return;
    }

    // Now pick what you want to butcher. This is only a problem
    // if there are several corpses on the square.
    bool butchered_any = false;
#ifdef TOUCH_UI
    vector<const item_def*> meat;
    for (const corpse_quality &entry : corpses)
        meat.push_back(entry.first);

    vector<SelItem> selected =
        select_items(meat, bottle_blood ? "Choose a corpse to bottle or butcher"
                                        : "Choose a corpse to butcher",
                     false, MT_ANY, _butcher_menu_title);
    redraw_screen();
    for (SelItem sel : selected)
        if (_start_butchering(const_cast<item_def &>(*sel.item)))
            butchered_any = true;
#else
    item_def* to_eat = nullptr;
    bool butcher_all    = false;
    bool butcher_edible = false;
    for (auto &entry : corpses)
    {
        item_def * const it = entry.first;
        to_eat = nullptr;

        if (butcher_all)
            to_eat = it;
        else if (butcher_edible)
        {
            if (is_bad_food(*it))
                continue;

            to_eat = it;
        }
        else
        {
            string corpse_name = it->name(DESC_A);

            // We don't need to check for undead because
            // * Mummies can't eat.
            // * Ghouls relish the bad things.
            // * Vampires won't bottle bad corpses.
            if (you.undead_state() == US_ALIVE)
                corpse_name = get_menu_colour_prefix_tags(*it, DESC_A);

            bool repeat_prompt = false;
            // Shall we butcher this corpse?
            do
            {
                const bool can_bottle =
                    can_bottle_blood_from_corpse(it->mon_type);
                mprf(MSGCH_PROMPT,
                     "%s %s? [(y)es/(c)hoosy/(n)o/(a)ll/(e)dible/(q)uit/?]",
                     can_bottle ? "Bottle" : "Butcher",
                     corpse_name.c_str());
                repeat_prompt = false;

                switch (toalower(getchm(KMC_CONFIRM)))
                {
                case 'a':
                    butcher_all = true;
                // fallthrough
                case 'y':
                case 'd':
                    to_eat = it;
                    break;

                case 'c':
                    // Since corpses are sorted by quality, we assume any
                    // subequent ones will be bad, and quit immediately.
                    if (is_bad_food(*it))
                    {
                        canned_msg(MSG_OK);
                        goto done;
                    }
                    to_eat = it;
                    break;

                case 'e':
                    butcher_edible = true;
                    if (is_bad_food(*it))
                        continue;
                    to_eat = it;
                    break;

                case 'q':
                CASE_ESCAPE
                    canned_msg(MSG_OK);
                    goto done;

                case '?':
                    show_butchering_help();
                    clear_messages();
                    redraw_screen();
                    repeat_prompt = true;
                    break;

                default:
                    break;
                }
            }
            while (repeat_prompt);
        }

        if (to_eat && _start_butchering(*to_eat))
            butchered_any = true;
    }

    // No point in displaying this if the player pressed 'a' above.
    if (!to_eat && !butcher_all)
    {
        mprf("There isn't anything %s to %sbutcher here.",
             butcher_edible ? "edible" : "else",
             bottle_blood ? "bottle or " : "");
    }
#endif

    //XXX: this assumes that we're not being called from a delay ourselves.
    // It's not a problem in the case of macros, though, because
    // delay.cc:_push_delay should handle them OK.
done:
    if (butchered_any)
        handle_delay();

    return;
}


static void _create_monster_hide(const item_def &corpse)
{
    // make certain sources of dragon hides less scummable
    // (kiku's corpse drop, gozag ghoul corpse shops)
    if (corpse.props.exists(MANGLED_CORPSE_KEY))
        return;

    const armour_type type = hide_for_monster(corpse.mon_type);
    ASSERT(type != NUM_ARMOURS);

    int o = items(false, OBJ_ARMOUR, type, 0);
    squash_plusses(o);

    if (o == NON_ITEM)
        return;
    item_def& item = mitm[o];

    do_uncurse_item(item);

    // Automatically identify the created hide.
    set_ident_flags(item, ISFLAG_IDENT_MASK);

    const monster_type montype =
    static_cast<monster_type>(corpse.orig_monnum);
    if (!invalid_monster_type(montype) && mons_is_unique(montype))
        item.inscription = mons_type_name(montype, DESC_PLAIN);

    const coord_def pos = item_pos(corpse);
    if (!pos.origin())
        move_item_to_grid(&o, pos);
}

void maybe_drop_monster_hide(const item_def &corpse)
{
    if (mons_class_leaves_hide(corpse.mon_type) && !one_chance_in(3))
        _create_monster_hide(corpse);
}

/** Skeletonise this corpse.
 *
 *  @param item the corpse to be turned into a skeleton.
 *  @returns whether a valid skeleton could be made.
 */
bool turn_corpse_into_skeleton(item_def &item)
{
    ASSERT(item.base_type == OBJ_CORPSES);
    ASSERT(item.sub_type == CORPSE_BODY);

    // Some monsters' corpses lack the structure to leave skeletons
    // behind.
    if (!mons_skeleton(item.mon_type))
        return false;

    item.sub_type = CORPSE_SKELETON;
    item.freshness = FRESHEST_CORPSE; // reset rotting counter
    item.rnd = 1 + random2(255); // not sure this is necessary, but...
    item.props.erase(FORCED_ITEM_COLOUR_KEY);
    return true;
}

static void _bleed_monster_corpse(const item_def &corpse)
{
    const coord_def pos = item_pos(corpse);
    if (!pos.origin())
    {
        const int max_chunks = max_corpse_chunks(corpse.mon_type);
        bleed_onto_floor(pos, corpse.mon_type, max_chunks, true);
    }
}

void turn_corpse_into_chunks(item_def &item, bool bloodspatter,
                             bool make_hide)
{
    ASSERT(item.base_type == OBJ_CORPSES);
    ASSERT(item.sub_type == CORPSE_BODY);
    const item_def corpse = item;
    const int max_chunks = max_corpse_chunks(item.mon_type);

    if (bloodspatter)
        _bleed_monster_corpse(corpse);

    item.base_type = OBJ_FOOD;
    item.sub_type  = FOOD_CHUNK;
    item.quantity  = 1 + random2(max_chunks);
    item.quantity  = stepdown_value(item.quantity, 4, 4, 12, 12);

    // Don't mark it as dropped if we are forcing autopickup of chunks.
    if (you.force_autopickup[OBJ_FOOD][FOOD_CHUNK] <= 0
        && is_bad_food(item))
    {
        item.flags |= ISFLAG_DROPPED;
    }
    else if (you.species != SP_VAMPIRE)
        clear_item_pickup_flags(item);

    // Initialise timer depending on corpse age
    init_perishable_stack(item, item.freshness * ROT_TIME_FACTOR);

    // Happens after the corpse has been butchered.
    if (make_hide)
        maybe_drop_monster_hide(corpse);
}

static void _turn_corpse_into_skeleton_and_chunks(item_def &item, bool prefer_chunks)
{
    item_def copy = item;

    // Complicated logic, but unless we use the original, both could fail if
    // mitm[] is overstuffed.
    if (prefer_chunks)
    {
        turn_corpse_into_chunks(item);
        turn_corpse_into_skeleton(copy);
    }
    else
    {
        turn_corpse_into_chunks(copy);
        turn_corpse_into_skeleton(item);
    }

    copy_item_to_grid(copy, item_pos(item));
}

void butcher_corpse(item_def &item, maybe_bool skeleton, bool chunks)
{
    item_was_destroyed(item);
    if (!mons_skeleton(item.mon_type))
        skeleton = MB_FALSE;
    if (skeleton == MB_TRUE || skeleton == MB_MAYBE && one_chance_in(3))
    {
        if (chunks)
            _turn_corpse_into_skeleton_and_chunks(item, skeleton != MB_TRUE);
        else
        {
            _bleed_monster_corpse(item);
            maybe_drop_monster_hide(item);
            turn_corpse_into_skeleton(item);
        }
    }
    else
    {
        if (chunks)
            turn_corpse_into_chunks(item);
        else
        {
            _bleed_monster_corpse(item);
            maybe_drop_monster_hide(item);
            destroy_item(item.index());
        }
    }
}

bool can_bottle_blood_from_corpse(monster_type mons_class)
{
    return you.species == SP_VAMPIRE && mons_has_blood(mons_class);
}

int num_blood_potions_from_corpse(monster_type mons_class)
{
    // Max. amount is about one third of the max. amount for chunks.
    const int max_chunks = max_corpse_chunks(mons_class);

    // Max. amount is about one third of the max. amount for chunks.
    int pot_quantity = max_chunks / 3;
    pot_quantity = stepdown_value(pot_quantity, 2, 2, 6, 6);

    if (pot_quantity < 1)
        pot_quantity = 1;

    return pot_quantity;
}

// If autopickup is active, the potions are auto-picked up after creation.
void turn_corpse_into_blood_potions(item_def &item)
{
    ASSERT(item.base_type == OBJ_CORPSES);

    const item_def corpse = item;
    const monster_type mons_class = corpse.mon_type;

    ASSERT(can_bottle_blood_from_corpse(mons_class));

    item.base_type = OBJ_POTIONS;
    item.sub_type  = POT_BLOOD;
    item_colour(item);
    clear_item_pickup_flags(item);
    item.props.clear();

    item.quantity = num_blood_potions_from_corpse(mons_class);

    // Initialise timer depending on corpse age
    init_perishable_stack(item,
                          item.freshness * ROT_TIME_FACTOR + FRESHEST_BLOOD);

    // Happens after the blood has been bottled.
    maybe_drop_monster_hide(corpse);
}

void turn_corpse_into_skeleton_and_blood_potions(item_def &item)
{
    item_def blood_potions = item;

    if (mons_skeleton(item.mon_type))
        turn_corpse_into_skeleton(item);

    int o = get_mitm_slot();
    if (o != NON_ITEM)
    {
        turn_corpse_into_blood_potions(blood_potions);
        copy_item_to_grid(blood_potions, you.pos());
    }
}
