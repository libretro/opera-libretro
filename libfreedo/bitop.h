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

#ifndef BITOPCLASS_DEFINITION_HEADER
#define BITOPCLASS_DEFINITION_HEADER

#include <stdint.h>

class BitReaderBig
{
   protected:
      uint32_t buf;
      uint32_t point;
      int32_t bitpoint;
      int32_t bitset;

   public:
      BitReaderBig()
      {
         buf=0;
         bitset=1;
         point=0;
         bitpoint=0;
      };
      void AttachBuffer(uint32_t buff)
      {
         buf=buff;
         point=0;
         bitpoint=0;
      };
      void SetBitRate(uint8_t bits)
      {
         bitset=bits;
         if(bitset>32)bitset=32;
         if(!bitset)bitset=1;
      };

      uint32_t GetBytePose(){return point;};

      uint32_t Read();
      uint32_t Read(uint8_t bits);

      void Skip(uint32_t bits)
      {
         bits+=bitpoint;
         point+=(bits>>3);
         bitpoint=bits&7;
      };


};

#endif
