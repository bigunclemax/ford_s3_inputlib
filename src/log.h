#ifndef __LOG_H__
#define __LOG_H__

enum LoggerChannel
{
	/* Common */
	LOG_TRACE       = 0x0001,
	LOG_INFO        = 0x0002,
	LOG_WARNING     = 0x0004,
	LOG_ERROR       = 0x0008,
	LOG_CRITICAL    = 0x0010,

	/* SDL specific */
	LOG_SDL_KEYBOARD_TRACE    = 0x0020,
	LOG_SDL_MOUSE_TRACE       = 0x0040,
	LOG_SDL_GAMEPAD_TRACE     = 0x0080,
	LOG_SDL_JOYSTICK_TRACE    = 0x0100,
	LOG_SDL_SYSJOYSTICK_TRACE = 0x0200,

	/* QNX HID driver */
	LOG_HID_INFO   = 0x08000000,
	LOG_HID_DEBUG1 = 0x10000000,
	LOG_HID_DEBUG2 = 0x20000000,
	LOG_HID_DEBUG3 = 0x40000000,
};

extern int g_log_settings;
extern FILE *g_log_stream;

#define LOG(level, fmt, _args...) \
	do { \
		if (!(g_log_settings & level)) \
			break; \
		fprintf(g_log_stream, fmt, ##_args); \
	} while( 0 )

void Log_setmask(int mask);
int Log_getmask(void);

#endif /* __LOG_H__ */