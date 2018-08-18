#include "disk_control.h"
#include "retro_callbacks.h"
#include "retro_cdimage.h"

#include <string.h>
#include <libretro.h>
#include <file/file_path.h>
#include <streams/file_stream.h>



static bool disk_set_eject_state(bool ejected)
{
	disk_control_state.ejected = ejected;
	return true;
}

static bool disk_get_eject_state(void)
{
	return disk_control_state.ejected;
}

static unsigned int disk_get_image_index(void)
{
	return disk_control_state.current_index;
}

static bool disk_set_image_index(unsigned int index)
{
  int rv;
  if ((rv = disk_control_swap(&disk_control_state, index)) != 0) {
    retro_log_printf_cb(RETRO_LOG_ERROR, "unable to switch to disk: #%u\n", index);
    return false;
  }
  retro_reset();
  return true;
}

static unsigned int disk_get_num_images(void)
{
	return disk_control_state.count;
}

static bool disk_replace_image_index(unsigned index,
	const struct retro_game_info *info)
{
  char *old_fname;
  bool rv = true;
  if (index >= disk_control_length(&disk_control_state)) {
    return false;
  }

  cdimage_t *cd = disk_control_get_disk(&disk_control_state, index);
  old_fname = cd->filename;
  cd->filename = NULL;

  if (info != NULL) {
    cd->filename = strdup(info->path);
    if (index == disk_control_state.current_index)
      rv = disk_set_image_index(index);
  }

  if (old_fname != NULL)
    free(old_fname);

  return rv;
}

static bool disk_add_image_index(void)
{
  if (disk_control_state.count >= 8)
    return false;

  disk_control_state.count++;
  return true;
}

int
disk_control_open_m3u_file(const char *path_)
{
  int rv;
  char line[PATH_MAX];
  char name[PATH_MAX];
  FILE *f = fopen(path_, "r");
  if (!f)
      return false;

  char *base_dir = strdup(path_);
  path_basedir(base_dir);

  disk_control_t *dc = disk_control_get();
  while (fgets(line, sizeof(line), f) && dc->count < disk_control_length(dc)) {
    if (line[0] == '#')
      continue;
    char *carrige_return = strchr(line, '\r');
    if (carrige_return)
      *carrige_return = '\0';
    char *newline = strchr(line, '\n');
    if (newline)
      *newline = '\0';
    if (line[0] != '\0') {
      snprintf(name, sizeof(name), "%s%c%s", base_dir, SLASH, line);
      cdimage_t *cd = malloc(sizeof(cdimage_t));
      cd->filename = strdup(name);
      disk_control_add_disk(dc, cd);
    }
  }
  free(base_dir);
  if ((rv = retro_cdimage_open_cue(disk_control_get_disk(dc, 0)->filename, disk_control_get_disk(dc, 0))) != 0) {
    retro_log_printf_cb(RETRO_LOG_ERROR, "error %d opening cdimage: %d\n", rv, 0);
    return -1;
  };

  fclose(f);
  return (dc->count != 0);
}

void
disk_control_destroy(disk_control_t *dc)
{
  int rv;
  for (int i = 0; i < DISK_MAX; i++) {
    cdimage_t *cd = disk_control_get_disk(dc, i);
    if (cd != NULL) {
      if ((rv = retro_cdimage_close(cd)) > 0) {
        retro_log_printf_cb(RETRO_LOG_ERROR, "error %d closing cdimage: %d\n", rv, i);
        continue;
      }
      if (cd->filename != NULL) {
        free(cd->filename);
      }
      free(cd);
    }
  }
}

static
int
disk_control_switch_image(disk_control_t *dc, int index)
{
  int rv;
  cdimage_t *new_disk = disk_control_get_disk(dc, index);
  if (new_disk->filename == NULL) {
    retro_log_printf_cb(RETRO_LOG_ERROR, "missing disk #%u\n", index);

    // RetroArch specifies "no disk" with index == count,
    // so don't fail here..
    dc->current_index = index;
    return -1;
  }

  retro_log_printf_cb(RETRO_LOG_INFO, "switching to disk %u: \"%s\"\n", index,
    new_disk->filename);

  if ((rv = retro_cdimage_close(disk_control_get_current_disk(dc))) > 0) {
    retro_log_printf_cb(RETRO_LOG_ERROR, "error %d closing cdimage: %d\n", rv, index);
    return -1;
  }

  if ((rv = retro_cdimage_open_cue(new_disk->filename, new_disk)) > 0) {
    retro_log_printf_cb(RETRO_LOG_ERROR, "error %d opening cdimage: %d\n", rv, index);
    return -1;
  }
  dc->current_index = index;
  return 0;
}

int
disk_control_open_file(const char *path_)
{
  const char *ext;
  int rv;

  ext = path_get_extension(path_);
  if(ext == NULL)
    return -1;

  if(!strcasecmp(ext,"m3u"))
    return disk_control_open_m3u_file(path_);

  disk_control_t *dc = &disk_control_state;
  cdimage_t cd;
  rv = retro_cdimage_open(path_, &cd);
  if (rv == -1)
    return -1;
  disk_control_add_disk(dc, &cd);
  return 0;
}

uint32_t disk_image_get_size(void)
{
  return retro_cdimage_get_number_of_logical_blocks(disk_control_get_current_disk(disk_control_get()));
}

void disk_image_set_sector(const uint32_t sector_)
{
  disk_control_state.current_sector = sector_;
}

void disk_image_read_sector(void *buf_)
{
  retro_cdimage_read(disk_control_get_current_disk(disk_control_get()), disk_control_state.current_sector, buf_, 2048);
}

struct retro_disk_control_callback disk_control_cb = {
	.set_eject_state = disk_set_eject_state,
	.get_eject_state = disk_get_eject_state,
	.get_image_index = disk_get_image_index,
	.set_image_index = disk_set_image_index,
	.get_num_images = disk_get_num_images,
	.replace_image_index = disk_replace_image_index,
	.add_image_index = disk_add_image_index,
};
