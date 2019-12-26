/**
 * @file
 * @brief Initializing non-player-related parts of a new game.
**/
/* TODO: 'you' shouldn't occur here.
 *       Some of these might fit better elsewhere.
 */

#include "AppHdr.h"

#include "ng-init.h"

#include "branch.h"
#include "describe.h"
#include "dungeon.h"
#include "end.h"
#include "item-name.h"
#include "libutil.h"
#include "maps.h"
#include "random.h"
#include "religion.h"
#include "state.h"
#include "stringutil.h"
#include "unicode.h"

#ifdef DEBUG_DIAGNOSTICS
#define DEBUG_TEMPLES
#endif

static uint8_t _random_potion_description()
{
    int desc;

    desc = random2(PDQ_NQUALS * PDC_NCOLOURS);

    if (coinflip())
        desc %= PDC_NCOLOURS;

    // nature and colour correspond to primary and secondary in
    // item-name.cc.

#if TAG_MAJOR_VERSION == 34
    if (PCOLOUR(desc) == PDC_CLEAR) // only water can be clear, re-roll
        return _random_potion_description();
#endif

    return desc;
}

// Determine starting depths of branches.
void initialise_branch_depths()
{
    root_branch = BRANCH_DUNGEON;

    // XXX: Should this go elsewhere?
    branch_bribe.init(0);

    for (branch_iterator it; it; ++it)
        brentry[it->id].clear();

    if (crawl_state.game_is_sprint())
    {
        brdepth.init(-1);
        brdepth[BRANCH_DUNGEON] = 1;
        return;
    }

    for (int branch = 0; branch < NUM_BRANCHES; ++branch)
    {
        const Branch *b = &branches[branch];
        ASSERT(b->id == branch);
    }

    for (branch_iterator it; it; ++it)
    {
        if (!branch_is_unfinished(it->id) && it->parent_branch != NUM_BRANCHES)
        {
            brentry[it->id] = level_id(it->parent_branch,
                                       random_range(it->mindepth,
                                                    it->maxdepth));
        }
    }

    // You will get one of Shoals/Swamp and one of Spider/Snake.
    // This way you get one "water" branch and one "poison" branch.
    vector<branch_type> disabled_branch;
    disabled_branch.push_back(random_choose(BRANCH_SWAMP, BRANCH_SHOALS));
    disabled_branch.push_back(random_choose(BRANCH_SNAKE, BRANCH_SPIDER));

    for (branch_type disabled : disabled_branch)
    {
        dprf("Disabling branch: %s", branches[disabled].shortname);
        brentry[disabled].clear();
    }

    for (branch_iterator it; it; ++it)
        brdepth[it->id] = it->numlevels;
}

#define MAX_OVERFLOW_LEVEL 9

static void _use_overflow_temple(vector<god_type> temple_gods)
{
    CrawlVector &overflow_temples
        = you.props[OVERFLOW_TEMPLES_KEY].get_vector();

    const unsigned int level = random_range(2, MAX_OVERFLOW_LEVEL);

    // List of overflow temples on this level.
    CrawlVector &level_temples = overflow_temples[level - 1].get_vector();

    CrawlHashTable temple;

    CrawlVector &gods = temple[TEMPLE_GODS_KEY].new_vector(SV_BYTE);

    for (unsigned int i = 0; i < temple_gods.size(); i++)
        gods.push_back((char) temple_gods[i]);

    level_temples.push_back(temple);
}

// Determine which altars go into the Ecumenical Temple, which go into
// overflow temples, and on what level the overflow temples are.
void initialise_temples()
{
    //////////////////////////////////////////
    // First determine main temple map to use.
    level_id ecumenical(BRANCH_TEMPLE, 1);

    map_def *main_temple = nullptr;
    for (int i = 0; i < 10; i++)
    {
        int altar_count = 0;

        main_temple
            = const_cast<map_def*>(random_map_for_place(ecumenical, false));

        if (main_temple == nullptr)
            end(1, false, "No temples?!");

        if (main_temple->has_tag("temple_variable"))
        {
            vector<int> sizes;
            for (const auto &tag : main_temple->get_tags())
            {
                if (starts_with(tag, "temple_altars_"))
                {
                    sizes.push_back(
                        atoi(tag_without_prefix(tag,
                                                "temple_altars_").c_str()));
                }
            }
            if (sizes.empty())
            {
                mprf(MSGCH_ERROR,
                     "Temple %s set as variable but has no sizes.",
                     main_temple->name.c_str());
                main_temple = nullptr;
                continue;
            }
            altar_count =
                you.props[TEMPLE_SIZE_KEY].get_int() =
                    sizes[random2(sizes.size())];
        }

        dgn_map_parameters mp(make_stringf("temple_altars_%d", altar_count));

        // Without all this find_glyph() returns 0.
        string err;
        main_temple->load();
        main_temple->reinit();
        err = main_temple->run_lua(true);

        if (!err.empty())
        {
            mprf(MSGCH_ERROR, "Temple %s: %s", main_temple->name.c_str(),
                 err.c_str());
            main_temple = nullptr;
            you.props.erase(TEMPLE_SIZE_KEY);
            continue;
        }

        main_temple->fixup();
        err = main_temple->resolve();

        if (!err.empty())
        {
            mprf(MSGCH_ERROR, "Temple %s: %s", main_temple->name.c_str(),
                 err.c_str());
            main_temple = nullptr;
            you.props.erase(TEMPLE_SIZE_KEY);
            continue;
        }
        break;
    }

    if (main_temple == nullptr)
        end(1, false, "No valid temples.");

    you.props[TEMPLE_MAP_KEY] = main_temple->name;

    const vector<coord_def> altar_coords
        = main_temple->find_glyph('B');
    const unsigned int main_temple_size = altar_coords.size();

    if (main_temple_size == 0)
    {
        end(1, false, "Main temple '%s' has no altars",
            main_temple->name.c_str());
    }

#ifdef DEBUG_TEMPLES
    mprf(MSGCH_DIAGNOSTICS, "Chose main temple %s, size %u",
         main_temple->name.c_str(), main_temple_size);
#endif

    ///////////////////////////////////
    // Now set up the overflow temples.

    vector<god_type> god_list = temple_god_list();
    shuffle_array(god_list);

    vector<god_type> overflow_gods;

    while (god_list.size() > main_temple_size)
    {
        overflow_gods.push_back(god_list.back());
        god_list.pop_back();
    }

#ifdef DEBUG_TEMPLES
    mprf(MSGCH_DIAGNOSTICS, "%u overflow altars", (unsigned int)overflow_gods.size());
#endif

    you.props.erase(TEMPLE_GODS_KEY);      // shouldn't be set normally, but
    you.props.erase(OVERFLOW_TEMPLES_KEY); // may be in tests
    CrawlVector &temple_gods
        = you.props[TEMPLE_GODS_KEY].new_vector(SV_BYTE);

    for (unsigned int i = 0; i < god_list.size(); i++)
        temple_gods.push_back((char) god_list[i]);

    CrawlVector &overflow_temples
        = you.props[OVERFLOW_TEMPLES_KEY].new_vector(SV_VEC);
    overflow_temples.resize(MAX_OVERFLOW_LEVEL);

    // Count god overflow temple weights.
    int overflow_weights[NUM_GODS + 1];
    overflow_weights[0] = 0;

    for (unsigned int i = 1; i < NUM_GODS; i++)
    {
        string mapname = make_stringf("temple_overflow_generic_%d", i);
        mapref_vector maps = find_maps_for_tag(mapname);
        if (!maps.empty())
        {
            int chance = 0;
            for (auto map : maps)
            {
                // XXX: this should handle level depth better
                chance += map->weight(level_id(BRANCH_DUNGEON,
                                               MAX_OVERFLOW_LEVEL));
            }
            overflow_weights[i] = chance;
        }
        else
            overflow_weights[i] = 0;
    }

    // Check for temple_overflow vaults that specify certain gods.
    mapref_vector maps;
    // the >1 range is based on previous code; 1-altar temple_overflow maps
    // are placed by the next part, though their weight is not used here. There
    // are currently no such vaults with more than 3 altars, but there's not
    // much cost to checking a few higher.
    for (int num = 2; num <= 5; num++)
    {
        mapref_vector num_maps = find_maps_for_tag(
            make_stringf("temple_overflow_%d", num));
        maps.insert(maps.end(), num_maps.begin(), num_maps.end());
    }

    for (const map_def *map : maps)
    {
        if (overflow_gods.size() < 2)
            break;
        unsigned int num = 0;
        vector<god_type> this_temple_gods;
        for (const auto &tag : map->get_tags())
        {
            if (!starts_with(tag, "temple_overflow_"))
                continue;
            string temple_tag = tag_without_prefix(tag, "temple_overflow_");
            if (temple_tag.empty())
            {
                mprf(MSGCH_ERROR, "Malformed temple tag '%s' in map %s",
                    tag.c_str(), map->name.c_str());
                continue;
            }
            int test_num;
            if (parse_int(temple_tag.c_str(), test_num) && test_num > 0)
                num = test_num;
            else
            {
                replace(temple_tag.begin(), temple_tag.end(), '_', ' ');
                god_type this_god = str_to_god(temple_tag);
                if (this_god == GOD_NO_GOD)
                {
                    mprf(MSGCH_ERROR, "Malformed temple tag '%s' in map %s",
                        tag.c_str(), map->name.c_str());
                    continue;
                }
                this_temple_gods.push_back(this_god);
            }
            if (num == 0)
            {
                if (this_temple_gods.size() > 0)
                {
                    mprf(MSGCH_ERROR,
                        "Map %s has temple_overflow_god tags but no count tag",
                        map->name.c_str());
                }
                continue;
            }
        }
        // there is one vault that currently triggers this, where it allows
        // one of two specified gods on a particular altar. This code won't
        // handle (or error) on that case right now.
        if (num != this_temple_gods.size())
            continue;

        // does this temple place only gods that we need to place?
        bool ok = true;
        for (auto god : this_temple_gods)
            if (count(overflow_gods.begin(), overflow_gods.end(), god) == 0)
            {
                ok = false;
                break;
            }
        if (!ok)
            continue;
        // finally: this overflow vault will place a subset of our current
        // overflow list. Do we actually place it?
        // TODO: The weight calculation here is kind of odd, though based on
        // what it is directly replacing. It should sum all compatible
        // maps first. But, the end result of this choice isn't a map anyways...
        // More generally, I wonder if this list should be shuffled before this
        // step, so that it's not prioritizing smaller vaults?
        int chance = map->weight(level_id(BRANCH_DUNGEON,
                                           MAX_OVERFLOW_LEVEL));
        if (x_chance_in_y(chance, overflow_weights[num] + chance))
        {
            vector<god_type> new_overflow_gods;
            for (auto god : overflow_gods)
                if (count(this_temple_gods.begin(), this_temple_gods.end(), god) == 0)
                    new_overflow_gods.push_back(god);
            _use_overflow_temple(this_temple_gods);

            overflow_gods = new_overflow_gods;
        }
    }

    // NOTE: The overflow temples don't have to contain only one
    // altar; they can contain any number of altars, so long as there's
    // at least one vault definition with the tag "overflow_temple_num"
    // (where "num" is the number of altars).
    for (unsigned int i = 0, size = overflow_gods.size(); i < size; i++)
    {
        unsigned int remaining_size = size - i;
        // At least one god.
        vector<god_type> this_temple_gods;
        this_temple_gods.push_back(overflow_gods[i]);

        // Maybe place a larger overflow temple.
        if (remaining_size > 1 && one_chance_in(remaining_size + 1))
        {
            vector<pair<unsigned int, int> > num_weights;
            unsigned int num_gods = 1;

            // Randomly choose from the sizes which have maps.
            for (unsigned int j = 2; j <= remaining_size; j++)
                if (overflow_weights[j] > 0)
                    num_weights.emplace_back(j, overflow_weights[j]);

            if (!num_weights.empty())
                num_gods = *(random_choose_weighted(num_weights));

            // Add any extra gods (the first was added already).
            for (; num_gods > 1; i++, num_gods--)
                this_temple_gods.push_back(overflow_gods[i + 1]);
        }

        _use_overflow_temple(this_temple_gods);
    }
}

#if TAG_MAJOR_VERSION == 34
static int _get_random_porridge_desc()
{
    return PDESCQ(PDQ_GLUGGY, one_chance_in(3) ? PDC_BROWN
                                               : PDC_WHITE);
}

static int _get_random_coagulated_blood_desc()
{
    potion_description_qualifier_type qualifier = PDQ_NONE;
    while (true)
    {
        switch (random2(4))
        {
        case 0:
            qualifier = PDQ_GLUGGY;
            break;
        case 1:
            qualifier = PDQ_LUMPY;
            break;
        case 2:
            qualifier = PDQ_SEDIMENTED;
            break;
        case 3:
            qualifier = PDQ_VISCOUS;
            break;
        }
        potion_description_colour_type colour = (coinflip() ? PDC_RED
                                                            : PDC_BROWN);

        uint32_t desc = PDESCQ(qualifier, colour);

        if (you.item_description[IDESC_POTIONS][POT_BLOOD] != desc)
            return desc;
    }
}

static int _get_random_blood_desc()
{
    return PDESCQ(random_choose_weighted(2, PDQ_NONE,
                                         1, PDQ_VISCOUS,
                                         1, PDQ_SEDIMENTED), PDC_RED);
}
#endif

void initialise_item_descriptions()
{
    // Must remember to check for already existing colours/combinations.
    you.item_description.init(255);

#if TAG_MAJOR_VERSION == 34
    you.item_description[IDESC_POTIONS][POT_BLOOD]
        = _get_random_blood_desc();
    you.item_description[IDESC_POTIONS][POT_BLOOD_COAGULATED]
        = _get_random_coagulated_blood_desc();
    you.item_description[IDESC_POTIONS][POT_PORRIDGE]
        = _get_random_porridge_desc();
#endif

    // The order here must match that of IDESC in describe.h
    const int max_item_number[6] = { NUM_WANDS,
                                     NUM_POTIONS,
                                     NUM_SCROLLS,
                                     NUM_JEWELLERY,
                                     NUM_SCROLLS,
                                     NUM_STAVES };

    for (int i = 0; i < NUM_IDESC; i++)
    {
        // Only loop until NUM_WANDS etc.
        for (int j = 0; j < max_item_number[i]; j++)
        {
            // Don't override predefines
            if (you.item_description[i][j] != 255)
                continue;

            // Pick a new description until it's good.
            while (true)
            {
                switch (i)
                {
                case IDESC_WANDS: // wands
                    you.item_description[i][j] = random2(NDSC_WAND_PRI
                                                         * NDSC_WAND_SEC);
                    if (coinflip())
                        you.item_description[i][j] %= NDSC_WAND_PRI;
                    break;

                case IDESC_POTIONS: // potions
                    you.item_description[i][j] = _random_potion_description();
                    break;


#if TAG_MAJOR_VERSION == 34
                case IDESC_SCROLLS_II: // unused but validated
#endif
                case IDESC_SCROLLS: // scrolls: random seed for the name
                {
                    // this is very weird and probably a linleyism.
                    const int seed_1 = random2(151); // why 151?
                    const int seed_2 = random2(151);
                    const int seed_3 = OBJ_SCROLLS; // yes, really
                    you.item_description[i][j] =   seed_1
                                                | (seed_2 << 8)
                                                | (seed_3 << 16);
                    break;
                }

                case IDESC_RINGS: // rings and amulets
                    you.item_description[i][j] = random2(NDSC_JEWEL_PRI
                                                         * NDSC_JEWEL_SEC);
                    if (coinflip())
                        you.item_description[i][j] %= NDSC_JEWEL_PRI;
                    break;

                case IDESC_STAVES: // staves
                    you.item_description[i][j] = random2(NDSC_STAVE_PRI
                                                         * NDSC_STAVE_SEC);
                    break;
                }

                bool is_ok = true;

                // Test whether we've used this description before.
                // Don't have p < j because some are preassigned.
                for (int p = 0; p < max_item_number[i]; p++)
                {
                    if (p == j)
                        continue;

                    if (you.item_description[i][p] == you.item_description[i][j])
                    {
                        is_ok = false;
                        break;
                    }
                }
                if (is_ok)
                    break;
            }
        }
    }
}

void fix_up_jiyva_name()
{
    you.jiyva_second_name = make_name(rng::get_uint32(), MNAME_JIYVA);
    ASSERT(you.jiyva_second_name[0] == 'J');
}
