#ifndef INPUT_EVENT_H
#define INPUT_EVENT_H

#include <SDL2/SDL.h>
#include <stdbool.h>

extern const Uint8 *key_state;

enum {
	LEFTX,
	LEFTY,
	RIGHTX,
	RIGHTY,
	TRIGGERLEFT,
	TRIGGERRIGHT,
	NUM_HANDLED_AXES
};

enum nes30_button {
	INPUT_BUTTON_A = 0,
	INPUT_BUTTON_B = 1,
	INPUT_BUTTON_X = 3,
	INPUT_BUTTON_Y = 4,
	INPUT_BUTTON_L1 = 6,
	INPUT_BUTTON_R1 = 7,
	INPUT_BUTTON_L2 = 8,
	INPUT_BUTTON_R2 = 9,
	INPUT_BUTTON_SELECT = 10,
	INPUT_BUTTON_START  = 11,
	INPUT_BUTTON_LSTICK = 13,
	INPUT_BUTTON_RSTICK = 14
};

extern Sint16 axes[NUM_HANDLED_AXES];
extern bool nes30_buttons[16];

void input_event_init();
void input_event_device_arrival(int which);
void caxisevent(SDL_Event e);
void jaxisevent(SDL_Event e);
void jbuttonevent(SDL_Event e);
void keyevent(SDL_Keysym keysym, SDL_EventType type);

#endif