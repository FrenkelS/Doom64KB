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
 - No audio
 - No texture mapped floors and ceilings
 - No light diminishing
 - No saving and loading
 - No multiplayer
 - No PWADs
 - No screen resizing

## Building:
|Platform|Platform specific code    |Compiler                                        |Build script|
|--------|--------------------------|------------------------------------------------|------------|
|Neo Geo |`i_neogeo.c`, `i_neogev.c`|[ngdevkit](https://github.com/dciabrin/ngdevkit)|`bneogeo.sh`|
