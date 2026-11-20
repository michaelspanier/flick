/*
 * Copyright 2026, Michael Spanier.
 * Distributed under the terms of the MIT License.
 *
 * SPDX-License-Identifier: MIT
 */


#include "FlickLogic.h"

#include <stdio.h>
#include <string.h>
#include <time.h>


static int sFailures = 0;
static int sTests = 0;


#define CHECK(condition) \
	do { \
		if (!(condition)) { \
			printf("  FAILED %s:%d: %s\n", __FILE__, __LINE__, #condition); \
			sFailures++; \
		} \
	} while (0)


static time_t
make_time(int year, int month, int day, int hour, int minute, int second)
{
	struct tm parts;
	memset(&parts, 0, sizeof(parts));
	parts.tm_year = year - 1900;
	parts.tm_mon = month - 1;
	parts.tm_mday = day;
	parts.tm_hour = hour;
	parts.tm_min = minute;
	parts.tm_sec = second;
	parts.tm_isdst = -1;
	return mktime(&parts);
}


static void
test_file_name_has_date_and_time()
{
	char name[64];
	size_t length = flick_photo_file_name(name, sizeof(name),
		make_time(2026, 11, 19, 23, 20, 45), 0);
	CHECK(strcmp(name, "Flick 2026-11-19 23.20.45.png") == 0);
	CHECK(length == strlen(name));
}


static void
test_file_name_numbers_a_second_photo_in_the_same_second()
{
	char name[64];
	flick_photo_file_name(name, sizeof(name),
		make_time(2026, 11, 19, 23, 20, 45), 1);
	CHECK(strcmp(name, "Flick 2026-11-19 23.20.45 2.png") == 0);
}


static void
test_file_name_is_cut_not_overrun()
{
	char name[10];
	memset(name, 'x', sizeof(name));
	flick_photo_file_name(name, sizeof(name),
		make_time(2026, 11, 19, 23, 20, 45), 0);
	CHECK(name[sizeof(name) - 1] == '\0');
}


static void
test_file_name_with_no_room_writes_nothing()
{
	char name[1] = { 'x' };
	size_t length = flick_photo_file_name(name, 0,
		make_time(2026, 11, 19, 23, 20, 45), 0);
	CHECK(length == 0);
	CHECK(name[0] == 'x');
}


static void
test_size_label()
{
	char label[32];
	flick_size_label(label, sizeof(label), 640, 480);
	CHECK(strcmp(label, "640 \xc3\x97 480") == 0);
	flick_size_label(label, sizeof(label), 1280, 720);
	CHECK(strcmp(label, "1280 \xc3\x97 720") == 0);
}


static void
test_default_size_prefers_640_by_480()
{
	const flick_size sizes[] = { {160, 120}, {320, 240}, {640, 480},
		{1280, 720} };
	CHECK(flick_default_size_index(sizes, 4) == 2);
}


static void
test_default_size_without_640_by_480_takes_the_largest_up_to_720()
{
	const flick_size sizes[] = { {320, 240}, {1920, 1080}, {1280, 720},
		{800, 600} };
	CHECK(flick_default_size_index(sizes, 4) == 2);
}


static void
test_default_size_takes_the_smallest_if_everything_is_above_720()
{
	const flick_size sizes[] = { {3840, 2160}, {1920, 1080} };
	CHECK(flick_default_size_index(sizes, 2) == 1);
}


static void
test_default_size_of_an_empty_list_is_invalid()
{
	CHECK(flick_default_size_index(NULL, 0) == -1);
}


static void
test_sorted_sizes_go_from_small_to_large_without_duplicates()
{
	flick_size sizes[] = { {640, 480}, {160, 120}, {640, 480}, {1280, 720},
		{320, 240} };
	int count = flick_sort_sizes(sizes, 5);
	CHECK(count == 4);
	CHECK(sizes[0].width == 160 && sizes[0].height == 120);
	CHECK(sizes[1].width == 320 && sizes[1].height == 240);
	CHECK(sizes[2].width == 640 && sizes[2].height == 480);
	CHECK(sizes[3].width == 1280 && sizes[3].height == 720);
}


static void
test_rate_is_only_reported_after_a_full_second()
{
	flick_rate_meter meter;
	flick_rate_start(&meter, 0);
	int rate = -1;
	CHECK(!flick_rate_add_frame(&meter, 500000, &rate));
	CHECK(rate == -1);
}


static void
test_rate_counts_frames_per_second()
{
	flick_rate_meter meter;
	flick_rate_start(&meter, 0);
	int rate = -1;
	for (int i = 1; i < 30; i++)
		CHECK(!flick_rate_add_frame(&meter, i * 33333, &rate));
	CHECK(flick_rate_add_frame(&meter, 1000000, &rate));
	CHECK(rate == 30);
}


static void
test_rate_is_rounded_to_whole_frames()
{
	flick_rate_meter meter;
	flick_rate_start(&meter, 0);
	int rate = -1;
	for (int i = 1; i < 10; i++)
		flick_rate_add_frame(&meter, i * 100000, &rate);
	// Ten frames in 1.05 seconds are 9.5 frames per second.
	CHECK(flick_rate_add_frame(&meter, 1050000, &rate));
	CHECK(rate == 10);
}


static void
test_rate_starts_a_new_second_after_reporting()
{
	flick_rate_meter meter;
	flick_rate_start(&meter, 0);
	int rate = -1;
	for (int i = 1; i <= 20; i++)
		flick_rate_add_frame(&meter, i * 50000, &rate);
	CHECK(rate == 20);
	for (int i = 1; i <= 10; i++)
		flick_rate_add_frame(&meter, 1000000 + i * 100000, &rate);
	CHECK(rate == 10);
}



static void
test_small_movement_is_a_click_not_a_drag()
{
	CHECK(!flick_is_drag(0, 0));
	CHECK(!flick_is_drag(4, -4));
	CHECK(flick_is_drag(5, 0));
	CHECK(flick_is_drag(0, -5));
}


static void
test_drag_image_keeps_proportions_within_the_limit()
{
	flick_size size = flick_fit_size(1280, 720, 160);
	CHECK(size.width == 160 && size.height == 90);

	size = flick_fit_size(480, 640, 160);
	CHECK(size.width == 120 && size.height == 160);
}


static void
test_drag_image_is_never_enlarged_or_empty()
{
	flick_size size = flick_fit_size(100, 50, 160);
	CHECK(size.width == 100 && size.height == 50);

	size = flick_fit_size(1000, 1, 160);
	CHECK(size.width == 160 && size.height == 1);

	size = flick_fit_size(0, 0, 160);
	CHECK(size.width == 0 && size.height == 0);
}


static void
test_size_from_camera_parameter()
{
	// The camera add-on publishes width << 16 | height (CamVideoSize.h).
	flick_size size = { 0, 0 };
	CHECK(flick_size_from_parameter(1920 << 16 | 1080, &size));
	CHECK(size.width == 1920 && size.height == 1080);
}


static void
test_parameter_without_a_side_is_no_size()
{
	flick_size size = { 7, 7 };
	CHECK(!flick_size_from_parameter(0, &size));
	CHECK(!flick_size_from_parameter(640 << 16, &size));
	CHECK(!flick_size_from_parameter(480, &size));
	CHECK(size.width == 7 && size.height == 7);
}


static void
test_countdown_shoots_after_the_last_second()
{
	flick_countdown countdown;
	flick_countdown_start(&countdown, 3);
	CHECK(flick_countdown_running(&countdown));
	CHECK(countdown.left == 3);
	CHECK(!flick_countdown_tick(&countdown));
	CHECK(countdown.left == 2);
	CHECK(!flick_countdown_tick(&countdown));
	CHECK(countdown.left == 1);
	CHECK(flick_countdown_tick(&countdown));
	CHECK(!flick_countdown_running(&countdown));
}


static void
test_stopped_countdown_never_shoots()
{
	flick_countdown countdown;
	flick_countdown_start(&countdown, 3);
	flick_countdown_stop(&countdown);
	CHECK(!flick_countdown_running(&countdown));
	CHECK(!flick_countdown_tick(&countdown));
	CHECK(!flick_countdown_tick(&countdown));
	CHECK(countdown.left == 0);
}


static void
test_flash_fades_from_white_to_nothing()
{
	CHECK(flick_flash_alpha(0, 300000) == 255);
	CHECK(flick_flash_alpha(150000, 300000) == 127);
	CHECK(flick_flash_alpha(300000, 300000) == 0);
	CHECK(flick_flash_alpha(900000, 300000) == 0);
	CHECK(flick_flash_alpha(-5, 300000) == 0);
	CHECK(flick_flash_alpha(10, 0) == 0);
}


#define RUN(test) \
	do { \
		int before = sFailures; \
		test(); \
		sTests++; \
		printf("%s %s\n", sFailures == before ? "ok  " : "FAIL", #test); \
	} while (0)


int
main()
{
	RUN(test_file_name_has_date_and_time);
	RUN(test_file_name_numbers_a_second_photo_in_the_same_second);
	RUN(test_file_name_is_cut_not_overrun);
	RUN(test_file_name_with_no_room_writes_nothing);
	RUN(test_size_label);
	RUN(test_default_size_prefers_640_by_480);
	RUN(test_default_size_without_640_by_480_takes_the_largest_up_to_720);
	RUN(test_default_size_takes_the_smallest_if_everything_is_above_720);
	RUN(test_default_size_of_an_empty_list_is_invalid);
	RUN(test_sorted_sizes_go_from_small_to_large_without_duplicates);
	RUN(test_rate_is_only_reported_after_a_full_second);
	RUN(test_rate_counts_frames_per_second);
	RUN(test_rate_is_rounded_to_whole_frames);
	RUN(test_rate_starts_a_new_second_after_reporting);
	RUN(test_small_movement_is_a_click_not_a_drag);
	RUN(test_drag_image_keeps_proportions_within_the_limit);
	RUN(test_drag_image_is_never_enlarged_or_empty);
	RUN(test_size_from_camera_parameter);
	RUN(test_parameter_without_a_side_is_no_size);
	RUN(test_countdown_shoots_after_the_last_second);
	RUN(test_stopped_countdown_never_shoots);
	RUN(test_flash_fades_from_white_to_nothing);

	printf("%d tests, %d failures\n", sTests, sFailures);
	return sFailures == 0 ? 0 : 1;
}
