/**
 * @file
 * @brief Functions used to print information about gods.
 **/

#include "AppHdr.h"

#include "describe-god.h"

#include <iomanip>

#include "ability.h"
#include "branch.h"
#include "cio.h"
#include "database.h"
#include "describe.h"
#include "english.h"
#include "godabil.h"
#include "godpassive.h"
#include "godprayer.h"
#include "libutil.h"
#include "macro.h"
#include "menu.h"
#include "religion.h"
#include "skills.h"
#include "spl-util.h"
#include "stringutil.h"
#include "unicode.h"
#include "xom.h"

enum god_desc_type
{
    GDESC_OVERVIEW,
    GDESC_DETAILED,
    GDESC_WRATH,
    NUM_GDESCS
};

static int _piety_level(int piety)
{
    return (piety >= piety_breakpoint(5)) ? 7 :
           (piety >= piety_breakpoint(4)) ? 6 :
           (piety >= piety_breakpoint(3)) ? 5 :
           (piety >= piety_breakpoint(2)) ? 4 :
           (piety >= piety_breakpoint(1)) ? 3 :
           (piety >= piety_breakpoint(0)) ? 2 :
           (piety >                    0) ? 1
                                          : 0;
}

static int _gold_level()
{
    return (you.gold >= 50000) ? 7 :
           (you.gold >= 10000) ? 6 :
           (you.gold >=  5000) ? 5 :
           (you.gold >=  1000) ? 4 :
           (you.gold >=   500) ? 3 :
           (you.gold >=   100) ? 2
                               : 1;
}

static string _describe_favour(god_type which_god)
{
    if (player_under_penance())
    {
        const int penance = you.penance[which_god];
        return (penance >= 50) ? "Godly wrath is upon you!" :
               (penance >= 20) ? "You've transgressed heavily! Be penitent!" :
               (penance >=  5) ? "You are under penance."
                               : "You should show more discipline.";
    }

    if (which_god == GOD_XOM)
        return uppercase_first(describe_xom_favour());

    const int rank = which_god == GOD_GOZAG ? _gold_level()
    : _piety_level(you.piety);

    const string godname = god_name(which_god);
    switch (rank)
    {
        case 7:  return "A prized avatar of " + godname;
        case 6:  return "A favoured servant of " + godname + ".";
        case 5:

            if (you_worship(GOD_DITHMENOS))
                return "A glorious shadow in the eyes of " + godname + ".";
            else
                return "A shining star in the eyes of " + godname + ".";

        case 4:

            if (you_worship(GOD_DITHMENOS))
                return "A rising shadow in the eyes of " + godname + ".";
            else
                return "A rising star in the eyes of " + godname + ".";

        case 3:  return uppercase_first(godname) + " is most pleased with you.";
        case 2:  return uppercase_first(godname) + " is pleased with you.";
        default: return uppercase_first(godname) + " is noncommittal.";
    }
}

// The various titles granted by the god of your choice. Note that Xom
// doesn't use piety the same way as the other gods, so these are just
// placeholders.
static const char *divine_title[][8] =
{
    // No god.
    {"Buglet",             "Firebug",               "Bogeybug",                 "Bugger",
        "Bugbear",            "Bugged One",            "Giant Bug",                "Lord of the Bugs"},

    // Zin.
    {"Blasphemer",         "Anchorite",             "Apologist",                "Pious",
        "Devout",             "Orthodox",              "Immaculate",               "Bringer of Law"},

    // The Shining One.
    {"Honourless",         "Acolyte",               "Righteous",                "Unflinching",
        "Holy Warrior",       "Exorcist",              "Demon Slayer",             "Bringer of Light"},

    // Kikubaaqudgha -- scholarly death.
    {"Tormented",          "Purveyor of Pain",      "Scholar of Death",         "Merchant of Misery",
        "Artisan of Death",   "Dealer of Despair",     "Black Sun",                "Lord of Darkness"},

    // Yredelemnul -- zombie death.
    {"Traitor",            "Tainted",                "Torchbearer",             "Fey @Genus@",
        "Black Crusader",     "Sculptor of Flesh",     "Harbinger of Death",       "Grim Reaper"},

    // Xom.
    {"Toy",                "Toy",                   "Toy",                      "Toy",
        "Toy",                "Toy",                   "Toy",                      "Toy"},

    // Vehumet -- battle mage theme.
    {"Meek",               "Sorcerer's Apprentice", "Scholar of Destruction",   "Caster of Ruination",
        "Traumaturge",        "Battlemage",            "Warlock",                  "Luminary of Lethal Lore"},

    // Okawaru -- battle theme.
    {"Coward",             "Struggler",             "Combatant",                "Warrior",
        "Knight",             "Warmonger",             "Commander",                "Victor of a Thousand Battles"},

    // Makhleb -- chaos theme.
    {"Orderly",            "Spawn of Chaos",        "Disciple of Destruction",  "Fanfare of Bloodshed",
        "Fiendish",           "Demolition @Genus@",    "Pandemonic",               "Champion of Chaos"},

    // Sif Muna -- scholarly theme.
    {"Ignorant",           "Disciple",              "Student",                  "Adept",
        "Scribe",             "Scholar",               "Sage",                     "Genius of the Arcane"},

    // Trog -- anger theme.
    {"Puny",               "Troglodyte",            "Angry Troglodyte",         "Frenzied",
        "@Genus@ of Prey",    "Rampant",               "Wild @Genus@",             "Bane of Scribes"},

    // Nemelex Xobeh -- alluding to Tarot and cards.
    {"Unlucky @Genus@",    "Pannier",               "Jester",                   "Fortune-Teller",
        "Soothsayer",         "Magus",                 "Cardsharp",                "Hand of Fortune"},

    // Elyvilon.
    {"Sinner",                "Practitioner",       "Comforter",             "Caregiver",
        "Mender",           "Pacifist",                "Purifying @Genus@",        "Bringer of Life"},

    // Lugonu -- distortion theme.
    {"Pure",               "Abyss-Baptised",        "Unweaver",                 "Distorting @Genus@",
        "Agent of Entropy",   "Schismatic",            "Envoy of Void",            "Corrupter of Planes"},

    // Beogh -- messiah theme.
    {"Apostate",           "Messenger",             "Proselytiser",             "Priest",
        "Missionary",         "Evangelist",            "Apostle",                  "Messiah"},

    // Jiyva -- slime and jelly theme.
    {"Scum",               "Squelcher",             "Ooze",                     "Jelly",
        "Slime Creature",     "Dissolving @Genus@",    "Blob",                     "Royal Jelly"},

    // Fedhas Madash -- nature theme.
    {"@Walking@ Fertiliser", "Fungal",              "Green @Genus@",            "Cultivator",
        "Fruitful",           "Photosynthesist",       "Green Death",              "Force of Nature"},

    // Cheibriados -- slow theme
    {"Hasty",              "Sluggish @Genus@",      "Deliberate",               "Unhurried",
     "Contemplative",         "Epochal",               "Timeless",                 "@Adj@ Aeon"},

    // Ashenzari -- divination theme
    {"Star-crossed",       "Cursed",                "Initiated",                "Soothsayer",
        "Seer",               "Oracle",                "Illuminatus",              "Omniscient"},

    // Dithmenos -- darkness theme
    {"Ember",              "Gloomy",                "Darkened",                 "Extinguished",
        "Caliginous",         "Umbral",                "Hand of Shadow",           "Eternal Night"},

    // Gozag -- entrepreneur theme
    {"Profligate",         "Pauper",                "Entrepreneur",             "Capitalist",
        "Rich",               "Opulent",               "Tycoon",                   "Plutocrat"},

    // Qazlal -- natural disaster theme
    {"Unspoiled",          "@Adj@ Mishap",          "Lightning Rod",            "@Adj@ Disaster",
        "Eye of the Storm",   "@Adj@ Catastrophe",     "@Adj@ Cataclysm",          "End of an Era"},

    // Ru -- enlightenment theme
    {"Sleeper",           "Questioner",             "Initiate",                 "Seeker of Truth",
        "@Walker@ of the Path","Lifter of the Veil",     "Transcendent",     "Drop of Water"},

    // Pakellas -- inventor theme
    {"Reactionary",       "Apprentice",             "Inquisitive",              "Experimenter",
        "Inventor",           "Pioneer",               "Brilliant",                "Grand Gadgeteer"},
};
COMPILE_CHECK(ARRAYSZ(divine_title) == NUM_GODS);

string god_title(god_type which_god, species_type which_species, int piety)
{
    string title;
    if (player_under_penance(which_god))
        title = divine_title[which_god][0];
    else if (which_god == GOD_GOZAG)
        title = divine_title[which_god][_gold_level()];
    else
        title = divine_title[which_god][_piety_level(piety)];

    const map<string, string> replacements =
    {
        { "Adj", species_name(which_species, SPNAME_ADJ) },
        { "Genus", species_name(which_species, SPNAME_GENUS) },
        { "Walking", species_walking_verb(which_species) + "ing" },
        { "Walker", species_walking_verb(which_species) + "er" },
    };

    return replace_keys(title, replacements);
}

static string _describe_ash_skill_boost()
{
    if (!you.bondage_level)
    {
        return "Ashenzari won't support your skills until you bind yourself "
               "with cursed items.";
    }

    static const char* bondage_parts[NUM_ET] = { "Weapon hand", "Shield hand",
                                                 "Armour", "Jewellery" };
    static const char* bonus_level[3] = { "Low", "Medium", "High" };
    ostringstream desc;
    desc.setf(ios::left);
    desc << "<white>";
    desc << setw(18) << "Bound part";
    desc << setw(30) << "Boosted skills";
    desc << "Bonus\n";
    desc << "</white>";

    for (int i = ET_WEAPON; i < NUM_ET; i++)
    {
        if (you.bondage[i] <= 0 || i == ET_SHIELD && you.bondage[i] == 3)
            continue;

        desc << setw(18);
        if (i == ET_WEAPON && you.bondage[i] == 3)
            desc << "Hands";
        else
            desc << bondage_parts[i];

        string skills;
        map<skill_type, int8_t> boosted_skills = ash_get_boosted_skills(eq_type(i));
        const int8_t bonus = boosted_skills.begin()->second;
        auto it = boosted_skills.begin();

        // First, we keep only one magic school skill (conjuration).
        // No need to list all of them since we boost all or none.
        while (it != boosted_skills.end())
        {
            if (it->first > SK_CONJURATIONS && it->first <= SK_LAST_MAGIC)
            {
                boosted_skills.erase(it);
                it = boosted_skills.begin();
            }
            else
                ++it;
        }

        it = boosted_skills.begin();
        while (!boosted_skills.empty())
        {
            // For now, all the bonuses from the same bounded part have
            // the same level.
            ASSERT(bonus == it->second);
            if (it->first == SK_CONJURATIONS)
                skills += "Magic schools";
            else
                skills += skill_name(it->first);

            if (boosted_skills.size() > 2)
                skills += ", ";
            else if (boosted_skills.size() == 2)
                skills += " and ";

            boosted_skills.erase(it++);
        }

        desc << setw(30) << skills;
        desc << bonus_level[bonus -1] << "\n";
    }

    return desc.str();
}

// from dgn-overview.cc
extern map<branch_type, set<level_id> > stair_level;

/**
 * Populate a provided vector with a list of bribable branches which are known
 * to the player.
 *
 * @param[out] targets      A list of bribable branches.
 */
static void _list_bribable_branches(vector<branch_type> &targets)
{
    for (branch_iterator it; it; ++it)
    {
        const branch_type br = it->id;
        if (!gozag_branch_bribable(br))
            continue;

        // Only list the Hells once.
        if (is_hell_subbranch(br))
            continue;

        // If you don't know the branch exists, don't list it;
        // this mainly plugs info leaks about Lair branch structure.
        if (!stair_level.count(br) && is_random_subbranch(br))
            continue;

        targets.push_back(br);
    }
}

/**
 * Describe the current options for Gozag's bribe branch ability.
 *
 * @return      A description of branches' bribe status.
 */
static string _describe_branch_bribability()
{
    string ret = "You can bribe the following branches of the dungeon:\n";
    vector<branch_type> targets;
    _list_bribable_branches(targets);

    size_t width = 0;
    for (branch_type br : targets)
        width = max(width, strlen(branches[br].shortname));

    for (branch_type br : targets)
    {
        string line = " ";
        line += branches[br].shortname;
        line += string(width + 3 - strwidth(line), ' ');

        if (!branch_bribe[br])
            line += "not bribed";
        else
            line += make_stringf("$%d", branch_bribe[br]);

        ret += line + "\n";
    }

    return ret;
}

/**
 * Print a guide to cycling between description screens, and check if the
 * player does so.
 *
 * @return Whether the player chose to cycle to the next description screen.
 */
static bool _check_description_cycle(god_desc_type gdesc)
{
    // Another function may have left a dangling recolour.
    textcolour(LIGHTGREY);

    const int bottom_line = min(30, get_number_of_lines());

    cgotoxy(1, bottom_line);
    const char* place = nullptr;
    switch (gdesc)
    {
        case GDESC_OVERVIEW: place = "<w>Overview</w>|Powers|Wrath"; break;
        case GDESC_DETAILED: place = "Overview|<w>Powers</w>|Wrath"; break;
        case GDESC_WRATH:    place = "Overview|Powers|<w>Wrath</w>"; break;
        default: die("Unknown god description type!");
    }
    formatted_string::parse_string(make_stringf("[<w>!</w>/<w>^</w>"
#ifdef USE_TILE_LOCAL
                                   "|<w>Right-click</w>"
#endif
    "]: %s", place)).display();

    mouse_control mc(MOUSE_MODE_MORE);

    const int keyin = getchm();
    return keyin == '!' || keyin == CK_MOUSE_CMD || keyin == '^';
}

/**
 * Linewrap & print a provided string, if non-empty.
 *
 * Also adds a pair of newlines, if the string is non-empty. (Ugly hack...)
 *
 * @param str       The string in question. (May be empty.)
 * @param width     The width to wrap to.
 */
static void _print_string_wrapped(string str, int width)
{
    if (!str.empty())
    {
        linebreak_string(str, width);
        display_tagged_block(str);
        cprintf("\n");
        cprintf("\n");
    }
}

/**
 * Describe the causes of the given god's wrath.
 *
 * @param which_god     The god in question.
 * @return              A description of the actions that cause this god's
 *                      wrath.
 */
static string _describe_god_wrath_causes(god_type which_god)
{
    if (which_god == GOD_RU)
        return ""; // no wrath
    vector<god_type> evil_gods;
    vector<god_type> chaotic_gods;
    for (god_iterator it; it; ++it)
    {
        const god_type god = *it;
        if (is_evil_god(god))
            evil_gods.push_back(god);
        else if (is_chaotic_god(god)) // intentionally not including evil!
            chaotic_gods.push_back(god);
        // XXX: refactor this if any god hates chaotic but not evil gods
    }

    switch (which_god)
    {
        case GOD_SHINING_ONE:
        case GOD_ELYVILON:
            return uppercase_first(god_name(which_god)) +
                   " forgives followers who leave " + god_name(which_god)+"'s"
                   " service; however, those who take up the worship of evil"
                   " gods will be punished. (" +
                   comma_separated_fn(begin(evil_gods), end(evil_gods),
                                      bind(god_name, placeholders::_1, false)) +
                   " are evil gods.)";

        case GOD_ZIN:
            return uppercase_first(god_name(which_god)) +
                   " does not punish followers who leave "+god_name(which_god)+
                   "'s service; however, those who take up the worship of evil"
                   " or chaotic gods will be scourged. (" +
                   comma_separated_fn(begin(evil_gods), end(evil_gods),
                                      bind(god_name, placeholders::_1, false)) +
                   " are evil, and " +
                   comma_separated_fn(begin(chaotic_gods), end(chaotic_gods),
                                      bind(god_name, placeholders::_1, false)) +
                   " are chaotic.)";
        default:
            return uppercase_first(god_name(which_god)) +
                   " does not appreciate abandonment, and will call down"
                   " fearful punishments on disloyal followers!";
    }
}

/**
 * Print the standard top line of the god description screens.
 *
 * @param god       The god in question.
 * @param width     The width of the screen.
 */
static void _print_top_line(god_type which_god, int width)
{
    const string godname = uppercase_first(god_name(which_god, true));
    textcolour(god_colour(which_god));
    const int len = width - strwidth(godname);
    cprintf("%s%s\n", string(len / 2, ' ').c_str(), godname.c_str());
    textcolour(LIGHTGREY);
    cprintf("\n");
}

/**
 * Print a description of the given god's dislikes & wrath effects.
 *
 * @param which_god     The god in question.
 */
static void _god_wrath_description(god_type which_god)
{
    clrscr();

    const int width = min(80, get_number_of_cols()) - 1;

    _print_top_line(which_god, width);

    _print_string_wrapped(get_god_dislikes(which_god, true), width);
    _print_string_wrapped(_describe_god_wrath_causes(which_god), width);
    _print_string_wrapped(getLongDescription(god_name(which_god) + " wrath"),
                          width);
}

/**
 * Describe miscellaneous information about the given god.
 *
 * @param which_god     The god in question.
 * @return              Info about gods which isn't covered by their powers,
 *                      likes, or dislikes.
 */
static string _get_god_misc_info(god_type which_god)
{
    switch (which_god)
    {
        case GOD_ASHENZARI:
        case GOD_JIYVA:
        case GOD_TROG:
        {
            const string piety_only = "Note that " + god_name(which_god) +
                                      " does not demand training of the"
                                      " Invocations skill. All abilities are"
                                      " purely based on piety.";

            if (which_god == GOD_ASHENZARI
                && in_good_standing(which_god, 1))
            {
                return piety_only + "\n\n" + _describe_ash_skill_boost();
            }

            return piety_only;
        }

        case GOD_KIKUBAAQUDGHA:
            return "The power of Kikubaaqudgha's abilities is governed by "
                   "Necromancy skill instead of Invocations.";

        case GOD_ELYVILON:
            return "Healing hostile monsters may pacify them, turning them "
                   "neutral. Pacification works best on natural beasts, "
                   "worse on monsters of your species, worse on other "
                   "species, worst of all on demons and undead, and not at "
                   "all on sleeping or mindless monsters. If it succeeds, "
                   "you gain half of the monster's experience value. Pacified "
                   "monsters try to leave the level.";

        case GOD_NEMELEX_XOBEH:
            return "The power of Nemelex Xobeh's abilities and of the "
                   "cards' effects is governed by Evocations skill "
                   "instead of Invocations.";

        case GOD_GOZAG:
            return _describe_branch_bribability();

        case GOD_PAKELLAS:
            return "The power of Pakellas' abilities is governed by "
                   "Evocations skill instead of Invocations.";

        default:
            return "";
    }
}

/**
 * Print a detailed description of the given god's likes and powers.
 *
 * @param god       The god in question.
 */
static void _detailed_god_description(god_type which_god)
{
    clrscr();

    const int width = min(80, get_number_of_cols()) - 1;

    _print_top_line(which_god, width);

    _print_string_wrapped(getLongDescription(god_name(which_god) + " powers"),
                          width);

    _print_string_wrapped(get_god_likes(which_god, true), width);
    _print_string_wrapped(_get_god_misc_info(which_god), width);
}

/**
 * Describe the given god's level of irritation at the player.
 *
 * Player may or may not be currently under penance.
 *
 * @param which_god     The god in question.
 * @return              A description of the god's ire (or lack thereof).
 */
static string _god_penance_message(god_type which_god)
{
    int which_god_penance = you.penance[which_god];

    // Give more appropriate message for the good gods.
    // XXX: ^ this is a hack
    if (which_god_penance > 0 && is_good_god(which_god))
    {
        if (is_good_god(you.religion))
            which_god_penance = 0;
        else if (!god_hates_your_god(which_god) && which_god_penance >= 5)
            which_god_penance = 2; // == "Come back to the one true church!"
    }

    const string penance_message =
        (which_god == GOD_NEMELEX_XOBEH
         && which_god_penance > 0 && which_god_penance <= 100)
            ? "%s doesn't play fair with you." :
        (which_god_penance >= 50)   ? "%s's wrath is upon you!" :
        (which_god_penance >= 20)   ? "%s is annoyed with you." :
        (which_god_penance >=  5)   ? "%s well remembers your sins." :
        (which_god_penance >   0)   ? "%s is ready to forgive your sins." :
        (you.worshipped[which_god]) ? "%s is ambivalent towards you."
                                    : "%s is neutral towards you.";

    return make_stringf(penance_message.c_str(),
                        uppercase_first(god_name(which_god)).c_str());
}

/**
 * Print a description of the powers & abilities granted to the player by the
 * given god. If player worships the god, the currently available powers are
 * highlighted.
 *
 * @param which_god     The god in question.
 */
static void _describe_god_powers(god_type which_god)
{
    int piety = you_worship(which_god) ? you.piety : 0;

    textcolour(LIGHTGREY);
    const char *header = "Granted powers:";
    const char *cost   = "(Cost)";
    cprintf("\n\n%s%*s%s\n", header,
            min(80, get_number_of_cols()) - 1 - strwidth(header) - strwidth(cost),
            "", cost);

    bool have_any = false;

    // set default color here, so we don't have to set in multiple places for
    // always available passive abilities
    if (!you_worship(which_god))
        textcolour(DARKGREY);
    else
        textcolour(god_colour(which_god));

    // mv: Some gods can protect you from harm.
    // The god isn't really protecting the player - only sometimes saving
    // his life.
    if (have_passive(passive_t::protect_from_harm))
    {
        have_any = true;

        int prot_chance = 10 + piety/10; // chance * 100
        const char *when = "";

        if (which_god == GOD_ELYVILON)
        {
            switch (elyvilon_lifesaving())
            {
                case 1:
                    when = ", especially when called upon";
                    prot_chance += 100 - 3000/piety;
                    break;
                case 2:
                    when = ", and always does so when called upon";
                    prot_chance = 100;
                    break;
            }
        }

        const char *how = (prot_chance >= 85) ? "carefully" :
                          (prot_chance >= 55) ? "often" :
                          (prot_chance >= 25) ? "sometimes"
                                              : "occasionally";

        cprintf("%s %s watches over you%s.\n",
                uppercase_first(god_name(which_god)).c_str(),
                how,
                when);
    }

    switch (which_god)
    {
    case GOD_ZIN:
    {
        have_any = true;
        const char *how =
            (piety >= piety_breakpoint(5)) ? "carefully" :
            (piety >= piety_breakpoint(3)) ? "often" :
            (piety >= piety_breakpoint(1)) ? "sometimes" :
                                             "occasionally";

        cprintf("%s %s shields you from chaos.\n",
                uppercase_first(god_name(which_god)).c_str(), how);
        break;
    }

    case GOD_SHINING_ONE:
    {
        have_any = true;
        if (piety < piety_breakpoint(1))
            textcolour(DARKGREY);
        else
            textcolour(god_colour(which_god));
        const char *how =
            (piety >= piety_breakpoint(5)) ? "completely" :
            (piety >= piety_breakpoint(3)) ? "mostly" :
                                             "partially";

        cprintf("%s %s shields you from negative energy.\n",
                uppercase_first(god_name(which_god)).c_str(), how);

        const int halo_size = you_worship(which_god) ? you.halo_radius() : -1;
        if (halo_size < 0)
            textcolour(DARKGREY);
        else
            textcolour(god_colour(which_god));
        cprintf("You radiate a%s righteous aura, and others within it are "
                "easier to hit.\n",
                halo_size > 5 ? " large" :
                halo_size > 3 ? "" :
                                " small");
        break;
    }

    case GOD_BEOGH:
        have_any = true;
        cprintf("You can pray to sacrifice all orcish remains on your "
                "square.\n");
        break;

    case GOD_JIYVA:
        have_any = true;
        if (piety < piety_breakpoint(2))
            textcolour(DARKGREY);
        else
            textcolour(god_colour(which_god));
        cprintf("%s shields you from corrosive effects.\n",
                uppercase_first(god_name(which_god)).c_str());

        if (piety < piety_breakpoint(1))
            textcolour(DARKGREY);
        else
            textcolour(god_colour(which_god));
        cprintf("You gain nutrition%s when your fellow slimes consume items.\n",
                piety >= piety_breakpoint(4) ? ", power and health" :
                piety >= piety_breakpoint(3) ? " and power" :
                                               "");
        break;

    case GOD_FEDHAS:
        have_any = true;
        cprintf("You can walk through plants and fire through allied plants.\n");
        break;

    case GOD_ASHENZARI:
        have_any = true;
        cprintf("You are provided with a bounty of information.\n");
        break;

    case GOD_CHEIBRIADOS:
        have_any = true;
        if (!in_good_standing(which_god))
            textcolour(DARKGREY);
        else
            textcolour(god_colour(which_god));
        cprintf("%s supports your attributes (+%d).\n",
                uppercase_first(god_name(which_god)).c_str(),
                chei_stat_boost(piety));
        break;

    case GOD_VEHUMET:
        have_any = true;
        if (const int numoffers = you.vehumet_gifts.size())
        {
            const char* offer = numoffers == 1
                               ? spell_title(*you.vehumet_gifts.begin())
                               : "some of Vehumet's most lethal spells";
            cprintf("You can memorise %s.\n", offer);
        }
        else
        {
            textcolour(DARKGREY);
            cprintf("You can memorise some of Vehumet's spells.\n");
        }
        break;

    case GOD_DITHMENOS:
    {
        have_any = true;
        const int umbra_size = you_worship(which_god) ? you.umbra_radius() : -1;
        if (umbra_size < 0)
            textcolour(DARKGREY);
        else
            textcolour(god_colour(which_god));
        cprintf("You radiate a%s aura of darkness, enhancing your stealth "
                "and reducing the accuracy of your foes.\n",
                umbra_size > 5 ? " large" :
                umbra_size > 3 ? "n" :
                                 " small");
        break;
    }

    case GOD_GOZAG:
        have_any = true;
        cprintf("You passively detect gold.\n");
        cprintf("%s turns your defeated foes' bodies to gold.\n",
                uppercase_first(god_name(which_god)).c_str());
        cprintf("Your enemies may become distracted by gold.\n");
        break;

    case GOD_PAKELLAS:
    {
        have_any = true;
        cprintf("%s identifies device charges for you.\n",
                uppercase_first(god_name(which_god)).c_str());
    }

    default:
        break;
    }

    for (const auto& power : get_god_powers(which_god))
    {
        // hack: don't mention the necronomicon alone unless it
        // wasn't already mentioned by the other description
        if (power.abil == ABIL_KIKU_GIFT_NECRONOMICON
            && you.species != SP_FELID)
        {
            continue;
        }
        have_any = true;

        if (you_worship(which_god)
            && (power.rank <= 0
                || power.rank == 7 && can_do_capstone_ability(which_god)
                || piety_rank(piety) >= power.rank)
            && (!player_under_penance()
                || power.rank == -1))
        {
            textcolour(god_colour(which_god));
        }
        else
            textcolour(DARKGREY);

        string buf = power.gain;
        if (!isupper(buf[0])) // Complete sentence given?
            buf = "You can " + buf + ".";
        const int desc_len = buf.size();

        string abil_cost = "(" + make_cost_description(power.abil) + ")";
        if (abil_cost == "(None)")
            abil_cost = "";

        cprintf("%s%*s%s\n", buf.c_str(),
                min(80, get_number_of_cols()) - 1 - desc_len - abil_cost.size(),
                "", abil_cost.c_str());
        textcolour(god_colour(which_god));
    }

    if (!have_any)
        cprintf("None.\n");
}

static void _god_overview_description(god_type which_god, bool give_title)
{
    clrscr();

    const int numcols = min(80, get_number_of_cols()) - 1;
    if (give_title)
    {
        textcolour(WHITE);
        cprintf("Religion");
        textcolour(LIGHTGREY);
    }
    // Center top line even if it already contains "Religion" (len = 8)
    _print_top_line(which_god, numcols - (give_title ? 2*8 : 0));

    // Print god's description.
    string god_desc = getLongDescription(god_name(which_god));
    cprintf("%s\n", get_linebreak_string(god_desc, numcols).c_str());

    // Title only shown for our own god.
    if (you_worship(which_god))
    {
        // Print title based on piety.
        cprintf("\nTitle  - ");
        textcolour(god_colour(which_god));

        string title = god_title(which_god, you.species, you.piety);
        cprintf("%s", title.c_str());
    }

    // mv: Now let's print favour as Brent suggested.
    // I know these messages aren't perfect so if you can think up
    // something better, do it.

    textcolour(LIGHTGREY);
    cprintf("\nFavour - ");
    textcolour(god_colour(which_god));

    if (!you_worship(which_god))
        cprintf(_god_penance_message(which_god).c_str());
    else
    {
        cprintf(_describe_favour(which_god).c_str());
        if (which_god == GOD_ASHENZARI)
            cprintf("\n%s", ash_describe_bondage(ETF_ALL, true).c_str());
    }
    _describe_god_powers(which_god);
}

static god_desc_type _describe_god_by_type(god_type which_god, bool give_title,
                                           god_desc_type gdesc)
{
    switch (gdesc)
    {
    case GDESC_OVERVIEW:
        _god_overview_description(which_god, give_title);
        break;
    case GDESC_DETAILED:
        _detailed_god_description(which_god);
        break;
    case GDESC_WRATH:
        _god_wrath_description(which_god);
        break;
    default:
        die("Unknown god description type!");
    }

    if (_check_description_cycle(gdesc))
        return static_cast<god_desc_type>((gdesc + 1) % NUM_GDESCS);
    else
        return NUM_GDESCS;
}

void describe_god(god_type which_god, bool give_title)
{
    if (which_god == GOD_NO_GOD) //mv: No god -> say it and go away.
    {
        mpr("You are not religious.");
        return;
    }

    god_desc_type gdesc = GDESC_OVERVIEW;
    while ((gdesc = _describe_god_by_type(which_god, give_title, gdesc))
            != NUM_GDESCS)
    {}
}
