#ifndef LIBOPERA_STATE_H_INCLUDED
#define LIBOPERA_STATE_H_INCLUDED

#include "boolean.h"

#include <stdint.h>

#define OPERA_STATE_VERSION_V1      0x01
#define OPERA_STATE_VERSION_V2      0x02
#define OPERA_STATE_VERSION_CURRENT OPERA_STATE_VERSION_V2

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

typedef struct opera_state_writer_s opera_state_writer_t;
struct opera_state_writer_s
{
  uint8_t *data;
  uint32_t size;
  uint32_t offset;
  bool failed;
};

typedef struct opera_state_reader_s opera_state_reader_t;
struct opera_state_reader_s
{
  uint8_t const *data;
  uint32_t       size;
  uint32_t       offset;
  bool           failed;
};

uint32_t opera_state_get_version(void const     *src,
                                 uint32_t const  src_size);
uint32_t opera_state_get_chunk_data_size(void const     *src,
                                         uint32_t const  src_size,
                                         char const     *name,
                                         uint32_t       *data_size);

uint32_t opera_state_save_size(uint32_t const src_size);
uint32_t opera_state_chunk_size(uint32_t const payload_size);

void     opera_state_writer_init(opera_state_writer_t *writer,
                                 void                 *data,
                                 uint32_t              size);
uint32_t opera_state_writer_used(opera_state_writer_t const *writer);
bool     opera_state_writer_ok(opera_state_writer_t const *writer);
bool     opera_state_write_chunk_header(opera_state_writer_t *writer,
                                        char const           *name,
                                        uint32_t              payload_size);
bool     opera_state_write_bytes(opera_state_writer_t *writer,
                                 void const           *src,
                                 uint32_t              size);
void    *opera_state_write_reserve(opera_state_writer_t *writer,
                                   uint32_t              size);
bool     opera_state_write_u8(opera_state_writer_t *writer, uint8_t value);
bool     opera_state_write_i8(opera_state_writer_t *writer, int8_t value);
bool     opera_state_write_u16(opera_state_writer_t *writer, uint16_t value);
bool     opera_state_write_i16(opera_state_writer_t *writer, int16_t value);
bool     opera_state_write_u32(opera_state_writer_t *writer, uint32_t value);
bool     opera_state_write_i32(opera_state_writer_t *writer, int32_t value);
bool     opera_state_write_u64(opera_state_writer_t *writer, uint64_t value);
bool     opera_state_write_i64(opera_state_writer_t *writer, int64_t value);
bool     opera_state_write_u16_array(opera_state_writer_t *writer,
                                     uint16_t const       *src,
                                     uint32_t              count);
bool     opera_state_write_i16_array(opera_state_writer_t *writer,
                                     int16_t const        *src,
                                     uint32_t              count);
bool     opera_state_write_u32_array(opera_state_writer_t *writer,
                                     uint32_t const       *src,
                                     uint32_t              count);
bool     opera_state_write_i32_array(opera_state_writer_t *writer,
                                     int32_t const        *src,
                                     uint32_t              count);

void     opera_state_reader_init(opera_state_reader_t *reader,
                                 void const           *data,
                                 uint32_t              size);
uint32_t opera_state_reader_used(opera_state_reader_t const *reader);
uint32_t opera_state_reader_remaining(opera_state_reader_t const *reader);
bool     opera_state_reader_finished(opera_state_reader_t const *reader);
bool     opera_state_reader_ok(opera_state_reader_t const *reader);
bool     opera_state_read_chunk(opera_state_reader_t *reader,
                                char const           *name,
                                opera_state_reader_t *payload);
bool     opera_state_read_bytes(opera_state_reader_t *reader,
                                void                 *dst,
                                uint32_t              size);
bool     opera_state_read_u8(opera_state_reader_t *reader, uint8_t *value);
bool     opera_state_read_i8(opera_state_reader_t *reader, int8_t *value);
bool     opera_state_read_u16(opera_state_reader_t *reader, uint16_t *value);
bool     opera_state_read_i16(opera_state_reader_t *reader, int16_t *value);
bool     opera_state_read_u32(opera_state_reader_t *reader, uint32_t *value);
bool     opera_state_read_i32(opera_state_reader_t *reader, int32_t *value);
bool     opera_state_read_u64(opera_state_reader_t *reader, uint64_t *value);
bool     opera_state_read_i64(opera_state_reader_t *reader, int64_t *value);
bool     opera_state_read_u16_array(opera_state_reader_t *reader,
                                    uint16_t             *dst,
                                    uint32_t              count);
bool     opera_state_read_i16_array(opera_state_reader_t *reader,
                                    int16_t              *dst,
                                    uint32_t              count);
bool     opera_state_read_u32_array(opera_state_reader_t *reader,
                                    uint32_t             *dst,
                                    uint32_t              count);
bool     opera_state_read_i32_array(opera_state_reader_t *reader,
                                    int32_t              *dst,
                                    uint32_t              count);

uint32_t opera_state_load(void           *dst,
                          char const     *name,
                          void const     *src,
                          uint32_t const  src_size);
uint32_t opera_state_save(void           *dst,
                          char const     *name,
                          void const     *src,
                          uint32_t const  src_size);
uint32_t opera_state_load_sized(void           *dst,
                                char const     *name,
                                void const     *src,
                                uint32_t const  src_size,
                                uint32_t const  dst_size);


#endif
