#pragma once

// Can't change this order without breaking saves.
enum map_marker_type
{
    MAT_FEATURE,              // Stock marker.
    MAT_LUA_MARKER,
    MAT_CORRUPTION_NEXUS,
    MAT_WIZ_PROPS,
    MAT_TOMB,
    MAT_MALIGN,
    MAT_POSITION,
    MAT_TERRAIN_CHANGE,
    MAT_CLOUD_SPREADER,
    NUM_MAP_MARKER_TYPES,
    MAT_ANY,
};
