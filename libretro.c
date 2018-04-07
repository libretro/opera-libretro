#ifndef _MSC_VER
#include <sched.h>
#endif
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <boolean.h>

#ifdef _MSC_VER
#define snprintf _snprintf
#pragma pack(1)
#endif

#include <libretro.h>
#include <retro_inline.h>
#include <streams/file_stream.h>
#include <file/file_path.h>

#include "nvram.h"
#include "retro_callbacks.h"
#include "retro_cdimage.h"

#include "libfreedo/freedo_arm.h"
#include "libfreedo/freedo_cdrom.h"
#include "libfreedo/freedo_core.h"
#include "libfreedo/freedo_frame.h"
#include "libfreedo/freedo_madam.h"
#include "libfreedo/freedo_quarz.h"
#include "libfreedo/freedo_vdlp.h"
#include "libfreedo/hack_flags.h"

#define TEMP_BUFFER_SIZE 512
#define ROM1_SIZE 1 * 1024 * 1024
#define ROM2_SIZE 933636 /* was 1 * 1024 * 1024, */

static char biosPath[1024];
static vdlp_frame_t *frame;

extern int HightResMode;
extern unsigned int _3do_SaveSize(void);
extern void _3do_Save(void *buff);
extern bool _3do_Load(void *buff);

static cdimage_t cdimage;

static int currentSector;

static uint32_t *videoBuffer;
static int videoWidth, videoHeight;
static int32_t sampleBuffer[TEMP_BUFFER_SIZE];
static unsigned int sampleCurrent;

static retro_video_refresh_t video_cb;
static retro_audio_sample_batch_t audio_batch_cb;

static bool x_button_also_p;
static int  controller_count;

void retro_set_environment(retro_environment_t cb)
{
   struct retro_vfs_interface_info vfs_iface_info;
   static const struct retro_variable vars[] = {
      { "4do_cpu_overclock",        "CPU overclock; 1x|2x|4x" },
      { "4do_high_resolution",      "High Resolution; disabled|enabled" },
      { "4do_nvram_storage",        "NVRAM Storage; per game|shared" },
      { "4do_x_button_also_p",      "Button X also acts as P; disabled|enabled" },
      { "4do_controller_count",     "Controller Count; 1|2|3|4|5|6|7|8|0" },
      { "4do_hack_timing_1",        "Timing Hack 1 (Crash 'n Burn); disabled|enabled" },
      { "4do_hack_timing_3",        "Timing Hack 3 (Dinopark Tycoon); disabled|enabled" },
      { "4do_hack_timing_5",        "Timing Hack 5 (Microcosm); disabled|enabled" },
      { "4do_hack_timing_6",        "Timing Hack 6 (Alone in the Dark); disabled|enabled" },
      { "4do_hack_graphics_step_y", "Graphics Step Y Hack (Samurai Shodown); disabled|enabled" },
      { NULL, NULL },
   };

   retro_set_environment_cb(cb);

   retro_environment_cb(RETRO_ENVIRONMENT_SET_VARIABLES,(void*)vars);

   vfs_iface_info.required_interface_version = 1;
   vfs_iface_info.iface                      = NULL;
   if (retro_environment_cb(RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &vfs_iface_info))
     filestream_vfs_init(&vfs_iface_info);
}

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }

void
retro_set_input_poll(retro_input_poll_t cb)
{
  retro_set_input_poll_cb(cb);
}

void
retro_set_input_state(retro_input_state_t cb)
{
  retro_set_input_state_cb(cb);
}

static void fsReadBios(const char *bios_path, void *prom)
{
   int64_t file_size = 0;
   int64_t readcount = 0;
   RFILE *bios1      = filestream_open(bios_path, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!bios1)
      return;

   filestream_seek(bios1, 0, RETRO_VFS_SEEK_POSITION_END);
   file_size = filestream_tell(bios1);
   filestream_rewind(bios1);
   readcount = filestream_read(bios1, prom, file_size);
   (void)readcount;

   filestream_close(bios1);
}

static void initVideo(void)
{
   if (!videoBuffer)
      videoBuffer = (uint32_t*)malloc(640 * 480 * 4);

   if (!frame)
      frame = (vdlp_frame_t*)malloc(sizeof(vdlp_frame_t));

   memset(frame, 0, sizeof(vdlp_frame_t));
}

static
uint32_t
cdimage_get_size(void)
{
  return retro_cdimage_get_number_of_logical_blocks(&cdimage);
}

static
void
cdimage_set_sector(const uint32_t sector_)
{
  currentSector = sector_;
}

static
void
cdimage_read_sector(void *buf_)
{
  retro_cdimage_read(&cdimage,currentSector,buf_,2048);
}

/* libfreedo callback */
static void *fdcCallback(int procedure, void *data)
{
   switch(procedure)
   {
      case EXT_READ_ROMS:
         fsReadBios(biosPath, data);
         break;
      case EXT_SWAPFRAME:
         freedo_frame_get_bitmap_xrgb_8888(frame,videoBuffer,videoWidth,videoHeight);
         return frame;
      case EXT_PUSH_SAMPLE:
         /* TODO: fix all this, not right */
         sampleBuffer[sampleCurrent] = (uintptr_t)data;
         sampleCurrent++;
         if(sampleCurrent >= TEMP_BUFFER_SIZE)
         {
            sampleCurrent = 0;
            audio_batch_cb((int16_t *)sampleBuffer, TEMP_BUFFER_SIZE);
         }
         break;
      case EXT_KPRINT:
         break;
      case EXT_FRAMETRIGGER_MT:
         _freedo_Interface(FDP_DO_FRAME_MT, frame);
         break;
      case EXT_ARM_SYNC:
#if 0
         printf("fdcCallback EXT_ARM_SYNC\n");
#endif
         break;

      default:
         break;
   }

   return NULL;
}

/* See Madam.c for details on bitfields being set below */
static INLINE uint8_t retro_poll_joypad(const int port_,
      const int id_)
{
  return retro_input_state_cb(port_,RETRO_DEVICE_JOYPAD,0,id_);
}

static INLINE void retro_poll_input(const int port_, uint8_t   buttons_[2])
{
  buttons_[0] =
    ((retro_poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_L)      << MADAM_PBUS_BYTE0_SHIFT_L)     |
     (retro_poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_R)      << MADAM_PBUS_BYTE0_SHIFT_R)     |
     (retro_poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_SELECT) << MADAM_PBUS_BYTE0_SHIFT_X)     |
     (retro_poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_START)  << MADAM_PBUS_BYTE0_SHIFT_P)     |
     ((x_button_also_p &&
       retro_poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_X))    << MADAM_PBUS_BYTE0_SHIFT_P)     |
     (retro_poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_A)      << MADAM_PBUS_BYTE0_SHIFT_C)     |
     (retro_poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_B)      << MADAM_PBUS_BYTE0_SHIFT_B));
  buttons_[1] =
    ((retro_poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_Y)      << MADAM_PBUS_BYTE1_SHIFT_A)     |
     (retro_poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_LEFT)   << MADAM_PBUS_BYTE1_SHIFT_LEFT)  |
     (retro_poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_RIGHT)  << MADAM_PBUS_BYTE1_SHIFT_RIGHT) |
     (retro_poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_UP)     << MADAM_PBUS_BYTE1_SHIFT_UP)    |
     (retro_poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_DOWN)   << MADAM_PBUS_BYTE1_SHIFT_DOWN)  |
     MADAM_PBUS_BYTE1_CONNECTED_MASK);
}

static
void
update_input(void)
{
  uint8_t *buttons;

  retro_input_poll_cb();

  buttons = freedo_madam_pbus_data_reset();

  buttons[0x00] = 0x00;
  buttons[0x01] = 0x48;
  buttons[0x0C] = 0x00;
  buttons[0x0D] = 0x80;
  switch(controller_count)
    {
    case 8:
      retro_poll_input(7,&buttons[MADAM_PBUS_CONTROLLER8_OFFSET]);
    case 7:
      retro_poll_input(6,&buttons[MADAM_PBUS_CONTROLLER7_OFFSET]);
    case 6:
      retro_poll_input(5,&buttons[MADAM_PBUS_CONTROLLER6_OFFSET]);
    case 5:
      retro_poll_input(4,&buttons[MADAM_PBUS_CONTROLLER5_OFFSET]);
    case 4:
      retro_poll_input(3,&buttons[MADAM_PBUS_CONTROLLER4_OFFSET]);
    case 3:
      retro_poll_input(2,&buttons[MADAM_PBUS_CONTROLLER3_OFFSET]);
    case 2:
      retro_poll_input(1,&buttons[MADAM_PBUS_CONTROLLER2_OFFSET]);
    case 1:
      retro_poll_input(0,&buttons[MADAM_PBUS_CONTROLLER1_OFFSET]);
    }
}

/************************************
 * libretro implementation
 ************************************/

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name = "4DO";
#ifndef GIT_VERSION
#define GIT_VERSION ""
#endif
   info->library_version = "1.3.2.4" GIT_VERSION;
   info->need_fullpath = true;
   info->valid_extensions = "iso|bin|chd|cue|m3u";
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   memset(info, 0, sizeof(*info));
   info->timing.fps            = 60;
   info->timing.sample_rate    = 44100;
   info->geometry.base_width   = videoWidth;
   info->geometry.base_height  = videoHeight;
   info->geometry.max_width    = videoWidth;
   info->geometry.max_height   = videoHeight;
   info->geometry.aspect_ratio = 4.0 / 3.0;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
   (void)port;
   (void)device;
}

size_t retro_serialize_size(void)
{
   return _3do_SaveSize();
}

bool retro_serialize(void *data, size_t size)
{
  if(size < _3do_SaveSize())
    return false;

  _3do_Save(data);

  return true;
}

bool retro_unserialize(const void *data, size_t size)
{
   _3do_Load((void*)data);
   return true;
}

void retro_cheat_reset(void)
{}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}

static
bool
environ_enabled(const char *key)
{
  int rv;
  struct retro_variable var;

  var.key   = key;
  var.value = NULL;
  rv = retro_environment_cb(RETRO_ENVIRONMENT_GET_VARIABLE,&var);
  if(rv && var.value)
    return (strcmp(var.value,"enabled") == 0);

  return false;
}

static
void
check_env_4do_high_resolution(void)
{
  if(environ_enabled("4do_high_resolution"))
    {
      HightResMode = 1;
      videoWidth   = 640;
      videoHeight  = 480;
    }
  else
    {
      HightResMode = 0;
      videoWidth   = 320;
      videoHeight  = 240;
    }
}

static
void
check_env_4do_cpu_overclock(void)
{
  int rv;
  struct retro_variable var;

  var.key   = "4do_cpu_overclock";
  var.value = NULL;

  rv = retro_environment_cb(RETRO_ENVIRONMENT_GET_VARIABLE,&var);
  if(rv && var.value)
    {
      if (!strcmp(var.value, "1x"))
        freedo_quarz_cpu_set_freq_mul(1.0);
      else if (!strcmp(var.value, "2x"))
        freedo_quarz_cpu_set_freq_mul(2.0);
      else if (!strcmp(var.value, "4x"))
        freedo_quarz_cpu_set_freq_mul(4.0);
    }
}

static
void
check_env_4do_x_button_also_p(void)
{
  x_button_also_p = environ_enabled("4do_x_button_also_p");
}

static
void
check_env_4do_controller_count(void)
{
  int rv;
  struct retro_variable var;

  controller_count = 0;

  var.key   = "4do_controller_count";
  var.value = NULL;

  rv = retro_environment_cb(RETRO_ENVIRONMENT_GET_VARIABLE,&var);
  if(rv && var.value)
    controller_count = atoi(var.value);

  if((controller_count < 0) || (controller_count > 8))
    controller_count = 1;
}

static
void
check_env_set_reset_bits(const char *key,
                         int        *input,
                         int         bitmask)
{
  *input = (environ_enabled(key) ?
            (*input | bitmask) :
            (*input & ~bitmask));
}

static
bool
check_env_nvram_per_game(void)
{
  int rv;
  struct retro_variable var;

  var.key   = "4do_nvram_storage";
  var.value = NULL;

  rv = retro_environment_cb(RETRO_ENVIRONMENT_GET_VARIABLE,&var);
  if(rv && var.value)
    {
      if(strcmp(var.value,"per game"))
        return false;
    }

  return true;
}

static
bool
check_env_nvram_shared(void)
{
  return !check_env_nvram_per_game();
}

static
void
check_variables(void)
{
   check_env_4do_high_resolution();
   check_env_4do_cpu_overclock();
   check_env_4do_x_button_also_p();
   check_env_4do_controller_count();
   check_env_set_reset_bits("4do_hack_timing_1",&fixmode,FIX_BIT_TIMING_1);
   check_env_set_reset_bits("4do_hack_timing_3",&fixmode,FIX_BIT_TIMING_3);
   check_env_set_reset_bits("4do_hack_timing_5",&fixmode,FIX_BIT_TIMING_5);
   check_env_set_reset_bits("4do_hack_timing_6",&fixmode,FIX_BIT_TIMING_6);
   check_env_set_reset_bits("4do_hack_graphics_step_y",&fixmode,FIX_BIT_GRAPHICS_STEP_Y);
}

#define CONTROLLER_DESC(PORT) \
  {PORT, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,   "D-Pad Left" }, \
  {PORT, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,     "D-Pad Up" }, \
  {PORT, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,   "D-Pad Down" }, \
  {PORT, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT,  "D-Pad Right" }, \
  {PORT, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,      "A" }, \
  {PORT, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,      "B" }, \
  {PORT, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,      "C" }, \
  {PORT, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,      "L" }, \
  {PORT, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,      "R" }, \
  {PORT, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "X (Stop)" }, \
  {PORT, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,  "P (Play/Pause)" }

static
void
retro_setup_input_descriptions(void)
{
   struct retro_input_descriptor desc[] =
     {
       CONTROLLER_DESC(0),
       CONTROLLER_DESC(1),
       CONTROLLER_DESC(2),
       CONTROLLER_DESC(3),
       CONTROLLER_DESC(4),
       CONTROLLER_DESC(5),
       CONTROLLER_DESC(6),
       CONTROLLER_DESC(7)
     };

  retro_environment_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS,desc);
}

/* multidisk support */
static bool disk_ejected;
static unsigned int disk_current_index;
static unsigned int disk_count;
static struct disks_state {
	char *fname;
	int internal_index; // for multidisk eboots
} disks[8];

static bool disk_set_eject_state(bool ejected)
{
	disk_ejected = ejected;
	return true;
}

static bool disk_get_eject_state(void)
{
	/* can't be controlled by emulated software */
	return disk_ejected;
}

static unsigned int disk_get_image_index(void)
{
	return disk_current_index;
}

static bool disk_set_image_index(unsigned int index)
{
	if (index >= sizeof(disks) / sizeof(disks[0])) {
		return false;
    }

	if (disks[index].fname == NULL) {
		retro_log_printf_cb(RETRO_LOG_ERROR, "missing disk #%u\n", index);

		// RetroArch specifies "no disk" with index == count,
		// so don't fail here..
		disk_current_index = index;
		return true;
	}

	retro_log_printf_cb(RETRO_LOG_INFO, "switching to disk %u: \"%s\" #%d\n", index,
		disks[index].fname, disks[index].internal_index);

    int ret = retro_cdimage_close(&cdimage);
    if (ret > 0) {
        retro_log_printf_cb(RETRO_LOG_ERROR, "error %d closing cdimage: %d\n", ret, index);
        return false;
    }

    ret = retro_cdimage_open(disks[index].fname, &cdimage);
    if (ret > 0) {
        retro_log_printf_cb(RETRO_LOG_ERROR, "error %d opening cdimage: %d\n", ret, index);
        return false;
    }

	disk_current_index = index;
	return true;
}

static unsigned int disk_get_num_images(void)
{
	return disk_count;
}

static bool disk_replace_image_index(unsigned index,
	const struct retro_game_info *info)
{
	char *old_fname;
	bool ret = true;

	if (index >= sizeof(disks) / sizeof(disks[0]))
		return false;

	old_fname = disks[index].fname;
	disks[index].fname = NULL;
	disks[index].internal_index = 0;

	if (info != NULL) {
		disks[index].fname = strdup(info->path);
		if (index == disk_current_index)
			ret = disk_set_image_index(index);
	}

	if (old_fname != NULL)
		free(old_fname);

	return ret;
}

static bool disk_add_image_index(void)
{
	if (disk_count >= 8)
		return false;

	disk_count++;
	return true;
}

static struct retro_disk_control_callback disk_control = {
	.set_eject_state = disk_set_eject_state,
	.get_eject_state = disk_get_eject_state,
	.get_image_index = disk_get_image_index,
	.set_image_index = disk_set_image_index,
	.get_num_images = disk_get_num_images,
	.replace_image_index = disk_replace_image_index,
	.add_image_index = disk_add_image_index,
};

// just in case, maybe a win-rt port in the future?
#ifdef _WIN32
#define SLASH '\\'
#else
#define SLASH '/'
#endif

#ifndef PATH_MAX
#define PATH_MAX  4096
#endif

static char base_dir[PATH_MAX];

static bool read_m3u(const char *file)
{
	char line[PATH_MAX];
	char name[PATH_MAX];
	FILE *f = fopen(file, "r");
	if (!f)
		return false;

	while (fgets(line, sizeof(line), f) && disk_count < sizeof(disks) / sizeof(disks[0])) {
		if (line[0] == '#')
			continue;
		char *carrige_return = strchr(line, '\r');
		if (carrige_return)
			*carrige_return = '\0';
		char *newline = strchr(line, '\n');
		if (newline)
			*newline = '\0';

		if (line[0] != '\0')
		{
			snprintf(name, sizeof(name), "%s%c%s", base_dir, SLASH, line);
			disks[disk_count++].fname = strdup(name);
		}
	}

	fclose(f);
	return (disk_count != 0);
}

static void extract_directory(char *buf, const char *path, size_t size)
{
   char *base;
   strncpy(buf, path, size - 1);
   buf[size - 1] = '\0';

   base = strrchr(buf, '/');
   if (!base)
      base = strrchr(buf, '\\');

   if (base)
      *base = '\0';
   else
   {
      buf[0] = '.';
      buf[1] = '\0';
   }
}

#if defined(__QNX__) || defined(_WIN32)
/* Blackberry QNX doesn't have strcasestr */

/*
 * Find the first occurrence of find in s, ignore case.
 */
char *
strcasestr(const char *s, const char*find)
{
	char c, sc;
	size_t len;

	if ((c = *find++) != 0) {
		c = tolower((unsigned char)c);
		len = strlen(find);
		do {
			do {
				if ((sc = *s++) == 0)
					return (NULL);
			} while ((char)tolower((unsigned char)sc) != c);
		} while (strncasecmp(s, find, len) != 0);
		s--;
	}
	return ((char *)s);
}
#endif


bool retro_load_game(const struct retro_game_info *info)
{
   size_t i;
   bool is_m3u = (strcasestr(info->path, ".m3u") != NULL);
   int rv;
   enum retro_pixel_format  fmt                = RETRO_PIXEL_FORMAT_XRGB8888;
   const char              *system_directory_c = NULL;
   const char              *full_path          = NULL;

    if (info == NULL || info->path == NULL) {
        retro_log_printf_cb(RETRO_LOG_ERROR, "info->path required\n");
        return false;
    }

   retro_setup_input_descriptions();

   if (!retro_environment_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      if (retro_log_printf_cb)
         retro_log_printf_cb(RETRO_LOG_INFO, "[4DO]: XRGB8888 is not supported.\n");
      return false;
   }

   currentSector = 0;
   sampleCurrent = 0;
   memset(sampleBuffer, 0, sizeof(int32_t) * TEMP_BUFFER_SIZE);

   full_path = info->path;

   *biosPath = '\0';

   for (i = 0; i < sizeof(disks) / sizeof(disks[0]); i++) {
      if (disks[i].fname != NULL) {
         free(disks[i].fname);
         disks[i].fname = NULL;
      }
      disks[i].internal_index = 0;
   }

   disk_current_index = 0;
   extract_directory(base_dir, info->path, sizeof(base_dir));

   if (is_m3u) {
      if (!read_m3u(info->path)) {
         retro_log_printf_cb(RETRO_LOG_ERROR, "failed to read m3u file\n");
         return false;
      }
   } else {
      disk_count = 1;
      disks[0].fname = strdup(info->path);
   }

   rv = retro_cdimage_open(disks[0].fname, &cdimage);
   if(rv == -1)
     return false;

   retro_environment_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_directory_c);
   if (!system_directory_c)
   {
      if (retro_log_printf_cb)
         retro_log_printf_cb(RETRO_LOG_WARN, "[4DO]: no system directory defined, unable to look for panafz10.bin\n");
   }
   else
   {
      char bios_path[1024];
      RFILE *fp;
#ifdef _WIN32
      char slash = '\\';
#else
      char slash = '/';
#endif
      sprintf(bios_path, "%s%c%s", system_directory_c, slash, "panafz10.bin");

      fp = filestream_open(bios_path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);

      if (!fp)
      {
         if (retro_log_printf_cb)
            retro_log_printf_cb(RETRO_LOG_WARN, "[4DO]: panafz10.bin not found, cannot load BIOS\n");
         return false;
      }

      filestream_close(fp);
      strcpy(biosPath, bios_path);
   }

   /* Initialize libfreedo */
   check_variables();
   initVideo();
   _freedo_Interface(FDP_INIT, (void*)*fdcCallback);

   /* XXX: Is this really a frontend responsibility? */
   nvram_init(freedo_arm_nvram_get());
   if(check_env_nvram_shared())
     retro_nvram_load(freedo_arm_nvram_get());

   return true;
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info)
{
   (void)game_type;
   (void)info;
   (void)num_info;
   return false;
}

void retro_unload_game(void)
{
   if(check_env_nvram_shared())
     retro_nvram_save(freedo_arm_nvram_get());

   _freedo_Interface(FDP_DESTROY, (void*)0);

   retro_cdimage_close(&cdimage);

   if (videoBuffer)
      free(videoBuffer);
   videoBuffer = NULL;

   if (frame)
      free(frame);
   frame       = NULL;
}

unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC;
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void*
retro_get_memory_data(unsigned id)
{
  if(id != RETRO_MEMORY_SAVE_RAM)
    return NULL;
  if(check_env_nvram_shared())
    return NULL;

  return freedo_arm_nvram_get();
}

size_t
retro_get_memory_size(unsigned id)
{
  if(id != RETRO_MEMORY_SAVE_RAM)
      return 0;
  if(check_env_nvram_shared())
    return 0;

   return NVRAM_SIZE;
}

void
retro_init(void)
{
  struct retro_log_callback log;
  unsigned level = 5;
  uint64_t serialization_quirks = RETRO_SERIALIZATION_QUIRK_SINGLE_SESSION;

  if(retro_environment_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
    retro_set_log_printf_cb(log.log);

  retro_environment_cb(RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL, &level);
  retro_environment_cb(RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS, &serialization_quirks);
  retro_environment_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE, &disk_control);

  freedo_cdrom_set_callbacks(cdimage_get_size,
                             cdimage_set_sector,
                             cdimage_read_sector);
}

void retro_deinit(void)
{
}

void
retro_reset(void)
{
  if(check_env_nvram_shared())
    retro_nvram_save(freedo_arm_nvram_get());

  _freedo_Interface(FDP_DESTROY, NULL);

  currentSector = 0;

  sampleCurrent = 0;
  memset(sampleBuffer, 0, sizeof(int32_t) * TEMP_BUFFER_SIZE);

  check_variables();
  initVideo();

  _freedo_Interface(FDP_INIT, (void*)*fdcCallback);

  nvram_init(freedo_arm_nvram_get());
  if(check_env_nvram_shared())
    retro_nvram_load(freedo_arm_nvram_get());
}

void
retro_run(void)
{
  bool updated = false;
  if(retro_environment_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE,&updated) && updated)
    check_variables();

  update_input();

  _freedo_Interface(FDP_DO_EXECFRAME, frame); /* FDP_DO_EXECFRAME_MT ? */

  video_cb(videoBuffer, videoWidth, videoHeight, videoWidth << 2);
}
