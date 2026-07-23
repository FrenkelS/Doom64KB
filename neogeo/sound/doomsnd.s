;; Doom64KB standalone YM2610 sound driver.
;;
;; Commands and V-ROM offsets come from tools/gen_neogeo_audio.py. ADPCM-A
;; effects use six priority-aware hardware channels; ADPCM-B music loops in
;; hardware until STOP or ALL_OFF is received.

        .module doomsnd
        .area   CODE (ABS)

        CMD_Q_HEAD      = 0xf800
        CMD_Q_TAIL      = 0xf801
        CMD_Q_BUF       = 0xf808
        CHAN_BUSY       = 0xf810
        CHAN_PRI        = 0xf816
        SFX_START_LO    = 0xf820
        SFX_START_HI    = 0xf821
        SFX_STOP_LO     = 0xf822
        SFX_STOP_HI     = 0xf823
        SFX_PRIORITY    = 0xf824
        SFX_PANVOL      = 0xf825
        SFX_CHANNEL     = 0xf826
        SFX_MASTER      = 0xf827
        MUSIC_MASTER    = 0xf828

        .org    0x0000
_reset:
        di
        ld      sp, #0xfffc
        jp      _init

        .org    0x0066
_nmi:
        push    af
        push    bc
        push    de
        push    hl
        in      a, (0x00)
        ld      b, a
        out     (0x0c), a
        or      a
        jr      z, _nmi_done
        cp      #0x01
        jp      z, _bios_prepare_switch
        cp      #0x03
        jp      z, _bios_soft_reset
        cp      #0x02
        jr      z, _nmi_done
        cp      #DOOM_AUDIO_CMD_MUSIC_BASE
        jr      nc, _nmi_queue
        cp      #DOOM_AUDIO_CMD_SFX_BASE
        jr      c, _nmi_done
        cp      #DOOM_AUDIO_CMD_SFX_END
        jr      nc, _nmi_done
_nmi_queue:
        ld      a, (CMD_Q_TAIL)
        ld      c, a
        inc     a
        and     #0x07
        ld      hl, #CMD_Q_HEAD
        cp      (hl)
        jr      z, _nmi_done
        ld      hl, #CMD_Q_BUF
        ld      e, c
        ld      d, #0
        add     hl, de
        ld      (hl), b
        ld      a, c
        inc     a
        and     #0x07
        ld      (CMD_Q_TAIL), a
_nmi_done:
        pop     hl
        pop     de
        pop     bc
        pop     af
        retn

_bios_soft_reset:
        jp      _reset

_bios_prepare_switch:
        call    _all_off
        ld      bc, #0xfe18
        ld      (0xfffe), bc
        ld      a, #1
        out     (0x08), a
        jp      0xfffe

        .org    0x0100
_init:
        xor     a
        ld      (CMD_Q_HEAD), a
        ld      (CMD_Q_TAIL), a
        ld      hl, #CHAN_BUSY
        ld      b, #12
_init_clear:
        ld      (hl), a
        inc     hl
        djnz    _init_clear
        ld      a, #0x3f
        ld      (SFX_MASTER), a
        ld      a, #0xff
        ld      (MUSIC_MASTER), a
        ld      a, #1
        out     (0x08), a
        call    _all_off
_idle:
        call    _poll_adpcma_end
        call    _dequeue_cmd
        or      a
        jr      z, _idle
        call    _dispatch_cmd
        jr      _idle

_dequeue_cmd:
        ld      a, (CMD_Q_HEAD)
        ld      c, a
        ld      hl, #CMD_Q_TAIL
        cp      (hl)
        jr      z, _dequeue_empty
        ld      hl, #CMD_Q_BUF
        ld      e, c
        ld      d, #0
        add     hl, de
        ld      b, (hl)
        ld      a, c
        inc     a
        and     #0x07
        ld      (CMD_Q_HEAD), a
        ld      a, b
        ret
_dequeue_empty:
        xor     a
        ret

_dispatch_cmd:
        cp      #DOOM_AUDIO_CMD_STOP
        jp      z, _music_stop
        cp      #DOOM_AUDIO_CMD_ALL_OFF
        jp      z, _all_off
        cp      #DOOM_AUDIO_CMD_SFX_VOLUME_BASE
        jr      c, _dispatch_playback
        cp      #DOOM_AUDIO_CMD_SFX_VOLUME_END
        jr      c, _set_sfx_volume
        cp      #DOOM_AUDIO_CMD_MUSIC_VOLUME_BASE
        ret     c
        cp      #DOOM_AUDIO_CMD_MUSIC_VOLUME_END
        jr      c, _set_music_volume
        ret
_dispatch_playback:
        cp      #DOOM_AUDIO_CMD_MUSIC_BASE
        jr      c, _dispatch_sfx
        cp      #DOOM_AUDIO_CMD_MUSIC_END
        ret     nc
        sub     #DOOM_AUDIO_CMD_MUSIC_BASE
        jp      _music_start_index
_dispatch_sfx:
        cp      #DOOM_AUDIO_CMD_SFX_BASE
        ret     c
        cp      #DOOM_AUDIO_CMD_SFX_END
        ret     nc
        sub     #DOOM_AUDIO_CMD_SFX_BASE
        call    _play_sfx
        ret

_set_sfx_volume:
        sub     #DOOM_AUDIO_CMD_SFX_VOLUME_BASE
        ld      e, a
        ld      d, #0
        ld      hl, #_sfx_volume_table
        add     hl, de
        ld      a, (hl)
        ld      (SFX_MASTER), a
        ld      b, a
        ld      c, #0x01
        call    _ymb
        ret

_set_music_volume:
        sub     #DOOM_AUDIO_CMD_MUSIC_VOLUME_BASE
        ld      e, a
        ld      d, #0
        ld      hl, #_music_volume_table
        add     hl, de
        ld      a, (hl)
        ld      (MUSIC_MASTER), a
        ld      b, a
        ld      c, #0x1b
        call    _yma
        ret

_poll_adpcma_end:
        in      a, (0x06)
        ld      b, a
        ld      hl, #CHAN_BUSY
        bit     0, b
        jr      z, 1$
        ld      (hl), #0
1$:     inc     hl
        bit     1, b
        jr      z, 2$
        ld      (hl), #0
2$:     inc     hl
        bit     2, b
        jr      z, 3$
        ld      (hl), #0
3$:     inc     hl
        bit     3, b
        jr      z, 4$
        ld      (hl), #0
4$:     inc     hl
        bit     4, b
        jr      z, 5$
        ld      (hl), #0
5$:     inc     hl
        bit     5, b
        ret     z
        ld      (hl), #0
        ret

_play_sfx:
        ld      l, a
        ld      h, #0
        add     hl, hl
        ld      d, h
        ld      e, l
        add     hl, hl
        add     hl, de
        ld      de, #doom_sfx_table
        add     hl, de
        ld      a, (hl)
        ld      (SFX_START_LO), a
        inc     hl
        ld      a, (hl)
        ld      (SFX_START_HI), a
        inc     hl
        ld      a, (hl)
        ld      (SFX_STOP_LO), a
        inc     hl
        ld      a, (hl)
        ld      (SFX_STOP_HI), a
        inc     hl
        ld      a, (hl)
        ld      (SFX_PRIORITY), a
        inc     hl
        ld      a, (hl)
        ld      (SFX_PANVOL), a
        call    _alloc_channel
        cp      #0xff
        ret     z
        ld      (SFX_CHANNEL), a

        ld      hl, #_chan_bit_table
        ld      e, a
        ld      d, #0
        add     hl, de
        ld      c, #0x00
        ld      a, (hl)
        or      #0x80
        ld      b, a
        call    _ymb

        ld      a, (SFX_CHANNEL)
        add     a, #0x10
        ld      c, a
        ld      a, (SFX_START_LO)
        ld      b, a
        call    _ymb
        ld      a, (SFX_CHANNEL)
        add     a, #0x18
        ld      c, a
        ld      a, (SFX_START_HI)
        ld      b, a
        call    _ymb
        ld      a, (SFX_CHANNEL)
        add     a, #0x20
        ld      c, a
        ld      a, (SFX_STOP_LO)
        ld      b, a
        call    _ymb
        ld      a, (SFX_CHANNEL)
        add     a, #0x28
        ld      c, a
        ld      a, (SFX_STOP_HI)
        ld      b, a
        call    _ymb
        ld      a, (SFX_CHANNEL)
        add     a, #0x08
        ld      c, a
        ld      a, (SFX_PANVOL)
        ld      b, a
        call    _ymb

        ld      a, (SFX_CHANNEL)
        ld      hl, #_chan_bit_table
        ld      e, a
        ld      d, #0
        add     hl, de
        ld      c, #0x00
        ld      b, (hl)
        call    _ymb
        ret

_alloc_channel:
        ld      hl, #CHAN_BUSY
        ld      c, #0
_alloc_free_loop:
        ld      a, (hl)
        or      a
        jr      z, _alloc_found
        inc     hl
        inc     c
        ld      a, c
        cp      #6
        jr      nz, _alloc_free_loop

        ld      hl, #CHAN_PRI
        ld      c, #0
        ld      d, #0xff
        ld      e, #0
_alloc_pri_loop:
        ld      a, (hl)
        cp      d
        jr      nc, _alloc_pri_keep
        ld      d, a
        ld      e, c
_alloc_pri_keep:
        inc     hl
        inc     c
        ld      a, c
        cp      #6
        jr      nz, _alloc_pri_loop
        ld      a, (SFX_PRIORITY)
        cp      d
        jr      c, _alloc_none
        ld      c, e
_alloc_found:
        ld      a, c
        push    af
        ld      hl, #CHAN_BUSY
        ld      e, c
        ld      d, #0
        add     hl, de
        ld      (hl), #1
        ld      hl, #CHAN_PRI
        add     hl, de
        ld      a, (SFX_PRIORITY)
        ld      (hl), a
        pop     af
        ret
_alloc_none:
        ld      a, #0xff
        ret

_all_off:
        ld      c, #0x00
        ld      b, #0xbf
        call    _ymb
        ld      c, #0x01
        ld      a, (SFX_MASTER)
        ld      b, a
        call    _ymb
        call    _music_stop
        xor     a
        ld      hl, #CHAN_BUSY
        ld      b, #12
_all_off_clear:
        ld      (hl), a
        inc     hl
        djnz    _all_off_clear
        ret

_music_stop:
        ld      c, #0x10
        ld      b, #0x01
        call    _yma
        ret

_music_start_index:
        cp      #DOOM_AUDIO_MUSIC_COUNT
        ret     nc
        ld      l, a
        ld      h, #0
        add     hl, hl
        ld      d, h
        ld      e, l
        add     hl, hl
        add     hl, hl
        add     hl, de
        ld      de, #doom_music_table
        add     hl, de
        ld      a, (hl)
        or      a
        ret     z
        inc     hl
        push    hl
        call    _music_stop
        ld      c, #0x11
        pop     hl
        push    hl
        ld      de, #6
        add     hl, de
        ld      b, (hl)
        call    _yma
        pop     hl
        push    hl
        ld      c, #0x12
        ld      b, (hl)
        call    _yma
        inc     hl
        ld      c, #0x13
        ld      b, (hl)
        call    _yma
        inc     hl
        ld      c, #0x14
        ld      b, (hl)
        call    _yma
        inc     hl
        ld      c, #0x15
        ld      b, (hl)
        call    _yma
        inc     hl
        ld      c, #0x19
        ld      b, (hl)
        call    _yma
        inc     hl
        ld      c, #0x1a
        ld      b, (hl)
        call    _yma
        pop     hl
        ld      de, #7
        add     hl, de
        ld      c, #0x1b
        ld      a, (MUSIC_MASTER)
        ld      b, a
        call    _yma
        inc     hl
        ld      a, (hl)
        or      a
        jr      z, _music_start_once
        ld      b, #0xb0
        jr      _music_start_go
_music_start_once:
        ld      b, #0xa0
_music_start_go:
        ld      c, #0x10
        call    _yma
        ret

_chan_bit_table:
        .db     0x01, 0x02, 0x04, 0x08, 0x10, 0x20

_sfx_volume_table:
        .db     0x00, 0x20, 0x28, 0x2c, 0x30, 0x32, 0x34, 0x36
        .db     0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f

_music_volume_table:
        .db     0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77
        .db     0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff

_yma:
        ld      a, c
        out     (0x04), a
        ld      a, #0x18
1$:     dec     a
        jr      nz, 1$
        ld      a, b
        out     (0x05), a
        ld      a, #0x18
2$:     dec     a
        jr      nz, 2$
        ret

_ymb:
        ld      a, c
        out     (0x06), a
        ld      a, #0x18
3$:     dec     a
        jr      nz, 3$
        ld      a, b
        out     (0x07), a
        ld      a, #0x18
4$:     dec     a
        jr      nz, 4$
        ret

        ;; Keep generated command constants and tables clear of BIOS vectors
        ;; and driver code. The driver currently occupies less than 0x1000.
        .org    0x1000
        .include "doom_audio_generated.inc"
