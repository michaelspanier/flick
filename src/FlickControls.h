/*
 * Copyright 2026, Michael Spanier.
 * Distributed under the terms of the MIT License.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef FLICK_CONTROLS_H
#define FLICK_CONTROLS_H


#include <MediaNode.h>
#include <Messenger.h>
#include <Window.h>

#include <vector>

class BParameter;
class BParameterGroup;
class BParameterWeb;
class BView;
struct FlickControlEntry;


/*!	The window with the picture settings of the camera: brightness, contrast,
	white balance, exposure and so on. It shows what the camera reports in its
	parameter web, one box for each group, and sends every change to the camera.
*/
class FlickControlsWindow : public BWindow {
public:
								FlickControlsWindow(const media_node& camera,
									const BMessenger& owner);
	virtual						~FlickControlsWindow();

	//! B_OK if the camera has controls to show.
			status_t			InitCheck() const { return fStatus; }

	virtual	bool				QuitRequested();
	virtual	void				MessageReceived(BMessage* message);

private:
			void				_Build();
			void				_AddGroup(BParameterGroup* group,
									BView* parent);
			void				_AddParameter(BParameter* parameter,
									BView* box);
			void				_Change(BMessage* message);
			void				_UpdateAutomatic(int32 group);

			BParameterWeb*		fWeb;
			media_node			fCamera;
			BMessenger			fOwner;
			status_t			fStatus;
			std::vector<FlickControlEntry*>	fEntries;
			int32				fGroupCount;
};


#endif	// FLICK_CONTROLS_H
