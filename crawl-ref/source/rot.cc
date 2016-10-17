/**
 * @file
 * @brief Functions for blood, chunk, & corpse rot.
 **/

#include "AppHdr.h"

#include "rot.h"

#include <algorithm>

#include "areas.h"
#include "butcher.h"
#include "delay.h"
#include "english.h"
#include "env.h"
#include "godconduct.h"
#include "itemprop.h"
#include "items.h"
#include "player-equip.h"
#include "religion.h"
#include "shopping.h"

#define TIMER_KEY "timer"

static bool _is_chunk(const item_def &item);
static bool _item_needs_rot_check(const item_def &item);
static int _get_initial_stack_longevity(const item_def &stack);

static void _rot_corpse(item_def &it, int mitm_index, int rot_time);
static int _rot_stack(item_def &it, int slot, bool in_inv);

static void _compare_stack_quantity(item_def &stack);

static void _print_chunk_messages(int num_chunks, int num_chunks_gone);

static void _potion_stack_changed_message(string item_name, int num_changed,
                                          int remainder);


/** * Checks if a given item is a stack of chunks.
 *
 * @param item  The stack to check.
 * @return      Whether the given item is a stack of chunks.
 */
static bool _is_chunk(const item_def &item)
{
    return item.is_type(OBJ_FOOD, FOOD_CHUNK);
}


/**
 * Checks if a given item can rot.
 *
 * @param stack  The item to check.
 * @return       Whether the given item is either chunks or blood potions.
 */
bool is_perishable_stack(const item_def &item)
{
    return _is_chunk(item) || is_blood_potion(item);
}

/**
 * Mark a given perishable stack with the remaining longevity of its newest
 * items in the 'freshness' (special) field, for stash tracking purposes.
 *
 * @param stack     The stack to be marked.
 */
static void _update_freshness(item_def &stack)
{
    ASSERT(is_perishable_stack(stack));
    ASSERT(stack.props.exists(TIMER_KEY));

    CrawlVector &timers = stack.props[TIMER_KEY].get_vector();
    if (!timers.size())
        return;

    // newest ones are in front
    ASSERT(timers[0].get_int() >= timers[timers.size() -1].get_int());
    stack.freshness = (timers[0].get_int() - you.elapsed_time)
                       / ROT_TIME_FACTOR;
}

/**
 * The initial longevity (in ROT_TIME) of a stack of blood potions or chunks.
 *
 * @param stack   The stack under consideration.
 * @return        How long the stack should last after creation, before rotting
 * away.
 */
static int _get_initial_stack_longevity(const item_def &stack)
{
    if (is_blood_potion(stack))
        return FRESHEST_BLOOD;

    ASSERT(_is_chunk(stack));
    if (stack.freshness) // legacy chunk
        return stack.freshness * ROT_TIME_FACTOR;
    return FRESHEST_CHUNK;
}

/**
 * Initialise a stack of perishable items with a vector of timers, representing
 * the time at which each item in the stack will rot.
 *
 * If the stack has CORPSE_NEVER_DECAYS marked, we won't actually initialize
 * the stack, since we don't want its rot timer to start yet.
 *
 * @param stack     The stack of items to be initialized.
 * @param age       The age for which the stack will last before rotting.
 * (If -1, will be initialized to a default value.)
 */
void init_perishable_stack(item_def &stack, int age)
{
    ASSERT(is_blood_potion(stack) || _is_chunk(stack));

    CrawlHashTable &props = stack.props;
    const bool never_decay = props.exists(CORPSE_NEVER_DECAYS)
                             && props[CORPSE_NEVER_DECAYS].get_bool();
    if (never_decay)
        return; // don't start rotting yet!

    props.clear(); // sanity measure
    props[TIMER_KEY].new_vector(SV_INT, SFLAG_CONST_TYPE);
    CrawlVector &timer = props[TIMER_KEY].get_vector();

    if (age == -1)
        age = _get_initial_stack_longevity(stack);

    const int max_age = age + you.elapsed_time;

    dprf("initializing perishable stack");

    // For a newly created stack, all potions/chunks use the same timer.
#ifdef DEBUG_BLOOD_POTIONS
    mprf(MSGCH_DIAGNOSTICS,
         "newly created stack of size %d will time out at aut %d",
         stack.quantity, max_age);
#endif
    for (int i = 0; i < stack.quantity; i++)
        timer.push_back(max_age);

    ASSERT(timer.size() == stack.quantity);
    props.assert_validity();

    _update_freshness(stack);
}

// Compare two CrawlStoreValues storing type T.
template<class T>
static bool _storeval_greater(const CrawlStoreValue &a,
                              const CrawlStoreValue &b)
{
    return T(a) > T(b);
}

// Sort a CrawlVector containing only Ts.
template<class T>
static void _sort_cvec(CrawlVector &vec)
{
    sort(vec.begin(), vec.end(), _storeval_greater<T>);
}

/**
 * Check whether an item decays over time. (Corpses, chunks, and blood.)
 *
 * @param item      The item to check.
 * @return          Whether the item changes (rots) over time.
 */
static bool _item_needs_rot_check(const item_def &item)
{
    if (!item.defined())
        return false;

    if (item.props.exists(CORPSE_NEVER_DECAYS))
        return false;

    if (is_perishable_stack(item))
        return true;

    return item.base_type == OBJ_CORPSES
           && item.sub_type <= CORPSE_SKELETON; // XXX: is this needed?
}

/**
 * Rot a corpse or skeleton lying on the floor.
 *
 * @param it            The corpse or skeleton to rot.
 * @param mitm_index    The slot of the corpse in the floor item array.
 * @param rot_time      The amount of time to rot the corpse for.
 */
static void _rot_corpse(item_def &it, int mitm_index, int rot_time)
{
    ASSERT(it.base_type == OBJ_CORPSES);
    ASSERT(!it.props.exists(CORPSE_NEVER_DECAYS));

    it.freshness -= rot_time;
    if (it.freshness > 0 || is_being_butchered(it))
        return;

    if (it.sub_type == CORPSE_SKELETON || !mons_skeleton(it.mon_type))
    {
        item_was_destroyed(it);
        destroy_item(mitm_index);
    }
    else
    {
        turn_corpse_into_skeleton(it);
        const int piety = x_chance_in_y(2, 5) ? 2 : 1; // match fungal_bloom()
        did_god_conduct(DID_ROT_CARRION, piety);
    }
}

/**
 * Ensure that a stack of blood potions or chunks has one timer per item in the
 * stack.
 *
 * @param stack         The stack to be potentially initialized.
 * @param timer_size    The # of timers the stack's current props have.
 */
static void _compare_stack_quantity(item_def &stack)
{
    CrawlVector &stack_timer = stack.props[TIMER_KEY].get_vector();
    const int timer_size = stack_timer.size();
    if (timer_size != stack.quantity)
    {
        mprf(MSGCH_WARN,
             "ERROR: stack quantity (%d) doesn't match timer (%d)",
             stack.quantity, timer_size);

        // sanity measure; sync stack/timer size

        // more items than timers
        const int default_timer = _get_initial_stack_longevity(stack)
                                  + you.elapsed_time;
        while (stack.quantity > stack_timer.size())
            stack_timer.push_back(default_timer);
        // more timers than items
        while (stack_timer.size() > stack.quantity)
            stack_timer.pop_back();

        ASSERT(stack.quantity == stack_timer.size());
    }
}

/**
 * Rot a stack of chunks or blood potions.
 *
 * @param it        The stack to rot.
 * @param inv_slot  The slot the item holds. (In mitm or inv.)
 * @param in_inv    Whether the item is in the player's inventory.
 * @return          The number of items rotted away completely.
 */
static int _rot_stack(item_def &it, int slot, bool in_inv)
{
    ASSERT(it.defined());
    ASSERT(is_perishable_stack(it));
    if (!it.props.exists(TIMER_KEY))
        init_perishable_stack(it);

    // we should have already checked that this isn't never_decay
    // so init should've gone through
    ASSERT(it.props.exists(TIMER_KEY));

    CrawlVector &stack_timer = it.props[TIMER_KEY].get_vector();
    _compare_stack_quantity(it);
    ASSERT(!stack_timer.empty());

    _update_freshness(it); // for external consumption

    // after initializing everything, skip the actual decay if we're eating
    // this stack - this will preserve it a little longer but that's ok.
    if (current_delay() && current_delay()->is_being_used(&it, OPER_EAT))
        return 0;

    int destroyed_count = 0;    // # of items decayed away entirely
    // will be filled in ascending (reversed) order.

    // iter from last to first; we're sorted descending, so it's guaranteed
    // that if the rearmost timer hasn't been reached yet, none of the ones
    // earlier in the vector will have been, either.
    while (stack_timer.size())
    {
        // the time at which the item in the stack will rot. (in aut.)
        const int rot_away_time = stack_timer[stack_timer.size() - 1];
        if (rot_away_time > you.elapsed_time)
            break;

        stack_timer.pop_back();
        destroyed_count++;
    }

    if (!destroyed_count)
        return 0; // Nothing to be done.

    dprf("%d items rotted away (timer size %d)",
         destroyed_count, stack_timer.size());

    if (in_inv)
    {
        // just in case
        // XXX: move this to the appropriate place(s)
        you.wield_change  = true;
        you.redraw_quiver = true;
    }

    bool all_gone;
    if (in_inv)
        all_gone = dec_inv_item_quantity(slot, destroyed_count);
    else
        all_gone = dec_mitm_item_quantity(slot, destroyed_count);

    if (!all_gone)
    {
        _compare_stack_quantity(it);
        ASSERT(!stack_timer.empty());
    }

    return destroyed_count;
}

/**
 * Decay items on the floor: corpses, chunks, and Gozag gold auras.
 *
 * @param elapsedTime   The amount of time to rot the corpses for.
 */
void rot_floor_items(int elapsedTime)
{
    if (elapsedTime <= 0)
        return;

    const int rot_time = elapsedTime / ROT_TIME_FACTOR;

    for (int mitm_index = 0; mitm_index < MAX_ITEMS; ++mitm_index)
    {
        item_def &it = mitm[mitm_index];

        if (is_shop_item(it) || !_item_needs_rot_check(it))
            continue;

        if (it.base_type == OBJ_CORPSES)
            _rot_corpse(it, mitm_index, rot_time);
        else
            _rot_stack(it, mitm_index, false);
    }
}

/**
 * Rot chunks & blood in the player's inventory.
 *
 * @param time_delta    The amount of time to rot for.
 */
void rot_inventory_food(int time_delta)
{
    int num_chunks         = 0;
    int num_chunks_gone    = 0;

    for (int i = 0; i < ENDOFPACK; i++)
    {
        item_def &item(you.inv[i]);

        if (item.quantity < 1 || !_item_needs_rot_check(item))
            continue;

#if TAG_MAJOR_VERSION == 34
        // cleanup
        if (item.base_type == OBJ_CORPSES)
        {
            if (you.equip[EQ_WEAPON] == i)
                unwield_item();

            item_was_destroyed(item);
            destroy_item(item);
        }
#endif

        const int initial_quantity = item.quantity;
        const string item_name = item.name(DESC_PLAIN, false);
        const bool is_chunk = _is_chunk(item);

        if (is_chunk)
            num_chunks += item.quantity;
        else
            ASSERT(is_blood_potion(item));

        const int rotted_away_count = _rot_stack(item, i, true);
        if (is_chunk)
            num_chunks_gone += rotted_away_count;
        else if (rotted_away_count)
        {
            _potion_stack_changed_message(item_name, rotted_away_count,
                                          initial_quantity);
        }
    }

    _print_chunk_messages(num_chunks, num_chunks_gone);
}

// XXX: unify this with blood messaging somehow?
static void _print_chunk_messages(int num_chunks, int num_chunks_gone)
{
    if (num_chunks_gone > 0)
    {
        mprf(MSGCH_ROTTEN_MEAT,
             "%s of the chunks of flesh in your inventory have rotted away.",
             num_chunks_gone == num_chunks ? "All" : "Some");
    }
}

// Prints messages for blood potions coagulating or rotting in inventory.
static void _potion_stack_changed_message(string item_name, int num_changed,
                                          int initial_quantity)
{
    ASSERT(num_changed > 0);

    mprf(MSGCH_ROTTEN_MEAT, "%s %s rot%s away.",
         get_desc_quantity(num_changed, initial_quantity).c_str(),
         item_name.c_str(),
         num_changed == 1 ? "s" : "");
}

// Removes the oldest timer of a stack of blood potions.
// Mostly used for (q)uaff and (f)ire.
int remove_oldest_perishable_item(item_def &stack)
{
    ASSERT(stack.defined());
    ASSERT(is_perishable_stack(stack));

    CrawlHashTable &props = stack.props;
    if (props.exists(CORPSE_NEVER_DECAYS))
        return -1; // it's not rotting, so we don't care about the rest

    if (!props.exists(TIMER_KEY))
        init_perishable_stack(stack);
    ASSERT(props.exists(TIMER_KEY));
    CrawlVector &timer = props[TIMER_KEY].get_vector();
    if (timer.empty())
    {
        dprf("Malformed stack; trying to pop from an empty timer list!");
        return -1;
    }

    // Assuming already sorted, and first (oldest) potion valid.
    const int val = timer[timer.size() - 1].get_int();
    timer.pop_back();

    dprf("Removed oldest item: %d timers, stack size %d",
         timer.size(), stack.quantity);

    _update_freshness(stack); // for external consumption

    // The quantity will be decreased elsewhere.
    return val;
}

// Used whenever copies of blood potions have to be cleaned up.
void remove_newest_perishable_item(item_def &stack, int quant)
{
    ASSERT(stack.defined());
    ASSERT(is_perishable_stack(stack));

    CrawlHashTable &props = stack.props;
    if (props.exists(CORPSE_NEVER_DECAYS))
        return; // it's not rotting, so we don't care about the rest

    if (!props.exists(TIMER_KEY))
        init_perishable_stack(stack);
    ASSERT(props.exists(TIMER_KEY));
    CrawlVector &timer = props[TIMER_KEY].get_vector();
    ASSERT(!timer.empty());

    if (quant == -1)
        quant = timer.size() - stack.quantity;

    // Overwrite newest potions with oldest ones.
    int repeats = stack.quantity;
    if (repeats > quant)
        repeats = quant;

    for (int i = 0; i < repeats; i++)
    {
        timer[i] = timer[timer.size() - 1];
        timer.pop_back();
    }

    // Now remove remaining oldest potions...
    repeats = quant - repeats;
    for (int i = 0; i < repeats; i++)
        timer.pop_back();

    // ... and re-sort.
    _sort_cvec<int>(timer);

    _update_freshness(stack); // for external consumption

    dprf("Removed newest item: %d timers, stack size %d",
         timer.size(), stack.quantity);
}

void merge_perishable_stacks(const item_def &source, item_def &dest, int quant)
{
    if (!source.defined() || !dest.defined())
        return;

    ASSERT_RANGE(quant, 1, source.quantity + 1);
    ASSERT(is_perishable_stack(source));
    ASSERT(is_perishable_stack(dest));
    ASSERT(_is_chunk(source) == _is_chunk(dest));

    const CrawlHashTable &props = source.props;
    CrawlHashTable &props2 = dest.props;
    if (props2.exists(CORPSE_NEVER_DECAYS))
    {
        // if both are unrotting, there's no timers to set.
        if (props.exists(CORPSE_NEVER_DECAYS))
            return;

        // otherwise, the destination should start rotting after rotting chunks
        // are merged in (this should never happen anyway)
        props2.erase(CORPSE_NEVER_DECAYS);
    }

    if (!props2.exists(TIMER_KEY))
        init_perishable_stack(dest);
    ASSERT(props2.exists(TIMER_KEY));
    CrawlVector &timer2 = props2[TIMER_KEY].get_vector();

    dprf("origin qt: %d, taking %d, putting into stack of size %d with initial timer count %d", source.quantity, quant, dest.quantity, timer2.size());

    ASSERT(timer2.size() == dest.quantity);

    const int default_timer = _get_initial_stack_longevity(source)
                              + you.elapsed_time;
    // Update timer2
    for (int i = 0; i < quant; i++)
    {
        const int timer_index = source.quantity - 1 - i;
        int timer_value = default_timer;

        if (props.exists(TIMER_KEY))
        {
            const CrawlVector &timer = props[TIMER_KEY].get_vector();
            if (timer.size() > timer_index)
                timer_value = timer[timer_index].get_int();
            else if (timer.size())
            {
                dprf("Source stack has truncated timer; using first elem");
                timer_value = timer[0].get_int();
            }
            else
                dprf("Source stack has empty timer; using 'fresh' age");
        }

        timer2.push_back(timer_value);
    }

    dprf("Eventual timer size: %d", timer2.size());

    ASSERT(timer2.size() == dest.quantity + quant);

    // Re-sort timer.
    _sort_cvec<int>(timer2);

    _update_freshness(dest); // for external consumption
}
