#include "freedo-cdrom.h"
#include "xbus.h"

#include <string.h>

/* FIXME: should not be using globals */
static cdrom_device_t CDROM_DEVICE = {0};

void*
xbus_cdrom_plugin(int   proc_,
                  void* data_)
{
  switch(proc_)
    {
    case XBP_INIT:
      freedo_cdrom_init(&CDROM_DEVICE);
      return (void*)true;
    case XBP_DESTROY:
      return (void*)true;
    case XBP_RESET:
      freedo_cdrom_init(&CDROM_DEVICE);
      break;
    case XBP_SET_COMMAND:
      freedo_cdrom_send_cmd(&CDROM_DEVICE,(uint8_t)(uintptr_t)data_);
      break;
    case XBP_FIQ:
      return (void*)freedo_cdrom_test_fiq(&CDROM_DEVICE);
    case XBP_GET_DATA:
      return (void*)(uintptr_t)freedo_cdrom_fifo_get_data(&CDROM_DEVICE);
    case XBP_GET_STATUS:
      return (void*)(uintptr_t)freedo_cdrom_fifo_get_status(&CDROM_DEVICE);
    case XBP_SET_POLL:
      freedo_cdrom_set_poll(&CDROM_DEVICE,(uint32_t)(uintptr_t)data_);
      break;
    case XBP_GET_POLL:
      return (void*)(uintptr_t)CDROM_DEVICE.poll;
    case XBP_GET_SAVESIZE:
      return (void*)(uintptr_t)sizeof(cdrom_device_t);
    case XBP_GET_SAVEDATA:
      memcpy(data_,&CDROM_DEVICE,sizeof(cdrom_device_t));
      break;
    case XBP_SET_SAVEDATA:
      memcpy(&CDROM_DEVICE,data_,sizeof(cdrom_device_t));
      return (void*)true;
    };

  return NULL;
}
