// Anything new pertaining to permabuffs
#pragma once
#include "duration-type.h"

enum permabuff_type {
	PERMA_NO_PERMA,
	PERMA_INFUSION,
	PERMA_SHROUD,
	PERMA_SONG,
	PERMA_REGEN,
	PERMA_FIRST_PERMA = PERMA_INFUSION,
	PERMA_LAST_PERMA = PERMA_REGEN,
    };

static const duration_type permabuff_durs[] = {
    NUM_DURATIONS, // bah, hope this won't be used
    DUR_INFUSION,
    DUR_SHROUD_OF_GOLUBRIA,
    DUR_SONG_OF_SLAYING,
    DUR_REGENERATION,
};

static const spell_type permabuff_spell[] = {
    NUM_SPELLS,
    SPELL_INFUSION,
    SPELL_SHROUD_OF_GOLUBRIA,
    SPELL_SONG_OF_SLAYING,
    SPELL_REGENERATION,
};

// This is the amount to divide the nominal duration by to determine how often
// to check for a miscast. It is meant to somehow approximate the proportion
// of turns you might reasonably expect to benefit from it normally - eg Regen
// has '1' because a lot of the time when you regenerate you gain HP for the 
// whole duration. Without this, we noticed miscasts almost never happen.
// If I don't know what the answer should be, it'll be '2'
static const int pb_dur_fudge[] = {
    0,
    2, // infusion
    1, // unused - shroud
    2, // song
    1  // regen
};
