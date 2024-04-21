#include "internal.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gamepad.h>
#include "SDL_joystick_c.h"
#include "SDL_gamepad_c.h"

#include <ctype.h>

int verbosity; /* QNX hid driver log level */
queue_t *l_evt_q; /* SDL event queue */

extern int handleMouseEvent(input_module_t *module, int data_size, void * data);
extern int handleJoystickEvent(input_module_t *module, int data_size, void * data);
extern int handleJoystickInsert(input_module_t *module, int data_size, void * data);
extern int handleJoystickRemove(input_module_t *module, int data_size, void * data);
extern int handleKeyboardEvent(input_module_t *module, int data_size, void * data);

static input_module_t joystick_input;
static input_module_t keyboard_input;
static input_module_t mouse_input;

static void *g_joystick_client_h;
static void *g_keyboard_client_h;
static void *g_mouse_client_h;

static SDL_Gamepad *g_gamepad;
static int g_is_input_init;

void SDL_StartTextInput(void)
{
	fprintf(stdout, "%s - not implemented\n", __func__);
}

int SDL_isalpha(int x) { return isalpha(x); }
int SDL_isalnum(int x) { return isalnum(x); }
int SDL_isdigit(int x) { return isdigit(x); }
int SDL_isxdigit(int x) { return isxdigit(x); }
int SDL_ispunct(int x) { return ispunct(x); }
int SDL_isspace(int x) { return isspace(x); }
int SDL_isupper(int x) { return isupper(x); }
int SDL_islower(int x) { return islower(x); }
int SDL_isprint(int x) { return isprint(x); }
int SDL_isgraph(int x) { return isgraph(x); }
int SDL_iscntrl(int x) { return iscntrl(x); }
int SDL_toupper(int x) { return toupper(x); }
int SDL_tolower(int x) { return tolower(x); }

int SDL_Error(SDL_errorcode code)
{ 
	return SDL_UNSUPPORTED;
}

int SDL_SetError(SDL_PRINTF_FORMAT_STRING const char *fmt, ...)
{
	va_list arglist;
	va_start( arglist, fmt);
	vprintf(fmt, arglist);
	va_end( arglist );
	printf("\n");

	return -1;
}

const char *SDL_GetError() { return ""; };

int SDL_asprintf(char **strp, SDL_PRINTF_FORMAT_STRING const char *fmt, ...)
{
	va_list ap;
	int retval;

	va_start(ap, fmt);
	retval = SDL_vasprintf(strp, fmt, ap);
	va_end(ap);

	return retval;
}

int SDL_vasprintf(char **strp, SDL_PRINTF_FORMAT_STRING const char *fmt, va_list ap)
{
	int retval;
	int size = 100; /* Guess we need no more than 100 bytes */
	char *p, *np;
	va_list aq;

	*strp = NULL;

	p = (char *)SDL_malloc(size);
	if (!p) {
		return -1;
	}

	while (1) {
		/* Try to print in the allocated space */
		va_copy(aq, ap);
		retval = SDL_vsnprintf(p, size, fmt, aq);
		va_end(aq);

		/* Check error code */
		if (retval < 0) {
			SDL_free(p);
			return retval;
		}

		/* If that worked, return the string */
		if (retval < size) {
			*strp = p;
			return retval;
		}

		/* Else try again with more space */
		size = retval + 1; /* Precisely what is needed */

		np = (char *)SDL_realloc(p, size);
		if (!np) {
			SDL_free(p);
			return -1;
		} else {
			p = np;
		}
	}
}

int SDL_PollEvent(SDL_Event * event)
{
	SDL_Event *ev = deque(l_evt_q);

	if (!ev)
		return 0;

	*event = *ev;
	free(ev);

	return 1;
}

SDL_bool SDL_IsGamepad(SDL_JoystickID instance_id)
{
	return 1;
}

SDL_bool SDL_JoystickGetAttached(SDL_Joystick *joystick)
{
	return 0;
}

Uint32 SDL_WasInit(Uint32 flags)
{
	if (flags != SDL_INIT_JOYSTICK)
		return 0;

	return g_is_input_init;
}

int _init_sdl()
{
	l_evt_q = queue_factory();
	if (NULL == l_evt_q)
		LOG(LOG_ERROR, "Couldn't init event queue_t\n");

	LOG(LOG_INFO, "SDL initialized\n");
}

int _init_hid()
{
	joystick_input.type = DEVI_CLASS_JOYSTICK;
	joystick_input.input = handleJoystickEvent;
	joystick_input.insertion = handleJoystickInsert;
	joystick_input.removal = handleJoystickRemove;

	keyboard_input.type = DEVI_CLASS_KBD;
	keyboard_input.input = handleKeyboardEvent;

	mouse_input.type = DEVI_CLASS_REL;
	mouse_input.input = handleMouseEvent;

	devi_hid_init();
	devi_hid_server_connect("/dev/io-hid/my-hid");

	/* joystick */
	g_joystick_client_h = devi_hid_register_client(&joystick_input,
						       HIDD_CONNECT_WILDCARD);
	if (g_joystick_client_h == NULL) {
		LOG(LOG_ERROR, "%s %d devi_hid_register_client error\n", __func__, __LINE__);
	}

	/* mouse */
	g_mouse_client_h = devi_hid_register_client(&mouse_input,
						    HIDD_CONNECT_WILDCARD);
	if (g_mouse_client_h == NULL) {
		LOG(LOG_ERROR, "%s %d devi_hid_register_client error\n", __func__, __LINE__);
	}

	/* keyboard */
	g_keyboard_client_h = devi_hid_register_client(&keyboard_input,
						       HIDD_CONNECT_WILDCARD);
	if (g_keyboard_client_h == NULL) {
		LOG(LOG_ERROR, "%s %d devi_hid_register_client error\n", __func__, __LINE__);
	}

	LOG(LOG_INFO, "QNX HID driver initialized\n");
}

void _init_log()
{
	int logmask;
	const char *env_log = getenv("LOGMASK");
	if (env_log == NULL)
		return;

	logmask = (int)strtol(env_log, NULL, 16);
	Log_setmask(logmask);

	if (logmask & LOG_HID_INFO)
		verbosity = 3;
	if (logmask & LOG_HID_DEBUG1)
		verbosity = 4;
	if (logmask & LOG_HID_DEBUG2)
		verbosity = 5;
	if (logmask & LOG_HID_DEBUG3)
		verbosity = 8;

	fprintf(stdout, "logmask was set to: 0x%x, verbosity to: %d\n",
		logmask, verbosity);
}

int SDL_Init(Uint32 flags)
{
	if (flags != SDL_INIT_JOYSTICK)
		return -1;

	if (SDL_WasInit(SDL_INIT_JOYSTICK))
		return 0;

	_init_log();
	_init_sdl();
	_init_hid();

	/* Initialize the joystick subsystem */
	if (SDL_InitJoysticks() < 0) {
		return -1;
	}

	if (SDL_InitGamepads() < 0) {
		return -1;
	}

	g_is_input_init = 1;
}

void SDL_QuitSubSystem(Uint32 flags)
{
	if (flags != SDL_INIT_JOYSTICK)
		return;

	devi_unregister_hid_client(g_joystick_client_h);
	devi_unregister_hid_client(g_keyboard_client_h);
	devi_unregister_hid_client(g_mouse_client_h);
	devi_hid_server_disconnect();

	if (NULL != l_evt_q)
		queue_destroy(l_evt_q);

	g_is_input_init = 0;

	LOG(LOG_INFO, "SDL subsystem stopped\n");
}
