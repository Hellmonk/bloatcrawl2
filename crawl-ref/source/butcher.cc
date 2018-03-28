/**
 * @file
 * @brief Functions for corpse butchering & bottling.
 **/

#include "AppHdr.h"

#include "butcher.h"

#include "bloodspatter.h"
#include "confirm-butcher-type.h"
#include "command.h"
#include "delay.h"
#include "env.h"
#include "food.h"
#include "god-conduct.h"
#include "item-name.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
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

/**
 * Start butchering a corpse.
 *
 * @param corpse       Which corpse to butcher.
 * @returns whether the player decided to actually butcher the corpse after all.
 */
static bool _start_butchering(item_def& corpse)
{
    const bool bottle_blood =
        you.species == SP_VAMPIRE
        && can_bottle_blood_from_corpse(corpse.mon_type);

    if (is_forbidden_food(corpse))
    {
        mprf("It would be a sin to %sbutcher this!",
             bottle_blood ? "bottle or " : "");
        return false;
    }

    // Yes, 0 is correct (no "continue butchering" stage).
    if (bottle_blood)
        start_delay<BottleBloodDelay>(0, corpse);
    else
        start_delay<ButcherDelay>(0, corpse);

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

static int _corpse_quality(const item_def &item)
{
    return 3 * item.freshness;
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
    const char * butcher_verb = bottle_blood ? "bottle" : "butcher";

    vector<item_def *> all_corpses;

    if (specific_corpse)
        all_corpses.push_back(specific_corpse); // doesn't check position
    else
        for (stack_iterator si(you.pos(), true); si; ++si)
            if (si->is_type(OBJ_CORPSES, CORPSE_BODY))
                all_corpses.push_back(&*si);

    if (all_corpses.empty())
    {
        mprf("There isn't anything to %s here.", butcher_verb);
        return;
    }
    if (you_foodless(false))
    {
        mprf("You can't eat.");
        return;
    }
    if (you.get_mutation_level(MUT_HERBIVOROUS) > 0)
    {
        mprf("Sorry, you're a herbivore.");
        return;
    }

    vector<item_def *> edible_corpses;

    // First determine the edible corpses.
    for (item_def * c : all_corpses)
        if (!is_inedible(*c, false))
            edible_corpses.push_back(c);

    const bool seen_inedible = (edible_corpses.size() != all_corpses.size());
    if (edible_corpses.empty())
    {
        if (all_corpses.size() == 1)
        {
            mprf("%s %s.", all_corpses[0]->name(DESC_THE).c_str(),
                bottle_blood ? "doesn't have any blood" : "isn't edible");
        }
        else
        {
            mprf("There isn't anything %s to %s here.",
                bottle_blood ? "with blood" : "edible", butcher_verb);
        }
        return;
    }

    typedef pair<item_def *, int> corpse_quality;
    vector<corpse_quality> corpse_qualities;

    for (item_def *c : edible_corpses)
        if (!is_forbidden_food(*c))
            corpse_qualities.emplace_back(c, _corpse_quality(*c));

    if (corpse_qualities.empty())
    {
        if (edible_corpses.size() == 1)
        {
            mprf("It would be a sin to %s the %s!", butcher_verb,
                                edible_corpses[0]->name(DESC_THE).c_str());
        }
        else
        {
            mprf("It would be a sin to %s any of the %scorpses here!",
                butcher_verb,
                (seen_inedible ? (bottle_blood ? "bloody " : "edible ") : ""));
        }
        return;
    }

    stable_sort(begin(corpse_qualities), end(corpse_qualities),
                                            greater_second<corpse_quality>());

    // Butcher pre-chosen corpse, if found, or if there is only one corpse.
    if (specific_corpse
        || corpse_qualities.size() == 1
                                && Options.confirm_butcher != CONFIRM_ALWAYS
        || Options.confirm_butcher == CONFIRM_NEVER)
    {
        //XXX: this assumes that we're not being called from a delay ourselves.
        if (_start_butchering(*corpse_qualities[0].first))
            handle_delay();
        return;
    }

    // Now pick what you want to butcher. This is only a problem
    // if there are several corpses on the square.
    bool butchered_any = false;
#ifdef TOUCH_UI
    vector<const item_def*> meat;
    for (const corpse_quality &entry : corpse_qualities)
        meat.push_back(entry.first);

    vector<SelItem> selected =
        select_items(meat, bottle_blood ? "Choose a corpse to bottle"
                                        : "Choose a corpse to butcher",
                     false, MT_ANY, _butcher_menu_title);
    redraw_screen();
    for (SelItem sel : selected)
        if (_start_butchering(const_cast<item_def &>(*sel.item)))
            butchered_any = true;
#else
    item_def* to_eat = nullptr;
    bool butcher_all    = false;
    bool all_done = false;
    for (auto &entry : corpse_qualities)
    {
        item_def * const it = entry.first;
        to_eat = nullptr;

        if (butcher_all)
            to_eat = it;
        else
        {
            string corpse_name = it->name(DESC_A);

            // We don't need to check for undead because
            // * Mummies can't eat.
            // * Ghouls relish the bad things.
            // * Vampires won't bottle bad corpses.
            if (you.undead_state() == US_ALIVE)
                corpse_name = menu_colour_item_name(*it, DESC_A);

            bool repeat_prompt = false;
            // Shall we butcher this corpse?
            do
            {
                const bool can_bottle =
                    can_bottle_blood_from_corpse(it->mon_type);
                mprf(MSGCH_PROMPT,
                     "%s %s? [(y)es/(n)o/(a)ll/(q)uit/?]",
                     can_bottle ? "Bottle" : "Butcher",
                     corpse_name.c_str());
                repeat_prompt = false;

                switch (toalower(getchm(KMC_CONFIRM)))
                {
                case 'a':
                case 'c': // legacy
                case 'e': // legacy
                    butcher_all = true;
                // fallthrough
                case 'y':
                case 'd': // ??
                    to_eat = it;
                    break;

                case 'q':
                CASE_ESCAPE
                    canned_msg(MSG_OK);
                    all_done = true;
                    break;

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
            while (repeat_prompt && !all_done);
        }

        if (to_eat && _start_butchering(*to_eat))
            butchered_any = true;
        if (all_done)
            break;
    }

    // No point in displaying this if the player pressed 'a' above.
    if (!to_eat && !butcher_all && !all_done)
        mprf("There isn't anything else to %s here.", butcher_verb);
#endif

    //XXX: this assumes that we're not being called from a delay ourselves.
    // It's not a problem in the case of macros, though, because
    // delay.cc:_push_delay should handle them OK.
    if (butchered_any)
        handle_delay();

    return;
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

void turn_corpse_into_chunks(item_def &item, bool bloodspatter)
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
