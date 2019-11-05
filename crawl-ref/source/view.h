/**
 * @file
 * @brief Misc function used to render the dungeon.
**/

#pragma once

#include "defines.h"
#include "options.h"
#include "player.h" // player_stealth()
#include "viewgeom.h"

bool mon_enemies_around(const monster* mons);
void seen_monsters_react(int stealth = player_stealth());

string describe_monsters_condensed(const vector<monster*>& monsters);

bool magic_mapping(int map_radius, int proportion, bool suppress_msg,
                   bool force = false, bool deterministic = false,
                   coord_def origin = coord_def(-1, -1));
void fully_map_level();

string screenshot();

int viewmap_flash_colour();
bool view_update();
void view_update_at(const coord_def &pos);
class targeter;

static inline void scaled_delay(unsigned int ms)
{
    delay(ms * Options.view_delay / DEFAULT_VIEW_DELAY);
}

// beware, flash_view is broken for USE_TILE_LOCAL
void flash_view(use_animation_type a, colour_t colour,
                targeter *where = nullptr);
void flash_view_delay(use_animation_type a, colour_t colour, int delay,
                      targeter *where = nullptr);

// warning: this function is broken (and ifdef'd out) in local tiles.
void flash_monster_colour(const monster* mon, colour_t fmc_colour,
                          int fmc_delay);



enum animation_type
{
    ANIMATION_SHAKE_VIEWPORT,
    ANIMATION_CHECKERBOARD,
    ANIMATION_BANISH,
    ANIMATION_SLIDEOUT,
    ANIMATION_ORB,
    NUM_ANIMATIONS
};

class animation
{
public:
    animation(): frames(10), frame_delay(50) { }
    virtual ~animation() { }

    virtual void init_frame(int frame)
    {
        UNUSED(frame);
    }
    virtual coord_def cell_cb(const coord_def &pos, int &colour) = 0;

    int frames;
    int frame_delay;
};

void run_animation(animation_type anim, use_animation_type type,
                   bool cleanup = true);
void viewwindow(bool show_updates = true, bool tiles_only = false,
                animation *a = nullptr);
void draw_cell(screen_cell_t *cell, const coord_def &gc,
               bool anim_updates, int flash_colour);

void update_monsters_in_view();
bool handle_seen_interrupt(monster* mons, vector<string>* msgs_buf = nullptr);
void flush_comes_into_view();
void toggle_show_terrain();
void reset_show_terrain();

void handle_terminal_resize();
