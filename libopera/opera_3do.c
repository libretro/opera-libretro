/*
  www.freedo.org
  The first working 3DO multiplayer emulator.

  The FreeDO licensed under modified GNU LGPL, with following notes:

  *   The owners and original authors of the FreeDO have full right to
  *   develop closed source derivative work.

  *   Any non-commercial uses of the FreeDO sources or any knowledge
  *   obtained by studying or reverse engineering of the sources, or
  *   any other material published by FreeDO have to be accompanied
  *   with full credits.

  *   Any commercial uses of FreeDO sources or any knowledge obtained
  *   by studying or reverse engineering of the sources, or any other
  *   material published by FreeDO is strictly forbidden without
  *   owners approval.

  The above notes are taking precedence over GNU LGPL in conflicting
  situations.

  Project authors:
  *  Alexander Troosh
  *  Maxim Grishin
  *  Allen Wright
  *  John Sammons
  *  Felix Lazarev
  */

#include "opera_arm.h"
#include "opera_clio.h"
#include "opera_clock.h"
#include "opera_core.h"
#include "opera_diag_port.h"
#include "opera_dsp.h"
#include "opera_log.h"
#include "opera_madam.h"
#include "opera_mem.h"
#include "opera_region.h"
#include "opera_sport.h"
#include "opera_state.h"
#include "opera_vdlp.h"
#include "opera_xbus.h"
#include "opera_xbus_cdrom_plugin.h"
#include "inline.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static opera_ext_interface_t io_interface;

extern int flagtime;

uint32_t FIXMODE = 0;
int      CNBFIX  = 0;

int
opera_3do_init(opera_ext_interface_t callback_)
{
  int i;

  if(opera_mem_cfg() == DRAM_VRAM_UNSET)
    opera_mem_init(DRAM_VRAM_STOCK);

  io_interface = callback_;

  CNBFIX = 0;

  opera_arm_init();

  opera_vdlp_init();
  opera_madam_init();
  opera_xbus_init(xbus_cdrom_plugin);

  /*
    0x40 for start from 3D0-CD
    0x01/0x02 from PhotoCD ??
    (NO use 0x40/0x02 for BIOS test)
  */
  opera_clio_init(0x40);
  opera_dsp_init();
  /* select test, use -1 -- if don't need tests */
  opera_diag_port_init(-1);
  /*
    00	DIAGNOSTICS TEST (1F,24,25,32,50,51,60,61,62,68,71,75,80,81,90)
    01	AUTO-DIAG TEST   (1F,24,25,32,50,51,60,61,62,68,80,81,90)
    12	DRAM1 DATA TEST
    1A	DRAM2 DATA TEST
    1E	EARLY RAM TEST
    1F	RAM DATA TEST
    22	VRAM1 DATA TEST
    24	VRAM1 FLASH TEST
    25	VRAM1 SPORT TEST
    32	SRAM DATA TEST
    50	MADAM TEST
    51	CLIO TEST
    60	CD-ROM POLL TEST
    61	CD-ROM PATH TEST
    62	CD-ROM READ TEST	???
    63	CD-ROM AutoAdjustValue TEST
    67	CD-ROM#2 AutoAdjustValue TEST
    68  DEV#15 POLL TEST
    71	JOYPAD1 PRESS TEST
    75	JOYPAD1 AUDIO TEST
    80	SIN WAVE TEST
    81	MUTING TEST
    90	COLORBAR
    F0	CHECK TESTTOOL  ???
    F1	REVISION TEST
    FF	TEST END (halt)
  */
  opera_xbus_device_load(0,NULL);

  return 0;
}

void
opera_3do_destroy()
{
  opera_arm_destroy();
  opera_xbus_destroy();
}

static
INLINE
void
opera_3do_internal_frame(uint32_t  cycles_,
                         uint32_t *line_,
                         int       field_)
{
  opera_clock_push_cycles(cycles_);
  if(opera_clock_dsp_queued())
    io_interface(EXT_DSP_TRIGGER,NULL);

  if(opera_clock_timer_queued())
    opera_clio_timer_execute();

  if(opera_clock_vdl_queued())
    {
      opera_clio_vcnt_update(*line_,field_);
      opera_vdlp_process_line(*line_);

      if(*line_ == opera_clio_line_vint0())
        opera_clio_fiq_generate(1<<0,0);

      if(*line_ == opera_clio_line_vint1())
        opera_clio_fiq_generate(1<<1,0);

      (*line_)++;
    }
}

void
opera_3do_process_frame(void)
{
  int32_t cnt;
  uint32_t line;
  uint32_t scanlines;
  static int field = 0;

  if(flagtime)
    flagtime--;

  cnt  = 0;
  line = 0;
  scanlines = opera_region_scanlines();
  do
    {
      if(opera_madam_fsm_get() == FSM_INPROCESS)
        {
          opera_madam_cel_handle();
          opera_madam_fsm_set(FSM_IDLE);
        }

      cnt += opera_arm_execute();
      if(cnt >= 32)
        {
          opera_3do_internal_frame(cnt,&line,field);
          cnt -= 32;
        }
    } while(line < scanlines);

  field = !field;
}

static
uint32_t
opera_3do_state_size_v1(void)
{
  uint32_t size;

  size  = 0;
  size += opera_state_save_size(sizeof(opera_state_hdr_t));
  size += opera_arm_state_size();
  size += opera_clio_state_size();
  size += opera_dsp_state_size();
  size += opera_madam_state_size();
  size += opera_mem_state_size();
  size += opera_sport_state_size();
  size += opera_vdlp_state_size();
  size += opera_xbus_state_size();

  return size;
}

uint32_t
opera_3do_state_size(void)
{
  return opera_3do_state_size_v1();
}

static
uint32_t
opera_3do_state_save_v1(void         *data_,
                        size_t const  size_)
{
  uint8_t *start = (uint8_t*)data_;
  uint8_t *data  = (uint8_t*)data_;
  opera_state_hdr_t hdr = {0};

  hdr.version = 0x01;

  data += opera_state_save(data,"3DO",&hdr,sizeof(hdr));
  data += opera_arm_state_save(data);
  data += opera_clio_state_save(data);
  data += opera_dsp_state_save(data);
  data += opera_madam_state_save(data);
  data += opera_mem_state_save(data);
  data += opera_sport_state_save(data);
  data += opera_vdlp_state_save(data);
  data += opera_xbus_state_save(data);

  return (data - start);
}

uint32_t
opera_3do_state_save(void         *data_,
                     size_t const  size_)
{
  return opera_3do_state_save_v1(data_,size_);
}

static
uint32_t
opera_3do_state_load_v1(const void   *data_,
                        size_t const  size_)
{
  uint8_t const *start = (uint8_t const*)data_;
  uint8_t const *data  = (uint8_t const*)data_;
  opera_state_hdr_t hdr = {0};

  if(size_ != opera_3do_state_size_v1())
    return 0;

  data += opera_state_load(&hdr,"3DO",data,sizeof(hdr));
  data += opera_arm_state_load(data);
  data += opera_clio_state_load(data);
  data += opera_dsp_state_load(data);
  data += opera_madam_state_load(data);
  data += opera_mem_state_load(data);
  data += opera_sport_state_load(data);
  data += opera_vdlp_state_load(data);
  data += opera_xbus_state_load(data);

  return (data - start);
}

uint32_t
opera_3do_state_load(void const *data_,
                     size_t      size_)
{
  uint32_t version;

  version = opera_state_get_version(data_,size_);

  opera_log_printf(OPERA_LOG_DEBUG,"[Opera]: loading state... version %x\n",version);

  switch(version)
    {
    case 0x01:
      return opera_3do_state_load_v1(data_,size_);
    default:
      opera_log_printf(OPERA_LOG_ERROR,
                       "[Opera]: unable to load state - unknown state version %x\n",
                       version);
      return 0;
    }
}
