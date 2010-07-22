/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
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
 * The "In Fine" finale sequence system.
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_defs.h"
#include "de_graphics.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_network.h"
#include "de_audio.h"
#include "de_infine.h"
#include "de_misc.h"

#include "finaleinterpreter.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct fi_object_collection_s {
    uint            num;
    fi_object_t**   vector;
} fi_object_collection_t;

typedef struct fi_state_s {
    finale_mode_t   mode;

    // Interactive script interpreter.
    finaleinterpreter_t _interpreter;

    // Known symbols (to this script).
    fi_namespace_t  _namespace;

    int             timer;
    material_t*     bgMaterial;
    animatorvector4_t bgColor;
    animatorvector2_t imgOffset;
    animatorvector4_t filter;
    animatorvector3_t textColor[9];

    int             initialGameState; // Game state before the script began.
    int             overlayGameState; // Overlay scripts run only in one gameMode.
    void*           extraData;
} fi_state_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(StartFinale);
D_CMD(StopFinale);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static fi_objectid_t toObjectId(fi_namespace_t* names, const char* name, fi_obtype_e type);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// Default color for Text objects.
float fiDefaultTextRGB[3] = { 1, 1, 1 };

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean inited = false;
static boolean active = false; // Interpreter active for the current stateStack top.

// Allow stretching to fill the screen at near 4:3 aspect ratios?
static byte noStretch = false;

// Finale script/state collection.
static uint numStates;
static fi_state_t** stateStack;

// Global Finale object store.
static fi_object_collection_t objects;

static void* defaultState = 0;

// CODE --------------------------------------------------------------------

/**
 * Called during pre-init to register cvars and ccmds for the finale system.
 */
void FI_Register(void)
{
    C_VAR_BYTE("finale-nostretch",  &noStretch, 0, 0, 1);

    C_CMD("startfinale",    "s",    StartFinale);
    C_CMD("startinf",       "s",    StartFinale);
    C_CMD("stopfinale",     "",     StopFinale);
    C_CMD("stopinf",        "",     StopFinale);
}

static void useColor(animator_t *color, int components)
{
    if(components == 3)
    {
        glColor3f(color[0].value, color[1].value, color[2].value);
    }
    else if(components == 4)
    {
        glColor4f(color[0].value, color[1].value, color[2].value, color[3].value);
    }
}

static void objectSetName(fi_object_t* obj, const char* name)
{
    strncpy(obj->name, name, sizeof(obj->name) - 1);
}

static void thinkObjectsInScope(fi_namespace_t* names)
{
    uint i;
    for(i = 0; i < names->num; ++i)
    {
        fi_object_t* obj = names->vector[i];
        switch(obj->type)
        {
        case FI_PIC:    FIData_PicThink((fidata_pic_t*)obj);    break;
        case FI_TEXT:   FIData_TextThink((fidata_text_t*)obj);  break;
        default: break;
        }
    }
}

static void drawObjectsInScope2(fi_namespace_t* names, fi_obtype_e type, float picOffsetX, float picOffsetY)
{
    static const vec3_t worldOrigin = { 0, 0, 0 };
    uint i;
    for(i = 0; i < names->num; ++i)
    {
        fi_object_t* obj = names->vector[i];
        if(obj->type != type)
            continue;
        switch(obj->type)
        {
        case FI_PIC:
            {
            vec3_t offset;
            V3_Set(offset, worldOrigin[VX]+picOffsetX, worldOrigin[VY]+picOffsetY, worldOrigin[VZ]);
            FIData_PicDraw((fidata_pic_t*)obj, offset);
            break;
            }
        case FI_TEXT:   FIData_TextDraw((fidata_text_t*)obj, worldOrigin); break;
        default: break;
        }
    }
}

static void drawObjectsInScope(fi_namespace_t* names, float picXOffset, float picYOffset)
{
    // Images first, then text.
    drawObjectsInScope2(names, FI_PIC, picXOffset, picYOffset);
    drawObjectsInScope2(names, FI_TEXT, 0, 0);
}

static fi_object_t* objectsAdd(fi_object_collection_t* c, fi_object_t* obj)
{
    // Link with this state.
    c->vector = Z_Realloc(c->vector, sizeof(*c->vector) * ++c->num, PU_STATIC);
    c->vector[c->num-1] = obj;
    return obj;
}

static void objectsRemove(fi_object_collection_t* c, fi_object_t* obj)
{
    uint i;
    for(i = 0; i < c->num; ++i)
    {
        fi_object_t* other = c->vector[i];
        if(other == obj)
        {
            if(i != c->num-1)
                memmove(&c->vector[i], &c->vector[i+1], sizeof(*c->vector) * (c->num-i));

            if(c->num > 1)
            {
                c->vector = Z_Realloc(c->vector, sizeof(*c->vector) * --c->num, PU_STATIC);
            }
            else
            {
                Z_Free(c->vector);
                c->vector = NULL;
                c->num = 0;
            }
            return;
        }
    }
}

static void objectsEmpty(fi_object_collection_t* c)
{
    if(c->num)
    {
        uint i;
        for(i = 0; i < c->num; ++i)
        {
            fi_object_t* obj = c->vector[i];
            switch(obj->type)
            {
            case FI_PIC: P_DestroyPic((fidata_pic_t*)obj); break;
            case FI_TEXT: P_DestroyText((fidata_text_t*)obj); break;
            }
        }
        Z_Free(c->vector);
    }
    c->vector = 0;
    c->num = 0;
}

static void destroyObjectsInScope(fi_namespace_t* names)
{
    // Delete external images, text strings etc.
    if(names->vector)
    {
        uint i;
        for(i = 0; i < names->num; ++i)
        {
            fi_object_t* obj = names->vector[i];
            objectsRemove(&objects, obj);
            switch(obj->type)
            {
            case FI_PIC:    P_DestroyPic((fidata_pic_t*)obj);   break;
            case FI_TEXT:   P_DestroyText((fidata_text_t*)obj); break;
            default:
                break;
            }
        }
        Z_Free(names->vector);
    }
    names->vector = NULL;
    names->num = 0;
}

static fi_objectid_t findIdForName(fi_namespace_t* names, const char* name)
{
    fi_objectid_t id;
    // First check all pics.
    id = toObjectId(names, name, FI_PIC);
    // Then check text objects.
    if(!id)
        id = toObjectId(names, name, FI_TEXT);
    return id;
}

static fi_objectid_t toObjectId(fi_namespace_t* names, const char* name, fi_obtype_e type)
{
    assert(name && name[0]);
    if(type == FI_NONE)
    {   // Use a priority-based search.
        return findIdForName(names, name);
    }

    {uint i;
    for(i = 0; i < names->num; ++i)
    {
        fi_object_t* obj = names->vector[i];
        if(obj->type == type && !stricmp(obj->name, name))
            return obj->id;
    }}
    return 0;
}

static fi_object_t* objectsById(fi_object_collection_t* c, fi_objectid_t id)
{
    if(id != 0)
    {
        uint i;
        for(i = 0; i < c->num; ++i)
        {
            fi_object_t* obj = c->vector[i];
            if(obj->id == id)
                return obj;
        }
    }
    return NULL;
}

/**
 * @return                  A new (unused) unique object id.
 */
static fi_objectid_t objectsUniqueId(fi_object_collection_t* c)
{
    fi_objectid_t id = 0;
    while(objectsById(c, ++id));
    return id;
}

static void stateChangeMode(fi_state_t* s, finale_mode_t mode)
{
    s->mode = mode;
}

static void stateSetExtraData(fi_state_t* s, const void* data)
{
    if(!data)
        return;
    if(!s->extraData || !(FINALE_SCRIPT_EXTRADATA_SIZE > 0))
        return;
    memcpy(s->extraData, data, FINALE_SCRIPT_EXTRADATA_SIZE);
}

static void stateSetInitialGameState(fi_state_t* s, int gameState, const void* clientState)
{
    s->initialGameState = gameState;

    if(FINALE_SCRIPT_EXTRADATA_SIZE > 0)
    {
        stateSetExtraData(s, &defaultState);
        if(clientState)
            stateSetExtraData(s, clientState);
    }

    if(s->mode == FIMODE_OVERLAY)
    {   // Overlay scripts stop when the gameMode changes.
        s->overlayGameState = gameState;
    }
}

/**
 * Clear the specified state to the default, blank state.
 */
static void stateClear(fi_state_t* s)
{
    s->timer = 0;
    s->bgMaterial = NULL; // No background material.

    destroyObjectsInScope(&s->_namespace);

    AnimatorVector4_Init(s->filter, 0, 0, 0, 0);
    AnimatorVector2_Init(s->imgOffset, 0, 0);  
    AnimatorVector4_Init(s->bgColor, 1, 1, 1, 1);
    {uint i;
    for(i = 0; i < 9; ++i)
    {
        AnimatorVector3_Init(s->textColor[i], 1, 1, 1);
    }}
}

static void stateInit(fi_state_t* s, finale_mode_t mode, int gameState, const void* clientState)
{
    stateClear(s);
    stateChangeMode(s, mode);
    stateSetInitialGameState(s, gameState, clientState);
}

static fi_state_t* newState(const char* scriptSrc)
{
    fi_state_t* s = Z_Calloc(sizeof(*s), PU_STATIC, 0);
    FinaleInterpreter_LoadScript(&s->_interpreter, scriptSrc);
    active = true;
    return s;
}

static void deleteState(fi_state_t* s)
{
    stateClear(s);
    FinaleInterpreter_ReleaseScript(&s->_interpreter);
    Z_Free(s);
}

static __inline fi_state_t* stackTop(void)
{
    return (numStates == 0? 0 : stateStack[numStates-1]);
}

static fi_state_t* stackPush(fi_state_t* s)
{
    stateStack = Z_Realloc(stateStack, sizeof(*stateStack) * ++numStates, PU_STATIC);
    stateStack[numStates-1] = s;
    return s;
}

static boolean stackPop(void)
{
    fi_state_t* s = stackTop();

    if(!s)
    {
#ifdef _DEBUG
Con_Printf("InFine: Pop in NULL state!\n");
#endif
        return false;
    }

    deleteState(s);
    
    // Should we go back to NULL?
    if(numStates > 1)
    {   // Return to previous state.
        stateStack = Z_Realloc(stateStack, sizeof(*stateStack) * --numStates, PU_STATIC);
        active = true;
    }
    else
    {
        Z_Free(stateStack); stateStack = 0;
        numStates = 0;
        active = false;
    }
    return active;
}

/**
 * Stop playing the script and go to next game state.
 */
static void scriptTerminate(fi_state_t* s)
{
    ddhook_finale_script_stop_paramaters_t p;
    int oldMode;

    if(!active)
        return;
    if(!FinaleInterpreter_CanSkip(&s->_interpreter))
        return;

#ifdef _DEBUG
    Con_Printf("Finale End: mode=%i '%.30s'\n", s->mode, s->_interpreter.script);
#endif

    oldMode = s->mode;

    memset(&p, 0, sizeof(p));
    p.initialGameState = s->initialGameState;
    p.extraData = s->extraData;

    // This'll set fi to NULL.
    stackPop();

    if(oldMode != FIMODE_LOCAL)
    {
        // Tell clients to stop the finale.
        Sv_Finale(FINF_END, 0, NULL, 0);
    }

    Plug_DoHook(HOOK_FINALE_SCRIPT_TERMINATE, oldMode, &p);
}

static uint objectIndexInNamespace(fi_namespace_t* names, fi_object_t* obj)
{
    if(obj)
    {
        uint i;
        for(i = 0; i < names->num; ++i)
        {
            fi_object_t* other = names->vector[i];
            if(other == obj)
                return i+1;
        }
    }
    return 0;
}

/**
 * Does not check if the object already exists in this scope. Assumes the caller
 * knows what they are doing.
 */
static fi_object_t* addObjectToNamespace(fi_namespace_t* names, fi_object_t* obj)
{
    names->vector = Z_Realloc(names->vector, sizeof(*names->vector) * ++names->num, PU_STATIC);
    names->vector[names->num-1] = obj;
    return obj;
}

/**
 * Assumes there is at most one reference to the obj in the scope and that the
 * caller knows what they are doing.
 */
static fi_object_t* removeObjectInNamespace(fi_namespace_t* names, fi_object_t* obj)
{
    uint idx;
    if((idx = objectIndexInNamespace(names, obj)))
    {
        idx -= 1; // Indices are 1-based.

        if(idx != names->num-1)
            memmove(&names->vector[idx], &names->vector[idx+1], sizeof(*names->vector) * (names->num-idx));

        if(names->num > 1)
        {
            names->vector = Z_Realloc(names->vector, sizeof(*names->vector) * --names->num, PU_STATIC);
        }
        else
        {
            Z_Free(names->vector);
            names->vector = NULL;
            names->num = 0;
        }
    }
    return obj;
}

static void scriptTick(fi_state_t* s)
{
    ddhook_finale_script_ticker_paramaters_t p;
    int i;

    memset(&p, 0, sizeof(p));
    p.runTick = true;
    p.canSkip = FinaleInterpreter_CanSkip(&s->_interpreter);
    p.gameState = (s->mode == FIMODE_OVERLAY? s->overlayGameState : s->initialGameState);
    p.extraData = s->extraData;

    Plug_DoHook(HOOK_FINALE_SCRIPT_TICKER, s->mode, &p);

    if(!p.runTick)
        return;

    // Test again - a call to scriptTerminate in a hook may have stopped the script.
    if(!active)
        return;

    s->timer++;

    // Interpolateable values.
    AnimatorVector4_Think(s->bgColor);
    AnimatorVector2_Think(s->imgOffset);
    AnimatorVector4_Think(s->filter);
    for(i = 0; i < 9; ++i)
        AnimatorVector3_Think(s->textColor[i]);

    thinkObjectsInScope(&s->_namespace);

    if(FinaleInterpreter_RunCommands(&s->_interpreter))
    {   // The script has ended!
        scriptTerminate(s);
    }
}

static void rotate(float angle)
{
    // Counter the VGA aspect ratio.
    glScalef(1, 200.0f / 240.0f, 1);
    glRotatef(angle, 0, 0, 1);
    glScalef(1, 240.0f / 200.0f, 1);
}

static void stateDraw(fi_state_t* s)
{
    // Draw the background.
    if(s->bgMaterial)
    {
        useColor(s->bgColor, 4);
        DGL_SetMaterial(s->bgMaterial);
        DGL_DrawRectTiled(0, 0, SCREENWIDTH, SCREENHEIGHT, 64, 64);
    }
    else if(s->bgColor[3].value > 0)
    {
        // Just clear the screen, then.
        DGL_Disable(DGL_TEXTURING);
        DGL_DrawRect(0, 0, SCREENWIDTH, SCREENHEIGHT, s->bgColor[0].value, s->bgColor[1].value, s->bgColor[2].value, s->bgColor[3].value);
        DGL_Enable(DGL_TEXTURING);
    }

    drawObjectsInScope(&s->_namespace, -s->imgOffset[0].value, -s->imgOffset[1].value);

    // Filter on top of everything. Only draw if necessary.
    if(s->filter[3].value > 0)
    {
        DGL_Disable(DGL_TEXTURING);
        useColor(s->filter, 4);

        glBegin(GL_QUADS);
            glVertex2f(0, 0);
            glVertex2f(SCREENWIDTH, 0);
            glVertex2f(SCREENWIDTH, SCREENHEIGHT);
            glVertex2f(0, SCREENHEIGHT);
        glEnd();

        DGL_Enable(DGL_TEXTURING);
    }
}

/**
 * Reset the entire InFine state stack.
 */
static void doReset(boolean doingShutdown)
{
    fi_state_t* s;
    if((s = stackTop()) && active)
    {
        if(!doingShutdown)
        {
            // The state is suspended when the PlayDemo command is used.
            // Being suspended means that InFine is currently not active, but
            // will be restored at a later time.
            if(s && FinaleInterpreter_Suspended(&s->_interpreter))
                return;
        }

        // Pop all the states.
        while(stackPop());
    }
}

static void picFrameDeleteXImage(fidata_pic_frame_t* f)
{
    DGL_DeleteTextures(1, (DGLuint*)&f->texRef.tex);
    f->texRef.tex = 0;
}

static fidata_pic_frame_t* createPicFrame(int type, int tics, void* texRef, short sound, boolean flagFlipH)
{
    fidata_pic_frame_t* f = (fidata_pic_frame_t*) Z_Malloc(sizeof(*f), PU_STATIC, 0);
    f->flags.flip = flagFlipH;
    f->type = type;
    f->tics = tics;
    switch(f->type)
    {
    case PFT_MATERIAL:  f->texRef.material = ((material_t*)texRef);     break;
    case PFT_PATCH:     f->texRef.patch = *((patchid_t*)texRef);    break;
    case PFT_RAW:       f->texRef.lump  = *((lumpnum_t*)texRef);    break;
    case PFT_XIMAGE:    f->texRef.tex = *((DGLuint*)texRef);        break;
    default:
        Con_Error("Error - InFine: unknown frame type %i.", (int)type);
    }
    f->sound = sound;
    return f;
}

static void destroyPicFrame(fidata_pic_frame_t* f)
{
    if(f->type == PFT_XIMAGE)
        picFrameDeleteXImage(f);
    Z_Free(f);
}

static fidata_pic_frame_t* picAddFrame(fidata_pic_t* p, fidata_pic_frame_t* f)
{
    p->frames = Z_Realloc(p->frames, sizeof(*p->frames) * ++p->numFrames, PU_STATIC);
    p->frames[p->numFrames-1] = f;
    return f;
}

static void picRotationOrigin(const fidata_pic_t* p, uint frame, float center[3])
{
    if(p->numFrames && frame < p->numFrames)
    {
        fidata_pic_frame_t* f = p->frames[frame];
        switch(f->type)
        {
        case PFT_PATCH:
            {
            patchinfo_t info;
            if(R_GetPatchInfo(f->texRef.patch, &info))
            {
                /// \fixme what about extraOffset?
                center[VX] = info.width / 2 - info.offset;
                center[VY] = info.height / 2 - info.topOffset;
                center[VZ] = 0;
            }
            else
            {
                center[VX] = center[VY] = center[VZ] = 0;
            }
            break;
            }
        case PFT_RAW:
        case PFT_XIMAGE:
            center[VX] = SCREENWIDTH/2;
            center[VY] = SCREENHEIGHT/2;
            center[VZ] = 0;
            break;
        case PFT_MATERIAL:
            center[VX] = center[VY] = center[VZ] = 0;
            break;
        }
    }
    else
    {
        center[VX] = center[VY] = .5f;
        center[VZ] = 0;
    }

    center[VX] *= p->scale[VX].value;
    center[VY] *= p->scale[VY].value;
    center[VZ] *= p->scale[VZ].value;
}

void FIObject_Destructor(fi_object_t* obj)
{
    assert(obj);
    // Destroy all references to this object in all scopes.
    if(numStates)
    {
        uint i;
        for(i = 0; i < numStates; ++i)
        {
            fi_state_t* s = stateStack[i];
            removeObjectInNamespace(&s->_namespace, obj);
        }
    }
    Z_Free(obj);
}

fidata_pic_t* P_CreatePic(fi_objectid_t id, const char* name)
{
    fidata_pic_t* p = Z_Calloc(sizeof(*p), PU_STATIC, 0);

    p->id = id;
    p->type = FI_PIC;
    objectSetName((fi_object_t*)p, name);
    AnimatorVector4_Init(p->color, 1, 1, 1, 1);
    AnimatorVector3_Init(p->scale, 1, 1, 1);

    FIData_PicClearAnimation(p);
    return p;
}

void P_DestroyPic(fidata_pic_t* pic)
{
    assert(pic);
    FIData_PicClearAnimation(pic);
    // Call parent destructor.
    FIObject_Destructor((fi_object_t*)pic);
}

fidata_text_t* P_CreateText(fi_objectid_t id, const char* name)
{
#define LEADING             (11.f/7-1)

    fidata_text_t* t = Z_Calloc(sizeof(*t), PU_STATIC, 0);
    float rgba[4];

    {int i;
    for(i = 0; i < 3; ++i)
        rgba[i] = fiDefaultTextRGB[i];
    }
    rgba[CA] = 1; // Opaque.

    // Initialize it.
    t->id = id;
    t->type = FI_TEXT;
    t->textFlags = DTF_ALIGN_TOPLEFT|DTF_NO_EFFECTS;
    objectSetName((fi_object_t*)t, name);
    AnimatorVector4_Init(t->color, rgba[CR], rgba[CG], rgba[CB], rgba[CA]);
    AnimatorVector3_Init(t->scale, 1, 1, 1);

    t->wait = 3;
    t->font = R_CompositeFontNumForName("a");
    t->lineheight = LEADING;

    return t;

#undef LEADING
}

void P_DestroyText(fidata_text_t* text)
{
    assert(text);
    if(text->text)
    {   // Free the memory allocated for the text string.
        Z_Free(text->text);
        text->text = NULL;
    }
    // Call parent destructor.
    FIObject_Destructor((fi_object_t*)text);
}

void FIObject_Think(fi_object_t* obj)
{
    assert(obj);

    AnimatorVector3_Think(obj->pos);
    AnimatorVector3_Think(obj->scale);
    Animator_Think(&obj->angle);
}

boolean FI_Active(void)
{
    fi_state_t* s;
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_Active: Not initialized yet!\n");
#endif
        return false;
    }
    if((s = stackTop()))
    {
        return active;
    }
    return false;
}

void FI_Init(void)
{
    if(inited)
        return; // Already been here.
    memset(&objects, 0, sizeof(objects));
    numStates = 0;
    stateStack = 0;

    inited = true;
}

void FI_Shutdown(void)
{
    if(!inited)
        return; // Huh?
    doReset(true);
    // Garbage collection.
    objectsEmpty(&objects);
    inited = false;
}

boolean FI_CmdExecuted(void)
{
    fi_state_t* s;
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_CmdExecuted: Not initialized yet!\n");
#endif
        return false;
    }
    if((s = stackTop()))
    {
        return FinaleInterpreter_CommandExecuted(&s->_interpreter);
    }
    return false;
}

void FI_Reset(void)
{
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_Reset: Not initialized yet!\n");
#endif
        return;
    }
    doReset(false);
}

/**
 * Start playing the given script.
 */
boolean FI_ScriptBegin(const char* scriptSrc, finale_mode_t mode, int gameState, void* clientState)
{
    fi_state_t* s;

    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_ScriptBegin: Not initialized yet!\n");
#endif
        return false;
    }
    if(!scriptSrc || !scriptSrc[0])
    {
#ifdef _DEBUG
        Con_Printf("FI_ScriptBegin: Warning, attempt to play empty script (mode=%i).\n", (int)mode);
#endif
        return false;
    }

    if(mode == FIMODE_LOCAL && isDedicated)
    {
        // Dedicated servers don't play local scripts.
#ifdef _DEBUG
        Con_Printf("Finale Begin: No local scripts in dedicated mode.\n");
#endif
        return false;
    }

#ifdef _DEBUG
    Con_Printf("Finale Begin: mode=%i '%.30s'\n", (int)mode, scriptSrc);
#endif

    // Init InFine state.
    s = stackPush(newState(scriptSrc));
    stateInit(s, mode, gameState, clientState);

    if(s->mode != FIMODE_LOCAL)
    {
        int flags = FINF_BEGIN | (s->mode == FIMODE_AFTER ? FINF_AFTER : s->mode == FIMODE_OVERLAY ? FINF_OVERLAY : 0);
        ddhook_finale_script_serialize_extradata_t p;
        boolean haveExtraData = false;

        memset(&p, 0, sizeof(p));

        if(s->extraData)
        {
            p.extraData = s->extraData;
            p.outBuf = 0;
            p.outBufSize = 0;
            haveExtraData = Plug_DoHook(HOOK_FINALE_SCRIPT_SERIALIZE_EXTRADATA, 0, &p);
        }

        // Tell clients to start this script.
        Sv_Finale(flags, scriptSrc, (haveExtraData? p.outBuf : 0), (haveExtraData? p.outBufSize : 0));
    }

    // Any hooks?
    Plug_DoHook(HOOK_FINALE_SCRIPT_BEGIN, (int) s->mode, s->extraData);

    return true;
}

void FI_ScriptTerminate(void)
{
    fi_state_t* s;
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_ScriptTerminate: Not initialized yet!\n");
#endif
        return;
    }
    if((s = stackTop()) && active)
    {
        FinaleInterpreter_AllowSkip(&s->_interpreter, true);
        scriptTerminate(s);
    }
}

fi_object_t* FI_Object(fi_objectid_t id)
{
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_Object: Not initialized yet!\n");
#endif
        return 0;
    }
    return objectsById(&objects, id);
}

fi_namespace_t* FI_ScriptNamespace(void)
{
    fi_state_t* s;
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_ScriptNamespace: Not initialized yet!\n");
#endif
        return 0;
    }
    if((s = stackTop()))
    {
        return &s->_namespace;
    }
    return 0;
}

finaleinterpreter_t* FI_ScriptInterpreter(void)
{
    fi_state_t* s;
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_ScriptInterpreter: Not initialized yet!\n");
#endif
        return 0;
    }
    if((s = stackTop()))
    {
        return &s->_interpreter;
    }
    return 0;
}

void* FI_ScriptExtraData(void)
{
    fi_state_t* s;
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_ScriptGetExtraData: Not initialized yet!\n");
#endif
        return 0;
    }
    if((s = stackTop()))
    {
        return s->extraData;
    }
    return 0;
}

fi_objectid_t FI_FindObjectIdForName(fi_namespace_t* names, const char* name, fi_obtype_e type)
{
    assert(names);
    if(!name || !name[0])
        return 0;
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_FindObjectIdForName: Not initialized yet!\n");
#endif
        return 0;
    }
    return toObjectId(names, name, type);
}

fi_object_t* FI_NewObject(fi_obtype_e type, const char* name)
{
    fi_object_t* obj;
    // Allocate and attach another.
    switch(type)
    {
    case FI_TEXT: obj = (fi_object_t*) P_CreateText(objectsUniqueId(&objects), name); break;
    case FI_PIC:  obj = (fi_object_t*) P_CreatePic(objectsUniqueId(&objects), name); break;
    default:
        Con_Error("FI_NewObject: Unknown type %i.", type);
    }
    return objectsAdd(&objects, obj);
}

void FI_DeleteObject(fi_object_t* obj)
{
    assert(obj);
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_DeleteObject: Not initialized yet!\n");
#endif
        return;
    }
    objectsRemove(&objects, obj);
    switch(obj->type)
    {
    case FI_PIC:    P_DestroyPic((fidata_pic_t*)obj);   break;
    case FI_TEXT:   P_DestroyText((fidata_text_t*)obj); break;
    default:
        Con_Error("FI_DeleteObject: Invalid type %i.", (int) obj->type);
    }
}

boolean FI_ObjectInNamespace(fi_namespace_t* names, fi_object_t* obj)
{
    assert(names);
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_ObjectInNamespace: Not initialized yet!\n");
#endif
        return false;
    }
    return objectIndexInNamespace(names, obj) != 0;
}

fi_object_t* FI_AddObjectInNamespace(fi_namespace_t* names, fi_object_t* obj)
{
    assert(names);
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_AddObjectToNamespace: Not initialized yet!\n");
#endif
        return obj;
    }
    if(obj && !objectIndexInNamespace(names, obj))
    {
        return addObjectToNamespace(names, obj);
    }
    return obj;
}

fi_object_t* FI_RemoveObjectInNamespace(fi_namespace_t* names, fi_object_t* obj)
{
    assert(names);
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_AddObjectToNamespace: Not initialized yet!\n");
#endif
        return obj;
    }
    if(obj && objectIndexInNamespace(names, obj))
    {
        return removeObjectInNamespace(names, obj);
    }
    return obj;
}

void FI_SetBackground(material_t* mat)
{
    fi_state_t* s;
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_SetBackground: Not initialized yet!\n");
#endif
        return;
    }
    if((s = stackTop()))
    {
        s->bgMaterial = mat;
    }
}

void FI_SetBackgroundColor(float red, float green, float blue, int steps)
{
    fi_state_t* s;
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_SetBackgroundColor: Not initialized yet!\n");
#endif
        return;
    }
    if((s = stackTop()))
    {
        AnimatorVector3_Set(s->bgColor, red, green, blue, steps);
    }
}

void FI_SetBackgroundColorAndAlpha(float red, float green, float blue, float alpha, int steps)
{
    fi_state_t* s;
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_SetBackgroundColorAndAlpha: Not initialized yet!\n");
#endif
        return;
    }
    if((s = stackTop()))
    {
        AnimatorVector4_Set(s->bgColor, red, green, blue, alpha, steps);
    }
}

void FI_SetImageOffsetX(float x, int steps)
{
    fi_state_t* s;
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_SetImageOffsetX: Not initialized yet!\n");
#endif
        return;
    }
    if((s = stackTop()))
    {
        Animator_Set(&s->imgOffset[0], x, steps);
    }
}

void FI_SetImageOffsetY(float y, int steps)
{
    fi_state_t* s;
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_SetImageOffsetY: Not initialized yet!\n");
#endif
        return;
    }
    if((s = stackTop()))
    {
        Animator_Set(&s->imgOffset[1], y, steps);
    }
}

void FI_SetFilterColorAndAlpha(float red, float green, float blue, float alpha, int steps)
{
    fi_state_t* s;
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_SetFilterColorAndAlpha: Not initialized yet!\n");
#endif
        return;
    }
    if((s = stackTop()))
    {
        AnimatorVector4_Set(s->filter, red, green, blue, alpha, steps);
    }
}

void FI_SetPredefinedColor(uint idx, float red, float green, float blue, int steps)
{
    fi_state_t* s;
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_SetPredefinedColor: Not initialized yet!\n");
#endif
        return;
    }
    if((s = stackTop()))
    {
        AnimatorVector3_Set(s->textColor[idx], red, green, blue, steps);
    }
}

void* FI_GetClientsideDefaultState(void)
{
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_GetClientsideDefaultState: Not initialized yet!\n");
#endif
        return 0;
    }
    return &defaultState;
}

/**
 * Set the truth conditions.
 * Used by clients after they've received a PSV_FINALE2 packet.
 */
void FI_SetClientsideDefaultState(void* data)
{
    assert(data);
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_SetClientsideDefaultState: Not initialized yet!\n");
#endif
        return;
    }
    memcpy(&defaultState, data, sizeof(FINALE_SCRIPT_EXTRADATA_SIZE));
}

void FI_Ticker(timespan_t ticLength)
{
    // Always tic.
    R_TextTicker(ticLength);

    /*if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_Ticker: Not initialized yet!\n");
#endif
        return;
    }*/

    if(!M_CheckTrigger(&sharedFixedTrigger, ticLength))
        return;

    // A new 'sharp' tick has begun.
    {fi_state_t* s;
    if((s = stackTop()) && active)
    {
        if(FinaleInterpreter_Suspended(&s->_interpreter))
            return;

        scriptTick(s);
    }}
}

/**
 * The user has requested a skip. Returns true if the skip was done.
 */
int FI_SkipRequest(void)
{
    fi_state_t* s;
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_SkipRequest: Not initialized yet!\n");
#endif
        return false;
    }
    if((s = stackTop()))
    {
        return FinaleInterpreter_Skip(&s->_interpreter);
    }
    return false;
}

/**
 * @return              @c true, if the event should open the menu.
 */
boolean FI_IsMenuTrigger(void)
{
    fi_state_t* s;
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_IsMenuTrigger: Not initialized yet!\n");
#endif
        return false;
    }
    if((s = stackTop()) && active)
        return FinaleInterpreter_IsMenuTrigger(&s->_interpreter);
    return false;
}

int FI_Responder(ddevent_t* ev)
{
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_Responder: Not initialized yet!\n");
#endif
        return false;
    }
    {fi_state_t* s;
    if((s = stackTop()) && active)
    {
        // During the first ~second disallow all events/skipping.
        if(s->timer < 20)
            return false;

        return FinaleInterpreter_Responder(&s->_interpreter, ev);
    }}
    return false;
}

/**
 * Drawing is the most complex task here.
 */
void FI_Drawer(void)
{
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_Drawer: Not initialized yet!\n");
#endif
        return;
    }

    {fi_state_t* s;
    if((s = stackTop()) && active)
    {
        // Don't draw anything until we are sure the script has started.
        if(!FinaleInterpreter_CommandExecuted(&s->_interpreter))
            return;
        // Don't draw anything if we are playing a demo.
        if(FinaleInterpreter_Suspended(&s->_interpreter))
            return;

        stateDraw(s);
    }}
}

void FIData_PicThink(fidata_pic_t* p)
{
    assert(p);

    // Call parent thinker.
    FIObject_Think((fi_object_t*)p);

    AnimatorVector4_Think(p->color);
    AnimatorVector4_Think(p->otherColor);
    AnimatorVector4_Think(p->edgeColor);
    AnimatorVector4_Think(p->otherEdgeColor);

    if(!(p->numFrames > 1))
        return;

    // If animating, decrease the sequence timer.
    if(p->frames[p->curFrame]->tics > 0)
    {
        if(--p->tics <= 0)
        {
            fidata_pic_frame_t* f;
            // Advance the sequence position. k = next pos.
            int next = p->curFrame + 1;

            if(next == p->numFrames)
            {   // This is the end.
                p->animComplete = true;

                // Stop the sequence?
                if(p->flags.looping)
                    next = 0; // Rewind back to beginning.
                else // Yes.
                    p->frames[next = p->curFrame]->tics = 0;
            }

            // Advance to the next pos.
            f = p->frames[p->curFrame = next];
            p->tics = f->tics;

            // Play a sound?
            if(f->sound > 0)
                S_LocalSound(f->sound, NULL);
        }
    }
}

static void drawRect(fidata_pic_t* p, uint frame, float angle, const float worldOffset[3])
{
    assert(p->numFrames && frame < p->numFrames);
    {
    float mid[3];
    fidata_pic_frame_t* f = (p->numFrames? p->frames[frame] : NULL);

    assert(f->type == PFT_MATERIAL);

    mid[VX] = mid[VY] = mid[VZ] = 0;

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(p->pos[0].value + worldOffset[VX], p->pos[1].value + worldOffset[VY], p->pos[2].value);
    glTranslatef(mid[VX], mid[VY], mid[VZ]);

    // Counter the VGA aspect ratio.
    if(p->angle.value != 0)
    {
        glScalef(1, 200.0f / 240.0f, 1);
        glRotatef(p->angle.value, 0, 0, 1);
        glScalef(1, 240.0f / 200.0f, 1);
    }

    // Move to origin.
    glTranslatef(-mid[VX], -mid[VY], -mid[VZ]);
    glScalef((p->numFrames && p->frames[p->curFrame]->flags.flip ? -1 : 1) * p->scale[0].value, p->scale[1].value, p->scale[2].value);

    {
    float offset[2] = { 0, 0 }, scale[2] = { 1, 1 }, color[4], bottomColor[4];
    int magMode = DGL_LINEAR, width = 1, height = 1;
    DGLuint tex = 0;
    fidata_pic_frame_t* f = (p->numFrames? p->frames[frame] : NULL);
    material_t* mat;

    if((mat = f->texRef.material))
    {
        material_load_params_t params;
        material_snapshot_t ms;
        surface_t suf;

        suf.header.type = DMU_SURFACE; /// \fixme: perhaps use the dummy object system?
        suf.owner = 0;
        suf.decorations = 0;
        suf.numDecorations = 0;
        suf.flags = suf.oldFlags = (f->flags.flip? DDSUF_MATERIAL_FLIPH : 0);
        suf.inFlags = SUIF_PVIS|SUIF_BLEND;
        suf.material = mat;
        suf.normal[VX] = suf.oldNormal[VX] = 0;
        suf.normal[VY] = suf.oldNormal[VY] = 0;
        suf.normal[VZ] = suf.oldNormal[VZ] = 1; // toward the viewer.
        suf.offset[0] = suf.visOffset[0] = suf.oldOffset[0][0] = suf.oldOffset[1][0] = worldOffset[VX];
        suf.offset[1] = suf.visOffset[1] = suf.oldOffset[0][1] = suf.oldOffset[1][1] = worldOffset[VY];
        suf.visOffsetDelta[0] = suf.visOffsetDelta[1] = 0;
        suf.rgba[CR] = p->color[0].value;
        suf.rgba[CG] = p->color[1].value;
        suf.rgba[CB] = p->color[2].value;
        suf.rgba[CA] = p->color[3].value;
        suf.blendMode = BM_NORMAL;

        memset(&params, 0, sizeof(params));
        params.pSprite = false;
        params.tex.border = 0; // Need to allow for repeating.
        Materials_Prepare(&ms, suf.material, (suf.inFlags & SUIF_BLEND), &params);

        {int i;
        for(i = 0; i < 4; ++i)
            color[i] = bottomColor[i] = suf.rgba[i];
        }

        if(ms.units[MTU_PRIMARY].texInst)
        {
            tex = ms.units[MTU_PRIMARY].texInst->id;
            magMode = ms.units[MTU_PRIMARY].magMode;
            offset[0] = ms.units[MTU_PRIMARY].offset[0];
            offset[1] = ms.units[MTU_PRIMARY].offset[1];
            scale[0] = 1;//ms.units[MTU_PRIMARY].scale[0];
            scale[1] = 1;//ms.units[MTU_PRIMARY].scale[1];
            color[CA] *= ms.units[MTU_PRIMARY].alpha;
            bottomColor[CA] *= ms.units[MTU_PRIMARY].alpha;
            width = ms.width;
            height = ms.height;
        }
    }

    // The fill.
    if(tex)
    {
        /// \fixme: do not override the value taken from the Material snapshot.
        magMode = (filterUI ? GL_LINEAR : GL_NEAREST);

        GL_BindTexture(tex, magMode);

        glMatrixMode(GL_TEXTURE);
        glPushMatrix();
        glTranslatef(offset[0], offset[1], 0);
        glScalef(scale[0], scale[1], 0);
    }
    else
        DGL_Disable(DGL_TEXTURING);

    glBegin(GL_QUADS);
        glColor4fv(color);
        glTexCoord2f(0, 0);
        glVertex2f(0, 0);

        glTexCoord2f(1, 0);
        glVertex2f(0+width, 0);

        glColor4fv(bottomColor);
        glTexCoord2f(1, 1);
        glVertex2f(0+width, 0+height);

        glTexCoord2f(0, 1);
        glVertex2f(0, 0+height);
    glEnd();

    if(tex)
    {
        glMatrixMode(GL_TEXTURE);
        glPopMatrix();
    }
    else
    {
        DGL_Enable(DGL_TEXTURING);
    }
    }

    // Restore original transformation.
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    }
}

static __inline boolean useRect(const fidata_pic_t* p, uint frame)
{
    fidata_pic_frame_t* f;
    if(!p->numFrames)
        return false;
    if(frame >= p->numFrames)
        return true;
    f = p->frames[frame];
    if(f->type == PFT_MATERIAL)
        return true;
    return false;
}

/**
 * Vertex layout:
 *
 * 0 - 1
 * | / |
 * 2 - 3
 */
static size_t buildGeometry(DGLuint tex, const float rgba[4], const float rgba2[4],
    boolean flagTexFlip, rvertex_t** verts, rcolor_t** colors, rtexcoord_t** coords)
{
    static rvertex_t rvertices[4];
    static rcolor_t rcolors[4];
    static rtexcoord_t rcoords[4];

    V3_Set(rvertices[0].pos, 0, 0, 0);
    V3_Set(rvertices[1].pos, 1, 0, 0);
    V3_Set(rvertices[2].pos, 0, 1, 0);
    V3_Set(rvertices[3].pos, 1, 1, 0);

    if(tex)
    {
        V2_Set(rcoords[0].st, (flagTexFlip? 1:0), 0);
        V2_Set(rcoords[1].st, (flagTexFlip? 0:1), 0);
        V2_Set(rcoords[2].st, (flagTexFlip? 1:0), 1);
        V2_Set(rcoords[3].st, (flagTexFlip? 0:1), 1);
    }

    V4_Copy(rcolors[0].rgba, rgba);
    V4_Copy(rcolors[1].rgba, rgba);
    V4_Copy(rcolors[2].rgba, rgba2);
    V4_Copy(rcolors[3].rgba, rgba2);

    *verts = rvertices;
    *coords = (tex!=0? rcoords : 0);
    *colors = rcolors;
    return 4;
}

static void drawGeometry(DGLuint tex, size_t numVerts, const rvertex_t* verts,
    const rcolor_t* colors, const rtexcoord_t* coords)
{
    glBindTexture(GL_TEXTURE_2D, tex);
    if(tex)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (filterUI ? GL_LINEAR : GL_NEAREST));
    }
    else
        DGL_Disable(DGL_TEXTURING);

    glBegin(GL_TRIANGLE_STRIP);
    {size_t i;
    for(i = 0; i < numVerts; ++i)
    {
        if(coords) glTexCoord2fv(coords[i].st);
        if(colors) glColor4fv(colors[i].rgba);
        glVertex3fv(verts[i].pos);
    }}
    glEnd();

    if(!tex)
        DGL_Enable(DGL_TEXTURING);
}

static void drawPicFrame(fidata_pic_t* p, uint frame, const float _origin[3],
    const float scale[3], const float rgba[4], const float rgba2[4], float angle,
    const float worldOffset[3])
{
    if(useRect(p, frame))
    {
        drawRect(p, frame, angle, worldOffset);
        return;
    }

    {
    vec3_t offset = { 0, 0, 0 }, dimensions, origin, originOffset, center;
    boolean showEdges = true, flipTextureS = false;
    DGLuint tex = 0;
    size_t numVerts;
    rvertex_t* rvertices;
    rcolor_t* rcolors;
    rtexcoord_t* rcoords;

    if(p->numFrames)
    {
        fidata_pic_frame_t* f = p->frames[frame];
        patchtex_t* patch;
        rawtex_t* rawTex;

        flipTextureS = (f->flags.flip != 0);
        showEdges = false;

        if(f->type == PFT_RAW && (rawTex = R_GetRawTex(f->texRef.lump)))
        {   
            tex = GL_PrepareRawTex(rawTex);
            V3_Set(offset, SCREENWIDTH/2, SCREENHEIGHT/2, 0);
            V3_Set(dimensions, rawTex->width, rawTex->height, 1);
        }
        else if(f->type == PFT_XIMAGE)
        {
            tex = (DGLuint)f->texRef.tex;
            V3_Set(offset, SCREENWIDTH/2, SCREENHEIGHT/2, 0);
            V3_Set(dimensions, 1, 1, 1); /// \fixme.
        }
        /*else if(f->type == PFT_MATERIAL)
        {
            V3_Set(dimensions, 1, 1, 1);
        }*/
        else if(f->type == PFT_PATCH && (patch = R_FindPatchTex(f->texRef.patch)))
        {
            tex = (renderTextures==1? GL_PreparePatch(patch) : 0);
            V3_Set(offset, patch->offX, patch->offY, 0);
            /// \todo need to decide what if any significance what depth will mean here.
            V3_Set(dimensions, patch->width, patch->height, 1);
        }
    }

    // If we've not chosen a texture by now set some defaults.
    if(!tex)
    {
        V3_Set(dimensions, 1, 1, 1);
    }

    V3_Set(center, dimensions[VX] / 2, dimensions[VY] / 2, dimensions[VZ] / 2);

    V3_Sum(origin, _origin, center);
    V3_Subtract(origin, origin, offset);
    V3_Sum(origin, origin, worldOffset);

    V3_Subtract(originOffset, offset, center);              
    offset[VX] *= scale[VX]; offset[VY] *= scale[VY]; offset[VZ] *= scale[VZ];
    V3_Sum(originOffset, originOffset, offset);

    numVerts = buildGeometry(tex, rgba, rgba2, flipTextureS, &rvertices, &rcolors, &rcoords);

    // Setup the transformation.
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    // Move to the object origin.
    glTranslatef(origin[VX], origin[VY], origin[VZ]);

    if(angle != 0)
    {
        // With rotation we must counter the VGA aspect ratio.
        glScalef(1, 200.0f / 240.0f, 1);
        glRotatef(angle, 0, 0, 1);
        glScalef(1, 240.0f / 200.0f, 1);
    }

    // Translate to the object center.
    glTranslatef(originOffset[VX], originOffset[VY], originOffset[VZ]);
    glScalef(scale[VX] * dimensions[VX], scale[VY] * dimensions[VY], scale[VZ] * dimensions[VZ]);

    drawGeometry(tex, numVerts, rvertices, rcolors, rcoords);

    if(showEdges)
    {
        // The edges never have a texture.
        DGL_Disable(DGL_TEXTURING);

        glBegin(GL_LINES);
            useColor(p->edgeColor, 4);
            glVertex2f(0, 0);
            glVertex2f(1, 0);
            glVertex2f(1, 0);

            useColor(p->otherEdgeColor, 4);
            glVertex2f(1, 1);
            glVertex2f(1, 1);
            glVertex2f(0, 1);
            glVertex2f(0, 1);

            useColor(p->edgeColor, 4);
            glVertex2f(0, 0);
        glEnd();
        
        DGL_Enable(DGL_TEXTURING);
    }

    // Restore original transformation.
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    }
}

void FIData_PicDraw(fidata_pic_t* p, const float worldOffset[3])
{
    assert(p);
    {
    vec3_t scale, origin;
    vec4_t rgba, rgba2;

    // Fully transparent pics will not be drawn.
    if(!(p->color[CA].value > 0))
        return;

    V3_Set(origin, p->pos[VX].value, p->pos[VY].value, p->pos[VZ].value);
    V3_Set(scale, p->scale[VX].value, p->scale[VY].value, p->scale[VZ].value);
    V4_Set(rgba, p->color[CR].value, p->color[CG].value, p->color[CB].value, p->color[CA].value);
    if(p->numFrames == 0)
        V4_Set(rgba2, p->otherColor[CR].value, p->otherColor[CG].value, p->otherColor[CB].value, p->otherColor[CA].value);

    drawPicFrame(p, p->curFrame, origin, scale, rgba, (p->numFrames==0? rgba2 : rgba), p->angle.value, worldOffset);
    }
}

uint FIData_PicAppendFrame(fidata_pic_t* p, int type, int tics, void* texRef, short sound,
    boolean flagFlipH)
{
    assert(p);
    picAddFrame(p, createPicFrame(type, tics, texRef, sound, flagFlipH));
    return p->numFrames-1;
}

void FIData_PicClearAnimation(fidata_pic_t* p)
{
    assert(p);
    if(p->frames)
    {
        uint i;
        for(i = 0; i < p->numFrames; ++i)
            destroyPicFrame(p->frames[i]);
        Z_Free(p->frames);
    }
    p->flags.looping = false; // Yeah?
    p->frames = 0;
    p->numFrames = 0;
    p->curFrame = 0;
    p->animComplete = true;
}

void FIData_TextThink(fidata_text_t* t)
{
    assert(t);

    // Call parent thinker.
    FIObject_Think((fi_object_t*)t);

    AnimatorVector4_Think(t->color);

    if(t->wait)
    {
        if(--t->timer <= 0)
        {
            t->timer = t->wait;
            t->cursorPos++;
        }
    }

    if(t->scrollWait)
    {
        if(--t->scrollTimer <= 0)
        {
            t->scrollTimer = t->scrollWait;
            t->pos[1].target -= 1;
            t->pos[1].steps = t->scrollWait;
        }
    }

    // Is the text object fully visible?
    t->animComplete = (!t->wait || t->cursorPos >= FIData_TextLength(t));
}

static int textLineWidth(const char* text, compositefontid_t font)
{
    int width = 0;

    for(; *text; text++)
    {
        if(*text == '\\')
        {
            if(!*++text)
                break;
            if(*text == 'n')
                break;
            if(*text >= '0' && *text <= '9')
                continue;
            if(*text == 'w' || *text == 'W' || *text == 'p' || *text == 'P')
                continue;
        }
        width += GL_CharWidth(*text, font);
    }

    return width;
}

void FIData_TextDraw(fidata_text_t* tex, const float offset[3])
{
    assert(tex);
    {
    fi_state_t* s = stackTop();
    int cnt, x = 0, y = 0;
    int ch, linew = -1;
    char* ptr;

    if(!tex->text)
        return;

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(tex->pos[0].value + offset[VX], tex->pos[1].value + offset[VY], tex->pos[2].value + offset[VZ]);

    rotate(tex->angle.value);
    glScalef(tex->scale[0].value, tex->scale[1].value, tex->scale[2].value);

    // Draw it.
    // Set color zero (the normal color).
    useColor(tex->color, 4);
    for(cnt = 0, ptr = tex->text; *ptr && (!tex->wait || cnt < tex->cursorPos); ptr++)
    {
        if(linew < 0)
            linew = textLineWidth(ptr, tex->font);

        ch = *ptr;
        if(*ptr == '\\') // Escape?
        {
            if(!*++ptr)
                break;

            // Change of color.
            if(*ptr >= '0' && *ptr <= '9')
            {
                animatorvector3_t* color;
                uint colorIdx = *ptr - '0';

                if(!colorIdx)
                    color = (animatorvector3_t*) &tex->color; // Use the default color.
                else
                    color = &s->textColor[colorIdx-1];

                glColor4f((*color)[0].value, (*color)[1].value, (*color)[2].value, tex->color[3].value);
                continue;
            }

            // 'w' = half a second wait, 'W' = second's wait
            if(*ptr == 'w' || *ptr == 'W') // Wait?
            {
                if(tex->wait)
                    cnt += (int) ((float)TICRATE / tex->wait / (*ptr == 'w' ? 2 : 1));
                continue;
            }

            // 'p' = 5 second wait, 'P' = 10 second wait
            if(*ptr == 'p' || *ptr == 'P') // Longer pause?
            {
                if(tex->wait)
                    cnt += (int) ((float)TICRATE / tex->wait * (*ptr == 'p' ? 5 : 10));
                continue;
            }

            if(*ptr == 'n' || *ptr == 'N') // Newline?
            {
                x = 0;
                y += GL_CharHeight('A', tex->font) * (1+tex->lineheight);
                linew = -1;
                cnt++; // Include newlines in the wait count.
                continue;
            }

            if(*ptr == '_')
                ch = ' ';
        }

        // Let's do Y-clipping (in case of tall text blocks).
        if(tex->scale[1].value * y + tex->pos[1].value >= -tex->scale[1].value * tex->lineheight &&
           tex->scale[1].value * y + tex->pos[1].value < SCREENHEIGHT)
        {
            GL_DrawChar2(ch, (tex->textFlags & DTF_ALIGN_LEFT) ? x : x - linew / 2, y, tex->font);
            x += GL_CharWidth(ch, tex->font);
        }

        cnt++; // Actual character drawn.
    }

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    }
}

/**
 * @return                  The length as a counter.
 */
int FIData_TextLength(fidata_text_t* tex)
{
    assert(tex);
    {
    int cnt = 0;
    if(tex->text)
    {
        float secondLen = (tex->wait ? TICRATE / tex->wait : 0);
        const char* ptr;
        for(ptr = tex->text; *ptr; ptr++)
        {
            if(*ptr == '\\') // Escape?
            {
                if(!*++ptr)
                    break;
                if(*ptr == 'w')
                    cnt += secondLen / 2;
                if(*ptr == 'W')
                    cnt += secondLen;
                if(*ptr == 'p')
                    cnt += 5 * secondLen;
                if(*ptr == 'P')
                    cnt += 10 * secondLen;
                if((*ptr >= '0' && *ptr <= '9') || *ptr == 'n' || *ptr == 'N')
                    continue;
            }

            cnt++; // An actual character.
        }
    }
    return cnt;
    }
}

void FIData_TextCopy(fidata_text_t* t, const char* str)
{
    assert(t);
    assert(str && str[0]);
    {
    size_t len = strlen(str) + 1;
    if(t->text)
        Z_Free(t->text); // Free any previous text.
    t->text = Z_Malloc(len, PU_STATIC, 0);
    memcpy(t->text, str, len);
    }
}

D_CMD(StartFinale)
{
    finale_mode_t mode = (FI_Active()? FIMODE_OVERLAY : FIMODE_LOCAL); //(G_GetGameState() == GS_MAP? FIMODE_OVERLAY : FIMODE_LOCAL);
    char* script;

    if(FI_Active())
        return false;

    if(!Def_Get(DD_DEF_FINALE, argv[1], &script))
    {
        Con_Printf("Script '%s' is not defined.\n", argv[1]);
        return false;
    }

    FI_ScriptBegin(script, mode, gx.FI_GetGameState(), 0);
    return true;
}

D_CMD(StopFinale)
{
    FI_ScriptTerminate();
    return true;
}
