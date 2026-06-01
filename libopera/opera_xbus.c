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

#include "opera_clio.h"
#include "opera_state.h"
#include "opera_xbus.h"

#include <stdint.h>
#include <string.h>

#define POLSTMASK 0x01
#define POLDTMASK 0x02
#define POLMAMASK 0x04
#define POLREMASK 0x08
#define POLST	  0x10
#define POLDT	  0x20
#define POLMA	  0x40
#define POLRE	  0x80
#define POL_INTEN_MASK (POLSTMASK | POLDTMASK | POLMAMASK)
#define POL_VALID_MASK (POLST | POLDT | POLMA)

#define XBUS_CMD_READ_ID 0x83
#define XBUS_ID_LEN      12
#define XBUS_DEVICE_CD   0x01
#define XBUS_STATUS_OK   0xE1

struct xbus_datum_s
{
  uint8_t xb_sel_l;
  uint8_t xb_sel_h;
  uint8_t polf;
  uint8_t poldevf;
  uint8_t stdevf[255]; // status of devices
  uint8_t stlenf; // pointer in FIFO
  uint8_t cmdf[7];
  uint8_t cmdptrf;
};

typedef struct xbus_datum_s xbus_datum_t;

static xbus_datum_t      XBUS;
static opera_xbus_device xdev[16];
static int               g_XBUS_LEGACY_NO_DEVICE_ABORT = 0;

static
opera_xbus_device
xbus_selected_device(void)
{
  return xdev[XBUS.xb_sel_l];
}

static
uint8_t
xbus_poll_f(void)
{
  return ((XBUS.polf & 0x0F) | (XBUS.poldevf & 0xF0));
}

static
int
xbus_poll_fiq_pending(uint8_t poll_)
{
  return (((poll_ & POLST) && (poll_ & POLSTMASK)) ||
          ((poll_ & POLDT) && (poll_ & POLDTMASK)) ||
          ((poll_ & POLMA) && (poll_ & POLMAMASK)));
}

static
void
xbus_clear_fifos_f(void)
{
  XBUS.stlenf = 0;
  XBUS.cmdptrf = 0;
  memset(XBUS.stdevf,0,sizeof(XBUS.stdevf));
  memset(XBUS.cmdf,0,sizeof(XBUS.cmdf));
  XBUS.poldevf &= ~POL_VALID_MASK;
}

int
opera_xbus_selected_device_absent(void)
{
  if(XBUS.xb_sel_l == 0x0F)
    return 0;

  if(g_XBUS_LEGACY_NO_DEVICE_ABORT)
    return 0;

  return (xbus_selected_device() == NULL);
}

void
opera_xbus_set_legacy_no_device_abort(int enabled_)
{
  g_XBUS_LEGACY_NO_DEVICE_ABORT = (enabled_ != 0);
}

int
opera_xbus_legacy_no_device_abort(void)
{
  return g_XBUS_LEGACY_NO_DEVICE_ABORT;
}

static
void
xbus_set_read_id_status_f(void)
{
  memset(XBUS.stdevf,0,XBUS_ID_LEN);

  XBUS.stlenf    = XBUS_ID_LEN;
  XBUS.stdevf[0] = XBUS_CMD_READ_ID;
  XBUS.stdevf[2] = 0x10;            /* MEI manufacturer ID */
  XBUS.stdevf[4] = XBUS_DEVICE_CD;  /* CD-ROM device number */
  XBUS.stdevf[11] = XBUS_STATUS_OK;
  XBUS.poldevf  |= POLST;
}

void
xbus_execute_command_f(void)
{
  if(XBUS.cmdf[0] == XBUS_CMD_READ_ID)
    xbus_set_read_id_status_f();

  if(xbus_poll_fiq_pending(xbus_poll_f()))
    opera_clio_fiq_generate(4,0);
}

void
opera_xbus_fifo_set_cmd(const uint32_t val_)
{
  opera_xbus_device dev;

  dev = xbus_selected_device();
  if(dev)
    {
      dev(XBP_SET_COMMAND,(void*)(uintptr_t)val_);
      if(dev(XBP_FIQ,NULL))
        opera_clio_fiq_generate(4,0);
    }
  else if(XBUS.xb_sel_l == 0x0F)
    {
      if(XBUS.cmdptrf < 7)
        {
          XBUS.cmdf[XBUS.cmdptrf] = (uint8_t)val_;
          XBUS.cmdptrf++;
        }

      if(XBUS.cmdptrf >= 7)
        {
          xbus_execute_command_f();
          XBUS.cmdptrf = 0;
        }
    }
}

uint32_t
opera_xbus_fifo_get_data(void)
{
  opera_xbus_device dev;

  dev = xbus_selected_device();
  if(dev)
    return (uintptr_t)dev(XBP_GET_DATA,NULL);

  return 0;
}

void
opera_xbus_tick(void)
{
  unsigned i;
  int fiq;

  fiq = 0;
  for(i = 0; i < 16; i++)
    {
      if(xdev[i] && xdev[i](XBP_FIQ,NULL))
        fiq = 1;
    }

  if(fiq)
    opera_clio_fiq_generate(4,0);
  else if((XBUS.xb_sel_l == 0x0F) && xbus_poll_fiq_pending(xbus_poll_f()))
    opera_clio_fiq_generate(4,0);
}

uint32_t
opera_xbus_get_poll(void)
{
  opera_xbus_device dev;
  uint32_t res = 0;

  dev = xbus_selected_device();
  if(XBUS.xb_sel_l == 0x0F)
    res = xbus_poll_f();
  else if(dev)
    res = (uintptr_t)dev(XBP_GET_POLL,NULL);
  else if(g_XBUS_LEGACY_NO_DEVICE_ABORT)
    res = 0x30;

  if(XBUS.xb_sel_h & 0x80)
    res &= 0x0F;

  return res;
}

uint32_t
opera_xbus_get_res(void)
{
  opera_xbus_device dev;

  dev = xbus_selected_device();
  if(dev)
    return (uintptr_t)dev(XBP_RESERV,NULL);
  return 0;
}


uint32_t
opera_xbus_fifo_get_status(void)
{
  opera_xbus_device dev;
  uint32_t rv;

  rv = 0;
  dev = xbus_selected_device();
  if(dev)
    {
      rv = (uintptr_t)dev(XBP_GET_STATUS,NULL);
    }
  else if(XBUS.xb_sel_l == 0x0F)
    {
      if(XBUS.stlenf > 0)
        {
          rv = XBUS.stdevf[0];
          XBUS.stlenf--;
          if(XBUS.stlenf > 0)
            {
              int i;
              for(i = 0; i < XBUS.stlenf; i++)
                XBUS.stdevf[i] = XBUS.stdevf[i+1];
            }
          else
            {
              XBUS.poldevf &= ~POLST;
            }
        }
    }

  return rv;
}

void
opera_xbus_fifo_set_data(const uint32_t val_)
{
  opera_xbus_device dev;

  dev = xbus_selected_device();
  if(dev)
    dev(XBP_SET_DATA,(void*)(uintptr_t)val_);
}

void
opera_xbus_set_poll(const uint32_t val_)
{
  opera_xbus_device dev;

  dev = xbus_selected_device();
  if(XBUS.xb_sel_l == 0x0F)
    {
      if(val_ & POLREMASK)
        xbus_clear_fifos_f();
      if(val_ & POLRE)
        XBUS.poldevf &= ~POLRE;
      XBUS.polf &= 0xF0;
      XBUS.polf |= (val_ & POL_INTEN_MASK);
      if(xbus_poll_fiq_pending(xbus_poll_f()))
        opera_clio_fiq_generate(4,0);
    }

  if(dev)
    {
      dev(XBP_SET_POLL,(void*)(uintptr_t)val_);
      if(dev(XBP_FIQ,NULL))
        opera_clio_fiq_generate(4,0);
    }
}

void opera_xbus_set_sel(const uint32_t val_)
{
  XBUS.xb_sel_l = ((uint8_t)val_ & 0x0F);
  XBUS.xb_sel_h = ((uint8_t)val_ & 0xF0);
}

void
opera_xbus_init(opera_xbus_device zero_dev_)
{
  memset(&XBUS,0,sizeof(XBUS));
  memset(xdev,0,sizeof(xdev));
  g_XBUS_LEGACY_NO_DEVICE_ABORT = 0;
  XBUS.polf = POL_INTEN_MASK;

  opera_xbus_attach(zero_dev_);
}

void
opera_xbus_reset(void)
{
  opera_xbus_device dev[16];
  unsigned          i;

  memcpy(dev,xdev,sizeof(dev));
  memset(&XBUS,0,sizeof(XBUS));
  g_XBUS_LEGACY_NO_DEVICE_ABORT = 0;
  XBUS.polf = POL_INTEN_MASK;

  for(i = 0; i < 16; i++)
    {
      if(dev[i])
        dev[i](XBP_RESET,NULL);
    }

  memcpy(xdev,dev,sizeof(xdev));
}

int
opera_xbus_attach(opera_xbus_device dev_)
{
  int i;

  for(i = 0; i < 16; i++)
    {
      if(!xdev[i])
        break;
    }

  if(i == 16)
    return -1;

  xdev[i] = dev_;
  xdev[i](XBP_INIT,NULL);

  return i;
}

void
opera_xbus_device_load(int   dev_,
                       const char *name_)
{
  xdev[dev_](XBP_RESET,(void*)name_);
}

void opera_xbus_device_eject(int dev_)
{
  xdev[dev_](XBP_RESET,NULL);
}

void
opera_xbus_destroy(void)
{
  unsigned i;

  for(i = 0; i < 16; i++)
    {
      if(xdev[i])
        {
          xdev[i](XBP_DESTROY,NULL);
          xdev[i] = NULL;
        }
    }

  memset(&XBUS,0,sizeof(XBUS));
  g_XBUS_LEGACY_NO_DEVICE_ABORT = 0;
}

uint32_t
opera_xbus_state_size_v1(void)
{
  int i;
  uint32_t size = sizeof(xbus_datum_t);

  size = opera_state_save_size(sizeof(xbus_datum_t));

  for(i = 0; i < 15; i++)
    {
      uint32_t device_size;

      if(!xdev[i])
        continue;
      device_size = (uintptr_t)xdev[i](XBP_GET_SAVESIZE,NULL);
      if(device_size == 0)
        return 0;
      size += device_size;
    }

  return size;
}

static
bool
xbus_state_write_payload(opera_state_writer_t *writer_)
{
  uint32_t i;
  uint32_t device_count;

  if(!opera_state_write_u8(writer_,XBUS.xb_sel_l) ||
     !opera_state_write_u8(writer_,XBUS.xb_sel_h) ||
     !opera_state_write_u8(writer_,XBUS.polf) ||
     !opera_state_write_u8(writer_,XBUS.poldevf) ||
     !opera_state_write_bytes(writer_,XBUS.stdevf,sizeof(XBUS.stdevf)) ||
     !opera_state_write_u8(writer_,XBUS.stlenf) ||
     !opera_state_write_bytes(writer_,XBUS.cmdf,sizeof(XBUS.cmdf)) ||
     !opera_state_write_u8(writer_,XBUS.cmdptrf))
    return false;

  device_count = 0;
  for(i = 0; i < 15; i++)
    if(xdev[i])
      device_count++;
  if(!opera_state_write_u32(writer_,device_count))
    return false;

  for(i = 0; i < 15; i++)
    {
      uint32_t device_size;
      void *device_data;

      if(!xdev[i])
        continue;

      device_size = (uintptr_t)xdev[i](XBP_GET_SAVESIZE,NULL);
      if(device_size == 0)
        {
          writer_->failed = true;
          return false;
        }
      if(!opera_state_write_u8(writer_,(uint8_t)i))
        return false;
      device_data = opera_state_write_reserve(writer_,device_size);
      if(writer_->failed)
        return false;
      if((device_data != NULL) &&
         ((uintptr_t)xdev[i](XBP_GET_SAVEDATA,device_data) != device_size))
        {
          writer_->failed = true;
          return false;
        }
    }

  return true;
}

static
uint32_t
xbus_state_payload_size(void)
{
  opera_state_writer_t writer;

  opera_state_writer_init(&writer,NULL,UINT32_MAX);
  xbus_state_write_payload(&writer);

  return opera_state_writer_ok(&writer) ? opera_state_writer_used(&writer) : 0;
}

uint32_t
opera_xbus_state_size(void)
{
  uint32_t payload_size;

  payload_size = xbus_state_payload_size();
  if(payload_size == 0)
    return 0;

  return opera_state_chunk_size(payload_size);
}

uint32_t
opera_xbus_state_save(void *data_)
{
  uint32_t payload_size;
  opera_state_writer_t writer;

  payload_size = xbus_state_payload_size();
  if(payload_size == 0)
    return 0;

  opera_state_writer_init(&writer,data_,opera_state_chunk_size(payload_size));
  opera_state_write_chunk_header(&writer,"XBUS",payload_size);
  xbus_state_write_payload(&writer);

  return opera_state_writer_ok(&writer) ? opera_state_writer_used(&writer) : 0;
}

uint32_t
opera_xbus_state_load_v1(void const     *data_,
                         uint32_t const  data_size_)
{
  uint32_t i;
  uint32_t size;
  uint32_t remaining;
  uint32_t rv;
  uint8_t const *data;
  opera_xbus_savedata_t savedata;

  data = (uint8_t const*)data_;

  size = opera_state_load_sized(&XBUS,"XBUS",data,data_size_,sizeof(XBUS));
  if(size == 0)
    return 0;

  for(i = 0; i < 15; i++)
    {
      if(!xdev[i])
        break;

      xdev[i](XBP_RESET,NULL);
      if(size > data_size_)
        return 0;
      remaining = data_size_ - size;
      savedata.data = &data[size];
      savedata.size = remaining;
      rv = (uintptr_t)xdev[i](XBP_SET_SAVEDATA,&savedata);
      if((rv == 0) || (rv > remaining))
        return 0;
      size += rv;
    }

  return size;
}

uint32_t
opera_xbus_state_load(void const     *data_,
                      uint32_t const  data_size_)
{
  uint32_t i;
  uint32_t device_count;
  xbus_datum_t state;
  opera_state_reader_t reader;
  opera_state_reader_t payload;
  opera_xbus_savedata_t savedata;

  opera_state_reader_init(&reader,data_,data_size_);
  if(!opera_state_read_chunk(&reader,"XBUS",&payload) ||
     !opera_state_read_u8(&payload,&state.xb_sel_l) ||
     !opera_state_read_u8(&payload,&state.xb_sel_h) ||
     !opera_state_read_u8(&payload,&state.polf) ||
     !opera_state_read_u8(&payload,&state.poldevf) ||
     !opera_state_read_bytes(&payload,state.stdevf,sizeof(state.stdevf)) ||
     !opera_state_read_u8(&payload,&state.stlenf) ||
     !opera_state_read_bytes(&payload,state.cmdf,sizeof(state.cmdf)) ||
     !opera_state_read_u8(&payload,&state.cmdptrf) ||
     !opera_state_read_u32(&payload,&device_count) ||
     (device_count > 15))
    return 0;

  XBUS = state;

  for(i = 0; i < device_count; i++)
    {
      uint8_t slot;
      uint32_t remaining;
      uint32_t rv;

      if(!opera_state_read_u8(&payload,&slot) ||
         (slot >= 15) ||
         (xdev[slot] == NULL))
        return 0;

      xdev[slot](XBP_RESET,NULL);
      remaining = opera_state_reader_remaining(&payload);
      savedata.data = &payload.data[payload.offset];
      savedata.size = remaining;
      rv = (uintptr_t)xdev[slot](XBP_SET_SAVEDATA,&savedata);
      if((rv == 0) || (rv > remaining))
        return 0;
      payload.offset += rv;
    }

  if(!opera_state_reader_finished(&payload))
    return 0;

  return opera_state_reader_used(&reader);
}
