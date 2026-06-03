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

#ifndef LIBOPERA_CDROM_H_INCLUDED
#define LIBOPERA_CDROM_H_INCLUDED

#include "extern_c.h"

#include <stddef.h>
#include <stdint.h>

#define STATDELAY 100
#define REQSIZE   2048
#define CDROM_MAX_SECTOR_SIZE 2449

enum MEI_CDROM_Error_Codes
  {
    MEI_CDROM_no_error        = 0x00,
    MEI_CDROM_recv_retry      = 0x01,
    MEI_CDROM_recv_ecc        = 0x02,
    MEI_CDROM_not_ready       = 0x03,
    MEI_CDROM_toc_error       = 0x04,
    MEI_CDROM_unrecv_error    = 0x05,
    MEI_CDROM_seek_error      = 0x06,
    MEI_CDROM_track_error     = 0x07,
    MEI_CDROM_ram_error       = 0x08,
    MEI_CDROM_diag_error      = 0x09,
    MEI_CDROM_focus_error     = 0x0A,
    MEI_CDROM_clv_error       = 0x0B,
    MEI_CDROM_data_error      = 0x0C,
    MEI_CDROM_address_error   = 0x0D,
    MEI_CDROM_cdb_error       = 0x0E,
    MEI_CDROM_end_address     = 0x0F,
    MEI_CDROM_mode_error      = 0x10,
    MEI_CDROM_media_changed   = 0x11,
    MEI_CDROM_hard_reset      = 0x12,
    MEI_CDROM_rom_error       = 0x13,
    MEI_CDROM_cmd_error       = 0x14,
    MEI_CDROM_disc_out        = 0x15,
    MEI_CDROM_hardware_error  = 0x16,
    MEI_CDROM_illegal_request = 0x17
  };

enum CDROM_Commands
  {
    CDROM_CMD_SEEK               = 0x01,
    CDROM_CMD_SPIN_UP            = 0x02,
    CDROM_CMD_SPIN_DOWN          = 0x03,
    CDROM_CMD_DIAGNOSTICS        = 0x04,
    CDROM_CMD_EJECT_DISC         = 0x06,
    CDROM_CMD_INJECT_DISC        = 0x07,
    CDROM_CMD_ABORT              = 0x08,
    CDROM_CMD_MODE_SET           = 0x09,
    CDROM_CMD_RESET              = 0x0A,
    CDROM_CMD_FLUSH              = 0x0B,
    CDROM_CMD_READ_DATA          = 0x10,
    CDROM_CMD_PLAY_AUDIO_MSF     = 0x0E,
    CDROM_CMD_PLAY_AUDIO_TRACK   = 0x0F,
    CDROM_CMD_DATA_PATH_CHECK    = 0x80,
    CDROM_CMD_GET_LAST_STATUS    = 0x82,
    CDROM_CMD_READ_ID            = 0x83,
    CDROM_CMD_MODE_SENSE         = 0x84,
    CDROM_CMD_READ_CAPACITY      = 0x85,
    CDROM_CMD_READ_HEADER        = 0x86,
    CDROM_CMD_READ_SUBQ          = 0x87,
    CDROM_CMD_READ_UPC           = 0x88,
    CDROM_CMD_READ_ISRC          = 0x89,
    CDROM_CMD_READ_DISC_CODE     = 0x8A,
    CDROM_CMD_READ_DISC_INFO     = 0x8B,
    CDROM_CMD_READ_TOC           = 0x8C,
    CDROM_CMD_READ_SESSION_INFO  = 0x8D,
    CDROM_CMD_READ_DEVICE_DRIVER = 0x8E
  };

#define POLSTMASK 0x01
#define POLDTMASK 0x02
#define POLMAMASK 0x04
#define POLREMASK 0x08
#define POLST	  0x10
#define POLDT	  0x20
#define POLMA	  0x40
#define POLRE	  0x80

#define CDROM_STATUS_DOOR         0x80
#define CDROM_STATUS_DISC_IN      0x40
#define CDROM_STATUS_SPIN_UP      0x20
#define CDROM_STATUS_ERROR        0x10
#define CDROM_STATUS_DOUBLE_SPEED 0x02
#define CDROM_STATUS_READY        0x01

#define CD_CTL_PREEMPHASIS    0x01
#define CD_CTL_COPY_PERMITTED 0x02
#define CD_CTL_DATA_TRACK     0x04
#define CD_CTL_FOUR_CHANNEL   0x08
#define CD_CTL_QMASK          0xF0
#define CD_CTL_Q_NONE         0x00
#define CD_CTL_Q_POSITION     0x10
#define CD_CTL_Q_MEDIACATALOG 0x20
#define CD_CTL_Q_ISRC         0x30

#define MEI_DISC_DA_OR_CDROM 0x00
#define MEI_DISC_CDI         0x10
#define MEI_DISC_CDROM_XA    0x20

#define CDROM_M1_D            2048
#define CDROM_DA              2352
#define CDROM_DA_PLUS_ERR     2353
#define CDROM_DA_PLUS_SUBCODE 2448
#define CDROM_DA_PLUS_BOTH    2449

#define MEI_CDROM_DEFAULT_DENSITY 0x00
#define MEI_CDROM_DATA            0x01
#define MEI_CDROM_MODE2_XA        0x81
#define MEI_CDROM_DIGITAL_AUDIO   0x82

#define MEI_CDROM_SINGLE_SPEED 0x00
#define MEI_CDROM_DOUBLE_SPEED 0x80

#define MEI_CDROM_DEFAULT_RECOVERY      0x00
#define MEI_CDROM_CIRC_RETRIES_ONLY     0x01
#define MEI_CDROM_BEST_ATTEMPT_RECOVERY 0x20

#define MEI_VPITCH_NORMAL_MSB 0x00
#define MEI_VPITCH_NORMAL_LSB 0x00
#define MEI_VPITCH_FAST_MSB   0x04
#define MEI_VPITCH_FAST_LSB   0x0A
#define MEI_VPITCH_SLOW_MSB   0x07
#define MEI_VPITCH_SLOW_LSB   0xF6

#define Address_Blocks    0
#define Address_Abs_MSF   1
#define Address_Track_MSF 2

EXTERN_C_BEGIN

struct toc_entry_s
{
  uint8_t res0;
  uint8_t CDCTL;
  uint8_t track_number;
  uint8_t res1;
  uint8_t minutes;
  uint8_t seconds;
  uint8_t frames;
  uint8_t res2;
};

typedef struct toc_entry_s toc_entry_t;

struct msf_s
{
  uint8_t minutes;
  uint8_t seconds;
  uint8_t frames;
};

typedef struct msf_s msf_t;

/*
  msf_session, track_first, track_last, and disc_id aren't really used
  but left in for clarity
*/
struct disc_data_s
{
  msf_t msf_total;
  msf_t msf_current;
  msf_t msf_session;
  uint8_t session_valid;

  uint8_t track_first;
  uint8_t track_last;

  uint8_t     disc_id;
  toc_entry_t disc_toc[100];
};

typedef struct disc_data_s disc_data_t;

struct cdrom_block_parameters_s
{
  uint8_t  density_code;
  uint8_t  length_msb;
  uint8_t  length_lsb;
  uint8_t  flags;
  uint32_t user_block_size;
};

typedef struct cdrom_block_parameters_s cdrom_block_parameters_t;

struct cdrom_error_recovery_parameters_s
{
  uint8_t type;
  uint8_t retry_count;
};

typedef struct cdrom_error_recovery_parameters_s cdrom_error_recovery_parameters_t;

struct cdrom_stop_time_parameters_s
{
  uint8_t time;
};

typedef struct cdrom_stop_time_parameters_s cdrom_stop_time_parameters_t;

struct cdrom_drive_speed_parameters_s
{
  uint8_t speed;
  uint8_t vpitch_msb;
  uint8_t vpitch_lsb;
};

typedef struct cdrom_drive_speed_parameters_s cdrom_drive_speed_parameters_t;

struct cdrom_chunk_size_parameters_s
{
  uint8_t size;
};

typedef struct cdrom_chunk_size_parameters_s cdrom_chunk_size_parameters_t;

struct cdrom_mode_parameters_s
{
  cdrom_block_parameters_t          block;
  cdrom_error_recovery_parameters_t error_recovery;
  cdrom_stop_time_parameters_t      stop_time;
  cdrom_drive_speed_parameters_t    drive_speed;
  cdrom_chunk_size_parameters_t     chunk_size;
};

typedef struct cdrom_mode_parameters_s cdrom_mode_parameters_t;

typedef uint32_t (*opera_cdrom_get_size_cb_t)(void);
typedef void (*opera_cdrom_set_sector_cb_t)(const uint32_t sector_);
typedef void (*opera_cdrom_read_sector_cb_t)(void *buf_, size_t len_);
typedef void (*opera_cdrom_get_toc_cb_t)(uint8_t  *track_first_,
                                          uint8_t  *track_last_,
                                          uint8_t  *disc_id_,
                                          void     *disc_toc_,
                                          uint32_t  disc_toc_size_);
#define OPERA_CDROM_ODE_LAUNCH_UPDATE_CONTENT_PATH 0x00000001

typedef int (*opera_cdrom_ode_launch_cb_t)(const char *path_,
                                           uint32_t    flags_);

typedef struct cdrom_callbacks_s cdrom_callbacks_t;
struct cdrom_callbacks_s
{
  opera_cdrom_get_size_cb_t    get_size;
  opera_cdrom_set_sector_cb_t  set_sector;
  opera_cdrom_read_sector_cb_t read_sector;
  opera_cdrom_get_toc_cb_t     get_toc;
};

struct cdrom_device_s
{
  uint8_t            poll;
  uint8_t            xbus_status;
  uint8_t            status_len;
  uint8_t            status[256];
  uint32_t           read_sector_size;
  uint32_t           data_len;
  uint32_t           data_idx;
  uint8_t            data[CDROM_MAX_SECTOR_SIZE];
  uint32_t           blocks_requested;
  uint8_t            cmd[7];
  uint8_t            cmd_idx;
  int8_t             STATCYC;
  uint32_t           MEI_status;
  uint32_t           current_sector;
  cdrom_mode_parameters_t mode;
  disc_data_t        disc;
  cdrom_callbacks_t  callbacks;
};

typedef struct cdrom_device_s cdrom_device_t;

void    opera_cdrom_init(cdrom_device_t *cd_);
void    opera_cdrom_restore_callbacks(cdrom_device_t *cd_);
uint32_t opera_cdrom_state_size(void);
uint32_t opera_cdrom_state_save(cdrom_device_t *cd_, void *data_);
uint32_t opera_cdrom_state_load(cdrom_device_t *cd_, void const *data_, uint32_t data_size_);
void    opera_cdrom_set_speed(uint32_t speed_);
void    opera_cdrom_send_cmd(cdrom_device_t *cd_, uint8_t val_);
void    opera_cdrom_set_poll(cdrom_device_t *cd_, uint32_t val_);
int     opera_cdrom_update_fiq(cdrom_device_t *cd_);
uint8_t opera_cdrom_fifo_get_status(cdrom_device_t *cd_);
void    opera_cdrom_fifo_set_data(cdrom_device_t *cd_, uint8_t val_);
uint8_t opera_cdrom_fifo_get_data(cdrom_device_t *cd_);
void    opera_cdrom_set_callbacks(opera_cdrom_get_size_cb_t    get_size_,
                                  opera_cdrom_set_sector_cb_t  set_sector_,
                                  opera_cdrom_read_sector_cb_t read_sector_,
                                  opera_cdrom_get_toc_cb_t     get_toc_);
int     opera_cdrom_media_ejected(const cdrom_device_t *cd_);
void    opera_cdrom_media_eject(cdrom_device_t *cd_);
void    opera_cdrom_ode_set_root(const char *root_);
void    opera_cdrom_ode_set_launch_callback(opera_cdrom_ode_launch_cb_t launch_);
int     opera_cdrom_ode_request_launch(const char *path_, uint32_t flags_);
int     opera_cdrom_ode_consume_restart_request(void);
int     opera_cdrom_ode_restart_requested(void);
void     opera_cdrom_ode_reset_session(void);

EXTERN_C_END

#endif /* LIBOPERA_CDROM_H_INCLUDED */
