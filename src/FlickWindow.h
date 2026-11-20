/*
 * Copyright 2026, Michael Spanier.
 * Distributed under the terms of the MIT License.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef FLICK_WINDOW_H
#define FLICK_WINDOW_H


#include <Window.h>

#include <vector>

#include "FlickCamera.h"
#include "FlickIcons.h"
#include "FlickLogic.h"

class BButton;
class BMenu;
class BMenuItem;
class BMessageRunner;
class BStringView;
class FlickControlsWindow;
class FlickView;
class ShutterButton;


typedef std::vector<flick_camera_info> CameraList;

static const int kMaxSizes = 32;


class FlickWindow : public BWindow {
public:
								FlickWindow(uint32 preferredWidth = 0,
									uint32 preferredHeight = 0);
	virtual						~FlickWindow();

	virtual	bool				QuitRequested();
	virtual	void				MessageReceived(BMessage* message);

private:
			void				_BuildMenus(BMenu* bar);
			BButton*			_CreateToggle(const char* name, flick_icon icon,
									uint32 what, const char* toolTip);
			BView*				_BuildBar();
			void				_FindCameras();
			void				_SelectCamera(int32 index);
			void				_SelectSize(int32 index);
			void				_Shutter();
			void				_CountDown();
			void				_StopCountdown();
			void				_TakePhoto();
			void				_ToggleSelfTimer();
			void				_ToggleMirror();
			void				_ReportPhoto(status_t status, const char* name);
			void				_SetStatus(const char* text, bool temporary);
			void				_ShowPlaceholder(bool problem, const char* title,
									const char* hint);
			void				_ShowStatusLine();
			void				_CountFrame();
			void				_ToggleRate(BMessage* message);
			void				_ShowControls();
			void				_CloseControls();

			FlickCamera*		fCamera;
			FlickControlsWindow*	fControlsWindow;
			FlickView*			fView;
			BStringView*		fStatus;
			ShutterButton*		fShutter;
			BMenu*				fCameraMenu;
			BMenu*				fCameraPopUp;
			BButton*			fSelfTimerButton;
			BButton*			fMirrorButton;
			BMenuItem*			fSelfTimerItem;
			BMenuItem*			fMirrorItem;
			BMenu*				fSizeMenu;
			CameraList			fCameras;
			int32				fCurrentCamera;
			flick_size			fSizes[kMaxSizes];
			int					fSizeCount;
			int32				fCurrentSize;
			uint32				fPreferredWidth;
			uint32				fPreferredHeight;
			BMessageRunner*		fStatusRunner;
			flick_rate_meter	fRateMeter;
			int					fRate;
			bool				fShowRate;
			bool				fSelfTimer;
			bool				fMirrored;
			flick_countdown		fCountdown;
			BMessageRunner*		fCountdownRunner;
};


#endif	// FLICK_WINDOW_H
