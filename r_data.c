/* Emacs style mode select   -*- C++ -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2002 by
 *  Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze
 *  Copyright 2005, 2006 by
 *  Florian Schulze, Colin Phipps, Neil Stevens, Andrey Budko
 *  Copyright 2023-2026 by
 *  Frenkel Smeijers
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
 * DESCRIPTION:
 *      Preparation of data for rendering,
 *      generation of lookups, caching, retrieval by name.
 *
 *-----------------------------------------------------------------------------*/

#include <stdint.h>

#include "compiler.h"
#include "d_player.h"
#include "w_wad.h"
#include "r_main.h"
#include "m_fixed.h"
#include "i_system.h"
#include "r_things.h"
#include "p_tick.h"
#include "p_tick.h"

#include "globdata.h"


int16_t V_NumPatchWidth(int16_t num)
{
	const patch_t __far* patch = W_GetLumpByNum(num);
	int16_t width = patch->width;
	return width;
}


//
// Graphics.
// DOOM graphics for walls and sprites
// is stored in vertical runs of opaque pixels (posts).
// A column is composed of zero or more posts,
// a patch or sprite is composed of zero or more columns.
//

//
// Texture definition.
// Each texture is composed of one or more patches,
// with patches being lumps stored in the WAD.
// The lumps are referenced by number, and patched
// into the rectangular texture space using origin
// and possibly other attributes.
//

typedef struct
{
  int16_t originx;
  int16_t originy;
  int16_t patch;
} mappatch_t;

typedef char assertMappatchSize[sizeof(mappatch_t) == 6 ? 1 : -1];


typedef struct
{
  char       name[8];
  int16_t      width;
  int16_t      height;
  int16_t      patchcount;
  mappatch_t patches[1];
} maptexture_t;

typedef char assertMaptextureSize[sizeof(maptexture_t) == 20 ? 1 : -1];


// A maptexture_t describes a rectangular texture, which is composed
// of one or more mappatch_t structures that arrange graphic patches.

static const uint16_t *texturep;
static int16_t numtextures;


const texture_t __far* R_GetTexture(int16_t texture)
{
#ifdef RANGECHECK
    if (texture >= numtextures)
        I_Error("R_GetTexture: Texture %d not in range.", texture);
#endif

#if defined ONE_WALL_TEXTURE
    texture = 46;
#endif

    return (texture_t*)((const byte*)texturep + texturep[texture]);
}

static int16_t R_GetTextureNumForName(const char* tex_name)
{
    char name8[8];
    strncpy(name8, tex_name, sizeof(name8));

    const int32_t __far* maptex = W_GetLumpByName("TEXTURE1");
    const int32_t __far* directory = maptex+1;

    for (int16_t i = 0; i < numtextures; i++)
    {
        int32_t offset = *directory++;

        const maptexture_t __far* mtexture = (const maptexture_t __far*) ( (const byte __far*)maptex + offset);

        if (Z_EqualNames(mtexture->name, name8))
        {
            return i;
        }
    }

    I_Error("R_GetTextureNumForName: texture name: %s not found.", tex_name);
    return -1;
}




//
// R_CheckTextureNumForName
// Check whether texture is available.
// Filter out NoTexture indicator.
//
// Rewritten by Lee Killough to use hash table for fast lookup. Considerably
// reduces the time needed to start new levels. See w_wad.c for comments on
// the hashing algorithm, which is also used for lump searches.
//
// killough 1/21/98, 1/31/98
//

#define NO_TEXTURE 0

int16_t PUREFUNC R_CheckTextureNumForName (const char *tex_name)
{
    // "NoTexture" marker.
    if (tex_name[0] == '-')
        return NO_TEXTURE;

    return R_GetTextureNumForName(tex_name);
}


//
// R_InitTextures
// Initializes the texture list
//  with the textures from the world map.
//

static void R_InitTextures()
{
	const int32_t __far* mtex1 = W_GetLumpByName("TEXTURE1");
	numtextures = *mtex1;

	texturep = W_GetLumpByName("TEXTUREP");

	textureheight = W_GetLumpByName("TEXHEIGH");
}


//
// R_Init
// Locates all the lumps
//  that will be used by all views
// Must be called after W_Init.
//

void R_Init(void)
{
  R_InitTextures();
  R_InitFlats();
  R_InitSky();
  R_InitSpriteLumps();
  R_InitColormaps();
}
