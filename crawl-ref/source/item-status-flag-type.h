#pragma once

enum item_status_flag_type  // per item flags: ie. ident status, cursed status
{
    ISFLAG_KNOW_CURSE        = 0x00000001,  // curse status
    ISFLAG_KNOW_TYPE         = 0x00000002,  // artefact name, sub/special types
    ISFLAG_KNOW_PLUSES       = 0x00000004,  // to hit/to dam/to AC
    ISFLAG_KNOW_PROPERTIES   = 0x00000008,  // know special artefact properties
    ISFLAG_IDENT_MASK        = 0x0000000F,  // mask of all id related flags

    ISFLAG_CURSED            = 0x00000100,  // cursed
    ISFLAG_HANDLED           = 0x00000200,  // player has handled this item
                             //0x00000400,  // was: ISFLAG_SEEN_CURSED
                             //0x00000800,  // was: ISFLAG_TRIED

    ISFLAG_RANDART           = 0x00001000,  // special value is seed
    ISFLAG_UNRANDART         = 0x00002000,  // is an unrandart
    ISFLAG_ARTEFACT_MASK     = 0x00003000,  // randart or unrandart
    ISFLAG_DROPPED           = 0x00004000,  // dropped item (no autopickup)
    ISFLAG_THROWN            = 0x00008000,  // thrown missile weapon

    // these don't have to remain as flags
    ISFLAG_NO_DESC           = 0x00000000,  // used for clearing these flags
    ISFLAG_GLOWING           = 0x00010000,  // weapons or armour
    ISFLAG_RUNED             = 0x00020000,  // weapons or armour
    ISFLAG_EMBROIDERED_SHINY = 0x00040000,  // armour: depends on sub-type
    ISFLAG_COSMETIC_MASK     = 0x00070000,  // mask of cosmetic descriptions

    ISFLAG_UNOBTAINABLE      = 0x00080000,  // vault on display

    ISFLAG_MIMIC             = 0x00100000,  // mimic
                             //0x00200000,  // was ISFLAG_NO_MIMIC

    ISFLAG_NO_PICKUP         = 0x00400000,  // Monsters won't pick this up

    ISFLAG_NOTED_ID          = 0x08000000,
    ISFLAG_NOTED_GET         = 0x10000000,

    ISFLAG_SEEN              = 0x20000000,  // has it been seen
    ISFLAG_SUMMONED          = 0x40000000,  // Item generated on a summon
};
