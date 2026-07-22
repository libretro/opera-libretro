/*
 * imgui_debug_main.cpp
 * Dear ImGUI/SDL2/OpenGL Debugging tool for Opera-Libretro
 */
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough" 
#pragma GCC diagnostic ignored "-Wunused-parameter"

#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_audio.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl2.h"
#include "imgui_memory_editor.h"

#include "libretro.h"
#include "opera_3do.h"
#include "opera_arm.h"
#include "opera_mem.h"
#include "opera_lr_dsp.h"

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

// SDL modules
static SDL_Event event;
static SDL_Window *window;
static SDL_GLContext glContext;
static SDL_AudioDeviceID audioDevice;

#define VIDEO_BUFFER_SIZE 307200 //320x240x4
unsigned char g_backbuffer_opera[VIDEO_BUFFER_SIZE];
unsigned char g_backbuffer[VIDEO_BUFFER_SIZE];

#define AUDIO_BUFFER_SIZE 0x10000
int16_t g_audiobuffer[AUDIO_BUFFER_SIZE];
int g_audiobuffer_begin = 0;
int g_audiobuffer_end = 0;

// 3do system vars
#define DBG_LOCAL_CFG_FILE "debugger_config.txt"
char systemDirectory[] = ".";
char biosfile[256] = ""; // .bin file, must be local
char isofile[256] = ""; // full path
#define DBG_OFF 0
#define DBG_INIT 1
#define DBG_PAUSED 2
#define DBG_RUN 3
#define DBG_STEP 4
#define DBG_STEP_NEXT 5
#define DBG_QUIT -1
char dbg_state = 0;
unsigned int last_tick; 

// opera libretro structs
int16_t g_joypad_1_state;
retro_system_info g_sys_info;
retro_game_info g_game_info;

// debugger/disassembler vars
uint32_t g_code = 0;
char g_disasm_txt[60];
char g_input_address[10];
uint32_t g_curr_pc = 0;
bool g_debug_follow_pc = 0;
uint32_t g_bp_address[10];
uint32_t g_bp_code[10];

static MemoryEditor mem_editor;
static int mem_option = 0;
const char *mem_name[] = {     "DRAM",     "VRAM",      "ROM",    "NVRAM",    "SPORT",    "MADAM",     "CLIO" };
const int mem_offset[] = {          0, 0x00200000, 0x00300000, 0x00310000, 0x00320000, 0x00330000, 0x00340000 };
const int mem_length[] = { 0x00200000, 0x00100000, 0x00100000, 0x00010000, 0x00010000, 0x00010000, 0x00010000 };
uint32_t mem_gotoaddr = 0;

int readTxtLine(char *output, int size, FILE *fp) {
	char ch = 0;
	int count = 0;
	memset(output, 0, size);
	while(1) {
		ch = fgetc(fp);
		if (ch == '\r') continue;
		if (ch == '\n') break;
		if (count == size) break;
		output[count] = ch;
		count++;
	}
	return count;
}

// convert Opera BGRA 24bpp to OpenGL RGBA flipped upside down 
void gfx_draw_to_window() {
	int i, line;
	uint8_t *src;
	uint8_t *dst;

	src = (uint8_t*) g_backbuffer_opera;
	line = 240;
	while(line--) {
		dst = line * 320 * 4 + g_backbuffer;

		i=320;
		while(i--) {
			*dst++ = src[2];
			*dst++ = src[1];
			*dst++ = src[0];
			*dst++ = 0xFF; 
			src += 4;
		}
	}
}

// fifo queue out
void sfx_sdl_audio_callback(void* userdata, int16_t* stream, int len) {
	uint8_t* src;
	uint8_t* dst;
	int firstlen = len;
	int secondlen = 0;
	int index = g_audiobuffer_begin * 2; 
	int buf_size = AUDIO_BUFFER_SIZE * 2;

	if (index + len > buf_size) {
		firstlen = buf_size - index;
		secondlen = len - firstlen;
	}

	src = (uint8_t*)(g_audiobuffer);
	src += index;
	dst = (uint8_t*)(stream);
	memcpy(dst, src, firstlen);

	if (secondlen > 0) {
		src = (uint8_t*)(g_audiobuffer);
		dst = (uint8_t*)(stream);
		dst += firstlen;
		memcpy(dst, src, secondlen);
	}

	g_audiobuffer_begin += len >> 1;
	g_audiobuffer_begin %= AUDIO_BUFFER_SIZE;

	//FIX: fill remaining bytes with silence
}

/** Libretro callbacks */

// fifo queue in
static size_t _debugger_set_audio_sample_batch(const int16_t *data, size_t frames) {
	uint8_t* src;
	uint8_t* dst;
	int len = frames * 4; //1 frame = 4 bytes
	int firstlen = len;
	int secondlen = 0;
	int index = g_audiobuffer_end * 2; 
	int buf_size = AUDIO_BUFFER_SIZE * 2;

	if (index + len > buf_size) {
		firstlen = buf_size - index;
		secondlen = len - firstlen;
	}

	src = (uint8_t*)(data);
	dst = (uint8_t*)(g_audiobuffer);
	dst += index;
	memcpy(dst, src, firstlen);

	if (secondlen > 0) {
		src += firstlen;
		dst = (uint8_t*)(g_audiobuffer);
		memcpy(dst, src, secondlen);
	}

	g_audiobuffer_end += frames * 2;
	g_audiobuffer_end %= AUDIO_BUFFER_SIZE;
	return 0;
}

static void _debugger_set_audio_sample(int16_t left, int16_t right) {
	// stub
}

static void _debugger_log_printf(enum retro_log_level level_, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	printf(fmt, ap);
	va_end(ap);
}

// single joypad only
static int16_t _debugger_input_state(unsigned port_, unsigned device_,
                                     unsigned index_, unsigned id_) {

  if((port_ != 0) || ((device_ & RETRO_DEVICE_MASK) != RETRO_DEVICE_JOYPAD))
    return 0;

  return (g_joypad_1_state >> id_) & 1;
}

static void _debugger_set_input_poll() { 
	// stub
}

static void _debugger_video_refresh(const void *data, unsigned width, unsigned height, size_t pitch) {
	// stub
}

// breakpoint check  
bool _debugger_breakpoint(void) {
	uint32_t instruction = 0;
	uint8_t * mem;

	g_curr_pc = opera_arm_get_pc();

	if (dbg_state == DBG_STEP) {
		dbg_state = DBG_STEP_NEXT;
		return false;
	} 
	if (dbg_state == DBG_STEP_NEXT) {
		dbg_state = DBG_PAUSED;
		return true;
	}

	for (int i = 0; i < 10; i++) {
		if (g_bp_address[i] != 0 && g_bp_address[i] == g_curr_pc) {
			dbg_state = DBG_PAUSED;
			SDL_PauseAudioDevice(audioDevice, 1);
			return true;
		} 
		if (g_bp_code[i] != 0) { 
			mem = g_curr_pc + DRAM;
			g_code = (unsigned int)(mem[0] | ((mem[1])<<8) | ((mem[2])<<16) | ((mem[3])<<24) );
			if (g_bp_code[i] == g_code) {
				dbg_state = DBG_PAUSED;
				SDL_PauseAudioDevice(audioDevice, 1);
				return true;
			}
		}
	}
	return false;
}

// libretro callbacks
static bool _debugger_retro_interface(unsigned int cmd_, void *data_) {
  retro_variable *var;
  switch (cmd_) {
  case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    *(const char **)data_ = systemDirectory;
    return true;

  case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
    ((struct retro_log_callback *)data_)->log = _debugger_log_printf;
    return true;

  case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
    //*(enum retro_pixel_format*)data_ = VDLP_PIXEL_FORMAT_0RGB1555; ???
	opera_vdlp_set_pixel_format(VDLP_PIXEL_FORMAT_XRGB8888);
    return true;

  case RETRO_ENVIRONMENT_GET_VARIABLE:
    var = (retro_variable *)data_;
    printf("Libretro get var: %s\n", var->key);

    if (strcmp("opera_bios", var->key) == 0) {
      var->value = biosfile;
      return true;
    } else if (strcmp("opera_kprint", var->key) == 0) {
      var->value = "enabled";
      return true;
    }
    return false;

  default:
    return false;
    break;
  }
}

void dbg_init3do() {
	if (dbg_state == DBG_OFF) {
		printf("Initializing Opera...\n");
		printf("BIOS: %s\n", biosfile);
		printf("ISO: %s\n", isofile);

		memset(&g_sys_info, 0, sizeof(g_sys_info));
		retro_get_system_info(&g_sys_info);
		retro_set_environment((retro_environment_t)_debugger_retro_interface);

		retro_set_video_refresh((retro_video_refresh_t)_debugger_video_refresh);

		retro_set_audio_sample((retro_audio_sample_t) _debugger_set_audio_sample);
		retro_set_audio_sample_batch((retro_audio_sample_batch_t) _debugger_set_audio_sample_batch);

		retro_set_input_state((retro_input_state_t)_debugger_input_state);
		retro_set_input_poll((retro_input_poll_t) _debugger_set_input_poll);

		retro_init();
		retro_set_controller_port_device(0, 1);

		memset(&g_game_info, 0, sizeof(g_game_info));
		g_game_info.path = isofile;
		if (retro_load_game(&g_game_info)) {
			dbg_state = DBG_RUN;
			SDL_PauseAudioDevice(audioDevice, 0);
		} else {
			printf("retro_load_game failed\n");
		}
	}

	opera_vdlp_set_video_buffer(g_backbuffer_opera); //opera video
#ifdef __IMGUI_DEBUGGER__
	opera_3do_set_debug_callback(_debugger_breakpoint);
#endif
}

void dbg_process3do() {
	if (dbg_state >= DBG_RUN) {
		retro_run();
		gfx_draw_to_window();
	}
}

void dbg_destroy3do() { 
	if (dbg_state >= DBG_INIT) {
		retro_unload_game(); 
		retro_deinit();
	}
	dbg_state = DBG_OFF;
	SDL_PauseAudioDevice(audioDevice, 1);
}

void dbg_reset3do() { 
	if (dbg_state >= DBG_INIT) {
		retro_reset();
		opera_vdlp_set_video_buffer(g_backbuffer_opera); //set opera video again
		dbg_state = DBG_RUN;
		SDL_PauseAudioDevice(audioDevice, 0);
	}
}

void dbg_pause3do() {
	if (dbg_state == DBG_OFF) {
		dbg_init3do();
	} else if (dbg_state == DBG_RUN) {
		dbg_state = DBG_PAUSED;
		SDL_PauseAudioDevice(audioDevice, 1);
		mem_gotoaddr = g_curr_pc - mem_offset[mem_option];
	} else if (dbg_state == DBG_PAUSED || dbg_state == DBG_INIT) {
		dbg_state = DBG_RUN;
		SDL_PauseAudioDevice(audioDevice, 0);
	}
}

void dbg_step3do() {
	if (dbg_state == DBG_PAUSED) {
		dbg_state = DBG_STEP;
		SDL_PauseAudioDevice(audioDevice, 1);
	} else if (dbg_state == DBG_RUN) {
		dbg_pause3do();
	}
}

void dbg_toggleBreakpoint(char type, int value) {
	for (int i = 0; i < 10; i++) {
		if (type == 0 && g_bp_address[i] == value) {
			g_bp_address[i] = 0;
			printf("Address BP undefined %d %x\n", type, value);
			return;
		}
		if (type == 1 && g_bp_code[i] == value) {
			g_bp_code[i] = 0;
			printf("Code BP undefined %d %x\n", type, value);
			return;
		}
	}
	for (int i = 0; i < 10; i++) {
		if (type == 0 && g_bp_address[i] == 0) {
			g_bp_address[i] = value;
			printf("Address BP defined %d %x\n", type, value);
			return;
		}
		if (type == 1 && g_bp_code[i] == 0) {
			g_bp_code[i] = value;
			printf("Code BP defined %d %x\n", type, value);
			return;
		}
	}	
}

/**** Quick and dirty ARM60 instruction disassembler ****/

const char *C_CONDITIONS[] = {"EQ", "NE", "CS", "CC", "MI", "PL", "VS", "VC",
                        "HI", "LS", "GE", "LT", "GT", "LE", ""/*"AL"*/, "NV"};

const char *C_SHIFTS[] = {"LSL", "LSR", "ASR", "ROR"};

const char *C_BDT_MODES[] = { "DB", "DA", "IA", "IB" };

const char *C_MUL_OPCODES[] = { "MUL", "MLA", "MULL", "MLAL" }; 

const char *C_ALU_OPCODES[] = {"AND",  "ANDS", "EOR",  "EORS", "SUB",  "SUBS", "RSB",
                         "RSBS", "ADD",  "ADDS", "ADC",  "ADCS", "SBC",  "SBCS",
                         "RSC",  "RSCS", "TST",  "TSTS", "TEQ",  "TEQS", "CMP",
                         "CMPS", "CMN",  "CMNS", "ORR",  "ORRS", "MOV",  "MOVS",
                         "BIC",  "BICS", "MVN",  "MVNS"};

void dbg_disasm(uint32_t cmd) {
	char shift_txt[16];
	int shift;
	g_disasm_txt[0] = 0;
	shift_txt[0] = 0;

	if (cmd == 0) {
		return;
	}
	if (cmd < 0x01000000) {
		sprintf(g_disasm_txt, "ADDR %x", cmd);
		return;
	}

	// shifts
	if((cmd&0x2000090)!=0x90) {
		if (cmd&(1<<25)) {
			shift = (cmd>>7) &0x1e;
			if (shift) 
				sprintf(shift_txt, "rotr %d",
						shift);
		} else {
			shift = cmd&(1<<4) ? (cmd>>8)&0xf : (cmd>>7)&0x1f;
			if (shift) 
				sprintf(shift_txt, "%s %d",
						C_SHIFTS[(cmd>>5)&3], 
						shift);
		}
	}

	// instruction
	switch ((cmd >> 24) & 0xf) {
		case 0x0: // Multiply
			if ((cmd & 0x0fc000f0) == 0x00000090) { 
				//<MUL|MLA>{cond}{S} Rd,Rm,Rs[Rn] 
				//<S|U><MULL|MLAL>{cond}{S} RdLo,RdHi,Rm,Rs
				sprintf(g_disasm_txt, "%s%s %d, %d, %d", 
					C_MUL_OPCODES[(cmd >> 20) & 3],
					C_CONDITIONS[(cmd>>28)&0xf],
					(cmd >> 16) & 0xf, 
					(cmd >> 12) & 0xf, 
					(cmd >> 8) & 0xf);
				break;
			}
		case 0x1: // Single Data Swap
			if ((cmd & 0x0fb00ff0) == 0x01000090) {
				//<SWP>{cond}{B} Rd,Rm,[Rn]
				sprintf(g_disasm_txt, "SWP%s %d, %d, %d", 
					C_CONDITIONS[(cmd>>28)&0xf], 
					(cmd >> 16) & 0xf, 
					(cmd >> 12) & 0xf, 
					(cmd >> 8) & 0xf);
				break;
			}
		case 0x2: // ALU
		case 0x3:
			if((cmd&0x2000090)!=0x90) {
				sprintf(g_disasm_txt, "%s%s %d, %d, %d %s", 
					C_ALU_OPCODES[(cmd>>20)&0x1f], 
					C_CONDITIONS[(cmd>>28)&0xf], 
					(cmd >> 12) & 0xf,
					(cmd >> 16) & 0xf,
					cmd & 0xf, 
					shift_txt);
				break;
			}
		case 0x6: // Undefined
		case 0x7:
			if((cmd&0x0e000010)==0x06000010) {
				break;
			}
		case 0x4: // Single Data Transfer
		case 0x5:
			if((cmd&0x2000090)!=0x2000090) {
				//<LDR|STR>{cond}{B}{T} Rd,<Address>, <LDR|STR>{cond}<H|SH|SB> Rd,<address>
				sprintf(g_disasm_txt, "%s%s%s%s %d, %d %s", 
					cmd&(1<<20) ? "LDR" : "STR",
					C_CONDITIONS[(cmd>>28)&0xf], 
					cmd&(1<<22) ? "B" : "",
					cmd&(1<<21) ? "T" : "",
					(cmd >> 12) & 0xf, 
					(cmd >> 16) & 0xf,
					shift_txt);
				break;
			}
		case 0x8: // Block Data Transfer
		case 0x9:
			//<LDM|STM>{cond}<FD|ED|FA|EA|IA|IB|DA|DB> Rn{!},<Rlist>{^}
			sprintf(g_disasm_txt, "%s%s%s %d %d", 
				cmd&(1<<20) ? "LDM" : "STM", 
				C_CONDITIONS[(cmd>>28)&0xf],
				C_BDT_MODES[(cmd>>22)&3],
				(cmd >> 16) & 0xf, 
				(cmd >> 12) & 0xf);
			break;
		case 0xa: // BRANCH
		case 0xb:
			//B{L}{cond} <expression>
			sprintf(g_disasm_txt, "%s%s %x", 
				cmd&(1<<24) ? "BL" : "B",
				C_CONDITIONS[(cmd>>28)&0xf], 
				(((cmd & 0xffffff) | ((cmd & 0x800000) ? 0xff000000 : 0)) << 2) + 4);
			break;
		case 0xf: // SWI
			//SWI{cond} <expression>
			sprintf(g_disasm_txt, "SWI %x", cmd & 0xffffff);
			break;
		default: //coprocessor
			if (cmd&0xC000000) { 
				//<LDC|STC>{cond}{L} p#,cd,<Address>
				sprintf(g_disasm_txt, "%s%s%s %x %x %x", 
					cmd&(1<<20) ? "LDC" : "STC",
					C_CONDITIONS[(cmd>>28)&0xf],
					cmd&(1<<22) ? "L" : "",
					(cmd >> 8) & 0xf, 
					(cmd >> 12) & 0xf, 
					(cmd >> 16) & 0xf);
			}
	};
}

void handleKeys() {
	if (event.type == SDL_KEYDOWN) {
		switch (event.key.keysym.sym) {
		case SDLK_LEFT:
			g_joypad_1_state |= 1U << RETRO_DEVICE_ID_JOYPAD_LEFT;
			break;
		case SDLK_RIGHT:
			g_joypad_1_state |= 1U << RETRO_DEVICE_ID_JOYPAD_RIGHT;
			break;
		case SDLK_UP:
			g_joypad_1_state |= 1U << RETRO_DEVICE_ID_JOYPAD_UP;
			break;
		case SDLK_DOWN:
			g_joypad_1_state |= 1U << RETRO_DEVICE_ID_JOYPAD_DOWN;
			break;
		case SDLK_a: //A
			g_joypad_1_state |= 1U << RETRO_DEVICE_ID_JOYPAD_Y;
			break;
		case SDLK_s: //B
			g_joypad_1_state |= 1U << RETRO_DEVICE_ID_JOYPAD_B;
			break;
		case SDLK_d: //C
			g_joypad_1_state |= 1U << RETRO_DEVICE_ID_JOYPAD_A;
			break;
		case SDLK_q: //L
			g_joypad_1_state |= 1U << RETRO_DEVICE_ID_JOYPAD_L;
			break;
		case SDLK_w: //R
			g_joypad_1_state |= 1U << RETRO_DEVICE_ID_JOYPAD_R;
			break;
		case SDLK_SPACE: //X
			g_joypad_1_state |= 1U << RETRO_DEVICE_ID_JOYPAD_SELECT;
			break;
		case SDLK_RETURN: //P
			g_joypad_1_state |= 1U << RETRO_DEVICE_ID_JOYPAD_START;
			break;
		case SDLK_F5:
			dbg_step3do();
			break;
		case SDLK_F8:
			dbg_pause3do();
			break;
		}
	}
	if (event.type == SDL_KEYUP) {
		switch (event.key.keysym.sym) {
		case SDLK_LEFT:
			g_joypad_1_state ^= 1U << RETRO_DEVICE_ID_JOYPAD_LEFT;
			break;
		case SDLK_RIGHT:
			g_joypad_1_state ^= 1U << RETRO_DEVICE_ID_JOYPAD_RIGHT;
			break;
		case SDLK_UP:
			g_joypad_1_state ^= 1U << RETRO_DEVICE_ID_JOYPAD_UP;
			break;
		case SDLK_DOWN:
			g_joypad_1_state ^= 1U << RETRO_DEVICE_ID_JOYPAD_DOWN;
			break;
		case SDLK_a: //A
			g_joypad_1_state ^= 1U << RETRO_DEVICE_ID_JOYPAD_Y;
			break;
		case SDLK_s: //B
			g_joypad_1_state ^= 1U << RETRO_DEVICE_ID_JOYPAD_B;
			break;
		case SDLK_d: //C
			g_joypad_1_state ^= 1U << RETRO_DEVICE_ID_JOYPAD_A;
			break;
		case SDLK_q: //L
			g_joypad_1_state ^= 1U << RETRO_DEVICE_ID_JOYPAD_L;
			break;
		case SDLK_w: //R
			g_joypad_1_state ^= 1U << RETRO_DEVICE_ID_JOYPAD_R;
			break;
		case SDLK_SPACE: //X
			g_joypad_1_state ^= 1U << RETRO_DEVICE_ID_JOYPAD_SELECT;
			break;
		case SDLK_RETURN: //P
			g_joypad_1_state ^= 1U << RETRO_DEVICE_ID_JOYPAD_START;
			break;
		default:
			break;
		}
	}
}

const char *C_BP_SIGN[] = { "   ", "BP*", "BP@" };
const char *C_CODE = "CODE";

void gui_process() {
	int pc;
	int address;
	unsigned char * mem;
	char * name;
	char ptr_sign;
	int bp_sign;
	FILE *cfgfile;

	pc = g_curr_pc;

	ImGui_ImplOpenGL2_NewFrame();
	ImGui_ImplSDL2_NewFrame();

	ImGui::NewFrame();
	{
		ImGui::SetNextWindowSize(ImVec2(480,800));
		ImGui::Begin("3DO Debugger");
		ImGui::SetNextItemWidth(800.0f);
		ImGui::Text("Welcome to the 3DO Debugger tool!");
		ImGui::Text("ISO:");
		ImGui::SameLine();
		ImGui::InputText("##ISO", isofile, 256);

		ImGui::Text("BIOS:");
		ImGui::SameLine();
		ImGui::InputText("##BIOS", biosfile, 256);

		if (ImGui::Button("Init")) { 
			dbg_init3do();
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset")) { 
			dbg_reset3do();
		}
		ImGui::SameLine();
		if (ImGui::Button("Shutdown")) { 
			dbg_destroy3do();
		}
		ImGui::SameLine();
		if (ImGui::Button("Pause")) { 
			dbg_pause3do();
		}
		ImGui::SameLine();
		if (ImGui::Button("Step")) {
			dbg_step3do(); 
		}
		ImGui::SameLine();
		ImGui::Checkbox("Follow PC", &g_debug_follow_pc);

		if (dbg_state == DBG_OFF) {
			ImGui::Text("System Off");
		} else if (dbg_state == DBG_INIT) {
			ImGui::Text("Initialized");
		} else if (dbg_state == DBG_RUN) {
			ImGui::Text("Running...");
		} else {
			ImGui::Text("Stopped at: %x", pc + mem_offset[mem_option]);
		}

		ImGuiStyle& style = ImGui::GetStyle();
		const float heightSeparator = style.ItemSpacing.y;
		float footerHeight = 0;
		footerHeight += heightSeparator * 2 + ImGui::GetTextLineHeightWithSpacing();
		
		ImGui::BeginChild("##ScrollingRegion", ImVec2(0, -footerHeight), true, ImGuiWindowFlags_HorizontalScrollbar);

		ImGuiListClipper clipper;
		clipper.Begin(mem_length[mem_option] / 4);

		if (g_debug_follow_pc) {
			mem_gotoaddr = g_curr_pc - mem_offset[mem_option] - 10;
		}
		if (mem_gotoaddr != 0) {
			ImGui::SetScrollY((mem_gotoaddr / 4) * 17 + 4);
			mem_gotoaddr = 0;
		}

		ImDrawList* drawList = ImGui::GetWindowDrawList();		
		while (clipper.Step()) {
			if (DRAM == 0) break;
			for (int x = clipper.DisplayStart; x < clipper.DisplayEnd; x++) {    
				address = x*4 + mem_offset[mem_option];
				mem = address + DRAM;
				g_code = (unsigned int)(mem[0] | ((mem[1])<<8) | ((mem[2])<<16) | ((mem[3])<<24) );
				if (mem_option == 0 && address < 0x400000) {
					dbg_disasm(g_code);
					name = (char*)C_CODE;
				} else {
					name = (char*)mem_name[mem_option];
					g_disasm_txt[0] = 0;
				}
				bp_sign = 0;
				for (int i=0;i<10;i++) {
					if (g_bp_address[i] != 0 && g_bp_address[i] == address) {
						bp_sign = 1;
						break;
					} else if (g_bp_code[i] != 0 && g_bp_code[i] == g_code) {
						bp_sign = 2;
						break;
					}
				}
				if (address == pc) {
					ptr_sign = '>';
				} else {
					ptr_sign = ' ';
				}
				ImGui::PushID(address);
				ImGui::Text("%c%s %s:%8.8x: %8.8x   %s", ptr_sign, C_BP_SIGN[bp_sign], name, (x*4), g_code, g_disasm_txt);
				if (ImGui::BeginPopupContextItem("ItemLoopContextMenu")) {
					if (ImGui::MenuItem("Set address breakpoint")) { dbg_toggleBreakpoint(0, address); }
					if (ImGui::MenuItem("Set code breakpoint")) { dbg_toggleBreakpoint(1, g_code); }
					if (ImGui::MenuItem("Go to Memory Editor")) { mem_editor.GotoAddrAndHighlight(x*4, x*4+4); }
					ImGui::EndPopup();
				}
				ImGui::PopID();
			}
		}
	    ImGui::EndChild();

		ImGui::Text("Address:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(100);
        if (ImGui::InputText("##addr", g_input_address, IM_ARRAYSIZE(g_input_address), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
            size_t goto_addr;
            if (sscanf(g_input_address, "%8lX", &goto_addr) == 1) {
                mem_gotoaddr = goto_addr;
            }
        }
		ImGui::SameLine();
		ImGui::Combo("##memblock", &mem_option, mem_name, IM_ARRAYSIZE(mem_name));

        ImGui::End();	
	}

	if (DRAM) {
		ImGui::SetNextWindowSize(ImVec2(480,800));
		mem_editor.DrawWindow("Memory Editor", DRAM + mem_offset[mem_option], mem_length[mem_option]);
	}

	// Rendering
	ImGui::Render();
	glViewport(0, 0, 480, 640);
	//glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
	//glClear(GL_COLOR_BUFFER_BIT);
	//glUseProgram(0); // You may want this if using this code in an OpenGL 3+ context where shaders may be bound
	ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

	// Update and Render additional Platform Windows
	// (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
	//  For this specific demo app we could also call SDL_GL_MakeCurrent(window, gl_context) directly)
	//if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	//{
		SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
		SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
	//}
}

int main(int argc, char **argv) {
	FILE *cfgfile;
	int wait;
	SDL_AudioSpec desiredSpec, obtainedSpec;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
		printf("SDL could not be initialized! SDL_Error: %s\n", SDL_GetError());
		return 0;
	}

#if defined linux && SDL_VERSION_ATLEAST(2, 0, 8)
	if (!SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0")) {
		printf("SDL can not disable compositor bypass!\n");
		return 0;
	}
#endif

	window = SDL_CreateWindow("3DO Screen", 100, 100, //
			SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);
	if (!window) {
		printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
		return 0;
	}

	// start OpenGL
	glContext = SDL_GL_CreateContext(window);
	if (!glContext) {
		printf("GL Context could not be created! SDL_Error: %s\n", SDL_GetError());
	}

	// start SDL Audio
	desiredSpec.freq = 44100;
	desiredSpec.format = AUDIO_S16SYS; // Signed 16-bit audio, system endian
	desiredSpec.channels = 2; // 1 Mono/2 Stereo
	desiredSpec.samples = 0; // Buffer size (power of 2)
	desiredSpec.callback = (SDL_AudioCallback) sfx_sdl_audio_callback;
	desiredSpec.userdata = NULL;

	audioDevice = SDL_OpenAudioDevice(NULL, 0, &desiredSpec, &obtainedSpec, 0);
	if (audioDevice == 0) {
		printf("Audio device could not be created! SDL_Error: %s\n", SDL_GetError());
	}

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(window, glContext);
    ImGui_ImplOpenGL2_Init();

	// setup OpenGL
    SDL_GL_MakeCurrent(window, glContext);
    SDL_GL_SetSwapInterval(1);
	glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_TEXTURE_2D);

	// read our config file
	cfgfile = fopen(DBG_LOCAL_CFG_FILE,"r");
	if (cfgfile) {
		readTxtLine(biosfile, 256, cfgfile);
		readTxtLine(isofile, 256, cfgfile);
		fclose(cfgfile);
		printf("Configuration file found: %s.\n", DBG_LOCAL_CFG_FILE);
	}

    // Main Loop
    while (dbg_state != DBG_QUIT) {
		// window events
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
			handleKeys();
            if (event.type == SDL_QUIT) {
                dbg_state = DBG_QUIT;
            }
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window)) {
                dbg_state = DBG_QUIT;
            }
        }
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(10);
            continue;
        }

		// process ImGUI controls
		gui_process();

		// process a single 3DO frame
		dbg_process3do();

		// display 3DO frame
		SDL_GL_SwapWindow(window);
		glBindTexture(GL_TEXTURE_2D, 0);
		glPixelZoom(2, 2);
		glDrawPixels(320, 240, GL_RGBA, GL_UNSIGNED_BYTE, &g_backbuffer);

		// Wait ~16.66ms (60 Hz)
		wait = 17 - (SDL_GetTicks() - last_tick); 
		if (wait > 0)
			SDL_Delay(wait);
		last_tick = SDL_GetTicks();
    }

	// write config file
	cfgfile = fopen(DBG_LOCAL_CFG_FILE,"w");
	if (cfgfile) {
		fprintf(cfgfile, "%s\r\n", biosfile);
		fprintf(cfgfile, "%s\r\n", isofile);
		fclose(cfgfile);
		printf("File saved: %s.\n", DBG_LOCAL_CFG_FILE);
	}

    // Cleanup
	printf("Exiting FreeDO.\n");
	dbg_destroy3do();
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
	SDL_GL_DeleteContext(glContext);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
