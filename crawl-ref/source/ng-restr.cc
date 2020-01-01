/**
 * @file
 * @brief Character choice restrictions.
 *
 * The functions in this file are "pure": They don't
 * access any global data.
**/

#include "AppHdr.h"

#include "ng-restr.h"

#include "jobs.h"
#include "newgame.h"
#include "newgame-def.h"
#include "size-type.h"
#include "species.h"

static bool _banned_combination(job_type job, species_type species)
{
    switch (species)
    {
    case SP_FELID:
    case SP_BUTTERFLY:
        if (job == JOB_GLADIATOR
            || job == JOB_ASSASSIN
            || job == JOB_HUNTER
            || job == JOB_ARCANE_MARKSMAN
            || job == JOB_NECKBEARD
            || job == JOB_ARCHAEOLOGIST
            || job == JOB_SNIPER
            || job == JOB_CAVEPERSON
            || job == JOB_RONIN
            || job == JOB_FENCER
            || job == JOB_DERSERKER)
        {
            return true;
        }
        break;
    case SP_DEMIGOD:
        return job_is_zealot(job) || job == JOB_MONK || job == JOB_RONIN;
    case SP_GNOLL:
        if (job == JOB_ARCHAEOLOGIST)
            return true;
        break;
    case SP_GARGOYLE:
        if (job == JOB_DEATH_BISHOP)
            return true;
        break;
    case SP_DEMONSPAWN:
    case SP_ONI:
        return job_is_good_god_zealot(job);
    case SP_TURTLE:
        return job_is_evil_god_zealot(job);
    case SP_ROBOT:
        return job == JOB_TRANSMUTER;
    case SP_SILENT_SPECTRE:
        return job_is_mage(job) || job_is_warrior_mage(job) || job_is_magic_god_zealot(job);
    default:
        break;
    }

    if (job_is_good_god_zealot(job) && species_is_demonic(species))
        return true;

    auto undead = species_undead_type(species);
    if (job == JOB_TRANSMUTER
        && (undead == US_UNDEAD || undead == US_HUNGRY_DEAD
            || undead == US_GHOST))
    {
        return true;
    }
    if (job_is_good_god_zealot(job) && undead != US_ALIVE)
        return true;

    // Cavemen [...] can be humans, dwarves or gnomes.
    if (job == JOB_CAVEPERSON
        && !(species == SP_HUMAN || species == SP_DEMIGOD
             || species == SP_DEEP_DWARF || species == SP_DAB_DWARF
             || species == SP_CRYSTAL_DWARF || species == SP_UNFATHOMED_DWARF
             || species == SP_GNOME
             ))
    {
        return true;
    }

    return false;
}

char_choice_restriction species_allowed(job_type job, species_type speci)
{
    if (!is_starting_species(speci) || !is_starting_job(job))
        return CC_BANNED;

    if (_banned_combination(job, speci))
        return CC_BANNED;

    if (job_recommends_species(job, speci))
        return CC_UNRESTRICTED;

    return CC_RESTRICTED;
}

bool character_is_allowed(species_type species, job_type job)
{
    return !_banned_combination(job, species);
}

char_choice_restriction job_allowed(species_type speci, job_type job)
{
    if (!is_starting_species(speci) || !is_starting_job(job))
        return CC_BANNED;

    if (_banned_combination(job, speci))
        return CC_BANNED;

    if (species_recommends_job(speci, job))
        return CC_UNRESTRICTED;

    return CC_RESTRICTED;
}

bool is_good_combination(species_type spc, job_type job, bool species_first,
                         bool good)
{
    const char_choice_restriction restrict =
        species_first ? job_allowed(spc, job) : species_allowed(job, spc);

    if (good)
        return restrict == CC_UNRESTRICTED;

    return restrict != CC_BANNED;
}

// Is the given god restricted for the character defined by ng?
// Only uses ng.species and ng.job.
char_choice_restriction weapon_restriction(weapon_type wpn,
                                           const newgame_def &ng)
{
    ASSERT_RANGE(ng.species, 0, NUM_SPECIES);
    ASSERT_RANGE(ng.job, 0, NUM_JOBS);
    ASSERT(ng.species == SP_BASE_DRACONIAN
           || !species_is_draconian(ng.species));

    // Some special cases:

    if ((ng.species == SP_FELID || ng.species == SP_BUTTERFLY) && wpn != WPN_UNARMED)
        return CC_BANNED;

    // These recommend short blades because they're good at stabbing,
    // but the fighter's armour hinders that.
    if ((ng.species == SP_NAGA || ng.species == SP_SLITHERIER_NAGA)
         && ng.job == JOB_FIGHTER && wpn == WPN_RAPIER)
    {
        return CC_RESTRICTED;
    }

    if (ng.species == SP_UNIPODE
        && (wpn == WPN_QUARTERSTAFF || wpn == WPN_SHORTBOW))
    {
       return CC_BANNED;
    }

    if (wpn == WPN_QUARTERSTAFF && ng.job != JOB_GLADIATOR
        && !(ng.job == JOB_FIGHTER && ng.species == SP_FORMICID))
    {
        return CC_BANNED;
    }

    // Javelins are always good, boomerangs not so much.
    if (wpn == WPN_THROWN)
    {
        return species_size(ng.species) >= SIZE_MEDIUM ? CC_UNRESTRICTED
                                                       : CC_RESTRICTED;
    }

    if (species_recommends_weapon(ng.species, wpn))
        return CC_UNRESTRICTED;

    return CC_RESTRICTED;
}
