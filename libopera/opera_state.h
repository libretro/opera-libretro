#ifndef LIBOPERA_STATE_H_INCLUDED
#define LIBOPERA_STATE_H_INCLUDED

#include <stdint.h>

typedef struct opera_state_data_t opera_state_data_t;
struct opera_state_data_t
{
  uint8_t data[1024];
};

typedef struct opera_state_hdr_v1_t opera_state_hdr_v1_t;
struct opera_state_hdr_v1_t
{
  uint8_t version;
};

typedef union opera_state_hdr_t opera_state_hdr_t;
union opera_state_hdr_t
{
  uint8_t version;
  opera_state_data_t data;
  opera_state_hdr_v1_t v1;
};

typedef struct opera_state_chunk_t opera_state_chunk_t;
struct opera_state_chunk_t
{
  char     name[4];
  uint32_t size;
  uint8_t  data[1];
};

uint32_t opera_state_get_version(void const     *src,
                                 uint32_t const  src_size);

uint32_t opera_state_save_size(uint32_t const src_size);

uint32_t opera_state_load(void           *dst,
                          char const     *name,
                          void const     *src,
                          uint32_t const  src_size);
uint32_t opera_state_save(void           *dst,
                          char const     *name,
                          void const     *src,
                          uint32_t const  src_size);


#endif
