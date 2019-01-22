#pragma once

enum killer_type                       // monster_die(), thing_thrown
{
    KILL_NONE,                         // no killer
    KILL_YOU,                          // you are the killer
    KILL_MON,                          // no, it was a monster!
    KILL_YOU_MISSILE,                  // in the library, with a dart
    KILL_MON_MISSILE,                  // in the dungeon, with a club
    KILL_YOU_CONF,                     // died while confused as caused by you
    KILL_MISCAST,                      // as a result of a spell miscast
    KILL_MISC,                         // any miscellaneous killing
    KILL_RESET,                        // excised from existence
    KILL_DISMISSED,                    // like KILL_RESET, but drops inventory
    KILL_BANISHED,                     // monsters what got banished
    KILL_TIMEOUT,                      // non-summoned monsters whose times ran out
    KILL_PACIFIED,                     // only used by milestones and notes
    KILL_ENSLAVED,                     // only used by milestones and notes
    KILL_SLIMIFIED,                    // only used by milestones and notes
};
