/*
 * Copyright 2026, Michael Spanier.
 * Distributed under the terms of the MIT License.
 *
 * SPDX-License-Identifier: MIT
 */


#include "FlickControls.h"

#include <Box.h>
#include <Catalog.h>
#include <CheckBox.h>
#include <LayoutBuilder.h>
#include <MediaRoster.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <ParameterWeb.h>
#include <PopUpMenu.h>
#include <Screen.h>
#include <ScrollView.h>
#include <Slider.h>

#include <stdio.h>
#include <string.h>

#include "FlickControlsLogic.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "FlickControls"


static const uint32 kMsgSlider = 'FcSl';
static const uint32 kMsgSwitch = 'FcSw';
static const uint32 kMsgChoice = 'FcCh';
static const uint32 kMsgControlsClosed = 'FcCl';


/*!	The names of groups and parameters come from the camera and are English.
	The ones we know are translated here.
*/
static const char* const kKnownNames[] = {
	B_TRANSLATE_MARK("Brightness"),
	B_TRANSLATE_MARK("Contrast"),
	B_TRANSLATE_MARK("Hue"),
	B_TRANSLATE_MARK("Saturation"),
	B_TRANSLATE_MARK("Sharpness"),
	B_TRANSLATE_MARK("Gamma"),
	B_TRANSLATE_MARK("White Balance Temperature"),
	B_TRANSLATE_MARK("WB Temperature"),
	B_TRANSLATE_MARK("White Balance Component"),
	B_TRANSLATE_MARK("WB Blue"),
	B_TRANSLATE_MARK("WB Red"),
	B_TRANSLATE_MARK("Backlight Compensation"),
	B_TRANSLATE_MARK("Gain"),
	B_TRANSLATE_MARK("Power Line Frequency"),
	B_TRANSLATE_MARK("Frequency"),
	B_TRANSLATE_MARK("Exposure"),
	B_TRANSLATE_MARK("Exposure Time"),
	B_TRANSLATE_MARK("Allow Lower Frame Rate"),
	B_TRANSLATE_MARK("Auto"),
	B_TRANSLATE_MARK("Off"),
	B_TRANSLATE_MARK("50 Hz"),
	B_TRANSLATE_MARK("60 Hz")
};


static const char*
translated(const char* name)
{
	for (size_t i = 0; i < sizeof(kKnownNames) / sizeof(kKnownNames[0]); i++) {
		if (strcmp(name, kKnownNames[i]) == 0)
			return B_TRANSLATE_NOCOLLECT(kKnownNames[i]);
	}
	return name;
}


/*!	A slider that shows the value of the parameter, not its position. */
class ValueSlider : public BSlider {
public:
	ValueSlider(const char* label, BMessage* message, int32 steps,
			float minimum, float maximum)
		:
		BSlider(label, label, message, 0, steps, B_HORIZONTAL),
		fMinimum(minimum),
		fMaximum(maximum),
		fSteps(steps)
	{
		fText[0] = '\0';
	}

	virtual const char* UpdateText() const
	{
		snprintf(fText, sizeof(fText), "%g",
			flick_slider_value(Value(), fMinimum, fMaximum, fSteps));
		return fText;
	}

private:
	float			fMinimum;
	float			fMaximum;
	int32			fSteps;
	mutable char	fText[32];
};


struct FlickControlEntry {
	BParameter*		parameter;
	BControl*		control;
	int32			group;
	float			minimum;
	float			maximum;
	int32			steps;
	bool			isAutomatic;
};


FlickControlsWindow::FlickControlsWindow(const media_node& camera,
	const BMessenger& owner)
	:
	BWindow(BRect(120, 120, 420, 400), B_TRANSLATE("Image settings"),
		B_TITLED_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS),
	fWeb(NULL),
	fCamera(camera),
	fOwner(owner),
	fStatus(B_ERROR),
	fGroupCount(0)
{
	_Build();
}


FlickControlsWindow::~FlickControlsWindow()
{
	for (size_t i = 0; i < fEntries.size(); i++)
		delete fEntries[i];
	delete fWeb;
}


bool
FlickControlsWindow::QuitRequested()
{
	fOwner.SendMessage(kMsgControlsClosed);
	return true;
}


void
FlickControlsWindow::_Build()
{
	BMediaRoster* roster = BMediaRoster::Roster();
	if (roster == NULL
		|| roster->GetParameterWebFor(fCamera, &fWeb) != B_OK || fWeb == NULL) {
		return;
	}

	BGroupView* list = new BGroupView(B_VERTICAL, B_USE_DEFAULT_SPACING);
	list->GroupLayout()->SetInsets(B_USE_WINDOW_INSETS);
	BScrollView* scroll = new BScrollView("scroll", list, 0, false, true,
		B_NO_BORDER);
	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(scroll);

	// Tall enough to show most controls, but never taller than the screen.
	float screenHeight = BScreen(this).Frame().Height();
	float height = screenHeight - 160.0f;
	if (height > 720.0f)
		height = 720.0f;
	scroll->SetExplicitMinSize(BSize(B_SIZE_UNSET, 160.0f));
	scroll->SetExplicitPreferredSize(BSize(B_SIZE_UNSET, height));

	// To the right of the picture, so that both can be seen.
	BRect screen = BScreen(this).Frame();
	ResizeTo(340.0f, height);
	MoveTo(screen.right - 340.0f - 40.0f, screen.top + 60.0f);

	for (int32 i = 0; i < fWeb->CountGroups(); i++)
		_AddGroup(fWeb->GroupAt(i), list);

	if (!fEntries.empty())
		fStatus = B_OK;
}


/*!	Walks through the groups of the web. A group with parameters of its own
	gets a box; the groups below it are searched as well.
*/
void
FlickControlsWindow::_AddGroup(BParameterGroup* group, BView* parent)
{
	const char* name = group->Name();
	if (!flick_control_group_is_hidden(name) && group->CountParameters() > 0) {
		// A group with one control needs no box: the control has the name.
		const bool single = group->CountParameters() == 1;
		BBox* box = NULL;
		BView* target = parent;
		if (!single) {
			box = new BBox(name);
			box->SetLabel(translated(name));
			BGroupView* inside
				= new BGroupView(B_VERTICAL, B_USE_HALF_ITEM_SPACING);
			inside->GroupLayout()->SetInsets(B_USE_ITEM_INSETS,
				B_USE_ITEM_INSETS, B_USE_ITEM_INSETS, B_USE_ITEM_INSETS);
			box->AddChild(inside);
			target = inside;
		}

		size_t before = fEntries.size();
		for (int32 i = 0; i < group->CountParameters(); i++)
			_AddParameter(group->ParameterAt(i), target);

		if (fEntries.size() > before) {
			if (box != NULL)
				parent->AddChild(box);
			_UpdateAutomatic(fGroupCount);
			fGroupCount++;
		} else
			delete box;
	}

	for (int32 i = 0; i < group->CountGroups(); i++)
		_AddGroup(group->GroupAt(i), parent);
}


void
FlickControlsWindow::_AddParameter(BParameter* parameter, BView* box)
{
	if (parameter->Kind() != NULL && strcmp(parameter->Kind(), B_RESOLUTION) == 0)
		return;

	const int32 index = (int32)fEntries.size();
	FlickControlEntry* entry = new FlickControlEntry;
	entry->parameter = parameter;
	entry->control = NULL;
	entry->group = fGroupCount;
	entry->minimum = 0.0f;
	entry->maximum = 0.0f;
	entry->steps = 0;
	entry->isAutomatic = false;

	const char* name = translated(parameter->Name());
	size_t size = 0;
	bigtime_t changed = 0;

	if (parameter->Type() == BParameter::B_CONTINUOUS_PARAMETER) {
		BContinuousParameter* range
			= static_cast<BContinuousParameter*>(parameter);
		entry->minimum = range->MinValue();
		entry->maximum = range->MaxValue();
		entry->steps = flick_slider_steps(entry->minimum, entry->maximum,
			range->ValueStep());
		if (entry->steps == 0) {
			delete entry;
			return;
		}

		BMessage* message = new BMessage(kMsgSlider);
		message->AddInt32("entry", index);
		BMessage* modification = new BMessage(kMsgSlider);
		modification->AddInt32("entry", index);
		ValueSlider* slider = new ValueSlider(name, message, entry->steps,
			entry->minimum, entry->maximum);
		slider->SetModificationMessage(modification);
		float value = entry->minimum;
		size = sizeof(value);
		if (parameter->GetValue(&value, &size, &changed) == B_OK) {
			slider->SetValue(flick_slider_position(value, entry->minimum,
				entry->maximum, entry->steps));
		}
		entry->control = slider;
	} else if (parameter->Type() == BParameter::B_DISCRETE_PARAMETER) {
		BDiscreteParameter* choice = static_cast<BDiscreteParameter*>(parameter);
		int32 value = 0;
		size = sizeof(value);
		parameter->GetValue(&value, &size, &changed);

		if (choice->CountItems() > 0) {
			BPopUpMenu* menu = new BPopUpMenu(name);
			for (int32 i = 0; i < choice->CountItems(); i++) {
				BMessage* item = new BMessage(kMsgChoice);
				item->AddInt32("entry", index);
				item->AddInt32("value", choice->ItemValueAt(i));
				BMenuItem* menuItem = new BMenuItem(
					translated(choice->ItemNameAt(i)), item);
				menuItem->SetMarked(choice->ItemValueAt(i) == value);
				menu->AddItem(menuItem);
			}
			BMenuField* field = new BMenuField(name, name, menu);
			box->AddChild(field);
			// The menu sends the value itself, the entry only keeps the parameter.
			menu->SetTargetForItems(this);
			fEntries.push_back(entry);
			return;
		}

		BMessage* message = new BMessage(kMsgSwitch);
		message->AddInt32("entry", index);
		BCheckBox* check = new BCheckBox(name, name, message);
		check->SetValue(value != 0 ? B_CONTROL_ON : B_CONTROL_OFF);
		entry->isAutomatic = strcmp(parameter->Name(), "Auto") == 0;
		entry->control = check;
	} else {
		delete entry;
		return;
	}

	entry->control->SetTarget(this);
	box->AddChild(entry->control);
	fEntries.push_back(entry);
}


void
FlickControlsWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgSlider:
		case kMsgSwitch:
		case kMsgChoice:
			_Change(message);
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


void
FlickControlsWindow::_Change(BMessage* message)
{
	int32 index;
	if (message->FindInt32("entry", &index) != B_OK || index < 0
		|| index >= (int32)fEntries.size()) {
		return;
	}

	FlickControlEntry* entry = fEntries[index];
	if (message->what == kMsgSlider) {
		BSlider* slider = static_cast<BSlider*>(entry->control);
		float value = flick_slider_value(slider->Value(), entry->minimum,
			entry->maximum, entry->steps);
		entry->parameter->SetValue(&value, sizeof(value), 0);
	} else if (message->what == kMsgSwitch) {
		int32 value = entry->control->Value() == B_CONTROL_ON ? 1 : 0;
		entry->parameter->SetValue(&value, sizeof(value), 0);
		if (entry->isAutomatic)
			_UpdateAutomatic(entry->group);
	} else {
		int32 value;
		if (message->FindInt32("value", &value) == B_OK)
			entry->parameter->SetValue(&value, sizeof(value), 0);
	}
}


/*!	While the camera adjusts a value on its own, the sliders of that group
	cannot be moved.
*/
void
FlickControlsWindow::_UpdateAutomatic(int32 group)
{
	bool automatic = false;
	for (size_t i = 0; i < fEntries.size(); i++) {
		if (fEntries[i]->group == group && fEntries[i]->isAutomatic
			&& fEntries[i]->control->Value() == B_CONTROL_ON) {
			automatic = true;
		}
	}
	for (size_t i = 0; i < fEntries.size(); i++) {
		if (fEntries[i]->group == group && !fEntries[i]->isAutomatic
			&& fEntries[i]->control != NULL
			&& fEntries[i]->parameter->Type()
				== BParameter::B_CONTINUOUS_PARAMETER) {
			fEntries[i]->control->SetEnabled(!automatic);
		}
	}
}
