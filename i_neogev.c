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
#define DITHER_CHARACTER 0xdb


extern const int16_t CENTERY;

//static uint16_t _s_screen[VIEWWINDOWWIDTH * VIEWWINDOWHEIGHT];


void I_ReloadPalette(void)
{
}


void I_InitGraphicsHardwareSpecificCode(void)
{
	*REG_VRAMMOD = 1;
}


void I_ShutdownGraphics(void)
{
}


void I_SetPalette(int8_t p)
{
}


void V_SetSTPalette(void)
{
}


void I_FinishUpdate(void)
{
	// Do nothing
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

	*REG_VRAMADDR = ADDR_FIXMAP + ((dcvars->x + 1) * 32) + dcvars->yl + 2;

	const uint16_t fracstep = dcvars->fracstep;
	uint16_t frac = (dcvars->texturemid >> COLEXTRABITS) + (dcvars->yl - CENTERY) * fracstep;

	switch (count)
	{
		case 28: *REG_VRAMRW = (colormap[source[frac >> COLBITS]] << 12) | DITHER_CHARACTER; frac += fracstep;
		case 27: *REG_VRAMRW = (colormap[source[frac >> COLBITS]] << 12) | DITHER_CHARACTER; frac += fracstep;
		case 26: *REG_VRAMRW = (colormap[source[frac >> COLBITS]] << 12) | DITHER_CHARACTER; frac += fracstep;
		case 25: *REG_VRAMRW = (colormap[source[frac >> COLBITS]] << 12) | DITHER_CHARACTER; frac += fracstep;
		case 24: *REG_VRAMRW = (colormap[source[frac >> COLBITS]] << 12) | DITHER_CHARACTER; frac += fracstep;
		case 23: *REG_VRAMRW = (colormap[source[frac >> COLBITS]] << 12) | DITHER_CHARACTER; frac += fracstep;
		case 22: *REG_VRAMRW = (colormap[source[frac >> COLBITS]] << 12) | DITHER_CHARACTER; frac += fracstep;
		case 21: *REG_VRAMRW = (colormap[source[frac >> COLBITS]] << 12) | DITHER_CHARACTER; frac += fracstep;
		case 20: *REG_VRAMRW = (colormap[source[frac >> COLBITS]] << 12) | DITHER_CHARACTER; frac += fracstep;
		case 19: *REG_VRAMRW = (colormap[source[frac >> COLBITS]] << 12) | DITHER_CHARACTER; frac += fracstep;
		case 18: *REG_VRAMRW = (colormap[source[frac >> COLBITS]] << 12) | DITHER_CHARACTER; frac += fracstep;
		case 17: *REG_VRAMRW = (colormap[source[frac >> COLBITS]] << 12) | DITHER_CHARACTER; frac += fracstep;
		case 16: *REG_VRAMRW = (colormap[source[frac >> COLBITS]] << 12) | DITHER_CHARACTER; frac += fracstep;
		case 15: *REG_VRAMRW = (colormap[source[frac >> COLBITS]] << 12) | DITHER_CHARACTER; frac += fracstep;
		case 14: *REG_VRAMRW = (colormap[source[frac >> COLBITS]] << 12) | DITHER_CHARACTER; frac += fracstep;
		case 13: *REG_VRAMRW = (colormap[source[frac >> COLBITS]] << 12) | DITHER_CHARACTER; frac += fracstep;
		case 12: *REG_VRAMRW = (colormap[source[frac >> COLBITS]] << 12) | DITHER_CHARACTER; frac += fracstep;
		case 11: *REG_VRAMRW = (colormap[source[frac >> COLBITS]] << 12) | DITHER_CHARACTER; frac += fracstep;
		case 10: *REG_VRAMRW = (colormap[source[frac >> COLBITS]] << 12) | DITHER_CHARACTER; frac += fracstep;
		case  9: *REG_VRAMRW = (colormap[source[frac >> COLBITS]] << 12) | DITHER_CHARACTER; frac += fracstep;
		case  8: *REG_VRAMRW = (colormap[source[frac >> COLBITS]] << 12) | DITHER_CHARACTER; frac += fracstep;
		case  7: *REG_VRAMRW = (colormap[source[frac >> COLBITS]] << 12) | DITHER_CHARACTER; frac += fracstep;
		case  6: *REG_VRAMRW = (colormap[source[frac >> COLBITS]] << 12) | DITHER_CHARACTER; frac += fracstep;
		case  5: *REG_VRAMRW = (colormap[source[frac >> COLBITS]] << 12) | DITHER_CHARACTER; frac += fracstep;
		case  4: *REG_VRAMRW = (colormap[source[frac >> COLBITS]] << 12) | DITHER_CHARACTER; frac += fracstep;
		case  3: *REG_VRAMRW = (colormap[source[frac >> COLBITS]] << 12) | DITHER_CHARACTER; frac += fracstep;
		case  2: *REG_VRAMRW = (colormap[source[frac >> COLBITS]] << 12) | DITHER_CHARACTER; frac += fracstep;
		case  1: *REG_VRAMRW = (colormap[source[frac >> COLBITS]] << 12) | DITHER_CHARACTER;
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

	*REG_VRAMADDR = ADDR_FIXMAP + ((dcvars->x + 1) * 32) + dcvars->yl + 2;

	uint16_t c = (color << 12) | DITHER_CHARACTER;

	switch (count)
	{
		case 28: *REG_VRAMRW = c;
		case 27: *REG_VRAMRW = c;
		case 26: *REG_VRAMRW = c;
		case 25: *REG_VRAMRW = c;
		case 24: *REG_VRAMRW = c;
		case 23: *REG_VRAMRW = c;
		case 22: *REG_VRAMRW = c;
		case 21: *REG_VRAMRW = c;
		case 20: *REG_VRAMRW = c;
		case 19: *REG_VRAMRW = c;
		case 18: *REG_VRAMRW = c;
		case 17: *REG_VRAMRW = c;
		case 16: *REG_VRAMRW = c;
		case 15: *REG_VRAMRW = c;
		case 14: *REG_VRAMRW = c;
		case 13: *REG_VRAMRW = c;
		case 12: *REG_VRAMRW = c;
		case 11: *REG_VRAMRW = c;
		case 10: *REG_VRAMRW = c;
		case  9: *REG_VRAMRW = c;
		case  8: *REG_VRAMRW = c;
		case  7: *REG_VRAMRW = c;
		case  6: *REG_VRAMRW = c;
		case  5: *REG_VRAMRW = c;
		case  4: *REG_VRAMRW = c;
		case  3: *REG_VRAMRW = c;
		case  2: *REG_VRAMRW = c;
		case  1: *REG_VRAMRW = c;
	}
}


void R_DrawFuzzColumn(const draw_column_vars_t *dcvars)
{
}


void V_ClearViewWindow(void)
{
}


void V_InitDrawLine(void)
{
}


void V_ShutdownDrawLine(void)
{
}


void V_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color)
{
}


void V_DrawBackground(int16_t backgroundnum)
{
}


void V_DrawRawFullScreen(int16_t num)
{
	const uint8_t *lump = W_GetLumpByNum(num);

	static const int16_t DXI = SCREENWIDTH / VIEWWINDOWWIDTH;
	static const fixed_t DYI = ((fixed_t)SCREENHEIGHT << FRACBITS) / VIEWWINDOWHEIGHT;

	int x = 0;
	for (int w = 0; w < VIEWWINDOWWIDTH; w++)
	{
		fixed_t y = 0;
		*REG_VRAMADDR = ADDR_FIXMAP + ((w + 1) * 32) + 0 + 2;
		for (int h = 0; h < VIEWWINDOWHEIGHT; h++)
		{
			*REG_VRAMRW = (lump[(y >> FRACBITS) * SCREENWIDTH + x] << 12) | DITHER_CHARACTER;
			y += DYI;
		}
		x += DXI;
	}
}


void V_DrawCharacter(int16_t x, int16_t y, uint8_t color, char c)
{
	*REG_VRAMADDR = ADDR_FIXMAP + ((x + 1) * 32) + y + 2;
	*REG_VRAMRW = (color << 12) | c;
}


void V_DrawSTCharacter(int16_t x, int16_t y, uint8_t color, char c)
{
	V_DrawCharacter(x, y, color, c);
}


void V_DrawCharacterForeground(int16_t x, int16_t y, uint8_t color, char c)
{
	I_Error("Implement me: V_DrawCharacterForeground");
}


void V_DrawString(int16_t x, int16_t y, uint8_t color, const char* s)
{
	*REG_VRAMADDR = ADDR_FIXMAP + ((x + 1) * 32) + y + 2;
	*REG_VRAMMOD = 32;
	while (*s)
		*REG_VRAMRW = (color << 12) | (*s++);

	*REG_VRAMMOD = 1;
}


void V_DrawSTString(int16_t x, int16_t y, uint8_t color, const char* s)
{
	V_DrawString(x, y, color, s);
}


void V_ClearString(int16_t y, size_t len)
{
	UNUSED(y);
	UNUSED(len);
}


void I_InitScreenPage(void)
{
}


void I_InitScreenPages(void)
{
}


void wipe_StartScreen(void)
{
}


void D_Wipe(void)
{
}
