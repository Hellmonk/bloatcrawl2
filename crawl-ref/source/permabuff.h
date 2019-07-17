// Anything new pertaining to permabuffs
#pragma once
#include "duration-type.h"

enum permabuff_type {
	PERMA_NO_PERMA,
	PERMA_INFUSION,
	PERMA_SHROUD,
	PERMA_SONG,
	PERMA_REGEN,
	PERMA_PPROJ,
	PERMA_DMSL,
	PERMA_EXCRU,
	PERMA_DCHAN,
	PERMA_ROF,
	PERMA_FIRST_PERMA = PERMA_INFUSION,
	PERMA_LAST_PERMA = PERMA_ROF,
    };

static const duration_type permabuff_durs[] = {
    NUM_DURATIONS, // bah, hope this won't be used
    DUR_INFUSION,
    DUR_SHROUD_OF_GOLUBRIA,
    DUR_SONG_OF_SLAYING,
    DUR_REGENERATION,
    DUR_PORTAL_PROJECTILE,
    DUR_DEFLECT_MISSILES,
    DUR_EXCRUCIATING_WOUNDS,
    DUR_DEATH_CHANNEL,
    DUR_FIRE_SHIELD, // bah
};

static const spell_type permabuff_spell[] = {
    NUM_SPELLS,
    SPELL_INFUSION,
    SPELL_SHROUD_OF_GOLUBRIA,
    SPELL_SONG_OF_SLAYING,
    SPELL_REGENERATION,
    SPELL_PORTAL_PROJECTILE,
    SPELL_DEFLECT_MISSILES,
    SPELL_EXCRUCIATING_WOUNDS,
    SPELL_DEATH_CHANNEL,
    SPELL_RING_OF_FLAMES,
};

// These PBs charge you MP regeneration based on their nominal duration.
// Others don't - Regen has its own MP reservoir.
// They all charge you hunger, however
static const permabuff_type pb_ordinary_mpregen[] = {
    PERMA_INFUSION,
    PERMA_SHROUD,
    PERMA_SONG,
    PERMA_PPROJ,
    PERMA_DMSL,
    PERMA_EXCRU,
    PERMA_DCHAN,
    PERMA_ROF,
};
static const int size_mpregen_pb = ARRAYSZ(pb_ordinary_mpregen);

// This is the amount to divide the nominal duration by to determine how often
// to check for a miscast. It is meant to somehow approximate the proportion
// of turns you might reasonably expect to benefit from it normally - eg Regen
// has '1' because a lot of the time when you regenerate you gain HP for the 
// whole duration. Without this, we noticed miscasts almost never happen.
// If I don't know what the answer should be, it'll be '2'
static const int pb_dur_fudge[] = {
    0,
    2, // infusion
    2, // shroud
    2, // song
    1, // regen
    1, // pproj
    2, // dmsl
    2, // excru
    1, // DChan - 1 because you might get many kills in a turn
    1, // rof - short duration, cast before unloading
};
