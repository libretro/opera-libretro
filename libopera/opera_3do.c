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
#include "opera_cdrom.h"
#include "opera_clio.h"
#include "opera_clock.h"
#include "opera_core.h"
#include "opera_diag_port.h"
#include "opera_dsp.h"
#include "opera_log.h"
#include "opera_madam.h"
#include "opera_mem.h"
#include "opera_pbus.h"
#include "opera_region.h"
#include "opera_sport.h"
#include "opera_state.h"
#include "opera_vdlp.h"
#include "opera_xbus.h"
#include "opera_xbus_cdrom_plugin.h"
#include "prng16.h"
#include "prng32.h"
#include "inline.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static opera_ext_interface_t io_interface;

extern int flagtime;

#define OPERA_3DO_CD_DIPIR_RESET 0x40
#define OPERA_3DO_CLOCK_STEP     32

static int field = 0;
static int32_t g_FRAME_CYCLE_REMAINDER = 0;

static uint32_t opera_3do_runtime_state_size(void);
static uint32_t opera_3do_runtime_state_save(void *data);
static uint32_t opera_3do_runtime_state_load(void const *data,
                                             uint32_t    size);

static
void
opera_3do_reset_runtime_state(void)
{
  field = 0;
  g_FRAME_CYCLE_REMAINDER = 0;

  opera_clock_reset();
  opera_pbus_reset();
  opera_sport_init();
  opera_xbus_set_legacy_no_device_abort(0);
}

int
opera_3do_init(opera_ext_interface_t callback_)
{
  int i;

  if(opera_mem_cfg() == DRAM_VRAM_UNSET)
    opera_mem_init(DRAM_VRAM_STOCK);
  else
    opera_mem_rom_select(ROM1);

  io_interface = callback_;

  opera_3do_reset_runtime_state();
  opera_arm_init();

  opera_vdlp_init();
  opera_madam_init();
  opera_xbus_init(xbus_cdrom_plugin);

  /*
    0x40 for start from 3D0-CD
    0x01/0x02 from PhotoCD ??
    (NO use 0x40/0x02 for BIOS test)
  */
  opera_clio_init(OPERA_3DO_CD_DIPIR_RESET);
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

void
opera_3do_soft_reset(void)
{
  opera_3do_reset_runtime_state();
  opera_arm_reset();

  opera_vdlp_init();
  opera_madam_init();
  opera_xbus_reset();

  opera_clio_init(OPERA_3DO_CD_DIPIR_RESET);
  opera_dsp_init();
  opera_diag_port_init(-1);
}

static
INLINE
void
opera_3do_internal_frame(uint32_t  cycles_,
                         uint32_t *line_,
                         int       field_)
{
  uint32_t timer;

  opera_clock_push_cycles(cycles_);

  if(opera_clock_dsp_queued())
    io_interface(EXT_DSP_TRIGGER,NULL);

  while(opera_clock_timer_queued(&timer))
    opera_clio_timer_execute(timer);

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

  opera_xbus_tick();
}

void
opera_3do_process_frame(void)
{
  int32_t cnt;
  uint32_t line;
  uint32_t scanlines;

  if(flagtime)
    flagtime--;

  cnt  = g_FRAME_CYCLE_REMAINDER;
  line = 0;
  scanlines = opera_region_scanlines();
  while((cnt >= OPERA_3DO_CLOCK_STEP) && (line < scanlines))
    {
      opera_3do_internal_frame(OPERA_3DO_CLOCK_STEP,&line,field);
      cnt -= OPERA_3DO_CLOCK_STEP;
    }

  while(line < scanlines)
    {
      if(opera_madam_fsm_get() == FSM_INPROCESS)
        {
          opera_madam_cel_handle();
          opera_madam_fsm_set(FSM_IDLE);
        }

      cnt += opera_arm_execute();
      if(opera_cdrom_ode_restart_requested())
        {
          g_FRAME_CYCLE_REMAINDER = cnt;
          return;
        }
      while((cnt >= OPERA_3DO_CLOCK_STEP) && (line < scanlines))
        {
          opera_3do_internal_frame(OPERA_3DO_CLOCK_STEP,&line,field);
          cnt -= OPERA_3DO_CLOCK_STEP;
        }
    }

  g_FRAME_CYCLE_REMAINDER = cnt;
  field = !field;
}

typedef uint32_t (*opera_3do_state_load_cb_t)(const void *data_, uint32_t size_);

static
uint32_t
opera_3do_state_header_size_v3(void)
{
  return opera_state_chunk_size(sizeof(uint32_t) * 2);
}

static
bool
opera_3do_runtime_state_write_payload(opera_state_writer_t *writer_)
{
  return (opera_state_write_i32(writer_,field) &&
          opera_state_write_i32(writer_,g_FRAME_CYCLE_REMAINDER));
}

static
uint32_t
opera_3do_runtime_state_payload_size(void)
{
  opera_state_writer_t writer;

  opera_state_writer_init(&writer,NULL,UINT32_MAX);
  opera_3do_runtime_state_write_payload(&writer);

  return opera_state_writer_ok(&writer) ? opera_state_writer_used(&writer) : 0;
}

static
uint32_t
opera_3do_runtime_state_size(void)
{
  uint32_t payload_size;

  payload_size = opera_3do_runtime_state_payload_size();
  if(payload_size == 0)
    return 0;

  return opera_state_chunk_size(payload_size);
}

static
uint32_t
opera_3do_runtime_state_save(void *data_)
{
  uint32_t payload_size;
  opera_state_writer_t writer;

  payload_size = opera_3do_runtime_state_payload_size();
  if(payload_size == 0)
    return 0;

  opera_state_writer_init(&writer,data_,opera_state_chunk_size(payload_size));
  opera_state_write_chunk_header(&writer,"3DRT",payload_size);
  opera_3do_runtime_state_write_payload(&writer);

  return opera_state_writer_ok(&writer) ? opera_state_writer_used(&writer) : 0;
}

static
uint32_t
opera_3do_runtime_state_load(void const     *data_,
                             uint32_t const  size_)
{
  int32_t field_state;
  int32_t remainder_state;
  opera_state_reader_t reader;
  opera_state_reader_t payload;

  opera_state_reader_init(&reader,data_,size_);
  if(!opera_state_read_chunk(&reader,"3DRT",&payload) ||
     !opera_state_read_i32(&payload,&field_state) ||
     !opera_state_read_i32(&payload,&remainder_state) ||
     !opera_state_reader_finished(&payload))
    return 0;

  if((field_state != 0) && (field_state != 1))
    return 0;

  field = field_state;
  g_FRAME_CYCLE_REMAINDER = remainder_state;

  return opera_state_reader_used(&reader);
}

static
bool
opera_3do_prng_state_write_payload(opera_state_writer_t *writer_)
{
  return (opera_state_write_u32(writer_,prng16_state_get()) &&
          opera_state_write_u32(writer_,prng32_state_get()));
}

static
uint32_t
opera_3do_prng_state_size(void)
{
  return opera_state_chunk_size(sizeof(uint32_t) * 2);
}

static
uint32_t
opera_3do_prng_state_save(void *data_)
{
  opera_state_writer_t writer;

  opera_state_writer_init(&writer,data_,opera_3do_prng_state_size());
  opera_state_write_chunk_header(&writer,"PRNG",sizeof(uint32_t) * 2);
  opera_3do_prng_state_write_payload(&writer);

  return opera_state_writer_ok(&writer) ? opera_state_writer_used(&writer) : 0;
}

static
uint32_t
opera_3do_prng_state_load(void const     *data_,
                          uint32_t const  size_)
{
  uint32_t prng16_state;
  uint32_t prng32_state;
  opera_state_reader_t reader;
  opera_state_reader_t payload;

  opera_state_reader_init(&reader,data_,size_);
  if(!opera_state_read_chunk(&reader,"PRNG",&payload) ||
     !opera_state_read_u32(&payload,&prng16_state) ||
     !opera_state_read_u32(&payload,&prng32_state) ||
     !opera_state_reader_finished(&payload))
    return 0;

  prng16_state_set(prng16_state);
  prng32_state_set(prng32_state);

  return opera_state_reader_used(&reader);
}

static
uint32_t
opera_3do_state_size_v3(void)
{
  uint32_t part;
  uint32_t size;

  size  = 0;

#define OPERA_3DO_ADD_STATE_SIZE(SIZE_) \
  do                                    \
    {                                   \
      part = (SIZE_);                   \
      if(part == 0)                     \
        return 0;                       \
      size += part;                     \
    }                                   \
  while(0)

  OPERA_3DO_ADD_STATE_SIZE(opera_3do_state_header_size_v3());
  OPERA_3DO_ADD_STATE_SIZE(opera_3do_runtime_state_size());
  OPERA_3DO_ADD_STATE_SIZE(opera_3do_prng_state_size());
  OPERA_3DO_ADD_STATE_SIZE(opera_arm_state_size());
  OPERA_3DO_ADD_STATE_SIZE(opera_clio_state_size());
  OPERA_3DO_ADD_STATE_SIZE(opera_dsp_state_size());
  OPERA_3DO_ADD_STATE_SIZE(opera_madam_state_size());
  OPERA_3DO_ADD_STATE_SIZE(opera_mem_state_size());
  OPERA_3DO_ADD_STATE_SIZE(opera_sport_state_size());
  OPERA_3DO_ADD_STATE_SIZE(opera_vdlp_state_size());
  OPERA_3DO_ADD_STATE_SIZE(opera_clock_state_size());
  OPERA_3DO_ADD_STATE_SIZE(opera_xbus_state_size());

#undef OPERA_3DO_ADD_STATE_SIZE

  return size;
}

uint32_t
opera_3do_state_size(void)
{
  return opera_3do_state_size_v3();
}

static
uint32_t
opera_3do_state_save_v3(void         *data_,
                        size_t const  size_)
{
  uint8_t *start = (uint8_t*)data_;
  uint8_t *data  = (uint8_t*)data_;
  opera_state_writer_t writer;
  uint32_t expected_size;

  expected_size = opera_3do_state_size_v3();
  if((expected_size == 0) || (size_ < expected_size))
    return 0;

  opera_state_writer_init(&writer,data,opera_3do_state_header_size_v3());
  opera_state_write_chunk_header(&writer,"3DO",sizeof(uint32_t) * 2);
  opera_state_write_u32(&writer,OPERA_STATE_VERSION_CURRENT);
  opera_state_write_u32(&writer,0);
  if(!opera_state_writer_ok(&writer))
    return 0;
  data += opera_state_writer_used(&writer);
  data += opera_3do_runtime_state_save(data);
  data += opera_3do_prng_state_save(data);
  data += opera_arm_state_save(data);
  data += opera_clio_state_save(data);
  data += opera_dsp_state_save(data);
  data += opera_madam_state_save(data);
  data += opera_mem_state_save(data);
  data += opera_sport_state_save(data);
  data += opera_vdlp_state_save(data);
  data += opera_clock_state_save(data);
  data += opera_xbus_state_save(data);

  return (data - start);
}

uint32_t
opera_3do_state_save(void         *data_,
                     size_t const  size_)
{
  return opera_3do_state_save_v3(data_,size_);
}

static
uint32_t
opera_3do_state_load_step(opera_3do_state_load_cb_t  load_,
                          uint32_t                   expected_size_,
                          uint8_t const            **data_,
                          uint8_t const             *end_)
{
  uint32_t rv;
  uint32_t remaining;

  if(*data_ > end_)
    return 0;

  remaining = (uint32_t)(end_ - *data_);
  if((expected_size_ != 0) && (remaining < expected_size_))
    return 0;

  rv = load_(*data_,remaining);
  if((rv == 0) || (rv > remaining))
    return 0;

  *data_ += rv;

  return rv;
}

static
uint32_t
opera_3do_state_load_v1(const void   *data_,
                        size_t const  size_)
{
  uint8_t const *start = (uint8_t const*)data_;
  uint8_t const *data  = (uint8_t const*)data_;
  uint8_t const *end;
  opera_state_hdr_t hdr = {0};
  uint32_t rv;

  if(size_ > UINT32_MAX)
    return 0;
  end = start + size_;

  rv = opera_state_load_sized(&hdr,
                              "3DO",
                              data,
                              (uint32_t)(end - data),
                              sizeof(hdr));
  if((rv == 0) || (hdr.version != OPERA_STATE_VERSION_V1))
    return 0;
  data += rv;

  opera_3do_reset_runtime_state();

  if(!opera_3do_state_load_step(opera_arm_state_load_v1,
                                opera_arm_state_size_v1(),
                                &data,
                                end))
    return 0;
  if(!opera_3do_state_load_step(opera_clio_state_load_v1,
                                opera_clio_state_size_v1(),
                                &data,
                                end))
    return 0;
  if(!opera_3do_state_load_step(opera_dsp_state_load_v1,
                                opera_dsp_state_size_v1(),
                                &data,
                                end))
    return 0;
  if(!opera_3do_state_load_step(opera_madam_state_load_v1,
                                opera_madam_state_size_v1(),
                                &data,
                                end))
    return 0;
  if(!opera_3do_state_load_step(opera_mem_state_load_v1,
                                opera_mem_state_size_v1(),
                                &data,
                                end))
    return 0;
  if(!opera_3do_state_load_step(opera_sport_state_load_v1,
                                opera_sport_state_size_v1(),
                                &data,
                                end))
    return 0;
  if(!opera_3do_state_load_step(opera_vdlp_state_load_v1,
                                opera_vdlp_state_size_v1(),
                                &data,
                                end))
    return 0;

  if(data > end)
    return 0;
  rv = opera_xbus_state_load_v1(data,(uint32_t)(end - data));
  if((rv == 0) || (rv > (uint32_t)(end - data)))
    return 0;
  data += rv;

  opera_xbus_set_legacy_no_device_abort(1);

  if(data != end)
    return 0;

  return (uint32_t)(data - start);
}

static
uint32_t
opera_3do_state_load_chunked(const void     *data_,
                             size_t const    size_,
                             uint32_t const  expected_version_,
                             bool const      load_prng_)
{
  uint8_t const *start = (uint8_t const*)data_;
  uint8_t const *data  = (uint8_t const*)data_;
  uint8_t const *end;
  opera_state_reader_t reader;
  opera_state_reader_t payload;
  uint32_t version;
  uint32_t flags;
  uint32_t rv;

  if(size_ > UINT32_MAX)
    return 0;
  end = start + size_;

  opera_state_reader_init(&reader,data,(uint32_t)(end - data));
  if(!opera_state_read_chunk(&reader,"3DO",&payload) ||
     !opera_state_read_u32(&payload,&version) ||
     !opera_state_read_u32(&payload,&flags) ||
     (version != expected_version_) ||
     (flags != 0) ||
     !opera_state_reader_finished(&payload))
    return 0;
  data += opera_state_reader_used(&reader);

  if(!opera_3do_state_load_step(opera_3do_runtime_state_load,
                                0,
                                &data,
                                end))
    return 0;
  if(load_prng_ &&
     !opera_3do_state_load_step(opera_3do_prng_state_load,
                                0,
                                &data,
                                end))
    return 0;
  if(!opera_3do_state_load_step(opera_arm_state_load,
                                0,
                                &data,
                                end))
    return 0;
  if(!opera_3do_state_load_step(opera_clio_state_load,
                                0,
                                &data,
                                end))
    return 0;
  if(!opera_3do_state_load_step(opera_dsp_state_load,
                                0,
                                &data,
                                end))
    return 0;
  if(!opera_3do_state_load_step(opera_madam_state_load,
                                0,
                                &data,
                                end))
    return 0;
  if(!opera_3do_state_load_step(opera_mem_state_load,
                                0,
                                &data,
                                end))
    return 0;
  if(!opera_3do_state_load_step(opera_sport_state_load,
                                0,
                                &data,
                                end))
    return 0;
  if(!opera_3do_state_load_step(opera_vdlp_state_load,
                                0,
                                &data,
                                end))
    return 0;
  if(!opera_3do_state_load_step(opera_clock_state_load,
                                0,
                                &data,
                                end))
    return 0;

  if(data > end)
    return 0;
  rv = opera_xbus_state_load(data,(uint32_t)(end - data));
  if((rv == 0) || (rv > (uint32_t)(end - data)))
    return 0;
  data += rv;

  if(data != end)
    return 0;

  opera_pbus_reset();
  opera_xbus_set_legacy_no_device_abort(0);

  return (uint32_t)(data - start);
}

static
uint32_t
opera_3do_state_load_v2(const void   *data_,
                        size_t const  size_)
{
  return opera_3do_state_load_chunked(data_,
                                      size_,
                                      OPERA_STATE_VERSION_V2,
                                      false);
}

static
uint32_t
opera_3do_state_load_v3(const void   *data_,
                        size_t const  size_)
{
  return opera_3do_state_load_chunked(data_,
                                      size_,
                                      OPERA_STATE_VERSION_V3,
                                      true);
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
    case OPERA_STATE_VERSION_V1:
      return opera_3do_state_load_v1(data_,size_);
    case OPERA_STATE_VERSION_V2:
      return opera_3do_state_load_v2(data_,size_);
    case OPERA_STATE_VERSION_V3:
      return opera_3do_state_load_v3(data_,size_);
    default:
      opera_log_printf(OPERA_LOG_ERROR,
                       "[Opera]: unable to load state - unknown state version %x\n",
                       version);
      return 0;
    }
}
