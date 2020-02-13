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

#include "opera_diag_port.h"

static uint16_t SNDDebugFIFO0;
static uint16_t SNDDebugFIFO1;
static uint16_t RCVDebugFIFO0;
static uint16_t RCVDebugFIFO1;
static uint16_t GetIdx;
static uint16_t SendIdx;

void
opera_diag_port_init(const int32_t test_code_)
{
  int32_t test_code = test_code_;

  GetIdx  = 16;
  SendIdx = 16;
  SNDDebugFIFO0 = 0;
  SNDDebugFIFO1 = 0;

  if(test_code >= 0)
    {
      test_code ^= 0xFF;
      test_code |= 0xA000;
    }
  else
    {
      test_code = 0;
    }

  RCVDebugFIFO0 = test_code;
  RCVDebugFIFO1 = test_code;
}

void
opera_diag_port_send(const uint32_t val_)
{
  if(GetIdx != 16)
    {
      GetIdx  = 16;
      SendIdx = 16;
      SNDDebugFIFO0 = 0;
      SNDDebugFIFO1 = 0;
    }

  SNDDebugFIFO0 |= ((val_ & 1) << (SendIdx - 1));
  SNDDebugFIFO1 |= (((val_ & 1) >> 1) << (SendIdx - 1));

  SendIdx--;

  if(SendIdx == 0)
    SendIdx = 16;
}

uint32_t
opera_diag_port_get(void)
{
  unsigned int val = 0;

  if(SendIdx != 16)
    {
      GetIdx  = 16;
      SendIdx = 16;
    }

  val  = ((RCVDebugFIFO0 >> (GetIdx - 1)) & 0x1);
  val |= (((RCVDebugFIFO1 >> (GetIdx - 1)) &0x1 ) << 0x1);
  GetIdx--;

  /*
    if(GetIdx > 0)
    {
      RCVDebugFIFO0 = (RCVDebugFIFO0 >> 1);
      RCVDebugFIFO1 = (RCVDebugFIFO1 >> 1);
      GetIdx--;
      val  = (RCVDebugFIFO0 & 0x1);
      val |= ((RCVDebugFIFO1 & 0x1) << 0x1);
    }
  */

  if(GetIdx == 0)
    GetIdx = 16;

  return val;
}
