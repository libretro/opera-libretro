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

/* VDLP.h: interface for the CVDLP class. */

#ifndef LIBFREEDO_VDLP_H_INCLUDED
#define LIBFREEDO_VDLP_H_INCLUDED

#include "extern_c.h"

#include <stdint.h>

struct vdlp_line_s
{
  uint16_t line[320*4];
  uint8_t  xCLUTB[32];
  uint8_t  xCLUTG[32];
  uint8_t  xCLUTR[32];
  uint32_t xOUTCONTROLL;
  uint32_t xCLUTDMA;
  uint32_t xBACKGROUND;
};

typedef struct vdlp_line_s vdlp_line_t;

struct vdlp_frame_s
{
  vdlp_line_t  lines[240*4];
  unsigned int src_w;
  unsigned int src_h;
};

typedef struct vdlp_frame_s vdlp_frame_t;

EXTERN_C_BEGIN

void     freedo_vdlp_init(uint8_t *vram_);

void     freedo_vdlp_process(const uint32_t addr_);
void     freedo_vdlp_process_line(int line_, vdlp_frame_t *frame_);

uint32_t freedo_vdlp_state_size(void);
void     freedo_vdlp_state_save(void *buf_);
void     freedo_vdlp_state_load(const void *buf_);

EXTERN_C_END

#endif /* LIBFREEDO_VDLP_H_INCLUDED */
