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

static void _eat_chunk(item_def& food);
static void _heal_from_food(int hp_amt);

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

/**
 * Attempt to reduce the player's hunger.
 *
 * @param satiated_amount       The amount by which to reduce hunger by.
 * @param suppress_msg          Whether to squelch messages about hunger
 *                              decreasing.
 * @param max                   The maximum hunger state which the player may
 *                              reach. If -1, defaults to HUNGER_MAXIMUM.
 */
void lessen_hunger(int satiated_amount, bool suppress_msg, int max)
{
	return;
}

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

// "initial" is true when setting the player's initial hunger state on game
// start or load: in that case it's not really a change, so we suppress the
// state change message and don't identify rings or stimulate Xom.
bool food_change(bool initial)
{
    bool state_changed = false;

    you.hunger = max(you_min_hunger(), you.hunger);
    you.hunger = min(you_max_hunger(), you.hunger);

    // Get new hunger state.
    hunger_state_t newstate = HS_FAINTING;
    while (newstate < HS_ENGORGED && you.hunger > hunger_threshold[newstate])
        newstate = (hunger_state_t)(newstate + 1);

    if (newstate != you.hunger_state)
    {
        state_changed = true;
        you.hunger_state = newstate;
        you.redraw_status_lights = true;

        if (newstate < HS_SATIATED)
            interrupt_activity(AI_HUNGRY);

        if (you.species == SP_VAMPIRE)
        {
            if (you.duration[DUR_BERSERK] > 1 && newstate <= HS_HUNGRY)
            {
                mprf(MSGCH_DURATION, "Your blood-deprived body can't sustain "
                                     "your rage any longer.");
                you.duration[DUR_BERSERK] = 1;
            }

            switch (lifeless_prevents_form())
            {
            case UFR_TOO_DEAD:
                if (you.duration[DUR_TRANSFORMATION] > 2 * BASELINE_DELAY)
                {
                    mprf(MSGCH_DURATION, "Your blood-deprived body can't sustain "
                                         "your transformation much longer.");
                    you.set_duration(DUR_TRANSFORMATION, 2);
                }
                break;
            case UFR_TOO_ALIVE:
                if (you.duration[DUR_TRANSFORMATION] > 5 * BASELINE_DELAY)
                {
                    print_stats();
                    mprf(MSGCH_WARN, "Your blood-filled body can't sustain your "
                                     "transformation much longer.");

                    // Give more time because suddenly stopping flying can be fatal.
                    you.set_duration(DUR_TRANSFORMATION, 5);
                }
                break;
            case UFR_GOOD:
                if (newstate == HS_ENGORGED && is_vampire_feeding()) // Alive
                {
                    print_stats();
                    mpr("You can't stomach any more blood right now.");
                }
            }
        }
    }
    return state_changed;
}

bool eat_item(item_def &food)
{
	return false;
}

// Handle messaging at the end of eating.
// Some food types may not get a message.
static void _finished_eating_message(food_type type)
{
    bool herbivorous = you.get_mutation_level(MUT_HERBIVOROUS) > 0;
    bool carnivorous = you.get_mutation_level(MUT_CARNIVOROUS) > 0;

    if (herbivorous)
    {
        if (food_is_meaty(type))
        {
            mpr("Blech - you need greens!");
            return;
        }
    }
    else
    {
        switch (type)
        {
        case FOOD_MEAT_RATION:
            mpr("That meat ration really hit the spot!");
            return;
        case FOOD_BEEF_JERKY:
            mprf("That beef jerky was %s!",
                 one_chance_in(4) ? "jerk-a-riffic"
                                  : "delicious");
            return;
        default:
            break;
        }
    }

    if (carnivorous)
    {
        if (food_is_veggie(type))
        {
            mpr("Blech - you need meat!");
            return;
        }
    }
    else
    {
        switch (type)
        {
        case FOOD_BREAD_RATION:
            mpr("That bread ration really hit the spot!");
            return;
        case FOOD_FRUIT:
        {
            string taste = getMiscString("eating_fruit");
            if (taste.empty())
                taste = "Eugh, buggy fruit.";
            mpr(taste);
            break;
        }
        default:
            break;
        }
    }

    switch (type)
    {
    case FOOD_ROYAL_JELLY:
        mpr("That royal jelly was delicious!");
        break;
    case FOOD_PIZZA:
    {
        if (!Options.pizzas.empty())
        {
            const string za = Options.pizzas[random2(Options.pizzas.size())];
            mprf("Mmm... %s.", trimmed_string(za).c_str());
            break;
        }

        const string taste = getMiscString("eating_pizza");
        if (taste.empty())
        {
            mpr("Bleh, bug pizza.");
            break;
        }

        mprf("%s", taste.c_str());
        break;
    }
    default:
        break;
    }
}


void finish_eating_item(item_def& food)
{
    if (food.sub_type == FOOD_CHUNK)
        _eat_chunk(food);
    else
    {
        int value = food_value(food);
        ASSERT(value > 0);
        lessen_hunger(value, true);
        _finished_eating_message(static_cast<food_type>(food.sub_type));
    }

    count_action(CACT_EAT, food.sub_type);

    if (is_perishable_stack(food)) // chunks
        remove_oldest_perishable_item(food);
    if (in_inventory(food))
        dec_inv_item_quantity(food.link, 1);
    else
        dec_mitm_item_quantity(food.index(), 1);
}

static const char *_chunk_flavour_phrase(bool likes_chunks)
{
    const char *phrase = "tastes terrible.";

    if (you.species == SP_GHOUL)
        phrase = "tastes great!";
    else if (likes_chunks)
        phrase = "tastes great.";
    else
    {
        const int gourmand = you.duration[DUR_GOURMAND];
        if (gourmand >= GOURMAND_MAX)
        {
            phrase = one_chance_in(1000) ? "tastes like chicken!"
                                         : "tastes great.";
        }
        else if (gourmand > GOURMAND_MAX * 75 / 100)
            phrase = "tastes very good.";
        else if (gourmand > GOURMAND_MAX * 50 / 100)
            phrase = "tastes good.";
        else if (gourmand > GOURMAND_MAX * 25 / 100)
            phrase = "is not very appetising.";
    }

    return phrase;
}

static void _chunk_nutrition_message(int nutrition)
{
    int perc_nutrition = nutrition * 100 / CHUNK_BASE_NUTRITION;
    if (perc_nutrition < 15)
        mpr("That was extremely unsatisfying.");
    else if (perc_nutrition < 35)
        mpr("That was not very filling.");
}

static int _apply_herbivore_nutrition_effects(int nutrition)
{
    int how_herbivorous = you.get_mutation_level(MUT_HERBIVOROUS);

    while (how_herbivorous--)
        nutrition = nutrition * 75 / 100;

    return nutrition;
}

static int _apply_gourmand_nutrition_effects(int nutrition, int gourmand)
{
    return nutrition * (gourmand + GOURMAND_NUTRITION_BASE)
                     / (GOURMAND_MAX + GOURMAND_NUTRITION_BASE);
}

static int _chunk_nutrition(int likes_chunks)
{
    int nutrition = CHUNK_BASE_NUTRITION;

    if (you.hunger_state < HS_SATIATED + likes_chunks)
    {
        return likes_chunks ? nutrition
                            : _apply_herbivore_nutrition_effects(nutrition);
    }

    const int gourmand = you.gourmand() ? you.duration[DUR_GOURMAND] : 0;
    const int effective_nutrition =
        _apply_gourmand_nutrition_effects(nutrition, gourmand);

#ifdef DEBUG_DIAGNOSTICS
    const int epercent = effective_nutrition * 100 / nutrition;
    mprf(MSGCH_DIAGNOSTICS,
            "Gourmand factor: %d, chunk base: %d, effective: %d, %%: %d",
                gourmand, nutrition, effective_nutrition, epercent);
#endif

    return _apply_herbivore_nutrition_effects(effective_nutrition);
}

/**
 * How intelligent was the monster that the given corpse came from?
 *
 * @param   The corpse being examined.
 * @return  The mon_intel_type of the monster that the given corpse was
 *          produced from.
 */
mon_intel_type corpse_intelligence(const item_def &corpse)
{
    // An optimising compiler can assume an enum value is in range, so
    // check the range on the uncast value.
    const bool bad = corpse.orig_monnum < 0
                     || corpse.orig_monnum >= NUM_MONSTERS;
    const monster_type orig_mt = static_cast<monster_type>(corpse.orig_monnum);
    const monster_type type = bad || invalid_monster_type(orig_mt)
                                ? corpse.mon_type
                                : orig_mt;
    return mons_class_intel(type);
}

// Never called directly - chunk_effect values must pass
// through food:determine_chunk_effect() first. {dlb}:
static void _eat_chunk(item_def& food)
{
    const corpse_effect_type chunk_effect = determine_chunk_effect(food);

    int likes_chunks  = player_likes_chunks(true);
    int nutrition     = _chunk_nutrition(likes_chunks);
    bool suppress_msg = false; // do we display the chunk nutrition message?
    bool do_eat       = false;

    switch (chunk_effect)
    {
    case CE_MUTAGEN:
        mpr("This meat tastes really weird.");
        mutate(RANDOM_MUTATION, "mutagenic meat");
        did_god_conduct(DID_DELIBERATE_MUTATING, 10);
        xom_is_stimulated(100);
        break;

    case CE_CLEAN:
    {
        if (you.species == SP_GHOUL)
        {
            suppress_msg = true;
            const int hp_amt = 1 + random2avg(5 + you.experience_level, 3);
            _heal_from_food(hp_amt);
        }

        mprf("This raw flesh %s", _chunk_flavour_phrase(likes_chunks));
        do_eat = true;
        break;
    }

    case CE_NOXIOUS:
    case CE_NOCORPSE:
        mprf(MSGCH_ERROR, "This flesh (%d) tastes buggy!", chunk_effect);
        break;
    }

    if (do_eat)
    {
        dprf("nutrition: %d", nutrition);
        lessen_hunger(nutrition, true);
        if (!suppress_msg)
            _chunk_nutrition_message(nutrition);
    }
}

// Divide full nutrition by duration, so that each turn you get the same
// amount of nutrition. Also, experimentally regenerate 1 hp per feeding turn
// - this is likely too strong.
// feeding is -1 at start, 1 when finishing, and 0 else

// Here are some values for nutrition (quantity * 1000) and duration:
//    max_chunks      quantity    duration
//     1               1           1
//     2               1           1
//     3               1           2
//     4               1           2
//     5               1           2
//     6               2           3
//     7               2           3
//     8               2           3
//     9               2           4
//    10               2           4
//    12               3           5
//    15               3           5
//    20               4           6
//    25               4           6
//    30               5           7

void vampire_nutrition_per_turn(const item_def &corpse, int feeding)
{
    const monster_type mons_type = corpse.mon_type;

    // Duration depends on corpse weight.
    const int max_chunks = max_corpse_chunks(mons_type);
    const int chunk_amount = stepdown_value(1 + max_chunks/3, 6, 6, 12, 12);

    // Add 1 for the artificial extra call at the start of draining.
    const int duration   = 1 + chunk_amount;

    // Use number of potions per corpse to calculate total nutrition, which
    // then gets distributed over the entire duration.
    int food_value = CHUNK_BASE_NUTRITION
                     * num_blood_potions_from_corpse(mons_type);

    bool start_feeding   = false;
    bool end_feeding     = false;

    if (feeding < 0)
        start_feeding = true;
    else if (feeding > 0)
        end_feeding = true;

    if (start_feeding)
    {
        mprf("This %sblood tastes delicious!",
             mons_class_flag(mons_type, M_WARM_BLOOD) ? "warm "
                                                      : "");
    }

    if (!end_feeding)
        lessen_hunger(food_value / duration, !start_feeding);
}

bool is_bad_food(const item_def &food)
{
    return is_mutagenic(food) || is_forbidden_food(food) || is_noxious(food);
}

// Returns true if a food item (or corpse) is mutagenic.
bool is_mutagenic(const item_def &food)
{
    if (food.base_type != OBJ_FOOD && food.base_type != OBJ_CORPSES)
        return false;

    return determine_chunk_effect(food) == CE_MUTAGEN;
}

// Returns true if a food item (or corpse) is totally inedible.
bool is_noxious(const item_def &food)
{
    if (food.base_type != OBJ_FOOD && food.base_type != OBJ_CORPSES)
        return false;

    return determine_chunk_effect(food) == CE_NOXIOUS;
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

/**
 * Is the given food item forbidden to the player by their god?
 *
 * @param food  The food item in question.
 * @return      Whether your god hates you eating it.
 */
bool is_forbidden_food(const item_def &food)
{
    // no food is forbidden to the player who does not yet exist
    if (!crawl_state.need_save)
        return false;

    // Only corpses are only forbidden, now.
    if (food.base_type != OBJ_CORPSES)
        return false;

    // Specific handling for intelligent monsters like Gastronok and Xtahua
    // of a normally unintelligent class.
    if (you_worship(GOD_ZIN) && corpse_intelligence(food) >= I_HUMAN)
        return true;

    return god_hates_eating(you.religion, food.mon_type);
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

/**
 * Determine the 'effective' chunk type for a given piece of carrion (chunk or
 * corpse), for the player.
 * E.g., ghouls treat rotting and mutagenic chunks as normal chunks.
 *
 * @param carrion       The actual chunk or corpse.
 * @return              A chunk type corresponding to the effect eating the
 *                      given item will have on the player.
 */
corpse_effect_type determine_chunk_effect(const item_def &carrion)
{
    return determine_chunk_effect(mons_corpse_effect(carrion.mon_type));
}

/**
 * Determine the 'effective' chunk type for a given input for the player.
 * E.g., ghouls/vampires treat rotting and mutagenic chunks as normal chunks.
 *
 * @param chunktype     The actual chunk type.
 * @return              A chunk type corresponding to the effect eating a chunk
 *                      of the given type will have on the player.
 */
corpse_effect_type determine_chunk_effect(corpse_effect_type chunktype)
{
    switch (chunktype)
    {
    case CE_NOXIOUS:
    case CE_MUTAGEN:
        if (you.species == SP_GHOUL || you.species == SP_VAMPIRE)
            chunktype = CE_CLEAN;
        break;

    default:
        break;
    }

    return chunktype;
}

static void _heal_from_food(int hp_amt)
{
    if (hp_amt > 0)
        inc_hp(hp_amt);

    if (player_rotted())
    {
        mpr("You feel more resilient.");
        unrot_hp(1);
    }

    calc_hp();
    calc_mp();
}

int you_max_hunger()
{
    return HUNGER_DEFAULT;
}

int you_min_hunger()
{
    return HUNGER_DEFAULT;
}

void handle_starvation()
{
    return;
}

static const int hunger_breakpoints[] = { 1, 21, 61, 121, 201, 301, 421 };

int hunger_bars(const int hunger)
{
    return breakpoint_rank(hunger, hunger_breakpoints,
                           ARRAYSZ(hunger_breakpoints));
}

string hunger_cost_string(const int hunger)
{
    return "N/A";
}
