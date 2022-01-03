// TODO: 
// - IN PROGRESS  new message window -- needs to blank out messages, then needs fixes on new mac end
// - chat during the day for a few minutes and figure out small issues
// - start writing blog posts
// - get new messages in other chats and display some sort of alert
// - need timeout on serial messages in case the computer at the other end dies (prevent hard reset)
// - delete doesnt work right (leaves characters at end of string)

#define WINDOW_WIDTH 510
#define WINDOW_HEIGHT 302

#define NK_ZERO_COMMAND_MEMORY
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
// #define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_IMPLEMENTATION
#define NK_QUICKDRAW_IMPLEMENTATION
// #define NK_BUTTON_TRIGGER_ON_RELEASE
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

#include <Types.h>
#include "nuklear.h"
#include "nuklear_quickdraw.h"
#include "coprocessorjs.h"

#define MAX_CHAT_MESSAGES 16

Boolean firstOrMouseMove = true;
Boolean gotMouseEvent = false;
char activeChat[64];
char activeChatMessages[MAX_CHAT_MESSAGES][2048]; // this should match to MAX_ROWS in index.js
char box_input_buffer[2048];
char chatFriendlyNames[16][64];
char ip_input_buffer[255];
char jsFunctionResponse[102400]; // Matches MAX_RECEIVE_SIZE
char new_message_input_buffer[255];
int activeMessageCounter = 0;
int chatFriendlyNamesCounter = 0;
int coprocessorLoaded = 0;
int drawChatsOneMoreTime = 2; // this is how many 'iterations' of the UI that we need to see every element for
int forceRedraw = 2; // this is how many 'iterations' of the UI that we need to see every element for
int haveRun = 0;
int ipAddressSet = 0;
int mouse_x;
int mouse_y;
int sendNewChat = 0;
short box_input_len;
short box_len;
short new_message_input_buffer_len;
static short ip_input_buffer_len; // TODO: setting a length here will make the default `http://...` work, but doesn't work right -- maybe due to perf work in nuklear
struct nk_rect chats_window_size;
struct nk_rect graphql_input_window_size;
struct nk_rect message_input_window_size;
struct nk_rect messages_window_size;
struct nk_context *ctx;

void refreshNuklearApp(Boolean blankInput);

void getMessagesFromjsFunctionResponse() {

    for (int i = 0; i < MAX_CHAT_MESSAGES; i++) {

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

    memset(&box_input_buffer, '\0', 2048);
    sprintf(box_input_buffer, "");
    box_input_len = 0;
    refreshNuklearApp(1);

    callFunctionOnCoprocessor("sendMessage", output, jsFunctionResponse);

    getMessagesFromjsFunctionResponse();

    forceRedraw = 2;
    firstOrMouseMove = true;

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

    forceRedraw = 3;

    return;
}

void getHasNewMessagesInChat(char *thread) {

    char output[62];
    sprintf(output, "%s", thread);
    // writeSerialPortDebug(boutRefNum, output);

    callFunctionOnCoprocessor("hasNewMessagesInChat", output, jsFunctionResponse);
    // writeSerialPortDebug(boutRefNum, jsFunctionResponse);

    if (!strcmp(jsFunctionResponse, "true")) {

        // writeSerialPortDebug(boutRefNum, "update current chat");
        SysBeep(1);
        getMessages(thread, 0);
        // force redraw
        firstOrMouseMove = true;
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

Boolean chatWindowCollision;
Boolean messageWindowCollision;

Boolean checkCollision(struct nk_rect window) {
    // writeSerialPortDebug(boutRefNum, "checkCollision!");

    // Boolean testout = (window.x < mouse_x &&
    //    window.x + window.w > mouse_x &&
    //    window.y < mouse_y &&
    //    window.y + window.h > mouse_y);
    // char str[255];
    // sprintf(str, "what %d", testout);
    //     	writeSerialPortDebug(boutRefNum, str);


    // if truthy return, mouse is over window!
    return (window.x < mouse_x &&
       window.x + window.w > mouse_x &&
       window.y < mouse_y &&
       window.y + window.h > mouse_y);
}

// UI setup and event handling goes here
static void nuklearApp(struct nk_context *ctx) {

    // prompt the user for the graphql instance
    if (!coprocessorLoaded) {

        if (nk_begin_titled(ctx, "Loading coprocessor services", "Loading coprocessor services", graphql_input_window_size, NK_WINDOW_TITLE|NK_WINDOW_BORDER)) {

            nk_layout_row_begin(ctx, NK_STATIC, 20, 1);
            {
                nk_layout_row_push(ctx, 200);
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
                nk_layout_row_push(ctx, 200);
                nk_label_wrap(ctx, "ex: http://127.0.0.1");
            }
            nk_layout_row_end(ctx);

            nk_layout_row_begin(ctx, NK_STATIC, 30, 2);
            {

                nk_layout_row_push(ctx, WINDOW_WIDTH / 2 - 100);
                nk_edit_string(ctx, NK_EDIT_SIMPLE, ip_input_buffer, &ip_input_buffer_len, 255, nk_filter_default);
                nk_layout_row_push(ctx, 55);

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

                nk_layout_row_push(ctx, WINDOW_WIDTH / 2 - 110);
                nk_edit_string(ctx, NK_EDIT_SIMPLE, new_message_input_buffer, &new_message_input_buffer_len, 2048, nk_filter_default);
                nk_layout_row_push(ctx, 80);

                if (nk_button_label(ctx, "open chat")) {
                
                    sendNewChat = 0;
                    forceRedraw = 2;

                    sprintf(activeChat, new_message_input_buffer);
                }
            }
            nk_layout_row_end(ctx);

            nk_end(ctx);
        }

        return;
    }

    chatWindowCollision = checkCollision(chats_window_size);

    if ((chatWindowCollision || forceRedraw || drawChatsOneMoreTime) && nk_begin(ctx, "Chats", chats_window_size, NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR)) {

        if (chatWindowCollision) {

            drawChatsOneMoreTime = 2;
        }

        getChats();

        nk_layout_row_begin(ctx, NK_STATIC, 25, 1);
        {
            for (int i = 0; i < chatFriendlyNamesCounter; i++) {

                // only display the first 8 chats, create new chat if you need someone not in your list
                if (i > 9) {

                    continue;
                }

                nk_layout_row_push(ctx, 169);

                if (nk_button_label(ctx, chatFriendlyNames[i])) {

                    sprintf(activeChat, "%s", chatFriendlyNames[i]);
                    getMessages(activeChat, 0);
                }
            }
        }
        nk_layout_row_end(ctx);

        nk_end(ctx);
    }


    if (nk_begin(ctx, "Message Input", message_input_window_size, NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR)) {

        // bottom text input
        nk_layout_row_begin(ctx, NK_STATIC, 28, 1);
        {
            nk_layout_row_push(ctx, 320);

            short edit_return_value = nk_edit_string(ctx, NK_EDIT_FIELD|NK_EDIT_SIG_ENTER, box_input_buffer, &box_input_len, 2048, nk_filter_default);

            // this is the enter key, obviously
            if (edit_return_value == 17) {

                sendMessage();
            }
        }
        nk_layout_row_end(ctx);

        nk_end(ctx);
    }

    if ((forceRedraw || drawChatsOneMoreTime) && nk_begin_titled(ctx, "Message", activeChat, messages_window_size, NK_WINDOW_BORDER|NK_WINDOW_TITLE|NK_WINDOW_NO_SCROLLBAR)) {

        nk_layout_row_begin(ctx, NK_STATIC, 12, 1);
        {

            for (int i = 0; i < activeMessageCounter; i++) {

                nk_layout_row_push(ctx, 305);

                nk_label(ctx, activeChatMessages[i], NK_TEXT_ALIGN_LEFT);
            }
        }

        nk_layout_row_end(ctx);
        nk_end(ctx);
    }

    if (forceRedraw) {

        forceRedraw--;
    }

    if (drawChatsOneMoreTime) {

        drawChatsOneMoreTime--;
    }
}

void refreshNuklearApp(Boolean blankInput) {

    nk_input_begin(ctx);

    if (blankInput) {

        nk_input_key(ctx, NK_KEY_DEL, 1);
        nk_input_key(ctx, NK_KEY_DEL, 0);
    }

    nk_input_end(ctx);
    nuklearApp(ctx);
    nk_quickdraw_render(FrontWindow(), ctx);
    nk_clear(ctx);
}

struct nk_context* initializeNuklearApp() {

    sprintf(activeChat, "no active chat");

    graphql_input_window_size = nk_rect(WINDOW_WIDTH / 2 - 118, 80, 234, 100);
    chats_window_size = nk_rect(0, 0, 180, WINDOW_HEIGHT);
    messages_window_size = nk_rect(180, 0, 330, WINDOW_HEIGHT - 36);
    message_input_window_size = nk_rect(180, WINDOW_HEIGHT - 36, 330, 36);

    ctx = nk_quickdraw_init(WINDOW_WIDTH, WINDOW_HEIGHT);
    refreshNuklearApp(false);

    sprintf(ip_input_buffer, "http://"); // doesn't work due to bug, see variable definition

    return ctx;
}