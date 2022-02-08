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
#include <stdlib.h>
#include <string.h>
#include "mac_main.h"

// #define MAC_APP_DEBUGGING
//#define PROFILING 1
#ifdef PROFILING

OSErr writeSerialPortProfile(const char* str)
{

    #define PRINTER_PORT_OUT "\p.BOut"

    OSErr err;
    short serialPort = 0;
    err = OpenDriver(PRINTER_PORT_OUT, &serialPort);    
    if (err < 0) return err;    
    
    CntrlParam cb2;
    cb2.ioCRefNum = serialPort;
    cb2.csCode = 8;
    cb2.csParam[0] = stop10 | noParity | data8 | baud9600;
    err = PBControl ((ParmBlkPtr) & cb2, 0);    
    if (err < 0) return err; 
            
    IOParam pb2;
    pb2.ioRefNum = serialPort;
    
    char str2[1024];
    sprintf(str2, "%s\n", str);
    pb2.ioBuffer = (Ptr) str2;
    pb2.ioReqCount = strlen(str2);
    
    err = PBWrite((ParmBlkPtr)& pb2, 0);          
    if (err < 0) return err;
    
    // hangs on Mac512K (write hasn't finished due to slow Speed when we wants to close driver
    // err = CloseDriver(serialPort);
    
    return err;
}

void PROFILE_START(char *name) {

    char profileMessage[255];
    sprintf(profileMessage, "PROFILE_START %s", name);

    writeSerialPortProfile(profileMessage);

    return;
}

void PROFILE_END(char *name) {

    char profileMessage[255];
    sprintf(profileMessage, "PROFILE_END %s", name);

    writeSerialPortProfile(profileMessage);

    return;
}

void PROFILE_COMPLETE() {

    writeSerialPortProfile("PROFILE_COMPLETE");

    return;
}

#endif

#include "SerialHelper.h"
#include "Quickdraw.h"
#include "output_js.h"
#include "coprocessorjs.h"

#include "nuklear_app.c"

/* GMac is used to hold the result of a SysEnvirons call. This makes
   it convenient for any routine to check the environment. */
SysEnvRec	gMac;				/* set up by Initialize */

/* GHasWaitNextEvent is set at startup, and tells whether the WaitNextEvent
   trap is available. If it is false, we know that we must call GetNextEvent. */
Boolean		gHasWaitNextEvent;	/* set up by Initialize */

/* GInBackground is maintained by our osEvent handling routines. Any part of
   the program can check it to find out if it is currently in the background. */
Boolean		gInBackground;		/* maintained by Initialize and DoEvent */

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

// this function, EventLoop, and DoEvent contain all of the business logic necessary
// for our application to run
int main()
{
    Initialize();					/* initialize the program */
    UnloadSeg((Ptr) Initialize);	/* note that Initialize must not be in Main! */

    // run our nuklear app one time to render the window telling us to be patient for the coprocessor
    // app to load up 
    struct nk_context *ctx = initializeNuklearApp();

    SysBeep(1);

    setupCoprocessor("nuklear", "modem"); // could also be "printer", modem is 0 in PCE settings - printer would be 1
    // we could build a nuklear window for selection

    char programResult[MAX_RECEIVE_SIZE];
    sendProgramToCoprocessor((char *)OUTPUT_JS, programResult);
    writeSerialPortDebug(boutRefNum, "coprocessor loaded");

    coprocessorLoaded = 1;

    EventLoop(ctx);

    return 0;
}

Boolean gotKeyboardEvent = false;
int gotKeyboardEventTime = 0;

void EventLoop(struct nk_context *ctx)
{
    Boolean	gotEvent;
    EventRecord	event;
    Point mouse;

    int lastMouseHPos = 0;
    int lastMouseVPos = 0;
    int lastUpdatedTickCountMessagesInChat = 0;
    int lastUpdatedTickCountChatCounts = 0;
    Boolean gotNewMessages = false;

    do {

        // Don't do this, it won't yield anything useful
        // and will make your app very slow:
        // #ifdef PROFILING
        //     PROFILE_START("eventloop");
        // #endif

        if (gotKeyboardEvent && TickCount() > gotKeyboardEventTime + 20) {

            gotKeyboardEvent = false;
            ShowCursor();
        }

        gotNewMessages = false;

        // check for new stuff every 10 sec?
        // note! this is used by some of the functionality in our nuklear_app to trigger
        // new chat lookups
        if (TickCount() - lastUpdatedTickCountMessagesInChat > 600) {

            gotNewMessages = true;

            // writeSerialPortDebug(boutRefNum, "update by tick count");
            lastUpdatedTickCountMessagesInChat = TickCount();

            if (strcmp(activeChat, "no active chat")) {

                // writeSerialPortDebug(boutRefNum, "check chat");
                getHasNewMessagesInChat(activeChat);
            }
        } 

        // this should be out of sync with the counter above it so that we dont end up making
        // two coprocessor calls on one event loop iteration
        if (!gotNewMessages && TickCount() - lastUpdatedTickCountChatCounts > 300) {

            // writeSerialPortDebug(boutRefNum, "update by tick count");
            lastUpdatedTickCountChatCounts = TickCount();

            if (chatFriendlyNamesCounter > 0) {

                // writeSerialPortDebug(boutRefNum, "check chat counts");
                getChatCounts();
            }
        }

        Boolean beganInput = false;

        GetGlobalMouse(&mouse);

        // as far as i can tell, there is no way to event on mouse movement with mac libraries,
        // so we are just going to track on our own, and create our own events.
        // this seems kind of a bummer to not pass this to event handling code, but to make
        // it work we would need to create a dummy event, etc, so we will just directly 
        // call the nk_input_motion command
        if (lastMouseHPos != mouse.h || lastMouseVPos != mouse.v) {

            // if the mouse is in motion, try to capture all motion before moving on to rendering
            while (lastMouseHPos != mouse.h || lastMouseVPos != mouse.v) {

                #ifdef MAC_APP_DEBUGGING

                    writeSerialPortDebug(boutRefNum, "nk_input_motion!");
                #endif

                Point tempPoint;
                SetPt(&tempPoint, mouse.h, mouse.v);
                GlobalToLocal(&tempPoint);

                if (!beganInput) {

                    nk_input_begin(ctx);
                    beganInput = true;
                }

                nk_input_motion(ctx, tempPoint.h, tempPoint.v);

                firstOrMouseMove = true;
                mouse_x = tempPoint.h;
                mouse_y = tempPoint.v;

                lastUpdatedTickCountChatCounts = TickCount();
                lastUpdatedTickCountMessagesInChat = TickCount();
                lastMouseHPos = mouse.h;
                lastMouseVPos = mouse.v;
                GetGlobalMouse(&mouse);
            }
        } else {

            gotEvent = GetNextEvent(everyEvent, &event);
            gotMouseEvent = false;

            // drain all events before rendering -- really this only applies to keyboard events and single mouse clicks now
            while (gotEvent) {

                lastUpdatedTickCountChatCounts = TickCount();
                lastUpdatedTickCountMessagesInChat = TickCount();

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

                if (!gotMouseEvent) {

                    gotEvent = GetNextEvent(everyEvent, &event);
                } else {

                    gotEvent = false;
                }
            }
        }

        lastMouseHPos = mouse.h;
        lastMouseVPos = mouse.v;

        SystemTask();

        // only re-render if there is an event, prevents screen flickering, speeds up app
        if (beganInput || firstOrMouseMove || forceRedraw) {

            #ifdef PROFILING
                PROFILE_START("nk_input_end");
            #endif

            nk_input_end(ctx);

            #ifdef PROFILING
                PROFILE_END("nk_input_end");
                PROFILE_START("nuklearApp");
            #endif

            firstOrMouseMove = false;

            #ifdef MAC_APP_DEBUGGING

                writeSerialPortDebug(boutRefNum, "nuklearApp");
            #endif

            nuklearApp(ctx);

            #ifdef PROFILING
                PROFILE_END("nuklearApp");
                PROFILE_START("nk_quickdraw_render");
            #endif

            #ifdef MAC_APP_DEBUGGING

                writeSerialPortDebug(boutRefNum, "nk_quickdraw_render");
                char x[255];
                sprintf(x, "why? beganInput: %d, firstOrMouseMove: %d, forceRedraw: %d", beganInput, firstOrMouseMove, forceRedraw);
                writeSerialPortDebug(boutRefNum, x);
            #endif

            nk_quickdraw_render(FrontWindow(), ctx);

            #ifdef PROFILING
                PROFILE_END("nk_quickdraw_render");
                PROFILE_START("nk_clear");
            #endif

            nk_clear(ctx);

            #ifdef PROFILING
                PROFILE_END("nk_clear");
            #endif

        }

        #ifdef MAC_APP_DEBUGGING

            writeSerialPortDebug(boutRefNum, "nk_input_render complete");
        #endif

        // again, don't do this
        // #ifdef PROFILING
        //     PROFILE_END("eventloop");
        // #endif
    } while ( true );	/* loop forever; we quit via ExitToShell */
}


/* Do the right thing for an event. Determine what kind of event it is, and call
 the appropriate routines. */

void DoEvent(EventRecord *event, struct nk_context *ctx) {

    short part;
    WindowPtr window;
    Boolean	hit;
    char key;
    Point aPoint;

    switch ( event->what ) {

        case mouseUp:

            gotMouseEvent = true;

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

            gotMouseEvent = true;

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

            if (!gotKeyboardEvent) {

                HideCursor();
                gotKeyboardEvent = true;
            }

            gotKeyboardEventTime = TickCount();

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

/*	Enable and disable menus based on the current state.
    The user can only select enabled menu items. We set up all the menu items
    before calling MenuSelect or MenuKey, since these are the only times that
    a menu item can be selected. Note that MenuSelect is also the only time
    the user will see menu items. This approach to deciding what enable/
    disable state a menu item has the advantage of concentrating all
    the decision-making in one routine, as opposed to being spread throughout
    the application. Other application designs may take a different approach
    that is just as valid. */

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
    } else {						/* but we dont use it */
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

void DoMenuCommand(menuResult)
    long		menuResult;
{
    short		menuID;				/* the resource ID of the selected menu */
    short		menuItem;			/* the item number of the selected menu */
    //short		itemHit;
    // Str255		daName;
    // short		daRefNum;
    // Boolean		handledByDA;

    menuID = HiWord(menuResult);	/* use macros for efficiency to... */
    menuItem = LoWord(menuResult);	/* get menu item number and menu number */
    switch ( menuID ) {
        case mApple:
            switch ( menuItem ) {
                case iAbout:		/* bring up alert for About */
                    Alert(rAboutAlert, nil);
                    break;
                default:
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
            // handledByDA = SystemEdit(menuItem-1);	/* since we donÕt do any Editing */
            break;
        case mLight:
            // note this was co-opted to send new chats instead of the demo functionality. do the
            // same thing for other menu items as necessary
            switch (menuItem) {
                case 2:
                    getChats();
                    break;
                default:
                    sendNewChat = 1;
                    break;
            }

            // char x[255];
            // sprintf(x, "MENU %d", menuItem);
            // writeSerialPortDebug(boutRefNum, x);
            break;

        case mHelp:
            switch ( menuItem ) {
                case iQuickHelp:
                    // itemHit = Alert(rAboutAlert, nil);                        
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

void Terminate()
{
    WindowPtr	aWindow;
    Boolean		closed;

    #ifdef PROFILING
        PROFILE_COMPLETE();
    #endif
    
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

void Initialize()
{
    Handle		menuBar;
    WindowPtr	window;
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

Boolean IsDAWindow(window)
    WindowPtr	window;
{
    if ( window == nil ) {
        return false;
    }
        /* DA windows have negative windowKinds */
    return ((WindowPeek) window)->windowKind < 0;
} /*IsDAWindow*/

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

void AlertUser() {
    // short itemHit;

    SetCursor(&qd.arrow);
    // itemHit = Alert(rUserAlert, nil);
    ExitToShell();
} /* AlertUser */