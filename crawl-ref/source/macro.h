/**
 * @file
 * @brief Crude macro-capability
**/

#pragma once

#include <deque>

#include "command-type.h"
#include "enum.h"
#include "KeymapContext.h"

class key_recorder;
typedef deque<int> keyseq;

class key_recorder
{
public:
    bool                  paused;
    keyseq                keys;

public:
    key_recorder();

    void add_key(int key, bool reverse = false);
    void clear();
};

class pause_all_key_recorders
{
public:
    pause_all_key_recorders();
    ~pause_all_key_recorders();

private:
    vector<bool> prev_pause_status;
};

int getchm(KeymapContext context = KMC_DEFAULT);

int get_ch();

int getch_with_command_macros();  // keymaps and macros (ie for commands)

void flush_input_buffer(int reason);

void macro_add_query();
void macro_init();
void macro_save();

void macro_clear_buffers();

void macro_userfn(const char *keys, const char *registryname);

// Add macro-expanded keys to the end or start of the keyboard buffer.
void macro_sendkeys_end_add_expanded(int key);
void macro_sendkeys_end_add_cmd(command_type cmd);

// [ds] Unless you know what you're doing, prefer macro_sendkeys_add_expanded
// to direct calls to macro_buf_add for pre-expanded key sequences.
//
// Crawl doesn't like holes in macro-expanded sequences in its main buffer.
void macro_buf_add(int key,
                   bool reverse = false, bool expanded = true);
void macro_buf_add(const keyseq &actions,
                   bool reverse = false, bool expanded = true);
int macro_buf_get();

void macro_buf_add_cmd(command_type cmd, bool reverse = false);
void macro_buf_add_with_keymap(keyseq keys, KeymapContext mc);

void process_command_on_record(command_type cmd);

bool is_userfunction(int key);
bool is_synthetic_key(int key);

string get_userfunction(int key);

void add_key_recorder(key_recorder* recorder);
void remove_key_recorder(key_recorder* recorder);

bool is_processing_macro();
bool has_pending_input();

int get_macro_buf_size();

///////////////////////////////////////////////////////////////
// Keybinding stuff

void init_keybindings();

command_type name_to_command(string name);
string  command_to_name(command_type cmd);

string keyseq_to_str(const keyseq &seq);

command_type  key_to_command(int key, KeymapContext context);
int           command_to_key(command_type cmd);

KeymapContext context_for_command(command_type cmd);

void bind_command_to_key(command_type cmd, int key);

string command_to_string(command_type cmd, bool tutorial = false);
void insert_commands(string &desc, const vector<command_type> &cmds,
                     bool formatted = true);

// Let rc files declare macros:
string read_rc_file_macro(const string& field);
