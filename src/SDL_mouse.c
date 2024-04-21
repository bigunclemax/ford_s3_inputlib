#include <SDL3/SDL_events.h>
#include "internal.h"

int SDL_SendMouseButton(uint64_t timestamp, int key_state, int key_code)
{
	LOG(LOG_SDL_GAMEPAD_TRACE, "%s [%d] Mouse Key: %d (0x%x) %s\n",
	    __func__, __LINE__, key_code, key_code,
	    key_state == SDL_RELEASED ? "RELEASED" : "PRESSED");

	if (!l_evt_q)
		return 0;

	SDL_Event *ev = malloc(sizeof(SDL_Event));
	ev->type = (key_state == SDL_RELEASED) ? SDL_EVENT_MOUSE_BUTTON_UP : SDL_EVENT_MOUSE_BUTTON_DOWN;
	ev->button.button = key_code;

	enque(l_evt_q, ev);

	return 1;
}

int SDL_SendMouseWheel(uint64_t timestamp, int z1, int wheel)
{
	LOG(LOG_SDL_GAMEPAD_TRACE, "%s [%d] Mouse wheel: %d (0x%x)\n",
	    __func__, __LINE__, wheel, wheel);

	if (!l_evt_q)
		return 0;

	SDL_Event *ev = malloc(sizeof(SDL_Event));
	ev->type = SDL_EVENT_MOUSE_WHEEL;
	ev->wheel.y = wheel;
	enque(l_evt_q, ev);

	return 1;
}

int SDL_SendMouseMotion(uint64_t timestamp, int z1, int horizontal_precision, int vertical_precision, int z2)
{
	LOG(LOG_SDL_GAMEPAD_TRACE, "%s [%d] Mouse H: %04d V: %04d\n",
	    __func__, __LINE__, horizontal_precision, vertical_precision);

	if (!l_evt_q)
		return 0;

	SDL_Event *ev = malloc(sizeof(SDL_Event));
	ev->type = SDL_EVENT_MOUSE_MOTION;
	ev->motion.xrel = horizontal_precision;
	ev->motion.yrel = vertical_precision;

	enque(l_evt_q, ev);

	return 1;
}

int handleMouseEvent(input_module_t *module, int data_size, void * data)
{
	static uint8_t prevBtnStates; //FIXME: this will not work for 2 or more mouses
	pMouse_raw_data_t m_data;
	uint8_t changedBtnStates;
	int i;

	if (data_size < sizeof(mouse_raw_data_t))
		return -1;

	m_data = (pMouse_raw_data_t)data;

	LOG(LOG_SDL_GAMEPAD_TRACE, "m_data: X:%04d Y:%04d Z:%04d btn:%02x\n",
	    m_data->x, m_data->y, m_data->z, m_data->btnStates);

	/* Send motion event if motion really was */
	if ((m_data->x != 0) || (m_data->y != 0))
		SDL_SendMouseMotion(0, 1, m_data->x, m_data->y, 0);

	/* Send mouse button press/release events */
	changedBtnStates = prevBtnStates ^ m_data->btnStates;
	if (changedBtnStates) {
		/* Cycle all buttons status */
		for (i = 0; i < 8; i++) {
			if (!((1 << i) & changedBtnStates))
				continue;
			
			if ((1 << i) & m_data->btnStates)
				SDL_SendMouseButton(0, SDL_PRESSED, i);
			else
				SDL_SendMouseButton(0, SDL_RELEASED, i);
		}

		prevBtnStates = m_data->btnStates;
	}

	/* Send mouse wheel events */
	/* Send vertical wheel event only */
	if (m_data->z != 0)
		SDL_SendMouseWheel(0, 0, m_data->z);
	
	return 0;
}
