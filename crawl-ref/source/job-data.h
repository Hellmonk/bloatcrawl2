enum weapon_choice
{
    WCHOICE_NONE,   ///< No weapon choice
    WCHOICE_PLAIN,  ///< Normal weapon choice
    WCHOICE_GOOD,   ///< Chooses from "good" weapons
    WCHOICE_RANGED, ///< Choice of ranged weapon
};

struct job_def
{
    const char* abbrev; ///< Two-letter abbreviation
    const char* name; ///< Long name
    int s, i, d; ///< Starting Str, Dex, and Int
    vector<species_type> recommended_species; ///< Which species are good at it
    /// Guaranteed starting equipment. Uses vault spec syntax, with the plus:,
    /// charges:, q:, and ego: tags supported.
    vector<string> equipment;
    weapon_choice wchoice; ///< how the weapon is chosen, if any
    vector<pair<skill_type, int>> skills; ///< starting skills
};

static const map<job_type, job_def> job_data =
{

{ JOB_AIR_ELEMENTALIST, {
    "AE", "Air Elementalist",
    0, 7, 5,
    { SP_DEEP_ELF, SP_TENGU, SP_BASE_DRACONIAN, SP_NAGA, SP_TITAN, },
    { "robe", "book of Air" },
    WCHOICE_NONE,
    { { SK_AIR_MAGIC, 4 },
      { SK_DODGING, 2 }, { SK_STEALTH, 2 }, },
} },

{ JOB_ARCANE_MARKSMAN, {
    "AM", "Arcane Marksman",
    3, 5, 4,
    { SP_FORMICID, SP_DEEP_ELF, SP_KOBOLD, SP_SPRIGGAN, SP_TROLL, },
    { "robe", "book of Arcane Marksmanship" },
    WCHOICE_RANGED,
    { { SK_DODGING, 2 },
      { SK_HEXES, 4 }, { SK_WEAPON, 2 }, },
} },

{ JOB_ARTIFICER, {
    "Ar", "Artificer",
    4, 3, 5,
    { SP_KOBOLD, SP_SPRIGGAN, SP_BASE_DRACONIAN,
      SP_DEMONSPAWN, },
    { "dagger", "leather armour", "wand of flame charges:15",
      "wand of enslavement charges:15", "wand of iceblast charges:1" },
    WCHOICE_NONE,
    { { SK_EVOCATIONS, 3 }, { SK_DODGING, 2 },
      { SK_WEAPON, 1 }, { SK_STEALTH, 1 }, },
} },

{ JOB_BERSERKER, {
    "Be", "Berserker",
    9, -1, 4,
    { SP_MOUNTAIN_DWARF, SP_MERFOLK, SP_MINOTAUR, SP_GARGOYLE,
      SP_DEMONSPAWN, },
    { "animal skin" },
    WCHOICE_PLAIN,
    { { SK_DODGING, 2 }, { SK_WEAPON, 3 }, },
} },

{ JOB_CHAOS_KNIGHT, {
    "CK", "Chaos Knight",
    4, 4, 4,
    { SP_MOUNTAIN_DWARF, SP_TROLL, SP_MERFOLK, SP_MINOTAUR,
      SP_BASE_DRACONIAN, SP_DEMONSPAWN, },
    { "leather armour plus:2" },
    WCHOICE_PLAIN,
    { { SK_ARMOUR, 1 }, { SK_DODGING, 1 },
      { SK_WEAPON, 3 } },
} },

{ JOB_EARTH_ELEMENTALIST, {
    "EE", "Earth Elementalist",
    0, 7, 5,
    { SP_DEEP_ELF, SP_SPRIGGAN, SP_GARGOYLE, SP_VINE_STALKER,
      SP_OCTOPODE, },
    { "book of Geomancy", "robe", },
    WCHOICE_NONE,
    { { SK_TRANSMUTATIONS, 2 }, { SK_EARTH_MAGIC, 4 },
      { SK_DODGING, 2 }, { SK_STEALTH, 2 }, }
} },

{ JOB_ENCHANTER, {
    "En", "Enchanter",
    0, 7, 5,
    { SP_DEEP_ELF, SP_KOBOLD, SP_SPRIGGAN, SP_NAGA, SP_VAMPIRE, },
    { "dagger plus:1", "robe plus:1", "book of Maledictions" },
    WCHOICE_NONE,
    { { SK_WEAPON, 1 }, { SK_HEXES, 4 },
      { SK_DODGING, 2 }, { SK_STEALTH, 3 }, },
} },

{ JOB_FIGHTER, {
    "Fi", "Fighter",
    8, 0, 4,
    { SP_MOUNTAIN_DWARF, SP_TROLL, SP_MINOTAUR, SP_GARGOYLE, SP_GNOLL },
    { "scale mail", "shield", "potion of augmentation" },
    WCHOICE_GOOD,
    { { SK_SHIELDS, 3 }, { SK_ARMOUR, 3 },
      { SK_WEAPON, 2 } },
} },

{ JOB_FIRE_ELEMENTALIST, {
    "FE", "Fire Elementalist",
    0, 7, 5,
    { SP_DEEP_ELF, SP_MOUNTAIN_DWARF, SP_NAGA, SP_TENGU, SP_DEMIGOD, SP_GARGOYLE, },
    { "robe", "book of Flames" },
    WCHOICE_NONE,
    { { SK_FIRE_MAGIC, 4 },
      { SK_DODGING, 2 }, { SK_STEALTH, 2 }, },
} },

{ JOB_GLADIATOR, {
    "Gl", "Gladiator",
    7, 0, 5,
    { SP_MOUNTAIN_DWARF, SP_MERFOLK, SP_MINOTAUR, SP_GARGOYLE, SP_GNOLL},
    { "leather armour", "helmet", "poisoned dart q:12", "curare dart q:3"},
    WCHOICE_GOOD,
    { { SK_THROWING, 2 }, { SK_DODGING, 3 },
      { SK_WEAPON, 3}, },
} },

{ JOB_HUNTER, {
    "Hu", "Hunter",
    4, 3, 5,
    { SP_MOUNTAIN_DWARF, SP_KOBOLD, SP_TROLL, SP_GNOLL},
    { "dagger", "leather armour" },
    WCHOICE_RANGED,
    { { SK_DODGING, 2 }, { SK_STEALTH, 1 },
      { SK_WEAPON, 4 }, },
} },

{ JOB_ICE_ELEMENTALIST, {
    "IE", "Ice Elementalist",
    0, 7, 5,
    { SP_DEEP_ELF, SP_MERFOLK, SP_NAGA, SP_BASE_DRACONIAN, SP_TITAN,
      SP_GARGOYLE, },
    { "robe", "book of Frost" },
    WCHOICE_NONE,
    { { SK_ICE_MAGIC, 4 },
      { SK_DODGING, 2 }, { SK_STEALTH, 2 }, },
} },

{ JOB_MONK, {
    "Mo", "Monk",
    3, 2, 7,
    { SP_MOUNTAIN_DWARF, SP_TROLL, SP_MERFOLK, SP_GARGOYLE, SP_DEMONSPAWN, },
    { "robe" },
    WCHOICE_PLAIN,
    { { SK_WEAPON, 3 }, { SK_DODGING, 3 },
      { SK_STEALTH, 2 }, {SK_INVOCATIONS, 3} },
} },

{ JOB_NECROMANCER, {
    "Ne", "Necromancer",
    0, 7, 5,
    { SP_DEEP_ELF, SP_MOUNTAIN_DWARF, SP_DEMONSPAWN, SP_MUMMY,
      SP_VAMPIRE, },
    { "robe", "book of Necromancy" },
    WCHOICE_NONE,
    { { SK_NECROMANCY, 5 }, { SK_DODGING, 2 },
      { SK_STEALTH, 2 }, },
} },

{ JOB_SKALD, {
    "Sk", "Skald",
    4, 4, 4,
    { SP_MERFOLK, SP_BASE_DRACONIAN, SP_VAMPIRE, SP_GNOLL},
    { "leather armour", "book of Battle" },
    WCHOICE_PLAIN,
    { { SK_ARMOUR, 1 }, { SK_DODGING, 1 },
      { SK_CHARMS, 4 }, { SK_WEAPON, 2 }, },
} },

{ JOB_SUMMONER, {
    "Su", "Summoner",
    0, 7, 5,
    { SP_DEEP_ELF, SP_MOUNTAIN_DWARF, SP_VINE_STALKER, SP_MERFOLK, SP_TENGU,
      SP_VAMPIRE, },
    { "robe", "book of Callings" },
    WCHOICE_NONE,
    { { SK_SUMMONINGS, 5 }, { SK_DODGING, 2 },
      { SK_STEALTH, 2 }, },
} },

{ JOB_TORPOR_KNIGHT, {
    "TK", "Torpor Knight",
    4, 4, 4,
    { SP_MINOTAUR, SP_FORMICID, SP_MOUNTAIN_DWARF, SP_DEMONSPAWN, },
    { "plate armour ego:ponderousness" },
    WCHOICE_PLAIN,
    { { SK_ARMOUR, 1 }, { SK_DODGING, 1 },
      { SK_WEAPON, 3 }, { SK_INVOCATIONS, 2 }, },
} },

{ JOB_TRANSMUTER, {
    "Tm", "Transmuter",
    2, 5, 5,
    { SP_NAGA, SP_MERFOLK, SP_BASE_DRACONIAN, SP_TITAN, SP_DEMONSPAWN,
      SP_TROLL, },
    { "robe", "book of Changes" },
    WCHOICE_NONE,
    { { SK_UNARMED_COMBAT, 3 }, { SK_DODGING, 2 },
      { SK_TRANSMUTATIONS, 3 }, },
} },

{ JOB_WANDERER, {
    "Wn", "Wanderer",
    0, 0, 0, // Randomised
    { SP_MOUNTAIN_DWARF, SP_SPRIGGAN, SP_MERFOLK, SP_BASE_DRACONIAN,
      SP_HUMAN, SP_DEMONSPAWN, },
    { }, // Randomised
    WCHOICE_NONE,
    { }, // Randomised
} },

{ JOB_WIZARD, {
    "Wz", "Wizard",
    -1, 10, 3,
    { SP_DEEP_ELF, SP_NAGA, SP_BASE_DRACONIAN, SP_OCTOPODE, SP_HUMAN,
      SP_MUMMY, },
    { "robe", "hat", "book of Minor Magic" },
    WCHOICE_NONE,
    { { SK_DODGING, 2 }, { SK_STEALTH, 2 },
      { SK_TRANSLOCATIONS, 1 }, { SK_SUMMONINGS, 1 }, { SK_AIR_MAGIC, 1 },
      { SK_TRANSMUTATIONS, 1 }, { SK_EARTH_MAGIC, 1 }, { SK_HEXES, 1 },
      { SK_NECROMANCY, 1 }, { SK_FIRE_MAGIC, 1 },
    },
} },
#if TAG_MAJOR_VERSION == 34
{ JOB_CONJURER, {
    "Cj", "Conjurer",
    0, 0, 0,
    { },
    { },
    WCHOICE_NONE,
    { },
} },

{ JOB_DEATH_KNIGHT, {
    "DK", "Death Knight",
    0, 0, 0,
    { },
    { },
    WCHOICE_NONE,
    { },
} },

{ JOB_HEALER, {
    "He", "Healer",
    0, 0, 0,
    { },
    { },
    WCHOICE_NONE,
    { },
} },

{ JOB_JESTER, {
    "Jr", "Jester",
    0, 0, 0,
    { },
    { },
    WCHOICE_NONE,
    { },
} },

{ JOB_PRIEST, {
    "Pr", "Priest",
    0, 0, 0,
    { },
    { },
    WCHOICE_NONE,
    { },
} },

{ JOB_STALKER, {
    "St", "Stalker",
    0, 0, 0,
    { },
    { },
    WCHOICE_NONE,
    { },
} },

{ JOB_VENOM_MAGE, {
    "VM", "Venom Mage",
    0, 0, 0,
    { },
    { },
    WCHOICE_NONE,
    { },
} },

{ JOB_WARPER, {
    "Wr", "Warper",
    0, 0, 0,
    { },
    { },
    WCHOICE_NONE,
    { },
} },

{ JOB_ASSASSIN, {
    "As", "Assassin",
    0, 0, 0,
    { },
    { },
    WCHOICE_NONE,
    { },
} },

{ JOB_ABYSSAL_KNIGHT, {
    "AK", "Abyssal Knight",
    0, 0, 0,
    { },
    { },
    WCHOICE_NONE,
    { },
} },
#endif
};
