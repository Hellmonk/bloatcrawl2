#pragma once

enum branch_type                // you.where_are_you
{
    BRANCH_DUNGEON,
    BRANCH_TEMPLE,
    BRANCH_FIRST_NON_DUNGEON = BRANCH_TEMPLE,
    BRANCH_ORC,
    BRANCH_ELF,
    BRANCH_LAIR,
    BRANCH_SWAMP,
    BRANCH_SHOALS,
    BRANCH_SNAKE,
    BRANCH_SPIDER,
    BRANCH_SLIME,
    BRANCH_VAULTS,
    BRANCH_CRYPT,
    BRANCH_TOMB,
    BRANCH_DEPTHS,
    BRANCH_VESTIBULE,
    BRANCH_DIS,
    BRANCH_GEHENNA,
    BRANCH_COCYTUS,
    BRANCH_TARTARUS,
      BRANCH_FIRST_HELL = BRANCH_DIS,
      BRANCH_LAST_HELL = BRANCH_TARTARUS,
    BRANCH_ZOT,
    BRANCH_ABYSS,
    BRANCH_PANDEMONIUM,
    BRANCH_ZIGGURAT,
    BRANCH_BAZAAR,
    BRANCH_TROVE,
    BRANCH_SEWER,
    BRANCH_OSSUARY,
    BRANCH_BAILEY,
    BRANCH_GAUNTLET,
    BRANCH_ICE_CAVE,
    BRANCH_VOLCANO,
    BRANCH_WIZLAB,
    BRANCH_DESOLATION,
    NUM_BRANCHES,

    GLOBAL_BRANCH_INFO = 127,
};
