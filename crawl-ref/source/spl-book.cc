/**
 * @file
 * @brief Spellbook/rod contents array and management functions
**/

#include "AppHdr.h"

#include "spl-book.h"
#include "book-data.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <set>

#include "artefact.h"
#include "colour.h"
#include "database.h"
#include "delay.h"
#include "describe.h"
#include "describe-spells.h"
#include "end.h"
#include "english.h"
#include "godconduct.h"
#include "goditem.h"
#include "invent.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "output.h"
#include "prompt.h"
#include "religion.h"
#include "spl-util.h"
#include "state.h"
#include "stringutil.h"
#ifdef USE_TILE
 #include "tilepick.h"
#endif
#include "transform.h"
#include "unicode.h"

#define RANDART_BOOK_TYPE_KEY  "randart_book_type"
#define RANDART_BOOK_LEVEL_KEY "randart_book_level"

#define RANDART_BOOK_TYPE_LEVEL "level"
#define RANDART_BOOK_TYPE_THEME "theme"

static const map<rod_type, spell_type> _rod_spells =
{
    { ROD_LIGHTNING,   SPELL_THUNDERBOLT },
#if TAG_MAJOR_VERSION == 34
    { ROD_SWARM,       SPELL_SUMMON_SWARM },
#endif
    { ROD_IGNITION,    SPELL_EXPLOSIVE_BOLT },
    { ROD_CLOUDS,      SPELL_CLOUD_CONE  },
#if TAG_MAJOR_VERSION == 34
    { ROD_DESTRUCTION, SPELL_RANDOM_BOLT },
#endif
    { ROD_INACCURACY,  SPELL_BOLT_OF_INACCURACY },
    { ROD_SHADOWS,     SPELL_WEAVE_SHADOWS },
    { ROD_IRON,        SPELL_SCATTERSHOT },
#if TAG_MAJOR_VERSION == 34
    { ROD_WARDING,     SPELL_NO_SPELL },
    { ROD_VENOM,       SPELL_NO_SPELL },
#endif
};

spell_type spell_in_rod(rod_type rod)
{
    if (const spell_type* const spl = map_find(_rod_spells, rod))
        return *spl;
    die("unknown rod type %d", rod);
}

vector<spell_type> spells_in_book(const item_def &book)
{
    ASSERT(book.base_type == OBJ_BOOKS || book.base_type == OBJ_RODS);

    vector<spell_type> ret;
    if (book.base_type == OBJ_RODS)
    {
        if (item_type_known(book))
            ret.emplace_back(spell_in_rod(static_cast<rod_type>(book.sub_type)));
        return ret;
    }

    const CrawlHashTable &props = book.props;
    if (!props.exists(SPELL_LIST_KEY))
        return spellbook_template(static_cast<book_type>(book.sub_type));

    const CrawlVector &spells = props[SPELL_LIST_KEY].get_vector();

    ASSERT(spells.get_type() == SV_INT);
    ASSERT(spells.size() == RANDBOOK_SIZE);

    for (int spell : spells)
        //TODO: don't put SPELL_NO_SPELL in them in the first place.
        if (spell != SPELL_NO_SPELL)
            ret.emplace_back(static_cast<spell_type>(spell));

    return ret;
}

vector<spell_type> spellbook_template(book_type book)
{
    ASSERT_RANGE(book, 0, (int)ARRAYSZ(spellbook_templates));
    return spellbook_templates[book];
}

// Rarity 100 is reserved for unused books.
int book_rarity(book_type which_book)
{
    if (item_type_removed(OBJ_BOOKS, which_book))
        return 100;

    switch (which_book)
    {
    case BOOK_MINOR_MAGIC:
    case BOOK_MISFORTUNE:
    case BOOK_CANTRIPS:
        return 1;

    case BOOK_CHANGES:
    case BOOK_MALEDICT:
        return 2;

    case BOOK_CONJURATIONS:
    case BOOK_NECROMANCY:
    case BOOK_CALLINGS:
        return 3;

    case BOOK_FLAMES:
    case BOOK_FROST:
    case BOOK_AIR:
    case BOOK_GEOMANCY:
        return 4;

    case BOOK_YOUNG_POISONERS:
    case BOOK_BATTLE:
    case BOOK_DEBILITATION:
        return 5;

    case BOOK_CLOUDS:
    case BOOK_POWER:
        return 6;

    case BOOK_ENCHANTMENTS:
    case BOOK_PARTY_TRICKS:
        return 7;

    case BOOK_TRANSFIGURATIONS:
    case BOOK_BEASTS:
        return 8;

    case BOOK_FIRE:
    case BOOK_ICE:
    case BOOK_SKY:
    case BOOK_EARTH:
    case BOOK_UNLIFE:
    case BOOK_SPATIAL_TRANSLOCATIONS:
        return 10;

    case BOOK_TEMPESTS:
    case BOOK_DEATH:
    case BOOK_SUMMONINGS:
        return 11;

    case BOOK_BURGLARY:
    case BOOK_ALCHEMY:
    case BOOK_DREAMS:
    case BOOK_FEN:
        return 12;

    case BOOK_ENVENOMATIONS:
    case BOOK_WARP:
    case BOOK_DRAGON:
        return 15;

    case BOOK_ANNIHILATIONS:
    case BOOK_GRAND_GRIMOIRE:
    case BOOK_NECRONOMICON:  // Kikubaaqudgha special
    case BOOK_AKASHIC_RECORD:
    case BOOK_MANUAL:
        return 20;

    default:
        return 1;
    }
}

static uint8_t _lowest_rarity[NUM_SPELLS];

static const set<book_type> rare_books =
{
    BOOK_ANNIHILATIONS, BOOK_GRAND_GRIMOIRE, BOOK_NECRONOMICON,
    BOOK_AKASHIC_RECORD,
};

bool is_rare_book(book_type type)
{
    return rare_books.find(type) != rare_books.end();
}

void init_spell_rarities()
{
    for (int i = 0; i < NUM_SPELLS; ++i)
        _lowest_rarity[i] = 255;

    for (int i = 0; i < NUM_FIXED_BOOKS; ++i)
    {
        const book_type book = static_cast<book_type>(i);
        // Manuals and books of destruction are not even part of this loop.
        if (is_rare_book(book))
            continue;

#ifdef DEBUG
        spell_type last = SPELL_NO_SPELL;
#endif
        for (spell_type spell : spellbook_template(book))
        {
#ifdef DEBUG
            ASSERT(spell != SPELL_NO_SPELL);
            if (last != SPELL_NO_SPELL
                && spell_difficulty(last) > spell_difficulty(spell))
            {
                item_def item;
                item.base_type = OBJ_BOOKS;
                item.sub_type  = i;

                end(1, false, "Spellbook '%s' has spells out of level order "
                    "('%s' is before '%s')",
                    item.name(DESC_PLAIN, false, true).c_str(),
                    spell_title(last),
                    spell_title(spell));
            }
            last = spell;

            unsigned int flags = get_spell_flags(spell);

            if (flags & (SPFLAG_MONSTER | SPFLAG_TESTING))
            {
                item_def item;
                item.base_type = OBJ_BOOKS;
                item.sub_type  = i;

                end(1, false, "Spellbook '%s' contains invalid spell "
                             "'%s'",
                    item.name(DESC_PLAIN, false, true).c_str(),
                    spell_title(spell));
            }
#endif

            const int rarity = book_rarity(book);
            if (rarity < _lowest_rarity[spell])
                _lowest_rarity[spell] = rarity;
        }
    }
}

bool is_player_spell(spell_type which_spell)
{
    for (int i = 0; i < NUM_FIXED_BOOKS; ++i)
        for (spell_type spell : spellbook_template(static_cast<book_type>(i)))
            if (spell == which_spell)
                return true;
    return false;
}

int spell_rarity(spell_type which_spell)
{
    const int rarity = _lowest_rarity[which_spell];

    if (rarity == 255)
        return -1;

    return rarity;
}

void mark_had_book(const item_def &book)
{
    ASSERT(book.base_type == OBJ_BOOKS);

    if (!item_is_spellbook(book))
        return;

    for (spell_type stype : spells_in_book(book))
        you.seen_spell.set(stype);

    if (!book.props.exists(SPELL_LIST_KEY))
        mark_had_book(static_cast<book_type>(book.sub_type));
}

void mark_had_book(book_type booktype)
{
    ASSERT_RANGE(booktype, 0, MAX_FIXED_BOOK + 1);

    you.had_book.set(booktype);
}

void read_book(item_def &book)
{
    clrscr();
    describe_item(book);
    redraw_screen();
}

/**
 * Is the player ever allowed to memorise the given spell? (Based on race, not
 * spell slot restrictions, etc)
 *
 * @param spell     The type of spell in question.
 * @return          Whether the the player is allowed to memorise the spell.
 */
bool you_can_memorise(spell_type spell)
{
    return !spell_is_useless(spell, false, true);
}

bool player_can_memorise(const item_def &book)
{
    if (!item_is_spellbook(book) || !player_spell_levels())
        return false;

    for (spell_type stype : spells_in_book(book))
    {
        // Easiest spell already too difficult?
        if (spell_difficulty(stype) > effective_xl()
            || player_spell_levels() < spell_levels_required(stype))
        {
            return false;
        }

        if (!you.has_spell(stype))
            return true;
    }
    return false;
}

typedef vector<spell_type>   spell_list;
typedef map<spell_type, int> spells_to_books;

static void _index_book(item_def& book, spells_to_books &book_hash,
                        bool &book_errors)
{
    mark_had_book(book);
    set_ident_flags(book, ISFLAG_KNOW_TYPE);
    set_ident_flags(book, ISFLAG_IDENT_MASK);

    int num_spells = 0;
    for (spell_type spell : spells_in_book(book))
    {
        num_spells++;

        auto it = book_hash.find(spell);
        if (it == book_hash.end())
            book_hash[spell] = book.sub_type;
    }

    if (num_spells == 0)
    {
        mprf(MSGCH_ERROR, "Spellbook \"%s\" contains no spells! Please "
             "file a bug report.", book.name(DESC_PLAIN).c_str());
        book_errors = true;
    }
}

static bool _get_mem_list(spell_list &mem_spells,
                          spells_to_books &book_hash,
                          unsigned int &num_misc,
                          bool just_check = false,
                          spell_type current_spell = SPELL_NO_SPELL)
{
    bool          book_errors    = false;
    unsigned int  num_on_ground  = 0;
    unsigned int  num_books      = 0;
    unsigned int  num_unknown    = 0;

    FixedVector<item_def, 52> *const inv = book_inv();
    // Collect the list of all spells in all available spellbooks.
    for (auto &book : (*inv))
    {
        if (!book.defined() || !item_is_spellbook(book))
            continue;

        num_books++;
        _index_book(book, book_hash, book_errors);
    }

    // We also check the ground
    vector<const item_def*> items;
    item_list_on_square(items, you.visible_igrd(you.pos()));

    for (const item_def *bptr : items)
    {
        item_def book(*bptr); // Copy
        if (!item_is_spellbook(book))
            continue;

        if (!item_type_known(book))
        {
            num_unknown++;
            continue;
        }

        num_books++;
        num_on_ground++;
        _index_book(book, book_hash, book_errors);
    }

    // Handle Vehumet gifts
    auto gift_iterator = you.vehumet_gifts.begin();
    if (gift_iterator != you.vehumet_gifts.end())
    {
        num_books++;
        while (gift_iterator != you.vehumet_gifts.end())
            book_hash[*gift_iterator++] = NUM_BOOKS;
    }

    if (book_errors)
        more();

    if (num_books == 0)
    {
        if (!just_check)
        {
            if (num_unknown > 1)
                mprf(MSGCH_PROMPT, "You must pick up those books before reading them.");
            else if (num_unknown == 1)
                mprf(MSGCH_PROMPT, "You must pick up this book before reading it.");
            else
                mprf(MSGCH_PROMPT, "You aren't carrying or standing over any spellbooks.");
        }
        return false;
    }
    else if (book_hash.empty())
    {
        if (!just_check)
            mprf(MSGCH_PROMPT, "None of the spellbooks you are carrying contain any spells.");
        return false;
    }

    unsigned int num_known      = 0;
                 num_misc       = 0;
    unsigned int num_restricted = 0;
    unsigned int num_low_xl     = 0;
    unsigned int num_low_levels = 0;
    unsigned int num_memable    = 0;
    bool         form           = false;

    for (const auto &entry : book_hash)
    {
        const spell_type spell = entry.first;

        if (spell == current_spell || you.has_spell(spell))
            num_known++;
        else if (!you_can_memorise(spell))
        {
            if (cannot_use_schools(get_spell_disciplines(spell)))
                num_restricted++;
            else
                num_misc++;
        }
        else
        {
            mem_spells.push_back(spell);

            int avail_slots = player_spell_levels();
            if (current_spell != SPELL_NO_SPELL)
                avail_slots -= spell_levels_required(current_spell);

            if (spell_difficulty(spell) > effective_xl())
                num_low_xl++;
            else if (avail_slots < spell_levels_required(spell))
                num_low_levels++;
            else
                num_memable++;
        }
    }

    if (num_memable)
        return true;

    // Return true even if there are only spells we can't memorise _yet_.
    if (just_check)
        return num_low_levels > 0 || num_low_xl > 0;

    unsigned int total = num_known + num_misc + num_low_xl + num_low_levels
            + num_restricted;

    if (num_known == total)
        mprf(MSGCH_PROMPT, "You already know all available spells.");
    else if (num_restricted == total || num_restricted + num_known == total)
    {
        mprf(MSGCH_PROMPT, "You cannot currently memorise any of the available "
             "spells because you cannot use those schools of magic.");
    }
    else if (num_misc == total || (num_known + num_misc) == total
            || num_misc + num_known + num_restricted == total)
    {
        if (form)
        {
            mprf(MSGCH_PROMPT, "You cannot currently memorise any of the "
                 "available spells because you are in %s form.",
                 uppercase_first(transform_name()).c_str());
        }
        else
        {
            mprf(MSGCH_PROMPT, "You cannot memorise any of the available "
                 "spells.");
        }
    }
    else if (num_low_levels > 0 || num_low_xl > 0)
    {
        // Just because we can't memorise them doesn't mean we don't want to
        // see what we have available. See FR #235. {due}
        return true;
    }
    else
    {
        mprf(MSGCH_PROMPT, "You can't memorise any new spells for an unknown "
                           "reason; please file a bug report.");
    }

    return false;
}

// If current_spell is a valid spell, returns whether you'll be able to
// memorise any further spells once this one is committed to memory.
bool has_spells_to_memorise(bool silent, spell_type current_spell)
{
    spell_list      mem_spells;
    spells_to_books book_hash;
    unsigned int    num_misc;

    return _get_mem_list(mem_spells, book_hash, num_misc, silent,
                         (spell_type) current_spell);
}

static bool _sort_mem_spells(spell_type a, spell_type b)
{
    // List the Vehumet gifts at the very top.
    bool offering_a = vehumet_is_offering(a);
    bool offering_b = vehumet_is_offering(b);
    if (offering_a != offering_b)
        return offering_a;

    // List spells we can memorize right away first.
    if (player_spell_levels() >= spell_levels_required(a)
        && player_spell_levels() < spell_levels_required(b))
    {
        return true;
    }
    else if (player_spell_levels() < spell_levels_required(a)
             && player_spell_levels() >= spell_levels_required(b))
    {
        return false;
    }

    // Don't sort by failure rate beyond what the player can see in the
    // success descriptions.
    const int fail_rate_a = failure_rate_to_int(raw_spell_fail(a));
    const int fail_rate_b = failure_rate_to_int(raw_spell_fail(b));
    if (fail_rate_a != fail_rate_b)
        return fail_rate_a < fail_rate_b;

    if (spell_difficulty(a) != spell_difficulty(b))
        return spell_difficulty(a) < spell_difficulty(b);

    return strcasecmp(spell_title(a), spell_title(b)) < 0;
}

vector<spell_type> get_mem_spell_list(vector<int> &books)
{
    vector<spell_type> spells;

    spell_list      mem_spells;
    spells_to_books book_hash;
    unsigned int    num_misc;

    if (!_get_mem_list(mem_spells, book_hash, num_misc))
        return spells;

    sort(mem_spells.begin(), mem_spells.end(), _sort_mem_spells);

    for (spell_type spell : mem_spells)
    {
        spells.push_back(spell);
        books.push_back(*map_find(book_hash, spell));
    }

    return spells;
}

static spell_type _choose_mem_spell(spell_list &spells,
                                    spells_to_books &book_hash,
                                    unsigned int num_misc)
{
    sort(spells.begin(), spells.end(), _sort_mem_spells);

#ifdef USE_TILE_LOCAL
    const bool text_only = false;
#else
    const bool text_only = true;
#endif

    ToggleableMenu spell_menu(MF_SINGLESELECT | MF_ANYPRINTABLE
                    | MF_ALWAYS_SHOW_MORE | MF_ALLOW_FORMATTING,
                    text_only);
#ifdef USE_TILE_LOCAL
    // [enne] Hack. Use a separate title, so the column headers are aligned.
    spell_menu.set_title(
        new MenuEntry(" Your Spells - Memorisation  (toggle to descriptions with '!')",
            MEL_TITLE));

    spell_menu.set_title(
        new MenuEntry(" Your Spells - Descriptions  (toggle to memorisation with '!')",
            MEL_TITLE), false);

    {
        MenuEntry* me =
            new MenuEntry("     Spells                        Type          "
                          "                Failure  Level",
                MEL_ITEM);
        me->colour = BLUE;
        spell_menu.add_entry(me);
    }
#else
    spell_menu.set_title(
        new MenuEntry("     Spells (Memorisation)         Type          "
                      "                Failure  Level",
            MEL_TITLE));

    spell_menu.set_title(
        new MenuEntry("     Spells (Description)          Type          "
                      "                Failure  Level",
            MEL_TITLE), false);
#endif

    spell_menu.set_highlighter(nullptr);
    spell_menu.set_tag("spell");

    spell_menu.action_cycle = Menu::CYCLE_TOGGLE;
    spell_menu.menu_action  = Menu::ACT_EXECUTE;

    string more_str = make_stringf("<lightgreen>%d spell level%s left"
                                   "<lightgreen>",
                                   player_spell_levels(),
                                   (player_spell_levels() > 1
                                    || player_spell_levels() == 0) ? "s" : "");

    if (num_misc > 0)
    {
        more_str += make_stringf(", <lightred>%u spell%s unmemorisable"
                                 "</lightred>",
                                 num_misc,
                                 num_misc > 1 ? "s" : "");
    }

#ifndef USE_TILE_LOCAL
    // Tiles menus get this information in the title.
    more_str += "   Toggle display with '<w>!</w>'";
#endif

    spell_menu.set_more(formatted_string::parse_string(more_str));

    // Don't make a menu so tall that we recycle hotkeys on the same page.
    if (spells.size() > 52
        && (spell_menu.maxpagesize() > 52 || spell_menu.maxpagesize() == 0))
    {
        spell_menu.set_maxpagesize(52);
    }

    for (unsigned int i = 0; i < spells.size(); i++)
    {
        const spell_type spell = spells[i];

        ostringstream desc;

        int colour = LIGHTGRAY;
        if (vehumet_is_offering(spell))
            colour = LIGHTBLUE;
        // Grey out spells for which you lack experience or spell levels.
        else if (spell_difficulty(spell) > effective_xl()
                 || player_spell_levels() < spell_levels_required(spell))
            colour = DARKGRAY;
        else
            colour = spell_highlight_by_utility(spell);

        desc << "<" << colour_to_str(colour) << ">";

        desc << left;
        desc << chop_string(spell_title(spell), 30);
        desc << spell_schools_string(spell);

        int so_far = strwidth(desc.str()) - (colour_to_str(colour).length()+2);
        if (so_far < 60)
            desc << string(60 - so_far, ' ');
        desc << "</" << colour_to_str(colour) << ">";

        colour = failure_rate_colour(spell);
        desc << "<" << colour_to_str(colour) << ">";
        desc << chop_string(failure_rate_to_string(raw_spell_fail(spell)), 12);
        desc << "</" << colour_to_str(colour) << ">";
        desc << spell_difficulty(spell);

        MenuEntry* me =
            new MenuEntry(desc.str(), MEL_ITEM, 1,
                          index_to_letter(i % 52));

#ifdef USE_TILE
        me->add_tile(tile_def(tileidx_spell(spell), TEX_GUI));
#endif

        me->data = &spells[i];
        spell_menu.add_entry(me);
    }

    while (true)
    {
        vector<MenuEntry*> sel = spell_menu.show();

        if (!crawl_state.doing_prev_cmd_again)
            redraw_screen();

        if (sel.empty())
            return SPELL_NO_SPELL;

        ASSERT(sel.size() == 1);

        const spell_type spell = *static_cast<spell_type*>(sel[0]->data);
        ASSERT(is_valid_spell(spell));

        if (spell_menu.menu_action == Menu::ACT_EXAMINE)
            describe_spell(spell, nullptr);
        else
            return spell;
    }
}

bool can_learn_spell(bool silent)
{
    if (you.duration[DUR_BRAINLESS])
    {
        if (!silent)
            mpr("Your brain is not functional enough to learn spells.");
        return false;
    }

    if (you.confused())
    {
        if (!silent)
            canned_msg(MSG_TOO_CONFUSED);
        return false;
    }

    if (you.berserk())
    {
        if (!silent)
            canned_msg(MSG_TOO_BERSERK);
        return false;
    }

    return true;
}

bool learn_spell()
{
    if (!can_learn_spell())
        return false;

    spell_list      mem_spells;
    spells_to_books book_hash;

    unsigned int num_misc;

    if (!_get_mem_list(mem_spells, book_hash, num_misc))
        return false;

    spell_type specspell = _choose_mem_spell(mem_spells, book_hash, num_misc);

    if (specspell == SPELL_NO_SPELL)
    {
        canned_msg(MSG_OK);
        return false;
    }

    you.prev_direction.reset();
    return learn_spell(specspell);
}

/**
 * Why can't the player memorize the given spell?
 *
 * @param spell     The spell in question.
 * @return          A string describing (one of) the reason(s) the player
 *                  can't memorize this spell.
 */
string desc_cannot_memorise_reason(spell_type spell)
{
    return spell_uselessness_reason(spell, false, true);
}

/**
 * Can the player learn the given spell?
 *
 * @param   specspell  The spell to be learned.
 * @param   wizard     Whether to skip some checks for wizmode memorisation.
 * @return             false if the player can't learn the spell for any
 *                     reason, true otherwise.
*/
static bool _learn_spell_checks(spell_type specspell, bool wizard = false)
{
    if (!wizard && !can_learn_spell())
        return false;

    if (already_learning_spell((int) specspell))
        return false;

    if (!you_can_memorise(specspell))
    {
        mpr(desc_cannot_memorise_reason(specspell));
        return false;
    }

    if (you.has_spell(specspell))
    {
        mpr("You already know that spell!");
        return false;
    }

    if (you.spell_no >= MAX_KNOWN_SPELLS)
    {
        mpr("Your head is already too full of spells!");
        return false;
    }

    if (effective_xl() < spell_difficulty(specspell) && !wizard)
    {
        mpr("You're too inexperienced to learn that spell!");
        return false;
    }

    if (player_spell_levels() < spell_levels_required(specspell) && !wizard)
    {
        mpr("You can't memorise that many levels of magic yet!");
        return false;
    }

    return true;
}

/**
 * Attempt to make the player learn the given spell.
 *
 * @param   specspell  The spell to be learned.
 * @param   wizard     Whether to memorise instantly and skip some checks for
 *                     wizmode memorisation.
 * @return             true if the player learned the spell, false
 *                     otherwise.
*/
bool learn_spell(spell_type specspell, bool wizard)
{
    if (!_learn_spell_checks(specspell, wizard))
        return false;

    if (!wizard)
    {
        int severity = fail_severity(specspell);

        if (raw_spell_fail(specspell) >= 100 && !vehumet_is_offering(specspell))
            mprf(MSGCH_WARN, "This spell is impossible to cast!");
        else if (severity > 0)
        {
            mprf(MSGCH_WARN, "This spell is %s to cast%s",
                             fail_severity_adjs[severity],
                             severity > 1 ? "!" : ".");
        }
    }

    const string prompt = make_stringf(
             "Memorise %s, consuming %d spell level%s and leaving %d?",
             spell_title(specspell), spell_levels_required(specspell),
             spell_levels_required(specspell) != 1 ? "s" : "",
             player_spell_levels() - spell_levels_required(specspell));

    // Deactivate choice from tile inventory.
    mouse_control mc(MOUSE_MODE_MORE);
    if (!yesno(prompt.c_str(), true, 'n', false))
    {
        canned_msg(MSG_OK);
        return false;
    }

    if (wizard)
        add_spell_to_memory(specspell);
    else
    {
        start_delay(DELAY_MEMORISE, spell_difficulty(specspell), specspell);
        you.turn_is_over = true;

        did_god_conduct(DID_SPELL_CASTING, 2 + random2(5));
    }

    return true;
}

bool forget_spell_from_book(spell_type spell, const item_def* book)
{
    string prompt;

    prompt += make_stringf("Forgetting %s from %s will destroy the book%s! "
                           "Are you sure?",
                           spell_title(spell),
                           book->name(DESC_THE).c_str(),
                           you_worship(GOD_SIF_MUNA)
                               ? " and put you under penance" : "");

    // Deactivate choice from tile inventory.
    mouse_control mc(MOUSE_MODE_MORE);
    if (!yesno(prompt.c_str(), false, 'n'))
    {
        canned_msg(MSG_OK);
        return false;
    }
    mprf("As you tear out the page describing %s, the book crumbles to dust.",
        spell_title(spell));

    if (del_spell_from_memory(spell))
    {
        item_was_destroyed(*book);
        destroy_spellbook(*book);
        FixedVector<item_def, 52> *const inv = book_inv();
        dec_inv_item_quantity((*inv), book->link, 1);
        you.turn_is_over = true;
        return true;
    }
    else
    {
        // This shouldn't happen.
        mprf("A bug prevents you from forgetting %s.", spell_title(spell));
        return false;
    }
}

bool book_has_title(const item_def &book)
{
    ASSERT(book.base_type == OBJ_BOOKS);

    if (!is_artefact(book))
        return false;

    return book.props.exists(BOOK_TITLED_KEY)
           && book.props[BOOK_TITLED_KEY].get_bool() == true;
}

void destroy_spellbook(const item_def &book)
{
    int maxlevel = 0;
    for (spell_type stype : spells_in_book(book))
        maxlevel = max(maxlevel, spell_difficulty(stype));

    did_god_conduct(DID_DESTROY_SPELLBOOK, maxlevel + 5);
}
