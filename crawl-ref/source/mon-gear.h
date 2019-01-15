/**
 * @file
 * @brief Monsters' initial equipment.
**/

#pragma once

void give_specific_item(monster* mon, const item_def& tpl);
void give_specific_item(monster* mon, int thing);
void give_item(monster *mon, int level_number, bool mons_summoned);
int make_mons_weapon(monster_type mtyp, int level, bool melee_only = false);
void give_weapon(monster *mon, int level_number);
int make_mons_armour(monster_type mtyp, int level);
void give_armour(monster *mon, int level_number);
void give_shield(monster *mon);
void view_monster_equipment(monster* mon);
