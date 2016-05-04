// if a mut isn't flagged as "jiyva", it should fall off in roughly the amount of time it takes
        // for yellow Drain to fall off. Your blobby DNA/body can't really hold any other forms.

// ?? 5 ?? anything-goes equipment slots. and a hat on top (not a helmet)
// due to deformed 2, armours contribute 1/3 of their AC to total AC.
        // no Armour skill to increase it, but no Encumbrance to deal with either
// weapons add 1 to your UC damage
        // so you almost (but not quite) have troll-like "claws 3", if you wear 5 weapons
        // maybe jewelry should increase hp+mp+sp by 1% so that it doesn't feel like
        // you're "losing something" by not wearing 5 weaps/armours

// start with bits from Felid, Octopode, demonspawn, Gray Drac, and modify from there
// try for a somewhat flatter race, as counterpoint to the Gurr school of leveled species design

{ SP_SLIMEOID, {
    "SL",
    "Slimeoid", nullptr, "Jelly",
    SPF_NO_HAIR,
    0, 0, 0,
    18, 5,
    MONS_GIANT_AMOEBA,
    HT_AMPHIBIOUS, US_ALIVE, SIZE_SMALL,
    9, 6, 9, // This slime only recently became self-aware 
    { STAT_INT, STAT_DEX }, 5, // with no muscles, getting stronger is not easy
    {
      { MUT_UNBREATHING, 1, 1 },
      { MUT_ACID_RESISTANCE, 1, 1 },
      { MUT_CORROSION_RESISTANCE, 1, 1 },
      { MUT_POISON_RESISTANCE, 1, 1 },
      { MUT_GELATINOUS_BODY, 1, 1 },
      { MUT_DEFORMED, 1, 1 },
      { MUT_TRANSLUCENT_SKIN, 1, 1 }, // does nothing, but prepares you for possibly getting rank 2
      { MUT_GELATINOUS_BODY, 1, 3 },
      { MUT_DEFORMED, 1, 3 },
      { rando bad mut AND Jiyva mut, 1, 10},
      { rando bad mut AND Jiyva mut, 1, 15},
      { rando bad mut AND Jiyva mut, 1, 20},
      { rando bad mut AND Jiyva mut, 1, 25},
    },
    { "You can balance one hat on top of your blobby self.",
      "You can keep up to ??5?? magical items in your body.",
      "Lacking arms, you are unable to attack with weapons.",
      "You can ooze through water." },
    { "no armour", "no weapon attacks", "walk through water" },
    { JOB_GLADIATOR, JOB_BERSERKER, JOB_ENCHANTER, JOB_TRANSMUTER, 
    JOB_ICE_ELEMENTALIST, JOB_CONJURER, JOB_SUMMONER, JOB_FIRE_ELEMENTALIST, JOB_VENOM_MAGE },
    { SK_UNARMED_COMBAT, SK_THROWING },
} },
