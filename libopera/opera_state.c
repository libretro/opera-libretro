#include "opera_state.h"

#include "opera_log.h"

#include <stdint.h>
#include <string.h>

uint32_t
opera_state_get_version(void const     *src_,
                        uint32_t const  src_size_)
{
  uint8_t const *src;
  opera_state_hdr_t hdr;

  src = (uint8_t const*)src_;
  if(sizeof(hdr) > src_size_)
    return 0;

  src += (sizeof(char[4]) + sizeof(uint32_t));
  memcpy(&hdr,src,sizeof(hdr));

  return hdr.version;
}

uint32_t
opera_state_save_size(uint32_t const src_size_)
{
  return (sizeof(char[4]) + sizeof(uint32_t) + src_size_);
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
