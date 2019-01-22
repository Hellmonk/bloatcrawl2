#pragma once

#include <set>

// Character info has its own top-level tag, mismatching majors don't break
// compatibility there.
// DO NOT BUMP THIS UNLESS YOU KNOW WHAT YOU'RE DOING. This would break
// the save browser across versions, possibly leading to overwritten games.
// It's only there in case there's no way out.
#define TAG_CHR_FORMAT 0
COMPILE_CHECK(TAG_CHR_FORMAT < 256);

// Let CDO updaters know if the syntax changes.
// Really, really, REALLY _never_ ever bump this and clean up old #ifdefs
// in a single commit, please. Making clean-up and actual code changes,
// especially of this size, separated is vital for sanity.
#ifndef TAG_MAJOR_VERSION
#define TAG_MAJOR_VERSION 34
#endif
COMPILE_CHECK(TAG_MAJOR_VERSION < 256);

// Minor version will be reset to zero when major version changes.
enum tag_minor_version
{
    TAG_MINOR_INVALID         = -1,
    TAG_MINOR_RESET           = 0, // Minor tags were reset
    NUM_TAG_MINORS,
    TAG_MINOR_VERSION = NUM_TAG_MINORS - 1
};

// Marshalled as a byte in several places.
COMPILE_CHECK(TAG_MINOR_VERSION <= 0xff);

// tags that affect loading bones files. If you do save compat that affects
// ghosts, these must be updated in addition to the enum above.
const set<int> bones_minor_tags =
        {TAG_MINOR_RESET,
        };

struct save_version
{
    save_version(int _major, int _minor) : major{_major}, minor{_minor}
    {
    }

    save_version() : save_version(-1,-1)
    {
    }

    static save_version current()
    {
        return save_version(TAG_MAJOR_VERSION, TAG_MINOR_VERSION);
    }

    static save_version current_bones()
    {
        return save_version(TAG_MAJOR_VERSION, *bones_minor_tags.crbegin());
    }

    bool valid() const
    {
        return major > 0 && minor > -1;
    }

    inline friend bool operator==(const save_version& lhs,
                                                    const save_version& rhs)
    {
        return lhs.major == rhs.major && lhs.minor == rhs.minor;
    }

    inline friend bool operator!=(const save_version& lhs,
                                                    const save_version& rhs)
    {
        return !operator==(lhs, rhs);
    }

    inline friend bool operator< (const save_version& lhs,
                                                    const save_version& rhs)
    {
        return lhs.major < rhs.major || lhs.major == rhs.major &&
                                                    lhs.minor < rhs.minor;
    }
    inline friend bool operator> (const save_version& lhs,
                                                    const save_version& rhs)
    {
        return  operator< (rhs, lhs);
    }
    inline friend bool operator<=(const save_version& lhs,
                                                    const save_version& rhs)
    {
        return !operator> (lhs, rhs);
    }
    inline friend bool operator>=(const save_version& lhs,
                                                    const save_version& rhs)
    {
        return !operator< (lhs,rhs);
    }


    bool is_future() const
    {
        return valid() && *this > save_version::current();
    }

    bool is_past() const
    {
        return valid() && *this < save_version::current();
    }

    bool is_ancient() const
    {
        return valid() && major < TAG_MAJOR_VERSION;
    }

    bool is_compatible() const
    {
        return valid() && !is_ancient() && !is_future();
    }

    bool is_current() const
    {
        return valid() && *this == save_version::current();
    }

    bool is_bones_version() const
    {
        return valid() && is_compatible() && bones_minor_tags.count(minor) > 0;
    }

    int major;
    int minor;
};
