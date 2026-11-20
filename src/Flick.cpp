/*
 * Copyright 2026, Michael Spanier.
 * Distributed under the terms of the MIT License.
 *
 * SPDX-License-Identifier: MIT
 */


#include <AboutWindow.h>
#include <Application.h>
#include <Catalog.h>

#include <stdio.h>
#include <string.h>

#include "FlickWindow.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Flick"


class FlickApp : public BApplication {
public:
	FlickApp()
		:
		BApplication("application/x-vnd.Flick")
	{
	}

	virtual void ArgvReceived(int32 argc, char** argv)
	{
		// "--size 640x480" starts with that size, if the camera has it.
		for (int32 i = 1; i + 1 < argc; i++) {
			unsigned width, height;
			if (strcmp(argv[i], "--size") == 0
				&& sscanf(argv[i + 1], "%ux%u", &width, &height) == 2) {
				fWidth = width;
				fHeight = height;
			}
		}
	}

	virtual void AboutRequested()
	{
		BAboutWindow* about = new BAboutWindow(B_TRANSLATE_SYSTEM_NAME("Flick"),
			"application/x-vnd.Flick");
		about->AddDescription(B_TRANSLATE_COMMENT(
			"Flick - pictures with a click.\n\n"
			"The photo lands on the Desktop as a PNG file, or can be dragged "
			"straight into any folder.",
			"Flick and click rhyme on purpose. Please keep a rhyme or a pun "
			"with the name if your language allows it."));
		about->AddCopyright(2026, "Michael Spanier");
		const char* authors[] = { "Michael Spanier", NULL };
		about->AddAuthors(authors);
		about->SetVersion("0.1");
		about->Show();
	}

	virtual void ReadyToRun()
	{
		(new FlickWindow(fWidth, fHeight))->Show();
	}

private:
	uint32	fWidth = 0;
	uint32	fHeight = 0;
};


int
main()
{
	FlickApp app;
	app.Run();
	return 0;
}
