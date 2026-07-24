/*-----------------------------------------------------------------------------
 *
 * Copyright (C) 2024-2026 Frenkel Smeijers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * DESCRIPTION:
 *  Neo Geo intermission screen library.
 *
 *-----------------------------------------------------------------------------*/

#include "doomdef.h"
#include "i_video.h"
#include "i_vtext.h"
#include "wi_lib.h"

#include "neogeo/doom_fix_intermission_assets.h"


#define WI_FIX_WIDTH  38
#define WI_FIX_HEIGHT 28


typedef struct
{
	const uint16_t *entries;
	uint8_t cols;
	uint8_t rows;
} wi_fix_patch_t;


typedef struct
{
	int16_t x;
	int16_t y;
} point_t;


typedef enum
{
	WI_FIX_PAGE_NONE,
	WI_FIX_PAGE_STATS,
	WI_FIX_PAGE_ENTERING
} wi_fix_page_t;


static wi_fix_page_t current_page = WI_FIX_PAGE_NONE;
extern boolean _g_menuactive;


static const wi_fix_patch_t level_names[] =
{
	{doom_intermission_wilv00, DOOM_INTERMISSION_WILV00_COLS, DOOM_INTERMISSION_WILV00_ROWS},
	{doom_intermission_wilv01, DOOM_INTERMISSION_WILV01_COLS, DOOM_INTERMISSION_WILV01_ROWS},
	{doom_intermission_wilv02, DOOM_INTERMISSION_WILV02_COLS, DOOM_INTERMISSION_WILV02_ROWS},
	{doom_intermission_wilv03, DOOM_INTERMISSION_WILV03_COLS, DOOM_INTERMISSION_WILV03_ROWS},
	{doom_intermission_wilv04, DOOM_INTERMISSION_WILV04_COLS, DOOM_INTERMISSION_WILV04_ROWS},
	{doom_intermission_wilv05, DOOM_INTERMISSION_WILV05_COLS, DOOM_INTERMISSION_WILV05_ROWS},
	{doom_intermission_wilv06, DOOM_INTERMISSION_WILV06_COLS, DOOM_INTERMISSION_WILV06_ROWS},
	{doom_intermission_wilv07, DOOM_INTERMISSION_WILV07_COLS, DOOM_INTERMISSION_WILV07_ROWS},
	{doom_intermission_wilv08, DOOM_INTERMISSION_WILV08_COLS, DOOM_INTERMISSION_WILV08_ROWS},
};


static const wi_fix_patch_t digits[] =
{
	{doom_intermission_winum0, DOOM_INTERMISSION_WINUM0_COLS, DOOM_INTERMISSION_WINUM0_ROWS},
	{doom_intermission_winum1, DOOM_INTERMISSION_WINUM1_COLS, DOOM_INTERMISSION_WINUM1_ROWS},
	{doom_intermission_winum2, DOOM_INTERMISSION_WINUM2_COLS, DOOM_INTERMISSION_WINUM2_ROWS},
	{doom_intermission_winum3, DOOM_INTERMISSION_WINUM3_COLS, DOOM_INTERMISSION_WINUM3_ROWS},
	{doom_intermission_winum4, DOOM_INTERMISSION_WINUM4_COLS, DOOM_INTERMISSION_WINUM4_ROWS},
	{doom_intermission_winum5, DOOM_INTERMISSION_WINUM5_COLS, DOOM_INTERMISSION_WINUM5_ROWS},
	{doom_intermission_winum6, DOOM_INTERMISSION_WINUM6_COLS, DOOM_INTERMISSION_WINUM6_ROWS},
	{doom_intermission_winum7, DOOM_INTERMISSION_WINUM7_COLS, DOOM_INTERMISSION_WINUM7_ROWS},
	{doom_intermission_winum8, DOOM_INTERMISSION_WINUM8_COLS, DOOM_INTERMISSION_WINUM8_ROWS},
	{doom_intermission_winum9, DOOM_INTERMISSION_WINUM9_COLS, DOOM_INTERMISSION_WINUM9_ROWS},
};


static const point_t lnodes[] =
{
	{185, 164},
	{148, 143},
	{ 69, 122},
	{209, 102},
	{116,  89},
	{166,  55},
	{ 71,  56},
	{135,  29},
	{ 71,  24},
};


static boolean WI_canDraw(void)
{
	if (_g_menuactive || I_NeoGeoFixWipeActive())
	{
		current_page = WI_FIX_PAGE_NONE;
		return false;
	}

	return true;
}


static void WI_selectPage(wi_fix_page_t page)
{
	if (current_page == page)
		return;

	I_InitScreenPage();
	current_page = page;
}


static void WI_drawPatch(const wi_fix_patch_t *patch, int16_t x, int16_t y)
{
	V_DrawFixPatch(x, y, patch->entries, patch->cols, patch->rows);
}


static void WI_drawPatchCentered(const wi_fix_patch_t *patch, int16_t y)
{
	WI_drawPatch(patch, (WI_FIX_WIDTH - patch->cols) / 2, y);
}


static int16_t WI_fixX(int16_t x)
{
	return (x * WI_FIX_WIDTH + SCREENWIDTH_VGA / 2) / SCREENWIDTH_VGA;
}


static int16_t WI_fixY(int16_t y)
{
	return (y * WI_FIX_HEIGHT + SCREENHEIGHT_VGA / 2) / SCREENHEIGHT_VGA;
}


void WI_drawLF(int16_t map)
{
	static const wi_fix_patch_t finished =
	{
		doom_intermission_wif,
		DOOM_INTERMISSION_WIF_COLS,
		DOOM_INTERMISSION_WIF_ROWS
	};

	if (!WI_canDraw())
		return;

	WI_selectPage(WI_FIX_PAGE_STATS);
	WI_drawPatchCentered(&level_names[map], 0);
	WI_drawPatchCentered(&finished, 2);
}


void WI_drawEL(int16_t map)
{
	static const wi_fix_patch_t entering =
	{
		doom_intermission_wienter,
		DOOM_INTERMISSION_WIENTER_COLS,
		DOOM_INTERMISSION_WIENTER_ROWS
	};

	if (!WI_canDraw())
		return;

	WI_selectPage(WI_FIX_PAGE_ENTERING);
	WI_drawPatchCentered(&entering, 0);
	WI_drawPatchCentered(&level_names[map], 2);
}


int16_t WI_getColonWidth(void)
{
	return DOOM_INTERMISSION_WICOLON_COLS;
}


void WI_drawColon(int16_t x, int16_t y)
{
	static const wi_fix_patch_t colon =
	{
		doom_intermission_wicolon,
		DOOM_INTERMISSION_WICOLON_COLS,
		DOOM_INTERMISSION_WICOLON_ROWS
	};
	if (WI_canDraw())
		WI_drawPatch(&colon, x, y);
}


void WI_drawSucks(int16_t x, int16_t y)
{
	static const wi_fix_patch_t sucks =
	{
		doom_intermission_wisucks,
		DOOM_INTERMISSION_WISUCKS_COLS,
		DOOM_INTERMISSION_WISUCKS_ROWS
	};
	if (WI_canDraw())
		WI_drawPatch(&sucks, x - sucks.cols, y);
}


void WI_drawPercentSign(int16_t x, int16_t y)
{
	static const wi_fix_patch_t percent =
	{
		doom_intermission_wipcnt,
		DOOM_INTERMISSION_WIPCNT_COLS,
		DOOM_INTERMISSION_WIPCNT_ROWS
	};
	if (WI_canDraw())
		WI_drawPatch(&percent, x, y);
}


int16_t WI_drawNum(int16_t x, int16_t y, int16_t n, int16_t count)
{
	if (!WI_canDraw())
		return x - count;

	while (count--)
	{
		const wi_fix_patch_t *digit = &digits[n % 10];
		x -= digit->cols;
		WI_drawPatch(digit, x, y);
		n /= 10;
	}

	return x;
}


void WI_drawStats(
	int16_t cnt_kills,
	int16_t cnt_items,
	int16_t cnt_secret,
	int32_t cnt_time,
	int32_t cnt_total_time,
	int16_t cnt_par)
{
	static const wi_fix_patch_t kills =
	{
		doom_intermission_wiostk,
		DOOM_INTERMISSION_WIOSTK_COLS,
		DOOM_INTERMISSION_WIOSTK_ROWS
	};
	static const wi_fix_patch_t items =
	{
		doom_intermission_wiosti,
		DOOM_INTERMISSION_WIOSTI_COLS,
		DOOM_INTERMISSION_WIOSTI_ROWS
	};
	static const wi_fix_patch_t secret =
	{
		doom_intermission_wiscrt2,
		DOOM_INTERMISSION_WISCRT2_COLS,
		DOOM_INTERMISSION_WISCRT2_ROWS
	};
	static const wi_fix_patch_t time =
	{
		doom_intermission_witime,
		DOOM_INTERMISSION_WITIME_COLS,
		DOOM_INTERMISSION_WITIME_ROWS
	};
	static const wi_fix_patch_t total =
	{
		doom_intermission_wimstt,
		DOOM_INTERMISSION_WIMSTT_COLS,
		DOOM_INTERMISSION_WIMSTT_ROWS
	};
	static const wi_fix_patch_t par =
	{
		doom_intermission_wipar,
		DOOM_INTERMISSION_WIPAR_COLS,
		DOOM_INTERMISSION_WIPAR_ROWS
	};

	if (!WI_canDraw())
		return;

	WI_drawPatch(&kills, 6, 7);
	WI_drawPercent(33, 7, cnt_kills);

	WI_drawPatch(&items, 6, 10);
	WI_drawPercent(33, 10, cnt_items);

	WI_drawPatch(&secret, 6, 13);
	WI_drawPercent(33, 13, cnt_secret);

	WI_drawPatch(&time, 1, 22);
	WI_drawTime(19, 22, cnt_time);

	WI_drawPatch(&total, 1, 25);
	WI_drawTime(19, 25, cnt_total_time);

	WI_drawPatch(&par, 20, 22);
	WI_drawTime(37, 22, cnt_par);
}


void WI_drawSplat(int16_t i)
{
	static const wi_fix_patch_t splat =
	{
		doom_intermission_wisplat,
		DOOM_INTERMISSION_WISPLAT_COLS,
		DOOM_INTERMISSION_WISPLAT_ROWS
	};

	if (!WI_canDraw())
		return;

	/* The first splat starts a fresh map overlay every frame. */
	if (i == 0)
	{
		I_InitScreenPage();
		current_page = WI_FIX_PAGE_ENTERING;
	}

	WI_drawPatch(
		&splat,
		WI_fixX(lnodes[i].x - 15),
		WI_fixY(lnodes[i].y - 11));
}


void WI_drawYouAreHere(int16_t i)
{
	static const wi_fix_patch_t yah =
	{
		doom_intermission_wiurh0,
		DOOM_INTERMISSION_WIURH0_COLS,
		DOOM_INTERMISSION_WIURH0_ROWS
	};

	if (!WI_canDraw())
		return;

	WI_selectPage(WI_FIX_PAGE_ENTERING);
	WI_drawPatch(
		&yah,
		WI_fixX(lnodes[i].x + 2),
		WI_fixY(lnodes[i].y - 15));
}
