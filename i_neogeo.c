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
 *      Neo Geo implementation of i_system.h
 *
 *-----------------------------------------------------------------------------*/

#include <stdarg.h>
#include <ngdevkit/bios-ram.h>
#include <ngdevkit/registers.h>

#include "doomdef.h"
#include "doomtype.h"
#include "compiler.h"
#include "d_main.h"
#include "i_system.h"

#include "globdata.h"


//#define TIMEDEMO


void I_InitGraphicsHardwareSpecificCode(void);
void I_ShutdownGraphics(void);


static boolean isGraphicsModeSet = false;


//**************************************************************************************
//
// Screen code
//

void I_InitGraphics(void)
{
	I_InitGraphicsHardwareSpecificCode();
	isGraphicsModeSet = true;
}


//**************************************************************************************
//
// Keyboard code
//

static boolean isKeyboardIsrSet = false;


void I_InitKeyboard(void)
{
	isKeyboardIsrSet = true;
}


static void I_PostEvent(boolean keyup, int16_t data1)
{
	event_t ev;
	ev.type  = keyup ? ev_keyup : ev_keydown;
	ev.data1 = data1;
	D_PostEvent(&ev);
}


#define KB_MATRIX_SIZE 2

void I_StartTic(void)
{
	static uint8_t kb_matrix[KB_MATRIX_SIZE * 2];
	static uint8_t *kb_matrix_cur = &kb_matrix[0];
	static uint8_t *kb_matrix_prv = &kb_matrix[KB_MATRIX_SIZE];

	uint8_t *tmp = kb_matrix_cur;
	kb_matrix_cur = kb_matrix_prv;
	kb_matrix_prv = tmp;

	kb_matrix_cur[0] = *REG_P1CNT;
	kb_matrix_cur[1] = *REG_STATUS_B;

	uint8_t diff;
	diff = kb_matrix_prv[0] ^ kb_matrix_cur[0];
	if (diff & CNT_UP)    I_PostEvent(kb_matrix_cur[0] & CNT_UP,    KEYD_UP);		// Up
	if (diff & CNT_DOWN)  I_PostEvent(kb_matrix_cur[0] & CNT_DOWN,  KEYD_DOWN);		// Down
	if (diff & CNT_LEFT)  I_PostEvent(kb_matrix_cur[0] & CNT_LEFT,  KEYD_LEFT);		// Left
	if (diff & CNT_RIGHT) I_PostEvent(kb_matrix_cur[0] & CNT_RIGHT, KEYD_RIGHT);	// Right
	if (diff & CNT_A)     I_PostEvent(kb_matrix_cur[0] & CNT_A,     KEYD_A);		// A
	if (diff & CNT_B)     I_PostEvent(kb_matrix_cur[0] & CNT_B,     KEYD_B);		// S
	if (diff & CNT_C)     I_PostEvent(kb_matrix_cur[0] & CNT_C,     KEYD_L);		// Q
	if (diff & CNT_D)     I_PostEvent(kb_matrix_cur[0] & CNT_D,     KEYD_R);		// W

	diff = kb_matrix_prv[1] ^ kb_matrix_cur[1];
	if (diff & CNT_START1) I_PostEvent(kb_matrix_cur[1] & CNT_START1, KEYD_START);	// 1
	if (diff & CNT_START2) I_PostEvent(kb_matrix_cur[1] & CNT_START2, KEYD_SELECT);	// 2
}


//**************************************************************************************
//
// Audio
//

#define RESET_SOUND_DRIVER 3


static const int16_t firstsfx = 3;


void DMX_Play(sfxenum_t id)
{
	*REG_SOUND = firstsfx + id;
}


void DMX_Init(void)
{
	*REG_SOUND = RESET_SOUND_DRIVER;
}


void DMX_Init2(void)
{
	// Do nothing
}


void DMX_Shutdown(void)
{
	*REG_SOUND = RESET_SOUND_DRIVER;
}


//**************************************************************************************
//
// Returns time in 1/35th second tics.
//

static volatile int32_t ticcount;

static boolean isTimerSet;


void rom_callback_VBlank() {
	ticcount++;
}


int32_t I_GetTime(void)
{
	return ticcount * TICRATE / 60;
}


void I_InitTimer(void)
{
	isTimerSet = true;
}


static void I_ShutdownTimer(void)
{
	// Do nothing
}


//**************************************************************************************
//
// Memory
//

// The Neo Geo has 64 KB of RAM.
// 53016 is the maximum value with which this program can still be compiled.
// Leave 2 KB for the stack.
#define HEAP_SIZE (53016-2*1024)
//#define HEAP_SIZE 53016


uint8_t __far* I_ZoneBase(uint32_t *heapSize)
{
	static uint8_t heap[HEAP_SIZE];
	uint32_t paragraphs = HEAP_SIZE / PARAGRAPH_SIZE;
	uint8_t *ptr = heap;

	// align ptr
	uint32_t m = (uint32_t) ptr;
	if ((m & (PARAGRAPH_SIZE - 1)) != 0)
	{
		paragraphs--;
		while ((m & (PARAGRAPH_SIZE - 1)) != 0)
			m = (uint32_t) ++ptr;
	}

	*heapSize = paragraphs * PARAGRAPH_SIZE;
	return ptr;
}


//**************************************************************************************
//
// Exit code
//

static void I_Shutdown(void)
{
	if (isGraphicsModeSet)
		I_ShutdownGraphics();

	I_ShutdownSound();

	if (isTimerSet)
		I_ShutdownTimer();

	if (isKeyboardIsrSet)
	{
		// Do nothing
	}
}


void I_Quit(void)
{
	I_Shutdown();

	for (;;) {}
	exit(0);
}


static void ng_printf(const char *text)
{
	int x = 0;
	int y = 0;
	MMAP_PALBANK1[1]     = 0x7FFF;
	MMAP_PALBANK1[0xfff] = 0x8000;
	*REG_VRAMADDR = ADDR_FIXMAP + ((x + 1) * 32) + y + 2;
	*REG_VRAMMOD = 32;
	while (*text)
	{
		if (*text == '\n')
		{
			text++;
			x = 0;
			y++;
			*REG_VRAMADDR = ADDR_FIXMAP + ((x + 1) * 32) + y + 2;
		}
		else
		{
			*REG_VRAMRW = *text++;
			x++;
			if (x == 38)
			{
				x = 0;
				y++;
				*REG_VRAMADDR = ADDR_FIXMAP + ((x + 1) * 32) + y + 2;
			}
		}
	}
}


void I_Error(const char *error, ...)
{
	va_list argptr;

	I_Shutdown();

	va_start(argptr, error);
	char buffer[80];
	vsprintf(buffer, error, argptr);
	ng_printf(buffer);
	va_end(argptr);

	for (;;) {}
	exit(1);
}


int main(void)
{
#if defined TIMEDEMO
	int argc = 3;
	const char * const argv[] = {"Doom64KB", "-timedemo", "demo3"};
#else
	int argc = 1;
	const char * const argv[] = {"Doom64KB"};
#endif
	D_DoomMain(argc, argv);

	return 0;
}
