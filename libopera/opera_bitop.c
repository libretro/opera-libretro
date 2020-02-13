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

#include <stdint.h>

#include "opera_arm.h"
#include "opera_bitop.h"

void BitReaderBig_AttachBuffer(struct BitReaderBig *bit, uint32_t buff)
{
  bit->buf      = buff;
  bit->point    = 0;
  bit->bitpoint = 0;
}

void BitReaderBig_Skip(struct BitReaderBig *bit, uint32_t bits)
{
  bits         += bit->bitpoint;
  bit->point   += (bits >> 3);
  bit->bitpoint = bits & 7;
}

void BitReaderBig_SetBitRate(struct BitReaderBig *bit, uint8_t bits)
{
  bit->bitset = bits;
  if(bit->bitset > 32)
    bit->bitset = 32;
  if(!bit->bitset)
    bit->bitset = 1;
};

uint32_t BitReaderBig_Read(struct BitReaderBig *bit, uint8_t bits)
{
  int32_t bitcnt;
  const static uint8_t mas[] = {0,1,3,7,15,31,63,127,255};
  uint32_t retval = 0;
  BitReaderBig_SetBitRate(bit, bits);

  bitcnt  = bit->bitset;
  if(!bit->buf)
    return retval;

  if((8 - bit->bitpoint) > bit->bitset)
    {
      retval    = opera_mem_read8(bit->buf + (bit->point ^ 3));
      retval  >>= 8 - bit->bitpoint - bit->bitset;
      retval   &= mas[bit->bitset];
      bit->bitpoint += bit->bitset;
      return retval;
    }

  if (bit->bitpoint)
    {
      retval = opera_mem_read8(bit->buf + (bit->point ^ 3)) & mas[8 - bit->bitpoint];
      bit->point++;
      bitcnt -= 8 - bit->bitpoint;
    }

  while(bitcnt>=8)
    {
      retval <<= 8;
      retval  |= opera_mem_read8(bit->buf + (bit->point ^ 3));
      bit->point++;
      bitcnt-=8;
    }

  if(bitcnt)
    {
      retval<<=bitcnt;
      retval|=opera_mem_read8(bit->buf + (bit->point^3)) >> (8 - bitcnt);
    }

  bit->bitpoint = bitcnt;

  return retval;
}
