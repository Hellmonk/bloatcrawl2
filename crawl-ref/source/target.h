#ifndef TARGET_H
#define TARGET_H

#include "beam.h"

enum aff_type // sign and non-zeroness matters
{
    AFF_TRACER = -1,
    AFF_NO      = 0,
    AFF_MAYBE   = 1, // can possibly affect
    AFF_YES,         // intended/likely to affect
    // If you want to extend this to pass the probability somehow, feel free to,
    // just keep AFF_YES the minimal "bright" value.
    AFF_LANDING,     // Valid shadow step landing site
    AFF_MULTIPLE,    // Passes through multiple times
};

class targetter
{
public:
    virtual ~targetter() {};

    coord_def origin;
    coord_def aim;
    const actor* agent;
    string why_not;

    virtual bool set_aim(coord_def a);
    virtual bool valid_aim(coord_def a) = 0;
    virtual bool can_affect_outside_range();
    virtual bool can_affect_walls();

    virtual aff_type is_affected(coord_def loc) = 0;
    virtual bool has_additional_sites(coord_def a);
    virtual bool affects_monster(const monster_info& mon);
protected:
    bool anyone_there(coord_def loc);
};

class targetter_beam : public targetter
{
public:
    targetter_beam(const actor *act, int range, zap_type zap, int pow,
                   int min_expl_rad, int max_expl_rad);
    bolt beam;
    virtual bool set_aim(coord_def a) override;
    bool valid_aim(coord_def a) override;
    bool can_affect_outside_range() override;
    virtual aff_type is_affected(coord_def loc) override;
    virtual bool affects_monster(const monster_info& mon) override;
protected:
    vector<coord_def> path_taken; // Path beam took.
    void set_explosion_aim(bolt tempbeam);
    void set_explosion_target(bolt &tempbeam);
    int min_expl_rad, max_expl_rad;
private:
    bool penetrates_targets;
    int range;
    explosion_map exp_map_min, exp_map_max;
};

class targetter_unravelling : public targetter_beam
{
public:
    targetter_unravelling(const actor *act, int range, int pow);
    bool set_aim(coord_def a) override;
};

class targetter_imb : public targetter_beam
{
public:
    targetter_imb(const actor *act, int pow, int range);
    bool set_aim(coord_def a) override;
    aff_type is_affected(coord_def loc) override;
private:
    vector<coord_def> splash;
    vector<coord_def> splash2;
};

class targetter_view : public targetter
{
public:
    targetter_view();
    bool valid_aim(coord_def a) override;
    aff_type is_affected(coord_def loc) override;
};

class targetter_smite : public targetter
{
public:
    targetter_smite(const actor *act, int range = LOS_RADIUS,
                    int exp_min = 0, int exp_max = 0, bool wall_ok = false,
                    bool (*affects_pos_func)(const coord_def &) = 0);
    virtual bool set_aim(coord_def a) override;
    virtual bool valid_aim(coord_def a) override;
    virtual bool can_affect_outside_range() override;
    aff_type is_affected(coord_def loc) override;
protected:
    // assumes exp_map is valid only if >0, so let's keep it private
    int exp_range_min, exp_range_max;
    explosion_map exp_map_min, exp_map_max;
private:
    int range;
    bool affects_walls;
    bool (*affects_pos)(const coord_def &);
};

class targetter_fragment : public targetter_smite
{
public:
    targetter_fragment(const actor *act, int power, int range = LOS_RADIUS);
    bool set_aim(coord_def a) override;
    bool valid_aim(coord_def a) override;
    bool can_affect_walls() override;
private:
    int pow;
};

class targetter_reach : public targetter
{
public:
    targetter_reach(const actor* act, reach_type ran = REACH_NONE);
    reach_type range;
    bool valid_aim(coord_def a) override;
    aff_type is_affected(coord_def loc) override;
};

class targetter_cleave : public targetter
{
public:
    targetter_cleave(const actor* act, coord_def target);
    aff_type is_affected(coord_def loc) override;
    bool valid_aim(coord_def a) override { return false; }
private:
    set<coord_def> targets;
};

class targetter_cloud : public targetter
{
public:
    targetter_cloud(const actor* act, int range = LOS_RADIUS,
                    int count_min = 8, int count_max = 10);
    bool set_aim(coord_def a) override;
    bool valid_aim(coord_def a) override;
    bool can_affect_outside_range() override;
    aff_type is_affected(coord_def loc) override;
    int range;
    int cnt_min, cnt_max;
    map<coord_def, aff_type> seen;
    vector<vector<coord_def> > queue;
    bool avoid_clouds;
};

class targetter_splash : public targetter
{
public:
    targetter_splash(const actor *act);
    bool valid_aim(coord_def a) override;
    aff_type is_affected(coord_def loc) override;
};

class targetter_los : public targetter
{
public:
    targetter_los(const actor *act, los_type los = LOS_DEFAULT,
                  int ran = LOS_RADIUS, int ran_max = 0);
    bool valid_aim(coord_def a) override;
    aff_type is_affected(coord_def loc) override;
private:
    los_type los;
    int range, range_max;
};

class targetter_thunderbolt : public targetter
{
public:
    targetter_thunderbolt(const actor *act, int r, coord_def _prev);

    bool valid_aim(coord_def a) override;
    bool set_aim(coord_def a) override;
    aff_type is_affected(coord_def loc) override;
    map<coord_def, aff_type> zapped;
    FixedVector<int, LOS_RADIUS + 1> arc_length;
private:
    coord_def prev;
    int range;
};

class targetter_spray : public targetter
{
public:
    targetter_spray(const actor* act, int range, zap_type zap);

    bool valid_aim(coord_def a) override;
    bool set_aim(coord_def a) override;
    aff_type is_affected(coord_def loc) override;
    bolt base_beam;
    vector<bolt> beams;
private:
    vector<vector<coord_def> > paths_taken;
    int _range;
};

enum shadow_step_block_reason
{
    BLOCKED_NONE,
    BLOCKED_OCCUPIED,
    BLOCKED_MOVE,
    BLOCKED_PATH,
    BLOCKED_NO_TARGET,
    BLOCKED_MOBILE,
};

class targetter_shadow_step : public targetter
{
public:
    targetter_shadow_step(const actor* act, int r);

    bool valid_aim(coord_def a) override;
    bool set_aim(coord_def a) override;
    bool step_is_blocked;
    aff_type is_affected(coord_def loc) override;
    bool has_additional_sites(coord_def a) override;
    set<coord_def> additional_sites;
    coord_def landing_site;
private:
    void set_additional_sites(coord_def a);
    void get_additional_sites(coord_def a);
    bool valid_landing(coord_def a, bool check_invis = true);
    shadow_step_block_reason no_landing_reason;
    shadow_step_block_reason blocked_landing_reason;
    set<coord_def> temp_sites;
    int range;
};

class targetter_explosive_bolt : public targetter_beam
{
public:
    targetter_explosive_bolt(const actor *act, int pow, int range);
    bool set_aim(coord_def a) override;
    aff_type is_affected(coord_def loc) override;
private:
    explosion_map exp_map;
};

class targetter_cone : public targetter
{
public:
    targetter_cone(const actor *act, int r);

    bool valid_aim(coord_def a) override;
    bool set_aim(coord_def a) override;
    aff_type is_affected(coord_def loc) override;
    map<coord_def, aff_type> zapped;
    FixedVector< map<coord_def, aff_type>, LOS_RADIUS + 1 > sweep;
private:
    int range;
};

#define CLOUD_CONE_BEAM_COUNT 11

class targetter_shotgun : public targetter
{
public:
    targetter_shotgun(const actor* act, size_t beam_count, int r);
    bool valid_aim(coord_def a) override;
    bool set_aim(coord_def a) override;
    aff_type is_affected(coord_def loc) override;
    vector<ray_def> rays;
    map<coord_def, size_t> zapped;
private:
    size_t num_beams;
    int range;
};

class targetter_monster_sequence : public targetter_beam
{
public:
    targetter_monster_sequence(const actor *act, int pow, int range);
    bool set_aim(coord_def a);
    bool valid_aim(coord_def a);
    aff_type is_affected(coord_def loc);
private:
    explosion_map exp_map;
};
#endif
