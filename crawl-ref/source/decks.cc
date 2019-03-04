/**
 * @file
 * @brief Functions with decks of cards.
**/

#include "AppHdr.h"

#include "decks.h"

#include <algorithm>
#include <iostream>
#include <iterator>
#include <sstream>
#include <unordered_set>

#include "ability.h"
#include "abyss.h"
#include "act-iter.h"
#include "artefact.h"
#include "attitude-change.h"
#include "beam.h"
#include "cloud.h"
#include "coordit.h"
#include "dactions.h"
#include "database.h"
#include "describe.h"
#include "directn.h"
#include "dungeon.h"
#include "english.h"
#include "evoke.h"
#include "food.h"
#include "ghost.h"
#include "god-passive.h" // passive_t::no_haste
#include "god-wrath.h"
#include "invent.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "item-use.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "mon-cast.h"
#include "mon-clone.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-poly.h"
#include "mon-project.h"
#include "mon-tentacle.h"
#include "mon-util.h"
#include "mutation.h"
#include "nearby-danger.h"
#include "notes.h"
#include "output.h"
#include "player-equip.h"
#include "player-stats.h"
#include "potion.h"
#include "prompt.h"
#include "random.h"
#include "religion.h"
#include "scroller.h"
#include "spl-clouds.h"
#include "spl-goditem.h"
#include "spl-miscast.h"
#include "spl-monench.h"
#include "spl-other.h"
#include "spl-selfench.h"
#include "spl-summoning.h"
#include "spl-transloc.h"
#include "spl-wpnench.h"
#include "state.h"
#include "stringutil.h"
#include "teleport.h"
#include "terrain.h"
#include "transform.h"
#include "traps.h"
#include "uncancel.h"
#include "unicode.h"
#include "view.h"
#include "viewchar.h"
#include "xom.h"

using namespace ui;

typedef map<card_type, int> deck_archetype;

deck_archetype deck_of_escape =
{
    { CARD_TOMB,       5 },
    { CARD_EXILE,      1 },
    { CARD_ELIXIR,     5 },
    { CARD_CLOUD,      5 },
    { CARD_VELOCITY,   5 },
    { CARD_SHAFT,      5 },
};

deck_archetype deck_of_destruction =
{
    { CARD_VITRIOL,    5 },
    { CARD_PAIN,       5 },
    { CARD_ORB,        5 },
    { CARD_DEGEN,      3 },
    { CARD_WILD_MAGIC, 5 },
    { CARD_STORM,      5 },
};

deck_archetype deck_of_summoning =
{
    { CARD_ELEMENTS,        5 },
    { CARD_SUMMON_DEMON,    5 },
    { CARD_SUMMON_WEAPON,   5 },
    { CARD_SUMMON_FLYING,   5 },
    { CARD_RANGERS,         5 },
    { CARD_ILLUSION,        5 },
};

deck_archetype deck_of_punishment =
{
    { CARD_WRAITH,     5 },
    { CARD_WRATH,      5 },
    { CARD_FAMINE,     5 },
    { CARD_SWINE,      5 },
    { CARD_TORMENT,    5 },
};

struct deck_type_data
{
    string name;
    string flavour;
    /// The list of cards this deck contains.
    deck_archetype cards;
    int deck_max;
};

static map<deck_type, deck_type_data> all_decks =
{
    { DECK_OF_ESCAPE, {
        "escape", "mainly dealing with various forms of escape.",
        deck_of_escape,
        13,
    } },
    { DECK_OF_DESTRUCTION, {
        "destruction", "most of which hurl death and destruction "
            "at one's foes (or, if unlucky, at oneself).",
        deck_of_destruction,
        26,
    } },
    { DECK_OF_SUMMONING, {
        "summoning", "depicting a range of weird and wonderful creatures.",
        deck_of_summoning,
        13,
    } },
    { DECK_OF_PUNISHMENT, {
        "punishment", "which wreak havoc on the user.", deck_of_punishment,
        0, // Not a user deck
    } },
};

vector<ability_type> deck_ability = {
    ABIL_NEMELEX_DRAW_ESCAPE,
    ABIL_NEMELEX_DRAW_DESTRUCTION,
    ABIL_NEMELEX_DRAW_SUMMONING,
    ABIL_NON_ABILITY,
    ABIL_NEMELEX_DRAW_STACK
};

const char* card_name(card_type card)
{
    switch (card)
    {
    case CARD_VELOCITY:        return "Velocity";
    case CARD_EXILE:           return "Exile";
    case CARD_ELIXIR:          return "the Elixir";
    case CARD_STAIRS:          return "the Stairs";
    case CARD_TOMB:            return "the Tomb";
    case CARD_WILD_MAGIC:      return "Wild Magic";
    case CARD_ELEMENTS:        return "the Elements";
    case CARD_SUMMON_DEMON:    return "the Pentagram";
    case CARD_SUMMON_WEAPON:   return "the Dance";
    case CARD_SUMMON_FLYING:   return "Foxfire";
    case CARD_RANGERS:         return "the Rangers";
    case CARD_SHAFT:           return "the Shaft";
    case CARD_VITRIOL:         return "Vitriol";
    case CARD_CLOUD:           return "the Cloud";
    case CARD_STORM:           return "the Storm";
    case CARD_PAIN:            return "Pain";
    case CARD_TORMENT:         return "Torment";
    case CARD_WRATH:           return "Wrath";
    case CARD_WRAITH:          return "the Wraith";
    case CARD_SWINE:           return "the Swine";
    case CARD_ORB:             return "the Orb";
    case CARD_ILLUSION:        return "the Illusion";
    case CARD_DEGEN:           return "Degeneration";
    case CARD_FAMINE:          return "Famine";

    case NUM_CARDS:            return "a buggy card";
    }
    return "a very buggy card";
}

card_type name_to_card(string name)
{
    for (int i = 0; i < NUM_CARDS; i++)
    {
        if (card_name(static_cast<card_type>(i)) == name)
            return static_cast<card_type>(i);
    }
    return NUM_CARDS;
}

static const deck_archetype _cards_in_deck(deck_type deck)
{
    deck_type_data *deck_data = map_find(all_decks, deck);

    if (deck_data)
        return deck_data->cards;

#ifdef ASSERTS
    die("No cards found for %u", unsigned(deck));
#endif
    return {};
}

const string stack_contents()
{
    const auto& stack = you.props[NEMELEX_STACK_KEY].get_vector();

    string output = "";
    output += comma_separated_fn(
                reverse_iterator<CrawlVector::const_iterator>(stack.end()),
                reverse_iterator<CrawlVector::const_iterator>(stack.begin()),
              [](const CrawlStoreValue& card) { return card_name((card_type)card.get_int()); });
    if (!stack.empty())
        output += ".";

    return output;
}

const string stack_top()
{
    const auto& stack = you.props[NEMELEX_STACK_KEY].get_vector();
    if (stack.empty())
        return "none";
    else
        return card_name((card_type) stack[stack.size() - 1].get_int());
}

const string deck_contents(deck_type deck)
{
    if (deck == DECK_STACK)
        return "Remaining cards: " + stack_contents();

    string output = "It may contain the following cards: ";

    // This way of doing things is intended to prevent a card
    // that appears in multiple subdecks from showing up twice in the
    // output.
    set<card_type> cards;
    const deck_archetype &pdeck =_cards_in_deck(deck);
    for (const auto& cww : pdeck)
        cards.insert(cww.first);

    output += comma_separated_fn(cards.begin(), cards.end(), card_name);
    output += ".";

    return output;
}

const string deck_flavour(deck_type deck)
{
    if (deck == DECK_STACK)
        return "set aside for later.";

    deck_type_data* deck_data = map_find(all_decks, deck);

    if (deck_data)
        return deck_data->flavour;

    return "";
}

static card_type _random_card(deck_type deck)
{
    const deck_archetype &pdeck = _cards_in_deck(deck);
    return *random_choose_weighted(pdeck);
}

int deck_cards(deck_type deck)
{
    return deck == DECK_STACK ? you.props[NEMELEX_STACK_KEY].get_vector().size()
                              : you.props[deck_name(deck)].get_int();
}

bool gift_cards()
{
    const int deal = random_range(MIN_GIFT_CARDS, MAX_GIFT_CARDS);
    bool dealt_cards = false;

    for (int i = 0; i < deal; i++)
    {
        deck_type choice = random_choose_weighted(
                                        3, DECK_OF_DESTRUCTION,
                                        1, DECK_OF_SUMMONING,
                                        1, DECK_OF_ESCAPE);
        if (deck_cards(choice) < all_decks[choice].deck_max)
        {
            you.props[deck_name(choice)]++;
            dealt_cards = true;
        }
    }

    return dealt_cards;
}

void reset_cards()
{
    for (int i = FIRST_PLAYER_DECK; i <= LAST_PLAYER_DECK; i++)
        you.props[deck_name((deck_type) i)] = 0;
    you.props[NEMELEX_STACK_KEY].get_vector().clear();
}

string deck_summary()
{
    vector<string> stats;
    for (int i = FIRST_PLAYER_DECK; i <= LAST_PLAYER_DECK; i++)
    {
        int cards = deck_cards((deck_type) i);
        const deck_type_data *deck_data = map_find(all_decks, (deck_type) i);
        const string name = deck_data ? deck_data->name : "bugginess";
        if (cards)
        {
            stats.push_back(make_stringf("%d %s card%s", cards,
               name.c_str(), cards == 1 ? "" : "s"));
        }
    }
    return comma_separated_line(stats.begin(), stats.end());
}

string which_decks(card_type card)
{
    vector<string> decks;
    string output = "\n";
    bool punishment = false;
    for (auto &deck_data : all_decks)
    {
        if (!deck_data.second.cards.count(card))
            continue;

        if (deck_data.first == DECK_OF_PUNISHMENT)
            punishment = true;
        else
            decks.push_back(deck_data.second.name);
    }

    if (!decks.empty())
    {
        output += "It is found in decks of "
               +  comma_separated_line(decks.begin(), decks.end());
        if (punishment)
            output += ", or in Nemelex Xobeh's deck of punishment";
        output += ".";
    }
    else if (punishment)
    {
        output += "It is only found in Nemelex Xobeh's deck of "
                  "punishment.";
    }
    else
        output += "It is normally not part of any deck.";

    return output;
}

static void _describe_cards(CrawlVector& cards)
{
    ASSERT(!cards.empty());

    auto scroller = make_shared<Scroller>();
    auto vbox = make_shared<Box>(Widget::VERT);

#ifdef USE_TILE_WEB
    tiles.json_open_object();
    tiles.json_open_array("cards");
#endif
    bool seen[NUM_CARDS] = {0};
    ostringstream data;
    bool first = true;
    for (auto& val : cards)
    {
        card_type card = (card_type) val.get_int();

        if (seen[card])
            continue;
        seen[card] = true;

        string name = card_name(card);
        string desc = getLongDescription(name + " card");
        if (desc.empty())
            desc = "No description found.\n";
        string decks = which_decks(card);

        name = uppercase_first(name);
        desc = desc + decks;

    auto title_hbox = make_shared<Box>(Widget::HORZ);
#ifdef USE_TILE
        auto icon = make_shared<Image>();
        icon->set_tile(tile_def(TILEG_NEMELEX_CARD, TEX_GUI));
        title_hbox->add_child(move(icon));
#endif
        auto title = make_shared<Text>(formatted_string(name, WHITE));
        title->set_margin_for_crt({0, 0, 0, 0});
        title->set_margin_for_sdl({0, 0, 0, 10});
        title_hbox->add_child(move(title));
        title_hbox->align_items = Widget::CENTER;
        title_hbox->set_margin_for_crt({first ? 0 : 1, 0, 1, 0});
        title_hbox->set_margin_for_sdl({first ? 0 : 20, 0, 20, 0});
        vbox->add_child(move(title_hbox));

        auto text = make_shared<Text>(desc);
        text->wrap_text = true;
        vbox->add_child(move(text));

#ifdef USE_TILE_WEB
        tiles.json_open_object();
        tiles.json_write_string("name", name);
        tiles.json_write_string("desc", desc);
        tiles.json_close_object();
#endif
        first = false;
    }

#ifdef USE_TILE_LOCAL
    vbox->max_size()[0] = tiles.get_crt_font()->char_width()*80;
#endif

    scroller->set_child(move(vbox));
    auto popup = make_shared<ui::Popup>(scroller);

    bool done = false;
    popup->on(Widget::slots.event, [&done, &scroller](wm_event ev) {
        if (ev.type != WME_KEYDOWN)
            return false;
        done = !scroller->on_event(ev);
        return true;
    });

#ifdef USE_TILE_WEB
    tiles.json_close_array();
    tiles.push_ui_layout("describe-cards", 0);
#endif

    ui::run_layout(move(popup), done);

#ifdef USE_TILE_WEB
    tiles.pop_ui_layout();
#endif
}

string deck_status(deck_type deck)
{
    const string name = deck_name(deck);
    const int cards   = deck_cards(deck);

    ostringstream desc;

    desc << chop_string(deck_name(deck), 24)
         << to_string(cards);

    return trimmed_string(desc.str());
}

string deck_description(deck_type deck)
{
    ostringstream desc;

    desc << "A deck of magical cards, ";
    desc << deck_flavour(deck) << "\n\n";
    desc << deck_contents(deck) << "\n";

    if (deck != DECK_STACK)
    {
        const int cards = deck_cards(deck);
        desc << "\n";

        if (cards > 1)
            desc << make_stringf("It currently has %d cards ", cards);
        else if (cards == 1)
            desc << "It currently has 1 card ";
        else
            desc << "It is currently empty ";

        desc << make_stringf("and can contain up to %d cards.",
                             all_decks[deck].deck_max);
        desc << "\n";
    }

    return desc.str();
}

/**
 * The deck a given ability uses. Asserts if called on an ability that does not
 * use decks.
 *
 * @param abil the ability
 *
 * @return the deck
 */
deck_type ability_deck(ability_type abil)
{
    auto deck = find(deck_ability.begin(), deck_ability.end(), abil);

    ASSERT(deck != deck_ability.end());
    return (deck_type) distance(deck_ability.begin(), deck);
}

// This will assert if the player doesn't have the ability to draw from the
// deck passed.
static char _deck_hotkey(deck_type deck)
{
    return get_talent(deck_ability[deck], false).hotkey;
}

static deck_type _choose_deck(const string title = "Draw")
{
    ToggleableMenu deck_menu(MF_SINGLESELECT | MF_ANYPRINTABLE
            | MF_NO_WRAP_ROWS | MF_TOGGLE_ACTION | MF_ALWAYS_SHOW_MORE);
    {
        ToggleableMenuEntry* me =
            new ToggleableMenuEntry(make_stringf("%s which deck?        "
                                    "Cards available", title.c_str()),
                                    "Describe which deck?    "
                                    "Cards available",
                                    MEL_TITLE);
#ifdef USE_TILE_LOCAL
        me->colour = BLUE;
#endif
        deck_menu.set_title(me, true, true);
    }
    deck_menu.set_tag("deck");
    deck_menu.add_toggle_key('!');
    deck_menu.add_toggle_key('?');
    deck_menu.menu_action = Menu::ACT_EXECUTE;

    deck_menu.set_more(formatted_string::parse_string(
                       "Press '<w>!</w>' or '<w>?</w>' to toggle "
                       "between deck selection and description."));

    int numbers[NUM_DECKS];

    for (int i = FIRST_PLAYER_DECK; i <= LAST_PLAYER_DECK; i++)
    {
        ToggleableMenuEntry* me =
            new ToggleableMenuEntry(deck_status(static_cast<deck_type>(i)),
                    deck_status(static_cast<deck_type>(i)),
                    MEL_ITEM, 1, _deck_hotkey(static_cast<deck_type>(i)));
        numbers[i] = i;
        me->data = &numbers[i];
        if (!deck_cards((deck_type)i))
            me->colour = COL_USELESS;

#ifdef USE_TILE
        me->add_tile(tile_def(TILEG_NEMELEX_DECK + i - FIRST_PLAYER_DECK + 1, TEX_GUI));
#endif
        deck_menu.add_entry(me);
    }

    int ret = NUM_DECKS;
    deck_menu.on_single_selection = [&deck_menu, &ret](const MenuEntry& sel)
    {
        ASSERT(sel.hotkeys.size() == 1);
        int selected = *(static_cast<int*>(sel.data));

        if (deck_menu.menu_action == Menu::ACT_EXAMINE)
            describe_deck((deck_type) selected);
        else
            ret = *(static_cast<int*>(sel.data));
        return deck_menu.menu_action == Menu::ACT_EXAMINE;
    };
    deck_menu.show(false);
    if (!crawl_state.doing_prev_cmd_again)
        redraw_screen();
    return (deck_type) ret;
}

/**
 * Printed when a deck is exhausted
 *
 * @return          A message to print;
 *                  e.g. "the deck of cards disappears without a trace."
 */
static string _empty_deck_msg()
{
    string message = random_choose("disappears without a trace.",
        "glows slightly and disappears.",
        "glows with a rainbow of weird colours and disappears.");
    return "The deck of cards " + message;
}

static void _evoke_deck(deck_type deck, bool dealt = false)
{
    ASSERT(deck_cards(deck) > 0);

    mprf("You %s a card...", dealt ? "deal" : "draw");

    if (deck == DECK_STACK)
    {
        auto& stack = you.props[NEMELEX_STACK_KEY].get_vector();
        card_type card = (card_type) stack[stack.size() - 1].get_int();
        stack.pop_back();
        card_effect(card, dealt);
    }
    else
    {
        --you.props[deck_name(deck)];
        card_effect(_random_card(deck), dealt);
    }

    if (!deck_cards(deck))
        mpr(_empty_deck_msg());
}

// Draw one card from a deck, prompting the user for a choice
bool deck_draw(deck_type deck)
{
    if (!deck_cards(deck))
    {
        mpr("That deck is empty!");
        return false;
    }

    _evoke_deck(deck);
    return true;
}

bool deck_stack()
{
    int total_cards = 0;

    for (int i = FIRST_PLAYER_DECK; i <= LAST_PLAYER_DECK; ++i)
        total_cards += deck_cards((deck_type) i);

    if (deck_cards(DECK_STACK) && !yesno("Replace your current stack?",
                                          false, 0))
    {
        return false;
    }

    if (!total_cards)
    {
        mpr("You are out of cards!");
        return false;
    }

    if (total_cards < 5 && !yesno("You have fewer than five cards, "
                                  "stack them anyway?", false, 0))
    {
        canned_msg(MSG_OK);
        return false;
    }

    you.props[NEMELEX_STACK_KEY].get_vector().clear();
    run_uncancel(UNC_STACK_FIVE, min(total_cards, 5));
    return true;
}

class StackFiveMenu : public Menu
{
    virtual bool process_key(int keyin) override;
    CrawlVector& draws;
public:
    StackFiveMenu(CrawlVector& d)
        : Menu(MF_NOSELECT | MF_UNCANCEL | MF_ALWAYS_SHOW_MORE), draws(d) {};
};

bool StackFiveMenu::process_key(int keyin)
{
    if (keyin == CK_ENTER)
    {
        formatted_string old_more = more;
        set_more(formatted_string::parse_string(
                "Are you done? (press y or Y to confirm)"));
        if (yesno(nullptr, true, 'n', false, false, true))
            return false;
        set_more(old_more);
    }
    else if (keyin == '?')
        _describe_cards(draws);
    else if (keyin >= '1' && keyin <= '0' + static_cast<int>(draws.size()))
    {
        const unsigned int i = keyin - '1';
        for (unsigned int j = 0; j < items.size(); j++)
            if (items[j]->selected())
            {
                swap(draws[i], draws[j]);
                swap(items[i]->text, items[j]->text);
                items[j]->colour = LIGHTGREY;
                select_item_index(i, 0, false); // this also updates the item
                select_item_index(j, 0, false);
                return true;
            }
        items[i]->colour = WHITE;
        select_item_index(i, 1, false);
    }
    else
        Menu::process_key(keyin);
    return true;
}

static void _draw_stack(int to_stack)
{
    ToggleableMenu deck_menu(MF_SINGLESELECT | MF_UNCANCEL
            | MF_NO_WRAP_ROWS | MF_TOGGLE_ACTION | MF_ALWAYS_SHOW_MORE);
    {
        ToggleableMenuEntry* me =
            new ToggleableMenuEntry("Draw which deck?        "
                                    "Cards available",
                                    "Describe which deck?    "
                                    "Cards available",
                                    MEL_TITLE);
#ifdef USE_TILE_LOCAL
        me->colour = BLUE;
#endif
        deck_menu.set_title(me, true, true);
    }
    deck_menu.set_tag("deck");
    deck_menu.add_toggle_key('!');
    deck_menu.add_toggle_key('?');
    deck_menu.menu_action = Menu::ACT_EXECUTE;

    deck_menu.set_more(formatted_string::parse_string(
                       "Press '<w>!</w>' or '<w>?</w>' to toggle "
                       "between deck selection and description."));

    int numbers[NUM_DECKS];

    for (int i = FIRST_PLAYER_DECK; i <= LAST_PLAYER_DECK; i++)
    {
        ToggleableMenuEntry* me =
            new ToggleableMenuEntry(deck_status((deck_type)i),
                    deck_status((deck_type)i),
                    MEL_ITEM, 1, _deck_hotkey((deck_type)i));
        numbers[i] = i;
        me->data = &numbers[i];
        if (!deck_cards((deck_type)i))
            me->colour = COL_USELESS;

#ifdef USE_TILE
        me->add_tile(tile_def(TILEG_NEMELEX_DECK + i - FIRST_PLAYER_DECK + 1, TEX_GUI));
#endif
        deck_menu.add_entry(me);
    }

    auto& stack = you.props[NEMELEX_STACK_KEY].get_vector();
    deck_menu.on_single_selection = [&deck_menu, &stack, to_stack](const MenuEntry& sel)
    {
        ASSERT(sel.hotkeys.size() == 1);
        deck_type selected = (deck_type) *(static_cast<int*>(sel.data));
        // Need non-const access to the selection.
        ToggleableMenuEntry* me =
            static_cast<ToggleableMenuEntry*>(deck_menu.selected_entries()[0]);

        if (deck_menu.menu_action == Menu::ACT_EXAMINE)
            describe_deck(selected);
        else
        {
            you.props[deck_name(selected)]--;
            me->text = deck_status(selected);
            me->alt_text = deck_status(selected);

            card_type draw = _random_card(selected);
            stack.push_back(draw);
            string status = "Drawn so far: " + stack_contents();
            deck_menu.set_more(formatted_string::parse_string(
                       status + "\n" +
                       "Press '<w>!</w>' or '<w>?</w>' to toggle "
                       "between deck selection and description."));
        }
        return stack.size() < to_stack
               || deck_menu.menu_action == Menu::ACT_EXAMINE;
    };
    deck_menu.show(false);
}

bool stack_five(int to_stack)
{
    auto& stack = you.props[NEMELEX_STACK_KEY].get_vector();

    while (stack.size() < to_stack)
    {
        if (crawl_state.seen_hups)
            return false;

        _draw_stack(to_stack);
    }

    StackFiveMenu menu(stack);
    MenuEntry *const title = new MenuEntry("Select two cards to swap them:", MEL_TITLE);
    menu.set_title(title);
    for (unsigned int i = 0; i < stack.size(); i++)
    {
        MenuEntry * const entry =
            new MenuEntry(card_name((card_type)stack[i].get_int()),
                          MEL_ITEM, 1, '1'+i);
#ifdef USE_TILE
        entry->add_tile(tile_def(TILEG_NEMELEX_CARD, TEX_GUI));
#endif
        menu.add_entry(entry);
    }
    menu.set_more(formatted_string::parse_string(
                "<lightgrey>Press <w>?</w> for the card descriptions"
                " or <w>Enter</w> to accept."));
    menu.show();

    std::reverse(stack.begin(), stack.end());

    return true;
}

// Draw the top four cards of an deck and play them all.
// Return false if the operation was failed/aborted along the way.
bool deck_deal()
{
    deck_type choice = _choose_deck("Deal");

    if (choice == NUM_DECKS)
        return false;

    int num_cards = deck_cards(choice);

    if (!num_cards)
    {
        mpr("That deck is empty!");
        return false;
    }

    const int num_to_deal = min(num_cards, 4);

    for (int i = 0; i < num_to_deal; ++i)
        _evoke_deck(choice, true);

    return true;
}

// Draw the next three cards, discard two and pick one.
bool deck_triple_draw()
{
    deck_type choice = _choose_deck();

    if (choice == NUM_DECKS)
        return false;

    int num_cards = deck_cards(choice);

    if (!num_cards)
    {
        mpr("That deck is empty!");
        return false;
    }

    if (num_cards < 3 && !yesno("There's fewer than three cards, "
                                "still triple draw?", false, 0))
    {
        canned_msg(MSG_OK);
        return false;
    }

    if (num_cards == 1)
    {
        // Only one card to draw, so just draw it.
        mpr("There's only one card left!");
        _evoke_deck(choice);
        return true;
    }

    const int num_to_draw = min(num_cards, 3);

    you.props[deck_name(choice)] = deck_cards(choice) - num_to_draw;

    auto& draw = you.props[NEMELEX_TRIPLE_DRAW_KEY].get_vector();
    draw.clear();

    for (int i = 0; i < num_to_draw; ++i)
        draw.push_back(_random_card(choice));

    run_uncancel(UNC_DRAW_THREE, 0);
    return true;
}

bool draw_three()
{
    auto& draws = you.props[NEMELEX_TRIPLE_DRAW_KEY].get_vector();

    int selected = -1;
    bool need_prompt_redraw = true;
    while (true)
    {
        if (need_prompt_redraw)
        {
            mpr("You draw... (choose one card, ? for their descriptions)");
            for (int i = 0; i < draws.size(); ++i)
            {
                msg::streams(MSGCH_PROMPT)
                    << msg::nocap << (static_cast<char>(i + 'a')) << " - "
                    << card_name((card_type)draws[i].get_int()) << endl;
            }
            need_prompt_redraw = false;
        }
        const int keyin = toalower(get_ch());

        if (crawl_state.seen_hups)
            return false;

        if (keyin == '?')
        {
            _describe_cards(draws);
            redraw_screen();
            need_prompt_redraw = true;
        }
        else if (keyin >= 'a' && keyin < 'a' + draws.size())
        {
            selected = keyin - 'a';
            break;
        }
        else
            canned_msg(MSG_HUH);
    }

    card_effect((card_type) draws[selected].get_int());

    return true;
}

// This is Nemelex retribution. If deal is true, use the word "deal"
// rather than "draw" (for the Deal Four out-of-cards situation).
void draw_from_deck_of_punishment(bool deal)
{
    card_type card = _random_card(DECK_OF_PUNISHMENT);

    mprf("You %s a card...", deal ? "deal" : "draw");
    card_effect(card, deal, true);
}

static int _get_power_level(int power)
{
    int power_level = x_chance_in_y(power, 900) + x_chance_in_y(power, 2700);

    // other functions in this file will break if this assertion is violated
    ASSERT(power_level >= 0 && power_level <= 2);
    return power_level;
}

// Actual card implementations follow.

static void _velocity_card(int power)
{

    const int power_level = _get_power_level(power);
    bool did_something = false;

    if (you.duration[DUR_SLOW] && x_chance_in_y(power_level, 2))
    {
        you.duration[DUR_SLOW] = 1;
        did_something = true;
    }

    if (!apply_visible_monsters([=](monster& mon)
          {
              bool affected = false;
              if (!mons_immune_magic(mon))
              {
                  const bool hostile = !mon.wont_attack();
                  const bool haste_immune = (mon.stasis()
                                             || mons_is_immotile(mon));

                  bool did_haste = false;

                  if (hostile)
                  {
                      if (x_chance_in_y(1 + power_level, 3))
                      {
                          do_slow_monster(mon, &you);
                          affected = true;
                      }
                  }
                  else //allies
                  {
                      if (!haste_immune && x_chance_in_y(power_level, 2))
                      {
                          mon.add_ench(ENCH_HASTE);
                          affected = true;
                          did_haste = true;
                      }
                  }

                  if (did_haste)
                      simple_monster_message(mon, " seems to speed up.");
              }
              return affected;
          })
        && !did_something)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
    }
}

static void _exile_card(int power)
{
    if (player_in_branch(BRANCH_ABYSS))
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return;
    }

    // Calculate how many extra banishments you get.
    const int power_level = _get_power_level(power);
    int extra_targets = power_level + random2(1 + power_level);

    for (int i = 0; i < 1 + extra_targets; ++i)
    {
        // Pick a random monster nearby to banish (or yourself).
        monster* mon_to_banish = choose_random_nearby_monster(1);

        // Bonus banishments only banish monsters.
        if (i != 0 && !mon_to_banish)
            continue;

        if (!mon_to_banish) // Banish yourself!
        {
            banished("drawing a card");
            break;              // Don't banish anything else.
        }
        else
            mon_to_banish->banish(&you);
    }
}

static void _shaft_card(int power)
{
    const int power_level = _get_power_level(power);
    bool did_something = false;

    if (is_valid_shaft_level())
    {
        if (grd(you.pos()) == DNGN_FLOOR)
        {
            place_specific_trap(you.pos(), TRAP_SHAFT);
            trap_at(you.pos())->reveal();
            mpr("A shaft materialises beneath you!");
            did_something = true;
        }

        did_something = apply_visible_monsters([=](monster& mons)
        {
            return !mons.wont_attack()
                   && mons_is_threatening(mons)
                   && x_chance_in_y(power_level, 3)
                   && mons.do_shaft();
        }) || did_something;
    }

    if (!did_something)
        canned_msg(MSG_NOTHING_HAPPENS);
}

static int stair_draw_count = 0;

// This does not describe an actual card. Instead, it only exists to test
// the stair movement effect in wizard mode ("&c stairs").
static void _stairs_card(int /*power*/)
{
    you.duration[DUR_REPEL_STAIRS_MOVE]  = 0;
    you.duration[DUR_REPEL_STAIRS_CLIMB] = 0;

    if (feat_stair_direction(grd(you.pos())) == CMD_NO_CMD)
        you.duration[DUR_REPEL_STAIRS_MOVE]  = 1000;
    else
        you.duration[DUR_REPEL_STAIRS_CLIMB] =  500; // more annoying

    vector<coord_def> stairs_avail;

    for (radius_iterator ri(you.pos(), LOS_DEFAULT, true); ri; ++ri)
    {
        dungeon_feature_type feat = grd(*ri);
        if (feat_stair_direction(feat) != CMD_NO_CMD
            && feat != DNGN_ENTER_SHOP)
        {
            stairs_avail.push_back(*ri);
        }
    }

    if (stairs_avail.empty())
    {
        mpr("No stairs available to move.");
        return;
    }

    shuffle_array(stairs_avail);

    for (coord_def stair : stairs_avail)
        move_stair(stair, stair_draw_count % 2, false);

    stair_draw_count++;
}

static monster* _friendly(monster_type mt, int dur)
{
    return create_monster(mgen_data(mt, BEH_FRIENDLY, you.pos(), MHITYOU,
                                    MG_AUTOFOE)
                          .set_summoned(&you, dur, 0));
}

static void _damaging_card(card_type card, int power,
                           bool dealt = false)
{
    const int power_level = _get_power_level(power);
    const char *participle = dealt ? "dealt" : "drawn";

    bool done_prompt = false;
    string prompt = make_stringf("You have %s %s.", participle,
                                 card_name(card));

    dist target;
    zap_type ztype = ZAP_DEBUGGING_RAY;
    const zap_type painzaps[2] = { ZAP_AGONY, ZAP_BOLT_OF_DRAINING };
    const zap_type acidzaps[3] = { ZAP_BREATHE_ACID, ZAP_CORROSIVE_BOLT,
                                   ZAP_CORROSIVE_BOLT };
    const zap_type orbzaps[3]  = { ZAP_ISKENDERUNS_MYSTIC_BLAST, ZAP_IOOD,
                                   ZAP_IOOD };

    switch (card)
    {
    case CARD_VITRIOL:
        if (power_level == 2)
        {
            done_prompt = true;
            mpr(prompt);
            mpr("You radiate a wave of entropy!");
            apply_visible_monsters([](monster& mons)
            {
                return !mons.wont_attack()
                       && mons_is_threatening(mons)
                       && coinflip()
                       && mons.corrode_equipment();
            });
        }
        ztype = acidzaps[power_level];
        break;

    case CARD_ORB:
        ztype = orbzaps[power_level];
        break;

    case CARD_PAIN:
        if (power_level == 2)
        {
            mpr(prompt);

            if (monster *ghost = _friendly(MONS_FLAYED_GHOST, 3))
            {
                apply_visible_monsters([&, ghost](monster& mons)
                {
                    if (mons.wont_attack()
                        || !(mons.holiness() & MH_NATURAL))
                    {
                        return false;
                    }


                    flay(*ghost, mons, mons.hit_points * 2 / 5);
                    return true;
                }, ghost->pos());

                ghost->foe = MHITYOU; // follow you around (XXX: rethink)
                return;
            }
            // else, fallback to level 1
        }

        ztype = painzaps[min(power_level, (int)ARRAYSZ(painzaps)-1)];
        break;

    default:
        break;
    }

    bolt beam;
    beam.range = LOS_RADIUS;

    direction_chooser_args args;
    args.mode = TARG_HOSTILE;
    if (!done_prompt)
        args.top_prompt = prompt;

    // Confirm aborts as they waste the card.
    prompt = make_stringf("Aiming: %s", card_name(card));
    while (!(spell_direction(target, beam, &args)
            && player_tracer(ZAP_DEBUGGING_RAY, power/6, beam)))
    {
        if (crawl_state.seen_hups
            || yesno("Really abort (and waste the card)?", false, 0))
        {
            canned_msg(MSG_OK);
            return;
        }
        args.top_prompt = prompt;
    }

    if (ztype == ZAP_IOOD)
    {
        if (power_level == 1)
        {
            cast_iood(&you, power/6, &beam, 0, 0,
                      mgrd(beam.target), false, false);
        }
        else
            cast_iood_burst(power/6, beam.target);
    }
    else
        zapping(ztype, power/6, beam);
}

static void _elixir_card(int power)
{
    int power_level = _get_power_level(power);

    you.duration[DUR_ELIXIR_HEALTH] = 0;
    you.duration[DUR_ELIXIR_MAGIC] = 0;

    switch (power_level)
    {
    case 0:
        if (coinflip())
            you.set_duration(DUR_ELIXIR_HEALTH, 1 + random2(3));
        else
            you.set_duration(DUR_ELIXIR_MAGIC, 3 + random2(5));
        break;
    case 1:
        if (you.hp * 2 < you.hp_max)
            you.set_duration(DUR_ELIXIR_HEALTH, 3 + random2(3));
        else
            you.set_duration(DUR_ELIXIR_MAGIC, 10);
        break;
    default:
        you.set_duration(DUR_ELIXIR_HEALTH, 10);
        you.set_duration(DUR_ELIXIR_MAGIC, 10);
    }

    if (you.duration[DUR_ELIXIR_HEALTH] && you.duration[DUR_ELIXIR_MAGIC])
        mpr("You begin rapidly regenerating health and magic.");
    else if (you.duration[DUR_ELIXIR_HEALTH])
        mpr("You begin rapidly regenerating.");
    else
        mpr("You begin rapidly regenerating magic.");

    apply_visible_monsters([=](monster& mon)
    {
        if (mon.wont_attack())
        {
            const int hp = mon.max_hit_points / (4 - power_level);
            if (mon.heal(hp + random2avg(hp, 2)))
               simple_monster_message(mon, " is healed.");
        }
        return true;
    });
}

// Special case for *your* god, maybe?
static void _godly_wrath()
{
    for (int tries = 0; tries < 100; tries++)
    {
        god_type god = random_god();

        // Don't recursively make player draw from the Deck of Punishment.
        if (god != GOD_NEMELEX_XOBEH && divine_retribution(god))
            return; // Stop once we find a god willing to punish the player.
    }

    mpr("You somehow manage to escape divine attention...");
}

static void _summon_demon_card(int power)
{
    const int power_level = _get_power_level(power);
    // one demon (potentially hostile), and one other demonic creature (always
    // friendly)
    monster_type dct, dct2;
    switch (power_level)
    {
    case 0:
        dct = random_demon_by_tier(4);
        dct2 = MONS_HELL_HOUND;
        break;
    case 1:
        dct = random_demon_by_tier(3);
        dct2 = MONS_RAKSHASA;
        break;
    default:
        dct = random_demon_by_tier(2);
        dct2 = MONS_PANDEMONIUM_LORD;
    }

    // FIXME: The manual testing for message printing is there because
    // we can't rely on create_monster() to do it for us. This is
    // because if you are completely surrounded by walls, create_monster()
    // will never manage to give a position which isn't (-1,-1)
    // and thus not print the message.
    // This hack appears later in this file as well.

    const bool hostile = one_chance_in(power_level + 4);

    if (!create_monster(mgen_data(dct, hostile ? BEH_HOSTILE : BEH_FRIENDLY,
                                  you.pos(), MHITYOU, MG_AUTOFOE)
                        .set_summoned(&you, 5 - power_level, 0)))
    {
        mpr("You see a puff of smoke.");
    }
    else if (hostile
             && mons_class_flag(dct, M_INVIS)
             && !you.can_see_invisible())
    {
        mpr("You sense the presence of something unfriendly.");
    }

    _friendly(dct2, 5 - power_level);
}

static void _elements_card(int power)
{

    const int power_level = _get_power_level(power);
    const monster_type element_list[][3] =
    {
        {MONS_RAIJU, MONS_WIND_DRAKE, MONS_SHOCK_SERPENT},
        {MONS_BASILISK, MONS_CATOBLEPAS, MONS_IRON_GOLEM},
        {MONS_FIRE_VORTEX, MONS_MOLTEN_GARGOYLE, MONS_FIRE_DRAGON},
        {MONS_ICE_BEAST, MONS_POLAR_BEAR, MONS_ICE_DRAGON}
    };

    int start = random2(ARRAYSZ(element_list));
    for (int i = 0; i < 3; ++i)
    {
        _friendly(element_list[start % ARRAYSZ(element_list)][power_level],
                  power_level + 2);
        start++;
    }

}

static void _summon_dancing_weapon(int power)
{
    const int power_level = _get_power_level(power);

    monster *mon =
        create_monster(
            mgen_data(MONS_DANCING_WEAPON, BEH_FRIENDLY, you.pos(), MHITYOU,
                      MG_AUTOFOE).set_summoned(&you, power_level + 2, 0),
            false);

    if (!mon)
    {
        mpr("You see a puff of smoke.");
        return;
    }

    // Override the weapon.
    ASSERT(mon->weapon() != nullptr);
    item_def& wpn(*mon->weapon());

    switch (power_level)
    {
    case 0:
        // Wimpy, negative-enchantment weapon.
        wpn.plus = random2(3) - 2;
        wpn.sub_type = random_choose(WPN_QUARTERSTAFF, WPN_HAND_AXE);

        set_item_ego_type(wpn, OBJ_WEAPONS,
                          random_choose(SPWPN_VENOM, SPWPN_NORMAL));
        break;
    case 1:
        // This is getting good.
        wpn.plus = random2(4) - 1;
        wpn.sub_type = random_choose(WPN_LONG_SWORD, WPN_TRIDENT);

        if (coinflip())
        {
            set_item_ego_type(wpn, OBJ_WEAPONS,
                              random_choose(SPWPN_FLAMING, SPWPN_FREEZING));
        }
        else
            set_item_ego_type(wpn, OBJ_WEAPONS, SPWPN_NORMAL);
        break;
    default:
        // Rare and powerful.
        wpn.plus = random2(4) + 2;
        wpn.sub_type = random_choose(WPN_DEMON_TRIDENT, WPN_EXECUTIONERS_AXE);

        set_item_ego_type(wpn, OBJ_WEAPONS,
                          random_choose(SPWPN_SPEED, SPWPN_ELECTROCUTION));
    }

    item_colour(wpn); // this is probably not needed

    // sometimes give a randart instead
    if (one_chance_in(3))
    {
        make_item_randart(wpn, true);
        set_ident_flags(wpn, ISFLAG_KNOW_PROPERTIES| ISFLAG_KNOW_TYPE);
    }

    // Don't leave a trail of weapons behind. (Especially not randarts!)
    mon->flags |= MF_HARD_RESET;

    ghost_demon newstats;
    newstats.init_dancing_weapon(wpn, power / 4);

    mon->set_ghost(newstats);
    mon->ghost_demon_init();
}

static void _summon_flying(int power)
{
    const int power_level = _get_power_level(power);

    const monster_type flytypes[] =
    {
        MONS_INSUBSTANTIAL_WISP, MONS_WYVERN, MONS_KILLER_BEE,
        MONS_VAMPIRE_MOSQUITO, MONS_HORNET
    };
    const int num_flytypes = ARRAYSZ(flytypes);

    // Choose what kind of monster.
    monster_type result;
    const int how_many = 2 + random2(3) + power_level * 3;
    bool hostile_invis = false;

    do
    {
        result = flytypes[random2(num_flytypes - 2) + power_level];
    }
    while (is_good_god(you.religion) && result == MONS_VAMPIRE_MOSQUITO);

    for (int i = 0; i < how_many; ++i)
    {
        const bool hostile = one_chance_in(power_level + 4);

        create_monster(
            mgen_data(result,
                      hostile ? BEH_HOSTILE : BEH_FRIENDLY, you.pos(), MHITYOU,
                      MG_AUTOFOE).set_summoned(&you, 3, 0));

        if (hostile && mons_class_flag(result, M_INVIS) && !you.can_see_invisible())
            hostile_invis = true;
    }

    if (hostile_invis)
        mpr("You sense the presence of something unfriendly.");
}

static void _summon_rangers(int power)
{
    const int power_level = _get_power_level(power);
    const monster_type dctr  = random_choose(MONS_CENTAUR, MONS_YAKTAUR),
                       dctr2 = random_choose(MONS_CENTAUR_WARRIOR, MONS_FAUN),
                       dctr3 = random_choose(MONS_YAKTAUR_CAPTAIN,
                                             MONS_NAGA_SHARPSHOOTER),
                       dctr4 = random_choose(MONS_SATYR,
                                             MONS_MERFOLK_JAVELINEER,
                                             MONS_DEEP_ELF_MASTER_ARCHER);

    const monster_type base_choice = power_level == 2 ? dctr2 :
                                                        dctr;
    monster_type placed_choice  = power_level == 2 ? dctr3 :
                                  power_level == 1 ? dctr2 :
                                                     dctr;
    const bool extra_monster = coinflip();

    if (!extra_monster && power_level > 0)
        placed_choice = power_level == 2 ? dctr4 : dctr3;

    for (int i = 0; i < 1 + extra_monster; ++i)
        _friendly(base_choice, 5 - power_level);

    _friendly(placed_choice, 5 - power_level);
}

static void _cloud_card(int power)
{
    const int power_level = _get_power_level(power);
    bool something_happened = false;

    for (radius_iterator di(you.pos(), LOS_NO_TRANS); di; ++di)
    {
        monster *mons = monster_at(*di);
        cloud_type cloudy;

        switch (power_level)
        {
            case 0: cloudy = !one_chance_in(5) ? CLOUD_MEPHITIC : CLOUD_POISON;
                    break;

            case 1: cloudy = coinflip() ? CLOUD_COLD : CLOUD_FIRE;
                    break;

            case 2: cloudy = coinflip() ? CLOUD_ACID: CLOUD_MIASMA;
                    break;

            default: cloudy = CLOUD_DEBUGGING;
        }

        if (!mons || mons->wont_attack() || !mons_is_threatening(*mons))
            continue;

        for (adjacent_iterator ai(mons->pos()); ai; ++ai)
        {
            // don't place clouds on the player or monsters
            if (*ai == you.pos() || monster_at(*ai))
                continue;

            if (grd(*ai) == DNGN_FLOOR && !cloud_at(*ai))
            {
                const int cloud_power = 5 + random2((power_level + 1) * 3);
                place_cloud(cloudy, *ai, cloud_power, &you);

                if (you.see_cell(*ai))
                    something_happened = true;
            }
        }
    }

    if (something_happened)
        mpr("Clouds appear around you!");
    else
        canned_msg(MSG_NOTHING_HAPPENS);
}

static void _storm_card(int power)
{
    const int power_level = _get_power_level(power);

    wind_blast(&you, (power_level + 1) * 66, coord_def(), true);
    redraw_screen(); // Update monster positions

    // 1-3, 4-6, 7-9
    const int max_explosions = random_range((power_level * 3) + 1, (power_level + 1) * 3);
    // Select targets based on simultaneously running max_explosions resivoir
    // samples from the radius iterator over valid targets.
    //
    // Once the possible targets are drawn, the result is deduplicated into a
    // set of targets.
    vector<coord_def> target_draws (max_explosions, you.pos());
    int valid_targets = 0;
    for (radius_iterator ri(you.pos(), LOS_NO_TRANS, true); ri; ++ri)
    {
        if (grid_distance(*ri, you.pos()) > 3 && !cell_is_solid(*ri))
        {
            ++valid_targets;
            for (int i = 0; i < max_explosions; ++i)
            {
                if (one_chance_in(valid_targets))
                    target_draws[i] = *ri;
            }
        }
    }

    unordered_set<coord_def> targets (target_draws.begin(), target_draws.end());
    targets.erase(you.pos());

    bool heard = false;
    for (auto p : targets)
    {
        bolt beam;
        beam.is_tracer         = false;
        beam.is_explosion      = true;
        beam.glyph             = dchar_glyph(DCHAR_FIRED_BURST);
        beam.name              = "electrical discharge";
        beam.aux_source        = "the storm";
        beam.explode_noise_msg = "You hear a clap of thunder!";
        beam.real_flavour      = beam.flavour;
        beam.colour            = LIGHTCYAN;
        beam.source_id         = MID_PLAYER;
        beam.thrower           = KILL_YOU;
        beam.is_explosion      = true;
        beam.ex_size           = 3;
        beam.damage            = dice_def(3, 9 + 9 * power_level);
        beam.source = p;
        beam.target = p;
        beam.explode();
        heard = heard || player_can_hear(p);
    }
    // Lots of loud bangs, even if everything is silenced get a message.
    // Thunder comes after the animation runs.
    if (targets.size() > 0)
    {
        vector<string> thunder_adjectives = { "mighty",
                                              "violent",
                                              "cataclysmic" };
        mprf("You %s %s%s peal%s of thunder!",
              heard ? "hear" : "feel",
              targets.size() > 1 ? "" : "a ",
              thunder_adjectives[power_level].c_str(),
              targets.size() > 1 ? "s" : "");
    }
}

static void _illusion_card(int power)
{
    const int power_level = _get_power_level(power);
    monster* mon = get_free_monster();

    if (!mon || monster_at(you.pos()))
        return;

    mon->type = MONS_PLAYER;
    mon->behaviour = BEH_SEEK;
    mon->attitude = ATT_FRIENDLY;
    mon->set_position(you.pos());
    mon->mid = MID_PLAYER;
    mgrd(you.pos()) = mon->mindex();

    mons_summon_illusion_from(mon, (actor *)&you, SPELL_NO_SPELL, power_level);
    mon->reset();
}

static void _degeneration_card(int power)
{
    const int power_level = _get_power_level(power);

    if (!apply_visible_monsters([power_level](monster& mons)
           {
               if (mons.wont_attack() || !mons_is_threatening(mons))
                   return false;

               if (!x_chance_in_y((power_level + 1) * 5 + random2(5),
                                  mons.get_hit_dice()))
               {
                   return false;
               }

               if (mons.can_polymorph())
               {
                   mons.polymorph(PPT_LESS);
                   mons.malmutate("");
               }
               else
               {
                   const int daze_time = (5 + 5 * power_level) * BASELINE_DELAY;
                   mons.add_ench(mon_enchant(ENCH_DAZED, 0, &you, daze_time));
                   simple_monster_message(mons,
                                          " is dazed by the mutagenic energy.");
               }
               return true;
           }))
    {
        canned_msg(MSG_NOTHING_HAPPENS);
    }
}

static void _wild_magic_card(int power)
{
    const int power_level = _get_power_level(power);
    int num_affected = 0;

    for (radius_iterator di(you.pos(), LOS_NO_TRANS); di; ++di)
    {
        monster *mons = monster_at(*di);

        if (!mons || mons->wont_attack() || !mons_is_threatening(*mons))
            continue;

        if (x_chance_in_y((power_level + 1) * 5 + random2(5),
                           mons->get_hit_dice()))
        {
            spschool type = random_choose(spschool::conjuration,
                                          spschool::fire,
                                          spschool::ice,
                                          spschool::earth,
                                          spschool::air,
                                          spschool::poison);

            MiscastEffect(mons, actor_by_mid(MID_YOU_FAULTLESS),
                          {miscast_source::deck}, type,
                          random2(power/15) + 5, random2(power),
                          "a card of wild magic");

            num_affected++;
        }
    }

    if (num_affected > 0)
    {
        int mp = 0;

        for (int i = 0; i < num_affected; ++i)
            mp += random2(5);

        mpr("You feel a surge of magic.");
        if (mp && you.magic_points < you.max_magic_points)
        {
            inc_mp(mp);
            canned_msg(MSG_GAIN_MAGIC);
        }
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);
}

static void _torment_card()
{
    if (you.undead_or_demonic())
        holy_word_player(HOLY_WORD_CARD);
    else
        torment_player(&you, TORMENT_CARDS);
}

// Punishment cards don't have their power adjusted depending on Nemelex piety,
// and are based on experience level instead of invocations skill.
// Max power = 200 * (2700+2500) / 2700 + 243 + 300 = 928
// Min power = 1 * 2501 / 2700 + 1 + 0 = 2
static int _card_power(bool punishment)
{
    if (punishment)
        return you.experience_level * 18;

    int result = you.piety;
    result *= you.skill(SK_INVOCATIONS, 100) + 2500;
    result /= 2700;
    result += you.skill(SK_INVOCATIONS, 9);
    result += (you.piety * 3) / 2;

    return result;
}

void card_effect(card_type which_card,
                 bool dealt,
                 bool punishment, bool tell_card)
{
    const char *participle = dealt ? "dealt" : "drawn";
    const int power = _card_power(punishment);

    dprf("Card power: %d", power);

    if (tell_card)
    {
        // These card types will usually give this message in the targeting
        // prompt, and the cases where they don't are handled specially.
        if (which_card != CARD_VITRIOL
            && which_card != CARD_PAIN
            && which_card != CARD_ORB)
        {
            mprf("You have %s %s.", participle, card_name(which_card));
        }
    }

    switch (which_card)
    {
    case CARD_VELOCITY:         _velocity_card(power); break;
    case CARD_EXILE:            _exile_card(power); break;
    case CARD_ELIXIR:           _elixir_card(power); break;
    case CARD_STAIRS:           _stairs_card(power); break;
    case CARD_SHAFT:            _shaft_card(power); break;
    case CARD_TOMB:             entomb(10 + power/20 + random2(power/4)); break;
    case CARD_WRAITH:           drain_player(power / 4, false, true); break;
    case CARD_WRATH:            _godly_wrath(); break;
    case CARD_SUMMON_DEMON:     _summon_demon_card(power); break;
    case CARD_ELEMENTS:         _elements_card(power); break;
    case CARD_RANGERS:          _summon_rangers(power); break;
    case CARD_SUMMON_WEAPON:    _summon_dancing_weapon(power); break;
    case CARD_SUMMON_FLYING:    _summon_flying(power); break;
    case CARD_TORMENT:          _torment_card(); break;
    case CARD_CLOUD:            _cloud_card(power); break;
    case CARD_STORM:            _storm_card(power); break;
    case CARD_ILLUSION:         _illusion_card(power); break;
    case CARD_DEGEN:            _degeneration_card(power); break;
    case CARD_WILD_MAGIC:       _wild_magic_card(power); break;

    case CARD_VITRIOL:
    case CARD_PAIN:
    case CARD_ORB:
        _damaging_card(which_card, power, dealt);
        break;

    case CARD_FAMINE:
        if (you_foodless())
            mpr("You feel rather smug.");
        else
            set_hunger(min(you.hunger, HUNGER_STARVING / 2), true);
        break;

    case CARD_SWINE:
        if (transform(5 + power/10 + random2(power/10), transformation::pig, true))
            you.transform_uncancellable = true;
        else
            mpr("You feel a momentary urge to oink.");
        break;

    case NUM_CARDS:
        // The compiler will complain if any card remains unhandled.
        mprf("You have %s a buggy card!", participle);
        break;
    }
}

/**
 * Return the appropriate name for a known deck of the given type.
 *
 * @param sub_type  The type of deck in question.
 * @return          A name, e.g. "deck of destruction".
 *                  If the given type isn't a deck, return "deck of bugginess".
 */
string deck_name(deck_type deck)
{
    if (deck == DECK_STACK)
        return "stacked deck";
    const deck_type_data *deck_data = map_find(all_decks, deck);
    const string name = deck_data ? deck_data->name : "bugginess";
    return "deck of " + name;
}

#if TAG_MAJOR_VERSION == 34
bool is_deck_type(uint8_t sub_type)
{
    return (MISC_FIRST_DECK <= sub_type && sub_type <= MISC_LAST_DECK)
        || sub_type == MISC_DECK_OF_ODDITIES
        || sub_type == MISC_DECK_UNKNOWN;
}

bool is_deck(const item_def &item)
{
    return item.base_type == OBJ_MISCELLANY
           && is_deck_type(item.sub_type);
}

void reclaim_decks_on_level()
{
    for (auto &item : mitm)
        if (item.defined() && is_deck(item))
            destroy_item(item.index());
}

static void _reclaim_inventory_decks()
{
    for (auto &item : you.inv)
        if (item.defined() && is_deck(item))
            dec_inv_item_quantity(item.link, 1);
}

void reclaim_decks()
{
    add_daction(DACT_RECLAIM_DECKS);
    _reclaim_inventory_decks();
}
#endif
