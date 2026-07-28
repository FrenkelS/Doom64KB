/* Emacs style mode select   -*- C++ -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2000 by
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
 *  Do all the WAD I/O, get map description,
 *  set up initial state and misc. LUTs.
 *
 *-----------------------------------------------------------------------------*/

#include <stdint.h>

#include "d_player.h"
#include "g_game.h"
#include "w_wad.h"
#include "r_main.h"
#include "r_things.h"
#include "p_maputl.h"
#include "p_map.h"
#include "p_setup.h"
#include "p_spec.h"
#include "p_tick.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "i_system.h"
#include "v_video.h"

#include "globdata.h"

#if defined NEOGEO_ROM_SECTOR_LINES
#include "neogeo/assets/generated/doom_sector_lines.h"
#endif


//
// MAP related Lookup tables.
// Store VERTEXES, LINEDEFS, SIDEDEFS, etc.
//

const seg_t    __far* _g_segs;

int16_t      _g_numsectors;
sector_t __far* _g_sectors;
const mapsector_t *_g_mapsectors;


static int16_t      numsubsectors;
subsector_t __far* _g_subsectors;
const mapsubsector_t *_g_mapsubsectors;



int16_t      _g_numlines;
linedata_t   __far* _g_lines;
const line_t *_g_maplines;
#if defined NEOGEO_COMPACT_LINESTATE
uint8_t __far* _g_lineRenderValid;
#endif

#if defined NEOGEO_ROM_SECTOR_LINES
const uint16_t __far* _g_sectorLineIndices;
const sectorlinespan_t __far* _g_sectorLineSpans;
#endif


static int16_t      numsides;
side_t   __far* _g_sides;
const mapsidedef_t *_g_mapsides;


// BLOCKMAP
// Created from axis aligned bounding box
// of the map, a rectangular array of
// blocks of size ...
// Used to speed up collision detection
// by spatial subdivision in 2D.
//
// Blockmap size.

int16_t       _g_bmapwidth, _g_bmapheight;  // size in mapblocks

// killough 3/1/98: remove blockmap limit internally:
const int16_t      __far* _g_blockmap;

// offsets in blockmap are from here
const int16_t      __far* _g_blockmaplump;

fixed_t   _g_bmaporgx, _g_bmaporgy;     // origin of block map

mobjindex_t __far* _g_blocklinks;        // indexes into _g_thingPool

//
// REJECT
// For fast sight rejection.
// Speeds up enemy AI by skipping detailed
//  LineOf Sight calculation.
// Without the special effect, this could
// be used as a PVS lookup as well.
//

const byte __far* _g_rejectmatrix;

mobj_t __far*      _g_thingPool;
int16_t _g_thingPoolSize;

#if defined LOW_MEMORY && defined NEOGEO_SPRITE_MICROFB
#define MAPTHING_DORMANT  0u
#define MAPTHING_ACTIVE   1u
#define MAPTHING_CONSUMED 2u
#define MAPTHING_SKIPPED  3u

#define MAPTHING_ACTIVATE_DISTANCE   1024L
#define MAPTHING_DEACTIVATE_DISTANCE 1536L
#define MAPTHING_TRANSIENT_SLOTS     16u
#define MAPTHING_RUNTIME_RESERVE     3072u

static const mapthing_t __far* _s_mapThings;
static uint8_t __far* _s_mapThingStates;
static uint16_t __far* _s_poolMapThing;
static int16_t _s_mapThingCount;
static int16_t _s_activeMapThings;
static boolean _s_streamMapThings;

static uint8_t P_GetMapThingState(int16_t index)
{
	const uint8_t packed = _s_mapThingStates[(uint16_t)index >> 2];
	return (packed >> (((uint16_t)index & 3u) * 2u)) & 3u;
}

static void P_SetMapThingState(int16_t index, uint8_t state)
{
	const uint16_t byte_index = (uint16_t)index >> 2;
	const uint8_t shift = ((uint16_t)index & 3u) * 2u;
	const uint8_t mask = 3u << shift;
	_s_mapThingStates[byte_index] =
		(_s_mapThingStates[byte_index] & ~mask) | (state << shift);
}

static int16_t P_ThingPoolFreeSlots(void)
{
	int16_t free_slots = 0;

	for (int16_t i = 0; i < _g_thingPoolSize; i++)
		if (_g_thingPool[i].type == MT_NOTHING)
			free_slots++;

	return free_slots;
}

static int32_t P_MapThingDistance(const mapthing_t __far* mt)
{
	int32_t dx = (int32_t)mt->x - (_g_player.mo->x >> FRACBITS);
	int32_t dy = (int32_t)mt->y - (_g_player.mo->y >> FRACBITS);

	if (dx < 0)
		dx = -dx;
	if (dy < 0)
		dy = -dy;

	return dx > dy ? dx : dy;
}

static void P_AssociateMapThing(mobj_t __far* mobj, int16_t map_index)
{
	if (mobj < _g_thingPool || mobj >= _g_thingPool + _g_thingPoolSize)
		I_Error("P_AssociateMapThing: non-pooled map thing");

	const int16_t pool_index = mobj - _g_thingPool;
	_s_poolMapThing[pool_index] = map_index;
	P_SetMapThingState(map_index, MAPTHING_ACTIVE);
	_s_activeMapThings++;
}
#endif


// Lump order in a map WAD: each map needs a couple of lumps
// to provide a complete scene geometry description.
enum {
  ML_LABEL,             // A separator, name, ExMx or MAPxx
  ML_THINGS,            // Monsters, items..
  ML_LINEDEFS,          // LineDefs, from editing
  ML_SIDEDEFS,          // SideDefs, from editing
  ML_SEGS,              // LineSegs, from LineDefs split by BSP
  ML_SSECTORS,          // SubSectors, list of LineSegs
  ML_NODES,             // BSP nodes
  ML_SECTORS,           // Sectors, from editing
  ML_REJECT,            // LUT, sector-sector visibility
  ML_BLOCKMAP           // LUT, motion clipping, walls/grid element
};


//
// P_LoadSegs
//

static void P_LoadSegs (int16_t lump)
{
    _g_segs = (const seg_t __far*)W_GetMapLumpByNum(lump);
}

//
// P_LoadSubsectors
//

// SubSector, as generated by BSP.

static void P_LoadSubsectors (int16_t lump)
{
  numsubsectors = W_MapLumpLength (lump) / sizeof(mapsubsector_t);
  _g_mapsubsectors = W_GetMapLumpByNum(lump);
  _g_subsectors = Z_CallocLevel(numsubsectors * sizeof(subsector_t));
}

//
// P_LoadSectors
//

// Sector definition, from editing.
#if defined FLAT_SPAN
#define R_FlatNumForFarName(p) (p)
#else
typedef struct {
  int16_t floorheight;
  int16_t ceilingheight;
  char  floorpic[8];
  char  ceilingpic[8];
  uint8_t lightlevel;
  int8_t special;
  int16_t tag;
} mapsector_t;

typedef char assertMapsectorSize[sizeof(mapsector_t) == 24 ? 1 : -1];

static int16_t R_FlatNumForFarName(const char __far* far_name)
{
	char near_name[8];
	_fmemcpy(near_name, far_name, sizeof(near_name));
	return R_FlatNumForName(near_name);
}
#endif


static void P_LoadSectors (int16_t lump)
{
  int16_t  i;

#if defined NEOGEO_COMPACT_MSECNODES
  const uint16_t sector_count =
    W_MapLumpLength(lump) / sizeof(mapsector_t);
  const uint32_t sector_bytes = (uint32_t)sector_count * sizeof(sector_t);
  if (!sector_count
      || sector_count > INT16_MAX
      || sector_bytes > MSECNODE_SECTOR_MAX_BYTES)
    I_Error("P_LoadSectors: %u sectors do not fit compact msecnode",
            sector_count);
  _g_numsectors = sector_count;
#else
  _g_numsectors = W_MapLumpLength (lump) / sizeof(mapsector_t);
#endif
  _g_mapsectors = W_GetMapLumpByNum(lump);
  _g_sectors = Z_CallocLevel(_g_numsectors * sizeof(sector_t));

  for (i=0; i<_g_numsectors; i++)
    {
      sector_t __far* ss = _g_sectors + i;
      const mapsector_t __far* ms = _g_mapsectors + i;

      ss->floorheight = ((int32_t)SHORT(ms->floorheight))<<FRACBITS;
      ss->ceilingheight = ((int32_t)SHORT(ms->ceilingheight))<<FRACBITS;
      ss->floorpic   = R_FlatNumForFarName(ms->floorpic);
      ss->ceilingpic = R_FlatNumForFarName(ms->ceilingpic);

      ss->lightlevel = ms->lightlevel;
      ss->special    = ms->special;
      ss->oldspecial = ms->special;

      ss->thinglist = NULL;
      ss->touching_thinglist = MSECNODE_NULL;
    }
}


//
// P_LoadNodes
//

static void P_LoadNodes (int16_t lump)
{
  numnodes = W_MapLumpLength (lump) / sizeof(mapnode_t);
  nodes = W_GetMapLumpByNum(lump);
}


/*
 * P_LoadThings
 *
 */

static void P_LoadThings(int16_t lump, int16_t map)
{
	int16_t mapThingCount = W_MapLumpLength(lump) / sizeof(mapthing_t);

#if defined LOW_MEMORY && defined NEOGEO_SPRITE_MICROFB
	_s_mapThings = W_GetMapLumpByNum(lump);
	_s_mapThingCount = mapThingCount;
	_s_activeMapThings = 0;
	_s_streamMapThings = map != 1 && map != 8;
	_s_mapThingStates = NULL;
	_s_poolMapThing = NULL;

	if (_s_streamMapThings)
	{
		const uint16_t state_bytes = ((uint16_t)mapThingCount + 3u) / 4u;
		_s_mapThingStates = Z_CallocLevel(state_bytes);

		for (int16_t i = 0; i < mapThingCount; i++)
		{
			const mobjtype_t type = P_MapThingMobjType(&_s_mapThings[i]);
			if (type == MT_NOTHING)
			{
				P_SetMapThingState(i, MAPTHING_SKIPPED);
				continue;
			}

			if (type != MT_PLAYER)
			{
				if (mobjinfo[type].flags & MF_COUNTKILL)
					_g_totalkills++;
				if (mobjinfo[type].flags & MF_COUNTITEM)
					_g_totalitems++;
			}
		}

		_g_thingPool = NULL;
		_g_thingPoolSize = 0;
		return;
	}

	_g_thingPoolSize = mapThingCount;
#elif defined LOW_MEMORY
	if (map == 1 || map == 8)
	{
		_g_thingPoolSize = mapThingCount;
	}
	else
	{
		const mapthing_t *data = W_GetMapLumpByNum(lump);

		_g_thingPoolSize = 0;
		for (int16_t i = 0; i < mapThingCount; i++)
		{
			const mapthing_t *mt = &data[i];
			if (mt->type == 1	// start spot player 1
			 || mt->type == 5	// blue keycard
			 || mt->type == 6	// yellow keycard
			 || mt->type == 13)	// red keycard
			 {
				 _g_thingPoolSize++;
			 }
		}
	}
#else
	_g_thingPoolSize = mapThingCount;
#endif

	_g_thingPool = Z_CallocLevel(_g_thingPoolSize * sizeof(mobj_t));
	for (int16_t i = 0; i < _g_thingPoolSize; i++)
		_g_thingPool[i].type = MT_NOTHING;
}


#if defined LOW_MEMORY && defined NEOGEO_SPRITE_MICROFB
static void P_AllocateStreamingThingPool(void)
{
	const uint16_t slot_size = sizeof(mobj_t) + sizeof(uint16_t);
	const uint16_t largest = Z_LargestFreeBlock();

	if (largest <= MAPTHING_RUNTIME_RESERVE + slot_size)
		I_Error("P_LoadThings: only %u B remain for actors", largest);

	uint16_t slots =
		(largest - MAPTHING_RUNTIME_RESERVE) / slot_size;
	const uint16_t wanted = (uint16_t)_s_mapThingCount +
		MAPTHING_TRANSIENT_SLOTS;
	if (slots > wanted)
		slots = wanted;
	if (slots <= MAPTHING_TRANSIENT_SLOTS)
		I_Error("P_LoadThings: only %u actor slots fit", slots);

	const uint16_t pool_bytes = slots * sizeof(mobj_t);
	const uint16_t allocation_bytes = slots * slot_size;
	_g_thingPool = Z_CallocLevel(allocation_bytes);
	_g_thingPoolSize = slots;
	_s_poolMapThing = (uint16_t __far*)
		((uint8_t __far*)_g_thingPool + pool_bytes);

	for (uint16_t i = 0; i < slots; i++)
	{
		_g_thingPool[i].type = MT_NOTHING;
		_s_poolMapThing[i] = NO_INDEX;
	}

	printf("Map things: %d in ROM, %u live slots, %u B free\n",
		_s_mapThingCount, slots, Z_LargestFreeBlock());
}


static boolean P_ActivateMapThing(int16_t index, mobjtype_t type)
{
	if (type != MT_PLAYER && type != MT_TELEPORTMAN
	 && P_ThingPoolFreeSlots() <= MAPTHING_TRANSIENT_SLOTS)
		return false;

	mobj_t __far* mobj = P_SpawnMapThing(&_s_mapThings[index], type, false);
	if (!mobj)
		return false;

	P_AssociateMapThing(mobj, index);
	return true;
}


static void P_ActivateNearbyMapThings(void)
{
	while (P_ThingPoolFreeSlots() > MAPTHING_TRANSIENT_SLOTS)
	{
		int16_t nearest = -1;
		int32_t nearest_distance = MAPTHING_ACTIVATE_DISTANCE + 1;

		for (int16_t i = 0; i < _s_mapThingCount; i++)
		{
			if (P_GetMapThingState(i) != MAPTHING_DORMANT)
				continue;

			const int32_t distance = P_MapThingDistance(&_s_mapThings[i]);
			if (distance < nearest_distance)
			{
				nearest = i;
				nearest_distance = distance;
			}
		}

		if (nearest < 0)
			return;

		const mobjtype_t type = P_MapThingMobjType(&_s_mapThings[nearest]);
		if (!P_ActivateMapThing(nearest, type))
			return;
	}
}


static void P_LoadStreamingThings(void)
{
	for (int16_t i = 0; i < _s_mapThingCount; i++)
	{
		if (P_GetMapThingState(i) != MAPTHING_DORMANT)
			continue;

		const mobjtype_t type = P_MapThingMobjType(&_s_mapThings[i]);
		if (type == MT_PLAYER)
		{
			P_ActivateMapThing(i, type);
			break;
		}
	}

	if (!_g_player.mo)
		I_Error("P_LoadThings: player start not found");

	for (int16_t i = 0; i < _s_mapThingCount; i++)
	{
		if (P_GetMapThingState(i) != MAPTHING_DORMANT)
			continue;

		const mobjtype_t type = P_MapThingMobjType(&_s_mapThings[i]);
		if (type == MT_TELEPORTMAN && !P_ActivateMapThing(i, type))
			break;
	}

	P_ActivateNearbyMapThings();
}
#endif


static void P_LoadThings2(int16_t lump, int16_t map)
{
	int16_t mapThingCount = W_MapLumpLength(lump) / sizeof(mapthing_t);
	const mapthing_t __far* data = W_GetMapLumpByNum(lump);

#if defined LOW_MEMORY && defined NEOGEO_SPRITE_MICROFB
	if (_s_streamMapThings)
	{
		P_LoadStreamingThings();
		return;
	}
#endif

	for (int16_t i = 0; i < mapThingCount; i++)
	{
		const mapthing_t __far* mt = &data[i];
		const mobjtype_t type = P_MapThingMobjType(mt);

#if defined LOW_MEMORY
		if (map == 1 || map == 8)
		{
			P_SpawnMapThing(mt, type, true);
		}
		else
		{
			if (mt->type == 1	// start spot player 1
			 || mt->type == 5	// blue keycard
			 || mt->type == 6	// yellow keycard
			 || mt->type == 13)	// red keycard
			 {
				 P_SpawnMapThing(mt, type, true);
			 }
		}
#else
		// Do spawn all other stuff.
		P_SpawnMapThing(mt, type, true);
#endif
	}
}


void P_MapThingRemoved(const mobj_t __far* mobj)
{
#if defined LOW_MEMORY && defined NEOGEO_SPRITE_MICROFB
	if (!_s_streamMapThings || !_s_poolMapThing
	 || mobj < _g_thingPool || mobj >= _g_thingPool + _g_thingPoolSize)
		return;

	const int16_t pool_index = mobj - _g_thingPool;
	const uint16_t map_index = _s_poolMapThing[pool_index];
	if (map_index == NO_INDEX || map_index >= (uint16_t)_s_mapThingCount)
		return;

	_s_poolMapThing[pool_index] = NO_INDEX;
	if (P_GetMapThingState(map_index) == MAPTHING_ACTIVE)
	{
		P_SetMapThingState(map_index, MAPTHING_CONSUMED);
		_s_activeMapThings--;
	}
#else
	(void)mobj;
#endif
}


void P_UpdateMapThings(void)
{
#if defined LOW_MEMORY && defined NEOGEO_SPRITE_MICROFB
	if (!_s_streamMapThings || !_g_player.mo)
		return;

	for (int16_t i = 0; i < _g_thingPoolSize; i++)
	{
		const uint16_t map_index = _s_poolMapThing[i];
		if (map_index == NO_INDEX || map_index >= (uint16_t)_s_mapThingCount)
			continue;

		mobj_t __far* mobj = &_g_thingPool[i];
		const mapthing_t __far* mt = &_s_mapThings[map_index];
		if (P_MapThingDistance(mt) <= MAPTHING_DEACTIVATE_DISTANCE)
			continue;
		if (mobj->type == MT_PLAYER || mobj->type == MT_TELEPORTMAN)
			continue;

		uint8_t state = MAPTHING_CONSUMED;
		if (mobj->health > 0)
		{
			const fixed_t spawn_x = ((int32_t)mt->x) << FRACBITS;
			const fixed_t spawn_y = ((int32_t)mt->y) << FRACBITS;

			if (mobj->target || mobj->lastenemy
			 || mobj->momx || mobj->momy || mobj->momz
			 || mobj->x != spawn_x || mobj->y != spawn_y
			 || mobj->health != mobjinfo[mobj->type].spawnhealth)
				continue;

			state = MAPTHING_DORMANT;
			if (mobj->flags & MF_COUNTKILL)
				_g_totallive--;
		}

		_s_poolMapThing[i] = NO_INDEX;
		P_SetMapThingState(map_index, state);
		_s_activeMapThings--;
		P_RemoveMobj(mobj);
	}

	P_ActivateNearbyMapThings();
#endif
}

//
// P_LoadLineDefs
// Also counts secret lines for intermissions.
//        ^^^
// ??? killough ???
// Does this mean secrets used to be linedef-based, rather than sector-based?
//

static void P_LoadLineDefs (int16_t lump)
{
	_g_numlines = W_MapLumpLength(lump) / sizeof(line_t);
	_g_maplines = W_GetMapLumpByNum(lump);
#if defined NEOGEO_COMPACT_LINESTATE
	_g_lines = Z_MallocLevel(
		_g_numlines * (sizeof(linedata_t) + sizeof(uint8_t)), NULL);
	_g_lineRenderValid = (uint8_t __far*)_g_lines
		+ _g_numlines * sizeof(linedata_t);
	_fmemset(_g_lineRenderValid, 0xff, _g_numlines);
#else
	_g_lines    = Z_MallocLevel(_g_numlines * sizeof(linedata_t), NULL);
#endif

	for (int16_t i = 0; i < _g_numlines; i++)
	{
		_g_lines[i].validcount   = 0;
#if !defined NEOGEO_COMPACT_LINESTATE
		_g_lines[i].r_validcount = 0;
#endif
		_g_lines[i].r_flags      = 0;
		_g_lines[i].special      = _g_maplines[i].const_special;
	}
}


//
// P_LoadSideDefs
//

static void P_LoadSideDefs (int16_t lump)
{
  numsides = W_MapLumpLength(lump) / sizeof(mapsidedef_t);
  _g_mapsides = W_GetMapLumpByNum(lump);
  _g_sides = Z_CallocLevel(numsides * sizeof(side_t));

    for (int16_t i = 0; i < numsides; i++)
    {
        const mapsidedef_t __far* msd = _g_mapsides + i;
        side_t __far* sd = _g_sides + i;

        sd->textureoffset = msd->textureoffset;
        sd->sector        = msd->sector;
        sd->midtexture    = msd->midtexture;
        sd->toptexture    = msd->toptexture;
        sd->bottomtexture = msd->bottomtexture;

        P_LoadTexture(sd->midtexture);
        P_LoadTexture(sd->toptexture);
        P_LoadTexture(sd->bottomtexture);
    }
}


//
// P_LoadBlockMap
//
// killough 3/1/98: substantially modified to work
// towards removing blockmap limit (a wad limitation)
//
// killough 3/30/98: Rewritten to remove blockmap limit,
// though current algorithm is brute-force and unoptimal.
//

static void P_LoadBlockMap (int16_t lump)
{
    _g_blockmaplump = W_GetMapLumpByNum(lump);

    _g_bmaporgx = ((int32_t)_g_blockmaplump[0])<<FRACBITS;
    _g_bmaporgy = ((int32_t)_g_blockmaplump[1])<<FRACBITS;
    _g_bmapwidth  = _g_blockmaplump[2];
    _g_bmapheight = _g_blockmaplump[3];


    // clear out mobj chains - CPhipps - use NO_INDEX as the empty head
    const uint16_t blocklinksize = _g_bmapwidth * _g_bmapheight * sizeof(*_g_blocklinks);
    _g_blocklinks = Z_MallocLevel(blocklinksize, NULL);
    _fmemset(_g_blocklinks, 0xff, blocklinksize);

    _g_blockmap = _g_blockmaplump+4;
}

//
// P_LoadReject - load the reject table
// 

static void P_LoadReject(int16_t lump)
{
  _g_rejectmatrix = W_GetMapLumpByNum(lump);
}

//
// P_GroupLines
// Builds sector line lists and subsector sector numbers.
// Finds block bounding boxes for sectors.
//
// killough 5/3/98: reformatted, cleaned up
// cph 18/8/99: rewritten to avoid O(numlines * numsectors) section
// It makes things more complicated, but saves seconds on big levels
// figgi 09/18/00 -- adapted for gl-nodes

// cph - convenient sub-function
#if !defined NEOGEO_ROM_SECTOR_LINES
static void P_AddLineToSector(const line_t __far* li, sector_t __far* sector)
{
  sector->lines[sector->linecount++] = li->lineno;
}
#endif

static void M_ClearBox (fixed_t *box)
{
    box[BOXTOP]    = box[BOXRIGHT] = INT32_MIN;
    box[BOXBOTTOM] = box[BOXLEFT]  = INT32_MAX;
}

static void M_AddToBox(fixed_t* box,fixed_t x,fixed_t y)
{
    if (x<box[BOXLEFT])
        box[BOXLEFT]  = x;
    else if (x>box[BOXRIGHT])
        box[BOXRIGHT] = x;

    if (y<box[BOXBOTTOM])
        box[BOXBOTTOM] = y;
    else if (y>box[BOXTOP])
        box[BOXTOP]    = y;
}

static void P_GroupLines (void)
{
    sector_t __far* sector;
    int16_t i;
    int16_t j;

    // figgi
    for (i=0 ; i<numsubsectors ; i++)
    {
        const seg_t __far* seg = &_g_segs[_g_mapsubsectors[i].firstseg];
        _g_subsectors[i].sector = 0;
        for(j=0; j<_g_mapsubsectors[i].numsegs; j++)
        {
            if(seg->sidenum != NO_INDEX)
            {
                _g_subsectors[i].sector = _g_sides[seg->sidenum].sector;
                break;
            }
            seg++;
        }
    }

#if !defined NEOGEO_ROM_SECTOR_LINES
    const line_t __far* li;
    int16_t total = _g_numlines;

    // count number of lines in each sector
    for (i=0,li=_g_maplines; i<_g_numlines; i++, li++)
    {
        LN_FRONTSECTOR(li)->linecount++;
        if (LN_BACKSECTOR(li) && LN_BACKSECTOR(li) != LN_FRONTSECTOR(li))
        {
            LN_BACKSECTOR(li)->linecount++;
            total++;
        }
    }

    {  // allocate line tables for each sector
        uint16_t __far* linebuffer = Z_MallocLevel(total*sizeof(*linebuffer), NULL);

        for (i=0, sector = _g_sectors; i<_g_numsectors; i++, sector++)
        {
            sector->lines = linebuffer;
            linebuffer += sector->linecount;
            sector->linecount = 0;
        }
    }

    // Enter those lines
    for (i=0,li=_g_maplines; i<_g_numlines; i++, li++)
    {
        P_AddLineToSector(li, LN_FRONTSECTOR(li));
        if (LN_BACKSECTOR(li) && LN_BACKSECTOR(li) != LN_FRONTSECTOR(li))
            P_AddLineToSector(li, LN_BACKSECTOR(li));
    }
#endif

    for (i=0, sector = _g_sectors; i<_g_numsectors; i++, sector++)
    {
        fixed_t bbox[4];
        M_ClearBox(bbox);

        for(int16_t l = 0; l < SECTOR_LINECOUNT(sector); l++)
        {
            const line_t __far* line = SECTOR_LINE(sector, l);
            M_AddToBox (bbox, (fixed_t)line->v1.x<<FRACBITS, (fixed_t)line->v1.y<<FRACBITS);
            M_AddToBox (bbox, (fixed_t)line->v2.x<<FRACBITS, (fixed_t)line->v2.y<<FRACBITS);
        }

#if defined NEOGEO_COMPACT_SECTORS
        sector->soundorg.x =
            (bbox[BOXRIGHT]/2+bbox[BOXLEFT]/2) >> FRACBITS;
        sector->soundorg.y =
            (bbox[BOXTOP]/2+bbox[BOXBOTTOM]/2) >> FRACBITS;
#else
        sector->soundorg.x = bbox[BOXRIGHT]/2+bbox[BOXLEFT]/2;
        sector->soundorg.y = bbox[BOXTOP]/2+bbox[BOXBOTTOM]/2;
#endif
    }
}


static void P_FreeLevelData()
{
#if !defined FLAT_SPAN
    R_ResetPlanes();
#endif

    Z_FreeTags();
}

//
// P_SetupLevel
//

void P_SetupLevel(int16_t map)
{
    int_fast8_t   i;
    char  lumpname[9];
    int16_t   lumpnum;

    _g_totallive = _g_totalkills = _g_totalitems = _g_totalsecret = 0;
    _g_wminfo.partime = 180;

    for (i=0; i<MAXPLAYERS; i++)
        _g_player.killcount = _g_player.secretcount = _g_player.itemcount = 0;

    // Initial height of PointOfView will be set by player think.
    _g_player.viewz = 1;

    // Make sure all sounds are stopped before Z_FreeTags.
    S_Start();

    P_FreeLevelData();

    P_InitThinkers();

    _g_leveltime = 0;
    _g_totallive = 0;

    // find map name
#if defined LOW_MEMORY && !defined NEOGEO_SPRITE_MICROFB
    if (map == 6)
        map = 1;
#endif
    sprintf(lumpname, "E1M%d", map);   // killough 1/24/98: simplify

    lumpnum = W_GetMapNumForName(lumpname);

#if defined NEOGEO_ROM_SECTOR_LINES
    const doom_sector_line_map_t __far* sector_line_map =
        &doom_sector_line_maps[map - 1];
    _g_sectorLineIndices =
        &doom_sector_line_indices[sector_line_map->line_base];
    _g_sectorLineSpans =
        &doom_sector_line_spans[sector_line_map->span_base];
#endif

    P_LoadThings    (lumpnum + ML_THINGS, map);
    P_LoadSectors   (lumpnum + ML_SECTORS);
    P_LoadLineDefs  (lumpnum + ML_LINEDEFS);
    P_LoadSegs      (lumpnum + ML_SEGS);
    P_LoadBlockMap  (lumpnum + ML_BLOCKMAP);
    P_LoadNodes     (lumpnum + ML_NODES);
    P_LoadSideDefs  (lumpnum + ML_SIDEDEFS);
    P_LoadReject    (lumpnum + ML_REJECT);
    P_LoadSubsectors(lumpnum + ML_SSECTORS);

    P_GroupLines();

    // Note: you don't need to clear player queue slots
    // a much simpler fix is in g_game.c

    for (i = 0; i < MAXPLAYERS; i++)
        _g_player.mo = NULL;

#if defined LOW_MEMORY && defined NEOGEO_SPRITE_MICROFB
    if (_s_streamMapThings)
    {
        // Allocate persistent level thinkers before using the remaining
        // contiguous level memory for the bounded live-actor working set.
        P_SpawnSpecials();
        P_AllocateStreamingThingPool();
        P_LoadThings2(lumpnum + ML_THINGS, map);
#if defined NEOGEO_MAP_REPORT
        I_Error("Map %d: pool %d active %d free %d heap %u",
            map, _g_thingPoolSize, _s_activeMapThings,
            P_ThingPoolFreeSlots(), Z_LargestFreeBlock());
#endif
    }
    else
    {
        P_LoadThings2(lumpnum + ML_THINGS, map);
        P_SpawnSpecials();
    }
#else
    P_LoadThings2(lumpnum + ML_THINGS, map);

    // set up world state
    P_SpawnSpecials();
#endif

    P_MapEnd();
}

//
// P_Init
//
void P_Init (void)
{
    P_InitSwitchList();
    P_InitPicAnims();
    R_InitSprites();
}
