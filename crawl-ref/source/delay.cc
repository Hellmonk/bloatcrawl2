/**
 * @file
 * @brief Functions for handling multi-turn actions.
**/

#include "AppHdr.h"

#include "delay.h"

#include <cstdio>
#include <cstring>

#include "ability.h"
#include "areas.h"
#include "artefact.h"
#include "bloodspatter.h"
#include "butcher.h"
#include "clua.h"
#include "command.h"
#include "coord.h"
#include "database.h"
#include "describe.h"
#include "directn.h"
#include "dungeon.h"
#include "english.h"
#include "env.h"
#include "fineff.h"
#include "food.h"
#include "fprop.h"
#include "god-companions.h"
#include "god-passive.h"
#include "god-wrath.h"
#include "hints.h"
#include "invent.h"
#include "item-prop.h"
#include "items.h"
#include "item-use.h"
#include "item-status-flag-type.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "mon-act.h"
#include "mon-behv.h"
#include "mon-gear.h"
#include "mon-tentacle.h"
#include "mon-util.h"
#include "mutation.h"
#include "nearby-danger.h"
#include "notes.h"
#include "options.h"
#include "ouch.h"
#include "output.h"
#include "player-equip.h"
#include "player.h"
#include "prompt.h"
#include "random.h"
#include "religion.h"
#include "shout.h"
#include "sound.h"
#include "spl-selfench.h"
#include "spl-util.h"
#include "stairs.h"
#include "state.h"
#include "stringutil.h"
#include "teleport.h"
#include "terrain.h"
#include "transform.h"
#include "traps.h"
#include "travel.h"
#include "xom.h"

int interrupt_block::interrupts_blocked = 0;

static void _xom_check_corpse_waste();
static const char *_activity_interrupt_name(activity_interrupt ai);

void push_delay(shared_ptr<Delay> delay)
{
    if (delay->is_run())
        clear_travel_trail();
    for (auto i = you.delay_queue.begin(); i != you.delay_queue.end(); ++i)
    {
        if ((*i)->is_parent())
        {
            you.delay_queue.insert(i, delay);
            you.redraw_evasion = true;
            return;
        }
    }
    you.delay_queue.push_back(delay);
    you.redraw_evasion = true;
}

static void _pop_delay()
{
    if (!you.delay_queue.empty())
        you.delay_queue.erase(you.delay_queue.begin());

    you.redraw_evasion = true;
}

static void _clear_pending_delays(size_t after_index = 1)
{
    while (you.delay_queue.size() > after_index)
    {
        auto delay = you.delay_queue.back();

        you.delay_queue.pop_back();

        if (delay->is_run() && you.running)
            // If you got here, you're already clearing the delays and there's
            // no need to clear them again.
            stop_running(false);
    }
}

static void _interrupt_butchering(const char* action)
{
    const bool multiple_corpses =
        // + 1 to avoid the first delay in the queue, which we know is
        // butchering.
        any_of(you.delay_queue.begin() + 1, you.delay_queue.end(),
               [] (const shared_ptr<Delay> d)
               {
                   return d->is_butcher();
               });
    mprf("You stop %s the corpse%s.", action, multiple_corpses ? "s" : "");
}

bool ButcherDelay::try_interrupt()
{
    _interrupt_butchering("butchering");
    return true;
}

bool MemoriseDelay::try_interrupt()
{
    // Losing work here is okay... having to start from
    // scratch is a reasonable behaviour. -- bwr
    mpr("Your memorisation is interrupted.");
    return true;
}

bool MultidropDelay::try_interrupt()
{
    // No work lost
    if (!items.empty())
        mpr("You stop dropping stuff.");
    return true;
}

bool BaseRunDelay::try_interrupt()
{
    // Keep things consistent, otherwise disturbing phenomena can occur.
    if (you.running)
        stop_running(false);
    update_turn_count();

    // Always interruptible.
    return true;
}

bool MacroDelay::try_interrupt()
{
    // Always interruptible.
    return true;
    // There's no special action needed for macros - if we don't call out
    // to the Lua function, it can't do damage.
}

bool ArmourOnDelay::try_interrupt()
{
    if (duration > 1 && !was_prompted)
    {
        if (!crawl_state.disables[DIS_CONFIRMATIONS]
            && !yesno("Keep equipping yourself?", false, 0, false))
        {
            mpr("You stop putting on your armour.");
            return true;
        }
        else
            was_prompted = true;
    }
    return false;
}

bool ArmourOffDelay::try_interrupt()
{
    if (duration > 1 && !was_prompted)
    {
        if (!crawl_state.disables[DIS_CONFIRMATIONS]
            && !yesno("Keep disrobing?", false, 0, false))
        {
            mpr("You stop removing your armour.");
            return true;
        }
        else
            was_prompted = true;
    }
    return false;
}

bool BlurryScrollDelay::try_interrupt()
{
    if (duration > 1 && !was_prompted)
    {
        if (!crawl_state.disables[DIS_CONFIRMATIONS]
            && !yesno("Keep reading the scroll?", false, 0, false))
        {
            mpr("You stop reading the scroll.");
            return true;
        }
        else
            was_prompted = true;
    }
    return false;
}

bool AscendingStairsDelay::try_interrupt()
{
    mpr("You stop ascending the stairs.");
    return true;  // short... and probably what people want
}

bool DescendingStairsDelay::try_interrupt()
{
    mpr("You stop descending the stairs.");
    return true;  // short... and probably what people want
}

bool PasswallDelay::try_interrupt()
{
    mpr("Your meditation is interrupted.");
    return true;
}

bool ShaftSelfDelay::try_interrupt()
{
    mpr("You stop digging.");
    return true;
}

bool ExsanguinateDelay::try_interrupt()
{
    if (duration > 1 && !was_prompted)
    {
        if (!crawl_state.disables[DIS_CONFIRMATIONS]
            && !yesno("Keep bloodletting?", false, 0, false))
        {
            mpr("You stop emptying yourself of blood.");
            return true;
        }
        else
            was_prompted = true;
    }
    return false;
}

bool RevivifyDelay::try_interrupt()
{
    if (duration > 1 && !was_prompted)
    {
        if (!crawl_state.disables[DIS_CONFIRMATIONS]
            && !yesno("Continue your ritual?", false, 0, false))
        {
            mpr("You stop revivifying.");
            return true;
        }
        else
            was_prompted = true;
    }
    return false;
}

void stop_delay(bool stop_stair_travel)
{
    if (you.delay_queue.empty())
        return;

    set_more_autoclear(false);

    ASSERT(!crawl_state.game_is_arena());

    auto delay = current_delay();

    // At the very least we can remove any queued delays, right
    // now there is no problem with doing this... note that
    // any queuing here can only happen from a single command,
    // as the effect of a delay doesn't normally allow interaction
    // until it is done... it merely chains up individual actions
    // into a single action.  -- bwr
    // Butcher delays do this on their own, in order to determine the old
    // list of delays before clearing it.
    if (!delay->is_butcher())
        _clear_pending_delays();

    if ((!delay->is_stair_travel() || stop_stair_travel)
        && delay->try_interrupt())
    {
        _pop_delay();
    }
}

bool you_are_delayed()
{
    return !you.delay_queue.empty();
}

shared_ptr<Delay> current_delay()
{
    return you_are_delayed() ? you.delay_queue.front()
                             : nullptr;
}

bool is_being_drained(const item_def &item)
{
    if (!you_are_delayed())
        return false;

    return current_delay()->is_being_used(&item, OPER_EAT);
}

bool is_being_butchered(const item_def &item, bool just_first)
{
    for (const auto delay : you.delay_queue)
    {
        if (delay->is_being_used(&item, OPER_BUTCHER))
            return true;

        if (just_first)
            break;
    }

    return false;
}

bool is_vampire_feeding()
{
    if (!you_are_delayed())
        return false;

    return current_delay()->is_being_used(nullptr, OPER_EAT);
}

bool player_stair_delay()
{
    auto delay = current_delay();
    return delay && delay->is_stairs();
}

/**
 * Is the player currently in the middle of memorizing a spell?
 *
 * @param spell     A specific spell, or -1 to check if we're memorizing any
 *                  spell at all.
 * @return          Whether the player is currently memorizing the given type
 *                  of spell.
 */
bool already_learning_spell(int spell)
{
    for (const auto delay : you.delay_queue)
    {
        auto mem = dynamic_cast<MemoriseDelay*>(delay.get());
        if (!mem)
            continue;

        if (spell == -1 || mem->spell == spell)
            return true;
    }
    return false;
}

static command_type _get_running_command()
{
    if (Options.travel_key_stop && kbhit()
        || !in_bounds(you.pos() + you.running.pos))
    {
        stop_running();
        return CMD_NO_CMD;
    }

    if (is_resting())
    {
        you.running.rest();

#ifdef USE_TILE
        if (Options.rest_delay >= 0 && tiles.need_redraw())
            tiles.redraw();
#endif

        if (!is_resting() && you.running.hp == you.hp
            && you.running.mp == you.magic_points)
        {
            mpr("Done waiting.");
        }

        if (Options.rest_delay > 0)
            delay(Options.rest_delay);

        return CMD_WAIT;
    }
    else if (you.running.is_explore() && Options.explore_delay > -1)
        delay(Options.explore_delay);
    else if (Options.travel_delay > 0)
        delay(Options.travel_delay);

    return direction_to_command(you.running.pos.x, you.running.pos.y);
}

/**
 * Can the player currently read the given scroll?
 *
 * Prints corresponding messages if the answer is false.
 *
 * @param inv_slot      The scroll in question.
 * @return              false if the player is confused, berserk, silenced,
 *                      etc; true otherwise.
 */
static bool _can_read_scroll(const item_def& scroll)
{
    // prints its own messages
    if (!player_can_read())
        return false;

    const string illiteracy_reason = cannot_read_item_reason(scroll);
    if (illiteracy_reason.empty())
        return true;

    mpr(illiteracy_reason);
    return false;
}

// Xom is amused by a potential food source going to waste, and is
// more amused the hungrier you are.
static void _xom_check_corpse_waste()
{
    const int food_need = max(HUNGER_SATIATED - you.hunger, 0);
    xom_is_stimulated(50 + (151 * food_need / 6000));
}

static bool _auto_eat()
{
    return Options.auto_eat_chunks
           && Options.autopickup_on > 0
           && (player_likes_chunks(true)
               || !you.gourmand()
               || you.duration[DUR_GOURMAND] >= GOURMAND_MAX / 4
               || you.hunger_state < HS_SATIATED);
}

void clear_macro_process_key_delay()
{
    if (dynamic_cast<MacroProcessKeyDelay*>(current_delay().get()))
        _pop_delay();
}

void ArmourOnDelay::start()
{
    mprf(MSGCH_MULTITURN_ACTION, "You start putting on your armour.");
}

void ArmourOffDelay::start()
{
    mprf(MSGCH_MULTITURN_ACTION, "You start removing your armour.");
}

void MemoriseDelay::start()
{
    if (vehumet_is_offering(spell))
    {
        string message = make_stringf(" grants you knowledge of %s.",
            spell_title(spell));
        simple_god_message(message.c_str());
    }
    mprf(MSGCH_MULTITURN_ACTION, "You start memorising the spell.");
}

void PasswallDelay::start()
{
    mprf(MSGCH_MULTITURN_ACTION, "You begin to meditate on the wall.");
}

void ShaftSelfDelay::start()
{
    mprf(MSGCH_MULTITURN_ACTION, "You begin to dig a shaft.");
}

void BlurryScrollDelay::start()
{
    mprf(MSGCH_MULTITURN_ACTION, "You begin reading the scroll.");
}

void ExsanguinateDelay::start()
{
    mprf(MSGCH_MULTITURN_ACTION, "You begin bloodletting.");
}

void RevivifyDelay::start()
{
    mprf(MSGCH_MULTITURN_ACTION, "You begin the revivification ritual.");
}

command_type RunDelay::move_cmd() const
{
    return _get_running_command();
}

command_type RestDelay::move_cmd() const
{
    return _get_running_command();
}

command_type TravelDelay::move_cmd() const
{
    return travel();
}

void BaseRunDelay::handle()
{
    if (!started)
        started = true;
    // Handle inconsistencies between the delay queue and you.running.
    // We don't want to send the game into a deadlock.
    if (!you.running)
    {
        update_turn_count();
        _pop_delay();
        return;
    }

    if (you.turn_is_over)
        return;

    command_type cmd = CMD_NO_CMD;

    if ((want_move() && you.confused()) || !i_feel_safe(true, want_move()))
        stop_running();
    else
    {
        if (want_autoeat() && _auto_eat())
        {
            const interrupt_block block_interrupts;
            if (prompt_eat_chunks(true) == 1)
                return;
        }

        cmd = move_cmd();
    }

    if (cmd != CMD_NO_CMD)
    {
        if (want_clear_messages())
            clear_messages();
        process_command(cmd);
    }

    if (!you.turn_is_over)
        you.time_taken = 0;

    // If you.running has gone to zero, and the run delay was not
    // removed, remove it now. This is needed to clean up after
    // find_travel_pos() function in travel.cc.
    if (!you.running
        && !you.delay_queue.empty()
        && you.delay_queue.front()->is_run())
    {
        update_turn_count();
        _pop_delay();
    }
}

void MacroDelay::handle()
{
    if (!started)
        started = true;
    if (!duration)
    {
        dprf("Expiring macro delay on turn: %d", you.num_turns);
        stop_delay();
    }
    else
        run_macro();

    // Macros may not use up turns, but unless we zero time_taken,
    // main.cc will call world_reacts and increase turn count.
    if (!you.turn_is_over && you.time_taken)
        you.time_taken = 0;
}

static bool _check_corpse_gone(item_def& item, const char* action)
{
    // A monster may have raised the corpse you're chopping up! -- bwr
    // Note that a monster could have raised the corpse and another
    // monster could die and create a corpse with the same ID number...
    // However, it would not be at the player's square like the
    // original and that's why we do it this way.
    if (!item.defined()
        || item.base_type != OBJ_CORPSES
        || item.pos != you.pos())
    {
        // There being no item at all could have happened for several
        // reasons, so don't bother to give a message.
        return true;
    }
    else if (item.is_type(OBJ_CORPSES, CORPSE_SKELETON))
    {
        mprf("The corpse has rotted away into a skeleton before "
             "you could %s!", action);
        _xom_check_corpse_waste();
        return true;
    }

    return false;
}

bool ButcherDelay::invalidated()
{
    return _check_corpse_gone(corpse, "butcher it");
}

bool MultidropDelay::invalidated()
{
    // Throw away invalid items. XXX: what are they?
    while (!items.empty()
           // Don't look for gold in inventory
           && items[0].slot != PROMPT_GOT_SPECIAL
           && !you.inv[items[0].slot].defined())
    {
        items.erase(items.begin());
    }

    if (items.empty())
    {
        // Ran out of things to drop.
        you.turn_is_over = false;
        you.time_taken = 0;
        return true;
    }
    return false;
}

bool BlurryScrollDelay::invalidated()
{
    if (!_can_read_scroll(scroll))
    {
        you.time_taken = 0;
        return true;
    }
    return false;
}

void MultidropDelay::tick()
{
    if (!drop_item(items[0].slot, items[0].quantity))
    {
        you.turn_is_over = false;
        you.time_taken = 0;
    }
    items.erase(items.begin());
}

void JewelleryOnDelay::tick()
{
    // This is a 1-turn delay where the time cost is handled
    // in finish().
    // FIXME: get rid of this hack!
    you.time_taken = 0;
}

void DropItemDelay::tick()
{
    // This is a 1-turn delay where the time cost is handled
    // in finish().
    // FIXME: get rid of this hack!
    you.time_taken = 0;
}

void Delay::handle()
{
    if (!started)
    {
        started = true;
        start();
    }

    // First check cases where delay may no longer be valid:
    // XXX: need to handle PasswallDelay when monster digs -- bwr
    if (invalidated())
    {
        _pop_delay();
        return;
    }

    // Actually handle delay:
    if (duration > 0)
    {
        dprf("Delay type: %s, duration: %d", name(), duration);
        --duration;
        tick();
    }
    else
    {
        finish();
        you.wield_change = true;
        _pop_delay();
        print_stats();  // force redraw of the stats
#ifdef USE_TILE
        tiles.update_tabs();
#endif
        if (!you.turn_is_over)
            you.time_taken = 0;
    }
}

void handle_delay()
{
    if (!you_are_delayed())
        return;

    shared_ptr<Delay> delay = current_delay();
    delay->handle();
}

void JewelleryOnDelay::finish()
{
    // Recheck -Tele here, since our condition may have changed since starting
    // the amulet swap process.
    // Just breaking here is okay because swapping jewellery is a one-turn
    // action, so conceptually there is nothing to interrupt - in other words,
    // this is equivalent to if the user took off the previous amulet and was
    // affected by tele other before putting the -Tele amulet on as a separate
    // action on the next turn.
    // XXX: duplicates a check in invent.cc:check_warning_inscriptions()
    if (!crawl_state.disables[DIS_CONFIRMATIONS]
        && needs_notele_warning(jewellery, OPER_PUTON)
        && item_ident(jewellery, ISFLAG_KNOW_TYPE))
    {
        string prompt = "Really put on ";
        prompt += jewellery.name(DESC_INVENTORY);
        prompt += " while about to teleport?";
        if (!yesno(prompt.c_str(), false, 'n'))
            return;
    }

#ifdef USE_SOUND
    parse_sound(WEAR_JEWELLERY_SOUND);
#endif
    puton_ring(jewellery, false, false);
}

void ArmourOnDelay::finish()
{
    const unsigned int old_talents = your_talents(false).size();

    set_ident_flags(armour, ISFLAG_IDENT_MASK);
    if (is_artefact(armour))
        armour.flags |= ISFLAG_NOTED_ID;

    const equipment_type eq_slot = get_armour_slot(armour);

#ifdef USE_SOUND
    parse_sound(EQUIP_ARMOUR_SOUND);
#endif
    mprf("You finish putting on %s.", armour.name(DESC_YOUR).c_str());

    if (eq_slot == EQ_BODY_ARMOUR)
    {
        if (you.duration[DUR_ICY_ARMOUR] != 0
            && !is_effectively_light_armour(&armour))
        {
            remove_ice_armour();
        }
    }

    equip_item(eq_slot, armour.link);

    check_item_hint(armour, old_talents);
}

bool ArmourOffDelay::invalidated()
{
    return !armour.defined();
}

void ArmourOffDelay::finish()
{
    const equipment_type slot = get_armour_slot(armour);
    ASSERT(you.equip[slot] == armour.link);

#ifdef USE_SOUND
    parse_sound(DEQUIP_ARMOUR_SOUND);
#endif
    mprf("You finish taking off %s.", armour.name(DESC_YOUR).c_str());
    unequip_item(slot);
}

void MemoriseDelay::finish()
{
#ifdef USE_SOUND
    parse_sound(MEMORISE_SPELL_SOUND);
#endif
    mpr("You finish memorising.");
    add_spell_to_memory(spell);
    vehumet_accept_gift(spell);
}

void PasswallDelay::finish()
{
    mpr("You finish merging with the rock.");
    // included in default force_more_message

    if (dest.x == 0 || dest.y == 0)
        return;

    switch (grd(dest))
    {
    default:
        if (!you.is_habitable(dest))
        {
            mpr("...yet there is something new on the other side. "
                "You quickly turn back.");
            redraw_screen();
            return;
        }
        break;

    case DNGN_CLOSED_DOOR:      // open the door
    case DNGN_CLOSED_CLEAR_DOOR:
    case DNGN_RUNED_DOOR:
    case DNGN_RUNED_CLEAR_DOOR:
        // Once opened, former runed doors become normal doors.
        dgn_open_door(dest);
        break;
    }

    // Move any monsters out of the way.
    if (monster* m = monster_at(dest))
    {
        // One square, a few squares, anywhere...
        if (!m->shift() && !monster_blink(m, true))
            monster_teleport(m, true, true);
        // Might still fail.
        if (monster_at(dest))
        {
            mpr("...and sense your way blocked. You quickly turn back.");
            redraw_screen();
            return;
        }

        move_player_to_grid(dest, false);

        // Wake the monster if it's asleep.
        if (m)
            behaviour_event(m, ME_ALERT, &you);
    }
    else
        move_player_to_grid(dest, false);

    // the last phase of the delay is a fake (0-time) turn, so world_reacts
    // and player_reacts aren't triggered. Need to do a tiny bit of cleanup.
    // This isn't very elegant, and perhaps a version of player_reacts that is
    // triggered by changing location would be better (per Pleasingfungus),
    // but player_reacts is very sensitive to order and can't be easily
    // refactored in this way.
    you.update_beholders();
    you.update_fearmongers();
}

void ShaftSelfDelay::finish()
{
    you.do_shaft_ability();
}

void BlurryScrollDelay::finish()
{
    // Make sure the scroll still exists, the player isn't confused, etc
    if (_can_read_scroll(scroll))
    {
        read_scroll(scroll);
        // we are now probably out of sync with regular world_reacts timing, so
        // trigger any fineffs that might have been caused by reading this
        // scroll, e.g. torment vs. TRJ. Otherwise they'd have to wait until
        // the next world_reacts.
        // TODO: is there a more general condition that this can be triggered
        // under? it might impact other obscure cases, e.g. passwalling with
        // spiny.
        fire_final_effects();
    }
}

static void _finish_butcher_delay(item_def& corpse)
{
    // We know the item is valid and a real corpse, because invalidated()
    // checked for that.
    finish_butchering(corpse);
    // Don't waste time picking up chunks if you're already
    // starving. (jpeg)
    if (you.hunger_state > HS_STARVING
        // Only pick up chunks if this is the last delay...
        && (you.delay_queue.size() == 1
        // ...Or, equivalently, if it's the last butcher one.
            || !you.delay_queue[1]->is_butcher()))
    {
        request_autopickup();
    }
    you.turn_is_over = true;
}

void ButcherDelay::finish()
{
    _finish_butcher_delay(corpse);
}

void DropItemDelay::finish()
{
    // We're here if dropping the item required some action to be done
    // first, like removing armour. At this point, it should be droppable
    // immediately.

    // Make sure item still exists.
    if (!item.defined())
        return;

    if (!drop_item(item.link, 1))
    {
        you.turn_is_over = false;
        you.time_taken = 0;
    }
}

void AscendingStairsDelay::finish()
{
    up_stairs();
}

void DescendingStairsDelay::finish()
{
    down_stairs();
}

void ExsanguinateDelay::finish()
{
    blood_spray(you.pos(), MONS_PLAYER, 10);
    you.vampire_alive = false;
    you.redraw_status_lights = true;
    calc_hp(true);
    mpr("Now bloodless.");
    vampire_update_transformations();
}

void RevivifyDelay::finish()
{
    you.vampire_alive = true;
    you.redraw_status_lights = true;
    mpr("Now alive.");
    temp_mutate(MUT_FRAIL, "vampire revification");
    vampire_update_transformations();
}

void run_macro(const char *macroname)
{
#ifdef CLUA_BINDINGS
    if (!clua)
    {
        mprf(MSGCH_DIAGNOSTICS, "Lua not initialised");
        stop_delay();
        return;
    }

    shared_ptr<Delay> delay;
    if (!macroname)
    {
        delay = current_delay();
        ASSERT(delay->is_macro());
    }
    else
        delay = start_delay<MacroDelay>();

    // If callbooleanfn returns false, that means the macro either exited
    // normally by returning or by calling coroutine.yield(false). Either way,
    // decrement the macro duration.
    if (!clua.callbooleanfn(false, "c_macro", "s", macroname))
    {
        if (!clua.error.empty())
        {
            mprf(MSGCH_ERROR, "Lua error: %s", clua.error.c_str());
            stop_delay();
        }
        else if (delay->duration > 0)
            --delay->duration;
    }
#else
    UNUSED(macroname);
    stop_delay();
#endif
}

// Returns TRUE if the delay should be interrupted, MAYBE if the user function
// had no opinion on the matter, FALSE if the delay should not be interrupted.
static maybe_bool _userdef_interrupt_activity(Delay* delay,
                                              activity_interrupt ai,
                                              const activity_interrupt_data &at)
{
#ifdef CLUA_BINDINGS
    lua_State *ls = clua.state();
    if (!ls || ai == activity_interrupt::force)
        return MB_TRUE;

    const char *interrupt_name = _activity_interrupt_name(ai);

    bool ran = clua.callfn("c_interrupt_activity", "1:ssA",
                           delay->name(), interrupt_name, &at);
    if (ran)
    {
        // If the function returned nil, we want to cease processing.
        if (lua_isnil(ls, -1))
        {
            lua_pop(ls, 1);
            return MB_FALSE;
        }

        bool stopact = lua_toboolean(ls, -1);
        lua_pop(ls, 1);
        if (stopact)
            return MB_TRUE;
    }

    if (delay->is_macro() && clua.callbooleanfn(true, "c_interrupt_macro",
                                                "sA", interrupt_name, &at))
    {
        return MB_TRUE;
    }
#else
    UNUSED(_activity_interrupt_name);
#endif
    return MB_MAYBE;
}

// Returns true if the activity should be interrupted, false otherwise.
static bool _should_stop_activity(Delay* delay,
                                  activity_interrupt ai,
                                  const activity_interrupt_data &at)
{
    switch (_userdef_interrupt_activity(delay, ai, at))
    {
    case MB_TRUE:
        return true;
    case MB_FALSE:
        return false;
    case MB_MAYBE:
        break;
    }

    // Don't interrupt player on monster's turn, they might wander off.
    if (you.turn_is_over
        && (at.context == SC_ALREADY_SEEN || at.context == SC_UNCHARM))
    {
        return false;
    }

    // No monster will attack you inside a sanctuary,
    // so presence of monsters won't matter.
    if (ai == activity_interrupt::see_monster && is_sanctuary(you.pos()))
        return false;

    auto curr = current_delay(); // Not necessarily what we were passed.

    if ((ai == activity_interrupt::see_monster
         || ai == activity_interrupt::mimic)
        && player_stair_delay())
    {
        return false;
    }

    if (ai == activity_interrupt::full_hp || ai == activity_interrupt::full_mp
        || ai == activity_interrupt::ancestor_hp)
    {
        if ((Options.rest_wait_both && curr->is_resting()
             && !you.is_sufficiently_rested())
            || (Options.rest_wait_ancestor && curr->is_resting()
                && !ancestor_full_hp()))
        {
            return false;
        }
    }

    // Don't interrupt feeding or butchering for monsters already in view.
    if (curr->is_butcher() && ai == activity_interrupt::see_monster
        && testbits(at.mons_data->flags, MF_WAS_IN_VIEW))
    {
        return false;
    }

    return ai == activity_interrupt::force
           || Options.activity_interrupts[delay->name()][static_cast<int>(ai)];
}

static string _abyss_monster_creation_message(const monster* mon)
{
    if (mon->type == MONS_DEATH_COB)
    {
        return coinflip() ? " appears in a burst of microwaves!"
                          : " pops from nullspace!";
    }

    // You may ask: "Why these weights?" So would I!
    const vector<pair<string, int>> messages = {
        { " appears in a shower of translocational energy.", 17 },
        { " appears in a shower of sparks.", 34 },
        { " materialises.", 45 },
        { " emerges from chaos.", 13 },
        { " emerges from the beyond.", 26 },
        { make_stringf(" assembles %s!",
                       mon->pronoun(PRONOUN_REFLEXIVE).c_str()), 33 },
        { " erupts from nowhere.", 9 },
        { " bursts from nowhere.", 18 },
        { " is cast out of space.", 7 },
        { " is cast out of reality.", 14 },
        { " coalesces out of pure chaos.", 5 },
        { " coalesces out of seething chaos.", 10 },
        { " punctures the fabric of time!", 2 },
        { " punctures the fabric of the universe.", 7 },
        { make_stringf(" manifests%s!",
                       silenced(you.pos()) ? "" : " with a bang"), 3 },


    };

    return *random_choose_weighted(messages);
}

static inline bool _monster_warning(activity_interrupt ai,
                                    const activity_interrupt_data &at,
                                    shared_ptr<Delay> delay,
                                    vector<string>* msgs_buf = nullptr)
{
    if (ai == activity_interrupt::sense_monster)
    {
        mprf(MSGCH_WARN, "You sense a monster nearby.");
        return true;
    }
    if (ai != activity_interrupt::see_monster)
        return false;
    if (delay && !delay->is_run() && !delay->is_butcher())
        return false;
    if (at.context != SC_NEWLY_SEEN && !delay)
        return false;

    ASSERT(at.apt == ai_payload::monster);
    monster* mon = at.mons_data;
    ASSERT(mon);
    if (!you.can_see(*mon))
        return false;

    // Disable message for summons.
    if (mon->is_summoned() && !delay)
        return false;

    if (at.context == SC_ALREADY_SEEN || at.context == SC_UNCHARM)
    {
        // Only say "comes into view" if the monster wasn't in view
        // during the previous turn.
        if (testbits(mon->flags, MF_WAS_IN_VIEW) && delay)
        {
            mprf(MSGCH_WARN, "%s is too close now for your liking.",
                 mon->name(DESC_THE).c_str());
        }
    }
    else if (mon->seen_context == SC_JUST_SEEN)
        return false;
    else
    {
        // XXX: This needs to be here to ensure correct messaging for
        // autoexplore, even though the correct place to process it is
        // seen_monster
        view_monster_equipment(mon);

        string text = getMiscString(mon->name(DESC_DBNAME) + " title");
        if (text.empty())
            text = mon->full_name(DESC_A);
        if (mon->type == MONS_PLAYER_GHOST)
        {
            text += make_stringf(" (%s)",
                                 short_ghost_description(mon).c_str());
        }

        if (at.context == SC_DOOR)
            text += " opens the door.";
        else if (at.context == SC_GATE)
            text += " opens the gate.";
        else if (at.context == SC_TELEPORT_IN)
            text += " appears from thin air!";
        else if (at.context == SC_LEAP_IN)
            text += " leaps into view!";
        else if (at.context == SC_FISH_SURFACES)
        {
            text += " bursts forth from the ";
            if (mons_primary_habitat(*mon) == HT_LAVA)
                text += "lava";
            else if (mons_primary_habitat(*mon) == HT_WATER)
                text += "water";
            else
                text += "realm of bugdom";
            text += ".";
        }
        else if (at.context == SC_NONSWIMMER_SURFACES_FROM_DEEP)
            text += " emerges from the water.";
        else if (at.context == SC_UPSTAIRS)
            text += " comes up the stairs.";
        else if (at.context == SC_DOWNSTAIRS)
            text += " comes down the stairs.";
        else if (at.context == SC_ARCH)
            text += " comes through the gate.";
        else if (at.context == SC_ABYSS)
            text += _abyss_monster_creation_message(mon);
        else if (at.context == SC_THROWN_IN)
            text += " is thrown into view!";
        else
            text += " comes into view.";

        bool zin_id = false;
        string god_warning;

        if (have_passive(passive_t::warn_shapeshifter)
            && mon->is_shapeshifter()
            && !(mon->flags & MF_KNOWN_SHIFTER))
        {
            zin_id = true;
            mon->props["zin_id"] = true;
            discover_shifter(*mon);
            god_warning = uppercase_first(god_name(you.religion))
                          + " warns you: "
                          + uppercase_first(mon->pronoun(PRONOUN_SUBJECTIVE))
                          + " "
                          + conjugate_verb("are", mon->pronoun_plurality())
                          + " a foul ";
            if (mon->has_ench(ENCH_GLOWING_SHAPESHIFTER))
                god_warning += "glowing ";
            god_warning += "shapeshifter.";
        }

        monster_info mi(mon);

        const string mweap = get_monster_equipment_desc(mi, DESC_IDENTIFIED,
                                                        DESC_NONE);

        if (!mweap.empty())
        {
            text += " " + uppercase_first(mon->pronoun(PRONOUN_SUBJECTIVE))
                + " " + conjugate_verb("are", mi.pronoun_plurality())
                + (mweap[0] != ' ' ? " " : "")
                + mweap + ".";
        }

        if (msgs_buf)
            msgs_buf->push_back(text);
        else
        {
            mprf(MSGCH_MONSTER_WARNING, "%s", text.c_str());
            if (zin_id)
                mprf(MSGCH_GOD, "%s", god_warning.c_str());
#ifndef USE_TILE_LOCAL
            if (zin_id)
                update_monster_pane();
#endif
            if (player_under_penance(GOD_GOZAG)
                && !mon->wont_attack()
                && !mon->is_stationary()
                && !mons_is_object(mon->type)
                && !mons_is_tentacle_or_tentacle_segment(mon->type))
            {
                if (coinflip()
                    && mon->get_experience_level() >=
                       random2(you.experience_level))
                {
                    mprf(MSGCH_GOD, GOD_GOZAG, "Gozag incites %s against you.",
                         mon->name(DESC_THE).c_str());
                    gozag_incite(mon);
                }
            }
        }
        if (you.has_mutation(MUT_SCREAM)
            && x_chance_in_y(3 + you.get_mutation_level(MUT_SCREAM) * 3, 100))
        {
            yell(mon);
        }
        mons_set_just_seen(mon);
    }

    if (crawl_state.game_is_hints())
        hints_monster_seen(*mon);

    return true;
}

// Turns autopickup off if we ran into an invisible monster or saw a monster
// turn invisible.
// Turns autopickup on if we saw an invisible monster become visible or
// killed an invisible monster.
void autotoggle_autopickup(bool off)
{
    if (off)
    {
        if (Options.autopickup_on > 0)
        {
            Options.autopickup_on = -1;
            mprf(MSGCH_WARN,
                 "Deactivating autopickup; reactivate with <w>%s</w>.",
                 command_to_string(CMD_TOGGLE_AUTOPICKUP).c_str());
        }
        if (crawl_state.game_is_hints())
        {
            learned_something_new(HINT_INVISIBLE_DANGER);
            Hints.hints_seen_invisible = you.num_turns;
        }
    }
    else if (Options.autopickup_on < 0) // was turned off automatically
    {
        Options.autopickup_on = 1;
        mprf(MSGCH_WARN, "Reactivating autopickup.");
    }
}

// Returns true if any activity was stopped. Not reentrant.
bool interrupt_activity(activity_interrupt ai,
                        const activity_interrupt_data &at,
                        vector<string>* msgs_buf)
{
    if (interrupt_block::blocked())
        return false;

    const interrupt_block block_recursive_interrupts;
    if (ai == activity_interrupt::hit_monster
        || ai == activity_interrupt::monster_attacks)
    {
        const monster* mon = at.mons_data;
        if (mon && !mon->visible_to(&you) && !mon->submerged())
            autotoggle_autopickup(true);
    }

    if (crawl_state.is_repeating_cmd())
        return interrupt_cmd_repeat(ai, at);

    if (!you_are_delayed())
    {
        // Printing "[foo] comes into view." messages even when not
        // auto-exploring/travelling.
        if (ai == activity_interrupt::see_monster)
            return _monster_warning(ai, at, nullptr, msgs_buf);
        else
            return false;
    }

    const auto delay = current_delay();

    // If we get hungry while traveling, let's try to auto-eat a chunk.
    if (ai == activity_interrupt::hungry && delay->want_autoeat() && _auto_eat()
        && prompt_eat_chunks(true) == 1)
    {
        return false;
    }

    dprf("Activity interrupt: %s", _activity_interrupt_name(ai));

    // First try to stop the current delay.
    if (ai == activity_interrupt::full_hp && !you.running.notified_hp_full)
    {
        you.running.notified_hp_full = true;
        mpr("HP restored.");
    }
    else if (ai == activity_interrupt::full_mp && !you.running.notified_mp_full)
    {
        you.running.notified_mp_full = true;
        mpr("Magic restored.");
    }
    else if (ai == activity_interrupt::ancestor_hp
             && !you.running.notified_ancestor_hp_full)
    {
        // This interrupt only triggers when the ancestor is in LOS,
        // so this message does not leak information.
        you.running.notified_ancestor_hp_full = true;
        mpr("Ancestor HP restored.");
    }

    if (_should_stop_activity(delay.get(), ai, at))
    {
        _monster_warning(ai, at, delay, msgs_buf);
        // Teleport stops stair delays.
        stop_delay(ai == activity_interrupt::teleport);

        return true;
    }

    // Check the other queued delays; the first delay that is interruptible
    // will kill itself and all subsequent delays. This is so that a travel
    // delay stacked behind a delay such as stair/autopickup will be killed
    // correctly by interrupts that the simple stair/autopickup delay ignores.
    for (int i = 1, size = you.delay_queue.size(); i < size; ++i)
    {
        if (_should_stop_activity(you.delay_queue[i].get(), ai, at))
        {
            // Do we have a queued run delay? If we do, flush the delay queue
            // so that stop running Lua notifications happen.
            for (int j = i; j < size; ++j)
            {
                if (you.delay_queue[j]->is_run())
                {
                    _monster_warning(ai, at, you.delay_queue[j], msgs_buf);
                    _clear_pending_delays(i);
                    return true;
                }
            }

            // Non-run queued delays can be discarded without any processing.
            you.delay_queue.erase(you.delay_queue.begin() + i,
                                  you.delay_queue.end());
            return true;
        }
    }

    return false;
}

// Must match the order of activity_interrupt.h!
static const char *activity_interrupt_names[] =
{
    "force", "keypress", "full_hp", "full_mp", "ancestor_hp", "hungry", "message",
    "hp_loss", "stat", "monster", "monster_attack", "teleport", "hit_monster",
    "sense_monster", "mimic"
};

static const char *_activity_interrupt_name(activity_interrupt ai)
{
    COMPILE_CHECK(ARRAYSZ(activity_interrupt_names) == NUM_ACTIVITY_INTERRUPTS);

    if (ai == activity_interrupt::COUNT)
        return "";

    return activity_interrupt_names[static_cast<int>(ai)];
}

activity_interrupt get_activity_interrupt(const string &name)
{
    COMPILE_CHECK(ARRAYSZ(activity_interrupt_names) == NUM_ACTIVITY_INTERRUPTS);

    for (int i = 0; i < NUM_ACTIVITY_INTERRUPTS; ++i)
        if (name == activity_interrupt_names[i])
            return activity_interrupt(i);

    return activity_interrupt::COUNT;
}
