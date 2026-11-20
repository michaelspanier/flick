/*
 * Copyright 2026, Michael Spanier.
 * Distributed under the terms of the MIT License.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef FLICK_ICONS_H
#define FLICK_ICONS_H


#include <SupportDefs.h>

class BBitmap;


enum flick_icon {
	FLICK_ICON_SELF_TIMER,
	FLICK_ICON_MIRROR,
	FLICK_ICON_SETTINGS
};


/*!	Draws \a icon in the color of control texts, \a size pixels wide and high.
	The caller owns the bitmap. Returns NULL if there is not enough memory.
*/
BBitmap*	flick_create_icon(flick_icon icon, float size);


#endif	// FLICK_ICONS_H
