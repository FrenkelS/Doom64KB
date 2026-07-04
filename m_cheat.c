#include "m_cheat.h"
#include "doomdef.h"
#include "d_englsh.h"
#include "p_inter.h"
#include "g_game.h"
#include "globdata.h"


static void cheat_god(void);
static void cheat_choppers(void);
static void cheat_idkfa(void);
static void cheat_ammo(void);
static void cheat_noclip(void);
static void cheat_invisibility(void);
static void cheat_rad(void);
static void cheat_map(void);
static void cheat_goggles(void);
static void cheat_fps(void);


typedef struct
{
	void (*cheat_function)(void);
	uint32_t packed_sequence;
} cheatseq_t;


#define CHEAT_SEQ(a,b,c,d,e,f,g,h) ((a << 28)|(b << 24)|(c << 20)|(d << 16)|(e << 12)|(f << 8)|(g << 4)|(h))


static const cheatseq_t cheat_def[] =
{
	{cheat_choppers,     CHEAT_SEQ(KEYD_L,    KEYD_UP,   KEYD_UP,   KEYD_LEFT,  KEYD_L,    KEYD_A,     KEYD_A,    KEYD_UP)},
	{cheat_god,          CHEAT_SEQ(KEYD_UP,   KEYD_UP,   KEYD_DOWN, KEYD_DOWN,  KEYD_LEFT, KEYD_RIGHT, KEYD_LEFT, KEYD_RIGHT)},
	{cheat_idkfa,        CHEAT_SEQ(KEYD_L,    KEYD_LEFT, KEYD_R,    KEYD_RIGHT, KEYD_A,    KEYD_UP,    KEYD_A,    KEYD_UP)},
	{cheat_ammo,         CHEAT_SEQ(KEYD_R,    KEYD_R,    KEYD_A,    KEYD_R,     KEYD_A,    KEYD_UP,    KEYD_UP,   KEYD_LEFT)},
	{cheat_noclip,       CHEAT_SEQ(KEYD_UP,   KEYD_DOWN, KEYD_LEFT, KEYD_RIGHT, KEYD_UP,   KEYD_DOWN,  KEYD_LEFT, KEYD_RIGHT)},
	{cheat_invisibility, CHEAT_SEQ(KEYD_A,    KEYD_A,    KEYD_A,    KEYD_B,     KEYD_A,    KEYD_A,     KEYD_L,    KEYD_B)},
	{cheat_rad,          CHEAT_SEQ(KEYD_B,    KEYD_B,    KEYD_R,    KEYD_UP,    KEYD_A,    KEYD_A,     KEYD_R,    KEYD_B)},
	{cheat_map,          CHEAT_SEQ(KEYD_L,    KEYD_A,    KEYD_R,    KEYD_B,     KEYD_A,    KEYD_R,     KEYD_L,    KEYD_UP)},
	{cheat_goggles,      CHEAT_SEQ(KEYD_DOWN, KEYD_LEFT, KEYD_R,    KEYD_LEFT,  KEYD_R,    KEYD_L,     KEYD_L,    KEYD_A)},
	{cheat_fps,          CHEAT_SEQ(KEYD_A,    KEYD_B,    KEYD_L,    KEYD_UP,    KEYD_DOWN, KEYD_B,     KEYD_LEFT, KEYD_LEFT)}
};


static const int16_t num_cheats = sizeof(cheat_def) / sizeof(cheatseq_t);


boolean C_Responder(event_t *ev)
{
	static uint32_t cheat_buffer;

	if (ev->type == ev_keydown)
	{
		//To enable fast cheat searching without having to
		//maintain buffer of keypresses, we ensure that
		//cheats are 8 keys long and the key-code is less
		//than 16 so they fit in 4 bits.

		//We can store a full 8 presses in a 32bit int.

		//Adding a press to the list just means shifting the
		//whole thing by 4 and ORing the next press into the
		//low bits.

		//We can test a cheat sequence with a simple int comparison.

		cheat_buffer = (cheat_buffer << 4) | ev->data1;

		for (int16_t i = 0; i < num_cheats; i++)
		{
			if (cheat_def[i].packed_sequence == cheat_buffer)
			{
				cheat_def[i].cheat_function();
				return true;
			}
		}
	}

	return false;
}


static void cheat_god()
{
    _g_player.cheats ^= CF_GODMODE;

    if(_g_player.cheats & CF_GODMODE)
    {
        _g_player.health = god_health;

        _g_player.message = STSTR_DQDON;
    }
    else
    {
        _g_player.message = STSTR_DQDOFF;

    }
}

static void cheat_choppers()
{
    _g_player.weaponowned[wp_chainsaw] = true;
    _g_player.pendingweapon = wp_chainsaw;

    _g_player.message = STSTR_CHOPPERS;
}

static void cheat_idkfa()
{
    int16_t i;

    player_t* plyr = &_g_player;

    if (!plyr->backpack)
    {
        for (i=0 ; i<NUMAMMO ; i++)
            plyr->maxammo[i] *= 2;

        plyr->backpack = true;
    }

    plyr->armorpoints = idfa_armor;      // Ty 03/09/98 - deh
    plyr->armortype = idfa_armor_class;  // Ty 03/09/98 - deh

    // You can't own weapons that aren't in the game // phares 02/27/98
    for (i=0;i<NUMWEAPONS;i++)
        if (!(i == wp_plasma || i == wp_bfg || i == wp_supershotgun))
            plyr->weaponowned[i] = true;

    for (i=0;i<NUMAMMO;i++)
        if (i!=am_cell)
            plyr->ammo[i] = plyr->maxammo[i];

    for (i=0;i<NUMCARDS;i++)
            plyr->cards[i] = true;

    plyr->message = STSTR_KFAADDED;
}

static void cheat_ammo()
{
    int16_t i;
    player_t* plyr = &_g_player;

    if (!plyr->backpack)
    {
        for (i=0 ; i<NUMAMMO ; i++)
            plyr->maxammo[i] *= 2;

        plyr->backpack = true;
    }

    plyr->armorpoints = idfa_armor;      // Ty 03/09/98 - deh
    plyr->armortype = idfa_armor_class;  // Ty 03/09/98 - deh

    // You can't own weapons that aren't in the game // phares 02/27/98
    for (i=0;i<NUMWEAPONS;i++)
        if (!(i == wp_plasma || i == wp_bfg || i == wp_supershotgun))
            plyr->weaponowned[i] = true;

    for (i=0;i<NUMAMMO;i++)
        if (i!=am_cell)
            plyr->ammo[i] = plyr->maxammo[i];

    plyr->message = STSTR_FAADDED;
}

static void cheat_noclip()
{
    _g_player.cheats ^= CF_NOCLIP;

    if(_g_player.cheats & CF_NOCLIP)
    {
        _g_player.message = STSTR_NCON;
    }
    else
    {
        _g_player.message = STSTR_NCOFF;

    }
}


static void cheat_invisibility()
{
    P_GivePower(&_g_player, pw_invisibility);
}

static void cheat_rad()
{
    P_GivePower(&_g_player, pw_ironfeet);
}

static void cheat_map()
{
    P_GivePower(&_g_player, pw_allmap);
}

static void cheat_goggles()
{
    P_GivePower(&_g_player, pw_infrared);
}


static void cheat_fps()
{
    _g_fps_show = !_g_fps_show;
	if(_g_fps_show)
	{
		_g_player.message = STSTR_FPSON;
	}else
	{
		_g_player.message = STSTR_FPSOFF;
	}
}
