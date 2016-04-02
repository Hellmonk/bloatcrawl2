/**
 * @file
 * @brief Spellbook/rod contents array and management functions
**/

#ifndef SPL_BOOK_H
#define SPL_BOOK_H

#define RANDBOOK_SIZE 8
#include "itemprop-enum.h"
#include "spl-util.h" // spschool_flag_type

#define SPELL_LIST_KEY "spell_list"

/// Should the book's name NOT use articles? (Foo's Bar of Baz, not the Foo's)
#define BOOK_TITLED_KEY "is_named"

class formatted_string;

int  book_rarity(book_type which_book);
int  spell_rarity(spell_type which_spell);
bool is_rare_book(book_type type);
void init_spell_rarities();
bool is_player_spell(spell_type which_spell);

void mark_had_book(const item_def &book);
void mark_had_book(book_type booktype);
bool book_has_title(const item_def &book);

void read_book(item_def &item);

bool player_can_memorise(const item_def &book);
bool can_learn_spell(bool silent = false);
bool learn_spell();
bool learn_spell(spell_type spell, bool wizard = false);
bool forget_spell_from_book(spell_type spell, const item_def* book);

string desc_cannot_memorise_reason(spell_type spell);

spell_type spell_in_rod(rod_type rod);
vector<spell_type> spellbook_template(book_type book);
vector<spell_type> spells_in_book(const item_def &book);

bool you_can_memorise(spell_type spell) PURE;
bool has_spells_to_memorise(bool silent = true,
                            spell_type current_spell = SPELL_NO_SPELL);
vector<spell_type> get_mem_spell_list(vector<int> &books);

void destroy_spellbook(const item_def &book);
#endif
