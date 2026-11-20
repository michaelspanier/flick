/*
 * Copyright 2026, Michael Spanier.
 * Distributed under the terms of the MIT License.
 *
 * SPDX-License-Identifier: MIT
 */


#include "FlickControlsLogic.h"

#include <stdio.h>


static int sFailures = 0;
static int sTests = 0;


#define CHECK(condition) \
	do { \
		if (!(condition)) { \
			printf("  FAILED %s:%d: %s\n", __FILE__, __LINE__, #condition); \
			sFailures++; \
		} \
	} while (0)


static void
test_info_and_format_groups_are_hidden()
{
	CHECK(flick_control_group_is_hidden("Info"));
	CHECK(flick_control_group_is_hidden("Format"));
	CHECK(flick_control_group_is_hidden(""));
	CHECK(flick_control_group_is_hidden(NULL));
}


static void
test_picture_groups_are_shown()
{
	CHECK(!flick_control_group_is_hidden("Brightness"));
	CHECK(!flick_control_group_is_hidden("White Balance Temperature"));
	CHECK(!flick_control_group_is_hidden("Exposure"));
	CHECK(!flick_control_group_is_hidden("info"));
}


static void
test_steps_follow_the_step_of_the_camera()
{
	CHECK(flick_slider_steps(0.0f, 255.0f, 1.0f) == 255);
	CHECK(flick_slider_steps(2800.0f, 6500.0f, 1.0f) == 3700);
	CHECK(flick_slider_steps(-64.0f, 64.0f, 1.0f) == 128);
	CHECK(flick_slider_steps(0.0f, 100.0f, 5.0f) == 20);
}


static void
test_steps_fall_back_to_a_fixed_number()
{
	CHECK(flick_slider_steps(0.0f, 1.0f, 0.0f) == 1000);
	CHECK(flick_slider_steps(0.0f, 1.0f, -1.0f) == 1000);
	CHECK(flick_slider_steps(0.0f, 1000000.0f, 1.0f) == 1000);
	CHECK(flick_slider_steps(0.0f, 1.0f, 100.0f) == 1000);
}


static void
test_range_without_width_has_no_steps()
{
	CHECK(flick_slider_steps(5.0f, 5.0f, 1.0f) == 0);
	CHECK(flick_slider_steps(10.0f, 5.0f, 1.0f) == 0);
}


static void
test_position_shows_the_value()
{
	CHECK(flick_slider_position(0.0f, 0.0f, 255.0f, 255) == 0);
	CHECK(flick_slider_position(255.0f, 0.0f, 255.0f, 255) == 255);
	CHECK(flick_slider_position(100.0f, 0.0f, 255.0f, 255) == 100);
	CHECK(flick_slider_position(0.0f, -64.0f, 64.0f, 128) == 64);
}


static void
test_position_stays_inside_the_slider()
{
	CHECK(flick_slider_position(-5.0f, 0.0f, 255.0f, 255) == 0);
	CHECK(flick_slider_position(999.0f, 0.0f, 255.0f, 255) == 255);
	CHECK(flick_slider_position(50.0f, 0.0f, 100.0f, 0) == 0);
}


static void
test_not_a_number_gives_the_first_position()
{
	float notANumber = 0.0f / 0.0f;
	CHECK(flick_slider_position(notANumber, 0.0f, 100.0f, 100) == 0);
}


static void
test_value_of_a_position_reaches_both_ends()
{
	CHECK(flick_slider_value(0, 10.0f, 90.0f, 80) == 10.0f);
	CHECK(flick_slider_value(80, 10.0f, 90.0f, 80) == 90.0f);
	CHECK(flick_slider_value(-3, 10.0f, 90.0f, 80) == 10.0f);
	CHECK(flick_slider_value(500, 10.0f, 90.0f, 80) == 90.0f);
}


static void
test_value_of_a_position_is_in_between()
{
	CHECK(flick_slider_value(40, 10.0f, 90.0f, 80) == 50.0f);
	CHECK(flick_slider_value(64, -64.0f, 64.0f, 128) == 0.0f);
}


static void
test_position_and_value_agree_for_whole_steps()
{
	for (int32_t position = 0; position <= 3700; position += 37) {
		float value = flick_slider_value(position, 2800.0f, 6500.0f, 3700);
		CHECK(flick_slider_position(value, 2800.0f, 6500.0f, 3700) == position);
	}
}


static void
run(const char* name, void (*test)())
{
	int before = sFailures;
	test();
	sTests++;
	printf("%s %s\n", sFailures == before ? "ok    " : "FAILED", name);
}


int
main()
{
	run("info and format groups are hidden",
		test_info_and_format_groups_are_hidden);
	run("picture groups are shown", test_picture_groups_are_shown);
	run("steps follow the step of the camera",
		test_steps_follow_the_step_of_the_camera);
	run("steps fall back to a fixed number",
		test_steps_fall_back_to_a_fixed_number);
	run("range without width has no steps",
		test_range_without_width_has_no_steps);
	run("position shows the value", test_position_shows_the_value);
	run("position stays inside the slider",
		test_position_stays_inside_the_slider);
	run("not a number gives the first position",
		test_not_a_number_gives_the_first_position);
	run("value of a position reaches both ends",
		test_value_of_a_position_reaches_both_ends);
	run("value of a position is in between",
		test_value_of_a_position_is_in_between);
	run("position and value agree for whole steps",
		test_position_and_value_agree_for_whole_steps);

	printf("%d tests, %d failures\n", sTests, sFailures);
	return sFailures == 0 ? 0 : 1;
}
