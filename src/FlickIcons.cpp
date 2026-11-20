/*
 * Copyright 2026, Michael Spanier.
 * Distributed under the terms of the MIT License.
 *
 * SPDX-License-Identifier: MIT
 */


#include "FlickIcons.h"

#include <Bitmap.h>
#include <InterfaceDefs.h>
#include <View.h>

#include <math.h>
#include <string.h>


//!	A stopwatch: a ring, the button on top and one hand.
static void
draw_self_timer(BView* view, float size)
{
	float pen = max_c(1.5f, size / 12);
	BPoint center(size / 2, size * 0.56f);
	float radius = size * 0.36f;
	view->SetPenSize(pen);
	view->StrokeEllipse(center, radius, radius);
	view->FillRect(BRect(size * 0.42f, size * 0.04f, size * 0.58f,
		size * 0.14f));
	view->StrokeLine(BPoint(center.x, center.y - radius),
		BPoint(center.x, center.y - radius - size * 0.06f));
	view->StrokeLine(center, BPoint(center.x + radius * 0.5f,
		center.y - radius * 0.6f));
}


//!	Two arrow heads facing away from a line in the middle.
static void
draw_mirror(BView* view, float size)
{
	float middle = size / 2;
	float gap = size * 0.09f;
	float top = size * 0.2f;
	float bottom = size * 0.8f;
	BPoint left[3] = { BPoint(middle - gap, top), BPoint(middle - gap, bottom),
		BPoint(size * 0.06f, size / 2) };
	BPoint right[3] = { BPoint(middle + gap, top), BPoint(middle + gap, bottom),
		BPoint(size * 0.94f, size / 2) };
	view->FillPolygon(left, 3);
	view->SetPenSize(max_c(1.0f, size / 16));
	view->StrokePolygon(right, 3);

	float dash = size / 8;
	for (float y = 0; y < size; y += dash * 2)
		view->StrokeLine(BPoint(middle, y), BPoint(middle, y + dash - 1));
}


//!	Three sliders with their knobs at different places.
static void
draw_settings(BView* view, float size)
{
	static const float kKnobs[3] = { 0.3f, 0.68f, 0.45f };
	float pen = max_c(1.0f, size / 14);
	float knob = size * 0.11f;
	view->SetPenSize(pen);
	for (int i = 0; i < 3; i++) {
		float y = size * (0.2f + 0.3f * i);
		view->StrokeLine(BPoint(size * 0.08f, y), BPoint(size * 0.92f, y));
		view->FillEllipse(BPoint(size * kKnobs[i], y), knob, knob);
	}
}


BBitmap*
flick_create_icon(flick_icon icon, float size)
{
	size = floorf(size);
	BRect bounds(0, 0, size - 1, size - 1);
	BBitmap* bitmap = new BBitmap(bounds, B_BITMAP_ACCEPTS_VIEWS, B_RGBA32);
	if (bitmap->InitCheck() != B_OK) {
		delete bitmap;
		return NULL;
	}
	memset(bitmap->Bits(), 0, bitmap->BitsLength());

	BView* view = new BView(bounds, "icon", B_FOLLOW_NONE, 0);
	bitmap->AddChild(view);
	bitmap->Lock();
	view->SetDrawingMode(B_OP_ALPHA);
	view->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_COMPOSITE);
	view->SetHighColor(ui_color(B_CONTROL_TEXT_COLOR));

	switch (icon) {
		case FLICK_ICON_SELF_TIMER:
			draw_self_timer(view, size);
			break;
		case FLICK_ICON_MIRROR:
			draw_mirror(view, size);
			break;
		case FLICK_ICON_SETTINGS:
			draw_settings(view, size);
			break;
	}

	view->Sync();
	bitmap->RemoveChild(view);
	bitmap->Unlock();
	delete view;
	return bitmap;
}
