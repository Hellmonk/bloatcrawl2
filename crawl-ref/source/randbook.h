/**
 * @file
 * @brief Functions for generating random spellbooks.
**/

#ifndef RANDBOOK_H
#define RANDBOOK_H

#include "spl-util.h"

typedef function<bool(spschool_flag_type discipline_1,
                      spschool_flag_type discipline_2,
                      int agent,
                      const vector<spell_type> &prev,
                      spell_type spell)> themed_spell_filter;

/// How many spells should be in a random theme book?
int theme_book_size();

spschool_flag_type random_book_theme();
spschool_flag_type matching_book_theme(const vector<spell_type> &forced_spells);
function<spschool_flag_type()> forced_book_theme(spschool_flag_type theme);

bool basic_themed_spell_filter(spschool_flag_type discipline_1,
                               spschool_flag_type discipline_2,
                               int agent,
                               const vector<spell_type> &prev,
                               spell_type spell);
themed_spell_filter capped_spell_filter(int max_levels,
                                        themed_spell_filter subfilter
                                            = basic_themed_spell_filter);
themed_spell_filter forced_spell_filter(const vector<spell_type> &forced_spells,
                                        themed_spell_filter subfilter
                                            = basic_themed_spell_filter);

void theme_book_spells(spschool_flag_type discipline_1,
                       spschool_flag_type discipline_2,
                       themed_spell_filter filter,
                       int agent,
                       int num_spells,
                       vector<spell_type> &spells);

void build_themed_book(item_def &book,
                       themed_spell_filter filter = basic_themed_spell_filter,
                       function<spschool_flag_type()> get_discipline
                            = random_book_theme,
                       int num_spells = -1,
                       string owner = "", string subject = "");

void fixup_randbook_disciplines(spschool_flag_type &discipline_1,
                                spschool_flag_type &discipline_2,
                                const vector<spell_type> &spells);
void init_book_theme_randart(item_def &book, vector<spell_type> spells);
void name_book_theme_randart(item_def &book, spschool_flag_type discipline_1,
                             spschool_flag_type discipline_2,
                             string owner = "", string subject = "");

bool make_book_level_randart(item_def &book, int level = -1);
void make_book_roxanne_special(item_def *book);
void make_book_kiku_gift(item_def &book, bool first);
void acquire_themed_randbook(item_def &book, int agent);

#endif
