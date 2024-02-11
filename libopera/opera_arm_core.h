#ifndef LIBOPERA_ARM_CORE_H_INCLUDED
#define LIBOPERA_ARM_CORE_H_INCLUDED

#include <stdint.h>

typedef struct arm_core_s arm_core_t;
struct arm_core_s
{
  uint32_t USER[16];
  uint32_t CASH[7];
  uint32_t SVC[2];
  uint32_t ABT[2];
  uint32_t FIQ[7];
  uint32_t IRQ[2];
  uint32_t UND[2];
  uint32_t SPSR[6];
  uint32_t CPSR;

  uint8_t nFIQ;                 /* external interrupt */
  uint8_t MAS_Access_Exept;	/* memory exceptions */
};

#endif
