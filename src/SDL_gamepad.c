// #include "s3_input.h"
// #include "internal.h"
#include <SDL3/SDL_gamepad.h>
#include "SDL_joystick_c.h"
#include "SDL_sysjoystick.h"
#include "SDL_gamepad_db.h"

#include <stdlib.h>
#include <stdio.h>

#define _guarded 

#define CHECK_GAMEPAD_MAGIC(joystick, retval) {};

/* Many gamepads turn the center button into an instantaneous button press */
#define SDL_MINIMUM_GUIDE_BUTTON_DELAY_MS 250

#define SDL_GAMEPAD_CRC_FIELD		   "crc:"
#define SDL_GAMEPAD_CRC_FIELD_SIZE	  4 /* hard-coded for speed */
#define SDL_GAMEPAD_TYPE_FIELD		  "type:"
#define SDL_GAMEPAD_TYPE_FIELD_SIZE	 SDL_strlen(SDL_GAMEPAD_TYPE_FIELD)
#define SDL_GAMEPAD_FACE_FIELD		  "face:"
#define SDL_GAMEPAD_FACE_FIELD_SIZE	 5 /* hard-coded for speed */
#define SDL_GAMEPAD_PLATFORM_FIELD	  "platform:"
#define SDL_GAMEPAD_PLATFORM_FIELD_SIZE SDL_strlen(SDL_GAMEPAD_PLATFORM_FIELD)
#define SDL_GAMEPAD_HINT_FIELD		  "hint:"
#define SDL_GAMEPAD_HINT_FIELD_SIZE	 SDL_strlen(SDL_GAMEPAD_HINT_FIELD)
#define SDL_GAMEPAD_SDKGE_FIELD		 "sdk>=:"
#define SDL_GAMEPAD_SDKGE_FIELD_SIZE	SDL_strlen(SDL_GAMEPAD_SDKGE_FIELD)
#define SDL_GAMEPAD_SDKLE_FIELD		 "sdk<=:"
#define SDL_GAMEPAD_SDKLE_FIELD_SIZE	SDL_strlen(SDL_GAMEPAD_SDKLE_FIELD)

static SDL_bool SDL_gamepads_initialized;
static SDL_Gamepad *SDL_gamepads SDL_GUARDED_BY(SDL_joystick_lock) = NULL;

/* our hard coded list of mapping support */
typedef enum
{
	SDL_GAMEPAD_MAPPING_PRIORITY_DEFAULT,
	SDL_GAMEPAD_MAPPING_PRIORITY_API,
	SDL_GAMEPAD_MAPPING_PRIORITY_USER,
} SDL_GamepadMappingPriority;

typedef struct GamepadMapping_t
{
	SDL_JoystickGUID guid _guarded;
	char *name _guarded;
	char *mapping _guarded;
	SDL_GamepadMappingPriority priority _guarded;
	struct GamepadMapping_t *next _guarded;
} GamepadMapping_t;

/* The SDL gamepad structure */
struct SDL_Gamepad
{
	const void *magic _guarded;

	SDL_Joystick *joystick _guarded; /* underlying joystick device */
	int ref_count _guarded;

	const char *name _guarded;
	SDL_GamepadType type _guarded;
#if 0
	SDL_GamepadFaceStyle face_style _guarded;
#endif
	GamepadMapping_t *mapping _guarded;
	int num_bindings _guarded;
	SDL_GamepadBinding *bindings _guarded;
	SDL_GamepadBinding **last_match_axis _guarded;
	Uint8 *last_hat_mask _guarded;
	Uint64 guide_button_down _guarded;

	struct SDL_Gamepad *next _guarded; /* pointer to next gamepad we have allocated */
};

static const char *map_StringForGamepadType[] = {
	"unknown",
	"standard",
	"xbox360",
	"xboxone",
	"ps3",
	"ps4",
	"ps5",
	"switchpro",
	"joyconleft",
	"joyconright",
	"joyconpair"
};
SDL_COMPILE_TIME_ASSERT(map_StringForGamepadType, SDL_arraysize(map_StringForGamepadType) == SDL_GAMEPAD_TYPE_MAX);

static SDL_JoystickGUID s_zeroGUID;
static GamepadMapping_t *s_pSupportedGamepads SDL_GUARDED_BY(SDL_joystick_lock) = NULL;
static GamepadMapping_t *s_pDefaultMapping SDL_GUARDED_BY(SDL_joystick_lock) = NULL;
static GamepadMapping_t *s_pXInputMapping SDL_GUARDED_BY(SDL_joystick_lock) = NULL;
static char gamepad_magic;

static void PushMappingChangeTracking(void) {};
static void AddMappingChangeTracking(GamepadMapping_t *mapping) {};
static void PopMappingChangeTracking(void) {};

/*
 * convert a string to its enum equivalent
 */
SDL_GamepadType SDL_GetGamepadTypeFromString(const char *str)
{
	int i;

	if (!str || str[0] == '\0') {
		return SDL_GAMEPAD_TYPE_UNKNOWN;
	}

	if (*str == '+' || *str == '-') {
		++str;
	}

	for (i = 0; i < SDL_arraysize(map_StringForGamepadType); ++i) {
		if (SDL_strcasecmp(str, map_StringForGamepadType[i]) == 0) {
			return (SDL_GamepadType)i;
		}
	}
	return SDL_GAMEPAD_TYPE_UNKNOWN;
}

/*
 * convert an enum to its string equivalent
 */
const char *SDL_GetGamepadStringForType(SDL_GamepadType type)
{
	if (type >= SDL_GAMEPAD_TYPE_STANDARD && type < SDL_GAMEPAD_TYPE_MAX) {
		return map_StringForGamepadType[type];
	}
	return NULL;
}

static const char *map_StringForGamepadAxis[] = {
	"leftx",
	"lefty",
	"rightx",
	"righty",
	"lefttrigger",
	"righttrigger"
};
SDL_COMPILE_TIME_ASSERT(map_StringForGamepadAxis, SDL_arraysize(map_StringForGamepadAxis) == SDL_GAMEPAD_AXIS_MAX);

/*
 * convert a string to its enum equivalent
 */
SDL_GamepadAxis SDL_GetGamepadAxisFromString(const char *str)
{
	int i;

	if (!str || str[0] == '\0') {
		return SDL_GAMEPAD_AXIS_INVALID;
	}

	if (*str == '+' || *str == '-') {
		++str;
	}

	for (i = 0; i < SDL_arraysize(map_StringForGamepadAxis); ++i) {
		if (SDL_strcasecmp(str, map_StringForGamepadAxis[i]) == 0) {
			return (SDL_GamepadAxis)i;
		}
	}
	return SDL_GAMEPAD_AXIS_INVALID;
}

/*
 * convert an enum to its string equivalent
 */
const char *SDL_GetGamepadStringForAxis(SDL_GamepadAxis axis)
{
	if (axis > SDL_GAMEPAD_AXIS_INVALID && axis < SDL_GAMEPAD_AXIS_MAX) {
		return map_StringForGamepadAxis[axis];
	}
	return NULL;
}

static const char *map_StringForGamepadButton[] = {
	"a",
	"b",
	"x",
	"y",
	"back",
	"guide",
	"start",
	"leftstick",
	"rightstick",
	"leftshoulder",
	"rightshoulder",
	"dpup",
	"dpdown",
	"dpleft",
	"dpright",
	"misc1",
	"paddle1",
	"paddle2",
	"paddle3",
	"paddle4",
	"touchpad",
	"misc2",
	"misc3",
	"misc4",
	"misc5",
	"misc6"
};
SDL_COMPILE_TIME_ASSERT(map_StringForGamepadButton, SDL_arraysize(map_StringForGamepadButton) == SDL_GAMEPAD_BUTTON_MAX);

/*
 * convert a string to its enum equivalent
 */
static SDL_GamepadButton SDL_PrivateGetGamepadButtonFromString(const char *str, SDL_bool baxy)
{
	int i;

	if (!str || str[0] == '\0') {
		return SDL_GAMEPAD_BUTTON_INVALID;
	}

	for (i = 0; i < SDL_arraysize(map_StringForGamepadButton); ++i) {
		if (SDL_strcasecmp(str, map_StringForGamepadButton[i]) == 0) {
			if (baxy) {
				/* Need to swap face buttons */
				switch (i) {
				case SDL_GAMEPAD_BUTTON_SOUTH:
					return SDL_GAMEPAD_BUTTON_EAST;
				case SDL_GAMEPAD_BUTTON_EAST:
					return SDL_GAMEPAD_BUTTON_SOUTH;
				case SDL_GAMEPAD_BUTTON_WEST:
					return SDL_GAMEPAD_BUTTON_NORTH;
				case SDL_GAMEPAD_BUTTON_NORTH:
					return SDL_GAMEPAD_BUTTON_WEST;
				default:
					break;
				}
			}
			return (SDL_GamepadButton)i;
		}
	}
	return SDL_GAMEPAD_BUTTON_INVALID;
}
SDL_GamepadButton SDL_GetGamepadButtonFromString(const char *str)
{
	return SDL_PrivateGetGamepadButtonFromString(str, SDL_FALSE);
}

/*
 * convert an enum to its string equivalent
 */
const char *SDL_GetGamepadStringForButton(SDL_GamepadButton button)
{
	if (button > SDL_GAMEPAD_BUTTON_INVALID && button < SDL_GAMEPAD_BUTTON_MAX) {
		return map_StringForGamepadButton[button];
	}
	return NULL;
}

/*
 * given a gamepad button name and a joystick name update our mapping structure with it
 */
static SDL_bool SDL_PrivateParseGamepadElement(SDL_Gamepad *gamepad, const char *szGameButton, const char *szJoystickButton)
{
	SDL_GamepadBinding bind;
	SDL_GamepadButton button;
	SDL_GamepadAxis axis;
	SDL_bool invert_input = SDL_FALSE;
	char half_axis_input = 0;
	char half_axis_output = 0;
	int i;
	SDL_GamepadBinding *new_bindings;
	SDL_bool baxy_mapping = SDL_FALSE;

	SDL_AssertJoysticksLocked();

	SDL_zero(bind);

	if (*szGameButton == '+' || *szGameButton == '-') {
		half_axis_output = *szGameButton++;
	}
#if 0
	if (SDL_strstr(gamepad->mapping->mapping, ",hint:SDL_GAMECONTROLLER_USE_BUTTON_LABELS:=1") != NULL) {
		baxy_mapping = SDL_TRUE;
	}
#endif
	axis = SDL_GetGamepadAxisFromString(szGameButton);
	button = SDL_PrivateGetGamepadButtonFromString(szGameButton, baxy_mapping);
	if (axis != SDL_GAMEPAD_AXIS_INVALID) {
		bind.output_type = SDL_GAMEPAD_BINDTYPE_AXIS;
		bind.output.axis.axis = axis;
		if (axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
			bind.output.axis.axis_min = 0;
			bind.output.axis.axis_max = SDL_JOYSTICK_AXIS_MAX;
		} else {
			if (half_axis_output == '+') {
				bind.output.axis.axis_min = 0;
				bind.output.axis.axis_max = SDL_JOYSTICK_AXIS_MAX;
			} else if (half_axis_output == '-') {
				bind.output.axis.axis_min = 0;
				bind.output.axis.axis_max = SDL_JOYSTICK_AXIS_MIN;
			} else {
				bind.output.axis.axis_min = SDL_JOYSTICK_AXIS_MIN;
				bind.output.axis.axis_max = SDL_JOYSTICK_AXIS_MAX;
			}
		}
	} else if (button != SDL_GAMEPAD_BUTTON_INVALID) {
		bind.output_type = SDL_GAMEPAD_BINDTYPE_BUTTON;
		bind.output.button = button;
	} else {
		return SDL_FALSE;
	}

	if (*szJoystickButton == '+' || *szJoystickButton == '-') {
		half_axis_input = *szJoystickButton++;
	}
	if (szJoystickButton[SDL_strlen(szJoystickButton) - 1] == '~') {
		invert_input = SDL_TRUE;
	}

	if (szJoystickButton[0] == 'a' && SDL_isdigit((unsigned char)szJoystickButton[1])) {
		bind.input_type = SDL_GAMEPAD_BINDTYPE_AXIS;
		bind.input.axis.axis = SDL_atoi(&szJoystickButton[1]);
		if (half_axis_input == '+') {
			bind.input.axis.axis_min = 0;
			bind.input.axis.axis_max = SDL_JOYSTICK_AXIS_MAX;
		} else if (half_axis_input == '-') {
			bind.input.axis.axis_min = 0;
			bind.input.axis.axis_max = SDL_JOYSTICK_AXIS_MIN;
		} else {
			bind.input.axis.axis_min = SDL_JOYSTICK_AXIS_MIN;
			bind.input.axis.axis_max = SDL_JOYSTICK_AXIS_MAX;
		}
		if (invert_input) {
			int tmp = bind.input.axis.axis_min;
			bind.input.axis.axis_min = bind.input.axis.axis_max;
			bind.input.axis.axis_max = tmp;
		}
	} else if (szJoystickButton[0] == 'b' && SDL_isdigit((unsigned char)szJoystickButton[1])) {
		bind.input_type = SDL_GAMEPAD_BINDTYPE_BUTTON;
		bind.input.button = SDL_atoi(&szJoystickButton[1]);
	} else if (szJoystickButton[0] == 'h' && SDL_isdigit((unsigned char)szJoystickButton[1]) &&
			   szJoystickButton[2] == '.' && SDL_isdigit((unsigned char)szJoystickButton[3])) {
		int hat = SDL_atoi(&szJoystickButton[1]);
		int mask = SDL_atoi(&szJoystickButton[3]);
		bind.input_type = SDL_GAMEPAD_BINDTYPE_HAT;
		bind.input.hat.hat = hat;
		bind.input.hat.hat_mask = mask;
	} else {
		return SDL_FALSE;
	}

	for (i = 0; i < gamepad->num_bindings; ++i) {
		if (SDL_memcmp(&gamepad->bindings[i], &bind, sizeof(bind)) == 0) {
			/* We already have this binding, could be different face button names? */
			return SDL_TRUE;
		}
	}

	++gamepad->num_bindings;
	new_bindings = (SDL_GamepadBinding *)SDL_realloc(gamepad->bindings, gamepad->num_bindings * sizeof(*gamepad->bindings));
	if (!new_bindings) {
		SDL_free(gamepad->bindings);
		gamepad->num_bindings = 0;
		gamepad->bindings = NULL;
		return SDL_FALSE;
	}
	gamepad->bindings = new_bindings;
	gamepad->bindings[gamepad->num_bindings - 1] = bind;
	return SDL_TRUE;
}

/*
 * given a gamepad mapping string update our mapping object
 */
static int SDL_PrivateParseGamepadConfigString(SDL_Gamepad *gamepad, const char *pchString)
{
	char szGameButton[20];
	char szJoystickButton[20];
	SDL_bool bGameButton = SDL_TRUE;
	int i = 0;
	const char *pchPos = pchString;

	SDL_zeroa(szGameButton);
	SDL_zeroa(szJoystickButton);

	while (pchPos && *pchPos) {
		if (*pchPos == ':') {
			i = 0;
			bGameButton = SDL_FALSE;
		} else if (*pchPos == ' ') {

		} else if (*pchPos == ',') {
			i = 0;
			bGameButton = SDL_TRUE;
			SDL_PrivateParseGamepadElement(gamepad, szGameButton, szJoystickButton);
			SDL_zeroa(szGameButton);
			SDL_zeroa(szJoystickButton);

		} else if (bGameButton) {
			if (i >= sizeof(szGameButton)) {
				szGameButton[sizeof(szGameButton) - 1] = '\0';
				return SDL_SetError("Button name too large: %s", szGameButton);
			}
			szGameButton[i] = *pchPos;
			i++;
		} else {
			if (i >= sizeof(szJoystickButton)) {
				szJoystickButton[sizeof(szJoystickButton) - 1] = '\0';
				return SDL_SetError("Joystick button name too large: %s", szJoystickButton);
			}
			szJoystickButton[i] = *pchPos;
			i++;
		}
		pchPos++;
	}

	/* No more values if the string was terminated by a comma. Don't report an error. */
	if (szGameButton[0] != '\0' || szJoystickButton[0] != '\0') {
		SDL_PrivateParseGamepadElement(gamepad, szGameButton, szJoystickButton);
	}
	return 0;
}

/*
 * Helper function to scan the mappings database for a gamepad with the specified GUID
 */
static GamepadMapping_t *SDL_PrivateMatchGamepadMappingForGUID(SDL_JoystickGUID guid, SDL_bool match_crc, SDL_bool match_version)
{
	GamepadMapping_t *mapping;
	Uint16 crc = 0;

	SDL_AssertJoysticksLocked();

	if (match_crc) {
		SDL_GetJoystickGUIDInfo(guid, NULL, NULL, NULL, &crc);
	}

	/* Clear the CRC from the GUID for matching, the mappings never include it in the GUID */
	SDL_SetJoystickGUIDCRC(&guid, 0);

	if (!match_version) {
		SDL_SetJoystickGUIDVersion(&guid, 0);
	}

	{
		char buff[100];
		SDL_GUIDToString(guid, buff, sizeof(buff));
		LOG(LOG_SDL_GAMEPAD_TRACE, "Search mapping for GUID: %s\n", buff);
	}

	for (mapping = s_pSupportedGamepads; mapping; mapping = mapping->next) {
		SDL_JoystickGUID mapping_guid;

		if (SDL_memcmp(&mapping->guid, &s_zeroGUID, sizeof(mapping->guid)) == 0) {
			continue;
		}

		SDL_memcpy(&mapping_guid, &mapping->guid, sizeof(mapping_guid));
		if (!match_version) {
			SDL_SetJoystickGUIDVersion(&mapping_guid, 0);
		}

		if (SDL_memcmp(&guid, &mapping_guid, sizeof(guid)) == 0) {
			Uint16 mapping_crc = 0;

			if (match_crc) {
				const char *crc_string = SDL_strstr(mapping->mapping, SDL_GAMEPAD_CRC_FIELD);
				if (crc_string) {
					mapping_crc = (Uint16)SDL_strtol(crc_string + SDL_GAMEPAD_CRC_FIELD_SIZE, NULL, 16);
				}
			}
			if (crc == mapping_crc) {
				return mapping;
			}
		}
	}

	return NULL;
}

/*
 * Helper function to scan the mappings database for a gamepad with the specified GUID
 */
static GamepadMapping_t *SDL_PrivateGetGamepadMappingForGUID(SDL_JoystickGUID guid, SDL_bool adding_mapping)
{
	GamepadMapping_t *mapping;
	Uint16 vendor, product, crc;

	SDL_GetJoystickGUIDInfo(guid, &vendor, &product, NULL, &crc);
	if (crc) {
		/* First check for exact CRC matching */
		mapping = SDL_PrivateMatchGamepadMappingForGUID(guid, SDL_TRUE, SDL_TRUE);
		if (mapping) {
			return mapping;
		}
	}

	/* Now check for a mapping without CRC */
	mapping = SDL_PrivateMatchGamepadMappingForGUID(guid, SDL_FALSE, SDL_TRUE);
	if (mapping) {
		return mapping;
	}

#if 0
	if (adding_mapping) {
		/* We didn't find an existing mapping */
		return NULL;
	}

	/* Try harder to get the best match, or create a mapping */

	if (SDL_JoystickGUIDUsesVersion(guid)) {
		/* Try again, ignoring the version */
		if (crc) {
			mapping = SDL_PrivateMatchGamepadMappingForGUID(guid, SDL_TRUE, SDL_FALSE);
			if (mapping) {
				return mapping;
			}
		}

		mapping = SDL_PrivateMatchGamepadMappingForGUID(guid, SDL_FALSE, SDL_FALSE);
		if (mapping) {
			return mapping;
		}
	}

#ifdef SDL_JOYSTICK_XINPUT
	if (SDL_IsJoystickXInput(guid)) {
		/* This is an XInput device */
		return s_pXInputMapping;
	}
#endif
	if (SDL_IsJoystickHIDAPI(guid)) {
		mapping = SDL_CreateMappingForHIDAPIGamepad(guid);
	} else if (SDL_IsJoystickRAWINPUT(guid)) {
		mapping = SDL_CreateMappingForRAWINPUTGamepad(guid);
	} else if (SDL_IsJoystickWGI(guid)) {
		mapping = SDL_CreateMappingForWGIGamepad(guid);
	} else if (SDL_IsJoystickVIRTUAL(guid)) {
		/* We'll pick up a robust mapping in VIRTUAL_JoystickGetGamepadMapping */
#ifdef SDL_PLATFORM_ANDROID
	} else {
		mapping = SDL_CreateMappingForAndroidGamepad(guid);
#endif
	}
	return mapping;
#else
	return NULL;
#endif
}

/*
 * Helper function to determine pre-calculated offset to certain joystick mappings
 */
static GamepadMapping_t *SDL_PrivateGetGamepadMappingForNameAndGUID(const char *name, SDL_JoystickGUID guid)
{
	GamepadMapping_t *mapping;

	SDL_AssertJoysticksLocked();

	mapping = SDL_PrivateGetGamepadMappingForGUID(guid, SDL_FALSE);
#ifdef SDL_PLATFORM_LINUX
	if (!mapping && name) {
		if (SDL_strstr(name, "Xbox 360 Wireless Receiver")) {
			/* The Linux driver xpad.c maps the wireless dpad to buttons */
			SDL_bool existing;
			mapping = SDL_PrivateAddMappingForGUID(guid,
												   "none,X360 Wireless Controller,a:b0,b:b1,back:b6,dpdown:b14,dpleft:b11,dpright:b12,dpup:b13,guide:b8,leftshoulder:b4,leftstick:b9,lefttrigger:a2,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b10,righttrigger:a5,rightx:a3,righty:a4,start:b7,x:b2,y:b3,",
												   &existing, SDL_GAMEPAD_MAPPING_PRIORITY_DEFAULT);
		}
	}
#endif /* SDL_PLATFORM_LINUX */

	if (!mapping) {
		mapping = s_pDefaultMapping;
	}

	return mapping;
}

static GamepadMapping_t *SDL_PrivateGetGamepadMapping(SDL_JoystickID instance_id, SDL_bool create_mapping)
{
	const char *name;
	SDL_JoystickGUID guid;
	GamepadMapping_t *mapping;

	SDL_AssertJoysticksLocked();

	name = SDL_GetJoystickInstanceName(instance_id);
	guid = SDL_GetJoystickInstanceGUID(instance_id);
	mapping = SDL_PrivateGetGamepadMappingForNameAndGUID(name, guid);
#if 0
	if (!mapping && create_mapping) {
		SDL_GamepadMapping raw_map;

		SDL_zero(raw_map);
		if (SDL_PrivateJoystickGetAutoGamepadMapping(instance_id, &raw_map)) {
			mapping = SDL_PrivateGenerateAutomaticGamepadMapping(name, guid, &raw_map);
		}
	}
#endif
	return mapping;
}
#if 0
static void SDL_UpdateGamepadType(SDL_Gamepad *gamepad)
{
	char *type_string, *comma;

	SDL_AssertJoysticksLocked();

	gamepad->type = SDL_GAMEPAD_TYPE_UNKNOWN;

	type_string = SDL_strstr(gamepad->mapping->mapping, SDL_GAMEPAD_TYPE_FIELD);
	if (type_string) {
		type_string += SDL_GAMEPAD_TYPE_FIELD_SIZE;
		comma = SDL_strchr(type_string, ',');
		if (comma) {
			*comma = '\0';
			gamepad->type = SDL_GetGamepadTypeFromString(type_string);
			*comma = ',';
		} else {
			gamepad->type = SDL_GetGamepadTypeFromString(type_string);
		}
	}
	if (gamepad->type == SDL_GAMEPAD_TYPE_UNKNOWN) {
		gamepad->type = SDL_GetRealGamepadInstanceType(gamepad->joystick->instance_id);
	}
}

SDL_GamepadType SDL_GetRealGamepadInstanceType(SDL_JoystickID instance_id)
{
	SDL_GamepadType type = SDL_GAMEPAD_TYPE_UNKNOWN;
	const SDL_SteamVirtualGamepadInfo *info;

	SDL_LockJoysticks();
	{
		info = SDL_GetJoystickInstanceVirtualGamepadInfo(instance_id);
		if (info) {
			type = info->type;
		} else {
			type = SDL_GetGamepadTypeFromGUID(SDL_GetJoystickInstanceGUID(instance_id), SDL_GetJoystickInstanceName(instance_id));
		}
	}
	SDL_UnlockJoysticks();

	return type;
}
#endif
/*
 * Make a new button mapping struct
 */
static void SDL_PrivateLoadButtonMapping(SDL_Gamepad *gamepad, GamepadMapping_t *pGamepadMapping)
{
	int i;

	SDL_AssertJoysticksLocked();

	gamepad->name = pGamepadMapping->name;
	gamepad->num_bindings = 0;
	gamepad->mapping = pGamepadMapping;
	if (gamepad->joystick->naxes != 0 && gamepad->last_match_axis) {
		SDL_memset(gamepad->last_match_axis, 0, gamepad->joystick->naxes * sizeof(*gamepad->last_match_axis));
	}
#if 0
	SDL_UpdateGamepadType(gamepad);
	SDL_UpdateGamepadFaceStyle(gamepad);
#endif
	SDL_PrivateParseGamepadConfigString(gamepad, pGamepadMapping->mapping);

	/* Set the zero point for triggers */
	for (i = 0; i < gamepad->num_bindings; ++i) {
		SDL_GamepadBinding *binding = &gamepad->bindings[i];
		if (binding->input_type == SDL_GAMEPAD_BINDTYPE_AXIS &&
			binding->output_type == SDL_GAMEPAD_BINDTYPE_AXIS &&
			(binding->output.axis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER ||
			 binding->output.axis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)) {
			if (binding->input.axis.axis < gamepad->joystick->naxes) {
				gamepad->joystick->axes[binding->input.axis.axis].value =
					gamepad->joystick->axes[binding->input.axis.axis].zero = (Sint16)binding->input.axis.axis_min;
			}
		}
	}
}

/*
 * Open a gamepad for use
 *
 * This function returns a gamepad identifier, or NULL if an error occurred.
 */
SDL_Gamepad *SDL_OpenGamepad(SDL_JoystickID instance_id)
{
	SDL_Gamepad *gamepad;
	SDL_Gamepad *gamepadlist;
	GamepadMapping_t *pSupportedGamepad = NULL;

	LOG(LOG_SDL_GAMEPAD_TRACE, "%s [%d] + | id: %u\n",
		__func__, __LINE__, instance_id);

	SDL_LockJoysticks();

	gamepadlist = SDL_gamepads;
	/* If the gamepad is already open, return it */
	while (gamepadlist) {
		if (instance_id == gamepadlist->joystick->instance_id) {
			gamepad = gamepadlist;
			++gamepad->ref_count;
			SDL_UnlockJoysticks();
			return gamepad;
		}
		gamepadlist = gamepadlist->next;
	}

	/* Find a gamepad mapping */
	pSupportedGamepad = SDL_PrivateGetGamepadMapping(instance_id, SDL_TRUE);
	if (!pSupportedGamepad) {
		SDL_SetError("Couldn't find mapping for device (%" SDL_PRIu32 ")", instance_id);
		SDL_UnlockJoysticks();
		return NULL;
	}

	/* Create and initialize the gamepad */
	gamepad = (SDL_Gamepad *)SDL_calloc(1, sizeof(*gamepad));
	if (!gamepad) {
		SDL_UnlockJoysticks();
		return NULL;
	}

	gamepad->magic = &gamepad_magic;

	gamepad->joystick = SDL_OpenJoystick(instance_id);
	if (!gamepad->joystick) {
		SDL_free(gamepad);
		SDL_UnlockJoysticks();
		return NULL;
	}

	if (gamepad->joystick->naxes) {
		gamepad->last_match_axis = (SDL_GamepadBinding **)SDL_calloc(gamepad->joystick->naxes, sizeof(*gamepad->last_match_axis));
		if (!gamepad->last_match_axis) {
			SDL_CloseJoystick(gamepad->joystick);
			SDL_free(gamepad);
			SDL_UnlockJoysticks();
			return NULL;
		}
	}
	if (gamepad->joystick->nhats) {
		gamepad->last_hat_mask = (Uint8 *)SDL_calloc(gamepad->joystick->nhats, sizeof(*gamepad->last_hat_mask));
		if (!gamepad->last_hat_mask) {
			SDL_CloseJoystick(gamepad->joystick);
			SDL_free(gamepad->last_match_axis);
			SDL_free(gamepad);
			SDL_UnlockJoysticks();
			return NULL;
		}
	}

	SDL_PrivateLoadButtonMapping(gamepad, pSupportedGamepad);

	/* Add the gamepad to list */
	++gamepad->ref_count;
	/* Link the gamepad in the list */
	gamepad->next = SDL_gamepads;
	SDL_gamepads = gamepad;

	SDL_UnlockJoysticks();

	LOG(LOG_SDL_GAMEPAD_TRACE, "%s [%d] - | id: %u\n",
		__func__, __LINE__, instance_id);

	return gamepad;
}

/*
 * grab the guid string from a mapping string
 */
static char *SDL_PrivateGetGamepadGUIDFromMappingString(const char *pMapping)
{
	const char *pFirstComma = SDL_strchr(pMapping, ',');
	if (pFirstComma) {
		char *pchGUID = (char *)SDL_malloc(pFirstComma - pMapping + 1);
		if (!pchGUID) {
			return NULL;
		}
		SDL_memcpy(pchGUID, pMapping, pFirstComma - pMapping);
		pchGUID[pFirstComma - pMapping] = '\0';

		/* Convert old style GUIDs to the new style in 2.0.5 */
#if defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_WINGDK)
		if (SDL_strlen(pchGUID) == 32 &&
			SDL_memcmp(&pchGUID[20], "504944564944", 12) == 0) {
			SDL_memcpy(&pchGUID[20], "000000000000", 12);
			SDL_memcpy(&pchGUID[16], &pchGUID[4], 4);
			SDL_memcpy(&pchGUID[8], &pchGUID[0], 4);
			SDL_memcpy(&pchGUID[0], "03000000", 8);
		}
#elif defined(SDL_PLATFORM_MACOS)
		if (SDL_strlen(pchGUID) == 32 &&
			SDL_memcmp(&pchGUID[4], "000000000000", 12) == 0 &&
			SDL_memcmp(&pchGUID[20], "000000000000", 12) == 0) {
			SDL_memcpy(&pchGUID[20], "000000000000", 12);
			SDL_memcpy(&pchGUID[8], &pchGUID[0], 4);
			SDL_memcpy(&pchGUID[0], "03000000", 8);
		}
#endif
		return pchGUID;
	}
	return NULL;
}

/*
 * grab the name string from a mapping string
 */
static char *SDL_PrivateGetGamepadNameFromMappingString(const char *pMapping)
{
	const char *pFirstComma, *pSecondComma;
	char *pchName;

	pFirstComma = SDL_strchr(pMapping, ',');
	if (!pFirstComma) {
		return NULL;
	}

	pSecondComma = SDL_strchr(pFirstComma + 1, ',');
	if (!pSecondComma) {
		return NULL;
	}

	pchName = (char *)SDL_malloc(pSecondComma - pFirstComma);
	if (!pchName) {
		return NULL;
	}
	SDL_memcpy(pchName, pFirstComma + 1, pSecondComma - pFirstComma);
	pchName[pSecondComma - pFirstComma - 1] = 0;
	return pchName;
}

/*
 * grab the button mapping string from a mapping string
 */
static char *SDL_PrivateGetGamepadMappingFromMappingString(const char *pMapping)
{
	const char *pFirstComma, *pSecondComma;
	char *result;
	size_t length;

	pFirstComma = SDL_strchr(pMapping, ',');
	if (!pFirstComma) {
		return NULL;
	}

	pSecondComma = SDL_strchr(pFirstComma + 1, ',');
	if (!pSecondComma) {
		return NULL;
	}

	/* Skip whitespace */
	while (SDL_isspace(pSecondComma[1])) {
		++pSecondComma;
	}

	result = SDL_strdup(pSecondComma + 1); /* mapping is everything after the 3rd comma */

	/* Trim whitespace */
	length = SDL_strlen(result);
	while (length > 0 && SDL_isspace(result[length - 1])) {
		--length;
	}
	result[length] = '\0';

	return result;
}

/*
 * Helper function to add a mapping for a guid
 */
static GamepadMapping_t *SDL_PrivateAddMappingForGUID(SDL_JoystickGUID jGUID, const char *mappingString, SDL_bool *existing, SDL_GamepadMappingPriority priority)
{
	char *pchName;
	char *pchMapping;
	GamepadMapping_t *pGamepadMapping;
	Uint16 crc;

	SDL_AssertJoysticksLocked();

	pchName = SDL_PrivateGetGamepadNameFromMappingString(mappingString);
	if (!pchName) {
		SDL_SetError("Couldn't parse name from %s", mappingString);
		return NULL;
	}

	pchMapping = SDL_PrivateGetGamepadMappingFromMappingString(mappingString);
	if (!pchMapping) {
		SDL_free(pchName);
		SDL_SetError("Couldn't parse %s", mappingString);
		return NULL;
	}

	/* Fix up the GUID and the mapping with the CRC, if needed */
	SDL_GetJoystickGUIDInfo(jGUID, NULL, NULL, NULL, &crc);
	if (crc) {
		/* Make sure the mapping has the CRC */
		char *new_mapping;
		const char *optional_comma;
		size_t mapping_length;
		char *crc_end = "";
		char *crc_string = SDL_strstr(pchMapping, SDL_GAMEPAD_CRC_FIELD);
		if (crc_string) {
			crc_end = SDL_strchr(crc_string, ',');
			if (crc_end) {
				++crc_end;
			} else {
				crc_end = "";
			}
			*crc_string = '\0';
		}

		/* Make sure there's a comma before the CRC */
		mapping_length = SDL_strlen(pchMapping);
		if (mapping_length == 0 || pchMapping[mapping_length - 1] == ',') {
			optional_comma = "";
		} else {
			optional_comma = ",";
		}

		if (SDL_asprintf(&new_mapping, "%s%s%s%.4x,%s", pchMapping, optional_comma, SDL_GAMEPAD_CRC_FIELD, crc, crc_end) >= 0) {
			SDL_free(pchMapping);
			pchMapping = new_mapping;
		}
	} else {
		/* Make sure the GUID has the CRC, for matching purposes */
		char *crc_string = SDL_strstr(pchMapping, SDL_GAMEPAD_CRC_FIELD);
		if (crc_string) {
			crc = (Uint16)SDL_strtol(crc_string + SDL_GAMEPAD_CRC_FIELD_SIZE, NULL, 16);
			if (crc) {
				SDL_SetJoystickGUIDCRC(&jGUID, crc);
			}
		}
	}

	PushMappingChangeTracking();

	pGamepadMapping = SDL_PrivateGetGamepadMappingForGUID(jGUID, SDL_TRUE);
	if (pGamepadMapping) {
		/* Only overwrite the mapping if the priority is the same or higher. */
		if (pGamepadMapping->priority <= priority) {
			/* Update existing mapping */
			SDL_free(pGamepadMapping->name);
			pGamepadMapping->name = pchName;
			SDL_free(pGamepadMapping->mapping);
			pGamepadMapping->mapping = pchMapping;
			pGamepadMapping->priority = priority;
		} else {
			SDL_free(pchName);
			SDL_free(pchMapping);
		}
		if (existing) {
			*existing = SDL_TRUE;
		}

		AddMappingChangeTracking(pGamepadMapping);

	} else {
		pGamepadMapping = (GamepadMapping_t *)SDL_malloc(sizeof(*pGamepadMapping));
		if (!pGamepadMapping) {

			PopMappingChangeTracking();

			SDL_free(pchName);
			SDL_free(pchMapping);
			return NULL;
		}
		/* Clear the CRC, we've already added it to the mapping */
		if (crc) {
			SDL_SetJoystickGUIDCRC(&jGUID, 0);
		}
		pGamepadMapping->guid = jGUID;
		pGamepadMapping->name = pchName;
		pGamepadMapping->mapping = pchMapping;
		pGamepadMapping->next = NULL;
		pGamepadMapping->priority = priority;

		if (s_pSupportedGamepads) {
			/* Add the mapping to the end of the list */
			GamepadMapping_t *pCurrMapping, *pPrevMapping;

			for (pPrevMapping = s_pSupportedGamepads, pCurrMapping = pPrevMapping->next;
				 pCurrMapping;
				 pPrevMapping = pCurrMapping, pCurrMapping = pCurrMapping->next) {
				/* continue; */
			}
			pPrevMapping->next = pGamepadMapping;
		} else {
			s_pSupportedGamepads = pGamepadMapping;
		}
		if (existing) {
			*existing = SDL_FALSE;
		}
	}

	PopMappingChangeTracking();

	return pGamepadMapping;
}

/*
 * Add or update an entry into the Mappings Database with a priority
 */
static int SDL_PrivateAddGamepadMapping(const char *mappingString, SDL_GamepadMappingPriority priority)
{
	char *remapped = NULL;
	char *pchGUID;
	SDL_JoystickGUID jGUID;
	SDL_bool is_default_mapping = SDL_FALSE;
	SDL_bool is_xinput_mapping = SDL_FALSE;
	SDL_bool existing = SDL_FALSE;
	GamepadMapping_t *pGamepadMapping;
	int retval = -1;

	SDL_AssertJoysticksLocked();

	if (!mappingString) {
		return SDL_InvalidParamError("mappingString");
	}
#if 0
	{ /* Extract and verify the hint field */
		const char *tmp;

		tmp = SDL_strstr(mappingString, SDL_GAMEPAD_HINT_FIELD);
		if (tmp) {
			SDL_bool default_value, value, negate;
			int len;
			char hint[128];

			tmp += SDL_GAMEPAD_HINT_FIELD_SIZE;

			if (*tmp == '!') {
				negate = SDL_TRUE;
				++tmp;
			} else {
				negate = SDL_FALSE;
			}

			len = 0;
			while (*tmp && *tmp != ',' && *tmp != ':' && len < (sizeof(hint) - 1)) {
				hint[len++] = *tmp++;
			}
			hint[len] = '\0';

			if (tmp[0] == ':' && tmp[1] == '=') {
				tmp += 2;
				default_value = SDL_atoi(tmp);
			} else {
				default_value = SDL_FALSE;
			}

			if (SDL_strcmp(hint, "SDL_GAMECONTROLLER_USE_BUTTON_LABELS") == 0) {
				/* This hint is used to signal whether the mapping uses positional buttons or not */
				if (negate) {
					/* This mapping uses positional buttons, we can use it as-is */
				} else {
					/* This mapping uses labeled buttons, we need to swap them to positional */
					remapped = SDL_ConvertMappingToPositional(mappingString);
					if (!remapped) {
						goto done;
					}
					mappingString = remapped;
				}
			} else {
				value = SDL_GetHintBoolean(hint, default_value);
				if (negate) {
					value = !value;
				}
				if (!value) {
					retval = 0;
					goto done;
				}
			}
		}
	}
#endif
#ifdef ANDROID
	{ /* Extract and verify the SDK version */
		const char *tmp;

		tmp = SDL_strstr(mappingString, SDL_GAMEPAD_SDKGE_FIELD);
		if (tmp) {
			tmp += SDL_GAMEPAD_SDKGE_FIELD_SIZE;
			if (!(SDL_GetAndroidSDKVersion() >= SDL_atoi(tmp))) {
				SDL_SetError("SDK version %d < minimum version %d", SDL_GetAndroidSDKVersion(), SDL_atoi(tmp));
				goto done;
			}
		}
		tmp = SDL_strstr(mappingString, SDL_GAMEPAD_SDKLE_FIELD);
		if (tmp) {
			tmp += SDL_GAMEPAD_SDKLE_FIELD_SIZE;
			if (!(SDL_GetAndroidSDKVersion() <= SDL_atoi(tmp))) {
				SDL_SetError("SDK version %d > maximum version %d", SDL_GetAndroidSDKVersion(), SDL_atoi(tmp));
				goto done;
			}
		}
	}
#endif

	pchGUID = SDL_PrivateGetGamepadGUIDFromMappingString(mappingString);
	if (!pchGUID) {
		SDL_SetError("Couldn't parse GUID from %s", mappingString);
		goto done;
	}
	if (!SDL_strcasecmp(pchGUID, "default")) {
		is_default_mapping = SDL_TRUE;
	} else if (!SDL_strcasecmp(pchGUID, "xinput")) {
		is_xinput_mapping = SDL_TRUE;
	}
	jGUID = SDL_GetJoystickGUIDFromString(pchGUID);
	SDL_free(pchGUID);

	pGamepadMapping = SDL_PrivateAddMappingForGUID(jGUID, mappingString, &existing, priority);
	if (!pGamepadMapping) {
		goto done;
	}

	if (existing) {
		retval = 0;
	} else {
		if (is_default_mapping) {
			s_pDefaultMapping = pGamepadMapping;
		} else if (is_xinput_mapping) {
			s_pXInputMapping = pGamepadMapping;
		}
		retval = 1;
	}
done:
	if (remapped) {
		SDL_free(remapped);
	}
	return retval;
}

/*
 * Add or update an entry into the Mappings Database
 */
int SDL_AddGamepadMapping(const char *mapping)
{
	int retval;

	SDL_LockJoysticks();
	{
		retval = SDL_PrivateAddGamepadMapping(mapping, SDL_GAMEPAD_MAPPING_PRIORITY_API);
	}
	SDL_UnlockJoysticks();

	return retval;
}

/*
 * Event filter to transform joystick events into appropriate gamepad ones
 */
static int SDL_SendGamepadButton(Uint64 timestamp, SDL_Gamepad *gamepad, SDL_GamepadButton button, Uint8 state)
{
	LOG(LOG_SDL_GAMEPAD_TRACE, "%s [%d] GamepadButton: %d (0x%x) %s\n",
		__func__, __LINE__, button, button,
		state == SDL_RELEASED ? "RELEASED" : "PRESSED");

	if (!l_evt_q)
		return 0;

	SDL_Event *ev = malloc(sizeof(SDL_Event));
	ev->type = (state == SDL_RELEASED) ? SDL_CONTROLLERBUTTONUP : SDL_CONTROLLERBUTTONDOWN;
	ev->cbutton.button = button;

	enque(l_evt_q, ev);

	return 1;
}

/*
 * Event filter to transform joystick events into appropriate gamepad ones
 */
static int SDL_SendGamepadAxis(Uint64 timestamp, SDL_Gamepad *gamepad, SDL_GamepadAxis axis, Sint16 value)
{
#if 0
	int posted;

	SDL_AssertJoysticksLocked();

	/* translate the event, if desired */
	posted = 0;
	if (SDL_EventEnabled(SDL_EVENT_GAMEPAD_AXIS_MOTION)) {
		SDL_Event event;
		event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
		event.common.timestamp = timestamp;
		event.gaxis.which = gamepad->joystick->instance_id;
		event.gaxis.axis = axis;
		event.gaxis.value = value;
		posted = SDL_PushEvent(&event) == 1;
	}
	return posted;
#else
	if (!l_evt_q)
		return 0;

	SDL_Event *ev = malloc(sizeof(SDL_Event));
	ev->type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
//	event.gaxis.which = gamepad->joystick->instance_id;
	ev->gaxis.axis = axis;
	ev->gaxis.value = value;

	enque(l_evt_q, ev);

	return 1;
#endif
}

static SDL_bool HasSameOutput(SDL_GamepadBinding *a, SDL_GamepadBinding *b)
{
	if (a->output_type != b->output_type) {
		return SDL_FALSE;
	}

	if (a->output_type == SDL_GAMEPAD_BINDTYPE_AXIS) {
		return a->output.axis.axis == b->output.axis.axis;
	} else {
		return a->output.button == b->output.button;
	}
}

static void ResetOutput(Uint64 timestamp, SDL_Gamepad *gamepad, SDL_GamepadBinding *bind)
{
	if (bind->output_type == SDL_GAMEPAD_BINDTYPE_AXIS) {
		SDL_SendGamepadAxis(timestamp, gamepad, bind->output.axis.axis, 0);
	} else {
		SDL_SendGamepadButton(timestamp, gamepad, bind->output.button, SDL_RELEASED);
	}
}

static void HandleJoystickAxis(Uint64 timestamp, SDL_Gamepad *gamepad, int axis, int value)
{
	int i;
	SDL_GamepadBinding *last_match;
	SDL_GamepadBinding *match = NULL;

	SDL_AssertJoysticksLocked();

	last_match = gamepad->last_match_axis[axis];
	for (i = 0; i < gamepad->num_bindings; ++i) {
		SDL_GamepadBinding *binding = &gamepad->bindings[i];
		if (binding->input_type == SDL_GAMEPAD_BINDTYPE_AXIS &&
			axis == binding->input.axis.axis) {
			if (binding->input.axis.axis_min < binding->input.axis.axis_max) {
				if (value >= binding->input.axis.axis_min &&
					value <= binding->input.axis.axis_max) {
					match = binding;
					break;
				}
			} else {
				if (value >= binding->input.axis.axis_max &&
					value <= binding->input.axis.axis_min) {
					match = binding;
					break;
				}
			}
		}
	}

	if (last_match && (!match || !HasSameOutput(last_match, match))) {
		/* Clear the last input that this axis generated */
		ResetOutput(timestamp, gamepad, last_match);
	}

	if (match) {
		if (match->output_type == SDL_GAMEPAD_BINDTYPE_AXIS) {
			if (match->input.axis.axis_min != match->output.axis.axis_min || match->input.axis.axis_max != match->output.axis.axis_max) {
				float normalized_value = (float)(value - match->input.axis.axis_min) / (match->input.axis.axis_max - match->input.axis.axis_min);
				value = match->output.axis.axis_min + (int)(normalized_value * (match->output.axis.axis_max - match->output.axis.axis_min));
			}
			SDL_SendGamepadAxis(timestamp, gamepad, match->output.axis.axis, (Sint16)value);
		} else {
			Uint8 state;
			int threshold = match->input.axis.axis_min + (match->input.axis.axis_max - match->input.axis.axis_min) / 2;
			if (match->input.axis.axis_max < match->input.axis.axis_min) {
				state = (value <= threshold) ? SDL_PRESSED : SDL_RELEASED;
			} else {
				state = (value >= threshold) ? SDL_PRESSED : SDL_RELEASED;
			}
			SDL_SendGamepadButton(timestamp, gamepad, match->output.button, state);
		}
	}
	gamepad->last_match_axis[axis] = match;
}

static void HandleJoystickButton(Uint64 timestamp, SDL_Gamepad *gamepad, int button, Uint8 state)
{
	int i;

	SDL_AssertJoysticksLocked();

	for (i = 0; i < gamepad->num_bindings; ++i) {
		SDL_GamepadBinding *binding = &gamepad->bindings[i];
		if (binding->input_type == SDL_GAMEPAD_BINDTYPE_BUTTON &&
			button == binding->input.button) {
			if (binding->output_type == SDL_GAMEPAD_BINDTYPE_AXIS) {
				int value = state ? binding->output.axis.axis_max : binding->output.axis.axis_min;
				SDL_SendGamepadAxis(timestamp, gamepad, binding->output.axis.axis, (Sint16)value);
			} else {
				SDL_SendGamepadButton(timestamp, gamepad, binding->output.button, state);
			}
			break;
		}
	}
}

static void HandleJoystickHat(Uint64 timestamp, SDL_Gamepad *gamepad, int hat, Uint8 value)
{
	int i;
	Uint8 last_mask, changed_mask;

	SDL_AssertJoysticksLocked();

	last_mask = gamepad->last_hat_mask[hat];
	changed_mask = (last_mask ^ value);
	for (i = 0; i < gamepad->num_bindings; ++i) {
		SDL_GamepadBinding *binding = &gamepad->bindings[i];
		if (binding->input_type == SDL_GAMEPAD_BINDTYPE_HAT && hat == binding->input.hat.hat) {
			if ((changed_mask & binding->input.hat.hat_mask) != 0) {
				if (value & binding->input.hat.hat_mask) {
					if (binding->output_type == SDL_GAMEPAD_BINDTYPE_AXIS) {
						SDL_SendGamepadAxis(timestamp, gamepad, binding->output.axis.axis, (Sint16)binding->output.axis.axis_max);
					} else {
						SDL_SendGamepadButton(timestamp, gamepad, binding->output.button, SDL_PRESSED);
					}
				} else {
					ResetOutput(timestamp, gamepad, binding);
				}
			}
		}
	}
	gamepad->last_hat_mask[hat] = value;
}

/*
 * Event filter to fire gamepad events from joystick ones
 */
int SDL_GamepadEventWatcher(void *userdata, SDL_Event *event)
{
	SDL_Gamepad *gamepad;

	switch (event->type) {
	case SDL_EVENT_JOYSTICK_AXIS_MOTION:
	{
		SDL_AssertJoysticksLocked();

		for (gamepad = SDL_gamepads; gamepad; gamepad = gamepad->next) {
			if (gamepad->joystick->instance_id == event->jaxis.which) {
				HandleJoystickAxis(event->common.timestamp, gamepad, event->jaxis.axis, event->jaxis.value);
				break;
			}
		}
	} break;
	case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
	case SDL_EVENT_JOYSTICK_BUTTON_UP:
	{
		SDL_AssertJoysticksLocked();

		for (gamepad = SDL_gamepads; gamepad; gamepad = gamepad->next) {
			if (gamepad->joystick->instance_id == event->jbutton.which) {
				HandleJoystickButton(event->common.timestamp, gamepad, event->jbutton.button, event->jbutton.state);
				break;
			}
		}
	} break;
	case SDL_EVENT_JOYSTICK_HAT_MOTION:
	{
		SDL_AssertJoysticksLocked();

		for (gamepad = SDL_gamepads; gamepad; gamepad = gamepad->next) {
			if (gamepad->joystick->instance_id == event->jhat.which) {
				HandleJoystickHat(event->common.timestamp, gamepad, event->jhat.hat, event->jhat.value);
				break;
			}
		}
	} break;
#if 0
	case SDL_EVENT_JOYSTICK_UPDATE_COMPLETE:
	{
		SDL_AssertJoysticksLocked();

		for (gamepad = SDL_gamepads; gamepad; gamepad = gamepad->next) {
			if (gamepad->joystick->instance_id == event->jdevice.which) {
				SDL_Event deviceevent;

				deviceevent.type = SDL_EVENT_GAMEPAD_UPDATE_COMPLETE;
				deviceevent.common.timestamp = event->jdevice.timestamp;
				deviceevent.gdevice.which = event->jdevice.which;
				SDL_PushEvent(&deviceevent);
				break;
			}
		}
	} break;
#endif
	default:
		break;
	}

	return 1;
}

/*
 * Get the current state of an axis control on a gamepad
 */
Sint16 SDL_GetGamepadAxis(SDL_Gamepad *gamepad, SDL_GamepadAxis axis)
{
	Sint16 retval = 0;

	SDL_LockJoysticks();
	{
		int i;

		CHECK_GAMEPAD_MAGIC(gamepad, 0);

		for (i = 0; i < gamepad->num_bindings; ++i) {
			SDL_GamepadBinding *binding = &gamepad->bindings[i];
			if (binding->output_type == SDL_GAMEPAD_BINDTYPE_AXIS && binding->output.axis.axis == axis) {
				int value = 0;
				SDL_bool valid_input_range;
				SDL_bool valid_output_range;

				if (binding->input_type == SDL_GAMEPAD_BINDTYPE_AXIS) {
					value = SDL_GetJoystickAxis(gamepad->joystick, binding->input.axis.axis);
					if (binding->input.axis.axis_min < binding->input.axis.axis_max) {
						valid_input_range = (value >= binding->input.axis.axis_min && value <= binding->input.axis.axis_max);
					} else {
						valid_input_range = (value >= binding->input.axis.axis_max && value <= binding->input.axis.axis_min);
					}
					if (valid_input_range) {
						if (binding->input.axis.axis_min != binding->output.axis.axis_min || binding->input.axis.axis_max != binding->output.axis.axis_max) {
							float normalized_value = (float)(value - binding->input.axis.axis_min) / (binding->input.axis.axis_max - binding->input.axis.axis_min);
							value = binding->output.axis.axis_min + (int)(normalized_value * (binding->output.axis.axis_max - binding->output.axis.axis_min));
						}
					} else {
						value = 0;
					}
				} else if (binding->input_type == SDL_GAMEPAD_BINDTYPE_BUTTON) {
					value = SDL_GetJoystickButton(gamepad->joystick, binding->input.button);
					if (value == SDL_PRESSED) {
						value = binding->output.axis.axis_max;
					}
				} else if (binding->input_type == SDL_GAMEPAD_BINDTYPE_HAT) {
					int hat_mask = SDL_GetJoystickHat(gamepad->joystick, binding->input.hat.hat);
					if (hat_mask & binding->input.hat.hat_mask) {
						value = binding->output.axis.axis_max;
					}
				}

				if (binding->output.axis.axis_min < binding->output.axis.axis_max) {
					valid_output_range = (value >= binding->output.axis.axis_min && value <= binding->output.axis.axis_max);
				} else {
					valid_output_range = (value >= binding->output.axis.axis_max && value <= binding->output.axis.axis_min);
				}
				/* If the value is zero, there might be another binding that makes it non-zero */
				if (value != 0 && valid_output_range) {
					retval = (Sint16)value;
					break;
				}
			}
		}
	}
	SDL_UnlockJoysticks();

	return retval;
}

/*
 * Get the joystick for this gamepad
 */
SDL_Joystick *SDL_GetGamepadJoystick(SDL_Gamepad *gamepad)
{
	SDL_Joystick *joystick;

	SDL_LockJoysticks();
	{
		CHECK_GAMEPAD_MAGIC(gamepad, NULL);

		joystick = gamepad->joystick;
	}
	SDL_UnlockJoysticks();

	return joystick;
}

const char *SDL_GetGamepadName(SDL_Gamepad *gamepad)
{
	const char *retval = NULL;

	SDL_LockJoysticks();
	{
		CHECK_GAMEPAD_MAGIC(gamepad, NULL);

		if (SDL_strcmp(gamepad->name, "*") == 0 ||
			gamepad->joystick->steam_handle != 0) {
			retval = SDL_GetJoystickName(gamepad->joystick);
		} else {
			retval = gamepad->name;
		}
	}
	SDL_UnlockJoysticks();

	return retval;
}

const char *SDL_GetGamepadPath(SDL_Gamepad *gamepad)
{
	SDL_Joystick *joystick = SDL_GetGamepadJoystick(gamepad);

	if (!joystick) {
		return NULL;
	}
	return SDL_GetJoystickPath(joystick);
}

/* The joystick layer will _also_ send events to recenter before disconnect,
   but it has to make (sometimes incorrect) guesses at what being "centered"
   is. The gamepad layer, however, can set a definite logical idle
   position, so set them all here. If we happened to already be at the
   center thanks to the joystick layer or idle hands, this won't generate
   duplicate events. */
static void RecenterGamepad(SDL_Gamepad *gamepad)
{
#if 0
	int i;
	Uint64 timestamp = SDL_GetTicksNS();

	for (i = 0; i < SDL_GAMEPAD_BUTTON_MAX; ++i) {
		SDL_GamepadButton button = (SDL_GamepadButton)i;
		if (SDL_GetGamepadButton(gamepad, button)) {
			SDL_SendGamepadButton(timestamp, gamepad, button, SDL_RELEASED);
		}
	}

	for (i = 0; i < SDL_GAMEPAD_AXIS_MAX; ++i) {
		SDL_GamepadAxis axis = (SDL_GamepadAxis)i;
		if (SDL_GetGamepadAxis(gamepad, axis) != 0) {
			SDL_SendGamepadAxis(timestamp, gamepad, axis, 0);
		}
	}
#else
	LOG(LOG_SDL_GAMEPAD_TRACE, "%s [%d] NOT IMPLEMENTED\n", __func__, __LINE__);
#endif
}

void SDL_PrivateGamepadAdded(SDL_JoystickID instance_id)
{
	SDL_Event event;

	LOG(LOG_SDL_GAMEPAD_TRACE, "%s [%d] +\n", __func__, __LINE__);

	if (!SDL_gamepads_initialized) {
		return;
	}
#if 0
	event.type = SDL_EVENT_GAMEPAD_ADDED;
	event.common.timestamp = 0;
	event.gdevice.which = instance_id;
	SDL_PushEvent(&event);
#else
	if (!l_evt_q)
		return;

	SDL_Event *ev = malloc(sizeof(SDL_Event));
	ev->type = SDL_EVENT_GAMEPAD_ADDED;
	ev->common.timestamp = 0;
	ev->gdevice.which = instance_id;

	enque(l_evt_q, ev);
#endif
	LOG(LOG_SDL_GAMEPAD_TRACE, "%s [%d] -\n", __func__, __LINE__);
}

void SDL_PrivateGamepadRemoved(SDL_JoystickID instance_id)
{
	SDL_Event event;
	SDL_Gamepad *gamepad;

	LOG(LOG_SDL_GAMEPAD_TRACE, "%s [%d] +\n", __func__, __LINE__);

	SDL_AssertJoysticksLocked();

	if (!SDL_gamepads_initialized) {
		return;
	}

	for (gamepad = SDL_gamepads; gamepad; gamepad = gamepad->next) {
		if (gamepad->joystick->instance_id == instance_id) {
			RecenterGamepad(gamepad);
			break;
		}
	}
#if 0
	event.type = SDL_EVENT_GAMEPAD_REMOVED;
	event.common.timestamp = 0;
	event.gdevice.which = instance_id;
	SDL_PushEvent(&event);
#else
	if (!l_evt_q)
		return;

	SDL_Event *ev = malloc(sizeof(SDL_Event));
	ev->type = SDL_EVENT_GAMEPAD_REMOVED;
	ev->common.timestamp = 0;
	ev->gdevice.which = instance_id;

	enque(l_evt_q, ev);
#endif

	LOG(LOG_SDL_GAMEPAD_TRACE, "%s [%d] -\n", __func__, __LINE__);
}

/*
 * Initialize the gamepad system, mostly load our DB of gamepad config mappings
 */
int SDL_InitGamepadMappings(void)
{
	char szGamepadMapPath[1024];
	int i = 0;
	const char *pMappingString = NULL;

	LOG(LOG_SDL_GAMEPAD_TRACE, "%s [%d] +\n", __func__, __LINE__);

	SDL_AssertJoysticksLocked();

	PushMappingChangeTracking();

	pMappingString = s_GamepadMappings[i];
	while (pMappingString) {
		SDL_PrivateAddGamepadMapping(pMappingString, SDL_GAMEPAD_MAPPING_PRIORITY_DEFAULT);

		i++;
		pMappingString = s_GamepadMappings[i];
	}
#if 0
	if (SDL_GetGamepadMappingFilePath(szGamepadMapPath, sizeof(szGamepadMapPath))) {
		SDL_AddGamepadMappingsFromFile(szGamepadMapPath);
	}

	/* load in any user supplied config */
	SDL_LoadGamepadHints();

	SDL_LoadVIDPIDList(&SDL_allowed_gamepads);
	SDL_LoadVIDPIDList(&SDL_ignored_gamepads);
#endif
	PopMappingChangeTracking();

	LOG(LOG_SDL_GAMEPAD_TRACE, "%s [%d] -\n", __func__, __LINE__);

	return 0;
}

int SDL_InitGamepads(void)
{
	int i;
	SDL_JoystickID *joysticks;

	LOG(LOG_SDL_GAMEPAD_TRACE, "%s [%d] +\n", __func__, __LINE__);

	SDL_gamepads_initialized = SDL_TRUE;
#if 0
	/* Watch for joystick events and fire gamepad ones if needed */
	SDL_AddEventWatch(SDL_GamepadEventWatcher, NULL);
#endif
	/* Send added events for gamepads currently attached */
	joysticks = SDL_GetJoysticks(NULL);
	if (joysticks) {
		for (i = 0; joysticks[i]; ++i) {
			if (SDL_IsGamepad(joysticks[i])) {
				SDL_PrivateGamepadAdded(joysticks[i]);
			}
		}
		SDL_free(joysticks);
	}

	LOG(LOG_SDL_GAMEPAD_TRACE, "%s [%d] -\n", __func__, __LINE__);

	return 0;
}
