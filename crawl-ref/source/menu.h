/**
 * @file
 * @brief Menus and associated malarkey.
**/

#ifndef __MENU_H__
#define __MENU_H__

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

#include "format.h"
#ifdef USE_TILE
 #include "tiledoll.h"
#endif
#ifdef USE_TILE_LOCAL
 #include "tilebuf.h"
#endif

class formatted_string;

enum MenuEntryLevel
{
    MEL_NONE = -1,
    MEL_TITLE,
    MEL_SUBTITLE,
    MEL_ITEM,
};

struct menu_letter
{
    char letter;

    menu_letter() : letter('a') { }
    menu_letter(char c) : letter(c) { }

    operator char () const { return letter; }

    const menu_letter &operator ++ ()
    {
        letter = letter == 'z'? 'A' :
                 letter == 'Z'? 'a' :
                                letter + 1;
        return *this;
    }

    // dummy postfix argument unnamed to stop gcc from complaining
    menu_letter operator ++ (int)
    {
        menu_letter copy = *this;
        operator++();
        return copy;
    }
};

// XXX Use inheritance instead of duplicate code
struct menu_letter2
{
    char letter;

    menu_letter2() : letter('a') { }
    menu_letter2(char c) : letter(c) { }

    operator char () const { return letter; }
    const menu_letter2 &operator ++ ()
    {
        letter = letter == 'z'? '0' :
                 letter == '9'? 'a' :
                                letter + 1;
        return *this;
    }

    menu_letter2 operator ++ (int)
    {
        menu_letter2 copy = *this;
        operator++();
        return copy;
    }
};

struct item_def;
class Menu;

int menu_colour(const string &itemtext,
                const string &prefix = "",
                const string &tag = "");

const int MENU_ITEM_STOCK_COLOUR = LIGHTGREY;
class MenuEntry
{
public:
    string tag;
    string text;
    int quantity, selected_qty;
    colour_t colour;
    vector<int> hotkeys;
    MenuEntryLevel level;
    bool preselected;
    void *data;

#ifdef USE_TILE
    vector<tile_def> tiles;
#endif

public:
    MenuEntry(const string &txt = string(),
               MenuEntryLevel lev = MEL_ITEM,
               int qty  = 0,
               int hotk = 0,
               bool preselect = false) :
        text(txt), quantity(qty), selected_qty(0), colour(-1),
        hotkeys(), level(lev), preselected(preselect), data(nullptr)
    {
        colour = (lev == MEL_ITEM     ?  MENU_ITEM_STOCK_COLOUR :
                  lev == MEL_SUBTITLE ?  BLUE  :
                                         WHITE);
        if (hotk)
            hotkeys.push_back(hotk);
    }
    virtual ~MenuEntry() { }

    bool operator<(const MenuEntry& rhs) const
    {
        return text < rhs.text;
    }

    void add_hotkey(int key)
    {
        if (key && !is_hotkey(key))
            hotkeys.push_back(key);
    }

    bool is_hotkey(int key) const
    {
        return find(hotkeys.begin(), hotkeys.end(), key) != hotkeys.end();
    }

    bool is_primary_hotkey(int key) const
    {
        return hotkeys.size()? hotkeys[0] == key : false;
    }

    virtual string get_text(const bool unused = false) const
    {
        if (level == MEL_ITEM && hotkeys.size())
        {
            char buf[300];
            snprintf(buf, sizeof buf,
                    " %c %c %s", hotkeys[0], preselected ? '+' : '-',
                                 text.c_str());
            return string(buf);
        }
        return text;
    }

    virtual int highlight_colour() const
    {
        return menu_colour(get_text(), "", tag);
    }

    virtual bool selected() const
    {
        return selected_qty > 0 && quantity;
    }

    // -1: Invert
    // -2: Select all
    virtual void select(int qty = -1)
    {
        if (qty == -2)
            selected_qty = quantity;
        else if (selected())
            selected_qty = 0;
        else if (quantity)
            selected_qty = (qty == -1 ? quantity : qty);
    }

    virtual string get_filter_text() const
    {
        return get_text();
    }

#ifdef USE_TILE
    virtual bool get_tiles(vector<tile_def>& tileset) const;

    virtual void add_tile(tile_def tile);
#endif
};

class ToggleableMenuEntry : public MenuEntry
{
public:
    string alt_text;

    ToggleableMenuEntry(const string &txt = string(),
                        const string &alt_txt = string(),
                        MenuEntryLevel lev = MEL_ITEM,
                        int qty = 0, int hotk = 0,
                        bool preselect = false) :
        MenuEntry(txt, lev, qty, hotk, preselect), alt_text(alt_txt) {}

    void toggle() { text.swap(alt_text); }
};

class MonsterMenuEntry : public MenuEntry
{
public:
    MonsterMenuEntry(const string &str, const monster_info* mon, int hotkey);

#ifdef USE_TILE
    virtual bool get_tiles(vector<tile_def>& tileset) const override;
#endif
};

#ifdef USE_TILE
class PlayerMenuEntry : public MenuEntry
{
public:
    PlayerMenuEntry(const string &str);

    virtual bool get_tiles(vector<tile_def>& tileset) const override;
};
#endif

class FeatureMenuEntry : public MenuEntry
{
public:
    coord_def            pos;
    dungeon_feature_type feat;

    FeatureMenuEntry(const string &str, const coord_def p, int hotkey);
    FeatureMenuEntry(const string &str, const dungeon_feature_type f,
                     int hotkey);

#ifdef USE_TILE
    virtual bool get_tiles(vector<tile_def>& tileset) const override;
#endif
};

class MenuHighlighter
{
public:
    virtual int entry_colour(const MenuEntry *entry) const;
    virtual ~MenuHighlighter() { }
};

enum MenuFlag
{
    MF_NOSELECT         = 0x0001,   ///< No selection is permitted
    MF_SINGLESELECT     = 0x0002,   ///< Select just one item
    MF_MULTISELECT      = 0x0004,   ///< Select multiple items
    MF_NO_SELECT_QTY    = 0x0008,   ///< Disallow partial selections
    MF_ANYPRINTABLE     = 0x0010,   ///< Any printable character is valid, and
                                    ///< closes the menu.
    MF_SELECT_BY_PAGE   = 0x0020,   ///< Allow selections to occur only on
                                    ///< currently-visible page.
    MF_ALWAYS_SHOW_MORE = 0x0040,   ///< Always show the -more- footer
    MF_NOWRAP           = 0x0080,   ///< Paging past the end will not wrap back.
    MF_ALLOW_FILTER     = 0x0100,   ///< Control-F will ask for regex and
                                    ///< select the appropriate items.
    MF_ALLOW_FORMATTING = 0x0200,   ///< Parse index for formatted-string
    MF_SHOW_PAGENUMBERS = 0x0400,   ///< Show "(page X of Y)" when appropriate
    MF_TOGGLE_ACTION    = 0x0800,   ///< ToggleableMenu toggles action as well
    MF_EASY_EXIT        = 0x1000,   ///< Exit when scrolling off the end
    MF_START_AT_END     = 0x2000,   ///< Scroll to end of list
    MF_PRESELECTED      = 0x4000,   ///< Has a preselected entry.
    MF_QUIET_SELECT     = 0x8000,   ///< No selection box and no count.
};

class MenuDisplay
{
public:
    MenuDisplay(Menu *menu);
    virtual ~MenuDisplay() {}
    virtual void draw_stock_item(int index, const MenuEntry *me) = 0;
    virtual void set_offset(int lines) = 0;
    virtual void draw_more() = 0;
    virtual void set_num_columns(int columns) = 0;
protected:
    Menu *m_menu;
};

class MenuDisplayText : public MenuDisplay
{
public:
    MenuDisplayText(Menu *menu);
    virtual void draw_stock_item(int index, const MenuEntry *me) override;
    virtual void draw_more() override;
    virtual void set_offset(int lines) override { m_starty = lines; }
    virtual void set_num_columns(int columns) override {}
protected:
    int m_starty;
};

class MenuDisplayTile : public MenuDisplay
{
public:
    MenuDisplayTile(Menu *menu);
    virtual void draw_stock_item(int index, const MenuEntry *me) override;
    virtual void set_offset(int lines) override;
    virtual void draw_more() override;
    virtual void set_num_columns(int columns) override;
};

///////////////////////////////////////////////////////////////////////
// NOTE
// As a general contract, any pointers you pass to Menu methods are OWNED BY
// THE MENU, and will be deleted by the Menu on destruction. So any pointers
// you pass in MUST be allocated with new, or Crawl will crash.

#define NUMBUFSIZ 10

// FIXME: MenuEntry is a large object, and shouldn't be used for
// showing text files.

class Menu
{
public:
    Menu(int flags = MF_MULTISELECT, const string& tagname = "",
         bool text_only = true);

    virtual ~Menu();

    // Remove all items from the Menu, leave title intact.
    void clear();

    // Sets menu flags to new_flags. If use_options is true, game options may
    // override options.
    void set_flags(int new_flags, bool use_options = true);
    int  get_flags() const        { return flags; }
    virtual bool is_set(int flag) const;
    void set_tag(const string& t) { tag = t; }

    bool draw_title_suffix(const string &s, bool titlefirst = true);
    bool draw_title_suffix(const formatted_string &fs, bool titlefirst = true);
    void update_title();

    // Sets a replacement for the default -more- string.
    void set_more(const formatted_string &more);
    // Shows a stock message about scrolling the menu instead of -more-
    void set_more();
    const formatted_string &get_more() const { return more; }

    void set_highlighter(MenuHighlighter *h);
    void set_title(MenuEntry *e, bool first = true);
    void add_entry(MenuEntry *entry);
    void add_entry(unique_ptr<MenuEntry> entry)
    {
        add_entry(entry.release());
    }
    void get_selected(vector<MenuEntry*> *sel) const;
    virtual int get_cursor() const;

    void set_maxpagesize(int max);
    int maxpagesize() const { return max_pagesize; }

    void set_select_filter(vector<text_pattern> filter)
    {
        select_filter = filter;
    }

    virtual int getkey() const { return lastch; }

    void reset();
    virtual vector<MenuEntry *> show(bool reuse_selections = false);
    vector<MenuEntry *> selected_entries() const;

    size_t item_count() const    { return items.size(); }

    // Get entry index, skipping quantity 0 entries. Returns -1 if not found.
    int get_entry_index(const MenuEntry *e) const;

    virtual int item_colour(int index, const MenuEntry *me) const;
    int get_y_offset() const { return y_offset; }
    int get_pagesize() const { return pagesize; }

    typedef string (*selitem_tfn)(const vector<MenuEntry*> *sel);
    typedef void (*drawitem_tfn)(int index, const MenuEntry *me);
    typedef int (*keyfilter_tfn)(int keyin);

    selitem_tfn      f_selitem;
    drawitem_tfn     f_drawitem;
    keyfilter_tfn    f_keyfilter;

    enum cycle  { CYCLE_NONE, CYCLE_TOGGLE, CYCLE_CYCLE } action_cycle;
    enum action { ACT_EXECUTE, ACT_EXAMINE, ACT_MISC, ACT_NUM } menu_action;

#ifdef USE_TILE_WEB
    void webtiles_write_menu(bool replace = false) const;
    void webtiles_scroll(int first);
    void webtiles_handle_item_request(int start, int end);
#endif
protected:
    MenuEntry *title;
    MenuEntry *title2;

    int flags;
    string tag;

    int first_entry, y_offset;
    int pagesize, max_pagesize;

    formatted_string more;

    vector<MenuEntry*>  items;
    vector<MenuEntry*>  sel;
    vector<text_pattern> select_filter;

    // Class that is queried to colour menu entries.
    MenuHighlighter *highlighter;

    int num;

    int lastch;

    bool alive;

    int last_selected;

    MenuDisplay *mdisplay;

protected:
    void check_add_formatted_line(int firstcol, int nextcol,
                                  string &line, bool check_eol);
    void do_menu();
    virtual string get_select_count_string(int count) const;
    virtual void draw_select_count(int count, bool force = false);
    virtual void draw_item(int index) const;
    virtual void draw_index_item(int index, const MenuEntry *me) const;
    virtual void draw_stock_item(int index, const MenuEntry *me) const;

#ifdef USE_TILE_WEB
    void webtiles_set_title(const formatted_string title);
    void webtiles_set_suffix(const formatted_string title);

    void webtiles_write_tiles(const MenuEntry& me) const;
    void webtiles_update_items(int start, int end) const;
    void webtiles_update_item(int index) const;
    void webtiles_update_title() const;
    void webtiles_update_scroll_pos() const;

    virtual void webtiles_write_title() const;
    virtual void webtiles_write_item(int index, const MenuEntry *me) const;

    void webtiles_update_section_boundaries();

    int _webtiles_section_start;
    int _webtiles_section_end;

    bool _webtiles_title_changed;
    formatted_string _webtiles_title;
    formatted_string _webtiles_suffix;

    inline int webtiles_section_start() const
    {
        return _webtiles_section_start == -1 ? 0 : _webtiles_section_start;
    }
    inline int webtiles_section_end() const
    {
        return _webtiles_section_end == -1 ? items.size() : _webtiles_section_end;
    }
#endif

    virtual void draw_title();
    virtual void write_title();
    virtual int title_height() const;
    virtual void draw_menu();
    virtual bool page_down();
    virtual bool line_down();
    virtual bool page_up();
    virtual bool line_up();

    virtual int pre_process(int key);
    virtual int post_process(int key);

    bool in_page(int index) const;

    void deselect_all(bool update_view = true);
    virtual void select_items(int key, int qty = -1);
    void select_item_index(int idx, int qty, bool draw_cursor = true);
    void select_index(int index, int qty = -1);

    bool is_hotkey(int index, int key);
    virtual bool is_selectable(int index) const;

    virtual bool process_key(int keyin);

    virtual bool allow_easy_exit() const;

    virtual string help_key() const { return ""; }
    virtual bool always_redraw() const { return false; }
};

/// Allows toggling by specific keys.
class ToggleableMenu : public Menu
{
public:
    ToggleableMenu(int _flags = MF_MULTISELECT, bool text_only = true)
        : Menu(_flags, "", text_only) {}
    void add_toggle_key(int newkey) { toggle_keys.push_back(newkey); }
protected:
    virtual int pre_process(int key) override;

    vector<int> toggle_keys;
};

// This is only tangentially related to menus, but what the heck.
// Note, column margins start on 1, not 0.
class column_composer
{
public:
    // Number of columns and left margins for 2nd, 3rd, ... nth column.
    column_composer(int ncols, ...);

    void clear();
    void add_formatted(int ncol,
            const string &tagged_text,
            bool  add_separator = true,
            bool  eol_ends_format = true,
            bool (*text_filter)(const string &tag) = nullptr,
            int   margin = -1);

    vector<formatted_string> formatted_lines() const;

    void set_pagesize(int pagesize);

private:
    struct column;
    void compose_formatted_column(
            const vector<formatted_string> &lines,
            int start_col,
            int margin);
    void strip_blank_lines(vector<formatted_string> &) const;

private:
    struct column
    {
        int margin;
        int lines;

        column(int marg = 1) : margin(marg), lines(0) { }
    };

    int pagesize;
    vector<column> columns;
    vector<formatted_string> flines;
};

// This class is for when (some of) your items are formatted, and
// you want mostly a browser.
//
// If MF_NOSELECT, hotkeys jump to menu items.
// If MF_SINGLESELECT, hotkeys end the menu immediately.
// MF_MULTISELECT is not supported.
class formatted_scroller : public Menu
{
public:
    formatted_scroller();
    formatted_scroller(int flags, const string& s);
    virtual void add_item_formatted_string(const formatted_string& s,
                                           int hotkey = 0);
    virtual void wrap_formatted_string(const formatted_string& s,
                                       int width = get_number_of_cols()-1);
    virtual void add_item_string(const string& s, int hotkey = 0);
    virtual void add_text(const string& s, bool new_line = false,
                          int wrap_col = 0);
    virtual void add_raw_text(const string& s, bool new_line = false,
                              int wrap_col = 0);
    virtual bool jump_to_hotkey(int keyin);
    virtual vector<MenuEntry *> show(bool reuse_selections = false) override;
    int get_lastch() { return lastch; }
    virtual ~formatted_scroller();
protected:
    virtual bool page_down() override;
    virtual bool line_down() override;
    virtual bool page_up() override;
    virtual bool line_up() override;

    virtual void draw_index_item(int index, const MenuEntry* me) const override;
    virtual bool process_key(int keyin) override;
    bool jump_to(int linenum);

#ifdef USE_TILE_WEB
    virtual void webtiles_write_item(int index,
                                     const MenuEntry* me) const override;
#endif
};

/**
 * @author Janne "felirx" Lahdenpera
 * Abstract base class interface for all menu items to inherit from.
 * Each item should know how it's rendered.
 * Rendering should only check the item bounds and screen bounds to prevent
 * assertion errors.
 */
class MenuItem
{
public:
    MenuItem();
    virtual ~MenuItem();

    void set_height(const int height);

    void set_id(int id) { m_item_id = id; }
    int get_id() const { return m_item_id; }

    virtual void set_bounds(const coord_def& min_coord, const coord_def& max_coord);
    virtual void set_bounds_no_multiply(const coord_def& min_coord,
                                        const coord_def& max_coord);
    virtual void move(const coord_def& delta);
    virtual const coord_def& get_min_coord() const { return m_min_coord; }
    virtual const coord_def& get_max_coord() const { return m_max_coord; }

    virtual void set_description_text(const string& text) { m_description = text; }
    virtual const string& get_description_text() const { return m_description; }

#ifdef USE_TILE_LOCAL
    virtual bool handle_mouse(const MouseEvent& me) {return false; }
#endif

    virtual void select(bool toggle);
    virtual void select(bool toggle, int value);
    virtual bool selected() const;
    virtual void allow_highlight(bool toggle);
    virtual bool can_be_highlighted() const;
    virtual void set_highlight_colour(COLOURS colour);
    virtual COLOURS get_highlight_colour() const;
    virtual void set_fg_colour(COLOURS colour);
    virtual void set_bg_colour(COLOURS colour);
    virtual COLOURS get_fg_colour() const;
    virtual COLOURS get_bg_colour() const;

    virtual void set_visible(bool flag);
    virtual bool is_visible() const;

    virtual void render() = 0;

    void add_hotkey(int key);
    const vector<int>& get_hotkeys() const;
    void clear_hotkeys();

    void set_link_left(MenuItem* item);
    void set_link_right(MenuItem* item);
    void set_link_up(MenuItem* item);
    void set_link_down(MenuItem* item);

    MenuItem* get_link_left() const;
    MenuItem* get_link_right() const;
    MenuItem* get_link_up() const;
    MenuItem* get_link_down() const;

protected:
    coord_def m_min_coord;
    coord_def m_max_coord;
    bool m_selected;
    bool m_allow_highlight;
    bool m_dirty;
    bool m_visible;
    vector<int> m_hotkeys;
    string m_description;

    COLOURS m_fg_colour;
    COLOURS m_highlight_colour;
    int m_bg_colour;

    MenuItem* m_link_left;
    MenuItem* m_link_right;
    MenuItem* m_link_up;
    MenuItem* m_link_down;

#ifdef USE_TILE_LOCAL
    // Holds the conversion values to translate unit values to pixel values
    unsigned int m_unit_width_pixels;
    unsigned int m_unit_height_pixels;
    int get_vertical_offset() const;
#endif

    int m_item_id;
};

/**
 * Basic Item with string unformatted text that can be selected
 */
class TextItem : public MenuItem
{
public:
    TextItem();
    virtual ~TextItem();

    virtual void set_bounds(const coord_def& min_coord, const coord_def& max_coord) override;
    virtual void set_bounds_no_multiply(const coord_def& min_coord,
                                        const coord_def& max_coord) override;

    virtual void render() override;

    void set_text(const string& text);
    const string& get_text() const;
protected:
    void _wrap_text();

    string m_text;
    string m_render_text;

#ifdef USE_TILE_LOCAL
    FontBuffer m_font_buf;
#endif
};

/**
 * Behaves the same as TextItem, except selection has been overridden to always
 * return false
 */
class NoSelectTextItem : public TextItem
{
public:
    NoSelectTextItem();
    virtual ~NoSelectTextItem();
    virtual bool selected() const override;
    virtual bool can_be_highlighted() const override;
};

/**
 * Behaves the same as TextItem but use formatted text for rendering.
 * It ignores bg_colour.
 * TODO: add bg_colour support to formatted_string and merge this with TextItem.
 */
class FormattedTextItem : public TextItem
{
public:
    virtual void render() override;
};

/**
 * Holds an arbitrary number of tiles, currently rendered on top of each other
 */
#ifdef USE_TILE_LOCAL
class TextTileItem : public TextItem
{
public:
    TextTileItem();
    virtual ~TextTileItem();

    virtual void set_bounds(const coord_def& min_coord, const coord_def& max_coord) override;
    virtual void render() override;

    virtual void add_tile(tile_def tile);
    void clear_tile() { m_tiles.clear(); };

protected:
    vector<tile_def> m_tiles;
    FixedVector<TileBuffer, TEX_MAX> m_tile_buf;
};

/**
 * Specialization of TextTileItem that knows how to pack a player doll
 * TODO: reform _pack_doll() since it currently holds duplicate code from
 * tilereg.cc pack_doll_buf()
 */
class SaveMenuItem : public TextTileItem
{
public:
    friend class TilesFramework;
    SaveMenuItem();
    virtual ~SaveMenuItem();

    virtual void render() override;

    void set_doll(dolls_data doll);

protected:
    void _pack_doll();
    dolls_data m_save_doll;
};
#endif

class PrecisionMenu;

/**
 * Abstract base class interface for all attachable objects.
 * Objects are generally containers that hold MenuItems, however special
 * objects are also possible, for instance MenuDescriptor.
 * All objects should have an unique string name, although the uniqueness
 * is not enforced or checked right now.
 */
class MenuObject
{
public:
    enum InputReturnValue
    {
        INPUT_NO_ACTION,          // Nothing happened
        INPUT_SELECTED,           // Something got selected
        INPUT_DESELECTED,         // Something got deselected
        INPUT_END_MENU_SUCCESS,   // Call the menu to end
        INPUT_END_MENU_ABORT,     // Call the menu to clear all selections and end
        INPUT_ACTIVE_CHANGED,     // Mouse position or keyboard event changed active
        INPUT_FOCUS_RELEASE_UP,   // Selection went out of menu from top
        INPUT_FOCUS_RELEASE_DOWN, // Selection went out of menu from down
        INPUT_FOCUS_RELEASE_LEFT, // Selection went out of menu from left
        INPUT_FOCUS_RELEASE_RIGHT,// Selection went out of menu from right
        INPUT_FOCUS_LOST,         // Eg. the user is moving his mouse somewhere else
    };

    MenuObject();
    virtual ~MenuObject();

    void set_height(const int height);
    void init(const coord_def& min_coord, const coord_def& max_coord,
              const string& name);
    const coord_def& get_min_coord() const { return m_min_coord; }
    const coord_def& get_max_coord() const { return m_max_coord; }
    const string& get_name() const { return m_object_name; }

    virtual void allow_focus(bool toggle);
    virtual bool can_be_focused();

    virtual InputReturnValue process_input(int key) = 0;
#ifdef USE_TILE_LOCAL
    virtual InputReturnValue handle_mouse(const MouseEvent& me) = 0;
#endif
    virtual void render() = 0;

    virtual void set_active_item(int ID) = 0;
    virtual void set_active_item(MenuItem* item) = 0;
    virtual void activate_first_item() = 0;
    virtual void activate_last_item() = 0;

    virtual void set_visible(bool flag);
    virtual bool is_visible() const;

    virtual vector<MenuItem*> get_selected_items();
    virtual bool select_item(int index) = 0;
    virtual bool select_item(MenuItem* item) = 0;
    virtual MenuItem* find_item_by_hotkey(int key);
    virtual MenuItem* select_item_by_hotkey(int key);
    virtual void clear_selections();
    virtual MenuItem* get_active_item() = 0;

    virtual bool attach_item(MenuItem* item) = 0;

protected:
    enum Direction
    {
        UP,
        DOWN,
        LEFT,
        RIGHT,
    };
    virtual void _place_items() = 0;
    virtual bool _is_mouse_in_bounds(const coord_def& pos);
    virtual MenuItem* _find_item_by_mouse_coords(const coord_def& pos);
    virtual MenuItem* _find_item_by_direction(const MenuItem* start,
                                              MenuObject::Direction dir) = 0;

    bool m_dirty;
    bool m_allow_focus;
    bool m_visible;

    coord_def m_min_coord;
    coord_def m_max_coord;
    string m_object_name;
    // by default, entries are held in a vector
    // if you need a different behaviour, please override the
    // affected methods
    vector<MenuItem*> m_entries;
#ifdef USE_TILE_LOCAL
    // Holds the conversion values to translate unit values to pixel values
    unsigned int m_unit_width_pixels;
    unsigned int m_unit_height_pixels;
#endif
};

/**
 * Container object that holds MenuItems in a 2d plane.
 * There is no internal hierarchy structure inside the menu, thus the objects
 * are freely placed within the boundaries of the container
 */
class MenuFreeform : public MenuObject
{
public:
    MenuFreeform();
    virtual ~MenuFreeform();

    virtual InputReturnValue process_input(int key) override;
#ifdef USE_TILE_LOCAL
    virtual InputReturnValue handle_mouse(const MouseEvent& me) override;
#endif
    virtual void render() override;
    virtual MenuItem* get_active_item() override;
    virtual void set_active_item(int ID) override;
    virtual void set_active_item(MenuItem* item) override;
    virtual void activate_first_item() override;
    virtual void activate_last_item() override;
    void set_default_item(MenuItem* item);
    void activate_default_item();

    virtual bool select_item(int index) override;
    virtual bool select_item(MenuItem* item) override;
    virtual bool attach_item(MenuItem* item) override;

protected:
    virtual void _place_items() override;
    virtual void _set_active_item_by_index(int index);
    virtual MenuItem* _find_item_by_direction(
            const MenuItem* start, MenuObject::Direction dir) override;

    // cursor position
    MenuItem* m_active_item;
    MenuItem* m_default_item;
};

/**
 * Container that can hold any number of objects in a scroll list style.
 * Only certain number of items are visible at the same time.
 * Navigating the list works with ARROW_UP and ARROW_DOWN keys.
 * Eventually it should also support scrollbars.
 */
class MenuScroller : public MenuObject
{
public:
    MenuScroller();
    virtual ~MenuScroller();

    virtual InputReturnValue process_input(int key) override;
#ifdef USE_TILE_LOCAL
    virtual InputReturnValue handle_mouse(const MouseEvent& me) override;
#endif
    virtual void render() override;
    virtual MenuItem* get_active_item() override;
    virtual void set_active_item(int ID) override;
    virtual void set_active_item(MenuItem* item) override;
    virtual void activate_first_item() override;
    virtual void activate_last_item() override;

    virtual bool select_item(int index) override;
    virtual bool select_item(MenuItem* item) override;
    virtual bool attach_item(MenuItem* item) override;
protected:
    virtual void _place_items() override;
    virtual void _set_active_item_by_index(int index);
    virtual MenuItem* _find_item_by_direction(int start_index,
                                              MenuObject::Direction dir);
    virtual MenuItem* _find_item_by_direction(
            const MenuItem* start, MenuObject::Direction dir) override
    {
        return nullptr;
    }

    int m_topmost_visible;
    int m_currently_active;
    int m_items_shown;

#ifdef USE_TILE_LOCAL
    TextTileItem *m_arrow_up;
    TextTileItem *m_arrow_down;
#endif
};

/**
 * Base class for various descriptor and highlighter objects.
 * These should probably be attached last to the menu to be rendered last.
 */
class MenuDescriptor : public MenuObject
{
public:
    MenuDescriptor(PrecisionMenu* parent);
    virtual ~MenuDescriptor();

    void init(const coord_def& min_coord, const coord_def& max_coord,
              const string& name);

    virtual InputReturnValue process_input(int key) override;
#ifdef USE_TILE_LOCAL
    virtual InputReturnValue handle_mouse(const MouseEvent& me) override;
#endif
    virtual void render() override;

    // these are not used, clear them
    virtual vector<MenuItem*> get_selected_items() override;
    virtual MenuItem* get_active_item() override { return nullptr; }
    virtual bool attach_item(MenuItem* item) override { return false; }
    virtual void set_active_item(int index) override {}
    virtual void set_active_item(MenuItem* item) override {}
    virtual void activate_first_item() override {}
    virtual void activate_last_item() override {}

    virtual bool select_item(int index) override { return false; }
    virtual bool select_item(MenuItem* item) override { return false;}
    virtual MenuItem* select_item_by_hotkey(int key) override
    {
        return nullptr;
    }
    virtual void clear_selections() override {}

    // Do not allow focus
    virtual void allow_focus(bool toggle) override {}
    virtual bool can_be_focused() override { return false; }

protected:
    virtual void _place_items() override;
    virtual MenuItem* _find_item_by_mouse_coords(const coord_def& pos) override
    {
        return nullptr;
    }
    virtual MenuItem* _find_item_by_direction(
            const MenuItem* start, MenuObject::Direction dir) override
    {
        return nullptr;
    }

    // Used to pull out currently active item
    PrecisionMenu* m_parent;
    MenuItem* m_active_item;
    NoSelectTextItem m_desc_item;
};

/**
 * Class for mouse over tooltips, does nothing if USE_TILE_LOCAL is not defined
 * TODO: actually implement render() and _place_items()
 */
class MenuTooltip : public MenuDescriptor
{
public:
    MenuTooltip(PrecisionMenu* parent);
    virtual ~MenuTooltip();

#ifdef USE_TILE_LOCAL
    virtual InputReturnValue handle_mouse(const MouseEvent& me) override;
#endif
    virtual void render() override;
protected:
    virtual void _place_items() override;

#ifdef USE_TILE_LOCAL
    ShapeBuffer m_background;
    FontBuffer m_font_buf;
#endif
};

/**
 * Highlighter object.
 * TILES: It will create a colored rectangle around the currently active item.
 * CONSOLE: It will muck with the Item background color, setting it to highlight
 *          colour, reverting the change when active changes.
 */
class BoxMenuHighlighter : public MenuObject
{
public:
    BoxMenuHighlighter(PrecisionMenu* parent);
    virtual ~BoxMenuHighlighter();

    virtual InputReturnValue process_input(int key) override;
#ifdef USE_TILE_LOCAL
    virtual InputReturnValue handle_mouse(const MouseEvent& me) override;
#endif
    virtual void render() override;

    // these are not used, clear them
    virtual vector<MenuItem*> get_selected_items() override;
    virtual MenuItem* get_active_item() override { return nullptr; }
    virtual bool attach_item(MenuItem* item) override { return false; }
    virtual void set_active_item(int index) override {}
    virtual void set_active_item(MenuItem* item) override {}
    virtual void activate_first_item() override {}
    virtual void activate_last_item() override {}

    virtual bool select_item(int index) override { return false; }
    virtual bool select_item(MenuItem* item) override { return false;}
    virtual MenuItem* select_item_by_hotkey(int key) override
    {
        return nullptr;
    }
    virtual void clear_selections() override {}

    // Do not allow focus
    virtual void allow_focus(bool toggle) override {}
    virtual bool can_be_focused() override { return false; }

protected:
    virtual void _place_items() override;
    virtual MenuItem* _find_item_by_mouse_coords(const coord_def& pos) override
    {
        return nullptr;
    }
    virtual MenuItem* _find_item_by_direction(
            const MenuItem* start, MenuObject::Direction dir) override
    {
        return nullptr;
    }

    // Used to pull out currently active item
    PrecisionMenu* m_parent;
    MenuItem* m_active_item;

#ifdef USE_TILE_LOCAL
    LineBuffer m_line_buf;
#else
    COLOURS m_old_bg_colour;
#endif
};

class BlackWhiteHighlighter : public BoxMenuHighlighter
{
public:
    BlackWhiteHighlighter(PrecisionMenu* parent);
    virtual ~BlackWhiteHighlighter();

    virtual void render() override;
protected:
    virtual void _place_items() override;

#ifdef USE_TILE_LOCAL
    // Tiles does not seem to support background colors
    ShapeBuffer m_shape_buf;
#endif
    COLOURS m_old_bg_colour;
    COLOURS m_old_fg_colour;
};

/**
 * Inheritable root node of a menu that holds MenuObjects.
 * It is always full screen.
 * Everything attached to it it owns, thus deleting the memory when it exits the
 * scope.
 * TODO: add multiple paging support
 */
class PrecisionMenu
{
public:
    enum SelectType
    {
        PRECISION_SINGLESELECT,
        PRECISION_MULTISELECT,
    };

    PrecisionMenu();
    virtual ~PrecisionMenu();

    virtual void set_select_type(SelectType flag);
    virtual void clear();

    virtual void draw_menu();
    virtual bool process_key(int key);
#ifdef USE_TILE_LOCAL
    virtual int handle_mouse(const MouseEvent& me);
#endif

    // not const on purpose
    virtual vector<MenuItem*> get_selected_items();

    virtual void attach_object(MenuObject* item);
    virtual MenuObject* get_object_by_name(const string& search);
    virtual MenuItem* get_active_item();
    virtual void set_active_object(MenuObject* object);
    virtual void clear_selections();
protected:
    // These correspond to the Arrow keys when used for browsing the menus
    enum Direction
    {
        UP,
        DOWN,
        LEFT,
        RIGHT,
    };
    MenuObject* _find_object_by_direction(const MenuObject* start, Direction dir);

    vector<MenuObject*> m_attached_objects;
    MenuObject* m_active_object;

    SelectType m_select_type;
};

int linebreak_string(string& s, int maxcol, bool indent = false);
string get_linebreak_string(const string& s, int maxcol);

#ifdef USE_TILE_LOCAL
class Popup
{
public:
    // constructors
    Popup(string prompt);

    // accessors
    //  get/set prompt text
    void set_prompt(string prompt);
    string get_prompt() { return m_prompt; }

    // methods
    //  add a menu entry to the popup
    void push_entry(MenuEntry *me);
    //  remove a menu entry from the popup
    MenuEntry* next_entry();

    //  draw the popup and return key pressed
    int pop();

protected:
    vector<MenuEntry*> m_entries;
    string m_prompt;

private:
    unsigned int m_curr;
};
#endif

#endif
