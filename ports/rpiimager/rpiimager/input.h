#ifndef INPUT_H_
#define INPUT_H_

#include "types.h"
#include <SDL2/SDL.h>

/*
 * Logical buttons – abstracted over gamepad and keyboard.
 */
typedef enum {
    BTN_NONE = 0,
    BTN_UP,
    BTN_DOWN,
    BTN_LEFT,
    BTN_RIGHT,
    BTN_A,       /* Confirm / Select       */
    BTN_B,       /* Back / Cancel          */
    BTN_Y,       /* Aux (e.g. Refresh)     */
    BTN_START,   /* Quit / pause           */
} LogicalBtn;

/*
 * input_map_controller – translate an SDL_GameControllerButton to LogicalBtn.
 * Returns BTN_NONE if not mapped.
 */
LogicalBtn input_map_controller(SDL_GameControllerButton b);

/*
 * input_map_key – translate an SDL_Keycode to LogicalBtn.
 * Returns BTN_NONE if not mapped.
 */
LogicalBtn input_map_key(SDL_Keycode k);

/*
 * input_handle – process one logical button press and update G accordingly.
 */
void input_handle(LogicalBtn btn);

/*
 * input_open_controller – open the first available game controller.
 * Safe to call multiple times.
 */
void input_open_controller(int joystick_idx);

/*
 * input_close_controller – close when a controller is removed.
 * instance_id comes from SDL_ControllerDeviceEvent.which.
 */
void input_close_controller(SDL_JoystickID instance_id);

#endif /* INPUT_H_ */
