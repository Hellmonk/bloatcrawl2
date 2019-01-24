#pragma once

#include "mon-attitude-type.h"

#define CLONE_MASTER_KEY "mcloneorig"
#define CLONE_SLAVE_KEY "mclonedupe"

// Formerly in mon-stuff:
bool mons_clonable(const monster* orig, bool needs_adjacent = true);
monster *clone_mons(const monster* orig, bool quiet = false,
                    bool* obvious = nullptr);

monster *clone_mons(const monster* orig, bool quiet,
                    bool* obvious, mon_attitude_type mon_att);

void mons_summon_illusion_from(monster* mons, actor *foe,
                               spell_type spell_cast = SPELL_NO_SPELL,
                               int card_power = -1);

bool actor_is_illusion_cloneable(actor *target);
