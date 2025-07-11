#include <ldp-in/ld700-interpreter.h>

/*
 * NOTES about how the real LD-700 behaves that we may or may not include in this interpreter.
 *
 * - If the tray is closed but no disc is present (something we probably will never emulate), when the play command is sent,
 *		after 11ms from receiving the play command, the LD-700 will respond with a single (~16ms) pulse low of the ACK line.
 * - If a seek command is received while the player is currently seeking, the player will start seeking to the newly requested frame (ACK' will stay low).
 * - Sending the following commands (4A,4A,[corrupt],4A) with 20ms intervals keeps EXT_ACK' low throughout.
 *		Adding two consecutive corrupted commands causes EXT_ACK' to go high when the next command starts (after the two corrupted commands).
 *		Having 20ms between or 80ms between commands doesn't seem to make a difference.
 *		Trying to emulate this exact behavior is probably not worth the effort, so I'm just making a note of it here.
 * - If a play command is sent before a seek finishes, the play command is queued and executes once the seek completes.
 * - If a pause command is sent while the disc is spinning up, it is ignored and the disc plays once spin-up finishes.
 */

// callbacks, must be assigned before calling any other function in this interpreter
void (*g_ld700i_play)() = 0;
void (*g_ld700i_pause)() = 0;
void (*g_ld700i_stop)() = 0;
void (*g_ld700i_eject)() = 0;
void (*g_ld700i_step)(LD700_BOOL bBackward) = 0;
void (*g_ld700i_begin_search)(uint32_t uFrameNumber) = 0;
void (*g_ld700i_change_audio)(LD700_BOOL bEnableLeft, LD700_BOOL bEnableRight) = 0;
void (*g_ld700i_change_audio_squelch)(LD700_BOOL bSquelched) = 0;
void (*g_ld700i_error)(LD700ErrCode_t code, uint8_t u8Val) = 0;
uint32_t (*g_ld700i_get_current_picnum)() = 0;
void (*g_ld700i_on_ext_ack_changed)(LD700_BOOL bActive) = 0;

/////////////////////////

// so we can decode incoming commands properly
typedef enum
{
	LD700I_CMD_PREFIX,
	LD700I_CMD_PREFIX_XOR,
	LD700I_CMD,
	LD700I_CMD_XOR,
} LD700CmdState_t;

LD700CmdState_t g_ld700i_cmd_state = LD700I_CMD_PREFIX;

typedef enum
{
	LD700I_STATE_NORMAL,
	LD700I_STATE_FRAME	// in the middle of receiving a frame number
} LD700State_t;

LD700State_t g_ld700i_state;
uint8_t g_ld700i_numBuf[5];	// the number buffer can wraparound to the beginning of it gets too many digits
uint8_t *g_ld700i_pNumBufStart = 0;
uint8_t *g_ld700i_pNumBufEnd = 0;
const uint8_t *g_ld700i_pNumBufLastGoodPtr = (g_ld700i_numBuf + sizeof(g_ld700i_numBuf) - 1);
uint8_t g_ld700i_u8NumBufCount = 0;	// this shows how many bytes are in the number buffer
#define NUM_BUF_WRAP(var) 	if (var > g_ld700i_pNumBufLastGoodPtr) var = g_ld700i_numBuf;

uint8_t g_ld700i_u8CmdTimeoutVsyncCounter;	// to detect duplicate commands to be dropped
LD700_BOOL g_ld700i_bNewCmdReceived;	// whether we've received a new command (as opposed to a dupe)
LD700_BOOL g_ld700i_bExtAckActive;
LD700_BOOL g_ld700i_bNumBufResetArmed;	// whether we will clear the num buf if any digit is received
uint8_t g_ld700i_u8QueuedCmd;
uint8_t g_ld700i_u8LastCmd;	// to drop rapidly repeated commands
LD700_BOOL g_ld700i_bEscapedActive;	// whether we are in the middle of an escaped command

//////////////////////////////////////////////

// call every time you want EXT_ACK' to be a certain value.  The method will track if it's changed and trigger the callback if needed.
void ld700i_change_ext_ack(LD700_BOOL bActive)
{
	// callback gets called every time this value changes
	if (g_ld700i_bExtAckActive != bActive)
	{
		g_ld700i_on_ext_ack_changed(bActive);
		g_ld700i_bExtAckActive = bActive;
	}
}

void ld700i_reset()
{
	g_ld700i_pNumBufStart = g_ld700i_numBuf;
	g_ld700i_pNumBufEnd = g_ld700i_numBuf;
	g_ld700i_u8NumBufCount = 0;
	g_ld700i_u8CmdTimeoutVsyncCounter = 0;
	g_ld700i_bNewCmdReceived = LD700_FALSE;

	// force callback to be called so our implementor will be in the proper initial state
	g_ld700i_bExtAckActive = LD700_TRUE;
	ld700i_change_ext_ack(LD700_FALSE);

	g_ld700i_bNumBufResetArmed = LD700_FALSE;
	g_ld700i_cmd_state = LD700I_CMD_PREFIX;
	g_ld700i_u8QueuedCmd = 0;	// apparently is not needed
	g_ld700i_u8LastCmd = 0xFF;
	g_ld700i_state = LD700I_STATE_NORMAL;
	g_ld700i_bEscapedActive = LD700_FALSE;
}

void ld700i_add_digit(uint8_t u8Digit)
{
	// the player will remember the previous frame and will only erase it once a digit is entered.
	if (g_ld700i_bNumBufResetArmed)
	{
		// erase anything that was in the buffer
		g_ld700i_pNumBufStart = g_ld700i_pNumBufEnd;
		g_ld700i_u8NumBufCount = 0;
		g_ld700i_bNumBufResetArmed = LD700_FALSE;
	}

	*g_ld700i_pNumBufEnd = u8Digit;
	g_ld700i_pNumBufEnd++;
	NUM_BUF_WRAP(g_ld700i_pNumBufEnd);
	g_ld700i_u8NumBufCount++;

	// if they enter too many digits, we start dropping digits from the beginning
	if (g_ld700i_u8NumBufCount > 5)
	{
		g_ld700i_pNumBufStart++;
		NUM_BUF_WRAP(g_ld700i_pNumBufStart);
		g_ld700i_u8NumBufCount = 5;
	}
}

void ld700i_cmd_error(uint8_t u8Cmd)
{
	g_ld700i_error(LD700_ERR_UNKNOWN_CMD_BYTE, u8Cmd);
	g_ld700i_cmd_state = LD700I_CMD_PREFIX;
}

void ld700i_clear()
{
	g_ld700i_bEscapedActive = LD700_FALSE;

	if (g_ld700i_state == LD700I_STATE_FRAME)
	{
		// if user has started entering in a number, we clear it but stay in 'retrieve number' mode
		if (g_ld700i_u8NumBufCount != 0)
		{
			g_ld700i_pNumBufStart = g_ld700i_pNumBufEnd;
			g_ld700i_u8NumBufCount = 0;
		}
		// else we leave 'entering in a number' mode
		else
		{
			g_ld700i_state = LD700I_STATE_NORMAL;
		}
	}
	// else nothing to clear
}

void ld700i_on_new_cmd()
{
	g_ld700i_cmd_state = LD700I_CMD_PREFIX;
}

// 0 means something else so we need another distinct value to indicate no change
#define NO_CHANGE 0xFF

void ld700i_write(uint8_t u8Cmd, const LD700Status_t status)
{
	uint8_t u8NewCmdTimeoutVsyncCounter = 4;	// default value

	switch (g_ld700i_cmd_state)
	{
	case LD700I_CMD_PREFIX:
		if (u8Cmd == 0xA8) g_ld700i_cmd_state++;
		else ld700i_cmd_error(u8Cmd);
		break;
	case LD700I_CMD_PREFIX_XOR:
		if (u8Cmd == 0x57) g_ld700i_cmd_state++;
		else ld700i_cmd_error(u8Cmd);
		break;
	case LD700I_CMD:
		g_ld700i_u8QueuedCmd = u8Cmd;
		g_ld700i_cmd_state++;
		break;
	default:	// LD700I_CMD_XOR
		g_ld700i_cmd_state = LD700I_CMD_PREFIX;
		if (u8Cmd != (g_ld700i_u8QueuedCmd ^ 0xFF))
		{
			ld700i_cmd_error(u8Cmd);
			return;
		}
		break;
	}

	if (g_ld700i_cmd_state != LD700I_CMD_PREFIX)
	{
		// if new commands come in while our cmd timeout counter is not 0, then we need to hold EXT_ACK' active to properly detect a held remote control button
		if (g_ld700i_u8CmdTimeoutVsyncCounter != 0) goto done;
		return;
	}

	// rapidly repeated commands are dropped
	// (a human pressing keys on a remote control will cause commands to rapidly repeat)
	if ((g_ld700i_u8QueuedCmd == g_ld700i_u8LastCmd) && (g_ld700i_u8CmdTimeoutVsyncCounter != 0))
	{
		goto done;
	}

	g_ld700i_bNewCmdReceived = LD700_TRUE;

	// if we're receiving a normal command
	if (!g_ld700i_bEscapedActive)
	{
		switch (g_ld700i_u8QueuedCmd)
		{
		default:	// unknown
			g_ld700i_error(LD700_ERR_UNKNOWN_CMD_BYTE, g_ld700i_u8QueuedCmd);
			break;
		case 0x0:	// 0
		case 0x1:
		case 0x2:
		case 0x3:
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
		case 0x8:
		case 0x9:	// 9
			// digits are ignored if disc is stopped
			if (status != LD700_STOPPED)
			{
				ld700i_add_digit(g_ld700i_u8QueuedCmd);
			}
			else
			{
				u8NewCmdTimeoutVsyncCounter = NO_CHANGE;
			}
			break;
		case 0x16: // reject
			if ((status == LD700_PLAYING) || (status == LD700_PAUSED))
			{
				g_ld700i_stop();
			}
			else if (status == LD700_STOPPED)
			{
				g_ld700i_eject();
			}
			else if (status == LD700_TRAY_EJECTED)
			{
				// do nothing because it's redundant to send an eject command when we're alrady ejected
			}
			else
			{
				g_ld700i_error(LD700_ERR_UNHANDLED_SITUATION, status);
			}
			u8NewCmdTimeoutVsyncCounter = NO_CHANGE;	// I've never seen this command respond with an ACK
			break;
		case 0x17:	// play
			g_ld700i_play();
			break;
		case 0x18:	// pause
			// If disc is already paused, a pause command will cause the screen to go blank briefly.
			// Halcyon exploits during the 'voice print' stage to indicate to the user to speak the same word again.
			// We simulate this by doing a search to the frame that we're already on.
			if (status == LD700_PAUSED)
			{
				uint32_t u32Frame = g_ld700i_get_current_picnum();
				g_ld700i_begin_search(u32Frame);
			}
			else
			{
				g_ld700i_pause();
			}
			break;
		case 0x41:	// prepare to enter frame number

			// frame number entry is ignored if disc is stopped
			if (status != LD700_STOPPED)
			{
				g_ld700i_state = LD700I_STATE_FRAME;

				// The player will remember the previous frame, but will erase it if a digit is entered.
				// This means 0x41 0x42 will seek to the previous frame.
				g_ld700i_bNumBufResetArmed = LD700_TRUE;
			}
			else
			{
				u8NewCmdTimeoutVsyncCounter = NO_CHANGE;
			}
			break;
		case 0x42:	// begin search
		{
			if ((g_ld700i_state == LD700I_STATE_FRAME) && (status != LD700_STOPPED))
			{
				uint32_t u32Frame = 0;
				uint8_t *bufStartTmp = g_ld700i_pNumBufStart;
				uint8_t u8NumBufCountTmp = g_ld700i_u8NumBufCount;
				while (u8NumBufCountTmp != 0)
				{
					uint8_t u8 = *bufStartTmp;
					u32Frame *= 10;
					u32Frame += u8;
					bufStartTmp++;
					NUM_BUF_WRAP(bufStartTmp);
					u8NumBufCountTmp--;
				}
				g_ld700i_state = LD700I_STATE_NORMAL;
				g_ld700i_begin_search(u32Frame);
			}
			// the original player does not ACK if not in 'enter number' mode or if disc is stopped
			else
			{
				u8NewCmdTimeoutVsyncCounter = NO_CHANGE;
			}
		}
			break;
		case 0x45:	// clear
			ld700i_clear();
			break;
		case 0x49:	// enable right
			g_ld700i_change_audio(LD700_FALSE, LD700_TRUE);
			break;
		case 0x4A:	// enable stereo
			if (status == LD700_TRAY_EJECTED)
			{
				u8NewCmdTimeoutVsyncCounter = NO_CHANGE;
			}

			g_ld700i_change_audio(LD700_TRUE, LD700_TRUE);
			break;
		case 0x4B:	// enable left
			g_ld700i_change_audio(LD700_TRUE, LD700_FALSE);
			break;
		case 0x50:	// step reverse
			g_ld700i_step(LD700_TRUE);
			break;
		case 0x54:	// step fwd
			g_ld700i_step(LD700_FALSE);
			break;
		case 0x5F:	// escape
			g_ld700i_bEscapedActive = LD700_TRUE;
			u8NewCmdTimeoutVsyncCounter = NO_CHANGE;
			break;
		}
	}
	// else we're receiving an escaped command
	else
	{
		// in most cases, after we process an escape command, we'll start processing normal commands again
		g_ld700i_bEscapedActive = LD700_FALSE;

		switch (g_ld700i_u8QueuedCmd)
		{
		default:	// unknown
			g_ld700i_error(LD700_ERR_UNKNOWN_CMD_BYTE, g_ld700i_u8QueuedCmd);
			break;
		case 0x02:	// disable video
		case 0x03:	// enable video
		case 0x06:	// disable character generator display
		case 0x07:	// enable character generator display
			// not supported, but we will control EXT_ACK'
			break;
		case 0x04:	// disable audio
			g_ld700i_change_audio_squelch(LD700_TRUE);
			break;
		case 0x05:	// enable audio
			g_ld700i_change_audio_squelch(LD700_FALSE);
			break;
		case 0x45:	// clear
			ld700i_clear();
			break;
		case 0x5F:	// repeated escapes are ignored (confirmed on real hardware)
			u8NewCmdTimeoutVsyncCounter = NO_CHANGE;	// observed on real hardware, escapes by themselves do not cause ACK'
			g_ld700i_bEscapedActive = LD700_TRUE;
			break;
		}
	}

	// to detect duplicates
	g_ld700i_u8LastCmd = g_ld700i_u8QueuedCmd;

done:
	// if this value has been set, then we replace the global value
	if (u8NewCmdTimeoutVsyncCounter != NO_CHANGE)
	{
		g_ld700i_u8CmdTimeoutVsyncCounter = u8NewCmdTimeoutVsyncCounter;
	}
}

void ld700i_on_vblank(const LD700Status_t stat)
{
	LD700_BOOL bExtAckEnabled = (g_ld700i_u8CmdTimeoutVsyncCounter != 0);

	// when new command comes in, EXT_ACK' pulses high for 1 vsync (overriding other behavior)
	if (g_ld700i_bNewCmdReceived)
	{
		g_ld700i_bNewCmdReceived = LD700_FALSE;
		bExtAckEnabled = LD700_FALSE;
	}

	// searching/spinning-up trumps all
	bExtAckEnabled |= ((stat == LD700_SEARCHING) || (stat == LD700_SPINNING_UP));

	// EXT_ACK' is active after a command has been received or if the disc is searching/spinning-up
	ld700i_change_ext_ack(bExtAckEnabled);

	if (g_ld700i_u8CmdTimeoutVsyncCounter != 0)
	{
		g_ld700i_u8CmdTimeoutVsyncCounter--;
	}
}
