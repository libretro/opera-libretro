#ifndef LIBFREEDO_3DO_H_INCLUDED
#define LIBFREEDO_3DO_H_INCLUDED

#include "freedo_core.h"
#include "freedo_vdlp.h"

#include <stdint.h>

uint32_t freedo_3do_state_size(void);
void     freedo_3do_state_save(void *buf_);
bool     freedo_3do_state_load(const void *buf_);

int      freedo_3do_init(freedo_ext_interface_t callback_);
void     freedo_3do_destroy(void);

void     freedo_3do_process_frame(void);

#endif /* LIBFREEDO_3DO_H_INCLUDED */
