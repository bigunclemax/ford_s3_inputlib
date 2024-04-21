#ifndef SDL_h_
#define SDL_h_

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_oldnames.h>

#define SDL_free free

extern int SDL_Init(Uint32 flags);
extern Uint32 SDL_WasInit(Uint32 flags);
extern void SDL_QuitSubSystem(Uint32 flags);

extern void SDL_StartTextInput(void);

#endif /* SDL_h_ */
