/**
 * @file
 * @brief Prayer and sacrifice.
**/

#ifndef GODPRAYER_H
#define GODPRAYER_H

#include "religion-enum.h"

string god_prayer_reaction();
void pray(bool allow_altar_prayer = true);

int zin_tithe(const item_def& item, int quant, bool quiet,
              bool converting = false);

#endif
