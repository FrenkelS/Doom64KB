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
 *      Neo Geo video code
 *
 *-----------------------------------------------------------------------------*/

#include <stdint.h>
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


//#define DITHER_CHARACTER 0xb1
//#define DITHER_CHARACTER 0xdb
#define DITHER_CHARACTER 0x00


extern const int16_t CENTERY;

static uint16_t _s_screen[VIEWWINDOWWIDTH * VIEWWINDOWHEIGHT];


static int16_t palettelumpnum;


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
	memcpy((uint8_t*)&MMAP_PALBANK1[0], &palette_lump[256 * pal], 256 * 2);
}


void I_InitGraphicsHardwareSpecificCode(void)
{
	*REG_VRAMMOD = 32;

	I_ReloadPalette();
	I_UploadNewPalette(0);

	uint16_t *dst = &_s_screen[0];
	for (int i = 0; i < VIEWWINDOWWIDTH * VIEWWINDOWHEIGHT; i++)
		*dst++ = 0x0000 | DITHER_CHARACTER;
}


void I_ShutdownGraphics(void)
{
	// Do nothing
}


static int8_t newpal;


void I_SetPalette(int8_t p)
{
	newpal = p;
}


void V_SetSTPalette(void)
{
	// Do nothing
}


#define NO_PALETTE_CHANGE 100


void I_FinishUpdate(void)
{
	if (newpal != NO_PALETTE_CHANGE)
	{
		I_UploadNewPalette(newpal);
		newpal = NO_PALETTE_CHANGE;
	}

	uint16_t *src = &_s_screen[0];
	for (int y = 0; y < VIEWWINDOWHEIGHT; y++)
	{
		*REG_VRAMADDR = ADDR_FIXMAP + ((0 + 1) * 32) + y + 2;
		for (int x = 0; x < VIEWWINDOWWIDTH; x++)
		{
			*REG_VRAMRW = *src++;
		}
	}
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

	uint8_t *dest = (uint8_t*)&_s_screen[dcvars->yl * VIEWWINDOWWIDTH + dcvars->x];

	const uint16_t fracstep = dcvars->fracstep;
	uint16_t frac = (dcvars->texturemid >> COLEXTRABITS) + (dcvars->yl - CENTERY) * fracstep;

	const uint8_t colbits = COLBITS;
	switch (count)
	{
		case 28: *dest = colormap[source[frac >> colbits]]; dest += VIEWWINDOWWIDTH * 2; frac += fracstep;
		case 27: *dest = colormap[source[frac >> colbits]]; dest += VIEWWINDOWWIDTH * 2; frac += fracstep;
		case 26: *dest = colormap[source[frac >> colbits]]; dest += VIEWWINDOWWIDTH * 2; frac += fracstep;
		case 25: *dest = colormap[source[frac >> colbits]]; dest += VIEWWINDOWWIDTH * 2; frac += fracstep;
		case 24: *dest = colormap[source[frac >> colbits]]; dest += VIEWWINDOWWIDTH * 2; frac += fracstep;
		case 23: *dest = colormap[source[frac >> colbits]]; dest += VIEWWINDOWWIDTH * 2; frac += fracstep;
		case 22: *dest = colormap[source[frac >> colbits]]; dest += VIEWWINDOWWIDTH * 2; frac += fracstep;
		case 21: *dest = colormap[source[frac >> colbits]]; dest += VIEWWINDOWWIDTH * 2; frac += fracstep;
		case 20: *dest = colormap[source[frac >> colbits]]; dest += VIEWWINDOWWIDTH * 2; frac += fracstep;
		case 19: *dest = colormap[source[frac >> colbits]]; dest += VIEWWINDOWWIDTH * 2; frac += fracstep;
		case 18: *dest = colormap[source[frac >> colbits]]; dest += VIEWWINDOWWIDTH * 2; frac += fracstep;
		case 17: *dest = colormap[source[frac >> colbits]]; dest += VIEWWINDOWWIDTH * 2; frac += fracstep;
		case 16: *dest = colormap[source[frac >> colbits]]; dest += VIEWWINDOWWIDTH * 2; frac += fracstep;
		case 15: *dest = colormap[source[frac >> colbits]]; dest += VIEWWINDOWWIDTH * 2; frac += fracstep;
		case 14: *dest = colormap[source[frac >> colbits]]; dest += VIEWWINDOWWIDTH * 2; frac += fracstep;
		case 13: *dest = colormap[source[frac >> colbits]]; dest += VIEWWINDOWWIDTH * 2; frac += fracstep;
		case 12: *dest = colormap[source[frac >> colbits]]; dest += VIEWWINDOWWIDTH * 2; frac += fracstep;
		case 11: *dest = colormap[source[frac >> colbits]]; dest += VIEWWINDOWWIDTH * 2; frac += fracstep;
		case 10: *dest = colormap[source[frac >> colbits]]; dest += VIEWWINDOWWIDTH * 2; frac += fracstep;
		case  9: *dest = colormap[source[frac >> colbits]]; dest += VIEWWINDOWWIDTH * 2; frac += fracstep;
		case  8: *dest = colormap[source[frac >> colbits]]; dest += VIEWWINDOWWIDTH * 2; frac += fracstep;
		case  7: *dest = colormap[source[frac >> colbits]]; dest += VIEWWINDOWWIDTH * 2; frac += fracstep;
		case  6: *dest = colormap[source[frac >> colbits]]; dest += VIEWWINDOWWIDTH * 2; frac += fracstep;
		case  5: *dest = colormap[source[frac >> colbits]]; dest += VIEWWINDOWWIDTH * 2; frac += fracstep;
		case  4: *dest = colormap[source[frac >> colbits]]; dest += VIEWWINDOWWIDTH * 2; frac += fracstep;
		case  3: *dest = colormap[source[frac >> colbits]]; dest += VIEWWINDOWWIDTH * 2; frac += fracstep;
		case  2: *dest = colormap[source[frac >> colbits]]; dest += VIEWWINDOWWIDTH * 2; frac += fracstep;
		case  1: *dest = colormap[source[frac >> colbits]];
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

	uint8_t *dest = (uint8_t*)&_s_screen[dcvars->yl * VIEWWINDOWWIDTH + dcvars->x];

	switch (count)
	{
		case 28: dest[VIEWWINDOWWIDTH * 2 * 27] = color;
		case 27: dest[VIEWWINDOWWIDTH * 2 * 26] = color;
		case 26: dest[VIEWWINDOWWIDTH * 2 * 25] = color;
		case 25: dest[VIEWWINDOWWIDTH * 2 * 24] = color;
		case 24: dest[VIEWWINDOWWIDTH * 2 * 23] = color;
		case 23: dest[VIEWWINDOWWIDTH * 2 * 22] = color;
		case 22: dest[VIEWWINDOWWIDTH * 2 * 21] = color;
		case 21: dest[VIEWWINDOWWIDTH * 2 * 20] = color;
		case 20: dest[VIEWWINDOWWIDTH * 2 * 19] = color;
		case 19: dest[VIEWWINDOWWIDTH * 2 * 18] = color;
		case 18: dest[VIEWWINDOWWIDTH * 2 * 17] = color;
		case 17: dest[VIEWWINDOWWIDTH * 2 * 16] = color;
		case 16: dest[VIEWWINDOWWIDTH * 2 * 15] = color;
		case 15: dest[VIEWWINDOWWIDTH * 2 * 14] = color;
		case 14: dest[VIEWWINDOWWIDTH * 2 * 13] = color;
		case 13: dest[VIEWWINDOWWIDTH * 2 * 12] = color;
		case 12: dest[VIEWWINDOWWIDTH * 2 * 11] = color;
		case 11: dest[VIEWWINDOWWIDTH * 2 * 10] = color;
		case 10: dest[VIEWWINDOWWIDTH * 2 *  9] = color;
		case  9: dest[VIEWWINDOWWIDTH * 2 *  8] = color;
		case  8: dest[VIEWWINDOWWIDTH * 2 *  7] = color;
		case  7: dest[VIEWWINDOWWIDTH * 2 *  6] = color;
		case  6: dest[VIEWWINDOWWIDTH * 2 *  5] = color;
		case  5: dest[VIEWWINDOWWIDTH * 2 *  4] = color;
		case  4: dest[VIEWWINDOWWIDTH * 2 *  3] = color;
		case  3: dest[VIEWWINDOWWIDTH * 2 *  2] = color;
		case  2: dest[VIEWWINDOWWIDTH * 2 *  1] = color;
		case  1: dest[VIEWWINDOWWIDTH * 2 *  0] = color;
	}
}


void R_DrawFuzzColumn(const draw_column_vars_t *dcvars)
{
	// TODO
}


void V_ClearViewWindow(void)
{
	// TODO
}


void V_InitDrawLine(void)
{
	// Do nothing
}


void V_ShutdownDrawLine(void)
{
	// Do nothing
}


void V_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color)
{
	// TODO
}


void V_DrawBackground(int16_t backgroundnum)
{
	// TODO
}


void V_DrawRawFullScreen(int16_t num)
{
#if defined SHOW_PALETTE
	int i = 0;
	for (int y = 0; y < 16; y++)
		for (int x = 0; x < 16; x++)
			_s_screen[y * VIEWWINDOWWIDTH + x] = ((i++) << 8) | DITHER_CHARACTER;
#else
	const uint8_t *lump = W_GetLumpByNum(num);

	static const fixed_t DXI = ((fixed_t)SCREENWIDTH << FRACBITS) / VIEWWINDOWWIDTH;
	static const fixed_t DYI = ((fixed_t)SCREENHEIGHT << FRACBITS) / VIEWWINDOWHEIGHT;

	uint16_t *dst = &_s_screen[0];

	fixed_t y = 0;
	for (int h = 0; h < VIEWWINDOWHEIGHT; h++)
	{
		fixed_t x = 0;
		for (int w = 0; w < VIEWWINDOWWIDTH; w++)
		{
			*dst++ = (lump[(y >> FRACBITS) * SCREENWIDTH + (x >> FRACBITS)] << 8) | DITHER_CHARACTER;
			x += DXI;
		}
		y += DYI;
	}
#endif
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

void V_DrawCharacter(int16_t x, int16_t y, uint8_t color, char c)
{
	_s_screen[y * VIEWWINDOWWIDTH + x] = colors[color] | c;
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
	uint16_t *dst = &_s_screen[y * VIEWWINDOWWIDTH + x];
	while (*s)
		*dst++ = colors[color] | (*s++);
}


void V_DrawSTString(int16_t x, int16_t y, uint8_t color, const char* s)
{
	V_DrawString(x, y, color, s);
}


void V_ClearString(int16_t y, size_t len)
{
	uint8_t *dst = (uint8_t*)&_s_screen[y * VIEWWINDOWWIDTH];
	for (int x = 0; x < len; x++)
	{
		dst++;
		*dst++ = DITHER_CHARACTER;
	}
}


void I_InitScreenPage(void)
{
	uint8_t *dst = (uint8_t*)&_s_screen[1 * VIEWWINDOWWIDTH + 0];
	// Skip the first row and the last 5 rows
	for (int i = 0; i < VIEWWINDOWWIDTH * (VIEWWINDOWHEIGHT - 1 - 5); i++)
	{
		dst++;
		*dst++ = DITHER_CHARACTER;
	}
}


void I_InitScreenPages(void)
{
	uint8_t *dst = (uint8_t*)&_s_screen[0];
	for (int i = 0; i < VIEWWINDOWWIDTH * VIEWWINDOWHEIGHT; i++)
	{
		dst++;
		*dst++ = DITHER_CHARACTER;
	}
}


void wipe_StartScreen(void)
{
	I_InitScreenPages();
}


void D_Wipe(void)
{
	// TODO
}
