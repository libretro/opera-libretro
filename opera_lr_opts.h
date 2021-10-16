#ifndef OPERA_LR_OPTS_H_INCLUDED
#define OPERA_LR_OPTS_H_INCLUDED

#include "libopera/opera_bios.h"
#include "libopera/opera_vdlp.h"

extern const opera_bios_t *g_OPT_BIOS;
extern const opera_bios_t *g_OPT_FONT;

extern uint32_t g_OPT_VIDEO_WIDTH;
extern uint32_t g_OPT_VIDEO_HEIGHT;
extern uint32_t g_OPT_VIDEO_PITCH_SHIFT;

extern uint32_t g_OPT_VDLP_FLAGS;
extern uint32_t g_OPT_VDLP_PIXEL_FORMAT;

extern uint32_t g_OPT_ACTIVE_DEVICES;

void                 opera_lr_opts_process(void);
bool                 opera_lr_opts_is_enabled(const char *key);
bool                 opera_lr_opts_is_nvram_shared(void);
uint8_t              opera_lr_opts_nvram_version(void);
const opera_bios_t  *opera_lr_opts_get_bios(void);
const opera_bios_t  *opera_lr_opts_get_font(void);
vdlp_pixel_format_e  opera_lr_opts_get_vdlp_pixel_format(void);
uint32_t             opera_lr_opts_get_vdlp_flags(void);
void                 opera_lr_opts_process_bios(void);
void                 opera_lr_opts_process_font(void);
void                 opera_lr_opts_process_cpu_overlock(void);
void                 opera_lr_opts_process_region(void);
void                 opera_lr_opts_process_vdlp_flags(void);
void                 opera_lr_opts_process_high_resolution(void);
void                 opera_lr_opts_process_vdlp_pixel_format(void);
void                 opera_lr_opts_process_active_devices(void);
void                 opera_lr_opts_process_madam_matrix_engine(void);
void                 opera_lr_opts_process_debug(void);
void                 opera_lr_opts_process_dsp_threaded(void);
void                 opera_lr_opts_process_swi_hle(void);
void                 opera_lr_opts_process_hacks(void);

#endif
