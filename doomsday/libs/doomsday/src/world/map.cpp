/** @file world.cpp  Base for world maps.
 *
 * @authors Copyright � 2014-2015 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/world/map.h"
#include "doomsday/EntityDatabase"

using namespace de;

namespace world {

DE_PIMPL(Map)
, DE_OBSERVES(Record, Deletion)
{
    EntityDatabase entityDatabase;
    res::MapManifest *manifest = nullptr;  ///< Not owned, may be @c nullptr.

    Impl(Public *i) : Base(i)
    {}

    ~Impl()
    {
        DE_NOTIFY_PUBLIC(Deletion, i) i->mapBeingDeleted(self());
    }

    void recordBeingDeleted(Record &record)
    {
        // The manifest is not owned by us, it may be deleted by others.
        if (manifest == &record)
        {
            manifest = nullptr;
        }
    }

    DE_PIMPL_AUDIENCE(Deletion)
};

DE_AUDIENCE_METHOD(Map, Deletion)

Map::Map(res::MapManifest *manifest) : d(new Impl(this))
{
    setManifest(manifest);
}

Map::~Map()
{}

String Map::id() const
{
    if (!hasManifest()) return "";
    return manifest().gets("id");
}

bool Map::hasManifest() const
{
    return d->manifest != nullptr;
}

res::MapManifest &Map::manifest() const
{
    if (hasManifest())
    {
        DE_ASSERT(d->manifest != nullptr);
        return *d->manifest;
    }
    /// @throw MissingResourceManifestError  No associated resource manifest.
    throw MissingResourceManifestError("Map::manifest", "No associated resource manifest");
}

void Map::setManifest(res::MapManifest *newManifest)
{
    if (d->manifest) d->manifest->audienceForDeletion() -= d;

    d->manifest = newManifest;

    if (d->manifest) d->manifest->audienceForDeletion() += d;
}

EntityDatabase &Map::entityDatabase() const
{
    return d->entityDatabase;
}

void Map::serializeInternalState(Writer &) const
{}

void Map::deserializeInternalState(Reader &, const IThinkerMapping &)
{}

}  // namespace world
