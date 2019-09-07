/** @file face.h Mesh Geometry Face.
 *
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_DATA_MESH_FACE_H
#define DE_DATA_MESH_FACE_H

#include <de/legacy/aabox.h>

#include <de/Vector>

#include "Mesh"

namespace de {

/**
 * Mesh face geometry.
 *
 * @ingroup data
 */
class Face : public MeshElement
{
public: /// @todo make private:
    /// Total number of half-edge's in the face geometry.
    dint _hedgeCount;

public:
    explicit Face(Mesh &mesh);

    /**
     * Total number of half-edges in the face geometry.
     */
    dint hedgeCount() const;

    /**
     * Returns a pointer to the first half-edge in the face geometry (note that
     * half-edges are sorted in a clockwise order). May return @c 0 if there is
     * no half-edge linked to the face.
     */
    HEdge *hedge() const;

    /**
     * Change the first half-edge in the face geometry.
     */
    void setHEdge(const HEdge *newHEdge);

    /**
     * Returns the axis-aligned bounding box which encompases all the vertexes
     * which define the face geometry.
     */
    const AABoxd &bounds() const;

    /**
     * Update the face geometry's axis-aligned bounding box to encompass all vertexes.
     */
    void updateBounds();

    /**
     * Returns the point described by the average origin coordinates of all the
     * vertexes which define the geometry.
     */
    const Vec2d &center() const;

    /**
     * Update the center point of the geometry.
     *
     * @pre Axis-aligned bounding box must have been initialized.
     */
    void updateCenter();

    /**
     * Determines whether the face geometry is currently convex.
     *
     * @note Due to the potential computational complexity of determining convexity
     * this should be called sparingly/only when necessary.
     *
     * @todo Cache this result.
     */
    bool isConvex() const;

    /**
     * Returns a textual human-readable description/representation of the face
     * suitable for writing to the application's output log.
     */
    String description() const;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // DE_DATA_MESH_FACE_H
