/*
 * Copyright 2026, Michael Spanier.
 * Distributed under the terms of the MIT License.
 *
 * SPDX-License-Identifier: MIT
 */


#include "FlickLogic.h"

#include <stdio.h>
#include <string.h>


static uint64_t
area_of(const flick_size& size)
{
	return (uint64_t)size.width * size.height;
}


size_t
flick_photo_file_name(char* buffer, size_t size, time_t when, int duplicate)
{
	if (size == 0)
		return 0;

	struct tm parts;
	localtime_r(&when, &parts);

	char date[32];
	strftime(date, sizeof(date), "%Y-%m-%d %H.%M.%S", &parts);

	if (duplicate > 0)
		snprintf(buffer, size, "Flick %s %d.png", date, duplicate + 1);
	else
		snprintf(buffer, size, "Flick %s.png", date);

	return strlen(buffer);
}


size_t
flick_size_label(char* buffer, size_t size, uint32_t width, uint32_t height)
{
	if (size == 0)
		return 0;

	snprintf(buffer, size, "%u \xc3\x97 %u", (unsigned)width,
		(unsigned)height);
	return strlen(buffer);
}


int
flick_default_size_index(const flick_size* sizes, int count)
{
	if (sizes == NULL || count <= 0)
		return -1;

	int largestFitting = -1;
	int smallest = 0;
	for (int i = 0; i < count; i++) {
		if (sizes[i].width == 640 && sizes[i].height == 480)
			return i;

		if (sizes[i].width <= 1280 && sizes[i].height <= 720
			&& (largestFitting < 0
				|| area_of(sizes[i]) > area_of(sizes[largestFitting]))) {
			largestFitting = i;
		}
		if (area_of(sizes[i]) < area_of(sizes[smallest]))
			smallest = i;
	}

	return largestFitting >= 0 ? largestFitting : smallest;
}


int
flick_sort_sizes(flick_size* sizes, int count)
{
	for (int i = 1; i < count; i++) {
		flick_size current = sizes[i];
		int j = i - 1;
		while (j >= 0 && area_of(sizes[j]) > area_of(current)) {
			sizes[j + 1] = sizes[j];
			j--;
		}
		sizes[j + 1] = current;
	}

	int unique = 0;
	for (int i = 0; i < count; i++) {
		if (unique > 0 && sizes[unique - 1].width == sizes[i].width
			&& sizes[unique - 1].height == sizes[i].height) {
			continue;
		}
		sizes[unique++] = sizes[i];
	}
	return unique;
}


static const int kParameterSizeShift = 16;
static const uint32_t kParameterSizeMask = 0xffff;


bool
flick_size_from_parameter(int32_t value, flick_size* size)
{
	uint32_t width = ((uint32_t)value >> kParameterSizeShift)
		& kParameterSizeMask;
	uint32_t height = (uint32_t)value & kParameterSizeMask;
	if (width == 0 || height == 0)
		return false;

	size->width = width;
	size->height = height;
	return true;
}


static const int64_t kMicrosecondsPerSecond = 1000000;


void
flick_rate_start(flick_rate_meter* meter, int64_t now)
{
	meter->start = now;
	meter->frames = 0;
}


bool
flick_rate_add_frame(flick_rate_meter* meter, int64_t now, int* rate)
{
	meter->frames++;
	int64_t elapsed = now - meter->start;
	if (elapsed < kMicrosecondsPerSecond)
		return false;

	*rate = (int)((meter->frames * kMicrosecondsPerSecond + elapsed / 2)
		/ elapsed);
	flick_rate_start(meter, now);
	return true;
}


static const float kDragDistance = 4;


bool
flick_is_drag(float dx, float dy)
{
	return dx > kDragDistance || dx < -kDragDistance
		|| dy > kDragDistance || dy < -kDragDistance;
}


static uint32_t
scaled_side(uint32_t side, uint32_t limit, uint32_t longest)
{
	uint32_t scaled = (uint32_t)((uint64_t)side * limit / longest);
	return scaled == 0 && side > 0 ? 1 : scaled;
}


flick_size
flick_fit_size(uint32_t width, uint32_t height, uint32_t limit)
{
	uint32_t longest = width > height ? width : height;
	if (longest <= limit)
		return flick_size{ width, height };

	return flick_size{ scaled_side(width, limit, longest),
		scaled_side(height, limit, longest) };
}


void
flick_countdown_start(flick_countdown* countdown, int seconds)
{
	countdown->left = seconds > 0 ? seconds : 0;
}


void
flick_countdown_stop(flick_countdown* countdown)
{
	countdown->left = 0;
}


bool
flick_countdown_running(const flick_countdown* countdown)
{
	return countdown->left > 0;
}


bool
flick_countdown_tick(flick_countdown* countdown)
{
	if (countdown->left <= 0)
		return false;
	countdown->left--;
	return countdown->left == 0;
}


uint8_t
flick_flash_alpha(int64_t elapsed, int64_t length)
{
	if (elapsed < 0 || elapsed >= length)
		return 0;
	return (uint8_t)(255 * (length - elapsed) / length);
}
