#ifndef LIBOPERA_3DO_H_INCLUDED
#define LIBOPERA_3DO_H_INCLUDED

#include "opera_core.h"
#include "opera_vdlp.h"

#include <stdint.h>

uint32_t opera_3do_state_size(void);
uint32_t opera_3do_state_save(void *buf, size_t size);
uint32_t opera_3do_state_load(void const *buf, size_t size);

int      opera_3do_init(opera_ext_interface_t callback);
void     opera_3do_destroy(void);

void     opera_3do_process_frame(void);

#endif /* LIBOPERA_3DO_H_INCLUDED */
