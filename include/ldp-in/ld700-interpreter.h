#ifndef LDP_IN_LD700_INTERPRETER_H
#define LDP_IN_LD700_INTERPRETER_H

#include "datatypes.h"

#ifdef __cplusplus
extern "C"
{
#endif // C++

typedef enum
{
	LD700_FALSE = 0,
	LD700_TRUE = 1
} LD700_BOOL;

typedef enum
{
	LD700_ERROR, LD700_SEARCHING, LD700_STOPPED, LD700_PLAYING, LD700_PAUSED, LD700_SPINNING_UP, LD700_TRAY_EJECTED
 }
LD700Status_t;

// resets all globals to default state
void ld700i_reset();

// Indicates that the 'leader' (low for 8 ms, high for 4 ms) has been seen and to expect a new 4-byte command.
// Usage of this is optional as the interpreter can still work without it, but can recover from invalid input faster if it is implemented.
void ld700i_on_new_cmd();

// sends control byte (for example, 0xA8)
void ld700i_write(uint8_t u8Cmd, LD700Status_t status);

// call for every vblank (helps us with timing)
void ld700i_on_vblank(LD700Status_t status);

///////////////////////////////////////////////////////////////////////////////

// CALLBACKS

// plays the laserdisc (if tray is ejected, this loads the disc and spins up.  If disc is stopped, this spins up the disc)
extern void (*g_ld700i_play)();

// pauses the laserdisc
extern void (*g_ld700i_pause)();

// stops the laserdisc
extern void (*g_ld700i_stop)();

// ejects the laserdisc
extern void (*g_ld700i_eject)();

// steps forward or backward
extern void (*g_ld700i_step)(LD700_BOOL bBackward);

// begins searching to a frame (non-blocking, should return immediately).
extern void (*g_ld700i_begin_search)(uint32_t uFrameNumber);

// enables/disables left/right audio channels
extern void (*g_ld700i_change_audio)(LD700_BOOL bEnableLeft, LD700_BOOL bEnableRight);

// enables/disables audio squelch
extern void (*g_ld700i_change_audio_squelch)(LD700_BOOL bSquelched);

// retrieves the current picture number (only valid if disc is playing or paused)
extern uint32_t (*g_ld700i_get_current_picnum)();

// will get called every time the EXT ACK line changes. This line is active low, so bActive=true means the line has gone low.
extern void (*g_ld700i_on_ext_ack_changed)(LD700_BOOL bActive);

typedef enum
{
	LD700_ERR_UNKNOWN_CMD_BYTE,
	LD700_ERR_UNSUPPORTED_CMD_BYTE,
	LD700_ERR_TOO_MANY_DIGITS,
	LD700_ERR_UNHANDLED_SITUATION,
	LD700_ERR_CORRUPT_INPUT
} LD700ErrCode_t;

// gets called by interpreter on error.  the code is an enum while the value relates to the error code (for example the value could be an unknown command byte)
extern void (*g_ld700i_error)(LD700ErrCode_t code, uint8_t u8Val);

// end callbacks

#ifdef __cplusplus
}
#endif // c++

#endif //LDP_IN_LD700_INTERPRETER_H
