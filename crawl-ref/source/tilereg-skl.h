#ifdef USE_TILE_LOCAL
#pragma once

#include "tilereg-grid.h"

class SkillRegion : public GridRegion
{
public:
    SkillRegion(const TileRegionInit &init);

    virtual void update() override;
    virtual int handle_mouse(wm_mouse_event &event) override;
    virtual bool update_tip_text(string &tip) override;
    virtual bool update_tab_tip_text(string &tip, bool active) override;
    virtual bool update_alt_text(string &alt) override;

    virtual const string name() const override { return "Skills"; }

protected:
    virtual void pack_buffers() override;
    virtual void draw_tag() override;
    virtual void activate() override;
};

#endif
