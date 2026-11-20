/*
 * Copyright 2026, Michael Spanier.
 * Distributed under the terms of the MIT License.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef FLICK_LOGIC_H
#define FLICK_LOGIC_H


#include <stddef.h>
#include <stdint.h>
#include <time.h>


struct flick_size {
			uint32_t			width;
			uint32_t			height;
};


/*!	Writes the file name for a photo taken at \a when, for example
	"Flick 2026-11-19 23.20.45.png". \a duplicate is 0 for the first photo
	of a second; a larger number is shown as " 2", " 3" and so on, so that two
	photos in the same second do not overwrite each other. The name is cut to
	fit \a size. Returns the length of the name.
*/
size_t	flick_photo_file_name(char* buffer, size_t size, time_t when,
			int duplicate);

//!	Writes a label like "640 x 480" (with the multiplication sign).
size_t	flick_size_label(char* buffer, size_t size, uint32_t width,
			uint32_t height);

/*!	Chooses the size a camera starts with: 640 x 480 if it has it, otherwise
	the largest one that is not bigger than 1280 x 720, otherwise the smallest.
	Returns the index into \a sizes, or -1 if there is none.
*/
int		flick_default_size_index(const flick_size* sizes, int count);

/*!	Sorts \a sizes from small to large and removes duplicates. Returns the
	new number of sizes.
*/
int		flick_sort_sizes(flick_size* sizes, int count);

/*!	Reads a size from an item of the camera's B_RESOLUTION parameter: width in
	the upper, height in the lower 16 bits. Returns false and leaves \a size
	alone if the value holds no size.
*/
bool	flick_size_from_parameter(int32_t value, flick_size* size);


//!	Counts shown frames over one second at a time. Times are in microseconds.
struct flick_rate_meter {
			int64_t				start;
			int					frames;
};

void	flick_rate_start(flick_rate_meter* meter, int64_t now);

/*!	Counts one frame shown at \a now. When a second or more has passed, writes
	the rounded frames per second to \a rate, starts a new second and returns
	true.
*/
bool	flick_rate_add_frame(flick_rate_meter* meter, int64_t now, int* rate);


//!	Tells whether the mouse moved far enough with the button down to drag.
bool	flick_is_drag(float dx, float dy);

/*!	Returns the size of a picture of \a width x \a height made to fit into
	\a limit x \a limit with its proportions. It is never made larger, and a
	side that is not empty does not become empty.
*/
flick_size	flick_fit_size(uint32_t width, uint32_t height, uint32_t limit);


//!	The self-timer: counts whole seconds down to the photo.
struct flick_countdown {
			int					left;
};

void	flick_countdown_start(flick_countdown* countdown, int seconds);
void	flick_countdown_stop(flick_countdown* countdown);
bool	flick_countdown_running(const flick_countdown* countdown);

/*!	One second has passed. Returns true when the photo is due; the countdown
	has then ended. A countdown that does not run never returns true.
*/
bool	flick_countdown_tick(flick_countdown* countdown);


/*!	Returns how strongly the flash covers the picture \a elapsed microseconds
	after the photo: 255 at the start, fading to 0 after \a length.
*/
uint8_t	flick_flash_alpha(int64_t elapsed, int64_t length);


#endif	// FLICK_LOGIC_H
