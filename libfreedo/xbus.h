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

/* xbus.h: interface for the XBUS class. */

#ifndef LIBFREEDO_XBUS_H_INCLUDED
#define LIBFREEDO_XBUS_H_INCLUDED

#include "extern_c.h"

#define XBP_INIT	 0	//plugin init, returns plugin version
#define XBP_RESET	 1	//plugin reset with parameter(image path)
#define XBP_SET_COMMAND  2	//XBUS
#define XBP_FIQ		 3	//check interrupt form device
#define XBP_SET_DATA     4	//XBUS
#define XBP_GET_DATA     5	//XBUS
#define XBP_GET_STATUS   6	//XBUS
#define XBP_SET_POLL     7	//XBUS
#define XBP_GET_POLL     8	//XBUS
#define XBP_SELECT	 9      //selects device by Opera
#define XBP_RESERV	 10     //reserved reading from device
#define XBP_DESTROY	 11     //plugin destroy
#define XBP_GET_SAVESIZE 19	//save support from emulator side
#define XBP_GET_SAVEDATA 20
#define XBP_SET_SAVEDATA 21

EXTERN_C_BEGIN

typedef void* (*freedo_xbus_device)(int, void*);

void     freedo_xbus_init(freedo_xbus_device zero_dev_);
void     freedo_xbus_destroy(void);

int      freedo_xbus_attach(freedo_xbus_device dev);

void     freedo_xbus_device_load(int dev, const char *name);
void     freedo_xbus_device_eject(int dev);

void     freedo_xbus_set_sel(const uint32_t val_);
uint32_t freedo_xbus_get_res(void);

void     freedo_xbus_set_poll(const uint32_t val_);
uint32_t freedo_xbus_get_poll(void);

void     freedo_xbus_fifo_set_cmd(const uint32_t val_);
uint32_t freedo_xbus_fifo_get_status(void);

void     freedo_xbus_fifo_set_data(const uint32_t val_);
uint32_t freedo_xbus_fifo_get_data(void);

uint32_t freedo_xbus_state_size(void);
void     freedo_xbus_state_save(void *buf_);
void     freedo_xbus_state_load(const void *buf_);

EXTERN_C_END

#endif
