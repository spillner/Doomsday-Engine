/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
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
 *
 * \bug Not 64bit clean: In function 'DS_CreateBuffer': cast to pointer from integer of different size
 * \bug Not 64bit clean: In function 'DS_DestroyBuffer': cast to pointer from integer of different size
 * \bug Not 64bit clean: In function 'DS_Load': cast to pointer from integer of different size
 * \bug Not 64bit clean: In function 'DS_Play': cast to pointer from integer of different size
 * \bug Not 64bit clean: In function 'DS_Stop': cast to pointer from integer of different size
 * \bug Not 64bit clean: In function 'DS_Refresh': cast to pointer from integer of different size
 * \bug Not 64bit clean: In function 'DS_Set': cast to pointer from integer of different size
 * \bug Not 64bit clean: In function 'DS_Setv': cast to pointer from integer of different size
 */

/*
 * driver_openal.c: OpenAL Doomsday Sfx Driver
 *
 * Link with openal32.lib (and doomsday.lib).
 */

// HEADER FILES ------------------------------------------------------------

#ifdef WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#ifdef MSVC
#  pragma warning (disable: 4244)
#endif

#ifndef FINK
   #include <AL/al.h>
   #include <AL/alc.h>
#else
   #include <openal/al.h>
   #include <openal/alc.h>
#endif
#include <string.h>
#include <math.h>

#include "doomsday.h"
#include "sys_sfxd.h"

// MACROS ------------------------------------------------------------------

#define PI          3.141592654

#define Src(buf)    ((ALuint)buf->ptr3d)
#define Buf(buf)    ((ALuint)buf->ptr)

// TYPES -------------------------------------------------------------------

enum { VX, VY, VZ };

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

#ifdef WIN32
ALenum(*EAXGet) (const struct _GUID *propertySetID, ALuint property,
                 ALuint source, ALvoid *value, ALuint size);
ALenum(*EAXSet) (const struct _GUID *propertySetID, ALuint property,
                 ALuint source, ALvoid *value, ALuint size);
#endif

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

int     DS_Init(void);
void    DS_Shutdown(void);
sfxbuffer_t *DS_CreateBuffer(int flags, int bits, int rate);
void    DS_DestroyBuffer(sfxbuffer_t *buf);
void    DS_Load(sfxbuffer_t *buf, struct sfxsample_s *sample);
void    DS_Reset(sfxbuffer_t *buf);
void    DS_Play(sfxbuffer_t *buf);
void    DS_Stop(sfxbuffer_t *buf);
void    DS_Refresh(sfxbuffer_t *buf);
void    DS_Event(int type);
void    DS_Set(sfxbuffer_t *buf, int property, float value);
void    DS_Setv(sfxbuffer_t *buf, int property, float *values);
void    DS_Listener(int property, float value);
void    DS_Listenerv(int property, float *values);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

#ifdef WIN32
// EAX 2.0 GUIDs
struct _GUID DSPROPSETID_EAX20_ListenerProperties =
    { 0x306a6a8, 0xb224, 0x11d2, {0x99, 0xe5, 0x0, 0x0, 0xe8, 0xd8, 0xc7,
                                  0x22}
};
struct _GUID DSPROPSETID_EAX20_BufferProperties =
    { 0x306a6a7, 0xb224, 0x11d2, {0x99, 0xe5, 0x0, 0x0, 0xe8, 0xd8, 0xc7,
                                  0x22}
};
#endif

boolean initOk = false, hasEAX = false;
int     verbose;
float   unitsPerMeter = 1;
float   headYaw, headPitch;     // In radians.
ALCdevice *device = 0;
ALCcontext *context = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static int error(const char *what, const char *msg)
{
    ALenum  code = alGetError();

    if(code == AL_NO_ERROR)
        return false;

    Con_Message("DS_%s(OpenAL): %s [%s]\n", what, msg, alGetString(code));
    return true;
}

int DS_Init(void)
{
    if(initOk)
        return true;

    // Are we in verbose mode?
    if((verbose = ArgExists("-verbose")))
        Con_Message("DS_Init(OpenAL): Starting OpenAL...\n");

    // Open device.
    if(!(device = alcOpenDevice((ALubyte *) "DirectSound3D")))
    {
        Con_Message("Failed to initialize OpenAL (DS3D).\n");
        return false;
    }
    // Create a context.
    alcMakeContextCurrent(context = alcCreateContext(device, NULL));

    // Clear error message.
    alGetError();

#ifdef WIN32
    // Check for EAX 2.0.
    if((hasEAX = alIsExtensionPresent((ALubyte *) "EAX2.0")))
    {
        if(!(EAXGet = alGetProcAddress("EAXGet")))
            hasEAX = false;
        if(!(EAXSet = alGetProcAddress("EAXSet")))
            hasEAX = false;
    }
    if(hasEAX && verbose)
        Con_Message("DS_Init(OpenAL): EAX 2.0 available.\n");
#else
    hasEAX = false;
#endif

    alListenerf(AL_GAIN, 1);
    alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
    headYaw = headPitch = 0;
    unitsPerMeter = 36;

    // Everything is OK.
    initOk = true;
    return true;
}

void DS_Shutdown(void)
{
    if(!initOk)
        return;

    alcMakeContextCurrent(NULL);
    alcDestroyContext(context);
    alcCloseDevice(device);

    context = NULL;
    device = NULL;
    initOk = false;
}

sfxbuffer_t *DS_CreateBuffer(int flags, int bits, int rate)
{
    sfxbuffer_t *buf;
    ALuint  bufferName, sourceName;

    // Create a new buffer and a new source.
    alGenBuffers(1, &bufferName);
    if(error("CreateBuffer", "GenBuffers"))
        return 0;

    alGenSources(1, &sourceName);
    if(error("CreateBuffer", "GenSources"))
    {
        alDeleteBuffers(1, &bufferName);
        return 0;
    }

    // Attach the buffer to the source.
    alSourcei(sourceName, AL_BUFFER, bufferName);
    error("CreateBuffer", "Source BUFFER");

    if(!(flags & SFXBF_3D))
    {   // 2D sounds are around the listener.
        alSourcei(sourceName, AL_SOURCE_RELATIVE, AL_TRUE);
        alSourcef(sourceName, AL_ROLLOFF_FACTOR, 0);
    }

    // Create the buffer object.
    buf = Z_Malloc(sizeof(*buf), PU_STATIC, 0);
    memset(buf, 0, sizeof(*buf));
    buf->ptr = (void *) bufferName;
    buf->ptr3d = (void *) sourceName;
    buf->bytes = bits / 8;
    buf->rate = rate;
    buf->flags = flags;
    buf->freq = rate; // Modified by calls to Set(SFXBP_FREQUENCY).
    return buf;
}

void DS_DestroyBuffer(sfxbuffer_t *buf)
{
    ALuint  srcName = Src(buf);
    ALuint  bufName = Buf(buf);

    alDeleteSources(1, &srcName);
    alDeleteBuffers(1, &bufName);

    Z_Free(buf);
}

void DS_Load(sfxbuffer_t *buf, struct sfxsample_s *sample)
{
    // Does the buffer already have a sample loaded?
    if(buf->sample)
    {
        // Is the same one?
        if(buf->sample->id == sample->id)
            return; // No need to reload.
    }

    alBufferData(Buf(buf),
                 sample->bytesper == 1 ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16,
                 sample->data, sample->size, sample->rate);

    error("Load", "BufferData");
    buf->sample = sample;
}

/**
 * Stops the buffer and makes it forget about its sample.
 */
void DS_Reset(sfxbuffer_t *buf)
{
    DS_Stop(buf);
    buf->sample = NULL;
}

void DS_Play(sfxbuffer_t *buf)
{
    ALint       i;
    float       f;
    ALuint      source = Src(buf), bn;

    // Playing is quite impossible without a sample.
    if(!buf->sample)
        return;

#if _DEBUG
alGetSourcei(source, AL_BUFFER, &bn);
Con_Message("Buffer = %x\n", bn);
if(bn != Buf(buf))
    Con_Message("Not the same!\n");
#endif

    alSourcei(source, AL_BUFFER, Buf(buf));
    alSourcei(source, AL_LOOPING, (buf->flags & SFXBF_REPEAT) != 0);
    alSourcePlay(source);
    error("Play", "SourcePlay");

    alGetSourcei(source, AL_BUFFER, &bn);
    Con_Message("Buffer = %x (real = %x), isBuf:%i\n", bn, Buf(buf),
                alIsBuffer(bn));

    alGetBufferi(bn, AL_SIZE, &i);
    Con_Message("Bufsize = %i bytes\n", i);

    alGetBufferi(bn, AL_BITS, &i);
    Con_Message("Bufbits = %i\n", i);

    alGetSourcef(source, AL_GAIN, &f);
    Con_Message("Gain = %g\n", f);

    alGetSourcef(source, AL_PITCH, &f);
    Con_Message("Pitch = %g\n", f);

    alGetSourcei(source, AL_SOURCE_STATE, &i);
    error("Play", "Get state");
    Con_Message("State = %x\n", i);
    if(i != AL_PLAYING)
        Con_Message("not playing...\n");

    // The buffer is now playing.
    buf->flags |= SFXBF_PLAYING;
}

void DS_Stop(sfxbuffer_t *buf)
{
    if(!buf->sample)
        return;

    alSourceRewind(Src(buf));
    buf->flags &= ~SFXBF_PLAYING;
}

void DS_Refresh(sfxbuffer_t *buf)
{
    ALint       state;

    if(!buf->sample)
        return;

    alGetSourcei(Src(buf), AL_SOURCE_STATE, &state);
    if(state == AL_STOPPED)
    {
        buf->flags &= ~SFXBF_PLAYING;
    }
}

void DS_Event(int type)
{
    // Not supported.
}

/**
 * @param yaw           Yaw in radians.
 * @param pitch         Pitch in radians.
 * @param front         Ptr to front vector, can be @c NULL.
 * @param up            Ptr to up vector, can be @c NULL.
 */
static void vectors(float yaw, float pitch, float *front, float *up)
{
    if(!front && !up)
        return; // Nothing to do.

    if(front)
    {
        front[VX] = (float) (cos(yaw) * cos(pitch));
        front[VZ] = (float) (sin(yaw) * cos(pitch));
        front[VY] = (float) sin(pitch);
    }

    if(up)
    {
        up[VX] = (float) (-cos(yaw) * sin(pitch));
        up[VZ] = (float) (-sin(yaw) * sin(pitch));
        up[VY] = (float) cos(pitch);
    }
}

/**
 * Pan is linear, from -1 to 1. 0 is in the middle.
 */
static void setPan(ALuint source, float pan)
{
    float       pos[3];

    vectors((float) (headYaw - pan * PI / 2), headPitch, pos, 0);
    alSourcefv(source, AL_POSITION, pos);
}

void DS_Set(sfxbuffer_t *buf, int prop, float value)
{
    unsigned int dw;
    ALuint      source = Src(buf);

    switch(prop)
    {
    case SFXBP_VOLUME:
        alSourcef(source, AL_GAIN, value);
        break;

    case SFXBP_FREQUENCY:
        dw = (int) (buf->rate * value);
        if(dw != buf->freq) // Don't set redundantly.
        {
            buf->freq = dw;
            alSourcef(source, AL_PITCH, value);
        }
        break;

    case SFXBP_PAN:
        setPan(source, value);
        break;

    case SFXBP_MIN_DISTANCE:
        alSourcef(source, AL_REFERENCE_DISTANCE, value / unitsPerMeter);
        break;

    case SFXBP_MAX_DISTANCE:
        alSourcef(source, AL_MAX_DISTANCE, value / unitsPerMeter);
        break;

    case SFXBP_RELATIVE_MODE:
        alSourcei(source, AL_SOURCE_RELATIVE, value ? AL_TRUE : AL_FALSE);
        break;

    default:
        break;
    }
}

void DS_Setv(sfxbuffer_t *buf, int prop, float *values)
{
    ALuint      source = Src(buf);

    switch(prop)
    {
    case SFXBP_POSITION:
        alSource3f(source, AL_POSITION, values[VX] / unitsPerMeter,
                   values[VZ] / unitsPerMeter, values[VY] / unitsPerMeter);
        break;

    case SFXBP_VELOCITY:
        alSource3f(source, AL_VELOCITY, values[VX] / unitsPerMeter,
                   values[VZ] / unitsPerMeter, values[VY] / unitsPerMeter);
        break;

    default:
        break;
    }
}

void DS_Listener(int prop, float value)
{
    switch(prop)
    {
    case SFXLP_UNITS_PER_METER:
        unitsPerMeter = value;
        break;

    case SFXLP_DOPPLER:
        alDopplerFactor(value);
        break;

    default:
        break;
    }
}

void DS_Listenerv(int prop, float *values)
{
    float       ori[6];

    switch(prop)
    {
    case SFXLP_PRIMARY_FORMAT:
        // No need to concern ourselves with this kind of things...
        break;

    case SFXLP_POSITION:
        alListener3f(AL_POSITION, values[VX] / unitsPerMeter,
                     values[VZ] / unitsPerMeter, values[VY] / unitsPerMeter);
        break;

    case SFXLP_VELOCITY:
        alListener3f(AL_VELOCITY, values[VX] / unitsPerMeter,
                     values[VZ] / unitsPerMeter, values[VY] / unitsPerMeter);
        break;

    case SFXLP_ORIENTATION:
        vectors(headYaw = (float) (values[VX] / 180 * PI),
                headPitch = (float) (values[VY] / 180 * PI),
                ori, ori + 3);
        alListenerfv(AL_ORIENTATION, ori);
        break;

    case SFXLP_REVERB: // Not supported.
        break;

    default:
        DS_Listener(prop, 0);
        break;
    }
}
