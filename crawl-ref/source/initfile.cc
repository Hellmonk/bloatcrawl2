/**
 * @file
 * @brief Simple reading of an init file and system variables
 * @detailed read_init_file is the main function, but read_option_line does
 * most of the work though. Read through read_init_file to get an overview of
 * how Crawl loads options. This file also contains a large number of utility
 * functions for setting particular options and converting between human
 * readable strings and internal values. (E.g. str_to_enemy_hp_colour,
 * _weapon_to_str). There is also some code dealing with sorting menus.
**/

#include "AppHdr.h"

#include "initfile.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>

#include "chardump.h"
#include "clua.h"
#include "colour.h"
#include "defines.h"
#include "delay.h"
#include "directn.h"
#include "dlua.h"
#include "end.h"
#include "errors.h"
#include "files.h"
#include "invent.h"
#include "itemprop.h"
#include "items.h"
#include "jobs.h"
#include "kills.h"
#include "libutil.h"
#include "macro.h"
#include "mapdef.h"
#include "message.h"
#include "misc.h"
#include "mon-util.h"
#include "newgame.h"
#include "options.h"
#include "playable.h"
#include "player.h"
#include "prompt.h"
#include "species.h"
#include "spl-util.h"
#include "stash.h"
#include "state.h"
#include "stringutil.h"
#include "syscalls.h"
#include "tags.h"
#include "throw.h"
#include "travel.h"
#include "unwind.h"
#include "version.h"
#include "viewchar.h"
#include "view.h"
#ifdef USE_TILE
#include "tilepick.h"
#include "tiledef-player.h"
#ifdef USE_TILE_WEB
#include "tileweb.h"
#endif
#endif

// For finding the executable's path
#ifdef TARGET_OS_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlwapi.h>
#include <shlobj.h>
#elif defined (__APPLE__)
extern char **NXArgv;
#elif defined (__linux__)
#include <unistd.h>
#endif

const string game_options::interrupt_prefix = "interrupt_";
system_environment SysEnv;
game_options Options;

template <class L, class E>
static L& remove_matching(L& lis, const E& entry)
{
    lis.erase(remove(lis.begin(), lis.end(), entry), lis.end());
    return lis;
}

object_class_type item_class_by_sym(ucs_t c)
{
    switch (c)
    {
    case ')':
        return OBJ_WEAPONS;
    case '(':
    case 0x27b9: // ➹
        return OBJ_MISSILES;
    case '[':
        return OBJ_ARMOUR;
    case '/':
        return OBJ_WANDS;
    case '%':
        return OBJ_FOOD;
    case '?':
        return OBJ_SCROLLS;
    case '"': // Make the amulet symbol equiv to ring -- bwross
    case '=':
    case 0x00B0: // °
        return OBJ_JEWELLERY;
    case '!':
        return OBJ_POTIONS;
    case ':':
    case '+': // ??? -- was the only symbol working for tile order up to 0.10,
              // so keeping it for compat purposes (user configs).
    case 0x221e: // ∞
        return OBJ_BOOKS;
    case '|':
        return OBJ_STAVES;
    case '0':
        return OBJ_ORBS;
    case '}':
        return OBJ_MISCELLANY;
    case '&':
    case 'X':
    case 'x':
        return OBJ_CORPSES;
    case '$':
    case 0x20ac: // €
    case 0x00a3: // £
    case 0x00a5: // ¥
        return OBJ_GOLD;
    case '\\': // Compat break: used to be staves (why not '|'?).
        return OBJ_RODS;
    default:
        return NUM_OBJECT_CLASSES;
    }
}

template<class A, class B> static void _merge_lists(A &dest, const B &src,
                                                    bool prepend)
{
    dest.insert(prepend ? dest.begin() : dest.end(), src.begin(), src.end());
}

// Returns MSGCOL_NONE if unmatched else returns 0-15.
static msg_colour_type _str_to_channel_colour(const string &str)
{
    int col = str_to_colour(str);
    msg_colour_type ret = MSGCOL_NONE;
    if (col == -1)
    {
        if (str == "mute")
            ret = MSGCOL_MUTED;
        else if (str == "plain" || str == "off")
            ret = MSGCOL_PLAIN;
        else if (str == "default" || str == "on")
            ret = MSGCOL_DEFAULT;
        else if (str == "alternate")
            ret = MSGCOL_ALTERNATE;
    }
    else
        ret = msg_colour(col);

    return ret;
}

static const string message_channel_names[] =
{
    "plain", "friend_action", "prompt", "god", "pray", "duration", "danger",
    "warning", "food", "recovery", "sound", "talk", "talk_visual",
    "intrinsic_gain", "mutation", "monster_spell", "monster_enchant",
    "friend_spell", "friend_enchant", "monster_damage", "monster_target",
    "banishment", "rotten_meat", "equipment", "floor", "multiturn", "examine",
    "examine_filter", "diagnostic", "error", "tutorial", "orb", "timed_portal",
    "hell_effect", "monster_warning", "dgl_message",
};

// returns -1 if unmatched else returns 0--(NUM_MESSAGE_CHANNELS-1)
int str_to_channel(const string &str)
{
    COMPILE_CHECK(ARRAYSZ(message_channel_names) == NUM_MESSAGE_CHANNELS);

    // widespread aliases
    if (str == "visual")
        return MSGCH_TALK_VISUAL;
    else if (str == "spell")
        return MSGCH_MONSTER_SPELL;

    for (int ret = 0; ret < NUM_MESSAGE_CHANNELS; ret++)
    {
        if (str == message_channel_names[ret])
            return ret;
    }

    return -1;
}

string channel_to_str(int channel)
{
    if (channel < 0 || channel >= NUM_MESSAGE_CHANNELS)
        return "";

    return message_channel_names[channel];
}


// The map used to interpret a crawlrc entry as a starting weapon
// type. For most entries, we can just look up which weapon has the entry as
// its name; this map contains the exceptions.
// This should be const, but operator[] on maps isn't const.
static map<string, weapon_type> _special_weapon_map = {

    // "staff" normally refers to a magical staff, but here we want to
    // interpret it as a quarterstaff.
    {"staff",       WPN_QUARTERSTAFF},

    // These weapons' base names have changed; we want to interpret the old
    // names correctly.
    {"sling",       WPN_HUNTING_SLING},
    {"crossbow",    WPN_HAND_CROSSBOW},

    // Pseudo-weapons.
    {"unarmed",     WPN_UNARMED},
    {"claws",       WPN_UNARMED},

    {"thrown",      WPN_THROWN},
    {"rocks",       WPN_THROWN},
    {"tomahawks",   WPN_THROWN},
    {"javelins",    WPN_THROWN},

    {"random",      WPN_RANDOM},

    {"viable",      WPN_VIABLE},
};

/**
 * Interpret a crawlrc entry as a starting weapon type.
 *
 * @param str   The value of the crawlrc entry.
 * @return      The weapon the string refers to, or WPN_UNKNOWN if invalid
 */
weapon_type str_to_weapon(const string &str)
{
    string str_nospace = str;
    remove_whitespace(str_nospace);

    // Synonyms and pseudo-weapons.
    if (_special_weapon_map.count(str_nospace))
        return _special_weapon_map[str_nospace];

    // Real weapons referred to by their standard names.
    return name_nospace_to_weapon(str_nospace);
}

static string _weapon_to_str(weapon_type wpn_type)
{
    if (wpn_type >= 0 && wpn_type < NUM_WEAPONS)
        return weapon_base_name(wpn_type);

    switch (wpn_type)
    {
    case WPN_UNARMED:
        return "claws";
    case WPN_THROWN:
        return "thrown";
    case WPN_VIABLE:
        return "viable";
    case WPN_RANDOM:
    default:
        return "random";
    }
}

// Summon types can be any of mon_summon_type (enum.h), or a relevant summoning
// spell.
int str_to_summon_type(const string &str)
{
    if (str == "clone")
        return MON_SUMM_CLONE;
    if (str == "animate")
        return MON_SUMM_ANIMATE;
    if (str == "chaos")
        return MON_SUMM_CHAOS;
    if (str == "miscast")
        return MON_SUMM_MISCAST;
    if (str == "zot")
        return MON_SUMM_ZOT;
    if (str == "wrath")
        return MON_SUMM_WRATH;
    if (str == "aid")
        return MON_SUMM_AID;
    if (str == "lantern")
        return MON_SUMM_LANTERN;

    return spell_by_name(str);
}

static fire_type _str_to_fire_types(const string &str)
{
    if (str == "launcher")
        return FIRE_LAUNCHER;
    else if (str == "stone")
        return FIRE_STONE;
    else if (str == "rock")
        return FIRE_ROCK;
    else if (str == "javelin")
        return FIRE_JAVELIN;
    else if (str == "tomahawk")
        return FIRE_TOMAHAWK;
    else if (str == "net")
        return FIRE_NET;
    else if (str == "return" || str == "returning")
        return FIRE_RETURNING;
    else if (str == "inscribed")
        return FIRE_INSCRIBED;

    return FIRE_NONE;
}

string gametype_to_str(game_type type)
{
    switch (type)
    {
    case GAME_TYPE_NORMAL:
        return "normal";
    case GAME_TYPE_TUTORIAL:
        return "tutorial";
    case GAME_TYPE_ARENA:
        return "arena";
    case GAME_TYPE_SPRINT:
        return "sprint";
    case GAME_TYPE_HINTS:
        return "hints";
    default:
        return "none";
    }
}

#ifndef DGAMELAUNCH
static game_type _str_to_gametype(const string& s)
{
    for (int i = 0; i < NUM_GAME_TYPE; ++i)
    {
        game_type t = static_cast<game_type>(i);
        if (s == gametype_to_str(t))
            return t;
    }
    return NUM_GAME_TYPE;
}
#endif

static string _species_to_str(species_type sp)
{
    if (sp == SP_RANDOM)
        return "random";
    else if (sp == SP_VIABLE)
        return "viable";
    else
        return species_name(sp);
}

static species_type _str_to_species(const string &str)
{
    if (str == "random")
        return SP_RANDOM;
    else if (str == "viable")
        return SP_VIABLE;

    species_type ret = SP_UNKNOWN;
    if (str.length() == 2) // scan abbreviations
        ret = get_species_by_abbrev(str.c_str());

    // if we don't have a match, scan the full names
    if (ret == SP_UNKNOWN)
        ret = str_to_species(str);

    if (!is_starting_species(ret))
        ret = SP_UNKNOWN;

    if (ret == SP_UNKNOWN)
        fprintf(stderr, "Unknown species choice: %s\n", str.c_str());

    return ret;
}

static string _job_to_str(job_type job)
{
    if (job == JOB_RANDOM)
        return "random";
    else if (job == JOB_VIABLE)
        return "viable";
    else
        return get_job_name(job);
}

job_type str_to_job(const string &str)
{
    if (str == "random")
        return JOB_RANDOM;
    else if (str == "viable")
        return JOB_VIABLE;

    job_type job = JOB_UNKNOWN;

    if (str.length() == 2) // scan abbreviations
        job = get_job_by_abbrev(str.c_str());

    // if we don't have a match, scan the full names
    if (job == JOB_UNKNOWN)
        job = get_job_by_name(str.c_str());

    if (!is_starting_job(job))
        job = JOB_UNKNOWN;

    if (job == JOB_UNKNOWN)
        fprintf(stderr, "Unknown background choice: %s\n", str.c_str());

    return job;
}

static bool _read_bool(const string &field, bool def_value)
{
    bool ret = def_value;

    if (field == "true" || field == "1" || field == "yes")
        ret = true;

    if (field == "false" || field == "0" || field == "no")
        ret = false;

    return ret;
}

// read a value which can be either a boolean (in which case return
// 0 for true, -1 for false), or a string of the form PREFIX:NUMBER
// (e.g., auto:7), in which case return NUMBER as an int.
static int _read_bool_or_number(const string &field, int def_value,
                                const string& num_prefix)
{
    int ret = def_value;

    if (field == "true" || field == "1" || field == "yes")
        ret = 0;

    if (field == "false" || field == "0" || field == "no")
        ret = -1;

    if (starts_with(field, num_prefix))
        ret = atoi(field.c_str() + num_prefix.size());

    return ret;
}

static unsigned curses_attribute(const string &field)
{
    if (field == "standout")               // probably reverses
        return CHATTR_STANDOUT;
    else if (field == "bold")              // probably brightens fg
        return CHATTR_BOLD;
    else if (field == "blink")             // probably brightens bg
        return CHATTR_BLINK;
    else if (field == "underline")
        return CHATTR_UNDERLINE;
    else if (field == "reverse")
        return CHATTR_REVERSE;
    else if (field == "dim")
        return CHATTR_DIM;
    else if (starts_with(field, "hi:")
             || starts_with(field, "hilite:")
             || starts_with(field, "highlight:"))
    {
        int col = field.find(":");
        int colour = str_to_colour(field.substr(col + 1));
        if (colour == -1)
            Options.report_error("Bad highlight string -- %s\n", field.c_str());
        else
            return CHATTR_HILITE | (colour << 8);
    }
    else if (field != "none")
        Options.report_error("Bad colour -- %s\n", field.c_str());
    return CHATTR_NORMAL;
}

void game_options::str_to_enemy_hp_colour(const string &colours, bool prepend)
{
    vector<string> colour_list = split_string(" ", colours, true, true);
    if (prepend)
        reverse(colour_list.begin(), colour_list.end());
    for (const string &colstr : colour_list)
    {
        const int col = str_to_colour(colstr);
        if (col < 0)
        {
            Options.report_error("Bad enemy_hp_colour: %s\n", colstr.c_str());
            return;
        }
        else if (prepend)
            enemy_hp_colour.insert(enemy_hp_colour.begin(), col);
        else
            enemy_hp_colour.push_back(col);
    }
}

#ifdef USE_TILE
static FixedVector<const char*, TAGPREF_MAX>
    tag_prefs("none", "tutorial", "named", "enemy");

static tag_pref _str_to_tag_pref(const char *opt)
{
    for (int i = 0; i < TAGPREF_MAX; i++)
    {
        if (!strcasecmp(opt, tag_prefs[i]))
            return (tag_pref)i;
    }

    return TAGPREF_ENEMY;
}
#endif

void game_options::new_dump_fields(const string &text, bool add, bool prepend)
{
    // Easy; chardump.cc has most of the intelligence.
    vector<string> fields = split_string(",", text, true, true);
    if (add)
        _merge_lists(dump_order, fields, prepend);
    else
    {
        for (const string &field : fields)
            erase_val(dump_order, field);
    }
}

void game_options::set_default_activity_interrupts()
{
    for (int adelay = 0; adelay < NUM_DELAYS; ++adelay)
        for (int aint = 0; aint < NUM_AINTERRUPTS; ++aint)
        {
            activity_interrupts[adelay].set(aint,
                is_delay_interruptible(static_cast<delay_type>(adelay)));
        }

    const char *default_activity_interrupts[] =
    {
        "interrupt_armour_on = hp_loss, monster_attack, monster, mimic",
        "interrupt_armour_off = interrupt_armour_on",
        "interrupt_drop_item = interrupt_armour_on",
        "interrupt_jewellery_on = interrupt_armour_on",
        "interrupt_memorise = hp_loss, monster_attack, stat",
        "interrupt_butcher = interrupt_armour_on, teleport, stat",
        "interrupt_bottle_blood = interrupt_butcher",
        "interrupt_vampire_feed = interrupt_butcher",
        "interrupt_multidrop = hp_loss, monster_attack, teleport, stat",
        "interrupt_macro = interrupt_multidrop",
        "interrupt_travel = interrupt_butcher, statue, hungry, hit_monster, "
                            "sense_monster",
        "interrupt_run = interrupt_travel, message",
        "interrupt_rest = interrupt_run, full_hp, full_mp",

        // Stair ascents/descents cannot be interrupted except by
        // teleportation. Attempts to interrupt the delay will just
        // trash all queued delays, including travel.
        "interrupt_ascending_stairs = teleport",
        "interrupt_descending_stairs = teleport",
        "interrupt_uninterruptible =",
        "interrupt_weapon_swap =",

        nullptr
    };

    for (int i = 0; default_activity_interrupts[i]; ++i)
        read_option_line(default_activity_interrupts[i], false);
}

void game_options::set_activity_interrupt(
        FixedBitVector<NUM_AINTERRUPTS> &eints,
        const string &interrupt)
{
    if (starts_with(interrupt, interrupt_prefix))
    {
        string delay_name = interrupt.substr(interrupt_prefix.length());
        delay_type delay = get_delay(delay_name);
        if (delay == NUM_DELAYS)
            return report_error("Unknown delay: %s\n", delay_name.c_str());

        FixedBitVector<NUM_AINTERRUPTS> &refints =
            activity_interrupts[delay];

        eints |= refints;
        return;
    }

    activity_interrupt_type ai = get_activity_interrupt(interrupt);
    if (ai == NUM_AINTERRUPTS)
    {
        return report_error("Delay interrupt name \"%s\" not recognised.\n",
                            interrupt.c_str());
    }

    eints.set(ai);
}

void game_options::set_activity_interrupt(const string &activity_name,
                                          const string &interrupt_names,
                                          bool append_interrupts,
                                          bool remove_interrupts)
{
    const delay_type delay = get_delay(activity_name);
    if (delay == NUM_DELAYS)
        return report_error("Unknown delay: %s\n", activity_name.c_str());

    vector<string> interrupts = split_string(",", interrupt_names);
    FixedBitVector<NUM_AINTERRUPTS> &eints = activity_interrupts[ delay ];

    if (remove_interrupts)
    {
        FixedBitVector<NUM_AINTERRUPTS> refints;
        for (const string &interrupt : interrupts)
            set_activity_interrupt(refints, interrupt);

        for (int i = 0; i < NUM_AINTERRUPTS; ++i)
            if (refints[i])
                eints.set(i, false);
    }
    else
    {
        if (!append_interrupts)
            eints.reset();

        for (const string &interrupt : interrupts)
            set_activity_interrupt(eints, interrupt);
    }

    eints.set(AI_FORCE_INTERRUPT);
}

#if defined(DGAMELAUNCH)
static string _resolve_dir(const char* path, const char* suffix)
{
    return catpath(path, "");
}
#else

static string _user_home_dir()
{
#ifdef TARGET_OS_WINDOWS
    wchar_t home[MAX_PATH];
    if (SHGetFolderPathW(0, CSIDL_APPDATA, 0, 0, home))
        return "./";
    else
        return utf16_to_8(home);
#else
    const char *home = getenv("HOME");
    if (!home || !*home)
        return "./";
    else
        return mb_to_utf8(home);
#endif
}

static string _user_home_subpath(const string &subpath)
{
    return catpath(_user_home_dir(), subpath);
}

static string _resolve_dir(const char* path, const char* suffix)
{
    if (path[0] != '~')
        return catpath(string(path), suffix);
    else
        return _user_home_subpath(catpath(path + 1, suffix));
}
#endif

void game_options::reset_options()
{
    filename     = "unknown";
    basefilename = "unknown";
    line_num     = -1;

    set_default_activity_interrupts();

#ifdef DEBUG_DIAGNOSTICS
    quiet_debug_messages.reset();
#ifdef DEBUG_MONSPEAK
    quiet_debug_messages.set(DIAG_SPEECH);
#endif
# ifdef DEBUG_MONINDEX
    quiet_debug_messages.set(DIAG_MONINDEX);
# endif
#endif

#if defined(USE_TILE_LOCAL)
    restart_after_game = true;
#else
    restart_after_game = false;
#endif
    restart_after_save = false;

    macro_dir = SysEnv.macro_dir;

#if !defined(DGAMELAUNCH)
    if (macro_dir.empty())
    {
#ifdef UNIX
        macro_dir = _user_home_subpath(".crawl");
#else
        macro_dir = "settings/";
#endif
    }
#endif

#if defined(TARGET_OS_MACOSX)
    UNUSED(_resolve_dir);
    const string tmp_path_base =
        _user_home_subpath("Library/Application Support/" CRAWL);
    save_dir   = tmp_path_base + "/saves/";
    morgue_dir = tmp_path_base + "/morgue/";
    if (SysEnv.macro_dir.empty())
        macro_dir  = tmp_path_base;
#else
    save_dir   = _resolve_dir(SysEnv.crawl_dir.c_str(), "saves/");
    morgue_dir = _resolve_dir(SysEnv.crawl_dir.c_str(), "morgue/");
#endif

#if defined(SHARED_DIR_PATH)
    shared_dir = _resolve_dir(SHARED_DIR_PATH, "");
#else
    shared_dir = save_dir;
#endif

    additional_macro_files.clear();

#ifdef DGL_SIMPLE_MESSAGING
    messaging = true;
#endif

    mouse_input = false;

    view_max_width   = max(VIEW_BASE_WIDTH, VIEW_MIN_WIDTH);
    view_max_height  = max(21, VIEW_MIN_HEIGHT);
    mlist_min_height = 4;
    msg_min_height   = max(7, MSG_MIN_HEIGHT);
    msg_max_height   = max(10, MSG_MIN_HEIGHT);
    mlist_allow_alternate_layout = false;
    messages_at_top  = false;
    mlist_targeting  = false;
    msg_condense_repeats = true;
    msg_condense_short = true;

    view_lock_x = true;
    view_lock_y = true;

    center_on_scroll = false;
    symmetric_scroll = true;
    scroll_margin_x  = 2;
    scroll_margin_y  = 2;
    always_show_exclusions = true;

    autopickup_on    = 1;
    autopickup_starting_ammo = true;
    default_manual_training = false;
    default_show_all_skills = false;

    show_newturn_mark = true;
    show_game_turns = true;

    game = newgame_def();

    remember_name = true;

    char_set      = CSET_DEFAULT;

    // set it to the .crawlrc default
    autopickups.reset();
    autopickups.set(OBJ_GOLD);
    autopickups.set(OBJ_SCROLLS);
    autopickups.set(OBJ_POTIONS);
    autopickups.set(OBJ_BOOKS);
    autopickups.set(OBJ_JEWELLERY);
    autopickups.set(OBJ_WANDS);
    autopickups.set(OBJ_FOOD);
    autopickups.set(OBJ_RODS);
    auto_switch             = false;
    suppress_startup_errors = false;

    show_uncursed          = true;
    travel_open_doors      = true;
    easy_unequip           = true;
    equip_unequip          = false;
    jewellery_prompt       = false;
    easy_door              = true;
    warn_hatches           = false;
    enable_recast_spell    = true;
    confirm_butcher        = CONFIRM_AUTO;
    easy_eat_chunks        = false;
    auto_eat_chunks        = true;
    easy_confirm           = CONFIRM_SAFE_EASY;
    easy_quit_item_prompts = true;
    allow_self_target      = CONFIRM_PROMPT;
    hp_warning             = 30;
    magic_point_warning    = 0;
    skill_focus            = SKM_FOCUS_ON;
    cloud_status           = !is_tiles();

    user_note_prefix       = "";
    note_all_skill_levels  = false;
    note_skill_max         = true;
    note_xom_effects       = true;
    note_chat_messages     = false;
    note_dgl_messages      = true;
    note_hp_percent        = 5;

    fail_severity_to_confirm = 3;

    clear_messages         = false;
#ifdef TOUCH_UI
    show_more              = false;
#else
    show_more              = true;
#endif
    small_more             = false;

    pickup_thrown          = true;

#ifdef DGAMELAUNCH
    travel_delay           = -1;
    explore_delay          = -1;
    rest_delay             = -1;
    show_travel_trail       = true;
#else
    travel_delay           = 20;
    explore_delay          = -1;
    rest_delay             = 0;
    show_travel_trail       = false;
#endif

    travel_stair_cost      = 500;

    view_delay             = DEFAULT_VIEW_DELAY;

    arena_dump_msgs        = false;
    arena_dump_msgs_all    = false;
    arena_list_eq          = false;

    // Sort only pickup menus by default.
    sort_menus.clear();
    set_menu_sort("pickup: true");

    tc_reachable           = BLUE;
    tc_excluded            = LIGHTMAGENTA;
    tc_exclude_circle      = RED;
    tc_dangerous           = CYAN;
    tc_disconnected        = DARKGREY;

    show_waypoints         = true;

    background_colour      = BLACK;
    // [ds] Default to jazzy colours.
    detected_item_colour   = GREEN;
    detected_monster_colour = LIGHTRED;
    remembered_monster_colour = DARKGREY;
    status_caption_colour  = BROWN;

    easy_exit_menu         = false;
    ability_menu           = true;
    dos_use_background_intensity = true;

    assign_item_slot       = SS_FORWARD;
    show_god_gift          = MB_MAYBE;

    // 10 was the cursor step default on Linux.
    level_map_cursor_step  = 7;
#ifdef UNIX
    use_fake_cursor        = true;
#else
    use_fake_cursor        = false;
#endif
    use_fake_player_cursor = true;
    show_player_species    = false;
    explore_stop           = (ES_ITEM | ES_STAIR | ES_PORTAL | ES_BRANCH
                              | ES_SHOP | ES_ALTAR | ES_RUNED_DOOR
                              | ES_GREEDY_PICKUP_SMART
                              | ES_GREEDY_VISITED_ITEM_STACK
                              | ES_GREEDY_SACRIFICEABLE);

    // The prompt conditions will be combined into explore_stop after
    // reading options.
    explore_stop_prompt    = ES_NONE;

    explore_stop_pickup_ignore.clear();

    explore_item_greed     = 10;
    explore_greedy         = true;

    explore_wall_bias      = 0;
    explore_auto_rest      = false;
    explore_improved       = false;
    travel_key_stop        = true;
    auto_sacrifice         = AS_NO;

    dump_on_save           = true;
    dump_kill_places       = KDO_ONE_PLACE;
    dump_message_count     = 20;
    dump_item_origins      = IODS_ARTEFACTS | IODS_RODS;
    dump_item_origin_price = -1;
    dump_book_spells       = true;

    pickup_menu_limit      = 1;

    flush_input[ FLUSH_ON_FAILURE ]     = true;
    flush_input[ FLUSH_BEFORE_COMMAND ] = false;
    flush_input[ FLUSH_ON_MESSAGE ]     = false;
    flush_input[ FLUSH_LUA ]            = true;

    fire_items_start       = 0;           // start at slot 'a'

    // Clear fire_order and set up the defaults.
    set_fire_order("launcher, return, "
                   "javelin / tomahawk / stone / rock / net, "
                   "inscribed",
                   false, false);

    item_stack_summary_minimum = 4;

    pizzas.clear();

    regex_search = false;

#ifdef WIZARD
    fsim_rounds = 4000L;
    fsim_csv    = false;
    fsim_mons   = "";
    fsim_scale.clear();
    fsim_kit.clear();
#endif

    // These are only used internally, and only from the commandline:
    // XXX: These need a better place.
    sc_entries             = 0;
    sc_format              = -1;

    friend_brand       = CHATTR_HILITE | (GREEN << 8);
    neutral_brand      = CHATTR_HILITE | (LIGHTGREY << 8);
    stab_brand         = CHATTR_HILITE | (BLUE << 8);
    may_stab_brand     = CHATTR_HILITE | (YELLOW << 8);
    heap_brand         = CHATTR_REVERSE;
    feature_item_brand = CHATTR_REVERSE;
    trap_item_brand    = CHATTR_REVERSE;

    no_dark_brand      = true;

#ifdef WIZARD
#ifdef DGAMELAUNCH
    if (wiz_mode != WIZ_NO)
    {
        wiz_mode         = WIZ_NEVER;
        explore_mode     = WIZ_NEVER;
    }
#else
    wiz_mode             = WIZ_NO;
    explore_mode         = WIZ_NO;
#endif
#endif
    terp_files.clear();

#ifdef USE_TILE
    tile_show_items      = "!?/%=([)x}:|\\";
    tile_skip_title      = false;
    tile_menu_icons      = true;

    // minimap colours
    tile_player_col       = str_to_tile_colour("white");
    tile_monster_col      = str_to_tile_colour("#660000");
    tile_plant_col        = str_to_tile_colour("#446633");
    tile_item_col         = str_to_tile_colour("#005544");
    tile_unseen_col       = str_to_tile_colour("black");
    tile_floor_col        = str_to_tile_colour("#333333");
    tile_wall_col         = str_to_tile_colour("#666666");
    tile_mapped_floor_col = str_to_tile_colour("#222266");
    tile_mapped_wall_col  = str_to_tile_colour("#444499");
    tile_door_col         = str_to_tile_colour("#775544");
    tile_downstairs_col   = str_to_tile_colour("#ff00ff");
    tile_upstairs_col     = str_to_tile_colour("cyan");
    tile_branchstairs_col = str_to_tile_colour("#ff7788");
    tile_portal_col       = str_to_tile_colour("#ffdd00");
    tile_feature_col      = str_to_tile_colour("#997700");
    tile_trap_col         = str_to_tile_colour("#aa6644");
    tile_water_col        = str_to_tile_colour("#114455");
    tile_deep_water_col   = str_to_tile_colour("#001122");
    tile_lava_col         = str_to_tile_colour("#552211");
    tile_excluded_col     = str_to_tile_colour("#552266");
    tile_excl_centre_col  = str_to_tile_colour("#552266");
    tile_window_col       = str_to_tile_colour("#558855");
#endif

#ifdef USE_TILE_LOCAL
    // font selection
    tile_font_crt_file   = MONOSPACED_FONT;
    tile_font_stat_file  = MONOSPACED_FONT;
    tile_font_msg_file   = MONOSPACED_FONT;
    tile_font_tip_file   = MONOSPACED_FONT;
    tile_font_lbl_file   = PROPORTIONAL_FONT;
#endif
#ifdef USE_TILE_WEB
    tile_font_crt_family  = "monospace";
    tile_font_stat_family = "monospace";
    tile_font_msg_family  = "monospace";
    tile_font_lbl_family  = "monospace";
#endif

#ifdef USE_TILE
    tile_font_crt_size   = 0;
    tile_font_stat_size  = 0;
    tile_font_msg_size   = 0;
    tile_font_tip_size   = 0;
    tile_font_lbl_size   = 0;
#endif
#ifdef USE_TILE_LOCAL
#ifdef USE_FT
    // TODO: init this from system settings. This would probably require
    // using fontconfig, but that's planned.
    tile_font_ft_light   = false;
#endif

    // window layout
    tile_full_screen      = SCREENMODE_AUTO;
    tile_window_width     = -90;
    tile_window_height    = -90;
# ifdef TOUCH_UI
    tile_layout_priority = split_string(",", "minimap, command, gold_turn, "
                                             "inventory, command2, spell, "
                                             "ability, monster");
# else
    tile_layout_priority = split_string(",", "minimap, inventory, gold_turn, "
                                             "command, spell, ability, "
                                             "monster");
# endif
    tile_use_small_layout = MB_MAYBE;
#endif

#ifdef USE_TILE
    tile_cell_pixels      = 32;
    tile_filter_scaling   = false;
    tile_map_pixels       = 0;
    tile_force_overlay    = false;
    // delays
    tile_update_rate      = 1000;
    tile_runrest_rate     = 100;
    tile_key_repeat_delay = 200;
    tile_tooltip_ms       = 500;
    // XXX: arena may now be chosen after options are read.
    tile_tag_pref         = crawl_state.game_is_arena() ? TAGPREF_NAMED
                                                        : TAGPREF_ENEMY;

    tile_show_minihealthbar  = true;
    tile_show_minimagicbar   = true;
    tile_show_demon_tier     = true;
#ifdef USE_TILE_WEB
    // disabled by default due to performance issues
    tile_water_anim          = false;
#else
    tile_water_anim          = true;
#endif
    tile_misc_anim           = true;
    tile_use_monster         = MONS_PROGRAM_BUG;
    tile_player_tile         = 0;
    tile_weapon_offsets.first  = INT_MAX;
    tile_weapon_offsets.second = INT_MAX;
    tile_shield_offsets.first  = INT_MAX;
    tile_shield_offsets.second = INT_MAX;
#endif

#ifdef USE_TILE_WEB
    tile_realtime_anim = false;
    tile_display_mode = "tiles";
    tile_level_map_hide_messages = true;
    tile_level_map_hide_sidebar = false;
#endif

    // map each colour to itself as default
    for (int i = 0; i < (int)ARRAYSZ(colour); ++i)
        colour[i] = i;

    // map each channel to plain (well, default for now since I'm testing)
    for (int i = 0; i < NUM_MESSAGE_CHANNELS; ++i)
        channels[i] = MSGCOL_DEFAULT;

    // Clear vector options.
    dump_order.clear();
    new_dump_fields("header,hiscore,stats,misc,inventory,"
                    "skills,spells,overview,mutations,messages,"
                    "screenshot,monlist,kills,notes");
    if (Version::ReleaseType == VER_ALPHA)
        new_dump_fields("vaults");
    new_dump_fields("action_counts");

    use_animations = (UA_BEAM | UA_RANGE | UA_HP | UA_MONSTER_IN_SIGHT
                      | UA_PICKUP | UA_MONSTER | UA_PLAYER | UA_BRANCH_ENTRY
                      | UA_ALWAYS_ON);

    hp_colour.clear();
    hp_colour.emplace_back(50, YELLOW);
    hp_colour.emplace_back(25, RED);
    mp_colour.clear();
    mp_colour.emplace_back(50, YELLOW);
    mp_colour.emplace_back(25, RED);
    stat_colour.clear();
    stat_colour.emplace_back(3, RED);
    enemy_hp_colour.clear();
    // I think these defaults are pretty ugly but apparently OS X has problems
    // with lighter colours
    enemy_hp_colour.push_back(GREEN);
    enemy_hp_colour.push_back(GREEN);
    enemy_hp_colour.push_back(BROWN);
    enemy_hp_colour.push_back(BROWN);
    enemy_hp_colour.push_back(MAGENTA);
    enemy_hp_colour.push_back(RED);
    enemy_hp_colour.push_back(LIGHTGREY);
    visual_monster_hp = false;

    force_autopickup.clear();
    note_monsters.clear();
    note_messages.clear();
    autoinscriptions.clear();
    note_items.clear();
    note_skill_levels.reset();
    note_skill_levels.set(1);
    note_skill_levels.set(5);
    note_skill_levels.set(10);
    note_skill_levels.set(15);
    note_skill_levels.set(27);
    auto_spell_letters.clear();
    auto_item_letters.clear();
    auto_ability_letters.clear();
    force_more_message.clear();
    flash_screen_message.clear();
    confirm_action.clear();
    sound_mappings.clear();
    menu_colour_mappings.clear();
    message_colour_mappings.clear();
    drop_filter.clear();
    map_file_name.clear();
    named_options.clear();

    clear_cset_overrides();

    clear_feature_overrides();
    mon_glyph_overrides.clear();
    item_glyph_overrides.clear();
    item_glyph_cache.clear();

    rest_wait_both = false;
    rest_wait_percent = 100;

    // Map each category to itself. The user can override in init.txt
    kill_map[KC_YOU] = KC_YOU;
    kill_map[KC_FRIENDLY] = KC_FRIENDLY;
    kill_map[KC_OTHER] = KC_OTHER;

    // Forget any files we remembered as included.
    included.clear();

    // Forget variables and such.
    aliases.clear();
    variables.clear();
    constants.clear();
}

void game_options::clear_cset_overrides()
{
    memset(cset_override, 0, sizeof cset_override);
}

void game_options::clear_feature_overrides()
{
    feature_colour_overrides.clear();
    feature_symbol_overrides.clear();
}

ucs_t get_glyph_override(int c)
{
    if (c < 0)
        c = -c;
    if (wcwidth(c) != 1)
    {
        mprf(MSGCH_ERROR, "Invalid glyph override: %X", c);
        c = 0;
    }
    return c;
}

static int read_symbol(string s)
{
    if (s.empty())
        return 0;

    if (s.length() > 1 && s[0] == '\\')
        s = s.substr(1);

    {
        ucs_t c;
        const char *nc = s.c_str();
        nc += utf8towc(&c, nc);
        // no control, combining or CJK characters, please
        if (!*nc && wcwidth(c) == 1)
            return c;
    }

    int base = 10;
    if (s.length() > 1 && s[0] == 'x')
    {
        s = s.substr(1);
        base = 16;
    }

    char *tail;
    return strtoul(s.c_str(), &tail, base);
}

void game_options::set_fire_order(const string &s, bool append, bool prepend)
{
    if (!append && !prepend)
        fire_order.clear();
    vector<string> slots = split_string(",", s);
    if (prepend)
        reverse(slots.begin(), slots.end());
    for (const string &slot : slots)
        add_fire_order_slot(slot, prepend);
}

void game_options::add_fire_order_slot(const string &s, bool prepend)
{
    unsigned flags = 0;
    for (const string &alt : split_string("/", s))
        flags |= _str_to_fire_types(alt);

    if (flags)
    {
        if (prepend)
            fire_order.insert(fire_order.begin(), flags);
        else
            fire_order.push_back(flags);
    }
}

static monster_type _mons_class_by_string(const string &name)
{
    const string match = lowercase_string(name);
    for (monster_type i = MONS_0; i < NUM_MONSTERS; ++i)
    {
        const monsterentry *me = get_monster_data(i);
        if (!me || me->mc == MONS_PROGRAM_BUG)
            continue;

        if (lowercase_string(me->name) == match)
            return i;
    }
    return MONS_0;
}

static set<monster_type> _mons_classes_by_glyph(const char letter)
{
    set<monster_type> matches;
    for (monster_type i = MONS_0; i < NUM_MONSTERS; ++i)
    {
        const monsterentry *me = get_monster_data(i);
        if (!me || me->mc == MONS_PROGRAM_BUG)
            continue;

        if (me->basechar == letter)
            matches.insert(i);
    }
    return matches;
}

cglyph_t game_options::parse_mon_glyph(const string &s) const
{
    cglyph_t md;
    md.col = 0;
    vector<string> phrases = split_string(" ", s);
    for (const string &p : phrases)
    {
        const int col = str_to_colour(p, -1, false);
        if (col != -1)
            md.col = col;
        else
            md.ch = p == "_"? ' ' : read_symbol(p);
    }
    return md;
}

void game_options::remove_mon_glyph_override(const string &text, bool prepend)
{
    vector<string> override = split_string(":", text);

    set<monster_type> matches;
    if (override[0].length() == 1)
        matches = _mons_classes_by_glyph(override[0][0]);
    else
    {
        const monster_type m = _mons_class_by_string(override[0]);
        if (m == MONS_0)
        {
            report_error("Unknown monster: \"%s\"", text.c_str());
            return;
        }
        matches.insert(m);
    }
    for (monster_type m : matches)
        mon_glyph_overrides.erase(m);;
}

void game_options::add_mon_glyph_override(const string &text, bool prepend)
{
    vector<string> override = split_string(":", text);
    if (override.size() != 2u)
        return;

    set<monster_type> matches;
    if (override[0].length() == 1)
        matches = _mons_classes_by_glyph(override[0][0]);
    else
    {
        const monster_type m = _mons_class_by_string(override[0]);
        if (m == MONS_0)
        {
            report_error("Unknown monster: \"%s\"", text.c_str());
            return;
        }
        matches.insert(m);
    }

    cglyph_t mdisp;

    // Look for monsters first so that "blue devil" works right.
    const monster_type n = _mons_class_by_string(override[1]);
    if (n != MONS_0)
    {
        const monsterentry *me = get_monster_data(n);
        mdisp.ch = me->basechar;
        mdisp.col = me->colour;
    }
    else
        mdisp = parse_mon_glyph(override[1]);

    if (mdisp.ch || mdisp.col)
        for (monster_type m : matches)
            mon_glyph_overrides[m] = mdisp;
}

void game_options::remove_item_glyph_override(const string &text, bool prepend)
{
    string key = text;
    trim_string(key);

    erase_if(item_glyph_overrides,
             [&key](const item_glyph_override_type& arg)
             { return key == arg.first; });
}

void game_options::add_item_glyph_override(const string &text, bool prepend)
{
    vector<string> override = split_string(":", text);
    if (override.size() != 2u)
        return;

    cglyph_t mdisp = parse_mon_glyph(override[1]);
    if (mdisp.ch || mdisp.col)
    {
        if (prepend)
        {
            item_glyph_overrides.emplace(item_glyph_overrides.begin(),
                                               override[0],mdisp);
        }
        else
            item_glyph_overrides.emplace_back(override[0], mdisp);
    }
}

void game_options::remove_feature_override(const string &text, bool prepend)
{
    string fname;
    string::size_type epos = text.rfind("}");
    if (epos != string::npos)
        fname = text.substr(0, text.rfind("{",epos));
    else
        fname = text;

    trim_string(fname);

    vector<dungeon_feature_type> feats = features_by_desc(text_pattern(fname));
    for (dungeon_feature_type f : feats)
    {
        feature_colour_overrides.erase(f);
        feature_symbol_overrides.erase(f);
    }
}

void game_options::add_feature_override(const string &text, bool prepend)
{
    string::size_type epos = text.rfind("}");
    if (epos == string::npos)
        return;

    string::size_type spos = text.rfind("{", epos);
    if (spos == string::npos)
        return;

    string fname = text.substr(0, spos);
    string props = text.substr(spos + 1, epos - spos - 1);
    vector<string> iprops = split_string(",", props, true, true);

    if (iprops.size() < 1 || iprops.size() > 7)
        return;

    if (iprops.size() < 7)
        iprops.resize(7);

    trim_string(fname);
    vector<dungeon_feature_type> feats = features_by_desc(text_pattern(fname));
    if (feats.empty())
        return;

    for (const dungeon_feature_type feat : feats)
    {
        if (feat >= NUM_FEATURES)
            continue; // TODO: handle other object types.

#define SYM(n, field) if (ucs_t s = read_symbol(iprops[n])) \
                          feature_symbol_overrides[feat][n] = s; \
                      else \
                          feature_symbol_overrides[feat][n] = '\0';
        SYM(0, symbol);
        SYM(1, magic_symbol);
#undef SYM
        feature_def &fov(feature_colour_overrides[feat]);
#define COL(n, field) if (colour_t c = str_to_colour(iprops[n], BLACK)) \
                          fov.field = c;
        COL(2, dcolour);
        COL(3, map_dcolour);
        COL(4, seen_dcolour);
        COL(5, em_dcolour);
        COL(6, seen_em_dcolour);
#undef COL
    }
}

void game_options::add_cset_override(dungeon_char_type dc, int symbol)
{
    cset_override[dc] = get_glyph_override(symbol);
}

string find_crawlrc()
{
    const char* locations_data[][2] =
    {
        { SysEnv.crawl_dir.c_str(), "init.txt" },
#ifdef UNIX
        { SysEnv.home.c_str(), ".crawl/init.txt" },
        { SysEnv.home.c_str(), ".crawlrc" },
        { SysEnv.home.c_str(), "init.txt" },
#endif
#ifndef DATA_DIR_PATH
        { "", "init.txt" },
        { "..", "init.txt" },
        { "../settings", "init.txt" },
#endif
        { nullptr, nullptr }                // placeholder to mark end
    };

    // We'll look for these files in any supplied -rcdirs.
    static const char *rc_dir_filenames[] =
    {
        ".crawlrc",
        "init.txt",
    };

    // -rc option always wins.
    if (!SysEnv.crawl_rc.empty())
        return SysEnv.crawl_rc;

    // If we have any rcdirs, look in them for files from the
    // rc_dir_names list.
    for (const string &rc_dir : SysEnv.rcdirs)
    {
        for (const string &rc_fn : rc_dir_filenames)
        {
            const string rc(catpath(rc_dir, rc_fn));
            if (file_exists(rc))
                return rc;
        }
    }

    // Check all possibilities for init.txt
    for (int i = 0; locations_data[i][1] != nullptr; ++i)
    {
        // Don't look at unset options
        if (locations_data[i][0] != nullptr)
        {
            const string rc = catpath(locations_data[i][0],
                                      locations_data[i][1]);
            if (file_exists(rc))
                return rc;
        }
    }

    // Last attempt: pick up init.txt from datafile_path, which will
    // also search the settings/ directory.
    return datafile_path("init.txt", false, false);
}

static const char* lua_builtins[] =
{
    "clua/stash.lua",
    "clua/runrest.lua",
    "clua/autofight.lua",
    "clua/automagic.lua",
    "clua/kills.lua",
};

static const char* config_defaults[] =
{
    "defaults/autopickup_exceptions.txt",
    "defaults/runrest_messages.txt",
    "defaults/standard_colours.txt",
    "defaults/food_colouring.txt",
    "defaults/menu_colours.txt",
    "defaults/glyph_colours.txt",
    "defaults/messages.txt",
    "defaults/misc.txt",
};

void read_init_file(bool runscript)
{
    Options.reset_options();

    // Load Lua builtins.
#ifdef CLUA_BINDINGS
    if (runscript)
    {
        for (const char *builtin : lua_builtins)
        {
            clua.execfile(builtin, false, false);
            if (!clua.error.empty())
                mprf(MSGCH_ERROR, "Lua error: %s", clua.error.c_str());
        }
    }

    // Load default options.
    for (const char *def_file : config_defaults)
        Options.include(datafile_path(def_file), false, runscript);
#else
    UNUSED(lua_builtins);
    UNUSED(config_defaults);
#endif

    // Load early binding extra options from the command line BEFORE init.txt.
    Options.filename     = "extra opts first";
    Options.basefilename = "extra opts first";
    Options.line_num     = 0;
    for (const string &extra : SysEnv.extra_opts_first)
    {
        Options.line_num++;
        Options.read_option_line(extra, true);
    }

    // Load init.txt.
    const string init_file_name(find_crawlrc());

    FileLineInput f(init_file_name.c_str());

    Options.filename = init_file_name;
    Options.line_num = 0;
#ifdef UNIX
    Options.basefilename = "~/.crawlrc";
#else
    Options.basefilename = "init.txt";
#endif

    if (f.error())
        return;
    Options.read_options(f, runscript);

    // Load late binding extra options from the command line AFTER init.txt.
    Options.filename     = "extra opts last";
    Options.basefilename = "extra opts last";
    Options.line_num     = 0;
    for (const string &extra : SysEnv.extra_opts_last)
    {
        Options.line_num++;
        Options.read_option_line(extra, true);
    }

    Options.filename     = init_file_name;
    Options.basefilename = get_base_filename(init_file_name);
    Options.line_num     = -1;
}

newgame_def read_startup_prefs()
{
#ifndef DISABLE_STICKY_STARTUP_OPTIONS
    FileLineInput fl(get_prefs_filename().c_str());
    if (fl.error())
        return newgame_def();

    game_options temp;
    temp.read_options(fl, false);

    if (!temp.game.allowed_species.empty())
        temp.game.species = temp.game.allowed_species[0];
    if (!temp.game.allowed_jobs.empty())
        temp.game.job = temp.game.allowed_jobs[0];
    if (!temp.game.allowed_weapons.empty())
        temp.game.weapon = temp.game.allowed_weapons[0];
    return temp.game;
#endif // !DISABLE_STICKY_STARTUP_OPTIONS
}

#ifndef DISABLE_STICKY_STARTUP_OPTIONS
static void write_newgame_options(const newgame_def& prefs, FILE *f)
{
    if (prefs.type != NUM_GAME_TYPE)
        fprintf(f, "type = %s\n", gametype_to_str(prefs.type).c_str());
    if (!prefs.map.empty())
        fprintf(f, "map = %s\n", prefs.map.c_str());
    if (!prefs.arena_teams.empty())
        fprintf(f, "arena_teams = %s\n", prefs.arena_teams.c_str());
    fprintf(f, "name = %s\n", prefs.name.c_str());
    if (prefs.species != SP_UNKNOWN)
        fprintf(f, "species = %s\n", _species_to_str(prefs.species).c_str());
    if (prefs.job != JOB_UNKNOWN)
        fprintf(f, "background = %s\n", _job_to_str(prefs.job).c_str());
    if (prefs.weapon != WPN_UNKNOWN)
        fprintf(f, "weapon = %s\n", _weapon_to_str(prefs.weapon).c_str());
    fprintf(f, "fully_random = %s\n", prefs.fully_random ? "yes" : "no");
}
#endif // !DISABLE_STICKY_STARTUP_OPTIONS

void write_newgame_options_file(const newgame_def& prefs)
{
#ifndef DISABLE_STICKY_STARTUP_OPTIONS
    // [ds] Saving startup prefs should work like this:
    //
    // 1. If the game is started without specifying a game type, always
    //    save startup preferences in the base savedir.
    // 2. If the game is started with a game type (Sprint), save startup
    //    preferences in the game-specific savedir.
    //
    // The idea is that public servers can use one instance of Crawl
    // but present Crawl and Sprint as two separate games in the
    // server-specific game menu -- the startup prefs file for Crawl and
    // Sprint should never collide, because the public server config will
    // specify the game type on the command-line.
    //
    // For normal users, startup prefs should always be saved in the
    // same base savedir so that when they start Crawl with "./crawl"
    // or the equivalent, their last game choices will be remembered,
    // even if they chose a Sprint game.
    //
    // Yes, this is unnecessarily complex. Better ideas welcome.
    //
    unwind_var<game_type> gt(crawl_state.type, Options.game.type);

    string fn = get_prefs_filename();
    FILE *f = fopen_u(fn.c_str(), "w");
    if (!f)
        return;
    write_newgame_options(prefs, f);
    fclose(f);
#endif // !DISABLE_STICKY_STARTUP_OPTIONS
}

void save_player_name()
{
#ifndef DISABLE_STICKY_STARTUP_OPTIONS
    if (!Options.remember_name)
        return ;

    // Read other preferences
    newgame_def prefs = read_startup_prefs();
    prefs.name = you.your_name;

    // And save
    write_newgame_options_file(prefs);
#endif // !DISABLE_STICKY_STARTUP_OPTIONS
}

void read_options(const string &s, bool runscript, bool clear_aliases)
{
    StringLineInput st(s);
    Options.read_options(st, runscript, clear_aliases);
}

game_options::game_options()
    : seed(0), no_save(false), language(LANG_EN), lang_name(nullptr)
{
    reset_options();
}

void game_options::read_options(LineInput &il, bool runscript,
                                bool clear_aliases)
{
    unsigned int line = 0;

    bool inscriptblock = false;
    bool inscriptcond  = false;
    bool isconditional = false;

    bool l_init        = false;

    if (clear_aliases)
        aliases.clear();

    dlua_chunk luacond(filename);
    dlua_chunk luacode(filename);

    while (!il.eof())
    {
        line_num++;
        string s   = il.get_line();
        string str = s;
        line++;

        trim_string(str);

        // This is to make some efficient comments
        if ((str.empty() || str[0] == '#') && !inscriptcond && !inscriptblock)
            continue;

        if (!inscriptcond && str[0] == ':')
        {
            // The init file is now forced into isconditional mode.
            isconditional = true;
            str = str.substr(1);
            if (!str.empty() && runscript)
            {
                // If we're in the middle of an option block, close it.
                if (!luacond.empty() && l_init)
                {
                    luacond.add(line - 1, "]])");
                    l_init = false;
                }
                luacond.add(line, str);
            }
            continue;
        }
        if (!inscriptcond && (starts_with(str, "L<") || starts_with(str, "<")))
        {
            // The init file is now forced into isconditional mode.
            isconditional = true;
            inscriptcond  = true;

            str = str.substr(starts_with(str, "L<") ? 2 : 1);
            // Is this a one-liner?
            if (!str.empty() && str.back() == '>')
            {
                inscriptcond = false;
                str = str.substr(0, str.length() - 1);
            }

            if (!str.empty() && runscript)
            {
                // If we're in the middle of an option block, close it.
                if (!luacond.empty() && l_init)
                {
                    luacond.add(line - 1, "]])");
                    l_init = false;
                }
                luacond.add(line, str);
            }
            continue;
        }
        else if (inscriptcond && !str.empty()
                 && (str.find(">") == str.length() - 1 || str == ">L"))
        {
            inscriptcond = false;
            str = str.substr(0, str.length() - 1);
            if (!str.empty() && runscript)
                luacond.add(line, str);
            continue;
        }
        else if (inscriptcond)
        {
            if (runscript)
                luacond.add(line, s);
            continue;
        }

        // Handle blocks of Lua
        if (!inscriptblock
            && (starts_with(str, "Lua{") || starts_with(str, "{")))
        {
            inscriptblock = true;
            luacode.clear();
            luacode.set_file(filename);

            // Strip leading Lua[
            str = str.substr(starts_with(str, "Lua{") ? 4 : 1);

            if (!str.empty() && str.find("}") == str.length() - 1)
            {
                str = str.substr(0, str.length() - 1);
                inscriptblock = false;
            }

            if (!str.empty())
                luacode.add(line, str);

            if (!inscriptblock && runscript)
            {
#ifdef CLUA_BINDINGS
                if (luacode.run(clua))
                {
                    mprf(MSGCH_ERROR, "Lua error: %s",
                         luacode.orig_error().c_str());
                }
                luacode.clear();
#endif
            }

            continue;
        }
        else if (inscriptblock && (str == "}Lua" || str == "}"))
        {
            inscriptblock = false;
#ifdef CLUA_BINDINGS
            if (runscript)
            {
                if (luacode.run(clua))
                {
                    mprf(MSGCH_ERROR, "Lua error: %s",
                         luacode.orig_error().c_str());
                }
            }
#endif
            luacode.clear();
            continue;
        }
        else if (inscriptblock)
        {
            luacode.add(line, s);
            continue;
        }

        if (isconditional && runscript)
        {
            if (!l_init)
            {
                luacond.add(line, "crawl.setopt([[");
                l_init = true;
            }

            luacond.add(line, s);
            continue;
        }

        read_option_line(str, runscript);
    }

#ifdef CLUA_BINDINGS
    if (runscript && !luacond.empty())
    {
        if (l_init)
            luacond.add(line, "]])");
        if (luacond.run(clua))
            mprf(MSGCH_ERROR, "Lua error: %s", luacond.orig_error().c_str());
    }
#endif

    Options.explore_stop |= Options.explore_stop_prompt;

    evil_colour = str_to_colour(variables["evil"]);
}

void game_options::fixup_options()
{
    // Validate save_dir
    if (!check_mkdir("Save directory", &save_dir))
        end(1);

    if (!SysEnv.morgue_dir.empty())
        morgue_dir = SysEnv.morgue_dir;

    if (!check_mkdir("Morgue directory", &morgue_dir))
        end(1);

    if (evil_colour == BLACK)
        evil_colour = MAGENTA;
}

static int _str_to_killcategory(const string &s)
{
    static const char *kc[] =
    {
        "you",
        "friend",
        "other",
    };

    for (unsigned i = 0; i < ARRAYSZ(kc); ++i)
        if (s == kc[i])
            return i;

    return -1;
}

#ifdef USE_TILE
void game_options::set_player_tile(const string &field)
{
    if (field == "normal")
    {
        tile_use_monster = MONS_0;
        tile_player_tile = 0;
        return;
    }
    else if (field == "playermons")
    {
        tile_use_monster = MONS_PLAYER;
        tile_player_tile = 0;
        return;
    }

    vector<string> fields = split_string(":", field);
    // Handle tile:<tile-name> values
    if (fields.size() == 2 && fields[0] == "tile")
    {
        // A variant tile. We have to find the base tile to look this up inthe
        // tile index.
        if (isdigit(*(fields[1].rbegin())))
        {
            string base_tname = fields[1];
            size_t found = base_tname.rfind('_');
            int offset = 0;
            tileidx_t base_tile = 0;
            if (found != std::string::npos
                && parse_int(fields[1].substr(found + 1).c_str(), offset))
            {
                base_tname = base_tname.substr(0, found);
                if (!tile_player_index(base_tname.c_str(), &base_tile))
                {
                    report_error("Can't find base tile \"%s\" of variant "
                                 "tile \"%s\"", base_tname.c_str(),
                                 fields[1].c_str());
                    return;
                }
                tile_player_tile = tileidx_mon_clamp(base_tile, offset);
            }
        }
        else if (!tile_player_index(fields[1].c_str(), &tile_player_tile))
        {
            report_error("Unknown tile: \"%s\"", fields[1].c_str());
            return;
        }
        tile_use_monster = MONS_PLAYER;
    }
    else if (fields.size() == 2 && fields[0] == "mons")
    {
        // Handle mons:<monster-name> values
        const monster_type m = _mons_class_by_string(fields[1]);
        if (m == MONS_0)
            report_error("Unknown monster: \"%s\"", fields[1].c_str());
        else
        {
            tile_use_monster = m;
            tile_player_tile = 0;
        }
    }
    else
    {
        report_error("Invalid setting for tile_player_tile: \"%s\"",
                     field.c_str());
    }
}

void game_options::set_tile_offsets(const string &field, bool set_shield)
{
    bool error = false;
    pair<int, int> *offsets;
    if (set_shield)
        offsets = &tile_shield_offsets;
    else
        offsets = &tile_weapon_offsets;

    if (field == "reset")
    {
        offsets->first = INT_MAX;
        offsets->second = INT_MAX;
        return;
    }

    vector<string> offs = split_string(",", field);
    if (offs.size() != 2
        || !parse_int(offs[0].c_str(), offsets->first)
        || abs(offsets->first) > 32
        || !parse_int(offs[1].c_str(), offsets->second)
        || abs(offsets->second) > 32)
    {
        report_error("Invalid %s tile offsets: \"%s\"",
                     set_shield ? "shield" : "weapon", field.c_str());
        error = true;
    }

    if (error)
    {
        offsets->first = INT_MAX;
        offsets->second = INT_MAX;
    }
}
#endif // USE_TILE

void game_options::do_kill_map(const string &from, const string &to)
{
    int ifrom = _str_to_killcategory(from),
        ito   = _str_to_killcategory(to);
    if (ifrom != -1 && ito != -1)
        kill_map[ifrom] = ito;
}

use_animations_type game_options::read_use_animations(const string &field) const
{
    use_animations_type animations;
    vector<string> types = split_string(",", field);
    for (const auto &type : types)
    {
        if (type == "beam")
            animations |= UA_BEAM;
        else if (type == "range")
            animations |= UA_RANGE;
        else if (type == "hp")
            animations |= UA_HP;
        else if (type == "monster_in_sight")
            animations |= UA_MONSTER_IN_SIGHT;
        else if (type == "pickup")
            animations |= UA_PICKUP;
        else if (type == "monster")
            animations |= UA_MONSTER;
        else if (type == "player")
            animations |= UA_PLAYER;
        else if (type == "branch_entry")
            animations |= UA_BRANCH_ENTRY;
    }

    return animations;
}

int game_options::read_explore_stop_conditions(const string &field) const
{
    int conditions = 0;
    vector<string> stops = split_string(",", field);
    for (const string &stop : stops)
    {
        const string c = replace_all_of(stop, " ", "_");
        if (c == "item" || c == "items")
            conditions |= ES_ITEM;
        else if (c == "greedy_pickup")
            conditions |= ES_GREEDY_PICKUP;
        else if (c == "greedy_pickup_gold")
            conditions |= ES_GREEDY_PICKUP_GOLD;
        else if (c == "greedy_pickup_smart")
            conditions |= ES_GREEDY_PICKUP_SMART;
        else if (c == "greedy_pickup_thrown")
            conditions |= ES_GREEDY_PICKUP_THROWN;
        else if (c == "shop" || c == "shops")
            conditions |= ES_SHOP;
        else if (c == "stair" || c == "stairs")
            conditions |= ES_STAIR;
        else if (c == "branch" || c == "branches")
            conditions |= ES_BRANCH;
        else if (c == "portal" || c == "portals")
            conditions |= ES_PORTAL;
        else if (c == "altar" || c == "altars")
            conditions |= ES_ALTAR;
        else if (c == "runed_door")
            conditions |= ES_RUNED_DOOR;
        else if (c == "greedy_item" || c == "greedy_items")
            conditions |= ES_GREEDY_ITEM;
        else if (c == "greedy_visited_item_stack")
            conditions |= ES_GREEDY_VISITED_ITEM_STACK;
        else if (c == "glowing" || c == "glowing_item"
                 || c == "glowing_items")
            conditions |= ES_GLOWING_ITEM;
        else if (c == "artefact" || c == "artefacts"
                 || c == "artifact" || c == "artifacts")
            conditions |= ES_ARTEFACT;
        else if (c == "rune" || c == "runes")
            conditions |= ES_RUNE;
        else if (c == "greedy_sacrificeable" || c == "greedy_sacrificeables"
                 || c == "greedy_sacrificable" || c == "greedy_sacrificables"
                 || c == "greedy_sacrificiable" || c == "greedy_sacrificiables")
        {
            conditions |= ES_GREEDY_SACRIFICEABLE;
        }
    }
    return conditions;
}

// Note the distinction between:
// 1. aliases "ae := autopickup_exception" "ae += useless_item"
//    stored in game_options.aliases.
// 2. variables "$slots := abc" "spell_slots += Dispel undead:$slots"
//    stored in game_options.variables.
// 3. constant variables "$slots = abc", "constant = slots".
//    stored in game_options.variables, but with an extra entry in
//    game_options.constants.
void game_options::add_alias(const string &key, const string &val)
{
    if (key[0] == '$')
    {
        string name = key.substr(1);
        // Don't alter if it's a constant.
        if (constants.count(name))
            return;
        variables[name] = val;
    }
    else
        aliases[key] = val;
}

string game_options::unalias(const string &key) const
{
    return lookup(aliases, key, key);
}

#define IS_VAR_CHAR(c) (isaalpha(c) || c == '_' || c == '-')

string game_options::expand_vars(const string &field) const
{
    string field_out = field;

    string::size_type curr_pos = 0;

    // Only try 100 times, so as to not get stuck in infinite recursion.
    for (int i = 0; i < 100; i++)
    {
        string::size_type dollar_pos = field_out.find("$", curr_pos);

        if (dollar_pos == string::npos || field_out.size() == (dollar_pos + 1))
            break;

        string::size_type start_pos = dollar_pos + 1;

        if (!IS_VAR_CHAR(field_out[start_pos]))
            continue;

        string::size_type end_pos;
        for (end_pos = start_pos; end_pos + 1 < field_out.size(); end_pos++)
        {
            if (!IS_VAR_CHAR(field_out[end_pos + 1]))
                break;
        }

        string var_name = field_out.substr(start_pos, end_pos - start_pos + 1);

        auto x = variables.find(var_name);

        if (x == variables.end())
        {
            curr_pos = end_pos + 1;
            continue;
        }

        string dollar_plus_name = "$";
        dollar_plus_name += var_name;

        field_out = replace_all(field_out, dollar_plus_name, x->second);

        // Start over at beginning
        curr_pos = 0;
    }

    return field_out;
}

void game_options::add_message_colour_mappings(const string &field,
                                               bool prepend, bool subtract)
{
    vector<string> fragments = split_string(",", field);
    if (prepend)
        reverse(fragments.begin(), fragments.end());
    for (const string &fragment : fragments)
        add_message_colour_mapping(fragment, prepend, subtract);
}

message_filter game_options::parse_message_filter(const string &filter)
{
    string::size_type pos = filter.find(":");
    if (pos && pos != string::npos)
    {
        string prefix = filter.substr(0, pos);
        int channel = str_to_channel(prefix);
        if (channel != -1 || prefix == "any")
        {
            string s = filter.substr(pos + 1);
            trim_string(s);
            return message_filter(channel, s);
        }
    }

    return message_filter(filter);
}

void game_options::add_message_colour_mapping(const string &field,
                                              bool prepend, bool subtract)
{
    vector<string> cmap = split_string(":", field, true, true, 1);

    if (cmap.size() != 2)
        return;

    const int col = str_to_colour(cmap[0]);
    msg_colour_type mcol;
    if (cmap[0] == "mute")
        mcol = MSGCOL_MUTED;
    else if (col == -1)
        return;
    else
        mcol = msg_colour(col);

    message_colour_mapping m = { parse_message_filter(cmap[1]), mcol };
    if (subtract)
        remove_matching(message_colour_mappings, m);
    else if (prepend)
        message_colour_mappings.insert(message_colour_mappings.begin(), m);
    else
        message_colour_mappings.push_back(m);
}

// Option syntax is:
// sort_menu = [menu_type:]yes|no|auto:n[:sort_conditions]
void game_options::set_menu_sort(string field)
{
    if (field.empty())
        return;

    menu_sort_condition cond(field);

    // Overrides all previous settings.
    if (cond.mtype == MT_ANY)
        sort_menus.clear();

    // Override existing values, if necessary.
    for (menu_sort_condition &m_cond : sort_menus)
        if (m_cond.mtype == cond.mtype)
        {
            m_cond.sort = cond.sort;
            m_cond.cmp  = cond.cmp;
            return;
        }

    sort_menus.push_back(cond);
}

// Lots of things use split parse, for some ^= and += should do different things,
// for others they should not. Split parse just pases them along.
void game_options::split_parse(const string &s, const string &separator,
                               void (game_options::*add)(const string &, bool),
                               bool prepend)
{
    const vector<string> defs = split_string(separator, s);
    if (prepend)
    {
        for ( auto it = defs.rbegin() ; it != defs.rend(); ++it)
            (this->*add)(*it, prepend);
    }
    else
    {
        for ( auto it = defs.begin() ; it != defs.end(); ++it)
            (this->*add)(*it, prepend);
    }
}

void game_options::set_option_fragment(const string &s, bool prepend)
{
    if (s.empty())
        return;

    string::size_type st = s.find(':');
    if (st == string::npos)
    {
        // Boolean option.
        if (s[0] == '!')
            read_option_line(s.substr(1) + " = false", true);
        else
            read_option_line(s + " = true", true);
    }
    else
    {
        // key:val option.
        read_option_line(s.substr(0, st) + " = " + s.substr(st + 1), true);
    }
}

// Not a method of the game_options class since keybindings aren't
// stored in that class.
static void _bindkey(string field)
{
    const size_t start_bracket = field.find_first_of('[');
    const size_t end_bracket   = field.find_last_of(']');

    if (start_bracket == string::npos
        || end_bracket == string::npos
        || start_bracket > end_bracket)
    {
        mprf(MSGCH_ERROR, "Bad bindkey bracketing in '%s'",
             field.c_str());
        return;
    }

    const string key_str = field.substr(start_bracket + 1,
                                        end_bracket - start_bracket - 1);

    int key;

    // TODO: Function keys.
    if (key_str.length() == 0)
    {
        mprf(MSGCH_ERROR, "No key in bindkey directive '%s'",
             field.c_str());
        return;
    }
    else if (key_str.length() == 1)
        key = key_str[0];
    else if (key_str.length() == 2)
    {
        if (key_str[0] != '^')
        {
            mprf(MSGCH_ERROR, "Invalid key '%s' in bindkey directive '%s'",
                 key_str.c_str(), field.c_str());
            return;
        }
        key = CONTROL(key_str[1]);
    }
    else
    {
        mprf(MSGCH_ERROR, "Invalid key '%s' in bindkey directive '%s'",
             key_str.c_str(), field.c_str());
        return;
    }

    const size_t start_name = field.find_first_not_of(' ', end_bracket + 1);
    if (start_name == string::npos)
    {
        mprf(MSGCH_ERROR, "No command name for bindkey directive '%s'",
             field.c_str());
        return;
    }

    const string       name = field.substr(start_name);
    const command_type cmd  = name_to_command(name);
    if (cmd == CMD_NO_CMD)
    {
        mprf(MSGCH_ERROR, "No command named '%s'", name.c_str());
        return;
    }

    bind_command_to_key(cmd, key);
}

static bool _is_autopickup_ban(pair<text_pattern, bool> entry)
{
    return !entry.second;
}

static bool _first_less(const pair<int, int> &l, const pair<int, int> &r)
{
    return l.first < r.first;
}

static bool _first_greater(const pair<int, int> &l, const pair<int, int> &r)
{
    return l.first > r.first;
}

// T must be convertible to from a string.
template <class T>
static void _handle_list(vector<T> &value_list, string field,
                         bool append, bool prepend, bool subtract)
{
    if (!append && !prepend && !subtract)
        value_list.clear();

    vector<T> new_entries;
    for (const auto &part : split_string(",", field))
    {
        if (part.empty())
            continue;

        if (subtract)
            remove_matching(value_list, part);
        else
            new_entries.push_back(part);
    }
    _merge_lists(value_list, new_entries, prepend);
}

void game_options::read_option_line(const string &str, bool runscript)
{
#define BOOL_OPTION_NAMED(_opt_str, _opt_var)               \
    if (key == _opt_str) do {                               \
        _opt_var = _read_bool(field, _opt_var); \
    } while (false)
#define BOOL_OPTION(_opt) BOOL_OPTION_NAMED(#_opt, _opt)

#define COLOUR_OPTION_NAMED(_opt_str, _opt_var, elemental)              \
    if (key == _opt_str) do {                                           \
        const int col = str_to_colour(field, -1, true, elemental);      \
        if (col != -1) {                                                \
            _opt_var = col;                                             \
        } else {                                                        \
            /*fprintf(stderr, "Bad %s -- %s\n", key, field.c_str());*/  \
            report_error("Bad %s -- %s\n", key.c_str(), field.c_str()); \
        }                                                               \
    } while (false)
#define COLOUR_OPTION(_opt) COLOUR_OPTION_NAMED(#_opt, _opt, true)
#define UI_COLOUR_OPTION(_opt) COLOUR_OPTION_NAMED(#_opt, _opt, false)

#define CURSES_OPTION_NAMED(_opt_str, _opt_var)     \
    if (key == _opt_str) do {                       \
        _opt_var = curses_attribute(field);   \
    } while (false)
#define CURSES_OPTION(_opt) CURSES_OPTION_NAMED(#_opt, _opt)

#define INT_OPTION_NAMED(_opt_str, _opt_var, _min_val, _max_val)        \
    if (key == _opt_str) do {                                           \
        const int min_val = (_min_val);                                 \
        const int max_val = (_max_val);                                 \
        int val = _opt_var;                                             \
        if (!parse_int(field.c_str(), val))                             \
            report_error("Bad %s: \"%s\"", _opt_str, field.c_str());    \
        else if (val < min_val)                                         \
            report_error("Bad %s: %d < %d", _opt_str, val, min_val);    \
        else if (val > max_val)                                         \
            report_error("Bad %s: %d > %d", _opt_str, val, max_val);    \
        else                                                            \
            _opt_var = val;                                             \
    } while (false)
#define INT_OPTION(_opt, _min_val, _max_val) \
    INT_OPTION_NAMED(#_opt, _opt, _min_val, _max_val)

#define LIST_OPTION_NAMED(_opt_str, _opt_var)                                \
    if (key == _opt_str) do {                                                \
        _handle_list(_opt_var, field, plus_equal, caret_equal, minus_equal); \
    } while (false)
#define LIST_OPTION(_opt) LIST_OPTION_NAMED(#_opt, _opt)
#define NEWGAME_OPTION(_opt, _conv, _type)                                     \
    if (plain)                                                                 \
        _opt.clear();                                                          \
    for (const auto &part : split_string(",", field))                          \
    {                                                                          \
        if (minus_equal)                                                       \
        {                                                                      \
            auto it2 = find(_opt.begin(), _opt.end(), _conv(part));            \
            if (it2 != _opt.end())                                             \
                _opt.erase(it2);                                               \
        }                                                                      \
        else                                                                   \
            _opt.push_back(_conv(part));                                       \
    }
    string key    = "";
    string subkey = "";
    string field  = "";

    bool plus_equal  = false;
    bool caret_equal = false;
    bool minus_equal = false;

    const int first_equals = str.find('=');

    // all lines with no equal-signs we ignore
    if (first_equals < 0)
        return;

    field = str.substr(first_equals + 1);
    field = expand_vars(field);

    string prequal = trimmed_string(str.substr(0, first_equals));

    // Is this a case of key += val?
    if (prequal.length() && prequal[prequal.length() - 1] == '+')
    {
        plus_equal = true;
        prequal = prequal.substr(0, prequal.length() - 1);
        trim_string(prequal);
    }
    else if (prequal.length() && prequal[prequal.length() - 1] == '-')
    {
        minus_equal = true;
        prequal = prequal.substr(0, prequal.length() - 1);
        trim_string(prequal);
    }
    else if (prequal.length() && prequal[prequal.length() - 1] == '^')
    {
        caret_equal = true;
        prequal = prequal.substr(0, prequal.length() - 1);
        trim_string(prequal);
    }
    else if (prequal.length() && prequal[prequal.length() - 1] == ':')
    {
        prequal = prequal.substr(0, prequal.length() - 1);
        trim_string(prequal);
        trim_string(field);

        add_alias(prequal, field);
        return;
    }

    bool plain = !plus_equal && !minus_equal && !caret_equal;

    prequal = unalias(prequal);

    const string::size_type first_dot = prequal.find('.');
    if (first_dot != string::npos)
    {
        key    = prequal.substr(0, first_dot);
        subkey = prequal.substr(first_dot + 1);
    }
    else
    {
        // no subkey (dots are okay in value field)
        key    = prequal;
    }

    // Clean up our data...
    lowercase(trim_string(key));
    lowercase(trim_string(subkey));

    // some fields want capitals... none care about external spaces
    trim_string(field);

    // Keep unlowercased field around
    const string orig_field = field;

    if (key != "name" && key != "crawl_dir" && key != "macro_dir"
        && key != "combo"
        && key != "species" && key != "background" && key != "job"
        && key != "race" && key != "class" && key != "ban_pickup"
        && key != "autopickup_exceptions"
        && key != "explore_stop_pickup_ignore"
        && key != "stop_travel" && key != "sound"
        && key != "force_more_message"
        && key != "flash_screen_message"
        && key != "confirm_action"
        && key != "drop_filter" && key != "lua_file" && key != "terp_file"
        && key != "note_items" && key != "autoinscribe"
        && key != "note_monsters" && key != "note_messages"
        && key != "display_char" && !starts_with(key, "cset") // compatibility
        && key != "dungeon" && key != "feature"
        && key != "mon_glyph" && key != "item_glyph"
        && key != "fire_items_start"
        && key != "opt" && key != "option"
        && key != "menu_colour" && key != "menu_color"
        && key != "message_colour" && key != "message_color"
        && key != "levels" && key != "level" && key != "entries"
        && key != "include" && key != "bindkey"
        && key != "spell_slot"
        && key != "item_slot"
        && key != "ability_slot"
        && key.find("font") == string::npos)
    {
        lowercase(field);
    }

    if (key == "include")
        include(field, true, runscript);
    else if (key == "opt" || key == "option")
        split_parse(field, ",", &game_options::set_option_fragment);
    else if (key == "autopickup")
    {
        // clear out autopickup
        autopickups.reset();

        ucs_t c;
        for (const char* tp = field.c_str(); int s = utf8towc(&c, tp); tp += s)
        {
            object_class_type type = item_class_by_sym(c);

            if (type < NUM_OBJECT_CLASSES)
                autopickups.set(type);
            else
                report_error("Bad object type '%*s' for autopickup.\n", s, tp);
        }
    }
#if !defined(DGAMELAUNCH) || defined(DGL_REMEMBER_NAME)
    else if (key == "name")
    {
        // field is already cleaned up from trim_string()
        game.name = field;
    }
#endif
    else if (key == "char_set")
    {
        if (field == "ascii")
            char_set = CSET_ASCII;
        else if (field == "default")
            char_set = CSET_DEFAULT;
        else
            fprintf(stderr, "Bad character set: %s\n", field.c_str());
    }
    else if (key == "language")
    {
        if (!set_lang(field.c_str()))
            report_error("No translations for language: %s\n", field.c_str());
    }
    else if (key == "fake_lang")
        set_fake_langs(field);
    else if (key == "default_autopickup")
    {
        if (_read_bool(field, true))
            autopickup_on = 1;
        else
            autopickup_on = 0;
    }
    else BOOL_OPTION(autopickup_starting_ammo);
    else if (key == "default_manual_training")
    {
        if (_read_bool(field, true))
            default_manual_training = true;
        else
            default_manual_training = false;
    }
    else BOOL_OPTION(default_show_all_skills);
#ifndef DGAMELAUNCH
    else BOOL_OPTION(restart_after_game);
    else BOOL_OPTION(restart_after_save);
#endif
    else BOOL_OPTION(auto_switch);
    else BOOL_OPTION(suppress_startup_errors);
    else if (key == "easy_confirm")
    {
        // decide when to allow both 'Y'/'N' and 'y'/'n' on yesno() prompts
        if (field == "none")
            easy_confirm = CONFIRM_NONE_EASY;
        else if (field == "safe")
            easy_confirm = CONFIRM_SAFE_EASY;
        else if (field == "all")
            easy_confirm = CONFIRM_ALL_EASY;
    }
    else if (key == "allow_self_target")
    {
        if (field == "yes")
            allow_self_target = CONFIRM_NONE;
        else if (field == "no")
            allow_self_target = CONFIRM_CANCEL;
        else if (field == "prompt")
            allow_self_target = CONFIRM_PROMPT;
    }
    else BOOL_OPTION(show_uncursed);
    else BOOL_OPTION(easy_quit_item_prompts);
    else BOOL_OPTION_NAMED("easy_quit_item_lists", easy_quit_item_prompts);
    else BOOL_OPTION(travel_open_doors);
    else BOOL_OPTION(easy_unequip);
    else BOOL_OPTION(equip_unequip);
    else BOOL_OPTION(jewellery_prompt);
    else BOOL_OPTION_NAMED("easy_armour", easy_unequip);
    else BOOL_OPTION_NAMED("easy_armor", easy_unequip);
    else BOOL_OPTION(easy_door);
    else BOOL_OPTION(warn_hatches);
    else BOOL_OPTION(enable_recast_spell);
    else if (key == "confirm_butcher")
    {
        if (field == "always")
            confirm_butcher = CONFIRM_ALWAYS;
        else if (field == "never")
            confirm_butcher = CONFIRM_NEVER;
        else if (field == "auto")
            confirm_butcher = CONFIRM_AUTO;
    }
    else BOOL_OPTION(easy_eat_chunks);
    else BOOL_OPTION(auto_eat_chunks);
    else if (key == "lua_file" && runscript)
    {
#ifdef CLUA_BINDINGS
        clua.execfile(field.c_str(), false, false);
        if (!clua.error.empty())
            mprf(MSGCH_ERROR, "Lua error: %s", clua.error.c_str());
#endif
    }
    else if (key == "terp_file" && runscript)
        terp_files.push_back(field);
    else if (key == "colour" || key == "color")
    {
        const int orig_col   = str_to_colour(subkey);
        const int result_col = str_to_colour(field);

        if (orig_col != -1 && result_col != -1)
            colour[orig_col] = result_col;
        else
        {
            fprintf(stderr, "Bad colour -- %s=%d or %s=%d\n",
                     subkey.c_str(), orig_col, field.c_str(), result_col);
        }
    }
    else if (key == "channel")
    {
        const int chnl = str_to_channel(subkey);
        const msg_colour_type col  = _str_to_channel_colour(field);

        if (chnl != -1 && col != MSGCOL_NONE)
            channels[chnl] = col;
        else if (chnl == -1)
            fprintf(stderr, "Bad channel -- %s\n", subkey.c_str());
        else if (col == MSGCOL_NONE)
            fprintf(stderr, "Bad colour -- %s\n", field.c_str());
    }
    else UI_COLOUR_OPTION(background_colour);
    else COLOUR_OPTION(detected_item_colour);
    else COLOUR_OPTION(detected_monster_colour);
    else COLOUR_OPTION(remembered_monster_colour);
    else if (key == "use_animations")
    {
        if (plain)
            use_animations = UA_ALWAYS_ON;

        const auto new_animations = read_use_animations(field);
        if (minus_equal)
            use_animations &= ~new_animations;
        else
            use_animations |= new_animations;
    }
    else if (starts_with(key, interrupt_prefix))
    {
        set_activity_interrupt(key.substr(interrupt_prefix.length()),
                               field,
                               plus_equal || caret_equal,
                               minus_equal);
    }
    else if (key == "display_char"
             || starts_with(key, "cset")) // compatibility with old rcfiles
    {
        for (const string &over : split_string(",", field))
        {
            vector<string> mapping = split_string(":", over);
            if (mapping.size() != 2)
                continue;

            dungeon_char_type dc = dchar_by_name(mapping[0]);
            if (dc == NUM_DCHAR_TYPES)
                continue;

            add_cset_override(dc, read_symbol(mapping[1]));
        }
    }
    else if (key == "feature" || key == "dungeon")
    {
        if (plain)
           clear_feature_overrides();

        if (minus_equal)
            split_parse(field, ";", &game_options::remove_feature_override);
        else
            split_parse(field, ";", &game_options::add_feature_override);
    }
    else if (key == "mon_glyph")
    {
        if (plain)
           mon_glyph_overrides.clear();

        if (minus_equal)
            split_parse(field, ",", &game_options::remove_mon_glyph_override);
        else
            split_parse(field, ",", &game_options::add_mon_glyph_override);
    }
    else if (key == "item_glyph")
    {
        if (plain)
        {
            item_glyph_overrides.clear();
            item_glyph_cache.clear();
        }

        if (minus_equal)
            split_parse(field, ",", &game_options::remove_item_glyph_override);
        else
            split_parse(field, ",", &game_options::add_item_glyph_override, caret_equal);
    }
    else CURSES_OPTION(friend_brand);
    else CURSES_OPTION(neutral_brand);
    else CURSES_OPTION(stab_brand);
    else CURSES_OPTION(may_stab_brand);
    else CURSES_OPTION(feature_item_brand);
    else CURSES_OPTION(trap_item_brand);
    // This is useful for terms where dark grey does
    // not have standout modes (since it's black on black).
    // This option will use light-grey instead in these cases.
    else BOOL_OPTION(no_dark_brand);
    // no_dark_brand applies here as well.
    else CURSES_OPTION(heap_brand);
    else UI_COLOUR_OPTION(status_caption_colour);
    else if (key == "arena_teams")
        game.arena_teams = field;
    // [ds] Allow changing map only if the map hasn't been set on the
    // command-line.
    else if (key == "map" && crawl_state.sprint_map.empty())
        game.map = field;
    // [ds] For dgamelaunch setups, the player should *not* be able to
    // set game type in their rc; the only way to set game type for
    // DGL builds should be the command-line options.
    else if (key == "type")
    {
#if defined(DGAMELAUNCH)
        game.type = Options.game.type;
#else
        game.type = _str_to_gametype(field);
#endif
    }
    else if (key == "combo")
    {
        game.allowed_species.clear();
        game.allowed_jobs.clear();
        game.allowed_weapons.clear();
        NEWGAME_OPTION(game.allowed_combos, string, string);
    }
    else if (key == "species" || key == "race")
    {
        game.allowed_combos.clear();
        NEWGAME_OPTION(game.allowed_species, _str_to_species,
                       species_type);
    }
    else if (key == "background" || key == "job" || key == "class")
    {
        game.allowed_combos.clear();
        NEWGAME_OPTION(game.allowed_jobs, str_to_job, job_type);
    }
    else if (key == "weapon")
    {
        // Choose this weapon for backgrounds that get choice.
        game.allowed_combos.clear();
        NEWGAME_OPTION(game.allowed_weapons, str_to_weapon, weapon_type);
    }
    BOOL_OPTION_NAMED("fully_random", game.fully_random);
    else if (key == "fire_items_start")
    {
        if (isaalpha(field[0]))
            fire_items_start = letter_to_index(field[0]);
        else
        {
            fprintf(stderr, "Bad fire item start index: %s\n",
                     field.c_str());
        }
    }
    else if (key == "assign_item_slot")
    {
        if (field == "forward")
            assign_item_slot = SS_FORWARD;
        else if (field == "backward")
            assign_item_slot = SS_BACKWARD;
    }
    else if (key == "show_god_gift")
    {
        if (field == "yes")
            show_god_gift = MB_TRUE;
        else if (field == "unid" || field == "unident" || field == "unidentified")
            show_god_gift = MB_MAYBE;
        else if (field == "no")
            show_god_gift = MB_FALSE;
        else
            report_error("Unknown show_god_gift value: %s\n", field.c_str());
    }
    else if (key == "fire_order")
        set_fire_order(field, plus_equal, caret_equal);
    else if (key == "pizza")
    {
        const vector<string> all_pizzas = split_string(",", field);
        // only non-empty pizza strings
        copy_if(all_pizzas.begin(), all_pizzas.end(), back_inserter(pizzas),
                [](string p) { return !trimmed_string(p).empty(); });
    }
    else BOOL_OPTION(regex_search);
#if !defined(DGAMELAUNCH) || defined(DGL_REMEMBER_NAME)
    else BOOL_OPTION(remember_name);
#endif
#ifndef DGAMELAUNCH
    else if (key == "save_dir")
        save_dir = field;
    else if (key == "morgue_dir")
        morgue_dir = field;
#endif
    else BOOL_OPTION(show_newturn_mark);
    else BOOL_OPTION(show_game_turns);
    else INT_OPTION(hp_warning, 0, 100);
    else INT_OPTION_NAMED("mp_warning", magic_point_warning, 0, 100);
    else LIST_OPTION(note_monsters);
    else LIST_OPTION(note_messages);
    else INT_OPTION(note_hp_percent, 0, 100);
#ifndef DGAMELAUNCH
    // If DATA_DIR_PATH is set, don't set crawl_dir from .crawlrc.
#ifndef DATA_DIR_PATH
    else if (key == "crawl_dir")
    {
        // We shouldn't bother to allocate this a second time
        // if the user puts two crawl_dir lines in the init file.
        SysEnv.crawl_dir = field;
    }
    else if (key == "macro_dir")
        macro_dir = field;
#endif
#endif
#ifdef DGL_SIMPLE_MESSAGING
    else BOOL_OPTION(messaging);
#endif
    else BOOL_OPTION(mouse_input);
    // These need to be odd, hence allow +1.
    else INT_OPTION(view_max_width, VIEW_MIN_WIDTH, GXM + 1);
    else INT_OPTION(view_max_height, VIEW_MIN_HEIGHT, GYM + 1);
    else INT_OPTION(mlist_min_height, 0, INT_MAX);
    else INT_OPTION(msg_min_height, MSG_MIN_HEIGHT, INT_MAX);
    else INT_OPTION(msg_max_height, MSG_MIN_HEIGHT, INT_MAX);
    else BOOL_OPTION(mlist_allow_alternate_layout);
    else BOOL_OPTION(messages_at_top);
#ifndef USE_TILE
    else BOOL_OPTION(mlist_targeting);
    else BOOL_OPTION_NAMED("mlist_targetting", mlist_targeting);
#endif
    else BOOL_OPTION(msg_condense_repeats);
    else BOOL_OPTION(msg_condense_short);
    else BOOL_OPTION(view_lock_x);
    else BOOL_OPTION(view_lock_y);
    else if (key == "view_lock")
    {
        const bool lock = _read_bool(field, true);
        view_lock_x = view_lock_y = lock;
    }
    else BOOL_OPTION(center_on_scroll);
    else BOOL_OPTION(symmetric_scroll);
    else if (key == "scroll_margin_x")
    {
        scroll_margin_x = atoi(field.c_str());
        if (scroll_margin_x < 0)
            scroll_margin_x = 0;
    }
    else if (key == "scroll_margin_y")
    {
        scroll_margin_y = atoi(field.c_str());
        if (scroll_margin_y < 0)
            scroll_margin_y = 0;
    }
    else if (key == "scroll_margin")
    {
        int scrollmarg = atoi(field.c_str());
        if (scrollmarg < 0)
            scrollmarg = 0;
        scroll_margin_x = scroll_margin_y = scrollmarg;
    }
    else BOOL_OPTION(always_show_exclusions);
    else if (key == "user_note_prefix")
    {
        // field is already cleaned up from trim_string()
        user_note_prefix = orig_field;
    }
    else if (key == "skill_focus")
    {
        if (field == "toggle")
            skill_focus = SKM_FOCUS_TOGGLE;
        else if (_read_bool(field, true))
            skill_focus = SKM_FOCUS_ON;
        else
            skill_focus = SKM_FOCUS_OFF;
    }
    else BOOL_OPTION(note_all_skill_levels);
    else BOOL_OPTION(note_skill_max);
    else BOOL_OPTION(note_xom_effects);
    else BOOL_OPTION(note_chat_messages);
    else BOOL_OPTION(note_dgl_messages);
    else BOOL_OPTION(clear_messages);
    else BOOL_OPTION(show_more);
    else BOOL_OPTION(small_more);
    else if (key == "flush")
    {
        if (subkey == "failure")
        {
            flush_input[FLUSH_ON_FAILURE]
                = _read_bool(field, flush_input[FLUSH_ON_FAILURE]);
        }
        else if (subkey == "command")
        {
            flush_input[FLUSH_BEFORE_COMMAND]
                = _read_bool(field, flush_input[FLUSH_BEFORE_COMMAND]);
        }
        else if (subkey == "message")
        {
            flush_input[FLUSH_ON_MESSAGE]
                = _read_bool(field, flush_input[FLUSH_ON_MESSAGE]);
        }
        else if (subkey == "lua")
        {
            flush_input[FLUSH_LUA]
                = _read_bool(field, flush_input[FLUSH_LUA]);
        }
    }
    else if (key == "wiz_mode")
    {
        // wiz_mode is recognised as a legal key in all compiles -- bwr
#ifdef WIZARD
    #ifndef DGAMELAUNCH
        if (field == "never")
            wiz_mode = WIZ_NEVER;
        else if (field == "no")
            wiz_mode = WIZ_NO;
        else if (field == "yes")
            wiz_mode = WIZ_YES;
        else
            report_error("Unknown wiz_mode option: %s\n", field.c_str());
    #endif
#endif
    }
    else if (key == "explore_mode")
    {
#ifdef WIZARD
    #ifndef DGAMELAUNCH
        if (field == "never")
            explore_mode = WIZ_NEVER;
        else if (field == "no")
            explore_mode = WIZ_NO;
        else if (field == "yes")
            explore_mode = WIZ_YES;
        else
            report_error("Unknown explore_mode option: %s\n", field.c_str());
    #endif
#endif
    }
    else if (key == "ban_pickup")
    {
        // Only remove negative, not positive, exceptions.
        if (plain)
            erase_if(force_autopickup, _is_autopickup_ban);

        vector<pair<text_pattern, bool> > new_entries;
        for (const string &s : split_string(",", field))
        {
            if (s.empty())
                continue;

            const pair<text_pattern, bool> f_a(s, false);

            if (minus_equal)
                remove_matching(force_autopickup, f_a);
            else
                new_entries.push_back(f_a);
        }
        _merge_lists(force_autopickup, new_entries, caret_equal);
    }
    else if (key == "autopickup_exceptions")
    {
        if (plain)
            force_autopickup.clear();

        vector<pair<text_pattern, bool> > new_entries;
        for (const string &s : split_string(",", field))
        {
            if (s.empty())
                continue;

            pair<text_pattern, bool> f_a;

            if (s[0] == '>')
                f_a = make_pair(s.substr(1), false);
            else if (s[0] == '<')
                f_a = make_pair(s.substr(1), true);
            else
                f_a = make_pair(s, false);

            if (minus_equal)
                remove_matching(force_autopickup, f_a);
            else
                new_entries.push_back(f_a);
        }
        _merge_lists(force_autopickup, new_entries, caret_equal);
    }
    else LIST_OPTION(note_items);
#ifndef _MSC_VER
    // break if-else chain on broken Microsoft compilers with stupid nesting limits
    else
#endif

    if (key == "autoinscribe")
    {
        if (plain)
            autoinscriptions.clear();

        const size_t first = field.find_first_of(':');
        const size_t last  = field.find_last_of(':');
        if (first == string::npos || first != last)
        {
            return report_error("Autoinscribe string must have exactly "
                                "one colon: %s\n", field.c_str());
        }

        if (first == 0)
        {
            report_error("Autoinscribe pattern is empty: %s\n", field.c_str());
            return;
        }

        if (last == field.length() - 1)
        {
            report_error("Autoinscribe result is empty: %s\n", field.c_str());
            return;
        }

        vector<string> thesplit = split_string(":", field);

        if (thesplit.size() != 2)
        {
            report_error("Error parsing autoinscribe string: %s\n",
                         field.c_str());
            return;
        }

        pair<text_pattern,string> entry(thesplit[0], thesplit[1]);

        if (minus_equal)
            remove_matching(autoinscriptions, entry);
        else if (caret_equal)
            autoinscriptions.insert(autoinscriptions.begin(), entry);
        else
            autoinscriptions.push_back(entry);
    }
#ifndef DGAMELAUNCH
    else if (key == "map_file_name")
        map_file_name = field;
#endif
    else if (key == "hp_colour" || key == "hp_color")
    {
        if (plain)
            hp_colour.clear();

        vector<string> thesplit = split_string(",", field);
        for (unsigned i = 0; i < thesplit.size(); ++i)
        {
            vector<string> insplit = split_string(":", thesplit[i]);
            int hp_percent = 100;

            if (insplit.empty() || insplit.size() > 2
                 || insplit.size() == 1 && i != 0)
            {
                report_error("Bad hp_colour string: %s\n", field.c_str());
                break;
            }

            if (insplit.size() == 2)
                hp_percent = atoi(insplit[0].c_str());

            const string colstr = insplit[(insplit.size() == 1) ? 0 : 1];
            const int scolour = str_to_colour(colstr);
            if (scolour > 0)
            {
                pair<int, int> entry(hp_percent, scolour);
                // We do not treat prepend differently since we will be sorting.
                if (minus_equal)
                    remove_matching(hp_colour, entry);
                else
                    hp_colour.push_back(entry);
            }
            else
            {
                report_error("Bad hp_colour: %s", colstr.c_str());
                break;
            }
        }
        stable_sort(hp_colour.begin(), hp_colour.end(), _first_greater);
    }
    else if (key == "mp_color" || key == "mp_colour")
    {
        if (plain)
            mp_colour.clear();

        vector<string> thesplit = split_string(",", field);
        for (unsigned i = 0; i < thesplit.size(); ++i)
        {
            vector<string> insplit = split_string(":", thesplit[i]);
            int mp_percent = 100;

            if (insplit.empty() || insplit.size() > 2
                 || insplit.size() == 1 && i != 0)
            {
                report_error("Bad mp_colour string: %s\n", field.c_str());
                break;
            }

            if (insplit.size() == 2)
                mp_percent = atoi(insplit[0].c_str());

            const string colstr = insplit[(insplit.size() == 1) ? 0 : 1];
            const int scolour = str_to_colour(colstr);
            if (scolour > 0)
            {
                pair<int, int> entry(mp_percent, scolour);
                // We do not treat prepend differently since we will be sorting.
                if (minus_equal)
                    remove_matching(mp_colour, entry);
                else
                    mp_colour.push_back(entry);
            }
            else
            {
                report_error("Bad mp_colour: %s", colstr.c_str());
                break;
            }
        }
        stable_sort(mp_colour.begin(), mp_colour.end(), _first_greater);
    }
    else if (key == "stat_colour" || key == "stat_color")
    {
        if (plain)
            stat_colour.clear();

        vector<string> thesplit = split_string(",", field);
        for (unsigned i = 0; i < thesplit.size(); ++i)
        {
            vector<string> insplit = split_string(":", thesplit[i]);

            if (insplit.empty() || insplit.size() > 2
                || insplit.size() == 1 && i != 0)
            {
                report_error("Bad stat_colour string: %s\n", field.c_str());
                break;
            }

            int stat_limit = 1;
            if (insplit.size() == 2)
                stat_limit = atoi(insplit[0].c_str());

            const string colstr = insplit[(insplit.size() == 1) ? 0 : 1];
            const int scolour = str_to_colour(colstr);
            if (scolour > 0)
            {
                pair<int, int> entry(stat_limit, scolour);
                // We do not treat prepend differently since we will be sorting.
                if (minus_equal)
                    remove_matching(stat_colour, entry);
                else
                    stat_colour.push_back(entry);
            }
            else
            {
                report_error("Bad stat_colour: %s", colstr.c_str());
                break;
            }
        }
        stable_sort(stat_colour.begin(), stat_colour.end(), _first_less);
    }
    else if (key == "enemy_hp_colour" || key == "enemy_hp_color")
    {
        if (plain)
            enemy_hp_colour.clear();
        str_to_enemy_hp_colour(field, caret_equal);
    }
    else if (key == "monster_list_colour" || key == "monster_list_color")
    {
        if (plain)
            clear_monster_list_colours();

        vector<string> thesplit = split_string(",", field);
        for (unsigned i = 0; i < thesplit.size(); ++i)
        {
            vector<string> insplit = split_string(":", thesplit[i]);

            if (insplit.empty() || insplit.size() > 2
                 || insplit.size() == 1 && !minus_equal
                 || insplit.size() == 2 && minus_equal)
            {
                report_error("Bad monster_list_colour string: %s\n",
                             field.c_str());
                break;
            }

            const int scolour = minus_equal ? -1 : str_to_colour(insplit[1]);

            // No elemental colours!
            if (scolour >= 16 || scolour < 0 && !minus_equal)
            {
                report_error("Bad monster_list_colour: %s", insplit[1].c_str());
                break;
            }
            if (!set_monster_list_colour(insplit[0], scolour))
            {
                report_error("Bad monster_list_colour key: %s\n",
                             insplit[0].c_str());
                break;
            }
        }
    }

    else if (key == "note_skill_levels")
    {
        if (plain)
            note_skill_levels.reset();
        vector<string> thesplit = split_string(",", field);
        for (unsigned i = 0; i < thesplit.size(); ++i)
        {
            int num = atoi(thesplit[i].c_str());
            if (num > 0 && num <= 27)
                note_skill_levels.set(num, !minus_equal);
            else
            {
                report_error("Bad skill level to note -- %s\n",
                             thesplit[i].c_str());
                continue;
            }
        }
    }
    else if (key == "spell_slot"
             || key == "item_slot"
             || key == "ability_slot")

    {
        auto& auto_letters = (key == "item_slot"  ? auto_item_letters
                           : (key == "spell_slot" ? auto_spell_letters
                                                  : auto_ability_letters));
        if (plain)
            auto_letters.clear();

        vector<string> thesplit = split_string(":", field);
        if (thesplit.size() != 2)
        {
            return report_error("Error parsing %s string: %s\n",
                                key.c_str(), field.c_str());
        }
        pair<text_pattern,string> entry(text_pattern(thesplit[0], true),
                                        thesplit[1]);

        if (minus_equal)
            remove_matching(auto_letters, entry);
        else if (caret_equal)
            auto_letters.insert(auto_letters.begin(), entry);
        else
            auto_letters.push_back(entry);
    }
    else BOOL_OPTION(pickup_thrown);
#ifdef WIZARD
    else if (key == "fsim_mode")
        fsim_mode = field;
    else BOOL_OPTION(fsim_csv);
    else LIST_OPTION(fsim_scale);
    else LIST_OPTION(fsim_kit);
    else if (key == "fsim_rounds")
    {
        fsim_rounds = atol(field.c_str());
        if (fsim_rounds < 1000)
            fsim_rounds = 1000;
        if (fsim_rounds > 500000L)
            fsim_rounds = 500000L;
    }
    else if (key == "fsim_mons")
        fsim_mons = field;
#endif // WIZARD
    else if (key == "sort_menus")
    {
        for (const string &frag : split_string(";", field))
            if (!frag.empty())
                set_menu_sort(frag);
    }
    else if (key == "travel_delay")
    {
        // Read travel delay in milliseconds.
        travel_delay = atoi(field.c_str());
        if (travel_delay < -1)
            travel_delay = -1;
        if (travel_delay > 2000)
            travel_delay = 2000;
    }
    else if (key == "explore_delay")
    {
        // Read explore delay in milliseconds.
        explore_delay = atoi(field.c_str());
        if (explore_delay < -1)
            explore_delay = -1;
        if (explore_delay > 2000)
            explore_delay = 2000;
    }
    else if (key == "rest_delay")
    {
        // Read explore delay in milliseconds.
        rest_delay = atoi(field.c_str());
        if (rest_delay < -1)
            rest_delay = -1;
        if (rest_delay > 2000)
            rest_delay = 2000;
    }
    else BOOL_OPTION(show_travel_trail);
    else if (key == "level_map_cursor_step")
    {
        level_map_cursor_step = atoi(field.c_str());
        if (level_map_cursor_step < 1)
            level_map_cursor_step = 1;
        if (level_map_cursor_step > 50)
            level_map_cursor_step = 50;
    }
    else BOOL_OPTION(use_fake_cursor);
    else BOOL_OPTION(use_fake_player_cursor);
    else BOOL_OPTION(show_player_species);
    else if (key == "force_more_message" || key == "flash_screen_message")
    {
        vector<message_filter> &filters = (key == "force_more_message" ? force_more_message : flash_screen_message);
        if (plain)
            filters.clear();

        vector<message_filter> new_entries;
        for (const string &fragment : split_string(",", field))
        {
            if (fragment.empty())
                continue;

            message_filter mf(fragment);

            string::size_type pos = fragment.find(":");
            if (pos && pos != string::npos)
            {
                string prefix = fragment.substr(0, pos);
                int channel = str_to_channel(prefix);
                if (channel != -1 || prefix == "any")
                {
                    string s = fragment.substr(pos + 1);
                    mf = message_filter(channel, trim_string(s));
                }
            }

            if (minus_equal)
                remove_matching(filters, mf);
            else
                new_entries.push_back(mf);
        }
        _merge_lists(filters, new_entries, caret_equal);
    }
    else LIST_OPTION(confirm_action);
    else LIST_OPTION(drop_filter);
    else if (key == "travel_avoid_terrain")
    {
        // TODO: allow resetting (need reset_forbidden_terrain())
        for (const string &seg : split_string(",", field))
            prevent_travel_to(seg);
    }
    else if (key == "tc_reachable")
        tc_reachable = str_to_colour(field, tc_reachable);
    else if (key == "tc_excluded")
        tc_excluded = str_to_colour(field, tc_excluded);
    else if (key == "tc_exclude_circle")
    {
        tc_exclude_circle =
            str_to_colour(field, tc_exclude_circle);
    }
    else if (key == "tc_dangerous")
        tc_dangerous = str_to_colour(field, tc_dangerous);
    else if (key == "tc_disconnected")
        tc_disconnected = str_to_colour(field, tc_disconnected);
    else LIST_OPTION(auto_exclude);
    else BOOL_OPTION(easy_exit_menu);
    else BOOL_OPTION(ability_menu);
    else BOOL_OPTION(dos_use_background_intensity);
    else if (key == "item_stack_summary_minimum")
        item_stack_summary_minimum = atoi(field.c_str());
    else if (key == "explore_stop")
    {
        if (plain)
            explore_stop = ES_NONE;

        const int new_conditions = read_explore_stop_conditions(field);
        if (minus_equal)
            explore_stop &= ~new_conditions;
        else
            explore_stop |= new_conditions;
    }
    else if (key == "explore_stop_prompt")
    {
        if (plain)
            explore_stop_prompt = ES_NONE;
        const int new_conditions = read_explore_stop_conditions(field);
        if (minus_equal)
            explore_stop_prompt &= ~new_conditions;
        else
            explore_stop_prompt |= new_conditions;
    }
    else LIST_OPTION(explore_stop_pickup_ignore);
    else if (key == "explore_item_greed")
    {
        explore_item_greed = atoi(field.c_str());
        if (explore_item_greed > 1000)
            explore_item_greed = 1000;
        else if (explore_item_greed < -1000)
            explore_item_greed = -1000;
    }
    else BOOL_OPTION(explore_greedy);
    else if (key == "explore_wall_bias")
    {
        explore_wall_bias = atoi(field.c_str());
        if (explore_wall_bias > 1000)
            explore_wall_bias = 1000;
        else if (explore_wall_bias < 0)
            explore_wall_bias = 0;
    }
    else BOOL_OPTION(explore_improved);
    else BOOL_OPTION(explore_auto_rest);
    else BOOL_OPTION(travel_key_stop);
    else if (key == "auto_sacrifice")
    {
        if (field == "prompt_ignore")
            auto_sacrifice = AS_PROMPT_IGNORE;
        else if (field == "prompt" || field == "ask")
            auto_sacrifice = AS_PROMPT;
        else if (field == "before_explore")
            auto_sacrifice = AS_BEFORE_EXPLORE;
        else
            auto_sacrifice = _read_bool(field, false) ? AS_YES : AS_NO;
    }
    else if (key == "sound")
    {
        if (plain)
            sound_mappings.clear();

        vector<sound_mapping> new_entries;
        for (const string &sub : split_string(",", field))
        {
            string::size_type cpos = sub.find(":", 0);
            if (cpos != string::npos)
            {
                sound_mapping entry;
                entry.pattern = sub.substr(0, cpos);
                entry.soundfile = sub.substr(cpos + 1);
                if (minus_equal)
                    remove_matching(sound_mappings, entry);
                else
                    new_entries.push_back(entry);
            }
        }
        _merge_lists(sound_mappings, new_entries, caret_equal);
    }
#ifndef TARGET_COMPILER_VC
    // MSVC has a limit on how many if/else if can be chained together.
    else
#endif
    if (key == "menu_colour" || key == "menu_color")
    {
        if (plain)
            menu_colour_mappings.clear();

        vector<colour_mapping> new_entries;
        for (const string &seg : split_string(",", field))
        {
            // Format is "tag:colour:pattern" or "colour:pattern" (default tag).
            // FIXME: arrange so that you can use ':' inside a pattern
            vector<string> subseg = split_string(":", seg, false);
            string tagname, patname, colname;
            if (subseg.size() < 2)
                continue;
            if (subseg.size() >= 3)
            {
                tagname = subseg[0];
                colname = subseg[1];
                patname = subseg[2];
            }
            else
            {
                colname = subseg[0];
                patname = subseg[1];
            }

            colour_mapping mapping;
            mapping.tag     = tagname;
            mapping.pattern = patname;
            const int col = str_to_colour(colname);
            mapping.colour = col;

            if (col == -1)
                continue;
            else if (minus_equal)
                remove_matching(menu_colour_mappings, mapping);
            else
                new_entries.push_back(mapping);
        }
        _merge_lists(menu_colour_mappings, new_entries, caret_equal);
    }
    else if (key == "message_colour" || key == "message_color")
    {
        // TODO: support -= here.
        if (plain)
            message_colour_mappings.clear();

        add_message_colour_mappings(field, caret_equal, minus_equal);
    }
    else if (key == "dump_order")
    {
        if (plain)
            dump_order.clear();

        new_dump_fields(field, !minus_equal, caret_equal);
    }
    else BOOL_OPTION(dump_on_save);
    else if (key == "dump_kill_places")
    {
        dump_kill_places = (field == "none" ? KDO_NO_PLACES :
                            field == "all"  ? KDO_ALL_PLACES
                                            : KDO_ONE_PLACE);
    }
    else if (key == "kill_map")
    {
        // TODO: treat this as a map option (e.g. kill_map.you = friendly)
        if (plain && field.empty())
        {
            kill_map[KC_YOU] = KC_YOU;
            kill_map[KC_FRIENDLY] = KC_FRIENDLY;
            kill_map[KC_OTHER] = KC_OTHER;
        }

        for (const string &s : split_string(",", field))
        {
            string::size_type cpos = s.find(":", 0);
            if (cpos != string::npos)
            {
                string from = s.substr(0, cpos);
                string to   = s.substr(cpos + 1);
                do_kill_map(from, to);
            }
        }
    }
    else BOOL_OPTION(rest_wait_both);
    else INT_OPTION(rest_wait_percent, 0, 100);
    else BOOL_OPTION(cloud_status);
    else if (key == "dump_message_count")
    {
        // Capping is implicit
        dump_message_count = atoi(field.c_str());
    }
    else if (key == "dump_item_origins")
    {
        if (plain)
            dump_item_origins = IODS_PRICE;

        for (const string &ch : split_string(",", field))
        {
            if (ch == "artefacts" || ch == "artifacts"
                || ch == "artefact" || ch == "artifact")
            {
                dump_item_origins |= IODS_ARTEFACTS;
            }
            else if (ch == "ego_arm" || ch == "ego armour"
                     || ch == "ego_armour" || ch == "ego armor"
                     || ch == "ego_armor")
            {
                dump_item_origins |= IODS_EGO_ARMOUR;
            }
            else if (ch == "ego_weap" || ch == "ego weapon"
                     || ch == "ego_weapon" || ch == "ego weapons"
                     || ch == "ego_weapons")
            {
                dump_item_origins |= IODS_EGO_WEAPON;
            }
            else if (ch == "jewellery" || ch == "jewelry")
                dump_item_origins |= IODS_JEWELLERY;
            else if (ch == "runes")
                dump_item_origins |= IODS_RUNES;
            else if (ch == "rods")
                dump_item_origins |= IODS_RODS;
            else if (ch == "staves")
                dump_item_origins |= IODS_STAVES;
            else if (ch == "books")
                dump_item_origins |= IODS_BOOKS;
            else if (ch == "all" || ch == "everything")
                dump_item_origins = IODS_EVERYTHING;
        }
    }
    else if (key == "dump_item_origin_price")
    {
        dump_item_origin_price = atoi(field.c_str());
        if (dump_item_origin_price < -1)
            dump_item_origin_price = -1;
    }
    else BOOL_OPTION(dump_book_spells);
    else INT_OPTION(pickup_menu_limit, INT_MIN, INT_MAX);
    else if (key == "additional_macro_file")
    {
        // TODO: this option could probably be improved. For now, keep the
        // "= means append" behaviour, and don't allow clearing the list;
        // if we rename to "additional_macro_files" then it could work like
        // other list options.
        const string resolved = resolve_include(orig_field, "macro ");
        if (!resolved.empty())
            additional_macro_files.push_back(resolved);
    }
    else if (key == "macros")
    {
        // orig_field because this function wants capitals
        const string possible_error = read_rc_file_macro(orig_field);

        if (possible_error != "")
            report_error(possible_error.c_str(), orig_field.c_str());
    }
#ifdef USE_TILE
    else if (key == "tile_show_items")
        tile_show_items = field;
    else BOOL_OPTION(tile_skip_title);
    else BOOL_OPTION(tile_menu_icons);
    else if (key == "tile_player_col")
        tile_player_col = str_to_tile_colour(field);
    else if (key == "tile_monster_col")
        tile_monster_col = str_to_tile_colour(field);
    else if (key == "tile_plant_col")
        tile_plant_col = str_to_tile_colour(field);
    else if (key == "tile_item_col")
        tile_item_col = str_to_tile_colour(field);
    else if (key == "tile_unseen_col")
        tile_unseen_col = str_to_tile_colour(field);
    else if (key == "tile_floor_col")
        tile_floor_col = str_to_tile_colour(field);
    else if (key == "tile_wall_col")
        tile_wall_col = str_to_tile_colour(field);
    else if (key == "tile_mapped_floor_col")
        tile_mapped_floor_col = str_to_tile_colour(field);
    else if (key == "tile_mapped_wall_col")
        tile_mapped_wall_col = str_to_tile_colour(field);
    else if (key == "tile_door_col")
        tile_door_col = str_to_tile_colour(field);
    else if (key == "tile_downstairs_col")
        tile_downstairs_col = str_to_tile_colour(field);
    else if (key == "tile_upstairs_col")
        tile_upstairs_col = str_to_tile_colour(field);
    else if (key == "tile_branchstairs_col")
        tile_branchstairs_col = str_to_tile_colour(field);
    else if (key == "tile_portal_col")
        tile_portal_col = str_to_tile_colour(field);
    else if (key == "tile_feature_col")
        tile_feature_col = str_to_tile_colour(field);
    else if (key == "tile_trap_col")
        tile_trap_col = str_to_tile_colour(field);
    else if (key == "tile_water_col")
        tile_water_col = str_to_tile_colour(field);
    else if (key == "tile_deep_water_col")
        tile_deep_water_col = str_to_tile_colour(field);
    else if (key == "tile_lava_col")
        tile_lava_col = str_to_tile_colour(field);
    else if (key == "tile_excluded_col")
        tile_excluded_col = str_to_tile_colour(field);
    else if (key == "tile_excl_centre_col" || key == "tile_excl_center_col")
        tile_excl_centre_col = str_to_tile_colour(field);
    else if (key == "tile_window_col")
        tile_window_col = str_to_tile_colour(field);
#ifdef USE_TILE_LOCAL
    else if (key == "tile_font_crt_file")
        tile_font_crt_file = field;
    else if (key == "tile_font_msg_file")
        tile_font_msg_file = field;
    else if (key == "tile_font_stat_file")
        tile_font_stat_file = field;
    else if (key == "tile_font_tip_file")
        tile_font_tip_file = field;
    else if (key == "tile_font_lbl_file")
        tile_font_lbl_file = field;
#endif
#ifdef USE_TILE_WEB
    else if (key == "tile_font_crt_family")
        tile_font_crt_family = field;
    else if (key == "tile_font_msg_family")
        tile_font_msg_family = field;
    else if (key == "tile_font_stat_family")
        tile_font_stat_family = field;
    else if (key == "tile_font_lbl_family")
        tile_font_lbl_family = field;
#endif
    else INT_OPTION(tile_font_crt_size, 0, INT_MAX);
    else INT_OPTION(tile_font_msg_size, 0, INT_MAX);
    else INT_OPTION(tile_font_stat_size, 0, INT_MAX);
    else INT_OPTION(tile_font_tip_size, 0, INT_MAX);
    else INT_OPTION(tile_font_lbl_size, 0, INT_MAX);
#ifdef USE_TILE_LOCAL
#ifdef USE_FT
    else BOOL_OPTION(tile_font_ft_light);
#endif
    else INT_OPTION(tile_key_repeat_delay, 0, INT_MAX);
    else if (key == "tile_full_screen")
        tile_full_screen = (screen_mode)_read_bool(field, tile_full_screen);
    else INT_OPTION(tile_window_width, INT_MIN, INT_MAX);
    else INT_OPTION(tile_window_height, INT_MIN, INT_MAX);
#endif // USE_TILE_LOCAL
#ifdef TOUCH_UI
//    else BOOL_OPTION(tile_use_small_layout);
    else if (key == "tile_use_small_layout")
    {
        if (field == "true")
            tile_use_small_layout = MB_TRUE;
        else if (field == "false")
            tile_use_small_layout = MB_FALSE;
        else
            tile_use_small_layout = MB_MAYBE;
    }
#endif
    else INT_OPTION(tile_cell_pixels, 1, INT_MAX);
    else BOOL_OPTION(tile_filter_scaling);
    else INT_OPTION(tile_map_pixels, 0, INT_MAX);
    else BOOL_OPTION(tile_force_overlay);
    else INT_OPTION(tile_tooltip_ms, 0, INT_MAX);
    else INT_OPTION(tile_update_rate, 50, INT_MAX);
    else INT_OPTION(tile_runrest_rate, 0, INT_MAX);
    else BOOL_OPTION(tile_show_minihealthbar);
    else BOOL_OPTION(tile_show_minimagicbar);
    else BOOL_OPTION(tile_show_demon_tier);
    else BOOL_OPTION(tile_water_anim);
    else BOOL_OPTION(tile_misc_anim);
    else if (key == "tile_show_player_species" && field == "true")
    {
        field = "playermons";
        set_player_tile(field);
    }
    else LIST_OPTION(tile_layout_priority);
    else if (key == "tile_player_tile")
        set_player_tile(field);
    else if (key == "tile_weapon_offsets")
        set_tile_offsets(field, false);
    else if (key == "tile_shield_offsets")
        set_tile_offsets(field, true);
    else if (key == "tile_tag_pref")
        tile_tag_pref = _str_to_tag_pref(field.c_str());
#ifdef USE_TILE_WEB
    else BOOL_OPTION(tile_realtime_anim);
    else if (key == "tile_display_mode")
    {
        if (field == "tiles" || field == "glyphs" || field == "hybrid")
            tile_display_mode = field;
    }
    else BOOL_OPTION(tile_level_map_hide_messages);
    else BOOL_OPTION(tile_level_map_hide_sidebar);
#endif
#endif // USE_TILE

    else if (key == "bindkey")
        _bindkey(field);
    else if (key == "constant")
    {
        if (!variables.count(field))
            report_error("No variable named '%s' to make constant", field.c_str());
        else if (constants.count(field))
            report_error("'%s' is already a constant", field.c_str());
        else
            constants.insert(field);
    }
    else INT_OPTION(view_delay, 0, INT_MAX);
    else BOOL_OPTION(arena_dump_msgs);
    else BOOL_OPTION(arena_dump_msgs_all);
    else BOOL_OPTION(arena_list_eq);
    else INT_OPTION(fail_severity_to_confirm, -1, 3);

    // Catch-all else, copies option into map
    else if (runscript)
    {
#ifdef CLUA_BINDINGS
        int setmode = 0;
        if (plus_equal)
            setmode = 1;
        if (minus_equal)
            setmode = -1;
        if (caret_equal)
            setmode = 2;

        if (!clua.callbooleanfn(false, "c_process_lua_option", "ssd",
                        key.c_str(), orig_field.c_str(), setmode))
#endif
        {
#ifdef CLUA_BINDINGS
            if (!clua.error.empty())
                mprf(MSGCH_ERROR, "Lua error: %s", clua.error.c_str());
#endif
            named_options[key] = orig_field;
        }
    }
}

static const map<string, flang_t> fake_lang_names = {
    { "dwarven", FLANG_DWARVEN },
    { "dwarf", FLANG_DWARVEN },

    { "jäger", FLANG_JAGERKIN },
    { "jägerkin", FLANG_JAGERKIN },
    { "jager", FLANG_JAGERKIN },
    { "jagerkin", FLANG_JAGERKIN },
    { "jaeger", FLANG_JAGERKIN },
    { "jaegerkin", FLANG_JAGERKIN },

    // Due to a historical conflict with actual german, slang names are
    // supported. Not the really rude ones, though.
    { "de", FLANG_KRAUT },
    { "german", FLANG_KRAUT },
    { "kraut", FLANG_KRAUT },
    { "jerry", FLANG_KRAUT },
    { "fritz", FLANG_KRAUT },

    { "futhark", FLANG_FUTHARK },
    { "runes", FLANG_FUTHARK },
    { "runic", FLANG_FUTHARK },

    { "wide", FLANG_WIDE },
    { "doublewidth", FLANG_WIDE },
    { "fullwidth", FLANG_WIDE },

    { "grunt", FLANG_GRUNT },
    { "sgrunt", FLANG_GRUNT },
    { "!!!", FLANG_GRUNT },

    { "butt", FLANG_BUTT },
    { "buttbot", FLANG_BUTT },
    { "tef", FLANG_BUTT },
};

bool game_options::set_lang(const char *lc)
{
    if (!lc)
        return false;

    if (lc[0] && lc[1] && (lc[2] == '_' || lc[2] == '-'))
        return set_lang(string(lc, 2).c_str());

    const string l = lowercase_string(lc); // Windows returns it capitalized.
    if (l == "en" || l == "english")
        language = LANG_EN, lang_name = 0; // disable the db
    else if (l == "cs" || l == "czech" || l == "český" || l == "cesky")
        language = LANG_CS, lang_name = "cs";
    else if (l == "da" || l == "danish" || l == "dansk")
        language = LANG_DA, lang_name = "da";
    else if (l == "de" || l == "german" || l == "deutsch")
        language = LANG_DE, lang_name = "de";
    else if (l == "el" || l == "greek" || l == "ελληνικά" || l == "ελληνικα")
        language = LANG_EL, lang_name = "el";
    else if (l == "es" || l == "spanish" || l == "español" || l == "espanol")
        language = LANG_ES, lang_name = "es";
    else if (l == "fi" || l == "finnish" || l == "suomi")
        language = LANG_FI, lang_name = "fi";
    else if (l == "fr" || l == "french" || l == "français" || l == "francais")
        language = LANG_FR, lang_name = "fr";
    else if (l == "hu" || l == "hungarian" || l == "magyar")
        language = LANG_HU, lang_name = "hu";
    else if (l == "it" || l == "italian" || l == "italiano")
        language = LANG_IT, lang_name = "it";
    else if (l == "ja" || l == "japanese" || l == "日本人")
        language = LANG_JA, lang_name = "ja";
    else if (l == "ko" || l == "korean" || l == "한국의")
        language = LANG_KO, lang_name = "ko";
    else if (l == "lt" || l == "lithuanian" || l == "lietuvos")
        language = LANG_LT, lang_name = "lt";
    else if (l == "lv" || l == "latvian" || l == "lettish"
             || l == "latvijas" || l == "latviešu"
             || l == "latvieshu" || l == "latviesu")
    {
        language = LANG_LV, lang_name = "lv";
    }
    else if (l == "nl" || l == "dutch" || l == "nederlands")
        language = LANG_NL, lang_name = "nl";
    else if (l == "pl" || l == "polish" || l == "polski")
        language = LANG_PL, lang_name = "pl";
    else if (l == "pt" || l == "portuguese" || l == "português" || l == "portugues")
        language = LANG_PT, lang_name = "pt";
    else if (l == "ru" || l == "russian" || l == "русский" || l == "русскии")
        language = LANG_RU, lang_name = "ru";
    else if (l == "sv" || l == "swedish" || l == "svenska")
        language = LANG_SV, lang_name = "sv";
    else if (l == "zh" || l == "chinese" || l == "中国的" || l == "中國的")
        language = LANG_ZH, lang_name = "zh";
    else if (const flang_t * const flang = map_find(fake_lang_names, l))
    {
        // Handle fake languages for backwards-compatibility with old rcs.
        // Override rather than stack, because that's how it used to work.
        fake_langs = { { *flang, -1 } };
    }
    else
        return false;
    return true;
}

/**
 * Apply the player's fake language settings.
 *
 * @param input     The value of the "fake_lang" field.
 */
void game_options::set_fake_langs(const string &input)
{
    fake_langs.clear();
    for (const string &flang_text : split_string(",", input))
    {
        const int MAX_LANGS = 3;
        if (fake_langs.size() >= (size_t) MAX_LANGS)
        {
            report_error("Too many fake langs; maximum is %d", MAX_LANGS);
            break;
        }

        const vector<string> split_flang = split_string(":", flang_text);
        const string flang_name = split_flang[0];
        if (split_flang.size() > 2)
        {
            report_error("Invalid fake-lang format: %s", flang_text.c_str());
            continue;
        }

        int tval = -1;
        const int value = split_flang.size() >= 2
                          && parse_int(split_flang[1].c_str(), tval) ? tval : -1;

        const flang_t *flang = map_find(fake_lang_names, flang_name);
        if (flang)
        {
            if (split_flang.size() >= 2)
            {
                if (*flang != FLANG_BUTT)
                {
                    report_error("Lang %s doesn't take a value",
                                 flang_name.c_str());
                    continue;
                }

                if (value == -1)
                {
                    report_error("Invalid value '%s' provided for lang",
                                 split_flang[1].c_str());
                    continue;
                }
            }

            fake_langs.push_back({*flang, value});
        }
        else
            report_error("Unknown language %s!", flang_name.c_str());

    }
}

// Checks an include file name for safety and resolves it to a readable path.
// If file cannot be resolved, returns the empty string (this does not throw!)
// If file can be resolved, returns the resolved path.
/// @throws unsafe_path if included_file fails the safety check.
string game_options::resolve_include(string parent_file, string included_file,
                                     const vector<string> *rcdirs)
{
    // Before we start, make sure we convert forward slashes to the platform's
    // favoured file separator.
    parent_file   = canonicalise_file_separator(parent_file);
    included_file = canonicalise_file_separator(included_file);

    // How we resolve include paths:
    // 1. If it's an absolute path, use it directly.
    // 2. Try the name relative to the parent filename, if supplied.
    // 3. Try the name relative to any of the provided rcdirs.
    // 4. Try locating the name as a regular data file (also checks for the
    //    file name relative to the current directory).
    // 5. Fail, and return empty string.

    assert_read_safe_path(included_file);

    // There's only so much you can do with an absolute path.
    // Note: absolute paths can only get here if we're not on a
    // multiuser system. On multiuser systems assert_read_safe_path()
    // will throw an exception if it sees absolute paths.
    if (is_absolute_path(included_file))
        return file_exists(included_file)? included_file : "";

    if (!parent_file.empty())
    {
        const string candidate = get_path_relative_to(parent_file,
                                                      included_file);
        if (file_exists(candidate))
            return candidate;
    }

    if (rcdirs)
    {
        for (const string &dir : *rcdirs)
        {
            const string candidate(catpath(dir, included_file));
            if (file_exists(candidate))
                return candidate;
        }
    }

    return datafile_path(included_file, false, true);
}

string game_options::resolve_include(const string &file, const char *type)
{
    try
    {
        const string resolved = resolve_include(filename, file, &SysEnv.rcdirs);

        if (resolved.empty())
            report_error("Cannot find %sfile \"%s\".", type, file.c_str());
        return resolved;
    }
    catch (const unsafe_path &err)
    {
        report_error("Cannot include %sfile: %s", type, err.what());
        return "";
    }
}

bool game_options::was_included(const string &file) const
{
    return included.count(file);
}

void game_options::include(const string &rawfilename, bool resolve,
                           bool runscript)
{
    const string include_file = resolve ? resolve_include(rawfilename)
                                        : rawfilename;

    if (was_included(include_file))
        return;

    included.insert(include_file);

    // Change filename to the included filename while we're reading it.
    unwind_var<string> optfile(filename, include_file);
    unwind_var<string> basefile(basefilename, rawfilename);

    // Save current line number
    unwind_var<int> currlinenum(line_num, 0);

    // Also unwind any aliases defined in included files.
    unwind_var<string_map> unwalias(aliases);

    FileLineInput fl(include_file.c_str());
    if (!fl.error())
        read_options(fl, runscript, false);
}

void game_options::report_error(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    string error = vmake_stringf(format, args);
    va_end(args);

    // If called before game starts, log a startup error,
    // otherwise spam the warning channel.
    if (crawl_state.need_save)
    {
        mprf(MSGCH_ERROR, "Warning: %s (%s:%d)", error.c_str(),
             basefilename.c_str(), line_num);
    }
    else
    {
        crawl_state.add_startup_error(make_stringf("%s (%s:%d)",
                                                   error.c_str(),
                                                   basefilename.c_str(),
                                                   line_num));
    }
}

static string check_string(const char *s)
{
    return s? s : "";
}

void get_system_environment()
{
    // The player's name
    SysEnv.crawl_name = check_string(getenv("CRAWL_NAME"));

    // The directory which contians init.txt, macro.txt, morgue.txt
    // This should end with the appropriate path delimiter.
    SysEnv.crawl_dir = check_string(getenv("CRAWL_DIR"));

#ifdef SAVE_DIR_PATH
    if (SysEnv.crawl_dir == "")
        SysEnv.crawl_dir = SAVE_DIR_PATH;
#endif

#ifdef DGL_SIMPLE_MESSAGING
    // Enable DGL_SIMPLE_MESSAGING only if SIMPLEMAIL and MAIL are set.
    const char *simplemail = getenv("SIMPLEMAIL");
    if (simplemail && strcmp(simplemail, "0"))
    {
        const char *mail = getenv("MAIL");
        SysEnv.messagefile = mail? mail : "";
    }
#endif

    // The full path to the init file -- this overrides CRAWL_DIR.
    SysEnv.crawl_rc = check_string(getenv("CRAWL_RC"));

#ifdef UNIX
    // The user's home directory (used to look for ~/.crawlrc file)
    SysEnv.home = check_string(getenv("HOME"));
#endif
}

static void set_crawl_base_dir(const char *arg)
{
    if (!arg)
        return;

    SysEnv.crawl_base = get_parent_directory(arg);
}

// parse args, filling in Options and game environment as we go.
// returns true if no unknown or malformed arguments were found.

// Keep this in sync with the option names.
enum commandline_option_type
{
    CLO_SCORES,
    CLO_NAME,
    CLO_RACE,
    CLO_CLASS,
    CLO_DIR,
    CLO_RC,
    CLO_RCDIR,
    CLO_TSCORES,
    CLO_VSCORES,
    CLO_SCOREFILE,
    CLO_MORGUE,
    CLO_MACRO,
    CLO_MAPSTAT,
    CLO_OBJSTAT,
    CLO_ITERATIONS,
    CLO_ARENA,
    CLO_DUMP_MAPS,
    CLO_TEST,
    CLO_SCRIPT,
    CLO_BUILDDB,
    CLO_HELP,
    CLO_VERSION,
    CLO_SEED,
    CLO_SAVE_VERSION,
    CLO_SPRINT,
    CLO_EXTRA_OPT_FIRST,
    CLO_EXTRA_OPT_LAST,
    CLO_SPRINT_MAP,
    CLO_EDIT_SAVE,
    CLO_PRINT_CHARSET,
    CLO_TUTORIAL,
    CLO_WIZARD,
    CLO_EXPLORE,
    CLO_NO_SAVE,
    CLO_GDB,
    CLO_NO_GDB, CLO_NOGDB,
    CLO_THROTTLE,
    CLO_NO_THROTTLE,
    CLO_PLAYABLE_JSON, // JSON metadata for species, jobs, combos.
#ifdef USE_TILE_WEB
    CLO_WEBTILES_SOCKET,
    CLO_AWAIT_CONNECTION,
    CLO_PRINT_WEBTILES_OPTIONS,
#endif

    CLO_NOPS
};

static const char *cmd_ops[] =
{
    "scores", "name", "species", "background", "dir", "rc",
    "rcdir", "tscores", "vscores", "scorefile", "morgue", "macro",
    "mapstat", "objstat", "iters", "arena", "dump-maps", "test", "script",
    "builddb", "help", "version", "seed", "save-version", "sprint",
    "extra-opt-first", "extra-opt-last", "sprint-map", "edit-save",
    "print-charset", "tutorial", "wizard", "explore", "no-save",
    "gdb", "no-gdb", "nogdb", "throttle", "no-throttle",
    "playable-json",
#ifdef USE_TILE_WEB
    "webtiles-socket", "await-connection", "print-webtiles-options",
#endif
};

static const int num_cmd_ops = CLO_NOPS;
static bool arg_seen[num_cmd_ops];

static string _find_executable_path()
{
    // A lot of OSes give ways to find the location of the running app's
    // binary executable. This is useful, because argv[0] can be relative
    // when we really need an absolute path in order to locate the game's
    // resources.
#if defined (TARGET_OS_WINDOWS)
    wchar_t tempPath[MAX_PATH];
    if (GetModuleFileNameW(nullptr, tempPath, MAX_PATH))
        return utf16_to_8(tempPath);
    else
        return "";
#elif defined (TARGET_OS_LINUX) || defined (TARGET_OS_CYGWIN)
    char tempPath[2048];
    const ssize_t rsize =
        readlink("/proc/self/exe", tempPath, sizeof(tempPath) - 1);
    if (rsize > 0)
    {
        tempPath[rsize] = 0;
        return mb_to_utf8(tempPath);
    }
    return "";
#elif defined (TARGET_OS_MACOSX)
    return mb_to_utf8(NXArgv[0]);
#else
    // We don't know how to find the executable's path on this OS.
    return "";
#endif
}

static void _print_version()
{
    printf("Crawl version %s%s", Version::Long, "\n");
    printf("Save file version %d.%d%s", TAG_MAJOR_VERSION, TAG_MINOR_VERSION, "\n");
    printf("%s", compilation_info);
}

static void _print_save_version(char *name)
{
    try
    {
        string filename = name;
        // Check for the exact filename first, then go by char name.
        if (!file_exists(filename))
            filename = get_savedir_filename(filename);
        package save(filename.c_str(), false);
        reader chrf(&save, "chr");

        int major, minor;
        if (!get_save_version(chrf, major, minor))
            fail("Save file is invalid.");
        else
            printf("Save file version for %s is %d.%d\n", name, major, minor);
    }
    catch (ext_fail_exception &fe)
    {
        fprintf(stderr, "Error: %s\n", fe.what());
    }
}

enum es_command_type
{
    ES_LS,
    ES_RM,
    ES_GET,
    ES_PUT,
    ES_REPACK,
    ES_INFO,
    NUM_ES
};

static struct es_command
{
    es_command_type cmd;
    const char* name;
    bool rw;
    int min_args, max_args;
} es_commands[] =
{
    { ES_LS,      "ls",      false, 0, 0, },
    { ES_GET,     "get",     false, 1, 2, },
    { ES_PUT,     "put",     true,  1, 2, },
    { ES_RM,      "rm",      true,  1, 1, },
    { ES_REPACK,  "repack",  false, 0, 0, },
    { ES_INFO,    "info",    false, 0, 0, },
};

#define FAIL(...) do { fprintf(stderr, __VA_ARGS__); return; } while (0)
static void _edit_save(int argc, char **argv)
{
    if (argc <= 1 || !strcmp(argv[1], "help"))
    {
        printf("Usage: crawl --edit-save <name> <command>, where <command> may be:\n"
               "  ls                          list the chunks\n"
               "  get <chunk> [<chunkfile>]   extract a chunk into <chunkfile>\n"
               "  put <chunk> [<chunkfile>]   import a chunk from <chunkfile>\n"
               "     <chunkfile> defaults to \"chunk\"; use \"-\" for stdout/stdin\n"
               "  rm <chunk>                  delete a chunk\n"
               "  repack                      defrag and reclaim unused space\n"
             );
        return;
    }
    const char *name = argv[0];
    const char *cmdn = argv[1];

    es_command_type cmd = NUM_ES;
    bool rw;

    for (const es_command &ec : es_commands)
        if (!strcmp(ec.name, cmdn))
        {
            if (argc < ec.min_args + 2)
                FAIL("Too few arguments for %s.\n", cmdn);
            else if (argc > ec.max_args + 2)
                FAIL("Too many arguments for %s.\n", cmdn);
            cmd = ec.cmd;
            rw = ec.rw;
            break;
        }
    if (cmd == NUM_ES)
        FAIL("Unknown command: %s.\n", cmdn);

    try
    {
        string filename = name;
        // Check for the exact filename first, then go by char name.
        if (!file_exists(filename))
            filename = get_savedir_filename(filename);
        package save(filename.c_str(), rw);

        if (cmd == ES_LS)
        {
            vector<string> list = save.list_chunks();
            sort(list.begin(), list.end(), numcmpstr);
            for (const string &s : list)
                printf("%s\n", s.c_str());
        }
        else if (cmd == ES_GET)
        {
            const char *chunk = argv[2];
            if (!*chunk || strlen(chunk) > MAX_CHUNK_NAME_LENGTH)
                FAIL("Invalid chunk name \"%s\".\n", chunk);
            if (!save.has_chunk(chunk))
                FAIL("No such chunk in the save file.\n");
            chunk_reader inc(&save, chunk);

            const char *file = (argc == 4) ? argv[3] : "chunk";
            FILE *f;
            if (strcmp(file, "-"))
                f = fopen_u(file, "wb");
            else
                f = stdout;
            if (!f)
                sysfail("Can't open \"%s\" for writing", file);

            char buf[16384];
            while (size_t s = inc.read(buf, sizeof(buf)))
                if (fwrite(buf, 1, s, f) != s)
                    sysfail("Error writing \"%s\"", file);

            if (f != stdout)
                if (fclose(f))
                    sysfail("Write error on close of \"%s\"", file);
        }
        else if (cmd == ES_PUT)
        {
            const char *chunk = argv[2];
            if (!*chunk || strlen(chunk) > MAX_CHUNK_NAME_LENGTH)
                FAIL("Invalid chunk name \"%s\".\n", chunk);

            const char *file = (argc == 4) ? argv[3] : "chunk";
            FILE *f;
            if (strcmp(file, "-"))
                f = fopen_u(file, "rb");
            else
                f = stdin;
            if (!f)
                sysfail("Can't read \"%s\"", file);
            chunk_writer outc(&save, chunk);

            char buf[16384];
            while (size_t s = fread(buf, 1, sizeof(buf), f))
                outc.write(buf, s);
            if (ferror(f))
                sysfail("Error reading \"%s\"", file);

            if (f != stdin)
                fclose(f);
        }
        else if (cmd == ES_RM)
        {
            const char *chunk = argv[2];
            if (!*chunk || strlen(chunk) > MAX_CHUNK_NAME_LENGTH)
                FAIL("Invalid chunk name \"%s\".\n", chunk);
            if (!save.has_chunk(chunk))
                FAIL("No such chunk in the save file.\n");

            save.delete_chunk(chunk);
        }
        else if (cmd == ES_REPACK)
        {
            package save2((filename + ".tmp").c_str(), true, true);
            for (const string &chunk : save.list_chunks())
            {
                char buf[16384];

                chunk_reader in(&save, chunk);
                chunk_writer out(&save2, chunk);

                while (plen_t s = in.read(buf, sizeof(buf)))
                    out.write(buf, s);
            }
            save2.commit();
            save.unlink();
            rename_u((filename + ".tmp").c_str(), filename.c_str());
        }
        else if (cmd == ES_INFO)
        {
            vector<string> list = save.list_chunks();
            sort(list.begin(), list.end(), numcmpstr);
            plen_t nchunks = list.size();
            plen_t frag = save.get_chunk_fragmentation("");
            plen_t flen = save.get_size();
            plen_t slack = save.get_slack();
            printf("Chunks: (size compressed/uncompressed, fragments, name)\n");
            for (const string &chunk : list)
            {
                int cfrag = save.get_chunk_fragmentation(chunk);
                frag += cfrag;
                int cclen = save.get_chunk_compressed_length(chunk);

                char buf[16384];
                chunk_reader in(&save, chunk);
                plen_t clen = 0;
                while (plen_t s = in.read(buf, sizeof(buf)))
                    clen += s;
                printf("%7d/%7d %3u %s\n", cclen, clen, cfrag, chunk.c_str());
            }
            // the directory is not a chunk visible from the outside
            printf("Fragmentation:    %u/%u (%4.2f)\n", frag, nchunks + 1,
                   ((float)frag) / (nchunks + 1));
            printf("Unused space:     %u/%u (%u%%)\n", slack, flen,
                   100 - (100 * (flen - slack) / flen));
            // there's also wasted space due to fragmentation, but since
            // it's linear, there's no need to print it
        }
    }
    catch (ext_fail_exception &fe)
    {
        fprintf(stderr, "Error: %s\n", fe.what());
    }
}
#undef FAIL

#ifdef USE_TILE_WEB
static void _write_colour_list(const vector<pair<int, int> > variable,
        const string &name)
{
    tiles.json_open_array(name);
    for (const auto &entry : variable)
    {
        tiles.json_open_object();
        tiles.json_write_int("value", entry.first);
        tiles.json_write_string("colour", colour_to_str(entry.second));
        tiles.json_close_object();
    }
    tiles.json_close_array();
}

static void _write_vcolour(const string &name, VColour colour)
{
    tiles.json_open_object(name);
    tiles.json_write_int("r", colour.r);
    tiles.json_write_int("g", colour.g);
    tiles.json_write_int("b", colour.b);
    if (colour.a != 255)
        tiles.json_write_int("a", colour.a);
    tiles.json_close_object();
}

static void _write_minimap_colours()
{
    _write_vcolour("tile_player_col", Options.tile_player_col);
    _write_vcolour("tile_monster_col", Options.tile_monster_col);
    _write_vcolour("tile_plant_col", Options.tile_plant_col);
    _write_vcolour("tile_item_col", Options.tile_item_col);
    _write_vcolour("tile_unseen_col", Options.tile_unseen_col);
    _write_vcolour("tile_floor_col", Options.tile_floor_col);
    _write_vcolour("tile_wall_col", Options.tile_wall_col);
    _write_vcolour("tile_mapped_floor_col", Options.tile_mapped_floor_col);
    _write_vcolour("tile_mapped_wall_col", Options.tile_mapped_wall_col);
    _write_vcolour("tile_door_col", Options.tile_door_col);
    _write_vcolour("tile_downstairs_col", Options.tile_downstairs_col);
    _write_vcolour("tile_upstairs_col", Options.tile_upstairs_col);
    _write_vcolour("tile_branchstairs_col", Options.tile_branchstairs_col);
    _write_vcolour("tile_portal_col", Options.tile_portal_col);
    _write_vcolour("tile_feature_col", Options.tile_feature_col);
    _write_vcolour("tile_trap_col", Options.tile_trap_col);
    _write_vcolour("tile_water_col", Options.tile_water_col);
    _write_vcolour("tile_deep_water_col", Options.tile_deep_water_col);
    _write_vcolour("tile_lava_col", Options.tile_lava_col);
    _write_vcolour("tile_excluded_col", Options.tile_excluded_col);
    _write_vcolour("tile_excl_centre_col", Options.tile_excl_centre_col);
    _write_vcolour("tile_window_col", Options.tile_window_col);
}

void game_options::write_webtiles_options(const string& name)
{
    tiles.json_open_object(name);

    _write_colour_list(Options.hp_colour, "hp_colour");
    _write_colour_list(Options.mp_colour, "mp_colour");
    _write_colour_list(Options.stat_colour, "stat_colour");

    tiles.json_write_bool("tile_show_minihealthbar",
                          Options.tile_show_minihealthbar);
    tiles.json_write_bool("tile_show_minimagicbar",
                          Options.tile_show_minimagicbar);
    tiles.json_write_bool("tile_show_demon_tier",
                          Options.tile_show_demon_tier);

    tiles.json_write_int("tile_map_pixels", Options.tile_map_pixels);

    tiles.json_write_string("tile_display_mode", Options.tile_display_mode);
    tiles.json_write_int("tile_cell_pixels", Options.tile_cell_pixels);
    tiles.json_write_bool("tile_filter_scaling", Options.tile_filter_scaling);
    tiles.json_write_bool("tile_water_anim", Options.tile_water_anim);
    tiles.json_write_bool("tile_misc_anim", Options.tile_misc_anim);
    tiles.json_write_bool("tile_realtime_anim", Options.tile_realtime_anim);
    tiles.json_write_bool("tile_level_map_hide_messages",
            Options.tile_level_map_hide_messages);
    tiles.json_write_bool("tile_level_map_hide_sidebar",
            Options.tile_level_map_hide_sidebar);

    tiles.json_write_string("tile_font_crt_family",
            Options.tile_font_crt_family);
    tiles.json_write_string("tile_font_stat_family",
            Options.tile_font_stat_family);
    tiles.json_write_string("tile_font_msg_family",
            Options.tile_font_msg_family);
    tiles.json_write_string("tile_font_lbl_family",
            Options.tile_font_lbl_family);
    tiles.json_write_int("tile_font_crt_size", Options.tile_font_crt_size);
    tiles.json_write_int("tile_font_stat_size", Options.tile_font_stat_size);
    tiles.json_write_int("tile_font_msg_size", Options.tile_font_msg_size);
    tiles.json_write_int("tile_font_lbl_size", Options.tile_font_lbl_size);

    tiles.json_write_bool("show_game_turns", Options.show_game_turns);

    _write_minimap_colours();

    tiles.json_close_object();
}

static void _print_webtiles_options()
{
    Options.write_webtiles_options("");
    printf("%s\n", tiles.get_message().c_str());
}
#endif

static bool _check_extra_opt(char* _opt)
{
    string opt(_opt);
    trim_string(opt);

    if (opt[0] == ':' || opt[0] == '<' || opt[0] == '{'
        || starts_with(opt, "L<") || starts_with(opt, "Lua{"))
    {
        fprintf(stderr, "An extra option can't use Lua (%s)\n",
                _opt);
        return false;
    }

    if (opt[0] == '#')
    {
        fprintf(stderr, "An extra option can't be a comment (%s)\n",
                _opt);
        return false;
    }

    if (opt.find_first_of('=') == string::npos)
    {
        fprintf(stderr, "An extra opt must contain a '=' (%s)\n",
                _opt);
        return false;
    }

    vector<string> parts = split_string(opt, "=");
    if (opt.find_first_of('=') == 0 || parts[0].length() == 0)
    {
        fprintf(stderr, "An extra opt must have an option name (%s)\n",
                _opt);
        return false;
    }

    return true;
}

bool parse_args(int argc, char **argv, bool rc_only)
{
    COMPILE_CHECK(ARRAYSZ(cmd_ops) == CLO_NOPS);

    if (crawl_state.command_line_arguments.empty())
    {
        crawl_state.command_line_arguments.insert(
            crawl_state.command_line_arguments.end(),
            argv, argv + argc);
    }

    string exe_path = _find_executable_path();

    if (!exe_path.empty())
        set_crawl_base_dir(exe_path.c_str());
    else
        set_crawl_base_dir(argv[0]);

    SysEnv.crawl_exe = get_base_filename(argv[0]);

    SysEnv.rcdirs.clear();
    SysEnv.map_gen_iters = 0;

    if (argc < 2)           // no args!
        return true;

    char *arg, *next_arg;
    int current = 1;
    bool nextUsed = false;
    int ecount;

    // initialise
    for (int i = 0; i < num_cmd_ops; i++)
         arg_seen[i] = false;

    if (SysEnv.cmd_args.empty())
    {
        for (int i = 1; i < argc; ++i)
            SysEnv.cmd_args.emplace_back(argv[i]);
    }

    while (current < argc)
    {
        // get argument
        arg = argv[current];

        // next argument (if there is one)
        if (current+1 < argc)
            next_arg = argv[current+1];
        else
            next_arg = nullptr;

        nextUsed = false;

        // arg MUST begin with '-'
        char c = arg[0];
        if (c != '-')
        {
            fprintf(stderr,
                    "Option '%s' is invalid; options must be prefixed "
                    "with -\n\n", arg);
            return false;
        }

        // Look for match (we accept both -option and --option).
        if (arg[1] == '-')
            arg = &arg[2];
        else
            arg = &arg[1];

        // Mac app bundle executables get a process serial number
        if (strncmp(arg, "psn_", 4) == 0)
        {
            current++;
            continue;
        }

        int o;
        for (o = 0; o < num_cmd_ops; o++)
            if (strcasecmp(cmd_ops[o], arg) == 0)
                break;

        // Print the list of commandline options for "--help".
        if (o == CLO_HELP)
            return false;

        if (o == num_cmd_ops)
        {
            fprintf(stderr,
                    "Unknown option: %s\n\n", argv[current]);
            return false;
        }

        // Disallow options specified more than once.
        if (arg_seen[o])
        {
            fprintf(stderr, "Duplicate option: %s\n\n", argv[current]);
            return false;
        }

        // Set arg to 'seen'.
        arg_seen[o] = true;

        // Partially parse next argument.
        bool next_is_param = false;
        if (next_arg != nullptr
            && (next_arg[0] != '-' || strlen(next_arg) == 1))
        {
            next_is_param = true;
        }

        // Take action according to the cmd chosen.
        switch (o)
        {
        case CLO_SCORES:
        case CLO_TSCORES:
        case CLO_VSCORES:
            if (!next_is_param)
                ecount = -1;            // default
            else // optional number given
            {
                ecount = atoi(next_arg);
                if (ecount < 1)
                    ecount = 1;

                if (ecount > SCORE_FILE_ENTRIES)
                    ecount = SCORE_FILE_ENTRIES;

                nextUsed = true;
            }

            if (!rc_only)
            {
                Options.sc_entries = ecount;

                if (o == CLO_TSCORES)
                    Options.sc_format = SCORE_TERSE;
                else if (o == CLO_VSCORES)
                    Options.sc_format = SCORE_VERBOSE;
                else if (o == CLO_SCORES)
                    Options.sc_format = SCORE_REGULAR;
            }
            break;

        case CLO_MAPSTAT:
        case CLO_OBJSTAT:
#ifdef DEBUG_STATISTICS
            if (o == CLO_MAPSTAT)
                crawl_state.map_stat_gen = true;
            else
                crawl_state.obj_stat_gen = true;
#ifdef USE_TILE_LOCAL
            crawl_state.tiles_disabled = true;
#endif

            if (!SysEnv.map_gen_iters)
                SysEnv.map_gen_iters = 100;
            if (next_is_param)
            {
                SysEnv.map_gen_range.reset(new depth_ranges);
                try
                {
                    *SysEnv.map_gen_range =
                        depth_ranges::parse_depth_ranges(next_arg);
                }
                catch (const bad_level_id &err)
                {
                    fprintf(stderr, "Error parsing depths: %s\n", err.what());
                    end(1);
                }
                nextUsed = true;
            }
            break;
#else
            fprintf(stderr, "mapstat and objstat are available only in "
                    "DEBUG_STATISTICS builds.\n");
            end(1);
#endif
        case CLO_ITERATIONS:
#ifdef DEBUG_STATISTICS
            if (!next_is_param || !isadigit(*next_arg))
            {
                fprintf(stderr, "Integer argument required for -%s\n", arg);
                end(1);
            }
            else
            {
                SysEnv.map_gen_iters = atoi(next_arg);
                if (SysEnv.map_gen_iters < 1)
                    SysEnv.map_gen_iters = 1;
                else if (SysEnv.map_gen_iters > 10000)
                    SysEnv.map_gen_iters = 10000;
                nextUsed = true;
            }
#else
            fprintf(stderr, "mapstat and objstat are available only in "
                    "DEBUG_STATISTICS builds.\n");
            end(1);
#endif
            break;

        case CLO_ARENA:
            if (!rc_only)
            {
                Options.game.type = GAME_TYPE_ARENA;
                Options.restart_after_game = false;
            }
            if (next_is_param)
            {
                if (!rc_only)
                    Options.game.arena_teams = next_arg;
                nextUsed = true;
            }
            break;

        case CLO_DUMP_MAPS:
            crawl_state.dump_maps = true;
            break;

        case CLO_PLAYABLE_JSON:
            fprintf(stdout, "%s", playable_metadata_json().c_str());
            end(0);

        case CLO_TEST:
            crawl_state.test = true;
            if (next_is_param)
            {
                if (!(crawl_state.test_list = !strcmp(next_arg, "list")))
                    crawl_state.tests_selected = split_string(",", next_arg);
                nextUsed = true;
            }
            break;

        case CLO_SCRIPT:
            crawl_state.test   = true;
            crawl_state.script = true;
            if (current < argc - 1)
            {
                crawl_state.tests_selected = split_string(",", next_arg);
                for (int extra = current + 2; extra < argc; ++extra)
                    crawl_state.script_args.emplace_back(argv[extra]);
                current = argc;
            }
            else
                end(1, false,
                    "-script must specify comma-separated script names");
            break;

        case CLO_BUILDDB:
            if (next_is_param)
                return false;
            crawl_state.build_db = true;
#ifdef USE_TILE_LOCAL
            crawl_state.tiles_disabled = true;
#endif
            break;

        case CLO_GDB:
            crawl_state.no_gdb = 0;
            break;

        case CLO_NO_GDB:
        case CLO_NOGDB:
            crawl_state.no_gdb = "GDB disabled via the command line.";
            break;

        case CLO_MACRO:
            if (!next_is_param)
                return false;
            SysEnv.macro_dir = next_arg;
            nextUsed = true;
            break;

        case CLO_MORGUE:
            if (!next_is_param)
                return false;
            if (!rc_only)
                SysEnv.morgue_dir = next_arg;
            nextUsed = true;
            break;

        case CLO_SCOREFILE:
            if (!next_is_param)
                return false;
            if (!rc_only)
                SysEnv.scorefile = next_arg;
            nextUsed = true;
            break;

        case CLO_NAME:
            if (!next_is_param)
                return false;
            if (!rc_only)
                Options.game.name = next_arg;
            nextUsed = true;
            break;

        case CLO_RACE:
        case CLO_CLASS:
            if (!next_is_param)
                return false;

            if (!rc_only)
            {
                if (o == 2)
                    Options.game.species = _str_to_species(string(next_arg));

                if (o == 3)
                    Options.game.job = str_to_job(string(next_arg));
            }
            nextUsed = true;
            break;

        case CLO_RCDIR:
            // Always parse.
            if (!next_is_param)
                return false;

            SysEnv.add_rcdir(next_arg);
            nextUsed = true;
            break;

        case CLO_DIR:
            // Always parse.
            if (!next_is_param)
                return false;

            SysEnv.crawl_dir = next_arg;
            nextUsed = true;
            break;

        case CLO_RC:
            // Always parse.
            if (!next_is_param)
                return false;

            SysEnv.crawl_rc = next_arg;
            nextUsed = true;
            break;

        case CLO_HELP:
            // Shouldn't happen.
            return false;

        case CLO_VERSION:
            _print_version();
            end(0);

        case CLO_SAVE_VERSION:
            // Always parse.
            if (!next_is_param)
                return false;

            _print_save_version(next_arg);
            end(0);

        case CLO_EDIT_SAVE:
            // Always parse.
            _edit_save(argc - current - 1, argv + current + 1);
            end(0);

        case CLO_SEED:
            if (!next_is_param)
                return false;

            if (!sscanf(next_arg, "%x", &Options.seed))
                return false;
            nextUsed = true;
            break;

        case CLO_SPRINT:
            if (!rc_only)
                Options.game.type = GAME_TYPE_SPRINT;
            break;

        case CLO_SPRINT_MAP:
            if (!next_is_param)
                return false;

            nextUsed               = true;
            crawl_state.sprint_map = next_arg;
            Options.game.map       = next_arg;
            break;

        case CLO_TUTORIAL:
            if (!rc_only)
                Options.game.type = GAME_TYPE_TUTORIAL;
            break;

        case CLO_WIZARD:
#ifdef WIZARD
            if (!rc_only)
                Options.wiz_mode = WIZ_NO;
#endif
            break;

        case CLO_EXPLORE:
#ifdef WIZARD
            if (!rc_only)
                Options.explore_mode = WIZ_NO;
#endif
            break;

        case CLO_NO_SAVE:
            if (!rc_only)
                Options.no_save = true;
            break;

#ifdef USE_TILE_WEB
        case CLO_WEBTILES_SOCKET:
            nextUsed          = true;
            tiles.m_sock_name = next_arg;
            break;

        case CLO_AWAIT_CONNECTION:
            tiles.m_await_connection = true;
            break;

        case CLO_PRINT_WEBTILES_OPTIONS:
            if (!rc_only)
            {
                _print_webtiles_options();
                end(0);
            }
            break;
#endif

        case CLO_PRINT_CHARSET:
            if (rc_only)
                break;
#ifdef DGAMELAUNCH
            // Tell DGL we don't use ancient charsets anymore. The glyph set
            // doesn't matter here, just the encoding.
            printf("UNICODE\n");
#else
            printf("This option is for DGL use only.\n");
#endif
            end(0);
            break;

        case CLO_THROTTLE:
            crawl_state.throttle = true;
            break;

        case CLO_NO_THROTTLE:
            crawl_state.throttle = false;
            break;

        case CLO_EXTRA_OPT_FIRST:
            if (!next_is_param)
                return false;

            // Don't print the help message if the opt was wrong
            if (!_check_extra_opt(next_arg))
                return true;

            SysEnv.extra_opts_first.emplace_back(next_arg);
            nextUsed = true;

            // Can be used multiple times.
            arg_seen[o] = false;
            break;

        case CLO_EXTRA_OPT_LAST:
            if (!next_is_param)
                return false;

            // Don't print the help message if the opt was wrong
            if (!_check_extra_opt(next_arg))
                return true;

            SysEnv.extra_opts_last.emplace_back(next_arg);
            nextUsed = true;

            // Can be used multiple times.
            arg_seen[o] = false;
            break;
        }

        // Update position.
        current++;
        if (nextUsed)
            current++;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
// game_options

int game_options::o_int(const char *name, int def) const
{
    int val = def;
    if (const string *value = map_find(named_options, name))
        val = atoi(value->c_str());
    return val;
}

bool game_options::o_bool(const char *name, bool def) const
{
    bool val = def;
    if (const string *value = map_find(named_options, name))
        val = _read_bool(*value, val);
    return val;
}

string game_options::o_str(const char *name, const char *def) const
{
    return lookup(named_options, name, def ? def : "");
}

int game_options::o_colour(const char *name, int def) const
{
    string val = o_str(name);
    trim_string(val);
    lowercase(val);
    int col = str_to_colour(val);
    return col == -1? def : col;
}

///////////////////////////////////////////////////////////////////////
// system_environment

void system_environment::add_rcdir(const string &dir)
{
    string cdir = canonicalise_file_separator(dir);
    if (dir_exists(cdir))
        rcdirs.push_back(cdir);
    else
        end(1, false, "Cannot find -rcdir \"%s\"", cdir.c_str());
}

///////////////////////////////////////////////////////////////////////
// menu_sort_condition

menu_sort_condition::menu_sort_condition(menu_type _mt, int _sort)
    : mtype(_mt), sort(_sort), cmp()
{
}

menu_sort_condition::menu_sort_condition(const string &s)
    : mtype(MT_ANY), sort(-1), cmp()
{
    string cp = s;
    set_menu_type(cp);
    set_sort(cp);
    set_comparators(cp);
}

bool menu_sort_condition::matches(menu_type mt) const
{
    return mtype == MT_ANY || mtype == mt;
}

void menu_sort_condition::set_menu_type(string &s)
{
    static struct
    {
        const string mname;
        menu_type mtype;
    } menu_type_map[] =
      {
          { "any:",    MT_ANY       },
          { "inv:",    MT_INVLIST   },
          { "drop:",   MT_DROP      },
          { "pickup:", MT_PICKUP    },
          { "know:",   MT_KNOW      }
      };

    for (const auto &mi : menu_type_map)
    {
        const string &name = mi.mname;
        if (starts_with(s, name))
        {
            s = s.substr(name.length());
            mtype = mi.mtype;
            break;
        }
    }
}

void menu_sort_condition::set_sort(string &s)
{
    // Strip off the optional sort clauses and get the primary sort condition.
    string::size_type trail_pos = s.find(':');
    if (starts_with(s, "auto:"))
        trail_pos = s.find(':', trail_pos + 1);

    string sort_cond = trail_pos == string::npos? s : s.substr(0, trail_pos);

    trim_string(sort_cond);
    sort = _read_bool_or_number(sort_cond, sort, "auto:");

    if (trail_pos != string::npos)
        s = s.substr(trail_pos + 1);
    else
        s.clear();
}

void menu_sort_condition::set_comparators(string &s)
{
    init_item_sort_comparators(
        cmp,
        s.empty()? "equipped, basename, qualname, curse, qty" : s);
}
