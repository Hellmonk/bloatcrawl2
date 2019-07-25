#pragma once

enum class player_religion
{
    /** You can worship the gods as normal. */
    theist,
    /** You cannot worship the gods. */
    atheist,
    /** You cannot worship the gods and have modified attributes/aptitudes. */
    demigod,

    // COUNT,
};
// If needed in future
// constexpr int NUM_PLAYER_RELIGIONS = static_cast<int>(player_religion::COUNT);
