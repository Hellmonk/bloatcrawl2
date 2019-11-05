/**
 * @file
 * @brief Functions for inventory related commands.
**/

#pragma once

#include <cstddef>
#include <functional>
#include <vector>

#include "enum.h"
#include "equipment-type.h"
#include "item-name.h"
#include "item-prop-enum.h"
#include "menu.h"
#include "operation-types.h"

enum object_selector
{
    OSEL_ANY                     =  -1,
    OSEL_WIELD                   =  -2,
    OSEL_UNIDENT                 =  -3,
#if TAG_MAJOR_VERSION == 34
    OSEL_RECHARGE                =  -4,
#endif
    OSEL_ENCHANTABLE_ARMOUR      =  -5,
    OSEL_BEOGH_GIFT              =  -6,
#if TAG_MAJOR_VERSION == 34
    OSEL_DRAW_DECK               =  -7,
#endif
    OSEL_THROWABLE               =  -8,
    OSEL_EVOKABLE                =  -9,
    OSEL_WORN_ARMOUR             = -10,
    OSEL_CURSED_WORN             = -11,
#if TAG_MAJOR_VERSION == 34
    OSEL_UNCURSED_WORN_ARMOUR    = -12,
    OSEL_UNCURSED_WORN_JEWELLERY = -13,
#endif
    OSEL_BRANDABLE_WEAPON        = -14,
    OSEL_ENCHANTABLE_WEAPON      = -15,
    OSEL_BLESSABLE_WEAPON        = -16,
    OSEL_CURSABLE                = -17, // Items that are cursable and not
                                        // known-cursed. Unknown-cursed items
                                        // are included, to prevent information
                                        // leakage.
#if TAG_MAJOR_VERSION == 34
    OSEL_DIVINE_RECHARGE         = -18,
#endif
    OSEL_UNCURSED_WORN_RINGS     = -19,
};

/// Behaviour flags for prompt_invent_item().
enum class invprompt_flag
{
    none               = 0,
    /// Warning inscriptions are not checked & the player will not be warned.
    no_warning         = 1 << 0,
    /// '\' will be ignored, instead of switching to the known item list.
    hide_known         = 1 << 1,
    /// Allow selecting items that do not exist.
    unthings_ok        = 1 << 2,
    /// Don't start in the '?' list InvMenu.
    manual_list        = 1 << 3,
    /// Only allow exiting with escape, not also space.
    escape_only        = 1 << 4,
};
DEF_BITFIELD(invent_prompt_flags, invprompt_flag);

#define PROMPT_ABORT         -1
#define PROMPT_GOT_SPECIAL   -2
#define PROMPT_NOTHING       -3

#define SLOT_BARE_HANDS      PROMPT_GOT_SPECIAL

extern FixedVector<int, NUM_OBJECT_CLASSES> inv_order;

struct SelItem
{
    int slot;
    int quantity;
    const item_def *item;
    bool has_star;
    SelItem() : slot(0), quantity(0), item(nullptr), has_star(false) { }
    SelItem(int s, int q, const item_def *it = nullptr, bool do_star = false)
        : slot(s), quantity(q), item(it), has_star(do_star)
    {
    }
};

typedef string (*invtitle_annotator)(const Menu *m, const string &oldtitle);

class InvTitle : public MenuEntry
{
public:
    InvTitle(Menu *mn, const string &title, invtitle_annotator tfn);

    string get_text(const bool = false) const override;

private:
    Menu *m;
    invtitle_annotator titlefn;
};

class InvEntry : public MenuEntry
{
private:
    static bool show_glyph;

    mutable string basename;
    mutable string qualname;
    mutable string dbname;

protected:
    static bool show_cursor;
    // Should we show the floor tile, etc?
    bool show_background = true;

public:
    const item_def *item;

    InvEntry(const item_def &i);
    string get_text(const bool need_cursor = false) const override;
    void set_show_glyph(bool doshow);
    static void set_show_cursor(bool doshow);

    const string &get_basename() const;
    const string &get_qualname() const;
    const string &get_fullname() const;
    const string &get_dbname() const;
    bool         is_cursed() const;
    bool         is_glowing() const;
    bool         is_ego() const;
    bool         is_art() const;
    bool         is_equipped() const;

    virtual int highlight_colour() const override
    {
        return menu_colour(get_text(), item_prefix(*item), tag);
    }

    virtual void select(int qty = -1) override;
    void set_star(bool);
    bool has_star() const;

    virtual string get_filter_text() const override;

    // returns const false and leaves arg unchanged unless USE_TILES is defined
    virtual bool get_tiles(vector<tile_def>& tiles) const override;

private:
    void add_class_hotkeys(const item_def &i);
    bool _has_star;
};

class InvMenu : public Menu
{
public:
    InvMenu(int mflags = MF_MULTISELECT);

public:
    int getkey() const override;

    void set_preselect(const vector<SelItem> *pre);
    void set_type(menu_type t);

    // Sets function to annotate the title with meta-information if needed.
    // If you set this, do so *before* calling set_title, or it won't take
    // effect.
    void set_title_annotator(invtitle_annotator fn);

    // Not an override, but an overload. Not virtual!
    void set_title(MenuEntry *title, bool first = true);
    void set_title(const string &s);

    // Loads items into the menu. If "procfn" is provided, it'll be called
    // for each MenuEntry added.
    // NOTE: Does not set menu title, ever! You *must* set the title explicitly
    menu_letter load_items(const vector<const item_def*> &items,
                           function<MenuEntry* (MenuEntry*)> procfn = nullptr,
                           menu_letter ckey = 'a', bool sort = true);

    // Make sure this menu does not outlive items, or mayhem will ensue!
    menu_letter load_items(const vector<item_def>& items,
                           function<MenuEntry* (MenuEntry*)> procfn = nullptr,
                           menu_letter ckey = 'a', bool sort = true);

    // Loads items from the player's inventory into the menu, and sets the
    // title to the stock title. If "procfn" is provided, it'll be called for
    // each MenuEntry added, *excluding the title*.
    void load_inv_items(int item_selector = OSEL_ANY, int excluded_slot = -1,
                        function<MenuEntry* (MenuEntry*)> procfn = nullptr);

    vector<SelItem> get_selitems() const;

    const menu_sort_condition *find_menu_sort_condition() const;
    void sort_menu(vector<InvEntry*> &items, const menu_sort_condition *cond);

    // Drop menu only: if true, dropped items are removed from autopickup.
    bool mode_special_drop() const;

protected:
    void do_preselect(InvEntry *ie);
    void select_item_index(int idx, int qty, bool draw_cursor = true) override;
    int pre_process(int key) override;
    virtual bool is_selectable(int index) const override;
    virtual string help_key() const override;

protected:
    menu_type type;
    const vector<SelItem> *pre_select;

    invtitle_annotator title_annotate;
    string temp_title;

private:
    bool _mode_special_drop;
};

void get_class_hotkeys(const int type, vector<char> &glyphs);

bool item_is_selected(const item_def &item, int selector);
bool any_items_of_type(int type_expect, int excluded_slot = -1, bool inspect_floor = false);
string no_selectables_message(int item_selector);

string slot_description();

int prompt_invent_item(const char *prompt,
                       menu_type type,
                       int type_expect,
                       operation_types oper = OPER_ANY,
                       invent_prompt_flags flags = invprompt_flag::none,
                       const char other_valid_char = '\0');

vector<SelItem> select_items(
                        const vector<const item_def*> &items,
                        const char *title, bool noselect = false,
                        menu_type mtype = menu_type::pickup);

vector<SelItem> prompt_drop_items(const vector<SelItem> &preselected_items);

void display_inventory();

bool in_inventory(const item_def &i);
void identify_inventory();

const char *item_class_name(int type, bool terse = false);
const char *item_slot_name(equipment_type type);

#ifdef USE_TILE
bool get_tiles_for_item(const item_def &item, vector<tile_def>& tileset, bool show_background);
#endif

bool check_old_item_warning(const item_def& item, operation_types oper);
bool check_warning_inscriptions(const item_def& item, operation_types oper);

void init_item_sort_comparators(item_sort_comparators &list,
                                const string &set);

bool prompt_failed(int retval);

void list_charging_evokers(FixedVector<item_def*, NUM_MISCELLANY> &evokers);

bool item_is_wieldable(const item_def &item);
bool item_is_evokable(const item_def &item, bool reach = true,
                      bool known = false, bool msg = false, bool equip = true);
bool needs_notele_warning(const item_def &item, operation_types oper);
bool needs_handle_warning(const item_def &item, operation_types oper,
                          bool &penance);
item_def *digit_inscription_to_item(char digit, operation_types oper);
