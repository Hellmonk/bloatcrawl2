/**
 * @file
 * @brief Functions with decks of cards.
**/

#ifndef DECKS_H
#define DECKS_H

#include "enum.h"

#define CARD_KEY "cards"
#define CARD_FLAG_KEY "card_flags"
#define DRAWN_CARD_KEY "drawn_cards"

// DECK STRUCTURE: deck.initial_cards is the number of cards the deck *started*
// with, deck.used_count is* the number of cards drawn, deck.rarity is the
// deck rarity, deck.props[CARD_KEY] holds the list of cards (with the
// highest index card being the top card, and index 0 being the bottom
// card), deck.props[DRAWN_CARD_KEY] holds the list of drawn cards
// (with index 0 being the first drawn), deck.props[CARD_FLAG_KEY]
// holds the flags for each card.
//
// *if deck.used_count is negative, it's actually -(cards_left). wtf.
//
// The card type and per-card flags are each stored as unsigned bytes,
// for a maximum of 256 different kinds of cards and 8 bits of flags.

/// The minimum number of cards a deck starts with, when generated normally.
const int MIN_STARTING_CARDS = 4;
/// The maximum number of cards a deck starts with, when generated normally.
const int MAX_STARTING_CARDS = 13;

enum deck_type
{
    // pure decks
    DECK_OF_ESCAPE,
    DECK_OF_DESTRUCTION,
#if TAG_MAJOR_VERSION == 34
    DECK_OF_DUNGEONS,
#endif
    DECK_OF_SUMMONING,
#if TAG_MAJOR_VERSION == 34
    DECK_OF_WONDERS,
#endif
};

enum card_flags_type
{
                      //1 << 0
    CFLAG_SEEN       = (1 << 1),
                      //1 << 2
    CFLAG_PUNISHMENT = (1 << 3),
    CFLAG_DEALT      = (1 << 4),
};

enum card_type
{
#if TAG_MAJOR_VERSION == 34
    CARD_PORTAL,              // teleport, maybe controlled
    CARD_WARP,                // blink, maybe controlled
    CARD_SWAP,                // player and monster position
#endif
    CARD_VELOCITY,            // remove slow, alter others' speeds

    CARD_TOMB,                // a ring of rock walls
#if TAG_MAJOR_VERSION == 34
    CARD_BANSHEE,             // cause fear and drain
#endif
    CARD_EXILE,               // banish others, maybe self
#if TAG_MAJOR_VERSION == 34
    CARD_SOLITUDE,            // dispersal
    CARD_WARPWRIGHT,          // create teleport trap
#endif
    CARD_SHAFT,               // under the user, maybe others

    CARD_VITRIOL,             // acid damage
    CARD_CLOUD,               // encage enemies in rings of clouds
#if TAG_MAJOR_VERSION == 34
    CARD_HAMMER,              // straightforward earth conjurations
    CARD_VENOM,               // poison damage, maybe poison vuln
    CARD_FORTITUDE,           // strength and damage shaving
#endif
    CARD_STORM,               // wind and rain
    CARD_PAIN,                // necromancy, manipulating life itself
    CARD_TORMENT,             // symbol of
    CARD_ORB,                 // pure bursts of energy

    CARD_ELIXIR,              // restoration of hp and mp
#if TAG_MAJOR_VERSION == 34
    CARD_BATTLELUST,          // melee boosts
    CARD_METAMORPHOSIS,       // transmutations
    CARD_HELM,                // defence boosts
    CARD_BLADE,               // cleave status
    CARD_SHADOW,              // stealth and darkness
    CARD_MERCENARY,           // costly perma-ally

    CARD_CRUSADE,             // aura of abjuration and mass enslave
    CARD_SUMMON_ANIMAL,       // scattered herd
#endif
    CARD_SUMMON_DEMON,        // dual demons
    CARD_SUMMON_WEAPON,       // a dance partner
    CARD_SUMMON_FLYING,       // swarms from the swamp
#if TAG_MAJOR_VERSION == 34
    CARD_SUMMON_SKELETON,     // bones, bones, bones
    CARD_SUMMON_UGLY,         // or very, or both

    CARD_POTION,              // random boost, probably also for allies
    CARD_FOCUS,               // lowest stat down, highest stat up
    CARD_SHUFFLE,             // stats, specifically
    CARD_EXPERIENCE,          // like the potion
#endif
    CARD_WILD_MAGIC,          // miscasts for everybody
#if TAG_MAJOR_VERSION == 34
    CARD_SAGE,                // skill training
    CARD_HELIX,               // precision mutation alteration
    CARD_ALCHEMIST,           // health / mp for gold

    CARD_WATER,               // flood squares, summon water monsters
    CARD_GLASS,               // make walls transparent
    CARD_DOWSING,             // mapping/detect traps/items/monsters
    CARD_TROWEL,              // create altars, statues, portal
    CARD_MINEFIELD,           // plant traps
#endif
    CARD_STAIRS,              // moves stairs around

#if TAG_MAJOR_VERSION == 34
    CARD_GENIE,               // acquirement or rotting/deterioration
    CARD_BARGAIN,             // shopping discount
#endif
    CARD_WRATH,               // random godly wrath
    CARD_WRAITH,              // drain XP
    CARD_XOM,                 // 's attention turns to you
#if TAG_MAJOR_VERSION == 34
    CARD_FEAST,               // engorged
#endif
    CARD_FAMINE,              // starving
    CARD_CURSE,               // curse your items
    CARD_SWINE,               // *oink*

    CARD_ILLUSION,            // a copy of the player
    CARD_DEGEN,               // polymorph hostiles down hd, malmutate
    CARD_ELEMENTS,            // primal animals of the elements
    CARD_RANGERS,             // sharpshooting
#if TAG_MAJOR_VERSION == 34
    CARD_PLACID_MAGIC,        // cancellation and antimagic
#endif

    NUM_CARDS
};

const char* card_name(card_type card);
card_type name_to_card(string name);
const string deck_contents(uint8_t deck_type);
void evoke_deck(item_def& deck);
bool deck_triple_draw();
bool deck_deal();
string which_decks(card_type card);
bool deck_stack();
void nemelex_shuffle_decks();
void shuffle_all_decks_on_level();

bool draw_three(int slot);
bool stack_five(int slot);
bool recruit_mercenary(int mid);

void card_effect(card_type which_card, deck_rarity_type rarity,
                 uint8_t card_flags = 0, bool tell_card = true);
void draw_from_deck_of_punishment(bool deal = false);

bool      top_card_is_known(const item_def &item);
card_type top_card(const item_def &item);

string deck_name(uint8_t type);
uint8_t deck_type_by_name(string name);
uint8_t random_deck_type();
bool is_deck_type(uint8_t type, bool allow_unided = false);
bool is_deck(const item_def &item, bool iinfo = false);
bool bad_deck(const item_def &item);
colour_t deck_rarity_to_colour(deck_rarity_type rarity);
void init_deck(item_def &item);

int cards_in_deck(const item_def &deck);
card_type get_card_and_flags(const item_def& deck, int idx,
                             uint8_t& _flags);

const vector<card_type> get_drawn_cards(const item_def& deck);

#endif
