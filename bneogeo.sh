mkdir neogeo
mkdir neogeo/rom

set -e

mkdir -p neogeo/assets/generated

unset CFLAGS


CROM_FILE_BYTES="${CROM_FILE_BYTES:-4194304}"

export RENDER_OPTIONS="-DFLAT_SPAN -DFLAT_NUKAGE1_COLOR=118 -DWAD_FILE=\"DOOM64TB.WAD\" -DVIEWWINDOWWIDTH=80 -DVIEWWINDOWHEIGHT=56 -DMAPWIDTH=80 -DLOW_MEMORY -DNEOGEO_SPRITE_MICROFB -DNEOGEO_HEAP_SIZE=44000"

export CPU=68000
m68k-neogeo-elf-gcc -c i_neogev.c $RENDER_OPTIONS -std=gnu17 -mlra -march=$CPU -Ofast -fomit-frame-pointer -fgcse-sm -flto -fwhole-program -funroll-loops -fira-loop-pressure -fno-tree-pre
m68k-neogeo-elf-gcc -c p_enemy2.c $RENDER_OPTIONS -std=gnu17 -mlra -march=$CPU -Ofast -fomit-frame-pointer -fgcse-sm -flto -fwhole-program -funroll-loops -fira-loop-pressure -fno-tree-pre
m68k-neogeo-elf-gcc -c p_map.c    $RENDER_OPTIONS -std=gnu17 -mlra -march=$CPU -Ofast -fomit-frame-pointer -fgcse-sm -flto -fwhole-program -funroll-loops -fira-loop-pressure -fno-tree-pre
m68k-neogeo-elf-gcc -c p_maputl.c $RENDER_OPTIONS -std=gnu17 -mlra -march=$CPU -Ofast -fomit-frame-pointer -fgcse-sm -flto -fwhole-program -funroll-loops -fira-loop-pressure -fno-tree-pre
m68k-neogeo-elf-gcc -c p_mobj.c   $RENDER_OPTIONS -std=gnu17 -mlra -march=$CPU -Ofast -fomit-frame-pointer -fgcse-sm -flto -fwhole-program -funroll-loops -fira-loop-pressure -fno-tree-pre
m68k-neogeo-elf-gcc -c p_sight.c  $RENDER_OPTIONS -std=gnu17 -mlra -march=$CPU -Ofast -fomit-frame-pointer -fgcse-sm -flto -fwhole-program -funroll-loops -fira-loop-pressure -fno-tree-pre
m68k-neogeo-elf-gcc -c r_data.c   $RENDER_OPTIONS -std=gnu17 -mlra -march=$CPU -Ofast -fomit-frame-pointer -fgcse-sm -flto -fwhole-program -funroll-loops -fira-loop-pressure -fno-tree-pre
m68k-neogeo-elf-gcc -c r_draw.c   $RENDER_OPTIONS -std=gnu17 -mlra -march=$CPU -Oz -fomit-frame-pointer -flto -fwhole-program -funroll-loops -fira-loop-pressure -funsafe-loop-optimizations -freorder-blocks-algorithm=stc -fno-tree-pre
m68k-neogeo-elf-gcc -c r_plane.c  $RENDER_OPTIONS -std=gnu17 -mlra -march=$CPU -Ofast -fomit-frame-pointer -fgcse-sm -flto -fwhole-program -funroll-loops -fira-loop-pressure -fno-tree-pre
m68k-neogeo-elf-gcc -c tables.c   $RENDER_OPTIONS -std=gnu17 -mlra -march=$CPU -Ofast -fomit-frame-pointer -fgcse-sm -flto -fwhole-program -funroll-loops -fira-loop-pressure -fno-tree-pre
m68k-neogeo-elf-gcc -c w_wad.c    $RENDER_OPTIONS -std=gnu17 -mlra -march=$CPU -Ofast -fomit-frame-pointer -fgcse-sm -flto -fwhole-program -funroll-loops -fira-loop-pressure -fno-tree-pre
m68k-neogeo-elf-gcc -c z_zone.c   $RENDER_OPTIONS -std=gnu17 -mlra -march=$CPU -Ofast -fomit-frame-pointer -fgcse-sm -flto -fwhole-program -funroll-loops -fira-loop-pressure -fno-tree-pre

export CFLAGS="-std=gnu17 -mlra -march=$CPU -Oz -fomit-frame-pointer -flto -fwhole-program -funroll-loops -fira-loop-pressure -funsafe-loop-optimizations -freorder-blocks-algorithm=stc -fno-tree-pre"
#export CFLAGS="$CFLAGS -Ofast -flto -fwhole-program -fomit-frame-pointer -funroll-loops -fgcse-sm -fgcse-las -fipa-pta -Wno-attributes -Wpedantic"
#export CFLAGS="$CFLAGS -Wall -Wextra"

for arg in "$@"
do
  if [ "$arg" = "-timedemo" ]
  then
    export CFLAGS="$CFLAGS -DTIMEDEMO"
    break
  fi
done

export GLOBOBJS="  am_map.c"
export GLOBOBJS+=" d_items.c"
export GLOBOBJS+=" d_main.c"
export GLOBOBJS+=" f_finale.c"
export GLOBOBJS+=" f_libt.c"
export GLOBOBJS+=" g_game.c"
export GLOBOBJS+=" hu_text.c"
export GLOBOBJS+=" i_audio.c"
export GLOBOBJS+=" i_neogeo.c"
#export GLOBOBJS+=" i_neogev.c"
export GLOBOBJS+=" i_neogev.o"
export GLOBOBJS+=" info.c"
export GLOBOBJS+=" m_cheat.c"
export GLOBOBJS+=" m_text.c"
export GLOBOBJS+=" m_random.c"
export GLOBOBJS+=" p_doors.c"
export GLOBOBJS+=" p_enemy.c"
#export GLOBOBJS+=" p_enemy2.c"
export GLOBOBJS+=" p_enemy2.o"
export GLOBOBJS+=" p_floor.c"
export GLOBOBJS+=" p_inter.c"
export GLOBOBJS+=" p_lights.c"
#export GLOBOBJS+=" p_map.c"
export GLOBOBJS+=" p_map.o"
#export GLOBOBJS+=" p_maputl.c"
export GLOBOBJS+=" p_maputl.o"
#export GLOBOBJS+=" p_mobj.c"
export GLOBOBJS+=" p_mobj.o"
export GLOBOBJS+=" p_plats.c"
export GLOBOBJS+=" p_pspr.c"
export GLOBOBJS+=" p_setup.c"
#export GLOBOBJS+=" p_sight.c"
export GLOBOBJS+=" p_sight.o"
export GLOBOBJS+=" p_spec.c"
export GLOBOBJS+=" p_switch.c"
export GLOBOBJS+=" p_telept.c"
export GLOBOBJS+=" p_tick.c"
export GLOBOBJS+=" p_user.c"
#export GLOBOBJS+=" r_data.c"
export GLOBOBJS+=" r_data.o"
#export GLOBOBJS+=" r_draw.c"
export GLOBOBJS+=" r_draw.o"
#export GLOBOBJS+=" r_plane.c"
export GLOBOBJS+=" r_plane.o"
export GLOBOBJS+=" r_sky.c"
export GLOBOBJS+=" r_things.c"
export GLOBOBJS+=" s_sound.c"
export GLOBOBJS+=" sounds.c"
export GLOBOBJS+=" st_pal.c"
export GLOBOBJS+=" st_text.c"
#export GLOBOBJS+=" tables.c"
export GLOBOBJS+=" tables.o"
#export GLOBOBJS+=" w_wad.c"
export GLOBOBJS+=" w_wad.o"
export GLOBOBJS+=" wi_libt.c"
export GLOBOBJS+=" wi_stuff.c"
export GLOBOBJS+=" z_bmallo.c"
#export GLOBOBJS+=" z_zone.c"
export GLOBOBJS+=" z_zone.o"

m68k-neogeo-elf-gcc -specs ngdevkit -lngdevkit $GLOBOBJS $CFLAGS $RENDER_OPTIONS -o neogeo/DOOM64KB.elf

rm i_neogev.o
rm p_enemy2.o
rm p_map.o
rm p_maputl.o
rm p_mobj.o
rm p_sight.o
rm r_data.o
rm r_draw.o
rm r_plane.o
rm tables.o
rm w_wad.o
rm z_zone.o

m68k-neogeo-elf-objcopy -O binary -S -R .text2 --gap-fill 0xff --pad-to 1048576 neogeo/DOOM64KB.elf neogeo/rom/doom64kb-p1.p1 && dd if=neogeo/rom/doom64kb-p1.p1 of=neogeo/rom/doom64kb-p1.p1 conv=notrunc,swab status=none
z80-neogeo-ihx-sdobjcopy -I ihex -O binary neogeo/assets/base-sound-driver.ihx neogeo/rom/doom64kb-m1.m1 --pad-to 131072
python3 tools/gen_neogeo_color_tiles.py \
  --base-c1 neogeo/assets/base-crom-logo.c1 \
  --base-c2 neogeo/assets/base-crom-logo.c2 \
  --out-c1 neogeo/assets/generated/doom_color_microfb.c1 \
  --out-c2 neogeo/assets/generated/doom_color_microfb.c2 \
  --crom-size "$CROM_FILE_BYTES"
cp neogeo/assets/generated/doom_color_microfb.c1 neogeo/rom/doom64kb-c1.c1 && truncate -s "$CROM_FILE_BYTES" neogeo/rom/doom64kb-c1.c1
cp neogeo/assets/generated/doom_color_microfb.c2 neogeo/rom/doom64kb-c2.c2 && truncate -s "$CROM_FILE_BYTES" neogeo/rom/doom64kb-c2.c2
echo neogeo/assets/ibmfont.fix | xargs -r cat > neogeo/rom/doom64kb-s1.s1 && truncate -s 131072 neogeo/rom/doom64kb-s1.s1
echo | xargs -r cat > neogeo/rom/doom64kb-v1.v1 && truncate -s 524288 neogeo/rom/doom64kb-v1.v1
romtool.py -b cartridge -f zip -p neogeo/rom/doom64kb-p1.p1 -c neogeo/rom/doom64kb-c1.c1 neogeo/rom/doom64kb-c2.c2 -v neogeo/rom/doom64kb-v1.v1 -s neogeo/rom/doom64kb-s1.s1 -m neogeo/rom/doom64kb-m1.m1 -n doom64kb -x "zip.comment=" -o neogeo/rom/doom64kb.zip
romtool.py -b hash -f mame  -p neogeo/rom/doom64kb-p1.p1 -c neogeo/rom/doom64kb-c1.c1 neogeo/rom/doom64kb-c2.c2 -v neogeo/rom/doom64kb-v1.v1 -s neogeo/rom/doom64kb-s1.s1 -m neogeo/rom/doom64kb-m1.m1 -n doom64kb -l "Doom64KB: Neo Geo Edition" -o neogeo/rom/neogeo.xml
romtool.py -b hash -f gngeo -p neogeo/rom/doom64kb-p1.p1 -c neogeo/rom/doom64kb-c1.c1 neogeo/rom/doom64kb-c2.c2 -v neogeo/rom/doom64kb-v1.v1 -s neogeo/rom/doom64kb-s1.s1 -m neogeo/rom/doom64kb-m1.m1 -n doom64kb -l "Doom64KB: Neo Geo Edition" -x gngeo.data=/usr/share/ngdevkit-gngeo/gngeo_data.zip -o neogeo/rom/gngeo_data.zip
cp /usr/share/ngdevkit/aes.zip    neogeo/rom/aes.zip
cp /usr/share/ngdevkit/neogeo.zip neogeo/rom/neogeo.zip

for arg in "$@"
do
  if [ "$arg" = "-run" ]
  then
    ngdevkit-gngeo -b glsl --shaderpath="/mnt/c/Projects/ngdevkit-examples/shaders" --shader="qcrt-flat.glslp" --scale 3 --no-resize --system home -i neogeo/rom -d neogeo/rom/gngeo_data.zip doom64kb --p1control A=K97,B=K115,C=K113,D=K119,START=K49,COIN=K51,UP=K82,DOWN=K81,LEFT=K80,RIGHT=K79
    break
  fi
done
