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

#ifndef	LIBOPERA_CLIO_H_INCLUDED
#define LIBOPERA_CLIO_H_INCLUDED

#include "extern_c.h"

#include <stdint.h>

EXTERN_C_BEGIN

void     opera_clio_init(int reason_);
void     opera_clio_reset(void);

uint32_t opera_clio_line_vint0(void);
uint32_t opera_clio_line_vint1(void);

int      opera_clio_fiq_needed(void);
void     opera_clio_fiq_generate(uint32_t reason1_, uint32_t reason2_);

void     opera_clio_fifo_write(uint32_t addr_, uint32_t val_);
uint32_t opera_clio_fifo_read(uint32_t addr_);
void     opera_clio_fifo_eo(uint16_t channel_, uint16_t val_);
uint16_t opera_clio_fifo_ei(uint16_t channel_);
uint16_t opera_clio_fifo_eo_status(uint8_t channel_);
uint16_t opera_clio_fifo_ei_status(uint8_t channel_);
uint16_t opera_clio_fifo_ei_read(uint16_t channel_);

uint32_t opera_clio_peek(uint32_t addr_);
int      opera_clio_poke(uint32_t addr_, uint32_t val_);

void     opera_clio_vcnt_update(int line_, int half_frame_);

uint32_t opera_clio_timer_get_delay(void);
void     opera_clio_timer_execute(void);

uint32_t opera_clio_state_size(void);
void     opera_clio_state_save(void *buf_);
void     opera_clio_state_load(const void *buf_);

EXTERN_C_END

#endif /* LIBOPERA_CLIO_H_INCLUDED */
