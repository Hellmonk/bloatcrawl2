#ifdef USE_TILE_LOCAL
#pragma once

#include "tilereg-grid.h"

struct monster_info;
class MonsterRegion : public GridRegion
{
public:
    MonsterRegion(const TileRegionInit &init);

    virtual void update() override;
    virtual int handle_mouse(wm_mouse_event &event) override;
    virtual bool update_tip_text(string &tip) override;
    virtual bool update_tab_tip_text(string &tip, bool active) override;
    virtual bool update_alt_text(string &alt) override;

    virtual const string name() const override { return "Monsters"; }

protected:
    const monster_info* get_monster(unsigned int idx) const;

    virtual void pack_buffers() override;
    virtual void draw_tag() override;
    virtual void activate() override;

    vector<monster_info> m_mon_info;
};

#endif
