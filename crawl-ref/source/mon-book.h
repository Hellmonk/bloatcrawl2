/**
 * @file
 * @brief Monster spellbook functions, types, and globals.
**/

#ifndef MONBOOK_H
#define MONBOOK_H

#include <vector>

#include "defines.h"
#include "enum.h"
#include "mon-mst.h"

struct mon_spellbook
{
    mon_spellbook_type type;
    vector<mon_spell_slot> spells;
};

typedef vector<vector<mon_spell_slot>> unique_books;

vector<mon_spellbook_type> get_spellbooks(const monster_info &mon);
unique_books get_unique_spells(const monster_info &mon,
                               mon_spell_slot_flags flags = MON_SPELL_NO_FLAGS);
#endif
