#pragma once

enum piety_gain_t
{
    PIETY_NONE, PIETY_SOME, PIETY_LOTS,
    NUM_PIETY_GAIN
};

enum tithe_state
{
    TS_DEFAULT,    // No state set
    TS_NO_PIETY,   // Don't give any piety
    TS_FULL_TITHE, // Tithe normally
    TS_NO_TITHE,   // Don't tithe at all
};


#define ACQUIRE_KEY "acquired" // acquirement source prop on acquired items

/// the name of the ally hepliaklqana granted the player
#define HEPLIAKLQANA_ALLY_NAME_KEY "hepliaklqana_ally_name"
/// ancestor gender
#define HEPLIAKLQANA_ALLY_GENDER_KEY "hepliaklqana_ally_gender"
/// chosen ancestor class (monster_type)
#define HEPLIAKLQANA_ALLY_TYPE_KEY "hepliaklqana_ally_type"

/// custom monster gender
#define MON_GENDER_KEY "mon_gender"
