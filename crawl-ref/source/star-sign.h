#pragma once

enum class star_sign
{
    none,
    conscript,
    thief,
    pilgrim,
    scout,
    apothecary,
    scholar,
    smith_weapon,
    traveller,
    dj,
    alex_jones,

    COUNT
};
constexpr int NUM_STAR_SIGNS = static_cast<int>(star_sign::COUNT);

star_sign random_star_sign();
void apply_star_sign();
const char* star_sign_name(star_sign sign);
