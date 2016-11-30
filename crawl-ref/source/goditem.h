#ifndef GODITEM_H
#define GODITEM_H

#include "enum.h"
#include "player.h"
#include "spl-util.h"

bool is_holy_item(const item_def& item);
bool is_potentially_evil_item(const item_def& item);
bool is_corpse_violating_item(const item_def& item);
bool is_evil_item(const item_def& item);
bool is_unclean_item(const item_def& item);
bool is_chaotic_item(const item_def& item);
bool is_hasty_item(const item_def& item);
bool is_fiery_item(const item_def& item);
bool is_channeling_item(const item_def& item);
bool is_corpse_violating_spell(spell_type spell);
bool is_evil_spell(spell_type spell);
bool is_unclean_spell(spell_type spell);
bool is_chaotic_spell(spell_type spell);
bool is_hasty_spell(spell_type spell);
bool is_fiery_spell(spell_type spell);
conduct_type god_hates_item_handling(const item_def &item);
bool god_hates_item(const item_def &item);
bool god_likes_item_type(const item_def &item, god_type which_god);
#endif
