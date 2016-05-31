/**
 * @file
 * @brief Stuff related to conducts.
**/

#ifndef GODCONDUCT_H
#define GODCONDUCT_H

// Calls did_god_conduct() when the object goes out of scope.
struct god_conduct_trigger
{
    conduct_type conduct;
    int pgain;
    bool known;
    bool enabled;
    unique_ptr<monster> victim;

    god_conduct_trigger(conduct_type c = NUM_CONDUCTS,
                        int pg = 0,
                        bool kn = true,
                        const monster* vict = nullptr);

    void set(conduct_type c = NUM_CONDUCTS,
             int pg = 0,
             bool kn = true,
             const monster* vict = nullptr);

    ~god_conduct_trigger();
};

void did_kill_conduct(conduct_type thing_done, const monster &victim);
void did_god_conduct(conduct_type thing_done, int level, bool known = true,
                     const monster* victim = nullptr);
void set_attack_conducts(god_conduct_trigger conduct[3], const monster* mon,
                         bool known = true);
void enable_attack_conducts(god_conduct_trigger conduct[3]);
void disable_attack_conducts(god_conduct_trigger conduct[3]);
void did_hurt_conduct(conduct_type thing_done, const monster &victim, int damage_done);

void god_conduct_turn_start();

bool god_punishes_spell(spell_type spell, god_type god);
bool god_hates_cannibalism(god_type god);

string get_god_likes(god_type which_god);
string get_god_dislikes(god_type which_god);
#endif
