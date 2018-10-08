#ifdef USE_TILE_LOCAL
#pragma once

#include "tilebuf.h"
#include "tilereg-text.h"

class StatRegion : public TextRegion
{
public:
    StatRegion(FontWrapper *font_arg);

    virtual int handle_mouse(MouseEvent &event) override;
    virtual bool update_tip_text(string &tip) override;

    virtual void render() override;
    virtual void clear() override;

protected:
    ShapeBuffer m_shape_buf;
    void _clear_buffers();
};

#endif
