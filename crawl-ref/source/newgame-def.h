#pragma once

#include "game-type.h"
#include "item-prop-enum.h"
#include "job-type.h"
#include "player-religion.h"
#include "species-type.h"
#include "undead-state-type.h"

// Either a character definition, with real species, job, and
// weapon, book, wand as appropriate.
// Or a character choice, with possibly random/viable entries.
struct newgame_def
{
    string name;
    game_type type;
    string filename;
    uint64_t seed;
    bool pregenerate;

    // map name for sprint (or others in the future)
    // XXX: "random" means a random eligible map
    string map;

    string arena_teams;

    vector<string> allowed_combos;
    vector<species_type> allowed_species;
    vector<job_type> allowed_jobs;
    vector<weapon_type> allowed_weapons;

    species_type species;
    job_type job;

    weapon_type weapon;

    // Only relevant for character choice, where the entire
    // character was randomly picked in one step.
    // If this is true, the species field encodes whether
    // the choice was for a viable character or not.
    bool fully_random;

    // Game variants
    undead_state_type undead_type;
    int skilled_type;
    bool chaoskin; // Xom acts on you always
    bool no_locks; // disable rune locks
    player_religion religion_type;

    newgame_def();
    void clear_character();
};
