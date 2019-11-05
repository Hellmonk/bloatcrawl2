/**
 * @file
 * @brief Tiles interface, either for SDL or web tiles. This file contains
 * definitions loaded in all versions (including console). See
 * `tiles-build-specific.h` for everything that is conditional on build type.
**/

#pragma once

// The different texture types.
enum TextureID
{
    TEX_FLOOR,   // floor.png
    TEX_WALL,    // wall.png
    TEX_FEAT,    // feat.png
    TEX_PLAYER,  // player.png
    TEX_DEFAULT, // main.png
    TEX_GUI,     // gui.png
    TEX_ICONS,   // icons.png
    TEX_MAX
};

struct VColour
{
    VColour(unsigned char _r, unsigned char _g, unsigned char _b,
            unsigned char _a = 255) : r(_r), g(_g), b(_b), a(_a) {}
    VColour() = default;
    VColour(const VColour &vc) = default;
    VColour& operator=(const VColour &vc) = default;

    inline void set(const VColour &in)
    {
        r = in.r;
        g = in.g;
        b = in.b;
        a = in.a;
    }

    bool operator==(const VColour &vc) const;
    bool operator!=(const VColour &vc) const;

    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;

    static VColour white;
    static VColour black;
    static VColour transparent;
};

struct tile_def
{
    tile_def(tileidx_t _tile, TextureID _tex, int _ymax = TILE_Y)
            : tile(_tile), tex(_tex), ymax(_ymax) {}

    tileidx_t tile;
    TextureID tex;
    int ymax;
};

TextureID get_dngn_tex(tileidx_t idx);
