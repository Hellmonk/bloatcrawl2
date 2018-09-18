/*
 * Entry format:
 *   row  0: species enum
 *   row  1: two-letter abbreviation
 *   row  2: name noun, name adjective, genus (null to use name)
 *   row  3: flags (SPF_*)
 *   row  4: XP "aptitude", HP mod (in tenths), MP mod, MR per XL
 *   row  5: corresponding monster
 *   row  6: habitat, undead state, size
 *   row  7: starting strength, intelligence, dexterity  // sum
 *   row  8: { level-up stats }, level for stat increase
 *   row  9: { { mutation, mutation level, XP level }, ... }
 *   row 10: { fake mutation messages for A screen }
 *   row 11: { fake mutation names for % screen }
 *   row 12: recommended jobs for character selection
 *   row 13: recommended weapons for character selection
 *
 * Rows 9-13 may span multiple lines if necessary.
 */
static const map<species_type, species_def> species_data =
{
	
{ SP_ANGEL, {
    "An",
    "Angel", "Angelic", nullptr,
    SPF_NO_HAIR,
    0, -1, 0, 4,
    MONS_ANGEL,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    8, 8, 14, // 30
    { STAT_DEX }, 4,
    {{ MUT_POISON_RESISTANCE, 1, 1 }, { MUT_SHOCK_RESISTANCE, 1, 7 },
     { MUT_ACUTE_VISION, 1, 1 }, },
    {"You can fly at any time."},
    {"flight"},
    { JOB_GLADIATOR, JOB_CONJURER, JOB_SUMMONER,
      JOB_FIRE_ELEMENTALIST, JOB_AIR_ELEMENTALIST },
    { SK_MACES_FLAILS, SK_POLEARMS, SK_LONG_BLADES, SK_STAVES,
      SK_BOWS, SK_CROSSBOWS },
} },

{ SP_BARACHI, {
    "Ba",
    "Barachi", "Barachian", "Frog",
    SPF_NO_HAIR,
    0, 0, 0, 3,
    MONS_BARACHI,
    HT_WATER, US_ALIVE, SIZE_MEDIUM,
    9, 8, 7, // 24
    { STAT_STR, STAT_INT, STAT_DEX }, 4,
    { { MUT_SLOW, 1, 1 }, { MUT_HOP, 1, 1}, {MUT_HOP, 1, 13}, },
    { "Shadows flee at your approach. (+LOS)", "You can swim through water.", },
    { "+LOS", "swims", },
    { JOB_FIGHTER, JOB_BERSERKER, JOB_SKALD, JOB_SUMMONER, JOB_ICE_ELEMENTALIST },
    { SK_MACES_FLAILS, SK_AXES, SK_POLEARMS, SK_LONG_BLADES, SK_STAVES,
      SK_BOWS, SK_CROSSBOWS, SK_SLINGS },
} },

{ SP_BODACH, {
    "Bo",
    "Bodach", nullptr, nullptr,
    SPF_NO_HAIR,
    -1, 0, 0, 3,
    MONS_WRETCHED_STAR,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    7, 7, 7, // 21
    { STAT_STR, STAT_INT, STAT_DEX }, 4,
    { },
    { "Your aspect changes uncontrollably as you grow."},
    { "aspect changes" },
    { JOB_BERSERKER, JOB_CONJURER, JOB_FIRE_ELEMENTALIST,
      JOB_ICE_ELEMENTALIST, JOB_AIR_ELEMENTALIST, JOB_EARTH_ELEMENTALIST },
    { SK_MACES_FLAILS, SK_AXES, SK_POLEARMS, SK_LONG_BLADES, SK_STAVES,
      SK_BOWS, SK_CROSSBOWS, SK_SLINGS },
} },

{ SP_FLAN, {
    "Fl",
    "Flan", nullptr, nullptr,
    SPF_NO_HAIR,
    -1, -1, 0, 3,
    MONS_JELLY,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    10, 10, 10, // 30
    { STAT_STR, STAT_INT, STAT_DEX }, 5,
    { { MUT_GELATINOUS_BODY, 3, 1}, {MUT_CORROSION_RESISTANCE, 1, 1} },
    { "Your gelatinous body takes the shape of your armour (ER/2)",
      "You sometimes corrode monsters when you hit them."},
    { "ER/2", "corrosion", },
    { JOB_CONJURER, JOB_FIRE_ELEMENTALIST, JOB_EARTH_ELEMENTALIST,
      JOB_ICE_ELEMENTALIST, JOB_AIR_ELEMENTALIST },
    { SK_MACES_FLAILS, SK_AXES, SK_POLEARMS, SK_LONG_BLADES, SK_STAVES,
      SK_BOWS, SK_CROSSBOWS, SK_SLINGS },
} },

{ SP_GNOLL, {
    "Gn",
    "Gnoll", nullptr, nullptr,
    SPF_NONE,
    0, 0, 0, 3,
    MONS_GNOLL,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    10, 10, 10, // 30
    { STAT_STR, STAT_INT, STAT_DEX }, 4,
    { { MUT_STRONG_NOSE, 1, 1 },  { MUT_FANGS, 1, 1 }, },
    { "Your experience applies equally to all skills."},
    { "distributed training", },
    { JOB_SKALD, JOB_WARPER, JOB_ARCANE_MARKSMAN, JOB_TRANSMUTER,
      JOB_WANDERER, JOB_BLOOD_KNIGHT },
    { SK_MACES_FLAILS, SK_AXES, SK_POLEARMS, SK_LONG_BLADES, SK_STAVES,
      SK_BOWS, SK_CROSSBOWS, SK_SLINGS },
} },

{ SP_CENTAUR, {
    "Ce",
    "Centaur", nullptr, nullptr,
    SPF_SMALL_TORSO,
    -1, 1, 0, 3,
    MONS_CENTAUR,
    HT_LAND, US_ALIVE, SIZE_LARGE,
    10, 7, 4, // 21
    { STAT_STR, STAT_DEX }, 4,
    { { MUT_TOUGH_SKIN, 3, 1 }, { MUT_FAST, 2, 1 },  { MUT_DEFORMED, 1, 1 },
      { MUT_HOOVES, 3, 1 }, },
    {},
    {},
    { JOB_FIGHTER, JOB_GLADIATOR, JOB_HUNTER, JOB_WARPER, JOB_ARCANE_MARKSMAN },
    { SK_MACES_FLAILS, SK_AXES, SK_POLEARMS, SK_LONG_BLADES, SK_STAVES,
      SK_BOWS, SK_CROSSBOWS, SK_SLINGS },
} },

{ SP_CHAOSKIN, {
    "Ch",
    "Chaoskin", "Chaotic", nullptr,
    SPF_NONE,
    1, 1, 0, 3,
    MONS_HUMAN,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    9, 9, 9, // 27
    { STAT_STR, STAT_INT, STAT_DEX }, 5,
    { { MUT_EVOLUTION, 1, 3 }, { MUT_EVOLUTION, 1, 12 }, },
    { "You are occasionally noticed by Xom."},
    {},
    { JOB_BERSERKER, JOB_CONJURER, JOB_NECROMANCER, JOB_FIRE_ELEMENTALIST,
      JOB_ICE_ELEMENTALIST },
    { SK_MACES_FLAILS, SK_AXES, SK_POLEARMS, SK_LONG_BLADES, SK_STAVES,
      SK_BOWS, SK_CROSSBOWS, SK_SLINGS },
} },

{ SP_CRYSTAL_DWARF, {
    "DD",
    "Crystal Dwarf", "Dwarven", "Dwarf",
    SPF_NONE,
    -1, -1, 0, 4,
    MONS_DEEP_DWARF,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    9, 9, 7, // 25
    { STAT_STR, STAT_INT }, 4,
    { },
    {"Your crystal skin shields you.", "Your crystal skin is reflective."},
    {"crystal skin", "reflection" },
    { JOB_FIGHTER, JOB_ICE_ELEMENTALIST, JOB_AIR_ELEMENTALIST,
      JOB_EARTH_ELEMENTALIST, JOB_FIRE_ELEMENTALIST, JOB_VENOM_MAGE },
    { SK_AXES, SK_CROSSBOWS },
} },

{ SP_DEEP_DWARF, {
    "DD",
    "Deep Dwarf", "Dwarven", "Dwarf",
    SPF_NONE,
    -1, 2, 0, 6,
    MONS_DEEP_DWARF,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    11, 8, 8, // 27
    { STAT_STR, STAT_INT }, 4,
    { { MUT_NO_REGENERATION, 1, 1 }, { MUT_PASSIVE_MAPPING, 1, 1 },
      { MUT_PASSIVE_MAPPING, 1, 9 }, { MUT_PASSIVE_MAPPING, 1, 18 },
      { MUT_NEGATIVE_ENERGY_RESISTANCE, 1, 14 }, },
    { "You are resistant to damage.",
      "You can heal yourself by infusing magical energy." },
    { "damage resistance", "heal wounds" },
    { JOB_FIGHTER, JOB_HUNTER, JOB_BERSERKER, JOB_NECROMANCER,
      JOB_EARTH_ELEMENTALIST, JOB_BLOOD_KNIGHT },
    { SK_MACES_FLAILS, SK_AXES, SK_LONG_BLADES, SK_CROSSBOWS, SK_SLINGS },
} },

{ SP_OBSIDIAN_DWARF, {
    "OD",
    "Obsidian Dwarf", "Dwarven", "Dwarf",
    SPF_NONE,
    -1, 1, 0, 3,
    MONS_DEEP_DWARF,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    9, 10, 6, // 25
    { STAT_STR, STAT_INT }, 4,
    { },
    { "You have no magical power.", "You expend HP in place of MP.", 
      "You both give and receive extra harm." },
    { "no MP", "cast from HP", "harm" },
    { JOB_BERSERKER, JOB_FIRE_ELEMENTALIST, JOB_EARTH_ELEMENTALIST, JOB_CONJURER },
    { SK_MACES_FLAILS, SK_AXES, SK_LONG_BLADES, SK_CROSSBOWS, SK_SLINGS },
} },

{ SP_DEEP_ELF, {
    "DE",
    "Deep Elf", "Elven", "Elf",
    SPF_ELVEN,
    -1, -2, 2, 4,
    MONS_ELF,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    5, 12, 10, // 27
    { STAT_INT, STAT_DEX }, 4,
    {},
    {},
    {},
    { JOB_WIZARD, JOB_CONJURER, JOB_SUMMONER, JOB_NECROMANCER,
      JOB_FIRE_ELEMENTALIST, JOB_ICE_ELEMENTALIST, JOB_AIR_ELEMENTALIST,
      JOB_EARTH_ELEMENTALIST },
    { SK_LONG_BLADES, SK_STAVES, SK_BOWS },
} },

{ SP_EMBER_ELF, {
    "EE",
    "Ember Elf", "Elven", "Elf",
    SPF_ELVEN,
    -1, -1, 2, 5,
    MONS_ELF,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    6, 10, 11, // 27
    { STAT_INT }, 4,
    {{ MUT_LOW_MAGIC, 3, 1 }, { MUT_MP_WANDS, 1, 1 }, 
     { MUT_WILD_MAGIC, 1, 7 }, { MUT_WILD_MAGIC, 1, 14 }, 
     { MUT_WILD_MAGIC, 1, 21 }, },
    {},
    {},
    { JOB_WIZARD, JOB_CONJURER, JOB_SUMMONER, JOB_NECROMANCER,
      JOB_FIRE_ELEMENTALIST, JOB_ICE_ELEMENTALIST, JOB_AIR_ELEMENTALIST,
      JOB_EARTH_ELEMENTALIST, JOB_VENOM_MAGE, JOB_ANNIHILATOR },
    { SK_SHORT_BLADES, SK_STAVES, SK_BOWS },
} },

{ SP_PROFOUND_ELF, {
    "PE",
    "Profound Elf", "Elven", "Elf",
    SPF_ELVEN,
    -2, -4, 4, 5,
    MONS_ELF,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    2, 16, 12, // 30
    { STAT_INT }, 2,
    {},
    {},
    {},
    { JOB_WIZARD, JOB_CONJURER, JOB_SUMMONER, JOB_NECROMANCER,
      JOB_FIRE_ELEMENTALIST, JOB_ICE_ELEMENTALIST, JOB_AIR_ELEMENTALIST,
      JOB_EARTH_ELEMENTALIST, JOB_VENOM_MAGE, JOB_ANNIHILATOR },
    { SK_SHORT_BLADES, SK_STAVES, SK_BOWS },
} },

{ SP_DEMIGOD, {
    "Dg",
    "Demigod", "Divine", nullptr,
    SPF_NONE,
    -2, 1, 2, 4,
    MONS_DEMIGOD,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    11, 12, 11, // 34
    set<stat_type>(), 28, // No natural stat gain (double chosen instead)
    { {MUT_HIGH_MAGIC, 1, 1} },
    {},
    {},
    { JOB_TRANSMUTER, JOB_CONJURER, JOB_FIRE_ELEMENTALIST, JOB_ICE_ELEMENTALIST,
      JOB_AIR_ELEMENTALIST, JOB_EARTH_ELEMENTALIST, JOB_NOBLE },
    { SK_MACES_FLAILS, SK_AXES, SK_POLEARMS, SK_LONG_BLADES, SK_STAVES,
      SK_BOWS, SK_CROSSBOWS, SK_SLINGS },
} },

{ SP_BASE_DRACONIAN, {
    "Dr",
    "Draconian", nullptr, nullptr,
    SPF_DRACONIAN,
    -1, 1, 0, 3,
    MONS_DRACONIAN,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    10, 8, 6, // 24
    { STAT_STR, STAT_INT, STAT_DEX }, 4,
    { { MUT_COLD_BLOODED, 1, 1 }, },
    {},
    {},
    { JOB_BERSERKER, JOB_TRANSMUTER, JOB_CONJURER, JOB_FIRE_ELEMENTALIST,
      JOB_ICE_ELEMENTALIST, JOB_AIR_ELEMENTALIST, JOB_EARTH_ELEMENTALIST,
      JOB_VENOM_MAGE, JOB_ANNIHILATOR, JOB_SORCERER },
    { SK_MACES_FLAILS, SK_AXES, SK_POLEARMS, SK_LONG_BLADES, SK_STAVES,
      SK_BOWS, SK_CROSSBOWS, SK_SLINGS },
} },

{ SP_RED_DRACONIAN, {
    "Dr",
    "Red Draconian", "Draconian", "Draconian",
    SPF_DRACONIAN,
    -1, 1, 0, 3,
    MONS_RED_DRACONIAN,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    10, 8, 6, // 24
    { STAT_STR, STAT_INT, STAT_DEX }, 4,
    { { MUT_COLD_BLOODED, 1, 1 }, { MUT_HEAT_RESISTANCE, 1, 7 }, },
    { "You can breathe blasts of fire." },
    { "breathe fire" },
    {}, // not a starting race
    {}, // not a starting race
} },

{ SP_WHITE_DRACONIAN, {
    "Dr",
    "White Draconian", "Draconian", "Draconian",
    SPF_DRACONIAN,
    -1, 1, 0, 3,
    MONS_WHITE_DRACONIAN,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    10, 8, 6, // 24
    { STAT_STR, STAT_INT, STAT_DEX }, 4,
    { { MUT_COLD_BLOODED, 1, 1 }, { MUT_COLD_RESISTANCE, 1, 7 }, },
    { "You can breathe waves of freezing cold.",
      "You can buffet flying creatures when you breathe cold." },
    { "breathe frost" },
    {}, // not a starting race
    {}, // not a starting race
} },

{ SP_GREEN_DRACONIAN, {
    "Dr",
    "Green Draconian", "Draconian", "Draconian",
    SPF_DRACONIAN,
    -1, 1, 0, 3,
    MONS_GREEN_DRACONIAN,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    10, 8, 6, // 24
    { STAT_STR, STAT_INT, STAT_DEX }, 4,
    { { MUT_COLD_BLOODED, 1, 1 }, { MUT_POISON_RESISTANCE, 1, 7 },
      { MUT_STINGER, 1, 14 }, },
    { "You can breathe blasts of noxious fumes." },
    { "breathe noxious fumes" },
    {}, // not a starting race
    {}, // not a starting race
} },

{ SP_YELLOW_DRACONIAN, {
    "Dr",
    "Yellow Draconian", "Draconian", "Draconian",
    SPF_DRACONIAN,
    -1, 1, 0, 3,
    MONS_YELLOW_DRACONIAN,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    10, 8, 6, // 24
    { STAT_STR, STAT_INT, STAT_DEX }, 4,
    { { MUT_COLD_BLOODED, 1, 1 }, { MUT_ACIDIC_BITE, 1, 14 }, },
    { "You can spit globs of acid.", "You are resistant to acid." },
    { "spit acid", "acid resistance" },
    {}, // not a starting race
    {}, // not a starting race
} },

{ SP_GREY_DRACONIAN, {
    "Dr",
    "Grey Draconian", "Draconian", "Draconian",
    SPF_DRACONIAN,
    -1, 1, 0, 3,
    MONS_GREY_DRACONIAN,
    HT_AMPHIBIOUS, US_ALIVE, SIZE_MEDIUM,
    10, 8, 6, // 24
    { STAT_STR, STAT_INT, STAT_DEX }, 4,
    { { MUT_COLD_BLOODED, 1, 1 }, { MUT_UNBREATHING, 1, 7 }, },
    { "You can walk through water." },
    { "walk through water" },
    {}, // not a starting race
    {}, // not a starting race
} },

{ SP_BLACK_DRACONIAN, {
    "Dr",
    "Black Draconian", "Draconian", "Draconian",
    SPF_DRACONIAN,
    -1, 1, 0, 3,
    MONS_BLACK_DRACONIAN,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    10, 8, 6, // 24
    { STAT_STR, STAT_INT, STAT_DEX }, 4,
    { { MUT_COLD_BLOODED, 1, 1 }, { MUT_SHOCK_RESISTANCE, 1, 7 },
      { MUT_BIG_WINGS, 1, 14 }, },
    { "You can breathe wild blasts of lightning." },
    { "breathe lightning" },
    {}, // not a starting race
    {}, // not a starting race
} },

{ SP_PURPLE_DRACONIAN, {
    "Dr",
    "Purple Draconian", "Draconian", "Draconian",
    SPF_DRACONIAN,
    -1, 1, 0, 6,
    MONS_PURPLE_DRACONIAN,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    10, 8, 6, // 24
    { STAT_STR, STAT_INT, STAT_DEX }, 4,
    { { MUT_COLD_BLOODED, 1, 1 }, },
    { "You can breathe bolts of dispelling energy." },
    { "breathe power" },
    {}, // not a starting race
    {}, // not a starting race
} },

#if TAG_MAJOR_VERSION == 34
{ SP_MOTTLED_DRACONIAN, {
    "Dr",
    "Mottled Draconian", "Draconian", "Draconian",
    SPF_DRACONIAN,
    -1, 1, 0, 3,
    MONS_MOTTLED_DRACONIAN,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    10, 8, 6, // 24
    { STAT_STR, STAT_INT, STAT_DEX }, 4,
    { { MUT_COLD_BLOODED, 1, 1 }, },
    { "You can spit globs of burning liquid.",
      "You can ignite nearby creatures when you spit burning liquid." },
    { "breathe sticky flame splash" },
    {}, // not a starting race
    {}, // not a starting race
} },
#endif

{ SP_PALE_DRACONIAN, {
    "Dr",
    "Pale Draconian", "Draconian", "Draconian",
    SPF_DRACONIAN,
    -1, 1, 0, 3,
    MONS_PALE_DRACONIAN,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    10, 8, 6, // 24
    { STAT_STR, STAT_INT, STAT_DEX }, 4,
    { { MUT_COLD_BLOODED, 1, 1 }, },
    { "You can breathe blasts of scalding, opaque steam." },
    { "breathe steam" },
    {}, // not a starting race
    {}, // not a starting race
} },

{ SP_DEMONSPAWN, {
    "Ds",
    "Demonspawn", "Demonic", nullptr,
    SPF_NONE,
    -1, 0, 0, 3,
    MONS_DEMONSPAWN,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    8, 9, 8, // 25
    { STAT_STR, STAT_INT, STAT_DEX }, 4,
    {},
    {},
    {},
    { JOB_GLADIATOR, JOB_BERSERKER, JOB_ABYSSAL_KNIGHT, JOB_WIZARD,
      JOB_NECROMANCER, JOB_FIRE_ELEMENTALIST, JOB_ICE_ELEMENTALIST,
      JOB_BLOOD_KNIGHT },
    { SK_MACES_FLAILS, SK_AXES, SK_POLEARMS, SK_LONG_BLADES, SK_STAVES,
      SK_BOWS, SK_CROSSBOWS, SK_SLINGS },
} },

{ SP_FELID, {
    "Fe",
    "Felid", "Feline", "Cat",
    SPF_NONE,
    -1, -4, 1, 6,
    MONS_FELID,
    HT_LAND, US_ALIVE, SIZE_LITTLE,
    4, 9, 11, // 24
    { STAT_INT, STAT_DEX }, 5,
    { { MUT_CARNIVOROUS, 1, 1 }, { MUT_FAST, 1, 1 }, { MUT_FANGS, 3, 1 },
      { MUT_SHAGGY_FUR, 1, 1 }, { MUT_ACUTE_VISION, 1, 1 }, { MUT_PAWS, 1, 1 },
      { MUT_SLOW_METABOLISM, 1, 1 }, { MUT_CLAWS, 1, 1 },
      { MUT_SHAGGY_FUR, 1, 6 }, { MUT_SHAGGY_FUR, 1, 12 }, },
    { "You cannot wear armour.",
      "You are incapable of wielding weapons or throwing items.",
      "Your paws allow you to move quietly. (Stealth)" },
    { "no armour", "no weapons or thrown items", "stealth" },
    { JOB_BERSERKER, JOB_ENCHANTER, JOB_TRANSMUTER, JOB_ICE_ELEMENTALIST,
      JOB_CONJURER, JOB_SUMMONER, JOB_AIR_ELEMENTALIST, JOB_VENOM_MAGE },
    { SK_UNARMED_COMBAT },
} },

{ SP_FELID_MUMMY, {
    "FM",
    "Felid Mummy", "Feline", "Cat",
    SPF_NONE,
    -1, -4, 1, 6,
    MONS_FELID,
    HT_LAND, US_UNDEAD, SIZE_LITTLE,
    4, 9, 11, // 24
    { STAT_INT, STAT_DEX }, 5,
    { { MUT_ACUTE_VISION, 1, 1 }, { MUT_NEGATIVE_ENERGY_RESISTANCE, 3, 1 }, 
      { MUT_COLD_RESISTANCE, 1, 1 }, { MUT_TORMENT_RESISTANCE, 1, 1 },
      { MUT_UNBREATHING, 1, 1 }, { MUT_NECRO_ENHANCER, 1, 13 }, 
      { MUT_NECRO_ENHANCER, 1, 26 }, },
    { "You do not eat or drink.",
      "Your flesh is vulnerable to fire." },
    { "no food or potions", "fire vulnerability" },
    { JOB_WIZARD, JOB_CONJURER, JOB_NECROMANCER, JOB_ICE_ELEMENTALIST,
      JOB_FIRE_ELEMENTALIST, JOB_SUMMONER },
    { SK_UNARMED_COMBAT },
} },

{ SP_FORMICID, {
    "Fo",
    "Formicid", nullptr, "Ant",
    SPF_NONE,
    1, 0, 0, 4,
    MONS_FORMICID,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    12, 7, 6, // 25
    { STAT_STR, STAT_INT }, 4,
    { { MUT_ANTENNAE, 3, 1 }, },
    { "You cannot be hasted, slowed, berserked, paralysed or teleported.",
      "You can dig through walls and to a lower floor.",
      "Your four strong arms can wield two-handed weapons with a shield." },
    { "permanent stasis", "dig shafts and tunnels", "four strong arms" },
    { JOB_FIGHTER, JOB_HUNTER, JOB_ABYSSAL_KNIGHT, JOB_ARCANE_MARKSMAN,
      JOB_EARTH_ELEMENTALIST, JOB_VENOM_MAGE, JOB_TORPOR_KNIGHT },
    { SK_MACES_FLAILS, SK_AXES, SK_POLEARMS, SK_LONG_BLADES, SK_STAVES,
      SK_CROSSBOWS, SK_SLINGS },
} },

{ SP_FUNGOID, {
    "Fu",
    "Fungoid", nullptr, "Mushroom",
    SPF_NONE,
    0, 1, 0, 4,
    MONS_WANDERING_MUSHROOM,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    9, 8, 8, // 25
    { STAT_STR, STAT_INT, STAT_DEX }, 4,
    { { MUT_SLOW, 3, 1 }, { MUT_NEGATIVE_ENERGY_RESISTANCE, 1, 1 }, },
    { "You have roots in place of feet.",
      "You sometimes release spores when you attack." },
    { "no boots", "spores" },
    { JOB_HUNTER, JOB_ARCANE_MARKSMAN, JOB_EARTH_ELEMENTALIST },
    { SK_MACES_FLAILS, SK_POLEARMS, SK_LONG_BLADES, SK_BOWS,
      SK_CROSSBOWS, SK_SLINGS },
} },

{ SP_GARGOYLE, {
    "Gr",
    "Gargoyle", nullptr, nullptr,
    SPF_NO_HAIR,
    0, -2, 0, 3,
    MONS_GARGOYLE,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    11, 8, 5, // 24
    { STAT_STR, STAT_INT }, 4,
    { { MUT_ROT_IMMUNITY, 1, 1 }, { MUT_NEGATIVE_ENERGY_RESISTANCE, 1, 1 },
      { MUT_PETRIFICATION_RESISTANCE, 1, 1 }, { MUT_SHOCK_RESISTANCE, 1, 1 },
      { MUT_UNBREATHING, 1, 1 }, { MUT_BIG_WINGS, 1, 14 }, },
    { "You are resistant to torment." },
    {},
    { JOB_FIGHTER, JOB_GLADIATOR, JOB_MONK, JOB_BERSERKER,
      JOB_FIRE_ELEMENTALIST, JOB_ICE_ELEMENTALIST, JOB_EARTH_ELEMENTALIST,
      JOB_VENOM_MAGE },
    { SK_MACES_FLAILS, SK_STAVES, SK_BOWS, SK_CROSSBOWS },
} },

{ SP_GHOUL, {
    "Gh",
    "Ghoul", "Ghoulish", nullptr,
    SPF_NO_HAIR,
    0, 1, -1, 3,
    MONS_GHOUL,
    HT_LAND, US_HUNGRY_DEAD, SIZE_MEDIUM,
    11, 3, 4, // 18
    { STAT_STR }, 5,
    { { MUT_CARNIVOROUS, 1, 1 }, { MUT_NEGATIVE_ENERGY_RESISTANCE, 3, 1 },
      { MUT_TORMENT_RESISTANCE, 1, 1 },
      { MUT_INHIBITED_REGENERATION, 1, 1 }, { MUT_COLD_RESISTANCE, 1, 1 },
      { MUT_CLAWS, 1, 1 }, { MUT_UNBREATHING, 1, 1 }, },
    { "Your body is rotting away.",
      "You thrive on raw meat." },
    { "rotting body" },
    { JOB_WARPER, JOB_GLADIATOR, JOB_MONK, JOB_NECROMANCER,
      JOB_ICE_ELEMENTALIST, JOB_EARTH_ELEMENTALIST },
    { SK_UNARMED_COMBAT, SK_BOWS, SK_CROSSBOWS, SK_SLINGS },
} },

{ SP_GRUE, {
    "Gu",
    "Grue", "Gruesome", nullptr,
    SPF_NO_HAIR,
    0, 0, 0, 3,
    MONS_GHOUL,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    9, 7, 10, // 26
    { STAT_STR, STAT_DEX }, 5,
    { { MUT_NIGHTSTALKER, 1, 1 }, { MUT_NIGHTSTALKER, 1, 11 }, 
      { MUT_NIGHTSTALKER, 1, 21 }, },
    { "You devour your enemies."},
    { "devour"},
    { JOB_ASSASSIN, JOB_EARTH_ELEMENTALIST, JOB_TRANSMUTER,
      JOB_GLADIATOR, JOB_VENOM_MAGE },
    { SK_UNARMED_COMBAT, SK_LONG_BLADES, SK_MACES_FLAILS, SK_BOWS },
} },

{ SP_HALFLING, {
    "Ha",
    "Halfling", nullptr, nullptr,
    SPF_NONE,
    1, -1, 0, 3,
    MONS_HALFLING,
    HT_LAND, US_ALIVE, SIZE_SMALL,
    9, 6, 9, // 24
    { STAT_DEX }, 5,
    { { MUT_MUTATION_RESISTANCE, 1, 1 }, },
    {},
    {},
    { JOB_FIGHTER, JOB_HUNTER, JOB_BERSERKER, JOB_ABYSSAL_KNIGHT, JOB_SKALD },
    { SK_SHORT_BLADES, SK_LONG_BLADES, SK_AXES, SK_SLINGS },
} },

{ SP_HILL_ORC, {
    "HO",
    "Hill Orc", "Orcish", "Orc",
    SPF_ORCISH,
    0, 1, 0, 3,
    MONS_ORC,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    10, 8, 6, // 24
    { STAT_STR }, 5,
    {},
    {},
    {},
    { JOB_FIGHTER, JOB_MONK, JOB_BERSERKER, JOB_ABYSSAL_KNIGHT,
      JOB_NECROMANCER, JOB_FIRE_ELEMENTALIST, JOB_TORPOR_KNIGHT },
    { SK_MACES_FLAILS, SK_AXES, SK_POLEARMS, SK_LONG_BLADES },
} },

{ SP_HEDGEHOG, {
    "OC",
    "Hedgehog", nullptr, nullptr,
    SPF_NONE,
    1, -2, 1, 3,
    MONS_PORCUPINE,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    9, 6, 9, // 24
    { STAT_STR, STAT_DEX }, 5,
    { { MUT_SCREAM, 3, 1 }, { MUT_FAST, 3, 1 }, { MUT_SPINY, 1, 1 }, 
	  { MUT_SPINY, 1, 11 }, { MUT_SPINY, 1, 21 } },
    { "You always go fast, even when slowed."},
    { "gotta go fast"},
    { JOB_BERSERKER, JOB_MONK, JOB_AIR_ELEMENTALIST, JOB_WARPER },
    { SK_SHORT_BLADES, SK_LONG_BLADES, SK_STAVES, SK_SLINGS },
} },

{ SP_HUMAN, {
    "Hu",
    "Human", nullptr, nullptr,
    SPF_NONE,
    1, 0, 0, 3,
    MONS_HUMAN,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    8, 8, 8, // 24
    { STAT_STR, STAT_INT, STAT_DEX }, 4,
    {},
    {},
    {},
    { JOB_BERSERKER, JOB_CONJURER, JOB_NECROMANCER, JOB_FIRE_ELEMENTALIST,
      JOB_ICE_ELEMENTALIST },
    { SK_MACES_FLAILS, SK_AXES, SK_POLEARMS, SK_LONG_BLADES, SK_STAVES,
      SK_BOWS, SK_CROSSBOWS, SK_SLINGS },
} },

{ SP_RATLING, {
    "Ra",
    "Ratling", nullptr, nullptr,
    SPF_NONE,
    5, -1, 0, 3,
    MONS_RAT,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    8, 8, 8, // 24
    { STAT_STR, STAT_INT, STAT_DEX }, 4,
    { { MUT_FANGS, 2, 1 }, { MUT_POISON_ENHANCER, 1, 12 }, },
    { "Your bite is poisonous."},
    { "poisonous bite"},
    { JOB_BERSERKER, JOB_VENOM_MAGE, JOB_GLADIATOR },
    { SK_MACES_FLAILS, SK_AXES, SK_POLEARMS, SK_LONG_BLADES, SK_STAVES,
      SK_BOWS, SK_CROSSBOWS, SK_SLINGS },
} },

{ SP_KITSUNE, {
    "Ki",
    "Kitsune", nullptr, "Fox",
    SPF_NONE,
    1, -1, 0, 6,
    MONS_HUMAN,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    5, 11, 11, // 27
    { STAT_INT, STAT_DEX }, 4,
    { {MUT_CLAWS, 1, 1 }, },
    {"You can charm monsters.", "You have fox ears (no helmets)."},
    {"charm monsters", "no helmets"},
    { JOB_WIZARD, JOB_TRANSMUTER, JOB_EARTH_ELEMENTALIST },
    { SK_UNARMED_COMBAT, SK_BOWS, SK_CROSSBOWS, SK_SLINGS },
} },

{ SP_KOBOLD, {
    "Ko",
    "Kobold", nullptr, nullptr,
    SPF_NONE,
    1, -2, 0, 3,
    MONS_KOBOLD,
    HT_LAND, US_ALIVE, SIZE_SMALL,
    5, 9, 10, // 24
    { STAT_STR, STAT_INT, STAT_DEX }, 5,
    { { MUT_CARNIVOROUS, 1, 1 }, },
    {},
    {},
    { JOB_HUNTER, JOB_BERSERKER, JOB_ARCANE_MARKSMAN, JOB_ENCHANTER,
      JOB_CONJURER, JOB_SUMMONER },
    { SK_SHORT_BLADES, SK_MACES_FLAILS, SK_STAVES, SK_CROSSBOWS },
} },

{ SP_LEPRECHAUN, {
    "Le",
    "Leprechaun", nullptr, nullptr,
    SPF_NONE,
    0, -2, 0, 3,
    MONS_BOGGART,
    HT_LAND, US_ALIVE, SIZE_SMALL,
    6, 6, 12, // 24
    { STAT_INT, STAT_DEX }, 5,
    {},
    {"You are lucky with gold and items."},
    {"luck"},
    { JOB_SKALD, JOB_WARPER, JOB_WIZARD, JOB_MERCHANT },
    { SK_MACES_FLAILS, SK_LONG_BLADES, SK_CROSSBOWS },
} },

{ SP_MERFOLK, {
    "Mf",
    "Merfolk", "Merfolkian", nullptr,
    SPF_NONE,
    0, 0, 0, 3,
    MONS_MERFOLK,
    HT_WATER, US_ALIVE, SIZE_MEDIUM,
    8, 7, 9, // 24
    { STAT_STR, STAT_INT, STAT_DEX }, 5,
    {},
    { "You revert to your normal form in water.",
      "You are very nimble and swift while swimming.",
      "You are very stealthy in the water. (Stealth+)" },
    { "change form in water", "swift swim", "stealthy swim" },
    { JOB_GLADIATOR, JOB_BERSERKER, JOB_SKALD, JOB_TRANSMUTER, JOB_SUMMONER,
      JOB_ICE_ELEMENTALIST, JOB_VENOM_MAGE },
    { SK_POLEARMS, SK_LONG_BLADES },
} },

{ SP_MINOTAUR, {
    "Mi",
    "Minotaur", nullptr, nullptr,
    SPF_NONE,
    -1, 1, -1, 3,
    MONS_MINOTAUR,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    12, 5, 5, // 22
    { STAT_STR, STAT_DEX }, 4,
    { { MUT_HORNS, 2, 1 }, },
    { "You reflexively headbutt those who attack you in melee." },
    { "retaliatory headbutt" },
    { JOB_FIGHTER, JOB_GLADIATOR, JOB_MONK, JOB_HUNTER, JOB_BERSERKER,
      JOB_ABYSSAL_KNIGHT, JOB_SLIME_PRIEST },
    { SK_MACES_FLAILS, SK_AXES, SK_POLEARMS, SK_LONG_BLADES, SK_STAVES,
      SK_BOWS, SK_CROSSBOWS, SK_SLINGS },
} },

{ SP_MUMMY, {
    "Mu",
    "Mummy", nullptr, nullptr,
    SPF_NONE,
    -1, 0, 0, 5,
    MONS_MUMMY,
    HT_LAND, US_UNDEAD, SIZE_MEDIUM,
    11, 7,  7, // 25
    { STAT_STR, STAT_INT, STAT_DEX }, 5,
    { { MUT_NEGATIVE_ENERGY_RESISTANCE, 3, 1 }, { MUT_COLD_RESISTANCE, 1, 1 },
      { MUT_TORMENT_RESISTANCE, 1, 1 },
      { MUT_UNBREATHING, 1, 1 },
      { MUT_NECRO_ENHANCER, 1, 13 }, { MUT_NECRO_ENHANCER, 1, 26 }, },
    { "You do not eat or drink.",
      "Your flesh is vulnerable to fire." },
    { "no food or potions", "fire vulnerability" },
    { JOB_WIZARD, JOB_CONJURER, JOB_NECROMANCER, JOB_ICE_ELEMENTALIST,
      JOB_FIRE_ELEMENTALIST, JOB_SUMMONER },
    { SK_MACES_FLAILS, SK_AXES, SK_POLEARMS, SK_LONG_BLADES, SK_STAVES,
      SK_BOWS, SK_CROSSBOWS, SK_SLINGS },
} },

{ SP_NAGA, {
    "Na",
    "Naga", nullptr, nullptr,
    SPF_SMALL_TORSO,
    0, 2, 0, 5,
    MONS_NAGA,
    HT_LAND, US_ALIVE, SIZE_LARGE,
    10, 8, 6, // 24
    { STAT_STR, STAT_INT, STAT_DEX }, 4,
    { { MUT_ACUTE_VISION, 1, 1 }, { MUT_SLOW, 2, 1 },  { MUT_DEFORMED, 1, 1 },
      { MUT_SPIT_POISON, 1, 1 },  { MUT_POISON_RESISTANCE, 1, 1 },
      { MUT_SLOW_METABOLISM, 1, 1 }, { MUT_CONSTRICTING_TAIL, 1, 13 } },
    { "You cannot wear boots." },
    {},
    { JOB_BERSERKER, JOB_TRANSMUTER, JOB_ENCHANTER, JOB_FIRE_ELEMENTALIST,
      JOB_ICE_ELEMENTALIST, JOB_WARPER, JOB_WIZARD, JOB_VENOM_MAGE },
    { SK_MACES_FLAILS, SK_AXES, SK_POLEARMS, SK_LONG_BLADES, SK_BOWS,
      SK_CROSSBOWS, SK_SLINGS },
} },

{ SP_GOLEM, {
    "Go",
    "Golem", nullptr, nullptr,
    SPF_NONE,
    -2, 2, 0, 6,
    MONS_TOENAIL_GOLEM,
    HT_LAND, US_ALIVE, SIZE_LARGE,
    20, 20, 20, // 60, no growth
    set<stat_type>(), 28,
    { { MUT_NEGATIVE_ENERGY_RESISTANCE, 3, 1 }, { MUT_TORMENT_RESISTANCE, 1, 1 },	
      { MUT_UNBREATHING, 1, 1 }, { MUT_ROT_IMMUNITY, 1, 1 }, { MUT_COLD_RESISTANCE, 1, 8 },
      { MUT_HEAT_RESISTANCE, 1, 8 }, { MUT_SHOCK_RESISTANCE, 1, 16 }, 
      { MUT_COLD_RESISTANCE, 1, 24 }, { MUT_HEAT_RESISTANCE, 1, 24 } },
    { "You cannot use equipment.", "You do not eat." },
    {},
    { JOB_EARTH_ELEMENTALIST, JOB_SUMMONER, JOB_MONK},
    { SK_UNARMED_COMBAT },
} },

{ SP_ROBOT, {
    "Ro",
    "Robot", nullptr, nullptr,
    SPF_NONE,
    -1, 0, -1, 3,
    MONS_IRON_GOLEM,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    9, 11, 4, // 24
    { STAT_STR, STAT_INT }, 4,
    { { MUT_NEGATIVE_ENERGY_RESISTANCE, 3, 1 }, { MUT_TORMENT_RESISTANCE, 1, 1 },	
      { MUT_UNBREATHING, 1, 1 }, { MUT_ROT_IMMUNITY, 1, 1 }, {MUT_LASER_BREATH, 1, 5} },
    { "You break items when uninstalling them.", "You are sometimes slowed by electricity and fire.", 
      "You do not eat." },
    { "fragile equipment", "slowed by fire and elec", "no food"},
    { JOB_FIGHTER, JOB_GLADIATOR, JOB_ARTIFICER },
    { SK_MACES_FLAILS, SK_AXES, SK_POLEARMS, SK_LONG_BLADES, SK_STAVES,
      SK_BOWS, SK_CROSSBOWS, SK_SLINGS },
} },

{ SP_OGRE, {
    "Og",
    "Ogre", "Ogreish", nullptr,
    SPF_NONE,
    0, 3, 0, 4,
    MONS_OGRE,
    HT_LAND, US_ALIVE, SIZE_LARGE,
    11, 9, 4, // 24
    { STAT_STR }, 3,
    { { MUT_TOUGH_SKIN, 1, 1 }, },
    {},
    {},
    { JOB_HUNTER, JOB_BERSERKER, JOB_ARCANE_MARKSMAN, JOB_WIZARD,
      JOB_FIRE_ELEMENTALIST, JOB_SORCERER },
    { SK_MACES_FLAILS, SK_POLEARMS, SK_STAVES, SK_AXES },
} },

{ SP_OCTOPODE, {
    "Op",
    "Octopode", "Octopoid", "Octopus",
    SPF_NO_HAIR,
    0, -1, 0, 3,
    MONS_OCTOPODE,
    HT_WATER, US_ALIVE, SIZE_MEDIUM,
    7, 10, 7, // 24
    { STAT_STR, STAT_INT, STAT_DEX }, 5,
    { { MUT_CAMOUFLAGE, 1, 1 }, { MUT_GELATINOUS_BODY, 1, 1 }, },
    { "You cannot wear most types of armour.",
      "You are very stealthy in the water. (Stealth+)" },
    { "almost no armour", "stealthy swim" },
    { JOB_TRANSMUTER, JOB_WIZARD, JOB_CONJURER, JOB_ASSASSIN,
      JOB_FIRE_ELEMENTALIST, JOB_ICE_ELEMENTALIST, JOB_EARTH_ELEMENTALIST,
      JOB_VENOM_MAGE, JOB_SLIME_PRIEST },
    { SK_MACES_FLAILS, SK_AXES, SK_POLEARMS, SK_LONG_BLADES, SK_STAVES,
      SK_BOWS, SK_CROSSBOWS, SK_SLINGS },
} },

{ SP_UNIPODE, {
    "Up",
    "Unipode", "Unipoid", "Unipus",
    SPF_NO_HAIR,
    0, -1, 0, 3,
    MONS_OCTOPODE,
    HT_WATER, US_ALIVE, SIZE_MEDIUM,
    7, 10, 7, // 24
    { STAT_STR, STAT_INT, STAT_DEX }, 5,
    { { MUT_CAMOUFLAGE, 1, 1 }, { MUT_GELATINOUS_BODY, 1, 1 }, },
    { "You cannot wear most types of armour.",
      "You are very stealthy in the water. (Stealth+)" },
    { "almost no armour", "stealthy swim" },
    { JOB_TRANSMUTER, JOB_WIZARD, JOB_CONJURER, JOB_ASSASSIN,
      JOB_FIRE_ELEMENTALIST, JOB_ICE_ELEMENTALIST, JOB_EARTH_ELEMENTALIST,
      JOB_VENOM_MAGE, JOB_SLIME_PRIEST },
    { SK_MACES_FLAILS, SK_AXES, SK_POLEARMS, SK_LONG_BLADES, SK_STAVES,
      SK_BOWS, SK_CROSSBOWS, SK_SLINGS },
} },

{ SP_SPRIGGAN, {
    "Sp",
    "Spriggan", nullptr, nullptr,
    SPF_NONE,
    -1, -3, 1, 7,
    MONS_SPRIGGAN,
    HT_LAND, US_ALIVE, SIZE_LITTLE,
    4, 9, 11, // 24
    { STAT_INT, STAT_DEX }, 5,
    { { MUT_FAST, 3, 1 }, { MUT_HERBIVOROUS, 1, 1 },
      { MUT_ACUTE_VISION, 1, 1 }, { MUT_SLOW_METABOLISM, 2, 1 }, },
    {},
    {},
    { JOB_ASSASSIN, JOB_ARTIFICER, JOB_ABYSSAL_KNIGHT, JOB_WARPER,
      JOB_ENCHANTER, JOB_CONJURER, JOB_EARTH_ELEMENTALIST, JOB_VENOM_MAGE, 
      JOB_ANNIHILATOR },
    { SK_SHORT_BLADES, SK_SLINGS },
} },

{ SP_WATER_SPRITE, {
    "WS",
    "Water Sprite", nullptr, "Sprite",
    SPF_NONE,
    0, -4, 1, 5,
    MONS_WATER_NYMPH,
    HT_WATER, US_ALIVE, SIZE_MEDIUM,
    6, 10, 10, // 26
    { STAT_INT, STAT_DEX }, 5,
    { { MUT_SLOW, 3, 1 }, { MUT_ACUTE_VISION, 1, 1 }, },
    {"You heal instantly when no monsters are nearby.", 
     "You cannot read scrolls in the presence of monsters.",
     "You can move somewhat faster in water.",
	 "You are resistant to damage."},
    {"heal when alone", "no scrolls in combat", "faster in water", "damage resistance"},
    { JOB_ASSASSIN, JOB_WARPER, JOB_HUNTER },
    { SK_LONG_BLADES, SK_STAVES, SK_POLEARMS, SK_BOWS, SK_CROSSBOWS },
} },

{ SP_TENGU, {
    "Te",
    "Tengu", nullptr, nullptr,
    SPF_NO_HAIR,
    0, -2, 1, 3,
    MONS_TENGU,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    8, 8, 9, // 25
    { STAT_STR, STAT_INT, STAT_DEX }, 4,
    { { MUT_BEAK, 1, 1 }, { MUT_TALONS, 3, 1 },
      { MUT_TENGU_FLIGHT, 1, 5 }, { MUT_TENGU_FLIGHT, 1, 14 }, },
    {},
    {},
    { JOB_BERSERKER, JOB_WIZARD, JOB_CONJURER, JOB_SUMMONER,
      JOB_FIRE_ELEMENTALIST, JOB_AIR_ELEMENTALIST, JOB_VENOM_MAGE },
    { SK_MACES_FLAILS, SK_AXES, SK_POLEARMS, SK_LONG_BLADES, SK_STAVES,
      SK_BOWS, SK_CROSSBOWS },
} },

{ SP_TROLL, {
    "Tr",
    "Troll", "Trollish", nullptr,
    SPF_NONE,
    -1, 3, -1, 3,
    MONS_TROLL,
    HT_LAND, US_ALIVE, SIZE_LARGE,
    15, 4, 5, // 24
    { STAT_STR }, 3,
    { { MUT_TOUGH_SKIN, 2, 1 }, { MUT_REGENERATION, 1, 1 }, { MUT_CLAWS, 3, 1 },
      { MUT_GOURMAND, 1, 1 }, { MUT_FAST_METABOLISM, 3, 1 },
      { MUT_SHAGGY_FUR, 1, 1 }, },
    {},
    {},
    { JOB_FIGHTER, JOB_MONK, JOB_HUNTER, JOB_BERSERKER, JOB_WARPER,
      JOB_EARTH_ELEMENTALIST, JOB_WIZARD, JOB_SLIME_PRIEST },
    { SK_UNARMED_COMBAT, SK_MACES_FLAILS },
} },

{ SP_VAMPIRE, {
    "Vp",
    "Vampire", "Vampiric", nullptr,
    SPF_NONE,
    -1, 0, 0, 4,
    MONS_VAMPIRE,
    HT_LAND, US_SEMI_UNDEAD, SIZE_MEDIUM,
    7, 10, 9, // 26
    { STAT_INT, STAT_DEX }, 5,
    { { MUT_FANGS, 3, 1 }, { MUT_ACUTE_VISION, 1, 1 },
      { MUT_UNBREATHING, 1, 1 }, },
    {},
    {},
    { JOB_GLADIATOR, JOB_ASSASSIN, JOB_ENCHANTER, JOB_EARTH_ELEMENTALIST,
      JOB_NECROMANCER, JOB_ICE_ELEMENTALIST },
    { SK_SHORT_BLADES, SK_AXES, SK_LONG_BLADES, SK_BOWS, SK_CROSSBOWS,
      SK_SLINGS },
} },

{ SP_VINE_STALKER, {
    "VS",
    "Vine Stalker", "Vine", "Vine",
    SPF_NONE,
    0, -3, 1, 5,
    MONS_VINE_STALKER,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    10, 8, 9, // 27
    { STAT_STR, STAT_DEX }, 4,
    { { MUT_FANGS, 2, 1 }, { MUT_FANGS, 1, 8 },
      { MUT_MANA_SHIELD, 1, 1 }, { MUT_ANTIMAGIC_BITE, 1, 1 },
      { MUT_NO_POTION_HEAL, 3, 1 }, { MUT_ROT_IMMUNITY, 1, 1 },
      { MUT_REGENERATION, 1, 4 }, { MUT_REGENERATION, 1, 12 }, },
    {},
    {},
    { JOB_FIGHTER, JOB_ASSASSIN, JOB_BERSERKER, JOB_ENCHANTER, JOB_CONJURER,
      JOB_NECROMANCER, JOB_ICE_ELEMENTALIST },
    { SK_MACES_FLAILS, SK_AXES, SK_POLEARMS, SK_LONG_BLADES, SK_STAVES,
      SK_BOWS, SK_CROSSBOWS, SK_SLINGS },
} },

{ SP_WEREWOLF, {
    "We",
    "Werewolf", nullptr, "Wolf",
    SPF_NONE,
    -1, 0, 0, 4,
    MONS_WOLF,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    10, 7, 9, // 26
    { STAT_STR, STAT_DEX }, 5,
    {},
    {"You sometimes transform uncontrollably when attacking."},
    {"werewolf transformation"},
    { JOB_MONK, JOB_GLADIATOR, JOB_BERSERKER, JOB_NECROMANCER },
    { SK_STAVES, SK_AXES, SK_LONG_BLADES, SK_MACES_FLAILS, SK_POLEARMS,
      SK_BOWS, SK_CROSSBOWS, SK_SLINGS },
} },

{ SP_YETI, {
    "Ye",
    "Yeti", nullptr, nullptr,
    SPF_NONE,
    0, 2, 0, 3,
    MONS_OGRE,
    HT_LAND, US_ALIVE, SIZE_LARGE,
    8, 9, 11, // 28
    { STAT_INT, STAT_DEX }, 5,
    { { MUT_PASSIVE_FREEZE, 1, 7 }, },
    {},
    {},
    { JOB_ICE_ELEMENTALIST, JOB_NECROMANCER, JOB_CONJURER },
    { SK_LONG_BLADES, SK_POLEARMS, SK_STAVES },
} },
#if TAG_MAJOR_VERSION == 34
{ SP_SLUDGE_ELF, {
    "SE",
    "Sludge Elf", "Elven", "Elf",
    SPF_ELVEN,
    0, -1, 1, 3,
    MONS_ELF,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    8, 8, 8, // 24
    { STAT_INT, STAT_DEX }, 4,
    {},
    {},
    {},
    {}, // not a starting race
    {}, // not a starting race
} },

{ SP_LAVA_ORC, {
    "LO",
    "Lava Orc", "Orcish", "Orc",
    SPF_ORCISH | SPF_NO_HAIR,
    -1, 1, 0, 3,
    MONS_LAVA_ORC,
    HT_AMPHIBIOUS_LAVA, US_ALIVE, SIZE_MEDIUM,
    10, 8, 6, // 24
    { STAT_INT, STAT_DEX }, 5,
    {},
    {},
    {},
    {}, // not a starting race
    {}, // not a starting race
} },

{ SP_DJINNI, {
    "Dj",
    "Djinni", "Djinn", nullptr,
    SPF_NONE,
    -1, -1, 0, 3,
    MONS_DJINNI,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    8, 8, 8, // 24
    { STAT_STR, STAT_INT, STAT_DEX }, 4,
    {},
    {},
    {},
    {}, // not a starting race
    {}, // not a starting race
} },

{ SP_HIGH_ELF, {
    "HE",
    "High Elf", "Elven", "Elf",
    SPF_ELVEN,
    -1, -1, 1, 4,
    MONS_ELF,
    HT_LAND, US_ALIVE, SIZE_MEDIUM,
    7, 11, 10, // 28
    { STAT_INT, STAT_DEX }, 3,
    {},
    {},
    {},
    { JOB_HUNTER, JOB_SKALD, JOB_WIZARD, JOB_CONJURER, JOB_FIRE_ELEMENTALIST,
      JOB_ICE_ELEMENTALIST, JOB_AIR_ELEMENTALIST },
    { SK_SHORT_BLADES, SK_LONG_BLADES, SK_STAVES, SK_BOWS },
} },
#endif
// Ideally this wouldn't be necessary...
{ SP_UNKNOWN, { // Line 1: enum
    "??", // Line 2: abbrev
    "Yak", nullptr, nullptr, // Line 3: name, genus name, adjectival name
    SPF_NONE, // Line 4: flags
    0, 0, 0, 0, // Line 5: XP, HP, MP, MR (gen-apt.pl needs them here!)
    MONS_PROGRAM_BUG, // Line 6: equivalent monster type
    HT_LAND, US_ALIVE, SIZE_MEDIUM, // Line 7: habitat, life, size
    0, 0, 0, // Line 8: str, int, dex
    set<stat_type>(), 28, // Line 9: str gain, int gain, dex gain, frequency
    {}, // Line 10: Mutations
    {}, // Line 11: Fake mutations
    {}, // Line 12: Fake mutations
    {}, // Line 13: Recommended jobs
    {}, // Line 14: Recommended weapons
} }
};
