/**
 * @file
 * @brief Functions used to print player related info.
**/

#pragma once

#ifdef DGL_SIMPLE_MESSAGING
void update_message_status();
#endif

void reset_hud();

void update_turn_count();

void print_stats();
void print_stats_level();
void draw_border();

#ifndef USE_TILE_LOCAL
void redraw_console_sidebar();
#endif

void redraw_screen(bool show_updates = true);

string mpr_monster_list(bool past = false);
int update_monster_pane();

const char *equip_slot_to_name(int equip);
int equip_name_to_slot(const char *s);

int stealth_breakpoint(int stealth);

void print_overview_screen();

string mutation_overview();

string dump_overview_screen(bool full_id);
