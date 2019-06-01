// Anything new pertaining to permabuffs
#pragma once
#include "duration-type.h"

enum permabuff_type {
	PERMA_NO_PERMA,
	PERMA_INFUSION,
	PERMA_FIRST_PERMA = PERMA_INFUSION,
	PERMA_LAST_PERMA = PERMA_INFUSION,
    };

static const duration_type permabuff_durs[] = {
    NUM_DURATIONS, // bah, hope this won't be used
    DUR_INFUSION
};

static const spell_type permabuff_spell[] = {
    NUM_SPELLS,
    SPELL_INFUSION
};
