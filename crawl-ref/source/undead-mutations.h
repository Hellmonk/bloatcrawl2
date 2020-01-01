#pragma once

struct undead_mut
{
    mutation_type mutation;
    int level;
    bool mummy; // applies to mummies
    bool zombie; // applies to zombies
    bool vampire; // applies to vampires
    bool ghost; // applies to ghosts
};

static const undead_mut undead_mutations[] = {
    { MUT_UNBREATHING, 1, true, true, true, true },
    { MUT_NEGATIVE_ENERGY_RESISTANCE, 3, true, true, false, true },
    { MUT_TORMENT_RESISTANCE, 1, true, true, false, true },
    { MUT_COLD_RESISTANCE, 1, true, true, false, false },
    { MUT_HEAT_VULNERABILITY, 1, true, false, false, false },
    { MUT_HUNGRY_DEAD, 1, false, true, false, false },
    { MUT_INHIBITED_REGENERATION, 1, false, true, false, false },
    { MUT_SEMI_UNDEAD, 1, false, false, true, false },
    { MUT_GHOST, 1, false, false, false, true}
};
