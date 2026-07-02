/*-----------------------------------------------------------------------------
 *
 *
 *  Copyright (C) 2026 Frenkel Smeijers
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
 *      Neo Geo video code.
 *
 *      This backend no longer uses the FIX layer as the game framebuffer.
 *      The Doom renderer uses a runtime-selected logical framebuffer size up
 *      to 80x56.  Sprite output displays that active area as 4x4, 6x6, or 8x8
 *      hardware-sprite microcells:
 *
 *          4x4: 160 sprite strips, 80 active per scanline = 320x224 pixels
 *          6x6: 106 sprite strips, 53 active per scanline = 318x222 pixels
 *          8x8:  80 sprite strips, 40 active per scanline = 320x224 pixels
 *      The full-color path uses one shrunk 16x16 C-ROM tile per logical
 *      pixel.  The hardware shrinker reduces each tile to the selected cell.
 *      Per-tile sprite attributes select repacked PLAYPAL sprite palettes,
 *      preserving the original 8-bit Doom palette colors.
 *
 *-----------------------------------------------------------------------------*/

#include <stdint.h>
#include <string.h>
#include <ngdevkit/registers.h>

#include "compiler.h"

#include "i_system.h"
#include "i_video.h"
#include "i_vtext.h"
#include "m_random.h"
#include "r_defs.h"
#include "v_video.h"
#include "w_wad.h"

#include "globdata.h"


extern int16_t CENTERY;

/*
 * Neo Geo VRAM sprite control blocks.  ngdevkit exposes the VRAM registers;
 * using the raw SCB addresses here keeps the renderer independent from the
 * higher-level sprite helpers.
 */
/* The BIOS/ngdevkit eyecatcher uses the first C-ROM tiles. */
#define MICROFB_TILE_BASE 256u
#define MICROFB_TILE_BLANK MICROFB_TILE_BASE

/* Sprite pixels are 4bpp and pixel index 0 is transparent.  The renderer
 * therefore stores PLAYPAL colors in 15 visible slots per sprite palette and
 * uses extra sprite palettes starting at palette 16, leaving the original first
 * 16 palettes untouched for the FIX overlay.
 */
#define MICROFB_VISIBLE_COLOR_SLOTS 15u
#define MICROFB_SPRITE_PALETTE_BASE 16u
#define MICROFB_SPRITE_PALETTES ((256u + MICROFB_VISIBLE_COLOR_SLOTS - 1u) / MICROFB_VISIBLE_COLOR_SLOTS)

#define MICROFB_DISPLAY_W 320u
#define MICROFB_DISPLAY_H 224u
#define MICROFB_COLUMN_CHUNKS 2u
#define MICROFB_FRAMEBUFFER_SETS 2u
#define MICROFB_SPRITE_BASE 1u
#define MICROFB_X_WORD(x) (((uint16_t)(x)) << 7)
#define MICROFB_PALETTE_ATTR(pal) ((uint16_t)(pal) << 8)

#define FIX_OVERLAY_WIDTH 38
#define FIX_OVERLAY_HEIGHT 28
#define FIX_CLEAR_CHAR ' '
#define FIX_WIPE_WIDTH 40
#define FIX_WIPE_HEIGHT 28
#define FIX_WIPE_Y_OFFSET 2
#define FIX_WIPE_CHAR 0x00u

static const uint16_t colors[] =
{
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	0x2000, // 4 red
	0x0000,
	0x0000,
	0x6000, // 7 light gray
	0x9000, // 8 dark gray
	0xc000, // 9 light blue
	0x0000,
	0x0000,
	0xb000, // 12 light red
	0x0000,
	0xa000, // 14 yellow
	0x3000  // 15 white
};

#if (VIEWWINDOWHEIGHT % MICROFB_COLUMN_CHUNKS) != 0
#error Neo Geo sprite microframebuffer requires VIEWWINDOWHEIGHT divisible by MICROFB_COLUMN_CHUNKS
#endif

#if VIEWWINDOWWIDTH > 96
#error Neo Geo sprite microframebuffer cannot exceed 96 logical columns because of the per-scanline sprite limit
#endif

#if (VIEWWINDOWWIDTH * 4) != MICROFB_DISPLAY_W || (VIEWWINDOWHEIGHT * 4) != MICROFB_DISPLAY_H
#error Neo Geo sprite microframebuffer runtime modes assume an 80x56 source framebuffer
#endif

#define MICROFB_CHUNK_CELLS (VIEWWINDOWHEIGHT / MICROFB_COLUMN_CHUNKS)
#define MICROFB_SPRITES_PER_SET (VIEWWINDOWWIDTH * MICROFB_COLUMN_CHUNKS)
#define MICROFB_SPRITE_COUNT (MICROFB_SPRITES_PER_SET * MICROFB_FRAMEBUFFER_SETS)

#if MICROFB_CHUNK_CELLS > 32
#error Neo Geo sprite microframebuffer chunk cannot exceed 32 source tiles
#endif

#if (MICROFB_SPRITE_BASE + MICROFB_SPRITE_COUNT) > 381
#error Neo Geo sprite microframebuffer double buffer exceeds displayable sprite budget
#endif

#if (MICROFB_SPRITE_PALETTE_BASE + MICROFB_SPRITE_PALETTES) > 256
#error Neo Geo sprite microframebuffer exceeds sprite palette budget
#endif

typedef struct
{
	const char *name;
	uint8_t cell_px;
	uint8_t cols;
	uint8_t rows;
	uint16_t shrink_word;
	uint16_t x_offset;
	uint16_t y_offset;
} microfb_mode_t;

static const microfb_mode_t microfb_modes[] =
{
	{ "Low", 8, 40, 28, 0x077fu, 0, 0 },
	{ "Medium", 6, 53, 37, 0x055fu, 1, 1 },
	{ "High", 4, 80, 56, 0x033fu, 0, 0 }
};

#define MICROFB_MODE_COUNT (sizeof(microfb_modes) / sizeof(microfb_modes[0]))
#define MICROFB_DEFAULT_MODE_INDEX (MICROFB_MODE_COUNT - 1u)


static uint8_t _s_screen[VIEWWINDOWWIDTH * VIEWWINDOWHEIGHT];
static uint8_t _s_color_to_tile_slot[256];
static uint8_t _s_color_to_palette[256];
static uint8_t _s_visible_sprite_set;
static uint8_t _s_microfb_mode_index;
static uint8_t _s_pending_microfb_mode_index;

static int16_t palettelumpnum;
static int8_t newpal = 100;


static uint16_t NG_SpriteYWord(uint16_t y, uint16_t height_tiles)
{
	return ((((uint16_t)(496u - y)) & 0x01ffu) << 7) | (height_tiles & 0x003fu);
}


static uint8_t NG_InVBlank(void)
{
	return (*REG_LSPCMODE & 0x8000u) == 0;
}


static uint8_t NG_WaitVBlankStart(void)
{
	const uint8_t overrun = NG_InVBlank();
	while (NG_InVBlank())
	{
	}
	while (!NG_InVBlank())
	{
	}
	return overrun;
}


static uint16_t NG_MicroSpriteIndex(uint16_t set, uint16_t chunk, uint16_t x)
{
	return MICROFB_SPRITE_BASE + set * MICROFB_SPRITES_PER_SET + chunk * VIEWWINDOWWIDTH + x;
}


static const microfb_mode_t *NG_MicroFramebufferMode(void)
{
	return &microfb_modes[_s_microfb_mode_index];
}


static void NG_RescaleMicroFramebuffer(const microfb_mode_t *old_mode, const microfb_mode_t *new_mode)
{
	if (new_mode->cols >= old_mode->cols)
	{
		for (int16_t y = new_mode->rows - 1; y >= 0; y--)
		{
			const uint16_t src_y = ((uint16_t)y * old_mode->rows) / new_mode->rows;
			for (int16_t x = new_mode->cols - 1; x >= 0; x--)
			{
				const uint16_t src_x = ((uint16_t)x * old_mode->cols) / new_mode->cols;
				_s_screen[y * VIEWWINDOWWIDTH + x] = _s_screen[src_y * VIEWWINDOWWIDTH + src_x];
			}
		}
	}
	else
	{
		for (uint16_t y = 0; y < new_mode->rows; y++)
		{
			const uint16_t src_y = (y * old_mode->rows) / new_mode->rows;
			for (uint16_t x = 0; x < new_mode->cols; x++)
			{
				const uint16_t src_x = (x * old_mode->cols) / new_mode->cols;
				_s_screen[y * VIEWWINDOWWIDTH + x] = _s_screen[src_y * VIEWWINDOWWIDTH + src_x];
			}
		}
	}
}


static void NG_ApplyMicroFramebufferMode(uint8_t mode_index)
{
	if (mode_index != _s_microfb_mode_index)
	{
		const microfb_mode_t *previous_mode = &microfb_modes[_s_microfb_mode_index];

		_s_microfb_mode_index = mode_index;

		const microfb_mode_t *mode = NG_MicroFramebufferMode();
		NG_RescaleMicroFramebuffer(previous_mode, mode);
		R_SetRenderSize(mode->cols, mode->rows);
	}
}


static uint16_t NG_MicroFramebufferChunkRows(const microfb_mode_t *mode, uint16_t chunk)
{
	const uint16_t row_base = chunk * MICROFB_CHUNK_CELLS;

	if (row_base >= mode->rows)
		return 0;

	const uint16_t rows_left = mode->rows - row_base;
	return rows_left > MICROFB_CHUNK_CELLS ? MICROFB_CHUNK_CELLS : rows_left;
}


static void NG_BuildSpritePalettes(const uint16_t *src)
{
	for (uint16_t pal = 0; pal < MICROFB_SPRITE_PALETTES; pal++)
	{
		const uint16_t hwpal = MICROFB_SPRITE_PALETTE_BASE + pal;
		uint16_t *dst = (uint16_t*)&MMAP_PALBANK1[hwpal * 16u];

		dst[0] = 0x8000; /* sprite pixel 0 is transparent; keep it black. */

		for (uint16_t slot = 1; slot <= MICROFB_VISIBLE_COLOR_SLOTS; slot++)
		{
			const uint16_t color = pal * MICROFB_VISIBLE_COLOR_SLOTS + (slot - 1u);
			dst[slot] = color < 256u ? src[color] : 0x8000;
		}
	}

	for (uint16_t color = 0; color < 256u; color++)
	{
		const uint16_t pal = color / MICROFB_VISIBLE_COLOR_SLOTS;
		const uint16_t slot = (color % MICROFB_VISIBLE_COLOR_SLOTS) + 1u;

		_s_color_to_tile_slot[color] = slot;
		_s_color_to_palette[color] = MICROFB_SPRITE_PALETTE_BASE + pal;
	}
}


void I_ReloadPalette(void)
{
	char lumpName[8] = "PLAYPAL";
	if (_g_gamma != 0)
	{
		lumpName[7] = '0' + _g_gamma;
	}

	palettelumpnum = W_GetNumForName(lumpName);
}


static void I_UploadNewPalette(int8_t pal)
{
	const uint16_t *palette_lump = W_GetLumpByNum(palettelumpnum);
	const uint16_t *src = &palette_lump[256 * pal];

	memcpy((uint8_t*)&MMAP_PALBANK1[0], src, 256 * 2);

	NG_BuildSpritePalettes(src);
}


static void NG_ClearFixOverlay(void)
{
	*REG_VRAMMOD = 32;
	for (uint16_t y = 0; y < 32; y++)
	{
		*REG_VRAMADDR = ADDR_FIXMAP + y;
		for (uint16_t x = 0; x < 40; x++)
			*REG_VRAMRW = FIX_CLEAR_CHAR;
	}
}


static void NG_UploadFixOverlay(void)
{
	/* FIX text is written directly to VRAM to keep the sprite backend out of
	 * the Neo Geo's tight 64 KB work RAM.
	 */
}


static void NG_ClearSpriteState(void)
{
	/* Hide the displayable sprite range in case the BIOS/eyecatcher left sprite
	 * data behind.  Size 0 is enough to remove the strip from the display list.
	 */
	*REG_VRAMMOD = 0x200;
	for (uint16_t i = 0; i < 381; i++)
	{
		*REG_VRAMADDR = ADDR_SCB2 + i;
		*REG_VRAMRW = 0x0000;
		*REG_VRAMRW = 0x0000;
		*REG_VRAMRW = 0x0000;
	}
}


static void NG_SetMicroSpriteSetVisible(uint16_t set, uint8_t visible)
{
	const microfb_mode_t *mode = NG_MicroFramebufferMode();

	*REG_VRAMMOD = 0x200;
	for (uint16_t chunk = 0; chunk < MICROFB_COLUMN_CHUNKS; chunk++)
	{
		const uint16_t row_base = chunk * MICROFB_CHUNK_CELLS;
		const uint16_t chunk_rows = NG_MicroFramebufferChunkRows(mode, chunk);
		const uint16_t y = mode->y_offset + row_base * mode->cell_px;
		const uint16_t height_word = visible && chunk_rows ? NG_SpriteYWord(y, chunk_rows) : 0u;

		for (uint16_t x = 0; x < VIEWWINDOWWIDTH; x++)
		{
			const uint16_t sprite = NG_MicroSpriteIndex(set, chunk, x);
			const uint8_t active = x < mode->cols && height_word != 0u;

			*REG_VRAMADDR = ADDR_SCB2 + sprite;
			*REG_VRAMRW = mode->shrink_word;
			*REG_VRAMRW = active ? height_word : 0u;
			*REG_VRAMRW = active ? MICROFB_X_WORD(mode->x_offset + x * mode->cell_px) : 0u;
		}
	}
}


static void NG_InitMicroSprites(void)
{
	NG_ClearSpriteState();

	/* Two complete sprite-framebuffer sets are reserved.  Only one set is visible
	 * while the next frame is uploaded into the hidden set.
	 */
	*REG_VRAMMOD = 1;
	for (uint16_t s = 0; s < MICROFB_SPRITE_COUNT; s++)
	{
		const uint16_t sprite = MICROFB_SPRITE_BASE + s;
		*REG_VRAMADDR = ADDR_SCB1 + (sprite * 64u);
		for (uint16_t t = 0; t < 32; t++)
		{
			*REG_VRAMRW = MICROFB_TILE_BLANK;
			*REG_VRAMRW = 0x0000;
		}
	}

	_s_visible_sprite_set = 0;
	NG_SetMicroSpriteSetVisible(0, true);
	NG_SetMicroSpriteSetVisible(1, false);
}


void I_InitGraphicsHardwareSpecificCode(void)
{
	I_ReloadPalette();
	I_UploadNewPalette(0);

	_s_microfb_mode_index = MICROFB_DEFAULT_MODE_INDEX;
	_s_pending_microfb_mode_index = _s_microfb_mode_index;
	R_SetRenderSize(microfb_modes[MICROFB_DEFAULT_MODE_INDEX].cols, microfb_modes[MICROFB_DEFAULT_MODE_INDEX].rows);
	memset(_s_screen, 0, sizeof(_s_screen));
	NG_ClearFixOverlay();
	NG_InitMicroSprites();
	NG_UploadFixOverlay();
}


void I_ShutdownGraphics(void)
{
	// Do nothing
}


void I_SetPalette(int8_t p)
{
	newpal = p;
}


void V_SetSTPalette(void)
{
	// Do nothing
}


#define NO_PALETTE_CHANGE 100

static void NG_UploadMicroFramebuffer(uint8_t set)
{
	const microfb_mode_t *mode = NG_MicroFramebufferMode();

	*REG_VRAMMOD = 1;
	for (uint16_t chunk = 0; chunk < MICROFB_COLUMN_CHUNKS; chunk++)
	{
		const uint16_t row_base = chunk * MICROFB_CHUNK_CELLS;
		const uint16_t chunk_rows = NG_MicroFramebufferChunkRows(mode, chunk);

		if (!chunk_rows)
			continue;

		for (uint16_t x = 0; x < mode->cols; x++)
		{
			const uint16_t sprite = NG_MicroSpriteIndex(set, chunk, x);

			*REG_VRAMADDR = ADDR_SCB1 + (sprite * 64u);
			for (uint16_t row = 0; row < chunk_rows; row++)
			{
				const uint8_t color = _s_screen[(row_base + row) * VIEWWINDOWWIDTH + x];
				*REG_VRAMRW = MICROFB_TILE_BASE + _s_color_to_tile_slot[color];
				*REG_VRAMRW = MICROFB_PALETTE_ATTR(_s_color_to_palette[color]);
			}
		}
	}
}


void I_NeoGeoChangeSpriteQuality(int16_t direction)
{
	if (direction < 0)
	{
		if (_s_pending_microfb_mode_index)
			_s_pending_microfb_mode_index--;
	}
	else if (_s_pending_microfb_mode_index + 1u < MICROFB_MODE_COUNT)
	{
		_s_pending_microfb_mode_index++;
	}
}


const char *I_NeoGeoSpriteQualityName(void)
{
	return microfb_modes[_s_pending_microfb_mode_index].name;
}


void I_FinishUpdate(void)
{
	if (newpal != NO_PALETTE_CHANGE)
	{
		I_UploadNewPalette(newpal);
		newpal = NO_PALETTE_CHANGE;
	}

	const uint8_t next_sprite_set = _s_visible_sprite_set ^ 1u;
	NG_UploadMicroFramebuffer(next_sprite_set);
	NG_WaitVBlankStart();
	NG_SetMicroSpriteSetVisible(_s_visible_sprite_set, false);
	NG_SetMicroSpriteSetVisible(next_sprite_set, true);
	_s_visible_sprite_set = next_sprite_set;
	NG_UploadFixOverlay();

	NG_ApplyMicroFramebufferMode(_s_pending_microfb_mode_index);
}


static const int LUTY[VIEWWINDOWHEIGHT] = {
	 0 * VIEWWINDOWWIDTH,  1 * VIEWWINDOWWIDTH,  2 * VIEWWINDOWWIDTH,  3 * VIEWWINDOWWIDTH,
	 4 * VIEWWINDOWWIDTH,  5 * VIEWWINDOWWIDTH,  6 * VIEWWINDOWWIDTH,  7 * VIEWWINDOWWIDTH,
	 8 * VIEWWINDOWWIDTH,  9 * VIEWWINDOWWIDTH, 10 * VIEWWINDOWWIDTH, 11 * VIEWWINDOWWIDTH,
	12 * VIEWWINDOWWIDTH, 13 * VIEWWINDOWWIDTH, 14 * VIEWWINDOWWIDTH, 15 * VIEWWINDOWWIDTH,
	16 * VIEWWINDOWWIDTH, 17 * VIEWWINDOWWIDTH, 18 * VIEWWINDOWWIDTH, 19 * VIEWWINDOWWIDTH,
	20 * VIEWWINDOWWIDTH, 21 * VIEWWINDOWWIDTH, 22 * VIEWWINDOWWIDTH, 23 * VIEWWINDOWWIDTH,
	24 * VIEWWINDOWWIDTH, 25 * VIEWWINDOWWIDTH, 26 * VIEWWINDOWWIDTH, 27 * VIEWWINDOWWIDTH,
	28 * VIEWWINDOWWIDTH, 29 * VIEWWINDOWWIDTH, 30 * VIEWWINDOWWIDTH, 31 * VIEWWINDOWWIDTH,
	32 * VIEWWINDOWWIDTH, 33 * VIEWWINDOWWIDTH, 34 * VIEWWINDOWWIDTH, 35 * VIEWWINDOWWIDTH,
	36 * VIEWWINDOWWIDTH, 37 * VIEWWINDOWWIDTH, 38 * VIEWWINDOWWIDTH, 39 * VIEWWINDOWWIDTH,
	40 * VIEWWINDOWWIDTH, 41 * VIEWWINDOWWIDTH, 42 * VIEWWINDOWWIDTH, 43 * VIEWWINDOWWIDTH,
	44 * VIEWWINDOWWIDTH, 45 * VIEWWINDOWWIDTH, 46 * VIEWWINDOWWIDTH, 47 * VIEWWINDOWWIDTH,
	48 * VIEWWINDOWWIDTH, 49 * VIEWWINDOWWIDTH, 50 * VIEWWINDOWWIDTH, 51 * VIEWWINDOWWIDTH,
	52 * VIEWWINDOWWIDTH, 53 * VIEWWINDOWWIDTH, 54 * VIEWWINDOWWIDTH, 55 * VIEWWINDOWWIDTH
};


#define COLEXTRABITS (8 - 1)
#define COLBITS (8 + 1)


void R_DrawColumnSprite(const draw_column_vars_t *dcvars)
{
	int16_t count = (dcvars->yh - dcvars->yl) + 1;

	if (count <= 0)
		return;

	const uint8_t *source = dcvars->source;
	const uint8_t *colormap = dcvars->colormap;
	uint8_t *dest = (uint8_t*)&_s_screen[LUTY[dcvars->yl] + dcvars->x];

	const uint16_t fracstep = dcvars->fracstep;
	uint16_t frac = (dcvars->texturemid >> COLEXTRABITS) + (dcvars->yl - CENTERY) * fracstep;

	while (count--)
	{
		*dest = colormap[source[frac >> COLBITS]];
		dest += VIEWWINDOWWIDTH;
		frac += fracstep;
	}
}


void R_DrawColumnWall(const draw_column_vars_t *dcvars)
{
	R_DrawColumnSprite(dcvars);
}


void R_DrawColumnFlat(uint8_t color, const draw_column_vars_t *dcvars)
{
	int16_t count = (dcvars->yh - dcvars->yl) + 1;

	if (count <= 0)
		return;

	uint8_t *dest = (uint8_t*)&_s_screen[LUTY[dcvars->yl] + dcvars->x];
	while (count--)
	{
		*dest = color;
		dest += VIEWWINDOWWIDTH;
	}
}


#define FUZZCOLOR1 106
#define FUZZCOLOR2 107
#define FUZZCOLOR3 108
#define FUZZCOLOR4 109
#define FUZZTABLE 50

static const uint8_t fuzzcolors[FUZZTABLE] =
{
	FUZZCOLOR1,FUZZCOLOR2,FUZZCOLOR3,FUZZCOLOR4,FUZZCOLOR1,FUZZCOLOR3,FUZZCOLOR2,
	FUZZCOLOR1,FUZZCOLOR3,FUZZCOLOR4,FUZZCOLOR1,FUZZCOLOR3,FUZZCOLOR1,FUZZCOLOR2,
	FUZZCOLOR3,FUZZCOLOR1,FUZZCOLOR3,FUZZCOLOR4,FUZZCOLOR2,FUZZCOLOR4,FUZZCOLOR2,
	FUZZCOLOR1,FUZZCOLOR4,FUZZCOLOR2,FUZZCOLOR3,FUZZCOLOR1,FUZZCOLOR3,FUZZCOLOR1,FUZZCOLOR4,
	FUZZCOLOR3,FUZZCOLOR2,FUZZCOLOR1,FUZZCOLOR3,FUZZCOLOR4,FUZZCOLOR2,FUZZCOLOR1,
	FUZZCOLOR3,FUZZCOLOR4,FUZZCOLOR2,FUZZCOLOR4,FUZZCOLOR2,FUZZCOLOR1,FUZZCOLOR3,
	FUZZCOLOR1,FUZZCOLOR3,FUZZCOLOR4,FUZZCOLOR1,FUZZCOLOR3,FUZZCOLOR2,FUZZCOLOR1
};


void R_DrawFuzzColumn(const draw_column_vars_t *dcvars)
{
	int16_t count = (dcvars->yh - dcvars->yl) + 1;

	if (count <= 0)
		return;

	uint8_t *dest = (uint8_t*)&_s_screen[LUTY[dcvars->yl] + dcvars->x];

	static int16_t fuzzpos = 0;

	do
	{
		*dest = fuzzcolors[fuzzpos];
		dest += VIEWWINDOWWIDTH;

		fuzzpos++;
		if (fuzzpos >= FUZZTABLE)
			fuzzpos = 0;

	} while (--count);
}


void V_ClearViewWindow(void)
{
	memset(_s_screen, 0, sizeof(_s_screen));
}


void V_InitDrawLine(void)
{
	// Do nothing
}


void V_ShutdownDrawLine(void)
{
	// Do nothing
}


static void NG_PutPixel(int16_t x, int16_t y, uint8_t color)
{
	if ((uint16_t)x < VIEWWINDOWWIDTH && (uint16_t)y < VIEWWINDOWHEIGHT)
		_s_screen[y * VIEWWINDOWWIDTH + x] = color;
}


void V_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color)
{
	int16_t dx = x1 > x0 ? x1 - x0 : x0 - x1;
	int16_t sx = x0 < x1 ? 1 : -1;
	int16_t dy = y1 > y0 ? y0 - y1 : y1 - y0;
	int16_t sy = y0 < y1 ? 1 : -1;
	int16_t err = dx + dy;

	for (;;)
	{
		NG_PutPixel(x0, y0, color);
		if (x0 == x1 && y0 == y1)
			break;
		int16_t e2 = err << 1;
		if (e2 >= dy)
		{
			err += dy;
			x0 += sx;
		}
		if (e2 <= dx)
		{
			err += dx;
			y0 += sy;
		}
	}
}


void V_DrawBackground(int16_t backgroundnum)
{
	UNUSED(backgroundnum);
	memset(_s_screen, 0, sizeof(_s_screen));
	NG_ClearFixOverlay();
}


void V_DrawRawFullScreen(int16_t num)
{
	const microfb_mode_t *mode = NG_MicroFramebufferMode();
	memset(_s_screen, 0, sizeof(_s_screen));

#if defined SHOW_PALETTE
	int i = 0;
	for (int y = 0; y < 16; y++)
		for (int x = 0; x < 16; x++)
			if ((uint16_t)x < mode->cols && (uint16_t)y < mode->rows)
				_s_screen[y * VIEWWINDOWWIDTH + x] = i++;
#else
	const uint8_t *lump = W_GetLumpByNum(num);

	const fixed_t dxi = ((fixed_t)SCREENWIDTH << FRACBITS) / mode->cols;
	const fixed_t dyi = ((fixed_t)SCREENHEIGHT << FRACBITS) / mode->rows;

	fixed_t y = 0;
	for (uint16_t h = 0; h < mode->rows; h++)
	{
		fixed_t x = 0;
		uint8_t *dst = &_s_screen[h * VIEWWINDOWWIDTH];
		for (uint16_t w = 0; w < mode->cols; w++)
		{
			*dst++ = lump[(y >> FRACBITS) * SCREENWIDTH + (x >> FRACBITS)];
			x += dxi;
		}
		y += dyi;
	}
#endif
	NG_ClearFixOverlay();
}


static int16_t NG_TextX(int16_t x)
{
#if VIEWWINDOWWIDTH > FIX_OVERLAY_WIDTH
	const int16_t center_bias = (VIEWWINDOWWIDTH - FIX_OVERLAY_WIDTH) / 2;
	const int16_t right_bias  =  VIEWWINDOWWIDTH - FIX_OVERLAY_WIDTH;

	if (x >= VIEWWINDOWWIDTH - (FIX_OVERLAY_WIDTH / 2))
		x -= right_bias;
	else if (x >= FIX_OVERLAY_WIDTH / 2)
		x -= center_bias;
#endif
	return x;
}


static int16_t NG_TextY(int16_t y)
{
#if VIEWWINDOWHEIGHT > FIX_OVERLAY_HEIGHT
	const int16_t center_bias = (VIEWWINDOWHEIGHT - FIX_OVERLAY_HEIGHT) / 2;
	const int16_t bottom_bias =  VIEWWINDOWHEIGHT - FIX_OVERLAY_HEIGHT;

	if (y >= VIEWWINDOWHEIGHT - (FIX_OVERLAY_HEIGHT / 2))
		y -= bottom_bias;
	else if (y >= FIX_OVERLAY_HEIGHT / 2)
		y -= center_bias;
#endif
	return y;
}


static void NG_DrawFixCharacter(int16_t x, int16_t y, uint8_t color, uint8_t c)
{
	if ((uint16_t)x >= FIX_OVERLAY_WIDTH || (uint16_t)y >= FIX_OVERLAY_HEIGHT)
		return;

	*REG_VRAMMOD = 32;
	*REG_VRAMADDR = ADDR_FIXMAP + ((0 + 1) * 32) + y + 2 + ((uint16_t)x * 32);
	*REG_VRAMRW = colors[color & 0x0f] | c;
}


void V_DrawCharacter(int16_t x, int16_t y, uint8_t color, char c)
{
	NG_DrawFixCharacter(NG_TextX(x), NG_TextY(y), color, (uint8_t)c);
}


void V_DrawSTCharacter(int16_t x, int16_t y, uint8_t color, char c)
{
	V_DrawCharacter(x, y, color, c);
}


void V_DrawCharacterForeground(int16_t x, int16_t y, uint8_t color, char c)
{
	V_DrawCharacter(x, y, color, c);
}


void V_DrawString(int16_t x, int16_t y, uint8_t color, const char* s)
{
	int16_t fx = NG_TextX(x);
	int16_t fy = NG_TextY(y);

	while (*s)
	{
		NG_DrawFixCharacter(fx++, fy, color, (uint8_t)*s++);
		if (fx >= FIX_OVERLAY_WIDTH)
			break;
	}
}


void V_DrawSTString(int16_t x, int16_t y, uint8_t color, const char* s)
{
	V_DrawString(x, y, color, s);
}


void V_ClearString(int16_t y, size_t len)
{
	int16_t fy = NG_TextY(y);
	if ((uint16_t)fy >= FIX_OVERLAY_HEIGHT)
		return;

	if (len > FIX_OVERLAY_WIDTH)
		len = FIX_OVERLAY_WIDTH;

	for (size_t x = 0; x < len; x++)
		NG_DrawFixCharacter((int16_t)x, fy, 0, FIX_CLEAR_CHAR);
}


void I_InitScreenPage(void)
{
	NG_ClearFixOverlay();
}


void I_InitScreenPages(void)
{
	NG_ClearFixOverlay();
}


static void NG_DrawFixWipeMask(void)
{
	const microfb_mode_t *mode = NG_MicroFramebufferMode();

	*REG_VRAMMOD = 32;
	for (uint16_t y = 0; y < FIX_WIPE_HEIGHT; y++)
	{
		*REG_VRAMADDR = ADDR_FIXMAP + FIX_WIPE_Y_OFFSET + y;
		for (uint16_t x = 0; x < FIX_WIPE_WIDTH; x++)
		{
			const uint16_t sx = (x * mode->cols) / FIX_WIPE_WIDTH;
			const uint16_t sy = (y * mode->rows) / FIX_WIPE_HEIGHT;
			const uint8_t color = _s_screen[sy * VIEWWINDOWWIDTH + sx];
			*REG_VRAMRW = ((uint16_t)color << 8) | FIX_WIPE_CHAR;
		}
	}
}


void wipe_StartScreen(void)
{
	NG_DrawFixWipeMask();
}


static int16_t *wipe_y_lookup;


static boolean wipe_ScreenWipe(int16_t ticks)
{
	boolean done = true;

	*REG_VRAMMOD = 1;

	while (ticks--)
	{
		for (int16_t i = 0; i < FIX_WIPE_WIDTH; i++)
		{
			if (wipe_y_lookup[i] < 0)
			{
				wipe_y_lookup[i]++;
				done = false;
				continue;
			}

			// scroll down columns, which are still visible
			if (wipe_y_lookup[i] < FIX_WIPE_HEIGHT)
			{
				int16_t dy = 1;
				// At most dy shall be so that the column is shifted just invisible.
				if (wipe_y_lookup[i] + dy >= FIX_WIPE_HEIGHT)
					dy = FIX_WIPE_HEIGHT - wipe_y_lookup[i];

				int16_t s = FIX_WIPE_Y_OFFSET + FIX_WIPE_HEIGHT - 1 - dy;
				int16_t d = FIX_WIPE_Y_OFFSET + FIX_WIPE_HEIGHT - 1;
				const uint16_t column = (uint16_t)i * 32u;

				for (int16_t j = FIX_WIPE_HEIGHT - wipe_y_lookup[i] - dy; j; j--)
				{
					*REG_VRAMADDR = ADDR_FIXMAP + column + (uint16_t)s;
					uint16_t entry = *REG_VRAMRW;
					*REG_VRAMADDR = ADDR_FIXMAP + column + (uint16_t)d;
					*REG_VRAMRW = entry;
					s--;
					d--;
				}

				*REG_VRAMADDR = ADDR_FIXMAP + column + FIX_WIPE_Y_OFFSET + (uint16_t)wipe_y_lookup[i];
				for (int16_t j = 0; j < dy; j++)
					*REG_VRAMRW = FIX_CLEAR_CHAR;

				wipe_y_lookup[i] += dy;
				done = false;
			}
		}
	}

	return done;
}


static void wipe_initMelt()
{
	wipe_y_lookup[0] = -(M_Random() % 16);
	for (int16_t i = 1; i < FIX_WIPE_WIDTH; i++)
	{
		int16_t r = (M_Random() % 3) - 1;

		wipe_y_lookup[i] = wipe_y_lookup[i - 1] + r;

		if (wipe_y_lookup[i] > 0)
			wipe_y_lookup[i] = 0;
		else if (wipe_y_lookup[i] == -16)
			wipe_y_lookup[i] = -15;
	}
}


void D_Wipe(void)
{
	wipe_y_lookup = Z_TryMallocStatic(FIX_WIPE_WIDTH * sizeof(int16_t));
	if (!wipe_y_lookup)
		return;

	I_FinishUpdate();

	wipe_initMelt();

	boolean done;
	int32_t wipestart = I_GetTime() - 1;

	do
	{
		int32_t nowtime;
		int16_t tics;
		do
		{
			nowtime = I_GetTime();
			tics = nowtime - wipestart;
		} while (!tics);

		wipestart = nowtime;
		done = wipe_ScreenWipe(tics);

		M_Drawer();                   // menu is drawn even on top of wipes
		NG_WaitVBlankStart();
		NG_UploadFixOverlay();

	} while (!done);

	NG_ClearFixOverlay();
	Z_Free(wipe_y_lookup);
}
