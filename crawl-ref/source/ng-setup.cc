#include "AppHdr.h"

#include "ng-setup.h"

#include "ability.h"
#include "adjust.h"
#include "art-enum.h"
#include "artefact.h"
#include "dungeon.h"
#include "end.h"
#include "files.h"
#include "food.h"
#include "god-abil.h"
#include "god-companions.h"
#include "hints.h"
#include "invent.h"
#include "item-name.h"
#include "item-prop.h"
#include "items.h"
#include "item-use.h"
#include "jobs.h"
#include "mutation.h"
#include "ng-init.h"
#include "ng-wanderer.h"
#include "options.h"
#include "prompt.h"
#include "religion.h"
#include "shopping.h"
#include "skills.h"
#include "spl-book.h"
#include "spl-util.h"
#include "sprint.h"
#include "state.h"
#include "tiledef-player.h"

#define MIN_START_STAT       3

static void _init_player()
{
    you = player();
    dlua.callfn("dgn_clear_data", "");
}

// Make sure no stats are unacceptably low
// (currently possible only for GhBe - 1KB)
static void _unfocus_stats()
{
    int needed;

    for (int i = 0; i < NUM_STATS; ++i)
    {
        int j = (i + 1) % NUM_STATS;
        int k = (i + 2) % NUM_STATS;
        if ((needed = MIN_START_STAT - you.base_stats[i]) > 0)
        {
            if (you.base_stats[j] > you.base_stats[k])
                you.base_stats[j] -= needed;
            else
                you.base_stats[k] -= needed;
            you.base_stats[i] = MIN_START_STAT;
        }
    }
}

// Some consumables to make the starts of Sprint a little easier.
static void _give_bonus_items()
{
    newgame_make_item(OBJ_POTIONS, POT_CURING);
    newgame_make_item(OBJ_POTIONS, POT_HEAL_WOUNDS);
    newgame_make_item(OBJ_POTIONS, POT_HASTE);
    newgame_make_item(OBJ_POTIONS, POT_MAGIC, 2);
    newgame_make_item(OBJ_POTIONS, POT_BERSERK_RAGE);
    newgame_make_item(OBJ_SCROLLS, SCR_BLINKING);
}

static void _autopickup_ammo(missile_type missile)
{
    if (Options.autopickup_starting_ammo)
        you.force_autopickup[OBJ_MISSILES][missile] = AP_FORCE_ON;
}

/**
 * Make an item during character creation.
 *
 * Puts the item in the first available slot in the inventory, equipping or
 * memorising the first spell from it as appropriate. If the item would be
 * useless, we try with a possibly more useful sub_type, then, if it's still
 * useless, give up and don't create any item.
 *
 * @param base, sub_type what the item is
 * @param qty            what size stack to make
 * @param plus           what the value of item_def::plus should be
 * @param force_ego      what the value of item_def::special should be
 * @param force_tutorial whether to create it even in the tutorial
 * @returns a pointer to the item created, if any.
 */
item_def* newgame_make_item(object_class_type base,
                            int sub_type, int qty, int plus,
                            int force_ego, bool force_tutorial)
{
    // Don't set normal equipment in the tutorial.
    if (!force_tutorial && crawl_state.game_is_tutorial())
        return nullptr;

    // not an actual item
    // the WPN_UNKNOWN case is used when generating a paper doll during
    // character creation
    if (sub_type == WPN_UNARMED || sub_type == WPN_UNKNOWN)
        return nullptr;

    // Oni are known for wearing animal skins (see Wikipedia).
    if (you.species == SP_ONI && base == OBJ_ARMOUR && sub_type == ARM_ROBE)
        sub_type = ARM_ANIMAL_SKIN;

    int slot;
    for (slot = 0; slot < ENDOFPACK; ++slot)
    {
        if (base == OBJ_FOOD && slot == letter_to_index('e'))
            continue;

        item_def& item = you.inv[slot];
        if (!item.defined())
            break;

        if (item.is_type(base, sub_type) && item.brand == force_ego
            && is_stackable_item(item))
        {
            item.quantity += qty;
            return &item;
        }
    }

    item_def &item(you.inv[slot]);
    item.base_type = base;
    item.sub_type  = sub_type;
    item.quantity  = qty;
    item.plus      = plus;
    item.brand     = force_ego;

    if (force_ego < 0)
    {
        make_item_unrandart(item, -force_ego);
        // Duplicate of logic below
        if (you.equip[get_item_slot(item)] == -1)
        {
            you.equip[get_item_slot(item)] = slot;
        }
        return &item;
    }

    if(item.base_type == OBJ_BOOKS && you.char_class == JOB_UNDERSTUDY)
        return &item;

    // If the character is restricted in wearing the requested armour,
    // hand out a replacement instead.
    if (item.base_type == OBJ_ARMOUR
        && !can_wear_armour(item, false, false))
    {
        if (item.sub_type == ARM_HELMET || item.sub_type == ARM_HAT)
            item.sub_type = ARM_HAT;
        else if (item.sub_type == ARM_BUCKLER)
            item.sub_type = ARM_SHIELD;
        else if (is_shield(item))
            item.sub_type = ARM_BUCKLER;
        else if (item.sub_type == ARM_CLOAK)
            item.sub_type = ARM_SCARF;
        else
            item.sub_type = ARM_ROBE;
    }

    // Make sure we didn't get a stack of shields or such nonsense.
    ASSERT(item.quantity == 1 || is_stackable_item(item));

    // If that didn't help, nothing will.
    if (item.base_type != OBJ_BOOKS && is_useless_item(item))
    {
        item = item_def();
        return nullptr;
    }

    if ((item.base_type == OBJ_WEAPONS && can_wield(&item, false, false)
         || (item.base_type == OBJ_ARMOUR && can_wear_armour(item, false, false))
         || item.base_type == OBJ_JEWELLERY)
        && you.equip[get_item_slot(item)] == -1)
    {
        you.equip[get_item_slot(item)] = slot;
    }

    if (item.base_type == OBJ_MISSILES)
        _autopickup_ammo(static_cast<missile_type>(item.sub_type));
    // You can get the books without the corresponding items as a wanderer.
    else if (item.base_type == OBJ_BOOKS && item.sub_type == BOOK_GEOMANCY)
        _autopickup_ammo(MI_STONE);
    else if (item.base_type == OBJ_BOOKS && item.sub_type == BOOK_CHANGES)
        _autopickup_ammo(MI_ARROW);
    // You probably want to pick up both.
    if (item.is_type(OBJ_MISSILES, MI_SLING_BULLET))
        _autopickup_ammo(MI_STONE);

    origin_set_startequip(item);

    // Wanderers may or may not already have a spell. - bwr
    // Also, when this function gets called their possible randbook
    // has not been initialised and will trigger an ASSERT.
    if (item.base_type == OBJ_BOOKS && you.char_class != JOB_WANDERER)
    {
        spell_type which_spell = spells_in_book(item)[0];
        if (!spell_is_useless(which_spell, false, true)
            && spell_difficulty(which_spell) <= 1)
        {
            add_spell_to_memory(which_spell);
        }
    }

    return &item;
}

static void _give_ranged_weapon(weapon_type weapon, int plus)
{
    ASSERT(weapon != NUM_WEAPONS);

    switch (weapon)
    {
    case WPN_SHORTBOW:
    case WPN_HAND_CROSSBOW:
    case WPN_HUNTING_SLING:
        newgame_make_item(OBJ_WEAPONS, weapon, 1, plus);
        break;
    default:
        break;
    }
}

static void _give_ammo(weapon_type weapon, int plus)
{
    ASSERT(weapon != NUM_WEAPONS);

    switch (weapon)
    {
    case WPN_THROWN:
        if (species_can_throw_large_rocks(you.species))
            newgame_make_item(OBJ_MISSILES, MI_LARGE_ROCK, 4 + plus);
        else if (you.body_size(PSIZE_TORSO) <= SIZE_SMALL)
            newgame_make_item(OBJ_MISSILES, MI_BOOMERANG, 8 + 2 * plus);
        else
            newgame_make_item(OBJ_MISSILES, MI_JAVELIN, 5 + plus);
        newgame_make_item(OBJ_MISSILES, MI_THROWING_NET, 2);
        break;
    case WPN_SHORTBOW:
        newgame_make_item(OBJ_MISSILES, MI_ARROW, 20);
        break;
    case WPN_HAND_CROSSBOW:
        newgame_make_item(OBJ_MISSILES, MI_BOLT, 20);
        break;
    case WPN_HUNTING_SLING:
        newgame_make_item(OBJ_MISSILES, MI_SLING_BULLET, 20);
        break;
    default:
        break;
    }
}

static skill_type _archaeologist_armour_skill_unusable(int type)
{  // Only deals with auxiliary armour unrands not handled in the
   // switch-case in _setup_archaeologist_crate()
    switch (type)
    {
    case UNRAND_STARLIGHT:
        return SK_FIGHTING;
    case UNRAND_ALCHEMIST:
        return SK_SPELLCASTING;
    case UNRAND_RATSKIN_CLOAK:
        return SK_EVOCATIONS;
    default: // UNRAND_DYROVEPREVA
        return SK_DODGING;
    }
}

static skill_type _setup_archaeologist_crate(item_def& crate)
{
    item_def unrand;
    int type;
    const vector<int> unrands = archaeologist_unrands();

    do
    {
        unrand = item_def();
        type = unrands[random2(unrands.size())];
        if (!make_item_unrandart(unrand, type))
            continue;
    }
    while (!you.could_wield(unrand) && !can_wear_armour(unrand, false, true));

    dprf("Initializing archaeologist crate with %s",
         unrand.name(DESC_A).c_str());

    crate.props[ARCHAEOLOGIST_CRATE_ITEM] = type;

    // Handle items unlocked through interesting skills.
    // Jewellery only happens on felids.
    switch (type)
    {
    case UNRAND_PONDERING:
    case UNRAND_MAJIN:
    case UNRAND_ETHERIC_CAGE:
        return SK_SPELLCASTING;
    case UNRAND_DRAGONMASK:
    case UNRAND_WAR:
    case UNRAND_BEAR_SPIRIT:
    case UNRAND_BLOODLUST:
    case UNRAND_ROBUSTNESS:
    case UNRAND_SHIELDING:
    case UNRAND_VITALITY:
        return SK_FIGHTING;
    case UNRAND_PHASING:
    case UNRAND_FENCERS:
        return SK_DODGING;
    case UNRAND_THIEF:
    case UNRAND_BOOTS_ASSASSIN:
    case UNRAND_NIGHT:
    case UNRAND_SHADOWS:
        return SK_STEALTH;
    case UNRAND_ELEMENTAL_STAFF:
    case UNRAND_OLGREB:
    case UNRAND_WUCAD_MU:
        return SK_EVOCATIONS;
    default:
        break;
    }

    if (unrand.base_type == OBJ_JEWELLERY)
        return SK_SPELLCASTING;
    else if (unrand.base_type == OBJ_WEAPONS)
    {
        if(you.species == SP_HILL_ORC && !is_range_weapon(unrand))
            return SK_FIGHTING;
        else
            return item_attack_skill(unrand);
    }
    else if (is_shield(unrand))
        return SK_SHIELDS;
    else if (unrand.sub_type == ARM_ROBE || unrand.sub_type == ARM_ANIMAL_SKIN)
        return coinflip() ? SK_SPELLCASTING : SK_DODGING;
    else
        return species_apt(SK_ARMOUR) == UNUSABLE_SKILL
               ? _archaeologist_armour_skill_unusable(type)
               : SK_ARMOUR;
}

static void _setup_archaeologist()
{
    for (uint8_t i = 0; i < ENDOFPACK; i++)
        if (you.inv[i].defined())
        {
            if (you.inv[i].is_type(OBJ_ARMOUR, ARM_ROBE))
                you.inv[i].props["worn_tile"] = (short)TILEP_BODY_SLIT_BLACK;
            if (you.inv[i].is_type(OBJ_ARMOUR, ARM_GLOVES))
                you.inv[i].props["worn_tile"] = (short)TILEP_ARM_GLOVE_BROWN;
            if (you.inv[i].is_type(OBJ_ARMOUR, ARM_BOOTS))
                you.inv[i].props["worn_tile"] = (short)TILEP_BOOTS_MESH_BLACK;
            if (you.inv[i].is_type(OBJ_ARMOUR, ARM_HAT))
                you.inv[i].props["worn_tile"] = (short)TILEP_HELM_HAT_BLACK;
        }

    skill_type manual_skill = SK_NONE;
    for (uint8_t i = 0; i < ENDOFPACK; i++)
        if (you.inv[i].defined()
            && you.inv[i].is_type(OBJ_MISCELLANY, MISC_ANCIENT_CRATE))

        {
            manual_skill = _setup_archaeologist_crate(you.inv[i]);
        }

    for (uint8_t i = 0; i < ENDOFPACK; i++)
        if (you.inv[i].defined()
            && you.inv[i].is_type(OBJ_MISCELLANY, MISC_DUSTY_TOME))

        {
            you.inv[i].props[ARCHAEOLOGIST_TOME_SKILL] = manual_skill;
        }
}

void give_items_skills(const newgame_def& ng)
{
    create_wanderer();

    switch (you.char_class)
    {
    case JOB_BERSERKER:
        you.religion = GOD_TROG;
        you.piety = 35;

        if (you_can_wear(EQ_BODY_ARMOUR))
            you.skills[SK_ARMOUR] += 2;
        else
        {
            you.skills[SK_DODGING]++;
            if (!is_useless_skill(SK_ARMOUR))
                you.skills[SK_ARMOUR]++; // converted later
        }
        break;

    case JOB_CHAOS_KNIGHT:
    {
        you.religion = GOD_XOM;
        you.piety = 100;
        int timeout_rnd = random2(40);
        timeout_rnd += random2(40); // force a sequence point between random2s
        you.gift_timeout = max(5, timeout_rnd);

        if (species_apt(SK_ARMOUR) < species_apt(SK_DODGING))
            you.skills[SK_DODGING]++;
        else
            you.skills[SK_ARMOUR]++;
        break;
    }
    case JOB_ABYSSAL_KNIGHT:
        you.religion = GOD_LUGONU;
        if (!crawl_state.game_is_sprint())
            you.chapter = CHAPTER_POCKET_ABYSS;
        you.piety = 38;

        if (species_apt(SK_ARMOUR) < species_apt(SK_DODGING))
            you.skills[SK_DODGING]++;
        else
            you.skills[SK_ARMOUR]++;

        break;

    case JOB_BOUND:
        you.religion = GOD_ASHENZARI;
        you.piety = 35;

        if (species_apt(SK_ARMOUR) < species_apt(SK_DODGING))
            you.skills[SK_DODGING]++;
        else
            you.skills[SK_ARMOUR]++;
        break;

    case JOB_TORPOR_KNIGHT:
        you.religion = GOD_CHEIBRIADOS;
        you.piety = 35;

        if (species_apt(SK_ARMOUR) < species_apt(SK_DODGING))
            you.skills[SK_DODGING]++;
        else
            you.skills[SK_ARMOUR]++;
        break;

    case JOB_PALADIN:
        you.religion = GOD_SHINING_ONE;
        you.piety = 35;

        if (species_apt(SK_ARMOUR) < species_apt(SK_DODGING))
            you.skills[SK_DODGING]++;
        else
            you.skills[SK_ARMOUR]++;
        break;

    case JOB_SLIME_PRIEST:
        you.religion = GOD_JIYVA;
        you.piety = 5;

        if (species_apt(SK_ARMOUR) < species_apt(SK_DODGING))
            you.skills[SK_DODGING]++;
        else
            you.skills[SK_ARMOUR]++;
        break;

    case JOB_BLOOD_KNIGHT:
        you.religion = GOD_MAKHLEB;
        you.piety = 35;

        if (species_apt(SK_ARMOUR) < species_apt(SK_DODGING))
            you.skills[SK_DODGING]++;
        else
            you.skills[SK_ARMOUR]++;
        break;

    case JOB_STORM_CLERIC:
        you.religion = GOD_QAZLAL;
        you.piety = 35;

        if (species_apt(SK_ARMOUR) < species_apt(SK_DODGING))
            you.skills[SK_DODGING]++;
        else
            you.skills[SK_ARMOUR]++;
        break;

    case JOB_DEATH_BISHOP:
        you.religion = GOD_YREDELEMNUL;
        add_spell_to_memory(SPELL_PAIN);
        you.piety = 35;
        break;

    case JOB_GAMBLER:
        you.religion = GOD_NEMELEX_XOBEH;
        you.piety = 35;
        you.gift_timeout = 1;
        break;

    case JOB_DOCTOR:
        you.religion = GOD_ELYVILON;
        you.piety = 35;
        break;

    case JOB_GARDENER:
        you.religion = GOD_FEDHAS;
        you.piety = 35;
        break;

    case JOB_HERMIT:
    {
        you.religion = GOD_RU;
        you.piety = 10;
        you.piety_hysteresis = 0;
        you.gift_timeout = 0;
        you.props[RU_SACRIFICE_PROGRESS_KEY] = 9999;
        {
            int delay = 50;
            if (crawl_state.game_is_sprint())
                delay /= SPRINT_MULTIPLIER;
            you.props[RU_SACRIFICE_DELAY_KEY] = delay;
        }
        you.props[RU_SACRIFICE_PENALTY_KEY] = 0;
    }
        break;

    case JOB_WARRIOR:
        you.religion = GOD_OKAWARU;
        you.piety = 35;
        break;

    case JOB_WITNESS:
        you.religion = GOD_BEOGH;
        you.piety = 35;
        break;

    case JOB_KIKUMANCER:
        you.religion = GOD_KIKUBAAQUDGHA;
        you.piety = 35;
        break;

    case JOB_ZINJA:
        you.religion = GOD_ZIN;
        you.piety = 35;
        break;

    case JOB_NIGHT_KNIGHT:
        you.religion = GOD_DITHMENOS;
        you.piety = 35;
        break;

    case JOB_DISCIPLE:
        you.religion = GOD_WU_JIAN;
        you.piety = 35;
        break;

    case JOB_MERCHANT:
        you.religion = GOD_GOZAG;
        you.gold = 1;
        break;

    case JOB_ANNIHILATOR:
        you.religion = GOD_VEHUMET;
        you.piety = 35;
        add_spell_to_memory(SPELL_MAGIC_DART);
        you.gift_timeout = roll_dice(2, 5);
        break;

    case JOB_LIBRARIAN:
    {
        you.religion = GOD_SIF_MUNA;
        you.piety = 35;
        add_spell_to_memory(SPELL_APPORTATION);
        librarian_book();
        break;
    }

    case JOB_INHERITOR:
    {
        you.religion = GOD_HEPLIAKLQANA;
        you.piety = 35;
        bool female = coinflip();
        you.props[HEPLIAKLQANA_ALLY_NAME_KEY] = make_ancestor_name(female);
        you.props[HEPLIAKLQANA_ALLY_GENDER_KEY] = female ? GENDER_FEMALE
                                                         : GENDER_MALE;
        break;
    }

    case JOB_DANCER:
        you.religion = GOD_USKAYAW;
        you.piety = 200; //you had a really good party right before entering the dungeon
        break;

    case JOB_UNDERSTUDY:
        create_understudy();
        break;

    case JOB_WANDERER:
        create_wanderer();
        break;

    case JOB_UNCLE:
        add_spell_to_memory(SPELL_CONFUSING_TOUCH);
        add_spell_to_memory(SPELL_PAIN);
        break;

    case JOB_PHILOSOPHER:
        add_spell_to_memory(SPELL_CONFUSE);
        break;

    case JOB_ENTOMOLOGIST:
        add_spell_to_memory(SPELL_SUMMON_BUTTERFLIES);
        break;

    default:
        break;
    }

    if (you.char_class == JOB_ABYSSAL_KNIGHT)
        newgame_make_item(OBJ_WEAPONS, ng.weapon, 1, +1);
    else if (you.char_class == JOB_CHAOS_KNIGHT)
        newgame_make_item(OBJ_WEAPONS, ng.weapon, 1, 0, SPWPN_CHAOS);
    else if (you.char_class == JOB_WARRIOR)
        newgame_make_item(OBJ_WEAPONS, ng.weapon, 1, -2);
    else if (you.char_class == JOB_DEATH_BISHOP)
        newgame_make_item(OBJ_WEAPONS, ng.weapon, 1, 0, SPWPN_DRAINING);
    else if (you.char_class == JOB_PALADIN)
        newgame_make_item(OBJ_WEAPONS, ng.weapon, 1, +1, SPWPN_HOLY_WRATH);
    else if (you.char_class == JOB_DISCIPLE)
        newgame_make_item(OBJ_WEAPONS, ng.weapon, 1, +3);
    else if (you.char_class == JOB_NIGHT_KNIGHT)
    {
       newgame_make_item(OBJ_ARMOUR, ARM_ROBE, 1, +1, SPARM_STEALTH);
    }
    else if (you.char_class == JOB_BOUND)
    {
        item_def* wpn = newgame_make_item(OBJ_WEAPONS, ng.weapon, 1, +2);
        if(wpn)
        {
            do_curse_item(*wpn);
		}
        item_def* armour = newgame_make_item(OBJ_ARMOUR, ARM_ROBE, 1, +1);
        if(armour)
        {
			do_curse_item(*armour);
        }
        item_def* amulet = newgame_make_item(OBJ_JEWELLERY, AMU_THE_GOURMAND);
        if(amulet)
        {
            do_curse_item(*amulet);
        }
    }
    else if (job_gets_ranged_weapons(you.char_class))
        _give_ranged_weapon(ng.weapon, you.char_class == JOB_HUNTER ? 1 : 0);
    else if (job_has_weapon_choice(you.char_class))
        newgame_make_item(OBJ_WEAPONS, ng.weapon);

    give_job_equipment(you.char_class);
    give_job_skills(you.char_class);

    if (job_gets_ranged_weapons(you.char_class))
        _give_ammo(ng.weapon, you.char_class == JOB_HUNTER ? 1 : 0);

    if (you.species == SP_FELID || you.species == SP_BUTTERFLY)
    {
        you.skills[SK_THROWING] = 0;
        you.skills[SK_SHIELDS] = 0;
    }

    if (!you_worship(GOD_NO_GOD))
    {
        you.worshipped[you.religion] = 1;
        set_god_ability_slots();
        if (!you_worship(GOD_XOM))
            you.piety_max[you.religion] = you.piety;
    }

    if (you.char_class == JOB_ARCHAEOLOGIST)
        _setup_archaeologist();
}

static void _give_starting_food()
{
    // No food for those who don't need it.
    if (you_foodless())
        return;

    if (you.char_class == JOB_CAVEPERSON)
        return;

    object_class_type base_type = OBJ_FOOD;
    int sub_type = FOOD_RATION;
    int quantity = 1;

    // Give another one for hungry species.
    if (you.get_mutation_level(MUT_FAST_METABOLISM))
        quantity = 2;

    newgame_make_item(base_type, sub_type, quantity);
}

static void _setup_tutorial_miscs()
{
    // Allow for a few specific hint mode messages.
    // A few more will be initialised by the tutorial map.
    tutorial_init_hints();

    // No gold to begin with.
    you.gold = 0;

    // Give them some mana to play around with.
    you.mp_max_adj += 2;

    newgame_make_item(OBJ_ARMOUR, ARM_ROBE, 1, 0, 0, true);

    // No need for Shields skill without shield.
    you.skills[SK_SHIELDS] = 0;

    // Some spellcasting for the magic tutorial.
    if (crawl_state.map.find("tutorial_lesson4") != string::npos)
        you.skills[SK_SPELLCASTING] = 1;
}

static void _give_basic_knowledge()
{
    identify_inventory();

    // Removed item types are handled in _set_removed_types_as_identified.
}

static void _setup_normal_game();
static void _setup_tutorial(const newgame_def& ng);
static void _setup_sprint(const newgame_def& ng);
static void _setup_hints();
static void _setup_generic(const newgame_def& ng);

// Initialise a game based on the choice stored in ng.
void setup_game(const newgame_def& ng)
{
    crawl_state.type = ng.type; // by default
    if (Options.seed_from_rc && ng.type != GAME_TYPE_CUSTOM_SEED)
    {
        // rc seed overrides seed from other sources, and forces a normal game
        // to be a custom seed game. There is currently no special designation
        // for sprint etc. games that involve a custom seed.
        // TODO: does this make any sense for hints mode?
        Options.seed = Options.seed_from_rc;
        if (ng.type == GAME_TYPE_NORMAL)
            crawl_state.type = GAME_TYPE_CUSTOM_SEED;
    }
    else if (Options.seed && ng.type != GAME_TYPE_CUSTOM_SEED)
    {
        // there's a seed lingering in the options, but we shouldn't use it.
        Options.seed = 0;
    }
    else if (!Options.seed && ng.type == GAME_TYPE_CUSTOM_SEED)
        crawl_state.type = GAME_TYPE_NORMAL;

    crawl_state.map  = ng.map;

    switch (crawl_state.type)
    {
    case GAME_TYPE_NORMAL:
    case GAME_TYPE_CUSTOM_SEED:
        _setup_normal_game();
        break;
    case GAME_TYPE_TUTORIAL:
        _setup_tutorial(ng);
        break;
    case GAME_TYPE_SPRINT:
        _setup_sprint(ng);
        break;
    case GAME_TYPE_HINTS:
        _setup_hints();
        break;
    case GAME_TYPE_ARENA:
    default:
        die("Bad game type");
        end(-1);
    }

    _setup_generic(ng);
}

/**
 * Special steps that normal game needs;
 */
static void _setup_normal_game()
{
    make_hungry(0, true);
}

/**
 * Special steps that tutorial game needs;
 */
static void _setup_tutorial(const newgame_def& ng)
{
    make_hungry(0, true);
}

/**
 * Special steps that sprint needs;
 */
static void _setup_sprint(const newgame_def& ng)
{
    // nothing currently
}

/**
 * Special steps that hints mode needs;
 */
static void _setup_hints()
{
    init_hints();
}

static void _free_up_slot(char letter)
{
    for (int slot = 0; slot < ENDOFPACK; ++slot)
    {
        if (!you.inv[slot].defined())
        {
            swap_inv_slots(letter_to_index(letter),
                           slot, false);
            break;
        }
    }
}

void initial_dungeon_setup()
{
    rng::generator levelgen_rng(BRANCH_DUNGEON);

    initialise_branch_depths();
    initialise_temples();
    init_level_connectivity();
    initialise_item_descriptions();
}

static void _setup_generic(const newgame_def& ng)
{
    rng::reset(); // initialize rng from Options.seed
    _init_player();
    you.game_seed = crawl_state.seed;
    you.deterministic_levelgen = Options.incremental_pregen;

#if TAG_MAJOR_VERSION == 34
    // Avoid the remove_dead_shops() Gozag fixup in new games: see
    // ShoppingList::item_type_identified().
    you.props[REMOVED_DEAD_SHOPS_KEY] = true;
#endif

    // Needs to happen before we give the player items, so that it's safe to
    // check whether those items need to be removed from their shopping list.
    shopping_list.refresh();

    you.your_name  = ng.name;
    you.species    = ng.species;
    you.char_class = ng.job;

    you.chr_class_name = get_job_name(you.char_class);

    // You can only set the undead type for species that default to alive
    const undead_state_type species_undead = species_undead_type(ng.species);
    you.undead_modifier = (species_undead == US_ALIVE) ? ng.undead_type : species_undead;

    you.skill_modifier = ng.skilled_type;
    you.chaoskin = ng.chaoskin;
    you.no_locks = ng.no_locks;
    you.trap_type = ng.trap_type;

    if (ng.species == SP_SHAPESHIFTER)
        you.shapeshifter_species = true;

    // Only relevant for Argons, but saved for every species
    you.vaporous_resistance_fire = 0;
    you.vaporous_resistance_cold = 0;
    you.vaporous_resistance_neg = 0;
    you.vaporous_resistance_elec = 0;
    you.vaporous_resistance_poison = 0;
    you.argon_flashes_available = 0;

    species_stat_init(you.species);     // must be down here {dlb}

    // Before we get into the inventory init, set light radius based
    // on species vision.
    update_vision_range();

    job_stat_init(you.char_class);

    _unfocus_stats();

    // Needs to be done before handing out food.
    give_basic_mutations(you.species);

    // This function depends on stats and mutations being finalised.
    give_items_skills(ng);

    roll_demonspawn_mutations();

    _give_starting_food();

    if (crawl_state.game_is_sprint())
        _give_bonus_items();

    // Leave the a/b slots open so if the first thing you pick up is a weapon,
    // you can use ' to swap between your items.
    if (you.char_class == JOB_EARTH_ELEMENTALIST)
        _free_up_slot('a');
    if (you.char_class == JOB_ARCANE_MARKSMAN && ng.weapon != WPN_THROWN)
        _free_up_slot('b');

    // Give tutorial skills etc
    if (crawl_state.game_is_tutorial())
        _setup_tutorial_miscs();

    _give_basic_knowledge();

    // Must be after _give_basic_knowledge
    add_held_books_to_library();

    if (you.char_class == JOB_WANDERER)
        memorise_wanderer_spell();

    // A first pass to link the items properly.
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        auto &item = you.inv[i];
        if (!item.defined())
            continue;
        item.pos = ITEM_IN_INVENTORY;
        item.link = i;
        item.slot = index_to_letter(item.link);
        item_colour(item);  // set correct special and colour
    }

    if (you.equip[EQ_WEAPON] > 0)
        swap_inv_slots(0, you.equip[EQ_WEAPON], false);

    // A second pass to apply the item_slot option.
    for (auto &item : you.inv)
    {
        if (!item.defined())
            continue;
        if (!item.props.exists("adjusted"))
        {
            item.props["adjusted"] = true;
            auto_assign_item_slot(item);
        }
    }

    reassess_starting_skills();
    init_skill_order();
    init_can_currently_train();
    init_train();
    init_training();

    // Apply autoinscribe rules to inventory.
    request_autoinscribe();
    autoinscribe();

    // We calculate hp and mp here; all relevant factors should be
    // finalised by now. (GDL)
    calc_hp();
    calc_mp();

    // Make sure the starting player is fully charged up.
    set_hp(you.hp_max);
    set_mp(you.max_magic_points);

    //Mutate Slime Priest
    if (you.char_class == JOB_SLIME_PRIEST)
    {
        mutate(RANDOM_SLIME_MUTATION, "Slime Spawn's Curse", false);
        mutate(RANDOM_MUTATION, "Slime Spawn's Curse", false);
        mutate(RANDOM_MUTATION, "Slime Spawn's Curse", false);
    }

    initial_dungeon_setup();

    // Generate the second name of Jiyva
    fix_up_jiyva_name();

    // Get rid of god companions left from previous games
    init_companions();

    // Create the save file.
    if (Options.no_save)
        you.save = new package();
    else
        you.save = new package(get_savedir_filename(you.your_name).c_str(),
                               true, true);
}
