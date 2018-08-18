#ifndef LIBRETRO_4DO_DISK_CONTROL_H_INCLUDED
#define LIBRETRO_4DO_DISK_CONTROL_H_INCLUDED

#include "retro_cdimage.h"

#ifdef _WIN32
#define SLASH '\\'
#else
#define SLASH '/'
#endif

#ifndef PATH_MAX
#define PATH_MAX  4096
#endif

#ifndef DISK_MAX
#define DISK_MAX  8
#endif

struct disk_control_s
{
  bool ejected;
  int current_sector;
  unsigned int current_index;
  unsigned int count;
  cdimage_t *disks[DISK_MAX];
};

typedef struct disk_control_s disk_control_t;

int disk_control_open_file(const char *path_);

static disk_control_t disk_control_state = {
  .ejected = false,
  .current_sector = 0,
  .current_index = 0,
  .count = 0,
};

#define disk_control_add_disk(obj, item) ((obj)->disks[(obj)->count++] = item)
#define disk_control_cleanup() (disk_control_destroy(disk_control_get()))
#define disk_control_get() (&(disk_control_state))
#define disk_control_get_current_disk(obj) (disk_control_get_disk(obj, (obj)->current_index))
#define disk_control_get_disk(obj, index) ((obj)->disks[index])
#define disk_control_length(obj) (sizeof((obj)->disks) / sizeof((obj)->disks[0]))
#define disk_control_swap(obj, index) (disk_control_switch_image(obj, index))

void disk_control_destroy(disk_control_t *dc);
static int disk_control_switch_image(disk_control_t *dc, int image_index);

// libfreedo cdrom callbacks
uint32_t disk_image_get_size(void);
void disk_image_set_sector(const uint32_t sector_);
void disk_image_read_sector(void *buf_);

// libretro disk control callbacks
struct retro_disk_control_callback disk_control_cb;

#endif
