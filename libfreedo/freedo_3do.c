/*
  www.freedo.org
  The first and only working 3DO multiplayer emulator.

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

#include "freedo_arm.h"
#include "freedo_clio.h"
#include "freedo_core.h"
#include "freedo_diag_port.h"
#include "freedo_dsp.h"
#include "freedo_frame.h"
#include "freedo_madam.h"
#include "freedo_quarz.h"
#include "freedo_sport.h"
#include "freedo_vdlp.h"
#include "freedo_xbus.h"
#include "freedo_xbus_cdrom_plugin.h"
#include "inline.h"
#include "endianness.h"

#include <stdint.h>
#include <stdlib.h>
#include <boolean.h>

static freedo_ext_interface_t io_interface;

extern int flagtime;

int HIRESMODE = 0;
int FIXMODE   = 0;
int CNBFIX    = 0;

static
void
copy_rom(void       *dst_,
         const void *src_)
{
  int i;
  uint32_t *src;
  uint32_t *dst;

  src = (uint32_t*)src_;
  dst = (uint32_t*)dst_;
  for(i = 0; i < (1024 * 1024 / 4); i++)
    dst[i] = SWAP32_IF_LITTLE_ENDIAN(src[i]);
}

int
freedo_3do_init(freedo_ext_interface_t  callback_,
                const void             *rom_)
{
  int i;
  uint8_t *rom;
  uint8_t *dram;
  uint8_t *vram;

  io_interface = callback_;

  CNBFIX = 0;

  freedo_arm_init();

  dram = freedo_arm_ram_get();
  vram = freedo_arm_vram_get();
  rom  = freedo_arm_rom_get();

  copy_rom(rom,rom_);

  freedo_vdlp_init(vram);
  freedo_sport_init(vram);
  freedo_madam_init(dram);
  freedo_xbus_init(xbus_cdrom_plugin);

  /*
    0x40 for start from 3D0-CD
    0x01/0x02 from PhotoCD ??
    (NO use 0x40/0x02 for BIOS test)
  */
  freedo_clio_init(0x40);
  freedo_dsp_init();
  /* select test, use -1 -- if don't need tests */
  freedo_diag_port_init(-1);
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
  freedo_xbus_device_load(0,NULL);

  freedo_quarz_init();

  return 0;
}

static
void
freedo_3do_internal_frame(vdlp_frame_t *frame_,
                          int           cycles_)
{
  int line;
  int half_frame;

  freedo_quarz_push_cycles(cycles_);
  if(freedo_quarz_queue_dsp())
    io_interface(EXT_PUSH_SAMPLE,(void*)(uintptr_t)freedo_dsp_loop());

  if(freedo_quarz_queue_timer())
    freedo_clio_timer_execute();

  if(freedo_quarz_queue_vdl())
    {
      line       = freedo_quarz_vd_current_line();
      half_frame = freedo_quarz_vd_half_frame();

      freedo_clio_vcnt_update(line,half_frame);
      freedo_vdlp_process_line(line,frame_);

      if(line == freedo_clio_line_v0())
        freedo_clio_fiq_generate(1<<0,0);

      if(line == freedo_clio_line_v1())
        {
          freedo_clio_fiq_generate(1<<1,0);
          io_interface(EXT_SWAPFRAME,frame_);
        }
    }
}

void
freedo_3do_process_frame(vdlp_frame_t *frame_)
{
  uint32_t i;
  uint32_t cnt;
  uint64_t freq;

  if(flagtime)
    flagtime--;

  i    = 0;
  cnt  = 0;
  freq = freedo_quarz_cpu_get_freq();
  do
    {
      if(freedo_madam_fsm_get() == FSM_INPROCESS)
        {
          freedo_madam_cel_handle();
          freedo_madam_fsm_set(FSM_IDLE);
        }

      cnt += freedo_arm_execute();

      if(cnt >= 32)
        {
          freedo_3do_internal_frame(frame_,cnt);
          i += cnt;
          cnt = 0;
        }
    } while(i < (freq / 60));
}

void
freedo_3do_destroy()
{
  freedo_arm_destroy();
  freedo_xbus_destroy();
}

uint32_t
freedo_3do_state_size(void)
{
  uint32_t tmp;

  tmp  = 0;
  tmp += 16 * 4;
  tmp += freedo_arm_state_size();
  tmp += freedo_vdlp_state_size();
  tmp += freedo_dsp_state_size();
  tmp += freedo_clio_state_size();
  tmp += freedo_quarz_state_size();
  tmp += freedo_sport_state_size();
  tmp += freedo_madam_state_size();
  tmp += freedo_xbus_state_size();

  return tmp;
}

void
freedo_3do_state_save(void *buf_)
{
  uint8_t *data;
  uint32_t *indexes;

  data    = buf_;
  indexes = buf_;

  indexes[0] = 0x97970101;
  indexes[1] = 16 * 4;
  indexes[2] = indexes[1] + freedo_arm_state_size();
  indexes[3] = indexes[2] + freedo_vdlp_state_size();
  indexes[4] = indexes[3] + freedo_dsp_state_size();
  indexes[5] = indexes[4] + freedo_clio_state_size();
  indexes[6] = indexes[5] + freedo_quarz_state_size();
  indexes[7] = indexes[6] + freedo_sport_state_size();
  indexes[8] = indexes[7] + freedo_madam_state_size();
  indexes[9] = indexes[8] + freedo_xbus_state_size();

  freedo_arm_state_save(&data[indexes[1]]);
  freedo_vdlp_state_save(&data[indexes[2]]);
  freedo_dsp_state_save(&data[indexes[3]]);
  freedo_clio_state_save(&data[indexes[4]]);
  freedo_quarz_state_save(&data[indexes[5]]);
  freedo_sport_state_save(&data[indexes[6]]);
  freedo_madam_state_save(&data[indexes[7]]);
  freedo_xbus_state_save(&data[indexes[8]]);
}

bool
freedo_3do_state_load(const void *buf_)
{
  const uint8_t *data;
  const uint32_t *indexes;

  data    = buf_;
  indexes = buf_;

  if(indexes[0] != 0x97970101)
    return false;

  freedo_arm_state_load(&data[indexes[1]]);
  freedo_vdlp_state_load(&data[indexes[2]]);
  freedo_dsp_state_load(&data[indexes[3]]);
  freedo_clio_state_load(&data[indexes[4]]);
  freedo_quarz_state_load(&data[indexes[5]]);
  freedo_sport_state_load(&data[indexes[6]]);
  freedo_madam_state_load(&data[indexes[7]]);
  freedo_xbus_state_load(&data[indexes[8]]);

  return true;
}
