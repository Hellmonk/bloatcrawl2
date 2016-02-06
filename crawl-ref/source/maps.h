/**
 * @file
 * @brief Functions used to create vaults.
**/

#ifndef MAPS_H
#define MAPS_H

#include <vector>

#include "mapdef.h"
#include "unwind.h"

class map_def;
struct map_file_place;
struct vault_placement;

typedef vector<map_def> map_vector;
typedef vector<const map_def *> mapref_vector;

map_section_type vault_main(vault_placement &vp, const map_def *vault,
                            bool check_place = false);

bool resolve_subvault(map_def &vault);

coord_def find_portal_place(const vault_placement *place, bool check_place);

const map_def *map_by_index(int index);
void strip_all_maps();
int map_count();

string vault_chance_tag(const map_def &map);

const map_def *find_map_by_name(const string &name);
const map_def *random_map_for_place(const level_id &place,
                                    bool minivault,
                                    maybe_bool extra = MB_MAYBE);
const map_def *random_map_in_depth(const level_id &lid,
                                   bool want_minivault = false,
                                   maybe_bool extra = MB_MAYBE);
const map_def *random_map_for_tag(const string &tag,
                                  bool check_depth = false,
                                  bool check_chance = false,
                                  maybe_bool extra = MB_MAYBE);
mapref_vector random_chance_maps_in_depth(const level_id &place,
                                          maybe_bool extra = MB_MAYBE);

void dump_map(const map_def &map);
void add_parsed_map(const map_def &md);

vector<string> find_map_matches(const string &name);

mapref_vector find_maps_for_tag(const string &tag,
                                bool check_depth = false,
                                bool check_used = true);

void read_maps();
void reread_maps();
void read_map(const string &file);
void run_map_global_preludes();
void run_map_local_preludes();
string get_descache_path(const string &file, const string &ext);

typedef map<string, map_file_place> map_load_info_t;

extern map_load_info_t lc_loaded_maps;
extern string          lc_desfile;
extern map_def         lc_map;
extern depth_ranges    lc_default_depths;
extern dlua_chunk      lc_global_prelude;
extern bool            lc_run_global_prelude;

typedef bool (*map_place_check_t)(const map_def &, const coord_def &c,
                                  const coord_def &size);

typedef vector<coord_def> point_vector;
typedef vector<string> string_vector;

extern map_place_check_t map_place_valid;
extern point_vector      map_anchor_points;

// Use dgn_map_parameters to modify:
extern string_vector     map_parameters;

class dgn_map_parameters
{
public:
    dgn_map_parameters(const string &astring);
    dgn_map_parameters(const string_vector &parameters);
private:
    unwind_var<string_vector> mpar;
};

#ifdef DEBUG_STATISTICS
void mapstat_report_random_maps(FILE *outf, const level_id &place);
#endif

#endif
