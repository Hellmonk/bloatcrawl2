#ifndef PLAYER_REACTS_H
#define PLAYER_REACTS_H
// Used only in world_reacts()
void player_reacts();
void player_reacts_to_monsters();

// Only function other than decrement_duratons() which uses decrement_a_duration()
void extract_manticore_spikes(const char* endmsg);
#endif
