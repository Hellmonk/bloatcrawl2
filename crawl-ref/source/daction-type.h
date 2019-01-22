#pragma once

enum daction_type
{
    DACT_ALLY_UNHOLY_EVIL,
    DACT_ALLY_UNCLEAN_CHAOTIC,
    DACT_ALLY_SPELLCASTER,
    DACT_ALLY_YRED_SLAVE,
    DACT_ALLY_BEOGH, // both orcs and demons summoned by high priests
    DACT_ALLY_SLIME,
    DACT_ALLY_PLANT,
    NUM_DACTION_COUNTERS,
    // Leave space for new counters, as they need to be at the start.
    DACT_OLD_ENSLAVED_SOULS_POOF = 16,
    DACT_SLIME_NEW_ATTEMPT,
    DACT_REAUTOMAP,
    DACT_REMOVE_JIYVA_ALTARS,
    DACT_PIKEL_SLAVES,
    DACT_ROT_CORPSES,
    DACT_KIRKE_HOGS,
    DACT_GOLD_ON_TOP,
    DACT_BRIBE_TIMEOUT,
    DACT_REMOVE_GOZAG_SHOPS,
    DACT_SET_BRIBES,
    DACT_ALLY_HEPLIAKLQANA,
    DACT_UPGRADE_ANCESTOR,
    NUM_DACTIONS,
    // If you want to add a new daction, you need to
    // add a corresponding entry to *daction_names[]
    // of dactions.cc to avoid breaking the debug build
};
