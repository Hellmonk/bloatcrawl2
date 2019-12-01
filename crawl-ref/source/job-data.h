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
    /// Which species are good at it
    /// No recommended species = job is disabled
    vector<species_type> recommended_species;
    /// Guaranteed starting equipment. Uses vault spec syntax, with the plus:,
    /// charges:, q:, and ego: tags supported.
    vector<string> equipment;
    weapon_choice wchoice; ///< how the weapon is chosen, if any
    vector<pair<skill_type, int>> skills; ///< starting skills
};

static const map<job_type, job_def> job_data =
{

{ JOB_ABYSSAL_KNIGHT, {
    "AK", "Abyssal Knight",
    4, 4, 4,
    { SP_HILL_ORC, SP_SPRIGGAN, SP_TROLL, SP_MERFOLK, SP_BASE_DRACONIAN,
      SP_DEMONSPAWN, },
    { "leather armour" },
    WCHOICE_PLAIN,
    { { SK_FIGHTING, 3 }, { SK_ARMOUR, 1 }, { SK_DODGING, 1 },
      { SK_INVOCATIONS, 2 }, { SK_WEAPON, 2 }, },
} },

{ JOB_AIR_ELEMENTALIST, {
    "AE", "Air Elementalist",
    0, 7, 5,
    { SP_DEEP_ELF, SP_TENGU, SP_BASE_DRACONIAN, SP_NAGA, SP_VINE_STALKER,
      SP_PROFOUND_ELF, },
    { "robe", "book of Air" },
    WCHOICE_NONE,
    { { SK_CONJURATIONS, 1 }, { SK_AIR_MAGIC, 3 }, { SK_SPELLCASTING, 2 },
      { SK_DODGING, 2 }, { SK_STEALTH, 2 }, },
} },

{ JOB_ANARCHIST, {
    "A!", "Anarchist",
    5, 2, 5,
    { SP_HUMAN, SP_TROLL, SP_OGRE, SP_KOBOLD,
      SP_CENTAUR, },
    { "robe", "stone q:69", "scroll of immolation q:420"},
    WCHOICE_NONE,
    { { SK_THROWING, 2 }, { SK_UNARMED_COMBAT, 2 }, { SK_FIGHTING, 2 },
      { SK_DODGING, 2 }, { SK_STEALTH, 2 }, },
} },

{ JOB_ANNIHILATOR, {
    "An", "Annihilator",
    0, 8, 4,
    { SP_DEEP_ELF, SP_SPRIGGAN, SP_NAGA, SP_TENGU, SP_BASE_DRACONIAN},
    { "robe" },
    WCHOICE_NONE,
    { { SK_CONJURATIONS, 3 }, { SK_SPELLCASTING, 3 },
      { SK_DODGING, 2 }, { SK_STEALTH, 1 }, },
} },

{ JOB_ARCHAEOLOGIST, {
    "Ac", "Archaeologist",
    3, 6, 3,
    { SP_CENTAUR, SP_FORMICID, SP_MINOTAUR, SP_NAGA, SP_VINE_STALKER, SP_GARGOYLE, SP_HILL_ORC, SP_HUMAN },
    { "whip", "robe plus:1", "hat plus:1", "pair of boots plus:1",
      "dusty tome", "ancient crate" },
    WCHOICE_NONE,
    { { SK_STEALTH, 3}, { SK_DODGING, 3}, { SK_FIGHTING, 1}, { SK_MACES_FLAILS, 1} },
} },

{ JOB_ARCANE_MARKSMAN, {
    "AM", "Arcane Marksman",
    2, 5, 5,
    { SP_FORMICID, SP_DEEP_ELF, SP_KOBOLD, SP_SPRIGGAN, SP_TROLL, SP_CENTAUR,
      SP_PROFOUND_ELF, },
    { "robe", "book of Debilitation" },
    WCHOICE_RANGED,
    { { SK_FIGHTING, 1 }, { SK_DODGING, 2 }, { SK_SPELLCASTING, 1 },
      { SK_HEXES, 3 }, { SK_WEAPON, 2 }, },
} },

{ JOB_ARTIFICER, {
    "Ar", "Artificer",
    4, 3, 5,
    { SP_DEEP_DWARF, SP_HALFLING, SP_KOBOLD, SP_SPRIGGAN, SP_BASE_DRACONIAN,
      SP_DEMONSPAWN, },
    { "short sword", "leather armour", "wand of flame charges:15",
      "wand of enslavement charges:15", "wand of random effects charges:15" },
    WCHOICE_NONE,
    { { SK_EVOCATIONS, 3 }, { SK_DODGING, 2 }, { SK_FIGHTING, 1 },
      { SK_WEAPON, 1 }, { SK_STEALTH, 1 }, },
} },

{ JOB_ASPIRANT, {
    "Ap", "Aspirant",
    0, 8, 4,
    { SP_EMBER_ELF, SP_DEEP_ELF, SP_NAGA, SP_TENGU, SP_SPRIGGAN, SP_OCTOPODE },
    { "robe", "quarterstaff" },
    WCHOICE_NONE,
    { { SK_DODGING, 2 }, { SK_STEALTH, 2 }, { SK_SPELLCASTING, 2 } },
} },

{ JOB_ASSASSIN, {
    "As", "Assassin",
    3, 3, 6,
    { SP_TROLL, SP_HALFLING, SP_SPRIGGAN, SP_DEMONSPAWN,
      SP_VINE_STALKER, },
    { "dagger plus:2", "robe", "cloak", "dart ego:poisoned q:8",
      "dart ego:curare q:2" },
    WCHOICE_NONE,
    { { SK_FIGHTING, 2 }, { SK_DODGING, 1 }, { SK_STEALTH, 4 },
      { SK_THROWING, 2 }, { SK_WEAPON, 2 }, },
} },

{ JOB_BERSERKER, {
    "Be", "Berserker",
    9, -1, 4,
    { SP_HILL_ORC, SP_HALFLING, SP_OGRE, SP_MERFOLK, SP_MINOTAUR, SP_GARGOYLE,
      SP_DEMONSPAWN, },
    { "animal skin" },
    WCHOICE_PLAIN,
    { { SK_FIGHTING, 3 }, { SK_DODGING, 2 }, { SK_WEAPON, 3 }, },
} },

{ JOB_BILLIONAIRE, {
    "Bi", "Billionaire",
    4, 4, 4,
    { SP_TROLL, },
    { "robe", "bread ration" },
    WCHOICE_NONE,
    { { SK_STEALTH, 1 }, { SK_DODGING, 4 }, },
} },

{ JOB_BLOOD_KNIGHT, {
    "BK", "Blood Knight",
    5, 4, 3,
    { SP_HILL_ORC, SP_GNOLL, SP_DEEP_DWARF, SP_DEMONSPAWN,
      SP_FORMICID },
    { "leather armour" },
    WCHOICE_PLAIN,
    { { SK_FIGHTING, 2 }, { SK_INVOCATIONS, 3 }, { SK_DODGING, 1 },
      { SK_ARMOUR, 1 }, { SK_WEAPON, 1 } },
} },

{ JOB_BOUND, {
    "Bo", "Bound",
    4, 4, 4,
    { SP_HILL_ORC, SP_MINOTAUR, SP_MERFOLK, SP_GARGOYLE },
    { },
    WCHOICE_PLAIN,
    { { SK_FIGHTING, 3 }, { SK_ARMOUR, 1 }, { SK_DODGING, 2 },
      { SK_WEAPON, 2 }, },
} },

{ JOB_CAVEPERSON, {
    "Ca", "Caveperson",
    6, 0, 0, // half normal stats
    { SP_DEEP_DWARF, },
    { "club plus:1", "hunting sling plus:2", "animal skin", "sling bullet q:22",
      "stone q:33", "chunk of flesh" },
    WCHOICE_NONE,
    { { SK_MACES_FLAILS, 2 }, { SK_SLINGS, 2 }, { SK_UNARMED_COMBAT, 2 }, },
} },

{ JOB_CHAOS_KNIGHT, {
    "CK", "Chaos Knight",
    4, 4, 4,
    { SP_HILL_ORC, SP_TROLL, SP_CENTAUR, SP_MERFOLK, SP_MINOTAUR,
      SP_BASE_DRACONIAN, SP_DEMONSPAWN },
    { "leather armour plus:2" },
    WCHOICE_PLAIN,
    { { SK_FIGHTING, 3 }, { SK_ARMOUR, 1 }, { SK_DODGING, 1 },
      { SK_WEAPON, 3 } },
} },

{ JOB_CONJURER, {
    "Cj", "Conjurer",
    0, 7, 5,
    { SP_DEEP_ELF, SP_NAGA, SP_TENGU, SP_BASE_DRACONIAN, SP_DEMIGOD,
      SP_PROFOUND_ELF, SP_FLAN, },
    { "robe", "book of Conjurations" },
    WCHOICE_NONE,
    { { SK_CONJURATIONS, 4 }, { SK_SPELLCASTING, 2 }, { SK_DODGING, 2 },
      { SK_STEALTH, 2 }, },
} },

{ JOB_DANCER, {
    "Da", "Dancer",
    3, 2, 7,
    { SP_VINE_STALKER, SP_HILL_ORC },
    { "dagger", "robe", "boots",  },
    WCHOICE_NONE,
    { {SK_DODGING, 5}, {SK_INVOCATIONS, 4}, },
} },

{ JOB_DEATH_BISHOP, {
    "DB", "Death Bishop",
    4, 4, 4,
    { SP_HILL_ORC, SP_GNOLL, SP_SPRIGGAN, SP_DEMONSPAWN,
      SP_FORMICID },
    { "robe" },
    WCHOICE_PLAIN,
    { { SK_FIGHTING, 2 }, { SK_INVOCATIONS, 2 }, { SK_DODGING, 2 }, { SK_WEAPON, 2 } },
} },

{ JOB_DEPRIVED, {
    "De", "Deprived",
    4, 4, 4,
    { SP_TROLL, },
    { "bread ration" },
    WCHOICE_NONE,
    { { SK_STEALTH, 1 }, },
} },

{ JOB_DISCIPLE, {
    "Di", "Disciple",
    4, 4, 4,
    { SP_HILL_ORC, SP_GNOLL, SP_SPRIGGAN, SP_DEMONSPAWN,
      SP_FORMICID },
    { "robe"},
    WCHOICE_GOOD,
    { { SK_FIGHTING, 2 }, { SK_DODGING, 2 }, { SK_WEAPON, 3 } },
} },

{ JOB_DOCTOR, {
    "Dr", "Doctor",
    3, 5, 4,
    { SP_HILL_ORC, SP_TROLL },
    { "dagger", "robe", "potion of curing", "potion of heal wounds" },
    WCHOICE_NONE,
    { {SK_FIGHTING, 2}, {SK_DODGING, 2}, {SK_STEALTH, 2}, {SK_INVOCATIONS, 4}, },
} },

{ JOB_EARTH_ELEMENTALIST, {
    "EE", "Earth Elementalist",
    0, 7, 5,
    { SP_DEEP_ELF, SP_DEEP_DWARF, SP_SPRIGGAN, SP_GARGOYLE, SP_DEMIGOD,
      SP_OCTOPODE, SP_PROFOUND_ELF, },
    { "book of Geomancy", "stone q:30", "robe", },
    WCHOICE_NONE,
    { { SK_TRANSMUTATIONS, 1 }, { SK_EARTH_MAGIC, 3 }, { SK_SPELLCASTING, 2 },
      { SK_DODGING, 2 }, { SK_STEALTH, 2 }, }
} },

{ JOB_ENCHANTER, {
    "En", "Enchanter",
    0, 7, 5,
    { SP_DEEP_ELF, SP_FELID, SP_KOBOLD, SP_SPRIGGAN, SP_NAGA,
      SP_PROFOUND_ELF, },
    { "dagger plus:1", "robe", "book of Maledictions" },
    WCHOICE_NONE,
    { { SK_WEAPON, 1 }, { SK_HEXES, 3 }, { SK_SPELLCASTING, 2 },
      { SK_DODGING, 2 }, { SK_STEALTH, 3 }, },
} },


{ JOB_ENTOMOLOGIST, {
    "Et", "Entomologist",
    2, 4, 6,
    { SP_TENGU, },
    { "dagger", "throwing net", "sack of spiders", "robe"},
    WCHOICE_NONE,
    { { SK_SPELLCASTING, 1 }, { SK_DODGING, 3 }, { SK_EVOCATIONS, 1 },
      { SK_WEAPON, 1 }, { SK_STEALTH, 3 }, },
} },

{ JOB_FENCER, {
    "Fn", "Fencer",
    0, 0, 12,
    { SP_MERFOLK, SP_MINOTAUR, SP_SPRIGGAN, SP_KOBOLD, },
    { "rapier", "robe", "fencer's gloves", "amulet of the acrobat"},
    WCHOICE_NONE,
    { { SK_FIGHTING, 1 }, { SK_DODGING, 5 }, { SK_WEAPON, 1 } },
} },


{ JOB_FIGHTER, {
    "Fi", "Fighter",
    8, 0, 4,
    { SP_DEEP_DWARF, SP_HILL_ORC, SP_TROLL, SP_MINOTAUR, SP_GARGOYLE,
      SP_CENTAUR, },
    { "scale mail", "shield", "potion of might" },
    WCHOICE_GOOD,
    { { SK_FIGHTING, 3 }, { SK_SHIELDS, 3 }, { SK_ARMOUR, 3 },
      { SK_WEAPON, 2 } },
} },

{ JOB_FIRE_ELEMENTALIST, {
    "FE", "Fire Elementalist",
    0, 7, 5,
    { SP_DEEP_ELF, SP_HILL_ORC, SP_NAGA, SP_TENGU, SP_DEMIGOD, SP_GARGOYLE,
      SP_PROFOUND_ELF, },
    { "robe", "book of Flames" },
    WCHOICE_NONE,
    { { SK_CONJURATIONS, 1 }, { SK_FIRE_MAGIC, 3 }, { SK_SPELLCASTING, 2 },
      { SK_DODGING, 2 }, { SK_STEALTH, 2 }, },
} },

{ JOB_GAMBLER, {
    "Ga", "Gambler",
    4, 4, 4,
    { SP_SPRIGGAN, SP_TROLL, SP_HILL_ORC },
    { },
    WCHOICE_NONE,
    { { SK_FIGHTING, 3 }, { SK_DODGING, 2 }, { SK_INVOCATIONS, 2}, { SK_STEALTH, 2 }, },
} },

{ JOB_GARDENER, {
    "Gr", "Gardener",
    4, 4, 4,
    { SP_MERFOLK, SP_TROLL, SP_HILL_ORC },
    { "ration q:4", "robe", "pair of gloves", "scythe"},
    WCHOICE_NONE,
    { { SK_FIGHTING, 2 }, { SK_DODGING, 2 }, { SK_INVOCATIONS, 3}, { SK_STEALTH, 2 }, },
} },

{ JOB_GLADIATOR, {
    "Gl", "Gladiator",
    6, 0, 6,
    { SP_DEEP_DWARF, SP_HILL_ORC, SP_MERFOLK, SP_MINOTAUR, SP_GARGOYLE,
      SP_CENTAUR, },
    { "leather armour", "helmet", "throwing net q:3" },
    WCHOICE_GOOD,
    { { SK_FIGHTING, 2 }, { SK_THROWING, 2 }, { SK_DODGING, 3 },
      { SK_WEAPON, 3}, },
} },

{ JOB_HERMIT, {
    "Hm", "Hermit",
    4, 4, 4,
    { SP_HILL_ORC, SP_HALFLING, SP_OGRE, SP_TROLL, SP_CENTAUR, },
    { "club", "robe" },
    WCHOICE_NONE,
    { { SK_FIGHTING, 3 }, { SK_DODGING, 3 }, { SK_STEALTH, 3 }, },
} },

{ JOB_HUNTER, {
    "Hu", "Hunter",
    4, 3, 5,
    { SP_HILL_ORC, SP_HALFLING, SP_KOBOLD, SP_OGRE, SP_TROLL, SP_CENTAUR, },
    { "short sword", "leather armour" },
    WCHOICE_RANGED,
    { { SK_FIGHTING, 2 }, { SK_DODGING, 2 }, { SK_STEALTH, 1 },
      { SK_WEAPON, 4 }, },
} },

{ JOB_ICE_ELEMENTALIST, {
    "IE", "Ice Elementalist",
    0, 7, 5,
    { SP_DEEP_ELF, SP_MERFOLK, SP_NAGA, SP_BASE_DRACONIAN, SP_DEMIGOD,
      SP_GARGOYLE, SP_PROFOUND_ELF, },
    { "robe", "book of Frost" },
    WCHOICE_NONE,
    { { SK_CONJURATIONS, 1 }, { SK_ICE_MAGIC, 3 }, { SK_SPELLCASTING, 2 },
      { SK_DODGING, 2 }, { SK_STEALTH, 2 }, },
} },

{ JOB_INHERITOR, {
    "In", "Inheritor",
    4, 4, 4,
    { SP_MERFOLK, SP_NAGA, SP_BASE_DRACONIAN, SP_OGRE, SP_TROLL, },
    { "robe" },
    WCHOICE_NONE,
    { { SK_INVOCATIONS, 3 }, },
} },

{ JOB_KIKUMANCER, {
    "Ki", "Kikumancer",
    0, 7, 5,
    { SP_DEEP_ELF, SP_DEEP_DWARF, SP_HILL_ORC, SP_DEMONSPAWN,
      SP_PROFOUND_ELF, },
    { "robe", "dagger" },
    WCHOICE_NONE,
    { { SK_SPELLCASTING, 2 }, { SK_NECROMANCY, 4 }, { SK_DODGING, 2 },
      { SK_STEALTH, 2 }, },
} },

{ JOB_LIBRARIAN, {
    "Li", "Librarian",
    0, 9, 3,
    { SP_SPRIGGAN, SP_OCTOPODE, SP_VINE_STALKER, SP_DEEP_ELF, },
    { "robe", "dagger" },
    WCHOICE_NONE,
    { { SK_DODGING, 2 }, { SK_STEALTH, 2 }, { SK_SPELLCASTING, 4 }, },
} },

{ JOB_MERCHANT, {
    "$$", "Merchant",
    4, 4, 4,
    { SP_HILL_ORC, SP_MINOTAUR, SP_LEPRECHAUN },
    { "robe", "pair of boots", "hat" },
    WCHOICE_PLAIN,
    { { SK_STEALTH, 1 }, { SK_DODGING, 1 }, { SK_WEAPON, 2 }, },
} },

{ JOB_METEOROLOGIST, {
    "Me", "Meteorologist",
    4, 4, 4,
    { SP_DEEP_DWARF, SP_HALFLING, SP_KOBOLD, SP_SPRIGGAN, SP_BASE_DRACONIAN,
      SP_DEMONSPAWN, },
    { "robe", "phial of floods", "fan of gales", "lightning rod", "air horn" },
    WCHOICE_NONE,
    { { SK_EVOCATIONS, 4 }, { SK_DODGING, 1 }, { SK_STEALTH, 1 }, },
} },

{ JOB_MONK, {
    "Mo", "Monk",
    3, 2, 7,
    { SP_DEEP_DWARF, SP_HILL_ORC, SP_TROLL, SP_CENTAUR, SP_MERFOLK,
      SP_GARGOYLE, SP_DEMONSPAWN, },
    { "robe" },
    WCHOICE_PLAIN,
    { { SK_FIGHTING, 3 }, { SK_WEAPON, 3 }, { SK_DODGING, 3 },
      { SK_STEALTH, 2 }, },
} },

{ JOB_NECKBEARD, {
    "Nb", "Neckbeard",
    4, 4, 4,
    { SP_DEEP_DWARF, SP_HILL_ORC, SP_MERFOLK, SP_MINOTAUR, SP_GARGOYLE,
      SP_CENTAUR, },
    { "robe", "hat", "cloak", "katana plus:-4" },
    WCHOICE_NONE,
    { { SK_FIGHTING, 2 }, { SK_DODGING, 2 }, { SK_WEAPON, 3},
      { SK_SPELLCASTING, 1}, { SK_TRANSLOCATIONS, 2}, }
} },

{ JOB_NECROMANCER, {
    "Ne", "Necromancer",
    0, 7, 5,
    { SP_DEEP_ELF, SP_DEEP_DWARF, SP_HILL_ORC, SP_DEMONSPAWN,
      SP_PROFOUND_ELF, },
    { "robe", "book of Necromancy" },
    WCHOICE_NONE,
    { { SK_SPELLCASTING, 2 }, { SK_NECROMANCY, 4 }, { SK_DODGING, 2 },
      { SK_STEALTH, 2 }, },
} },

{ JOB_NIGHT_KNIGHT, {
    "NK", "Night Knight",
    4, 4, 4,
    { SP_SPRIGGAN, SP_HALFLING, SP_KOBOLD },
    { "dagger plus:1"},
    WCHOICE_NONE,
    { { SK_FIGHTING, 1 }, { SK_WEAPON, 2 }, { SK_DODGING, 2 },
      { SK_STEALTH, 4 }, { SK_INVOCATIONS, 2 }, },
} },

{ JOB_PALADIN, {
    "Pa", "Paladin",
    4, 4, 4,
    { SP_OGRE, SP_MINOTAUR },
    { "chain mail" },
    WCHOICE_PLAIN,
    { { SK_FIGHTING, 3 }, { SK_ARMOUR, 1 }, { SK_DODGING, 1 },
      { SK_WEAPON, 2 }, { SK_INVOCATIONS, 2 }, },
} },

{ JOB_PHILOSOPHER, {
    "Ph", "Philosopher",
    0, 12, 0,
    { SP_CENTAUR, SP_NAGA, SP_SPRIGGAN, },
    { "robe", "hat of pondering", "staff of wizardry", "scroll of random uselessness"},
    WCHOICE_NONE,
    { { SK_FIGHTING, 2 }, { SK_SPELLCASTING, 2 }, { SK_HEXES, 1 }, { SK_STEALTH, 2 }, },
} },

{ JOB_SKALD, {
    "Sk", "Skald",
    3, 5, 4,
    { SP_HALFLING, SP_CENTAUR, SP_MERFOLK, SP_BASE_DRACONIAN, },
    { "leather armour", "book of Battle" },
    WCHOICE_PLAIN,
    { { SK_FIGHTING, 2 }, { SK_ARMOUR, 1 }, { SK_DODGING, 2 },
      { SK_SPELLCASTING, 2 }, { SK_CHARMS, 3 }, { SK_WEAPON, 2 }, },
} },

{ JOB_SLIME_PRIEST, {
    "SP", "Slime Priest",
    4, 4, 4,
    { SP_TROLL, SP_OCTOPODE, SP_MINOTAUR, SP_SPRIGGAN,
      SP_DEMONSPAWN, },
    { "ring mail plus:1" },
    WCHOICE_PLAIN,
    { { SK_FIGHTING, 3 }, { SK_ARMOUR, 1 }, { SK_DODGING, 1 },
      { SK_WEAPON, 3 }, },
} },

{ JOB_SNIPER, {
    "Sn", "Sniper",
    5, 2, 5,
    { SP_HILL_ORC, SP_HALFLING, SP_KOBOLD, SP_OGRE, SP_TROLL, SP_CENTAUR, },
    { "robe", "amulet of harm" },
    WCHOICE_RANGED,
    { { SK_FIGHTING, 2 }, { SK_DODGING, 2 }, { SK_STEALTH, 3 },
      { SK_WEAPON, 3 }, },
} },

{ JOB_STORM_CLERIC, {
    "SC", "Storm Cleric",
    4, 4, 4,
    { SP_FORMICID, SP_MINOTAUR },
    { "ring mail", "air horn" },
    WCHOICE_PLAIN,
    { { SK_FIGHTING, 2 }, { SK_ARMOUR, 1 }, { SK_DODGING, 1 },
      { SK_WEAPON, 3 }, { SK_INVOCATIONS, 3 }, },
} },

{ JOB_SOOTHSLAYER, {
    "So", "Soothslayer",
    4, 4, 4,
    { SP_HILL_ORC, SP_CENTAUR, SP_MERFOLK, SP_BASE_DRACONIAN, },
    { "ring mail" },
    WCHOICE_PLAIN,
    { { SK_FIGHTING, 2 }, { SK_ARMOUR, 1 }, { SK_DODGING, 1 },
      { SK_SPELLCASTING, 2 }, { SK_CHARMS, 1 }, { SK_HEXES, 1 }, 
      { SK_TRANSLOCATIONS, 1 }, { SK_TRANSMUTATIONS, 1 }, 
      { SK_NECROMANCY, 1 }, { SK_SUMMONINGS, 1 }, { SK_WEAPON, 2 }, },
} },

{ JOB_SUMMONER, {
    "Su", "Summoner",
    0, 7, 5,
    { SP_DEEP_ELF, SP_HILL_ORC, SP_VINE_STALKER, SP_MERFOLK, SP_TENGU,
      SP_PROFOUND_ELF, },
    { "robe", "book of Callings" },
    WCHOICE_NONE,
    { { SK_SUMMONINGS, 4 }, { SK_SPELLCASTING, 2 }, { SK_DODGING, 2 },
      { SK_STEALTH, 2 }, },
} },

{ JOB_TORPOR_KNIGHT, {
    "TK", "Torpor Knight",
    4, 4, 4,
    { SP_HILL_ORC, SP_MINOTAUR },
    { "plate armour ego:ponderousness" },
    WCHOICE_PLAIN,
    { { SK_FIGHTING, 3 }, { SK_ARMOUR, 1 }, { SK_DODGING, 1 },
      { SK_WEAPON, 2 }, { SK_INVOCATIONS, 2 }, },
} },

{ JOB_TRANSMUTER, {
    "Tm", "Transmuter",
    2, 5, 5,
    { SP_NAGA, SP_MERFOLK, SP_BASE_DRACONIAN, SP_DEMIGOD, SP_DEMONSPAWN,
      SP_TROLL, },
    { "arrow q:12", "robe", "book of Changes" },
    WCHOICE_NONE,
    { { SK_FIGHTING, 1 }, { SK_UNARMED_COMBAT, 3 }, { SK_DODGING, 2 },
      { SK_SPELLCASTING, 2 }, { SK_TRANSMUTATIONS, 2 }, },
} },

{ JOB_UNCLE, {
    "Un", "Uncle",
    4, 4, 4,
    { SP_HUMAN, SP_HILL_ORC, SP_GNOLL },
    { "club" },
    WCHOICE_NONE,
    { { SK_FIGHTING, 2 }, { SK_DODGING, 2 },  { SK_SPELLCASTING, 2 }, { SK_STEALTH, 1 }, },
} },

{ JOB_UNDERSTUDY, {
    "Us", "Understudy",
    4, 4, 4,
    { SP_BASE_DRACONIAN, SP_GNOLL },
    { "robe" },
    WCHOICE_NONE,
    { { SK_FIGHTING, 1 }, { SK_DODGING, 1 }, }, // Also gets 4 randomized weapon skill
} },

{ JOB_VENOM_MAGE, {
    "VM", "Venom Mage",
    0, 7, 5,
    { SP_DEEP_ELF, SP_SPRIGGAN, SP_NAGA, SP_MERFOLK, SP_TENGU, SP_FELID,
      SP_DEMONSPAWN, SP_PROFOUND_ELF, },
    { "robe", "Young Poisoner's Handbook" },
    WCHOICE_NONE,
    { { SK_CONJURATIONS, 1 }, { SK_POISON_MAGIC, 3 }, { SK_SPELLCASTING, 2 },
      { SK_DODGING, 2 }, { SK_STEALTH, 2 }, },
} },

{ JOB_WANDERER, {
    "Wn", "Wanderer",
    0, 0, 0, // Randomised
    { SP_HILL_ORC, SP_SPRIGGAN, SP_CENTAUR, SP_MERFOLK, SP_BASE_DRACONIAN,
      SP_HUMAN, SP_DEMONSPAWN, },
    { }, // Randomised
    WCHOICE_NONE,
    { }, // Randomised
} },

{ JOB_WARPER, {
    "Wr", "Warper",
    3, 5, 4,
    { SP_FELID, SP_HALFLING, SP_DEEP_DWARF, SP_SPRIGGAN, SP_CENTAUR,
      SP_BASE_DRACONIAN, },
    { "leather armour", "book of Spatial Translocations", "scroll of blinking",
      "boomerang ego:dispersal q:5" },
    WCHOICE_PLAIN,
    { { SK_FIGHTING, 2 }, { SK_ARMOUR, 1 }, { SK_DODGING, 2 },
      { SK_SPELLCASTING, 2 }, { SK_TRANSLOCATIONS, 3 }, { SK_THROWING, 1 },
      { SK_WEAPON, 2 }, },
} },

{ JOB_WARRIOR, {
    "Wa", "Warrior",
    8, 0, 4,
    { SP_MINOTAUR, SP_HILL_ORC, SP_MERFOLK, SP_GARGOYLE, SP_CENTAUR, },
    { "scale mail plus:-2", "pair of gloves plus:-1", "hat", "dagger plus:-1",
      "bardiche plus:-6", "executioner's axe plus:-6",
      "triple sword plus:-6", "hunting sling plus:-2", "shortbow plus:-2",
      "hand crossbow plus:-2", "javelin q:1" },
    WCHOICE_PLAIN,
    { { SK_FIGHTING, 2 }, { SK_INVOCATIONS, 2 }, { SK_DODGING, 1 }, { SK_ARMOUR, 2 },
      { SK_WEAPON, 2}, },
} },

{ JOB_WITNESS, {
    "Wi", "Witness",
    6, 4, 2,
    { SP_HILL_ORC, },
    { "ring mail", "scroll of noise"},
    WCHOICE_PLAIN,
    { { SK_FIGHTING, 2 }, { SK_WEAPON, 2 }, { SK_ARMOUR, 2 }, { SK_DODGING, 2 },
    { SK_INVOCATIONS, 2 }, },
} },

{ JOB_WIZARD, {
    "Wz", "Wizard",
    -1, 10, 3,
    { SP_DEEP_ELF, SP_NAGA, SP_BASE_DRACONIAN, SP_OCTOPODE, SP_HUMAN,
      SP_PROFOUND_ELF, },
    { "robe", "hat", "book of Minor Magic" },
    WCHOICE_NONE,
    { { SK_DODGING, 2 }, { SK_STEALTH, 2 }, { SK_SPELLCASTING, 3 },
      { SK_TRANSLOCATIONS, 1 }, { SK_CONJURATIONS, 1 }, { SK_SUMMONINGS, 1 }, },
} },

{ JOB_ZINJA, {
    "Zi", "Zinja",
    4, 4, 4,
    { SP_HILL_ORC, SP_GNOLL, SP_SPRIGGAN, SP_DEMONSPAWN,
      SP_FORMICID },
    { "robe", "boomerang ego:silver q:3" },
    WCHOICE_PLAIN,
    { { SK_FIGHTING, 1 }, { SK_INVOCATIONS, 2 }, { SK_DODGING, 1 }, { SK_WEAPON, 2 }, { SK_STEALTH, 4 } },
} },
#if TAG_MAJOR_VERSION == 34
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
#endif
};
