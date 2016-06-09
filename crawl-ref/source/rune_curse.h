#ifndef DCSSCA_RUNE_CURSES_H
#define DCSSCA_RUNE_CURSES_H

#include "itemprop-enum.h"

const int rune_curse_hd_adjust(int hd, bool absolute = true);
const int rune_curse_hp_adjust(int hp, bool absolute = true);
const int rune_curse_dam_adjust(int dam, bool absolute = true);
const int rune_curse_mon_spellpower_adjustment(int spellpower);
const int rune_curse_depth_adjust(int depth);

void list_rune_curses();
const string rune_curse_description(const rune_type rune);
void choose_branch_rune_requirements();

#endif //DCSSCA_RUNE_CURSES_H
