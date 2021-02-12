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

void
xbus_execute_command_f(void)
{
  if(XBUS.cmdf[0] == 0x83)
    {
      XBUS.stlenf      = 12;
      XBUS.stdevf[0]   = 0x83;
      XBUS.stdevf[1]   = 0x01;
      XBUS.stdevf[2]   = 0x01;
      XBUS.stdevf[3]   = 0x01;
      XBUS.stdevf[4]   = 0x01;
      XBUS.stdevf[5]   = 0x01;
      XBUS.stdevf[6]   = 0x01;
      XBUS.stdevf[7]   = 0x01;
      XBUS.stdevf[8]   = 0x01;
      XBUS.stdevf[9]   = 0x01;
      XBUS.stdevf[10]  = 0x01;
      XBUS.stdevf[11]  = 0x01;
      XBUS.poldevf    |= POLST;
    }

  if(((XBUS.poldevf & POLST)      &&
      (XBUS.poldevf & POLSTMASK)) ||
     ((XBUS.poldevf & POLDT)      &&
      (XBUS.poldevf & POLDTMASK)))
    opera_clio_fiq_generate(4,0);
}

void
opera_xbus_fifo_set_cmd(const uint32_t val_)
{
  if(xdev[XBUS.xb_sel_l])
    {
      xdev[XBUS.xb_sel_l](XBP_SET_COMMAND,(void*)(uintptr_t)val_);
      if(xdev[XBUS.xb_sel_l](XBP_FIQ,NULL))
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
  if(xdev[XBUS.xb_sel_l])
    return (uintptr_t)xdev[XBUS.xb_sel_l](XBP_GET_DATA,NULL);

  return 0;
}

uint32_t
opera_xbus_get_poll(void)
{
  uint32_t res = 0x30;

  if(XBUS.xb_sel_l == 0x0F)
    res = XBUS.polf;
  else if(xdev[XBUS.xb_sel_l])
    res = (uintptr_t)xdev[XBUS.xb_sel_l](XBP_GET_POLL,NULL);

  if(XBUS.xb_sel_h & 0x80)
    res &= 0x0F;

  return res;
}

uint32_t
opera_xbus_get_res(void)
{
  if(xdev[XBUS.xb_sel_l])
    return (uintptr_t)xdev[XBUS.xb_sel_l](XBP_RESERV,NULL);
  return 0;
}


uint32_t
opera_xbus_fifo_get_status(void)
{
  uint32_t rv;

  rv = 0;
  if(xdev[XBUS.xb_sel_l])
    {
      rv = (uintptr_t)xdev[XBUS.xb_sel_l](XBP_GET_STATUS,NULL);
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
  if(xdev[XBUS.xb_sel_l])
    xdev[XBUS.xb_sel_l](XBP_SET_DATA,(void*)(uintptr_t)val_);
}

void
opera_xbus_set_poll(const uint32_t val_)
{
  if(XBUS.xb_sel_l == 0x0F)
    {
      XBUS.polf &= 0xF0;
      XBUS.polf |= (val_ & 0x0F);
    }

  if(xdev[XBUS.xb_sel_l])
    {
      xdev[XBUS.xb_sel_l](XBP_SET_POLL,(void*)(uintptr_t)val_);
      if(xdev[XBUS.xb_sel_l](XBP_FIQ,NULL))
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
  int i;

  XBUS.polf = 0x0F;

  for(i = 0; i < 15; i++)
    xdev[i] = NULL;

  opera_xbus_attach(zero_dev_);
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
}

uint32_t
opera_xbus_state_size(void)
{
  int i;
  uint32_t tmp = sizeof(xbus_datum_t);

  tmp += (16 * 4);
  for(i = 0; i < 15; i++)
    {
      if(!xdev[i])
        continue;
      tmp += (uintptr_t)xdev[i](XBP_GET_SAVESIZE,NULL);
    }

  return tmp;
}

void opera_xbus_state_save(void *buf_)
{
  uint32_t i;
  uint32_t j;
  uint32_t off;
  uint32_t tmp;

  memcpy(buf_,&XBUS,sizeof(xbus_datum_t));

  j = off = sizeof(xbus_datum_t);
  off += (16 * 4);

  for(i = 0; i < 15; i++)
    {
      if(!xdev[i])
        {
          tmp = 0;
          memcpy(&((uint8_t*)buf_)[j+i*4],&tmp,4);
        }
      else
        {
          xdev[i](XBP_GET_SAVEDATA,&((uint8_t*)buf_)[off]);
          memcpy(&((uint8_t*)buf_)[j+i*4],&off,4);
          off += (uintptr_t)xdev[i](XBP_GET_SAVESIZE,NULL);
        }
    }
}

void
opera_xbus_state_load(const void *buf_)
{
  uint32_t i;
  uint32_t j;
  uint32_t offd;

  j = sizeof(xbus_datum_t);

  memcpy(&XBUS,buf_,j);

  for(i = 0; i < 15; i++)
    {
      memcpy(&offd,&((uint8_t*)buf_)[j+i*4],4);

      if(!xdev[i])
        continue;

      if(!offd)
        xdev[i](XBP_RESET,NULL);
      else
        xdev[i](XBP_SET_SAVEDATA,&((uint8_t*)buf_)[offd]);
    }
}
