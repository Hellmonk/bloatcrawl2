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

/**
 * Start butchering a corpse.
 *
 * @param corpse       Which corpse to butcher.
 * @returns whether the player decided to actually butcher the corpse after all.
 */
static bool _start_butchering(item_def& corpse)
{
	mpr("Butchering corpses is strictly forbidden in this dungeon.");
    return false;
}

void finish_butchering(item_def& corpse, bool bottling)
{
    ASSERT(corpse.base_type == OBJ_CORPSES);
    ASSERT(corpse.sub_type == CORPSE_BODY);
    const bool was_holy = bool(mons_class_holiness(corpse.mon_type) & MH_HOLY);
    const bool was_same_genus = is_player_same_genus(corpse.mon_type);

    if (bottling)
    {
        mpr("You bottle the corpse's blood.");
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
    // Being almost rotten away has 480 badness.
    int badness = 3 * item.freshness;

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
                const bool can_bottle = false;
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
                    to_eat = it;
                    break;

                case 'e':
                    butcher_edible = true;
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

void turn_corpse_into_chunks(item_def &item, bool bloodspatter)
{
    ASSERT(item.base_type == OBJ_CORPSES);
    ASSERT(item.sub_type == CORPSE_BODY);
    const item_def corpse = item;
    const int max_chunks = max_corpse_chunks(item.mon_type);

    item.base_type = OBJ_FOOD;
    item.sub_type  = FOOD_CHUNK;
    item.quantity  = 1 + random2(max_chunks);
    item.quantity  = stepdown_value(item.quantity, 4, 4, 12, 12);

    // Don't mark it as dropped if we are forcing autopickup of chunks.
    if (you.force_autopickup[OBJ_FOOD][FOOD_CHUNK] <= 0)
    {
        item.flags |= ISFLAG_DROPPED;
    }
    else if (you.species != SP_VAMPIRE)
        clear_item_pickup_flags(item);

    // Initialise timer depending on corpse age
    init_perishable_stack(item, item.freshness * ROT_TIME_FACTOR);
}

void butcher_corpse(item_def &item, maybe_bool skeleton, bool chunks)
{
    item_was_destroyed(item);
    if (!mons_skeleton(item.mon_type))
        skeleton = MB_FALSE;
    if (skeleton == MB_TRUE || skeleton == MB_MAYBE && one_chance_in(3))
    {
        turn_corpse_into_skeleton(item);        
    }
    else
    {
        destroy_item(item.index());
    }
}
