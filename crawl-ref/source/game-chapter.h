#pragma once

enum game_chapter
{
    CHAPTER_NONDUNGEON_START = 0, // a start outside the dungeon (originally just for AK)
    CHAPTER_ORB_HUNTING, // entered the dungeon but not found the orb yet
    CHAPTER_ESCAPING, // ascending with the orb
    CHAPTER_ANGERED_PANDEMONIUM, // moved the orb without picking it up
    NUM_CHAPTERS,
};
