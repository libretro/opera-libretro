/*
  www.freedo.org
  The first and only working 3DO multiplayer emulator.

  The FreeDO licensed under modified GNU LGPL, with following notes:

  *   The owners and original authors of the FreeDO have full right to develop closed source derivative work.
  *   Any non-commercial uses of the FreeDO sources or any knowledge obtained by studying or reverse engineering
  of the sources, or any other material published by FreeDO have to be accompanied with full credits.
  *   Any commercial uses of FreeDO sources or any knowledge obtained by studying or reverse engineering of the sources,
  or any other material published by FreeDO is strictly forbidden without owners approval.

  The above notes are taking precedence over GNU LGPL in conflicting situations.

  Project authors:

  Alexander Troosh
  Maxim Grishin
  Allen Wright
  John Sammons
  Felix Lazarev
*/

// Madam.h: interface for the CMadam class.
//
//////////////////////////////////////////////////////////////////////

#ifndef	MADAM_3DO_HEADER
#define MADAM_3DO_HEADER

#include "extern_c.h"

#define FSM_IDLE 1
#define FSM_INPROCESS 2
#define FSM_SUSPENDED 3

#define MADAM_PBUS_CONTROLLER1_OFFSET 0x02
#define MADAM_PBUS_CONTROLLER2_OFFSET 0x06
#define MADAM_PBUS_CONTROLLER3_OFFSET 0x04
#define MADAM_PBUS_CONTROLLER4_OFFSET 0x0A
#define MADAM_PBUS_CONTROLLER5_OFFSET 0x08
#define MADAM_PBUS_CONTROLLER6_OFFSET 0x0E
#define MADAM_PBUS_CONTROLLER7_OFFSET 0x12
#define MADAM_PBUS_CONTROLLER8_OFFSET 0x10

#define MADAM_PBUS_BYTE0_SHIFT_L        0x02
#define MADAM_PBUS_BYTE0_SHIFT_R        0x03
#define MADAM_PBUS_BYTE0_SHIFT_X        0x04
#define MADAM_PBUS_BYTE0_SHIFT_P        0x05
#define MADAM_PBUS_BYTE0_SHIFT_C        0x06
#define MADAM_PBUS_BYTE0_SHIFT_B        0x07
#define MADAM_PBUS_BYTE1_SHIFT_A        0x00
#define MADAM_PBUS_BYTE1_SHIFT_LEFT     0x01
#define MADAM_PBUS_BYTE1_SHIFT_RIGHT    0x02
#define MADAM_PBUS_BYTE1_SHIFT_UP       0x03
#define MADAM_PBUS_BYTE1_SHIFT_DOWN     0x04
#define MADAM_PBUS_BYTE1_CONNECTED_MASK 0x80

EXTERN_C_BEGIN

void      freedo_madam_init(uint8_t *mem_);
void      freedo_madam_reset(void);

uint32_t  freedo_madam_fsm_get(void);
void      freedo_madam_fsm_set(uint32_t val_);

void      freedo_madam_mapping_set(uint32_t flag_);

uint32_t  freedo_madam_cel_get_cycles(void);
uint32_t  freedo_madam_cel_handle(void);

uint32_t *freedo_madam_registers(void);

uint8_t  *freedo_madam_pbus_data_get(void);
uint8_t  *freedo_madam_pbus_data_reset(void);

void      freedo_madam_poke(uint32_t addr_, uint32_t val_);
uint32_t  freedo_madam_peek(uint32_t addr_);

uint32_t  freedo_madam_state_size(void);
void      freedo_madam_state_save(void *buf_);
void      freedo_madam_state_load(const void *buf_);

EXTERN_C_END

#endif
