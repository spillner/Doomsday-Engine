/** @file r_fakeradio.cpp  Faked Radiosity Lighting.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "de_base.h"
#include "render/rend_fakeradio.h"

#include <de/memoryzone.h>  /// @todo remove me
#include <de/vector1.h>     /// @todo remove me
#include <de/Log>
#include <de/Vector>
#include "world/blockmap.h"
#include "world/lineowner.h"
#include "world/map.h"
#include "ConvexSubspace"
#include "Face"
#include "MaterialAnimator"
#include "SectorCluster"
#include "Surface"
#include "Vertex"

#include "render/rend_main.h"

using namespace de;

void Rend_RadioInitForMap(Map &map)
{
    Time begunAt;

    LOG_AS("Rend_RadioInitForMap");

    map.forAllVertexs([] (Vertex &vertex)
    {
        vertex.updateShadowOffsets();
        return LoopContinue;
    });

    /// The algorithm:
    ///
    /// 1. Use the subspace blockmap to look for all the blocks that are within the line's shadow
    ///    bounding box.
    /// 2. Check the ConvexSubspaces whose sector is the same as the line.
    /// 3. If any of the shadow points are in the subspace, or any of the shadow edges cross one
    ///    of the subspace's edges (not parallel), link the line to the ConvexSubspace.
    map.forAllLines([] (Line &line)
    {
        if(line.castsShadow())
        {
            // For each side of the line.
            for(dint i = 0; i < 2; ++i)
            {
                LineSide &side = line.side(i);

                if(!side.hasSector()) continue;
                if(!side.hasSections()) continue;

                Vertex const &vtx0   = line.vertex(i);
                Vertex const &vtx1   = line.vertex(i ^ 1);
                LineOwner const &vo0 = line.vertexOwner(i)->next();
                LineOwner const &vo1 = line.vertexOwner(i ^ 1)->prev();

                AABoxd bounds = line.aaBox();

                // Use the extended points, they are wider than inoffsets.
                Vector2d const sv0 = vtx0.origin() + vo0.extendedShadowOffset();
                V2d_AddToBoxXY(bounds.arvec2, sv0.x, sv0.y);

                Vector2d const sv1 = vtx1.origin() + vo1.extendedShadowOffset();
                V2d_AddToBoxXY(bounds.arvec2, sv1.x, sv1.y);

                // Link the shadowing line to all the subspaces whose axis-aligned bounding box
                // intersects 'bounds'.
                validCount++;
                dint const localValidCount = validCount;
                line.map().subspaceBlockmap().forAllInBox(bounds, [&bounds, &side, &localValidCount] (void *object)
                {
                    auto &sub = *(ConvexSubspace *)object;
                    if(sub.validCount() != localValidCount)  // not yet processed
                    {
                        sub.setValidCount(localValidCount);
                        if(&sub.sector() == side.sectorPtr())
                        {
                            // Check the bounds.
                            AABoxd const &polyBox = sub.poly().aaBox();
                            if(!(polyBox.maxX < bounds.minX ||
                                 polyBox.minX > bounds.maxX ||
                                 polyBox.minY > bounds.maxY ||
                                 polyBox.maxY < bounds.minY))
                            {
                                sub.addShadowLine(side);
                            }
                        }
                    }
                    return LoopContinue;
                });
            }
        }
        return LoopContinue;
    });

    LOGDEV_GL_MSG("Completed in %.2f seconds") << begunAt.since();
}
