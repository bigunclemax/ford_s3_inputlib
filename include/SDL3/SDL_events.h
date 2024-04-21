#ifndef SDL_events_h_
#define SDL_events_h_

#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_gamepad.h>

/* General keyboard/mouse state definitions */
#define SDL_RELEASED    0
#define SDL_PRESSED 1


#define SDL_BUTTON_LEFT		0
#define SDL_BUTTON_RIGHT	1
#define SDL_BUTTON_MIDDLE	2
#define SDL_BUTTON_X1		3
#define SDL_BUTTON_X2		4

typedef enum
{
	SDL_INIT_JOYSTICK     = 0x00000200,  /**< `SDL_INIT_JOYSTICK` implies `SDL_INIT_EVENTS` */
} SDL_InitFlags;

typedef enum SDL_EventType
{
	SDL_WINDOWEVENT,
	SDL_QUIT,

	SDL_EVENT_JOYSTICK_BUTTON_DOWN,
	SDL_EVENT_JOYSTICK_BUTTON_UP,
	SDL_EVENT_JOYSTICK_AXIS_MOTION,
	SDL_EVENT_JOYSTICK_HAT_MOTION,

	/* Keyboard events */
	SDL_EVENT_KEY_DOWN        = 0x300, /**< Key pressed */
	SDL_EVENT_KEY_UP,                  /**< Key released */
	SDL_EVENT_TEXT_EDITING,            /**< Keyboard text editing (composition) */
	SDL_EVENT_TEXT_INPUT,              /**< Keyboard text input */
	SDL_EVENT_KEYMAP_CHANGED,          /**< Keymap changed due to a system event such as an
	                                        input language or keyboard layout change. */
	SDL_EVENT_KEYBOARD_ADDED,          /**< A new keyboard has been inserted into the system */
	SDL_EVENT_KEYBOARD_REMOVED,        /**< A keyboard has been removed */

	/* Mouse events */
	SDL_EVENT_MOUSE_MOTION    = 0x400, /**< Mouse moved */
	SDL_EVENT_MOUSE_BUTTON_DOWN,       /**< Mouse button pressed */
	SDL_EVENT_MOUSE_BUTTON_UP,         /**< Mouse button released */
	SDL_EVENT_MOUSE_WHEEL,             /**< Mouse wheel motion */
	SDL_EVENT_MOUSE_ADDED,             /**< A new mouse has been inserted into the system */
	SDL_EVENT_MOUSE_REMOVED,           /**< A mouse has been removed */

	/* Gamepad events */
	SDL_EVENT_GAMEPAD_AXIS_MOTION  = 0x650, /**< Gamepad axis motion */
	SDL_EVENT_GAMEPAD_BUTTON_DOWN,          /**< Gamepad button pressed */
	SDL_EVENT_GAMEPAD_BUTTON_UP,            /**< Gamepad button released */
	SDL_EVENT_GAMEPAD_ADDED,               /**< A new gamepad has been inserted into the system */
	SDL_EVENT_GAMEPAD_REMOVED,             /**< An opened gamepad has been removed */
	SDL_EVENT_GAMEPAD_REMAPPED,            /**< The gamepad mapping was updated */
	SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN,        /**< Gamepad touchpad was touched */
	SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION,      /**< Gamepad touchpad finger was moved */
	SDL_EVENT_GAMEPAD_TOUCHPAD_UP,          /**< Gamepad touchpad finger was lifted */
	SDL_EVENT_GAMEPAD_SENSOR_UPDATE,        /**< Gamepad sensor was updated */
	SDL_EVENT_GAMEPAD_UPDATE_COMPLETE,      /**< Gamepad update is complete */
	SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED,  /**< Gamepad Steam handle has changed */

} SDL_EventType;

typedef struct {
	SDL_EventType type;

	struct
	{
		uint64_t timestamp;
	} common;

	union {
		/**< Keyboard event data */
		struct {
			struct {
				int sym;
			} keysym;
		} key;

		/**< Mouse button event data */
		struct {
			uint8_t button;
		} button;

		/**< Mouse motion event data */
		struct {
			int32_t xrel;
			int32_t yrel;
		} motion;

		/**< Mouse wheel event data */
		struct {
			int32_t y;
		} wheel;

		/**< Gamepad button event data */
		struct {
			uint8_t button;
		} cbutton;

		/**< Gamepad axis event data */
		struct {
			uint8_t axis;
			int16_t value;
		} gaxis;
		
		/**< Gamepad device event data */
		struct {
			SDL_JoystickID which;
		} gdevice;

		/**< Joystick button event data */
		struct {
			SDL_JoystickID which; /**< The joystick instance id */
			Uint8 button;       /**< The joystick button index */
			Uint8 state;        /**< ::SDL_PRESSED or ::SDL_RELEASED */
		} jbutton;

		/**< Joystick hat event data */
		struct {
			SDL_JoystickID which; /**< The joystick instance id */
			Uint8 hat;	/**< The joystick hat index */
			Uint8 value;	/**< The hat position value.
					 *   \sa ::SDL_HAT_LEFTUP ::SDL_HAT_UP ::SDL_HAT_RIGHTUP
					 *   \sa ::SDL_HAT_LEFT ::SDL_HAT_CENTERED ::SDL_HAT_RIGHT
					 *   \sa ::SDL_HAT_LEFTDOWN ::SDL_HAT_DOWN ::SDL_HAT_RIGHTDOWN
					 *
					 *   Note that zero means the POV is centered.
					 */
		} jhat;

		/**< Joystick axis event data */
		struct {
			SDL_JoystickID which; /**< The joystick instance id */
			Uint8 axis;         /**< The joystick axis index */
			Sint16 value;       /**< The axis value (range: -32768 to 32767) */
		} jaxis;
	};	
} SDL_Event;

extern DECLSPEC SDL_bool SDLCALL SDL_PollEvent(SDL_Event *event);

#endif /* SDL_events_h_ */