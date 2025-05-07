#include "stdafx.h"
#include "ld700_test_interface.h"

class ld700_test_wrapper
{
public:
	static void play() { m_pInstance->Play(); }
	static void pause() { m_pInstance->Pause(); }
	static void step(LD700_BOOL bStepBackward) { m_pInstance->Step(bStepBackward); }
	static void stop() { m_pInstance->Stop(); }
	static void eject() { m_pInstance->Eject(); }
	static void begin_search(uint32_t u32FrameNum) { m_pInstance->BeginSearch(u32FrameNum); }
	static void on_ext_ack_changed(LD700_BOOL bActive) { m_pInstance->OnExtAckChanged(bActive); }
	static void on_error(LD700ErrCode_t err, uint8_t val) { m_pInstance->OnError(err, val); }
	static void change_audio(LD700_BOOL bEnableLeft, LD700_BOOL bEnableRight) { m_pInstance->ChangeAudio(bEnableLeft, bEnableRight); }
	static void change_audio_squelch(LD700_BOOL bSquelched) { m_pInstance->ChangeAudioSquelch(bSquelched); }

	static void setup(ILD700Test *pInstance)
	{
		m_pInstance = pInstance;
		g_ld700i_play = play;
		g_ld700i_pause = pause;
		g_ld700i_step = step;
		g_ld700i_stop = stop;
		g_ld700i_eject = eject;
		g_ld700i_begin_search = begin_search;
		g_ld700i_on_ext_ack_changed = on_ext_ack_changed;
		g_ld700i_error = on_error;
		g_ld700i_change_audio = change_audio;
		g_ld700i_change_audio_squelch = change_audio_squelch;
	}

private:
	static ILD700Test *m_pInstance;
};

ILD700Test *ld700_test_wrapper::m_pInstance = nullptr;

class LD700Tests : public ::testing::Test
{
public:
	void SetUp() override
	{
		ld700_test_wrapper::setup(&mockLD700);

		EXPECT_CALL(mockLD700, OnExtAckChanged(LD700_FALSE)).Times(1);

		ld700i_reset();
	}

	void ld700_write_helper(uint8_t u8Cmd)
	{
		// prefix
		ld700i_write(0xA8, m_curStatus);
		ld700i_write(0xA8 ^ 0xFF, m_curStatus);

		ld700i_write(u8Cmd, m_curStatus);
		ld700i_write(u8Cmd ^ 0xFF, m_curStatus);
	}

	// when the number of vblanks doesn't matter that much and 1 is as good as any
	void ld700_write_helper_with_vblank(LD700_BOOL bExtAckToActivate, uint8_t u8Cmd)
	{
		ld700_write_helper(u8Cmd);

		// on the next vblank, ExtAck should change
		EXPECT_CALL(mockLD700, OnExtAckChanged(bExtAckToActivate)).Times(1);
		ld700i_on_vblank(m_curStatus);
	}

	// when status change is deferred by 1 vblank
	void ld700_write_helper_with_2_vblanks(LD700_BOOL bExtAckToActivate, uint8_t u8Cmd)
	{
		ld700_write_helper(u8Cmd);
		ld700i_on_vblank(m_curStatus);

		// on the next vblank, ExtAck should change
		EXPECT_CALL(mockLD700, OnExtAckChanged(bExtAckToActivate)).Times(1);
		ld700i_on_vblank(m_curStatus);
	}

	// simulate vblanks firing in the middle of sending data
	void ld700_write_helper_with_vblanks(LD700_BOOL bExtAckToActivate, size_t uVblankCountBeforeChange, uint8_t u8Cmd)
	{
		// the way our interpreter is currently written, there's no change in behavior whether the vblanks come between each write or all at once,
		//  so for clarity, I'm sending them all at once
		for (int i = 0; i < uVblankCountBeforeChange - 1; i++)
		{
			ld700i_on_vblank(m_curStatus);
		}
		ld700_write_helper_with_vblank(bExtAckToActivate, u8Cmd);
	}

	// wait N number of vblanks, where EXT_ACK' changes on the last vblank
	void wait_vblanks_for_ext_ack_change(LD700_BOOL bExtAckToActivate, size_t uVblankCount)
	{
		for (int i = 0; i < uVblankCount - 1; i++)
		{
			ld700i_on_vblank(m_curStatus);
		}
		EXPECT_CALL(mockLD700, OnExtAckChanged(bExtAckToActivate)).Times(1);
		ld700i_on_vblank(m_curStatus);
	}

protected:
	MockLD700Test mockLD700;

	LD700Status_t m_curStatus = LD700_ERROR;
};

////////////////////////////////////////////////////

TEST_F(LD700Tests, cmd_pattern1)
{
	EXPECT_CALL(mockLD700, OnError(_, _)).Times(3);

	// prefix
	ld700i_write(0xA8 + 1, m_curStatus);	// error

	// implicitly we are testing that the next byte should be an 0xA8 when an error is received

	ld700i_write(0xA8, m_curStatus);
	ld700i_write(0xA9 ^ 0xFF, m_curStatus);	// error

	ld700i_write(0xA8, m_curStatus);
	ld700i_write(0xA8 ^ 0xFF, m_curStatus);

	ld700i_write(0, m_curStatus);	// arbitrary
	ld700i_write(0, m_curStatus);	// error because it's not 0 XOR FF
}

TEST_F(LD700Tests, recovery_using_leader)
{
	ld700i_write(0xA8, m_curStatus);	// incomplete command

	EXPECT_CALL(mockLD700, ChangeAudioSquelch(LD700_TRUE));
	ld700i_on_new_cmd();
	ld700_write_helper(0x5F);
	ld700_write_helper(4);

	ld700i_write(0xA8, m_curStatus);
	ld700i_write(0xA8 ^ 0xFF, m_curStatus);
	ld700i_write(0, m_curStatus);	// incomplete command

	EXPECT_CALL(mockLD700, ChangeAudioSquelch(LD700_TRUE));
	ld700i_on_new_cmd();
	ld700_write_helper(0x5F);
	ld700_write_helper(4);
}

TEST_F(LD700Tests, reject_from_playing)
{
	m_curStatus = LD700_PLAYING;
	EXPECT_CALL(mockLD700, Stop());
	EXPECT_CALL(mockLD700, OnError(_, _)).Times(0);
	EXPECT_CALL(mockLD700, OnExtAckChanged(_)).Times(0);	// reject should not ACK

	ld700_write_helper(0x16);	// reject (to stop playing)
	ld700i_on_vblank(m_curStatus);
}

TEST_F(LD700Tests, reject_from_stopped)
{
	m_curStatus = LD700_STOPPED;
	EXPECT_CALL(mockLD700, Eject());
	EXPECT_CALL(mockLD700, OnError(_, _)).Times(0);
	EXPECT_CALL(mockLD700, OnExtAckChanged(_)).Times(0);	// reject should not ACK

	ld700_write_helper(0x16);	// reject (to stop playing)
	ld700i_on_vblank(m_curStatus);
}

TEST_F(LD700Tests, reject_when_already_ejected)
{
	m_curStatus = LD700_TRAY_EJECTED;
	EXPECT_CALL(mockLD700, Eject()).Times(0);
	EXPECT_CALL(mockLD700, OnError(_, _)).Times(0);
	EXPECT_CALL(mockLD700, OnExtAckChanged(_)).Times(0);	// reject should not ACK

	ld700_write_helper(0x16);	// reject (to stop playing)
	ld700i_on_vblank(m_curStatus);
}

TEST_F(LD700Tests, playing)
{
	EXPECT_CALL(mockLD700, Play());
	EXPECT_CALL(mockLD700, OnError(_, _)).Times(0);
	m_curStatus = LD700_PAUSED;

	// Observed on logic analyzer capture from real hardware:
	// About 3ms after the play command is finished transmitting, EXT_ACK' enables and stays enabled for about 49ms (about 3 vsync pulses)
	// The delay between the command ending and EXT_ACK' activating seems variable (ie it's not always 3ms)
	ld700_write_helper_with_2_vblanks(LD700_TRUE, 0x17);

	// to make it easy to troubleshoot
	ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mockLD700));

	wait_vblanks_for_ext_ack_change(LD700_FALSE, 3);
}

TEST_F(LD700Tests, pause)
{
	EXPECT_CALL(mockLD700, Pause());
	EXPECT_CALL(mockLD700, OnError(_, _)).Times(0);
	m_curStatus = LD700_PLAYING;

	ld700_write_helper_with_2_vblanks(LD700_TRUE, 0x18);
	wait_vblanks_for_ext_ack_change(LD700_FALSE, 3);
}

TEST_F(LD700Tests, step_rev)
{
	EXPECT_CALL(mockLD700, Step(LD700_TRUE));
	EXPECT_CALL(mockLD700, OnError(_, _)).Times(0);
	m_curStatus = LD700_PLAYING;

	ld700_write_helper_with_2_vblanks(LD700_TRUE, 0x50);
	wait_vblanks_for_ext_ack_change(LD700_FALSE, 3);
}

TEST_F(LD700Tests, step_fwd)
{
	EXPECT_CALL(mockLD700, Step(LD700_FALSE));
	EXPECT_CALL(mockLD700, OnError(_, _)).Times(0);
	m_curStatus = LD700_PLAYING;

	ld700_write_helper_with_2_vblanks(LD700_TRUE, 0x54);
	wait_vblanks_for_ext_ack_change(LD700_FALSE, 3);
}

TEST_F(LD700Tests, search)
{
	// This test ignores vblank and is more basic.
	// We have other tests that tests searching using vblank.

	EXPECT_CALL(mockLD700, BeginSearch(12345));
	EXPECT_CALL(mockLD700, OnError(_, _)).Times(0);

	// frame/time
	ld700_write_helper(0x41);

	// frame 12345
	ld700_write_helper(1);
	ld700_write_helper(2);
	ld700_write_helper(3);
	ld700_write_helper(4);
	ld700_write_helper(5);

	// begin search
	ld700_write_helper(0x42);
}

TEST_F(LD700Tests, search_edge_case)
{
	// This test ignores vblank and is more basic.
	// We have other tests that tests searching using vblank.

	EXPECT_CALL(mockLD700, BeginSearch(2345)).Times(2);
	EXPECT_CALL(mockLD700, BeginSearch(6)).Times(1);
	EXPECT_CALL(mockLD700, OnError(_, _)).Times(0);

	ld700_write_helper(1);	// this should get dropped

	// frame/time
	ld700_write_helper(0x41);

	// frame 2345
	ld700_write_helper(2);
	ld700_write_helper(3);
	ld700_write_helper(4);
	ld700_write_helper(5);

	// begin search
	ld700_write_helper(0x42);

	// frame/time
	ld700_write_helper(0x41);

	// by not entering a frame number, it will remember and search to the previous frame

	// begin search
	ld700_write_helper(0x42);

	// seek to frame 6 (should clear out previous frame)
	ld700_write_helper(0x41);
	ld700_write_helper(6);
	ld700_write_helper(0x42);
}

TEST_F(LD700Tests, search_after_disc_flip)
{
	EXPECT_CALL(mockLD700, ChangeAudio(LD700_TRUE, LD700_TRUE));
	m_curStatus = LD700_PAUSED;

	// NOTE: For this logic analyzer capture, commands are spaced out enough that EXT_ACK' deactivates before next command

	// a lot of Halcyon search commands start with this for some reason
	ld700_write_helper_with_2_vblanks(LD700_TRUE, 0x4A);
	wait_vblanks_for_ext_ack_change(LD700_FALSE, 3);		// logic analyzer capture shows EXT_ACK' lasting 3 vblanks if disc is paused and 4A command is sent

	// frame/time
	ld700_write_helper_with_2_vblanks(LD700_TRUE, 0x41);
	wait_vblanks_for_ext_ack_change(LD700_FALSE, 3);

	ld700_write_helper_with_2_vblanks(LD700_TRUE, 0);
	wait_vblanks_for_ext_ack_change(LD700_FALSE, 3);

	ld700_write_helper_with_2_vblanks(LD700_TRUE, 1);
	wait_vblanks_for_ext_ack_change(LD700_FALSE, 3);

	ld700_write_helper_with_2_vblanks(LD700_TRUE, 7);
	wait_vblanks_for_ext_ack_change(LD700_FALSE, 3);

	ld700_write_helper_with_2_vblanks(LD700_TRUE, 2);
	wait_vblanks_for_ext_ack_change(LD700_FALSE, 3);

	ld700_write_helper_with_2_vblanks(LD700_TRUE, 1);
	wait_vblanks_for_ext_ack_change(LD700_FALSE, 3);

	EXPECT_CALL(mockLD700, BeginSearch(1721));
	ld700_write_helper_with_2_vblanks(LD700_TRUE, 0x42);
}

TEST_F(LD700Tests, search_with_mixed_audio_squelch)
{
	m_curStatus = LD700_PAUSED;

	// NOTE: For simplicity, commands are spaced out enough that EXT_ACK' deactivates before next command

	// frame/time
	ld700_write_helper_with_2_vblanks(LD700_TRUE, 0x41);
	wait_vblanks_for_ext_ack_change(LD700_FALSE, 3);

	ld700_write_helper_with_2_vblanks(LD700_TRUE, 1);
	wait_vblanks_for_ext_ack_change(LD700_FALSE, 3);

	ld700_write_helper_with_2_vblanks(LD700_TRUE, 2);
	wait_vblanks_for_ext_ack_change(LD700_FALSE, 3);

	ld700_write_helper_with_2_vblanks(LD700_TRUE, 3);
	wait_vblanks_for_ext_ack_change(LD700_FALSE, 3);

	ld700_write_helper_with_2_vblanks(LD700_TRUE, 4);
	wait_vblanks_for_ext_ack_change(LD700_FALSE, 3);

	ld700_write_helper(0x5F);	// sending ESCAPE does not cause ACK' to go low
	ld700i_on_vblank(m_curStatus);	// to match the surrounding commands that wait for 2 vblanks
	ld700i_on_vblank(m_curStatus);

	EXPECT_CALL(mockLD700, ChangeAudioSquelch(LD700_TRUE));
	ld700_write_helper_with_2_vblanks(LD700_TRUE, 4);	// audio squelch
	wait_vblanks_for_ext_ack_change(LD700_FALSE, 3);

	ld700_write_helper_with_2_vblanks(LD700_TRUE, 5);
	wait_vblanks_for_ext_ack_change(LD700_FALSE, 3);

	EXPECT_CALL(mockLD700, BeginSearch(12345));
	ld700_write_helper_with_2_vblanks(LD700_TRUE, 0x42);

	// we don't expect ACK' to go high again
	m_curStatus = LD700_SEARCHING;
	ld700i_on_vblank(m_curStatus);
	ld700i_on_vblank(m_curStatus);
	ld700i_on_vblank(m_curStatus);
}

TEST_F(LD700Tests, search_with_double_clear)
{
	m_curStatus = LD700_PAUSED;

	// NOTE: For simplicity, commands are spaced out enough that EXT_ACK' deactivates before next command

	// frame/time
	ld700_write_helper_with_2_vblanks(LD700_TRUE, 0x41);
	wait_vblanks_for_ext_ack_change(LD700_FALSE, 3);

	ld700_write_helper_with_2_vblanks(LD700_TRUE, 1);
	wait_vblanks_for_ext_ack_change(LD700_FALSE, 3);

	ld700_write_helper_with_2_vblanks(LD700_TRUE, 0x45);	// clear	(resets current frame to 0)
	wait_vblanks_for_ext_ack_change(LD700_FALSE, 3);

	ld700_write_helper_with_2_vblanks(LD700_TRUE, 0x45);	// clear	(cancels 'enter a frame' mode)
	wait_vblanks_for_ext_ack_change(LD700_FALSE, 3);

	// we don't expect ACK' to go low again because 0x42 should not be accepted
	EXPECT_CALL(mockLD700, OnExtAckChanged(LD700_TRUE)).Times(0);

	ld700_write_helper(0x42);
	ld700i_on_vblank(m_curStatus);
	ld700i_on_vblank(m_curStatus);
	ld700i_on_vblank(m_curStatus);
	ld700i_on_vblank(m_curStatus);
}

TEST_F(LD700Tests, seek_edge_cases_digits)
{
	EXPECT_CALL(mockLD700, BeginSearch(34567));

	ld700_write_helper(1);
	ld700_write_helper(0x41);	// the 1 should be dropped
	ld700_write_helper(2);
	ld700_write_helper(3);
	ld700_write_helper(4);
	ld700_write_helper(5);
	ld700_write_helper(6);
	ld700_write_helper(7);	// an extra digit forces the first digit to be dropped
	ld700_write_helper(0x42);	// execute search
}

TEST_F(LD700Tests, seek_edge_cases_digit2)
{
	EXPECT_CALL(mockLD700, BeginSearch(34));

	ld700_write_helper(0x41);
	ld700_write_helper(1);
	ld700_write_helper(2);
	ld700_write_helper(0x41);	// '12' should be dropped
	ld700_write_helper(3);
	ld700_write_helper(4);
	ld700_write_helper(0x42);	// execute search
}

TEST_F(LD700Tests, repeated_command_ignored)
{
	m_curStatus = LD700_PAUSED;
	EXPECT_CALL(mockLD700, Play()).Times(1);
	EXPECT_CALL(mockLD700, OnError(_, _)).Times(0);

	ld700_write_helper(0x17);
	ld700i_on_vblank(m_curStatus);	// EXT_ACK' will briefly disable to indicate that a new command was received.  But it's already disabled, so we won't see that for this test.

	EXPECT_CALL(mockLD700, OnExtAckChanged(LD700_TRUE)).Times(1);
	ld700i_write(0xA8, m_curStatus);
	ld700i_on_vblank(m_curStatus);

	// at this point, EXT_ACK' should not change because we are repeating the command before it times out

	EXPECT_CALL(mockLD700, OnExtAckChanged(_)).Times(0);
	ld700i_write(0xA8 ^ 0xFF, m_curStatus);
	ld700i_on_vblank(m_curStatus);

	ld700i_write(0x17, m_curStatus);
	ld700i_on_vblank(m_curStatus);

	ld700i_write(0x17 ^ 0xFF, m_curStatus);
	ld700i_on_vblank(m_curStatus);

	// here is where EXT_ACK' should finally 'expire'
	wait_vblanks_for_ext_ack_change(LD700_FALSE, 4);	// 4 vblanks is approximately 60ms which is when the expiration takes place

	// to make it easy to troubleshoot
	ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mockLD700));
}

TEST_F(LD700Tests, repeated_command_accepted)
{
	EXPECT_CALL(mockLD700, Play()).Times(2);
	m_curStatus = LD700_PAUSED;	// we know how long EXT_ACK' lasts when play command is sent when disc is paused
	EXPECT_CALL(mockLD700, OnError(_, _)).Times(0);

	ld700_write_helper_with_2_vblanks(LD700_TRUE, 0x17);
	wait_vblanks_for_ext_ack_change(LD700_FALSE, 3);

	// this one should get accepted since we've waited long enough
	ld700_write_helper(0x17);
}

TEST_F(LD700Tests, tray_ejected)
{
	EXPECT_CALL(mockLD700, OnError(_, _)).Times(0);
	EXPECT_CALL(mockLD700, ChangeAudio(LD700_TRUE, LD700_TRUE));

	m_curStatus = LD700_TRAY_EJECTED;
	ld700_write_helper(0x4A);

	// EXT_ACK' should not enable because the tray is ejected
	ld700i_on_vblank(LD700_TRAY_EJECTED);
}

TEST_F(LD700Tests, enable_left)
{
	m_curStatus = LD700_PAUSED;
	EXPECT_CALL(mockLD700, ChangeAudio(LD700_TRUE, LD700_FALSE));
	ld700_write_helper_with_2_vblanks(LD700_TRUE, 0x4B);
	wait_vblanks_for_ext_ack_change(LD700_FALSE, 3);
}

TEST_F(LD700Tests, enable_right)
{
	m_curStatus = LD700_PAUSED;
	EXPECT_CALL(mockLD700, ChangeAudio(LD700_FALSE, LD700_TRUE));
	ld700_write_helper_with_2_vblanks(LD700_TRUE, 0x49);
	wait_vblanks_for_ext_ack_change(LD700_FALSE, 3);
}

TEST_F(LD700Tests, enable_text_overlay)
{
	m_curStatus = LD700_STOPPED;
	EXPECT_CALL(mockLD700, OnError(_, _)).Times(0);
	EXPECT_CALL(mockLD700, OnExtAckChanged(_)).Times(0);	// no ACK expected

	// make sure EXT_ACK' never changes
	ld700_write_helper(0x5F);	// escape
	ld700i_on_vblank(m_curStatus);
	ld700i_on_vblank(m_curStatus);
	ld700i_on_vblank(m_curStatus);
	ld700_write_helper(0x07);	// enable text overlay
	ld700i_on_vblank(m_curStatus);
	ld700i_on_vblank(m_curStatus);
	ld700i_on_vblank(m_curStatus);
}

TEST_F(LD700Tests, escape_by_itself_never_acks)
{
	m_curStatus = LD700_PLAYING;	// arbitrary
	EXPECT_CALL(mockLD700, OnError(_, _)).Times(0);
	EXPECT_CALL(mockLD700, OnExtAckChanged(_)).Times(0);	// escape by itself should never ACK

	// make sure EXT_ACK' never changes
	ld700_write_helper(0x5F);	// escape
	ld700i_on_vblank(m_curStatus);
	ld700i_on_vblank(m_curStatus);
	ld700i_on_vblank(m_curStatus);
	ld700_write_helper(0x5F);	// escape
	ld700i_on_vblank(m_curStatus);
	ld700i_on_vblank(m_curStatus);
	ld700i_on_vblank(m_curStatus);
}

TEST_F(LD700Tests, escape_has_no_timeout)
{
	m_curStatus = LD700_PLAYING;	// arbitrary
	EXPECT_CALL(mockLD700, OnError(_, _)).Times(0);
	EXPECT_CALL(mockLD700, ChangeAudioSquelch(LD700_TRUE)).Times(1);

	ld700_write_helper(0x5F);	// escape
	ld700i_on_vblank(m_curStatus);
	ld700i_on_vblank(m_curStatus);
	ld700i_on_vblank(m_curStatus);
	ld700i_on_vblank(m_curStatus);
	ld700i_on_vblank(m_curStatus);
	ld700i_on_vblank(m_curStatus);
	ld700i_on_vblank(m_curStatus);
	ld700i_on_vblank(m_curStatus);
	ld700i_on_vblank(m_curStatus);
	ld700i_on_vblank(m_curStatus);
	ld700i_on_vblank(m_curStatus);
	ld700i_on_vblank(m_curStatus);
	ld700i_on_vblank(m_curStatus);
	ld700i_on_vblank(m_curStatus);
	ld700i_on_vblank(m_curStatus);
	ld700_write_helper(0x4);	// audio squelch
}

TEST_F(LD700Tests, escape_gets_cleared)
{
	m_curStatus = LD700_PLAYING;	// arbitrary
	EXPECT_CALL(mockLD700, OnError(_, _)).Times(0);
	EXPECT_CALL(mockLD700, ChangeAudioSquelch(LD700_TRUE)).Times(0);

	ld700_write_helper(0x5F);	// escape
	ld700_write_helper(0x45);	// clear
	ld700_write_helper(0x4);	// audio squelch	(should not take effect since we cleared)
}

TEST_F(LD700Tests, clear_inside_search_with_escape)
{
	m_curStatus = LD700_PLAYING;	// arbitrary
	EXPECT_CALL(mockLD700, OnError(_, _)).Times(0);
	EXPECT_CALL(mockLD700, BeginSearch(5)).Times(1);

	ld700_write_helper(0x41);
	ld700_write_helper(0x01);
	ld700_write_helper(0x02);
	ld700_write_helper(0x03);
	ld700_write_helper(0x04);
	ld700_write_helper(0x5F);	// escape
	ld700_write_helper(0x45);	// clear
	ld700_write_helper(0x5);	// digit 5
	ld700_write_helper(0x42);
}

TEST_F(LD700Tests, boot1)
{
	EXPECT_CALL(mockLD700, OnError(_, _)).Times(0);
	EXPECT_CALL(mockLD700, ChangeAudio(LD700_TRUE, LD700_TRUE));
	m_curStatus = LD700_STOPPED;

	// EXT_ACK' should activate because tray is not ejected
	ld700_write_helper_with_2_vblanks(LD700_TRUE, 0x4A);

	// to make it easy to troubleshoot
	ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mockLD700));

	ld700_write_helper_with_vblanks(LD700_FALSE, 3, 0x5F);

	ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mockLD700));

	ld700_write_helper_with_2_vblanks(LD700_TRUE, 0x02);

	ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mockLD700));

	ld700_write_helper_with_vblanks(LD700_FALSE, 3, 0x5F);

	ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mockLD700));

	EXPECT_CALL(mockLD700, ChangeAudioSquelch(LD700_TRUE));
	ld700_write_helper_with_2_vblanks(LD700_TRUE, 0x04);
	wait_vblanks_for_ext_ack_change(LD700_FALSE, 3);

	ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mockLD700));

	EXPECT_CALL(mockLD700, Play());
	ld700_write_helper_with_2_vblanks(LD700_TRUE, 0x17);
}

TEST_F(LD700Tests, boot2)
{
	EXPECT_CALL(mockLD700, OnError(_, _)).Times(0);
	m_curStatus = LD700_PLAYING; // NOTE: For this test, the disc starts out playing (this is from a logic analyzer capture)

	// EXT_ACK' is already inactive and sending this command won't change that
	ld700_write_helper(0x5F);
	ld700_write_helper_with_2_vblanks(LD700_TRUE, 0x06);
	wait_vblanks_for_ext_ack_change(LD700_FALSE, 3);

	// to make it easy to troubleshoot
	ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mockLD700));

	EXPECT_CALL(mockLD700, Pause());
	ld700_write_helper_with_2_vblanks(LD700_TRUE, 0x18);
	wait_vblanks_for_ext_ack_change(LD700_FALSE, 3);

	ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mockLD700));

	// EXT_ACK' is already inactive and sending this command won't change that
	ld700_write_helper(0x5F);

	ld700_write_helper_with_2_vblanks(LD700_TRUE, 0x03);
	wait_vblanks_for_ext_ack_change(LD700_FALSE, 3);
	ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mockLD700));

	// EXT_ACK' is already inactive and sending this command won't change that
	ld700_write_helper(0x5F);

	EXPECT_CALL(mockLD700, ChangeAudioSquelch(LD700_FALSE));
	ld700_write_helper_with_2_vblanks(LD700_TRUE, 0x05);
	wait_vblanks_for_ext_ack_change(LD700_FALSE, 3);
	ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mockLD700));
}

TEST_F(LD700Tests, disc_searching)
{
	m_curStatus = LD700_SEARCHING;

	// EXT_ACK' should enable
	wait_vblanks_for_ext_ack_change(LD700_TRUE, 1);

	// make it easy to troubleshoot problems
	ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mockLD700));
}

TEST_F(LD700Tests, disc_spinning_up)
{
	m_curStatus = LD700_SPINNING_UP;

	// EXT_ACK' should enable
	wait_vblanks_for_ext_ack_change(LD700_TRUE, 1);

	// make it easy to troubleshoot problems
	ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mockLD700));

	// ACK' line should not change while disc is spinning up
	EXPECT_CALL(mockLD700, OnExtAckChanged(LD700_FALSE)).Times(0);

	EXPECT_CALL(mockLD700, ChangeAudio(LD700_TRUE, LD700_TRUE)).Times(1);

	ld700i_on_vblank(m_curStatus);

	ld700_write_helper(0x4A);
	ld700i_on_vblank(m_curStatus);

	// sending 4A during disc spin-up should not cause any change to ACK line
}
