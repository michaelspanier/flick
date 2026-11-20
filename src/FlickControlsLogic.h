/*
 * Copyright 2026, Michael Spanier.
 * Distributed under the terms of the MIT License.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef FLICK_CONTROLS_LOGIC_H
#define FLICK_CONTROLS_LOGIC_H


#include <math.h>
#include <stdint.h>
#include <string.h>


static const int32_t kFlickMaxSliderSteps = 100000;
static const int32_t kFlickDefaultSliderSteps = 1000;


/*!	Tells whether a group of the camera's parameter web is left out of the
	controls: the info text and the format list, because Flick sets the size
	itself.
*/
static inline bool
flick_control_group_is_hidden(const char* name)
{
	return name == NULL || name[0] == '\0' || strcmp(name, "Info") == 0
		|| strcmp(name, "Format") == 0;
}


/*!	How many steps a slider for the range \a minimum to \a maximum has, when
	the camera counts in steps of \a step. Without a usable step the slider
	gets a fixed number of steps. Returns 0 for a range that has no width:
	there is nothing to move.
*/
static inline int32_t
flick_slider_steps(float minimum, float maximum, float step)
{
	if (!(maximum > minimum))
		return 0;

	if (step > 0.0f) {
		float steps = floorf((maximum - minimum) / step + 0.5f);
		if (steps >= 1.0f && steps <= (float)kFlickMaxSliderSteps)
			return (int32_t)steps;
	}
	return kFlickDefaultSliderSteps;
}


//!	The slider position (0 to \a steps) that shows \a value.
static inline int32_t
flick_slider_position(float value, float minimum, float maximum, int32_t steps)
{
	if (steps <= 0 || !(maximum > minimum) || !(value == value))
		return 0;
	if (value <= minimum)
		return 0;
	if (value >= maximum)
		return steps;

	return (int32_t)floorf((value - minimum) / (maximum - minimum)
		* (float)steps + 0.5f);
}


//!	The parameter value that a slider \a position stands for.
static inline float
flick_slider_value(int32_t position, float minimum, float maximum,
	int32_t steps)
{
	if (steps <= 0 || position <= 0)
		return minimum;
	if (position >= steps)
		return maximum;

	return minimum + (maximum - minimum) * (float)position / (float)steps;
}


#endif	// FLICK_CONTROLS_LOGIC_H
