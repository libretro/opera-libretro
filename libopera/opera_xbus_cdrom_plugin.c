#include "boolean.h"

#include "opera_cdrom.h"
#include "opera_state.h"
#include "opera_xbus.h"

#include <string.h>

static cdrom_device_t g_CDROM_DEVICE = {0};

void*
xbus_cdrom_plugin(int   proc_,
                  void* data_)
{
  switch(proc_)
    {
    case XBP_INIT:
      opera_cdrom_init(&g_CDROM_DEVICE);
      return (void*)true;
    case XBP_DESTROY:
      return (void*)true;
    case XBP_RESET:
      opera_cdrom_init(&g_CDROM_DEVICE);
      break;
    case XBP_SET_COMMAND:
      opera_cdrom_send_cmd(&g_CDROM_DEVICE,(uint8_t)(uintptr_t)data_);
      break;
    case XBP_FIQ:
      return (void*)(uintptr_t)opera_cdrom_test_fiq(&g_CDROM_DEVICE);
    case XBP_GET_DATA:
      return (void*)(uintptr_t)opera_cdrom_fifo_get_data(&g_CDROM_DEVICE);
    case XBP_GET_STATUS:
      return (void*)(uintptr_t)opera_cdrom_fifo_get_status(&g_CDROM_DEVICE);
    case XBP_SET_POLL:
      opera_cdrom_set_poll(&g_CDROM_DEVICE,(uint32_t)(uintptr_t)data_);
      break;
    case XBP_GET_POLL:
      return (void*)(uintptr_t)g_CDROM_DEVICE.poll;
    case XBP_GET_SAVESIZE:
      return (void*)(uintptr_t)opera_state_save_size(sizeof(cdrom_device_t));
    case XBP_GET_SAVEDATA:
      return (void*)(uintptr_t)opera_state_save(data_,"CDRM",&g_CDROM_DEVICE,sizeof(g_CDROM_DEVICE));
    case XBP_SET_SAVEDATA:
      return (void*)(uintptr_t)opera_state_load(&g_CDROM_DEVICE,"CDRM",data_,sizeof(g_CDROM_DEVICE));
    };

  return NULL;
}
