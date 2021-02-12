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

#ifndef LIBOPERA_ARM_H_INCLUDED
#define LIBOPERA_ARM_H_INCLUDED

#include <stdint.h>

#include "extern_c.h"

EXTERN_C_BEGIN

int32_t  opera_arm_execute(void);
void     opera_arm_init(void);
void     opera_arm_reset(void);
void     opera_arm_destroy(void);

void     opera_mem_write8(uint32_t addr_, uint8_t val_);
void     opera_mem_write16(uint32_t addr_, uint16_t val_);
void     opera_mem_write32(uint32_t addr_, uint32_t val_);
uint8_t  opera_mem_read8(uint32_t addr_);
uint16_t opera_mem_read16(uint32_t addr_);
uint32_t opera_mem_read32(uint32_t addr_);

void     opera_io_write(const uint32_t addr_, const uint32_t val_);
uint32_t opera_io_read(const uint32_t addr_);

uint32_t opera_arm_state_size(void);
void     opera_arm_state_save(void *buf_);
void     opera_arm_state_load(const void *buf_);

uint8_t* opera_arm_nvram_get(void);
uint64_t opera_arm_nvram_size(void);

void     opera_arm_rom_select(int n_);

uint8_t* opera_arm_rom1_get(void);
uint64_t opera_arm_rom1_size(void);
void     opera_arm_rom1_byteswap_if_necessary(void);

uint8_t* opera_arm_rom2_get(void);
uint64_t opera_arm_rom2_size(void);
void     opera_arm_rom2_byteswap_if_necessary(void);

uint8_t* opera_arm_ram_get(void);
uint64_t opera_arm_ram_size(void);

uint8_t* opera_arm_vram_get(void);
uint64_t opera_arm_vram_size(void);

void     opera_arm_swi_hle_set(const int hle);
int      opera_arm_swi_hle_get(void);

EXTERN_C_END

#endif /* LIBOPERA_ARM_H_INCLUDED */
