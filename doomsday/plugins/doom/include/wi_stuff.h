/** @file wi_stuff.h  DOOM specific intermission screens.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1993-1996 id Software, Inc.
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

#ifndef LIBDOOM_WI_STUFF_H
#define LIBDOOM_WI_STUFF_H

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#include "d_player.h"

// Global locations
#define WI_TITLEY               (2)
#define WI_SPACINGY             (33)

// Single-player stuff
#define SP_STATSX               (50)
#define SP_STATSY               (50)
#define SP_TIMEX                (16)
#define SP_TIMEY                (SCREENHEIGHT-32)

// Net game stuff
#define NG_STATSY               (50)
#define NG_STATSX               (32)
#define NG_SPACINGX             (64)

// Deathmatch stuff
#define DM_MATRIXX              (42)
#define DM_MATRIXY              (68)
#define DM_SPACINGX             (40)
#define DM_TOTALSX              (269)
#define DM_KILLERSX             (10)
#define DM_KILLERSY             (100)
#define DM_VICTIMSX             (5)
#define DM_VICTIMSY             (50)

// States for single-player
#define SP_KILLS                (0)
#define SP_ITEMS                (2)
#define SP_SECRET               (4)
#define SP_FRAGS                (6)
#define SP_TIME                 (8)
#define SP_PAR                  (ST_TIME)
#define SP_PAUSE                (1)

#define SHOWNEXTLOCDELAY        (4) // In seconds.

// States for the intermission
typedef enum {
    ILS_NONE = -1,
    ILS_SHOW_STATS,
    ILS_SHOW_NEXTMAP
} interludestate_t;

//
// INTERMISSION
// Structure passed e.g. to WI_Init(wb)
//
typedef struct {
    dd_bool         inGame; // Whether the player is in game.

    // Player stats, kills, collected items etc.
    int             kills;
    int             items;
    int             secret;
    int             time;
    int             frags[MAXPLAYERS];
    int             score; // Current score on entry, modified on return.
} wbplayerstruct_t;

typedef struct {
    uint            episode;
    dd_bool         didSecret; // If true, splash the secret level.
    uint            currentMap, nextMap; // This and next maps.
    int             maxKills;
    int             maxItems;
    int             maxSecret;
    int             maxFrags;
    int             parTime;
    int             pNum; // Index of this player in game.
    wbplayerstruct_t plyr[MAXPLAYERS];
} wbstartstruct_t;

#ifdef __cplusplus
extern "C" {
#endif

/// To be called to register the console commands and variables of this module.
void WI_Register(void);

/**
 * Perform setup for an intermission.
 */
void WI_Init(wbstartstruct_t *wbstartstruct);

void WI_Shutdown(void);

/**
 * Process game tic for the intermission.
 *
 * @note Handles user input due to timing issues in netgames.
 */
void WI_Ticker(void);

/**
 * Draw the intermission.
 */
void WI_Drawer(void);

/**
 * Change the current intermission state.
 */
void WI_SetState(interludestate_t st);

/**
 * End the current intermission.
 */
void WI_End(void);

/**
 * Skip to the next state in the intermission.
 */
void IN_SkipToNext(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDOOM_WI_STUFF_H
