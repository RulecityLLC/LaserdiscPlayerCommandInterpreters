#include "stdafx.h"
#include "ldp1000_test_interface.h"

// since we will be making these calculations a lot
#define LATVAL_CLEAR	(LDP1000_LATENCY_CLEAR << 8)
#define	LATVAL_NUMBER	(LDP1000_LATENCY_NUMBER << 8)
#define LATVAL_ENTER	(LDP1000_LATENCY_ENTER << 8)
#define	LATVAL_PLAY		(LDP1000_LATENCY_PLAY << 8)
#define LATVAL_STILL	(LDP1000_LATENCY_STILL << 8)
#define LATVAL_INQUIRY	(LDP1000_LATENCY_INQUIRY << 8)
#define LATVAL_GENERIC	(LDP1000_LATENCY_GENERIC << 8)

#define LATACK_CLEAR	(LATVAL_CLEAR | 0x0A)
#define LATACK_NUMBER	(LATVAL_NUMBER | 0x0A)
#define LATACK_ENTER	(LATVAL_ENTER | 0x0A)
#define LATACK_PLAY		(LATVAL_PLAY | 0x0A)
#define LATACK_STILL	(LATVAL_STILL | 0x0A)
#define LATACK_INQUIRY	(LATVAL_INQUIRY | 0x0A)
#define LATACK_GENERIC	(LATVAL_GENERIC | 0x0A)
#define LATNAK_GENERIC	(LATVAL_GENERIC | 0x0B)

class ldp1000_test_wrapper
{
public:
	static void play(uint8_t u8Numerator, uint8_t u8Denominator, LDP1000_BOOL bBackward, LDP1000_BOOL bAudioSquelched) { m_pInstance->Play(u8Numerator, u8Denominator, (bBackward != LDP1000_FALSE), (bAudioSquelched != LDP1000_FALSE)); }
	static void pause() { m_pInstance->Pause(); }
	static void begin_search(uint32_t u32FrameNum) { m_pInstance->BeginSearch(u32FrameNum); }
	static void step_forward() { m_pInstance->StepForward(); }
	static void step_reverse() { m_pInstance->StepReverse(); }
	static void skip(int16_t iTracksToSkip) { m_pInstance->Skip(iTracksToSkip); }
	static void change_audio(uint8_t u8Channel, uint8_t u8Enabled) { m_pInstance->ChangeAudio(u8Channel, u8Enabled); }
	static void change_video(LDP1000_BOOL u8Enabled) { m_pInstance->ChangeVideo(u8Enabled != LDP1000_FALSE); }
	static LDP1000Status_t GetStatus() { return m_pInstance->GetStatus(); }
	static uint32_t get_cur_frame() { return m_pInstance->GetCurFrame(); }

	static void text_enable_changed(LDP1000_BOOL bEnabled) { m_pInstance->TextEnableChanged(bEnabled != LDP1000_FALSE); }
	static void text_buffer_contents_changed(const uint8_t *p8Buf32Bytes) { m_pInstance->TextBufferContentsChanged(p8Buf32Bytes); }
	static void text_buffer_start_index_changed(uint8_t u8StartIdx) { m_pInstance->TextBufferStartIndexChanged(u8StartIdx); }
	static void text_modes_changed(uint8_t u8Mode, uint8_t u8X, uint8_t u8Y) { m_pInstance->TextModesChanged(u8Mode, u8X, u8Y); }

	static void OnError(LDP1000ErrCode_t code, uint8_t u8Val) { m_pInstance->OnError(code, u8Val); }

	static void setup(ILDP1000Test *pInstance)
	{
		m_pInstance = pInstance;
		g_ldp1000i_play = play;
		g_ldp1000i_pause = pause;
		g_ldp1000i_begin_search = begin_search;
		g_ldp1000i_step_forward = step_forward;
		g_ldp1000i_step_reverse = step_reverse;
		g_ldp1000i_skip = skip;
		g_ldp1000i_change_audio = change_audio;
		g_ldp1000i_change_video = change_video;
		g_ldp1000i_get_status = GetStatus;
		g_ldp1000i_get_cur_frame_num = get_cur_frame;
		g_ldp1000i_text_enable_changed = text_enable_changed;
		g_ldp1000i_text_buffer_contents_changed = text_buffer_contents_changed;
		g_ldp1000i_text_buffer_start_index_changed = text_buffer_start_index_changed;
		g_ldp1000i_text_modes_changed = text_modes_changed;
		g_ldp1000i_error = OnError;
	}

private:
	static ILDP1000Test *m_pInstance;
};

ILDP1000Test *ldp1000_test_wrapper::m_pInstance = 0;

/////////////////////////////////////////////////////////////////

void test_ldp1000_playing()
{
	MockLDP1000Test mockLDP1000;

	ldp1000_test_wrapper::setup(&mockLDP1000);

	EXPECT_CALL(mockLDP1000, Play(1, 1, false, false));

	uint16_t u16;

	ldp1000i_reset(LDP1000_EMU_LDP1000A);
	ldp1000i_write(0x3A);	// send play

	LDP1000_BOOL b = ldp1000i_can_read();
	TEST_CHECK(b != 0);

	u16 = ldp1000i_read();
	TEST_CHECK_EQUAL(u16, LATACK_PLAY);
}

TEST_CASE(ldp1000_playing)
{
	test_ldp1000_playing();
}

void test_ldp1000_searching()
{
	MockLDP1000Test mockLDP1000;

	ldp1000_test_wrapper::setup(&mockLDP1000);

	EXPECT_CALL(mockLDP1000, Pause()).Times(3);
	EXPECT_CALL(mockLDP1000, BeginSearch(12345));
	EXPECT_CALL(mockLDP1000, BeginSearch(23456));
	EXPECT_CALL(mockLDP1000, GetStatus()).WillRepeatedly(Return(LDP1000_PAUSED));

	uint16_t u16;

	ldp1000i_reset(LDP1000_EMU_LDP1000A);
	ldp1000i_write(0x43);	// start search
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());
	ldp1000i_write('0');	// M1
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('2');	// M2
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('3');	// M3
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('4');	// M4
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('5');	// M5
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());

	ldp1000i_write(0x41);	// Clear Entry
	u16 = ldp1000i_read();
	TEST_REQUIRE_EQUAL(LATACK_GENERIC, u16);

	ldp1000i_write(0x43);	// start search
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());
	ldp1000i_write('1');	// M1
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('2');	// M2
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('3');	// M3
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('4');	// M4
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('5');	// M5
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());

	ldp1000i_write(0x40);	// enter
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());

	// let search complete
	ldp1000i_think_during_vblank();

	u16 = ldp1000i_read();
	TEST_CHECK_EQUAL(LATVAL_GENERIC | 1, u16);

	// do another search

	ldp1000i_write(0x43);	// start search
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());
	// make sure thinker doesn't throw an error while we are waiting for search digits
	ldp1000i_think_during_vblank();
	ldp1000i_write('2');	// M1
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('3');	// M2
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('4');	// M3
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('5');	// M4
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('6');	// M5
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());

	ldp1000i_write(0x40);	// enter
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());

	// let search complete
	ldp1000i_think_during_vblank();

	u16 = ldp1000i_read();
	TEST_CHECK_EQUAL(LATVAL_GENERIC | 1, u16);

}

TEST_CASE(ldp1000_searching)
{
	test_ldp1000_searching();
}

void test_ldp1000_pause()
{
	MockLDP1000Test mockLDP;

	ldp1000_test_wrapper::setup(&mockLDP);

	EXPECT_CALL(mockLDP, Pause());

	ldp1000i_reset(LDP1000_EMU_LDP1000A);
	ldp1000i_write(0x4F);	// pause
	TEST_CHECK_EQUAL(LATACK_STILL, ldp1000i_read());
}

TEST_CASE(ldp1000_pause)
{
	test_ldp1000_pause();
}

void test_ldp1000_enable_audio()
{
	MockLDP1000Test mockLDP;

	ldp1000_test_wrapper::setup(&mockLDP);

	EXPECT_CALL(mockLDP, ChangeAudio(0, 1));
	EXPECT_CALL(mockLDP, ChangeAudio(1, 1));
	EXPECT_CALL(mockLDP, ChangeAudio(0, 0));
	EXPECT_CALL(mockLDP, ChangeAudio(1, 0));

	ldp1000i_reset(LDP1000_EMU_LDP1000A);
	ldp1000i_write(0x46);	// enable left audio
	TEST_CHECK_EQUAL(LATACK_STILL, ldp1000i_read());

	ldp1000i_write(0x48);	// enable right audio
	TEST_CHECK_EQUAL(LATACK_STILL, ldp1000i_read());

	ldp1000i_write(0x47);	// disable left audio
	TEST_CHECK_EQUAL(LATACK_STILL, ldp1000i_read());

	ldp1000i_write(0x49);	// disable right audio
	TEST_CHECK_EQUAL(LATACK_STILL, ldp1000i_read());

}

TEST_CASE(ldp1000_enable_audio)
{
	test_ldp1000_enable_audio();
}

void test_ldp1000_get_cur_frame()
{
	MockLDP1000Test mockLDP;

	ldp1000_test_wrapper::setup(&mockLDP);

	EXPECT_CALL(mockLDP, GetCurFrame()).WillOnce(Return(12345)).WillOnce(Return(1));

	ldp1000i_reset(LDP1000_EMU_LDP1000A);
	ldp1000i_write(0x60);	// get current frame
	TEST_CHECK_EQUAL(LATVAL_GENERIC | 0x31, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x32, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x33, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x34, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x35, ldp1000i_read());

	ldp1000i_write(0x60);	// get current frame
	TEST_CHECK_EQUAL(LATVAL_GENERIC | 0x30, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x30, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x30, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x30, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x31, ldp1000i_read());

}

TEST_CASE(ldp1000_get_cur_frame)
{
	test_ldp1000_get_cur_frame();
}

void test_ldp1000_clear()
{
	MockLDP1000Test mockLDP;

	ldp1000_test_wrapper::setup(&mockLDP);

	ldp1000i_reset(LDP1000_EMU_LDP1000A);
	ldp1000i_write(0x56);	// clear
	TEST_CHECK_EQUAL(LATACK_CLEAR, ldp1000i_read());
}

TEST_CASE(ldp1000_clear)
{
	test_ldp1000_clear();
}

void test_ldp1000_stepfwd()
{
	MockLDP1000Test mockLDP;

	EXPECT_CALL(mockLDP, StepForward());

	ldp1000_test_wrapper::setup(&mockLDP);

	ldp1000i_reset(LDP1000_EMU_LDP1450);
	ldp1000i_write(0x2B);	// step fwd
	TEST_CHECK_EQUAL(LATACK_STILL, ldp1000i_read());
}

TEST_CASE(ldp1000_stepfwd)
{
	test_ldp1000_stepfwd();
}

void test_ldp1000_steprev()
{
	MockLDP1000Test mockLDP;

	EXPECT_CALL(mockLDP, StepReverse());

	ldp1000_test_wrapper::setup(&mockLDP);

	ldp1000i_reset(LDP1000_EMU_LDP1450);
	ldp1000i_write(0x2C);	// step reverse
	TEST_CHECK_EQUAL(LATACK_STILL, ldp1000i_read());
}

TEST_CASE(ldp1000_steprev)
{
	test_ldp1000_steprev();
}

// test some of the commands dragon's lair 2 issues upon initialization
void test_ldp1000_dl2_init()
{
	MockLDP1000Test mockLDP;

	EXPECT_CALL(mockLDP, OnError(LDP1000_ERR_UNKNOWN_CMD_BYTE, 0xC));
	EXPECT_CALL(mockLDP, ChangeVideo(true));
	EXPECT_CALL(mockLDP, OnError(LDP1000_ERR_UNSUPPORTED_CMD_BYTE, 0x24));

	ldp1000_test_wrapper::setup(&mockLDP);

	ldp1000i_reset(LDP1000_EMU_LDP1450);
	ldp1000i_write(0xC);	// bogus command DL2 sends upon startup
	TEST_CHECK_EQUAL(LATVAL_GENERIC | 0xB, ldp1000i_read());

	ldp1000i_write(0x62);	// motor on, this should return 0xB if the motor is already on (which it always will be since we don't support turning the motor off)
	TEST_CHECK_EQUAL(LATVAL_GENERIC | 0xB, ldp1000i_read());

	ldp1000i_write(0x6E);	// CX ON
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	ldp1000i_write(0x27);	// video on
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	ldp1000i_write(0x28);	// PSC enable
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	ldp1000i_write(0x55);	// frame mode
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	ldp1000i_write(0x24);	// audio on
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
}

TEST_CASE(ldp1000_dl2_init)
{
	test_ldp1000_dl2_init();
}

void test_ldp1000_status_inq()
{
	MockLDP1000Test mockLDP;

	ldp1000_test_wrapper::setup(&mockLDP);

	EXPECT_CALL(mockLDP, GetStatus()).WillOnce(Return(LDP1000_PLAYING)).WillOnce(Return(LDP1000_PAUSED));

	ldp1000i_reset(LDP1000_EMU_LDP1450);

	ldp1000i_write(0x67);	// status inquiry

	// for playing disc
	TEST_CHECK_EQUAL(LATVAL_GENERIC | 0x80, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x0, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x10, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x0, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x1, ldp1000i_read());

	ldp1000i_write(0x67);	// status inquiry

	// for paused disc
	TEST_CHECK_EQUAL(LATVAL_GENERIC | 0x80, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x0, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x10, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x0, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x20, ldp1000i_read());
}

TEST_CASE(ldp1000_status_inq)
{
	test_ldp1000_status_inq();
}

void test_ldp1000_status_inq_search()
{
	MockLDP1000Test mockLDP;

	ldp1000_test_wrapper::setup(&mockLDP);

	EXPECT_CALL(mockLDP, Pause());
	EXPECT_CALL(mockLDP, GetStatus()).WillOnce(Return(LDP1000_PAUSED)).WillOnce(Return(LDP1000_SEARCHING));
	EXPECT_CALL(mockLDP, BeginSearch(23456));

	ldp1000i_reset(LDP1000_EMU_LDP1450);

	ldp1000i_write(0x43);	// start search
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());
	ldp1000i_write('2');	// M1
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('3');	// M2
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('4');	// M3
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('5');	// M4
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('6');	// M5
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());

	ldp1000i_write(0x67);	// status inquiry

	// for paused disc, when we are in the middle of a search command but have not yet sent "enter"
	TEST_CHECK_EQUAL(LATVAL_GENERIC | 0x80, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x0, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x10, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x03, ldp1000i_read());	// the 3 means it is accepting digits as input and is in the middle of a search command
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x20, ldp1000i_read());

	ldp1000i_write(0x40);	// enter
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());

	ldp1000i_write(0x67);	// status inquiry

	// for when we are in the middle of an actual search and it hasn't found the destination frame yet
	TEST_CHECK_EQUAL(LATVAL_GENERIC | 0xC0, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x0, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x10, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x0, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x00, ldp1000i_read());

}

TEST_CASE(ldp1000_status_inq_search)
{
	test_ldp1000_status_inq_search();
}

void test_ldp1000_repeat1()
{
	MockLDP1000Test mockLDP1000;

	ldp1000_test_wrapper::setup(&mockLDP1000);

	{
		InSequence dummy;

		EXPECT_CALL(mockLDP1000, GetCurFrame()).WillOnce(Return(100));
		EXPECT_CALL(mockLDP1000, Pause());	// should get a pause here since repeat cmd always pauses

		// after ENTER
		EXPECT_CALL(mockLDP1000, Play(1, 1, false, false));

		// repeat state 3 (taking a short cut)
		EXPECT_CALL(mockLDP1000, GetCurFrame()).WillOnce(Return(300));
		EXPECT_CALL(mockLDP1000, BeginSearch(100));

		// repeat state 2
		EXPECT_CALL(mockLDP1000, GetStatus()).WillOnce(Return(LDP1000_PAUSED));
		EXPECT_CALL(mockLDP1000, Play(1, 1, false, false));

		// repeat state 3
		EXPECT_CALL(mockLDP1000, GetCurFrame()).WillOnce(Return(300));
		EXPECT_CALL(mockLDP1000, Pause());
	}

	ldp1000i_reset(LDP1000_EMU_LDP1450);
	ldp1000i_write(0x44);	// start repeat
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write('0');	// M1
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');	// M2
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('3');	// M3
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');	// M4
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');	// M5
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());

	ldp1000i_write(0x40);	// enter
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());

	// 2 iterations
	ldp1000i_write('0');
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('2');
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());

	ldp1000i_write(0x40);	// enter
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());

	// make repeat play out
	ldp1000i_think_during_vblank();
	ldp1000i_think_during_vblank();
	ldp1000i_think_during_vblank();

	TEST_CHECK_EQUAL(LATVAL_GENERIC | 1, ldp1000i_read());
}

TEST_CASE(ldp1000_repeat1)
{
	test_ldp1000_repeat1();
}

void test_ldp1000_repeat_implied_iterations()
{
	MockLDP1000Test mockLDP1000;

	ldp1000_test_wrapper::setup(&mockLDP1000);

	{
		InSequence dummy;

		EXPECT_CALL(mockLDP1000, GetCurFrame()).WillOnce(Return(301));
		EXPECT_CALL(mockLDP1000, Pause());	// should get a pause here since repeat cmd always pauses

		// after ENTER
		EXPECT_CALL(mockLDP1000, Play(1, 1, false, false));

		// repeat state 3 (taking a short cut)
		EXPECT_CALL(mockLDP1000, GetCurFrame()).WillOnce(Return(764));
		EXPECT_CALL(mockLDP1000, Pause());
	}

	ldp1000i_reset(LDP1000_EMU_LDP1000A);
	ldp1000i_write(0x44);	// start repeat
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write('0');	// M1
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');	// M2
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('7');	// M3
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('6');	// M4
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('4');	// M5
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());

	ldp1000i_write(0x40);	// enter
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());

	// 1 iteration implied

	ldp1000i_write(0x40);	// enter
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());

	// exercise repeat logic
	ldp1000i_think_during_vblank();

	TEST_CHECK_EQUAL(LATVAL_GENERIC | 1, ldp1000i_read());
}

TEST_CASE(ldp1000_repeat_implied_iterations)
{
	test_ldp1000_repeat_implied_iterations();
}

void test_ldp1000_repeat_reverse()
{
	MockLDP1000Test mockLDP1000;

	ldp1000_test_wrapper::setup(&mockLDP1000);

	{
		InSequence dummy;

		EXPECT_CALL(mockLDP1000, GetCurFrame()).WillOnce(Return(300));
		EXPECT_CALL(mockLDP1000, Pause());	// should get a pause here since repeat cmd always pauses

		// after ENTER
		EXPECT_CALL(mockLDP1000, Play(1, 1, true, true));

		// repeat state 3 (taking a short cut)
		EXPECT_CALL(mockLDP1000, GetCurFrame()).WillOnce(Return(100));
		EXPECT_CALL(mockLDP1000, BeginSearch(300));

		// repeat state 2
		EXPECT_CALL(mockLDP1000, GetStatus()).WillOnce(Return(LDP1000_PAUSED));
		EXPECT_CALL(mockLDP1000, Play(1, 1, true, true));

		// repeat state 3
		EXPECT_CALL(mockLDP1000, GetCurFrame()).WillOnce(Return(100));
		EXPECT_CALL(mockLDP1000, Pause());
	}

	ldp1000i_reset(LDP1000_EMU_LDP1450);
	ldp1000i_write(0x44);	// start repeat
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write('0');	// M1
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');	// M2
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('1');	// M3
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');	// M4
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');	// M5
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());

	ldp1000i_write(0x40);	// enter
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());

	// 2 iterations
	ldp1000i_write('0');
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('2');
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());

	ldp1000i_write(0x40);	// enter
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());

	// make repeat play out
	ldp1000i_think_during_vblank();
	ldp1000i_think_during_vblank();
	ldp1000i_think_during_vblank();

	TEST_CHECK_EQUAL(LATVAL_GENERIC | 1, ldp1000i_read());
}

TEST_CASE(ldp1000_repeat_reverse)
{
	test_ldp1000_repeat_reverse();
}

void test_ldp1000_repeat_cancel_with_search()
{
	MockLDP1000Test mockLDP1000;

	ldp1000_test_wrapper::setup(&mockLDP1000);

	{
		InSequence dummy;

		EXPECT_CALL(mockLDP1000, GetCurFrame()).WillOnce(Return(100));
		EXPECT_CALL(mockLDP1000, Pause());	// should get a pause here since the repeat cmd will always pause the disc
		
		// initite repeat
		EXPECT_CALL(mockLDP1000, Play(1, 1, false, false));

		// search command
		EXPECT_CALL(mockLDP1000, Pause());	// search command always calls pause
		EXPECT_CALL(mockLDP1000, BeginSearch(250));		
	}

	ldp1000i_reset(LDP1000_EMU_LDP1450);
	ldp1000i_write(0x44);	// start repeat
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write('0');	// M1
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');	// M2
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('3');	// M3
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');	// M4
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');	// M5
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());

	ldp1000i_write(0x40);	// enter
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());

	// 2 iterations
	ldp1000i_write('0');
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('2');
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());

	ldp1000i_write(0x40);	// enter
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());

	// now cancel repeat by initiating a search
	ldp1000i_write(0x43);	// start search
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());
	ldp1000i_write('0');	// M1
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');	// M2
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('2');	// M3
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('5');	// M4
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');	// M5
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());

	ldp1000i_write(0x40);	// enter
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());

	// make sure that repeat is inactive
	TEST_CHECK_EQUAL(LDP1000_FALSE, ldp1000i_isRepeatActive());
}

TEST_CASE(ldp1000_repeat_cancel_with_search)
{
	test_ldp1000_repeat_cancel_with_search();
}

void test_ldp1000_repeat_no_cancel()
{
	MockLDP1000Test mockLDP1000;

	ldp1000_test_wrapper::setup(&mockLDP1000);

	{
		InSequence dummy;

		EXPECT_CALL(mockLDP1000, GetCurFrame()).WillOnce(Return(100));
		EXPECT_CALL(mockLDP1000, Pause());	// should get a pause here since the repeat cmd will always pause the disc
		
		// initite repeat
		EXPECT_CALL(mockLDP1000, Play(1, 1, false, false));

		// multistep command initiation
		EXPECT_CALL(mockLDP1000, Play(1, 7, false, true));

		// multistep command explicit
		EXPECT_CALL(mockLDP1000, Play(1, 15, false, true));

		// still command
		EXPECT_CALL(mockLDP1000, Pause());

		// play reverse 1x
		EXPECT_CALL(mockLDP1000, Play(1, 1, true, true));
	}

	ldp1000i_reset(LDP1000_EMU_LDP1450);
	ldp1000i_write(0x44);	// start repeat
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write('0');	// M1
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');	// M2
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('3');	// M3
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');	// M4
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');	// M5
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());

	ldp1000i_write(0x40);	// enter
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());

	// 2 iterations
	ldp1000i_write('0');
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('2');
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());

	ldp1000i_write(0x40);	// enter
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());

	// none of these commands should cancel the repeat

	ldp1000i_write(0x3d);	// multistep
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write('1');	// M1
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('5');	// M2
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());

	ldp1000i_write(0x40);	// enter
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());

	ldp1000i_write(0x4f);	// still
	TEST_CHECK_EQUAL(LATACK_STILL, ldp1000i_read());

	ldp1000i_write(0x4a);	// rev 1x
	TEST_CHECK_EQUAL(LATACK_PLAY, ldp1000i_read());

	// make sure that repeat is active
	TEST_CHECK_EQUAL(LDP1000_TRUE, ldp1000i_isRepeatActive());
}

TEST_CASE(ldp1000_repeat_no_cancel)
{
	test_ldp1000_repeat_no_cancel();
}

// TODO : do test of REPEAT where frame number ends up before start of repeat loop. make sure result is unsupported situation.

// TODO : do test of REPEAT multispeed to see what happens after a loop (does multispeed stay?)

// TODO : do a test of 3d to make sure that 3a cancels it and reverts 'enter state' to normal

// TODO : do a test of 43 and see what happens if a 3a comes in before the 43 command has finished. does 3a cancel the 43? same with 44? (unsupported situation)

// TODO : do a test of 43 and see what happens if a 3d comes in

// TODO : do a test of 44 and see what happens if a 3d comes in.  manual says that this is actually supported.

void test_ldp1000_overlay1()
{
	MockLDP1000Test mockLDP1000;

	ldp1000_test_wrapper::setup(&mockLDP1000);

	EXPECT_CALL(mockLDP1000, TextModesChanged(0x0,0,0)).Times(1);
	EXPECT_CALL(mockLDP1000, TextModesChanged(0x20,0,0)).Times(1);
	EXPECT_CALL(mockLDP1000, TextModesChanged(0x20,0x21,0x1F)).Times(1);

	ldp1000i_reset(LDP1000_EMU_LDP1450);
	ldp1000i_write(0x80);	// start User Index Control
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(0);	// set position and mode
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(0x0);	// X
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(0x0);	// Y
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(0x0);	// mode
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	// now keep the coordinates the same but only change the mode, we should only get a mode notification

	ldp1000i_write(0x80);	// start User Index Control
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(0);	// set position and mode
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(0x0);	// X
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(0x0);	// Y
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(0x20);	// mode
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	// now keep mode the same, but change cordinates, we should only get a coord notification

	ldp1000i_write(0x80);	// start User Index Control
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(0);	// set position and mode
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(0x21);	// X
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(0x1F);	// Y
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(0x20);	// mode
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

}

TEST_CASE(ldp1000_overlay1)
{
	test_ldp1000_overlay1();
}

void test_ldp1000_overlay2()
{
	MockLDP1000Test mockLDP1000;

	ldp1000_test_wrapper::setup(&mockLDP1000);

	EXPECT_CALL(mockLDP1000, TextBufferStartIndexChanged(0)).Times(1);
	EXPECT_CALL(mockLDP1000, TextBufferStartIndexChanged(1)).Times(1);

	ldp1000i_reset(LDP1000_EMU_LDP1450);
	ldp1000i_write(0x80);	// start User Index Control
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(2);	// set window
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(0x0);	// idx
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	// now set it again with the same value
	ldp1000i_write(0x80);	// start User Index Control
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(2);	// set window
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(0x0);	// idx
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	// now set it again with a different value
	ldp1000i_reset(LDP1000_EMU_LDP1450);
	ldp1000i_write(0x80);	// start User Index Control
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(2);	// set window
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(0x1);	// idx
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

}

TEST_CASE(ldp1000_overlay2)
{
	test_ldp1000_overlay2();
}

void test_ldp1000_overlay3()
{
	MockLDP1000Test mockLDP1000;

	ldp1000_test_wrapper::setup(&mockLDP1000);

	EXPECT_CALL(mockLDP1000, TextEnableChanged(true)).Times(1);
	EXPECT_CALL(mockLDP1000, TextEnableChanged(false)).Times(1);

	ldp1000i_reset(LDP1000_EMU_LDP1450);
	ldp1000i_write(0x82);	// disable UIC
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	ldp1000i_write(0x81);	// enable UIC
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	ldp1000i_write(0x82);	// disable UIC
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
}

TEST_CASE(ldp1000_overlay3)
{
	test_ldp1000_overlay3();
}

void CheckOverlayBuffer4(const uint8_t *p8Buf)
{
	for (int i = 0; i < 32; i++)
	{
		TEST_REQUIRE_EQUAL(p8Buf[i], 32);
	}
}

void test_ldp1000_overlay4()
{
	MockLDP1000Test mockLDP1000;

	ldp1000_test_wrapper::setup(&mockLDP1000);

	EXPECT_CALL(mockLDP1000, TextBufferContentsChanged(_)).WillOnce(Invoke(CheckOverlayBuffer4));
	EXPECT_CALL(mockLDP1000, GetStatus()).WillOnce(Return(LDP1000_PAUSED));

	ldp1000i_reset(LDP1000_EMU_LDP1450);

	ldp1000i_write(0x80);	// UIC command
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	ldp1000i_write(0x1);	// set buffer
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	ldp1000i_write(0x0);	// starting at position 0
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	// add 32 characters
	for (int i = 0; i < 32; i ++)
	{
		ldp1000i_write(0x20);	// space character
		TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	}

	// add end-of-line character
	ldp1000i_write(0x1A);
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	// make sure we are out of overlay buffer mode
	ldp1000i_write(0x67);

	TEST_CHECK_EQUAL(LATVAL_GENERIC | 0x80, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x0, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x10, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x0, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x20, ldp1000i_read());

}

TEST_CASE(ldp1000_overlay4)
{
	test_ldp1000_overlay4();
}

void test_ldp1000_overlay4_err()
{
	MockLDP1000Test mockLDP1000;

	ldp1000_test_wrapper::setup(&mockLDP1000);

//	EXPECT_CALL(mockLDP1000, TextBufferContentsChanged(_)).WillOnce(Invoke(CheckOverlayBuffer4));
	EXPECT_CALL(mockLDP1000, GetStatus()).WillOnce(Return(LDP1000_PAUSED));

	ldp1000i_reset(LDP1000_EMU_LDP1450);

	ldp1000i_write(0x80);	// UIC command
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	ldp1000i_write(0x1);	// set buffer
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	ldp1000i_write(0x0);	// starting at position 0
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	ldp1000i_write(0x60);	// invalid character
	TEST_CHECK_EQUAL(LATVAL_GENERIC | 0xB, ldp1000i_read());

	// make sure we are out of overlay buffer mode
	ldp1000i_write(0x67);

	TEST_CHECK_EQUAL(LATVAL_GENERIC | 0x80, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x0, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x10, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x0, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x20, ldp1000i_read());

}

TEST_CASE(ldp1000_overlay4_err)
{
	test_ldp1000_overlay4_err();
}

void CheckOverlayBuffer5(const uint8_t *p8Buf)
{
	TEST_REQUIRE_EQUAL(p8Buf[0], 0x5F);
	TEST_REQUIRE_EQUAL(p8Buf[31], 0x5F);
	for (int i = 1; i < 30; i++)
	{
		TEST_REQUIRE_EQUAL(p8Buf[i], 0);
	}
}

void test_ldp1000_overlay5()
{
	MockLDP1000Test mockLDP1000;

	ldp1000_test_wrapper::setup(&mockLDP1000);

	EXPECT_CALL(mockLDP1000, TextBufferContentsChanged(_)).WillOnce(Invoke(CheckOverlayBuffer5));

	ldp1000i_reset(LDP1000_EMU_LDP1450);

	ldp1000i_write(0x80);	// UIC command
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	ldp1000i_write(0x1);	// set buffer
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	ldp1000i_write(31);	// starting at position 31
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	// add two 0x5F's which should appear at the beginning and end of the buffer
	ldp1000i_write(0x5F);
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(0x5F);
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	// add 30 characters
	for (int i = 0; i < 30; i ++)
	{
		ldp1000i_write(0x0);	// null character
		TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	}

	// add end-of-line character
	ldp1000i_write(0x1A);
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
}

TEST_CASE(ldp1000_overlay5)
{
	test_ldp1000_overlay5();
}

void test_ldp1000_overlay6()
{
	MockLDP1000Test mockLDP1000;

	ldp1000_test_wrapper::setup(&mockLDP1000);

	ldp1000i_reset(LDP1000_EMU_LDP1450);

	ldp1000i_write(0x80);	// UIC command
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	ldp1000i_write(0x1);	// set buffer
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	ldp1000i_write(0);	// starting at position 0
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	// out of range byte
	ldp1000i_write(0x60);
	TEST_CHECK_EQUAL(LATVAL_GENERIC | 0xB, ldp1000i_read());
}

TEST_CASE(ldp1000_overlay6)
{
	test_ldp1000_overlay6();
}

void test_ldp1000_overlay_while_seeking()
{
	MockLDP1000Test mockLDP1000;

	EXPECT_CALL(mockLDP1000, Pause());
	EXPECT_CALL(mockLDP1000, BeginSearch(2108));
	EXPECT_CALL(mockLDP1000, TextBufferContentsChanged(_));
	EXPECT_CALL(mockLDP1000, GetCurFrame()).WillOnce(Return(1));
	EXPECT_CALL(mockLDP1000, GetStatus()).WillOnce(Return(LDP1000_PAUSED));

	ldp1000_test_wrapper::setup(&mockLDP1000);

	ldp1000i_reset(LDP1000_EMU_LDP1450);

	ldp1000i_write(0x43);	// start search
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());
	ldp1000i_write(0x30);	// 0 command
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write(0x32);	// 2 command
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write(0x31);	// 1 command
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write(0x30);	// 0 command
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write(0x38);	// 8 command
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write(0x40);	// enter command
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());

	ldp1000i_write(0x80);	// UIC command
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	ldp1000i_write(0x1);	// set buffer
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	ldp1000i_write(0x14);	// starting position
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	ldp1000i_write(0x32);
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	ldp1000i_write(0x1A);
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	ldp1000i_write(0x60);
	TEST_CHECK_EQUAL(LATVAL_GENERIC | 0x30, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x30, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x30, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x30, ldp1000i_read());
	TEST_CHECK_EQUAL(LATVAL_INQUIRY | 0x31, ldp1000i_read());

	// search completes at this point
	ldp1000i_think_during_vblank();

	TEST_CHECK_EQUAL(LATVAL_GENERIC | 0x01, ldp1000i_read());
}

TEST_CASE(ldp1000_overlay_while_seeking)
{
	test_ldp1000_overlay_while_seeking();
}

void test_ldp1000_multispeed()
{
	MockLDP1000Test mockLDP1000;

	ldp1000_test_wrapper::setup(&mockLDP1000);

	EXPECT_CALL(mockLDP1000, Pause());
	EXPECT_CALL(mockLDP1000, BeginSearch(30000));
	EXPECT_CALL(mockLDP1000, GetStatus()).WillRepeatedly(Return(LDP1000_PAUSED));
	EXPECT_CALL(mockLDP1000, Play(1, 1, true, true));
	EXPECT_CALL(mockLDP1000, Play(3, 1, true, true));
	EXPECT_CALL(mockLDP1000, Play(1, 5, true, true));
	EXPECT_CALL(mockLDP1000, Play(3, 1, false, true));
	EXPECT_CALL(mockLDP1000, Play(1, 5, false, true));

	ldp1000i_reset(LDP1000_EMU_LDP1450);
	ldp1000i_write(0x43);	// start search
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());
	ldp1000i_write('3');	// M1
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');	// M2
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');	// M3
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');	// M4
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');	// M5
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());

	ldp1000i_write(0x40);	// enter
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());

	ldp1000i_write(0x4A);	// playback reverse at 1X
	TEST_CHECK_EQUAL(LATACK_PLAY, ldp1000i_read());

	ldp1000i_write(0x4B);	// playback reverse at 3X
	TEST_CHECK_EQUAL(LATACK_PLAY, ldp1000i_read());

	ldp1000i_write(0x4C);	// playback reverse at 1/5X
	TEST_CHECK_EQUAL(LATACK_PLAY, ldp1000i_read());

	ldp1000i_write(0x3B);	// playback forward at 3X
	TEST_CHECK_EQUAL(LATACK_PLAY, ldp1000i_read());

	ldp1000i_write(0x3C);	// playback forward at 1/5X
	TEST_CHECK_EQUAL(LATACK_PLAY, ldp1000i_read());

}

TEST_CASE(ldp1000_multispeed)
{
	test_ldp1000_multispeed();
}

void test_ldp1000_multispeed2()
{
	MockLDP1000Test mockLDP1000;

	ldp1000_test_wrapper::setup(&mockLDP1000);

	EXPECT_CALL(mockLDP1000, Pause());
	EXPECT_CALL(mockLDP1000, BeginSearch(30000));
	EXPECT_CALL(mockLDP1000, GetStatus()).WillRepeatedly(Return(LDP1000_PAUSED));
	EXPECT_CALL(mockLDP1000, Play(1, 7, false, true));	// 0x3d command always sends this
	EXPECT_CALL(mockLDP1000, Play(1, 2, false, true));

	ldp1000i_reset(LDP1000_EMU_LDP1450);
	ldp1000i_write(0x43);	// start search
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());
	ldp1000i_write('3');	// M1
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');	// M2
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');	// M3
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');	// M4
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');	// M5
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());

	ldp1000i_write(0x40);	// enter
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());

	ldp1000i_write(0x3D);	// variable speed playback start
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	ldp1000i_write(0x32);	// variable speed playback value
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());

	ldp1000i_write(0x40);	// enter
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());
}

TEST_CASE(ldp1000_multispeed2)
{
	test_ldp1000_multispeed2();
}

void test_ldp1000_multispeed3()
{
	MockLDP1000Test mockLDP1000;

	ldp1000_test_wrapper::setup(&mockLDP1000);

	EXPECT_CALL(mockLDP1000, Pause());
	EXPECT_CALL(mockLDP1000, BeginSearch(30000));
	EXPECT_CALL(mockLDP1000, GetStatus()).WillRepeatedly(Return(LDP1000_PAUSED));
	EXPECT_CALL(mockLDP1000, Play(1, 7, true, true));	// 0x4d command sends this
	EXPECT_CALL(mockLDP1000, Play(1, 255, true, true));

	ldp1000i_reset(LDP1000_EMU_LDP1450);
	ldp1000i_write(0x43);	// start search
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());
	ldp1000i_write('3');	// M1
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');	// M2
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');	// M3
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');	// M4
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');	// M5
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());

	ldp1000i_write(0x40);	// enter
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());

	ldp1000i_write(0x4D);	// variable speed playback start (backward)
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	ldp1000i_write(0x32);	// variable speed playback value
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());

	ldp1000i_write(0x35);	// variable speed playback value
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());

	ldp1000i_write(0x35);	// variable speed playback value
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());

	ldp1000i_write(0x40);	// enter
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());
}

TEST_CASE(ldp1000_multispeed3)
{
	test_ldp1000_multispeed3();
}

void test_ldp1000_multispeed4()
{
	MockLDP1000Test mockLDP1000;

	ldp1000_test_wrapper::setup(&mockLDP1000);

	{
		InSequence dummy;
		EXPECT_CALL(mockLDP1000, Pause());
		EXPECT_CALL(mockLDP1000, BeginSearch(30000));
		EXPECT_CALL(mockLDP1000, Play(1, 7, false, true));	// 0x3d command starts playing 1/7X before it gets its paramter for some reason
		EXPECT_CALL(mockLDP1000, Play(1, 1, false, true));
		EXPECT_CALL(mockLDP1000, Play(1, 1, false, false));
	}

	ldp1000i_reset(LDP1000_EMU_LDP1450);
	ldp1000i_write(0x43);	// start search
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());
	ldp1000i_write('3');	// M1
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');	// M2
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');	// M3
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');	// M4
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');	// M5
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());

	ldp1000i_write(0x40);	// enter
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());

	ldp1000i_write(0x3D);	// variable speed playback start
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	ldp1000i_write(0x31);	// variable speed playback value
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());

	ldp1000i_write(0x40);	// enter
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());

	// then play normally with audio being restored
	ldp1000i_write(0x3A);
	TEST_CHECK_EQUAL(LATACK_PLAY, ldp1000i_read());
}

TEST_CASE(ldp1000_multispeed4)
{
	test_ldp1000_multispeed4();
}

void test_ldp1000_video()
{
	MockLDP1000Test mockLDP1000;

	ldp1000_test_wrapper::setup(&mockLDP1000);

	{
		InSequence dummy;
		EXPECT_CALL(mockLDP1000, ChangeVideo(false));
		EXPECT_CALL(mockLDP1000, ChangeVideo(true));
	}

	ldp1000i_reset(LDP1000_EMU_LDP1450);
	ldp1000i_write(0x26);	// video off
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(0x27);	// video on
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
}

TEST_CASE(ldp1000_video)
{
	test_ldp1000_video();
}

void test_ldp1000_skip_forward()
{
	MockLDP1000Test mockLDP1000;

	ldp1000_test_wrapper::setup(&mockLDP1000);

	{
		InSequence dummy;
		EXPECT_CALL(mockLDP1000, Pause());
		EXPECT_CALL(mockLDP1000, Skip(150));
	}

	ldp1000i_reset(LDP1000_EMU_LDP1450);
	ldp1000i_write(0x2D);	// start skip forward
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());
	ldp1000i_write('1');
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('5');
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());

	ldp1000i_write(0x40);	// enter
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());
	
	// verify that we also get a '1' after the ACK
	TEST_CHECK_EQUAL(LATVAL_GENERIC | 1, ldp1000i_read());
}

TEST_CASE(ldp1000_skip_forward)
{
	test_ldp1000_skip_forward();
}

void test_ldp1000_skip_backward()
{
	MockLDP1000Test mockLDP1000;

	ldp1000_test_wrapper::setup(&mockLDP1000);

	{
		InSequence dummy;
		EXPECT_CALL(mockLDP1000, Pause());
		EXPECT_CALL(mockLDP1000, Skip(-150));
	}

	ldp1000i_reset(LDP1000_EMU_LDP1450);
	ldp1000i_write(0x2E);	// start skip backward
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());
	ldp1000i_write('1');
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('5');
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());

	ldp1000i_write(0x40);	// enter
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());

	// verify that we also get a '1' after the ACK
	TEST_CHECK_EQUAL(LATVAL_GENERIC | 1, ldp1000i_read());
}

TEST_CASE(ldp1000_skip_backward)
{
	test_ldp1000_skip_backward();
}

// Verifies that the tx-queue's count tracks push/pop precisely: ldp1000i_can_read
// must be FALSE after reset, TRUE after a command that enqueues a byte, and back
// to FALSE after the byte is consumed.  This exercises the post-increment in
// push_queue and post-decrement in pop_queue.
void test_ldp1000_queue_count_tracking()
{
	MockLDP1000Test mockLDP;
	ldp1000_test_wrapper::setup(&mockLDP);

	EXPECT_CALL(mockLDP, Pause());

	ldp1000i_reset(LDP1000_EMU_LDP1000A);

	// queue should be empty right after reset
	TEST_CHECK_EQUAL(LDP1000_FALSE, ldp1000i_can_read());

	// pause pushes exactly one byte
	ldp1000i_write(0x4F);	// pause
	TEST_CHECK_EQUAL(LDP1000_TRUE, ldp1000i_can_read());

	// read it; queue must go empty
	TEST_CHECK_EQUAL(LATACK_STILL, ldp1000i_read());
	TEST_CHECK_EQUAL(LDP1000_FALSE, ldp1000i_can_read());
}

TEST_CASE(ldp1000_queue_count_tracking)
{
	test_ldp1000_queue_count_tracking();
}

// Multi-push variant: a status inquiry pushes 5 bytes, which must be readable
// in order with can_read() ticking down to false after the last.
void test_ldp1000_queue_count_multi()
{
	MockLDP1000Test mockLDP;
	ldp1000_test_wrapper::setup(&mockLDP);

	EXPECT_CALL(mockLDP, GetStatus()).WillOnce(Return(LDP1000_PAUSED));

	ldp1000i_reset(LDP1000_EMU_LDP1450);

	TEST_CHECK_EQUAL(LDP1000_FALSE, ldp1000i_can_read());
	ldp1000i_write(0x67);	// status inquiry: 5 bytes queued

	for (int i = 0; i < 5; i++)
	{
		TEST_REQUIRE_EQUAL(LDP1000_TRUE, ldp1000i_can_read());
		ldp1000i_read();
	}

	// queue must be empty after all 5 bytes consumed
	TEST_CHECK_EQUAL(LDP1000_FALSE, ldp1000i_can_read());
}

TEST_CASE(ldp1000_queue_count_multi)
{
	test_ldp1000_queue_count_multi();
}

// Step forward/reverse each push exactly one byte.  Verifying can_read()
// transitions covers the push_queue calls that otherwise survive mutation
// because residual bytes from prior tests may linger in the global tx_buf.
void test_ldp1000_step_queue_state()
{
	MockLDP1000Test mockLDP;
	ldp1000_test_wrapper::setup(&mockLDP);

	EXPECT_CALL(mockLDP, StepForward());
	EXPECT_CALL(mockLDP, StepReverse());

	ldp1000i_reset(LDP1000_EMU_LDP1450);

	TEST_CHECK_EQUAL(LDP1000_FALSE, ldp1000i_can_read());
	ldp1000i_write(0x2B);	// step forward - must push LATACK_STILL
	TEST_CHECK_EQUAL(LDP1000_TRUE, ldp1000i_can_read());
	TEST_CHECK_EQUAL(LATACK_STILL, ldp1000i_read());
	TEST_CHECK_EQUAL(LDP1000_FALSE, ldp1000i_can_read());

	ldp1000i_write(0x2C);	// step reverse - must push LATACK_STILL
	TEST_CHECK_EQUAL(LDP1000_TRUE, ldp1000i_can_read());
	TEST_CHECK_EQUAL(LATACK_STILL, ldp1000i_read());
	TEST_CHECK_EQUAL(LDP1000_FALSE, ldp1000i_can_read());
}

TEST_CASE(ldp1000_step_queue_state)
{
	test_ldp1000_step_queue_state();
}

// The STOP command (0x3F) both pushes a response byte and reports an
// unsupported-command error.
void test_ldp1000_stop_cmd()
{
	MockLDP1000Test mockLDP;
	ldp1000_test_wrapper::setup(&mockLDP);

	EXPECT_CALL(mockLDP, OnError(LDP1000_ERR_UNSUPPORTED_CMD_BYTE, 0x3F));

	ldp1000i_reset(LDP1000_EMU_LDP1000A);

	TEST_CHECK_EQUAL(LDP1000_FALSE, ldp1000i_can_read());
	ldp1000i_write(0x3F);	// stop
	TEST_REQUIRE_EQUAL(LDP1000_TRUE, ldp1000i_can_read());
	TEST_CHECK_EQUAL(LATACK_PLAY, ldp1000i_read());	// stop returns same latency as play
	TEST_CHECK_EQUAL(LDP1000_FALSE, ldp1000i_can_read());
}

TEST_CASE(ldp1000_stop_cmd)
{
	test_ldp1000_stop_cmd();
}

// Too-many-digits test: entering 6 digits into a search should accept 5 and
// reject the 6th with an error plus a LATVAL_NUMBER|0xB response byte.
void test_ldp1000_too_many_digits()
{
	MockLDP1000Test mockLDP;
	ldp1000_test_wrapper::setup(&mockLDP);

	EXPECT_CALL(mockLDP, Pause());
	EXPECT_CALL(mockLDP, OnError(LDP1000_ERR_TOO_MANY_DIGITS, 0));

	ldp1000i_reset(LDP1000_EMU_LDP1450);

	ldp1000i_write(0x43);	// start search
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());

	for (int i = 0; i < 5; i++)
	{
		ldp1000i_write('0' + i);
		TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	}

	// the 6th digit must trigger TOO_MANY_DIGITS + a LATVAL_NUMBER|0xB push
	ldp1000i_write('9');
	TEST_REQUIRE_EQUAL(LDP1000_TRUE, ldp1000i_can_read());
	TEST_CHECK_EQUAL(LATVAL_NUMBER | 0xB, ldp1000i_read());
}

TEST_CASE(ldp1000_too_many_digits)
{
	test_ldp1000_too_many_digits();
}

// Variable speed with frame value of 0 pauses the disc and pushes a LATACK_ENTER
// response.  This exercises a branch that otherwise has no coverage.
void test_ldp1000_multispeed_zero()
{
	MockLDP1000Test mockLDP;
	ldp1000_test_wrapper::setup(&mockLDP);

	{
		InSequence s;
		EXPECT_CALL(mockLDP, Play(1, 7, false, true));	// 0x3d initiates playback at 1/7X
		EXPECT_CALL(mockLDP, Pause());	// value 0 pauses
	}

	ldp1000i_reset(LDP1000_EMU_LDP1450);

	ldp1000i_write(0x3D);	// variable speed forward
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	// enter with no digit buffered = frame number 0
	ldp1000i_write(0x40);	// enter
	TEST_REQUIRE_EQUAL(LDP1000_TRUE, ldp1000i_can_read());
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());
	TEST_CHECK_EQUAL(LDP1000_FALSE, ldp1000i_can_read());
}

TEST_CASE(ldp1000_multispeed_zero)
{
	test_ldp1000_multispeed_zero();
}

// Variable speed with frame value > 255 returns NAK.
void test_ldp1000_multispeed_out_of_range()
{
	MockLDP1000Test mockLDP;
	ldp1000_test_wrapper::setup(&mockLDP);

	EXPECT_CALL(mockLDP, Play(1, 7, false, true));	// 0x3d initiates playback at 1/7X

	ldp1000i_reset(LDP1000_EMU_LDP1450);

	ldp1000i_write(0x3D);	// variable speed forward
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	// 256 is out of range (allowed range is 1..255 here)
	ldp1000i_write('2');
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('5');
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('6');
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());

	ldp1000i_write(0x40);	// enter
	TEST_REQUIRE_EQUAL(LDP1000_TRUE, ldp1000i_can_read());
	TEST_CHECK_EQUAL(LATNAK_GENERIC, ldp1000i_read());
}

TEST_CASE(ldp1000_multispeed_out_of_range)
{
	test_ldp1000_multispeed_out_of_range();
}

// Skip backward also pushes a 'skip complete' result byte (LATVAL_GENERIC | 1).
// The existing skip_backward test covers that; this one adds a smaller value
// to exercise the skip callback differently and checks the queue state.
void test_ldp1000_skip_backward_small()
{
	MockLDP1000Test mockLDP;
	ldp1000_test_wrapper::setup(&mockLDP);

	EXPECT_CALL(mockLDP, Pause());
	EXPECT_CALL(mockLDP, Skip(-5));

	ldp1000i_reset(LDP1000_EMU_LDP1450);

	ldp1000i_write(0x2E);	// start skip backward
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());
	ldp1000i_write('5');
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());

	ldp1000i_write(0x40);	// enter
	TEST_REQUIRE_EQUAL(LDP1000_TRUE, ldp1000i_can_read());
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());
	TEST_REQUIRE_EQUAL(LDP1000_TRUE, ldp1000i_can_read());
	TEST_CHECK_EQUAL(LATVAL_GENERIC | 1, ldp1000i_read());
	TEST_CHECK_EQUAL(LDP1000_FALSE, ldp1000i_can_read());
}

TEST_CASE(ldp1000_skip_backward_small)
{
	test_ldp1000_skip_backward_small();
}

// UI enable/disable must properly track the bUI_Enabled state.  Strictly
// ordering text_enable_changed callbacks detects a mutant that flips the
// equality check on bUI_Enabled (which would change the order of FALSE/TRUE
// notifications).
void test_ldp1000_ui_enable_state()
{
	MockLDP1000Test mockLDP;
	ldp1000_test_wrapper::setup(&mockLDP);

	{
		InSequence s;
		EXPECT_CALL(mockLDP, TextEnableChanged(true));	// first enable
		EXPECT_CALL(mockLDP, TextEnableChanged(false));	// first disable
	}

	ldp1000i_reset(LDP1000_EMU_LDP1450);

	// a redundant disable (already disabled after reset) must NOT fire a callback
	ldp1000i_write(0x82);
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	// first enable: must fire TextEnableChanged(true)
	ldp1000i_write(0x81);
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	// redundant enable: must NOT fire
	ldp1000i_write(0x81);
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	// disable: must fire TextEnableChanged(false)
	ldp1000i_write(0x82);
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	TEST_CHECK_EQUAL(LDP1000_FALSE, ldp1000i_can_read());
}

TEST_CASE(ldp1000_ui_enable_state)
{
	test_ldp1000_ui_enable_state();
}

// UIC mode change where only X coordinate changes.  Without the |=
// NOTIFY_MODES bit being set, the final text_modes_changed callback won't
// fire.  This covers the X-coordinate |= mutant.
void test_ldp1000_uic_x_only()
{
	MockLDP1000Test mockLDP;
	ldp1000_test_wrapper::setup(&mockLDP);

	// after reset, X/Y/Mode are 0xFF.  we change only X and leave Y/Mode at 0xFF
	// to isolate the X-coordinate notification accumulator.
	EXPECT_CALL(mockLDP, TextModesChanged(0xFF, 0x05, 0xFF));

	ldp1000i_reset(LDP1000_EMU_LDP1450);
	ldp1000i_write(0x80);	// start UIC
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(0);	// set position and mode
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(0x05);	// X = 5 (changed from 0xFF)
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(0xFF);	// Y = 0xFF (unchanged)
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(0xFF);	// mode = 0xFF (unchanged)
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
}

TEST_CASE(ldp1000_uic_x_only)
{
	test_ldp1000_uic_x_only();
}

// UIC mode change where only Y coordinate changes.  Covers the Y-coordinate
// |= mutant (line 658).
void test_ldp1000_uic_y_only()
{
	MockLDP1000Test mockLDP;
	ldp1000_test_wrapper::setup(&mockLDP);

	EXPECT_CALL(mockLDP, TextModesChanged(0xFF, 0xFF, 0x07));

	ldp1000i_reset(LDP1000_EMU_LDP1450);
	ldp1000i_write(0x80);	// start UIC
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(0);	// set position and mode
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(0xFF);	// X = 0xFF (unchanged)
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(0x07);	// Y = 7 (changed)
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(0xFF);	// mode = 0xFF (unchanged)
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
}

TEST_CASE(ldp1000_uic_y_only)
{
	test_ldp1000_uic_y_only();
}

// UIC mode where NOTHING changes: no text_modes_changed callback should fire.
// Covers the `& NOTIFY_MODES` mutant at line 718 (replacing & with |).
void test_ldp1000_uic_no_change()
{
	MockLDP1000Test mockLDP;
	ldp1000_test_wrapper::setup(&mockLDP);

	// text_modes_changed must NOT be called since nothing changed
	EXPECT_CALL(mockLDP, TextModesChanged(_, _, _)).Times(0);

	ldp1000i_reset(LDP1000_EMU_LDP1450);
	ldp1000i_write(0x80);	// start UIC
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(0);	// set position and mode
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(0xFF);	// X = 0xFF (unchanged)
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(0xFF);	// Y = 0xFF (unchanged)
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(0xFF);	// mode = 0xFF (unchanged)
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
}

TEST_CASE(ldp1000_uic_no_change)
{
	test_ldp1000_uic_no_change();
}

// UIC text buffer starting-index parsing uses `u8Byte & 31`.  Sending 32 as the
// start index should mask down to 0, so writing one character lands at buf[0].
// The mutant that replaces & with | would instead give 32|31=63 which would
// cause out-of-bounds behavior.
void CheckUicStartIdxMasked(const uint8_t *p8Buf)
{
	// first byte of buffer should be the one we wrote (0x42)
	TEST_REQUIRE_EQUAL(0x42, p8Buf[0]);
	// none of the other positions should contain it
	for (int i = 1; i < 32; i++)
	{
		TEST_REQUIRE_EQUAL(0, p8Buf[i]);
	}
}

void test_ldp1000_uic_start_idx_masked()
{
	MockLDP1000Test mockLDP;
	ldp1000_test_wrapper::setup(&mockLDP);

	EXPECT_CALL(mockLDP, TextBufferContentsChanged(_)).WillOnce(Invoke(CheckUicStartIdxMasked));

	ldp1000i_reset(LDP1000_EMU_LDP1450);

	ldp1000i_write(0x80);	// start UIC
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(0x1);	// set buffer
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(32);	// start index 32 -> masked to 0
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(0x42);	// one character
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
	ldp1000i_write(0x1A);	// end-of-line
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());
}

TEST_CASE(ldp1000_uic_start_idx_masked)
{
	test_ldp1000_uic_start_idx_masked();
}

// Repeat where end-frame equals start-frame must play FORWARD (not reverse).
// Exercises the `>=` boundary in the direction-of-repeat decision.
void test_ldp1000_repeat_equal_frames()
{
	MockLDP1000Test mockLDP;
	ldp1000_test_wrapper::setup(&mockLDP);

	{
		InSequence s;
		EXPECT_CALL(mockLDP, GetCurFrame()).WillOnce(Return(500));
		EXPECT_CALL(mockLDP, Pause());	// repeat always pauses
		// when start == end, direction must be forward (bBackward = false)
		EXPECT_CALL(mockLDP, Play(1, 1, false, false));
		EXPECT_CALL(mockLDP, GetCurFrame()).WillOnce(Return(500));
		EXPECT_CALL(mockLDP, Pause());	// last iteration stops
	}

	ldp1000i_reset(LDP1000_EMU_LDP1450);

	ldp1000i_write(0x44);	// start repeat
	TEST_CHECK_EQUAL(LATACK_GENERIC, ldp1000i_read());

	// end frame 500 (same as current)
	ldp1000i_write('0');
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('5');
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());
	ldp1000i_write('0');
	TEST_CHECK_EQUAL(LATACK_NUMBER, ldp1000i_read());

	ldp1000i_write(0x40);	// enter
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());

	// implied 1 iteration
	ldp1000i_write(0x40);	// enter
	TEST_CHECK_EQUAL(LATACK_ENTER, ldp1000i_read());

	ldp1000i_think_during_vblank();
	TEST_CHECK_EQUAL(LATVAL_GENERIC | 1, ldp1000i_read());
}

TEST_CASE(ldp1000_repeat_equal_frames)
{
	test_ldp1000_repeat_equal_frames();
}
