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

#include "boolean.h"
#include "inline.h"
#include "opera_clock.h"
#include "opera_cdrom.h"
#include "opera_log.h"
#include "opera_state.h"

#include "file/file_path.h"
#include "lists/dir_list.h"
#include "lists/string_list.h"
#include "retro_miscellaneous.h"
#include "streams/file_stream.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MSF_BIAS_IN_SECONDS 2
#define MSF_BIAS_IN_FRAMES  150
#define FRAMES_PER_SECOND   75
#define SECONDS_PER_MINUTE  60
#define CDROM_DA_DEFAULT_READ_AHEAD_SECTORS 48
#define CDROM_BLOCK_FLAG_SUBCODE 0x40
#define CDROM_BLOCK_FLAG_ERROR   0x80
#define CDROM_SUBCODE_SIZE       96
#define CDROM_POLL_INTEN_MASK (POLSTMASK | POLDTMASK | POLMAMASK)
/*
  Portfolio requests ABORT/FLUSH status index 31 but treats 32+ bytes
  as an error:
  https://github.com/trapexit/portfolio_os/blob/master/src/app/oper/CDROMDriver.c#L2071
  https://github.com/trapexit/portfolio_os/blob/master/src/filesystem/includes/cdrom.h#L356
*/
#define CDROM_ABORT_FLUSH_STATUS_LEN 31

typedef enum cdrom_status_layout_e
  {
    CDROM_STATUS_LAYOUT_NATIVE_TAGGED
  } cdrom_status_layout_t;

typedef enum cdrom_persona_feature_e
  {
    CDROM_PERSONA_FEATURE_DISABLED = 0,
    CDROM_PERSONA_FEATURE_ENABLED  = 1
  } cdrom_persona_feature_t;

typedef struct cdrom_persona_s
{
  const char              *name;
  uint16_t                 manufacturer_id;
  uint16_t                 manufacturer_device;
  uint16_t                 manufacturer_revision;
  uint16_t                 manufacturer_flags;
  uint16_t                 driver_tag_table_words;
  cdrom_status_layout_t    status_layout;
  cdrom_persona_feature_t  sends_tag;
  cdrom_persona_feature_t  supports_chunks;
  cdrom_persona_feature_t  supports_subcode;
  cdrom_persona_feature_t  slow_dma;
  cdrom_persona_feature_t  emulate_media_access;
  cdrom_persona_feature_t  old_status;
} cdrom_persona_t;

#define CDROM_PORTFOLIO_MFG_MEI       0x0010
#define CDROM_PORTFOLIO_DEVICE_CDROM  0x0001
#define CDROM_READ_ID_FLAG_DEVHAS_DRIVER 0x0500
#define CDROM_PERSONA_NAME_NATIVE_MEI "native_mei"
#define CDROM_READ_ID_MANUFACTURER_REVISION_NONE 0x0000
#define CDROM_READ_ID_MANUFACTURER_FLAGS_NONE 0x0000
#define CDROM_READ_ID_DRIVER_TAG_TABLE_WORDS_NONE 0x0000

typedef enum cdrom_read_id_payload_offset_e
  {
    CDROM_READ_ID_PAYLOAD_MANUFACTURER_ID        = 0,
    CDROM_READ_ID_PAYLOAD_MANUFACTURER_DEVICE    = 2,
    CDROM_READ_ID_PAYLOAD_MANUFACTURER_REVISION  = 4,
    CDROM_READ_ID_PAYLOAD_MANUFACTURER_FLAGS     = 6,
    CDROM_READ_ID_PAYLOAD_DRIVER_TAG_TABLE_WORDS = 8
  } cdrom_read_id_payload_offset_t;

/*
  Why this persona is explicit:

  Portfolio selects CD-ROM driver behavior from the XBus READ_ID identity.
  MEI manufacturer 0x0010 / CD-ROM device 0x0001 gets tagged status,
  chunk support, and subcode support. The CR-563 identity gets slow-DMA
  and emulated-media-access behavior instead. During XBus READ_ID parsing,
  Portfolio also sets XBUS_OLDSTAT when the first status byte is not the
  READ_ID tag, and later synthesizes tag bytes for that old-status path.

  Keeping identity, tag layout, chunk/subcode flags, DMA behavior, and
  old-status behavior together prevents future edits from advertising one
  Portfolio persona while returning another packet layout. Future work
  may include adding other personas for completeness.

  Original Portfolio references:
  https://github.com/trapexit/portfolio_os/blob/master/src/app/oper/CDROMDriver.c#L678-L696
  https://github.com/trapexit/portfolio_os/blob/master/src/app/oper/xbusdevice.c#L1164-L1175
  https://github.com/trapexit/portfolio_os/blob/master/src/filesystem/includes/cdrom.h#L411-L416
*/
static const cdrom_persona_t g_CDROM_PERSONA =
  {
    CDROM_PERSONA_NAME_NATIVE_MEI,             /* name */
    CDROM_PORTFOLIO_MFG_MEI,                   /* manufacturer_id */
    CDROM_PORTFOLIO_DEVICE_CDROM,              /* manufacturer_device */
    CDROM_READ_ID_MANUFACTURER_REVISION_NONE,  /* manufacturer_revision */
    CDROM_READ_ID_MANUFACTURER_FLAGS_NONE,     /* manufacturer_flags */
    CDROM_READ_ID_DRIVER_TAG_TABLE_WORDS_NONE, /* driver_tag_table_words */
    CDROM_STATUS_LAYOUT_NATIVE_TAGGED,         /* status_layout */
    CDROM_PERSONA_FEATURE_ENABLED,             /* sends_tag */
    CDROM_PERSONA_FEATURE_ENABLED,             /* supports chunks */
    CDROM_PERSONA_FEATURE_ENABLED,             /* supports subcode */
    CDROM_PERSONA_FEATURE_DISABLED,            /* slow_dma */
    CDROM_PERSONA_FEATURE_DISABLED,            /* emulate_media_access */
    CDROM_PERSONA_FEATURE_DISABLED             /* old_status */
  };

static cdrom_callbacks_t g_CDROM_CALLBACKS = {0};

/*
  Boot device identifiers from FCare's SataTo3DO interface
  explanation: https://github.com/FCare/SataTo3DO
*/
#define ODE_BOOT_MICROSD     0x01
#define ODE_BOOT_USBMSC      0x02
#define ODE_BOOT_FLASH       0x04
#define ODE_BOOT_SATA        0x08
#define ODE_TOC_FLAG_FILE    0x00000001
#define ODE_TOC_FLAG_DIR     0x00000002
#define ODE_TOC_FLAG_INVALID 0xFFFFFFFF
#define ODE_TOC_NAME_LIMIT   128
#define ODE_TOC_DEVICE_MASK  0xFF000000
#define ODE_TOC_ENTRY_MASK   0x00FFFFFF
#define ODE_TOC_SUPPORTED_EXTS "iso|ISO|cue|CUE"
#define ODE_PLAYLIST_MAX     16

enum ODE_Commands
  {
    ODE_CMD_EXTENDED_ID       = 0x93,
    ODE_CMD_CHANGE_TOC        = 0xC0,
    ODE_CMD_READ_TOC_WINDOW   = 0xC1,
    ODE_CMD_READ_DESCRIPTION  = 0xC2,
    ODE_CMD_CLEAR_PLAYLIST    = 0xC3,
    ODE_CMD_ADD_PLAYLIST      = 0xC4,
    ODE_CMD_LAUNCH_PLAYLIST   = 0xC5,
    ODE_CMD_READ_TOC_LIST     = 0xD1,
    ODE_CMD_CREATE_FILE       = 0xE0,
    ODE_CMD_OPEN_FILE         = 0xE1,
    ODE_CMD_SEEK_FILE         = 0xE2,
    ODE_CMD_READ_FILE         = 0xE3,
    ODE_CMD_WRITE_FILE        = 0xE4,
    ODE_CMD_CLOSE_FILE        = 0xE5,
    ODE_CMD_WRITE_BUFFER_WORD = 0xE6,
    ODE_CMD_READ_FILE_BUFFER  = 0xE7,
    ODE_CMD_FAST_BUFFER_SEND  = 0xE8,
    ODE_CMD_START_UPDATE      = 0xF0
  };

/*
  FCare/Fixel ODE extensions live in the CDROM device because the ODE
  replaces the CDROM-facing XBus device.
*/
typedef struct cdrom_ode_runtime_state_s
{
  char     root[PATH_MAX_LENGTH];
  char     current[PATH_MAX_LENGTH];
  char     playlist[ODE_PLAYLIST_MAX][PATH_MAX_LENGTH];
  uint32_t playlist_count;
  char     pending_launch_path[PATH_MAX_LENGTH];
  uint8_t  toc[REQSIZE];
  uint8_t  file_buffer[REQSIZE];
  uint8_t  file_buffer_write_data[REQSIZE];
  uint32_t file_buffer_write_idx;
  bool     file_buffer_write_overflow;
  RFILE   *file;
  opera_cdrom_ode_launch_cb_t launch;
  bool     restart_armed;
  bool     restart_requested;
  bool     media_access_after_reset;
  char     file_path[PATH_MAX_LENGTH];
  int      file_mode;
  bool     savestate_blocked;
} cdrom_ode_runtime_state_t;

typedef struct cdrom_data_runtime_state_s
{
  uint64_t next_ready_cycle;
  uint32_t sectors_queued;
  uint32_t sectors_drained;
  uint32_t empty_reads;
  uint32_t read_expected_lba;
  bool     read_have_expected_lba;
  bool     audio_burst_active;
  uint32_t audio_burst_starts;
  uint32_t audio_min_ahead;
  uint32_t audio_max_ahead;
  uint32_t active_lba;
  uint32_t active_bytes;
  uint32_t audio_base_lba;
  uint64_t audio_base_cycle;
  bool     audio_clock_active;
  bool     read_status_pending;
  uint32_t wait_ready;
  uint64_t wait_cycles;
  uint64_t max_wait_cycles;
} cdrom_data_runtime_state_t;

typedef struct cdrom_runtime_state_s
{
  cdrom_ode_runtime_state_t  ode;
  cdrom_data_runtime_state_t data;
  uint32_t                   configured_speed;
} cdrom_runtime_state_t;

static cdrom_runtime_state_t g_CDROM_STATE = {0};

typedef struct cdrom_device_state_v2_s
{
  uint8_t                  poll;
  uint8_t                  xbus_status;
  uint8_t                  status_len;
  uint8_t                  status[256];
  uint32_t                 read_sector_size;
  uint32_t                 data_len;
  uint32_t                 data_idx;
  uint8_t                  data[CDROM_MAX_SECTOR_SIZE];
  uint32_t                 blocks_requested;
  uint8_t                  cmd[7];
  uint8_t                  cmd_idx;
  int8_t                   STATCYC;
  uint32_t                 MEI_status;
  uint32_t                 current_sector;
  cdrom_mode_parameters_t  mode;
  disc_data_t              disc;
} cdrom_device_state_v2_t;

typedef struct cdrom_device_state_v1_legacy_s
{
  uint8_t           poll;
  uint8_t           xbus_status;
  uint8_t           status_len;
  uint8_t           status[256];
  uint32_t          data_len;
  uint32_t          data_idx;
  uint8_t           data[CDROM_MAX_SECTOR_SIZE];
  uint32_t          blocks_requested;
  uint8_t           cmd[7];
  uint8_t           cmd_idx;
  int8_t            STATCYC;
  uint32_t          MEI_status;
  uint32_t          current_sector;
  uint8_t           disc[360];
  uint8_t           callbacks[48];
} cdrom_device_state_v1_legacy_t;

typedef char cdrom_device_state_v1_legacy_size_check[
  (sizeof(cdrom_device_state_v1_legacy_t) == 3152) ? 1 : -1];

typedef struct cdrom_data_state_v2_s
{
  uint64_t next_ready_cycles_remaining;
  uint32_t sectors_queued;
  uint32_t sectors_drained;
  uint32_t empty_reads;
  uint32_t read_expected_lba;
  uint8_t  read_have_expected_lba;
  uint8_t  audio_burst_active;
  uint32_t audio_burst_starts;
  uint32_t audio_min_ahead;
  uint32_t audio_max_ahead;
  uint32_t active_lba;
  uint32_t active_bytes;
  uint32_t audio_base_lba;
  uint64_t audio_base_cycle_age;
  uint8_t  audio_clock_active;
  uint8_t  read_status_pending;
  uint32_t wait_ready;
  uint64_t wait_cycles;
  uint64_t max_wait_cycles;
} cdrom_data_state_v2_t;

#define CDROM_STATE_V2_MAGIC {'C','D','R','M'}

static void LBA2MSF(const uint32_t lba_, msf_t *msf_);
static uint32_t MSF2LBA(const msf_t *msf_);
static void cdrom_data_clear_transfer(cdrom_device_t *cd_);
static void ode_file_buffer_write_reset(void);
static void ode_complete_restart_if_armed(cdrom_device_t *cd_);
static bool cdrom_persona_is_portfolio_native_mei(void);

static
uint32_t
cdrom_audio_read_ahead_sectors(void)
{
  return CDROM_DA_DEFAULT_READ_AHEAD_SECTORS;
}

static
uint32_t
cdrom_data_wire_size(const cdrom_device_t *cd_)
{
  return cd_->mode.block.user_block_size;
}

static
uint32_t
cdrom_toc_capacity(const disc_data_t *disc_)
{
  return (sizeof(disc_->disc_toc) / sizeof(disc_->disc_toc[0]));
}

static
bool
cdrom_toc_track_in_range(const disc_data_t *disc_,
                         uint32_t           track_)
{
  return ((track_ > 0) && (track_ < cdrom_toc_capacity(disc_)));
}

static
bool
cdrom_msf_valid(const msf_t *msf_)
{
  return ((msf_->seconds < SECONDS_PER_MINUTE) &&
          (msf_->frames  < FRAMES_PER_SECOND));
}

static
bool
cdrom_toc_entry_present(const toc_entry_t *toc_)
{
  return ((toc_->track_number != 0) ||
          (toc_->CDCTL != 0) ||
          (toc_->minutes != 0) ||
          (toc_->seconds != 0) ||
          (toc_->frames != 0));
}

static
bool
cdrom_toc_entry_valid(const disc_data_t  *disc_,
                      uint32_t            track_,
                      const toc_entry_t  *toc_)
{
  msf_t msf;

  if(!cdrom_toc_track_in_range(disc_,track_))
    return false;
  if(!cdrom_toc_entry_present(toc_))
    return false;

  msf.minutes = toc_->minutes;
  msf.seconds = toc_->seconds;
  msf.frames  = toc_->frames;

  return cdrom_msf_valid(&msf);
}

static
uint8_t
cdrom_toc_position_control(uint8_t control_)
{
  return ((control_ &
           (CD_CTL_PREEMPHASIS |
            CD_CTL_COPY_PERMITTED |
            CD_CTL_DATA_TRACK |
            CD_CTL_FOUR_CHANNEL)) |
          CD_CTL_Q_POSITION);
}

static
int
cdrom_track_for_lba(const cdrom_device_t *cd_,
                    uint32_t              lba_,
                    uint32_t             *track_start_lba_)
{
  int i;
  int track_idx;
  uint32_t track_start_lba;
  uint32_t selected_start_lba;

  if(track_start_lba_ != NULL)
    *track_start_lba_ = 0;

  if((!cdrom_toc_track_in_range(&cd_->disc,cd_->disc.track_first)) ||
     (!cdrom_toc_track_in_range(&cd_->disc,cd_->disc.track_last)) ||
     (cd_->disc.track_last < cd_->disc.track_first))
    return -1;

  track_idx = -1;
  selected_start_lba = 0;
  for(i = cd_->disc.track_first; i <= cd_->disc.track_last; i++)
    {
      msf_t track_msf;

      if(!cdrom_toc_entry_valid(&cd_->disc,i,&cd_->disc.disc_toc[i]))
        continue;

      track_msf.minutes = cd_->disc.disc_toc[i].minutes;
      track_msf.seconds = cd_->disc.disc_toc[i].seconds;
      track_msf.frames  = cd_->disc.disc_toc[i].frames;
      track_start_lba   = MSF2LBA(&track_msf);
      if(track_idx < 0)
        {
          track_idx = i;
          selected_start_lba = track_start_lba;
        }
      if(track_start_lba > lba_)
        break;

      track_idx = i;
      selected_start_lba = track_start_lba;
    }

  if(track_idx < 0)
    return -1;

  if(track_start_lba_ != NULL)
    *track_start_lba_ = selected_start_lba;

  return track_idx;
}

static
uint64_t
cdrom_requested_speed(const cdrom_device_t *cd_)
{
  return (cd_->xbus_status & CDROM_STATUS_DOUBLE_SPEED) ? 2 : 1;
}

static
uint32_t
cdrom_configured_speed(void)
{
  return g_CDROM_STATE.configured_speed;
}

static
uint32_t
cdrom_effective_speed(const cdrom_device_t *cd_)
{
  const uint32_t speed = cdrom_configured_speed();

  if(speed == 0)
    return 0;

  if(speed == 1)
    return 1;

  if(cdrom_requested_speed(cd_) == 1)
    return 1;

  return speed;
}

static
uint32_t
cdrom_data_sector_overhead_compensation_x10_usec(const cdrom_device_t *cd_)
{
  /* Keep measured 2048-byte data throughput aligned with the speed option. */
  switch(cdrom_effective_speed(cd_))
    {
    case 1:
      return 1660;
    case 2:
      return 1670;
    case 4:
      return 1690;
    case 8:
      return 1695;
    case 16:
      return 1704;
    default:
      return 1695;
    }
}

static
int32_t
cdrom_pitch_basis_points(const cdrom_device_t *cd_)
{
  if((cd_->mode.drive_speed.vpitch_msb == MEI_VPITCH_FAST_MSB) &&
     (cd_->mode.drive_speed.vpitch_lsb == MEI_VPITCH_FAST_LSB))
    return 100;

  if((cd_->mode.drive_speed.vpitch_msb == MEI_VPITCH_SLOW_MSB) &&
     (cd_->mode.drive_speed.vpitch_lsb == MEI_VPITCH_SLOW_LSB))
    return -100;

  return 0;
}

static
uint64_t
cdrom_apply_pitch_to_cycles(const cdrom_device_t *cd_,
                            uint64_t              cycles_)
{
  const int32_t pitch_basis_points = cdrom_pitch_basis_points(cd_);
  const int32_t scale = 10000 + pitch_basis_points;

  if((pitch_basis_points == 0) || (scale <= 0))
    return cycles_;

  return ((cycles_ * 10000) / (uint32_t)scale);
}

static
uint32_t
cdrom_mode_block_base_size(const cdrom_mode_parameters_t *mode_)
{
  uint32_t size;

  size = ((uint32_t)mode_->block.length_msb << 8) | mode_->block.length_lsb;
  if(size == 0)
    size = REQSIZE;
  if(size > CDROM_MAX_SECTOR_SIZE)
    size = CDROM_MAX_SECTOR_SIZE;

  return size;
}

static
bool
cdrom_mode_density_supported(uint8_t density_code_)
{
  switch(density_code_)
    {
    case MEI_CDROM_DEFAULT_DENSITY:
    case MEI_CDROM_DATA:
    case MEI_CDROM_MODE2_XA:
    case MEI_CDROM_DIGITAL_AUDIO:
      return true;
    default:
      return false;
    }
}

static
uint32_t
cdrom_mode_block_transfer_size(const cdrom_mode_parameters_t *mode_)
{
  uint32_t size;

  size = cdrom_mode_block_base_size(mode_);
  if(mode_->block.flags & CDROM_BLOCK_FLAG_SUBCODE)
    size += CDROM_SUBCODE_SIZE;
  if(mode_->block.flags & CDROM_BLOCK_FLAG_ERROR)
    size += 1;
  if(size > CDROM_MAX_SECTOR_SIZE)
    size = CDROM_MAX_SECTOR_SIZE;

  return size;
}

static
bool
cdrom_mode_block_supported(uint8_t density_code_,
                           uint8_t length_msb_,
                           uint8_t length_lsb_,
                           uint8_t flags_)
{
  cdrom_mode_parameters_t mode;
  uint32_t raw_size;
  uint32_t base_size;
  uint32_t transfer_size;

  if(!cdrom_mode_density_supported(density_code_))
    return false;

  if(flags_ & ~(CDROM_BLOCK_FLAG_SUBCODE | CDROM_BLOCK_FLAG_ERROR))
    return false;

  raw_size = (((uint32_t)length_msb_) << 8) | length_lsb_;
  if(raw_size == 0)
    raw_size = REQSIZE;
  if(raw_size > CDROM_MAX_SECTOR_SIZE)
    return false;

  memset(&mode,0,sizeof(mode));
  mode.block.density_code = density_code_;
  mode.block.length_msb   = length_msb_;
  mode.block.length_lsb   = length_lsb_;
  mode.block.flags        = flags_;

  base_size = cdrom_mode_block_base_size(&mode);
  transfer_size = base_size;
  if(flags_ & CDROM_BLOCK_FLAG_SUBCODE)
    transfer_size += CDROM_SUBCODE_SIZE;
  if(flags_ & CDROM_BLOCK_FLAG_ERROR)
    transfer_size += 1;

  return ((base_size > 0) && (base_size <= CDROM_MAX_SECTOR_SIZE) &&
          (transfer_size <= CDROM_MAX_SECTOR_SIZE));
}

static
void
cdrom_mode_apply_block(cdrom_device_t *cd_)
{
  cd_->read_sector_size = cdrom_mode_block_base_size(&cd_->mode);
  cd_->mode.block.user_block_size = cdrom_mode_block_transfer_size(&cd_->mode);
}

static
void
cdrom_mode_apply_drive_speed(cdrom_device_t *cd_)
{
  if(cd_->mode.drive_speed.speed == MEI_CDROM_DOUBLE_SPEED)
    cd_->xbus_status |= CDROM_STATUS_DOUBLE_SPEED;
  else
    cd_->xbus_status &= ~CDROM_STATUS_DOUBLE_SPEED;
}

static
void
cdrom_mode_reset(cdrom_device_t *cd_)
{
  cd_->mode.block.density_code = MEI_CDROM_DEFAULT_DENSITY;
  cd_->mode.block.length_msb = REQSIZE >> 8;
  cd_->mode.block.length_lsb = REQSIZE & 0xFF;
  cd_->mode.block.flags = 0;

  cd_->mode.error_recovery.type = MEI_CDROM_DEFAULT_RECOVERY;
  cd_->mode.error_recovery.retry_count = 8;

  cd_->mode.stop_time.time = 0;

  cd_->mode.drive_speed.speed = MEI_CDROM_DOUBLE_SPEED;
  cd_->mode.drive_speed.vpitch_msb = MEI_VPITCH_NORMAL_MSB;
  cd_->mode.drive_speed.vpitch_lsb = MEI_VPITCH_NORMAL_LSB;

  cd_->mode.chunk_size.size = 1;

  cdrom_mode_apply_block(cd_);
  cdrom_mode_apply_drive_speed(cd_);
}

static
bool
cdrom_mode_set_page(cdrom_device_t *cd_)
{
  switch(cd_->cmd[1])
    {
    case 0:
      if(!cdrom_mode_block_supported(cd_->cmd[2],
                                     cd_->cmd[3],
                                     cd_->cmd[4],
                                     cd_->cmd[5]))
        {
          opera_log_printf(OPERA_LOG_WARN,
                           "[Opera]: CDROM unsupported block mode density=%02x length=%02x%02x flags=%02x\n",
                           cd_->cmd[2],
                           cd_->cmd[3],
                           cd_->cmd[4],
                           cd_->cmd[5]);
          return false;
        }

      cd_->mode.block.density_code = cd_->cmd[2];
      cd_->mode.block.length_msb = cd_->cmd[3];
      cd_->mode.block.length_lsb = cd_->cmd[4];
      cd_->mode.block.flags = cd_->cmd[5];
      cdrom_mode_apply_block(cd_);

      return true;

    case 1:
      cd_->mode.error_recovery.type = cd_->cmd[2];
      cd_->mode.error_recovery.retry_count = cd_->cmd[3];

      return true;

    case 2:
      cd_->mode.stop_time.time = cd_->cmd[2];

      return true;

    case 3:
      cd_->mode.drive_speed.speed = cd_->cmd[2];
      cd_->mode.drive_speed.vpitch_msb = cd_->cmd[3];
      cd_->mode.drive_speed.vpitch_lsb = cd_->cmd[4];
      if((cd_->mode.drive_speed.speed != MEI_CDROM_SINGLE_SPEED) &&
         (cd_->mode.drive_speed.speed != MEI_CDROM_DOUBLE_SPEED))
        opera_log_printf(OPERA_LOG_WARN,
                         "[Opera]: CDROM unsupported drive speed byte=%02x; treating as single speed\n",
                         cd_->mode.drive_speed.speed);
      cdrom_mode_apply_drive_speed(cd_);

      return true;

    case 4:
      cd_->mode.chunk_size.size = cd_->cmd[2];
      if(cd_->mode.chunk_size.size == 0)
        cd_->mode.chunk_size.size = 1;
      if(cd_->mode.chunk_size.size > 8)
        cd_->mode.chunk_size.size = 8;

      return true;

    default:
      opera_log_printf(OPERA_LOG_WARN,
                       "[Opera]: CDROM unsupported mode set page=%02x args=%02x %02x %02x %02x\n",
                       cd_->cmd[1],
                       cd_->cmd[2],
                       cd_->cmd[3],
                       cd_->cmd[4],
                       cd_->cmd[5]);
      return false;
    }
}

static
void
cdrom_mode_sense_payload(const cdrom_device_t *cd_, uint8_t payload_[3])
{
  memset(payload_,0,3);

  switch(cd_->cmd[1])
    {
    case 0:
      payload_[0] = cd_->mode.block.density_code;
      payload_[1] = cd_->mode.block.length_msb;
      payload_[2] = cd_->mode.block.length_lsb;
      break;

    case 1:
      payload_[0] = cd_->mode.error_recovery.type;
      payload_[1] = cd_->mode.error_recovery.retry_count;
      break;

    case 2:
      payload_[0] = cd_->mode.stop_time.time;
      break;

    case 3:
      payload_[0] = cd_->mode.drive_speed.speed;
      payload_[1] = cd_->mode.drive_speed.vpitch_msb;
      payload_[2] = cd_->mode.drive_speed.vpitch_lsb;
      break;

    case 4:
      payload_[0] = cd_->mode.chunk_size.size;
      break;

    default:
      opera_log_printf(OPERA_LOG_WARN,
                       "[Opera]: CDROM unsupported mode sense page=%02x\n",
                       cd_->cmd[1]);
      break;
    }
}

static
uint64_t
cdrom_data_sector_cycles(const cdrom_device_t *cd_)
{
  const uint32_t speed = cdrom_effective_speed(cd_);
  uint64_t cycles;
  uint64_t compensation_cycles;

  if((cd_->read_sector_size != CDROM_DA) && (speed == 0))
    return 0;

  cycles = (opera_clock_cpu_get_freq() /
            (FRAMES_PER_SECOND *
             ((speed != 0) ? speed : cdrom_requested_speed(cd_))));

  if(cd_->read_sector_size == CDROM_DA)
    return cdrom_apply_pitch_to_cycles(cd_,cycles);

  compensation_cycles =
    ((uint64_t)opera_clock_cpu_get_freq() *
     cdrom_data_sector_overhead_compensation_x10_usec(cd_)) / 10000000;

  if(cycles > compensation_cycles)
    return cycles - compensation_cycles;

  return cycles;
}

static
void
cdrom_data_schedule_next(const cdrom_device_t *cd_)
{
  const uint64_t cycles = opera_clock_cpu_get_cycles();
  const uint64_t sector_cycles = cdrom_data_sector_cycles(cd_);

  if(g_CDROM_STATE.data.next_ready_cycle == 0)
    {
      g_CDROM_STATE.data.next_ready_cycle = cycles + sector_cycles;
      return;
    }

  g_CDROM_STATE.data.next_ready_cycle += sector_cycles;
  if((int64_t)(g_CDROM_STATE.data.next_ready_cycle - cycles) < -(int64_t)sector_cycles)
    g_CDROM_STATE.data.next_ready_cycle = cycles;
}

static
void
cdrom_data_schedule_start(const cdrom_device_t *cd_,
                          bool                  continuous_)
{
  const uint64_t cycles = opera_clock_cpu_get_cycles();

  if(continuous_ && (g_CDROM_STATE.data.next_ready_cycle != 0))
    {
      if(g_CDROM_STATE.data.next_ready_cycle < cycles)
        g_CDROM_STATE.data.next_ready_cycle = cycles;
      return;
    }

  g_CDROM_STATE.data.next_ready_cycle = cycles + cdrom_data_sector_cycles(cd_);
}

static
void
cdrom_data_reset_readahead_state(void)
{
  memset(&g_CDROM_STATE.data,0,sizeof(g_CDROM_STATE.data));
  g_CDROM_STATE.data.audio_min_ahead = UINT32_MAX;
}

static
void
cdrom_data_publish_read_status_if_ready(cdrom_device_t *cd_)
{
  /* MEI DIPIR consumes read data only when data is ready with the status. */
  if(!g_CDROM_STATE.data.read_status_pending || (cd_->data_len == 0))
    return;

  g_CDROM_STATE.data.read_status_pending = false;
  cd_->poll |= POLST;
}

static
void
cdrom_data_abort_readahead(cdrom_device_t *cd_)
{
  cd_->data_len = 0;
  cd_->data_idx = 0;
  cd_->poll &= ~POLDT;
  cdrom_data_reset_readahead_state();
}

static
void
cdrom_data_clear_transfer(cdrom_device_t *cd_)
{
  cd_->data_len = 0;
  cd_->data_idx = 0;
  cd_->blocks_requested = 0;
  memset(cd_->data,0,sizeof(cd_->data));
  cd_->poll &= ~POLDT;
  g_CDROM_STATE.data.read_status_pending = false;

  cdrom_data_reset_readahead_state();
}

static
bool
cdrom_mode_set_preserves_transfer(const cdrom_device_t *cd_)
{
  return ((cd_->cmd[1] == 3) &&
          (cd_->read_sector_size == CDROM_DA) &&
          g_CDROM_STATE.data.audio_clock_active &&
          (cd_->blocks_requested == 0) &&
          (cd_->data_len == 0) &&
          ((cd_->poll & POLDT) == 0));
}

static
void
cdrom_fifo_reset(cdrom_device_t *cd_)
{
  cdrom_data_clear_transfer(cd_);
  cd_->status_len = 0;
  cd_->cmd_idx = 0;
  memset(cd_->status,0,sizeof(cd_->status));
  memset(cd_->cmd,0,sizeof(cd_->cmd));
  cd_->poll &= ~POLST;
  ode_file_buffer_write_reset();
}

static
uint64_t
cdrom_data_audio_elapsed_frames(uint64_t cycles_)
{
  if(cycles_ <= g_CDROM_STATE.data.audio_base_cycle)
    return 0;

  return (((cycles_ - g_CDROM_STATE.data.audio_base_cycle) * FRAMES_PER_SECOND) /
          opera_clock_cpu_get_freq());
}

static
uint32_t
cdrom_data_audio_position_lba(const cdrom_device_t *cd_,
                              uint64_t              cycles_)
{
  uint64_t playback_lba;

  if(!g_CDROM_STATE.data.audio_clock_active ||
     (cd_->current_sector < g_CDROM_STATE.data.audio_base_lba))
    return MSF2LBA(&cd_->disc.msf_current);

  playback_lba = (uint64_t)g_CDROM_STATE.data.audio_base_lba +
    cdrom_data_audio_elapsed_frames(cycles_);

  if(playback_lba > cd_->current_sector)
    playback_lba = cd_->current_sector;

  return (uint32_t)playback_lba;
}

static
void
cdrom_data_audio_clock_start(cdrom_device_t *cd_,
                             uint32_t        start_lba_,
                             uint32_t        end_lba_)
{
  uint32_t disc_blocks;

  disc_blocks = cd_->callbacks.get_size();
  if(end_lba_ > disc_blocks)
    end_lba_ = disc_blocks;
  if(start_lba_ > end_lba_)
    start_lba_ = end_lba_;

  g_CDROM_STATE.data.audio_base_lba = start_lba_;
  g_CDROM_STATE.data.audio_base_cycle = opera_clock_cpu_get_cycles();
  g_CDROM_STATE.data.audio_clock_active = (start_lba_ < end_lba_);
  g_CDROM_STATE.data.audio_burst_active = false;

  cd_->current_sector = end_lba_;
  LBA2MSF(start_lba_,&cd_->disc.msf_current);
}

static
void
cdrom_data_audio_clock_stop(cdrom_device_t *cd_)
{
  uint32_t current_lba;

  current_lba = cdrom_data_audio_position_lba(cd_,opera_clock_cpu_get_cycles());
  LBA2MSF(current_lba,&cd_->disc.msf_current);

  cd_->current_sector = current_lba;
  g_CDROM_STATE.data.audio_clock_active = false;
  g_CDROM_STATE.data.audio_burst_active = false;
}

static
uint32_t
cdrom_data_audio_read_ahead(const cdrom_device_t *cd_,
                            uint64_t              cycles_)
{
  uint64_t elapsed_frames;
  uint32_t streamed_frames;

  if((cd_->read_sector_size != CDROM_DA) ||
     !g_CDROM_STATE.data.audio_clock_active ||
     (cd_->current_sector < g_CDROM_STATE.data.audio_base_lba))
    return 0;

  streamed_frames = cd_->current_sector - g_CDROM_STATE.data.audio_base_lba;
  elapsed_frames = cdrom_data_audio_elapsed_frames(cycles_);

  if(streamed_frames <= elapsed_frames)
    return 0;

  return (uint32_t)(streamed_frames - elapsed_frames);
}

static
void
cdrom_data_preserve_drive_speed_mode_set(const cdrom_device_t *cd_)
{
  const uint64_t cycles = opera_clock_cpu_get_cycles();

  if((g_CDROM_STATE.data.next_ready_cycle != 0) &&
     (g_CDROM_STATE.data.next_ready_cycle < cycles))
    g_CDROM_STATE.data.next_ready_cycle = cycles;
}

static
uint32_t
cdrom_data_audio_refill_threshold(uint32_t cap_)
{
  if(cap_ <= 8)
    return 0;

  return (cap_ - 1);
}

static
bool
cdrom_data_audio_can_burst(const cdrom_device_t *cd_,
                           uint64_t              cycles_)
{
  const uint32_t cap = cdrom_audio_read_ahead_sectors();
  const uint32_t ahead = cdrom_data_audio_read_ahead(cd_,cycles_);

  if((cd_->read_sector_size != CDROM_DA) ||
     !g_CDROM_STATE.data.audio_clock_active ||
     (cap == 0))
    return false;

  if(ahead >= cap)
    {
      g_CDROM_STATE.data.audio_burst_active = false;
      return false;
    }

  if(!g_CDROM_STATE.data.audio_burst_active &&
     (ahead <= cdrom_data_audio_refill_threshold(cap)))
    {
      g_CDROM_STATE.data.audio_burst_active = true;
      g_CDROM_STATE.data.audio_burst_starts++;
    }

  return g_CDROM_STATE.data.audio_burst_active;
}

static
uint64_t
cdrom_data_audio_read_ahead_cycle(const cdrom_device_t *cd_)
{
  uint64_t elapsed_frames;
  uint32_t cap;

  if((cd_->read_sector_size != CDROM_DA) ||
     !g_CDROM_STATE.data.audio_clock_active ||
     (cd_->current_sector < g_CDROM_STATE.data.audio_base_lba))
    return 0;

  cap = cdrom_audio_read_ahead_sectors();
  if(cap == 0)
    return 0;

  elapsed_frames = cd_->current_sector - g_CDROM_STATE.data.audio_base_lba;
  if(elapsed_frames < cap)
    return 0;

  elapsed_frames -= (cap - 1);
  return (g_CDROM_STATE.data.audio_base_cycle +
          ((elapsed_frames * opera_clock_cpu_get_freq()) / FRAMES_PER_SECOND));
}

static
void
cdrom_data_load_next_if_ready(cdrom_device_t *cd_)
{
  uint64_t cycles;
  uint64_t audio_ready_cycle;
  bool audio_burst;
  uint32_t lba;
  uint32_t read_size;
  uint32_t wire_size;

  if((cd_->data_len > 0) || (cd_->blocks_requested == 0))
    return;

  cycles = opera_clock_cpu_get_cycles();
  audio_burst = cdrom_data_audio_can_burst(cd_,cycles);

  if(!audio_burst && (cycles < g_CDROM_STATE.data.next_ready_cycle))
    {
      uint64_t wait_cycles;

      wait_cycles = g_CDROM_STATE.data.next_ready_cycle - cycles;
      g_CDROM_STATE.data.wait_ready++;
      g_CDROM_STATE.data.wait_cycles += wait_cycles;
      if(wait_cycles > g_CDROM_STATE.data.max_wait_cycles)
        g_CDROM_STATE.data.max_wait_cycles = wait_cycles;

      cd_->poll &= ~POLDT;
      return;
    }

  audio_ready_cycle = cdrom_data_audio_read_ahead_cycle(cd_);
  if((audio_ready_cycle != 0) && (cycles < audio_ready_cycle))
    {
      uint64_t wait_cycles;

      wait_cycles = audio_ready_cycle - cycles;
      g_CDROM_STATE.data.audio_burst_active = false;
      g_CDROM_STATE.data.next_ready_cycle = audio_ready_cycle;
      g_CDROM_STATE.data.wait_ready++;
      g_CDROM_STATE.data.wait_cycles += wait_cycles;
      if(wait_cycles > g_CDROM_STATE.data.max_wait_cycles)
        g_CDROM_STATE.data.max_wait_cycles = wait_cycles;

      cd_->poll &= ~POLDT;
      return;
    }

  lba = cd_->current_sector++;
  cd_->callbacks.set_sector(lba);
  read_size = cd_->read_sector_size;
  wire_size = cdrom_data_wire_size(cd_);
  if(read_size > sizeof(cd_->data))
    read_size = sizeof(cd_->data);
  if(wire_size > sizeof(cd_->data))
    wire_size = sizeof(cd_->data);

  memset(cd_->data,0,wire_size);
  cd_->callbacks.read_sector(cd_->data,read_size);
  LBA2MSF(cd_->current_sector,&cd_->disc.msf_current);
  cd_->data_len = wire_size;
  g_CDROM_STATE.data.active_lba = lba;
  g_CDROM_STATE.data.active_bytes = cd_->data_len;
  cd_->blocks_requested--;
  cd_->poll |= POLDT;
  g_CDROM_STATE.data.sectors_queued++;
  cdrom_data_publish_read_status_if_ready(cd_);

  if(audio_burst)
    g_CDROM_STATE.data.next_ready_cycle = cycles;
  else
    cdrom_data_schedule_next(cd_);

}

static
void
ode_file_buffer_write_reset(void)
{
  memset(g_CDROM_STATE.ode.file_buffer_write_data,0,sizeof(g_CDROM_STATE.ode.file_buffer_write_data));
  g_CDROM_STATE.ode.file_buffer_write_idx = 0;
  g_CDROM_STATE.ode.file_buffer_write_overflow = false;
}

static
void
ode_playlist_clear(void)
{
  uint32_t i;

  for(i = 0; i < ODE_PLAYLIST_MAX; i++)
    g_CDROM_STATE.ode.playlist[i][0] = 0;

  g_CDROM_STATE.ode.playlist_count = 0;
}

static
void
ode_str_copy(char       *dst_,
             const char *src_,
             size_t      size_)
{
  size_t len;

  if(size_ == 0)
    return;

  if(src_ == NULL)
    src_ = "";

  len = strlen(src_);
  if(len >= size_)
    len = size_ - 1;

  memcpy(dst_,src_,len);
  dst_[len] = 0;
}

static
void
ode_state_str_restore(char       *dst_,
                      const char *src_,
                      size_t      size_)
{
  if(size_ == 0)
    return;

  if(src_ == NULL)
    {
      dst_[0] = 0;
      return;
    }

  memcpy(dst_,src_,size_);
  dst_[size_ - 1] = 0;
}

static
void
ode_file_close(void)
{
  if(g_CDROM_STATE.ode.file != NULL)
    {
      filestream_close(g_CDROM_STATE.ode.file);
      g_CDROM_STATE.ode.file = NULL;
    }
  g_CDROM_STATE.ode.file_path[0] = 0;
  g_CDROM_STATE.ode.file_mode = 0;
}

static
void
ode_reset_session(void)
{
  ode_file_close();
  ode_str_copy(g_CDROM_STATE.ode.current,g_CDROM_STATE.ode.root,sizeof(g_CDROM_STATE.ode.current));
  ode_playlist_clear();
  g_CDROM_STATE.ode.pending_launch_path[0] = 0;
  memset(g_CDROM_STATE.ode.toc,0xFF,sizeof(g_CDROM_STATE.ode.toc));
  memset(g_CDROM_STATE.ode.file_buffer,0,sizeof(g_CDROM_STATE.ode.file_buffer));
  ode_file_buffer_write_reset();
  g_CDROM_STATE.ode.restart_armed = false;
  g_CDROM_STATE.ode.restart_requested = false;
  g_CDROM_STATE.ode.media_access_after_reset = false;
}

void
opera_cdrom_ode_set_root(const char *root_)
{
  if((root_ == NULL) || (root_[0] == 0))
    {
      g_CDROM_STATE.ode.root[0] = 0;
      ode_reset_session();
      g_CDROM_STATE.ode.savestate_blocked = false;
      return;
    }

  ode_str_copy(g_CDROM_STATE.ode.root,root_,sizeof(g_CDROM_STATE.ode.root));
  ode_reset_session();
  g_CDROM_STATE.ode.savestate_blocked = false;
}

void
opera_cdrom_ode_set_launch_callback(opera_cdrom_ode_launch_cb_t launch_)
{
  g_CDROM_STATE.ode.launch = launch_;
}

static
void
cdrom_media_access_latch(cdrom_device_t *cd_)
{
  cd_->poll |= POLRE;
}

static
void
ode_complete_restart_if_armed(cdrom_device_t *cd_)
{
  if(!g_CDROM_STATE.ode.restart_armed)
    return;

  g_CDROM_STATE.ode.restart_armed = false;
  /* Simulate disc insertion before the host swaps media and resets. */
  cd_->xbus_status |= (CDROM_STATUS_READY |
                       CDROM_STATUS_DOOR |
                       CDROM_STATUS_DISC_IN |
                       CDROM_STATUS_SPIN_UP);
  cd_->xbus_status &= ~CDROM_STATUS_ERROR;
  cdrom_mode_apply_drive_speed(cd_);
  cdrom_media_access_latch(cd_);
  g_CDROM_STATE.ode.restart_requested = true;

}

int
opera_cdrom_ode_consume_restart_request(void)
{
  bool requested;

  requested = g_CDROM_STATE.ode.restart_requested;
  g_CDROM_STATE.ode.restart_requested = false;
  if(!requested)
    return false;

  requested = false;
  if((g_CDROM_STATE.ode.pending_launch_path[0] != 0) &&
     (g_CDROM_STATE.ode.launch != NULL) &&
     (g_CDROM_STATE.ode.launch(g_CDROM_STATE.ode.pending_launch_path) == 0))
    {
      g_CDROM_STATE.ode.media_access_after_reset = true;
      requested = true;
    }

  g_CDROM_STATE.ode.pending_launch_path[0] = 0;

  return requested;
}

int
opera_cdrom_ode_restart_requested(void)
{
  return g_CDROM_STATE.ode.restart_requested;
}

void
opera_cdrom_ode_reset_session(void)
{
  ode_reset_session();
}

static
size_t
ode_trimmed_len(const char *path_)
{
  size_t len;

  if(path_ == NULL)
    return 0;

  len = strlen(path_);
  while((len > 1) && ((path_[len - 1] == '/') || (path_[len - 1] == '\\')))
    len--;

  return len;
}

static
bool
ode_path_equal(const char *a_,
               const char *b_)
{
  size_t a_len;
  size_t b_len;

  a_len = ode_trimmed_len(a_);
  b_len = ode_trimmed_len(b_);

  return ((a_len == b_len) && (strncmp(a_,b_,a_len) == 0));
}

static
bool
ode_current_is_root(void)
{
  return ode_path_equal(g_CDROM_STATE.ode.current,g_CDROM_STATE.ode.root);
}

static
bool
ode_relative_path_is_safe(const char *path_)
{
  const char *component;
  const char *p;

  if((path_ == NULL) || (path_[0] == 0) || path_is_absolute(path_))
    return false;

  component = path_;
  for(p = path_; ; p++)
    {
      if((*p != '/') && (*p != '\\') && (*p != 0))
        continue;

      if((p == component) ||
         (((p - component) == 1) && (component[0] == '.')) ||
         (((p - component) == 2) && (component[0] == '.') && (component[1] == '.')))
        return false;

      if(*p == 0)
        break;

      component = p + 1;
    }

  return true;
}

static
bool
ode_path_is_under_root(const char *path_)
{
  size_t root_len;

  if((path_ == NULL) || (g_CDROM_STATE.ode.root[0] == 0))
    return false;

  root_len = ode_trimmed_len(g_CDROM_STATE.ode.root);
  if(strncmp(path_,g_CDROM_STATE.ode.root,root_len) != 0)
    return false;

  return ((path_[root_len] == 0) ||
          (path_[root_len] == '/') ||
          (path_[root_len] == '\\') ||
          (g_CDROM_STATE.ode.root[root_len - 1] == '/') ||
          (g_CDROM_STATE.ode.root[root_len - 1] == '\\'));
}

static
bool
ode_file_buffer_name_path(uint16_t name_len_,
                          char    *path_,
                          size_t   path_size_)
{
  char name[REQSIZE];
  size_t actual_len;

  if((path_ == NULL) || (path_size_ == 0))
    return false;

  path_[0] = 0;

  if((name_len_ == 0) || (name_len_ >= REQSIZE))
    return false;

  actual_len = strnlen((const char*)g_CDROM_STATE.ode.file_buffer, name_len_);
  if(actual_len == 0)
    return false;

  memcpy(name, g_CDROM_STATE.ode.file_buffer, actual_len);
  name[actual_len] = 0;

  if(!ode_relative_path_is_safe(name))
    return false;

  fill_pathname_join(path_, g_CDROM_STATE.ode.current, name, path_size_);

  return ((path_[0] != 0) && ode_path_is_under_root(path_));
}

static
void
ode_go_parent(void)
{
  char parent[PATH_MAX_LENGTH];

  if(ode_current_is_root())
    return;

  ode_str_copy(parent,g_CDROM_STATE.ode.current,sizeof(parent));
  path_parent_dir(parent);
  if(!ode_path_is_under_root(parent))
    ode_str_copy(parent,g_CDROM_STATE.ode.root,sizeof(parent));

  ode_str_copy(g_CDROM_STATE.ode.current,parent,sizeof(g_CDROM_STATE.ode.current));
}

static
void
ode_toc_put_u32(uint8_t  *dst_,
                uint32_t *pos_,
                uint32_t  val_)
{
  dst_[(*pos_)++] = (uint8_t)(val_ >> 24);
  dst_[(*pos_)++] = (uint8_t)(val_ >> 16);
  dst_[(*pos_)++] = (uint8_t)(val_ >> 8);
  dst_[(*pos_)++] = (uint8_t)val_;
}

static
bool
ode_toc_add_entry(uint8_t    *toc_,
                  uint32_t   *pos_,
                  uint32_t    flags_,
                  uint32_t    toc_id_,
                  const char *name_)
{
  size_t name_len;

  name_len = strlen(name_) + 1;
  if(name_len > ODE_TOC_NAME_LIMIT)
    return false;
  if((*pos_ + 12 + name_len) > REQSIZE)
    return false;

  ode_toc_put_u32(toc_,pos_,flags_);
  ode_toc_put_u32(toc_,pos_,toc_id_);
  ode_toc_put_u32(toc_,pos_,(uint32_t)name_len);
  memcpy(&toc_[*pos_],name_,name_len);
  *pos_ += (uint32_t)name_len;

  return true;
}

static
void
ode_build_toc(uint32_t start_id_,
              uint16_t count_)
{
  struct string_list *list;
  uint32_t pos;
  uint32_t emitted;
  uint32_t id;
  uint32_t start;
  uint32_t limit;
  size_t i;

  memset(g_CDROM_STATE.ode.toc,0xFF,sizeof(g_CDROM_STATE.ode.toc));

  if(g_CDROM_STATE.ode.root[0] == 0)
    return;

  pos     = 0;
  emitted = 0;
  id      = 0;
  start   = (start_id_ & ODE_TOC_ENTRY_MASK);
  limit   = (count_ == 0) ? UINT32_MAX : count_;

  if(!ode_current_is_root() && (start == 0) && (emitted < limit))
    {
      if(ode_toc_add_entry(g_CDROM_STATE.ode.toc,&pos,ODE_TOC_FLAG_DIR,ODE_TOC_FLAG_INVALID,".."))
        emitted++;
    }

  list = dir_list_new(g_CDROM_STATE.ode.current,
                      ODE_TOC_SUPPORTED_EXTS,
                      true,     /* include_dirs */
                      false,    /* include_hidden */
                      false,    /* include_compressed */
                      false);   /* recursive */
  if(list == NULL)
    return;

  dir_list_sort(list,true);

  for(i = 0; i < list->size; i++)
    {
      const char *name;
      uint32_t flags;

      id++;
      if((start != 0) && (id < start))
        continue;
      if(emitted >= limit)
        break;

      name = path_basename(list->elems[i].data);
      if((name == NULL) || (name[0] == 0))
        continue;

      flags = (list->elems[i].attr.i == RARCH_DIRECTORY) ?
        ODE_TOC_FLAG_DIR :
        ODE_TOC_FLAG_FILE;

      if(ode_toc_add_entry(g_CDROM_STATE.ode.toc,&pos,flags,id,name))
        emitted++;
    }

  dir_list_free(list);
}

static
bool
ode_find_toc_path(uint32_t  toc_id_,
                  char     *path_,
                  size_t    path_size_,
                  bool     *is_dir_)
{
  struct string_list *list;
  uint32_t target;
  uint32_t id;
  size_t i;

  if(path_size_ > 0)
    path_[0] = 0;
  if(is_dir_ != NULL)
    *is_dir_ = false;

  if(g_CDROM_STATE.ode.root[0] == 0)
    return false;

  target = (toc_id_ & ODE_TOC_ENTRY_MASK);
  if(target == ODE_TOC_ENTRY_MASK)
    {
      ode_str_copy(path_,g_CDROM_STATE.ode.current,path_size_);
      path_parent_dir(path_);
      if(!ode_path_is_under_root(path_))
        ode_str_copy(path_,g_CDROM_STATE.ode.root,path_size_);
      if(is_dir_ != NULL)
        *is_dir_ = true;
      return true;
    }

  if(target == 0)
    {
      ode_str_copy(path_,g_CDROM_STATE.ode.current,path_size_);
      if(is_dir_ != NULL)
        *is_dir_ = true;
      return true;
    }

  list = dir_list_new(g_CDROM_STATE.ode.current,
                      ODE_TOC_SUPPORTED_EXTS,
                      true,     /* include_dirs */
                      false,    /* include_hidden */
                      false,    /* include_compressed */
                      false);   /* recursive */
  if(list == NULL)
    return false;

  dir_list_sort(list,true);

  id = 0;
  for(i = 0; i < list->size; i++)
    {
      id++;
      if(id != target)
        continue;

      ode_str_copy(path_,list->elems[i].data,path_size_);
      if(is_dir_ != NULL)
        *is_dir_ = (list->elems[i].attr.i == RARCH_DIRECTORY);
      dir_list_free(list);
      return true;
    }

  dir_list_free(list);
  return false;
}

static
INLINE
void
FRAMES2MSF(const uint32_t frames_,
           msf_t          *msf_)
{
  uint32_t mod;

  mod           = (frames_ % (SECONDS_PER_MINUTE * FRAMES_PER_SECOND));
  msf_->minutes = (frames_ / (SECONDS_PER_MINUTE * FRAMES_PER_SECOND));
  msf_->seconds = (mod / FRAMES_PER_SECOND);
  msf_->frames  = (mod % FRAMES_PER_SECOND);
}

static
INLINE
void
LBA2MSF(const uint32_t  lba_,
        msf_t          *msf_)
{
  FRAMES2MSF(lba_ + MSF_BIAS_IN_FRAMES,msf_);
}

static
INLINE
uint32_t
MSF2LBA(const msf_t *msf_)
{
  uint32_t frames;

  frames = ((msf_->minutes * SECONDS_PER_MINUTE * FRAMES_PER_SECOND) +
            (msf_->seconds * FRAMES_PER_SECOND) +
            (msf_->frames));
  if(frames < MSF_BIAS_IN_FRAMES)
    return 0;

  return (frames - MSF_BIAS_IN_FRAMES);
}

static
void
cdrom_disc_layout_set_single_track(cdrom_device_t *cd_,
                                   uint32_t        file_size_in_blocks_)
{
  msf_t start_msf;

  memset(cd_->disc.disc_toc,0,sizeof(cd_->disc.disc_toc));

  cd_->disc.track_first = 1;
  cd_->disc.track_last  = 1;
  cd_->disc.disc_id     = MEI_DISC_DA_OR_CDROM;
  cd_->disc.session_valid = 0;
  memset(&cd_->disc.msf_session,0,sizeof(cd_->disc.msf_session));

  cd_->disc.disc_toc[1].CDCTL        =
    CD_CTL_DATA_TRACK | CD_CTL_Q_POSITION;
  cd_->disc.disc_toc[1].track_number = 1;
  LBA2MSF(0,&start_msf);
  cd_->disc.disc_toc[1].minutes = start_msf.minutes;
  cd_->disc.disc_toc[1].seconds = start_msf.seconds;
  cd_->disc.disc_toc[1].frames  = start_msf.frames;
  LBA2MSF(file_size_in_blocks_,&cd_->disc.msf_total);
}

static
void
cdrom_disc_layout_sanitize(cdrom_device_t *cd_,
                           uint32_t        file_size_in_blocks_)
{
  uint32_t i;
  uint32_t first;
  uint32_t last;

  LBA2MSF(file_size_in_blocks_,&cd_->disc.msf_total);

  if(!cdrom_msf_valid(&cd_->disc.msf_session))
    {
      cd_->disc.session_valid = 0;
      memset(&cd_->disc.msf_session,0,sizeof(cd_->disc.msf_session));
    }

  if((cd_->disc.disc_id != MEI_DISC_DA_OR_CDROM) &&
     (cd_->disc.disc_id != MEI_DISC_CDI) &&
     (cd_->disc.disc_id != MEI_DISC_CDROM_XA))
    cd_->disc.disc_id = MEI_DISC_DA_OR_CDROM;

  if((!cdrom_toc_track_in_range(&cd_->disc,cd_->disc.track_first)) ||
     (!cdrom_toc_track_in_range(&cd_->disc,cd_->disc.track_last)) ||
     (cd_->disc.track_last < cd_->disc.track_first))
    {
      cdrom_disc_layout_set_single_track(cd_,file_size_in_blocks_);
      return;
    }

  first = 0;
  last = 0;
  for(i = cd_->disc.track_first; i <= cd_->disc.track_last; i++)
    {
      toc_entry_t *toc = &cd_->disc.disc_toc[i];

      if(!cdrom_toc_entry_valid(&cd_->disc,i,toc))
        continue;

      toc->CDCTL = cdrom_toc_position_control(toc->CDCTL);
      toc->track_number = (uint8_t)i;
      if(first == 0)
        first = i;
      last = i;
    }

  if(first == 0)
    {
      cdrom_disc_layout_set_single_track(cd_,file_size_in_blocks_);
      return;
    }

  cd_->disc.track_first = (uint8_t)first;
  cd_->disc.track_last = (uint8_t)last;
}

static
void
opera_cdrom_update_disc_layout(cdrom_device_t *cd_)
{
  uint32_t file_size_in_blocks;

  file_size_in_blocks = cd_->callbacks.get_size();

  cd_->disc.msf_current.minutes = 0;
  cd_->disc.msf_current.seconds = MSF_BIAS_IN_SECONDS;
  cd_->disc.msf_current.frames  = 0;

  /* Try to populate TOC from disc image metadata */
  if(cd_->callbacks.get_toc != NULL)
    {
      memset(cd_->disc.disc_toc,0,sizeof(cd_->disc.disc_toc));
      cd_->disc.session_valid = 0;
      memset(&cd_->disc.msf_session,0,sizeof(cd_->disc.msf_session));
      cd_->callbacks.get_toc(&cd_->disc.track_first,
                             &cd_->disc.track_last,
                             &cd_->disc.disc_id,
                             cd_->disc.disc_toc,
                             sizeof(cd_->disc.disc_toc));
    }
  else
    {
      /* Fallback: single-track data disc */
      cdrom_disc_layout_set_single_track(cd_,file_size_in_blocks);
    }

  cdrom_disc_layout_sanitize(cd_,file_size_in_blocks);
}

void
opera_cdrom_set_callbacks(opera_cdrom_get_size_cb_t    get_size_,
                          opera_cdrom_set_sector_cb_t  set_sector_,
                          opera_cdrom_read_sector_cb_t read_sector_,
                          opera_cdrom_get_toc_cb_t     get_toc_)
{
  g_CDROM_CALLBACKS.get_size    = get_size_;
  g_CDROM_CALLBACKS.set_sector  = set_sector_;
  g_CDROM_CALLBACKS.read_sector = read_sector_;
  g_CDROM_CALLBACKS.get_toc     = get_toc_;
}

void
opera_cdrom_set_speed(uint32_t speed_)
{
  switch(speed_)
    {
    case 0:
    case 1:
    case 2:
    case 4:
    case 8:
    case 16:
    case 24:
    case 32:
      g_CDROM_STATE.configured_speed = speed_;
      break;
    default:
      opera_log_printf(OPERA_LOG_WARN,
                       "[Opera]: CDROM unsupported configured speed=%u; using unbounded\n",
                       speed_);
      g_CDROM_STATE.configured_speed = 0;
      break;
    }

  if(cdrom_configured_speed() == 0)
    opera_log_printf(OPERA_LOG_INFO,
                     "[Opera]: CDROM configured host speed=unbounded\n");
  else
    opera_log_printf(OPERA_LOG_INFO,
                     "[Opera]: CDROM configured host speed=%ux\n",
                     cdrom_configured_speed());
}

static
bool
cdrom_configured_speed_valid(uint32_t speed_)
{
  switch(speed_)
    {
    case 0:
    case 1:
    case 2:
    case 4:
    case 8:
    case 16:
    case 24:
    case 32:
      return true;
    default:
      return false;
    }
}

static
bool
cdrom_cmd_is_ode(uint8_t cmd_)
{
  switch(cmd_)
    {
    case ODE_CMD_EXTENDED_ID:
    case ODE_CMD_CHANGE_TOC:
    case ODE_CMD_READ_TOC_WINDOW:
    case ODE_CMD_READ_DESCRIPTION:
    case ODE_CMD_CLEAR_PLAYLIST:
    case ODE_CMD_ADD_PLAYLIST:
    case ODE_CMD_LAUNCH_PLAYLIST:
    case ODE_CMD_READ_TOC_LIST:
    case ODE_CMD_CREATE_FILE:
    case ODE_CMD_OPEN_FILE:
    case ODE_CMD_SEEK_FILE:
    case ODE_CMD_READ_FILE:
    case ODE_CMD_WRITE_FILE:
    case ODE_CMD_CLOSE_FILE:
    case ODE_CMD_WRITE_BUFFER_WORD:
    case ODE_CMD_READ_FILE_BUFFER:
    case ODE_CMD_FAST_BUFFER_SEND:
    case ODE_CMD_START_UPDATE:
      return true;
    default:
      return false;
    }
}

static
void
cdrom_state_v2_device_save(const cdrom_device_t *cd_,
                           cdrom_device_state_v2_t *state_)
{
  memset(state_,0,sizeof(*state_));

  state_->poll = cd_->poll;
  state_->xbus_status = cd_->xbus_status;
  state_->status_len = cd_->status_len;
  memcpy(state_->status,cd_->status,sizeof(state_->status));
  state_->read_sector_size = cd_->read_sector_size;
  state_->data_len = cd_->data_len;
  state_->data_idx = cd_->data_idx;
  memcpy(state_->data,cd_->data,sizeof(state_->data));
  state_->blocks_requested = cd_->blocks_requested;
  memcpy(state_->cmd,cd_->cmd,sizeof(state_->cmd));
  state_->cmd_idx = cd_->cmd_idx;
  state_->STATCYC = cd_->STATCYC;
  state_->MEI_status = cd_->MEI_status;
  state_->current_sector = cd_->current_sector;
  state_->mode = cd_->mode;
  state_->disc = cd_->disc;
}

static
bool
cdrom_state_v2_device_load(cdrom_device_t *cd_,
                           const cdrom_device_state_v2_t *state_)
{
  memset(cd_,0,sizeof(*cd_));

  cd_->poll = state_->poll;
  cd_->xbus_status = state_->xbus_status;
  cd_->status_len = state_->status_len;
  if(cd_->status_len > sizeof(cd_->status))
    return false;
  memcpy(cd_->status,state_->status,sizeof(cd_->status));
  cd_->read_sector_size = state_->read_sector_size;
  if(cd_->read_sector_size > CDROM_MAX_SECTOR_SIZE)
    return false;
  cd_->data_len = state_->data_len;
  cd_->data_idx = state_->data_idx;
  if((cd_->data_idx > sizeof(cd_->data)) ||
     (cd_->data_len > (sizeof(cd_->data) - cd_->data_idx)))
    return false;
  memcpy(cd_->data,state_->data,sizeof(cd_->data));
  cd_->blocks_requested = state_->blocks_requested;
  memcpy(cd_->cmd,state_->cmd,sizeof(cd_->cmd));
  cd_->cmd_idx = state_->cmd_idx;
  if(cd_->cmd_idx > sizeof(cd_->cmd))
    return false;
  cd_->STATCYC = state_->STATCYC;
  cd_->MEI_status = state_->MEI_status;
  cd_->current_sector = state_->current_sector;
  cd_->mode = state_->mode;
  cd_->disc = state_->disc;

  return true;
}

static
bool
cdrom_state_v1_legacy_device_load(cdrom_device_t *cd_,
                                  const cdrom_device_state_v1_legacy_t *state_)
{
  memset(cd_,0,sizeof(*cd_));

  cdrom_mode_reset(cd_);

  cd_->poll = state_->poll;
  cd_->xbus_status = state_->xbus_status;
  cd_->status_len = state_->status_len;
  memcpy(cd_->status,state_->status,sizeof(cd_->status));
  cd_->data_len = state_->data_len;
  cd_->data_idx = state_->data_idx;
  if((cd_->data_idx > sizeof(cd_->data)) ||
     (cd_->data_len > (sizeof(cd_->data) - cd_->data_idx)))
    return false;
  memcpy(cd_->data,state_->data,sizeof(state_->data));
  cd_->blocks_requested = state_->blocks_requested;
  memcpy(cd_->cmd,state_->cmd,sizeof(cd_->cmd));
  cd_->cmd_idx = state_->cmd_idx;
  if(cd_->cmd_idx > sizeof(cd_->cmd))
    return false;
  cd_->STATCYC = state_->STATCYC;
  cd_->MEI_status = state_->MEI_status;
  cd_->current_sector = state_->current_sector;

  cd_->mode.drive_speed.speed =
    (state_->xbus_status & CDROM_STATUS_DOUBLE_SPEED) ?
    MEI_CDROM_DOUBLE_SPEED :
    MEI_CDROM_SINGLE_SPEED;
  cdrom_mode_apply_drive_speed(cd_);
  opera_cdrom_restore_callbacks(cd_);
  opera_cdrom_update_disc_layout(cd_);

  return true;
}

static
uint64_t
cdrom_state_v2_cycles_remaining(uint64_t target_,
                                uint64_t now_)
{
  if(target_ <= now_)
    return 0;

  return (target_ - now_);
}

static
uint64_t
cdrom_state_v2_cycle_age(uint64_t base_,
                         uint64_t now_)
{
  if(base_ >= now_)
    return 0;

  return (now_ - base_);
}

static
uint64_t
cdrom_state_v2_cycles_after(uint64_t now_,
                            uint64_t delta_)
{
  if(delta_ > (UINT64_MAX - now_))
    return UINT64_MAX;

  return (now_ + delta_);
}

static
uint64_t
cdrom_state_v2_cycles_before(uint64_t now_,
                             uint64_t age_)
{
  if(age_ > now_)
    return 0;

  return (now_ - age_);
}

static
void
cdrom_state_v2_data_save(cdrom_data_state_v2_t *state_)
{
  uint64_t now;

  now = opera_clock_cpu_get_cycles();

  memset(state_,0,sizeof(*state_));
  state_->next_ready_cycles_remaining =
    cdrom_state_v2_cycles_remaining(g_CDROM_STATE.data.next_ready_cycle,now);
  state_->sectors_queued = g_CDROM_STATE.data.sectors_queued;
  state_->sectors_drained = g_CDROM_STATE.data.sectors_drained;
  state_->empty_reads = g_CDROM_STATE.data.empty_reads;
  state_->read_expected_lba = g_CDROM_STATE.data.read_expected_lba;
  state_->read_have_expected_lba = g_CDROM_STATE.data.read_have_expected_lba;
  state_->audio_burst_active = g_CDROM_STATE.data.audio_burst_active;
  state_->audio_burst_starts = g_CDROM_STATE.data.audio_burst_starts;
  state_->audio_min_ahead = g_CDROM_STATE.data.audio_min_ahead;
  state_->audio_max_ahead = g_CDROM_STATE.data.audio_max_ahead;
  state_->active_lba = g_CDROM_STATE.data.active_lba;
  state_->active_bytes = g_CDROM_STATE.data.active_bytes;
  state_->audio_base_lba = g_CDROM_STATE.data.audio_base_lba;
  state_->audio_base_cycle_age =
    cdrom_state_v2_cycle_age(g_CDROM_STATE.data.audio_base_cycle,now);
  state_->audio_clock_active = g_CDROM_STATE.data.audio_clock_active;
  state_->read_status_pending = g_CDROM_STATE.data.read_status_pending;
  state_->wait_ready = g_CDROM_STATE.data.wait_ready;
  state_->wait_cycles = g_CDROM_STATE.data.wait_cycles;
  state_->max_wait_cycles = g_CDROM_STATE.data.max_wait_cycles;
}

static
void
cdrom_state_v2_data_load(const cdrom_data_state_v2_t *state_)
{
  uint64_t now;

  now = opera_clock_cpu_get_cycles();

  g_CDROM_STATE.data.next_ready_cycle =
    (state_->next_ready_cycles_remaining == 0) ?
    0 :
    cdrom_state_v2_cycles_after(now,state_->next_ready_cycles_remaining);
  g_CDROM_STATE.data.sectors_queued = state_->sectors_queued;
  g_CDROM_STATE.data.sectors_drained = state_->sectors_drained;
  g_CDROM_STATE.data.empty_reads = state_->empty_reads;
  g_CDROM_STATE.data.read_expected_lba = state_->read_expected_lba;
  g_CDROM_STATE.data.read_have_expected_lba =
    (state_->read_have_expected_lba != 0);
  g_CDROM_STATE.data.audio_burst_active =
    (state_->audio_burst_active != 0);
  g_CDROM_STATE.data.audio_burst_starts = state_->audio_burst_starts;
  g_CDROM_STATE.data.audio_min_ahead = state_->audio_min_ahead;
  g_CDROM_STATE.data.audio_max_ahead = state_->audio_max_ahead;
  g_CDROM_STATE.data.active_lba = state_->active_lba;
  g_CDROM_STATE.data.active_bytes = state_->active_bytes;
  g_CDROM_STATE.data.audio_base_lba = state_->audio_base_lba;
  g_CDROM_STATE.data.audio_base_cycle =
    cdrom_state_v2_cycles_before(now,state_->audio_base_cycle_age);
  g_CDROM_STATE.data.audio_clock_active =
    (state_->audio_clock_active != 0);
  g_CDROM_STATE.data.read_status_pending =
    (state_->read_status_pending != 0);
  g_CDROM_STATE.data.wait_ready = state_->wait_ready;
  g_CDROM_STATE.data.wait_cycles = state_->wait_cycles;
  g_CDROM_STATE.data.max_wait_cycles = state_->max_wait_cycles;
}

static
bool
cdrom_state_v2_write_msf(opera_state_writer_t *writer_,
                         msf_t const          *msf_)
{
  if(msf_ == NULL)
    return false;

  return (opera_state_write_u8(writer_,msf_->minutes) &&
          opera_state_write_u8(writer_,msf_->seconds) &&
          opera_state_write_u8(writer_,msf_->frames));
}

static
bool
cdrom_state_v2_read_msf(opera_state_reader_t *reader_,
                        msf_t                *msf_)
{
  return (opera_state_read_u8(reader_,&msf_->minutes) &&
          opera_state_read_u8(reader_,&msf_->seconds) &&
          opera_state_read_u8(reader_,&msf_->frames));
}

static
bool
cdrom_state_v2_write_toc_entry(opera_state_writer_t *writer_,
                               toc_entry_t const    *entry_)
{
  if(entry_ == NULL)
    return false;

  return (opera_state_write_u8(writer_,entry_->res0) &&
          opera_state_write_u8(writer_,entry_->CDCTL) &&
          opera_state_write_u8(writer_,entry_->track_number) &&
          opera_state_write_u8(writer_,entry_->res1) &&
          opera_state_write_u8(writer_,entry_->minutes) &&
          opera_state_write_u8(writer_,entry_->seconds) &&
          opera_state_write_u8(writer_,entry_->frames) &&
          opera_state_write_u8(writer_,entry_->res2));
}

static
bool
cdrom_state_v2_read_toc_entry(opera_state_reader_t *reader_,
                              toc_entry_t          *entry_)
{
  return (opera_state_read_u8(reader_,&entry_->res0) &&
          opera_state_read_u8(reader_,&entry_->CDCTL) &&
          opera_state_read_u8(reader_,&entry_->track_number) &&
          opera_state_read_u8(reader_,&entry_->res1) &&
          opera_state_read_u8(reader_,&entry_->minutes) &&
          opera_state_read_u8(reader_,&entry_->seconds) &&
          opera_state_read_u8(reader_,&entry_->frames) &&
          opera_state_read_u8(reader_,&entry_->res2));
}

static
bool
cdrom_state_v2_write_disc(opera_state_writer_t *writer_,
                          disc_data_t const    *disc_)
{
  uint32_t i;

  if(disc_ == NULL)
    return false;

  if(!cdrom_state_v2_write_msf(writer_,&disc_->msf_total) ||
     !cdrom_state_v2_write_msf(writer_,&disc_->msf_current) ||
     !cdrom_state_v2_write_msf(writer_,&disc_->msf_session) ||
     !opera_state_write_u8(writer_,disc_->session_valid) ||
     !opera_state_write_u8(writer_,disc_->track_first) ||
     !opera_state_write_u8(writer_,disc_->track_last) ||
     !opera_state_write_u8(writer_,disc_->disc_id))
    return false;

  for(i = 0; i < 100; i++)
    if(!cdrom_state_v2_write_toc_entry(writer_,&disc_->disc_toc[i]))
      return false;

  return true;
}

static
bool
cdrom_state_v2_read_disc(opera_state_reader_t *reader_,
                         disc_data_t          *disc_)
{
  uint32_t i;

  if(!cdrom_state_v2_read_msf(reader_,&disc_->msf_total) ||
     !cdrom_state_v2_read_msf(reader_,&disc_->msf_current) ||
     !cdrom_state_v2_read_msf(reader_,&disc_->msf_session) ||
     !opera_state_read_u8(reader_,&disc_->session_valid) ||
     !opera_state_read_u8(reader_,&disc_->track_first) ||
     !opera_state_read_u8(reader_,&disc_->track_last) ||
     !opera_state_read_u8(reader_,&disc_->disc_id))
    return false;

  for(i = 0; i < 100; i++)
    if(!cdrom_state_v2_read_toc_entry(reader_,&disc_->disc_toc[i]))
      return false;

  return true;
}

#define CDROM_STATE_V2_MODE_FIELDS(OP_) \
  OP_(u8,  block.density_code) \
  OP_(u8,  block.length_msb) \
  OP_(u8,  block.length_lsb) \
  OP_(u8,  block.flags) \
  OP_(u32, block.user_block_size) \
  OP_(u8,  error_recovery.type) \
  OP_(u8,  error_recovery.retry_count) \
  OP_(u8,  stop_time.time) \
  OP_(u8,  drive_speed.speed) \
  OP_(u8,  drive_speed.vpitch_msb) \
  OP_(u8,  drive_speed.vpitch_lsb) \
  OP_(u8,  chunk_size.size)

static
bool
cdrom_state_v2_write_mode(opera_state_writer_t          *writer_,
                          cdrom_mode_parameters_t const *mode_)
{
  if(mode_ == NULL)
    return false;

#define WRITE_MODE_FIELD(type_, field_) \
  if(!opera_state_write_##type_(writer_,mode_->field_)) \
    return false;

  CDROM_STATE_V2_MODE_FIELDS(WRITE_MODE_FIELD)
#undef WRITE_MODE_FIELD

  return true;
}

static
bool
cdrom_state_v2_read_mode(opera_state_reader_t    *reader_,
                         cdrom_mode_parameters_t *mode_)
{
  if(mode_ == NULL)
    return false;

#define READ_MODE_FIELD(type_, field_) \
  if(!opera_state_read_##type_(reader_,&mode_->field_)) \
    return false;

  CDROM_STATE_V2_MODE_FIELDS(READ_MODE_FIELD)
#undef READ_MODE_FIELD

  return true;
}

#undef CDROM_STATE_V2_MODE_FIELDS

static
bool
cdrom_state_v2_write_device(opera_state_writer_t      *writer_,
                            cdrom_device_state_v2_t const *state_)
{
  if(state_ == NULL)
    return false;

  return (opera_state_write_u8(writer_,state_->poll) &&
          opera_state_write_u8(writer_,state_->xbus_status) &&
          opera_state_write_u8(writer_,state_->status_len) &&
          opera_state_write_bytes(writer_,state_->status,256) &&
          opera_state_write_u32(writer_,state_->read_sector_size) &&
          opera_state_write_u32(writer_,state_->data_len) &&
          opera_state_write_u32(writer_,state_->data_idx) &&
          opera_state_write_bytes(writer_,state_->data,CDROM_MAX_SECTOR_SIZE) &&
          opera_state_write_u32(writer_,state_->blocks_requested) &&
          opera_state_write_bytes(writer_,state_->cmd,7) &&
          opera_state_write_u8(writer_,state_->cmd_idx) &&
          opera_state_write_i8(writer_,state_->STATCYC) &&
          opera_state_write_u32(writer_,state_->MEI_status) &&
          opera_state_write_u32(writer_,state_->current_sector) &&
          cdrom_state_v2_write_mode(writer_,&state_->mode) &&
          cdrom_state_v2_write_disc(writer_,&state_->disc));
}

static
bool
cdrom_state_v2_read_device(opera_state_reader_t      *reader_,
                           cdrom_device_state_v2_t   *state_)
{
  memset(state_,0,sizeof(*state_));

  return (opera_state_read_u8(reader_,&state_->poll) &&
          opera_state_read_u8(reader_,&state_->xbus_status) &&
          opera_state_read_u8(reader_,&state_->status_len) &&
          opera_state_read_bytes(reader_,state_->status,256) &&
          opera_state_read_u32(reader_,&state_->read_sector_size) &&
          opera_state_read_u32(reader_,&state_->data_len) &&
          opera_state_read_u32(reader_,&state_->data_idx) &&
          opera_state_read_bytes(reader_,state_->data,CDROM_MAX_SECTOR_SIZE) &&
          opera_state_read_u32(reader_,&state_->blocks_requested) &&
          opera_state_read_bytes(reader_,state_->cmd,7) &&
          opera_state_read_u8(reader_,&state_->cmd_idx) &&
          opera_state_read_i8(reader_,&state_->STATCYC) &&
          opera_state_read_u32(reader_,&state_->MEI_status) &&
          opera_state_read_u32(reader_,&state_->current_sector) &&
          cdrom_state_v2_read_mode(reader_,&state_->mode) &&
          cdrom_state_v2_read_disc(reader_,&state_->disc));
}

static
bool
cdrom_state_v2_write_data(opera_state_writer_t        *writer_,
                          cdrom_data_state_v2_t const *state_)
{
  if(state_ == NULL)
    return false;

  return (opera_state_write_u64(writer_,state_->next_ready_cycles_remaining) &&
          opera_state_write_u32(writer_,state_->sectors_queued) &&
          opera_state_write_u32(writer_,state_->sectors_drained) &&
          opera_state_write_u32(writer_,state_->empty_reads) &&
          opera_state_write_u32(writer_,state_->read_expected_lba) &&
          opera_state_write_u8(writer_,state_->read_have_expected_lba) &&
          opera_state_write_u8(writer_,state_->audio_burst_active) &&
          opera_state_write_u32(writer_,state_->audio_burst_starts) &&
          opera_state_write_u32(writer_,state_->audio_min_ahead) &&
          opera_state_write_u32(writer_,state_->audio_max_ahead) &&
          opera_state_write_u32(writer_,state_->active_lba) &&
          opera_state_write_u32(writer_,state_->active_bytes) &&
          opera_state_write_u32(writer_,state_->audio_base_lba) &&
          opera_state_write_u64(writer_,state_->audio_base_cycle_age) &&
          opera_state_write_u8(writer_,state_->audio_clock_active) &&
          opera_state_write_u8(writer_,state_->read_status_pending) &&
          opera_state_write_u32(writer_,state_->wait_ready) &&
          opera_state_write_u64(writer_,state_->wait_cycles) &&
          opera_state_write_u64(writer_,state_->max_wait_cycles));
}

static
bool
cdrom_state_v2_read_data(opera_state_reader_t  *reader_,
                         cdrom_data_state_v2_t *state_)
{
  memset(state_,0,sizeof(*state_));

  return (opera_state_read_u64(reader_,&state_->next_ready_cycles_remaining) &&
          opera_state_read_u32(reader_,&state_->sectors_queued) &&
          opera_state_read_u32(reader_,&state_->sectors_drained) &&
          opera_state_read_u32(reader_,&state_->empty_reads) &&
          opera_state_read_u32(reader_,&state_->read_expected_lba) &&
          opera_state_read_u8(reader_,&state_->read_have_expected_lba) &&
          opera_state_read_u8(reader_,&state_->audio_burst_active) &&
          opera_state_read_u32(reader_,&state_->audio_burst_starts) &&
          opera_state_read_u32(reader_,&state_->audio_min_ahead) &&
          opera_state_read_u32(reader_,&state_->audio_max_ahead) &&
          opera_state_read_u32(reader_,&state_->active_lba) &&
          opera_state_read_u32(reader_,&state_->active_bytes) &&
          opera_state_read_u32(reader_,&state_->audio_base_lba) &&
          opera_state_read_u64(reader_,&state_->audio_base_cycle_age) &&
          opera_state_read_u8(reader_,&state_->audio_clock_active) &&
          opera_state_read_u8(reader_,&state_->read_status_pending) &&
          opera_state_read_u32(reader_,&state_->wait_ready) &&
          opera_state_read_u64(reader_,&state_->wait_cycles) &&
          opera_state_read_u64(reader_,&state_->max_wait_cycles));
}

static
bool
cdrom_state_v2_write_payload(opera_state_writer_t       *writer_,
                             cdrom_device_t const       *cd_)
{
  static const char magic[4] = CDROM_STATE_V2_MAGIC;
  cdrom_device_state_v2_t device_state;
  cdrom_data_state_v2_t data_state;

  if(cd_ == NULL)
    return false;

  memset(&device_state,0,sizeof(device_state));
  memset(&data_state,0,sizeof(data_state));
  cdrom_state_v2_device_save(cd_,&device_state);
  cdrom_state_v2_data_save(&data_state);

  return (opera_state_write_bytes(writer_,magic,sizeof(magic)) &&
          opera_state_write_u32(writer_,OPERA_STATE_VERSION_V2) &&
          cdrom_state_v2_write_device(writer_,&device_state) &&
          cdrom_state_v2_write_data(writer_,&data_state) &&
          opera_state_write_u32(writer_,g_CDROM_STATE.configured_speed));
}

static
uint32_t
cdrom_state_v2_payload_size(void)
{
  static const char magic[4] = CDROM_STATE_V2_MAGIC;
  cdrom_device_state_v2_t device_state;
  cdrom_data_state_v2_t data_state;
  opera_state_writer_t writer;

  memset(&device_state,0,sizeof(device_state));
  memset(&data_state,0,sizeof(data_state));

  opera_state_writer_init(&writer,NULL,UINT32_MAX);
  opera_state_write_bytes(&writer,magic,sizeof(magic));
  opera_state_write_u32(&writer,OPERA_STATE_VERSION_V2);
  cdrom_state_v2_write_device(&writer,&device_state);
  cdrom_state_v2_write_data(&writer,&data_state);
  opera_state_write_u32(&writer,0);

  return opera_state_writer_used(&writer);
}

static
bool
cdrom_state_v2_read_payload(opera_state_reader_t  *reader_,
                            cdrom_device_state_v2_t *device_state_,
                            cdrom_data_state_v2_t   *data_state_,
                            uint32_t                *configured_speed_)
{
  static const char magic[4] = CDROM_STATE_V2_MAGIC;
  char actual_magic[4];
  uint32_t version;

  return (opera_state_read_bytes(reader_,actual_magic,sizeof(actual_magic)) &&
          !memcmp(actual_magic,magic,sizeof(magic)) &&
          opera_state_read_u32(reader_,&version) &&
          (version == OPERA_STATE_VERSION_V2) &&
          cdrom_state_v2_read_device(reader_,device_state_) &&
          cdrom_state_v2_read_data(reader_,data_state_) &&
          opera_state_read_u32(reader_,configured_speed_) &&
          cdrom_configured_speed_valid(*configured_speed_));
}

static
void
cdrom_runtime_state_reset_v1(void)
{
  char root[PATH_MAX_LENGTH];
  opera_cdrom_ode_launch_cb_t launch;
  uint32_t configured_speed;

  ode_state_str_restore(root,g_CDROM_STATE.ode.root,sizeof(root));
  launch = g_CDROM_STATE.ode.launch;
  configured_speed = g_CDROM_STATE.configured_speed;

  ode_file_close();
  memset(&g_CDROM_STATE,0,sizeof(g_CDROM_STATE));
  ode_state_str_restore(g_CDROM_STATE.ode.root,
                        root,
                        sizeof(g_CDROM_STATE.ode.root));
  g_CDROM_STATE.ode.launch = launch;
  g_CDROM_STATE.configured_speed = configured_speed;
  ode_reset_session();
  cdrom_data_reset_readahead_state();
}

uint32_t
opera_cdrom_state_size(void)
{
  if(g_CDROM_STATE.ode.savestate_blocked)
    return 0;

  return opera_state_chunk_size(cdrom_state_v2_payload_size());
}

uint32_t
opera_cdrom_state_save(cdrom_device_t *cd_,
                       void           *data_)
{
  uint32_t payload_size;
  opera_state_writer_t writer;

  if(g_CDROM_STATE.ode.savestate_blocked)
    return 0;

  payload_size = cdrom_state_v2_payload_size();
  opera_state_writer_init(&writer,data_,opera_state_chunk_size(payload_size));
  opera_state_write_chunk_header(&writer,"CDRM",payload_size);
  cdrom_state_v2_write_payload(&writer,cd_);

  return opera_state_writer_ok(&writer) ? opera_state_writer_used(&writer) : 0;
}

static
uint32_t
opera_cdrom_state_load_v1(cdrom_device_t *cd_,
                          void const     *data_,
                          uint32_t        data_size_)
{
  uint32_t rv;

  rv = opera_state_load_sized(cd_,
                              "CDRM",
                              data_,
                              data_size_,
                              sizeof(*cd_));
  if(rv == 0)
    return 0;

  cdrom_runtime_state_reset_v1();
  opera_cdrom_restore_callbacks(cd_);

  return rv;
}

static
uint32_t
opera_cdrom_state_load_v1_legacy(cdrom_device_t *cd_,
                                 void const     *data_,
                                 uint32_t        data_size_)
{
  cdrom_device_state_v1_legacy_t state;
  uint32_t rv;

  rv = opera_state_load_sized(&state,
                              "CDRM",
                              data_,
                              data_size_,
                              sizeof(state));
  if(rv == 0)
    return 0;

  cdrom_runtime_state_reset_v1();
  if(!cdrom_state_v1_legacy_device_load(cd_,&state))
    return 0;

  return rv;
}

static
uint32_t
opera_cdrom_state_load_v2(cdrom_device_t *cd_,
                          void const     *data_,
                          uint32_t        data_size_)
{
  cdrom_device_state_v2_t device_state;
  cdrom_data_state_v2_t data_state;
  opera_cdrom_ode_launch_cb_t launch;
  char root[PATH_MAX_LENGTH];
  opera_state_reader_t reader;
  opera_state_reader_t payload;
  uint32_t configured_speed;
  uint32_t rv;

  opera_state_reader_init(&reader,data_,data_size_);
  if(!opera_state_read_chunk(&reader,"CDRM",&payload) ||
     !cdrom_state_v2_read_payload(&payload,
                                  &device_state,
                                  &data_state,
                                  &configured_speed) ||
     !opera_state_reader_finished(&payload))
    return 0;
  rv = opera_state_reader_used(&reader);

  launch = g_CDROM_STATE.ode.launch;
  ode_state_str_restore(root,
                        g_CDROM_STATE.ode.root,
                        sizeof(root));
  ode_file_close();
  memset(&g_CDROM_STATE,0,sizeof(g_CDROM_STATE));
  ode_state_str_restore(g_CDROM_STATE.ode.root,
                        root,
                        sizeof(g_CDROM_STATE.ode.root));
  g_CDROM_STATE.ode.launch = launch;
  ode_reset_session();

  if(!cdrom_state_v2_device_load(cd_,&device_state))
    return 0;

  g_CDROM_STATE.configured_speed = configured_speed;
  cdrom_state_v2_data_load(&data_state);

  opera_cdrom_restore_callbacks(cd_);

  return rv;
}

uint32_t
opera_cdrom_state_load(cdrom_device_t *cd_,
                       void const     *data_,
                       uint32_t        data_size_)
{
  uint32_t data_size;
  uint32_t chunk_size;

  chunk_size = opera_state_get_chunk_data_size(data_,
                                               data_size_,
                                               "CDRM",
                                               &data_size);
  if(chunk_size == 0)
    return 0;

  if(data_size == cdrom_state_v2_payload_size())
    {
      uint32_t rv;

      rv = opera_cdrom_state_load_v2(cd_,data_,chunk_size);
      if(rv != 0)
        return rv;
    }

  if(data_size == sizeof(*cd_))
    return opera_cdrom_state_load_v1(cd_,data_,chunk_size);

  if(data_size == sizeof(cdrom_device_state_v1_legacy_t))
    return opera_cdrom_state_load_v1_legacy(cd_,data_,chunk_size);

  return 0;
}

void
opera_cdrom_restore_callbacks(cdrom_device_t *cd_)
{
  cd_->callbacks = g_CDROM_CALLBACKS;
  if(cd_->callbacks.set_sector != NULL)
    cd_->callbacks.set_sector(cd_->current_sector);
}

void
opera_cdrom_init(cdrom_device_t *cd_)
{
  char pending_launch_path[PATH_MAX_LENGTH];
  bool preserve_pending_launch;
  bool media_access_after_reset;

  pending_launch_path[0] = 0;
  preserve_pending_launch = ((g_CDROM_STATE.ode.restart_armed || g_CDROM_STATE.ode.restart_requested) &&
                             (g_CDROM_STATE.ode.pending_launch_path[0] != 0));
  media_access_after_reset = g_CDROM_STATE.ode.media_access_after_reset;
  if(preserve_pending_launch)
    ode_str_copy(pending_launch_path,
                 g_CDROM_STATE.ode.pending_launch_path,
                 sizeof(pending_launch_path));

  if(!cdrom_persona_is_portfolio_native_mei())
    opera_log_printf(OPERA_LOG_WARN,
                     "[Opera]: CDROM persona \"%s\" is not the Portfolio native MEI persona; identity, status tags, chunks, subcode, DMA, and old-status behavior must be updated together\n",
                     g_CDROM_PERSONA.name);

  memset(cd_,0,sizeof(*cd_));
  cd_->current_sector = 0;
  opera_cdrom_restore_callbacks(cd_);

  cd_->xbus_status  = 0;
  cd_->poll         = CDROM_POLL_INTEN_MASK;
  cd_->xbus_status |= CDROM_STATUS_READY;
  if(cd_->callbacks.get_size() > 0)
    cd_->xbus_status |= (CDROM_STATUS_DOOR |
                         CDROM_STATUS_DISC_IN |
                         CDROM_STATUS_SPIN_UP);

  cdrom_mode_reset(cd_);
  cd_->MEI_status = MEI_CDROM_no_error;

  opera_cdrom_update_disc_layout(cd_);
  ode_reset_session();
  if(preserve_pending_launch)
    {
      ode_str_copy(g_CDROM_STATE.ode.pending_launch_path,
                   pending_launch_path,
                   sizeof(g_CDROM_STATE.ode.pending_launch_path));
      g_CDROM_STATE.ode.restart_requested = true;
    }
  if(media_access_after_reset)
    cdrom_media_access_latch(cd_);
  cdrom_data_reset_readahead_state();

  cd_->STATCYC = STATDELAY;
}

uint8_t
opera_cdrom_fifo_get_status(cdrom_device_t *cd_)
{
  uint8_t rv;

  cdrom_data_load_next_if_ready(cd_);

  rv = 0;
  if(cd_->status_len > 0)
    {
      rv = cd_->status[0];
      cd_->status_len--;
      if(cd_->status_len > 0)
        memmove(cd_->status,cd_->status + 1,cd_->status_len);
      else
        {
          cd_->poll &= ~POLST;
          /* ODE media switches after the launch command response is drained. */
          ode_complete_restart_if_armed(cd_);
        }
    }

  return rv;
}

static
uint32_t
ode_cmd_be32(const uint8_t *cmd_)
{
  return (((uint32_t)cmd_[1]) << 24) |
    (((uint32_t)cmd_[2]) << 16) |
    (((uint32_t)cmd_[3]) << 8)  |
    (((uint32_t)cmd_[4]));
}

static
uint16_t
ode_cmd_be16_at(const uint8_t *cmd_,
                int            idx_)
{
  return (uint16_t)((((uint16_t)cmd_[idx_]) << 8) |
                    (((uint16_t)cmd_[idx_ + 1])));
}

static
void
cdrom_store_be16(uint8_t *dst_,
                 uint16_t value_)
{
  dst_[0] = (uint8_t)(value_ >> 8);
  dst_[1] = (uint8_t)value_;
}

static
void
cdrom_persona_read_id_payload(uint8_t *payload_)
{
  cdrom_store_be16(&payload_[CDROM_READ_ID_PAYLOAD_MANUFACTURER_ID],
                   g_CDROM_PERSONA.manufacturer_id);
  cdrom_store_be16(&payload_[CDROM_READ_ID_PAYLOAD_MANUFACTURER_DEVICE],
                   g_CDROM_PERSONA.manufacturer_device);
  cdrom_store_be16(&payload_[CDROM_READ_ID_PAYLOAD_MANUFACTURER_REVISION],
                   g_CDROM_PERSONA.manufacturer_revision);
  cdrom_store_be16(&payload_[CDROM_READ_ID_PAYLOAD_MANUFACTURER_FLAGS],
                   g_CDROM_PERSONA.manufacturer_flags);
  cdrom_store_be16(&payload_[CDROM_READ_ID_PAYLOAD_DRIVER_TAG_TABLE_WORDS],
                   g_CDROM_PERSONA.driver_tag_table_words);
}

static
bool
cdrom_persona_is_portfolio_native_mei(void)
{
  return ((g_CDROM_PERSONA.manufacturer_id == CDROM_PORTFOLIO_MFG_MEI) &&
          (g_CDROM_PERSONA.manufacturer_device == CDROM_PORTFOLIO_DEVICE_CDROM) &&
          (g_CDROM_PERSONA.status_layout == CDROM_STATUS_LAYOUT_NATIVE_TAGGED) &&
          g_CDROM_PERSONA.sends_tag &&
          g_CDROM_PERSONA.supports_chunks &&
          g_CDROM_PERSONA.supports_subcode &&
          !(g_CDROM_PERSONA.manufacturer_flags & CDROM_READ_ID_FLAG_DEVHAS_DRIVER) &&
          (g_CDROM_PERSONA.driver_tag_table_words ==
           CDROM_READ_ID_DRIVER_TAG_TABLE_WORDS_NONE) &&
          !g_CDROM_PERSONA.slow_dma &&
          !g_CDROM_PERSONA.emulate_media_access &&
          !g_CDROM_PERSONA.old_status);
}

static
void
cdrom_status_tagged(cdrom_device_t *cd_,
                    uint8_t         opcode_,
                    const uint8_t  *payload_,
                    uint8_t         payload_len_,
                    uint32_t        mei_status_)
{
  cd_->status_len = (uint8_t)(payload_len_ + 2);
  cd_->status[0]  = opcode_;
  if((payload_ != NULL) && (payload_len_ > 0))
    memcpy(&cd_->status[1],payload_,payload_len_);
  cd_->status[payload_len_ + 1] = cd_->xbus_status;
  cd_->MEI_status = mei_status_;
  cd_->poll      |= POLST;
}

static
void
cdrom_status_tagged_padded(cdrom_device_t *cd_,
                           uint8_t         opcode_,
                           uint8_t         total_len_,
                           uint32_t        mei_status_)
{
  if(total_len_ < 2)
    total_len_ = 2;

  memset(cd_->status,0,total_len_);
  cd_->status_len             = total_len_;
  cd_->status[0]              = opcode_;
  cd_->status[total_len_ - 1] = cd_->xbus_status;
  cd_->MEI_status             = mei_status_;
  cd_->poll                  |= POLST;
}

static
void
cdrom_status_tagged_error(cdrom_device_t *cd_,
                          uint8_t         opcode_,
                          uint32_t        mei_status_)
{
  cd_->xbus_status |= CDROM_STATUS_ERROR;
  cd_->xbus_status &= ~CDROM_STATUS_READY;
  cdrom_status_tagged(cd_,opcode_,NULL,0,mei_status_);
}

typedef enum cdrom_media_requirement_e
  {
    CDROM_MEDIA_NO_SPIN_REQUIRED = 0,
    CDROM_MEDIA_SPIN_REQUIRED    = 1
  } cdrom_media_requirement_t;

static
uint32_t
cdrom_media_error(const cdrom_device_t *cd_,
                  cdrom_media_requirement_t requirement_)
{
  if(!(cd_->xbus_status & CDROM_STATUS_DOOR) ||
     !(cd_->xbus_status & CDROM_STATUS_DISC_IN))
    return MEI_CDROM_disc_out;

  if((requirement_ == CDROM_MEDIA_SPIN_REQUIRED) &&
     !(cd_->xbus_status & CDROM_STATUS_SPIN_UP))
    return MEI_CDROM_not_ready;

  return MEI_CDROM_no_error;
}

static
bool
cdrom_media_ready(const cdrom_device_t *cd_,
                  cdrom_media_requirement_t requirement_)
{
  return (cdrom_media_error(cd_,requirement_) == MEI_CDROM_no_error);
}

static
void
cdrom_status_media_error(cdrom_device_t *cd_,
                         uint8_t         opcode_,
                         cdrom_media_requirement_t requirement_)
{
  uint32_t mei_status;

  mei_status = cdrom_media_error(cd_,requirement_);
  if(mei_status == MEI_CDROM_no_error)
    mei_status = MEI_CDROM_not_ready;

  cdrom_status_tagged_error(cd_,opcode_,mei_status);
}

static
void
ode_status(cdrom_device_t *cd_,
           uint8_t         opcode_,
           const uint8_t  *payload_,
           uint8_t         payload_len_)
{
  cd_->xbus_status |= CDROM_STATUS_READY;
  cdrom_status_tagged(cd_,opcode_,payload_,payload_len_,MEI_CDROM_no_error);
}

static
void
ode_data_response(cdrom_device_t *cd_,
                  uint8_t         opcode_,
                  const uint8_t  *data_)
{
  memcpy(cd_->data,data_,REQSIZE);
  cd_->data_idx          = 0;
  cd_->data_len          = REQSIZE;
  cd_->blocks_requested  = 0;
  ode_status(cd_,opcode_,NULL,0);
  cd_->poll |= POLDT;
}

static
void
ode_change_toc(cdrom_device_t *cd_)
{
  uint32_t toc_id;
  char path[PATH_MAX_LENGTH];
  bool is_dir;

  toc_id = ode_cmd_be32(cd_->cmd);
  if(toc_id == 0)
    {
      ode_str_copy(g_CDROM_STATE.ode.current,g_CDROM_STATE.ode.root,sizeof(g_CDROM_STATE.ode.current));
    }
  else if((toc_id & ODE_TOC_ENTRY_MASK) == ODE_TOC_ENTRY_MASK)
    {
      ode_go_parent();
    }
  else if(ode_find_toc_path(toc_id,path,sizeof(path),&is_dir) && is_dir)
    {
      ode_str_copy(g_CDROM_STATE.ode.current,path,sizeof(g_CDROM_STATE.ode.current));
    }

  ode_status(cd_,cd_->cmd[0],NULL,0);
}

static
void
ode_read_toc_window(cdrom_device_t *cd_)
{
  uint32_t toc_id;
  uint16_t offset;
  uint8_t payload[16];

  toc_id = ode_cmd_be32(cd_->cmd);
  offset = ode_cmd_be16_at(cd_->cmd,5);

  if(offset == 0)
    ode_build_toc(toc_id,0);

  memset(payload,0xFF,sizeof(payload));
  if(offset < REQSIZE)
    {
      uint16_t copy_len = (uint16_t)(REQSIZE - offset);
      if(copy_len > sizeof(payload))
        copy_len = sizeof(payload);
      memcpy(payload,&g_CDROM_STATE.ode.toc[offset],copy_len);
    }

  ode_status(cd_,cd_->cmd[0],payload,sizeof(payload));
}

static
void
ode_read_toc_list(cdrom_device_t *cd_)
{
  uint32_t toc_id;
  uint16_t count;

  toc_id = ode_cmd_be32(cd_->cmd);
  count  = ode_cmd_be16_at(cd_->cmd,5);

  ode_build_toc(toc_id,count);
  ode_data_response(cd_,cd_->cmd[0],g_CDROM_STATE.ode.toc);
}

static
void
ode_add_playlist(cdrom_device_t *cd_)
{
  uint32_t toc_id;
  uint8_t payload[2];
  char path[PATH_MAX_LENGTH];
  bool is_dir;

  toc_id = ode_cmd_be32(cd_->cmd);
  payload[0] = 0;
  payload[1] = 0;

  if(ode_find_toc_path(toc_id,path,sizeof(path),&is_dir) && !is_dir)
    {
      payload[0] = 1;
      if(g_CDROM_STATE.ode.playlist_count < ODE_PLAYLIST_MAX)
        {
          ode_str_copy(g_CDROM_STATE.ode.playlist[g_CDROM_STATE.ode.playlist_count],
                       path,
                       sizeof(g_CDROM_STATE.ode.playlist[g_CDROM_STATE.ode.playlist_count]));
          g_CDROM_STATE.ode.playlist_count++;
          payload[1] = 1;
        }
    }

  ode_status(cd_,cd_->cmd[0],payload,sizeof(payload));
}

static
void
ode_launch_playlist(cdrom_device_t *cd_)
{
  cdrom_data_clear_transfer(cd_);
  g_CDROM_STATE.ode.restart_armed = false;
  g_CDROM_STATE.ode.pending_launch_path[0] = 0;

  if((g_CDROM_STATE.ode.playlist_count > 0) && (g_CDROM_STATE.ode.launch != NULL))
    {
      /* Simulate disc eject before the 0xC5 response. */
      cd_->xbus_status &= ~CDROM_STATUS_DOOR;
      cd_->xbus_status &= ~CDROM_STATUS_DISC_IN;
      cd_->xbus_status &= ~CDROM_STATUS_SPIN_UP;
      cd_->xbus_status &= ~CDROM_STATUS_DOUBLE_SPEED;
      cd_->xbus_status &= ~CDROM_STATUS_ERROR;
      cdrom_media_access_latch(cd_);
      ode_str_copy(g_CDROM_STATE.ode.pending_launch_path,
                   g_CDROM_STATE.ode.playlist[0],
                   sizeof(g_CDROM_STATE.ode.pending_launch_path));
      g_CDROM_STATE.ode.restart_armed = true;
    }

  ode_status(cd_,cd_->cmd[0],NULL,0);
}

static
void
ode_create_file(cdrom_device_t *cd_)
{
  uint16_t name_len;
  uint8_t payload[1];
  char path[PATH_MAX_LENGTH];

  payload[0] = 0;
  name_len = ode_cmd_be16_at(cd_->cmd,1);

  ode_file_close();
  if(ode_file_buffer_name_path(name_len,path,sizeof(path)))
    {
      g_CDROM_STATE.ode.file = filestream_open(path,
                                               RETRO_VFS_FILE_ACCESS_READ_WRITE,
                                               RETRO_VFS_FILE_ACCESS_HINT_NONE);
      payload[0] = (g_CDROM_STATE.ode.file != NULL);
      if(g_CDROM_STATE.ode.file != NULL)
        {
          ode_str_copy(g_CDROM_STATE.ode.file_path,path,sizeof(g_CDROM_STATE.ode.file_path));
          g_CDROM_STATE.ode.file_mode = 3; /* w+b */
        }
    }

  ode_file_buffer_write_reset();
  ode_status(cd_,cd_->cmd[0],payload,sizeof(payload));
}

static
void
ode_open_file(cdrom_device_t *cd_)
{
  uint8_t payload[1];
  char path[PATH_MAX_LENGTH];
  bool write;

  payload[0] = 0;
  write = (cd_->cmd[6] != 0);

  ode_file_close();
  path[0] = 0;

  if(cd_->cmd[1] == 0)
    {
      uint16_t name_len;

      name_len = ode_cmd_be16_at(cd_->cmd,2);
      ode_file_buffer_name_path(name_len,path,sizeof(path));
    }
  else
    {
      uint32_t toc_id;
      bool is_dir;

      toc_id = (((uint32_t)cd_->cmd[2]) << 24) |
        (((uint32_t)cd_->cmd[3]) << 16) |
        (((uint32_t)cd_->cmd[4]) << 8)  |
        (((uint32_t)cd_->cmd[5]));
      if(!ode_find_toc_path(toc_id,path,sizeof(path),&is_dir) || is_dir)
        path[0] = 0;
    }

  if((path[0] != 0) && ode_path_is_under_root(path))
    {
      g_CDROM_STATE.ode.file = filestream_open(path,
                                               write ?
                                               (RETRO_VFS_FILE_ACCESS_READ_WRITE |
                                                RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING) :
                                               RETRO_VFS_FILE_ACCESS_READ,
                                               RETRO_VFS_FILE_ACCESS_HINT_NONE);
      if((g_CDROM_STATE.ode.file == NULL) && write)
        g_CDROM_STATE.ode.file = filestream_open(path,
                                                 RETRO_VFS_FILE_ACCESS_READ_WRITE,
                                                 RETRO_VFS_FILE_ACCESS_HINT_NONE);
      payload[0] = (g_CDROM_STATE.ode.file != NULL);
      if(g_CDROM_STATE.ode.file != NULL)
        {
          ode_str_copy(g_CDROM_STATE.ode.file_path,path,sizeof(g_CDROM_STATE.ode.file_path));
          g_CDROM_STATE.ode.file_mode = write ? 2 : 1; /* 2=r+b, 1=rb */
        }
    }

  ode_file_buffer_write_reset();
  ode_status(cd_,cd_->cmd[0],payload,sizeof(payload));
}

static
void
ode_seek_file(cdrom_device_t *cd_)
{
  uint32_t offset;
  bool success;

  offset = ode_cmd_be32(cd_->cmd);
  success = false;

  if(g_CDROM_STATE.ode.file != NULL)
    success = (filestream_seek(g_CDROM_STATE.ode.file,(int64_t)offset,SEEK_SET) >= 0);

  if(!success)
    cd_->xbus_status |= CDROM_STATUS_ERROR;

  ode_status(cd_,cd_->cmd[0],NULL,0);
}

static
void
ode_read_file(cdrom_device_t *cd_)
{
  uint16_t len;
  uint16_t actual;
  uint8_t payload[2];

  len = ode_cmd_be16_at(cd_->cmd,1);
  if(len > REQSIZE)
    len = REQSIZE;

  actual = 0;
  if(g_CDROM_STATE.ode.file != NULL)
    {
      int64_t result;

      result = filestream_read(g_CDROM_STATE.ode.file,g_CDROM_STATE.ode.file_buffer,(int64_t)len);
      if(result < 0)
        cd_->xbus_status |= CDROM_STATUS_ERROR;
      else
        actual = (uint16_t)result;
    }

  ode_file_buffer_write_reset();
  payload[0] = (uint8_t)(actual >> 8);
  payload[1] = (uint8_t)actual;
  ode_status(cd_,cd_->cmd[0],payload,sizeof(payload));
}

static
void
ode_write_file(cdrom_device_t *cd_)
{
  uint16_t len;
  uint16_t actual;
  uint8_t payload[2];

  len = ode_cmd_be16_at(cd_->cmd,1);
  if(len > REQSIZE)
    len = REQSIZE;

  actual = 0;
  if(g_CDROM_STATE.ode.file != NULL)
    {
      int64_t result;

      result = filestream_write(g_CDROM_STATE.ode.file,g_CDROM_STATE.ode.file_buffer,(int64_t)len);
      if(result < 0)
        cd_->xbus_status |= CDROM_STATUS_ERROR;
      else
        actual = (uint16_t)result;
    }

  ode_file_buffer_write_reset();
  payload[0] = (uint8_t)(actual >> 8);
  payload[1] = (uint8_t)actual;
  ode_status(cd_,cd_->cmd[0],payload,sizeof(payload));
}

static
void
ode_write_buffer_word(cdrom_device_t *cd_)
{
  uint16_t word_offset;
  uint32_t offset;

  word_offset = ode_cmd_be16_at(cd_->cmd,1);
  offset = ((uint32_t)word_offset << 2);
  if((offset + 4) <= REQSIZE)
    memcpy(&g_CDROM_STATE.ode.file_buffer[offset],&cd_->cmd[3],4);

  ode_file_buffer_write_reset();
  ode_status(cd_,cd_->cmd[0],NULL,0);
}

static
void
ode_fast_buffer_send(cdrom_device_t *cd_)
{
  if(g_CDROM_STATE.ode.file_buffer_write_overflow)
    cd_->xbus_status |= CDROM_STATUS_ERROR;
  else if(g_CDROM_STATE.ode.file_buffer_write_idx > 0)
    {
      uint32_t len;

      len = g_CDROM_STATE.ode.file_buffer_write_idx;
      if(len > REQSIZE)
        len = REQSIZE;
      memcpy(g_CDROM_STATE.ode.file_buffer,g_CDROM_STATE.ode.file_buffer_write_data,len);
    }

  ode_file_buffer_write_reset();
  ode_status(cd_,cd_->cmd[0],NULL,0);
}

static
void
ode_read_file_buffer(cdrom_device_t *cd_)
{
  ode_file_buffer_write_reset();
  ode_data_response(cd_,cd_->cmd[0],g_CDROM_STATE.ode.file_buffer);
}

static
void
ode_extended_id(cdrom_device_t *cd_)
{
  static const uint8_t payload[10] =
    {
      ODE_BOOT_MICROSD,
      'L','O','C','O','D','E',
      1,1,0
    };

  ode_status(cd_,cd_->cmd[0],payload,sizeof(payload));
}

static
void
cdrom_cmd_seek(cdrom_device_t *cd_)
{
  cdrom_status_tagged_error(cd_,CDROM_CMD_SEEK,MEI_CDROM_illegal_request);
}

static
void
cdrom_cmd_spin_up(cdrom_device_t *cd_)
{
  if(cdrom_media_ready(cd_,CDROM_MEDIA_NO_SPIN_REQUIRED))
    {
      cd_->xbus_status |= CDROM_STATUS_SPIN_UP;
      cd_->xbus_status |= CDROM_STATUS_READY;
      cdrom_status_tagged(cd_,CDROM_CMD_SPIN_UP,NULL,0,MEI_CDROM_no_error);
    }
  else
    {
      cdrom_status_media_error(cd_,CDROM_CMD_SPIN_UP,CDROM_MEDIA_NO_SPIN_REQUIRED);
    }
}

static
void
cdrom_cmd_spin_down(cdrom_device_t *cd_)
{
  if(cdrom_media_ready(cd_,CDROM_MEDIA_NO_SPIN_REQUIRED))
    {
      cd_->xbus_status &= ~CDROM_STATUS_SPIN_UP;
      cd_->xbus_status |= CDROM_STATUS_READY;
      cdrom_status_tagged(cd_,CDROM_CMD_SPIN_DOWN,NULL,0,MEI_CDROM_no_error);
    }
  else
    {
      cdrom_status_media_error(cd_,CDROM_CMD_SPIN_DOWN,CDROM_MEDIA_NO_SPIN_REQUIRED);
    }
}

static
void
cdrom_cmd_diagnostics(cdrom_device_t *cd_)
{
  cdrom_status_tagged_error(cd_,CDROM_CMD_DIAGNOSTICS,MEI_CDROM_illegal_request);
}

static
void
cdrom_cmd_eject_disc(cdrom_device_t *cd_)
{
  cdrom_data_clear_transfer(cd_);
  cd_->xbus_status |= CDROM_STATUS_READY;
  cd_->xbus_status &= ~CDROM_STATUS_DOOR;
  cd_->xbus_status &= ~CDROM_STATUS_DISC_IN;
  cd_->xbus_status &= ~CDROM_STATUS_SPIN_UP;
  cd_->xbus_status &= ~CDROM_STATUS_DOUBLE_SPEED;
  cd_->xbus_status &= ~CDROM_STATUS_ERROR;
  cd_->MEI_status   = MEI_CDROM_no_error;

  cd_->status_len = 2;
  cd_->status[0]  = CDROM_CMD_EJECT_DISC;
  cd_->status[1]  = cd_->xbus_status;

  cd_->poll |= POLST;
  cdrom_media_access_latch(cd_);
}

static
void
cdrom_cmd_inject_disc(cdrom_device_t *cd_)
{
  cdrom_data_clear_transfer(cd_);
  cd_->xbus_status |= (CDROM_STATUS_READY |
                       CDROM_STATUS_DOOR |
                       CDROM_STATUS_DISC_IN |
                       CDROM_STATUS_SPIN_UP);
  cd_->xbus_status &= ~CDROM_STATUS_ERROR;
  cdrom_mode_apply_drive_speed(cd_);
  cd_->MEI_status = MEI_CDROM_no_error;
  opera_cdrom_update_disc_layout(cd_);

  cd_->status_len = 2;
  cd_->status[0]  = CDROM_CMD_INJECT_DISC;
  cd_->status[1]  = cd_->xbus_status;

  cd_->poll |= POLST;
  cdrom_media_access_latch(cd_);
}

static
void
cdrom_cmd_abort(cdrom_device_t *cd_)
{
  cdrom_data_clear_transfer(cd_);
  cd_->xbus_status |= CDROM_STATUS_READY;
  cd_->xbus_status &= ~CDROM_STATUS_ERROR;
  cdrom_status_tagged_padded(cd_,
                             CDROM_CMD_ABORT,
                             CDROM_ABORT_FLUSH_STATUS_LEN,
                             MEI_CDROM_no_error);
}

static
void
cdrom_cmd_mode_set(cdrom_device_t *cd_)
{
  const bool preserve_transfer = cdrom_mode_set_preserves_transfer(cd_);

  if(!preserve_transfer)
    cdrom_data_clear_transfer(cd_);

  if(cdrom_mode_set_page(cd_))
    {
      if(preserve_transfer)
        cdrom_data_preserve_drive_speed_mode_set(cd_);

      cd_->xbus_status |= CDROM_STATUS_READY;
      cdrom_status_tagged(cd_,CDROM_CMD_MODE_SET,NULL,0,MEI_CDROM_no_error);
    }
  else
    {
      cdrom_status_tagged_error(cd_,CDROM_CMD_MODE_SET,MEI_CDROM_mode_error);
    }
}

static
void
cdrom_cmd_reset(cdrom_device_t *cd_)
{
  cdrom_data_clear_transfer(cd_);
  cdrom_mode_reset(cd_);
  cd_->xbus_status |= CDROM_STATUS_READY;
  cd_->xbus_status &= ~CDROM_STATUS_ERROR;
  cd_->MEI_status = MEI_CDROM_no_error;
  cd_->status_len = 2;
  cd_->status[0] = CDROM_CMD_RESET;
  cd_->status[1] = cd_->xbus_status;
  cd_->poll |= POLST;
}

static
void
cdrom_cmd_flush(cdrom_device_t *cd_)
{
  cdrom_data_clear_transfer(cd_);
  cd_->xbus_status |= CDROM_STATUS_READY;
  cd_->xbus_status &= ~CDROM_STATUS_ERROR;
  cdrom_status_tagged_padded(cd_,
                             CDROM_CMD_FLUSH,
                             CDROM_ABORT_FLUSH_STATUS_LEN,
                             MEI_CDROM_no_error);
}

/*
  Normal 3DO software, including the CD player, does not use the drive's
  PLAY_AUDIO/CD-DA playback path; it issues READ_DATA requests for audio
  sectors and sends the decoded stream to DSPP itself. The emulator only
  tracks command state, play position, and Sub-Q position.
*/
static
void
cdrom_cmd_play_audio_msf(cdrom_device_t *cd_)
{
  msf_t start_msf;
  msf_t end_msf;
  uint32_t start_lba;
  uint32_t end_lba;

  start_msf.minutes = cd_->cmd[1];
  start_msf.seconds = cd_->cmd[2];
  start_msf.frames  = cd_->cmd[3];
  end_msf.minutes   = cd_->cmd[4];
  end_msf.seconds   = cd_->cmd[5];
  end_msf.frames    = cd_->cmd[6];

  start_lba = MSF2LBA(&start_msf);
  end_lba   = MSF2LBA(&end_msf);

  if((end_msf.minutes == 0xFF) && (end_msf.seconds == 0xFF) && (end_msf.frames == 0xFF))
    end_lba = cd_->callbacks.get_size();

  if((start_msf.minutes == 0) && (start_msf.seconds == 0) && (start_msf.frames == 0) &&
     (end_msf.minutes == 0) && (end_msf.seconds == 0) && (end_msf.frames == 0))
    {
      if((cd_->xbus_status & CDROM_STATUS_DOOR) &&
         (cd_->xbus_status & CDROM_STATUS_DISC_IN) &&
         (cd_->xbus_status & CDROM_STATUS_SPIN_UP))
        {
          cd_->xbus_status |= CDROM_STATUS_READY;
          cd_->MEI_status   = MEI_CDROM_no_error;
        }
      else
        {
          cdrom_status_media_error(cd_,CDROM_CMD_PLAY_AUDIO_MSF,CDROM_MEDIA_SPIN_REQUIRED);
        }

      cdrom_data_audio_clock_stop(cd_);

      if(!(cd_->poll & POLST))
        cdrom_status_tagged(cd_,CDROM_CMD_PLAY_AUDIO_MSF,NULL,0,cd_->MEI_status);
      return;
    }

  if((cd_->xbus_status & CDROM_STATUS_DOOR) &&
     (cd_->xbus_status & CDROM_STATUS_DISC_IN) &&
     (cd_->xbus_status & CDROM_STATUS_SPIN_UP))
    {
      cd_->xbus_status |= CDROM_STATUS_READY;
      cd_->MEI_status   = MEI_CDROM_no_error;
      cdrom_data_audio_clock_start(cd_,start_lba,end_lba);
    }
  else
    {
      cdrom_status_media_error(cd_,CDROM_CMD_PLAY_AUDIO_MSF,CDROM_MEDIA_SPIN_REQUIRED);

      opera_log_printf(OPERA_LOG_WARN,
                       "[Opera]: CDROM play MSF rejected status=%02x\n",
                       cd_->xbus_status);
    }

  if(!(cd_->poll & POLST))
    cdrom_status_tagged(cd_,CDROM_CMD_PLAY_AUDIO_MSF,NULL,0,cd_->MEI_status);
}

static
void
cdrom_cmd_play_audio_track(cdrom_device_t *cd_)
{
  uint8_t start_track;
  uint8_t end_track;
  uint32_t start_lba;
  uint32_t end_lba;

  start_track = cd_->cmd[1];
  end_track   = cd_->cmd[3];
  start_lba   = 0;
  end_lba     = 0;

  if(start_track > 0 && start_track <= cd_->disc.track_last)
    {
      toc_entry_t *toc = &cd_->disc.disc_toc[start_track];
      msf_t msf;
      msf.minutes = toc->minutes;
      msf.seconds = toc->seconds;
      msf.frames  = toc->frames;
      start_lba   = MSF2LBA(&msf);
    }

  if(end_track > 0 && end_track < cd_->disc.track_last)
    {
      toc_entry_t *toc = &cd_->disc.disc_toc[end_track + 1];
      msf_t msf;
      msf.minutes = toc->minutes;
      msf.seconds = toc->seconds;
      msf.frames  = toc->frames;
      end_lba     = MSF2LBA(&msf);
    }
  else
    {
      end_lba = cd_->callbacks.get_size();
    }

  if((cd_->xbus_status & CDROM_STATUS_DOOR) &&
     (cd_->xbus_status & CDROM_STATUS_DISC_IN) &&
     (cd_->xbus_status & CDROM_STATUS_SPIN_UP))
    {
      cd_->xbus_status |= CDROM_STATUS_READY;
      cd_->MEI_status   = MEI_CDROM_no_error;
      cdrom_data_audio_clock_start(cd_,start_lba,end_lba);
    }
  else
    {
      cdrom_status_media_error(cd_,CDROM_CMD_PLAY_AUDIO_TRACK,CDROM_MEDIA_SPIN_REQUIRED);

      opera_log_printf(OPERA_LOG_WARN,
                       "[Opera]: CDROM play track rejected status=%02x\n",
                       cd_->xbus_status);
    }

  if(!(cd_->poll & POLST))
    cdrom_status_tagged(cd_,CDROM_CMD_PLAY_AUDIO_TRACK,NULL,0,cd_->MEI_status);
}

static
void
cdrom_cmd_read_data(cdrom_device_t *cd_)
{
  bool continuous_read;
  bool reset_readahead;
  bool had_expected_lba;
  uint32_t expected_lba;
  uint32_t disc_blocks;

  if(!((cd_->xbus_status & CDROM_STATUS_DOOR) &&
       (cd_->xbus_status & CDROM_STATUS_DISC_IN) &&
       (cd_->xbus_status & CDROM_STATUS_SPIN_UP)))
    {
      cdrom_status_media_error(cd_,CDROM_CMD_READ_DATA,CDROM_MEDIA_SPIN_REQUIRED);
      return;
    }

  cd_->xbus_status |= CDROM_STATUS_READY;
  cd_->status[0]    = CDROM_CMD_READ_DATA;
  cd_->status[1]    = cd_->xbus_status;
  cd_->status_len   = 2;

  cd_->disc.msf_current.minutes = cd_->cmd[1];
  cd_->disc.msf_current.seconds = cd_->cmd[2];
  cd_->disc.msf_current.frames  = cd_->cmd[3];
  if(!cdrom_msf_valid(&cd_->disc.msf_current))
    {
      cdrom_data_clear_transfer(cd_);
      cdrom_status_tagged_error(cd_,CDROM_CMD_READ_DATA,MEI_CDROM_address_error);
      opera_log_printf(OPERA_LOG_WARN,
                       "[Opera]: CDROM read rejected invalid MSF=%02u:%02u:%02u\n",
                       cd_->cmd[1],
                       cd_->cmd[2],
                       cd_->cmd[3]);
      return;
    }

  cd_->current_sector = MSF2LBA(&cd_->disc.msf_current);
  cd_->blocks_requested =
    (((uint32_t)cd_->cmd[4]) << 16) |
    (((uint32_t)cd_->cmd[5]) << 8)  |
    ((uint32_t)cd_->cmd[6]);
  disc_blocks = cd_->callbacks.get_size ? cd_->callbacks.get_size() : 0;
  if((cd_->blocks_requested != 0) &&
     ((cd_->current_sector >= disc_blocks) ||
      (cd_->blocks_requested > (disc_blocks - cd_->current_sector))))
    {
      cdrom_data_clear_transfer(cd_);
      cdrom_status_tagged_error(cd_,CDROM_CMD_READ_DATA,MEI_CDROM_end_address);
      opera_log_printf(OPERA_LOG_WARN,
                       "[Opera]: CDROM read rejected past end lba=%u blocks=%u disc_blocks=%u msf=%02u:%02u:%02u\n",
                       cd_->current_sector,
                       cd_->blocks_requested,
                       disc_blocks,
                       cd_->cmd[1],
                       cd_->cmd[2],
                       cd_->cmd[3]);
      return;
    }

  had_expected_lba = g_CDROM_STATE.data.read_have_expected_lba;
  expected_lba = g_CDROM_STATE.data.read_expected_lba;
  continuous_read = ((cd_->read_sector_size == CDROM_DA) &&
                     had_expected_lba &&
                     (cd_->current_sector == expected_lba));
  reset_readahead = (!continuous_read &&
                     (had_expected_lba ||
                      g_CDROM_STATE.data.audio_clock_active ||
                      (cd_->data_len > 0) ||
                      (cd_->poll & POLDT)));

  if(reset_readahead)
    cdrom_data_abort_readahead(cd_);

  if(cd_->read_sector_size == CDROM_DA)
    {
      if(!continuous_read || !g_CDROM_STATE.data.audio_clock_active)
        {
          g_CDROM_STATE.data.audio_base_lba = cd_->current_sector;
          g_CDROM_STATE.data.audio_base_cycle = opera_clock_cpu_get_cycles();
          g_CDROM_STATE.data.audio_clock_active = true;
          g_CDROM_STATE.data.audio_burst_active =
            (cdrom_audio_read_ahead_sectors() != 0);
        }
    }
  else
    {
      g_CDROM_STATE.data.audio_clock_active = false;
      g_CDROM_STATE.data.audio_burst_active = false;
    }

  g_CDROM_STATE.data.read_expected_lba = cd_->current_sector + cd_->blocks_requested;
  g_CDROM_STATE.data.read_have_expected_lba = (cd_->blocks_requested != 0);

  if(cd_->blocks_requested)
    {
      cd_->data_len = 0;
      cd_->data_idx = 0;
      g_CDROM_STATE.data.read_status_pending = true;
      cdrom_data_schedule_start(cd_,continuous_read);
      cd_->poll &= ~POLST;
      cd_->poll &= ~POLDT;
      if(cd_->read_sector_size != CDROM_DA)
        cdrom_data_load_next_if_ready(cd_);
    }
  else
    {
      cd_->data_len = 0;
      cd_->status_len = 2;
      cd_->poll |= POLST;
    }

  cd_->MEI_status = MEI_CDROM_no_error;
}

static
void
cdrom_cmd_data_path_check(cdrom_device_t *cd_)
{
  cd_->xbus_status |= CDROM_STATUS_READY;
  cd_->status_len   = 4;
  cd_->status[0]    = CDROM_CMD_DATA_PATH_CHECK;
  cd_->status[1]    = 0xAA;
  cd_->status[2]    = 0x55;
  cd_->status[3]    = cd_->xbus_status;
  cd_->MEI_status   = MEI_CDROM_no_error;
  cd_->poll        |= POLST;
}

static
void
cdrom_cmd_get_last_status(cdrom_device_t *cd_)
{
  uint8_t payload[8];

  memset(payload,cd_->MEI_status,sizeof(payload));
  cd_->xbus_status |= CDROM_STATUS_READY;
  cdrom_status_tagged(cd_,CDROM_CMD_GET_LAST_STATUS,payload,sizeof(payload),cd_->MEI_status);
  cd_->MEI_status = MEI_CDROM_no_error;
}

static
void
cdrom_cmd_read_id(cdrom_device_t *cd_)
{
  uint8_t payload[10];

  cdrom_persona_read_id_payload(payload);

  cd_->xbus_status |= CDROM_STATUS_READY;
  cdrom_status_tagged(cd_,CDROM_CMD_READ_ID,payload,sizeof(payload),MEI_CDROM_no_error);
}

static
void
cdrom_cmd_mode_sense(cdrom_device_t *cd_)
{
  if((cd_->xbus_status & CDROM_STATUS_DOOR) &&
     (cd_->xbus_status & CDROM_STATUS_DISC_IN))
    {
      uint8_t payload[3];

      cd_->xbus_status |= CDROM_STATUS_READY;
      cdrom_mode_sense_payload(cd_,payload);
      cdrom_status_tagged(cd_,CDROM_CMD_MODE_SENSE,payload,sizeof(payload),MEI_CDROM_no_error);
    }
  else
    {
      cdrom_status_media_error(cd_,CDROM_CMD_MODE_SENSE,CDROM_MEDIA_NO_SPIN_REQUIRED);
    }
}

static
void
cdrom_cmd_read_capacity(cdrom_device_t *cd_)
{
  if((cd_->xbus_status & CDROM_STATUS_DOOR) &&
     (cd_->xbus_status & CDROM_STATUS_DISC_IN) &&
     (cd_->xbus_status & CDROM_STATUS_SPIN_UP))
    {
      uint8_t payload[6];

      payload[0] = 0;
      payload[1] = cd_->disc.msf_total.minutes;
      payload[2] = cd_->disc.msf_total.seconds;
      payload[3] = cd_->disc.msf_total.frames;
      payload[4] = 0x00;
      payload[5] = 0x00;
      cd_->xbus_status |= CDROM_STATUS_READY;
      cdrom_status_tagged(cd_,CDROM_CMD_READ_CAPACITY,payload,sizeof(payload),MEI_CDROM_no_error);
    }
  else
    {
      cdrom_status_media_error(cd_,CDROM_CMD_READ_CAPACITY,CDROM_MEDIA_SPIN_REQUIRED);
    }
}

static
void
cdrom_cmd_read_header(cdrom_device_t *cd_)
{
  cdrom_status_tagged_error(cd_,CDROM_CMD_READ_HEADER,MEI_CDROM_illegal_request);
}

static
void
cdrom_cmd_read_subq(cdrom_device_t *cd_)
{
  int track_idx;
  uint32_t current_lba;
  uint32_t track_start_lba;
  uint32_t rel_lba;
  msf_t abs_msf;
  msf_t rel_msf;
  uint8_t ctl_adr;
  uint8_t index_bcd;
  uint8_t track_bcd;
  uint8_t payload[10];

  if(!((cd_->xbus_status & CDROM_STATUS_DOOR) &&
       (cd_->xbus_status & CDROM_STATUS_DISC_IN) &&
       (cd_->xbus_status & CDROM_STATUS_SPIN_UP)))
    {
      cdrom_status_media_error(cd_,CDROM_CMD_READ_SUBQ,CDROM_MEDIA_SPIN_REQUIRED);
      return;
    }

  current_lba = cdrom_data_audio_position_lba(cd_,opera_clock_cpu_get_cycles());

  track_idx = cdrom_track_for_lba(cd_,current_lba,&track_start_lba);
  if(track_idx < 0)
    {
      cdrom_status_tagged_error(cd_,CDROM_CMD_READ_SUBQ,MEI_CDROM_track_error);
      opera_log_printf(OPERA_LOG_WARN,
                       "[Opera]: CDROM read Sub-Q rejected no current track lba=%u tracks=%u..%u\n",
                       current_lba,
                       cd_->disc.track_first,
                       cd_->disc.track_last);
      return;
    }

  if(current_lba < track_start_lba)
    {
      index_bcd = 0x00;
      rel_lba = track_start_lba - current_lba;
    }
  else
    {
      index_bcd = 0x01;
      rel_lba = current_lba - track_start_lba;
    }

  LBA2MSF(current_lba,&abs_msf);
  FRAMES2MSF(rel_lba,&rel_msf);

  ctl_adr = cd_->disc.disc_toc[track_idx].CDCTL;
  ctl_adr = ((ctl_adr << 4) & 0xF0) | ((ctl_adr >> 4) & 0x0F);

  track_bcd = cd_->disc.disc_toc[track_idx].track_number;
  if(track_bcd < 100)
    track_bcd = (((track_bcd / 10) << 4) | (track_bcd % 10));

  payload[0] = 0x00;
  payload[1] = ctl_adr;
  payload[2] = track_bcd;
  payload[3] = index_bcd;
  payload[4] = abs_msf.minutes;
  payload[5] = abs_msf.seconds;
  payload[6] = abs_msf.frames;
  payload[7] = rel_msf.minutes;
  payload[8] = rel_msf.seconds;
  payload[9] = rel_msf.frames;

  cd_->xbus_status |= CDROM_STATUS_READY;
  cdrom_status_tagged(cd_,CDROM_CMD_READ_SUBQ,payload,sizeof(payload),MEI_CDROM_no_error);
}

static
void
cdrom_cmd_read_upc(cdrom_device_t *cd_)
{
  cdrom_status_tagged_error(cd_,CDROM_CMD_READ_UPC,MEI_CDROM_illegal_request);
}

static
void
cdrom_cmd_read_isrc(cdrom_device_t *cd_)
{
  cdrom_status_tagged_error(cd_,CDROM_CMD_READ_ISRC,MEI_CDROM_illegal_request);
}

static
void
cdrom_cmd_read_disc_code(cdrom_device_t *cd_)
{
  if((cd_->xbus_status & CDROM_STATUS_DOOR) &&
     (cd_->xbus_status & CDROM_STATUS_DISC_IN) &&
     (cd_->xbus_status & CDROM_STATUS_SPIN_UP))
    {
      uint8_t payload[10];

      memset(payload,0,sizeof(payload));
      cd_->xbus_status |= CDROM_STATUS_READY;
      cdrom_status_tagged(cd_,CDROM_CMD_READ_DISC_CODE,payload,sizeof(payload),MEI_CDROM_no_error);
    }
  else
    {
      cdrom_status_media_error(cd_,CDROM_CMD_READ_DISC_CODE,CDROM_MEDIA_SPIN_REQUIRED);
    }
}

static
void
cdrom_cmd_read_disc_info(cdrom_device_t *cd_)
{
  if((cd_->xbus_status & CDROM_STATUS_DOOR) &&
     (cd_->xbus_status & CDROM_STATUS_DISC_IN) &&
     (cd_->xbus_status & CDROM_STATUS_SPIN_UP))
    {
      uint8_t payload[6];

      payload[0] = cd_->disc.disc_id;
      payload[1] = cd_->disc.track_first;
      payload[2] = cd_->disc.track_last;
      payload[3] = cd_->disc.msf_total.minutes;
      payload[4] = cd_->disc.msf_total.seconds;
      payload[5] = cd_->disc.msf_total.frames;
      cd_->xbus_status |= CDROM_STATUS_READY;
      cdrom_status_tagged(cd_,CDROM_CMD_READ_DISC_INFO,payload,sizeof(payload),MEI_CDROM_no_error);
    }
  else
    {
      cdrom_status_media_error(cd_,CDROM_CMD_READ_DISC_INFO,CDROM_MEDIA_SPIN_REQUIRED);
    }
}

static
void
cdrom_cmd_read_toc(cdrom_device_t *cd_)
{
  toc_entry_t leadout;
  toc_entry_t *toc;
  uint8_t req_track;
  uint8_t payload[8];

  if(!((cd_->xbus_status & CDROM_STATUS_DOOR) &&
       (cd_->xbus_status & CDROM_STATUS_DISC_IN) &&
       (cd_->xbus_status & CDROM_STATUS_SPIN_UP)))
    {
      cdrom_status_media_error(cd_,CDROM_CMD_READ_TOC,CDROM_MEDIA_SPIN_REQUIRED);
      return;
    }

  req_track = cd_->cmd[2];
  toc = NULL;

  if(req_track == 0xAA)
    {
      uint8_t leadout_control;

      leadout_control = CD_CTL_DATA_TRACK | CD_CTL_Q_POSITION;
      if(cdrom_toc_track_in_range(&cd_->disc,cd_->disc.track_last) &&
         cdrom_toc_entry_valid(&cd_->disc,
                               cd_->disc.track_last,
                               &cd_->disc.disc_toc[cd_->disc.track_last]))
        leadout_control = cdrom_toc_position_control(
                                                     cd_->disc.disc_toc[cd_->disc.track_last].CDCTL);

      memset(&leadout,0,sizeof(leadout));
      leadout.CDCTL = leadout_control;
      leadout.track_number = 0xAA;
      leadout.minutes = cd_->disc.msf_total.minutes;
      leadout.seconds = cd_->disc.msf_total.seconds;
      leadout.frames  = cd_->disc.msf_total.frames;
      toc = &leadout;
    }
  else if((req_track >= cd_->disc.track_first) &&
          (req_track <= cd_->disc.track_last) &&
          (req_track < (sizeof(cd_->disc.disc_toc) /
                        sizeof(cd_->disc.disc_toc[0]))) &&
          cdrom_toc_entry_valid(&cd_->disc,
                                req_track,
                                &cd_->disc.disc_toc[req_track]))
    {
      toc = &cd_->disc.disc_toc[req_track];
    }

  if(toc == NULL)
    {
      cdrom_status_tagged_error(cd_,CDROM_CMD_READ_TOC,MEI_CDROM_track_error);
      opera_log_printf(OPERA_LOG_WARN,
                       "[Opera]: CDROM read TOC rejected req_track=%u valid=%u..%u\n",
                       req_track,
                       cd_->disc.track_first,
                       cd_->disc.track_last);
      return;
    }

  payload[0] = toc->res0;
  payload[1] = toc->CDCTL;
  payload[2] = toc->track_number;
  payload[3] = toc->res1;
  payload[4] = toc->minutes;
  payload[5] = toc->seconds;
  payload[6] = toc->frames;
  payload[7] = toc->res2;
  cd_->xbus_status |= CDROM_STATUS_READY;
  cdrom_status_tagged(cd_,CDROM_CMD_READ_TOC,payload,sizeof(payload),MEI_CDROM_no_error);
}

static
void
cdrom_cmd_read_session_info(cdrom_device_t *cd_)
{
  if((cd_->xbus_status & CDROM_STATUS_DOOR) &&
     (cd_->xbus_status & CDROM_STATUS_DISC_IN))
    {
      uint8_t payload[6];

      payload[0] = cd_->disc.session_valid ? 0x80 : 0x00;
      payload[1] = cd_->disc.session_valid ? cd_->disc.msf_session.minutes : 0x00;
      payload[2] = cd_->disc.session_valid ? cd_->disc.msf_session.seconds : 0x00;
      payload[3] = cd_->disc.session_valid ? cd_->disc.msf_session.frames  : 0x00;
      payload[4] = 0x00;
      payload[5] = 0x00;
      cd_->xbus_status |= CDROM_STATUS_READY;
      cdrom_status_tagged(cd_,CDROM_CMD_READ_SESSION_INFO,payload,sizeof(payload),MEI_CDROM_no_error);
    }
  else
    {
      cdrom_status_media_error(cd_,CDROM_CMD_READ_SESSION_INFO,CDROM_MEDIA_NO_SPIN_REQUIRED);
    }
}

static
void
cdrom_cmd_read_device_driver(cdrom_device_t *cd_)
{
  opera_log_printf(OPERA_LOG_WARN,
                   "[Opera]: CDROM READ_DEVICE_DRIVER requested but no device-ROM driver is advertised\n");
  cdrom_status_tagged_error(cd_,CDROM_CMD_READ_DEVICE_DRIVER,MEI_CDROM_illegal_request);
}

static
void
ode_read_description(cdrom_device_t *cd_)
{
  ode_status(cd_,cd_->cmd[0],NULL,0);
}

static
void
ode_clear_playlist(cdrom_device_t *cd_)
{
  ode_playlist_clear();
  ode_status(cd_,cd_->cmd[0],NULL,0);
}

static
void
ode_close_file_cmd(cdrom_device_t *cd_)
{
  ode_file_close();
  ode_status(cd_,cd_->cmd[0],NULL,0);
}

static
void
ode_start_update(cdrom_device_t *cd_)
{
  ode_status(cd_,cd_->cmd[0],NULL,0);
}

static
void
cdrom_cmd_unhandled(cdrom_device_t *cd_)
{
  opera_log_printf(OPERA_LOG_WARN,
                   "[Opera]: CDROM unhandled cmd %02x args=%02x %02x %02x %02x %02x %02x\n",
                   cd_->cmd[0],
                   cd_->cmd[1],
                   cd_->cmd[2],
                   cd_->cmd[3],
                   cd_->cmd[4],
                   cd_->cmd[5],
                   cd_->cmd[6]);
  cdrom_status_tagged_error(cd_,cd_->cmd[0],MEI_CDROM_cdb_error);
}

void
opera_cdrom_do_cmd(cdrom_device_t *cd_)
{
  cd_->status_len = 0;

  cd_->poll        &= ~POLST;
  cd_->poll        &= ~POLDT;
  cd_->xbus_status &= ~CDROM_STATUS_ERROR;
  cd_->xbus_status &= ~CDROM_STATUS_READY;

  if(cdrom_cmd_is_ode(cd_->cmd[0]))
    g_CDROM_STATE.ode.savestate_blocked = true;

  switch(cd_->cmd[0])
    {
    case CDROM_CMD_SEEK:
      cdrom_cmd_seek(cd_);
      break;
    case CDROM_CMD_SPIN_UP:
      cdrom_cmd_spin_up(cd_);
      break;
    case CDROM_CMD_SPIN_DOWN:
      cdrom_cmd_spin_down(cd_);
      break;
    case CDROM_CMD_DIAGNOSTICS:
      cdrom_cmd_diagnostics(cd_);
      break;
    case CDROM_CMD_EJECT_DISC:
      cdrom_cmd_eject_disc(cd_);
      break;
    case CDROM_CMD_INJECT_DISC:
      cdrom_cmd_inject_disc(cd_);
      break;
    case CDROM_CMD_ABORT:
      cdrom_cmd_abort(cd_);
      break;
    case CDROM_CMD_MODE_SET:
      cdrom_cmd_mode_set(cd_);
      break;
    case CDROM_CMD_RESET:
      cdrom_cmd_reset(cd_);
      break;
    case CDROM_CMD_FLUSH:
      cdrom_cmd_flush(cd_);
      break;
    case CDROM_CMD_PLAY_AUDIO_MSF:
      cdrom_cmd_play_audio_msf(cd_);
      break;
    case CDROM_CMD_PLAY_AUDIO_TRACK:
      cdrom_cmd_play_audio_track(cd_);
      break;
    case CDROM_CMD_READ_DATA:
      cdrom_cmd_read_data(cd_);
      break;
    case CDROM_CMD_DATA_PATH_CHECK:
      cdrom_cmd_data_path_check(cd_);
      break;
    case CDROM_CMD_GET_LAST_STATUS:
      cdrom_cmd_get_last_status(cd_);
      break;
    case CDROM_CMD_READ_ID:
      cdrom_cmd_read_id(cd_);
      break;
    case CDROM_CMD_MODE_SENSE:
      cdrom_cmd_mode_sense(cd_);
      break;
    case CDROM_CMD_READ_CAPACITY:
      cdrom_cmd_read_capacity(cd_);
      break;
    case CDROM_CMD_READ_HEADER:
      cdrom_cmd_read_header(cd_);
      break;
    case CDROM_CMD_READ_SUBQ:
      cdrom_cmd_read_subq(cd_);
      break;
    case CDROM_CMD_READ_UPC:
      cdrom_cmd_read_upc(cd_);
      break;
    case CDROM_CMD_READ_ISRC:
      cdrom_cmd_read_isrc(cd_);
      break;
    case CDROM_CMD_READ_DISC_CODE:
      cdrom_cmd_read_disc_code(cd_);
      break;
    case CDROM_CMD_READ_DISC_INFO:
      cdrom_cmd_read_disc_info(cd_);
      break;
    case CDROM_CMD_READ_TOC:
      cdrom_cmd_read_toc(cd_);
      break;
    case CDROM_CMD_READ_SESSION_INFO:
      cdrom_cmd_read_session_info(cd_);
      break;
    case CDROM_CMD_READ_DEVICE_DRIVER:
      cdrom_cmd_read_device_driver(cd_);
      break;

    case ODE_CMD_EXTENDED_ID:
      ode_extended_id(cd_);
      break;
    case ODE_CMD_CHANGE_TOC:
      ode_change_toc(cd_);
      break;
    case ODE_CMD_READ_TOC_WINDOW:
      ode_read_toc_window(cd_);
      break;
    case ODE_CMD_READ_DESCRIPTION:
      ode_read_description(cd_);
      break;
    case ODE_CMD_CLEAR_PLAYLIST:
      ode_clear_playlist(cd_);
      break;
    case ODE_CMD_ADD_PLAYLIST:
      ode_add_playlist(cd_);
      break;
    case ODE_CMD_LAUNCH_PLAYLIST:
      ode_launch_playlist(cd_);
      break;
    case ODE_CMD_READ_TOC_LIST:
      ode_read_toc_list(cd_);
      break;
    case ODE_CMD_CREATE_FILE:
      ode_create_file(cd_);
      break;
    case ODE_CMD_OPEN_FILE:
      ode_open_file(cd_);
      break;
    case ODE_CMD_SEEK_FILE:
      ode_seek_file(cd_);
      break;
    case ODE_CMD_READ_FILE:
      ode_read_file(cd_);
      break;
    case ODE_CMD_WRITE_FILE:
      ode_write_file(cd_);
      break;
    case ODE_CMD_CLOSE_FILE:
      ode_close_file_cmd(cd_);
      break;
    case ODE_CMD_WRITE_BUFFER_WORD:
      ode_write_buffer_word(cd_);
      break;
    case ODE_CMD_READ_FILE_BUFFER:
      ode_read_file_buffer(cd_);
      break;
    case ODE_CMD_FAST_BUFFER_SEND:
      ode_fast_buffer_send(cd_);
      break;
    case ODE_CMD_START_UPDATE:
      ode_start_update(cd_);
      break;
    default:
      cdrom_cmd_unhandled(cd_);
      break;
    }
}

static
void
cdrom_drain_pending_status(cdrom_device_t *cd_,
                           uint8_t         opcode_)
{
  if((cd_->status_len == 0) && !(cd_->poll & POLST))
    {
      ode_complete_restart_if_armed(cd_);
      return;
    }

  ode_complete_restart_if_armed(cd_);
  cd_->status_len = 0;
  cd_->poll &= ~POLST;
}

static
void
cdrom_log_pending_data_before_command(const cdrom_device_t *cd_,
                                      uint8_t               opcode_)
{
  if((cd_->data_len == 0) && (cd_->blocks_requested == 0) &&
     !(cd_->poll & POLDT))
    return;

}

void
opera_cdrom_send_cmd(cdrom_device_t *cd_,
                     uint8_t         val_)
{
  uint8_t cmd_len;
  uint8_t cmd[7];

  if(cd_->cmd_idx == 0)
    {
      ode_file_buffer_write_reset();
      cdrom_drain_pending_status(cd_,val_);
      cdrom_log_pending_data_before_command(cd_,val_);
    }

  if(cd_->cmd_idx < 7)
    cd_->cmd[cd_->cmd_idx++] = (uint8_t)val_;

  if((cd_->cmd_idx >= 7) || (cd_->cmd[0] == 0x08))
    {
      cmd_len = cd_->cmd_idx;
      memset(cmd,0,sizeof(cmd));
      memcpy(cmd,cd_->cmd,cmd_len);
      opera_cdrom_do_cmd(cd_);

      cd_->cmd_idx = 0;
    }
}

int
opera_cdrom_update_fiq(cdrom_device_t *cd_)
{
  cdrom_data_load_next_if_ready(cd_);

  return (((cd_->poll & POLST) && (cd_->poll & POLSTMASK)) ||
          ((cd_->poll & POLDT) && (cd_->poll & POLDTMASK)) ||
          ((cd_->poll & POLMA) && (cd_->poll & POLMAMASK)));
}

void
opera_cdrom_set_poll(cdrom_device_t *cd_,
                     uint32_t        val_)
{
  uint8_t enable_bits;

  enable_bits = ((uint8_t)val_ & CDROM_POLL_INTEN_MASK);

  if(val_ & POLREMASK)
    cdrom_fifo_reset(cd_);

  if(val_ & POLRE)
    cd_->poll &= ~POLRE;

  cd_->poll = ((cd_->poll & 0xF0) | enable_bits);

}

void
opera_cdrom_fifo_set_data(cdrom_device_t *cd_,
                          uint8_t         val_)
{
  (void)cd_;

  /* ODE 0xE8 fast-buffer input uses the normal CDROM data FIFO. */
  if(g_CDROM_STATE.ode.file_buffer_write_idx < REQSIZE)
    g_CDROM_STATE.ode.file_buffer_write_data[g_CDROM_STATE.ode.file_buffer_write_idx] = val_;
  else
    g_CDROM_STATE.ode.file_buffer_write_overflow = true;

  if(g_CDROM_STATE.ode.file_buffer_write_idx < UINT32_MAX)
    g_CDROM_STATE.ode.file_buffer_write_idx++;
}

uint8_t
opera_cdrom_fifo_get_data(cdrom_device_t *cd_)
{
  uint8_t rv;

  cdrom_data_load_next_if_ready(cd_);

  rv = 0;
  if(cd_->data_len > 0)
    {
      rv = cd_->data[cd_->data_idx];
      cd_->data_idx++;
      cd_->data_len--;

      if(cd_->data_len == 0)
        {
          cd_->data_idx = 0;
          g_CDROM_STATE.data.sectors_drained++;
          if(cd_->blocks_requested)
            {
              cdrom_data_load_next_if_ready(cd_);
            }
          else
            {
              cd_->poll             &= ~POLDT;
              cd_->blocks_requested  = 0;
              cd_->data_len          = 0;
              cd_->data_idx          = 0;
            }
        }
    }
  else
    {
      g_CDROM_STATE.data.empty_reads++;
    }

  return rv;
}
