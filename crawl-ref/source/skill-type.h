#pragma once

// [dshaligram] If you add a new skill, update skills.cc, specifically
// the skills[] array and skill_display_order[]. New skills must go at the
// end of the list or in the unused skill numbers. NEVER rearrange this enum or
// move existing skills to new numbers; save file compatibility depends on this
// order.
enum skill_type
{
    SK_FIGHTING,
    SK_FIRST_SKILL = SK_FIGHTING,
    SK_SHORT_BLADES,
    SK_FIRST_WEAPON = SK_SHORT_BLADES,
    SK_LONG_BLADES,
    SK_AXES,
    SK_MACES_FLAILS,
    SK_POLEARMS,
    SK_STAVES,
    SK_LAST_MELEE_WEAPON = SK_STAVES,
    SK_SLINGS,
    SK_BOWS,
    SK_CROSSBOWS,
    SK_LAST_WEAPON = SK_CROSSBOWS,
    SK_THROWING,
    SK_ARMOUR,
    SK_DODGING,
    SK_STEALTH,
#if TAG_MAJOR_VERSION == 34
    SK_STABBING,
#endif
    SK_SHIELDS,
#if TAG_MAJOR_VERSION == 34
    SK_TRAPS,
#endif
    SK_UNARMED_COMBAT,
    SK_LAST_MUNDANE = SK_UNARMED_COMBAT,
    SK_SPELLCASTING,
    SK_CONJURATIONS,
    SK_FIRST_MAGIC_SCHOOL = SK_CONJURATIONS, // not SK_FIRST_MAGIC as no Spc
    SK_HEXES,
    SK_CHARMS,
    SK_SUMMONINGS,
    SK_NECROMANCY,
    SK_TRANSLOCATIONS,
    SK_TRANSMUTATIONS,
    SK_FIRE_MAGIC,
    SK_ICE_MAGIC,
    SK_AIR_MAGIC,
    SK_EARTH_MAGIC,
    SK_POISON_MAGIC,
    SK_LAST_MAGIC = SK_POISON_MAGIC,
    SK_INVOCATIONS,
    SK_EVOCATIONS,
    SK_LAST_SKILL = SK_EVOCATIONS,
    NUM_SKILLS,                        // must remain last regular member

    SK_BLANK_LINE,                     // used for skill output
    SK_COLUMN_BREAK,                   // used for skill output
    SK_TITLE,                          // used for skill output
    SK_NONE,
    SK_WEAPON,                         // used in character generation
};
