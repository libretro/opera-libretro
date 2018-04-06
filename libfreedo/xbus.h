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

/* xbus.h: interface for the CXBUS class. */

#ifndef LIBFREEDO_XBUS_H_INCLUDED
#define LIBFREEDO_XBUS_H_INCLUDED

#include "iso.h"

#include "extern_c.h"

EXTERN_C_BEGIN

typedef void* (*freedo_xbus_device)(int, void*);

void     freedo_xbus_init(freedo_xbus_device zero_dev);
void     freedo_xbus_destroy(void);

int      freedo_xbus_attach(freedo_xbus_device dev);

void     freedo_xbus_device_load(int dev, const char *name);
void     freedo_xbus_device_eject(int dev);

void     freedo_xbus_set_sel(const uint32_t val_);
uint32_t freedo_xbus_get_res(void);

void     freedo_xbus_set_poll(const uint32_t val_);
uint32_t freedo_xbus_get_poll(void);

void     freedo_xbus_set_cmd_FIFO(const uint32_t val_);
uint32_t freedo_xbus_get_status_FIFO(void);

void     freedo_xbus_set_data_FIFO(const uint32_t val_);
uint32_t freedo_xbus_get_data_FIFO(void);

uint32_t freedo_xbus_state_size(void);
void     freedo_xbus_state_save(void *buf_);
void     freedo_xbus_state_load(const void *buf_);

EXTERN_C_END

#endif
