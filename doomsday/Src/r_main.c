
//**************************************************************************
//**
//** R_MAIN.C
//**
//** The refresh daemon has the highest-level rendering code.
//** The view window is handled by refresh. The more specialized
//** rendering code in rend_*.c does things inside the view window.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <math.h>
#include <assert.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_render.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_audio.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// $smoothplane: Maximum speed for a smoothed plane.
#define MAX_SMOOTH_PLANE_MOVE	(64*FRACUNIT)

// TYPES -------------------------------------------------------------------

typedef struct viewer_s {
	fixed_t x, y, z;
	angle_t angle;
	float   pitch;
} viewer_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void    R_InitSkyMap(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     viewangleoffset = 0;
int     validcount = 1;			// increment every time a check is made
int     framecount;				// just for profiling purposes
int     rend_info_tris = 0;
int     rend_camera_smooth = true;	// smoothed by default
fixed_t viewx, viewy, viewz;
float   viewfrontvec[3], viewupvec[3], viewsidevec[3];
fixed_t viewxOffset = 0, viewyOffset = 0, viewzOffset = 0;
angle_t viewangle;
float   viewpitch;				// player->lookdir, global version
fixed_t viewcos, viewsin;
ddplayer_t *viewplayer;
boolean setsizeneeded;

// Precalculated math tables.
fixed_t *finecosine = &finesine[FINEANGLES / 4];

int     extralight;				// bumped light from gun blasts

int     skyflatnum;
char    skyflatname[9] = "F_SKY";

double  lastSharpFrameTime;

int     sharpWorldUpdated;		// Set to true after game ticker has been called.

float   frameTimePos;			// 0...1: fractional part for sharp game tics
boolean resyncFrameTimePos = true;

int		loadInStartupMode = true;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static viewer_t lastSharpView[2];
static boolean resetNextViewer = true;

static boolean showFrameTimePos = false;

// BSP cvars.
static int bspBuild = true;
static int bspCache = true;
static int bspFactor = 7;

// CODE --------------------------------------------------------------------

/*
 * Register console variables.
 */
void R_Register(void)
{
	C_VAR_BYTE("rend-info-tris", &rend_info_tris, 0, 0, 1,
			   "1=Print triangle count after rendering a frame.");

	C_VAR_BYTE("rend-info-frametime", &showFrameTimePos, 0, 0, 1,
			   "1=Print frame time offsets.");
	
	C_VAR_INT("rend-camera-smooth", &rend_camera_smooth, 0, 0, 1,
			  "1=Filter camera movement between game tics.");

	C_VAR_INT("bsp-build", &bspBuild, 0, 0, 1,
			  "1=Build GL nodes when loading a map.");

	C_VAR_INT("bsp-cache", &bspCache, 0, 0, 1,
			  "1=Load generated GL nodes data from the bspcache directory.\n"
			  "0=Always generate new GL data.");

	C_VAR_INT("bsp-factor", &bspFactor, CVF_NO_MAX, 0, 0,
			  "glBSP: changes the cost assigned to SEG splits (default: 7).");

	C_VAR_INT("con-show-during-setup", &loadInStartupMode, 0, 0, 1,
			  "1=Show console when a map is being loaded.");
}

/*
 * The skyflat is the special flat used for surfaces that should show
 * a view of the sky.
 */
void R_InitSkyMap(void)
{
	skyflatnum = R_FlatNumForName(skyflatname);
}

//===========================================================================
// R_ViewWindow
//  Don't really change anything here, because i might be in the middle of
//  a refresh.  The change will take effect next refresh.
//===========================================================================
void R_ViewWindow(int x, int y, int w, int h)
{
	viewwindowx = x;
	viewwindowy = y;
	viewwidth = w;
	viewheight = h;
}

//===========================================================================
// R_Init
//  One-time initialization of the refresh daemon. Called by DD_Main.
//  GL has not yet been inited.
//===========================================================================
void R_Init(void)
{
	R_InitData();
	// viewwidth / viewheight / detailLevel are set by the defaults
	R_ViewWindow(0, 0, 320, 200);
	R_InitSprites();
	R_InitModels();
	R_InitSkyMap();
	R_InitTranslationTables();
	// Call the game DLL's refresh initialization, if necessary.
	if(gx.R_Init)
		gx.R_Init();
	Rend_Init();
	framecount = 0;
	R_InitViewBorder();
	Def_PostInit();
}

//===========================================================================
// R_Update
//  Re-initialize almost everything.
//===========================================================================
void R_Update(void)
{
	int     i;

	// Stop playing sounds and music.
	Demo_StopPlayback();
	S_Reset();

	// Go back to startup-screen mode.
	Con_StartupInit();
	GL_TotalReset(true, false);
	GL_TotalReset(false, false);	// Bring GL back online (no lightmaps yet).
	R_UpdateData();
	R_InitSprites();			// Fully reinitialize sprites.
	R_InitSkyMap();
	R_UpdateTranslationTables();
	// Re-read definitions.
	Def_Read();
	// Now that we've read the defs, we can load lightmaps.
	GL_LoadSystemTextures(true);
	Def_PostInit();
	R_InitModels();				// Defs might've changed.
	for(i = 0; i < DDMAXPLAYERS; i++)
	{
		// States have changed, the states are unknown.
		players[i].psprites[0].stateptr = players[i].psprites[1].stateptr =
			NULL;
	}
	// The rendeling lists have persistent data that has changed during
	// the re-initialization.
	RL_DeleteLists();
	// Back to the game.
	Con_StartupDone();

#if _DEBUG
	Z_CheckHeap();
#endif
}

//===========================================================================
// R_Shutdown
//  Shutdown the refresh daemon.
//===========================================================================
void R_Shutdown(void)
{
	R_ShutdownModels();
	R_ShutdownData();
	// Most allocated memory goes down with the zone.
}

//===========================================================================
// R_ResetViewer
//===========================================================================
void R_ResetViewer(void)
{
	resetNextViewer = true;
}

//===========================================================================
// R_InterpolateViewer
//===========================================================================
void R_InterpolateViewer(viewer_t * start, viewer_t * end, float pos,
						 viewer_t * out)
{
	float   inv = 1 - pos;

	out->x = inv * start->x + pos * end->x;
	out->y = inv * start->y + pos * end->y;
	out->z = inv * start->z + pos * end->z;
//	out->angle = start->angle + pos * ((int) end->angle - (int) start->angle);
//	out->pitch = inv * start->pitch + pos * end->pitch;
}

//===========================================================================
// R_SetViewPos
//===========================================================================
void R_SetViewPos(viewer_t * v)
{
	viewx = v->x;
	viewy = v->y;
	viewz = v->z;
	viewangle = v->angle;
	viewpitch = v->pitch;
}

//===========================================================================
// R_CheckViewerLimits
//  The components whose difference is too large for interpolation will be 
//  snapped to the sharp values.
//===========================================================================
void R_CheckViewerLimits(viewer_t * src, viewer_t * dst)
{
#define MAXMOVE	(FRACUNIT*32)
	if(abs(dst->x - src->x) > MAXMOVE || abs(dst->y - src->y) > MAXMOVE)
	{
		src->x = dst->x;
		src->y = dst->y;
		src->z = dst->z;
	}
	if(abs((int) dst->angle - (int) src->angle) >= ANGLE_45)
		src->angle = dst->angle;
}

//===========================================================================
// R_SetupFrame
//  Prepare rendering the view of the given player.
//  Also handles smoothing of camera and plane movement.
//===========================================================================
void R_SetupFrame(ddplayer_t *player)
{
	int     tableAngle, i;
	float   yawRad, pitchRad;
	double  nowTime;
	viewer_t sharpView, smoothView;
	sector_t *sector;
	boolean justResynced = false;

	// Reset the DGL triangle counter.
	gl.GetInteger(DGL_POLY_COUNT);

	viewplayer = player;

	sharpView.angle = player->clAngle + viewangleoffset;
	sharpView.pitch = player->clLookDir;
	sharpView.x = player->mo->x + viewxOffset;
	sharpView.y = player->mo->y + viewyOffset;
	sharpView.z = player->viewz + viewzOffset;

	// Check that the viewz doesn't go too high or low.
	if(sharpView.z > player->mo->ceilingz - 4 * FRACUNIT)
	{
		sharpView.z = player->mo->ceilingz - 4 * FRACUNIT;
	}
	if(sharpView.z < player->mo->floorz + 4 * FRACUNIT)
	{
		sharpView.z = player->mo->floorz + 4 * FRACUNIT;
	}

	// Camera smoothing is only enabled if the frame rate is above 35.
	if(!rend_camera_smooth || resetNextViewer) // || DD_GetFrameRate() < 35)
	{
		resetNextViewer = false;

		// Just view from the sharp position.
		R_SetViewPos(&sharpView);
		frameTimePos = 0;
		lastSharpFrameTime = Sys_GetTimef();
		memcpy(&lastSharpView[0], &sharpView, sizeof(sharpView));
		memcpy(&lastSharpView[1], &sharpView, sizeof(sharpView));

		// $smoothplane: Reset the plane height trackers.
		for(i = 0; i < numsectors; i++)
		{
			secinfo[i].visceiloffset = secinfo[i].visflooroffset = 0;

			// Reset the old Z values.
			sector = SECTOR_PTR(i);
			secinfo[i].oldfloor[0] = secinfo[i].oldfloor[1] =
				sector->floorheight;
			secinfo[i].oldceil[0] = secinfo[i].oldceil[1] =
				sector->ceilingheight;
		}
	}
	// While the game is paused there is no need to calculate any
	// time offsets or interpolated camera positions.
	else if(!clientPaused)
	{
		static timespan_t oldTime = 0;
		nowTime = Sys_GetTimef();
		if(sharpWorldUpdated)
		{
			extern int numSharpTics;

			sharpWorldUpdated = false;

			frameTimePos -= numSharpTics;
			numSharpTics = 0;

			// The game tic has changed, which means we have an
			// updated sharp camera position. However, the position is
			// at the beginning of the tic and we are most likely not
			// at a sharp tic boundary, in time. We will update
			// lastSharpFrameTime so it tells the time of the new
			// sharp position, and then move the viewer positions one
			// step back in the buffer.  The effect of this is that
			// [0] is the previous sharp position and [1] is the
			// current one.

			memcpy(&lastSharpView[0], &lastSharpView[1], sizeof(viewer_t));
			memcpy(&lastSharpView[1], &sharpView, sizeof(sharpView));
			R_CheckViewerLimits(lastSharpView, &sharpView);

			// $smoothplane: Roll the height tracker buffers.
			for(i = 0; i < numsectors; i++)
			{
				sector = SECTOR_PTR(i);
				secinfo[i].oldfloor[0] = secinfo[i].oldfloor[1];
				secinfo[i].oldfloor[1] = sector->floorheight;

				if(abs(secinfo[i].oldfloor[0] - secinfo[i].oldfloor[1]) >=
				   MAX_SMOOTH_PLANE_MOVE)
				{
					// Too fast: make an instantaneous jump.
					secinfo[i].oldfloor[0] = secinfo[i].oldfloor[1];
				}

				secinfo[i].oldceil[0] = secinfo[i].oldceil[1];
				secinfo[i].oldceil[1] = sector->ceilingheight;

				if(abs(secinfo[i].oldceil[0] - secinfo[i].oldceil[1]) >=
				   MAX_SMOOTH_PLANE_MOVE)
				{
					// Too fast: make an instantaneous jump.
					secinfo[i].oldceil[0] = secinfo[i].oldceil[1];
				}
			}
			
			// If the frametime gets too far from the sharp times, it
			// will be forced back into the correct range.  This
			// doesn't happen during normal gameplay, only when there
			// has been an abnormal skip in the timing (tics without
			// frames or visa versa).
			if(resyncFrameTimePos || frameTimePos > 3)
			{
				resyncFrameTimePos = false;
				VERBOSE(Con_Message("Resyncing frametime...\n"));
				frameTimePos = 0; //M_CycleIntoRange(frameTimePos, 1);
				justResynced = true;
			}
		}

		// Calculate the smoothed camera position, which is somewhere
		// between the previous and current sharp positions. This
		// introduces a slight delay (max. 1/35 sec) to the movement
		// of the smoothed camera.

		if(!justResynced)
			frameTimePos += nowTime - oldTime;

		oldTime = nowTime;

		R_InterpolateViewer(lastSharpView, &sharpView, frameTimePos,
							&smoothView);

		smoothView.angle = sharpView.angle;
		smoothView.pitch = sharpView.pitch;
		R_SetViewPos(&smoothView);

#if 0
		// The Rdx and Rdy should stay constant when moving.
		{
			static double oldtime = 0;
			static fixed_t oldx, oldy;
			fprintf(outFile, "F=%.3f dt=%-5.3f dx=%-5.3f dy=%-5.3f "
					"Rdx=%-5.3f Rdy=%-5.3f\n", 
					frameTimePos,
					nowTime - oldtime,
					FIX2FLT(smoothView.x - oldx),
					FIX2FLT(smoothView.y - oldy),
					FIX2FLT(smoothView.x - oldx) / (nowTime - oldtime),
					FIX2FLT(smoothView.y - oldy) / (nowTime - oldtime));
			oldx = smoothView.x;
			oldy = smoothView.y;
			oldtime = nowTime;
		}
#endif

		if(showFrameTimePos)
		{
			Con_Printf("frametime = %f\n", frameTimePos);
		}
		
		// $smoothplane: Set the visible offsets.
		for(i = 0; i < numsectors; i++)
		{
			sector = SECTOR_PTR(i);

			secinfo[i].visflooroffset =
				FIX2FLT(secinfo[i].oldfloor[0] * (1 - frameTimePos) +
						sector->floorheight * frameTimePos -
						sector->floorheight);

			secinfo[i].visceiloffset =
				FIX2FLT(secinfo[i].oldceil[0] * (1 - frameTimePos) +
						sector->ceilingheight * frameTimePos -
						sector->ceilingheight);
		}
	}

	extralight = player->extralight;
	tableAngle = viewangle >> ANGLETOFINESHIFT;
	viewsin = finesine[tableAngle];
	viewcos = finecosine[tableAngle];
	validcount++;
	if(BorderNeedRefresh)
	{
		R_DrawViewBorder();
		BorderNeedRefresh = false;
		BorderTopRefresh = false;
		UpdateState |= I_FULLSCRN;
	}
	if(BorderTopRefresh)
	{
		if(viewwindowx > 0)
		{
			R_DrawTopBorder();
		}
		BorderTopRefresh = false;
		UpdateState |= I_MESSAGES;
	}

	// Calculate the front, up and side unit vectors.
	// The vectors are in the DGL coordinate system, which is a left-handed
	// one (same as in the game, but Y and Z have been swapped). Anyone
	// who uses these must note that it might be necessary to fix the aspect
	// ratio of the Y axis by dividing the Y coordinate by 1.2.
	yawRad = viewangle / (float) ANGLE_MAX *2 * PI;

	pitchRad = viewpitch * 85 / 110.f / 180 * PI;

	// The front vector.
	viewfrontvec[VX] = cos(yawRad) * cos(pitchRad);
	viewfrontvec[VZ] = sin(yawRad) * cos(pitchRad);
	viewfrontvec[VY] = sin(pitchRad);

	// The up vector.
	viewupvec[VX] = -cos(yawRad) * sin(pitchRad);
	viewupvec[VZ] = -sin(yawRad) * sin(pitchRad);
	viewupvec[VY] = cos(pitchRad);

	// The side vector is the cross product of the front and up vectors.
	M_CrossProduct(viewfrontvec, viewupvec, viewsidevec);
}

//===========================================================================
// R_RenderPlayerView
//  Draw the view of the player inside the view window.
//===========================================================================
void R_RenderPlayerView(ddplayer_t *player)
{
	extern boolean firstFrameAfterLoad;
	extern int psp3d, model_tri_count;
	int     i, oldFlags;

	if(firstFrameAfterLoad)
	{
		// Don't let the clock run yet.  There may be some texture
		// loading still left to do that we have been unable to
		// predetermine.
		firstFrameAfterLoad = false;
		DD_ResetTimer();
	}

	// Setup for rendering the frame.
	R_SetupFrame(player);
	R_ClearSprites();
	R_ProjectPlayerSprites();	// Only if 3D models exists for them.
	PG_InitForNewFrame();

	// Hide the viewplayer's mobj.
	oldFlags = player->mo->ddflags;
	player->mo->ddflags |= DDMF_DONTDRAW;

	// Go to wireframe mode?
	if(renderWireframe)
		gl.Enable(DGL_WIREFRAME_MODE);

	// GL is in 3D transformation state only during the frame.
	GL_SwitchTo3DState(true);
	Rend_RenderMap();
	// Orthogonal projection to the view window.
	GL_Restore2DState(1);
	Rend_DrawPlayerSprites();	// If the 2D versions are needed.
	// Fullscreen viewport.
	GL_Restore2DState(2);
	// Do we need to render any 3D psprites?
	if(psp3d)
	{
		GL_SwitchTo3DState(false);
		Rend_Draw3DPlayerSprites();
		GL_Restore2DState(2);	// Restore viewport.
	}
	// Original matrices and state: back to normal 2D.
	GL_Restore2DState(3);

	// Back from wireframe mode?
	if(renderWireframe)
		gl.Disable(DGL_WIREFRAME_MODE);

	// Now we can show the viewplayer's mobj again.
	player->mo->ddflags = oldFlags;

	// Should we be counting triangles?
	if(rend_info_tris)
	{
		// This count includes all triangles drawn since R_SetupFrame.
		i = gl.GetInteger(DGL_POLY_COUNT);
		Con_Printf("Tris: %-4i (Mdl=%-4i)\n", i, model_tri_count);
		model_tri_count = 0;
	}
	if(rend_info_lums)
	{
		Con_Printf("LumObjs: %-4i\n", numLuminous);
	}
}
