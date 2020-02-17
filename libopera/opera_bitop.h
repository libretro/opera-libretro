/*
  www.freedo.org
  The first working 3DO multiplayer emulator.

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

#ifndef BITOPCLASS_DEFINITION_HEADER
#define BITOPCLASS_DEFINITION_HEADER

#include <stdint.h>

#include "extern_c.h"

EXTERN_C_BEGIN

struct BitReaderBig
{
  uint32_t buf;
  uint32_t point;
  int32_t bitpoint;
  int32_t bitset;
};

uint32_t BitReaderBig_Read(struct BitReaderBig *bit, uint8_t bits);

void BitReaderBig_AttachBuffer(struct BitReaderBig *bit, uint32_t buff);

void BitReaderBig_SetBitRate(struct BitReaderBig *bit, uint8_t bits);

void BitReaderBig_Skip(struct BitReaderBig *bit, uint32_t bits);

EXTERN_C_END

#endif
