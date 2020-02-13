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

#ifndef	LIBOPERA_MADAM_H_INCLUDED
#define LIBOPERA_MADAM_H_INCLUDED

#include "extern_c.h"

#define FSM_IDLE 1
#define FSM_INPROCESS 2
#define FSM_SUSPENDED 3

EXTERN_C_BEGIN

void      opera_madam_init(uint8_t *mem_);
void      opera_madam_reset(void);

uint32_t  opera_madam_fsm_get(void);
void      opera_madam_fsm_set(uint32_t val_);

void      opera_madam_cel_handle(void);

uint32_t *opera_madam_registers(void);

void      opera_madam_poke(uint32_t addr_, uint32_t val_);
uint32_t  opera_madam_peek(uint32_t addr_);

void      opera_madam_kprint_enable(void);
void      opera_madam_kprint_disable(void);
void      opera_madam_me_mode_software(void);
void      opera_madam_me_mode_hardware(void);

uint32_t  opera_madam_state_size(void);
void      opera_madam_state_save(void *buf_);
void      opera_madam_state_load(const void *buf_);

EXTERN_C_END

#endif /* LIBOPERA_MADAM_H_INCLUDED */
