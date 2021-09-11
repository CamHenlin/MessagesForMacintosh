#include <Types.h>
#include <Resources.h>
#include <Quickdraw.h>
#include <Fonts.h>
#include <Events.h>
#include <Windows.h>
#include <Menus.h>
#include <TextEdit.h>
#include <Dialogs.h>
#include <ToolUtils.h>
#include <Memory.h>
#include <SegLoad.h>
#include <Files.h>
#include <OSUtils.h>
#include <DiskInit.h>
#include <Packages.h>
#include <Traps.h>
#include <Serial.h>
#include <Devices.h>
#include <stdio.h>
#include <string.h>
#include "Sample.h"
#include "SerialHelper.h"
#include "Quickdraw.h"
#include "output_js.h"
#include "coprocessorjs.h"


// needed by overview.c:
#include <limits.h>
#include <time.h>
#include <math.h>

#define WINDOW_WIDTH 510
#define WINDOW_HEIGHT 302

#define NK_ZERO_COMMAND_MEMORY
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_IMPLEMENTATION
#define NK_QUICKDRAW_IMPLEMENTATION
#define NK_MEMSET memset
#define NK_MEMCPY memcpy

void aFailed(char *file, int line) {
    
    MoveTo(10, 10);
    char *textoutput;
    sprintf(textoutput, "%s:%d", file, line);
    writeSerialPortDebug(boutRefNum, "assertion failure");
    writeSerialPortDebug(boutRefNum, textoutput);
    // hold the program - we want to be able to read the text! assuming anything after the assert would be a crash
    while (true) {}
}

#define NK_ASSERT(e) \
    if (!(e)) \
        aFailed(__FILE__, __LINE__)
#include "nuklear.h"
#include "nuklear_quickdraw.h"
#include "overview.c"
/* GMac is used to hold the result of a SysEnvirons call. This makes
   it convenient for any routine to check the environment. */
SysEnvRec	gMac;				/* set up by Initialize */

/* GHasWaitNextEvent is set at startup, and tells whether the WaitNextEvent
   trap is available. If it is false, we know that we must call GetNextEvent. */
Boolean		gHasWaitNextEvent;	/* set up by Initialize */

/* GInBackground is maintained by our osEvent handling routines. Any part of
   the program can check it to find out if it is currently in the background. */
Boolean		gInBackground;		/* maintained by Initialize and DoEvent */

// #define MAC_APP_DEBUGGING
/* The following globals are the state of the window. If we supported more than
   one window, they would be attatched to each document, rather than globals. */

/* Here are declarations for all of the C routines. In MPW 3.0 we can use
   actual prototypes for parameter type checking. */
void EventLoop( struct nk_context *ctx );
void DoEvent( EventRecord *event, struct nk_context *ctx );
void GetGlobalMouse( Point *mouse );
void DoUpdate( WindowPtr window );
void DoActivate( WindowPtr window, Boolean becomingActive );
void DoContentClick( WindowPtr window );
void AdjustMenus( void );
void DoMenuCommand( long menuResult );
Boolean DoCloseWindow( WindowPtr window );
void Terminate( void );
void Initialize( void );
Boolean GoGetRect( short rectID, Rect *theRect );
void ForceEnvirons( void );
Boolean IsAppWindow( WindowPtr window );
Boolean IsDAWindow( WindowPtr window );
Boolean TrapAvailable( short tNumber, TrapType tType );
void AlertUser( void );


/* Define HiWrd and LoWrd macros for efficiency. */
#define HiWrd(aLong)	(((aLong) >> 16) & 0xFFFF)
#define LoWrd(aLong)	((aLong) & 0xFFFF)

/* Define TopLeft and BotRight macros for convenience. Notice the implicit
   dependency on the ordering of fields within a Rect */
#define TopLeft(aRect)	(* (Point *) &(aRect).top)
#define BotRight(aRect)	(* (Point *) &(aRect).bottom)

// TODO: 
// set up coprocessor
// set up imessage service
// 	 new typescript one that responds via graphql endpoints?????
// finish setting up UI (maybe mock data from coprocessor?)
// need UI to start new chat threads
// need function to create new chat + send message in it
// QUESTION should the chat UI be a scrollable panel with labels inside of it so both sides of the conversation can be shown?
// further improve ui performance -- it's close! eliminate more on keyboard events and we'll be fine

char jsFunctionResponse[102400]; // Matches MAX_RECEIVE_SIZE

// function to send messages in chat
void sendMessage(message) {

	char output[128];
	// sprintf(output, "D%d,%d", x, y);

	callFunctionOnCoprocessor("runEvent", output, jsFunctionResponse);
}

// set up function to get messages in current chat
// 	 limit to recent messages 
// 	 figure out pagination?? button on the top that says "get previous chats"?
void getMessages(thread, page) {

	char output[128];
	// sprintf(output, "D%d,%d", x, y);

	callFunctionOnCoprocessor("runEvent", output, jsFunctionResponse);
}

// set up function to get available chat (fill buttons on right hand side)
//	 run it on some interval? make sure user is not typing!!!
void getChats(thread) {

	char output[128];
	// sprintf(output, "D%d,%d", x, y);

	callFunctionOnCoprocessor("runEvent", output, jsFunctionResponse);
}

static void boxTest(struct nk_context *ctx) {

    if (nk_begin(ctx, "Chats",  nk_rect(410, 0, 100, WINDOW_HEIGHT), NK_WINDOW_BORDER)) {

        nk_layout_row_dynamic(ctx, 25, 1);
        if (nk_button_label(ctx, "chat 1")) {

            writeSerialPortDebug(boutRefNum, "CLICK!");
        }
        nk_button_label(ctx, "chat 2");
        nk_button_label(ctx, "chat 3");
        nk_button_label(ctx, "chat 4");
        nk_button_label(ctx, "chat 5");
        nk_button_label(ctx, "chat 6");
        nk_button_label(ctx, "chat 7");
        nk_button_label(ctx, "chat 8");
        nk_button_label(ctx, "chat 9");
        nk_button_label(ctx, "chat 10");
        nk_button_label(ctx, "chat 11");
        nk_button_label(ctx, "chat 12");
        nk_button_label(ctx, "chat 13");
        nk_button_label(ctx, "chat 14");
        nk_button_label(ctx, "chat 15");
        nk_button_label(ctx, "chat 16");
        nk_button_label(ctx, "chat 17");
        nk_button_label(ctx, "chat 18");
        nk_button_label(ctx, "chat 19");
        nk_button_label(ctx, "chat 20");

    	nk_end(ctx);
    }

    if (nk_begin(ctx, "Message", nk_rect(0, 0, 410, WINDOW_HEIGHT), NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR)) {

		static char box_buffer[512];
		static char box_input_buffer[512];
		static int box_len;
		static int box_input_len;
		int options = NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR;

		// top part of the window
		nk_layout_row_dynamic(ctx, 20, 1);
		{
			// chat label
			nk_label(ctx, "chat name", NK_TEXT_ALIGN_CENTERED);
		}
		nk_layout_row_end(ctx);

		nk_layout_row_dynamic(ctx, 208, 1);
		{
			// chat contents
			nk_edit_string(ctx, NK_EDIT_BOX, box_buffer, &box_len, 512, nk_filter_default);
		}
		nk_layout_row_end(ctx);

		// bottom text input		
		nk_layout_row_begin(ctx, NK_STATIC, 50, 2);
		{
			nk_layout_row_push(ctx, 300); // 40% wide

			nk_edit_string(ctx, NK_EDIT_BOX, box_input_buffer, &box_input_len, 512, nk_filter_default);

			nk_layout_row_push(ctx, 96); // 40% wide
			if (nk_button_label(ctx, "send")) {
        		//fprintf(stdout, "pushed!\n");
			}

            nk_layout_row_end(ctx);
		}
		nk_layout_row_end(ctx);

    	nk_end(ctx);
	}


}

#pragma segment Main
void main()
{	
	Initialize();					/* initialize the program */

	UnloadSeg((Ptr) Initialize);	/* note that Initialize must not be in Main! */

    setupCoprocessor("nuklear", "modem"); // could also be "printer", modem is 0 in PCE settings - printer would be 1

    char programResult[MAX_RECEIVE_SIZE];
    sendProgramToCoprocessor(OUTPUT_JS, programResult);

    nk_quickdraw_init(WINDOW_WIDTH, WINDOW_HEIGHT);

    struct nk_context *ctx;

    #ifdef MAC_APP_DEBUGGING
    
    	writeSerialPortDebug(boutRefNum, "call nk_init");
    #endif

    ctx = nk_quickdraw_init(WINDOW_WIDTH, WINDOW_HEIGHT);

    #ifdef MAC_APP_DEBUGGING

	    writeSerialPortDebug(boutRefNum, "call into event loop");
	#endif

	EventLoop(ctx);					/* call the main event loop */
}


#pragma segment Main
void EventLoop(struct nk_context *ctx)
{
	RgnHandle cursorRgn;
	Boolean	gotEvent;
	EventRecord	event;
	Point mouse;
	cursorRgn = NewRgn();

	Boolean firstOrMouseMove = true;
	int lastMouseHPos = 0;
	int lastMouseVPos = 0;

	do {

		Boolean beganInput = false;

		#ifdef MAC_APP_DEBUGGING

	        writeSerialPortDebug(boutRefNum, "nk_input_begin");
	    #endif

        #ifdef MAC_APP_DEBUGGING

        	writeSerialPortDebug(boutRefNum, "nk_input_begin complete");
        #endif


		GetGlobalMouse(&mouse);


		// as far as i can tell, there is no way to event on mouse movement with mac libraries,
		// so we are just going to track on our own, and create our own events.
		// this seems kind of a bummer to not pass this to event handling code, but to make
		// it work we would need to create a dummy event, etc, so we will just directly 
		// call the nk_input_motion command
        if (lastMouseHPos != mouse.h || lastMouseVPos != mouse.v) {

        	#ifdef MAC_APP_DEBUGGING

        		writeSerialPortDebug(boutRefNum, "nk_input_motion!");
        	#endif

        	firstOrMouseMove = true;

            Point tempPoint;
            SetPt(&tempPoint, mouse.h, mouse.v);
            GlobalToLocal(&tempPoint);

            beganInput = true;
        	nk_input_begin(ctx);
        	nk_input_motion(ctx, tempPoint.h, tempPoint.v);
        }

        lastMouseHPos = mouse.h;
        lastMouseVPos = mouse.v;

		if (gHasWaitNextEvent) {

			gotEvent = WaitNextEvent(everyEvent, &event, MAXLONG, cursorRgn);
		} else {

			SystemTask();
			gotEvent = GetNextEvent(everyEvent, &event);
		}

	    long start;
	    long end;
	    long total;
	    long eventTime0;
	    long eventTime1;
	    long eventTime2;
	    long eventTime3;

	    start = TickCount();
		if (gotEvent) {

			#ifdef MAC_APP_DEBUGGING

        		writeSerialPortDebug(boutRefNum, "calling to DoEvent");
        	#endif

        	if (!beganInput) {

        		nk_input_begin(ctx);
        	}

        	beganInput = true;

			DoEvent(&event, ctx);

			#ifdef MAC_APP_DEBUGGING

        		writeSerialPortDebug(boutRefNum, "done with DoEvent");
        	#endif
		}

	    end = TickCount();

		total = end - start;
	    eventTime0 = total;// / 60.0;
	    start = TickCount();
		#ifdef MAC_APP_DEBUGGING

        	writeSerialPortDebug(boutRefNum, "nk_input_end");
        #endif

        if (beganInput) {

        	nk_input_end(ctx);
        }

        #ifdef MAC_APP_DEBUGGING
        	
        	writeSerialPortDebug(boutRefNum, "nk_input_end complete");
        #endif

        // only re-render if there is an event, prevents screen flickering
        if (gotEvent || firstOrMouseMove) {

        	firstOrMouseMove = false;

	        #ifdef MAC_APP_DEBUGGING

		        writeSerialPortDebug(boutRefNum, "nk_quickdraw_render");
		    #endif



		    start = TickCount();

	        //calculator(ctx);
	        //overview(ctx);
        	boxTest(ctx);


		    end = TickCount();

    		total = end - start;
		    eventTime1 = total;// / 60.0;
		    start = TickCount();
        	nk_quickdraw_render(FrontWindow(), ctx);
		    end = TickCount();

    		total = end - start;
		    eventTime2 = total;// / 60.0;
		    start = TickCount();
    		nk_clear(ctx);
		    end = TickCount();

    		total = end - start;
		    eventTime3 = total;// / 60.0;
		    start = TickCount();
    
		    // char logx[255];
		    // sprintf(logx, "EventLoop() eventTime0 (handle event) %ld, eventTime1 (nuklear UI) %ld, eventTime2 (render function) %ld, eventTime3 (clear) %ld ticks to execute\n", eventTime0, eventTime1, eventTime2, eventTime3);
		    // writeSerialPortDebug(boutRefNum, logx);
        }


    	#ifdef MAC_APP_DEBUGGING

        	writeSerialPortDebug(boutRefNum, "nk_input_render complete");
        #endif
	} while ( true );	/* loop forever; we quit via ExitToShell */
} /*EventLoop*/


/* Do the right thing for an event. Determine what kind of event it is, and call
 the appropriate routines. */

#pragma segment Main
void DoEvent(EventRecord *event, struct nk_context *ctx) {

	short part;
    short err;
	WindowPtr window;
	Boolean	hit;
	char key;
	Point aPoint;

	switch ( event->what ) {

        case mouseUp:

	    	#ifdef MAC_APP_DEBUGGING
	        	writeSerialPortDebug(boutRefNum, "mouseup");
        	#endif

            part = FindWindow(event->where, &window);
            switch (part)
            {
                case inContent:
                    nk_quickdraw_handle_event(event, ctx);
                    break;
                default:
                	break;
            }
            break;
		case mouseDown:


	    	#ifdef MAC_APP_DEBUGGING
	        	writeSerialPortDebug(boutRefNum, "mousedown");
	        #endif

			part = FindWindow(event->where, &window);
			switch ( part ) {
				case inMenuBar:				/* process a mouse menu command (if any) */
					AdjustMenus();
					DoMenuCommand(MenuSelect(event->where));
					break;
				case inSysWindow:			/* let the system handle the mouseDown */
					SystemClick(event, window);
					break;
				case inContent:
					if ( window != FrontWindow() ) {
						SelectWindow(window);

					}
                    nk_quickdraw_handle_event(event, ctx);
					break;
				case inDrag:				/* pass screenBits.bounds to get all gDevices */
					DragWindow(window, event->where, &qd.screenBits.bounds);
					break;
				case inGrow:
					break;
				case inZoomIn:
				case inZoomOut:
					hit = TrackBox(window, event->where, part);
					if ( hit ) {
						SetPort(window);				/* the window must be the current port... */
						EraseRect(&window->portRect);	/* because of a bug in ZoomWindow */
						ZoomWindow(window, part, true);	/* note that we invalidate and erase... */
						InvalRect(&window->portRect);	/* to make things look better on-screen */
					}
					break;
			}
			break;
		case keyDown:
		case autoKey:						/* check for menukey equivalents */
        	

	    	#ifdef MAC_APP_DEBUGGING
	        	writeSerialPortDebug(boutRefNum, "key");
	        #endif

			key = event->message & charCodeMask;
			if ( event->modifiers & cmdKey )	{		/* Command key down */
				if ( event->what == keyDown ) {
					AdjustMenus();						/* enable/disable/check menu items properly */
					DoMenuCommand(MenuKey(key));
				}
			}

            nk_quickdraw_handle_event(event, ctx);
			break;
		case activateEvt:
			
    		#ifdef MAC_APP_DEBUGGING
        		writeSerialPortDebug(boutRefNum, "activate");
        	#endif
			DoActivate((WindowPtr) event->message, (event->modifiers & activeFlag) != 0);
			break;
		case updateEvt:
			
			#ifdef MAC_APP_DEBUGGING
        		writeSerialPortDebug(boutRefNum, "update");
        	#endif
			DoUpdate((WindowPtr) event->message);
			break;
		/*	1.01 - It is not a bad idea to at least call DIBadMount in response
			to a diskEvt, so that the user can format a floppy. */
		case diskEvt:
			
    		#ifdef MAC_APP_DEBUGGING
        		writeSerialPortDebug(boutRefNum, "disk");
        	#endif
			if ( HiWord(event->message) != noErr ) {
				SetPt(&aPoint, kDILeft, kDITop);
				err = DIBadMount(aPoint, event->message);
			}
			break;

		case osEvt:
			
    		#ifdef MAC_APP_DEBUGGING
        		writeSerialPortDebug(boutRefNum, "os");
        	#endif

			// this should be trigger on mousemove but does not -- if we can figure that out, we should call through to 
			// nk_quickdraw_handle_event, and allow it to handle the mousemove events
		/*	1.02 - must BitAND with 0x0FF to get only low byte */
			switch ((event->message >> 24) & 0x0FF) {		/* high byte of message */
				case kSuspendResumeMessage:		/* suspend/resume is also an activate/deactivate */
					gInBackground = (event->message & kResumeMask) == 0;
					DoActivate(FrontWindow(), !gInBackground);
					break;
			}
			break;
	}
} /*DoEvent*/

/*	Get the global coordinates of the mouse. When you call OSEventAvail

	it will return either a pending event or a null event. In either case,
	the where field of the event record will contain the current position
	of the mouse in global coordinates and the modifiers field will reflect
	the current state of the modifiers. Another way to get the global
	coordinates is to call GetMouse and LocalToGlobal, but that requires
	being sure that thePort is set to a valid port. */

#pragma segment Main
void GetGlobalMouse(mouse)
	Point	*mouse;
{
	EventRecord	event;
	
	OSEventAvail(kNoEvents, &event);	/* we aren't interested in any events */
	*mouse = event.where;				/* just the mouse position */
} /*GetGlobalMouse*/


/*	This is called when an update event is received for a window.
	It calls DrawWindow to draw the contents of an application window.
	As an effeciency measure that does not have to be followed, it
	calls the drawing routine only if the visRgn is non-empty. This
	will handle situations where calculations for drawing or drawing
	itself is very time-consuming. */

#pragma segment Main
void DoUpdate(window)
	WindowPtr	window;
{
	if ( IsAppWindow(window) ) {
		BeginUpdate(window);		
		EndUpdate(window);
	}
} /*DoUpdate*/


/*	This is called when a window is activated or deactivated.
	In Sample, the Window Manager's handling of activate and
	deactivate events is sufficient. Other applications may have
	TextEdit records, controls, lists, etc., to activate/deactivate. */

#pragma segment Main
void DoActivate(window, becomingActive)
	WindowPtr	window;
	Boolean		becomingActive;
{
	if ( IsAppWindow(window) ) {
		if ( becomingActive )
			/* do whatever you need to at activation */ ;
		else
			/* do whatever you need to at deactivation */ ;
	}
} /*DoActivate*/



/* Draw the contents of the application window. We do some drawing in color, using
   Classic QuickDraw's color capabilities. This will be black and white on old
   machines, but color on color machines. At this point, the windowÕs visRgn
   is set to allow drawing only where it needs to be done. */

static Rect				okayButtonBounds;


/*	Enable and disable menus based on the current state.
	The user can only select enabled menu items. We set up all the menu items
	before calling MenuSelect or MenuKey, since these are the only times that
	a menu item can be selected. Note that MenuSelect is also the only time
	the user will see menu items. This approach to deciding what enable/
	disable state a menu item has the advantage of concentrating all
	the decision-making in one routine, as opposed to being spread throughout
	the application. Other application designs may take a different approach
	that is just as valid. */

#pragma segment Main
void AdjustMenus()
{
	WindowPtr	window;
	MenuHandle	menu;

	window = FrontWindow();

	menu = GetMenuHandle(mFile);
	if ( IsDAWindow(window) )		/* we can allow desk accessories to be closed from the menu */
		EnableItem(menu, iClose);
	else
		DisableItem(menu, iClose);	/* but not our traffic light window */

	menu = GetMenuHandle(mEdit);
	if ( IsDAWindow(window) ) {		/* a desk accessory might need the edit menuÉ */
		EnableItem(menu, iUndo);
		EnableItem(menu, iCut);
		EnableItem(menu, iCopy);
		EnableItem(menu, iClear);
		EnableItem(menu, iPaste);
	} else {						/* Ébut we donÕt use it */
		DisableItem(menu, iUndo);
		DisableItem(menu, iCut);
		DisableItem(menu, iCopy);
		DisableItem(menu, iClear);
		DisableItem(menu, iPaste);
	}

	menu = GetMenuHandle(mLight);
	if ( IsAppWindow(window) ) {	/* we know that it must be the traffic light */
		EnableItem(menu, iStop);
		EnableItem(menu, iGo);
	} else {
		DisableItem(menu, iStop);
		DisableItem(menu, iGo);
	}
} /*AdjustMenus*/


/*	This is called when an item is chosen from the menu bar (after calling
	MenuSelect or MenuKey). It performs the right operation for each command.
	It is good to have both the result of MenuSelect and MenuKey go to
	one routine like this to keep everything organized. */

#pragma segment Main
void DoMenuCommand(menuResult)
	long		menuResult;
{
	short		menuID;				/* the resource ID of the selected menu */
	short		menuItem;			/* the item number of the selected menu */
	short		itemHit;
	Str255		daName;
	short		daRefNum;
	Boolean		handledByDA;

	menuID = HiWord(menuResult);	/* use macros for efficiency to... */
	menuItem = LoWord(menuResult);	/* get menu item number and menu number */
	switch ( menuID ) {
		case mApple:
			switch ( menuItem ) {
				// case iAbout:		/* bring up alert for About */
                default:
					itemHit = Alert(rAboutAlert, nil);
					break;

                /*
				default:			// all non-About items in this menu are DAs
					// type Str255 is an array in MPW 3
					GetItem(GetMHandle(mApple), menuItem, daName);
					daRefNum = OpenDeskAcc(daName);
					break;
                */
			}
			break;
		case mFile:
			switch ( menuItem ) {
				case iClose:
					DoCloseWindow(FrontWindow());
					break;
				case iQuit:
					Terminate();
					break;
			}
			break;
		case mEdit:					/* call SystemEdit for DA editing & MultiFinder */
			handledByDA = SystemEdit(menuItem-1);	/* since we donÕt do any Editing */
			break;
		case mLight:
			switch ( menuItem ) {
				case iStop:
					break;
				case iGo:
					break;
			}
			break;

        case mHelp:
			switch ( menuItem ) {
				case iQuickHelp:
                    itemHit = Alert(rAboutAlert, nil);                        
					break;
				case iUserGuide:
                {
					// AlertUser();

                    // write data to serial port
                    // Configure PCE/macplus to map serial port to ser_b.out. This port can then be used for debug output
                    // by using: tail -f ser_b.out
                    // OSErr res = writeSerialPortDebug(boutRefNum, "Hello World");
                    // if (res < 0)
                    //    AlertUser();

                    // http://www.mac.linux-m68k.org/devel/macalmanac.php
                    short* ROM85      = (short*) 0x028E; 
                    short* SysVersion = (short*) 0x015A; 
                    short* ScrVRes    = (short*) 0x0102; 
                    short* ScrHRes    = (short*) 0x0104; 
                    unsigned long*  Time       = (unsigned long*) 0x020C; 

                    char str2[255];
                    sprintf(str2, "ROM85: %d - SysVersion: %d - VRes: %d - HRes: %d - Time: %lu", *ROM85, *SysVersion, *ScrVRes, *ScrHRes, *Time);
                    // writeSerialPortDebug(boutRefNum, str2);               

                    Boolean is128KROM = ((*ROM85) > 0);
                    Boolean hasSysEnvirons = false;
                    Boolean hasStripAddr = false;
                    Boolean hasSetDefaultStartup = false;
                    if (is128KROM)
                    {
                        UniversalProcPtr trapSysEnv = GetOSTrapAddress(_SysEnvirons);
                        UniversalProcPtr trapStripAddr = GetOSTrapAddress(_StripAddress);
                        UniversalProcPtr trapSetDefaultStartup = GetOSTrapAddress(_SetDefaultStartup);
                        UniversalProcPtr trapUnimpl = GetOSTrapAddress(_Unimplemented);

                        hasSysEnvirons = (trapSysEnv != trapUnimpl);
                        hasStripAddr = (trapStripAddr != trapUnimpl);
                        hasSetDefaultStartup = (trapSetDefaultStartup != trapUnimpl);
                    }
                    
                    sprintf(str2, "is128KROM: %d - hasSysEnvirons: %d - hasStripAddr: %d - hasSetDefaultStartup - %d", 
                                    is128KROM, hasSysEnvirons, hasStripAddr, hasSetDefaultStartup);
                    // writeSerialPortDebug(boutRefNum, str2);               

					break;
                }
			}
			break;
	}
	HiliteMenu(0);					/* unhighlight what MenuSelect (or MenuKey) hilited */
} /*DoMenuCommand*/

/* Close a window. This handles desk accessory and application windows. */

/*	1.01 - At this point, if there was a document associated with a
	window, you could do any document saving processing if it is 'dirty'.
	DoCloseWindow would return true if the window actually closed, i.e.,
	the user didnÕt cancel from a save dialog. This result is handy when
	the user quits an application, but then cancels the save of a document
	associated with a window. */

#pragma segment Main
Boolean DoCloseWindow(window)
	WindowPtr	window;
{
	/* if ( IsDAWindow(window) )
		CloseDeskAcc(((WindowPeek) window)->windowKind);
	else */ if ( IsAppWindow(window) )
		CloseWindow(window);
	return true;
} /*DoCloseWindow*/


/* Clean up the application and exit. We close all of the windows so that
 they can update their documents, if any. */
 
/*	1.01 - If we find out that a cancel has occurred, we won't exit to the
	shell, but will return instead. */

#pragma segment Main
void Terminate()
{
	WindowPtr	aWindow;
	Boolean		closed;
	
	closed = true;
	do {
		aWindow = FrontWindow();				/* get the current front window */
		if (aWindow != nil)
			closed = DoCloseWindow(aWindow);	/* close this window */	
	}
	while (closed && (aWindow != nil));
	if (closed)
		ExitToShell();							/* exit if no cancellation */
} /*Terminate*/


#pragma segment Initialize
void Initialize()
{
	Handle		menuBar;
	WindowPtr	window;
	long		total, contig;
	EventRecord event;
	short		count;

	gInBackground = false;

	InitGraf((Ptr) &qd.thePort);
	InitFonts();
	InitWindows();
	InitMenus();
	TEInit();
	InitDialogs(nil);
	InitCursor();

	for (count = 1; count <= 3; count++) {
		EventAvail(everyEvent, &event);
	}

	window = (WindowPtr) NewPtr(sizeof(WindowRecord));

	if ( window == nil ) {
		AlertUser();
	}

	window = GetNewWindow(rWindow, (Ptr) window, (WindowPtr) -1);
	SetPort(window);

	menuBar = GetNewMBar(rMenuBar);			/* read menus into menu bar */

	if ( menuBar == nil ) {

		AlertUser();
	}

	SetMenuBar(menuBar);					/* install menus */
	DisposeHandle(menuBar);
	AppendResMenu(GetMenuHandle(mApple), 'DRVR');	/* add DA names to Apple menu */    
	DrawMenuBar();
	
} /*Initialize*/


#pragma segment Main
Boolean IsAppWindow(window)
	WindowPtr	window;
{
	short windowKind;

	if ( window == nil ) {
		return false;
	}	/* application windows have windowKinds = userKind (8) */

	windowKind = ((WindowPeek) window)->windowKind;

	return (windowKind = userKind);
} /*IsAppWindow*/


/* Check to see if a window belongs to a desk accessory. */

#pragma segment Main
Boolean IsDAWindow(window)
	WindowPtr	window;
{
	if ( window == nil ) {
		return false;
	}
		/* DA windows have negative windowKinds */
	return ((WindowPeek) window)->windowKind < 0;
} /*IsDAWindow*/


#pragma segment Initialize
Boolean TrapAvailable(tNumber,tType)
	short		tNumber;
	TrapType	tType;
{    
	if ( ( tType == ToolTrap ) &&
		( gMac.machineType > envMachUnknown ) &&
		( gMac.machineType < envMacII ) ) {		/* it's a 512KE, Plus, or SE */
		tNumber = tNumber & 0x03FF;
		if ( tNumber > 0x01FF )		 {			/* which means the tool traps */
			tNumber = _Unimplemented;			/* only go to 0x01FF */
		}
	}

	return NGetTrapAddress(tNumber, tType) != GetTrapAddress(_Unimplemented);
} /*TrapAvailable*/


#pragma segment Main
void AlertUser() {
	short itemHit;

	SetCursor(&qd.arrow);
	itemHit = Alert(rUserAlert, nil);
	ExitToShell();
} /* AlertUser */