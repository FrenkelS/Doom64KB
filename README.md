## Doom64KB
![Doom64KB: Neo Geo Edition](readme_imgs/neogeo.png?raw=true)

Doom was originally designed in 1993 for 32-bit DOS computers with 4 MB of RAM.
It's mostly written in C code with very little assembly code.
It has been ported to all kinds of systems.
Usually these systems are 32-bit or more and have a flat memory model.

Doom64KB is a port for computers with only 64 KB of RAM and lots of room for ROM.
It's based on [Doom8088](https://github.com/FrenkelS/Doom8088), a port of Doom for 16-bit DOS computers.

**What's special?:**
 - Supports only Doom 1 E1M1 and E1M8
 - Digital sound effects
 - No music
 - No texture mapped floors and ceilings
 - No light diminishing
 - No saving and loading
 - No multiplayer
 - No screen resizing

## Controls:
|Action      |Neo Geo |
|------------|--------|
|Walk        |Joystick|
|Fire        |B       |
|Use / Sprint|A       |
|Strafe      |C & D   |
|Weapon up   |A + D   |
|Weapon down |A + C   |
|Menu        |Start   |

## Cheats:
|Effects                 |Code                                             |
|------------------------|-------------------------------------------------|
|Chainsaw                |C,    Up,   Up,   Left,  C,    A,    A,     Up   |
|God mode                |Up,   UP,   Down, Down,  Left, Left, Right, Right|
|Weapons & Keys          |C,    Left, D,    Right, A,    Up,   A,     Up   |
|Weapons                 |D,    D,    A,    D,     A,    Up,   Up,    Left |
|No Clipping             |Up,   Down, Left, Right, Up,   Down, Left,  Right|
|Invisibility            |A,    A,    A,    B,     A,    A,    C,     B    |
|Radiation shielding suit|B,    B,    D,    Up,    A,    A,    D,     B    |
|Auto-map                |C,    A,    D,    B,     A,    D,    C,     Up   |
|Lite-Amp Goggles        |Down, Left, D,    Left,  D,    C,    C,     A    |
|Toggle FPS counter      |A,    B,    C,    Up,    Down, B,    Left,  Left |

## Building:
|Platform|Platform specific code    |Compiler                                        |Build script|Additional information                                                                                                       |
|--------|--------------------------|------------------------------------------------|------------|-----------------------------------------------------------------------------------------------------------------------------|
|Neo Geo |`i_neogeo.c`, `i_neogev.c`|[ngdevkit](https://github.com/dciabrin/ngdevkit)|`bneogeo.sh`|Use command line argument `-timedemo` to build a version that runs the timedemo benchmark, `-run` to compile and run the game|
