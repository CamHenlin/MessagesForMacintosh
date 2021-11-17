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
#include <Sound.h>
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
// #define NK_INCLUDE_STANDARD_VARARGS
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

int mouse_x;
int mouse_y;

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
// - IN PROGRESS  new message window -- needs to blank out messages, then needs fixes on new mac end
// - chat during the day for a few minutes and figure out small issues
// - start writing blog posts
// - get new messages in other chats and display some sort of alert
// - why does the automator script sometimes not send


char jsFunctionResponse[102400]; // Matches MAX_RECEIVE_SIZE

Boolean firstOrMouseMove = true;
int haveRun = 0;
int chatFriendlyNamesCounter = 0;
int ipAddressSet = 0;
int sendNewChat = 0;
char chatFriendlyNames[16][64];
char activeChat[64];
int activeMessageCounter = 0;
char activeChatMessages[64][2048];
char box_input_buffer[2048];
char ip_input_buffer[255];
char new_message_input_buffer[255];
short box_len;
short box_input_len;
short new_message_input_buffer_len;
static short ip_input_buffer_len; // TODO: setting a length here will make the default `http://...` work, but doesn't work right -- maybe due to perf work in nuklear
int shouldScrollMessages = 0;
int forceRedraw = 2; // this is how many 'iterations' of the UI that we need to see every element for
int messagesScrollBarLocation = 0;
int messageWindowWasDormant = 0;
int coprocessorLoaded = 0;

void getMessagesFromjsFunctionResponse() {

	for (int i = 0; i < 64; i++) {

		memset(&activeChatMessages[i], '\0', 2048);
	}

	activeMessageCounter = 0;

	// writeSerialPortDebug(boutRefNum, "BEGIN");

	// writeSerialPortDebug(boutRefNum, jsFunctionResponse);
	char *token = (char *)strtokm(jsFunctionResponse, "ENDLASTMESSAGE");
	// loop through the string to extract all other tokens
	while (token != NULL) {

    	// writeSerialPortDebug(boutRefNum, "LOAD VALUE TO TOKEN");
    	// writeSerialPortDebug(boutRefNum, token);
		sprintf(activeChatMessages[activeMessageCounter], "%s", token); 
    	// writeSerialPortDebug(boutRefNum, activeChatMessages[activeMessageCounter]);
    	// writeSerialPortDebug(boutRefNum, "DONE! LOAD VALUE TO TOKEN");
		token = (char *)strtokm(NULL, "ENDLASTMESSAGE");
		activeMessageCounter++;
	}

	return;
}
// function to send messages in chat
void sendMessage() {

	char output[2048];
	sprintf(output, "%s&&&%s", activeChat, box_input_buffer);

	callFunctionOnCoprocessor("sendMessage", output, jsFunctionResponse);
	getMessagesFromjsFunctionResponse();

	return;
}

void sendIPAddressToCoprocessor() {

	char output[2048];
	sprintf(output, "%s", ip_input_buffer);


    writeSerialPortDebug(boutRefNum, output);
	callFunctionOnCoprocessor("setIPAddress", output, jsFunctionResponse);

	return;
}
// set up function to get messages in current chat
// 	 limit to recent messages 
// 	 figure out pagination?? button on the top that says "get previous chats"?
void getMessages(char *thread, int page) {

	char output[62];
	sprintf(output, "%s&&&%d", thread, page);
    // writeSerialPortDebug(boutRefNum, output);

	callFunctionOnCoprocessor("getMessages", output, jsFunctionResponse);
    // writeSerialPortDebug(boutRefNum, jsFunctionResponse);
    getMessagesFromjsFunctionResponse();

	return;
}

void getHasNewMessagesInChat(char *thread) {

	char output[62];
	sprintf(output, "%s", thread);
    // writeSerialPortDebug(boutRefNum, output);

	callFunctionOnCoprocessor("hasNewMessagesInChat", output, jsFunctionResponse);
    writeSerialPortDebug(boutRefNum, jsFunctionResponse);
    if (!strcmp(jsFunctionResponse, "true")) {

    	writeSerialPortDebug(boutRefNum, "update current chat");
		SysBeep(1);
		getMessages(thread, 0);
		// force redraw
		firstOrMouseMove = true;
	} else {


	}

	return;
}
// set up function to get available chat (fill buttons on right hand side)
//	 run it on some interval? make sure user is not typing!!!
void getChats() {

	if (haveRun) {

		return;
	}

	haveRun = 1;

	callFunctionOnCoprocessor("getChats", "", jsFunctionResponse);

	char * token = (char *)strtokm(jsFunctionResponse, ",");
	// loop through the string to extract all other tokens
	while (token != NULL) {
		sprintf(chatFriendlyNames[chatFriendlyNamesCounter++], "%s", token); 
		token = (char *)strtokm(NULL, ",");
	}

	return;
}

struct nk_rect graphql_input_window_size;
struct nk_rect chats_window_size;
struct nk_rect messages_window_size;
struct nk_rect message_input_window_size;

static void boxTest(struct nk_context *ctx) {

	// prompt the user for the graphql instance
	if (!coprocessorLoaded) {

	    if (nk_begin_titled(ctx, "Loading coprocessor services", "Loading coprocessor services", graphql_input_window_size, NK_WINDOW_TITLE|NK_WINDOW_BORDER)) {

	    	nk_layout_row_begin(ctx, NK_STATIC, 20, 1);
	    	{
				nk_layout_row_push(ctx, 200); // 40% wide
				nk_label_wrap(ctx, "Please wait");
	    	}
	    	nk_layout_row_end(ctx);

	    	nk_end(ctx);
	    }

		return;
	}

	// prompt the user for the graphql instance
	if (!ipAddressSet) {

	    if (nk_begin_titled(ctx, "Enter iMessage GraphQL Server", "Enter iMessage GraphQL Server", graphql_input_window_size, NK_WINDOW_TITLE|NK_WINDOW_BORDER)) {

	    	nk_layout_row_begin(ctx, NK_STATIC, 20, 1);
	    	{
				nk_layout_row_push(ctx, 200); // 40% wide
				nk_label_wrap(ctx, "ex: http://127.0.0.1");
	    	}
	    	nk_layout_row_end(ctx);

			nk_layout_row_begin(ctx, NK_STATIC, 30, 2);
			{
				nk_layout_row_push(ctx, WINDOW_WIDTH / 2 - 90); // 40% wide

				nk_edit_string(ctx, NK_EDIT_SIMPLE, ip_input_buffer, &ip_input_buffer_len, 255, nk_filter_default);

				nk_layout_row_push(ctx, 60); // 40% wide
				if (nk_button_label(ctx, "save")) {
	        	
	        		ipAddressSet = 1;
	        		forceRedraw = 2;
	        		sendIPAddressToCoprocessor();
				}
			}
			nk_layout_row_end(ctx);

	    	nk_end(ctx);
	    }

		return;
	}

	// prompt the user for  new chat
	if (sendNewChat) {

	    if (nk_begin_titled(ctx, "Enter New Message Recipient", "Enter New Message Recipient",  nk_rect(WINDOW_WIDTH / 4, WINDOW_HEIGHT / 4, WINDOW_WIDTH / 2, 120), NK_WINDOW_TITLE|NK_WINDOW_BORDER)) {

			nk_layout_row_begin(ctx, NK_STATIC, 30, 2);
			{
				nk_layout_row_push(ctx, WINDOW_WIDTH / 2 - 110); // 40% wide

				nk_edit_string(ctx, NK_EDIT_SIMPLE, new_message_input_buffer, &new_message_input_buffer_len, 2048, nk_filter_default);

				nk_layout_row_push(ctx, 80); // 40% wide
				if (nk_button_label(ctx, "open chat")) {
	        	
	        		sendNewChat = 0;
		        	forceRedraw = 2;
	        		// sendIPAddressToCoprocessor();

	        		sprintf(activeChat, new_message_input_buffer);
				}
			}
			nk_layout_row_end(ctx);

	    	nk_end(ctx);
	    }

		return;
	}


    if (nk_begin(ctx, "Chats", chats_window_size, NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR)) {

        getChats();

		nk_layout_row_begin(ctx, NK_STATIC, 25, 1);
		{
	        for (int i = 0; i < chatFriendlyNamesCounter; i++) {

				// only display the first 8 chats, create new chat if you need someone not in your list
	        	if (i > 9) {

	        		continue;
	        	}

				nk_layout_row_push(ctx, 185); // 40% wide

		        if (nk_button_label(ctx, chatFriendlyNames[i])) {

		            sprintf(activeChat, "%s", chatFriendlyNames[i]);
		            getMessages(activeChat, 0);
					shouldScrollMessages = 1;
		        }
	        }
		}
		nk_layout_row_end(ctx);

    	nk_end(ctx);
    }


    if (nk_begin(ctx, "Message Input", message_input_window_size, NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR)) {

		// bottom text input		
		nk_layout_row_begin(ctx, NK_STATIC, 40, 2);
		{
			nk_layout_row_push(ctx, 220); // 40% wide

			nk_edit_string(ctx, NK_EDIT_BOX, box_input_buffer, &box_input_len, 2048, nk_filter_default);

			nk_layout_row_push(ctx, 76); // 40% wide
			if (nk_button_label(ctx, "send")) {
        		//fprintf(stdout, "pushed!\n");
        		sendMessage();

				memset(&box_input_buffer, '\0', 2048);
			}
		}
		nk_layout_row_end(ctx);

    	nk_end(ctx);
	}


    if (nk_begin_titled(ctx, "Message", activeChat, messages_window_size, NK_WINDOW_BORDER|NK_WINDOW_TITLE)) {

		nk_layout_row_begin(ctx, NK_STATIC, 15, 1);
		{
		    for (int i = 0; i < activeMessageCounter; i++) {
		        
				nk_layout_row_push(ctx, 285); // 40% wide
				// message label
	            // writeSerialPortDebug(boutRefNum, "create label!");
	            // writeSerialPortDebug(boutRefNum, activeChatMessages[i]);

				nk_label_wrap(ctx, activeChatMessages[i]);
		    }

		    if (shouldScrollMessages) {

				ctx->current->scrollbar.y = 10000;
				shouldScrollMessages = 0;
		    }
		}

		nk_layout_row_end(ctx);
    	nk_end(ctx);
    }
}

#pragma segment Main
void main()
{	
	Initialize();					/* initialize the program */
    sprintf(activeChat, "no active chat");

	UnloadSeg((Ptr) Initialize);	/* note that Initialize must not be in Main! */


    struct nk_context *ctx;

    #ifdef MAC_APP_DEBUGGING
    
    	// writeSerialPortDebug(boutRefNum, "call nk_init");
    #endif

    graphql_input_window_size = nk_rect(WINDOW_WIDTH / 4, WINDOW_HEIGHT / 4, WINDOW_WIDTH / 2, 120);
	chats_window_size = nk_rect(0, 0, 200, WINDOW_HEIGHT);
	messages_window_size = nk_rect(200, 0, 310, WINDOW_HEIGHT - 50);
	message_input_window_size = nk_rect(200, WINDOW_HEIGHT - 50, 310, 50);
    ctx = nk_quickdraw_init(WINDOW_WIDTH, WINDOW_HEIGHT);

   	// run our nuklear app one time to render the window telling us to be patient for the coprocessor
   	// app to load up 
	nk_input_begin(ctx);
	nk_input_end(ctx);
	boxTest(ctx);
	nk_quickdraw_render(FrontWindow(), ctx);
	nk_clear(ctx);
	SysBeep(1);

    writeSerialPortDebug(boutRefNum, "setupCoprocessor!");
    setupCoprocessor("nuklear", "modem"); // could also be "printer", modem is 0 in PCE settings - printer would be 1

    char programResult[MAX_RECEIVE_SIZE];
    writeSerialPortDebug(boutRefNum, "sendProgramToCoprocessor!");
    sendProgramToCoprocessor(OUTPUT_JS, programResult);

	coprocessorLoaded = 1;
    sprintf(ip_input_buffer, "http://");

    #ifdef MAC_APP_DEBUGGING

	    // writeSerialPortDebug(boutRefNum, "call into event loop");
	#endif

	EventLoop(ctx);					/* call the main event loop */
}


#pragma segment Main
void EventLoop(struct nk_context *ctx)
{
	RgnHandle cursorRgn;
	Boolean	gotEvent;
	Boolean hasNextEvent;
	EventRecord	event;
	EventRecord nextEventRecord;
	Point mouse;
	cursorRgn = NewRgn();

	int lastMouseHPos = 0;
	int lastMouseVPos = 0;
	int lastUpdatedTickCount = 0;

	do {

		// check for new stuff ever 5 sec?
		if (TickCount() - lastUpdatedTickCount > 300) {

    		writeSerialPortDebug(boutRefNum, "update by tick count");
			lastUpdatedTickCount = TickCount();

			if (strcmp(activeChat, "no active chat")) {

    			writeSerialPortDebug(boutRefNum, "check chat");
				getHasNewMessagesInChat(activeChat);
			}
		} 

		Boolean beganInput = false;

		#ifdef MAC_APP_DEBUGGING

	        // writeSerialPortDebug(boutRefNum, "nk_input_begin");
	    #endif

        #ifdef MAC_APP_DEBUGGING

        	// writeSerialPortDebug(boutRefNum, "nk_input_begin complete");
        #endif

		GetGlobalMouse(&mouse);

		// as far as i can tell, there is no way to event on mouse movement with mac libraries,
		// so we are just going to track on our own, and create our own events.
		// this seems kind of a bummer to not pass this to event handling code, but to make
		// it work we would need to create a dummy event, etc, so we will just directly 
		// call the nk_input_motion command
        if (lastMouseHPos != mouse.h || lastMouseVPos != mouse.v) {

        	#ifdef MAC_APP_DEBUGGING

        		// writeSerialPortDebug(boutRefNum, "nk_input_motion!");
        	#endif

        	firstOrMouseMove = true;

            Point tempPoint;
            SetPt(&tempPoint, mouse.h, mouse.v);
            GlobalToLocal(&tempPoint);

            beganInput = true;
        	nk_input_begin(ctx);
        	nk_input_motion(ctx, tempPoint.h, tempPoint.v);

			mouse_x = tempPoint.h;
			mouse_y = tempPoint.v;

			lastUpdatedTickCount = TickCount();
        }

        lastMouseHPos = mouse.h;
        lastMouseVPos = mouse.v;

		SystemTask();
		gotEvent = GetNextEvent(everyEvent, &event);

	    // drain all events before rendering
		while (gotEvent) {

			lastUpdatedTickCount = TickCount();

			#ifdef MAC_APP_DEBUGGING

        		writeSerialPortDebug(boutRefNum, "calling to DoEvent");
        	#endif

        	if (!beganInput) {

        		nk_input_begin(ctx);
        		beganInput = true;
        	}

			DoEvent(&event, ctx);

			#ifdef MAC_APP_DEBUGGING

        		writeSerialPortDebug(boutRefNum, "done with DoEvent");
        	#endif

        	gotEvent = GetNextEvent(everyEvent, &event);
		}

        // only re-render if there is an event, prevents screen flickering
        if (beganInput || firstOrMouseMove) {

        	nk_input_end(ctx);

        	firstOrMouseMove = false;

	        #ifdef MAC_APP_DEBUGGING

		        // writeSerialPortDebug(boutRefNum, "nk_quickdraw_render");
		    #endif

			boxTest(ctx);

			nk_quickdraw_render(FrontWindow(), ctx);

			nk_clear(ctx);
        }


    	#ifdef MAC_APP_DEBUGGING

        	// writeSerialPortDebug(boutRefNum, "nk_input_render complete");
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
	        	// writeSerialPortDebug(boutRefNum, "mouseup");
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
	        	// writeSerialPortDebug(boutRefNum, "mousedown");
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
	        	// writeSerialPortDebug(boutRefNum, "key");
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
        		// writeSerialPortDebug(boutRefNum, "activate");
        	#endif
			DoActivate((WindowPtr) event->message, (event->modifiers & activeFlag) != 0);
			break;
		case updateEvt:
			
			#ifdef MAC_APP_DEBUGGING
        		// writeSerialPortDebug(boutRefNum, "update");
        	#endif
			DoUpdate((WindowPtr) event->message);
			break;
		/*	1.01 - It is not a bad idea to at least call DIBadMount in response
			to a diskEvt, so that the user can format a floppy. */
		case diskEvt:
			
    		#ifdef MAC_APP_DEBUGGING
        		// writeSerialPortDebug(boutRefNum, "disk");
        	#endif
			if ( HiWord(event->message) != noErr ) {
				SetPt(&aPoint, kDILeft, kDITop);
				err = DIBadMount(aPoint, event->message);
			}
			break;

		case osEvt:
			
    		#ifdef MAC_APP_DEBUGGING
        		// writeSerialPortDebug(boutRefNum, "os");
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
			sendNewChat = 1;
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
                    // // OSErr res = writeSerialPortDebug(boutRefNum, "Hello World");
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
                    // // writeSerialPortDebug(boutRefNum, str2);               

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
                    // // writeSerialPortDebug(boutRefNum, str2);               

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
		( gMac.machineType < envMacII ) ) {		/* it's a 2048KE, Plus, or SE */
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