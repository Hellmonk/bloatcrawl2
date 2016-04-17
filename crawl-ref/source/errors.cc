/**
 * @file
 * @brief Handling of error conditions that are not program bugs.
**/

#include "AppHdr.h"

#include "errors.h"

#include <cerrno>
#include <cstdarg>
#include <cstring>

#include "stringutil.h"

NORETURN void fail(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    string buf = vmake_stringf(msg, args);
    va_end(args);

    // Do we want to call end() right on when there's no one trying catching,
    // or should we risk exception code mess things up?
    throw ext_fail_exception(buf);
}

NORETURN void sysfail(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    string buf = vmake_stringf(msg, args);
    va_end(args);

    buf += ": ";
    buf += strerror(errno);

    throw ext_fail_exception(buf);
}

NORETURN void corrupted(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    string buf = vmake_stringf(msg, args);
    va_end(args);

    throw corrupted_save(buf);
}

/**
 * Toss failures from a test into stderr & an error file, if there were any
 * failures.
 *
 * @param fails     The failures, if any.
 * @param name      Errors are dumped to "[name].out"; name should be unique.
 */
void dump_test_fails(string fails, string name)
{
    if (fails.empty())
        return;

    fprintf(stderr, "%s", fails.c_str());

    const string outfile = make_stringf("%s.out", name.c_str());
    FILE *f = fopen(outfile.c_str(), "w");
    if (!f)
        sysfail("can't write test output");
    fprintf(f, "%s", fails.c_str());
    fclose(f);
    fail("%s event errors (dumped to %s)",
         name.c_str(), outfile.c_str());
}
