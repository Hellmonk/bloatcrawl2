#ifdef USE_TILE_LOCAL
#pragma once

#include "tilereg-text.h"

class CRTMenuEntry;

/**
 * Expanded CRTRegion to support highlightable and clickable entries - felirx
 * The user of this region will have total control over the positioning of
 * objects at all times
 * The base class behaves like the current CRTRegion used commonly
 * It's identity is mapped to the CRT_NOMOUSESELECT value in TilesFramework
 */
class CRTRegion : public TextRegion
{
public:

    CRTRegion(FontWrapper *font_arg);
    virtual ~CRTRegion();

    virtual void render() override;

    virtual int handle_mouse(wm_mouse_event& event) override;

    virtual void on_resize() override;
};

/**
 * Enhanced Mouse handling for CRTRegion
 * The behaviour is CRT_SINGESELECT
 */
class CRTSingleSelect : public CRTRegion
{
public:
    CRTSingleSelect(FontWrapper* font);

    virtual int handle_mouse(wm_mouse_event& event) override;
};

#endif
