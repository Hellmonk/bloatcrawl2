/**
 * @file
 * @brief Misc functions.
**/

#ifndef ITEMPROP_H
#define ITEMPROP_H

#include <set>

#include "itemprop-enum.h"

struct bolt;

void init_properties();


typedef uint32_t armflags_t;
#define ard(flg, lev) (armflags_t)((flg) * ((lev) & 7))

enum armour_flag
{
    ARMF_NO_FLAGS           = 0,
    // multilevel resistances
    ARMF_RES_FIRE           = 1 << 0,
    ARMF_RES_COLD           = 1 << 3,
    ARMF_RES_NEG            = 1 << 6,
    // misc (multilevel)
    ARMF_STEALTH            = 1 << 9,
    ARMF_REGENERATION       = 1 << 13,

    ARMF_LAST_MULTI, // must be >= any multi, < any boolean, exact value doesn't matter

    // boolean resists
    ARMF_RES_MAGIC          = 1 << 17,
    ARMF_RES_ELEC           = 1 << 18,
    ARMF_RES_POISON         = 1 << 19,
    ARMF_RES_STICKY_FLAME   = 1 << 20,
    ARMF_RES_STEAM          = 1 << 21,
    // vulnerabilities
    ARMF_VUL_FIRE           = ard(ARMF_RES_FIRE, -1),
    ARMF_VUL_COLD           = ard(ARMF_RES_COLD, -1),
};

/// Removed items that have item knowledge.
extern const set<pair<object_class_type, int> > removed_items;
/// Check for membership in removed_items.
bool item_type_removed(object_class_type base, int subtype);

// cursed:
bool item_known_cursed(const item_def &item) PURE;
bool item_is_cursable(const item_def &item, bool ignore_holy_wrath = false);
bool curse_an_item(bool ignore_holy_wrath = false);
void do_curse_item(item_def &item, bool quiet = true);
void do_uncurse_item(item_def &item, bool inscribe = true, bool no_ash = false,
                     bool check_bondage = true);
inline constexpr bool item_type_has_curses(object_class_type base_type)
{
        return base_type == OBJ_WEAPONS || base_type == OBJ_ARMOUR
               || base_type == OBJ_JEWELLERY || base_type == OBJ_STAVES
               || base_type == OBJ_RODS;
}

// stationary:
void set_net_stationary(item_def &item);
bool item_is_stationary(const item_def &item) PURE;
bool item_is_stationary_net(const item_def &item) PURE;
actor *net_holdee(const item_def &net);

// ident:
bool item_ident(const item_def &item, iflags_t flags) PURE;
void set_ident_flags(item_def &item, iflags_t flags);
void unset_ident_flags(item_def &item, iflags_t flags);
bool fully_identified(const item_def &item) PURE;

// item descriptions:
void     set_equip_desc(item_def &item, iflags_t flags);
iflags_t get_equip_desc(const item_def &item) PURE;

bool  is_helmet(const item_def &item) PURE;
bool  is_hard_helmet(const item_def &item) PURE;

// ego items:
brand_type choose_weapon_brand(weapon_type wpn_type);
bool set_item_ego_type(item_def &item, object_class_type item_type,
                       int ego_type);
brand_type get_weapon_brand(const item_def &item) PURE;
special_armour_type get_armour_ego_type(const item_def &item) PURE;
special_missile_type get_ammo_brand(const item_def &item) PURE;

// armour functions:
int armour_max_enchant(const item_def &item) PURE;
bool armour_type_is_hide(int type, bool inc_made = false) PURE;
bool armour_is_hide(const item_def &item, bool inc_made = false) PURE;
bool armour_is_special(const item_def &item) PURE;
int armour_acq_weight(const armour_type armour) PURE;

equipment_type get_armour_slot(const item_def &item) PURE;
equipment_type get_armour_slot(armour_type arm) IMMUTABLE;

bool jewellery_is_amulet(const item_def &item) PURE;
bool jewellery_is_amulet(int sub_type) IMMUTABLE;

armour_type hide_for_monster(monster_type mc) PURE;
armour_type armour_for_hide(armour_type hide_type) PURE;
monster_type monster_for_hide(armour_type arm) PURE;
bool  hide2armour(item_def &item);

int fit_armour_size(const item_def &item, size_type size) PURE;
bool check_armour_size(const item_def &item, size_type size) PURE;
bool check_armour_size(armour_type sub_type, size_type size) PURE;

bool item_is_rechargeable(const item_def &it, bool hide_charged = false) PURE;
int wand_charge_value(int type) PURE;
int wand_max_charges(const item_def &item) PURE;
bool is_known_empty_wand(const item_def &item) PURE;
bool is_offensive_wand(const item_def &item) PURE;
bool is_enchantable_armour(const item_def &arm, bool uncurse,
                           bool unknown = false) PURE;

bool is_shield(const item_def &item) PURE;
bool is_shield_incompatible(const item_def &weapon,
                            const item_def *shield = nullptr) PURE;
bool shield_reflects(const item_def &shield) PURE;
void ident_reflector(item_def *item);

// Only works for armour/weapons/missiles
// weapon functions:
int weapon_rarity(int w_type) IMMUTABLE;

bool  is_weapon_wieldable(const item_def &item, size_type size) PURE;

hands_reqd_type basic_hands_reqd(const item_def &item, size_type size) PURE;
hands_reqd_type hands_reqd(const actor* ac, object_class_type base_type, int sub_type);

bool is_giant_club_type(int wpn_type) IMMUTABLE;
bool is_ranged_weapon_type(int wpn_type) IMMUTABLE;
bool is_blessed_weapon_type(int wpn_type) IMMUTABLE;
bool is_demonic_weapon_type(int wpn_type) IMMUTABLE;
bool is_magic_weapon_type(int wpn_type) IMMUTABLE;

bool is_melee_weapon(const item_def &weapon) PURE;
bool is_demonic(const item_def &item) PURE;
bool is_blessed(const item_def &item) PURE;
bool is_blessed_convertible(const item_def &item) PURE;
bool convert2good(item_def &item);
bool convert2bad(item_def &item);

int get_vorpal_type(const item_def &item) PURE;
int get_damage_type(const item_def &item) PURE;
int single_damage_type(const item_def &item) PURE;

bool is_brandable_weapon(const item_def &wpn, bool allow_ranged, bool divine = false);

skill_type item_attack_skill(const item_def &item) PURE;
skill_type item_attack_skill(object_class_type wclass, int wtype) IMMUTABLE;

bool staff_uses_evocations(const item_def &item);
bool item_skills(const item_def &item, set<skill_type> &skills);

// launcher and ammo functions:
bool is_range_weapon(const item_def &item) PURE;
missile_type fires_ammo_type(const item_def &item) PURE;
const char *ammo_name(missile_type ammo) IMMUTABLE;
const char *ammo_name(const item_def &bow) PURE;
const char *ammo_name(const weapon_type bow) PURE;
bool has_launcher(const item_def &ammo) PURE;
bool is_throwable(const actor *actor, const item_def &wpn,
                  bool force = false) PURE;
launch_retval is_launched(const actor *actor, const item_def *launcher,
                          const item_def &missile) PURE;

bool ammo_always_destroyed(const item_def &missile) PURE;
bool ammo_never_destroyed(const item_def &missile) PURE;
int  ammo_type_destroy_chance(int missile_type) PURE;
int  ammo_type_damage(int missile_type) PURE;


reach_type weapon_reach(const item_def &item) PURE;

// Macguffins
bool item_is_unique_rune(const item_def &item) PURE;
bool item_is_orb(const item_def &orb) PURE;
bool item_is_horn_of_geryon(const item_def &item) PURE;
bool item_is_spellbook(const item_def &item) PURE;

bool is_xp_evoker(const item_def &item);
int &evoker_debt(int evoker_type);
bool evoker_is_charged(const item_def &item);

// ring functions:
bool jewellery_has_pluses(const item_def &item) PURE;
bool ring_has_stackable_effect(const item_def &item) PURE;

// food functions:
#if TAG_MAJOR_VERSION == 34
bool is_real_food(food_type food) PURE;
#endif
bool is_blood_potion(const item_def &item) PURE;
bool food_is_meaty(int food_type) PURE;
bool food_is_meaty(const item_def &item) PURE;
bool food_is_veggie(int food_type) PURE;
bool food_is_veggie(const item_def &item) PURE;
int food_value(const item_def &item) PURE;
int food_turns(const item_def &item) PURE;
bool can_cut_meat(const item_def &item) PURE;
bool is_fruit(const item_def &item) PURE;

// generic item property functions:
int armour_type_prop(const uint8_t arm, const armour_flag prop) PURE;

int get_armour_res_fire(const item_def &arm, bool check_artp) PURE;
int get_armour_res_cold(const item_def &arm, bool check_artp) PURE;
int get_armour_res_poison(const item_def &arm, bool check_artp) PURE;
int get_armour_res_elec(const item_def &arm, bool check_artp) PURE;
int get_armour_life_protection(const item_def &arm, bool check_artp) PURE;
int get_armour_res_magic(const item_def &arm, bool check_artp) PURE;
int get_armour_res_sticky_flame(const item_def &arm) PURE;
bool get_armour_see_invisible(const item_def &arm, bool check_artp) PURE;

int get_jewellery_res_fire(const item_def &ring, bool check_artp) PURE;
int get_jewellery_res_cold(const item_def &ring, bool check_artp) PURE;
int get_jewellery_res_poison(const item_def &ring, bool check_artp) PURE;
int get_jewellery_res_elec(const item_def &ring, bool check_artp) PURE;
int get_jewellery_life_protection(const item_def &ring, bool check_artp) PURE;
int get_jewellery_res_magic(const item_def &ring, bool check_artp) PURE;
bool get_jewellery_see_invisible(const item_def &ring, bool check_artp) PURE;

int property(const item_def &item, int prop_type) PURE;
int armour_prop(int armour, int prop_type) PURE;
bool gives_ability(const item_def &item) PURE;
bool gives_resistance(const item_def &item) PURE;
bool item_is_jelly_edible(const item_def &item);
equipment_type get_item_slot(object_class_type type, int sub_type) IMMUTABLE;
equipment_type get_item_slot(const item_def &item) PURE;

string item_base_name(const item_def &item);
string item_base_name(object_class_type type, int sub_type);
string food_type_name(int sub_type);
const char *weapon_base_name(weapon_type subtype) IMMUTABLE;
weapon_type name_nospace_to_weapon(string name_nospace);

void seen_item(const item_def &item);

static inline bool is_weapon(const item_def &item)
{
    return item.base_type == OBJ_WEAPONS
           || item.base_type == OBJ_STAVES
           || item.base_type == OBJ_RODS;
}

void remove_whitespace(string &str);

void auto_id_inventory();

#endif
