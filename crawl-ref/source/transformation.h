#pragma once

enum class transformation
{
    none,
    spider,
    blade_hands,
    statue,
    ice_beast,
    dragon,
    lich,
    bat,
    pig,
    appendage,
    tree,
    wisp,
    fungus,
    shadow,
    hydra,
    COUNT
};
constexpr int NUM_TRANSFORMS = static_cast<int>(transformation::COUNT);
