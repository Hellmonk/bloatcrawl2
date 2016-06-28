/**
 * @file
 * @brief String manipulation functions that don't fit elsewhere.
 **/

#ifndef STRINGS_H
#define STRINGS_H

#include "config.h"
#include "libutil.h" // always_true

#ifdef CRAWL_HAVE_STRLCPY
#include <cstring>
#else
size_t strlcpy(char *dst, const char *src, size_t n);
#endif

string lowercase_string(const string &s);
string &lowercase(string &s);
string &uppercase(string &s);
string uppercase_string(string s);
string lowercase_first(string);
string uppercase_first(string);

/**
 * Returns 1 + the index of the first suffix that matches the given string,
 * 0 if no suffixes match.
 */
int ends_with(const string &s, const char * const suffixes[]);

string wordwrap_line(string &s, int cols, bool tags = false,
                     bool indent = false);

string strip_filename_unsafe_chars(const string &s);

string vmake_stringf(const char *format, va_list args);
string make_stringf(PRINTF(0, ));

bool strip_suffix(string &s, const string &suffix);

string replace_all(string s, const string &tofind, const string &replacement);

string replace_all_of(string s, const string &tofind, const string &replacement);

string replace_keys(const string &text, const map<string, string>& replacements);

string maybe_capitalise_substring(string s);
string maybe_pick_random_substring(string s);

int count_occurrences(const string &text, const string &searchfor);

string &trim_string(string &str);
string &trim_string_right(string &str);
string trimmed_string(string s);

/**
 * Find the enumerator e between begin and end that satisfies pred(e) and
 * whose name, as given by namefunc(e), has the earliest occurrence of the
 * substring spec.
 *
 * @tparam Enum     An integer-like or C-style enum type no larger than
 *                  size_t. More specifically, Enum must be implicitly
 *                  convertible to size_t, and must be explicitly convertible
 *                  from size_t with static_cast. There should be no gaps
 *                  in enumerator values between begin and end.
 *
 * @param spec      The substring to search for.
 * @param begin     The beginning of the enumerator range to search in.
 * @param end       One past the end of the enum range to search in.
 * @param pred      A function from Enum to bool. Enumerators that do not
 *                  satisfy the predicate are ignored.
 * @param namefunc  A function from Enum to string or const char * giving
 *                  the name of the enumerator.
 * @return The enumerator that satisfies pred and whose name contains the
 *         spec substring beginning at the earliest position. If no such
 *         enumerator exists, returns end. If there are multiple strings
 *         containing the spec as a prefix, returns the shortest such string
 *         (so exact matches are preferred); otherwise ties are broken in
 *         an unspecified manner.
 */
template<class Enum, class Pred, class NameFunc>
Enum find_earliest_match(const string &spec, Enum begin, Enum end,
                         Pred pred, NameFunc namefunc)
{
    Enum selected = end;
    const size_t speclen = spec.length();
    size_t bestpos = string::npos;
    size_t bestlen = string::npos;
    for (size_t i = begin; i < (size_t) end; ++i)
    {
        const Enum curr = static_cast<Enum>(i);

        if (!pred(curr))
            continue;

        const string name = lowercase_string(namefunc(curr));
        const size_t pos = name.find(spec);
        const size_t len = name.length();

        if (pos < bestpos || pos == 0 && len < bestlen)
        {
            // Exit early if we found an exact match.
            if (pos == 0 && len == speclen)
                return curr;

            // npos is never less than bestpos, so the spec was found.
            bestpos = pos;
            if (pos == 0)
                bestlen = len;
            selected = curr;
        }
    }
    return selected;
}

/**
 * Join together strings computed by a function applied to some elements
 * of a range.
 *
 * @tparam Z An iterator or pointer type.
 * @tparam F A callable type that takes whatever Z points to, and
 *     returns a string or null-terminated char *.
 * @tparam G A callable type that takes whatever Z points to, and
 *     returns some type that is explicitly convertable to bool
 *
 * @param start An iterator to the beginning of the range of elements to
 *     consider.
 * @param end An iterator to one spot past the end of the range of
 *     elements to consider.
 * @param stringify A function or function-like object that takes an
 *     element from the range and returns a string or C string. Will be
 *     called once per selected element.
 * @param andc The separator to use before the last selected element.
 * @param comma The separator to use between elements other than the last.
 * @param filter A function or function-like object to select elements.
 *     Should accept as a single argument an element from the range, and
 *     return true if the element should be included in the result string.
 *     Will be called between N and 2N times, where N is the total number
 *     of elements in the range.
 *
 * @return A string containing the stringifications of all the elements
 *     for which filter returns true, with andc separating the last two
 *     elements and comma separating the other elements. If the range is
 *     empty, returns the empty string.
 */
template <typename Z, typename F, typename G>
string comma_separated_fn(Z start, Z end, F stringify,
                          const string &andc, const string &comma,
                          G filter)
{
    string text;
    bool first = true;
    for (Z i = start; i != end; ++i)
    {
        if (!filter(*i))
            continue;

        if (first)
            first = false;
        else
        {
            Z tmp = i;
            // Advance until we find an item selected by the filter.
            //
            // This loop iterates (and calls filter) a linear number of times
            // over the entire call to comma_separated_fn. Some cases:
            //
            // filter is always true: do loop iterates once, is reached N-1
            //   times: N-1 iterations total.
            //
            // filter is true half the time: do loop iterates twice on average,
            //   is reached N/2 - 1 times: N-2 iterations total.
            //
            // filter is true for sqrt(N) elements: do loop iterates sqrt(N)
            //   times on average, is reached sqrt(N) - 1 times: N - sqrt(N)
            //   iterations total.
            //
            // filter is always false: do loop is never reached: 0 iterations.
            do
            {
                // TODO: really, we could update i here (one fewer time than
                // tmp): if the filter returns false, we might as well have
                // the outer for loop skip that element, so it doesn't have
                // to call the filter again before deciding to "continue;".
                ++tmp;
            }
            while (tmp != end && !filter(*tmp));

            if (tmp != end)
                text += comma;
            else
                text += andc;
        }

        text += stringify(*i);
    }
    return text;
}

template <typename Z, typename F>
string comma_separated_fn(Z start, Z end, F stringify,
                          const string &andc = " and ",
                          const string &comma = ", ")
{
    return comma_separated_fn(start, end, stringify, andc, comma,
                              always_true<decltype(*start)>);
}

template <typename Z>
string comma_separated_line(Z start, Z end, const string &andc = " and ",
                            const string &comma = ", ")
{
    return comma_separated_fn(start, end, [] (const string &s) { return s; },
                              andc, comma);
}

static inline bool starts_with(const string &s, const string &prefix)
{
    return s.rfind(prefix, 0) != string::npos;
}

static inline bool ends_with(const string &s, const string &suffix)
{
    if (s.length() < suffix.length())
        return false;
    return s.find(suffix, s.length() - suffix.length()) != string::npos;
}

// Splits string 's' on the separator 'sep'. If trim == true, trims each
// segment. If accept_empties == true, accepts empty segments. If nsplits >= 0,
// splits on the first nsplits occurrences of the separator, and stores the
// remainder of the string as the last segment; negative values of nsplits
// split on all occurrences of the separator.
vector<string> split_string(const string &sep, string s, bool trim = true,
                            bool accept_empties = false, int nsplits = -1);

// time

string make_time_string(time_t abs_time, bool terse = false);
string make_file_time(time_t when);

// Work around missing std::to_string. This will break when newlib adds
// support for long double, which will enable std::to_string in libstdc++.
//
// See http://permalink.gmane.org/gmane.os.cygwin/150485 for more info.
// Disable this workaround under MSYS2, which is based on CYGWIN but has
// std::to_string.
#if defined(TARGET_COMPILER_CYGWIN) && !defined(__MSYS__)
// Injecting into std:: because we sometimes use std::to_string to
// disambiguate.
namespace std
{
    static inline string to_string(int value)
    {
        return make_stringf("%d", value);
    }
    static inline string to_string(long value)
    {
        return make_stringf("%ld", value);
    }
    static inline string to_string(long long value)
    {
        return make_stringf("%lld", value);
    }
    static inline string to_string(unsigned value)
    {
        return make_stringf("%u", value);
    }
    static inline string to_string(unsigned long value)
    {
        return make_stringf("%lu", value);
    }
    static inline string to_string(unsigned long long value)
    {
        return make_stringf("%llu", value);
    }
    static inline string to_string(float value)
    {
        return make_stringf("%f", value);
    }
    static inline string to_string(double value)
    {
        return make_stringf("%f", value);
    }
}
#endif

#endif
