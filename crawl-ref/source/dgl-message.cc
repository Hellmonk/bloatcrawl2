#include "AppHdr.h"

#ifdef DGL_SIMPLE_MESSAGING

#include "dgl-message.h"

#include <cerrno>
#include <sys/stat.h>

#include "files.h"
#include "format.h"
#include "initfile.h"
#include "libutil.h"
#include "message.h"
#include "notes.h"
#include "options.h"
#include "output.h"
#include "stringutil.h"
#include "syscalls.h"

static struct stat mfilestat;

static void _show_message_line(string line)
{
    const string::size_type sender_pos = line.find(":");
    if (sender_pos == string::npos)
        mpr(line);
    else
    {
        string sender = line.substr(0, sender_pos);
        line = line.substr(sender_pos + 1);
        trim_string(line);
        mprf_nocap(MSGCH_DGL_MESSAGE, "<white>%s:</white> %s", sender.c_str(),
                                                               line.c_str());
        if (Options.note_dgl_messages)
        {
            take_note(Note(NOTE_MESSAGE, MSGCH_DGL_MESSAGE, 0,
                           (sender + ": " + line).c_str()));
        }
    }
}

static void _kill_messaging(FILE *mf)
{
    if (mf)
        fclose(mf);
    SysEnv.have_messages = false;
    Options.messaging = false;
}

static void _read_each_message()
{
    bool say_got_msg = true;
    FILE *mf = fopen_u(SysEnv.messagefile.c_str(), "r+");
    if (!mf)
    {
        mprf(MSGCH_ERROR, "Couldn't read %s: %s", SysEnv.messagefile.c_str(),
             strerror(errno));
        _kill_messaging(mf);
        return;
    }

    // Read messages, code borrowed from the SIMPLEMAIL patch.
    char line[120];

    if (!lock_file_handle(mf, false))
    {
        mprf(MSGCH_ERROR, "Failed to lock %s: %s", SysEnv.messagefile.c_str(),
             strerror(errno));
        _kill_messaging(mf);
        return;
    }

    while (fgets(line, sizeof line, mf))
    {
        unlock_file_handle(mf);

        const int len = strlen(line);
        if (len)
        {
            if (line[len - 1] == '\n')
                line[len - 1] = 0;

            if (say_got_msg)
            {
                mprf(MSGCH_DGL_MESSAGE, "Your messages:");
                say_got_msg = false;
            }

            _show_message_line(line);
        }

        if (!lock_file_handle(mf, false))
        {
            mprf(MSGCH_ERROR, "Failed to lock %s: %s",
                 SysEnv.messagefile.c_str(),
                 strerror(errno));
            _kill_messaging(mf);
            return;
        }
    }
    if (!lock_file_handle(mf, true))
    {
        mprf(MSGCH_ERROR, "Unable to write lock %s: %s",
             SysEnv.messagefile.c_str(),
             strerror(errno));
    }
    if (!ftruncate(fileno(mf), 0))
        mfilestat.st_mtime = 0;
    unlock_file_handle(mf);
    fclose(mf);

    SysEnv.have_messages = false;
}

void read_messages()
{
    _read_each_message();
    you.redraw_title = true;
}

static void _announce_messages()
{
    // XXX: We could do a NetHack-like mail daemon here at some point.
    mprf(MSGCH_DGL_MESSAGE, "Beep! Your pager goes off! Use _ to check your messages.");
}

void check_messages()
{
    if (!Options.messaging
        || SysEnv.have_messages
        || SysEnv.messagefile.empty()
        || kbhit()
        || (SysEnv.message_check_tick++ % DGL_MESSAGE_CHECK_INTERVAL))
    {
        return;
    }

    const bool had_messages = SysEnv.have_messages;
    struct stat st;
    if (stat(SysEnv.messagefile.c_str(), &st))
    {
        mfilestat.st_mtime = 0;
        return;
    }

    if (st.st_mtime > mfilestat.st_mtime)
    {
        if (st.st_size)
            SysEnv.have_messages = true;
        mfilestat.st_mtime = st.st_mtime;
    }

    if (SysEnv.have_messages && !had_messages)
    {
        _announce_messages();
        update_message_status();
    }
}

#endif
