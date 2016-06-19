#include "AppHdr.h"

#include "branch.h"
#include "branch-data.h"

#include "itemname.h"
#include "player.h"
#include "stringutil.h"
#include "travel.h"

FixedVector<level_id, NUM_BRANCHES> brentry;
FixedVector<int, NUM_BRANCHES> brdepth;
FixedVector<int, NUM_BRANCHES> branch_bribe;
branch_type root_branch;

branch_iterator::branch_iterator() :
    i(BRANCH_FIRST)
{
}

branch_iterator::operator bool() const
{
    return i < NUM_BRANCHES;
}

const Branch* branch_iterator::operator*() const
{
    static const branch_type branch_order[] = {
        BRANCH_ABYSS,
        BRANCH_BAILEY,
        BRANCH_BAZAAR,
        BRANCH_COCYTUS,
        BRANCH_CRYPT,
        BRANCH_DEPTHS,
        BRANCH_DIS,
        BRANCH_DUNGEON,
        BRANCH_DWARF,
        BRANCH_ELF,
        BRANCH_FOREST,
        BRANCH_GEHENNA,
        BRANCH_ICE_CAVE,
        BRANCH_LABYRINTH,
        BRANCH_LAIR,
        BRANCH_ORC,
        BRANCH_OSSUARY,
        BRANCH_PANDEMONIUM,
        BRANCH_SEWER,
        BRANCH_SHOALS,
        BRANCH_SLIME,
        BRANCH_SNAKE,
        BRANCH_SPIDER,
        BRANCH_SWAMP,
        BRANCH_TARTARUS,
        BRANCH_TEMPLE,
        BRANCH_TOMB,
        BRANCH_TROVE,
        BRANCH_VAULTS,
        BRANCH_VESTIBULE,
        BRANCH_VOLCANO,
        BRANCH_WIZLAB,
        BRANCH_ZIGGURAT,
        BRANCH_ZOT,
    };
    COMPILE_CHECK(ARRAYSZ(branch_order) == NUM_BRANCHES);

    if (i < NUM_BRANCHES)
        return &branches[branch_order[i]];
    else
        return nullptr;
}

const Branch* branch_iterator::operator->() const
{
    return **this;
}

branch_iterator& branch_iterator::operator++()
{
    i++;
    return *this;
}

branch_iterator branch_iterator::operator++(int)
{
    branch_iterator copy = *this;
    ++(*this);
    return copy;
}

bool is_safe_branch(branch_type branch)
{
    return branch == BRANCH_TROVE
           || branch == BRANCH_BAZAAR
           || branch == BRANCH_TEMPLE;
}

const Branch& your_branch()
{
    return branches[you.where_are_you];
}

bool at_branch_bottom()
{
    return brdepth[you.where_are_you] == you.depth;
}

level_id current_level_parent()
{
    // Never called from X[], we don't have to support levels you're not on.
    if (!you.level_stack.empty())
        return you.level_stack.back().id;

    return find_up_level(level_id::current());
}

bool is_hell_subbranch(branch_type branch)
{
    return branch == BRANCH_COCYTUS
           || branch == BRANCH_TARTARUS
           || branch == BRANCH_DIS
           || branch == BRANCH_GEHENNA;
}

bool is_random_subbranch(branch_type branch)
{
    return parent_branch(branch) == BRANCH_LAIR
           && branch != BRANCH_SLIME;
}

bool is_connected_branch(const Branch *branch)
{
    return !(branch->branch_flags & BFLAG_NO_XLEV_TRAVEL);
}

bool is_connected_branch(branch_type branch)
{
    ASSERT_RANGE(branch, 0, NUM_BRANCHES);
    return is_connected_branch(&branches[branch]);
}

bool is_connected_branch(level_id place)
{
    return is_connected_branch(place.branch);
}

bool in_lower_half_of_branch()
{
    return you.depth > (brdepth[you.where_are_you] + 1) / 2;
}

bool is_double_deep_branch(branch_type branch)
{
    return false;
}

bool player_can_gain_experience_here()
{
    return !is_double_deep_branch(you.where_are_you) || !in_lower_half_of_branch();
}

branch_type branch_by_abbrevname(const string &branch, branch_type err)
{
    for (branch_iterator it; it; ++it)
        if (it->abbrevname && it->abbrevname == branch)
            return it->id;

    return err;
}

branch_type branch_by_shortname(const string &branch)
{
    for (branch_iterator it; it; ++it)
        if (it->shortname && it->shortname == branch)
            return it->id;

    return NUM_BRANCHES;
}

int current_level_ambient_noise()
{
    return branches[you.where_are_you].ambient_noise;
}

branch_type get_branch_at(const coord_def& pos)
{
    return level_id::current().get_next_level_id(pos).branch;
}

bool branch_is_unfinished(branch_type branch)
{
    return false;
}

branch_type parent_branch(branch_type branch)
{
    if (brentry[branch].is_valid())
        return brentry[branch].branch;
    // If it's not in the game, use the default parent.
    return branches[branch].parent_branch;
}

int runes_for_branch(branch_type branch)
{
    int runes_required = 0;
    switch (branch)
    {
    case BRANCH_VAULTS:   runes_required = VAULTS_ENTRY_RUNES;
    case BRANCH_ZIGGURAT: runes_required = ZIG_ENTRY_RUNES;
    case BRANCH_ZOT:      runes_required = ZOT_ENTRY_RUNES;
    default:              runes_required = 0;
    }

    runes_required += you.branch_requires_runes[branch];

    return runes_required;
}

/**
 * Write a description of the rune(s), if any, this branch contains.
 *
 * @param br             the branch in question
 * @param remaining_only whether to only mention a rune if the player
 *                       hasn't picked it up yet.
 * @returns a string mentioning all applicable runes.
 */
string branch_rune_desc(branch_type br, bool remaining_only)
{
    string desc;
    vector<string> rune_names;

    for (rune_type rune : branches[br].runes)
        if (!(remaining_only && you.runes[rune]))
            rune_names.push_back(rune_type_name(rune));

    if (!rune_names.empty())
    {
        desc = make_stringf("This branch contains the %s rune%s of Zot.",
                            comma_separated_line(begin(rune_names),
                                                 end(rune_names)).c_str(),
                            rune_names.size() > 1 ? "s" : "");
    }

    return desc;
}

branch_type rune_location(rune_type rune)
{
    for (const auto& br : branches)
        if (find(br.runes.begin(), br.runes.end(), rune) != br.runes.end())
            return br.id;

    return NUM_BRANCHES;
}
