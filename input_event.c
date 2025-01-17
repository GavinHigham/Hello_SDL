#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "input_event.h"
#include "macros.h"

static int key_state_numkeys = 0;
const uint8_t *key_state = NULL;
uint8_t *key_state_prev = NULL;
static uint32_t mouse_state = 0;
static uint32_t mouse_state_prev = 0;
SDL_Event input_mouse_wheel_sum;

Sint16 axes[NUM_HANDLED_AXES] = {0};
bool nes30_buttons[16] = {false};

SDL_GameController *input_controller = NULL;
SDL_Joystick *input_joystick = NULL;

void controller_init()
{
	/* Open the first available controller. */
	SDL_GameController *controller = NULL;
	SDL_Joystick *joystick = NULL;
	for (int i = 0; i < SDL_NumJoysticks(); ++i) {
		printf("Testing controller %i\n", i);
		if (SDL_IsGameController(i)) {
			controller = SDL_GameControllerOpen(i);
			if (controller) {
				printf("Successfully opened controller %i\n", i);
				break;
			} else {
				printf("Could not open gamecontroller %i: %s\n", i, SDL_GetError());
			}
		} else {
			joystick = SDL_JoystickOpen(i);
			printf("Controller %i is not a controller?\n", i);
		}
	}
}

void input_event_device_arrival(int which)
{
	if (SDL_IsGameController(which))
		input_controller = input_controller ? input_controller : SDL_GameControllerOpen(which);
	else
		input_joystick = input_joystick ? input_joystick : SDL_JoystickOpen(which);
}

void input_event_init()
{
	key_state = SDL_GetKeyboardState(&key_state_numkeys);
	key_state_prev = calloc(key_state_numkeys, sizeof(uint8_t));

	for (int i = 0; i < LENGTH(nes30_buttons); i++)
		nes30_buttons[i] = false;
	controller_init();
}

void input_event_deinit()
{
	free(key_state_prev);
	key_state = NULL;
}

void input_event_save_prev_key_state()
{
	memcpy(key_state_prev, key_state, key_state_numkeys * sizeof(uint8_t));
}

void input_event_save_prev_mouse_state()
{
	mouse_state_prev = mouse_state;
	mouse_state = SDL_GetMouseState(NULL, NULL);
}

void caxisevent(SDL_Event e)
{
	switch (e.caxis.axis) {
	case SDL_CONTROLLER_AXIS_LEFTX:
		axes[LEFTX] = e.caxis.value; break;
	case SDL_CONTROLLER_AXIS_LEFTY:
		axes[LEFTY] = e.caxis.value; break;
	case SDL_CONTROLLER_AXIS_RIGHTX:
		axes[RIGHTX] = e.caxis.value; break;
	case SDL_CONTROLLER_AXIS_RIGHTY:
		axes[RIGHTY] = e.caxis.value; break;
	case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
		axes[TRIGGERLEFT] = e.caxis.value; break;
	case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
		axes[TRIGGERRIGHT] = e.caxis.value; break;
	}
}

void jaxisevent(SDL_Event e)
{
	switch (e.jaxis.axis) {
	case 0:
		axes[LEFTX] = e.jaxis.value; break;
	case 1:
		axes[LEFTY] = e.jaxis.value; break;
	case 2:
		axes[RIGHTX] = e.jaxis.value; break;
	case 3:
		axes[RIGHTY] = e.jaxis.value; break;
	case 4:
		axes[TRIGGERLEFT] = e.jaxis.value; break;
	case 5:
		axes[TRIGGERRIGHT] = e.jaxis.value; break;
	}
}

void jbuttonevent(SDL_Event e)
{
	// I can uncomment this if I want to interpret the triggers as analogue triggers.
	// if (e.jbutton.type == SDL_JOYBUTTONDOWN) {
	// 	switch (e.jbutton.button) {
	// 	case 8: axes[TRIGGERLEFT] = 32768; break;
	// 	case 9: axes[TRIGGERRIGHT] = 32768; break;
	// 	}
	// } else if (e.jbutton.type == SDL_JOYBUTTONUP) {
	// 	switch (e.jbutton.button) {
	// 	case 8: axes[TRIGGERLEFT] = 0; break;
	// 	case 9: axes[TRIGGERRIGHT] = 0; break;
	// 	}
	// }

	int button = e.jbutton.button;
	if (0 <= button && button < LENGTH(nes30_buttons)) {
		if (e.jbutton.type == SDL_JOYBUTTONDOWN)
			nes30_buttons[button] = true;
		else if (e.jbutton.type == SDL_JOYBUTTONUP)
			nes30_buttons[button] = false;
	} else {
		printf("Button %d not supported.", button);
	}
}

struct controller_axis_input controller_input_apply_threshold(struct controller_axis_input input, float threshold)
{
	input.leftx = fabs(input.leftx) > threshold ? input.leftx : 0;
	input.lefty = fabs(input.lefty) > threshold ? input.lefty : 0;
	input.rightx = fabs(input.rightx) > threshold ? input.rightx : 0;
	input.righty = fabs(input.righty) > threshold ? input.righty : 0;
	return input;
}

void mousewheelevent(SDL_Event e)
{
	//Huge hack. Just add all scroll amounts within a frame, and reset on update.
	Sint32 prev_x = input_mouse_wheel_sum.wheel.x, prev_y = input_mouse_wheel_sum.wheel.y;
	input_mouse_wheel_sum = e;
	input_mouse_wheel_sum.wheel.x += prev_x;
	input_mouse_wheel_sum.wheel.y += prev_y;
}

void mousewheelreset()
{
	input_mouse_wheel_sum.wheel.x = 0;
	input_mouse_wheel_sum.wheel.y = 0;
}

//A key is "pressed" if this is the first frame in which it is down.
//A key is "held" if it is down, and it was down the previous frame.
// key held == ! key pressed
bool key_pressed(SDL_Scancode s)
{
	return key_state[s] && !key_state_prev[s];
}

bool key_down(SDL_Scancode s)
{
	return key_state[s];
}

uint8_t mouse_button_pressed(int *x, int *y)
{
	//Does not necessarily return the most recent mouse state, for intra-frame consistency
	SDL_GetMouseState(x, y);
	return mouse_state & ~mouse_state_prev;
}

