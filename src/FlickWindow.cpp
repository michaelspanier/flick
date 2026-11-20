/*
 * Copyright 2026, Michael Spanier.
 * Distributed under the terms of the MIT License.
 *
 * SPDX-License-Identifier: MIT
 */


#include "FlickWindow.h"

#include <AffineTransform.h>
#include <Alert.h>
#include <Application.h>
#include <Bitmap.h>
#include <BitmapStream.h>
#include <Button.h>
#include <Catalog.h>
#include <ControlLook.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <MessageRunner.h>
#include <NodeInfo.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <String.h>
#include <StringView.h>
#include <TranslatorFormats.h>
#include <TranslatorRoster.h>
#include <View.h>

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "FlickConsumer.h"
#include "FlickControls.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "FlickWindow"


static const uint32 kMsgShutter = 'FlSh';
static const uint32 kMsgCamera = 'FlCm';
static const uint32 kMsgSize = 'FlSz';
static const uint32 kMsgResetStatus = 'FlRs';
static const uint32 kMsgStartCamera = 'FlSt';
static const uint32 kMsgToggleRate = 'FlRt';
static const uint32 kMsgPhotoWritten = 'FlPw';
static const uint32 kMsgShowControls = 'FlCt';
static const uint32 kMsgControlsClosed = 'FcCl';
static const uint32 kMsgSelfTimer = 'FlTm';
static const uint32 kMsgMirror = 'FlMi';
static const uint32 kMsgCountdownTick = 'FlCd';

static const uint32 kDragImageLimit = 160;
static const bigtime_t kFlashLength = 350000;
static const int kSelfTimerSeconds = 3;
static const uint8 kDragImageAlpha = 170;


/*!	Writes \a picture as PNG into \a file. The bitmap stays with the caller.
*/
static status_t
write_png(BBitmap* picture, BFile* file)
{
	BBitmapStream stream(picture);
	status_t status = BTranslatorRoster::Default()->Translate(&stream, NULL,
		NULL, file, B_PNG_FORMAT);

	BBitmap* released;
	stream.DetachBitmap(&released);

	if (status == B_OK)
		BNodeInfo(file).SetType("image/png");
	return status;
}


//	#pragma mark - FlickView


/*!	Shows the live picture, as large as fits and with its own proportions.
*/
class FlickView : public BView {
public:
	FlickView()
		:
		BView("preview", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
		fBitmap(NULL),
		fHasPicture(false),
		fFlashStart(0),
		fMirrored(true),
		fCountdown(0),
		fCrossedOut(false),
		fDragPicture(NULL),
		fMayDrag(false)
	{
		SetViewColor(B_TRANSPARENT_COLOR);
		SetExplicitMinSize(BSize(320, 240));
		SetExplicitPreferredSize(BSize(640, 480));
	}

	virtual ~FlickView()
	{
		delete fBitmap;
		delete fDragPicture;
	}

	virtual void MouseDown(BPoint where)
	{
		if (fBitmap == NULL || !fHasPicture)
			return;
		fMayDrag = true;
		fDragStart = where;
		SetMouseEventMask(B_POINTER_EVENTS);
	}

	virtual void MouseMoved(BPoint where, uint32 transit,
		const BMessage* dragMessage)
	{
		if (!fMayDrag
			|| !flick_is_drag(where.x - fDragStart.x, where.y - fDragStart.y))
			return;
		fMayDrag = false;
		_BeginDrag();
	}

	virtual void MouseUp(BPoint where)
	{
		fMayDrag = false;
	}

	virtual void MessageReceived(BMessage* message)
	{
		if (message->what == B_COPY_TARGET) {
			_WriteDragPicture(message);
			return;
		}
		BView::MessageReceived(message);
	}

	void SetPictureSize(uint32 width, uint32 height)
	{
		delete fBitmap;
		fBitmap = new BBitmap(BRect(0, 0, width - 1, height - 1), B_RGB32);
		if (fBitmap->InitCheck() != B_OK) {
			delete fBitmap;
			fBitmap = NULL;
		}
		fHasPicture = false;
		Invalidate();
	}

	/*!	Shown instead of the picture as long as there is none: a camera, crossed
		out if there is no camera or it does not work, with a short text.
	*/
	void SetPlaceholder(bool crossedOut, const char* title, const char* hint)
	{
		fCrossedOut = crossedOut;
		fTitle = title;
		fHint = hint;
		Invalidate();
	}

	BBitmap* Bitmap() const
	{
		return fBitmap;
	}

	bool HasPicture() const
	{
		return fHasPicture;
	}

	void PictureChanged()
	{
		fHasPicture = true;
		Invalidate();
	}

	void Flash()
	{
		fFlashStart = system_time();
		Invalidate();
	}

	//!	Only the live picture is mirrored, photos keep the camera's view.
	void SetMirrored(bool mirrored)
	{
		fMirrored = mirrored;
		Invalidate();
	}

	//!	Shows the seconds left of the self-timer; 0 hides them.
	void SetCountdown(int secondsLeft)
	{
		fCountdown = secondsLeft;
		Invalidate();
	}

	virtual void Draw(BRect updateRect)
	{
		if (fBitmap == NULL || !fHasPicture) {
			_DrawPlaceholder();
			return;
		}

		SetHighColor(0, 0, 0);
		FillRect(Bounds());

		BRect target = _PictureFrame();
		if (fMirrored) {
			BAffineTransform mirror;
			mirror.ScaleBy(BPoint((target.left + target.right) / 2, 0), -1, 1);
			SetTransform(mirror);
		}
		SetDrawingMode(B_OP_COPY);
		DrawBitmap(fBitmap, fBitmap->Bounds(), target,
			B_FILTER_BITMAP_BILINEAR);
		SetTransform(BAffineTransform());

		_DrawFlash(target);
		_DrawCountdown(target);
	}

private:
	BRect _PictureFrame() const
	{
		BRect bounds = Bounds();
		float scale = min_c(bounds.Width() / fBitmap->Bounds().Width(),
			bounds.Height() / fBitmap->Bounds().Height());
		float width = fBitmap->Bounds().Width() * scale;
		float height = fBitmap->Bounds().Height() * scale;
		BRect frame(0, 0, width, height);
		frame.OffsetTo(bounds.left + (bounds.Width() - width) / 2,
			bounds.top + (bounds.Height() - height) / 2);
		return frame;
	}

	//!	White over the picture that fades out; the next frames redraw it.
	void _DrawFlash(BRect frame)
	{
		uint8 alpha = flick_flash_alpha(system_time() - fFlashStart,
			kFlashLength);
		if (alpha == 0)
			return;
		SetDrawingMode(B_OP_ALPHA);
		SetBlendingMode(B_CONSTANT_ALPHA, B_ALPHA_OVERLAY);
		SetHighColor(255, 255, 255, alpha);
		FillRect(frame);
		SetDrawingMode(B_OP_COPY);
	}

	//!	A large number in the middle of the picture, with a dark outline.
	void _DrawCountdown(BRect frame)
	{
		if (fCountdown <= 0)
			return;
		BString number;
		number << fCountdown;

		BFont font(be_bold_font);
		font.SetSize(frame.Height() * 0.4f);
		SetFont(&font);
		font_height height;
		font.GetHeight(&height);
		BPoint where((frame.left + frame.right - StringWidth(number)) / 2,
			(frame.top + frame.bottom + height.ascent - height.descent) / 2);

		SetDrawingMode(B_OP_ALPHA);
		SetBlendingMode(B_CONSTANT_ALPHA, B_ALPHA_OVERLAY);
		SetHighColor(0, 0, 0, 120);
		DrawString(number, where + BPoint(3, 3));
		SetHighColor(255, 255, 255, 230);
		DrawString(number, where);
		SetDrawingMode(B_OP_COPY);
		SetFont(be_plain_font);
	}

	/*!	Starts the drag of the picture shown now, with the "Passing Data via
		File" protocol: the target (for example Tracker) creates a file and
		answers with B_COPY_TARGET, then the picture is written into it.
	*/
	void _BeginDrag()
	{
		delete fDragPicture;
		fDragPicture = new BBitmap(fBitmap);
		if (fDragPicture->InitCheck() != B_OK) {
			delete fDragPicture;
			fDragPicture = NULL;
			return;
		}

		char name[B_FILE_NAME_LENGTH];
		flick_photo_file_name(name, sizeof(name), time(NULL), 0);

		BMessage drag(B_SIMPLE_DATA);
		drag.AddInt32("be:actions", B_COPY_TARGET);
		drag.AddString("be:clip_name", name);
		drag.AddString("be:types", B_FILE_MIME_TYPE);
		drag.AddString("be:filetypes", "image/png");
		drag.AddString("be:type_descriptions", B_TRANSLATE("PNG image"));

		BBitmap* image = _DragImage();
		if (image == NULL) {
			DragMessage(&drag, Bounds());
			return;
		}
		BRect bounds = image->Bounds();
		BPoint offset(bounds.Width() / 2, bounds.Height() / 2);
		// DragMessage() takes over the image.
		DragMessage(&drag, image, B_OP_ALPHA, offset);
	}

	//!	A small, see-through copy of the picture that follows the mouse.
	BBitmap* _DragImage()
	{
		BRect source = fDragPicture->Bounds();
		flick_size size = flick_fit_size(source.IntegerWidth() + 1,
			source.IntegerHeight() + 1, kDragImageLimit);
		BRect rect(0, 0, size.width - 1, size.height - 1);

		BBitmap* image = new BBitmap(rect, B_BITMAP_ACCEPTS_VIEWS, B_RGBA32);
		if (image->InitCheck() != B_OK) {
			delete image;
			return NULL;
		}

		BView* view = new BView(rect, "drag image", B_FOLLOW_NONE, 0);
		image->AddChild(view);
		image->Lock();
		view->DrawBitmap(fDragPicture, source, rect, B_FILTER_BITMAP_BILINEAR);
		view->Sync();
		image->Unlock();

		uint8* row = (uint8*)image->Bits();
		for (uint32 y = 0; y < size.height; y++, row += image->BytesPerRow()) {
			for (uint32 x = 0; x < size.width; x++)
				row[x * 4 + 3] = kDragImageAlpha;
		}
		return image;
	}

	void _WriteDragPicture(BMessage* reply)
	{
		entry_ref directoryRef;
		const char* name;
		status_t status = B_BAD_VALUE;
		// The answer comes from another program: the name must stay inside
		// the directory.
		if (fDragPicture != NULL
			&& reply->FindRef("directory", &directoryRef) == B_OK
			&& reply->FindString("name", &name) == B_OK
			&& strchr(name, '/') == NULL) {
			BDirectory directory(&directoryRef);
			BFile file(&directory, name,
				B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
			status = file.InitCheck();
			if (status == B_OK)
				status = write_png(fDragPicture, &file);
		}

		BMessage written(kMsgPhotoWritten);
		written.AddInt32("status", status);
		if (status == B_OK)
			written.AddString("name", name);
		Window()->PostMessage(&written);
	}

	void _DrawPlaceholder()
	{
		BRect bounds = Bounds();

		// Dark background with a soft gradient.
		for (float y = bounds.top; y <= bounds.bottom; y++) {
			float t = (y - bounds.top) / max_c(bounds.Height(), 1.0f);
			SetHighColor(38 + 14 * t, 40 + 14 * t, 46 + 16 * t);
			StrokeLine(BPoint(bounds.left, y), BPoint(bounds.right, y));
		}

		float size = max_c(64.0f, min_c(bounds.Width(), bounds.Height()) * 0.28f);
		BPoint center(bounds.Width() / 2, bounds.Height() / 2 - size * 0.25f);
		_DrawCamera(center, size);

		SetDrawingMode(B_OP_OVER);
		font_height height;
		be_bold_font->GetHeight(&height);
		float lineHeight = height.ascent + height.descent + height.leading;
		float y = center.y + size * 0.62f + lineHeight;

		SetFont(be_bold_font);
		SetHighColor(225, 228, 235);
		DrawString(fTitle.String(), BPoint(
			(bounds.Width() - StringWidth(fTitle.String())) / 2, y));

		SetFont(be_plain_font);
		SetHighColor(160, 165, 175);
		DrawString(fHint.String(), BPoint(
			(bounds.Width() - StringWidth(fHint.String())) / 2,
			y + lineHeight + 4));
	}

	void _DrawCamera(BPoint center, float size)
	{
		SetDrawingMode(B_OP_ALPHA);
		SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
		SetPenSize(max_c(2.0f, size / 28));

		float half = size / 2;
		BRect body(center.x - half, center.y - half * 0.62f,
			center.x + half, center.y + half * 0.62f);
		SetHighColor(190, 196, 208);
		StrokeRoundRect(body, size / 12, size / 12);

		// Bump on top and the flash at the corner.
		StrokeRoundRect(BRect(center.x - half * 0.32f,
			body.top - half * 0.2f, center.x + half * 0.32f, body.top),
			size / 24, size / 24);

		// Lens: two rings.
		BPoint lens(center.x, center.y + half * 0.02f);
		StrokeEllipse(lens, half * 0.42f, half * 0.42f);
		StrokeEllipse(lens, half * 0.22f, half * 0.22f);

		if (fCrossedOut) {
			// A slash, with a dark edge so that it stays visible.
			BPoint from(center.x - half * 1.05f, center.y + half * 0.8f);
			BPoint to(center.x + half * 1.05f, center.y - half * 0.8f);
			SetPenSize(max_c(5.0f, size / 12));
			SetHighColor(38, 40, 46);
			StrokeLine(from, to);
			SetPenSize(max_c(3.0f, size / 20));
			SetHighColor(225, 70, 70);
			StrokeLine(from, to);
		}

		SetPenSize(1);
		SetDrawingMode(B_OP_COPY);
	}

	BBitmap*	fBitmap;
	bool		fHasPicture;
	bigtime_t	fFlashStart;
	bool		fMirrored;
	int			fCountdown;
	bool		fCrossedOut;
	BString		fTitle;
	BString		fHint;
	BBitmap*	fDragPicture;
	bool		fMayDrag;
	BPoint		fDragStart;
};


//	#pragma mark - ShutterButton


/*!	The red button that takes the photo.
*/
class ShutterButton : public BView {
public:
	ShutterButton(BMessage* message)
		:
		BView("shutter", B_WILL_DRAW),
		fMessage(message),
		fPressed(false),
		fEnabled(true)
	{
		SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
		SetExplicitMinSize(BSize(56, 56));
		SetExplicitMaxSize(BSize(56, 56));
		SetExplicitPreferredSize(BSize(56, 56));
		SetToolTip(B_TRANSLATE("Take a photo (Space)"));
	}

	virtual ~ShutterButton()
	{
		delete fMessage;
	}

	void SetEnabled(bool enabled)
	{
		fEnabled = enabled;
		Invalidate();
	}

	virtual void MouseDown(BPoint where)
	{
		if (!fEnabled)
			return;
		fPressed = true;
		SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
		Invalidate();
	}

	virtual void MouseUp(BPoint where)
	{
		if (!fPressed)
			return;
		fPressed = false;
		Invalidate();
		if (Bounds().Contains(where))
			Window()->PostMessage(fMessage);
	}

	virtual void Draw(BRect updateRect)
	{
		SetLowUIColor(B_PANEL_BACKGROUND_COLOR);
		FillRect(Bounds(), B_SOLID_LOW);

		SetDrawingMode(B_OP_ALPHA);
		SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);

		BRect circle = Bounds().InsetByCopy(3, 3);
		BPoint center((circle.left + circle.right) / 2,
			(circle.top + circle.bottom) / 2);
		SetHighColor(_FaceColor());
		FillEllipse(circle);
		SetHighColor(110, 10, 10, 200);
		StrokeEllipse(circle);

		// A small camera, centered as a whole: the bump on top and the body
		// span kCameraTop to kCameraBottom around the center of the button.
		const float kHalfWidth = 12;
		const float kCameraTop = -10;
		const float kBodyTop = -6;
		const float kCameraBottom = 10;
		BRect body(center.x - kHalfWidth, center.y + kBodyTop,
			center.x + kHalfWidth, center.y + kCameraBottom);
		SetHighColor(255, 255, 255);
		FillRoundRect(body, 3, 3);
		FillRoundRect(BRect(center.x - 5, center.y + kCameraTop, center.x + 5,
			center.y + kBodyTop + 1), 2, 2);

		// The lens: a ring in the button color and a white dot, both around
		// the center of the body.
		BPoint lens(center.x, (body.top + body.bottom) / 2);
		SetHighColor(_FaceColor());
		FillEllipse(lens, 6, 6);
		SetHighColor(255, 255, 255);
		FillEllipse(lens, 3, 3);
	}

private:
	rgb_color _FaceColor() const
	{
		if (!fEnabled)
			return make_color(170, 170, 170);
		if (fPressed)
			return make_color(150, 15, 15);
		return make_color(225, 30, 30);
	}


	BMessage*	fMessage;
	bool		fPressed;
	bool		fEnabled;
};


//	#pragma mark - FlickWindow


FlickWindow::FlickWindow(uint32 preferredWidth, uint32 preferredHeight)
	:
	BWindow(BRect(100, 100, 740, 640), B_TRANSLATE_SYSTEM_NAME("Flick"),
		B_TITLED_WINDOW,
		B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS),
	fCamera(NULL),
	fControlsWindow(NULL),
	fView(NULL),
	fStatus(NULL),
	fShutter(NULL),
	fCameraMenu(NULL),
	fCameraPopUp(NULL),
	fSelfTimerButton(NULL),
	fMirrorButton(NULL),
	fSelfTimerItem(NULL),
	fMirrorItem(NULL),
	fSizeMenu(NULL),
	fCurrentCamera(-1),
	fSizeCount(0),
	fCurrentSize(-1),
	fPreferredWidth(preferredWidth),
	fPreferredHeight(preferredHeight),
	fStatusRunner(NULL),
	fRate(-1),
	fShowRate(false),
	fSelfTimer(false),
	fMirrored(true),
	fCountdownRunner(NULL)
{
	flick_rate_start(&fRateMeter, system_time());
	flick_countdown_start(&fCountdown, 0);
	BMenuBar* menuBar = new BMenuBar("menubar");
	_BuildMenus(menuBar);

	fView = new FlickView();
	fView->SetMirrored(fMirrored);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(menuBar)
		.Add(fView, 1.0f)
		.Add(_BuildBar());

	fCamera = new FlickCamera(BMessenger(this));

	// The window comes first, the camera takes a moment.
	_ShowPlaceholder(false, B_TRANSLATE("Looking for a camera" B_UTF8_ELLIPSIS),
		"");
	fShutter->SetEnabled(false);
	PostMessage(kMsgStartCamera);
}


FlickWindow::~FlickWindow()
{
	delete fCountdownRunner;
	delete fStatusRunner;
	delete fCamera;
}


bool
FlickWindow::QuitRequested()
{
	// The camera must be given back before the application ends, otherwise
	// the next start finds it still taken.
	_StopCountdown();
	_CloseControls();
	delete fCamera;
	fCamera = NULL;
	be_app->PostMessage(B_QUIT_REQUESTED);
	return true;
}


void
FlickWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgFlickFrame:
			if (fCamera != NULL && fCamera->Consumer() != NULL
				&& fView->Bitmap() != NULL
				&& fCamera->Consumer()->CopyFrame(fView->Bitmap())) {
				fView->PictureChanged();
				_CountFrame();
			}
			break;

		case kMsgToggleRate:
			_ToggleRate(message);
			break;

		case kMsgShowControls:
			_ShowControls();
			break;

		case kMsgControlsClosed:
			// The window is gone, it told us before it quit.
			fControlsWindow = NULL;
			break;

		case kMsgStartCamera:
			_FindCameras();
			break;

		case kMsgShutter:
			_Shutter();
			break;

		case kMsgCountdownTick:
			_CountDown();
			break;

		case kMsgSelfTimer:
			_ToggleSelfTimer();
			break;

		case kMsgMirror:
			_ToggleMirror();
			break;

		case kMsgCamera:
		{
			int32 index;
			if (message->FindInt32("index", &index) == B_OK)
				_SelectCamera(index);
			break;
		}

		case kMsgSize:
		{
			int32 index;
			if (message->FindInt32("index", &index) == B_OK)
				_SelectSize(index);
			break;
		}

		case kMsgResetStatus:
			_ShowStatusLine();
			break;

		case kMsgPhotoWritten:
		{
			int32 status = B_ERROR;
			const char* name = "";
			message->FindInt32("status", &status);
			message->FindString("name", &name);
			_ReportPhoto(status, name);
			break;
		}

		case B_KEY_DOWN:
		{
			const char* bytes;
			if (message->FindString("bytes", &bytes) == B_OK
				&& bytes[0] == B_SPACE) {
				_Shutter();
				break;
			}
			BWindow::MessageReceived(message);
			break;
		}

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


void
FlickWindow::_BuildMenus(BMenu* bar)
{
	BMenu* fileMenu = new BMenu(B_TRANSLATE("File"));
	BMenuItem* about = new BMenuItem(B_TRANSLATE("About Flick" B_UTF8_ELLIPSIS),
		new BMessage(B_ABOUT_REQUESTED));
	about->SetTarget(be_app);
	fileMenu->AddItem(about);
	fileMenu->AddSeparatorItem();
	fileMenu->AddItem(new BMenuItem(B_TRANSLATE("Take photo"),
		new BMessage(kMsgShutter), 'S'));
	fSelfTimerItem = new BMenuItem(B_TRANSLATE("Self-timer"),
		new BMessage(kMsgSelfTimer), 'T');
	fileMenu->AddItem(fSelfTimerItem);
	fileMenu->AddSeparatorItem();
	fileMenu->AddItem(new BMenuItem(B_TRANSLATE("Quit"),
		new BMessage(B_QUIT_REQUESTED), 'Q'));
	bar->AddItem(fileMenu);

	fCameraMenu = new BMenu(B_TRANSLATE("Camera"));
	fCameraMenu->SetRadioMode(true);
	bar->AddItem(fCameraMenu);

	fSizeMenu = new BMenu(B_TRANSLATE("Size"));
	fSizeMenu->SetRadioMode(true);
	bar->AddItem(fSizeMenu);

	BMenu* viewMenu = new BMenu(B_TRANSLATE("View"));
	viewMenu->AddItem(new BMenuItem(B_TRANSLATE("Image settings" B_UTF8_ELLIPSIS),
		new BMessage(kMsgShowControls), 'I'));
	fMirrorItem = new BMenuItem(B_TRANSLATE("Mirror picture"),
		new BMessage(kMsgMirror), 'M');
	fMirrorItem->SetMarked(fMirrored);
	viewMenu->AddItem(fMirrorItem);
	viewMenu->AddItem(new BMenuItem(B_TRANSLATE("Show frame rate"),
		new BMessage(kMsgToggleRate), 'R'));
	bar->AddItem(viewMenu);
}


/*!	The bar below the picture: the camera and its size on the left, the
	shutter in the middle, the switches on the right. Both sides have the same
	weight, so the shutter stays in the middle of the window.
*/
BView*
FlickWindow::_BuildBar()
{
	fCameraPopUp = new BPopUpMenu(B_TRANSLATE("No camera"));
	BMenuField* cameraField = new BMenuField("camera", NULL, fCameraPopUp);

	fStatus = new BStringView("status", "");
	fStatus->SetExplicitMinSize(BSize(120, B_SIZE_UNSET));
	fShutter = new ShutterButton(new BMessage(kMsgShutter));

	fSelfTimerButton = _CreateToggle("self timer", FLICK_ICON_SELF_TIMER,
		kMsgSelfTimer, B_TRANSLATE("Self-timer: 3 seconds (Alt-T)"));
	fMirrorButton = _CreateToggle("mirror", FLICK_ICON_MIRROR, kMsgMirror,
		B_TRANSLATE("Mirror the picture (Alt-M)"));
	fMirrorButton->SetValue(fMirrored ? B_CONTROL_ON : B_CONTROL_OFF);
	BButton* settings = _CreateToggle("settings", FLICK_ICON_SETTINGS,
		kMsgShowControls, B_TRANSLATE("Image settings (Alt-I)"));
	settings->SetBehavior(BButton::B_BUTTON_BEHAVIOR);

	BView* bar = new BView("bar", 0);
	BLayoutBuilder::Group<>(bar, B_HORIZONTAL, B_USE_DEFAULT_SPACING)
		.SetInsets(B_USE_WINDOW_INSETS, B_USE_HALF_ITEM_INSETS)
		.AddGroup(B_VERTICAL, 0, 1.0f)
			.Add(cameraField)
			.AddGroup(B_HORIZONTAL, 0)
				.AddStrut(be_control_look->DefaultLabelSpacing())
				.Add(fStatus)
				.AddGlue()
			.End()
		.End()
		.Add(fShutter)
		.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING, 1.0f)
			.AddGlue()
			.Add(fSelfTimerButton)
			.Add(fMirrorButton)
			.Add(settings)
		.End();
	return bar;
}


//!	A flat button with an icon that stays pressed while its switch is on.
BButton*
FlickWindow::_CreateToggle(const char* name, flick_icon icon, uint32 what,
	const char* toolTip)
{
	BButton* button = new BButton(name, NULL, new BMessage(what));
	button->SetBehavior(BButton::B_TOGGLE_BEHAVIOR);
	button->SetFlat(true);
	button->SetToolTip(toolTip);
	// Keyboard focus belongs to the window, Space takes the photo.
	button->SetFlags(button->Flags() & ~B_NAVIGABLE);

	float size = roundf(be_plain_font->Size() * 20 / 12);
	BBitmap* bitmap = flick_create_icon(icon, size);
	if (bitmap != NULL) {
		button->SetIcon(bitmap);
		delete bitmap;
	}
	return button;
}


void
FlickWindow::_FindCameras()
{
	FlickCamera::List(fCameras);

	if (fCameras.empty()) {
		_ShowPlaceholder(true, B_TRANSLATE("No camera found"),
			B_TRANSLATE("Connect a camera and start Flick again."));
		fShutter->SetEnabled(false);
		return;
	}

	for (size_t i = 0; i < fCameras.size(); i++) {
		BMessage* message = new BMessage(kMsgCamera);
		message->AddInt32("index", (int32)i);
		fCameraMenu->AddItem(new BMenuItem(fCameras[i].name, message));
		fCameraPopUp->AddItem(new BMenuItem(fCameras[i].name,
			new BMessage(*message)));
	}

	_SelectCamera(0);
}


void
FlickWindow::_SelectCamera(int32 index)
{
	if (index < 0 || index >= (int32)fCameras.size())
		return;

	_StopCountdown();
	_CloseControls();
	fCurrentCamera = index;
	if (BMenuItem* item = fCameraMenu->ItemAt(index))
		item->SetMarked(true);
	if (BMenuItem* item = fCameraPopUp->ItemAt(index))
		item->SetMarked(true);

	_ShowPlaceholder(false,
		B_TRANSLATE("Opening the camera" B_UTF8_ELLIPSIS), "");
	UpdateIfNeeded();

	status_t status = fCamera->Open(fCameras[index]);
	if (status != B_OK) {
		_ShowPlaceholder(true, B_TRANSLATE("The camera cannot be opened"),
			B_TRANSLATE("Is another program using it?"));
		fShutter->SetEnabled(false);
		return;
	}

	fSizeCount = fCamera->FindSizes(fSizes, kMaxSizes);
	while (fSizeMenu->CountItems() > 0)
		delete fSizeMenu->RemoveItem((int32)0);

	for (int i = 0; i < fSizeCount; i++) {
		char label[32];
		flick_size_label(label, sizeof(label), fSizes[i].width,
			fSizes[i].height);
		BMessage* message = new BMessage(kMsgSize);
		message->AddInt32("index", i);
		fSizeMenu->AddItem(new BMenuItem(label, message));
	}

	int defaultIndex = flick_default_size_index(fSizes, fSizeCount);
	for (int i = 0; i < fSizeCount; i++) {
		if (fSizes[i].width == fPreferredWidth
			&& fSizes[i].height == fPreferredHeight) {
			defaultIndex = i;
		}
	}
	if (defaultIndex < 0) {
		_ShowPlaceholder(true, B_TRANSLATE("The camera reports no size"),
			B_TRANSLATE("Try another camera."));
		fShutter->SetEnabled(false);
		return;
	}

	_SelectSize(defaultIndex);
}


void
FlickWindow::_ShowControls()
{
	if (fControlsWindow != NULL) {
		fControlsWindow->Activate();
		return;
	}
	if (fCamera == NULL || !fCamera->HasProducer())
		return;

	FlickControlsWindow* window
		= new FlickControlsWindow(fCamera->Producer(), BMessenger(this));
	if (window->InitCheck() != B_OK) {
		window->Lock();
		window->Quit();
		_SetStatus(B_TRANSLATE("This camera has no picture settings"), true);
		return;
	}

	fControlsWindow = window;
	fControlsWindow->Show();
}


//!	The settings belong to the open camera and must go before it does.
void
FlickWindow::_CloseControls()
{
	if (fControlsWindow == NULL)
		return;

	fControlsWindow->Lock();
	fControlsWindow->Quit();
	fControlsWindow = NULL;
}


void
FlickWindow::_SelectSize(int32 index)
{
	if (index < 0 || index >= fSizeCount)
		return;

	_StopCountdown();
	fCurrentSize = index;
	if (BMenuItem* item = fSizeMenu->ItemAt(index))
		item->SetMarked(true);

	fRate = -1;
	flick_rate_start(&fRateMeter, system_time());

	status_t status = fCamera->Start(fSizes[index].width,
		fSizes[index].height);
	if (status != B_OK) {
		_ShowPlaceholder(true, B_TRANSLATE("The camera cannot be started"),
			B_TRANSLATE("Disconnect the camera and connect it again."));
		fShutter->SetEnabled(false);
		return;
	}

	fView->SetPictureSize(fCamera->Consumer()->Width(),
		fCamera->Consumer()->Height());
	_ShowPlaceholder(false,
		B_TRANSLATE("Waiting for the picture" B_UTF8_ELLIPSIS), "");
	fShutter->SetEnabled(true);
	_ShowStatusLine();
}


void
FlickWindow::_ShowPlaceholder(bool problem, const char* title, const char* hint)
{
	fView->SetPlaceholder(problem, title, hint);
	_SetStatus(title, false);
}


void
FlickWindow::_ShowStatusLine()
{
	if (fCurrentSize < 0 || fCurrentSize >= fSizeCount)
		return;

	char size[32];
	flick_size_label(size, sizeof(size), fSizes[fCurrentSize].width,
		fSizes[fCurrentSize].height);

	// The name of the camera is shown in the menu field above.
	BString label(size);

	if (fShowRate && fRate >= 0) {
		BString rate(B_TRANSLATE("%rate% frames/s"));
		BString number;
		number << fRate;
		rate.ReplaceFirst("%rate%", number);
		label << " · " << rate;
	}
	_SetStatus(label.String(), false);
}


void
FlickWindow::_CountFrame()
{
	int rate;
	if (!flick_rate_add_frame(&fRateMeter, system_time(), &rate))
		return;

	fRate = rate;
	// A temporary message such as "Saved: …" stays until it runs out.
	if (fShowRate && fStatusRunner == NULL)
		_ShowStatusLine();
}


void
FlickWindow::_ToggleRate(BMessage* message)
{
	fShowRate = !fShowRate;

	BMenuItem* item;
	if (message->FindPointer("source", (void**)&item) == B_OK)
		item->SetMarked(fShowRate);

	if (fStatusRunner == NULL)
		_ShowStatusLine();
}


void
FlickWindow::_SetStatus(const char* text, bool temporary)
{
	fStatus->SetText(text);

	delete fStatusRunner;
	fStatusRunner = NULL;
	if (temporary) {
		fStatusRunner = new BMessageRunner(BMessenger(this),
			new BMessage(kMsgResetStatus), 3000000, 1);
	}
}


//!	Takes the photo now, or with the self-timer on, starts or stops it.
void
FlickWindow::_Shutter()
{
	if (fView->Bitmap() == NULL || !fView->HasPicture())
		return;
	if (!fSelfTimer) {
		_TakePhoto();
		return;
	}
	if (flick_countdown_running(&fCountdown)) {
		_StopCountdown();
		return;
	}

	flick_countdown_start(&fCountdown, kSelfTimerSeconds);
	fView->SetCountdown(fCountdown.left);
	fCountdownRunner = new BMessageRunner(BMessenger(this),
		new BMessage(kMsgCountdownTick), 1000000, kSelfTimerSeconds);
}


void
FlickWindow::_CountDown()
{
	bool due = flick_countdown_tick(&fCountdown);
	fView->SetCountdown(fCountdown.left);
	if (!due)
		return;
	delete fCountdownRunner;
	fCountdownRunner = NULL;
	_TakePhoto();
}


//!	Also when the camera or its size changes: the photo would be another one.
void
FlickWindow::_StopCountdown()
{
	delete fCountdownRunner;
	fCountdownRunner = NULL;
	flick_countdown_stop(&fCountdown);
	if (fView != NULL)
		fView->SetCountdown(0);
}


void
FlickWindow::_ToggleSelfTimer()
{
	fSelfTimer = !fSelfTimer;
	if (!fSelfTimer)
		_StopCountdown();
	fSelfTimerItem->SetMarked(fSelfTimer);
	fSelfTimerButton->SetValue(fSelfTimer ? B_CONTROL_ON : B_CONTROL_OFF);
}


void
FlickWindow::_ToggleMirror()
{
	fMirrored = !fMirrored;
	fView->SetMirrored(fMirrored);
	fMirrorItem->SetMarked(fMirrored);
	fMirrorButton->SetValue(fMirrored ? B_CONTROL_ON : B_CONTROL_OFF);
}


void
FlickWindow::_TakePhoto()
{
	if (fView->Bitmap() == NULL || !fView->HasPicture())
		return;

	BPath directory;
	if (find_directory(B_DESKTOP_DIRECTORY, &directory) != B_OK) {
		_SetStatus(B_TRANSLATE("Desktop not found"), true);
		return;
	}

	// The frame is copied, the live picture goes on while it is written.
	BBitmap* copy = new BBitmap(*fView->Bitmap());
	if (copy->InitCheck() != B_OK) {
		delete copy;
		_SetStatus(B_TRANSLATE("Not enough memory"), true);
		return;
	}

	fView->Flash();

	BFile file;
	char name[B_FILE_NAME_LENGTH];
	status_t status = B_ERROR;
	for (int duplicate = 0; duplicate < 100; duplicate++) {
		flick_photo_file_name(name, sizeof(name), time(NULL), duplicate);
		BPath path(directory);
		path.Append(name);
		status = file.SetTo(path.Path(),
			B_CREATE_FILE | B_WRITE_ONLY | B_FAIL_IF_EXISTS);
		if (status != B_FILE_EXISTS)
			break;
	}

	if (status != B_OK) {
		delete copy;
		_SetStatus(B_TRANSLATE("The photo cannot be saved"), true);
		return;
	}

	status = write_png(copy, &file);
	delete copy;
	_ReportPhoto(status, name);
}


void
FlickWindow::_ReportPhoto(status_t status, const char*)
{
	if (status != B_OK) {
		_SetStatus(B_TRANSLATE("The photo cannot be saved"), true);
		return;
	}

	// The name would not fit beside the shutter; the file is on the Desktop.
	_SetStatus(B_TRANSLATE("Photo saved"), true);
}
