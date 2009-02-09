/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * p_player.h: Common playsim routines relating to players.
 */

#ifndef __COMMON_PLAYER_H__
#define __COMMON_PLAYER_H__

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
# include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#if __JHEXEN__
void        P_InitPlayerClassInfo(void);
#endif

int         P_GetPlayerNum(player_t* player);
int         P_GetPlayerCheats(player_t* player);

weapontype_t P_PlayerFindWeapon(player_t* player, boolean next);
weapontype_t P_MaybeChangeWeapon(player_t* player, weapontype_t weapon,
                                 ammotype_t ammo, boolean force);
boolean     P_CheckAmmo(player_t* player);
void        P_ShotAmmo(player_t* player);

#if __JHEXEN__
void        P_PlayerChangeClass(player_t* player, playerclass_t newClass);
#endif

void        P_SetMessage(player_t* player, char* msg, boolean noHide);
#if __JHEXEN__ || __JSTRIFE__
void        P_SetYellowMessage(player_t* player, char* msg, boolean noHide);
#endif

boolean     P_IsCamera(mobj_t* mo);
void        P_PlayerThinkCamera(player_t* player);
#endif
