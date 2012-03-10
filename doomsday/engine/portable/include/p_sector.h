/**\file sector.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * Map sector.
 */

#ifndef LIBDENG_MAP_SECTOR_H
#define LIBDENG_MAP_SECTOR_H

#include "r_data.h"
#include "p_dmu.h"

/**
 * Update the origin of the sector according to the point defined by the center of
 * the sector's axis-aligned bounding box (which must be initialized before calling).
 *
 * @param sector  Sector instance.
 */
void Sector_UpdateOrigin(Sector* sector);

/**
 * Get the value of a sector property, selected by DMU_* name.
 *
 * @param sector  Sector instance.
 */
int Sector_GetProperty(const Sector* sector, setargs_t* args);

/**
 * Update the sector property, selected by DMU_* name.
 *
 * @param sector  Sector instance.
 */
int Sector_SetProperty(Sector* sector, const setargs_t* args);

#endif /* LIBDENG_MAP_SECTOR_H */
