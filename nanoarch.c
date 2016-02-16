
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <errno.h>
#include <dlfcn.h>

#include "libretro.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <alsa/asoundlib.h>
#include <SDL.h>
#include "SDL/SDL_rotozoom.h"
#include "SDL/SDL_gfxPrimitives.h"

// forward declarations:
static void core_log(enum retro_log_level level, const char *fmt, ...);
static bool video_set_pixel_format(unsigned format);
static void video_refresh(const void *data, unsigned width, unsigned height, unsigned pitch);

//static GLFWwindow *g_win = NULL;
//static snd_pcm_t *g_pcm = NULL;  // variable used for audio
//static float g_scale = 3;

//static GLfloat g_vertex[] = {
//	-1.0f, -1.0f, // left-bottom
//	-1.0f,  1.0f, // left-top
//	 1.0f, -1.0f, // right-bottom
//	 1.0f,  1.0f, // right-top
//};
//static GLfloat g_texcoords[] ={
//	0.0f,  1.0f,
//	0.0f,  0.0f,
//	1.0f,  1.0f,
//	1.0f,  0.0f,
//};

// holds pixel/screen settings
static struct {
//	GLuint tex_id;
//	GLuint pitch;
//	GLint tex_w, tex_h;
//	GLuint clip_w, clip_h;
//
//	GLuint pixfmt;
//	GLuint pixtype;
//
//	GLuint bpp;

	// nadav implementations below:
	struct retro_game_geometry rGeom;
    uint32_t rmask;
    uint32_t gmask;
    uint32_t bmask;
    uint32_t amask;
    uint32_t nbpp;

	SDL_Surface* screen;
} g_video  = {0};

// holds pointers to core's functions
static struct {
	void *handle;
	bool initialized;

	void (*retro_init)(void);
	void (*retro_deinit)(void);
	unsigned (*retro_api_version)(void);
	void (*retro_get_system_info)(struct retro_system_info *info);
	void (*retro_get_system_av_info)(struct retro_system_av_info *info);
	void (*retro_set_controller_port_device)(unsigned port, unsigned device);
	void (*retro_reset)(void);
	void (*retro_run)(void);
//	size_t retro_serialize_size(void);
//	bool retro_serialize(void *data, size_t size);
//	bool retro_unserialize(const void *data, size_t size);
//	void retro_cheat_reset(void);
//	void retro_cheat_set(unsigned index, bool enabled, const char *code);
	bool (*retro_load_game)(const struct retro_game_info *game);
//	bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info);
	void (*retro_unload_game)(void);
//	unsigned retro_get_region(void);
//	void *retro_get_memory_data(unsigned id);
//	size_t retro_get_memory_size(unsigned id);
} g_retro;


// halps to map keyboard buttons to retro buttons
struct keymap {
	unsigned k;
	unsigned rk;
};

struct keymap g_binds[] = {
//	{ GLFW_KEY_X, RETRO_DEVICE_ID_JOYPAD_A },
//	{ GLFW_KEY_Z, RETRO_DEVICE_ID_JOYPAD_B },
//	{ GLFW_KEY_A, RETRO_DEVICE_ID_JOYPAD_Y },
//	{ GLFW_KEY_S, RETRO_DEVICE_ID_JOYPAD_X },
//	{ GLFW_KEY_UP, RETRO_DEVICE_ID_JOYPAD_UP },
//	{ GLFW_KEY_DOWN, RETRO_DEVICE_ID_JOYPAD_DOWN },
//	{ GLFW_KEY_LEFT, RETRO_DEVICE_ID_JOYPAD_LEFT },
//	{ GLFW_KEY_RIGHT, RETRO_DEVICE_ID_JOYPAD_RIGHT },
//	{ GLFW_KEY_ENTER, RETRO_DEVICE_ID_JOYPAD_START },
//	{ GLFW_KEY_BACKSPACE, RETRO_DEVICE_ID_JOYPAD_SELECT },
//
//	{ 0, 0 }
};

static unsigned g_joy[RETRO_DEVICE_ID_JOYPAD_R3+1] = { 0 };

// macro to initialize function pointers
#define load_sym(V, S) do {\
	if (!((*(void**)&V) = dlsym(g_retro.handle, #S))) \
		die("Failed to load symbol '" #S "'': %s", dlerror()); \
	} while (0)
#define load_retro_sym(S) load_sym(g_retro.S, S)

// **************** Auxiliary functions **************** //
// function to terminate program
static void die(const char *fmt, ...) {
	char buffer[4096];

	va_list va;
	va_start(va, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, va);
	va_end(va);

	fputs(buffer, stderr);
	fputc('\n', stderr);
	fflush(stderr);

	exit(EXIT_FAILURE);
}

// move window??
//static void refresh_vertex_data() {
//	assert(g_video.tex_w);
//	assert(g_video.tex_h);
//	assert(g_video.clip_w);
//	assert(g_video.clip_h);
//
//	GLfloat *coords = g_texcoords;
//	coords[1] = coords[5] = (float)g_video.clip_h / g_video.tex_h;
//	coords[4] = coords[6] = (float)g_video.clip_w / g_video.tex_w;
//}

// resize windows
//static void resize_cb(GLFWwindow *win, int w, int h) {
//	glViewport(0, 0, w, h);
//}


//static void create_window(int width, int height) {
//	// set window properties
//	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
//	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
//	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
//
//	// create window
//	g_win = glfwCreateWindow(width, height, "nanoarch", NULL, NULL);
//
//	if (!g_win)
//		die("Failed to create window.");
//
//	// set the function which is called when the window is resized
//	glfwSetFramebufferSizeCallback(g_win, resize_cb);
//
//	// make the thread of the window current ?
//	glfwMakeContextCurrent(g_win);
//
//	glewExperimental = GL_TRUE;
//	if (glewInit() != GLEW_OK)
//		die("Failed to initialize glew");
//
//	// sets how many screen updates before a contex switch will occur
//	glfwSwapInterval(1);
//
//	printf("GLSL Version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
//
//	glEnable(GL_TEXTURE_2D);
//
////	refresh_vertex_data();
//
//	resize_cb(g_win, width, height);
//}

// sets dw,dh (destination) according to the ration and sw,sh (source)
//static void resize_to_aspect(double ratio, int sw, int sh, int *dw, int *dh) {
//	*dw = sw;
//	*dh = sh;
//
//	if (ratio <= 0)
//		ratio = (double)sw / sh;
//
//	if ((float)sw / sh < 1)
//		*dw = *dh * ratio;
//	else
//		*dh = *dw / ratio;
//}

static size_t audio_write(const void *buf, unsigned frames) {
//	int written = snd_pcm_writei(g_pcm, buf, frames);
//
//	if (written < 0) {
//		printf("Alsa warning/error #%i: ", -written);
//		snd_pcm_recover(g_pcm, written, 0);
//
//		return 0;
//	}
//
//	return written;
}

static void audio_deinit() {
//	snd_pcm_close(g_pcm);
}


// **************** Auxiliary functions end **************** //

// **************** libretro front-end callbacks **************** //
// callbacks - functions used by the core to communicate with the frontend
//


// allows the core to communicate with the frontend.
// the cmd are macros defined in libretro.h
static bool core_environment(unsigned cmd, void *data) {
	bool *bval;

	switch (cmd) {
	case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
		struct retro_log_callback *cb = (struct retro_log_callback *)data;
		cb->log = core_log;
		break;
	}
	case RETRO_ENVIRONMENT_GET_CAN_DUPE:
		bval = (bool*)data;
		*bval = true;
		break;
	case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
		const enum retro_pixel_format *fmt = (enum retro_pixel_format *)data;

		if (*fmt > RETRO_PIXEL_FORMAT_RGB565)
			return false;

		return video_set_pixel_format(*fmt);
	}
	default:
		core_log(RETRO_LOG_DEBUG, "Unhandled env #%u", cmd);
		return false;
	}

	return true;
}


static void core_video_refresh(const void *data, unsigned width, unsigned height, size_t pitch) {
	if (data)
		video_refresh(data, width, height, pitch);
}

// receive data from the core
static void video_refresh(const void *data, unsigned width, unsigned height, unsigned pitch) {
	// ******* nadav implementation:
	if (g_video.rGeom.base_width != width || g_video.rGeom.base_height != height) {
		g_video.rGeom.base_height = height;
		g_video.rGeom.base_width  = width;
	}
	SDL_Surface * screen = SDL_CreateRGBSurfaceFrom(data, g_video.rGeom.base_width, g_video.rGeom.base_height,
								g_video.nbpp, pitch, g_video.rmask, g_video.gmask, g_video.bmask, g_video.amask);
	SDL_Surface* zoomed = zoomSurface(screen, g_video.rGeom.base_width/(double)width, g_video.rGeom.base_height/(double)height, 0);

    g_video.screen = SDL_SetVideoMode(g_video.rGeom.base_width, g_video.rGeom.base_height, g_video.nbpp, SDL_SWSURFACE);
	SDL_BlitSurface(zoomed, NULL, g_video.screen, NULL);
	SDL_UpdateRect(g_video.screen, 0, 0, g_video.rGeom.base_width, g_video.rGeom.base_height);

    SDL_Flip(screen);
    SDL_FreeSurface(screen);
    SDL_FreeSurface(zoomed);

	// ******* end
//	if (g_video.clip_w != width || g_video.clip_h != height) {
//		g_video.clip_h = height;
//		g_video.clip_w = width;
//
//		refresh_vertex_data();
//	}
//
//	glBindTexture(GL_TEXTURE_2D, g_video.tex_id);
//
//	// update pitch if necessary
//	if (pitch != g_video.pitch) {
//		g_video.pitch = pitch;
//		glPixelStorei(GL_UNPACK_ROW_LENGTH, g_video.pitch / g_video.bpp);
//	}
//
//	// display new image
//	if (data) {
//		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
//						g_video.pixtype, g_video.pixfmt, data);
//	}
}


static void core_audio_sample(int16_t left, int16_t right) {
//	int16_t buf[2] = {left, right};
//	audio_write(buf, 1);
}


static void video_configure(const struct retro_game_geometry *geom) {
	// ******* nadav implementation:
	g_video.rGeom.base_height  = geom->base_height;
	g_video.rGeom.base_width   = geom->base_width;
	g_video.rGeom.max_height   = geom->max_height;
	g_video.rGeom.max_width    = geom->max_width;
	g_video.rGeom.aspect_ratio = geom->aspect_ratio;

    /* Initialize the SDL library and the video subsystem */
    if( SDL_Init(SDL_INIT_VIDEO) < 0 ) {
        fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
        exit(1);
    }

    SDL_WM_SetCaption("ALE", NULL);

    /* Clean up on exit */
    atexit(SDL_Quit);

    // if pixformat wasn't set yet, set default bpp
    if(!g_video.nbpp){
    	g_video.nbpp = 16;
    }

    // TODO make sure we update the bpp
    g_video.screen = SDL_SetVideoMode(geom->max_width, geom->max_height, g_video.nbpp, SDL_SWSURFACE);
    if ( g_video.screen == NULL ) {
        fprintf(stderr, "Couldn't set %dx%dx%d video mode: %s\n", geom->max_width, geom->max_height, 8,  SDL_GetError());
        exit(1);
    }
	// ******* end

//	int nwidth, nheight;
//
//	resize_to_aspect(geom->aspect_ratio, geom->base_width * 1, geom->base_height * 1, &nwidth, &nheight);
//
//	nwidth *= g_scale;
//	nheight *= g_scale;
//
//	if (!g_win)
//		create_window(nwidth, nheight);
//
//	if (g_video.tex_id)
//		glDeleteTextures(1, &g_video.tex_id);
//
//	g_video.tex_id = 0;
//
//	// set default pixel format
//	if (!g_video.pixfmt)
//		g_video.pixfmt = GL_UNSIGNED_SHORT_5_5_5_1;
//
//	// resize window
//	glfwSetWindowSize(g_win, nwidth, nheight);
//
//	// set window format/texture
//	glGenTextures(1, &g_video.tex_id);
//
//	if (!g_video.tex_id)
//		die("Failed to create the video texture");
//
//	// save the video's pitch
//	g_video.pitch = geom->base_width * g_video.bpp;
//
//	glBindTexture(GL_TEXTURE_2D, g_video.tex_id);
//
////	glPixelStorei(GL_UNPACK_ALIGNMENT, s_video.pixfmt == GL_UNSIGNED_INT_8_8_8_8_REV ? 4 : 2);
////	glPixelStorei(GL_UNPACK_ROW_LENGTH, s_video.pitch / s_video.bpp);
//
//	// set texture parameters
//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//
//	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, geom->max_width, geom->max_height, 0,
//			g_video.pixtype, g_video.pixfmt, NULL);
//
//	glBindTexture(GL_TEXTURE_2D, 0);
//
//	// save the width, height
//	g_video.tex_w = geom->max_width;
//	g_video.tex_h = geom->max_height;
//	g_video.clip_w = geom->base_width;
//	g_video.clip_h = geom->base_height;
//
//	refresh_vertex_data();
}

// sets the pixel format as requested by the core
static bool video_set_pixel_format(unsigned format) {
//	if (g_video.tex_id)
//		die("Tried to change pixel format after initialization.");
//
//	switch (format) {
//	case RETRO_PIXEL_FORMAT_0RGB1555:
//		g_video.pixfmt = GL_UNSIGNED_SHORT_1_5_5_5_REV;
//		g_video.pixtype = GL_BGRA;
//		g_video.bpp = sizeof(uint16_t);
//		break;
//	case RETRO_PIXEL_FORMAT_XRGB8888:
//		g_video.pixfmt = GL_UNSIGNED_INT_8_8_8_8_REV;
//		g_video.pixtype = GL_BGRA;
//		g_video.bpp = sizeof(uint32_t);
//		break;
//	case RETRO_PIXEL_FORMAT_RGB565:
//		g_video.pixfmt  = GL_UNSIGNED_SHORT_5_6_5;
//		g_video.pixtype = GL_RGB;
//		g_video.bpp = sizeof(uint16_t);
//		break;
//	default:
//		die("Unknown pixel type %u", format);
//	}

	// nadav
	switch (format) {
	case RETRO_PIXEL_FORMAT_0RGB1555:
        if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
			g_video.rmask = 0x001f;
			g_video.gmask = 0x03e0;
			g_video.bmask = 0x7c00;
			g_video.amask = 0x0000;
        }else{
			g_video.rmask = 0x7c00;
			g_video.gmask = 0x03e0;
			g_video.bmask = 0x001f;
			g_video.amask = 0x0000;
		}
		g_video.nbpp = 8*sizeof(uint16_t);
		break;
	case RETRO_PIXEL_FORMAT_XRGB8888:
		if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
			g_video.rmask = 0x000000ff;
			g_video.gmask = 0x0000ff00;
			g_video.bmask = 0x00ff0000;
			g_video.amask = 0xff000000;
		}else{
			g_video.rmask = 0xff000000;
			g_video.gmask = 0x00ff0000;
			g_video.bmask = 0x0000ff00;
			g_video.amask = 0x000000ff;
		}
		g_video.nbpp = 8*sizeof(uint32_t);
		break;
	case RETRO_PIXEL_FORMAT_RGB565:
		if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
			g_video.rmask = 0x001f;
			g_video.gmask = 0x07e0;
			g_video.bmask = 0xf800;
			g_video.amask = 0x0000;
		}else{
			g_video.rmask = 0xf800;
			g_video.gmask = 0x07e0;
			g_video.bmask = 0x001f;
			g_video.amask = 0x0000;
		}
		g_video.nbpp = 8*sizeof(uint16_t);
		break;
	default:
		die("Unknown pixel type %u", format);
	}
	// nadav ebd

	return true;
}


static void video_deinit() {
//	if (g_video.tex_id)
//		glDeleteTextures(1, &g_video.tex_id);

//	g_video.tex_id = 0;

}


static void audio_init(int frequency) {
//	int err;
//
//	if ((err = snd_pcm_open(&g_pcm, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0)
//		die("Failed to open playback device: %s", snd_strerror(err));
//
//	err = snd_pcm_set_params(g_pcm, SND_PCM_FORMAT_S16, SND_PCM_ACCESS_RW_INTERLEAVED, 2, frequency, 1, 64 * 1000);
//
//	if (err < 0)
//		die("Failed to configure playback device: %s", snd_strerror(err));
}

static size_t core_audio_sample_batch(const int16_t *data, size_t frames) {
//	return audio_write(data, frames);
}

// reads for input from keyboard
static void core_input_poll(void) {
//	int i;
//	for (i = 0; g_binds[i].k || g_binds[i].rk; ++i)
//		g_joy[g_binds[i].rk] = (glfwGetKey(g_win, g_binds[i].k) == GLFW_PRESS);
}

// returns for every key whether it is pressed or not
static int16_t core_input_state(unsigned port, unsigned device, unsigned index, unsigned id) {
	if (port || index || device != RETRO_DEVICE_JOYPAD)
		return 0;

	return g_joy[id];
}

// **************** libretro front-end callbacks end **************** //

// used to print the logging info from the core
static void core_log(enum retro_log_level level, const char *fmt, ...) {
	char buffer[4096] = {0};
	static const char * levelstr[] = { "dbg", "inf", "wrn", "err" };
	va_list va;

	va_start(va, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, va);
	va_end(va);

	if (level == 0)
		return;

	fprintf(stderr, "[%s] %s", levelstr[level], buffer);
	fflush(stderr);

	if (level == RETRO_LOG_ERROR)
		exit(EXIT_FAILURE);
}


// load and initialize core
static void core_load(const char *sofile) {
	void (*set_environment)(retro_environment_t) = NULL;
	void (*set_video_refresh)(retro_video_refresh_t) = NULL;
	void (*set_input_poll)(retro_input_poll_t) = NULL;
	void (*set_input_state)(retro_input_state_t) = NULL;
	void (*set_audio_sample)(retro_audio_sample_t) = NULL;
	void (*set_audio_sample_batch)(retro_audio_sample_batch_t) = NULL;

	memset(&g_retro, 0, sizeof(g_retro));
	g_retro.handle = dlopen(sofile, RTLD_LAZY);

	if (!g_retro.handle)
		die("Failed to load core: %s", dlerror());

	dlerror();

	load_retro_sym(retro_init);
	load_retro_sym(retro_deinit);
	load_retro_sym(retro_api_version);
	load_retro_sym(retro_get_system_info);
	load_retro_sym(retro_get_system_av_info);
	load_retro_sym(retro_set_controller_port_device);
	load_retro_sym(retro_reset);
	load_retro_sym(retro_run);
	load_retro_sym(retro_load_game);
	load_retro_sym(retro_unload_game);

	load_sym(set_environment, retro_set_environment);
	load_sym(set_video_refresh, retro_set_video_refresh);
	load_sym(set_input_poll, retro_set_input_poll);
	load_sym(set_input_state, retro_set_input_state);
	load_sym(set_audio_sample, retro_set_audio_sample);
	load_sym(set_audio_sample_batch, retro_set_audio_sample_batch);

	set_environment(core_environment);
	set_video_refresh(core_video_refresh);
	set_input_poll(core_input_poll);
	set_input_state(core_input_state);
	set_audio_sample(core_audio_sample);
	set_audio_sample_batch(core_audio_sample_batch);

	g_retro.retro_init();
	g_retro.initialized = true;

	puts("Core loaded");
}

// loads the game
static void core_load_game(const char *filename) {
	struct retro_system_av_info av = {{0}};
	struct retro_system_info system = {0};
	struct retro_game_info info = { filename, 0 };
	FILE *file = fopen(filename, "rb");

	if (!file)
		goto libc_error;

	fseek(file, 0, SEEK_END);
	info.size = ftell(file);
	rewind(file);

	g_retro.retro_get_system_info(&system);

	if (!system.need_fullpath) {
		info.data = malloc(info.size);

		if (!info.data || !fread((void*)info.data, info.size, 1, file))
			goto libc_error;
	}

	if (!g_retro.retro_load_game(&info))
		die("The core failed to load the content.");

	g_retro.retro_get_system_av_info(&av);

	video_configure(&av.geometry);
	audio_init(av.timing.sample_rate);

	return;

libc_error:
	die("Failed to load content '%s': %s", filename, strerror(errno));
}


static void core_unload() {
	if (g_retro.initialized)
		g_retro.retro_deinit();

	if (g_retro.handle)
		dlclose(g_retro.handle);
}


//static void video_render() {
//	glBindTexture(GL_TEXTURE_2D, g_video.tex_id);
//
//	// unlock the array for writing
//	glEnableClientState(GL_VERTEX_ARRAY);
//	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
//
//	glVertexPointer(2, GL_FLOAT, 0, g_vertex);
//	glTexCoordPointer(2, GL_FLOAT, 0, g_texcoords);
//
//	// Specifies what kind of primitives to render
//	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
//}

int main(int argc, char *argv[]) {
	if (argc < 3)
		die("usage: %s <core> <game>", argv[0]);

//	if (!glfwInit())
//		die("Failed to initialize glfw");

	core_load(argv[1]);
	core_load_game(argv[2]);

	SDL_Event event;
//	while (!glfwWindowShouldClose(g_win)) {
	while (SDL_PollEvent(&event)) {
//		glfwPollEvents();

		if(event.type == SDL_KEYDOWN){
			if(event.key.keysym.sym == SDLK_ESCAPE)
				SDL_Quit();
		}

		g_retro.retro_run();

//		glClear(GL_COLOR_BUFFER_BIT);
//
//		video_render();
//
//		glfwSwapBuffers(g_win);
	}

	core_unload();
	audio_deinit();
	video_deinit();

//	glfwTerminate();
	return 0;
}
