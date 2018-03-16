/**
 * @file
 * @brief Functions for eating and butchering.
**/

#ifndef FOOD_H
#define FOOD_H

#include "mon-enum.h"

#define BERSERK_NUTRITION     700

#define HUNGER_FAINTING       400
#define HUNGER_STARVING       900
#define HUNGER_NEAR_STARVING 1433
#define HUNGER_VERY_HUNGRY   1966
#define HUNGER_HUNGRY        2500
#define HUNGER_SATIATED      6900
#define HUNGER_FULL          8900
#define HUNGER_VERY_FULL    10900
#define HUNGER_ENGORGED     39900

#define HUNGER_DEFAULT       5900
#define HUNGER_MAXIMUM      11900

bool eat_food(int slot = -1);

void make_hungry(int hunger_amount, bool suppress_msg, bool magic = false);

void set_hunger(int new_hunger_level, bool suppress_msg);

bool is_inedible(const item_def &item);
bool is_preferred_food(const item_def &food);

bool can_eat(const item_def &food, bool suppress_msg, bool check_hunger = true);

bool eat_item(item_def &food);

bool you_foodless(bool can_eat = false);

void handle_starvation();
int hunger_bars(const int hunger);
string hunger_cost_string(const int hunger);
#endif
