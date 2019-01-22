#pragma once

// When adding:
// * Add an entry in feature-data.h for the feature.
// * edit dat/descript/features.txt and add a
//      long description if appropriate.
// * check the feat_* functions in terrain.cc and make sure
//      they return sane values for your new feature.
// * edit mapdef.cc and add a symbol to _glyph_to_feat() for the feature,
//      if you want vault maps to be able to use it directly . If you do, also
//      update docs/develop/levels/syntax.txt with the new symbol.
enum dungeon_feature_type
{
    DNGN_UNSEEN = 0,                   // must be zero
    DNGN_CLOSED_DOOR,
    DNGN_CLOSED_CLEAR_DOOR,
    DNGN_RUNED_DOOR,
    DNGN_RUNED_CLEAR_DOOR,
    DNGN_SEALED_DOOR,
    DNGN_SEALED_CLEAR_DOOR,
    DNGN_TREE,

    // Walls
    DNGN_METAL_WALL,
    DNGN_CRYSTAL_WALL,
    DNGN_ROCK_WALL,
    DNGN_SLIMY_WALL,
    DNGN_STONE_WALL,
    DNGN_PERMAROCK_WALL,               // for undiggable walls
    DNGN_CLEAR_ROCK_WALL,              // transparent walls
    DNGN_CLEAR_STONE_WALL,
    DNGN_CLEAR_PERMAROCK_WALL,

    DNGN_GRATE,

    // Misc solid features
    DNGN_OPEN_SEA,                     // Shoals equivalent for permarock
    DNGN_LAVA_SEA,                     // Gehenna equivalent for permarock
    DNGN_ENDLESS_SALT,                 // Desolation equivalent for permarock
    DNGN_ORCISH_IDOL,
    DNGN_GRANITE_STATUE,
    DNGN_MALIGN_GATEWAY,

    DNGN_LAVA,
    DNGN_DEEP_WATER,

    DNGN_SHALLOW_WATER,

    DNGN_FLOOR,
    DNGN_OPEN_DOOR,
    DNGN_OPEN_CLEAR_DOOR,

    DNGN_TRAP_MECHANICAL,
    DNGN_TRAP_TELEPORT,
    DNGN_TRAP_SHAFT,
    DNGN_TRAP_WEB,
    DNGN_TRAP_ALARM,
    DNGN_TRAP_ZOT,
    DNGN_TRAP_DISPERSAL,
    DNGN_PASSAGE_OF_GOLUBRIA,

    DNGN_ENTER_SHOP,
    DNGN_ABANDONED_SHOP,

    DNGN_STONE_STAIRS_DOWN_I,
    DNGN_STONE_STAIRS_DOWN_II,
    DNGN_STONE_STAIRS_DOWN_III,
    DNGN_ESCAPE_HATCH_DOWN,

    // corresponding up stairs (same order as above)
    DNGN_STONE_STAIRS_UP_I,
    DNGN_STONE_STAIRS_UP_II,
    DNGN_STONE_STAIRS_UP_III,
    DNGN_ESCAPE_HATCH_UP,

    DNGN_TRANSPORTER,
    DNGN_TRANSPORTER_LANDING,

    // Various gates
    DNGN_ENTER_DIS,
    DNGN_ENTER_GEHENNA,
    DNGN_ENTER_COCYTUS,
    DNGN_ENTER_TARTARUS,
    DNGN_ENTER_ABYSS,
    DNGN_EXIT_ABYSS,
    DNGN_STONE_ARCH,
    DNGN_ENTER_PANDEMONIUM,
    DNGN_EXIT_PANDEMONIUM,
    DNGN_TRANSIT_PANDEMONIUM,
    DNGN_EXIT_DUNGEON,
    DNGN_EXIT_THROUGH_ABYSS,
    DNGN_EXIT_HELL,
    DNGN_ENTER_HELL,
    DNGN_EXPIRED_PORTAL,

    // Entrances to various branches
    DNGN_ENTER_ORC,
    DNGN_ENTER_LAIR,
    DNGN_ENTER_SLIME,
    DNGN_ENTER_VAULTS,
    DNGN_ENTER_CRYPT,
    DNGN_ENTER_ZOT,
    DNGN_ENTER_TEMPLE,
    DNGN_ENTER_SNAKE,
    DNGN_ENTER_ELF,
    DNGN_ENTER_TOMB,
    DNGN_ENTER_SWAMP,
    DNGN_ENTER_SHOALS,
    DNGN_ENTER_SPIDER,
    DNGN_ENTER_DEPTHS,

    // Exits from various branches
    // Order must be the same as above
    DNGN_EXIT_ORC,
    DNGN_EXIT_LAIR,
    DNGN_EXIT_SLIME,
    DNGN_EXIT_VAULTS,
    DNGN_EXIT_CRYPT,
    DNGN_EXIT_ZOT,
    DNGN_EXIT_TEMPLE,
    DNGN_EXIT_SNAKE,
    DNGN_EXIT_ELF,
    DNGN_EXIT_TOMB,
    DNGN_EXIT_SWAMP,
    DNGN_EXIT_SHOALS,
    DNGN_EXIT_SPIDER,
    DNGN_EXIT_DEPTHS,

    DNGN_ALTAR_ZIN,
    DNGN_ALTAR_SHINING_ONE,
    DNGN_ALTAR_KIKUBAAQUDGHA,
    DNGN_ALTAR_YREDELEMNUL,
    DNGN_ALTAR_XOM,
    DNGN_ALTAR_VEHUMET,
    DNGN_ALTAR_OKAWARU,
    DNGN_ALTAR_MAKHLEB,
    DNGN_ALTAR_SIF_MUNA,
    DNGN_ALTAR_TROG,
    DNGN_ALTAR_NEMELEX_XOBEH,
    DNGN_ALTAR_ELYVILON,
    DNGN_ALTAR_LUGONU,
    DNGN_ALTAR_BEOGH,
    DNGN_ALTAR_JIYVA,
    DNGN_ALTAR_FEDHAS,
    DNGN_ALTAR_CHEIBRIADOS,
    DNGN_ALTAR_ASHENZARI,
    DNGN_ALTAR_DITHMENOS,
    DNGN_ALTAR_GOZAG,
    DNGN_ALTAR_QAZLAL,
    DNGN_ALTAR_RU,
    DNGN_ALTAR_USKAYAW,
    DNGN_ALTAR_HEPLIAKLQANA,
    DNGN_ALTAR_WU_JIAN,
    DNGN_ALTAR_ECUMENICAL,

    DNGN_FOUNTAIN_BLUE,
    DNGN_FOUNTAIN_SPARKLING,           // aka 'Magic Fountain' {dlb}
    DNGN_FOUNTAIN_BLOOD,
    DNGN_DRY_FOUNTAIN,

    // Not meant to ever appear in grd().
    DNGN_EXPLORE_HORIZON, // dummy for redefinition

    DNGN_UNKNOWN_ALTAR,
    DNGN_UNKNOWN_PORTAL,

    DNGN_ABYSSAL_STAIR,

    DNGN_SEALED_STAIRS_UP,
    DNGN_SEALED_STAIRS_DOWN,

    DNGN_ENTER_ZIGGURAT,
    DNGN_ENTER_BAZAAR,
    DNGN_ENTER_TROVE,
    DNGN_ENTER_SEWER,
    DNGN_ENTER_OSSUARY,
    DNGN_ENTER_BAILEY,
    DNGN_ENTER_GAUNTLET,
    DNGN_ENTER_ICE_CAVE,
    DNGN_ENTER_VOLCANO,
    DNGN_ENTER_WIZLAB,
    DNGN_ENTER_DESOLATION,

    DNGN_EXIT_ZIGGURAT,
    DNGN_EXIT_BAZAAR,
    DNGN_EXIT_TROVE,
    DNGN_EXIT_SEWER,
    DNGN_EXIT_OSSUARY,
    DNGN_EXIT_BAILEY,
    DNGN_EXIT_GAUNTLET,
    DNGN_EXIT_ICE_CAVE,
    DNGN_EXIT_VOLCANO,
    DNGN_EXIT_WIZLAB,
    DNGN_EXIT_DESOLATION,

    NUM_FEATURES
};
COMPILE_CHECK(NUM_FEATURES <= 256);
