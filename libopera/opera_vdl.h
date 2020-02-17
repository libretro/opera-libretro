#ifndef LIBOPERA_VDL_H_INCLUDED
#define LIBOPERA_VDL_H_INCLUDED

/* === VDL DMA control === */
/* Bit fields 0xF8000000 are reserved */
#define VDL_640SC         0x04000000
#define VDL_DISPMOD_MASK  0x03800000
#define VDL_SLIPEN        0x00400000
#define VDL_ENVIDDMA      0x00200000
#define VDL_SLIPCOMMSEL   0x00100000
#define VDL_480RES        0x00080000
#define VDL_RELSEL        0x00040000
#define VDL_PREVSEL       0x00020000
#define VDL_LDCUR         0x00010000
#define VDL_LDPREV        0x00008000
#define VDL_LEN_MASK      0x00007E00
#define VDL_LINE_MASK     0x000001FF

#define VDL_LINE_SHIFT     0
#define VDL_LEN_SHIFT      9

#define VDL_LEN_PREFETCH   4

/* VDL_DISPMOD_MASK definitions */
#define VDL_DISPMOD_320   0x00000000
#define VDL_DISPMOD_384   0x00800000
#define VDL_DISPMOD_512   0x01000000
#define VDL_DISPMOD_640   0x01800000
#define VDL_DISPMOD_1024  0x02000000
#define VDL_DISPMOD_res5  0x02800000
#define VDL_DISPMOD_res6  0x03000000
#define VDL_DISPMOD_res7  0x03800000


/* === VDL Palette data === */
#define VDL_CONTROL     0x80000000
#define VDL_RGBCTL_MASK 0x60000000
#define VDL_PEN_MASK    0x1F000000
#define VDL_R_MASK      0x00FF0000
#define VDL_G_MASK      0x0000FF00
#define VDL_B_MASK      0x000000FF

#define VDL_B_SHIFT       0
#define VDL_G_SHIFT       8
#define VDL_R_SHIFT       16
#define VDL_PEN_SHIFT     24
#define VDL_RGBSEL_SHIFT  29

/* VDL_RGBCTL_MASK definitions */
#define VDL_FULLRGB     0x00000000
#define VDL_REDONLY     0x60000000
#define VDL_GREENONLY   0x40000000
#define VDL_BLUEONLY    0x20000000


/* === VDL display control word === */
#define VDL_DISPCTRL      0xC0000000
#define VDL_BACKGROUND    0x20000000
#define VDL_NULLAMY       0x10000000
#define VDL_PALSEL        0x08000000
#define VDL_S640SEL       0x04000000
#define VDL_CLUTBYPASSEN  0x02000000
#define VDL_SLPDCEL       0x01000000
#define VDL_FORCETRANS    0x00800000
#define VDL_BACKTRANS     0x00400000
#define VDL_WINSWAPHV     0x00200000
#define VDL_WINVSUB_MASK  0x00180000 /* See definitions below */
#define VDL_WINHSUB_MASK  0x00060000 /* See definitions below */
#define VDL_WINBLSB_MASK  0x00018000 /* See definitions below */
#define VDL_WINVINTEN     0x00004000
#define VDL_WINHINTEN     0x00002000
#define VDL_RANDOMEN      0x00001000
#define VDL_WINREPEN      0x00000800
#define VDL_SWAPHV        0x00000400
#define VDL_VSUB_MASK     0x00000300 /* See definitions below */
#define VDL_HSUB_MASK     0x000000C0 /* See definitions below */
#define VDL_BLSB_MASK     0x00000030 /* See definitions below */
#define VDL_VINTEN        0x00000008
#define VDL_HINTEN        0x00000004
#define VDL_COLORSONLY    0x00000002
#define VDL_ONEVINTDIS    0x00000001

/* VDL_BLSB_MASK definitions */
#define VDL_BLSB_NOP    0x00000030
#define VDL_BLSB_BLUE   0x00000020 /* Normal */
#define VDL_BLSB_GREEN  0x00000010
#define VDL_BLSB_ZERO   0x00000000

/* VDL_HSUB_MASK definitions */
#define VDL_HSUB_NOP    0x000000C0
#define VDL_HSUB_FRAME  0x00000080 /* Normal */
#define VDL_HSUB_ONE    0x00000040
#define VDL_HSUB_ZERO   0x00000000

/* VDL_VSUB_MASK definitions */
#define VDL_VSUB_NOP    0x00000300
#define VDL_VSUB_FRAME  0x00000200 /* Normal */
#define VDL_VSUB_ONE    0x00000100
#define VDL_VSUB_ZERO   0x00000000

/* VDL_WBLSB_MASK definitions */
#define VDL_WINBLSB_NOP    0x00018000
#define VDL_WINBLSB_BLUE   0x00010000 /* Normal */
#define VDL_WINBLSB_GREEN  0x00008000
#define VDL_WINBLSB_ZERO   0x00000000

/* VDL_HSUB_MASK definitions */
#define VDL_WINHSUB_NOP    0x00060000
#define VDL_WINHSUB_FRAME  0x00040000 /* Normal */
#define VDL_WINHSUB_ONE    0x00020000
#define VDL_WINHSUB_ZERO   0x00000000

/* VDL_VSUB_MASK definitions */
#define VDL_WINVSUB_NOP    0x00180000
#define VDL_WINVSUB_FRAME  0x00100000 /* Normal */
#define VDL_WINVSUB_ONE    0x00080000
#define VDL_WINVSUB_ZERO   0x00000000


/* === AMY control word === */
#define VDL_AMYCTRL  0x80000000


/* === Special VDL 'NOP' === */
#define VDL_NOP      0xE1000000
#define VDL_NULLVDL  VDL_NOP
#define VDL_AMY_NOP  VDL_AMYCTRL+0
#define VDL_AMYNULL  VDL_AMY_NOP

#endif
