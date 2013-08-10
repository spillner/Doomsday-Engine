/** @file segment.h World map line segment.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_WORLD_SEGMENT_H
#define DENG_WORLD_SEGMENT_H

#include <de/Vector>

#include "MapElement"
#include "HEdge"
#include "Line"

#ifdef __CLIENT__
#  include "BiasSurface"
#endif

/**
 * @ingroup world
 */
class Segment : public de::MapElement
#ifdef __CLIENT__
, public BiasSurface
#endif
{
    DENG2_NO_COPY  (Segment)
    DENG2_NO_ASSIGN(Segment)

public:
    /// Required line attribution is missing. @ingroup errors
    DENG2_ERROR(MissingLineSideError);

#ifdef __CLIENT__

    enum Flag
    {
        FacingFront = 0x1
    };
    Q_DECLARE_FLAGS(Flags, Flag)

#endif

public:
    Segment(de::HEdge &hedge, Line::Side *lineSide = 0);

    //de::HEdge &hedge() const;

    //inline Vertex &from() const { return hedge().vertex(); }

    //inline Vertex &to() const { return hedge().twin().vertex(); }

    /**
     * Returns the point on the line segment which lies at the exact center of
     * the two vertexes.
     */
    //inline de::Vector2d center() const { return (from().origin() + to().origin()) / 2; }

    /**
     * Returns @c true iff a polygon attributed to a BSP leaf is associated
     * with the line segment.
     *
     * @see HEdge::hasFace(), Face::mapElement()
     */
    //inline bool hasBspLeaf() const {
    //    return hedge().hasFace() && hedge().face().mapElement() != 0;
    //}

    /**
     * Convenience accessor which returns the BspLeaf attributed to the polygon
     * of which the line segment is a part.
     *
     * @see hasBspLeaf(), Face::mapElement()
     */
    //inline BspLeaf &bspLeaf() const {
    //    return *hedge().face().mapElement()->as<BspLeaf>();
    //}

    /**
     * Convenience accessor which returns the Sector attributed to the BspLeaf
     * attributed to the polygon of which the line segment is a part.
     *
     * @see BspLeaf::sector()
     */
    //Sector &sector() const;

    /**
     * Convenience accessor which returns a pointer to the Sector attributed to
     * the BspLeaf attributed to the polygon of which the line segment is a part.
     *
     * @see hasBspLeaf(), BspLeaf::sectorPtr()
     */
    //Sector *sectorPtr() const;

    /**
     * Returns @c true iff a Line::Side is attributed to the line segment.
     */
    bool hasLineSide() const;

    /**
     * Returns the Line::Side attributed to the line segment.
     *
     * @see hasLineSide()
     */
    Line::Side &lineSide() const;

    /**
     * Convenient accessor method for returning the line of the Line::Side
     * attributed to the line segment.
     *
     * @see hasLineSide(), lineSide()
     */
    inline Line &line() const { return lineSide().line(); }

    /**
     * Returns the distance along the attributed map line at which the from vertex
     * vertex occurs. If the segment is not attributed to a map line then @c 0 is
     * returned.
     *
     * @see hasLineSide(), lineSide()
     */
    coord_t lineSideOffset() const;

    /// @todo Refactor away.
    void setLineSideOffset(coord_t newOffset);

    /**
     * Returns the accurate length of the line segment, from the 'from' vertex to
     * the 'to' vertex in map coordinate space units.
     */
    coord_t length() const;

    /// @todo Refactor away.
    void setLength(coord_t newLength);

#ifdef __CLIENT__

    /**
     * Returns the current value of the flags of the line segment.
     */
    Flags flags() const;

    /**
     * Returns @c true iff the line segment is flagged @a flagsToTest.
     */
    inline bool isFlagged(int flagsToTest) const { return (flags() & flagsToTest) != 0; }

    /**
     * Change the line segment's flags.
     *
     * @param flagsToChange  Flags to change the value of.
     * @param operation      Logical operation to perform on the flags.
     */
    void setFlags(Flags flagsToChange, de::FlagOp operation = de::SetFlags);

    /**
     * Perform bias lighting for the supplied geometry.
     *
     * @important It is assumed there are least @em four elements!
     *
     * @param group        Geometry group identifier.
     * @param posCoords    World coordinates for each vertex.
     * @param colorCoords  Final lighting values will be written here.
     */
    void lightBiasPoly(int group, de::Vector3f const *posCoords,
                       de::Vector4f *colorCoords);

    void updateBiasAfterGeometryMove(int group);

    BiasTracker *biasTracker(int group);

#endif // __CLIENT__

private:
    DENG2_PRIVATE(d)
};

#ifdef __CLIENT__
Q_DECLARE_OPERATORS_FOR_FLAGS(Segment::Flags)
#endif

#endif // DENG_WORLD_SEGMENT_H
