// start with bits from Felid, Octopode, demonspawn, Gray Drac, and modify from there

{ SP_SLIMEOID, {
    "SL",
    "Slimeoid", nullptr, "Jelly",
    SPF_NO_HAIR,
    0, -1, 1,
    18, 6,
    MONS_FELID,
    HT_AMPHIBIOUS, US_ALIVE, SIZE_SMALL,
    9, 6, 9, // This slime only recently became self-aware 
    { STAT_INT, STAT_DEX }, 5, // with no muscles, getting stronger is not easy
    {
      { MUT_GELATINOUS_BODY, 1, 1 },
      { MUT_SLOW, 1, 4 },
      { MUT_SIZE_UPGRADE, 1, 4 },
      { MUT_GELATINOUS_BODY, 1, 4 },
      { MUT_GELATINOUS_BODY, 1, 8 },
      { MUT_SLOW, 1, 16 },
      { MUT_SIZE_UPGRADE, 1, 16 },
    },
    { "You can only wear hats.",
      "You can keep up to (size) magical trinkets in your body."
      "With no arms, you can't attack with weapons."
      "You may hold one weapon and benefit from its magic."
      "You can ooze through water." },
    { "no armour", "no weapon attacks", "walk through water" },
    { JOB_GLADIATOR, JOB_BERSERKER, JOB_ENCHANTER, JOB_TRANSMUTER, 
    JOB_ICE_ELEMENTALIST, JOB_CONJURER, JOB_SUMMONER, JOB_FIRE_ELEMENTALIST, JOB_VENOM_MAGE },
    { SK_UNARMED_COMBAT, SK_THROWING },
} },
