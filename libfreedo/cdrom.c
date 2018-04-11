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

/* cdrom.c: implementation of the CIso class. */

#include "cdrom.h"

#include <boolean.h>
#include <retro_inline.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MSF_BIAS_IN_SECONDS 2
#define MSF_BIAS_IN_FRAMES  150
#define FRAMES_PER_SECOND   75
#define SECONDS_PER_MINUTE  60

/* FIXME: should not be using globals */
freedo_cdrom_get_size_cb_t    CDROM_GET_SIZE;
freedo_cdrom_set_sector_cb_t  CDROM_SET_SECTOR;
freedo_cdrom_read_sector_cb_t CDROM_READ_SECTOR;

static
INLINE
void
LBA2MSF(const uint32_t  lba_,
        msf_t          *msf_)
{
  uint32_t mod;

  mod           = ((lba_ + MSF_BIAS_IN_FRAMES) % (SECONDS_PER_MINUTE * FRAMES_PER_SECOND));
  msf_->minutes = ((lba_ + MSF_BIAS_IN_FRAMES) / (SECONDS_PER_MINUTE * FRAMES_PER_SECOND));
  msf_->seconds = (mod / FRAMES_PER_SECOND);
  msf_->frames  = (mod % FRAMES_PER_SECOND);
}

static
INLINE
uint32_t
MSF2LBA(const msf_t *msf_)
{
  uint32_t lba;

  lba = ((msf_->minutes * SECONDS_PER_MINUTE * FRAMES_PER_SECOND) +
         (msf_->seconds * FRAMES_PER_SECOND) +
         (msf_->frames) -
         (MSF_BIAS_IN_FRAMES));
  if(lba < 0)
    lba = 0;

  return lba;
}

void
freedo_cdrom_set_callbacks(freedo_cdrom_get_size_cb_t    get_size_,
                           freedo_cdrom_set_sector_cb_t  set_sector_,
                           freedo_cdrom_read_sector_cb_t read_sector_)
{
  CDROM_GET_SIZE    = get_size_;
  CDROM_SET_SECTOR  = set_sector_;
  CDROM_READ_SECTOR = read_sector_;
}

void
freedo_cdrom_init(cdrom_device_t *cd_)
{
  uint32_t file_size_in_blocks;

  cd_->current_sector = 0;
  CDROM_SET_SECTOR(cd_->current_sector);
  file_size_in_blocks = CDROM_GET_SIZE();

  cd_->data_idx     = 0;
  cd_->xbus_status  = 0;
  cd_->poll         = 0x0F;
  cd_->xbus_status |= CDST_OK;

  cd_->MEI_status = MEI_CDROM_no_error;

  cd_->disc.track_first = 1;
  cd_->disc.track_last  = 1;

  cd_->disc.disc_id = MEI_DISC_DA_OR_CDROM;

  cd_->disc.msf_current.minutes = 0;
  cd_->disc.msf_current.seconds = MSF_BIAS_IN_SECONDS;
  cd_->disc.msf_current.frames  = 0;

  cd_->disc.disc_toc[1].CDCTL        = CD_CTL_DATA_TRACK|CD_CTL_Q_NONE; /* |CD_CTL_COPY_PERMITTED; */
  cd_->disc.disc_toc[1].track_number = 1;
  cd_->disc.disc_toc[1].minutes      = 0;
  cd_->disc.disc_toc[1].seconds      = MSF_BIAS_IN_SECONDS;
  cd_->disc.disc_toc[1].frames       = 0;

  LBA2MSF(file_size_in_blocks + MSF_BIAS_IN_FRAMES,&cd_->disc.msf_total);
  LBA2MSF(file_size_in_blocks,&cd_->disc.msf_session);

  cd_->STATCYC = STATDELAY;
}

uint8_t
freedo_cdrom_fifo_get_status(cdrom_device_t *cd_)
{
  uint8_t rv;

  rv = 0;
  if(cd_->status_len > 0)
    {
      rv = cd_->status[0];
      cd_->status_len--;
      if(cd_->status_len > 0)
        memmove(cd_->status,cd_->status + 1,cd_->status_len);
      else
        cd_->poll &= ~POLST;
    }

  return rv;
}

void
freedo_cdrom_do_cmd(cdrom_device_t *cd_)
{
  int i;

  cd_->status_len = 0;

  cd_->poll        &= ~POLST;
  cd_->poll        &= ~POLDT;
  cd_->xbus_status &= ~CDST_ERRO;
  cd_->xbus_status &= ~CDST_RDY;

  switch(cd_->cmd[0])
    {
      /*
        seek
        not used in opera
        01 00 ll-bb-aa 00 00.
        01 02 mm-ss-ff 00 00.
        status 4 bytes
        xx xx xx XS  (xs = xbus status)
      */
    case CDROM_CMD_SEEK:
      break;

      /*
        spin up
        opera status request = 0
        status 4 bytes
        xx xx xx XS (XS = xbus status)
      */
    case CDROM_CMD_SPIN_UP:
      if((cd_->xbus_status & CDST_TRAY) &&
         (cd_->xbus_status & CDST_DISC))
        {
          cd_->xbus_status |= CDST_SPIN;
          cd_->xbus_status |= CDST_RDY;
          cd_->MEI_status   = MEI_CDROM_no_error;
        }
      else
        {
          cd_->xbus_status |= CDST_ERRO;
          cd_->xbus_status &= ~CDST_RDY;
          cd_->MEI_status   = MEI_CDROM_recv_ecc;
        }

      cd_->status_len = 2;
      cd_->status[0]  = CDROM_CMD_SPIN_UP;
      cd_->status[1]  = cd_->xbus_status;

      cd_->poll |= POLST;
      break;

      /*
        spin down
        opera status request = 0 // not used in opera
        status 4 bytes
        xx xx xx XS  (XS = xbus status)
      */
    case CDROM_CMD_SPIN_DOWN:
      cd_->xbus_status |= CDST_RDY;
      if((cd_->xbus_status & CDST_TRAY) &&
         (cd_->xbus_status & CDST_DISC))
        {
          cd_->xbus_status &= ~CDST_SPIN;
          cd_->MEI_status   = MEI_CDROM_no_error;
        }
      else
        {
          cd_->xbus_status |= CDST_ERRO;
          cd_->MEI_status   = MEI_CDROM_recv_ecc;
        }

      cd_->status_len = 2;
      cd_->status[0]  = CDROM_CMD_SPIN_DOWN;
      cd_->status[1]  = cd_->xbus_status;

      cd_->poll |= POLST;
      break;

      /*
        diagnostics
        not used in opera
        04 00 ll-bb-aa 00 00.
        04 02 mm-ss-ff 00 00.
        status 4 bytes
        xx S1 S2 XS
      */
    case CDROM_CMD_DIAGNOSTICS:
      break;

      /*
        eject disc
        opera status request  = 0
        status 4 bytes
        xx xx xx XS
        1b command of scsi
        emulation ---
        Execute EJECT command;
        Check Sense, update PollRegister (if medium present)
      */
    case CDROM_CMD_EJECT_DISC:
      cd_->xbus_status |= CDST_RDY;
      cd_->xbus_status &= ~CDST_TRAY;
      cd_->xbus_status &= ~CDST_DISC;
      cd_->xbus_status &= ~CDST_SPIN;
      cd_->xbus_status &= ~CDST_2X;
      cd_->xbus_status &= ~CDST_ERRO;
      cd_->MEI_status   = MEI_CDROM_no_error;

      cd_->status_len = 2;
      cd_->status[0]  = CDROM_CMD_EJECT_DISC;
      cd_->status[1]  = cd_->xbus_status;

      cd_->poll |= POLST;
      cd_->poll &= ~POLMA;
      break;

      /*
        inject disc
        opera status request = 0
        status 4 bytes
        xx xx xx XS
        1b command of scsi
      */
    case CDROM_CMD_INJECT_DISC:
      cd_->status_len = 2;
      cd_->status[0]  = CDROM_CMD_INJECT_DISC;
      cd_->status[1]  = cd_->xbus_status;

      cd_->poll |= POLST;
      break;

      /*
        abort !!!
        opera status request = 31
        status 4 bytes
        xx xx xx XS
      */
    case CDROM_CMD_ABORT:
      cd_->status_len = 33;
      cd_->status[0]  = CDROM_CMD_ABORT;
      for(i = 1; i < 32; i++)
        cd_->status[i] = 0;
      cd_->status[32] = cd_->xbus_status;

      cd_->xbus_status |= CDST_RDY;
      cd_->MEI_status   = MEI_CDROM_no_error;
      break;

      /*
        mode set
        09 MM nn 00 00 00 00    // 2048 or 2340 transfer size
        to be checked -- wasn't called even once
        2nd byte is type selector
        MM = mode nn= value
        opera status request = 0
        status 4 bytes
        xx xx xx XS
      */
    case CDROM_CMD_MODE_SET:
      /* if((xbus_status&CDST_TRAY) && (xbus_status&CDST_disc)) */
      /*   { */
      cd_->xbus_status |= CDST_RDY;
      cd_->MEI_status   = MEI_CDROM_no_error;

      /*     CDMode[cmd[1]]  = cmd[2]; */
      /*   } */
      /* else */
      /* 	{ */
      /*     xbus_status     |= CDST_ERRO; */
      /*     xbus_status     &= ~CDST_RDY; */
      /* 	} */

      cd_->status_len = 2;
      cd_->status[0]  = CDROM_CMD_MODE_SET;
      cd_->status[1]  = cd_->xbus_status;

      cd_->poll |= POLST;
      break;

      /*
        reset
        not used in opera
        status 4 bytes
        xx xx xx XS
      */
    case CDROM_CMD_RESET:
      break;

      /*
        flush
        opera status request  = 31
        status 4 bytes
        xx xx xx XS
        returns data
        flush all internal buffer
        1+31+1
      */
    case CDROM_CMD_FLUSH:
      cd_->xbus_status |= CDST_RDY;
      cd_->status_len   = 33;
      cd_->status[0]    = CDROM_CMD_FLUSH;
      for(i = 1; i < 32; i++)
        cd_->status[i] = 0;
      cd_->status[32] = cd_->xbus_status;

      /* cd_->xbus_status |= CDST_RDY; */
      cd_->MEI_status = MEI_CDROM_no_error;
      break;

      /*
        READ DATA
        10 01 00 00 00 00 01
        read 1 blocks from MSF = 1.0.0
        10 xx-xx-xx fl nn-nn
        00 01 02 03 04 05 06
        reads nn blocks from xx
        fl = 0 xx="msf" ?
        fl = 1 xx="lba" ?
        block = 2048 bytes
        opera status request = 0
        status 4 bytes
        xx xx xx xbus_status
        returns data
      */
    case CDROM_CMD_READ_DATA:
      if((cd_->xbus_status & CDST_TRAY) &&
         (cd_->xbus_status & CDST_DISC) &&
         (cd_->xbus_status & CDST_SPIN))
        {
          cd_->xbus_status |= CDST_RDY;
          cd_->status_len   = 2;
          cd_->status[0]    = CDROM_CMD_READ_DATA;
          cd_->status[1]    = cd_->xbus_status;

          cd_->disc.msf_current.minutes = cd_->cmd[1];
          cd_->disc.msf_current.seconds = cd_->cmd[2];
          cd_->disc.msf_current.frames  = cd_->cmd[3];
          cd_->current_sector           = MSF2LBA(&cd_->disc.msf_current);
          cd_->blocks_requested         = ((cd_->cmd[5] << 8) + cd_->cmd[6]);

          CDROM_SET_SECTOR(cd_->current_sector);
          if(cd_->blocks_requested)
            {
              cd_->current_sector++;
              CDROM_READ_SECTOR(cd_->data);
              cd_->data_len = REQSIZE;
              cd_->blocks_requested--;
            }
          else
            {
              cd_->data_len = 0;
            }

          cd_->MEI_status  = MEI_CDROM_no_error;
          cd_->poll       |= (POLDT | POLST);
        }
      else
        {
          cd_->xbus_status |= CDST_ERRO;
          cd_->xbus_status &= ~CDST_RDY;
          cd_->status_len   = 2;
          cd_->status[0]    = CDROM_CMD_READ_DATA;
          cd_->status[1]    = cd_->xbus_status;
          cd_->MEI_status   = MEI_CDROM_recv_ecc;
          cd_->poll        |= POLST;
        }
      break;

      /*
        data path check
        opera status request  = 2
        MKE                   = 2
        status 4 bytes
        80 AA 55 XS
      */
    case CDROM_CMD_DATA_PATH_CHECK:
      cd_->xbus_status |= CDST_RDY;
      cd_->status_len   = 4;
      cd_->status[0]    = CDROM_CMD_DATA_PATH_CHECK;
      cd_->status[1]    = 0xAA;
      cd_->status[2]    = 0x55;
      cd_->status[3]    = cd_->xbus_status;
      cd_->MEI_status   = MEI_CDROM_no_error;
      cd_->poll        |= POLST;
      break;

      /*
        read error (get last status???)
        opera status request  = 8 ---- tests status req=9?????
        MKE                   = 8!!!
        00
        11
        22   Current status     //MKE / Opera???
        33
        44
        55
        66
        77
        88   Current status     //TEST
      */
    case CDROM_CMD_GET_LAST_STATUS:
      cd_->xbus_status |= CDST_RDY;
      cd_->status_len   = 10;
      cd_->status[0]    = CDROM_CMD_GET_LAST_STATUS;
      cd_->status[1]    = cd_->MEI_status;
      cd_->status[2]    = cd_->MEI_status;
      cd_->status[3]    = cd_->MEI_status;
      cd_->status[4]    = cd_->MEI_status;
      cd_->status[5]    = cd_->MEI_status;
      cd_->status[6]    = cd_->MEI_status;
      cd_->status[7]    = cd_->MEI_status;
      cd_->status[8]    = cd_->MEI_status;
      cd_->status[9]    = cd_->xbus_status; /* 1 == disc present */
      cd_->poll        |= POLST;
      break;

      /*
        read id
        opera status request = 10
        status 12 bytes (3 words)
        MEI text + XS
        00 M E I 1 01 00 00 00 00 00 XS
      */
    case CDROM_CMD_READ_ID:
      cd_->xbus_status |= CDST_RDY;
      cd_->status_len   = 12;
      cd_->status[0]    = CDROM_CMD_READ_ID;
      cd_->status[1]    = 0x00; /* manufacture id */
      cd_->status[2]    = 0x10; /* 10 */
      cd_->status[3]    = 0x00; /* manufacture number */
      cd_->status[4]    = 0x01; /* 01 */
      cd_->status[5]    = 00;
      cd_->status[6]    = 00;
      cd_->status[7]    = 0;    /* revision number */
      cd_->status[8]    = 0;
      cd_->status[9]    = 0x00; /* flag bytes */
      cd_->status[10]   = 0x00;
      cd_->status[11]   = cd_->xbus_status; /* device driver size */
      cd_->MEI_status   = MEI_CDROM_no_error;
      cd_->poll        |= POLST;
      break;

      /*
        mode sense
        not used in opera
        84 mm 00 00 00 00 00.
        status 4 bytes
        xx S1 S2 XS
        xx xx nn XS
      */
    case CDROM_CMD_MODE_SENSE:
      if((cd_->xbus_status & CDST_TRAY) &&
         (cd_->xbus_status & CDST_DISC))
        {
          cd_->xbus_status |= CDST_RDY;
        }
      else
        {
          cd_->xbus_status |= CDST_ERRO;
          cd_->xbus_status &= ~CDST_RDY;
        }

      cd_->status_len  = 4;
      cd_->status[0]   = 0x00;
      cd_->status[1]   = 0x00;
      cd_->status[2]   = 0x00;
      cd_->status[3]   = cd_->xbus_status;
      cd_->poll       |= POLST;
      break;

      /*
        read capacity
        status 8 bytes
        opera status request = 6
        cc cc cc cc cc cc cc XS
        data?
        00 85
        11 mm  total
        22 ss  total
        33 ff  total
        44 ??
        55 ??
        66 ??
      */
    case CDROM_CMD_READ_CAPACITY:
      if((cd_->xbus_status & CDST_TRAY) &&
         (cd_->xbus_status & CDST_DISC) &&
         (cd_->xbus_status & CDST_SPIN))
        {
          cd_->status_len   = 8; //CMD+status+DRVSTAT
          cd_->status[0]    = CDROM_CMD_READ_CAPACITY;
          cd_->status[1]    = 0;
          cd_->status[2]    = cd_->disc.msf_total.minutes;
          cd_->status[3]    = cd_->disc.msf_total.seconds;
          cd_->status[4]    = cd_->disc.msf_total.frames;
          cd_->status[5]    = 0x00;
          cd_->status[6]    = 0x00;
          cd_->xbus_status |= CDST_RDY;
          cd_->status[7]    = cd_->xbus_status;
          cd_->MEI_status   = MEI_CDROM_no_error;
          cd_->poll        |= POLST;
        }
      else
        {
          cd_->xbus_status |= CDST_ERRO;
          cd_->xbus_status &= ~CDST_RDY;
          cd_->status_len   = 2; //CMD+status+DRVSTAT
          cd_->status[0]    = CDROM_CMD_READ_CAPACITY;
          cd_->status[1]    = cd_->xbus_status;
          cd_->poll        |= POLST;
          cd_->MEI_status   = MEI_CDROM_recv_ecc;
        }
      break;

      /*
        read header
        not used in opera
        86 00 ll-bb-aa 00 00.
        86 02 mm-ss-ff 00 00.
        status 8 bytes
        data?
      */
    case CDROM_CMD_READ_HEADER:
      break;

      /*
        read subq
        opera status request = 10
        87 fl 00 00 00 00 00
        fl                   = 0 "lba"
        fl                   = 1 "msf"

        11 00 (if != 00 then break)
        22 Subq_ctl_adr = swapnibles(_11_)
        33 Subq_trk = but2bcd(_22_)
        44 Subq_pnt_idx = byt2bcd(_33_)
        55 mm run tot
        66 ss run tot
        77 ff run tot
        88 mm run trk
        99 ss run trk
        aa ff run trk
      */
    case CDROM_CMD_READ_SUBQ:
      if((cd_->xbus_status & CDST_TRAY) &&
         (cd_->xbus_status & CDST_DISC) &&
         (cd_->xbus_status & CDST_SPIN))
        {
          cd_->xbus_status |= CDST_RDY;
          cd_->status_len   = 12; /* CMD+status+DRVSTAT */
          cd_->status[0]    = CDROM_CMD_READ_SUBQ;
          cd_->status[1]    = 0; /* disc.msf_total.minutes; minutes */
          cd_->status[2]    = 0; /* disc.msf_total.seconds; seconds */
          cd_->status[3]    = 0; /* disc.msf_total.frames; frames  */
          cd_->status[4]    = 0;
          cd_->status[5]    = 0;
          cd_->status[6]    = 0x0;
          cd_->status[7]    = 0x0;
          cd_->status[8]    = 0x0;
          cd_->status[9]    = 0x0;
          cd_->status[10]   = 0x0;
          cd_->status[11]   = cd_->xbus_status;
          cd_->MEI_status   = MEI_CDROM_no_error;
          cd_->poll        |= POLST;
        }
      else
        {
          cd_->xbus_status |= CDST_ERRO;
          cd_->xbus_status &= ~CDST_RDY;
          cd_->status_len   = 2; /* CMD+status+DRVSTAT */
          cd_->status[0]    = CDROM_CMD_READ_SUBQ;
          cd_->status[1]    = cd_->xbus_status;
          cd_->MEI_status   = MEI_CDROM_recv_ecc;
          cd_->poll        |= POLST;
        }
      break;

      /*
        read upc
        not used in opera
        88 00 ll-bb-aa 00 00
        88 02 mm-ss-ff 00 00
        status 20(16) bytes
        data?
      */
    case CDROM_CMD_READ_UPC:
      break;

      /*
        read isrc
        not used in opera
        89 00 ll-bb-aa 00 00
        89 02 mm-ss-ff 00 00
        status 16(15) bytes
        data?
      */
    case CDROM_CMD_READ_ISRC:
      break;

      /*
        read disc code
        ignore it yet...
        opera status request = 10
        8a 00 00 00 00 00 00
        status 10 bytes
        ????? which code???
      */
    case CDROM_CMD_READ_DISC_CODE:
      if((cd_->xbus_status & CDST_TRAY) &&
         (cd_->xbus_status & CDST_DISC) &&
         (cd_->xbus_status & CDST_SPIN))
        {
          cd_->xbus_status |= CDST_RDY;
          cd_->status_len   = 12; /* CMD+status+DRVSTAT */
          cd_->status[0]    = CDROM_CMD_READ_DISC_CODE;
          cd_->status[1]    = 0; /* disc.msf_total.minutes; */
          cd_->status[2]    = 0; /* disc.msf_total.seconds;  */
          cd_->status[3]    = 0; /* disc.msf_total.frames; */
          cd_->status[4]    = 0;
          cd_->status[5]    = 0;
          cd_->status[6]    = 0x00;
          cd_->status[7]    = 0x00;
          cd_->status[8]    = 0x00;
          cd_->status[9]    = 0x00;
          cd_->status[10]   = 0x00;
          cd_->status[11]   = cd_->xbus_status;
          cd_->MEI_status   = MEI_CDROM_no_error;
          cd_->poll        |= POLST;
        }
      else
        {
          cd_->xbus_status |= CDST_ERRO;
          cd_->xbus_status &= ~CDST_RDY;
          cd_->status_len   = 2; /* CMD+status+DRVSTAT */
          cd_->status[0]    = CDROM_CMD_READ_DISC_CODE;
          cd_->status[1]    = cd_->xbus_status;
          cd_->MEI_status   = MEI_CDROM_recv_ecc;
          cd_->poll        |= POLST;
        }
      break;

      /*
        MKE !!!v the same
        read disc information
        opera status request = 6
        8b 00 00 00 00 00 00
        status 8(6) bytes
        read the toc descritor
        00 11 22 33 44 55 XS
        00 = 8b                 // command code
        11 = Disc ID            // XA_BYTE
        22 = 1st track#
        33 = last track#
        44 = minutes
        55 = seconds
        66 = frames
      */
    case CDROM_CMD_READ_DISC_INFO:
      if((cd_->xbus_status & CDST_TRAY) &&
         (cd_->xbus_status & CDST_DISC) &&
         (cd_->xbus_status & CDST_SPIN))
        {
          cd_->xbus_status |= CDST_RDY;
          cd_->status_len   = 8;
          cd_->status[0]    = CDROM_CMD_READ_DISC_INFO;
          cd_->status[1]    = cd_->disc.disc_id;
          cd_->status[2]    = cd_->disc.track_first;
          cd_->status[3]    = cd_->disc.track_last;
          cd_->status[4]    = cd_->disc.msf_total.minutes;
          cd_->status[5]    = cd_->disc.msf_total.seconds;
          cd_->status[6]    = cd_->disc.msf_total.frames;
          cd_->status[7]    = cd_->xbus_status;
          cd_->MEI_status   = MEI_CDROM_no_error;
        }
      else
        {
          cd_->status_len   = 2;
          cd_->xbus_status |= CDST_ERRO;
          cd_->status[0]    = CDROM_CMD_READ_DISC_INFO;
          cd_->status[1]    = cd_->xbus_status;
          cd_->MEI_status   = MEI_CDROM_recv_ecc;
        }

      cd_->poll |= POLST;
      break;

      /*
        read toc
        MKE !!!v the same
        opera status request = 8
        8c fl nn 00 00 00 00    // reads nn entry
        status 12(8) bytes
        00 11 22 33 44 55 66 77 XS
        00 = 8c
        11 = reserved0;         //NIX BYTE
        22 = addressAndControl; //TOCENT_CTL_ADR=swapnibbles(11) ??? UPCCTLADR=_10_ | x02 (_11_ &F0 = _10_)
        33 = trackNumber;       //TOC_ENT NUMBER
        44 = reserved3;         //TOC_ENT FORMAT
        55 = minutes;           //TOC_ENT ADRESS == 0x00445566
        66 = seconds;
        77 = frames;
        88 = reserved7;
      */
    case CDROM_CMD_READ_TOC:
      if((cd_->xbus_status & CDST_TRAY) &&
         (cd_->xbus_status & CDST_DISC) &&
         (cd_->xbus_status & CDST_SPIN))
        {
          toc_entry_t *toc = &cd_->disc.disc_toc[cd_->cmd[2]];

          cd_->xbus_status |= CDST_RDY;
          cd_->status_len   = 10; /* CMD+status+DRVSTAT */
          cd_->status[0]    = CDROM_CMD_READ_TOC;
          cd_->status[1]    = toc->res0;
          cd_->status[2]    = toc->CDCTL;
          cd_->status[3]    = toc->track_number;
          cd_->status[4]    = toc->res1;
          cd_->status[5]    = toc->minutes;
          cd_->status[6]    = toc->seconds;
          cd_->status[7]    = toc->frames;
          cd_->status[8]    = toc->res2;
          cd_->status[9]    = cd_->xbus_status;
          cd_->MEI_status   = MEI_CDROM_no_error;
        }
      else
        {
          cd_->status_len   = 2;
          cd_->xbus_status |= CDST_ERRO;
          cd_->status[0]    = CDROM_CMD_READ_TOC;
          cd_->status[1]    = cd_->xbus_status;
          cd_->MEI_status   = MEI_CDROM_recv_ecc;
        }

      cd_->poll |= POLST;
      break;

      /*
        read session information
        MKE !!!v the same
        opera status request  = 6
        status 8(6)
        00 11 22 33 44 55 XS ==
        00 = 8d
        11 = valid;             // 0x80 = MULTISESS
        22 = minutes;
        33 = seconds;
        44 = frames;
        55 = rfu1;              //ignore
        66 = rfu2               //ignore
      */
    case CDROM_CMD_READ_SESSION_INFO:
      if((cd_->xbus_status & CDST_TRAY) &&
         (cd_->xbus_status & CDST_DISC))
        {
          cd_->xbus_status |= CDST_RDY;
          cd_->status_len   = 8; /* CMD+status+DRVSTAT */
          cd_->status[0]    = CDROM_CMD_READ_SESSION_INFO;
          cd_->status[1]    = 0x00;
          cd_->status[2]    = cd_->disc.msf_session.minutes;
          cd_->status[3]    = cd_->disc.msf_session.seconds;
          cd_->status[4]    = cd_->disc.msf_session.frames;
          cd_->status[5]    = 0x00;
          cd_->status[6]    = 0x00;
          cd_->status[7]    = cd_->xbus_status;
          cd_->MEI_status   = MEI_CDROM_no_error;
        }
      else
        {
          cd_->status_len   = 2; //CMD+status+DRVSTAT
          cd_->xbus_status |= CDST_ERRO;
          cd_->status[0]    = CDROM_CMD_READ_SESSION_INFO;
          cd_->status[1]    = cd_->xbus_status;
          cd_->MEI_status   = MEI_CDROM_recv_ecc;
        }

      cd_->poll |= POLST;
      break;

      /* read device driver */
    case CDROM_CMD_READ_DEVICE_DRIVER:
      break;

      /* ????? */
    case CDROM_CMD_UNKNOWN_0x93:
      if((cd_->xbus_status & CDST_TRAY) &&
         (cd_->xbus_status & CDST_DISC))
        {
          cd_->xbus_status |= CDST_RDY;
        }
      else
        {
          cd_->xbus_status |= CDST_ERRO;
          cd_->xbus_status |= CDST_RDY;
        }

      cd_->status_len = 4;
      cd_->status[0]  = 0x0;
      cd_->status[1]  = 0x0;
      cd_->status[2]  = 0x0;
      cd_->status[3]  = cd_->xbus_status;

      cd_->poll |= POLST;
      break;

      /* error: shouldn't happen */
    default:
      break;
    }
}

void
freedo_cdrom_send_cmd(cdrom_device_t *cd_,
                      uint8_t         val_)
{
  if(cd_->cmd_idx < 7)
    cd_->cmd[cd_->cmd_idx++] = (uint8_t)val_;

  if((cd_->cmd_idx >= 7) || (cd_->cmd[0] == 0x08))
    {
      freedo_cdrom_do_cmd(cd_);
      cd_->cmd_idx = 0;
    }
}

bool
freedo_cdrom_test_fiq(cdrom_device_t *cd_)
{
  return (((cd_->poll & POLST) && (cd_->poll & POLSTMASK)) ||
          ((cd_->poll & POLDT) && (cd_->poll & POLDTMASK)));
}

void
freedo_cdrom_set_poll(cdrom_device_t *cd_,
                      uint32_t        val_)
{
  cd_->poll = ((cd_->poll & 0xF0) | (val_ & 0x0F));
}

uint8_t
freedo_cdrom_fifo_get_data(cdrom_device_t *cd_)
{
  uint8_t rv;

  rv = 0;
  if(cd_->data_len > 0)
    {
      rv = cd_->data[cd_->data_idx];
      cd_->data_idx++;
      cd_->data_len--;

      if(cd_->data_len == 0)
        {
          cd_->data_idx = 0;
          if(cd_->blocks_requested)
            {
              CDROM_SET_SECTOR(cd_->current_sector++);
              CDROM_READ_SECTOR(cd_->data);
              cd_->data_len = REQSIZE;
              cd_->blocks_requested--;
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

  return rv;
}
