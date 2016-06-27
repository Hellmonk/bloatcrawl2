/**
 * @file
 * @brief Functions for letting the player adjust letter assignments.
 **/

#ifndef ADJUST_H
#define ADJUST_H

void adjust();
void adjust_item(int from_slot = -1);
void swap_inv_slots(int slot1, int slot2, bool verbose);

#endif
