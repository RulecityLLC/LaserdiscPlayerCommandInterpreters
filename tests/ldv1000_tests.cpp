#include "stdafx.h"
#include "ldv1000_test_wrapper.h"

ILDV1000Test *ldv1000_test_wrapper::m_pInstance = 0;

///////////////////////////////
void test_ldv1000_clear()
{
	MockLDV1000Test mockLDV1000;
	ldv1000_test_wrapper::setup(&mockLDV1000);

	{
		InSequence s;
		EXPECT_CALL(mockLDV1000, GetStatus()).WillRepeatedly(Return(LDV1000_STOPPED));
	}

	reset_ldv1000i(LDV1000_EMU_STANDARD);

	unsigned char ch;

	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(0xFC, ch);

	write_ldv1000i(0xBF);	// send clear command

	// status should become not ready (Super Don specifically relies on this)
	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(0x7C, ch);
}

TEST_CASE(ldv1000_clear)
{
	test_ldv1000_clear();
}

void test_ldv1000_spinup()
{
	MockLDV1000Test mockLDV1000;
	ldv1000_test_wrapper::setup(&mockLDV1000);

	{
		InSequence s;
		EXPECT_CALL(mockLDV1000, GetStatus()).WillOnce(Return(LDV1000_STOPPED));
		EXPECT_CALL(mockLDV1000, Play());
		EXPECT_CALL(mockLDV1000, GetStatus()).Times(2).WillRepeatedly(Return(LDV1000_SPINNING_UP));
		EXPECT_CALL(mockLDV1000, GetStatus()).WillOnce(Return(LDV1000_PLAYING));
	}

	reset_ldv1000i(LDV1000_EMU_STANDARD);

	unsigned char ch;

	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(0xFC, ch);

	write_ldv1000i(0xFD);	// send play command

	// should be busy spinning up
	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(0x64, ch);

	// send an 0xFF and make sure disc continues to be spinning up (ie not ready)
	write_ldv1000i(0xFF);
	ch = read_ldv1000i();
	TEST_REQUIRE_EQUAL(0x64, ch);

	// now the disc status should be playing based on our mock setup

	write_ldv1000i(0xFF);	// no entry
	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(0xE4, ch);	// disc is full on in playing mode
}

TEST_CASE(ldv1000_spinup)
{
	test_ldv1000_spinup();
}

////////////////////////////

void test_ldv1000_bad_seek()
{
	MockLDV1000Test mockLDV1000;
	ldv1000_test_wrapper::setup(&mockLDV1000);

	{
		InSequence s;
		EXPECT_CALL(mockLDV1000, Play());
		EXPECT_CALL(mockLDV1000, BeginSearch(0));
		EXPECT_CALL(mockLDV1000, GetStatus()).Times(3).WillRepeatedly(Return(LDV1000_SEARCHING));
		EXPECT_CALL(mockLDV1000, GetStatus()).WillRepeatedly(Return(LDV1000_ERROR));
	}

	reset_ldv1000i(LDV1000_EMU_STANDARD);

	unsigned char ch;

	write_ldv1000i(0xFD);	// send play command

	// send a seek command (doesn't matter what it is because it will always fail)

	write_ldv1000i(0xFF);	// no entry
	write_ldv1000i(0xF7);	// seek to whatever frame (should fail)

	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(0x50, ch);	// seek busy

	do
	{
		ch = read_ldv1000i();
	} while (ch == 0x50);

	TEST_CHECK_EQUAL(0x90, ch);	// seek failed

	// successive reads should continue to return 0x90
	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(0x90, ch);	// seek failed
	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(0x90, ch);	// seek failed

	// if we send a digit, then the status should change to 0x10
	write_ldv1000i(0x3F);	// digit 0
	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(0x10, ch);	// seek failed, not ready
	
	// then sending a NO ENTRY should put us back at seek failed and ready
	write_ldv1000i(0xFF);
	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(0x90, ch);	// seek failed

}

TEST_CASE(ldv1000_bad_seek)
{
	test_ldv1000_bad_seek();
}

////////////////////////////

void test_ldv1000_good_seek()
{
	MockLDV1000Test mockLDV1000;
	ldv1000_test_wrapper::setup(&mockLDV1000);

	{
		InSequence s;
		EXPECT_CALL(mockLDV1000, Play());
		EXPECT_CALL(mockLDV1000, BeginSearch(0));
		EXPECT_CALL(mockLDV1000, GetStatus()).Times(10).WillRepeatedly(Return(LDV1000_SEARCHING));
		EXPECT_CALL(mockLDV1000, GetStatus()).WillRepeatedly(Return(LDV1000_PAUSED));
	}

	reset_ldv1000i(LDV1000_EMU_STANDARD);

	unsigned char ch;

	write_ldv1000i(0xFD);	// send play command

	// send a seek command (doesn't matter what it is because it should always succeed

	write_ldv1000i(0xFF);	// no entry
	write_ldv1000i(0xF7);	// seek to whatever frame

	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(0x50, ch);	// seek busy

	unsigned int uBusyIterations = 0;

	while (ch == 0x50)
	{
		write_ldv1000i(0xFF);	// no entry (should not change status from 0x50)
		ch = read_ldv1000i();
		uBusyIterations++;
	};

	TEST_REQUIRE_EQUAL(10, uBusyIterations);

	TEST_CHECK_EQUAL(0xD0, ch);	// seek succeeded

	// successive reads should continue to return the same result
	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(0xD0, ch);
	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(0xD0, ch);

	// if we send a digit, then the status should change to not ready
	write_ldv1000i(0x3F);	// digit 0
	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(0x50, ch);
	
	// then sending a NO ENTRY should put us back at seek succeeded and ready
	write_ldv1000i(0xFF);
	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(0xD0, ch);

}

TEST_CASE(ldv1000_good_seek)
{
	test_ldv1000_good_seek();
}

//////////////////////////////////////////////////

void test_ldv1000_delayed_good_seek()
{
	MockLDV1000Test mockLDV1000;
	ldv1000_test_wrapper::setup(&mockLDV1000);

	{
		InSequence s;
		EXPECT_CALL(mockLDV1000, Play());
		EXPECT_CALL(mockLDV1000, BeginSearch(0));
		EXPECT_CALL(mockLDV1000, GetStatus()).Times(51).WillRepeatedly(Return(LDV1000_SEARCHING));
		EXPECT_CALL(mockLDV1000, GetStatus()).WillRepeatedly(Return(LDV1000_PAUSED));
	}

	reset_ldv1000i(LDV1000_EMU_STANDARD);

	unsigned char ch;

	write_ldv1000i(0xFD);	// send play command

	// send a seek command (doesn't matter what it is because it should always succeed

	write_ldv1000i(0xFF);	// no entry
	write_ldv1000i(0xF7);	// seek to whatever frame

	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(0x50, ch);	// seek busy

	unsigned int uBusyIterations = 0;

	while (uBusyIterations < 50)
	{
		write_ldv1000i(0xF7);	// keep sending 0xF7 to force status to stay at 0x50 even after seek has succeeded
		ch = read_ldv1000i();
		TEST_REQUIRE_EQUAL(0x50, ch);	// seek busy
		uBusyIterations++;
	};

	write_ldv1000i(0xFF);	// no entry
	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(0xD0, ch);	// seek succeeded
}

TEST_CASE(ldv1000_delayed_good_seek)
{
	test_ldv1000_delayed_good_seek();
}

void test_ldv1000_get_curframe()
{
	MockLDV1000Test mockLDV1000;
	ldv1000_test_wrapper::setup(&mockLDV1000);

	{
		InSequence s;
		EXPECT_CALL(mockLDV1000, GetCurFrameNum()).WillOnce(Return(12345));
	}

	reset_ldv1000i(LDV1000_EMU_STANDARD);

	unsigned char ch;

	write_ldv1000i(0xC2);	// frame number query

	ch = read_ldv1000i();
	TEST_CHECK_EQUAL('1', ch);
	ch = read_ldv1000i();
	TEST_CHECK_EQUAL('2', ch);
	ch = read_ldv1000i();
	TEST_CHECK_EQUAL('3', ch);
	ch = read_ldv1000i();
	TEST_CHECK_EQUAL('4', ch);
	ch = read_ldv1000i();
	TEST_CHECK_EQUAL('5', ch);
}

TEST_CASE(ldv1000_get_curframe)
{
	test_ldv1000_get_curframe();
}

void test_ldv1000_audio()
{
	MockLDV1000Test mockLDV1000;
	ldv1000_test_wrapper::setup(&mockLDV1000);

	{
		InSequence s;
		EXPECT_CALL(mockLDV1000, ChangeAudio(0, 0)).Times(2);
		EXPECT_CALL(mockLDV1000, ChangeAudio(0, 1)).Times(2);
		EXPECT_CALL(mockLDV1000, ChangeAudio(1, 0)).Times(2);
		EXPECT_CALL(mockLDV1000, ChangeAudio(1, 1)).Times(2);
	}

	reset_ldv1000i(LDV1000_EMU_STANDARD);

	write_ldv1000i(0xFF);
	write_ldv1000i(0xF4);	// toggle off left audio

	write_ldv1000i(0xFF);
	write_ldv1000i(0x8F);	// '2', an even number	
	write_ldv1000i(0xFF);
	write_ldv1000i(0xF4);	// audio left

	write_ldv1000i(0xFF);
	write_ldv1000i(0xF4);	// toggle on left audio

	write_ldv1000i(0xFF);
	write_ldv1000i(0x4F);	// '3', an odd number
	write_ldv1000i(0xFF);
	write_ldv1000i(0xF4);	// audio left


	write_ldv1000i(0xFF);
	write_ldv1000i(0xFc);	// toggle off right audio

	write_ldv1000i(0xFF);
	write_ldv1000i(0x8F);	// '2', an even number	
	write_ldv1000i(0xFF);
	write_ldv1000i(0xFc);	// audio right

	write_ldv1000i(0xFF);
	write_ldv1000i(0xFc);	// toggle on right audio

	write_ldv1000i(0xFF);
	write_ldv1000i(0x4F);	// '3', an odd number
	write_ldv1000i(0xFF);
	write_ldv1000i(0xFc);	// audio right

}

TEST_CASE(ldv1000_audio)
{
	test_ldv1000_audio();
}

//////////////////////////////////////////////////////////

void test_ldv1000_extended_hello()
{
	MockLDV1000Test mockLDV1000;
	ldv1000_test_wrapper::setup(&mockLDV1000);

	{
		InSequence s;
		EXPECT_CALL(mockLDV1000, GetStatus()).WillRepeatedly(Return(LDV1000_STOPPED));
	}

	reset_ldv1000i(LDV1000_EMU_STANDARD);

	unsigned char ch;

	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(0xFC, ch);	// stopped and ready

	write_ldv1000i(0x90);	// send extended 'hello' command

	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(0xA2, ch);	// 'hello' response

	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(0x7c, ch);	// stopped and not ready

	write_ldv1000i(0x90);	// send extended 'hello' command while we are NOT ready

	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(0x7c, ch);	// stopped and not ready
}

TEST_CASE(ldv1000_extended_hello)
{
	test_ldv1000_extended_hello();
}

void test_ldv1000_extended_query_all_discs()
{
	uint8_t arrDiscs[] = { 0x12, 0x10, 0x34, 0 };

	MockLDV1000Test mockLDV1000;
	ldv1000_test_wrapper::setup(&mockLDV1000);

	EXPECT_CALL(mockLDV1000, QueryAvailableDiscs()).WillOnce(Return(arrDiscs));
	EXPECT_CALL(mockLDV1000, GetStatus()).WillRepeatedly(Return(LDV1000_STOPPED));

	reset_ldv1000i(LDV1000_EMU_STANDARD);

	unsigned char ch;

	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(0xFC, ch);	// stopped and ready

	write_ldv1000i(0x91);	// query all available discs

	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(arrDiscs[0], ch);	// dragon's lair

	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(arrDiscs[1], ch);	// dragon's lair 2

	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(arrDiscs[2], ch);	// space ace

	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(arrDiscs[3], ch);	// terminator

	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(0x7c, ch);	// stopped and not ready

	write_ldv1000i(0x91);	// send extended 'hello' command while we are NOT ready

	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(0x7c, ch);	// stopped and not ready
}

TEST_CASE(ldv1000_extended_query_all_discs)
{
	test_ldv1000_extended_query_all_discs();
}

void test_ldv1000_extended_query_active_disc()
{
	uint8_t activeDisc = { 0x12 };

	MockLDV1000Test mockLDV1000;
	ldv1000_test_wrapper::setup(&mockLDV1000);

	EXPECT_CALL(mockLDV1000, QueryActiveDisc()).WillOnce(Return(activeDisc));
	EXPECT_CALL(mockLDV1000, GetStatus()).WillRepeatedly(Return(LDV1000_STOPPED));

	reset_ldv1000i(LDV1000_EMU_STANDARD);

	unsigned char ch;

	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(0xFC, ch);	// stopped and ready

	write_ldv1000i(0x92);	// query active disc

	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(activeDisc, ch);	// dragon's lair

	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(0x7c, ch);	// stopped and not ready

	write_ldv1000i(0x92);	// re-send query while disc is not ready

	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(0x7c, ch);	// stopped and not ready
}

TEST_CASE(ldv1000_extended_query_active_disc)
{
	test_ldv1000_extended_query_active_disc();
}

void test_ldv1000_extended_change_disc_nop()
{
	uint8_t curDiscId = 0x12;

	MockLDV1000Test mockLDV1000;
	ldv1000_test_wrapper::setup(&mockLDV1000);

	EXPECT_CALL(mockLDV1000, QueryActiveDisc()).WillOnce(Return(curDiscId));
	EXPECT_CALL(mockLDV1000, GetStatus()).WillRepeatedly(Return(LDV1000_STOPPED));

	reset_ldv1000i(LDV1000_EMU_STANDARD);

	unsigned char ch;

	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(0xFC, ch);	// stopped and ready

	write_ldv1000i(0x93);	// prepare to switch discs

	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(0x7c, ch);	// stopped and not ready

	write_ldv1000i(curDiscId);	// this will be ignored because we aren't ready

	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(0x7c, ch);	// stopped and not ready

	// send no-entry
	write_ldv1000i(0xff);

	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(0xFC, ch);	// stopped and ready

	write_ldv1000i(curDiscId);	// issue command to switch to the disc that is already active

	// switch should 'instantly' succeed since we are already on this disc
	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(0x50, ch);

	// send no-entry
	write_ldv1000i(0xff);

	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(0xd0, ch);	// success
}

TEST_CASE(ldv1000_extended_change_disc_nop)
{
	test_ldv1000_extended_change_disc_nop();
}

void test_ldv1000_extended_change_disc_success()
{
	// these actual values don't matter at all; the only thing that matters is that they are different from each other
	uint8_t curDiscId = 0x12;
	uint8_t newDiscId = 0x10;
	uint8_t ignoredDiscId = 0x34;

	MockLDV1000Test mockLDV1000;
	ldv1000_test_wrapper::setup(&mockLDV1000);

	{
		InSequence dummy;
		EXPECT_CALL(mockLDV1000, GetStatus()).WillOnce(Return(LDV1000_STOPPED));
		EXPECT_CALL(mockLDV1000, QueryActiveDisc()).WillOnce(Return(curDiscId));
		EXPECT_CALL(mockLDV1000, BeginChangingToDisc(newDiscId));
		EXPECT_CALL(mockLDV1000, GetStatus()).Times(2).WillRepeatedly(Return(LDV1000_DISC_SWITCHING));	// so that we can test that there is an actual delay
		EXPECT_CALL(mockLDV1000, GetStatus()).WillOnce(Return(LDV1000_STOPPED));
		EXPECT_CALL(mockLDV1000, QueryActiveDisc()).WillOnce(Return(newDiscId));
	}

	reset_ldv1000i(LDV1000_EMU_STANDARD);

	unsigned char ch;

	ch = read_ldv1000i();
	TEST_REQUIRE_EQUAL(0xFC, ch);	// stopped and ready

	write_ldv1000i(0x93);	// prepare to switch discs
	write_ldv1000i(0xff);
	write_ldv1000i(newDiscId);	// issue command to switch to a new disc

	write_ldv1000i(0xff);
	write_ldv1000i(0x90);	// send 'hello' command, it should be returned (similar to get current frame overrides normal ld-v1000 status)
	ch = read_ldv1000i();
	TEST_REQUIRE_EQUAL(0xA2, ch);

	write_ldv1000i(0xff);
	write_ldv1000i(0x91);	// send 'query all discs' command, it should be ignored
	write_ldv1000i(0xff);	// send an extra 'FF' to make sure read status stays unready
	ch = read_ldv1000i();
	TEST_REQUIRE_EQUAL(0x50, ch);

	write_ldv1000i(0xff);
	write_ldv1000i(0x92);	// send 'query active disc' command, it should be ignored
	write_ldv1000i(0xff);	// send an extra 'FF' to make sure read status stays unready
	ch = read_ldv1000i();
	TEST_REQUIRE_EQUAL(0x50, ch);

	write_ldv1000i(0xff);
	write_ldv1000i(0x93);	// prepare to switch discs (this command should be ignored)
	write_ldv1000i(0xff);
	write_ldv1000i(ignoredDiscId);
	write_ldv1000i(0xff);

	ch = read_ldv1000i();
	TEST_REQUIRE_EQUAL(0xd0, ch);	// success

	// query active disc to make sure it is the one we expect
	write_ldv1000i(0x92);	// query active disc

	ch = read_ldv1000i();
	TEST_CHECK_EQUAL(newDiscId, ch);	// make sure it's not the ignored disc id
}

TEST_CASE(ldv1000_extended_change_disc_success)
{
	test_ldv1000_extended_change_disc_success();
}

void test_ldv1000_extended_enable_disable_delays()
{
	MockLDV1000Test mockLDV1000;
	ldv1000_test_wrapper::setup(&mockLDV1000);

	{
		InSequence s;
		EXPECT_CALL(mockLDV1000, ChangeSpinUpDelay(false));
		EXPECT_CALL(mockLDV1000, ChangeSpinUpDelay(true));
		EXPECT_CALL(mockLDV1000, ChangeSpinUpDelay(false));
		EXPECT_CALL(mockLDV1000, ChangeSeekDelay(false));
		EXPECT_CALL(mockLDV1000, ChangeSeekDelay(true));
		EXPECT_CALL(mockLDV1000, ChangeSeekDelay(false));
	}

	reset_ldv1000i(LDV1000_EMU_STANDARD);

	write_ldv1000i(0xff);
	write_ldv1000i(0x94);

	write_ldv1000i(0xff);
	write_ldv1000i(0x0f);
	write_ldv1000i(0xff);
	write_ldv1000i(0x94);

	write_ldv1000i(0xff);
	write_ldv1000i(0x3f);
	write_ldv1000i(0xff);
	write_ldv1000i(0x94);

	write_ldv1000i(0xff);
	write_ldv1000i(0x95);

	write_ldv1000i(0xff);
	write_ldv1000i(0x0f);
	write_ldv1000i(0xff);
	write_ldv1000i(0x95);

	write_ldv1000i(0xff);
	write_ldv1000i(0x3f);
	write_ldv1000i(0xff);
	write_ldv1000i(0x95);
}

TEST_CASE(ldv1000_extended_enable_disable_delays)
{
	test_ldv1000_extended_enable_disable_delays();
}

void test_ldv1000_enable_super_mode()
{
	MockLDV1000Test mockLDV1000;
	ldv1000_test_wrapper::setup(&mockLDV1000);

	{
		InSequence s;
		EXPECT_CALL(mockLDV1000, ChangeSuperMode(true));
	}

	reset_ldv1000i(LDV1000_EMU_STANDARD);

	write_ldv1000i(0xff);
	write_ldv1000i(0x9e);
}

TEST_CASE(ldv1000_enable_super_mode)
{
	test_ldv1000_enable_super_mode();
}

void test_ldv1000_disable_super_mode()
{
	MockLDV1000Test mockLDV1000;
	ldv1000_test_wrapper::setup(&mockLDV1000);

	{
		InSequence s;
		EXPECT_CALL(mockLDV1000, ChangeSuperMode(false));
	}

	reset_ldv1000i(LDV1000_EMU_SUPER);

	write_ldv1000i(0x9d);
}

TEST_CASE(ldv1000_disable_super_mode)
{
	test_ldv1000_disable_super_mode();
}

// Exercises every ldv1000_add_digit branch in write_ldv1000i.  Each digit
// command byte is sent and a search issued, verifying the correct frame is
// passed to BeginSearch.
void test_ldv1000_search_each_digit()
{
	const unsigned char cmds[10] = {
		0x3F, // 0
		0x0F, // 1
		0x8F, // 2
		0x4F, // 3
		0x2F, // 4
		0xAF, // 5
		0x6F, // 6
		0x1F, // 7
		0x9F, // 8
		0x5F  // 9
	};

	for (uint32_t uDigit = 0; uDigit < 10; uDigit++)
	{
		MockLDV1000Test mockLDV1000;
		ldv1000_test_wrapper::setup(&mockLDV1000);

		EXPECT_CALL(mockLDV1000, BeginSearch(uDigit));
		EXPECT_CALL(mockLDV1000, GetStatus()).WillRepeatedly(Return(LDV1000_SEARCHING));

		reset_ldv1000i(LDV1000_EMU_STANDARD);

		write_ldv1000i(cmds[uDigit]);
		write_ldv1000i(0xFF);	// no entry (become ready)
		write_ldv1000i(0xF7);	// search
	}
}

TEST_CASE(ldv1000_search_each_digit)
{
	test_ldv1000_search_each_digit();
}

// The clear command (0xBF) must actually empty the digit buffer; if the
// clear call is optimized away, a search after 'digits then clear' would
// still use the leftover digits.
void test_ldv1000_clear_command_empties_buffer()
{
	MockLDV1000Test mockLDV1000;
	ldv1000_test_wrapper::setup(&mockLDV1000);

	EXPECT_CALL(mockLDV1000, BeginSearch(0));
	EXPECT_CALL(mockLDV1000, GetStatus()).WillRepeatedly(Return(LDV1000_SEARCHING));

	reset_ldv1000i(LDV1000_EMU_STANDARD);

	// enter several digits (each digit write requires a 0xFF between to stay ready)
	write_ldv1000i(0x0F);	// 1
	write_ldv1000i(0xFF);
	write_ldv1000i(0x8F);	// 2
	write_ldv1000i(0xFF);
	write_ldv1000i(0x4F);	// 3
	write_ldv1000i(0xFF);

	// a 0xBF clear must wipe those digits
	write_ldv1000i(0xBF);	// clear
	write_ldv1000i(0xFF);	// become ready again

	write_ldv1000i(0xF7);	// search -> should be frame 0, not 123
}

TEST_CASE(ldv1000_clear_command_empties_buffer)
{
	test_ldv1000_clear_command_empties_buffer();
}

// After a search, the digit buffer must be cleared so a subsequent short
// search does not inherit stale digits.
void test_ldv1000_search_clears_buffer()
{
	MockLDV1000Test mockLDV1000;
	ldv1000_test_wrapper::setup(&mockLDV1000);

	{
		InSequence s;
		EXPECT_CALL(mockLDV1000, BeginSearch(999));
		EXPECT_CALL(mockLDV1000, GetStatus()).WillRepeatedly(Return(LDV1000_SEARCHING));
		EXPECT_CALL(mockLDV1000, BeginSearch(1));
		EXPECT_CALL(mockLDV1000, GetStatus()).WillRepeatedly(Return(LDV1000_SEARCHING));
	}

	reset_ldv1000i(LDV1000_EMU_STANDARD);

	write_ldv1000i(0x5F);	// 9
	write_ldv1000i(0xFF);
	write_ldv1000i(0x5F);	// 9
	write_ldv1000i(0xFF);
	write_ldv1000i(0x5F);	// 9
	write_ldv1000i(0xFF);
	write_ldv1000i(0xF7);	// search -> 999

	// A fresh single-digit search must go to 1, not 9991 (or similar)
	write_ldv1000i(0xFF);	// become ready (after search output is 0x50)
	write_ldv1000i(0x0F);	// 1
	write_ldv1000i(0xFF);
	write_ldv1000i(0xF7);	// search -> 1
}

TEST_CASE(ldv1000_search_clears_buffer)
{
	test_ldv1000_search_clears_buffer();
}

// Autostop command (0xF3) reads the buffered frame AND must clear the buffer
// so that subsequent commands depending on the digit buffer start fresh.
void test_ldv1000_autostop_clears_buffer()
{
	MockLDV1000Test mockLDV1000;
	ldv1000_test_wrapper::setup(&mockLDV1000);

	// autostop should kick in play, then later BeginSearch(0) should fire for
	// the subsequent 0xF7 search (proving the digit buffer was cleared)
	EXPECT_CALL(mockLDV1000, Play());
	EXPECT_CALL(mockLDV1000, BeginSearch(0));
	EXPECT_CALL(mockLDV1000, GetCurFrameNum()).WillRepeatedly(Return(0));
	EXPECT_CALL(mockLDV1000, GetStatus()).WillRepeatedly(Return(LDV1000_SEARCHING));

	reset_ldv1000i(LDV1000_EMU_STANDARD);

	write_ldv1000i(0x0F);	// 1
	write_ldv1000i(0xFF);
	write_ldv1000i(0x8F);	// 2
	write_ldv1000i(0xFF);
	write_ldv1000i(0x4F);	// 3
	write_ldv1000i(0xFF);
	write_ldv1000i(0xF3);	// autostop - consumes the buffer

	// now start a fresh search with no digits - should land on frame 0
	write_ldv1000i(0xFF);	// no entry (we were mid-autostop - become ready)
	write_ldv1000i(0xF7);	// search -> 0
}

TEST_CASE(ldv1000_autostop_clears_buffer)
{
	test_ldv1000_autostop_clears_buffer();
}

// The change_spinup_delay (0x94) extended command uses the last buffered digit
// (bit 0) and then must clear the digit buffer.  Without the clear, a second
// 0x94 sent with no fresh digit would re-use the stale digit instead of the
// cleared (zero) value.
void test_ldv1000_spinup_delay_clears_buffer()
{
	MockLDV1000Test mockLDV1000;
	ldv1000_test_wrapper::setup(&mockLDV1000);

	{
		InSequence s;
		EXPECT_CALL(mockLDV1000, ChangeSpinUpDelay(true));	// from digit '1'
		EXPECT_CALL(mockLDV1000, ChangeSpinUpDelay(false));	// no digit, cleared -> 0
	}

	reset_ldv1000i(LDV1000_EMU_STANDARD);

	write_ldv1000i(0xFF);
	write_ldv1000i(0x0F);	// digit '1'
	write_ldv1000i(0xFF);
	write_ldv1000i(0x94);	// change_spinup_delay(true) and clear buffer

	// no digit now; without the clear, buffer would still have '1'
	write_ldv1000i(0xFF);
	write_ldv1000i(0x94);	// change_spinup_delay(false)
}

TEST_CASE(ldv1000_spinup_delay_clears_buffer)
{
	test_ldv1000_spinup_delay_clears_buffer();
}

// Same as above but for the change_seek_delay (0x95) command.
void test_ldv1000_seek_delay_clears_buffer()
{
	MockLDV1000Test mockLDV1000;
	ldv1000_test_wrapper::setup(&mockLDV1000);

	{
		InSequence s;
		EXPECT_CALL(mockLDV1000, ChangeSeekDelay(true));	// from digit '1'
		EXPECT_CALL(mockLDV1000, ChangeSeekDelay(false));	// cleared buffer -> 0
	}

	reset_ldv1000i(LDV1000_EMU_STANDARD);

	write_ldv1000i(0xFF);
	write_ldv1000i(0x0F);	// digit '1'
	write_ldv1000i(0xFF);
	write_ldv1000i(0x95);	// change_seek_delay(true) and clear

	write_ldv1000i(0xFF);
	write_ldv1000i(0x95);	// change_seek_delay(false)
}

TEST_CASE(ldv1000_seek_delay_clears_buffer)
{
	test_ldv1000_seek_delay_clears_buffer();
}

// Skip forward command 0xB1..0xBa computes tracks_to_skip as 10 * (value & 0x0f).
// Using a command with different low nibbles ensures the bitmask and multiply
// are both checked.
void test_ldv1000_skip_forward_values()
{
	MockLDV1000Test mockLDV1000;
	ldv1000_test_wrapper::setup(&mockLDV1000);

	{
		InSequence s;
		EXPECT_CALL(mockLDV1000, SkipForward(30));	// 0xB3
		EXPECT_CALL(mockLDV1000, SkipForward(70));	// 0xB7
		EXPECT_CALL(mockLDV1000, SkipForward(100));	// 0xBa
	}

	reset_ldv1000i(LDV1000_EMU_STANDARD);

	write_ldv1000i(0xB3);
	write_ldv1000i(0xFF);
	write_ldv1000i(0xB7);
	write_ldv1000i(0xFF);
	write_ldv1000i(0xBa);
}

TEST_CASE(ldv1000_skip_forward_values)
{
	test_ldv1000_skip_forward_values();
}

// Skip backward command 0x31..0x39 computes tracks_to_skip as 10 * (value & 0x0f).
void test_ldv1000_skip_backward_values()
{
	MockLDV1000Test mockLDV1000;
	ldv1000_test_wrapper::setup(&mockLDV1000);

	{
		InSequence s;
		EXPECT_CALL(mockLDV1000, SkipBackward(20));	// 0x32
		EXPECT_CALL(mockLDV1000, SkipBackward(80));	// 0x38
	}

	reset_ldv1000i(LDV1000_EMU_STANDARD);

	write_ldv1000i(0x32);
	write_ldv1000i(0xFF);
	write_ldv1000i(0x38);
}

TEST_CASE(ldv1000_skip_backward_values)
{
	test_ldv1000_skip_backward_values();
}
