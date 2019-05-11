/**
 * @file
 * @brief Tracking monsters in transit between levels.
 * Also functions to put monsters into and remove them from limbo
**/

#pragma once

#include <list>
#include <map>

#include "monster.h"
#include "view.h"

struct follower
{
    monster mons;
    FixedVector<item_def, NUM_MONSTER_SLOTS> items;
    int transit_start_time;

    follower() : mons(), items() { }
    follower(const monster& m);

    // if placement was successful, returns a pointer to the placed monster
    monster* place(const coord_def *defined_pos = NULL, 
		   bool exact_pos = false);
    void load_mons_items();
    void restore_mons_items(monster& m);
};

// Several erase() calls rely on this being a linked list (so erasing does not
// invalidate the iterators).
typedef list<follower> m_transit_list;
typedef map<level_id, m_transit_list> monsters_in_transit;

// This one too.
#if TAG_MAJOR_VERSION == 34
typedef list<item_def> i_transit_list;
typedef map<level_id, i_transit_list> items_in_transit;
#endif

extern monsters_in_transit the_lost_ones;

void transit_lists_clear();

m_transit_list *get_transit_list(const level_id &where);
void add_monster_to_transit(const level_id &dest, const monster& m);

void remove_monster_from_transit(const level_id &lid, mid_t mid);

// Places (some of the) monsters eligible to be placed on this level.
void place_transiting_monsters();
void place_followers();
void handle_followers(const coord_def &from,
                      bool (*handler)(const coord_def &pos,
                                      const coord_def &from, bool &real));
void tag_followers();
void untag_followers();
void transport_followers_from(const coord_def &from);

void apply_daction_to_transit(daction_type act);
int count_daction_in_transit(daction_type act);

// Follower a bit of a misnomer here but the data structure is otherwise ideal
typedef list<follower> m_limbo_list;
typedef map<level_id, m_limbo_list> monsters_in_limbo;
extern monsters_in_limbo limbo_monsters;

void add_monster_to_limbo(monster *m, 
			  const level_id &level = level_id::current());
bool extract_monster_from_limbo(mid_t mid, const coord_def &pos, 
				bool exact_pos = true, unsigned short foe = 0);
void wizard_extract_limbo();
mid_t is_limbo_mons(std::function <bool (const monster &mons)> test);
