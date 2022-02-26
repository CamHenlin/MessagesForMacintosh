/*
#
#	Apple Macintosh Developer Technical Support
#
#	MultiFinder-Aware Simple Sample Application
#
#	Sample
#
#	Sample.r	-	Rez Source
#
#	Copyright © 1989 Apple Computer, Inc.
#	All rights reserved.
#
#	Versions:	
#				1.00				08/88
#				1.01				11/88
#				1.02				04/89	MPW 3.1
#
#	Components:
#				Sample.p			April 1, 1989
#				Sample.c			April 1, 1989
#				Sample.a			April 1, 1989
#				Sample.inc1.a		April 1, 1989
#				SampleMisc.a		April 1, 1989
#				Sample.r			April 1, 1989
#				Sample.h			April 1, 1989
#				[P]Sample.make		April 1, 1989
#				[C]Sample.make		April 1, 1989
#				[A]Sample.make		April 1, 1989
#
#	Sample is an example application that demonstrates how to
#	initialize the commonly used toolbox managers, operate 
#	successfully under MultiFinder, handle desk accessories, 
#	and create, grow, and zoom windows.
#
#	It does not by any means demonstrate all the techniques 
#	you need for a large application. In particular, Sample 
#	does not cover exception handling, multiple windows/documents, 
#	sophisticated memory management, printing, or undo. All of 
#	these are vital parts of a normal full-sized application.
#
#	This application is an example of the form of a Macintosh 
#	application; it is NOT a template. It is NOT intended to be 
#	used as a foundation for the next world-class, best-selling, 
#	600K application. A stick figure drawing of the human body may 
#	be a good example of the form for a painting, but that does not 
#	mean it should be used as the basis for the next Mona Lisa.
#
#	We recommend that you review this program or TESample before 
#	beginning a new application.
*/

#include "Types.r"
#include "mac_main.h"

/* this is a definition for a resource which contains only a rectangle */
type 'RECT' {
	rect;
};


/* we use an MBAR resource to conveniently load all the menus */

resource 'MBAR' (rMenuBar, preload) {
	{ mApple, mFile, mEdit, mLight, mHelp };	/* five menus */
};


resource 'MENU' (mApple, preload) {
	mApple, textMenuProc,
	AllItems & ~MenuItem2,	/* Disable dashed line, enable About and DAs */
	enabled, apple,
	{
		"About Messages",
			noicon, nokey, nomark, plain;
		"-",
			noicon, nokey, nomark, plain
	}
};

resource 'MENU' (mFile, preload) {
	mFile, textMenuProc,
	MenuItem12,				/* enable Quit only, program enables others */
	enabled, "File",
	{
		"New",
			noicon, "N", nomark, plain;
		"Open",
			noicon, "O", nomark, plain;
		"-",
			noicon, nokey, nomark, plain;
		"Close",
			noicon, "W", nomark, plain;
		"Save",
			noicon, "S", nomark, plain;
		"Save As",
			noicon, nokey, nomark, plain;
		"Revert",
			noicon, nokey, nomark, plain;
		"-",
			noicon, nokey, nomark, plain;
		"Page Setup",
			noicon, nokey, nomark, plain;
		"Print",
			noicon, nokey, nomark, plain;
		"-",
			noicon, nokey, nomark, plain;
		"Quit",
			noicon, "Q", nomark, plain
	}
};

resource 'MENU' (mEdit, preload) {
	mEdit, textMenuProc,
	NoItems,				/* disable everything, program does the enabling */
	enabled, "Edit",
	 {
		"Undo",
			noicon, "Z", nomark, plain;
		"-",
			noicon, nokey, nomark, plain;
		"Cut",
			noicon, "X", nomark, plain;
		"Copy",
			noicon, "C", nomark, plain;
		"Paste",
			noicon, "V", nomark, plain;
		"Clear",
			noicon, nokey, nomark, plain
	}
};

resource 'MENU' (mLight, preload) {
	mLight, textMenuProc,
	NoItems,				/* disable everything, program does the enabling */
	enabled, "Messages",
	 {
		"New Message",
			noicon, nokey, nomark, plain;
		"Refresh Chat List",
			noicon, nokey, nomark, plain;
		"Refresh Messages",
			noicon, nokey, nomark, plain;
		"Clear Chat Input",
			noicon, nokey, nomark, plain;
	}
};

resource 'MENU' (mHelp, preload) {
	mHelp, textMenuProc,
	AllItems,				/* disable everything, program does the enabling */
	enabled, "Help",
	 {
		"Quick Help",
			noicon, nokey, nomark, plain;
		"User Guide",
			noicon, nokey, nomark, plain;
        "Test Entry",
			noicon, nokey, nomark, plain;
        "Test Entry 2",
			noicon, nokey, nomark, plain;
        "Test Entry 3",
			noicon, nokey, nomark, plain;
	}
};

/* this ALRT and DITL are used as an About screen */
resource 'ALRT' (rAboutAlert, purgeable) {
	{40, 20, 194, 412},
	rAboutAlert,
	{ /* array: 4 elements */
		/* [1] */
		OK, visible, silent,
		/* [2] */
		OK, visible, silent,
		/* [3] */
		OK, visible, silent,
		/* [4] */
		OK, visible, silent
	},
    centerMainScreen       // Where to show the alert
};

resource 'DITL' (rAboutAlert, purgeable) {
	{ /* array DITLarray: 5 elements */
		/* [1] */
		{119, 8, 138, 80},
		Button {
			enabled,
			"OK"
		},
		/* [2] */
		{8, 8, 24, 264},
		StaticText {
			disabled,
			"Messages for Macintosh"
		},
		/* [3] */
		{32, 8, 48, 267},
		StaticText {
			disabled,
			"Copyright © 2021-22 Cameron Henlin"
		},
		/* [4] */
		{56, 8, 72, 166},
		StaticText {
			disabled,
			"cam.henlin@gmail.com"
		},
		/* [5] */
		{80, 8, 112, 407},
		StaticText {
			disabled,
			"https://github.com/CamHenlin/MessagesForMacintosh"
		}
	}
};


/* this ALRT and DITL are used as an error screen */

resource 'ALRT' (rUserAlert, purgeable) {
	{40, 20, 120, 260},
	rUserAlert,
	{ /* array: 4 elements */
		/* [1] */
		OK, visible, silent,
		/* [2] */
		OK, visible, silent,
		/* [3] */
		OK, visible, silent,
		/* [4] */
		OK, visible, silent
	},
    centerMainScreen       // Where to show the alert
};


resource 'DITL' (rUserAlert, purgeable) {
	{ /* array DITLarray: 3 elements */
		/* [1] */
		{50, 150, 70, 230},
		Button {
			enabled,
			"OK"
		},
		/* [2] */
		{10, 60, 30, 230},
		StaticText {
			disabled,
			"Sample - Error occurred!"
		},
		/* [3] */
		{8, 8, 40, 40},
		Icon {
			disabled,
			2
		}
	}
};


resource 'WIND' (rWindow, preload, purgeable) {
	{42, 4, 336, 506},
	zoomDocProc, visible, noGoAway, 0x0, "Messages",
    centerMainScreen       // Where to show the alert
};

resource 'RECT' (rStopRect, preload, purgeable) {
	{69, 69, 110, 110}
};

resource 'RECT' (rXRect, preload, purgeable) {
	{32, 32, 69, 69}
};

resource 'RECT' (rGoRect, preload, purgeable) {
	{120, 10, 220, 110}
};


/* here is the quintessential MultiFinder friendliness device, the SIZE resource */

resource 'SIZE' (-1) {
	dontSaveScreen,
	acceptSuspendResumeEvents,
	enableOptionSwitch,
	canBackground,				/* we can background; we don't currently, but our sleep value */
								/* guarantees we don't hog the Mac while we are in the background */
	multiFinderAware,			/* this says we do our own activate/deactivate; don't fake us out */
	backgroundAndForeground,	/* this is definitely not a background-only application! */
	dontGetFrontClicks,			/* change this is if you want "do first click" behavior like the Finder */
	ignoreChildDiedEvents,		/* essentially, I'm not a debugger (sub-launching) */
	not32BitCompatible,			/* this app should not be run in 32-bit address space */
	reserved,
	reserved,
	reserved,
	reserved,
	reserved,
	reserved,
	reserved,
	kPrefSize * 1024,
	kMinSize * 1024	
};