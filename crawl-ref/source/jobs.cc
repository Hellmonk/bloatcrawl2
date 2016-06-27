#include "AppHdr.h"

#include "jobs.h"

#include "enum.h"
#include "errors.h"
#include "itemprop.h"
#include "libutil.h"
#include "mapdef.h"
#include "ng-setup.h"
#include "player.h"
#include "stringutil.h"

#include "job-data.h"

static const job_def& _job_def(job_type job)
{
    ASSERT_RANGE(job, 0, NUM_JOBS);
    return job_data.at(job);
}

const char *get_job_abbrev(job_type which_job)
{
    if (which_job == JOB_UNKNOWN)
        return "Un";
    return _job_def(which_job).abbrev;
}

job_type get_job_by_abbrev(const char *abbrev)
{
    for (auto& entry : job_data)
        if (lowercase_string(abbrev) == lowercase_string(entry.second.abbrev))
            return entry.first;

    return JOB_UNKNOWN;
}

const char *get_job_name(job_type which_job)
{
    if (which_job == JOB_UNKNOWN)
        return "Unemployed";

    return _job_def(which_job).name;
}

job_type get_job_by_name(const char *name)
{
    job_type job = JOB_UNKNOWN;

    const string low_name = lowercase_string(name);

    for (auto& entry : job_data)
    {
        string low_job = lowercase_string(entry.second.name);

        const size_t pos = low_job.find(low_name);
        if (pos != string::npos)
        {
            job = entry.first;
            if (!pos)  // prefix takes preference
                break;
        }
    }

    return job;
}

// Must be called after species_stat_init for the wanderer formula to work.
void job_stat_init(job_type job)
{
    you.hp_max_adj_perm = 0;

    you.base_stats[STAT_STR] += _job_def(job).s;
    you.base_stats[STAT_INT] += _job_def(job).i;
    you.base_stats[STAT_DEX] += _job_def(job).d;

    if (job == JOB_WANDERER)
    {
        for (int i = 0; i < 12; i++)
        {
            const stat_type stat = static_cast<stat_type>(random2(NUM_STATS));
            // Stats that are already high will be chosen half as often.
            if (you.base_stats[stat] > 17 && coinflip())
            {
                i--;
                continue;
            }

            you.base_stats[stat]++;
        }
    }
}

bool job_has_weapon_choice(job_type job)
{
    return _job_def(job).wchoice != WCHOICE_NONE;
}

bool job_gets_good_weapons(job_type job)
{
    return _job_def(job).wchoice == WCHOICE_GOOD;
}

bool job_gets_ranged_weapons(job_type job)
{
    return _job_def(job).wchoice == WCHOICE_RANGED;
}

void give_job_equipment(job_type job)
{
    item_list items;
    for (const string& it : _job_def(job).equipment)
        items.add_item(it);
    for (size_t i = 0; i < items.size(); i++)
    {
        const item_spec spec = items.get_item(i);
        int plus = 0;
        if (spec.props.exists("charges"))
            plus = spec.props["charges"];
        if (spec.props.exists("plus"))
            plus = spec.props["plus"];
        newgame_make_item(spec.base_type, spec.sub_type, max(spec.qty, 1),
                          plus, spec.ego);
    }
}

// Must be called after equipment is given for weapon skill to be given.
void give_job_skills(job_type job)
{
    for (const pair<skill_type, int>& entry : _job_def(job).skills)
    {
        skill_type skill = entry.first;
        int amount = entry.second;
        if (skill == SK_WEAPON)
        {
            const item_def *weap = you.weapon();
            skill = weap ? item_attack_skill(*weap) : SK_UNARMED_COMBAT;
            //XXX: WTF?
            if (you.species == SP_FELID && job == JOB_FIGHTER)
                amount += 2;
            // Don't give throwing hunters Short Blades skill.
            if (job_gets_ranged_weapons(job) && !(weap && is_range_weapon(*weap)))
                skill = SK_THROWING;
        }
        you.skills[skill] += amount;
    }
}

void debug_jobdata()
{
    string fails;

    for (int i = 0; i < NUM_JOBS; i++)
        if (!job_data.count(static_cast<job_type>(i)))
            fails += "job number " + to_string(i) + "is not present\n";

    item_list dummy;
    for (auto& entry : job_data)
        for (const string& it : entry.second.equipment)
        {
            const string error = dummy.add_item(it, false);
            if (error != "")
                fails += error + "\n";
        }

    dump_test_fails(fails, "job-data");
}

bool job_recommends_species(job_type job, species_type species)
{
    return find(_job_def(job).recommended_species.begin(),
                _job_def(job).recommended_species.end(),
                species) != _job_def(job).recommended_species.end();
}
