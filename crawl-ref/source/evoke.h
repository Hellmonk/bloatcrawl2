/**
 * @file
 * @brief Functions for using some of the wackier inventory items.
**/

#pragma once

int manual_slot_for_skill(skill_type skill);
int get_all_manual_charges_for_skill(skill_type skill);
bool skill_has_manual(skill_type skill);
void finish_manual(int slot);
void get_all_manual_charges(vector<int> &charges);
void set_all_manual_charges(const vector<int> &charges);
string manual_skill_names(bool short_text=false);

void wind_blast(actor* agent, int pow, coord_def target, bool card = false);

bool evoke_check(int slot, bool quiet = false);
bool evoke_item(int slot = -1);
int wand_mp_cost();
void zap_wand(int slot = -1);

void archaeologist_open_crate(item_def& crate);
void archaeologist_read_tome(item_def& tome);

void black_drac_breath();
