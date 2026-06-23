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
 *      The Doom renderer writes an 80x56 8-bit logical framebuffer.  Each
 *      logical pixel is displayed by a 4x4 hardware-sprite microcell:
 *
 *          160 sprite strips, 80 active per scanline = 320x224 visible pixels
 *
 *      The full-color path uses one shrunk 16x16 C-ROM tile per logical
 *      pixel.  The hardware shrinker reduces each tile to a 4x4 cell.
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


extern const int16_t CENTERY;

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

#define MICROFB_PHYSICAL_CELL_W 4u
#define MICROFB_PHYSICAL_CELL_H 4u
#define MICROFB_COLUMN_CHUNKS 2u
#define MICROFB_FRAMEBUFFER_SETS 2u
#define MICROFB_SPRITE_BASE 1u
#define MICROFB_SPRITE_SHRINK_4PX_CELL 0x033fu
#define MICROFB_X_WORD(x) (((uint16_t)(x)) << 7)
#define MICROFB_PALETTE_ATTR(pal) ((uint16_t)(pal) << 8)

#define FIX_OVERLAY_WIDTH 38
#define FIX_OVERLAY_HEIGHT 28
#define FIX_CLEAR_CHAR ' '

#if (VIEWWINDOWHEIGHT % MICROFB_COLUMN_CHUNKS) != 0
#error Neo Geo sprite microframebuffer requires VIEWWINDOWHEIGHT divisible by MICROFB_COLUMN_CHUNKS
#endif

#if VIEWWINDOWWIDTH > 96
#error Neo Geo sprite microframebuffer cannot exceed 96 logical columns because of the per-scanline sprite limit
#endif

#define MICROFB_CHUNK_CELLS (VIEWWINDOWHEIGHT / MICROFB_COLUMN_CHUNKS)
#define MICROFB_CHUNK_PIXELS (MICROFB_CHUNK_CELLS * MICROFB_PHYSICAL_CELL_H)

#if (MICROFB_CHUNK_PIXELS % 16) != 0
#error Neo Geo sprite microframebuffer chunk height must be a whole number of display tiles
#endif

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


static uint8_t _s_screen[VIEWWINDOWWIDTH * VIEWWINDOWHEIGHT];
static uint16_t _s_color_to_tile[256];
static uint16_t _s_color_to_attr[256];
static uint8_t _s_visible_sprite_set;

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

		_s_color_to_tile[color] = MICROFB_TILE_BASE + slot;
		_s_color_to_attr[color] = MICROFB_PALETTE_ATTR(MICROFB_SPRITE_PALETTE_BASE + pal);
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
	for (uint16_t y = 0; y < FIX_OVERLAY_HEIGHT; y++)
	{
		*REG_VRAMADDR = ADDR_FIXMAP + ((0 + 1) * 32) + y + 2;
		for (uint16_t x = 0; x < FIX_OVERLAY_WIDTH; x++)
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
	*REG_VRAMMOD = 0x200;
	for (uint16_t chunk = 0; chunk < MICROFB_COLUMN_CHUNKS; chunk++)
	{
		const uint16_t y = chunk * MICROFB_CHUNK_PIXELS;
		const uint16_t height_word = visible ? NG_SpriteYWord(y, MICROFB_CHUNK_CELLS) : 0u;

		for (uint16_t x = 0; x < VIEWWINDOWWIDTH; x++)
		{
			const uint16_t sprite = NG_MicroSpriteIndex(set, chunk, x);
			*REG_VRAMADDR = ADDR_SCB2 + sprite;
			*REG_VRAMRW = MICROFB_SPRITE_SHRINK_4PX_CELL;
			*REG_VRAMRW = height_word;
			*REG_VRAMRW = MICROFB_X_WORD(x * MICROFB_PHYSICAL_CELL_W);
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
	*REG_VRAMMOD = 1;
	for (uint16_t chunk = 0; chunk < MICROFB_COLUMN_CHUNKS; chunk++)
	{
		const uint16_t row_base = chunk * MICROFB_CHUNK_CELLS;

		for (uint16_t x = 0; x < VIEWWINDOWWIDTH; x++)
		{
			const uint16_t sprite = NG_MicroSpriteIndex(set, chunk, x);
			const uint8_t *src = &_s_screen[row_base * VIEWWINDOWWIDTH + x];

			*REG_VRAMADDR = ADDR_SCB1 + (sprite * 64u);
			for (uint16_t row = 0; row < MICROFB_CHUNK_CELLS; row++)
			{
				const uint8_t color = src[row * VIEWWINDOWWIDTH];
				*REG_VRAMRW = _s_color_to_tile[color];
				*REG_VRAMRW = _s_color_to_attr[color];
			}
		}
	}
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
}


#define COLEXTRABITS (8 - 1)
#define COLBITS (8 + 1)


void R_DrawColumnSprite(const draw_column_vars_t *dcvars)
{
	int16_t count = (dcvars->yh - dcvars->yl) + 1;

	if (count <= 0)
		return;

	const uint8_t *source = dcvars->source;
	const uint8_t *colormap = dcvars->colormap;
	uint8_t *dest = &_s_screen[dcvars->yl * VIEWWINDOWWIDTH + dcvars->x];

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

	uint8_t *dest = &_s_screen[dcvars->yl * VIEWWINDOWWIDTH + dcvars->x];
	while (count--)
	{
		*dest = color;
		dest += VIEWWINDOWWIDTH;
	}
}


void R_DrawFuzzColumn(const draw_column_vars_t *dcvars)
{
	/* Cheap spectre fallback: draw the column black instead of attempting the
	 * original neighboring-pixel fuzz on the scaled sprite framebuffer.
	 */
	R_DrawColumnFlat(0, dcvars);
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
#if defined SHOW_PALETTE
	int i = 0;
	for (int y = 0; y < 16; y++)
		for (int x = 0; x < 16; x++)
			_s_screen[y * VIEWWINDOWWIDTH + x] = i++;
#else
	const uint8_t *lump = W_GetLumpByNum(num);

	static const fixed_t DXI = ((fixed_t)SCREENWIDTH << FRACBITS) / VIEWWINDOWWIDTH;
	static const fixed_t DYI = ((fixed_t)SCREENHEIGHT << FRACBITS) / VIEWWINDOWHEIGHT;

	uint8_t *dst = &_s_screen[0];

	fixed_t y = 0;
	for (int h = 0; h < VIEWWINDOWHEIGHT; h++)
	{
		fixed_t x = 0;
		for (int w = 0; w < VIEWWINDOWWIDTH; w++)
		{
			*dst++ = lump[(y >> FRACBITS) * SCREENWIDTH + (x >> FRACBITS)];
			x += DXI;
		}
		y += DYI;
	}
#endif
	NG_ClearFixOverlay();
}


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


void wipe_StartScreen(void)
{
	I_InitScreenPages();
}


void D_Wipe(void)
{
	// TODO
}
