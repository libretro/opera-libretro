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
#include "opera_madam.h"
#include "opera_region.h"
#include "opera_sport.h"
#include "opera_vdlp.h"
#include "opera_xbus.h"
#include "opera_xbus_cdrom_plugin.h"
#include "inline.h"

#include <stdint.h>
#include <stdlib.h>

static opera_ext_interface_t io_interface;

extern int flagtime;

int      HIRESMODE = 0;
uint32_t FIXMODE   = 0;
int      CNBFIX    = 0;

int
opera_3do_init(opera_ext_interface_t callback_)
{
  int i;
  uint8_t *dram;
  uint8_t *vram;

  io_interface = callback_;

  CNBFIX = 0;

  opera_clock_init();

  opera_arm_init();

  dram = opera_arm_ram_get();
  vram = opera_arm_vram_get();

  opera_vdlp_init(vram);
  opera_sport_init(vram);
  opera_madam_init(dram);
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

uint32_t
opera_3do_state_size(void)
{
  uint32_t tmp;

  tmp  = 0;
  tmp += 16 * 4;
  tmp += opera_arm_state_size();
  tmp += opera_vdlp_state_size();
  tmp += opera_dsp_state_size();
  tmp += opera_clio_state_size();
  tmp += opera_clock_state_size();
  tmp += opera_sport_state_size();
  tmp += opera_madam_state_size();
  tmp += opera_xbus_state_size();

  return tmp;
}

void
opera_3do_state_save(void *buf_)
{
  uint8_t *data;
  uint32_t *indexes;

  data    = buf_;
  indexes = buf_;

  indexes[0] = 0x97970101;
  indexes[1] = 16 * 4;
  indexes[2] = indexes[1] + opera_arm_state_size();
  indexes[3] = indexes[2] + opera_vdlp_state_size();
  indexes[4] = indexes[3] + opera_dsp_state_size();
  indexes[5] = indexes[4] + opera_clio_state_size();
  indexes[6] = indexes[5] + opera_clock_state_size();
  indexes[7] = indexes[6] + opera_sport_state_size();
  indexes[8] = indexes[7] + opera_madam_state_size();
  indexes[9] = indexes[8] + opera_xbus_state_size();

  opera_arm_state_save(&data[indexes[1]]);
  opera_vdlp_state_save(&data[indexes[2]]);
  opera_dsp_state_save(&data[indexes[3]]);
  opera_clio_state_save(&data[indexes[4]]);
  opera_clock_state_save(&data[indexes[5]]);
  opera_sport_state_save(&data[indexes[6]]);
  opera_madam_state_save(&data[indexes[7]]);
  opera_xbus_state_save(&data[indexes[8]]);
}

int
opera_3do_state_load(const void *buf_)
{
  const uint8_t *data;
  const uint32_t *indexes;

  data    = buf_;
  indexes = buf_;

  if(indexes[0] != 0x97970101)
    return 0;

  opera_arm_state_load(&data[indexes[1]]);
  opera_vdlp_state_load(&data[indexes[2]]);
  opera_dsp_state_load(&data[indexes[3]]);
  opera_clio_state_load(&data[indexes[4]]);
  opera_clock_state_load(&data[indexes[5]]);
  opera_sport_state_load(&data[indexes[6]]);
  opera_madam_state_load(&data[indexes[7]]);
  opera_xbus_state_load(&data[indexes[8]]);

  return 1;
}
