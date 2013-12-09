/** @file viewports.h  Player viewports and related low-level rendering.
 *
 * @author Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DENG_CLIENT_VIEWPORTS_H
#define DENG_CLIENT_VIEWPORTS_H

#ifdef __SERVER__
#  error "viewports.h is for the client only"
#endif

#include <de/Vector>
#include <de/rect.h>

class BspLeaf;
class Lumobj;

struct viewport_t
{
    int console;
    RectRaw geometry;
};

struct viewer_t
{
    de::Vector3d origin;
    angle_t angle;
    float pitch;

    viewer_t(de::Vector3d const &origin = de::Vector3d(),
             angle_t angle              = 0,
             float pitch                = 0)
        : origin(origin)
        , angle(angle)
        , pitch(pitch)
    {}
    viewer_t(viewer_t const &other)
        : origin(other.origin)
        , angle(other.angle)
        , pitch(other.pitch)
    {}

    viewer_t lerp(viewer_t const &end, float pos) const {
        return viewer_t(de::lerp(origin, end.origin, pos),
                        angle + int(pos * (int(end.angle) - int(angle))),
                        de::lerp(pitch,  end.pitch,  pos));
    }
};

struct viewdata_t
{
    viewer_t current;
    viewer_t lastSharp[2]; ///< For smoothing.
    viewer_t latest; ///< "Sharp" values taken from here.

    /*
     * These vectors are in the DGL coordinate system, which is a left-handed
     * one (same as in the game, but Y and Z have been swapped). Anyone who uses
     * these must note that it might be necessary to fix the aspect ratio of the
     * Y axis by dividing the Y coordinate by 1.2.
     */
    de::Vector3f frontVec, upVec, sideVec; /* to the left */

    float viewCos, viewSin;

    RectRaw window, windowTarget, windowOld;
    float windowInter;
};

namespace ui {
    enum ViewPortLayer {
        Player3DViewLayer,
        ViewBorderLayer,
        HUDLayer
    };
}

DENG_EXTERN_C int      rendInfoTris;
DENG_EXTERN_C boolean  firstFrameAfterLoad;

/**
 * Register console variables.
 */
void Viewports_Register(void);

int R_FrameCount();

void R_ResetFrameCount();

/**
 * Render all view ports in the viewport grid.
 */
void R_RenderViewPorts(ui::ViewPortLayer layer);

/**
 * Render a blank view for the specified player.
 */
void R_RenderBlankView(void);

/**
 * Draw the border around the view window.
 */
void R_RenderPlayerViewBorder(void);

/// @return  Current viewport; otherwise @c NULL.
viewport_t const *R_CurrentViewPort(void);

/**
 * Set the current GL viewport.
 */
void R_UseViewPort(viewport_t const *vp);

viewdata_t const *R_ViewData(int consoleNum);

void R_UpdateViewer(int consoleNum);

void R_ResetViewer(void);

int R_NextViewer(void);

void R_ClearViewData(void);

/**
 * To be called at the beginning of a render frame to perform necessary initialization.
 */
void R_BeginFrame(void);

/**
 * Update the sharp world data by rotating the stored values of plane
 * heights and sharp camera positions.
 */
void R_NewSharpWorld(void);

/**
 * Returns @c true iff the BSP leaf is marked as visible for the current frame.
 *
 * @see R_ViewerBspLeafMarkVisible()
 */
bool R_ViewerBspLeafIsVisible(BspLeaf const &bspLeaf);

/**
 * Mark the BSP leaf as visible for the current frame.
 *
 * @see R_ViewerBspLeafIsVisible()
 */
void R_ViewerBspLeafMarkVisible(BspLeaf const &bspLeaf, bool yes = true);

/// @return  Distance in map space units between the lumobj and viewer.
double R_ViewerLumobjDistance(int idx);

/// @return  @c true if the lumobj is clipped for the viewer.
bool R_ViewerLumobjIsClipped(int idx);

/// @return  @c true if the lumobj is hidden for the viewer.
bool R_ViewerLumobjIsHidden(int idx);

/**
 * Clipping strategy:
 *
 * If culling world surfaces with the angle clipper and the viewer is not in the
 * void; use the angle clipper. Otherwise, use the BSP-based LOS algorithm.
 */
void R_ViewerClipLumobj(Lumobj *lum);

void R_ViewerClipLumobjBySight(Lumobj *lum, BspLeaf *bspLeaf);

/**
 * Attempt to set up a view grid and calculate the viewports. Set 'numCols' and
 * 'numRows' to zero to just update the viewport coordinates.
 */
boolean R_SetViewGrid(int numCols, int numRows);

void R_SetupDefaultViewWindow(int consoleNum);

/**
 * Animates the view window towards the target values.
 */
void R_ViewWindowTicker(int consoleNum, timespan_t ticLength);

#endif // DENG_CLIENT_VIEWPORTS_H
