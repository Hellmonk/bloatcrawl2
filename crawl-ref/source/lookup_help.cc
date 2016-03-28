/**
 * @file
 * @brief Let the player search for descriptions of monsters, items, etc.
 **/

#include "AppHdr.h"

#include "lookup_help.h"

#include <functional>

#include "ability.h"
#include "branch.h"
#include "cio.h"
#include "colour.h"
#include "cloud.h"
#include "database.h"
#include "decks.h"
#include "describe.h"
#include "describe-god.h"
#include "describe-spells.h"
#include "directn.h"
#include "english.h"
#include "enum.h"
#include "env.h"
#include "godmenu.h"
#include "itemprop.h"
#include "itemname.h"
#include "items.h"
#include "libutil.h" // map_find
#include "macro.h"
#include "makeitem.h" // item_colour
#include "menu.h"
#include "message.h"
#include "mon-info.h"
#include "mon-tentacle.h"
#include "output.h"
#include "prompt.h"
#include "religion.h"
#include "skills.h"
#include "stringutil.h"
#include "spl-book.h"
#include "spl-util.h"
#include "terrain.h"
#ifdef USE_TILE
#include "tilepick.h"
#include "tileview.h"
#endif
#include "view.h"
#include "viewchar.h"


typedef vector<string> (*keys_by_glyph)(ucs_t showchar);
typedef vector<string> (*simple_key_list)();
typedef void (*db_keys_recap)(vector<string>&);
typedef MenuEntry* (*menu_entry_generator)(char letter, const string &str,
                                           string &key);
typedef function<int (const string &, const string &, string)> key_describer;

/// A set of optional functionality for lookup types.
enum class lookup_type
{
    NONE            = 0,
    /// append the 'type' to the db lookup (e.g. "<input> spell")
    DB_SUFFIX       = 1<<0,
    /// whether the sorting functionality should be turned off
    DISABLE_SORT    = 1<<1,
    /// whether the display menu for this supports tiles
    SUPPORT_TILES   = 1<<2,
    /// whether the display menu for this has toggleable sorting
    TOGGLEABLE_SORT = 1<<3,
};
DEF_BITFIELD(lookup_type_flags, lookup_type);

/// A description of a lookup that the player can do. (e.g. (M)onster data)
class LookupType
{
public:
    LookupType(char _symbol, string _type, db_keys_recap _recap,
               db_find_filter _filter_forbid, keys_by_glyph _glyph_fetch,
               simple_key_list _simple_key_fetch,
               menu_entry_generator _menu_gen, key_describer _describer,
               lookup_type_flags _flags)
    : symbol(_symbol), type(_type), filter_forbid(_filter_forbid),
      flags(_flags),
      simple_key_fetch(_simple_key_fetch), glyph_fetch(_glyph_fetch),
      recap(_recap), menu_gen(_menu_gen), describer(_describer)
    {
        // XXX: will crash at startup; compile-time would be better
        // also, ugh
        ASSERT(menu_gen != nullptr || type == "monster");
        ASSERT(describer != nullptr);
    }

    string prompt_string() const;
    string suffix() const;
    vector<string> matching_keys(string regex) const;
    void display_keys(vector<string> &key_list) const;

    /**
     * Does this lookup type have special support for single-character input
     * (looking up corresponding glyphs - e.g. 'o' for orc, '(' for ammo...)
     */
    bool supports_glyph_lookup() const { return glyph_fetch != nullptr; }

    /**
     * Does this lookup type return a list of keys without taking a search
     * request (e.g. branches or gods)?
     */
    bool no_search() const { return simple_key_fetch != nullptr; }

    int describe(const string &key, bool exact_match = false) const;

public:
    /// The letter pressed to choose this (e.g. 'M'). case insensitive
    char symbol;
    /// A description of the lookup type (e.g. "monster"). case insensitive
    string type;
    /// a function returning 'true' if the search result corresponding to
    /// the corresponding search should be filtered out of the results
    db_find_filter filter_forbid;
    /// A set of optional functionality; see lookup_type for details
    lookup_type_flags flags;
private:
    MenuEntry* make_menu_entry(char letter, string &key) const;
    string key_to_menu_str(const string &key) const;

    /**
     * Does this lookup type support toggling the sort order of results?
     */
    bool toggleable_sort() const
    {
        return bool(flags & lookup_type::TOGGLEABLE_SORT);
    }

private:
    /// Function that fetches a list of keys, without taking arguments.
    simple_key_list simple_key_fetch;
    /// a function taking a single character & returning a list of keys
    /// corresponding to that glyph
    keys_by_glyph glyph_fetch;
    /// no idea, sorry :(
    db_keys_recap recap;
    /// take a letter & a key, return a corresponding new menu entry
    menu_entry_generator menu_gen;
    /// A function to handle describing & interacting with a given key.
    key_describer describer;
};





static bool _compare_mon_names(MenuEntry *entry_a, MenuEntry* entry_b)
{
    monster_info* a = static_cast<monster_info* >(entry_a->data);
    monster_info* b = static_cast<monster_info* >(entry_b->data);

    if (a->type == b->type)
        return false;

    string a_name = mons_type_name(a->type, DESC_PLAIN);
    string b_name = mons_type_name(b->type, DESC_PLAIN);
    return lowercase(a_name) < lowercase(b_name);
}

// Compare monsters by location-independent level, or by hitdice if
// levels are equal, or by name if both level and hitdice are equal.
static bool _compare_mon_toughness(MenuEntry *entry_a, MenuEntry* entry_b)
{
    monster_info* a = static_cast<monster_info* >(entry_a->data);
    monster_info* b = static_cast<monster_info* >(entry_b->data);

    if (a->type == b->type)
        return false;

    int a_toughness = mons_avg_hp(a->type);
    int b_toughness = mons_avg_hp(b->type);

    if (a_toughness == b_toughness)
    {
        string a_name = mons_type_name(a->type, DESC_PLAIN);
        string b_name = mons_type_name(b->type, DESC_PLAIN);
        return lowercase(a_name) < lowercase(b_name);
    }
    return a_toughness > b_toughness;
}

class DescMenu : public Menu
{
public:
    DescMenu(int _flags, bool _toggleable_sort, bool _text_only)
    : Menu(_flags, "", _text_only), sort_alpha(true),
    toggleable_sort(_toggleable_sort)
    {
        set_highlighter(nullptr);

        if (_toggleable_sort)
            toggle_sorting();

        set_prompt();
    }

    bool sort_alpha;
    bool toggleable_sort;

    void set_prompt()
    {
        string prompt = "Describe which? ";

        if (toggleable_sort)
        {
            if (sort_alpha)
                prompt += "(CTRL-S to sort by monster toughness)";
            else
                prompt += "(CTRL-S to sort by name)";
        }
        set_title(new MenuEntry(prompt, MEL_TITLE));
    }

    void sort()
    {
        if (!toggleable_sort)
            return;

        if (sort_alpha)
            ::sort(items.begin(), items.end(), _compare_mon_names);
        else
            ::sort(items.begin(), items.end(), _compare_mon_toughness);

        for (unsigned int i = 0, size = items.size(); i < size; i++)
        {
            const char letter = index_to_letter(i);

            items[i]->hotkeys.clear();
            items[i]->add_hotkey(letter);
        }
    }

    void toggle_sorting()
    {
        if (!toggleable_sort)
            return;

        sort_alpha = !sort_alpha;

        sort();
        set_prompt();
    }
};

static vector<string> _get_desc_keys(string regex, db_find_filter filter)
{
    vector<string> key_matches = getLongDescKeysByRegex(regex, filter);

    if (key_matches.size() == 1)
        return key_matches;
    else if (key_matches.size() > 52)
        return key_matches;

    vector<string> body_matches = getLongDescBodiesByRegex(regex, filter);

    if (key_matches.empty() && body_matches.empty())
        return key_matches;
    else if (key_matches.empty() && body_matches.size() == 1)
        return body_matches;

    // Merge key_matches and body_matches, discarding duplicates.
    vector<string> tmp = key_matches;
    tmp.insert(tmp.end(), body_matches.begin(), body_matches.end());
    sort(tmp.begin(), tmp.end());
    vector<string> all_matches;
    for (unsigned int i = 0, size = tmp.size(); i < size; i++)
        if (i == 0 || all_matches[all_matches.size() - 1] != tmp[i])
            all_matches.push_back(tmp[i]);

    return all_matches;
}

static vector<string> _get_monster_keys(ucs_t showchar)
{
    vector<string> mon_keys;

    for (monster_type i = MONS_0; i < NUM_MONSTERS; ++i)
    {
        if (i == MONS_PROGRAM_BUG)
            continue;

        const monsterentry *me = get_monster_data(i);

        if (me == nullptr || me->name == nullptr || me->name[0] == '\0')
            continue;

        if (me->mc != i)
            continue;

        if (getLongDescription(me->name).empty())
            continue;

        if ((ucs_t)me->basechar == showchar)
            mon_keys.push_back(me->name);
    }

    return mon_keys;
}


static vector<string> _get_god_keys()
{
    vector<string> names;

    for (int i = GOD_NO_GOD + 1; i < NUM_GODS; i++)
    {
        god_type which_god = static_cast<god_type>(i);
        names.push_back(god_name(which_god));
    }

    return names;
}

static vector<string> _get_branch_keys()
{
    vector<string> names;

    for (branch_iterator it; it; ++it)
    {
        // Skip unimplemented branches
        if (branch_is_unfinished(it->id))
            continue;

        names.push_back(it->shortname);
    }
    return names;
}

static vector<string> _get_cloud_keys()
{
    vector<string> names;

    for (int i = CLOUD_NONE + 1; i < NUM_CLOUD_TYPES; i++)
        names.push_back(cloud_type_name((cloud_type) i) + " cloud");

    return names;
}

/**
 * Return a list of all skill names.
 */
static vector<string> _get_skill_keys()
{
    vector<string> names;
    for (skill_type sk = SK_FIRST_SKILL; sk < NUM_SKILLS; ++sk)
    {
        const string name = lowercase_string(skill_name(sk));
#if TAG_MAJOR_VERSION == 34
        if (getLongDescription(name).empty())
            continue; // obsolete skills
#endif

        names.emplace_back(name);
    }
    return names;
}

static bool _monster_filter(string key, string body)
{
    monster_type mon_num = get_monster_by_name(key);
    return mons_class_flag(mon_num, M_CANT_SPAWN)
           || mons_is_tentacle_segment(mon_num);
}

static bool _spell_filter(string key, string body)
{
    if (!strip_suffix(key, "spell"))
        return true;

    spell_type spell = spell_by_name(key);

    if (spell == SPELL_NO_SPELL)
        return true;

    if (get_spell_flags(spell) & SPFLAG_TESTING)
        return !you.wizard;

    return false;
}

static bool _item_filter(string key, string body)
{
    return item_kind_by_name(key).base_type == OBJ_UNASSIGNED;
}

static bool _feature_filter(string key, string body)
{
    return feat_by_desc(key) == DNGN_UNSEEN;
}

static bool _card_filter(string key, string body)
{
    lowercase(key);

    // Every card description contains the keyword "card".
    if (!strip_suffix(key, "card"))
        return true;

    for (int i = 0; i < NUM_CARDS; ++i)
    {
        if (key == lowercase_string(card_name(static_cast<card_type>(i))))
            return false;
    }
    return true;
}

static bool _ability_filter(string key, string body)
{
    lowercase(key);

    if (!strip_suffix(key, "ability"))
        return true;

    return !string_matches_ability_name(key);
}

static bool _status_filter(string key, string body)
{
    return !strip_suffix(lowercase(key), " status");
}


static void _recap_mon_keys(vector<string> &keys)
{
    for (unsigned int i = 0, size = keys.size(); i < size; i++)
    {
        monster_type type = get_monster_by_name(keys[i]);
        keys[i] = mons_type_name(type, DESC_PLAIN);
    }
}

/**
 * Fixup spell names. (Correcting capitalization, mainly.)
 *
 * @param[in,out] keys      A lowercased list of spell names.
 */
static void _recap_spell_keys(vector<string> &keys)
{
    for (unsigned int i = 0, size = keys.size(); i < size; i++)
    {
        // first, strip " spell"
        const string key_name = keys[i].substr(0, keys[i].length() - 6);
        // then get the real name
        keys[i] = make_stringf("%s spell",
                               spell_title(spell_by_name(key_name)));
    }
}

/**
 * Fixup ability names. (Correcting capitalization, mainly.)
 *
 * @param[in,out] keys      A lowercased list of ability names.
 */
static void _recap_ability_keys(vector<string> &keys)
{
    for (auto &key : keys)
    {
        strip_suffix(key, "ability");
        // get the real name
        key = make_stringf("%s ability", ability_name(ability_by_name(key)));
    }
}

static void _recap_feat_keys(vector<string> &keys)
{
    for (unsigned int i = 0, size = keys.size(); i < size; i++)
    {
        dungeon_feature_type type = feat_by_desc(keys[i]);
        if (type == DNGN_ENTER_SHOP)
            keys[i] = "A shop";
        else
        {
            keys[i] = feature_description(type, NUM_TRAPS, "", DESC_A,
                                          false);
        }
    }
}

static void _recap_card_keys(vector<string> &keys)
{
    for (unsigned int i = 0, size = keys.size(); i < size; i++)
    {
        lowercase(keys[i]);

        for (int j = 0; j < NUM_CARDS; ++j)
        {
            card_type card = static_cast<card_type>(j);
            if (keys[i] == lowercase_string(card_name(card)) + " card")
            {
                keys[i] = string(card_name(card)) + " card";
                break;
            }
        }
    }
}

static bool _is_rod_spell(spell_type spell)
{
    if (spell == SPELL_NO_SPELL)
        return false;

    for (int i = 0; i < NUM_RODS; i++)
        if (spell_in_rod(static_cast<rod_type>(i)) == spell)
            return true;

    return false;
}

/**
 * Make a list of all books/rods that contain a given spell.
 *
 * @param spell_type spell      The spell in question.
 * @return                      A formatted list of books & rods containing
 *                              the spell, e.g.:
 *    \n\nThis spell can be found in the following rod: iron rod.
 *    or
 *    \n\nThis spell can be found in the following books: dreams, burglary.
 *    \n\nThis spell can be found in the following rods: warding, clouds.
 *    or
 *    \n\nThis spell is not found in any books or rods.
 */
static string _spell_sources(const spell_type spell)
{
    item_def item;
    set_ident_flags(item, ISFLAG_IDENT_MASK);
    vector<string> books;
    vector<string> rods;

    item.base_type = OBJ_BOOKS;
    for (int i = 0; i < NUM_FIXED_BOOKS; i++)
        for (spell_type sp : spellbook_template(static_cast<book_type>(i)))
            if (sp == spell)
            {
                item.sub_type = i;
                books.push_back(item.name(DESC_PLAIN));
            }

    item.base_type = OBJ_RODS;
    for (int i = 0; i < NUM_RODS; i++)
    {
        item.sub_type = i;
        if (spell_in_rod(static_cast<rod_type>(i)) == spell)
            rods.push_back(item.name(DESC_BASENAME));
    }

    if (books.empty() && rods.empty())
        return "\n\nThis spell is not found in any books or rods.";

    string desc;

    if (!books.empty())
    {
        desc += "\n\nThis spell can be found in the following book";
        if (books.size() > 1)
            desc += "s";
        desc += ":\n ";
        desc += comma_separated_line(books.begin(), books.end(), "\n ", "\n ");
    }

    if (!rods.empty())
    {
        desc += "\n\nThis spell can be found in the following rod";
        if (rods.size() > 1)
            desc += "s";
        desc += ":\n ";
        desc += comma_separated_line(rods.begin(), rods.end(), "\n ", "\n ");
    }

    return desc;
}

/**
 * Make a basic, no-frills ?/<foo> menu entry.
 *
 * @param letter    The letter for the entry. (E.g. 'e' for the fifth entry.)
 * @param str       A processed string for the entry. (E.g. "Blade".)
 * @param key       The raw database key for the entry. (E.g. "blade card".)
 * @return          A new menu entry.
 */
static MenuEntry* _simple_menu_gen(char letter, const string &str, string &key)
{
    MenuEntry* me = new MenuEntry(str, MEL_ITEM, 1, letter);
    me->data = &key;
    return me;
}

/**
 * Generate a ?/M entry.
 *
 * @param letter      The letter for the entry. (E.g. 'e' for the fifth entry.)
 * @param str         A processed string for the entry. (E.g. "Blade".)
 * @param mslot[out]  A space in memory to store a fake monster.
 * @return            A new menu entry.
 */
static MenuEntry* _monster_menu_gen(char letter, const string &str,
                             monster_info &mslot)
{
    // Create and store fake monsters, so the menu code will
    // have something valid to refer to.
    monster_type m_type = get_monster_by_name(str);

    monster_type base_type = MONS_NO_MONSTER;
    // HACK: Set an arbitrary humanoid monster as base type.
    if (mons_class_is_zombified(m_type))
        base_type = MONS_GOBLIN;
    // FIXME: This doesn't generate proper draconian monsters.
    monster_info fake_mon(m_type, base_type);
    fake_mon.props["fake"] = true;

    mslot = fake_mon;

#ifndef USE_TILE_LOCAL
    int colour = mons_class_colour(m_type);
    if (colour == BLACK)
        colour = LIGHTGREY;

    string prefix = "(<";
    prefix += colour_to_str(colour);
    prefix += ">";
    prefix += stringize_glyph(mons_char(m_type));
    prefix += "</";
    prefix += colour_to_str(colour);
    prefix += ">) ";

    const string title = prefix + str;
#else
    const string &title = str;
#endif

    // NOTE: MonsterMenuEntry::get_tiles() takes care of setting
    // up a fake weapon when displaying a fake dancing weapon's
    // tile.
    return new MonsterMenuEntry(title, &mslot, letter);
}

/**
 * Generate a ?/F menu entry. (ref. _simple_menu_gen()).
 */
static MenuEntry* _feature_menu_gen(char letter, const string &str, string &key)
{
    const dungeon_feature_type feat = feat_by_desc(str);
    MenuEntry* me = new FeatureMenuEntry(str, feat, letter);
    me->data = &key;

#ifdef USE_TILE
    if (feat)
    {
        const tileidx_t idx = tileidx_feature_base(feat);
        me->add_tile(tile_def(pick_dngn_tile(idx, ui_random(INT_MAX)),
                                             get_dngn_tex(idx)));
    }
#endif

    return me;
}

/**
 * Generate a ?/G menu entry. (ref. _simple_menu_gen()).
 */
static MenuEntry* _god_menu_gen(char letter, const string &str, string &key)
{
    return new GodMenuEntry(str_to_god(key));
}

/**
 * Generate a ?/A menu entry. (ref. _simple_menu_gen()).
 */
static MenuEntry* _ability_menu_gen(char letter, const string &str, string &key)
{
    MenuEntry* me = _simple_menu_gen(letter, str, key);

#ifdef USE_TILE
    const ability_type ability = ability_by_name(str);
    if (ability != ABIL_NON_ABILITY)
        me->add_tile(tile_def(tileidx_ability(ability), TEX_GUI));
#endif

    return me;
}

/**
 * Generate a ?/S menu entry. (ref. _simple_menu_gen()).
 */
static MenuEntry* _spell_menu_gen(char letter, const string &str, string &key)
{
    MenuEntry* me = _simple_menu_gen(letter, str, key);

    const spell_type spell = spell_by_name(str);
#ifdef USE_TILE
    if (spell != SPELL_NO_SPELL)
        me->add_tile(tile_def(tileidx_spell(spell), TEX_GUI));
#endif
    me->colour = is_player_spell(spell) ? WHITE
               : _is_rod_spell(spell)   ? LIGHTGREY
                                        : DARKGREY; // monster-only

    return me;
}

/**
 * Generate a ?/K menu entry. (ref. _simple_menu_gen()).
 */
static MenuEntry* _skill_menu_gen(char letter, const string &str, string &key)
{
    MenuEntry* me = _simple_menu_gen(letter, str, key);

#ifdef USE_TILE
    const skill_type skill = str_to_skill(str);
    me->add_tile(tile_def(tileidx_skill(skill, 1), TEX_GUI));
#endif

    return me;
}

/**
 * Generate a ?/L menu entry. (ref. _simple_menu_gen()).
 */
static MenuEntry* _cloud_menu_gen(char letter, const string &str, string &key)
{
    MenuEntry* me = _simple_menu_gen(letter, str, key);

    const string cloud_name = lowercase_string(str);
    const cloud_type cloud = cloud_name_to_type(cloud_name);
    ASSERT(cloud != NUM_CLOUD_TYPES);

    cloud_struct fake_cloud;
    fake_cloud.type = cloud;
    fake_cloud.decay = 1000;
    me->colour = element_colour(get_cloud_colour(fake_cloud));

#ifdef USE_TILE
    cloud_info fake_cloud_info;
    fake_cloud_info.type = cloud;
    fake_cloud_info.colour = me->colour;
    const tileidx_t idx = tileidx_cloud(fake_cloud_info) & ~TILE_FLAG_FLYING;
    me->add_tile(tile_def(idx, TEX_DEFAULT));
#endif

    return me;
}


/**
 * How should this type be expressed in the prompt string?
 *
 * @return The 'type', with the first instance of the 'symbol' found &
 *          replaced with an uppercase version surrounded by parens
 *          e.g. "monster", 'm' -> "(M)onster"
 */
string LookupType::prompt_string() const
{
    string prompt_str = lowercase_string(type);
    const size_t symbol_pos = prompt_str.find(tolower(symbol));
    ASSERT(symbol_pos != string::npos);

    prompt_str.replace(symbol_pos, 1, make_stringf("(%c)", toupper(symbol)));
    return prompt_str;
}

/**
 * A suffix to be appended to the provided search string when looking for
 * db info.
 *
 * @return      An appropriate suffix for types that need them (e.g.
 *              " cards"); otherwise "".
 */
string LookupType::suffix() const
{
    if (flags & lookup_type::DB_SUFFIX)
        return " " + type;
    return "";
}

/**
 * Get a list of string corresponding to the given regex.
 */
vector<string> LookupType::matching_keys(string regex) const
{
    vector<string> key_list;

    if (no_search())
        key_list = simple_key_fetch();
    else if (regex.size() == 1 && supports_glyph_lookup())
        key_list = glyph_fetch(regex[0]);
    else
        key_list = _get_desc_keys(regex, filter_forbid);

    if (recap != nullptr)
        (*recap)(key_list);

    return key_list;
}

/**
 * Build a menu listing the given keys, and allow the player to interact
 * with them.
 */
void LookupType::display_keys(vector<string> &key_list) const
{
    // For tiles builds use a tiles menu to display monsters.
    const bool text_only =
#ifdef USE_TILE_LOCAL
    !(flags & lookup_type::SUPPORT_TILES);
#else
    true;
#endif

    DescMenu desc_menu(MF_SINGLESELECT | MF_ANYPRINTABLE | MF_ALLOW_FORMATTING,
                       toggleable_sort(), text_only);
    desc_menu.set_tag("description");

    // XXX: ugh
    const bool doing_mons = type == "monster";
    monster_info monster_list[52];
    for (unsigned int i = 0, size = key_list.size(); i < size; i++)
    {
        const char letter = index_to_letter(i);
        string &key = key_list[i];
        // XXX: double ugh
        if (doing_mons)
        {
            desc_menu.add_entry(_monster_menu_gen(letter,
                                                  key_to_menu_str(key),
                                                  monster_list[i]));
        } else
            desc_menu.add_entry(make_menu_entry(letter, key));
    }

    desc_menu.sort();

    while (true)
    {
        vector<MenuEntry*> sel = desc_menu.show();
        redraw_screen();
        if (sel.empty())
        {
            if (toggleable_sort() && desc_menu.getkey() == CONTROL('S'))
                desc_menu.toggle_sorting();
            else
                return; // only exit from this function
        }
        else
        {
            ASSERT(sel.size() == 1);
            ASSERT(sel[0]->hotkeys.size() >= 1);

            string key;

            if (doing_mons)
            {
                monster_info* mon = (monster_info*) sel[0]->data;
                key = mons_type_name(mon->type, DESC_PLAIN);
            }
            else
                key = *((string*) sel[0]->data);

            describe(key);
        }
    }
}

/**
 * Generate a description menu entry for the given key.
 *
 * @param letter    The letter with which the entry should be labeled.
 * @param key       The key for the entry.
 * @return          A pointer to a new MenuEntry object.
 */
MenuEntry* LookupType::make_menu_entry(char letter, string &key) const
{
    ASSERT(menu_gen);
    return menu_gen(letter, key_to_menu_str(key), key);
}

/**
 * Turn a DB string into a nice menu title.
 *
 * @param key       The key in question. (E.g. "blade card").
 * @return          A nicer string. (E.g. "Blade").
 */
string LookupType::key_to_menu_str(const string &key) const
{
    string str = uppercase_first(key);
    // perhaps we should assert this?
    strip_suffix(str, suffix());
    return str;
}

/**
 * Handle describing & interacting with a given key.
 * @return the last key pressed.
 */
int LookupType::describe(const string &key, bool exact_match) const
{
    const string footer
        = exact_match ? "This entry is an exact match for '" + key
        + "'. To see non-exact matches, press space."
        : "";
    return describer(key, suffix(), footer);
}

/**
 * Describe the thing with the given name.
 *
 * @param key           The name of the thing in question.
 * @param suffix        A suffix to trim from the key when making the title.
 * @param footer        A footer to append to the end of descriptions.
 * @param extra_info    Extra info to append to the database description.
 * @return              The keypress the user made to exit.
 */
static int _describe_key(const string &key, const string &suffix,
                         string footer, const string &extra_info)
{
    describe_info inf;
    inf.quote = getQuoteString(key);

    const string desc = getLongDescription(key);
    const int width = min(80, get_number_of_cols());

    inf.body << desc << extra_info;

    string title = key;
    strip_suffix(title, suffix);
    title = uppercase_first(title);
    linebreak_string(footer, width - 1);

    inf.footer = footer;
    inf.title  = title;

#ifdef USE_TILE_WEB
    tiles_crt_control show_as_menu(CRT_MENU, "description");
#endif

    print_description(inf);
    return getchm();
}

/**
 * Describe the thing with the given name.
 *
 * @param key       The name of the thing in question.
 * @param suffix    A suffix to trim from the key when making the title.
 * @param footer    A footer to append to the end of descriptions.
 * @return          The keypress the user made to exit.
 */
static int _describe_generic(const string &key, const string &suffix,
                             string footer)
{
    return _describe_key(key, suffix, footer, "");
}

/**
 * Describe & allow examination of the monster with the given name.
 *
 * @param key       The name of the monster in question.
 * @param suffix    A suffix to trim from the key when making the title.
 * @param footer    A footer to append to the end of descriptions.
 * @return          The keypress the user made to exit.
 */
static int _describe_monster(const string &key, const string &suffix,
                             string footer)
{
    const monster_type mon_num = get_monster_by_name(key);
    ASSERT(mon_num != MONS_PROGRAM_BUG);
    // Don't attempt to get more information on ghost demon
    // monsters, as the ghost struct has not been initialised, which
    // will cause a crash. Similarly for zombified monsters, since
    // they require a base monster.
    if (mons_is_ghost_demon(mon_num) || mons_class_is_zombified(mon_num))
        return _describe_generic(key, suffix, footer);

    monster_info mi(mon_num);
    // Avoid slime creature being described as "buggy"
    if (mi.type == MONS_SLIME_CREATURE)
        mi.slime_size = 1;
    return describe_monsters(mi, true, footer);
}


/**
 * Describe the spell with the given name.
 *
 * @param key       The name of the spell in question.
 * @param suffix    A suffix to trim from the key when making the title.
 * @param footer    A footer to append to the end of descriptions.
 * @return          The keypress the user made to exit.
 */
static int _describe_spell(const string &key, const string &suffix,
                             string footer)
{
    const string spell_name = key.substr(0, key.size() - suffix.size());
    const spell_type spell = spell_by_name(spell_name, true);
    ASSERT(spell != SPELL_NO_SPELL);

    const string spell_info = player_spell_desc(spell);
    const string source_info = _spell_sources(spell);
    return _describe_key(key, suffix, footer, spell_info + source_info);
}


/**
 * Describe the card with the given name.
 *
 * @param key       The name of the card in question.
 * @param suffix    A suffix to trim from the key when making the title.
 * @param footer    A footer to append to the end of descriptions.
 * @return          The keypress the user made to exit.
 */
static int _describe_card(const string &key, const string &suffix,
                           string footer)
{
    const string card_name = key.substr(0, key.size() - suffix.size());
    const card_type card = name_to_card(card_name);
    ASSERT(card != NUM_CARDS);
    return _describe_key(key, suffix, footer, which_decks(card) + "\n");
}

/**
 * Describe the cloud with the given name.
 *
 * @param key       The name of the cloud in question.
 * @param suffix    A suffix to trim from the key when making the title.
 * @param footer    A footer to append to the end of descriptions.
 * @return          The keypress the user made to exit.
 */
static int _describe_cloud(const string &key, const string &suffix,
                          string footer)
{
    const string cloud_name = key.substr(0, key.size() - suffix.size());
    const cloud_type cloud = cloud_name_to_type(cloud_name);
    ASSERT(cloud != NUM_CLOUD_TYPES);

    const string extra_info = is_opaque_cloud(cloud) ?
        "\nThis cloud is opaque; one tile will not block vision, but "
        "multiple will."
        : "";
    return _describe_key(key, suffix, footer, extra_info);
}


/**
 * Describe the given spell-holding item. (Book or rod)
 *
 * @param item      The item in question.
 * @return          0.
 *                  TODO: change to the last keypress (to allow exact match
 *                  support)
 */
static int _describe_spell_item(const item_def &item)
{
    const string desc = get_item_description(item, true);
    formatted_string fdesc;
    fdesc.cprintf("%s", desc.c_str());

    list_spellset(item_spellset(item), nullptr, &item, fdesc);
    return 0; // XXX: this breaks exact match stuff
}


/**
 * Describe the item with the given name.
 *
 * @param key       The name of the item in question.
 * @param suffix    A suffix to trim from the key when making the title.
 * @param footer    A footer to append to the end of descriptions.
 * @return          The keypress the user made to exit.
 */
static int _describe_item(const string &key, const string &suffix,
                           string footer)
{
    item_def item;
    string stats;
    if (get_item_by_name(&item, key.c_str(), OBJ_WEAPONS)
        || get_item_by_name(&item, key.c_str(), OBJ_ARMOUR)
        || get_item_by_name(&item, key.c_str(), OBJ_MISSILES)
        || get_item_by_name(&item, key.c_str(), OBJ_MISCELLANY))
    {
        // don't request description since _describe_key handles that
        stats = get_item_description(item, true, false, true);
    }
    // spellbooks/rods are interactive & so require special handling
    else if (get_item_by_name(&item, key.c_str(), OBJ_BOOKS)
        || get_item_by_name(&item, key.c_str(), OBJ_RODS))
    {
        item_colour(item);
        return _describe_spell_item(item);
    }

    return _describe_key(key, suffix, footer, stats);
}

/**
 * Describe the god with the given name.
 *
 * @param key       The name of the god in question.
 * @return          0.
 */
static int _describe_god(const string &key, const string &/*suffix*/,
                          string /*footer*/)
{
    const god_type which_god = str_to_god(key);
    ASSERT(which_god != GOD_NO_GOD);

#ifdef USE_TILE_WEB
    tiles_crt_control show_as_menu(CRT_MENU, "describe_god");
#endif
    describe_god(which_god, true);

    return 0; // no exact matches for gods, so output doesn't matter
}

static string _branch_entry_runes(branch_type br)
{
    string desc;
    const int num_runes = runes_for_branch(br);

    if (num_runes > 0)
    {
        desc = make_stringf("\n\nThis %s can only be entered while carrying "
                            "at least %d rune%s of Zot.",
                            br == BRANCH_ZIGGURAT ? "portal" : "branch",
                            num_runes, num_runes > 1 ? "s" : "");
    }

    return desc;
}

static string _branch_depth(branch_type br)
{
    string desc;
    const int depth = branches[br].numlevels;

    // Abyss depth is explained in the description.
    if (depth > 1 && br != BRANCH_ABYSS)
    {
        desc = make_stringf("\n\nThis %s is %d levels deep.",
                            br == BRANCH_ZIGGURAT ? "portal"
                                                  : "branch",
                            depth);
    }

    return desc;
}

static string _branch_location(branch_type br)
{
    string desc;
    const branch_type parent = branches[br].parent_branch;
    const int min = branches[br].mindepth;
    const int max = branches[br].maxdepth;

    // Ziggurat locations are explained in the description.
    if (parent != NUM_BRANCHES && br != BRANCH_ZIGGURAT)
    {
        desc = "\n\nThe entrance to this branch can be found ";
        if (min == max)
        {
            if (branches[parent].numlevels == 1)
                desc += "in ";
            else
                desc += make_stringf("on level %d of ", min);
        }
        else
            desc += make_stringf("between levels %d and %d of ", min, max);
        desc += branches[parent].longname;
        desc += ".";
    }

    return desc;
}

static string _branch_subbranches(branch_type br)
{
    string desc;
    vector<string> subbranch_names;

    for (branch_iterator it; it; ++it)
        if (it->parent_branch == br && !branch_is_unfinished(it->id))
            subbranch_names.push_back(it->longname);

    // Lair's random branches are explained in the description.
    if (!subbranch_names.empty() && br != BRANCH_LAIR)
    {
        desc += make_stringf("\n\nThis branch contains the entrance%s to %s.",
                             subbranch_names.size() > 1 ? "s" : "",
                             comma_separated_line(begin(subbranch_names),
                                                  end(subbranch_names)).c_str());
    }

    return desc;
}

static string _branch_noise(branch_type br)
{
    string desc;
    const int noise = branches[br].ambient_noise;
    if (noise != 0)
    {
        desc = "\n\nThis branch is ";
        if (noise > 0)
        {
            desc += make_stringf("filled with %snoise, and thus all sounds "
                                 "travel %sless far.",
                                 noise > 5 ? "deafening " : "",
                                 noise > 5 ? "much " : "");
        }
        else
        {
            desc += make_stringf("%s, and thus all sounds travel %sfurther.",
                                 noise < -5 ? "unnaturally silent"
                                            : "very quiet",
                                 noise < -5 ? "much " : "");
        }
    }

    return desc;
}

/**
 * Describe the branch with the given name.
 *
 * @param key       The name of the branch in question.
 * @param suffix    A suffix to trim from the key when making the title.
 * @param footer    A footer to append to the end of descriptions.
 * @return          The keypress the user made to exit.
 */
static int _describe_branch(const string &key, const string &suffix,
                            string footer)
{
    const string branch_name = key.substr(0, key.size() - suffix.size());
    const branch_type branch = branch_by_shortname(branch_name);
    ASSERT(branch != NUM_BRANCHES);

    const string info  = _branch_noise(branch)
                         + _branch_location(branch)
                         + _branch_entry_runes(branch)
                         + _branch_depth(branch)
                         + _branch_subbranches(branch)
                         + "\n\n"
                         + branch_rune_desc(branch, false);

    return _describe_key(key, suffix, footer, info);
}

/// All types of ?/ queries the player can enter.
static const vector<LookupType> lookup_types = {
    LookupType('M', "monster", _recap_mon_keys, _monster_filter,
               _get_monster_keys, nullptr, nullptr,
               _describe_monster,
               lookup_type::SUPPORT_TILES | lookup_type::TOGGLEABLE_SORT),
    LookupType('S', "spell", _recap_spell_keys, _spell_filter,
               nullptr, nullptr, _spell_menu_gen,
               _describe_spell,
               lookup_type::DB_SUFFIX | lookup_type::SUPPORT_TILES),
    LookupType('K', "skill", nullptr, nullptr,
               nullptr, _get_skill_keys, _skill_menu_gen,
               _describe_generic,
               lookup_type::SUPPORT_TILES),
    LookupType('A', "ability", _recap_ability_keys, _ability_filter,
               nullptr, nullptr, _ability_menu_gen,
               _describe_generic,
               lookup_type::DB_SUFFIX | lookup_type::SUPPORT_TILES),
    LookupType('C', "card", _recap_card_keys, _card_filter,
               nullptr, nullptr, _simple_menu_gen,
               _describe_card,
               lookup_type::DB_SUFFIX),
    LookupType('I', "item", nullptr, _item_filter,
               item_name_list_for_glyph, nullptr, _simple_menu_gen,
               _describe_item,
               lookup_type::NONE),
    LookupType('F', "feature", _recap_feat_keys, _feature_filter,
               nullptr, nullptr, _feature_menu_gen,
               _describe_generic,
               lookup_type::SUPPORT_TILES),
    LookupType('G', "god", nullptr, nullptr,
               nullptr, _get_god_keys, _god_menu_gen,
               _describe_god,
               lookup_type::SUPPORT_TILES),
    LookupType('B', "branch", nullptr, nullptr,
               nullptr, _get_branch_keys, _simple_menu_gen,
               _describe_branch,
               lookup_type::DISABLE_SORT),
    LookupType('L', "cloud", nullptr, nullptr,
               nullptr, _get_cloud_keys, _cloud_menu_gen,
               _describe_cloud,
               lookup_type::DB_SUFFIX | lookup_type::SUPPORT_TILES),
    LookupType('T', "status", nullptr, _status_filter,
               nullptr, nullptr, _simple_menu_gen,
               _describe_generic,
               lookup_type::DB_SUFFIX),
};

/**
 * Build a mapping from LookupTypes' symbols to the objects themselves.
 */
static map<char, const LookupType*> _build_lookup_type_map()
{
    map<char, const LookupType*> lookup_map;
    for (const auto &lookup : lookup_types)
        lookup_map[lookup.symbol] = &lookup;
    return lookup_map;
}
static const map<char, const LookupType*> _lookup_types_by_symbol
    = _build_lookup_type_map();

/**
 * Prompt the player for a search string for the given lookup type.
 *
 * @param lookup_type  The LookupType in question (e.g. monsters, items...)
 * @param err[out]     Will be set to a non-empty string if the user failed to
 *                     provide a string.
 * @return             A search string, if one was provided; else "".
 */
static string _prompt_for_regex(const LookupType &lookup_type, string &err)
{
    const string type = lowercase_string(lookup_type.type);
    const string extra = lookup_type.supports_glyph_lookup() ?
        make_stringf(" Enter a single letter to list %s displayed by that"
                     " symbol.", pluralise(type).c_str()) :
        "";
    mprf(MSGCH_PROMPT,
         "Describe a %s; partial names and regexps are fine.%s",
         type.c_str(), extra.c_str());

    mprf(MSGCH_PROMPT, "Describe what? ");
    char buf[80];
    if (cancellable_get_line(buf, sizeof(buf)) || buf[0] == '\0')
    {
        err = "Okay, then.";
        return "";
    }

    const string regex = strlen(buf) == 1 ? buf : trimmed_string(buf);
    return regex;
}

static bool _exact_lookup_match(const LookupType &lookup_type,
                                const string &regex)
{
    if (lookup_type.no_search())
        return false; // no search, no exact match

    if (lookup_type.supports_glyph_lookup() && regex.size() == 1)
        return false; // glyph search doesn't have the concept

    if (lookup_type.filter_forbid && (*lookup_type.filter_forbid)(regex, ""))
        return false; // match found, but incredibly illegal to display

    return !getLongDescription(regex + lookup_type.suffix()).empty();
}

/**
 * Check if the provided keylist is invalid; if so, return the reason why.
 *
 * @param key_list      The list of keys to be checked before display.
 * @param type          The singular name of the things being listed.
 * @param regex         The search term that was used to fetch this list.
 * @param by_symbol     Whether the search is by regex or by glyph.
 * @return              A reason why the list is invalid, if it is
 *                      (e.g. "No monsters with symbol '👻'."),
 *                      or the empty string if the list is valid.
 */
static string _keylist_invalid_reason(const vector<string> &key_list,
                                      const string &type,
                                      const string &regex,
                                      bool by_symbol)
{
    const string plur_type = pluralise(type);

    if (key_list.empty())
    {
        if (by_symbol)
            return "No " + plur_type + " with symbol '" + regex + "'.";
        return "No matching " + plur_type + ".";
    }

    if (key_list.size() > 52)
    {
        if (by_symbol)
        {
            return "Too many " + plur_type + " with symbol '" + regex +
                    "' to display.";
        }

        return make_stringf("Too many matching %s (%d) to display.",
                            plur_type.c_str(), (int) key_list.size());
    }

    // we're good!
    return "";
}

/**
 * Run an iteration of ?/.
 *
 * @param response[out]   A response to input, to print before the next iter.
 * @return                true if the ?/ loop should continue
 *                        false if it should return control to the caller
 */
static bool _find_description(string &response)
{

    const string lookup_type_prompts =
        comma_separated_fn(lookup_types.begin(), lookup_types.end(),
                           mem_fn(&LookupType::prompt_string), " or ");
    mprf(MSGCH_PROMPT, "Describe a %s? ", lookup_type_prompts.c_str());

    int ch;
    {
        cursor_control con(true);
        ch = toupper(getchm());
    }

    const LookupType * const *lookup_type_ptr
        = map_find(_lookup_types_by_symbol, ch);
    if (!lookup_type_ptr)
        return false;

    ASSERT(*lookup_type_ptr);
    const LookupType ltype = **lookup_type_ptr;

    const bool want_regex = !(ltype.no_search());
    const string regex = want_regex ?
                         _prompt_for_regex(ltype, response) :
                         "";

    if (!response.empty())
        return true;

    // not actually sure how to trigger this branch...
    if (want_regex && regex.empty())
    {
        response = "Description must contain at least one non-space.";
        return true;
    }


    // Try to get an exact match first.
    const bool exact_match = _exact_lookup_match(ltype, regex);

    vector<string> key_list = ltype.matching_keys(regex);

    const bool by_symbol = ltype.supports_glyph_lookup()
                           && regex.size() == 1;
    const string type = lowercase_string(ltype.type);
    response = _keylist_invalid_reason(key_list, type, regex, by_symbol);
    if (!response.empty())
        return true;

    if (key_list.size() == 1)
    {
        ltype.describe(key_list[0]);
        return true;
    }

    if (exact_match && ltype.describe(regex, true) != ' ')
        return true;

    if (!(ltype.flags & lookup_type::DISABLE_SORT))
        sort(key_list.begin(), key_list.end());

    ltype.display_keys(key_list);
    return true;
}

/**
 * Run the ?/ loop, repeatedly prompting the player to query for monsters,
 * etc, until they indicate they're done.
 */
void keyhelp_query_descriptions()
{
    string response;
    while (true)
    {
        redraw_screen();

        if (!response.empty())
            mprf(MSGCH_PROMPT, "%s", response.c_str());
        response = "";

        if (!_find_description(response))
            break;

        clear_messages();
    }

    viewwindow();
    mpr("Okay, then.");
}
