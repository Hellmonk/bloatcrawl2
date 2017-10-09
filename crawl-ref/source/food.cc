/**
 * @file
 * @brief Functions for eating.
**/

#include "AppHdr.h"

#include "food.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include "butcher.h"
#include "chardump.h"
#include "database.h"
#include "delay.h"
#include "env.h"
#include "godabil.h"
#include "godconduct.h"
#include "hints.h"
#include "invent.h"
#include "itemprop.h"
#include "items.h"
#include "item_use.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "misc.h"
#include "mutation.h"
#include "nearby-danger.h"
#include "notes.h"
#include "options.h"
#include "output.h"
#include "religion.h"
#include "rot.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "travel.h"
#include "transform.h"
#include "xom.h"

void make_hungry(int hunger_amount, bool suppress_msg,
                 bool magic)
{
	return;
}

// Must match the order of hunger_state_t enums
static constexpr int hunger_threshold[HS_ENGORGED + 1] =
{
    HUNGER_FAINTING, HUNGER_STARVING, HUNGER_NEAR_STARVING, HUNGER_VERY_HUNGRY,
    HUNGER_HUNGRY, HUNGER_SATIATED, HUNGER_FULL, HUNGER_VERY_FULL,
    HUNGER_ENGORGED
};

void set_hunger(int new_hunger_level, bool suppress_msg)
{
    return;
}

bool you_foodless(bool can_eat)
{
    return you.undead_state() == US_UNDEAD
#if TAG_MAJOR_VERSION == 34
        || you.species == SP_DJINNI && !can_eat
#endif
        ;
}

// [ds] Returns true if something was eaten.
bool eat_food(int slot)
{
    return false;
}

bool eat_item(item_def &food)
{
	return false;
}

// Returns true if an item of basetype FOOD or CORPSES cannot currently
// be eaten (respecting species and mutations set).
bool is_inedible(const item_def &item)
{
    return true;
}

// As we want to avoid autocolouring the entire food selection, this should
// be restricted to the absolute highlights, even though other stuff may
// still be edible or even delicious.
bool is_preferred_food(const item_def &food)
{
    return false;
}

/** Can the player eat this item?
 *
 *  @param food the item (must be a corpse or food item)
 *  @param suppress_msg whether to print why you can't eat it
 *  @param check_hunger whether to check how hungry you are currently
 */
bool can_eat(const item_def &food, bool suppress_msg, bool check_hunger)
{
#define FAIL(msg) { if (!suppress_msg) mpr(msg); return false; }
    ASSERT(food.base_type == OBJ_FOOD || food.base_type == OBJ_CORPSES);

return false;
}

void handle_starvation()
{
    return;
}

int hunger_bars(const int hunger)
{
    return 0;
}

string hunger_cost_string(const int hunger)
{
    return "N/A";
}
