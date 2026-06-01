#include "opera_state.h"

#include "opera_log.h"

#include <stdint.h>
#include <string.h>

static uint32_t
opera_state_chunk_header_size(void)
{
  return (sizeof(char[4]) + sizeof(uint32_t));
}

uint32_t
opera_state_get_chunk_data_size(void const     *src_,
                                uint32_t const  src_size_,
                                char const     *name_,
                                uint32_t       *data_size_)
{
  opera_state_reader_t reader;
  opera_state_reader_t payload;

  if(data_size_ != NULL)
    *data_size_ = 0;

  if(src_ == NULL)
    return 0;

  opera_state_reader_init(&reader,src_,src_size_);
  if(!opera_state_read_chunk(&reader,name_,&payload))
    return 0;

  if(data_size_ != NULL)
    *data_size_ = payload.size;

  return opera_state_save_size(payload.size);
}

uint32_t
opera_state_get_version(void const     *src_,
                        uint32_t const  src_size_)
{
  uint8_t const *src;
  uint32_t       data_size;

  if(!opera_state_get_chunk_data_size(src_,src_size_,"3DO",&data_size))
    return 0;
  if(data_size < sizeof(uint8_t))
    return 0;

  src = (uint8_t const*)src_;
  src += opera_state_chunk_header_size();
  if(data_size < sizeof(uint32_t))
    return 0;
  if((data_size == sizeof(opera_state_data_t)) &&
     (src[0] == OPERA_STATE_VERSION_V1))
    return src[0];

  if(data_size >= sizeof(uint32_t))
    return (((uint32_t)src[0] << 0)  |
            ((uint32_t)src[1] << 8)  |
            ((uint32_t)src[2] << 16) |
            ((uint32_t)src[3] << 24));

  return src[0];
}

uint32_t
opera_state_save_size(uint32_t const src_size_)
{
  return (opera_state_chunk_header_size() + src_size_);
}

uint32_t
opera_state_chunk_size(uint32_t const payload_size_)
{
  return opera_state_save_size(payload_size_);
}

static
void
opera_state_writer_fail(opera_state_writer_t *writer_)
{
  writer_->failed = true;
}

void
opera_state_writer_init(opera_state_writer_t *writer_,
                        void                 *data_,
                        uint32_t              size_)
{
  writer_->data = data_;
  writer_->size = size_;
  writer_->offset = 0;
  writer_->failed = false;
}

uint32_t
opera_state_writer_used(opera_state_writer_t const *writer_)
{
  return writer_->offset;
}

bool
opera_state_writer_ok(opera_state_writer_t const *writer_)
{
  return !writer_->failed;
}

bool
opera_state_write_bytes(opera_state_writer_t *writer_,
                        void const           *src_,
                        uint32_t              size_)
{
  void *dst;

  dst = opera_state_write_reserve(writer_,size_);
  if(dst == NULL)
    return !writer_->failed;
  if((size_ > 0) && (src_ == NULL))
    {
      opera_state_writer_fail(writer_);
      return false;
    }
  if(size_ > 0)
    memcpy(dst,src_,size_);

  return true;
}

void *
opera_state_write_reserve(opera_state_writer_t *writer_,
                          uint32_t              size_)
{
  uint32_t offset;

  if(writer_->failed)
    return NULL;
  if(size_ > (UINT32_MAX - writer_->offset))
    {
      opera_state_writer_fail(writer_);
      return NULL;
    }

  offset = writer_->offset;
  writer_->offset += size_;

  if((writer_->data != NULL) && (writer_->offset > writer_->size))
    {
      opera_state_writer_fail(writer_);
      return NULL;
    }

  return (writer_->data == NULL) ? NULL : &writer_->data[offset];
}

bool
opera_state_write_u8(opera_state_writer_t *writer_,
                     uint8_t               value_)
{
  return opera_state_write_bytes(writer_,&value_,sizeof(value_));
}

bool
opera_state_write_i8(opera_state_writer_t *writer_,
                     int8_t                value_)
{
  return opera_state_write_u8(writer_,(uint8_t)value_);
}

bool
opera_state_write_u16(opera_state_writer_t *writer_,
                      uint16_t              value_)
{
  uint8_t data[2];

  data[0] = (uint8_t)(value_ >> 0);
  data[1] = (uint8_t)(value_ >> 8);

  return opera_state_write_bytes(writer_,data,sizeof(data));
}

bool
opera_state_write_i16(opera_state_writer_t *writer_,
                      int16_t               value_)
{
  return opera_state_write_u16(writer_,(uint16_t)value_);
}

bool
opera_state_write_u32(opera_state_writer_t *writer_,
                      uint32_t              value_)
{
  uint8_t data[4];

  data[0] = (uint8_t)(value_ >> 0);
  data[1] = (uint8_t)(value_ >> 8);
  data[2] = (uint8_t)(value_ >> 16);
  data[3] = (uint8_t)(value_ >> 24);

  return opera_state_write_bytes(writer_,data,sizeof(data));
}

bool
opera_state_write_i32(opera_state_writer_t *writer_,
                      int32_t               value_)
{
  return opera_state_write_u32(writer_,(uint32_t)value_);
}

bool
opera_state_write_u64(opera_state_writer_t *writer_,
                      uint64_t              value_)
{
  uint8_t data[8];

  data[0] = (uint8_t)(value_ >> 0);
  data[1] = (uint8_t)(value_ >> 8);
  data[2] = (uint8_t)(value_ >> 16);
  data[3] = (uint8_t)(value_ >> 24);
  data[4] = (uint8_t)(value_ >> 32);
  data[5] = (uint8_t)(value_ >> 40);
  data[6] = (uint8_t)(value_ >> 48);
  data[7] = (uint8_t)(value_ >> 56);

  return opera_state_write_bytes(writer_,data,sizeof(data));
}

bool
opera_state_write_i64(opera_state_writer_t *writer_,
                      int64_t               value_)
{
  return opera_state_write_u64(writer_,(uint64_t)value_);
}

bool
opera_state_write_u16_array(opera_state_writer_t *writer_,
                            uint16_t const       *src_,
                            uint32_t              count_)
{
  uint32_t i;

  for(i = 0; i < count_; i++)
    if(!opera_state_write_u16(writer_,src_ == NULL ? 0 : src_[i]))
      return false;

  return true;
}

bool
opera_state_write_i16_array(opera_state_writer_t *writer_,
                            int16_t const        *src_,
                            uint32_t              count_)
{
  uint32_t i;

  for(i = 0; i < count_; i++)
    if(!opera_state_write_i16(writer_,src_ == NULL ? 0 : src_[i]))
      return false;

  return true;
}

bool
opera_state_write_u32_array(opera_state_writer_t *writer_,
                            uint32_t const       *src_,
                            uint32_t              count_)
{
  uint32_t i;

  for(i = 0; i < count_; i++)
    if(!opera_state_write_u32(writer_,src_ == NULL ? 0 : src_[i]))
      return false;

  return true;
}

bool
opera_state_write_i32_array(opera_state_writer_t *writer_,
                            int32_t const        *src_,
                            uint32_t              count_)
{
  uint32_t i;

  for(i = 0; i < count_; i++)
    if(!opera_state_write_i32(writer_,src_ == NULL ? 0 : src_[i]))
      return false;

  return true;
}

bool
opera_state_write_chunk_header(opera_state_writer_t *writer_,
                               char const           *name_,
                               uint32_t              payload_size_)
{
  char name[4] = {0};

  memcpy(name,name_,sizeof(name));

  return (opera_state_write_bytes(writer_,name,sizeof(name)) &&
          opera_state_write_u32(writer_,payload_size_));
}

static
void
opera_state_reader_fail(opera_state_reader_t *reader_)
{
  reader_->failed = true;
}

void
opera_state_reader_init(opera_state_reader_t *reader_,
                        void const           *data_,
                        uint32_t              size_)
{
  reader_->data = data_;
  reader_->size = size_;
  reader_->offset = 0;
  reader_->failed = false;
}

uint32_t
opera_state_reader_used(opera_state_reader_t const *reader_)
{
  return reader_->offset;
}

uint32_t
opera_state_reader_remaining(opera_state_reader_t const *reader_)
{
  if(reader_->offset > reader_->size)
    return 0;

  return (reader_->size - reader_->offset);
}

bool
opera_state_reader_finished(opera_state_reader_t const *reader_)
{
  return (!reader_->failed && (reader_->offset == reader_->size));
}

bool
opera_state_reader_ok(opera_state_reader_t const *reader_)
{
  return !reader_->failed;
}

bool
opera_state_read_bytes(opera_state_reader_t *reader_,
                       void                 *dst_,
                       uint32_t              size_)
{
  if(reader_->failed)
    return false;
  if(size_ > opera_state_reader_remaining(reader_))
    {
      opera_state_reader_fail(reader_);
      return false;
    }

  if(size_ > 0)
    memcpy(dst_,&reader_->data[reader_->offset],size_);
  reader_->offset += size_;

  return true;
}

bool
opera_state_read_u8(opera_state_reader_t *reader_,
                    uint8_t              *value_)
{
  return opera_state_read_bytes(reader_,value_,sizeof(*value_));
}

bool
opera_state_read_i8(opera_state_reader_t *reader_,
                    int8_t               *value_)
{
  uint8_t value;

  if(!opera_state_read_u8(reader_,&value))
    return false;

  *value_ = (int8_t)value;
  return true;
}

bool
opera_state_read_u16(opera_state_reader_t *reader_,
                     uint16_t             *value_)
{
  uint8_t data[2];

  if(!opera_state_read_bytes(reader_,data,sizeof(data)))
    return false;

  *value_ = ((uint16_t)data[0] << 0) |
            ((uint16_t)data[1] << 8);
  return true;
}

bool
opera_state_read_i16(opera_state_reader_t *reader_,
                     int16_t              *value_)
{
  uint16_t value;

  if(!opera_state_read_u16(reader_,&value))
    return false;

  *value_ = (int16_t)value;
  return true;
}

bool
opera_state_read_u32(opera_state_reader_t *reader_,
                     uint32_t             *value_)
{
  uint8_t data[4];

  if(!opera_state_read_bytes(reader_,data,sizeof(data)))
    return false;

  *value_ = ((uint32_t)data[0] << 0)  |
            ((uint32_t)data[1] << 8)  |
            ((uint32_t)data[2] << 16) |
            ((uint32_t)data[3] << 24);
  return true;
}

bool
opera_state_read_i32(opera_state_reader_t *reader_,
                     int32_t              *value_)
{
  uint32_t value;

  if(!opera_state_read_u32(reader_,&value))
    return false;

  *value_ = (int32_t)value;
  return true;
}

bool
opera_state_read_u64(opera_state_reader_t *reader_,
                     uint64_t             *value_)
{
  uint8_t data[8];

  if(!opera_state_read_bytes(reader_,data,sizeof(data)))
    return false;

  *value_ = ((uint64_t)data[0] << 0)  |
            ((uint64_t)data[1] << 8)  |
            ((uint64_t)data[2] << 16) |
            ((uint64_t)data[3] << 24) |
            ((uint64_t)data[4] << 32) |
            ((uint64_t)data[5] << 40) |
            ((uint64_t)data[6] << 48) |
            ((uint64_t)data[7] << 56);
  return true;
}

bool
opera_state_read_i64(opera_state_reader_t *reader_,
                     int64_t              *value_)
{
  uint64_t value;

  if(!opera_state_read_u64(reader_,&value))
    return false;

  *value_ = (int64_t)value;
  return true;
}

bool
opera_state_read_u16_array(opera_state_reader_t *reader_,
                           uint16_t             *dst_,
                           uint32_t              count_)
{
  uint32_t i;

  for(i = 0; i < count_; i++)
    if(!opera_state_read_u16(reader_,&dst_[i]))
      return false;

  return true;
}

bool
opera_state_read_i16_array(opera_state_reader_t *reader_,
                           int16_t              *dst_,
                           uint32_t              count_)
{
  uint32_t i;

  for(i = 0; i < count_; i++)
    if(!opera_state_read_i16(reader_,&dst_[i]))
      return false;

  return true;
}

bool
opera_state_read_u32_array(opera_state_reader_t *reader_,
                           uint32_t             *dst_,
                           uint32_t              count_)
{
  uint32_t i;

  for(i = 0; i < count_; i++)
    if(!opera_state_read_u32(reader_,&dst_[i]))
      return false;

  return true;
}

bool
opera_state_read_i32_array(opera_state_reader_t *reader_,
                           int32_t              *dst_,
                           uint32_t              count_)
{
  uint32_t i;

  for(i = 0; i < count_; i++)
    if(!opera_state_read_i32(reader_,&dst_[i]))
      return false;

  return true;
}

bool
opera_state_read_chunk(opera_state_reader_t *reader_,
                       char const           *name_,
                       opera_state_reader_t *payload_)
{
  char     expected[4] = {0};
  char     actual[4];
  uint32_t payload_size;

  memcpy(expected,name_,sizeof(expected));
  if(!opera_state_read_bytes(reader_,actual,sizeof(actual)) ||
     memcmp(expected,actual,sizeof(expected)) ||
     !opera_state_read_u32(reader_,&payload_size) ||
     (payload_size > opera_state_reader_remaining(reader_)))
    {
      opera_state_reader_fail(reader_);
      return false;
    }

  opera_state_reader_init(payload_,
                          &reader_->data[reader_->offset],
                          payload_size);
  reader_->offset += payload_size;

  return true;
}

uint32_t
opera_state_save(void           *dst_,
                 char const     *name_,
                 void const     *src_,
                 uint32_t const  src_size_)
{
  char     name[4] = {0};
  uint8_t *dst     = dst_;

  opera_log_printf(OPERA_LOG_DEBUG,
                   "[Opera]: saving state %s of size %u\n",
                   name_,
                   src_size_);

  memcpy(name,name_,sizeof(name));
  memcpy(dst,name,sizeof(name));
  dst += sizeof(name);

  memcpy(dst,&src_size_,sizeof(src_size_));
  dst += sizeof(src_size_);

  memcpy(dst,src_,src_size_);

  return opera_state_save_size(src_size_);
}

uint32_t
opera_state_load(void           *dst_,
                 char const     *name_,
                 void const     *src_,
                 uint32_t const  src_size_)
{
  char           name[4] = {0};
  uint8_t const *src     = src_;
  uint32_t       size    = 0;

  opera_log_printf(OPERA_LOG_DEBUG,
                   "[Opera]: loading state %s of size %u\n",
                   name_,
                   src_size_);

  memcpy(name,src,sizeof(name));
  if(memcmp(name,name_,sizeof(name)))
    return 0;
  src += sizeof(name);

  memcpy(&size,src,sizeof(size));
  if(size != src_size_)
    return 0;
  src += sizeof(size);

  memcpy(dst_,src,src_size_);

  return opera_state_save_size(src_size_);
}

uint32_t
opera_state_load_sized(void           *dst_,
                       char const     *name_,
                       void const     *src_,
                       uint32_t const  src_size_,
                       uint32_t const  dst_size_)
{
  uint8_t const *src;
  uint32_t       data_size;
  uint32_t       chunk_size;

  chunk_size = opera_state_get_chunk_data_size(src_,src_size_,name_,&data_size);
  if((chunk_size == 0) || (data_size != dst_size_))
    return 0;

  src = (uint8_t const*)src_;
  src += opera_state_chunk_header_size();
  memcpy(dst_,src,dst_size_);

  return chunk_size;
}
