#include <math.h>

#include "SDL_sysjoystick.h"
#include "SDL_joystick_c.h"

#include "internal.h"

static SDL_Joystick *g_joystick;

Uint32 SDL_GetNextObjectID(void)
{
	static int last_id;

	Uint32 id = last_id++ + 1;
	if (id == 0) {
		id = last_id++ + 1;
	}
	return id;
}

/* A linked list of available joysticks */
typedef struct SDL_joylist_item
{
	SDL_JoystickID device_instance;
#if 0
	char *path; /* "/dev/input/event2" or whatever */
	char *name; /* "SideWinder 3D Pro" or whatever */
#endif
	SDL_JoystickGUID guid;
	dev_t devnum;
#if 0
	int steam_virtual_gamepad_slot;
	struct joystick_hwdata *hwdata;
#endif
	struct SDL_joylist_item *next;
#if 0
	/* Steam Controller support */
	SDL_bool m_bSteamController;

	SDL_bool checked_mapping;
	SDL_GamepadMapping *mapping;
#endif
	joystick_attrib_t jattr;
} SDL_joylist_item;

static SDL_joylist_item *SDL_joylist SDL_GUARDED_BY(SDL_joystick_lock) = NULL;
static SDL_joylist_item *SDL_joylist_tail SDL_GUARDED_BY(SDL_joystick_lock) = NULL;
static int numjoysticks SDL_GUARDED_BY(SDL_joystick_lock) = 0;

#if 0
static int IsJoystick(const char *path, int fd, char **name_return, Uint16 *vendor_return, Uint16 *product_return, SDL_JoystickGUID *guid)
{
	char *name;
	char product_string[128];
	int class = 0;

	if (ioctl(fd, JSIOCGNAME(sizeof(product_string)), product_string) <= 0) {
		/* When udev is enabled we only get joystick devices here, so there's no need to test them */
		if (enumeration_method != ENUMERATION_LIBUDEV &&
			!(class & SDL_UDEV_DEVICE_JOYSTICK) && ( class || !GuessIsJoystick(fd))) {
			return 0;
		}

		/* Could have vendor and product already from udev, but should agree with evdev */
		if (ioctl(fd, EVIOCGID, &inpid) < 0) {
			return 0;
		}

		if (ioctl(fd, EVIOCGNAME(sizeof(product_string)), product_string) < 0) {
			return 0;
		}
	}

	name = SDL_CreateJoystickName(inpid.vendor, inpid.product, NULL, product_string);
	if (!name) {
		return 0;
	}

#ifdef DEBUG_JOYSTICK
	SDL_Log("Joystick: %s, bustype = %d, vendor = 0x%.4x, product = 0x%.4x, version = %d\n", name, inpid.bustype, inpid.vendor, inpid.product, inpid.version);
#endif

	*guid = SDL_CreateJoystickGUID(inpid.bustype, inpid.vendor, inpid.product, inpid.version, NULL, product_string, 0, 0);

	*name_return = name;
	*vendor_return = inpid.vendor;
	*product_return = inpid.product;
	return 1;
}
#endif

static void MaybeAddDevice(const joystick_attrib_t *jattr)
{
	SDL_JoystickGUID guid;
	SDL_joylist_item *item;

	if (!jattr) {
		return;
	}

	SDL_LockJoysticks();

	/* Check to make sure it's not already in list. */
	for (item = SDL_joylist; item; item = item->next) {
		if (jattr->devno == item->devnum) {
			goto done; /* already have this one */
		}
	}

	LOG(LOG_SDL_SYSJOYSTICK_TRACE, "Joystick: %d, bustype = %d, vendor = 0x%.4x, product = 0x%.4x, version = %d\n",
	    jattr->devno, SDL_HARDWARE_BUS_USB, jattr->vendor_id,
	    jattr->product_id, jattr->version);

	guid = SDL_CreateJoystickGUID(SDL_HARDWARE_BUS_USB,
				      jattr->vendor_id, jattr->product_id,
				      jattr->version, NULL,
				      "", //FIXME: jattr->product_string,
				      0, 0);

	item = (SDL_joylist_item *)SDL_calloc(1, sizeof(SDL_joylist_item));
	if (!item) {
		goto done;
	}

	item->devnum = jattr->devno;
	item->guid = guid;

	item->device_instance = SDL_GetNextObjectID();
	if (!SDL_joylist_tail) {
		SDL_joylist = SDL_joylist_tail = item;
	} else {
		SDL_joylist_tail->next = item;
		SDL_joylist_tail = item;
	}

	{
		item->jattr = *jattr;
	}

	/* Need to increment the joystick count before we post the event */
	++numjoysticks;

	SDL_PrivateJoystickAdded(item->device_instance);
	goto done;

done:
	SDL_UnlockJoysticks();
}

static void RemoveJoylistItem(SDL_joylist_item *item, SDL_joylist_item *prev)
{
	SDL_AssertJoysticksLocked();
#if 0
	if (item->hwdata) {
		item->hwdata->item = NULL;
	}
#endif
	if (prev) {
		prev->next = item->next;
	} else {
		SDL_assert(SDL_joylist == item);
		SDL_joylist = item->next;
	}

	if (item == SDL_joylist_tail) {
		SDL_joylist_tail = prev;
	}

	/* Need to decrement the joystick count before we post the event */
	--numjoysticks;

	SDL_PrivateJoystickRemoved(item->device_instance);
#if 0
	FreeJoylistItem(item);
#else
	SDL_free(item);
#endif
}

static void MaybeRemoveDevice(uint32_t *devno)
{
	SDL_joylist_item *item;
	SDL_joylist_item *prev = NULL;

	if (!devno) {
		return;
	}

	SDL_LockJoysticks();

	for (item = SDL_joylist; item; item = item->next) {
		/* found it, remove it. */
		if (*devno == item->devnum) {
			RemoveJoylistItem(item, prev);
			break;
		}
		prev = item;
	}

	SDL_UnlockJoysticks();
}

static int QNX_JoystickInit(void)
{
	LOG(LOG_SDL_SYSJOYSTICK_TRACE, "%s [%d] +\n",
	    __func__, __LINE__);

	return 1;
}

static int QNX_JoystickGetCount(void)
{
	SDL_AssertJoysticksLocked();

	return numjoysticks;
}

static void QNX_JoystickDetect(void)
{
}

static SDL_bool QNX_JoystickIsDevicePresent(Uint16 vendor_id, Uint16 product_id, Uint16 version, const char *name)
{
	/* We don't override any other drivers */
	return SDL_FALSE;
}

static SDL_joylist_item *GetJoystickByDevIndex(int device_index)
{
	SDL_joylist_item *item;

	SDL_AssertJoysticksLocked();

	if ((device_index < 0) || (device_index >= numjoysticks)) {
		return NULL;
	}

	item = SDL_joylist;
	while (device_index > 0) {
		SDL_assert(item != NULL);
		device_index--;
		item = item->next;
	}

	return item;
}

static const char *QNX_JoystickGetDeviceName(int device_index)
{
	// return GetJoystickByDevIndex(device_index)->name;
	return NULL;
}

static const char *QNX_JoystickGetDevicePath(int device_index)
{
	// return GetJoystickByDevIndex(device_index)->path;
	return NULL;
}

static int QNX_JoystickGetDeviceSteamVirtualGamepadSlot(int device_index)
{
	return -1;
}

static int QNX_JoystickGetDevicePlayerIndex(int device_index)
{
	return -1;
}

static void QNX_JoystickSetDevicePlayerIndex(int device_index, int player_index)
{
}

static SDL_JoystickGUID QNX_JoystickGetDeviceGUID(int device_index)
{
	return GetJoystickByDevIndex(device_index)->guid;
}

/* Function to perform the mapping from device index to the instance id for this index */
static SDL_JoystickID QNX_JoystickGetDeviceInstanceID(int device_index)
{
	return GetJoystickByDevIndex(device_index)->device_instance;
}

static void ConfigJoystick(SDL_Joystick *joystick,
			   const joystick_attrib_t *jattr)
{
	joystick->naxes = jattr->naxis;
	joystick->nbuttons = jattr->nButtons;
	
	if (jattr->has_hat)
		joystick->nhats = 1;

	joystick->is_gamepad = 1;
}

static int QNX_JoystickOpen(SDL_Joystick *joystick, int device_index)
{
	SDL_joylist_item *item;

	LOG(LOG_SDL_SYSJOYSTICK_TRACE, "%s [%d] +\n",
	    __func__, __LINE__);

	SDL_AssertJoysticksLocked();

	item = GetJoystickByDevIndex(device_index);
	if (!item) {
		return SDL_SetError("No such device");
	}

	joystick->instance_id = item->device_instance;

	/* Get the number of buttons and axes on the joystick */
	ConfigJoystick(joystick, &item->jattr);

	// FIXME:
	g_joystick = joystick;

	LOG(LOG_SDL_SYSJOYSTICK_TRACE, "%s [%d] -\n",
	    __func__, __LINE__);

	return 0;
}

static int QNX_JoystickRumble(SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
	return SDL_Unsupported();
}

static int QNX_JoystickRumbleTriggers(SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
	return SDL_Unsupported();
}

static int QNX_JoystickSetLED(SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
	return SDL_Unsupported();
}

static int QNX_JoystickSendEffect(SDL_Joystick *joystick, const void *data, int size)
{
	return SDL_Unsupported();
}

static int QNX_JoystickSetSensorsEnabled(SDL_Joystick *joystick, SDL_bool enabled)
{
	return SDL_Unsupported();
}

static void QNX_JoystickUpdate(SDL_Joystick *joystick)
{
	LOG(LOG_SDL_SYSJOYSTICK_TRACE, "%s [%d] +\n",
	    __func__, __LINE__);
}

static void QNX_JoystickClose(SDL_Joystick *joystick)
{
	LOG(LOG_SDL_SYSJOYSTICK_TRACE, "%s [%d] +\n",
	    __func__, __LINE__);

	g_joystick = NULL;
}

static void QNX_JoystickQuit(void)
{
	LOG(LOG_SDL_SYSJOYSTICK_TRACE, "%s [%d] +\n",
	    __func__, __LINE__);
}
#if 0
static SDL_bool QNX_JoystickGetGamepadMapping(int device_index, SDL_GamepadMapping *out)
{
	return SDL_FALSE;
}
#endif

int handleJoystickInsert(input_module_t *module, int data_size, void *data)
{
	LOG(LOG_SDL_SYSJOYSTICK_TRACE, "%s [%d] +\n",
	    __func__, __LINE__);

	MaybeAddDevice(data);

	return 0;
}

int handleJoystickRemove(input_module_t *module, int data_size, void * data)
{
	LOG(LOG_SDL_SYSJOYSTICK_TRACE, "%s [%d] +\n",
	    __func__, __LINE__);

	MaybeRemoveDevice(data);

	return 0;
}

static int AxisCorrect(int value)
{
	float value_range = (255 - 0);
	float output_range = (SDL_JOYSTICK_AXIS_MAX - SDL_JOYSTICK_AXIS_MIN);
	float scale = 256; //(output_range / value_range);

	value = (int)floorf((value - 0) * scale + SDL_JOYSTICK_AXIS_MIN + 0.5f);
	// fprintf(stdout, "update_axis#%d: val: %d v2 %d, res: %d\n",axis, value, v2,(int)floorf((value - 0) * scale + SDL_JOYSTICK_AXIS_MIN + 0.5f));

	/* Clamp and return */
	if (value < SDL_JOYSTICK_AXIS_MIN) {
		return SDL_JOYSTICK_AXIS_MIN;
	}
	if (value > SDL_JOYSTICK_AXIS_MAX) {
		return SDL_JOYSTICK_AXIS_MAX;
	}
	return value;
}

static void updateAxis(SDL_Joystick *joystick, int axis, int value)
{
	SDL_JoystickAxisInfo *info = &joystick->axes[axis];
	value = AxisCorrect(value);

	if (info->value != value) {
		info->value = value;
		SDL_SendJoystickAxis(0, joystick, axis, value);
	}
}

static int updateHat(SDL_Joystick *joystick, int hat, uint32_t value)
{
	const Uint8 position_map[9] = {
		SDL_HAT_UP, SDL_HAT_RIGHTUP, SDL_HAT_RIGHT,
		SDL_HAT_RIGHTDOWN, SDL_HAT_DOWN, SDL_HAT_LEFTDOWN,
		SDL_HAT_LEFT, SDL_HAT_LEFTUP, SDL_HAT_CENTERED };

	if (value > 8)
		return 0;

	value = position_map[value];
#if 0
	/* Make sure we're not getting garbage or duplicate events */
	if (hat >= joystick->nhats) {
		return 0;
	}

	if (value == joystick->hats[hat]) {
		return 0;
	}

	/* Update internal joystick state */
	joystick->hats[hat] = value;
#endif
	SDL_SendJoystickHat(0, joystick, hat, value);

	return 1;
}

int handleJoystickEvent(input_module_t *module, int data_size, void *data)
{
	SDL_joylist_item *item;
	SDL_joylist_item *prev = NULL;

	pJoystick_raw_data_t j_data = (pJoystick_raw_data_t)data;

	if (data_size != sizeof(*j_data))
		return -1;
#if 0
	/* Find joystick item */
	for (item = SDL_joylist; item; item = item->next) {
		if (j_data->devno == item->devnum) {
			break;
		}
	}

	if (item == NULL)
		return -1;
#endif
	SDL_Joystick *joystick = g_joystick;

	if (g_joystick == NULL)
		return -1;

	//FIXME: this will not work for 2 or more devices
	static uint64_t prevBtnStates;
	static uint32_t prevHatState;

	uint64_t changedBtnStates;
	int i;

	/* Send button press/release events */
	changedBtnStates = prevBtnStates ^ j_data->button_state;
	if (changedBtnStates) {
		/* Cycle all buttons status */
		for (i = 0; i < 64; i++) {
			uint8_t state;

			if (!((1 << i) & changedBtnStates))
				continue;

			state = ((1 << i) & j_data->button_state) ? SDL_PRESSED : SDL_RELEASED;

			uint8_t button = i;
#if 0
			/* Make sure we're not getting garbage or duplicate events */
			if (button >= joystick->nbuttons) {
				continue;
			}
			if (state == joystick->buttons[button]) {
				continue;
			}

			joystick->buttons[button] = state;
#endif
			SDL_SendJoystickButton(0, joystick, button, state);
		}

		prevBtnStates = j_data->button_state;
	}

	/* Update axis data */
	updateAxis(joystick, 0, j_data->x);
	updateAxis(joystick, 1, j_data->y);
	updateAxis(joystick, 2, j_data->z);
	updateAxis(joystick, 3, j_data->Rx);
	updateAxis(joystick, 4, j_data->Ry);
	updateAxis(joystick, 5, j_data->Rz);

	/* Hat */
	updateHat(joystick, 0, j_data->hat_switch);

	return 0;
}

SDL_JoystickDriver SDL_QNX_JoystickDriver = {
	QNX_JoystickInit,
	QNX_JoystickGetCount,
	QNX_JoystickDetect,
	QNX_JoystickIsDevicePresent,
	QNX_JoystickGetDeviceName,
	QNX_JoystickGetDevicePath,
	QNX_JoystickGetDeviceSteamVirtualGamepadSlot,
	QNX_JoystickGetDevicePlayerIndex,
	QNX_JoystickSetDevicePlayerIndex,
	QNX_JoystickGetDeviceGUID,
	QNX_JoystickGetDeviceInstanceID,
	QNX_JoystickOpen,
	QNX_JoystickRumble,
	QNX_JoystickRumbleTriggers,
	QNX_JoystickSetLED,
	QNX_JoystickSendEffect,
	QNX_JoystickSetSensorsEnabled,
	QNX_JoystickUpdate,
	QNX_JoystickClose,
	QNX_JoystickQuit,
//	QNX_JoystickGetGamepadMapping
};
