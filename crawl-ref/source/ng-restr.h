#pragma once

#include "item-prop-enum.h"
#include "job-type.h"
#include "species-type.h"

struct newgame_def;

enum char_choice_restriction
{
    CC_BANNED = 0,
    CC_RESTRICTED,
    CC_UNRESTRICTED,
};

char_choice_restriction species_allowed(job_type job, species_type speci);
char_choice_restriction job_allowed(species_type speci, job_type job);
bool character_is_allowed(species_type species, job_type job);
char_choice_restriction weapon_restriction(weapon_type wpn,
                                           const newgame_def &ng);
