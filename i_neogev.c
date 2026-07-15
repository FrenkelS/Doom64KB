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
 *      Neo Geo video code 38x28
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
	const uint16_t *palette = &palette_lump[256 * pal];
	MMAP_PALBANK1[0xfff] = *palette++;
	for (int i = 1; i < 256; i++)
		MMAP_PALBANK1[i] = *palette++;
}


void I_InitGraphicsHardwareSpecificCode(void)
{
	*REG_VRAMMOD = 32;
	MMAP_PALBANK1[0] = 0x8000;

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


static const int LUTY[VIEWWINDOWHEIGHT] = {
	 0 * VIEWWINDOWWIDTH,  1 * VIEWWINDOWWIDTH,  2 * VIEWWINDOWWIDTH,  3 * VIEWWINDOWWIDTH,
	 4 * VIEWWINDOWWIDTH,  5 * VIEWWINDOWWIDTH,  6 * VIEWWINDOWWIDTH,  7 * VIEWWINDOWWIDTH,
	 8 * VIEWWINDOWWIDTH,  9 * VIEWWINDOWWIDTH, 10 * VIEWWINDOWWIDTH, 11 * VIEWWINDOWWIDTH,
	12 * VIEWWINDOWWIDTH, 13 * VIEWWINDOWWIDTH, 14 * VIEWWINDOWWIDTH, 15 * VIEWWINDOWWIDTH,
	16 * VIEWWINDOWWIDTH, 17 * VIEWWINDOWWIDTH, 18 * VIEWWINDOWWIDTH, 19 * VIEWWINDOWWIDTH,
	20 * VIEWWINDOWWIDTH, 21 * VIEWWINDOWWIDTH, 22 * VIEWWINDOWWIDTH, 23 * VIEWWINDOWWIDTH,
	24 * VIEWWINDOWWIDTH, 25 * VIEWWINDOWWIDTH, 26 * VIEWWINDOWWIDTH, 27 * VIEWWINDOWWIDTH
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

	uint8_t *dest = (uint8_t*)&_s_screen[LUTY[dcvars->yl] + dcvars->x];

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


#define FUZZCOLOR1 34
#define FUZZCOLOR2 148
#define FUZZCOLOR3 212
#define FUZZCOLOR4 246
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
		dest += VIEWWINDOWWIDTH * 2;

		fuzzpos++;
		if (fuzzpos >= FUZZTABLE)
			fuzzpos = 0;

	} while (--count);
}


void V_ClearViewWindow(void)
{
	memset(_s_screen, 0, VIEWWINDOWWIDTH * VIEWWINDOWHEIGHT * sizeof(uint16_t));
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
	int16_t dx = abs(x1 - x0);
	int16_t sx = x0 < x1 ? 1 : -1;

	int16_t dy = -abs(y1 - y0);
	int16_t sy = y0 < y1 ? 1 : -1;

	int16_t err = dx + dy;

	while (true)
	{
		uint8_t *dest = (uint8_t*)&_s_screen[y0 * VIEWWINDOWWIDTH + x0];
		*dest = color;

		if (x0 == x1 && y0 == y1)
			break;

		int16_t e2 = 2 * err;

		if (e2 >= dy)
		{
			err += dy;
			x0  += sx;
		}

		if (e2 <= dx)
		{
			err += dx;
			y0  += sy;
		}
	}
}


void V_DrawBackground(int16_t backgroundnum)
{
	const uint16_t *lump = W_GetLumpByNum(backgroundnum);

	for (int16_t y = 0; y < VIEWWINDOWHEIGHT; y++)
	{
		uint16_t *dst = &_s_screen[y * VIEWWINDOWWIDTH + 0];
		for (int16_t x = 0; x < VIEWWINDOWWIDTH; x++)
		{
			*dst++ = lump[(y & 7) * 8 + (x & 7)];
		}
	}
}


void V_DrawRawFullScreen(int16_t num)
{
#if defined SHOW_PALETTE
	int i = 0;
	for (int y = 0; y < 16; y++)
		for (int x = 0; x < 16; x++)
			_s_screen[y * VIEWWINDOWWIDTH + x] = ((i++) << 8) | DITHER_CHARACTER;
#else
	W_ReadLumpByNum(num, &_s_screen[0]);
#endif
}


void V_DrawCharacter(int16_t x, int16_t y, uint16_t color, char c)
{
	_s_screen[y * VIEWWINDOWWIDTH + x] = color | c;
}


void V_DrawSTCharacter(int16_t x, int16_t y, uint16_t color, char c)
{
	V_DrawCharacter(x, y, color, c);
}


void V_DrawCharacterForeground(int16_t x, int16_t y, uint16_t color, char c)
{
	V_DrawCharacter(x, y, color, c);
}


void V_DrawString(int16_t x, int16_t y, uint16_t color, const char* s)
{
	uint16_t *dst = &_s_screen[y * VIEWWINDOWWIDTH + x];
	while (*s)
		*dst++ = color | (*s++);
}


void V_DrawSTString(int16_t x, int16_t y, uint16_t color, const char* s)
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


static int16_t *wipe_y_lookup;


static boolean wipe_ScreenWipe(int16_t ticks)
{
	boolean done = true;

	*REG_VRAMMOD = 1;

	while (ticks--)
	{
		for (int16_t i = 0; i < VIEWWINDOWWIDTH; i++)
		{
			if (wipe_y_lookup[i] < 0)
			{
				wipe_y_lookup[i]++;
				done = false;
				continue;
			}

			// scroll down columns, which are still visible
			if (wipe_y_lookup[i] < VIEWWINDOWHEIGHT)
			{
				int16_t dy = 1;
				// At most dy shall be so that the column is shifted by VIEWWINDOWHEIGHT (i.e. just invisible)
				if (wipe_y_lookup[i] + dy >= VIEWWINDOWHEIGHT)
					dy = VIEWWINDOWHEIGHT - wipe_y_lookup[i];

				int16_t s = ((i + 1) * 32) + (VIEWWINDOWHEIGHT - 1 - dy) + 2;
				int16_t d = ((i + 1) * 32) + (VIEWWINDOWHEIGHT - 1)      + 2;

				// scroll down the column. Of course we need to copy from the bottom... up to
				// VIEWWINDOWHEIGHT - yLookup - dy

				for (int16_t j = VIEWWINDOWHEIGHT - wipe_y_lookup[i] - dy; j; j--)
				{
					*REG_VRAMADDR = ADDR_FIXMAP + s;
					uint16_t entry = *REG_VRAMRW;
					*REG_VRAMADDR = ADDR_FIXMAP + d;
					*REG_VRAMRW = entry;
					s--;
					d--;
				}

				// copy new screen. We need to copy only between y_lookup and + dy y_lookup
				uint16_t *sptr = &_s_screen[wipe_y_lookup[i] * VIEWWINDOWWIDTH + i];
				*REG_VRAMADDR = ADDR_FIXMAP + ((i + 1) * 32) + wipe_y_lookup[i] + 2;

				for (int16_t j = 0 ; j < dy; j++)
				{
					*REG_VRAMRW = *sptr;
					sptr += VIEWWINDOWWIDTH;
				}

				wipe_y_lookup[i] += dy;
				done = false;
			}
		}
	}

	*REG_VRAMMOD = 32;

	return done;
}


static void wipe_initMelt()
{
	wipe_y_lookup[0] = -(M_Random() % 16);
	for (int16_t i = 1; i < VIEWWINDOWWIDTH; i++)
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
	wipe_y_lookup = Z_TryMallocStatic(VIEWWINDOWWIDTH * sizeof(int16_t));
	if (!wipe_y_lookup)
		return;

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

	} while (!done);

	Z_Free(wipe_y_lookup);
}
