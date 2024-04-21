#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>

#include <SDL3/SDL.h>
#include "SDL3/SDL_gamepad.h"

#include "internal.h"

int verbose;

static int do_exit = 0;

static SDL_Gamepad *l_controller;
static SDL_Joystick *l_joystick;

const char *SDLEventName(int t)
{
	switch(t) {
	case SDL_EVENT_JOYSTICK_BUTTON_DOWN: return "SDL_EVENT_JOYSTICK_BUTTON_DOWN";
	case SDL_EVENT_JOYSTICK_BUTTON_UP: return "SDL_EVENT_JOYSTICK_BUTTON_UP";
	case SDL_EVENT_JOYSTICK_AXIS_MOTION: return "SDL_EVENT_JOYSTICK_AXIS_MOTION";
	case SDL_EVENT_JOYSTICK_HAT_MOTION: return "SDL_EVENT_JOYSTICK_HAT_MOTION";
	case SDL_EVENT_KEY_DOWN: return "SDL_EVENT_KEY_DOWN";
	case SDL_EVENT_KEY_UP: return "SDL_EVENT_KEY_UP";
	case SDL_EVENT_TEXT_EDITING: return "SDL_EVENT_TEXT_EDITING";
	case SDL_EVENT_TEXT_INPUT: return "SDL_EVENT_TEXT_INPUT";
	case SDL_EVENT_KEYMAP_CHANGED: return "SDL_EVENT_KEYMAP_CHANGED";
	case SDL_EVENT_KEYBOARD_ADDED: return "SDL_EVENT_KEYBOARD_ADDED";
	case SDL_EVENT_KEYBOARD_REMOVED: return "SDL_EVENT_KEYBOARD_REMOVED";
	case SDL_EVENT_MOUSE_MOTION: return "SDL_EVENT_MOUSE_MOTION";
	case SDL_EVENT_MOUSE_BUTTON_DOWN: return "SDL_EVENT_MOUSE_BUTTON_DOWN";
	case SDL_EVENT_MOUSE_BUTTON_UP: return "SDL_EVENT_MOUSE_BUTTON_UP";
	case SDL_EVENT_MOUSE_WHEEL: return "SDL_EVENT_MOUSE_WHEEL";
	case SDL_EVENT_MOUSE_ADDED: return "SDL_EVENT_MOUSE_ADDED";
	case SDL_EVENT_MOUSE_REMOVED: return "SDL_EVENT_MOUSE_REMOVED";
	case SDL_EVENT_GAMEPAD_AXIS_MOTION: return "SDL_EVENT_GAMEPAD_AXIS_MOTION";
	case SDL_EVENT_GAMEPAD_BUTTON_DOWN: return "SDL_EVENT_GAMEPAD_BUTTON_DOWN";
	case SDL_EVENT_GAMEPAD_BUTTON_UP: return "SDL_EVENT_GAMEPAD_BUTTON_UP";
	case SDL_EVENT_GAMEPAD_ADDED: return "SDL_EVENT_GAMEPAD_ADDED";
	case SDL_EVENT_GAMEPAD_REMOVED: return "SDL_EVENT_GAMEPAD_REMOVED";
	case SDL_EVENT_GAMEPAD_REMAPPED: return "SDL_EVENT_GAMEPAD_REMAPPED";
	case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN: return "SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN";
	case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION: return "SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION";
	case SDL_EVENT_GAMEPAD_TOUCHPAD_UP: return "SDL_EVENT_GAMEPAD_TOUCHPAD_UP";
	case SDL_EVENT_GAMEPAD_SENSOR_UPDATE: return "SDL_EVENT_GAMEPAD_SENSOR_UPDATE";
	case SDL_EVENT_GAMEPAD_UPDATE_COMPLETE: return "SDL_EVENT_GAMEPAD_UPDATE_COMPLETE";
	case SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED: return "SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED";
	default: return "Unknown";
	}
}

static void HandleGamepadAdded(SDL_JoystickID id)
{
	if (!l_controller) {
		l_controller = SDL_OpenGamepad(id);
		if (l_controller) {
			const char *name = SDL_GetGamepadName(l_controller);
			const char *path = SDL_GetGamepadPath(l_controller);
			fprintf(stdout, "Opened gamepad %s%s%s\n",
				name, path ? ", " : "", path ? path : "");
		}
	}
}

static void HandleGamepadRemoved(SDL_JoystickID id)
{
	l_controller = NULL;
}

int ProcessEvent(SDL_Event *event)
{
	fprintf(stdout, "Got event: %s (%d)\n",
		SDLEventName(event->type), event->type);

	switch (event->type) {
	case SDL_MOUSEWHEEL:
		fprintf(stdout, "MOUSE WHEEL: %02d %s\n", event->wheel.y,
			event->wheel.y > 0 ? "UP" : "DOWN");
		break;

	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		fprintf(stdout, "MOUSE BUTTON: %02d %s\n", event->button.button,
			event->type == SDL_MOUSEBUTTONUP ? "UP" : "DOWN");
		break;

	case SDL_MOUSEMOTION:
		fprintf(stdout, "MOUSE MOTION: x:%04d y:%04d\n",
			event->motion.xrel, event->motion.yrel);
		break;

	case SDL_KEYDOWN:
	case SDL_KEYUP:
		fprintf(stdout, "KEY: %04d\n", event->key.keysym.sym,
			event->type == SDL_KEYUP ? "UP" : "DOWN");
		break;

	case SDL_CONTROLLERBUTTONDOWN:
	case SDL_CONTROLLERBUTTONUP:
		fprintf(stdout, "CONTROLLER BUTTON: %04d\n", event->cbutton.button,
			event->type == SDL_KEYUP ? "UP" : "DOWN");
		break;

	case SDL_EVENT_GAMEPAD_AXIS_MOTION:
		fprintf(stdout, "SDL_EVENT_GAMEPAD_AXIS_MOTION BUTTON: Axis: %04d Value: %04d\n",
			event->gaxis.axis, event->gaxis.value);
		break;

	case SDL_EVENT_GAMEPAD_ADDED:
		HandleGamepadAdded(event->gdevice.which);
		break;

	case SDL_EVENT_GAMEPAD_REMOVED:
		HandleGamepadRemoved(event->gdevice.which);
		break;
	}

	return 0;
}

#if 0
//bt "050000004c050000c405000000010000,PS4 Controller,a:b1,b:b2,back:b8,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b4,leftstick:b10,lefttrigger:a3,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b11,righttrigger:a4,rightx:a2,righty:a5,start:b9,x:b0,y:b3,platform:Linux";
const char ps4_mapping_1[] = "030000004c050000c405000000000000,PS4 Controller,a:b1,b:b2,back:b8,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b4,leftstick:b10,lefttrigger:a3,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b11,righttrigger:a4,rightx:a2,righty:a5,start:b9,touchpad:b13,x:b0,y:b3,platform:Windows";
const char ps4_mapping_2[] = "030000004c050000c405000000000000,PS4 Controller,a:b1,b:b2,back:b8,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b4,leftstick:b10,lefttrigger:a3,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b11,righttrigger:a4,rightx:a2,righty:a5,start:b9,x:b0,y:b3,platform:Mac OS X";
const char ps4_mapping_3[] = "030000004c050000c405000000010000,PS4 Controller,a:b1,b:b2,back:b8,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b4,leftstick:b10,lefttrigger:a3,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b11,righttrigger:a4,rightx:a2,righty:a5,start:b9,x:b0,y:b3,platform:Mac OS X";
const char ps4_mapping_4[] = "030000004c050000c405000011010000,PS4 Controller,a:b1,b:b2,back:b8,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b4,leftstick:b10,lefttrigger:a3,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b11,righttrigger:a4,rightx:a2,righty:a5,start:b9,x:b0,y:b3,platform:Linux";
const char ps4_mapping_5[] = "030000004c050000c405000011810000,PS4 Controller,a:b0,b:b1,back:b8,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b10,leftshoulder:b4,leftstick:b11,lefttrigger:a2,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b12,righttrigger:a5,rightx:a3,righty:a4,start:b9,x:b3,y:b2,platform:Linux";
#endif

float stick_deadzone = 0.2;
float stick_curve = 1;
float stick_sensitivity = 1;
float running = 1.0f;
float cl_speed_side = 200, cl_speed_forward = 200;

static float ComputeStickValue(float stickValue)
{
	float deadzone = stick_deadzone * (1.0f / 2.0f);
	if (stickValue < 0.0f)
	{
		if (stickValue > -deadzone)
			stickValue = 0.0f;
		else
			stickValue = (stickValue + deadzone) / (1.0f - deadzone); // Normalize.
	}
	else
	{
		if (stickValue < deadzone)
			stickValue = 0.0f;
		else
			stickValue = (stickValue - deadzone) / (1.0f - deadzone); // Normalize.
	}

	// for stick_curve, 0.5 gives quick response, 1.0 is linear, 2.0 gives slow responses.
	if (stickValue < 0.0f)
		stickValue = -powf(-stickValue, stick_curve);
	else
		stickValue = +powf(+stickValue, stick_curve);

	return stickValue * stick_sensitivity;
}

void SigintHandler(int sig_number)
{
	do_exit = 1;
}

int main(int argc, char *argv[]) {
	int opt, ret;

	fprintf(stdout, "Start gamepad tester\n");

	while ((opt = getopt(argc, argv, "v:")) != -1) {
		switch (opt) {
		case 'v':
			verbose = atoi(optarg);
			break;
		}
	}

	signal(SIGINT, SigintHandler);

	if (!SDL_WasInit(SDL_INIT_JOYSTICK)) {
		if (SDL_Init(SDL_INIT_JOYSTICK) == -1) {
			fprintf(stdout, "Couldn't init SDL l_joystick\n");
			return -1;
		}
	}
	
	int i, num_joysticks;
	SDL_JoystickID *joysticks = SDL_GetJoysticks(&num_joysticks);

	fprintf(stdout, "Found %d joysticks at startup\n", num_joysticks);

	for (i = 0; i < num_joysticks; i++) {
		SDL_JoystickID instance_id = joysticks[i];
		if (SDL_IsGameController(i)) {
			if (!l_controller) {
				l_controller = SDL_GameControllerOpen(instance_id);

				if (!l_controller)
					fprintf(stdout, "Could not open gamecontroller %i: %s\n",
						i, SDL_GetError());
			}
		}
		else
		{
			if (!l_joystick)
			{
				l_joystick = SDL_JoystickOpen(i);
				if (!l_joystick)
				{
					fprintf(stdout, "Could not open l_joystick %i: %s\n",
						i, SDL_GetError());
				}
			}
		}
	}
	SDL_free(joysticks);

	while(!do_exit) {
		SDL_Event event;

		while (SDL_PollEvent(&event))
			ProcessEvent(&event);

		if (!l_controller)
			continue;

		static int prev_sidemove, prev_forwardmove;
		int sidemove = 0, forwardmove = 0;
		float joyX, joyY;
		float joyXFloat = 0.0f, joyYFloat = 0.0f;

		joyX = (float)SDL_GameControllerGetAxis(l_controller, SDL_CONTROLLER_AXIS_RIGHTX) / 32768.0f;;
		joyY = (float)SDL_GameControllerGetAxis(l_controller, SDL_CONTROLLER_AXIS_RIGHTY) / 32768.0f;;
		joyXFloat += ComputeStickValue(joyX);
		joyYFloat += ComputeStickValue(joyY);

		if (verbose >= 3)
			fprintf(stdout, "joyRX: %f joyRY: %f joyRXFloat: %f joyRYFloat: %f\n",
				joyX, joyY, joyXFloat, joyYFloat);

		joyX = (float)SDL_GameControllerGetAxis(l_controller, SDL_CONTROLLER_AXIS_LEFTX) / 32768.0f;;
		joyY = (float)SDL_GameControllerGetAxis(l_controller, SDL_CONTROLLER_AXIS_LEFTY) / 32768.0f;;

		if (verbose >= 3)
			fprintf(stdout, "joyLX: %f joyLY: %f ", joyX, joyY);

		joyX = ComputeStickValue(joyX);
		joyY = ComputeStickValue(joyY);

		if (verbose >= 3)
			fprintf(stdout, "joyLXFloat: %f joyLYFloat: %f\n", joyX, joyY);

		sidemove += cl_speed_side * joyX * running;
		forwardmove += -1 * cl_speed_forward * joyY * running;

		if ((prev_sidemove != sidemove) || (prev_forwardmove != forwardmove)) {
			fprintf(stdout, "sidemove: %04d forwardmove: %04d\n", 
				sidemove, forwardmove);

			prev_sidemove = sidemove;
			prev_forwardmove = forwardmove;
		}
	}

	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);

	return 0;
}