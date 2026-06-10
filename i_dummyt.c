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
 *      Dummy text mode code
 *
 *-----------------------------------------------------------------------------*/

#include <stdint.h>

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


void I_ReloadPalette(void)
{
}


void I_InitGraphicsHardwareSpecificCode(void)
{
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
	I_Error("Implement me: I_FinishUpdate: page flip or blit buffer");
}


void R_DrawColumnSprite(const draw_column_vars_t *dcvars)
{
	I_Error("Implement me: R_DrawColumnSprite");
}


void R_DrawColumnWall(const draw_column_vars_t *dcvars)
{
	I_Error("Implement me: R_DrawColumnWall");
}


void R_DrawColumnFlat(uint8_t color, const draw_column_vars_t *dcvars)
{
	I_Error("Implement me: R_DrawColumnFlat");
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
}


void V_DrawCharacter(int16_t x, int16_t y, uint8_t color, char c)
{
}


void V_DrawSTCharacter(int16_t x, int16_t y, uint8_t color, char c)
{
}


void V_DrawCharacterForeground(int16_t x, int16_t y, uint8_t color, char c)
{
}


void V_DrawString(int16_t x, int16_t y, uint8_t color, const char* s)
{
}


void V_DrawSTString(int16_t x, int16_t y, uint8_t color, const char* s)
{
}


void V_ClearString(int16_t y, size_t len)
{
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
