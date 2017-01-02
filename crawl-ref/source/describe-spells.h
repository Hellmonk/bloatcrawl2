/**
 * @file
 * @brief Functions used to print information about spells, spellbooks, etc.
 **/

#ifndef DESCRIBE_SPELLS_H
#define DESCRIBE_SPELLS_H

#include <map>

#include "enum.h"
#include "format.h"
#include "menu.h"
#include "mon-info.h"

/// What's in a given spellbook?
struct spellbook_contents
{
    /// A label for the book.
    string label;
    /// The spells contained in the book (or 'book').
    vector<spell_type> spells;
};

typedef vector<spellbook_contents> spellset;

class spell_scroller : public formatted_scroller
{
public:
    spell_scroller(const spellset &_spells, const monster_info *_mon_owner,
                   const item_def *_source_item,
                   bool _show_quote_toggle = false) :
    formatted_scroller(), spells(_spells), mon_owner(_mon_owner),
    source_item(_source_item), show_quote_toggle(_show_quote_toggle)
    { };

    virtual ~spell_scroller();
protected:
    bool process_key(int keyin) override;
protected:
    spellset spells;
    const monster_info *mon_owner;
    const item_def *source_item;
    bool show_quote_toggle;
};

spellset item_spellset(const item_def &item);
spellset monster_spellset(const monster_info &mi);
vector<spell_type> map_chars_to_spells(const spellset &spells,
                                       const item_def* const source_item);
void describe_spellset(const spellset &spells,
                       const item_def* const source_item,
                       formatted_string &description,
                       const monster_info *mon_owner = nullptr);
string describe_item_spells(const item_def &item);
void list_spellset(const spellset &spells, const monster_info *mon_owner,
                   const item_def *source_item, formatted_string &initial_desc);

#endif
