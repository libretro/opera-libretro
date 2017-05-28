#include <stdio.h>
#include <string.h>
#include "libretro.h"
#include "cuefile.h"

#define STRING_MAX 4096

static FILE *cue_get_file_for_image(const char *path)
{
	char cue_path_base[STRING_MAX];

	strncpy(cue_path_base, path, STRING_MAX);

	char *last_dot = strrchr(cue_path_base, '.');
	if (last_dot == NULL) return NULL;

	// cut original extension
	*(last_dot) = '\0';

	char cue_path[PATH_MAX];
	char *exts[] = {".cue", ".CUE"};
	int i;
	for(i=0; i<2; i++) {
		strcpy(cue_path, cue_path_base);
		strcat(cue_path, exts[i]);

		FILE *cue_file = fopen(cue_path, "r");
		if (cue_file) return cue_file;
	}

	return NULL;
}

static void str_to_upper(char *s) {
	for ( ; *s; ++s) *s = toupper(*s);
}

cueFile *cue_get(const char *path, retro_log_printf_t log_cb) {
	FILE *cue_file = cue_get_file_for_image(path);
	if (!cue_file) {
		return NULL;
	}

    cueFile *cue = (cueFile *)malloc(sizeof(cueFile));
    cue->cd_format = CUE_MODE_UNKNOWN;

    char line[STRING_MAX];
    while ((fgets(line, STRING_MAX, cue_file))) {
    	str_to_upper(line);
    	if (strstr(line, "TRACK 01")) {
			if (strstr(line,"TRACK 01 MODE1/2048")) {
				cue->cd_format = MODE1_2048;
			} else if (strstr(line, "TRACK 01 MODE1/2352")) {
				cue->cd_format = MODE1_2352;
			} else if (strstr(line, "TRACK 01 MODE2/2352")) {
				cue->cd_format = MODE2_2352;
			} else {
				log_cb(RETRO_LOG_INFO, "[4DO]: Unknown file format in CUE file: %s -> %s", line);
			}
			break;
    	}
    }
    fclose(cue_file);

    if (cue->cd_format != CUE_MODE_UNKNOWN) {
    	return cue;
    }
    free(cue);
    return NULL;
}

const char *cue_get_cd_format_name(CD_format cd_format) {
	switch (cd_format) {
	case MODE1_2048: return "MODE1/2048";
	case MODE1_2352: return "MODE1/2352";
	case MODE2_2352: return "MODE2/2352";
	default: return "UNKNOWN";
	}
}
