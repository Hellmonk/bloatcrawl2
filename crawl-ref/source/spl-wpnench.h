#pragma once

#include "item-prop-enum.h"
#include "spl-cast.h"

#define ORIGINAL_BRAND_KEY "orig brand"

class dist;

void end_weapon_brand(item_def &weapon, bool verbose = false);

spret cast_excruciating_wounds(int power, bool fail);
spret cast_confusing_touch(int power, bool fail);
