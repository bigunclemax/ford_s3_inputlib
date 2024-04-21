#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>

#include "internal.h"

#include "SDL_sysjoystick.h"
#include "SDL_gamepad_c.h"

extern int SDL_GamepadEventWatcher(void *userdata, SDL_Event *event);

#define _guarded 

#define CHECK_JOYSTICK_MAGIC(joystick, retval) {};

extern SDL_JoystickDriver SDL_QNX_JoystickDriver;

static SDL_JoystickDriver *SDL_joystick_drivers[] = {
	&SDL_QNX_JoystickDriver,
};

static SDL_bool SDL_joysticks_initialized;

static SDL_Joystick *SDL_joysticks SDL_GUARDED_BY(SDL_joystick_lock) = NULL;
char SDL_joystick_magic;

void SDL_AssertJoysticksLocked(void) {};
void SDL_LockJoysticks(void) {};
void SDL_UnlockJoysticks(void) {};

/* convert the string version of a joystick guid to the struct */
SDL_JoystickGUID SDL_GetJoystickGUIDFromString(const char *pchGUID)
{
	return SDL_GUIDFromString(pchGUID);
}

void SDL_SetJoystickGUIDVendor(SDL_JoystickGUID *guid, Uint16 vendor)
{
	Uint16 *guid16 = (Uint16 *)guid->data;

	guid16[2] = SDL_SwapLE16(vendor);
}

void SDL_SetJoystickGUIDProduct(SDL_JoystickGUID *guid, Uint16 product)
{
	Uint16 *guid16 = (Uint16 *)guid->data;

	guid16[4] = SDL_SwapLE16(product);
}

void SDL_SetJoystickGUIDVersion(SDL_JoystickGUID *guid, Uint16 version)
{
	Uint16 *guid16 = (Uint16 *)guid->data;

	guid16[6] = SDL_SwapLE16(version);
}

void SDL_SetJoystickGUIDCRC(SDL_JoystickGUID *guid, Uint16 crc)
{
	Uint16 *guid16 = (Uint16 *)guid->data;

	guid16[1] = SDL_SwapLE16(crc);
}

void SDL_GetJoystickGUIDInfo(SDL_JoystickGUID guid, Uint16 *vendor, Uint16 *product, Uint16 *version, Uint16 *crc16)
{
	Uint16 *guid16 = (Uint16 *)guid.data;
	Uint16 bus = SDL_SwapLE16(guid16[0]);

	if ((bus < ' ' || bus == SDL_HARDWARE_BUS_VIRTUAL) && guid16[3] == 0x0000 && guid16[5] == 0x0000) {
		/* This GUID fits the standard form:
		 * 16-bit bus
		 * 16-bit CRC16 of the joystick name (can be zero)
		 * 16-bit vendor ID
		 * 16-bit zero
		 * 16-bit product ID
		 * 16-bit zero
		 * 16-bit version
		 * 8-bit driver identifier ('h' for HIDAPI, 'x' for XInput, etc.)
		 * 8-bit driver-dependent type info
		 */
		if (vendor) {
			*vendor = SDL_SwapLE16(guid16[2]);
		}
		if (product) {
			*product = SDL_SwapLE16(guid16[4]);
		}
		if (version) {
			*version = SDL_SwapLE16(guid16[6]);
		}
		if (crc16) {
			*crc16 = SDL_SwapLE16(guid16[1]);
		}
	} else if (bus < ' ' || bus == SDL_HARDWARE_BUS_VIRTUAL) {
		/* This GUID fits the unknown VID/PID form:
		 * 16-bit bus
		 * 16-bit CRC16 of the joystick name (can be zero)
		 * 11 characters of the joystick name, null terminated
		 */
		if (vendor) {
			*vendor = 0;
		}
		if (product) {
			*product = 0;
		}
		if (version) {
			*version = 0;
		}
		if (crc16) {
			*crc16 = SDL_SwapLE16(guid16[1]);
		}
	} else {
		if (vendor) {
			*vendor = 0;
		}
		if (product) {
			*product = 0;
		}
		if (version) {
			*version = 0;
		}
		if (crc16) {
			*crc16 = 0;
		}
	}
}

SDL_JoystickGUID SDL_CreateJoystickGUID(Uint16 bus, Uint16 vendor, Uint16 product, Uint16 version, const char *vendor_name, const char *product_name, Uint8 driver_signature, Uint8 driver_data)
{
	SDL_JoystickGUID guid;
	Uint16 *guid16 = (Uint16 *)guid.data;
	Uint16 crc = 0;

	SDL_zero(guid);
#if 0
	if (vendor_name && *vendor_name && product_name && *product_name) {
		crc = SDL_crc16(crc, vendor_name, SDL_strlen(vendor_name));
		crc = SDL_crc16(crc, " ", 1);
		crc = SDL_crc16(crc, product_name, SDL_strlen(product_name));
	} else if (product_name) {
		crc = SDL_crc16(crc, product_name, SDL_strlen(product_name));
	}
#endif
	/* We only need 16 bits for each of these; space them out to fill 128. */
	/* Byteswap so devices get same GUID on little/big endian platforms. */
	*guid16++ = SDL_SwapLE16(bus);
	*guid16++ = SDL_SwapLE16(crc);

	if (vendor) {
		*guid16++ = SDL_SwapLE16(vendor);
		*guid16++ = 0;
		*guid16++ = SDL_SwapLE16(product);
		*guid16++ = 0;
		*guid16++ = SDL_SwapLE16(version);
		guid.data[14] = driver_signature;
		guid.data[15] = driver_data;
	} else {
		size_t available_space = sizeof(guid.data) - 4;

		if (driver_signature) {
			available_space -= 2;
			guid.data[14] = driver_signature;
			guid.data[15] = driver_data;
		}
		if (product_name) {
			SDL_strlcpy((char *)guid16, product_name, available_space);
		}
	}
	return guid;
}

/*
 * Get the driver and device index for a joystick instance ID
 * This should be called while the joystick lock is held, to prevent another thread from updating the list
 */
static SDL_bool SDL_GetDriverAndJoystickIndex(SDL_JoystickID instance_id, SDL_JoystickDriver **driver, int *driver_index)
{
	int i, num_joysticks, device_index;

	SDL_AssertJoysticksLocked();

	if (instance_id > 0) {
		for (i = 0; i < SDL_arraysize(SDL_joystick_drivers); ++i) {
			num_joysticks = SDL_joystick_drivers[i]->GetCount();
			for (device_index = 0; device_index < num_joysticks; ++device_index) {
				SDL_JoystickID joystick_id = SDL_joystick_drivers[i]->GetDeviceInstanceID(device_index);
				if (joystick_id == instance_id) {
					*driver = SDL_joystick_drivers[i];
					*driver_index = device_index;
					return SDL_TRUE;
				}
			}
		}
	}

	SDL_SetError("Joystick %" SDL_PRIu32 " not found", instance_id);
	return SDL_FALSE;
}

/* return the guid for this index */
SDL_JoystickGUID SDL_GetJoystickInstanceGUID(SDL_JoystickID instance_id)
{
	SDL_JoystickDriver *driver;
	int device_index;
	SDL_JoystickGUID guid;

	SDL_LockJoysticks();
	if (SDL_GetDriverAndJoystickIndex(instance_id, &driver, &device_index)) {
		guid = driver->GetDeviceGUID(device_index);
	} else {
		SDL_zero(guid);
	}
	SDL_UnlockJoysticks();

	return guid;
}

/*
 * Get the implementation dependent name of a joystick
 */
const char *SDL_GetJoystickInstanceName(SDL_JoystickID instance_id)
{
	SDL_JoystickDriver *driver;
	int device_index;
	const char *name = NULL;
#if 0
	const SDL_SteamVirtualGamepadInfo *info;
#endif
	SDL_LockJoysticks();
#if 0
	info = SDL_GetJoystickInstanceVirtualGamepadInfo(instance_id);
	if (info) {
		name = info->name;
	} else
#endif
	if (SDL_GetDriverAndJoystickIndex(instance_id, &driver, &device_index)) {
		name = driver->GetDeviceName(device_index);
	}
	SDL_UnlockJoysticks();

	/* FIXME: Really we should reference count this name so it doesn't go away after unlock */
	return name;
}

/*
 * Open a joystick for use - the index passed as an argument refers to
 * the N'th joystick on the system.  This index is the value which will
 * identify this joystick in future joystick events.
 *
 * This function returns a joystick identifier, or NULL if an error occurred.
 */
SDL_Joystick *SDL_OpenJoystick(SDL_JoystickID instance_id) 
{
	SDL_JoystickDriver *driver;
	int device_index;
	SDL_Joystick *joystick;
	SDL_Joystick *joysticklist;
	const char *joystickname = NULL;
	const char *joystickpath = NULL;
	SDL_JoystickPowerLevel initial_power_level;
	SDL_bool invert_sensors = SDL_FALSE;
#if 0
	const SDL_SteamVirtualGamepadInfo *info;
#endif
	LOG(LOG_SDL_JOYSTICK_TRACE, "%s [%d] + | id: %u\n",
		__func__, __LINE__, instance_id);

	SDL_LockJoysticks();

	if (!SDL_GetDriverAndJoystickIndex(instance_id, &driver, &device_index)) {
		SDL_UnlockJoysticks();
		return NULL;
	}

	joysticklist = SDL_joysticks;
	/* If the joystick is already open, return it
	 * it is important that we have a single joystick for each instance id
	 */
	while (joysticklist) {
		if (instance_id == joysticklist->instance_id) {
			joystick = joysticklist;
			++joystick->ref_count;
			SDL_UnlockJoysticks();
			return joystick;
		}
		joysticklist = joysticklist->next;
	}

	/* Create and initialize the joystick */
	joystick = (SDL_Joystick *)SDL_calloc(sizeof(*joystick), 1);
	if (!joystick) {
		SDL_UnlockJoysticks();
		return NULL;
	}
	joystick->magic = &SDL_joystick_magic;
	joystick->driver = driver;
	joystick->instance_id = instance_id;
	joystick->attached = SDL_TRUE;
	joystick->epowerlevel = SDL_JOYSTICK_POWER_UNKNOWN;
#if 0
	joystick->led_expiration = SDL_GetTicks();
#endif

	if (driver->Open(joystick, device_index) < 0) {
		SDL_free(joystick);
		SDL_UnlockJoysticks();
		return NULL;
	}

	joystickname = driver->GetDeviceName(device_index);
	if (joystickname) {
		joystick->name = SDL_strdup(joystickname);
	}

	joystickpath = driver->GetDevicePath(device_index);
	if (joystickpath) {
		joystick->path = SDL_strdup(joystickpath);
	}

	joystick->guid = driver->GetDeviceGUID(device_index);

	if (joystick->naxes > 0) {
		joystick->axes = (SDL_JoystickAxisInfo *)SDL_calloc(joystick->naxes, sizeof(SDL_JoystickAxisInfo));
	}
	if (joystick->nhats > 0) {
		joystick->hats = (Uint8 *)SDL_calloc(joystick->nhats, sizeof(Uint8));
	}
	if (joystick->nbuttons > 0) {
		joystick->buttons = (Uint8 *)SDL_calloc(joystick->nbuttons, sizeof(Uint8));
	}
	if (((joystick->naxes > 0) && !joystick->axes) || ((joystick->nhats > 0) && !joystick->hats) || ((joystick->nbuttons > 0) && !joystick->buttons)) {
		SDL_CloseJoystick(joystick);
		SDL_UnlockJoysticks();
		return NULL;
	}
#if 0
	/* If this joystick is known to have all zero centered axes, skip the auto-centering code */
	if (SDL_JoystickAxesCenteredAtZero(joystick)) {
		int i;

		for (i = 0; i < joystick->naxes; ++i) {
			joystick->axes[i].has_initial_value = SDL_TRUE;
		}
	}
#endif
	joystick->is_gamepad = SDL_IsGamepad(instance_id);
#if 0
	/* Get the Steam Input API handle */
	info = SDL_GetJoystickInstanceVirtualGamepadInfo(instance_id);
	if (info) {
		joystick->steam_handle = info->handle;
	}

	/* Use system gyro and accelerometer if the gamepad doesn't have built-in sensors */
	if (ShouldAttemptSensorFusion(joystick, &invert_sensors)) {
		AttemptSensorFusion(joystick, invert_sensors);
	}
#endif
	/* Add joystick to list */
	++joystick->ref_count;
	/* Link the joystick in the list */
	joystick->next = SDL_joysticks;
	SDL_joysticks = joystick;
#if 0
	/* send initial battery event */
	initial_power_level = joystick->epowerlevel;
	joystick->epowerlevel = SDL_JOYSTICK_POWER_UNKNOWN;
	SDL_SendJoystickBatteryLevel(joystick, initial_power_level);

	driver->Update(joystick);
#endif
	SDL_UnlockJoysticks();

	LOG(LOG_SDL_JOYSTICK_TRACE, "%s [%d] + | id: %u\n",
		__func__, __LINE__, instance_id);

	return joystick;
}

/*
 * Close a joystick previously opened with SDL_OpenJoystick()
 */
void SDL_CloseJoystick(SDL_Joystick *joystick)
{
	LOG(LOG_SDL_JOYSTICK_TRACE, "%s [%d]\n", __func__, __LINE__);

	return;
}

/*
 * Get the current state of an axis control on a joystick
 */
Sint16 SDL_GetJoystickAxis(SDL_Joystick *joystick, int axis)
{
	Sint16 state;

	SDL_LockJoysticks();
	{
		CHECK_JOYSTICK_MAGIC(joystick, 0);

		if (axis < joystick->naxes) {
			state = joystick->axes[axis].value;
		} else {
			SDL_SetError("Joystick only has %d axes", joystick->naxes);
			state = 0;
		}
	}
	SDL_UnlockJoysticks();

	return state;
}

/*
 * Get the current state of a button on a joystick
 */
Uint8 SDL_GetJoystickButton(SDL_Joystick *joystick, int button)
{
	Uint8 state;

	SDL_LockJoysticks();
	{
		CHECK_JOYSTICK_MAGIC(joystick, 0);

		if (button < joystick->nbuttons) {
			state = joystick->buttons[button];
		} else {
			SDL_SetError("Joystick only has %d buttons", joystick->nbuttons);
			state = 0;
		}
	}
	SDL_UnlockJoysticks();

	return state;
}

/*
 * Get the current state of a hat on a joystick
 */
Uint8 SDL_GetJoystickHat(SDL_Joystick *joystick, int hat)
{
	Uint8 state;

	SDL_LockJoysticks();
	{
		CHECK_JOYSTICK_MAGIC(joystick, 0);

		if (hat < joystick->nhats) {
			state = joystick->hats[hat];
		} else {
			SDL_SetError("Joystick only has %d hats", joystick->nhats);
			state = 0;
		}
	}
	SDL_UnlockJoysticks();

	return state;
}

/*
 * Get the friendly name of this joystick
 */
const char *SDL_GetJoystickName(SDL_Joystick *joystick)
{
	const char *retval;
#if 0
	const SDL_SteamVirtualGamepadInfo *info;
#endif
	SDL_LockJoysticks();
	{
		CHECK_JOYSTICK_MAGIC(joystick, NULL);
#if 0
		info = SDL_GetJoystickInstanceVirtualGamepadInfo(joystick->instance_id);
		if (info) {
			retval = info->name;
		} else
#endif
		{
			retval = joystick->name;
		}
	}
	SDL_UnlockJoysticks();

	/* FIXME: Really we should reference count this name so it doesn't go away after unlock */
	return retval;
}

/*
 * Get the implementation dependent path of this joystick
 */
const char *SDL_GetJoystickPath(SDL_Joystick *joystick)
{
	const char *retval;

	SDL_LockJoysticks();
	{
		CHECK_JOYSTICK_MAGIC(joystick, NULL);

		if (joystick->path) {
			retval = joystick->path;
		} else {
			SDL_Unsupported();
			retval = NULL;
		}
	}
	SDL_UnlockJoysticks();

	return retval;
}

SDL_bool SDL_JoysticksInitialized(void)
{
	return SDL_joysticks_initialized;
}

void SDL_PrivateJoystickAdded(SDL_JoystickID instance_id)
{
	SDL_AssertJoysticksLocked();
#if 0
	{
		SDL_Event event;

		event.type = SDL_EVENT_JOYSTICK_ADDED;
		event.common.timestamp = 0;

		if (SDL_EventEnabled(event.type)) {
			event.jdevice.which = instance_id;
			SDL_PushEvent(&event);
		}
	}
#endif
	SDL_PrivateGamepadAdded(instance_id);
}

void SDL_PrivateJoystickRemoved(SDL_JoystickID instance_id)
{
	SDL_Joystick *joystick = NULL;
	SDL_Event event;

	SDL_AssertJoysticksLocked();

	/* Find this joystick... */
	for (joystick = SDL_joysticks; joystick; joystick = joystick->next) {
		if (joystick->instance_id == instance_id) {
			// SDL_PrivateJoystickForceRecentering(joystick);
			joystick->attached = SDL_FALSE;
			break;
		}
	}

	/* FIXME: The driver no longer provides the name and GUID at this point, so we
	 *		don't know whether this was a gamepad. For now always send the event.
	 */
	if (SDL_TRUE /*SDL_IsGamepad(instance_id)*/) {
		SDL_PrivateGamepadRemoved(instance_id);
	}
#if 0
	event.type = SDL_EVENT_JOYSTICK_REMOVED;
	event.common.timestamp = 0;

	if (SDL_EventEnabled(event.type)) {
		event.jdevice.which = instance_id;
		SDL_PushEvent(&event);
	}
#endif
}

int SDL_SendJoystickAxis(Uint64 timestamp, SDL_Joystick *joystick, Uint8 axis, Sint16 value)
{
	int posted;
	SDL_JoystickAxisInfo *info;

	SDL_AssertJoysticksLocked();

	/* Make sure we're not getting garbage or duplicate events */
	if (axis >= joystick->naxes) {
		return 0;
	}

	info = &joystick->axes[axis];
#if 0
	if (!info->has_initial_value ||
		(!info->has_second_value && (info->initial_value <= -32767 || info->initial_value == 32767) && SDL_abs(value) < (SDL_JOYSTICK_AXIS_MAX / 4))) {
		info->initial_value = value;
		info->value = value;
		info->zero = value;
		info->has_initial_value = SDL_TRUE;
	} else if (value == info->value && !info->sending_initial_value) {
		return 0;
	} else {
		info->has_second_value = SDL_TRUE;
	}
	if (!info->sent_initial_value) {
		/* Make sure we don't send motion until there's real activity on this axis */
		const int MAX_ALLOWED_JITTER = SDL_JOYSTICK_AXIS_MAX / 80; /* ShanWan PS3 controller needed 96 */
		if (SDL_abs(value - info->value) <= MAX_ALLOWED_JITTER &&
			!SDL_IsJoystickVIRTUAL(joystick->guid)) {
			return 0;
		}
		info->sent_initial_value = SDL_TRUE;
		info->sending_initial_value = SDL_TRUE;
		SDL_SendJoystickAxis(timestamp, joystick, axis, info->initial_value);
		info->sending_initial_value = SDL_FALSE;
	}

	/* We ignore events if we don't have keyboard focus, except for centering
	 * events.
	 */
	if (SDL_PrivateJoystickShouldIgnoreEvent()) {
		if (info->sending_initial_value ||
			(value > info->zero && value >= info->value) ||
			(value < info->zero && value <= info->value)) {
			return 0;
		}
	}
#endif
	/* Update internal joystick state */
	SDL_assert(timestamp != 0);
	info->value = value;
	joystick->update_complete = timestamp;
#if 0
	/* Post the event, if desired */
	posted = 0;
	if (SDL_EventEnabled(SDL_EVENT_JOYSTICK_AXIS_MOTION)) {
		SDL_Event event;
		event.type = SDL_EVENT_JOYSTICK_AXIS_MOTION;
		event.common.timestamp = timestamp;
		event.jaxis.which = joystick->instance_id;
		event.jaxis.axis = axis;
		event.jaxis.value = value;
		posted = SDL_PushEvent(&event) == 1;
	}
	return posted;
#else
	SDL_Event event;
	event.type = SDL_EVENT_JOYSTICK_AXIS_MOTION;
	event.common.timestamp = timestamp;
	event.jaxis.which = joystick->instance_id;
	event.jaxis.axis = axis;
	event.jaxis.value = value;
	SDL_GamepadEventWatcher(NULL, &event);
	//posted = SDL_PushEvent(&event) == 1;
	// HandleJoystickAxis(0, gamepad, axis, value);
#endif
}

int SDL_SendJoystickHat(Uint64 timestamp, SDL_Joystick *joystick, Uint8 hat, Uint8 value)
{
	int posted;

	SDL_AssertJoysticksLocked();

	/* Make sure we're not getting garbage or duplicate events */
	if (hat >= joystick->nhats) {
		return 0;
	}

	if (value == joystick->hats[hat]) {
		return 0;
	}
#if 0
	/* We ignore events if we don't have keyboard focus, except for centering
	 * events.
	 */
	if (SDL_PrivateJoystickShouldIgnoreEvent()) {
		if (value != SDL_HAT_CENTERED) {
			return 0;
		}
	}
#endif
	/* Update internal joystick state */
	SDL_assert(timestamp != 0);
	joystick->hats[hat] = value;
	joystick->update_complete = timestamp;
#if 0
	/* Post the event, if desired */
	posted = 0;
	if (SDL_EventEnabled(SDL_EVENT_JOYSTICK_HAT_MOTION)) {
		SDL_Event event;
		event.type = SDL_EVENT_JOYSTICK_HAT_MOTION;
		event.common.timestamp = timestamp;
		event.jhat.which = joystick->instance_id;
		event.jhat.hat = hat;
		event.jhat.value = value;
		posted = SDL_PushEvent(&event) == 1;
	}
	return posted;
#else
	SDL_Event event;
	event.type = SDL_EVENT_JOYSTICK_HAT_MOTION;
	event.common.timestamp = timestamp;
	event.jhat.which = joystick->instance_id;
	event.jhat.hat = hat;
	event.jhat.value = value;
	SDL_GamepadEventWatcher(NULL, &event);
	// posted = SDL_PushEvent(&event) == 1;
	// HandleJoystickHat(0, gamepad, hat, value);
#endif
}

int SDL_SendJoystickButton(Uint64 timestamp, SDL_Joystick *joystick, Uint8 button, Uint8 state)
{
	int posted;
	SDL_Event event;

	SDL_AssertJoysticksLocked();

	switch (state) {
	case SDL_PRESSED:
		event.type = SDL_EVENT_JOYSTICK_BUTTON_DOWN;
		break;
	case SDL_RELEASED:
		event.type = SDL_EVENT_JOYSTICK_BUTTON_UP;
		break;
	default:
		/* Invalid state -- bail */
		return 0;
	}

	SDL_AssertJoysticksLocked();

	/* Make sure we're not getting garbage or duplicate events */
	if (button >= joystick->nbuttons) {
		return 0;
	}

	if (state == joystick->buttons[button]) {
		return 0;
	}

	/* Update internal joystick state */
	joystick->buttons[button] = state;
	joystick->update_complete = timestamp;
#if 0
	/* Post the event, if desired */
	posted = 0;
	if (SDL_EventEnabled(event.type)) {
		event.common.timestamp = timestamp;
		event.jbutton.which = joystick->instance_id;
		event.jbutton.button = button;
		event.jbutton.state = state;
		posted = SDL_PushEvent(&event) == 1;
	}
	return posted;
#else
	event.common.timestamp = timestamp;
	event.jbutton.which = joystick->instance_id;
	event.jbutton.button = button;
	event.jbutton.state = state;
	SDL_GamepadEventWatcher(NULL, &event);
	return 1;
#endif
}

SDL_JoystickID *SDL_GetJoysticks(int *count)
{
	int i, num_joysticks, device_index;
	int joystick_index = 0, total_joysticks = 0;
	SDL_JoystickID *joysticks;

	SDL_LockJoysticks();
	{
		for (i = 0; i < SDL_arraysize(SDL_joystick_drivers); ++i) {
			total_joysticks += SDL_joystick_drivers[i]->GetCount();
		}

		joysticks = (SDL_JoystickID *)SDL_malloc((total_joysticks + 1) * sizeof(*joysticks));
		if (joysticks) {
			if (count) {
				*count = total_joysticks;
			}

			for (i = 0; i < SDL_arraysize(SDL_joystick_drivers); ++i) {
				num_joysticks = SDL_joystick_drivers[i]->GetCount();
				for (device_index = 0; device_index < num_joysticks; ++device_index) {
					SDL_assert(joystick_index < total_joysticks);
					joysticks[joystick_index] = SDL_joystick_drivers[i]->GetDeviceInstanceID(device_index);
					SDL_assert(joysticks[joystick_index] > 0);
					++joystick_index;
				}
			}
			SDL_assert(joystick_index == total_joysticks);
			joysticks[joystick_index] = 0;
		} else {
			if (count) {
				*count = 0;
			}
		}
	}
	SDL_UnlockJoysticks();

	return joysticks;
}

int SDL_InitJoysticks(void)
{
	int i, status;
#if 0
	/* Create the joystick list lock */
	if (SDL_joystick_lock == NULL) {
		SDL_joystick_lock = SDL_CreateMutex();
	}

	if (SDL_InitSubSystem(SDL_INIT_EVENTS) < 0) {
		return -1;
	}
#endif
	SDL_LockJoysticks();

	SDL_joysticks_initialized = SDL_TRUE;

	SDL_InitGamepadMappings();

	status = -1;
	for (i = 0; i < SDL_arraysize(SDL_joystick_drivers); ++i) {
		if (SDL_joystick_drivers[i]->Init() >= 0) {
			status = 0;
		}
	}
}

int SDL_NumJoysticks(void)
{
	int i, total_joysticks = 0;

	SDL_LockJoysticks();
	{
		for (i = 0; i < SDL_arraysize(SDL_joystick_drivers); ++i) {
			total_joysticks += SDL_joystick_drivers[i]->GetCount();
		}
	}
	SDL_UnlockJoysticks();

	return total_joysticks;
}
