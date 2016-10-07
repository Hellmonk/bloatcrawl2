#include "AppHdr.h"

#include "colour.h"

#include <cmath>
#include <memory>
#include <utility>

#include "areas.h"
#include "branch.h"
#include "cloud.h"
#include "dgn-height.h"
#include "options.h"
#include "stringutil.h"
#include "libutil.h" // map_find

static FixedVector<unique_ptr<element_colour_calc>, NUM_COLOURS> element_colours;
// Values point into element_colours.
static map<string, element_colour_calc*> element_colours_str;

typedef vector< pair<int, int> > random_colour_map;
typedef int (*randomized_element_colour_calculator)(int, const coord_def&,
                                                    random_colour_map);

static int _randomized_element_colour(int, const coord_def&, random_colour_map);

struct random_element_colour_calc : public element_colour_calc
{
    random_element_colour_calc(element_type _type, string _name,
                               vector< pair<int, int> > _rand_vals)
        : element_colour_calc(_type, _name, (element_colour_calculator)_randomized_element_colour),
          rand_vals(_rand_vals)
        {};

    virtual int get(const coord_def& loc = coord_def(),
                    bool non_random = false) override;

protected:
    random_colour_map rand_vals;
};

int element_colour_calc::rand(bool non_random)
{
    return non_random ? 0 : ui_random(120);
}

int element_colour_calc::get(const coord_def& loc, bool non_random)
{
    return (*calc)(rand(non_random), loc);
}

int random_element_colour_calc::get(const coord_def& loc, bool non_random)
{
    // casting function pointers from other function pointers is guaranteed
    // to be safe, but calling them on pointers not of their type isn't, so
    // assert here to be safe - add to this assert if something different is
    // needed
    ASSERT((randomized_element_colour_calculator)calc ==
                _randomized_element_colour);
    randomized_element_colour_calculator real_calc =
        (randomized_element_colour_calculator)calc;
    return (*real_calc)(rand(non_random), loc, rand_vals);
}

colour_t random_colour(bool ui_rand)
{
    return 1 + (ui_rand ? ui_random : random2)(15);
}

static bool _ui_coinflip()
{
    return static_cast<bool>(ui_random(2));
}

colour_t random_uncommon_colour()
{
    colour_t result;

    do
    {
        result = random_colour();
    }
    while (result == LIGHTCYAN || result == CYAN || result == BROWN);

    return result;
}

bool is_low_colour(colour_t colour)
{
    return colour <= 7;
}

bool is_high_colour(colour_t colour)
{
    return colour >= 8 && colour <= 15;
}

colour_t make_low_colour(colour_t colour)
{
    if (is_high_colour(colour))
        return colour - 8;

    return colour;
}

colour_t make_high_colour(colour_t colour)
{
    if (is_low_colour(colour))
        return colour + 8;

    return colour;
}

// returns if a colour is one of the special element colours (ie not regular)
static bool _is_element_colour(int col)
{
    // stripping any COLFLAGS (just in case)
    col = col & 0x007f;
    ASSERT(col < NUM_COLOURS);
    return col >= ETC_FIRE;
}

static int _randomized_element_colour(int rand, const coord_def&,
                                      random_colour_map rand_vals)
{
    int accum = 0;
    for (const auto &entry : rand_vals)
        if ((accum += entry.first) > rand)
            return entry.second;

    return BLACK;
}

static int _etc_floor(int, const coord_def& loc)
{
    return element_colour(env.floor_colour, false, loc);
}

static int _etc_rock(int, const coord_def& loc)
{
    return element_colour(env.rock_colour, false, loc);
}

static int _etc_elven_brick(int, const coord_def& loc)
{
    if ((loc.x + loc.y) % 2)
        return WHITE;
    else
    {
        if ((loc.x / 2 + loc.y / 2) % 2)
            return LIGHTGREEN;
        else
            return LIGHTBLUE;
    }
}

static int _etc_waves(int, const coord_def& loc)
{
    // Shouldn't have this outside of Shoals, but it can happen.
    // See !lg lakren crash 8 .
    if (!env.heightmap.get())
        return CYAN;
    short height = dgn_height_at(loc);
    int cycle_point = you.num_turns % 20;
    int min_height = -90 + 5 * cycle_point,
        max_height = -80 + 5 * cycle_point;
    if (height > min_height && height < max_height)
        return LIGHTCYAN;
    else
        return CYAN;
}

static int _etc_elemental(int, const coord_def& loc)
{
    int cycle = (you.elapsed_time / 200) % 4;
    switch (cycle)
    {
        default:
        case 0:
            return element_colour(ETC_EARTH, false, loc);
        case 1:
            return element_colour(_ui_coinflip() ? ETC_AIR : ETC_ELECTRICITY,
                                  false, loc);
        case 2:
            // Not ETC_FIRE, which is Makhleb; instead do magma-y colours.
            if (_ui_coinflip())
                return RED;
            return _ui_coinflip() ? BROWN : LIGHTRED;
        case 3:
            return element_colour(ETC_ICE, false, loc);
    }
}

int get_disjunct_phase(const coord_def& loc)
{
    static int turns = you.num_turns;
    static coord_def centre = find_centre_for(loc, AREA_DISJUNCTION);

    if (turns != you.num_turns || (centre-loc).abs() > 15)
    {
        centre = find_centre_for(loc, AREA_DISJUNCTION);
        turns = you.num_turns;
    }

    if (centre.origin())
        return 2;

    int x = loc.x - centre.x;
    int y = loc.y - centre.y;
    double dist = sqrt(x*x + y*y);
    int parity = ((int) (dist / PI) + you.frame_no / 11) % 2 ? 1 : -1;
    double dir = sin(atan2(x, y)*PI + parity * you.frame_no / 3) + 1;
    return 1 + (int) floor(dir * 2);
}

static int _etc_disjunction(int, const coord_def& loc)
{
    switch (get_disjunct_phase(loc))
    {
    case 1:
        return LIGHTBLUE;
    case 2:
        return BLUE;
    case 3:
        return MAGENTA;
    default:
        return LIGHTMAGENTA;
    }
}

static int _etc_liquefied(int, const coord_def& loc)
{
    static int turns = you.num_turns;
    static coord_def centre = find_centre_for(loc, AREA_LIQUID);

    if (turns != you.num_turns || (centre-loc).abs() > 15)
    {
        centre = find_centre_for(loc, AREA_LIQUID);
        turns = you.num_turns;
    }

    if (centre.origin())
        return BROWN;

    int x = loc.x - centre.x;
    int y = loc.y - centre.y;
    double dir = atan2(x, y)/PI;
    double dist = sqrt(x*x + y*y);
    bool phase = ((int)floor(dir*0.3 + dist*0.5 + (you.frame_no % 54)/2.7))&1;

    if (branches[you.where_are_you].floor_colour == BROWN)
        return phase ? LIGHTRED : RED;
    else
        return phase ? YELLOW : BROWN;
}

static int _etc_tree(int, const coord_def& loc)
{
    uint32_t h = loc.x;
    h+=h<<10; h^=h>>6;
    h += loc.y;
    h+=h<<10; h^=h>>6;
    h+=h<<3; h^=h>>11; h+=h<<15;
    return (h>>30) ? GREEN :
        player_in_branch(BRANCH_SWAMP) ? BROWN : LIGHTGREEN; // Swamp trees are mangroves.
}

bool get_tornado_phase(const coord_def& loc)
{
    coord_def center = get_cloud_originator(loc);
    if (center.origin())
        return _ui_coinflip(); // source died/went away
    else
    {
        int x = loc.x - center.x;
        int y = loc.y - center.y;
        double dir = atan2(x, y)/PI;
        double dist = sqrt(x*x + y*y);
        return ((int)floor(dir*2 + dist*0.33 - (you.frame_no % 54)/2.7))&1;
    }
}

static int _etc_tornado(int, const coord_def& loc)
{
    const bool phase = get_tornado_phase(loc);
    switch (grd(loc))
    {
    case DNGN_LAVA:
        return phase ? LIGHTRED : RED;
    case DNGN_SHALLOW_WATER:
        return phase ? LIGHTCYAN : CYAN;
    case DNGN_DEEP_WATER:
        return phase ? LIGHTBLUE : BLUE;
    default:
        return phase ? WHITE : LIGHTGREY;
    }
}

bool get_orb_phase(const coord_def& loc)
{
    int dist = (loc - env.orb_pos).abs();
    return (you.frame_no - dist*2/3)&4;
}

static int _etc_orb_glow(int, const coord_def& loc)
{
    return get_orb_phase(loc) ? LIGHTMAGENTA : MAGENTA;
}

int dam_colour(const monster_info& mi)
{
    switch (mi.dam)
    {
        case MDAM_OKAY:                 return Options.enemy_hp_colour[0];
        case MDAM_LIGHTLY_DAMAGED:      return Options.enemy_hp_colour[1];
        case MDAM_MODERATELY_DAMAGED:   return Options.enemy_hp_colour[2];
        case MDAM_HEAVILY_DAMAGED:      return Options.enemy_hp_colour[3];
        case MDAM_SEVERELY_DAMAGED:     return Options.enemy_hp_colour[4];
        case MDAM_ALMOST_DEAD:          return Options.enemy_hp_colour[5];
        case MDAM_DEAD:                 return BLACK; // this should never happen
        default:                        return CYAN; // this should really never happen
    }
}

colour_t rune_colour(int type)
{
    switch (type)
    {
        case RUNE_SWAMP: return GREEN;
        case RUNE_SNAKE: return GREEN;
        case RUNE_SHOALS: return LIGHTBLUE;
        case RUNE_SLIME: return LIGHTGREEN;
        case RUNE_VAULTS: return WHITE;
        case RUNE_TOMB: return YELLOW;
        case RUNE_DIS: return CYAN;
        case RUNE_GEHENNA: return RED;
        case RUNE_COCYTUS: return LIGHTCYAN;
        case RUNE_TARTARUS: return LIGHTMAGENTA;
        case RUNE_ABYSSAL: return MAGENTA;
        case RUNE_DEMONIC: return MAGENTA;
        case RUNE_MNOLEG: return LIGHTGREEN;
        case RUNE_LOM_LOBON: return BLUE;
        case RUNE_CEREBOV: return RED;
        case RUNE_GLOORX_VLOQ: return DARKGRAY;
        case RUNE_SPIDER: return BROWN;
        default: return GREEN;
    }
}

static int _etc_random(int, const coord_def&)
{
    return random_colour(true);
}

static element_colour_calc *_create_random_element_colour_calc(element_type type,
                                                               string name, ...)
{
    random_colour_map rand_vals;
    va_list ap;

    va_start(ap, name);

    for (;;)
    {
        int prob = va_arg(ap, int);
        if (!prob)
            break;

        int colour = va_arg(ap, int);

        rand_vals.emplace_back(prob, colour);
    }

    va_end(ap);

    return new random_element_colour_calc(type, name, rand_vals);
}

void add_element_colour(element_colour_calc *colour)
{
    // or else lookups won't work: we strip high bits (because of colflags)
    ASSERT(colour->type < 128);
    COMPILE_CHECK(NUM_COLOURS <= 128);
    if (colour->type >= ETC_FIRST_LUA)
    {
        ASSERT(element_colours[colour->type].get()
               == element_colours_str[colour->name]);
    }
    else
    {
        ASSERT(!element_colours[colour->type]);
        ASSERT(!element_colours_str.count(colour->name));
    }
    element_colours[colour->type].reset(colour);
    element_colours_str[colour->name] = colour;
}

void init_element_colours()
{
    add_element_colour(_create_random_element_colour_calc(
                            ETC_FIRE, "fire",
                            40,  RED,
                            40,  YELLOW,
                            40,  LIGHTRED,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_ICE, "ice",
                            40,  LIGHTBLUE,
                            40,  BLUE,
                            40,  WHITE,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_EARTH, "earth",
                            70,  BROWN,
                            50,  GREEN,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_ELECTRICITY, "electricity",
                            40,  LIGHTCYAN,
                            40,  LIGHTBLUE,
                            40,  CYAN,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_AIR, "air",
                            60,  LIGHTGREY,
                            60,  WHITE,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_POISON, "poison",
                            60,  LIGHTGREEN,
                            60,  GREEN,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_WATER, "water",
                            60,  BLUE,
                            60,  CYAN,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_MAGIC, "magic",
                            30,  LIGHTMAGENTA,
                            30,  LIGHTBLUE,
                            30,  MAGENTA,
                            30,  BLUE,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_MUTAGENIC, "mutagenic",
                            60,  LIGHTMAGENTA,
                            60,  MAGENTA,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_WARP, "warp",
                            60,  LIGHTMAGENTA,
                            60,  MAGENTA,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_ENCHANT, "enchant",
                            60,  LIGHTBLUE,
                            60,  BLUE,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_HEAL, "heal",
                            60,  LIGHTBLUE,
                            60,  YELLOW,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_HOLY, "holy",
                            60,  YELLOW,
                            60,  WHITE,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_DARK, "dark",
                            80,  DARKGREY,
                            40,  LIGHTGREY,
                        0));
    // assassin, necromancer
    add_element_colour(_create_random_element_colour_calc(
                            ETC_DEATH, "death",
                            80,  DARKGREY,
                            40,  MAGENTA,
                        0));
    // ie demonology
    add_element_colour(_create_random_element_colour_calc(
                            ETC_UNHOLY, "unholy",
                            80,  DARKGREY,
                            40,  RED,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_VEHUMET, "vehumet",
                            40,  LIGHTRED,
                            40,  LIGHTMAGENTA,
                            40,  LIGHTBLUE,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_BEOGH, "beogh",
                            // plain Orc colour
                            60,  LIGHTRED,
                            // Orcish mines wall/idol colour
                            60,  BROWN,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_CRYSTAL, "crystal",
                            40,  LIGHTGREY,
                            40,  GREEN,
                            40,  LIGHTRED,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_BLOOD, "blood",
                            60,  RED,
                            60,  DARKGREY,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_SMOKE, "smoke",
                            30,  LIGHTGREY,
                            30,  DARKGREY,
                            30,  LIGHTBLUE,
                            30,  MAGENTA,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_SLIME, "slime",
                            40,  GREEN,
                            40,  BROWN,
                            40,  LIGHTGREEN,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_JEWEL, "jewel",
                            12,  WHITE,
                            12,  YELLOW,
                            12,  LIGHTMAGENTA,
                            12,  LIGHTRED,
                            12,  LIGHTGREEN,
                            12,  LIGHTBLUE,
                            12,  MAGENTA,
                            12,  RED,
                            12,  GREEN,
                            12,  BLUE,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_ELVEN, "elven",
                            40,  LIGHTGREEN,
                            40,  GREEN,
                            20,  LIGHTBLUE,
                            20,  BLUE,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_DWARVEN, "dwarven",
                            40,  BROWN,
                            40,  LIGHTRED,
                            20,  LIGHTGREY,
                            20,  CYAN,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_ORCISH, "orcish",
                            40,  DARKGREY,
                            40,  RED,
                            20,  BROWN,
                            20,  MAGENTA,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_FLASH, "flash",
                            30,  LIGHTMAGENTA,
                            30,  MAGENTA,
                            30,  YELLOW,
                            15,  LIGHTRED,
                            15,  RED,
                        0));
    add_element_colour(new element_colour_calc(
                            ETC_FLOOR, "floor", _etc_floor
                       ));
    add_element_colour(new element_colour_calc(
                            ETC_ROCK, "rock", _etc_rock
                       ));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_MIST, "mist",
                            100, CYAN,
                            20,  BLUE,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_SHIMMER_BLUE, "shimmer_blue",
                            90,  BLUE,
                            20,  LIGHTBLUE,
                            10,  CYAN,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_DECAY, "decay",
                            60,  BROWN,
                            60,  GREEN,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_SILVER, "silver",
                            90,  LIGHTGREY,
                            30,  WHITE,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_GOLD, "gold",
                            60,  YELLOW,
                            60,  BROWN,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_IRON, "iron",
                            40,  CYAN,
                            40,  LIGHTGREY,
                            40,  DARKGREY,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_BONE, "bone",
                            90,  WHITE,
                            30,  LIGHTGREY,
                        0));
    add_element_colour(new element_colour_calc(
                            ETC_ELVEN_BRICK, "elven_brick", _etc_elven_brick
                       ));
    add_element_colour(new element_colour_calc(
                            ETC_WAVES, "waves", _etc_waves
                       ));
    add_element_colour(new element_colour_calc(
                            ETC_TREE, "tree", _etc_tree
                       ));
    add_element_colour(new element_colour_calc(
                            ETC_TORNADO, "tornado", _etc_tornado
                       ));
    add_element_colour(new element_colour_calc(
                            ETC_LIQUEFIED, "liquefied", _etc_liquefied
                       ));
    add_element_colour(new element_colour_calc(
                            ETC_ORB_GLOW, "orb_glow", _etc_orb_glow
                       ));
    add_element_colour(new element_colour_calc(
                            ETC_DISJUNCTION, "disjunction", _etc_disjunction
                       ));
    add_element_colour(new element_colour_calc(
                            ETC_RANDOM, "random", _etc_random
                       ));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_DITHMENOS, "dithmenos",
                            40,  DARKGREY,
                            40,  MAGENTA,
                            40,  BLUE,
                        0));
    add_element_colour(new element_colour_calc(
                            ETC_ELEMENTAL, "elemental", _etc_elemental
                       ));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_INCARNADINE, "incarnadine",
                            60,  MAGENTA,
                            60,  RED,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_PAKELLAS, "pakellas",
                            40,  LIGHTGREEN,
                            40,  LIGHTMAGENTA,
                            40,  LIGHTCYAN,
                        0));
    add_element_colour(_create_random_element_colour_calc(
                            ETC_AWOKEN_FOREST, "awoken_forest",
                            40, GREEN,
                            40, LIGHTGREEN,
                        0));
    // redefined by Lua later
    add_element_colour(new element_colour_calc(
                            ETC_DISCO, "disco", _etc_random
                       ));
}

int element_colour(int element, bool no_random, const coord_def& loc)
{
    // pass regular colours through for safety.
    if (!_is_element_colour(element))
        return element;

    // Strip COLFLAGs just in case.
    element &= 0x007f;

    ASSERT(element_colours[element]);
    int ret = element_colours[element]->get(loc, no_random);

    ASSERT(!_is_element_colour(ret));

    return (ret == BLACK) ? GREEN : ret;
}

#ifdef USE_TILE
static int _hex(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return 10 + c - 'a';
    if (c >= 'A' && c <= 'F')
        return 10 + c - 'A';
    return 0;
}

VColour str_to_tile_colour(string colour)
{
    if (colour.empty())
        return VColour(0, 0, 0);

    if (colour[0] == '#' && colour.length() == 7)
    {
        return VColour((_hex(colour[1]) << 4) + _hex(colour[2]),
                (_hex(colour[3]) << 4) + _hex(colour[4]),
                (_hex(colour[5]) << 4) + _hex(colour[6]));
    }
    else
    {
        lowercase(colour);

        if (colour == "darkgray")
            colour = "darkgrey";
        else if (colour == "gray")
            colour = "grey";
        else if (colour == "lightgray")
            colour = "lightgrey";

        if (colour == "black")             return VColour(  0,   0,   0);
        else if (colour == "darkgrey")     return VColour(128, 128, 128);
        else if (colour == "grey")         return VColour(160, 160, 160);
        else if (colour == "lightgrey")    return VColour(192, 192, 192);
        else if (colour == "white")        return VColour(255, 255, 255);
        else if (colour == "blue")         return VColour(  0,  64, 255);
        else if (colour == "lightblue")    return VColour(128, 128, 255);
        else if (colour == "darkblue")     return VColour(  0,  32, 128);
        else if (colour == "green")        return VColour(  0, 255,   0);
        else if (colour == "lightgreen")   return VColour(128, 255, 128);
        else if (colour == "darkgreen")    return VColour(  0, 128,   0);
        else if (colour == "cyan")         return VColour(  0, 255, 255);
        else if (colour == "lightcyan")    return VColour( 64, 255, 255);
        else if (colour == "darkcyan")     return VColour(  0, 128, 128);
        else if (colour == "red")          return VColour(255,   0,   0);
        else if (colour == "lightred")     return VColour(255, 128, 128);
        else if (colour == "darkred")      return VColour(128,   0,   0);
        else if (colour == "magenta")      return VColour(192,   0, 255);
        else if (colour == "lightmagenta") return VColour(255, 128, 255);
        else if (colour == "darkmagenta")  return VColour( 96,   0, 128);
        else if (colour == "yellow")       return VColour(255, 255,   0);
        else if (colour == "lightyellow")  return VColour(255, 255,  64);
        else if (colour == "darkyellow")   return VColour(128, 128,   0);
        else if (colour == "brown")        return VColour(165,  91,   0);
        else return VColour(0, 0, 0);
    }
}
#endif

static const char* const cols[16] =
{
    "black", "blue", "green", "cyan", "red", "magenta", "brown",
    "lightgrey", "darkgrey", "lightblue", "lightgreen", "lightcyan",
    "lightred", "lightmagenta", "yellow", "white"
};
COMPILE_CHECK(ARRAYSZ(cols) == NUM_TERM_COLOURS);

const string colour_to_str(colour_t colour)
{
    if (colour >= NUM_TERM_COLOURS)
        return "lightgrey";
    else
        return cols[colour];
}

// Returns default_colour (default -1) if unmatched else returns 0-15.
int str_to_colour(const string &str, int default_colour, bool accept_number,
                  bool accept_elemental)
{
    int ret;

    for (ret = 0; ret < NUM_TERM_COLOURS; ++ret)
    {
        if (str == cols[ret])
            break;
    }

    // Check for alternate spellings.
    if (ret == NUM_TERM_COLOURS)
    {
        if (str == "lightgray")
            ret = 7;
        else if (str == "darkgray")
            ret = 8;
    }

    if (ret == NUM_TERM_COLOURS && accept_elemental)
    {
        // Maybe we have an element colour attribute.
        if (element_colour_calc **calc = map_find(element_colours_str, str))
        {
            ASSERT(*calc);
            ret = (*calc)->type;
        }
    }

    if (ret == NUM_TERM_COLOURS && accept_number)
    {
        // Check if we have a direct colour index.
        const char *s = str.c_str();
        char *es = nullptr;
        const int ci = static_cast<int>(strtol(s, &es, 10));
        if (s != es && es && ci >= 0 && ci < NUM_TERM_COLOURS)
            ret = ci;
    }

    return (ret == NUM_TERM_COLOURS) ? default_colour : ret;
}

#if defined(TARGET_OS_WINDOWS) || defined(USE_TILE_LOCAL)
static unsigned short _dos_reverse_brand(unsigned short colour)
{
    if (Options.dos_use_background_intensity)
    {
        // If the console treats the intensity bit on background colours
        // correctly, we can do a very simple colour invert.

        // Special casery for shadows.
        if (colour == BLACK)
            colour = (DARKGREY << 4);
        else
            colour = (colour & 0xF) << 4;
    }
    else
    {
        // If we're on a console that takes its DOSness very seriously the
        // background high-intensity bit is actually a blink bit. Blinking is
        // evil, so we strip the background high-intensity bit. This, sadly,
        // limits us to 7 background colours.

        // Strip off high-intensity bit. Special case DARKGREY, since it's the
        // high-intensity counterpart of black, and we don't want black on
        // black.
        //
        // We *could* set the foreground colour to WHITE if the background
        // intensity bit is set, but I think we've carried the
        // angry-fruit-salad theme far enough already.

        if (colour == DARKGREY)
            colour |= (LIGHTGREY << 4);
        else if (colour == BLACK)
            colour = LIGHTGREY << 4;
        else
        {
            // Zap out any existing background colour, and the high
            // intensity bit.
            colour  &= 7;

            // And swap the foreground colour over to the background
            // colour, leaving the foreground black.
            colour <<= 4;
        }
    }

    return colour;
}

static unsigned short _dos_hilite_brand(unsigned short colour,
                                        unsigned short hilite)
{
    if (!hilite)
        return colour;

    if (colour == hilite)
        colour = 0;

    colour |= (hilite << 4);
    return colour;
}

static unsigned short _dos_brand(unsigned short colour, unsigned brand)
{
    if ((brand & CHATTR_ATTRMASK) == CHATTR_NORMAL)
        return colour;

    colour &= 0xFF;

    if ((brand & CHATTR_ATTRMASK) == CHATTR_HILITE)
        return _dos_hilite_brand(colour, (brand & CHATTR_COLMASK) >> 8);
    else
        return _dos_reverse_brand(colour);
}
#endif

#if defined(TARGET_OS_WINDOWS) || defined(USE_TILE_LOCAL)
static unsigned _colflag2brand(int colflag)
{
    switch (colflag)
    {
    case COLFLAG_ITEM_HEAP:
        return Options.heap_brand;
    case COLFLAG_FRIENDLY_MONSTER:
        return Options.friend_brand;
    case COLFLAG_NEUTRAL_MONSTER:
        return Options.neutral_brand;
    case COLFLAG_WILLSTAB:
        return Options.stab_brand;
    case COLFLAG_MAYSTAB:
        return Options.may_stab_brand;
    case COLFLAG_FEATURE_ITEM:
        return Options.feature_item_brand;
    case COLFLAG_TRAP_ITEM:
        return Options.trap_item_brand;
    default:
        return CHATTR_NORMAL;
    }
}
#endif

unsigned real_colour(unsigned raw_colour, const coord_def& loc)
{
    // This order is important - is_element_colour() doesn't want to see the
    // munged colours returned by dos_brand, so it should always be done
    // before applying DOS brands.
    const int colflags = raw_colour & 0xFF00;

    // Evaluate any elemental colours to guarantee vanilla colour is returned
    if (_is_element_colour(raw_colour))
        raw_colour = colflags | element_colour(raw_colour, false, loc);

#if defined(TARGET_OS_WINDOWS) || defined(USE_TILE_LOCAL)
    if (colflags)
    {
        unsigned brand = _colflag2brand(colflags);
        raw_colour = _dos_brand(raw_colour & 0xFF, brand);
    }
#endif

    return raw_colour;
}
