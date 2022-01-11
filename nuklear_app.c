// TODO: 
// - test, bug fixes, write blog posts

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

// #define MESSAGES_FOR_MACINTOSH_DEBUGGING

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
char chatCountFunctionResponse[102400]; // Matches MAX_RECEIVE_SIZE
char tempChatCountFunctionResponse[102400]; // Matches MAX_RECEIVE_SIZE
char previousChatCountFunctionResponse[102400]; // Matches MAX_RECEIVE_SIZE
char new_message_input_buffer[255];
int activeMessageCounter = 0;
int chatFriendlyNamesCounter = 0;
int coprocessorLoaded = 0;
int forceRedraw = 2; // this is how many 'iterations' of the UI that we need to see every element for
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

    char *token = (char *)strtokm(jsFunctionResponse, "ENDLASTMESSAGE");

    // loop through the string to extract all other tokens
    while (token != NULL) {

        sprintf(activeChatMessages[activeMessageCounter], "%s", token); 
        token = (char *)strtokm(NULL, "ENDLASTMESSAGE");
        activeMessageCounter++;
    }

    return;
}

// function to send messages in chat
void sendMessage() {

    char output[2048];
    sprintf(output, "%s&&&%.*s", activeChat, box_input_len, box_input_buffer);

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

// set up function to get available chat (fill buttons on the left hand side)
// interval is set by the event loop in mac_main
void getChats() {

    writeSerialPortDebug(boutRefNum, "getChats!");

    callFunctionOnCoprocessor("getChats", "", jsFunctionResponse);

    char * token = (char *)strtokm(jsFunctionResponse, ",");
    // loop through the string to extract all other tokens
    while (token != NULL) {
        writeSerialPortDebug(boutRefNum, token);
        sprintf(chatFriendlyNames[chatFriendlyNamesCounter++], "%s", token); 
        token = (char *)strtokm(NULL, ",");
    }

    return;
}

void sendIPAddressToCoprocessor() {

    char output[2048];
    sprintf(output, "%.*s", ip_input_buffer_len, ip_input_buffer);

    writeSerialPortDebug(boutRefNum, output);
    callFunctionOnCoprocessor("setIPAddress", output, jsFunctionResponse);

    // now that the IP is set, we can get all of our chats
    getChats();

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

// from https://stackoverflow.com/a/4770992
Boolean prefix(const char *pre, const char *str) {
    return strncmp(pre, str, strlen(pre)) == 0;
}

void getChatCounts() {

    char output[62];
    sprintf(output, "");
    // writeSerialPortDebug(boutRefNum, output);

    callFunctionOnCoprocessor("getChatCounts", output, chatCountFunctionResponse);

    #ifdef MESSAGES_FOR_MACINTOSH_DEBUGGING
        writeSerialPortDebug(boutRefNum, "getChatCounts");
        writeSerialPortDebug(boutRefNum, chatCountFunctionResponse);
    #endif

    if (strcmp(chatCountFunctionResponse, previousChatCountFunctionResponse)) {

        #ifdef MESSAGES_FOR_MACINTOSH_DEBUGGING
            writeSerialPortDebug(boutRefNum, "update current chat count");
            writeSerialPortDebug(boutRefNum, chatCountFunctionResponse);
        #endif

        SysBeep(1);
        char *saveptr;
        strcpy(tempChatCountFunctionResponse, chatCountFunctionResponse);
        char *token = strtok_r(tempChatCountFunctionResponse, ",", &saveptr);

        // loop through the string to extract all other tokens
        while (token != NULL) {

            #ifdef MESSAGES_FOR_MACINTOSH_DEBUGGING 
                writeSerialPortDebug(boutRefNum, "update current chat count loop");
                writeSerialPortDebug(boutRefNum, token);
            #endif
            // should be in format NAME:::COUNT

            char *saveptr2;
            char *name = strtok_r(token, ":::", &saveptr2);
            char *countString = strtok_r(NULL, ":::", &saveptr2);
            short count = atoi(countString);

            #ifdef MESSAGES_FOR_MACINTOSH_DEBUGGING
                char x[255];
                sprintf(x, "name: %s, countString: %s, count: %d", name, countString, count);
                writeSerialPortDebug(boutRefNum, x);
            #endif

            for (int i = 0; i < chatFriendlyNamesCounter; i++) {

                if (strstr(chatFriendlyNames[i], " new) ") != NULL) {

                    char chatName[64];
                    sprintf(chatName, "%s", chatFriendlyNames[i]);

                    // we are throwing out the first token
                    strtok_r(chatName, " new) ", &saveptr2);

                    char *tempChatFriendlyName = strtok_r(NULL, " new) ", &saveptr2);

                    if (prefix(tempChatFriendlyName, name)) {

                        #ifdef MESSAGES_FOR_MACINTOSH_DEBUGGING
                            writeSerialPortDebug(boutRefNum, "match1");
                            writeSerialPortDebug(boutRefNum, name);
                        #endif

                        if (count == 0) {

                            sprintf(chatFriendlyNames[i], "%s", name);
                        } else {

                            sprintf(chatFriendlyNames[i], "(%d new) %s", count, name);
                        }
                        break;
                    }
                } else {

                    if (prefix(chatFriendlyNames[i], name)) {

                        #ifdef MESSAGES_FOR_MACINTOSH_DEBUGGING
                            writeSerialPortDebug(boutRefNum, "match2");
                            writeSerialPortDebug(boutRefNum, name);
                        #endif

                        if (count == 0) {

                            sprintf(chatFriendlyNames[i], "%s", name);
                        } else {

                            sprintf(chatFriendlyNames[i], "(%d new) %s", count, name);
                        }
                        break;
                    }
                }
            }

            token = strtok_r(NULL, ",", &saveptr);
        }

        strcpy(previousChatCountFunctionResponse, chatCountFunctionResponse);
        forceRedraw = 3;
    } else {

        writeSerialPortDebug(boutRefNum, "no need to update current chat count");
    }

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
        forceRedraw = 3;
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

        if (nk_begin_titled(ctx, "Enter New Message Recipient", "Enter New Message Recipient",  nk_rect(50, WINDOW_HEIGHT / 4, WINDOW_WIDTH - 100, 140), NK_WINDOW_TITLE|NK_WINDOW_BORDER)) {

            nk_layout_row_begin(ctx, NK_STATIC, 30, 1);
            {
                nk_layout_row_push(ctx, WINDOW_WIDTH - 120);
                nk_label(ctx, "enter contact name as it would appear", NK_TEXT_ALIGN_LEFT);
                nk_layout_row_push(ctx, WINDOW_WIDTH - 120);
                nk_label(ctx, "on your iPhone, iPad, modern Mac, etc", NK_TEXT_ALIGN_LEFT);
            }
            nk_layout_row_end(ctx);

            nk_layout_row_begin(ctx, NK_STATIC, 30, 2);
            {
                nk_layout_row_push(ctx, WINDOW_WIDTH / 2);
                nk_edit_string(ctx, NK_EDIT_SIMPLE, new_message_input_buffer, &new_message_input_buffer_len, 2048, nk_filter_default);
                nk_layout_row_push(ctx, 100);

                if (nk_button_label(ctx, "open chat")) {
                
                    sendNewChat = 0;
                    forceRedraw = 2;

                    sprintf(activeChat, "%.*s", new_message_input_buffer_len, new_message_input_buffer);

                    for (int i = 0; i < MAX_CHAT_MESSAGES; i++) {

                        memset(&activeChatMessages[i], '\0', 2048);
                    }

                    getMessages(activeChat, 0);
                }
            }
            nk_layout_row_end(ctx);

            nk_end(ctx);
        }

        return;
    }

    chatWindowCollision = checkCollision(chats_window_size);

    if ((chatWindowCollision || forceRedraw) && nk_begin(ctx, "Chats", chats_window_size, NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR)) {

        if (chatWindowCollision) {

            forceRedraw = 2;
        }

        nk_layout_row_begin(ctx, NK_STATIC, 25, 1);
        {
            for (int i = 0; i < chatFriendlyNamesCounter; i++) {

                // only display the first 8 chats, create new chat if you need someone not in your list
                if (i > 9) {

                    continue;
                }

                nk_layout_row_push(ctx, 169);

                if (nk_button_label(ctx, chatFriendlyNames[i])) {

                    if (strstr(chatFriendlyNames[i], " new) ") != NULL) {

                        char chatName[96];
                        sprintf(chatName, "%s", chatFriendlyNames[i]);

                        #ifdef MESSAGES_FOR_MACINTOSH_DEBUGGING
                            writeSerialPortDebug(boutRefNum, "clicked1 chatName");
                            writeSerialPortDebug(boutRefNum, chatName);
                        #endif

                        // we are throwing out the first token
                        char *name = (char *)strtokm(chatName, " new) ");

                        #ifdef MESSAGES_FOR_MACINTOSH_DEBUGGING
                            writeSerialPortDebug(boutRefNum, "clicked1 portion1 of string, will toss");
                            writeSerialPortDebug(boutRefNum, name);
                        #endif

                        name = (char *)strtokm(NULL, " new) ");

                        #ifdef MESSAGES_FOR_MACINTOSH_DEBUGGING
                            writeSerialPortDebug(boutRefNum, "clicked1 have name to assign to activeChat");
                            writeSerialPortDebug(boutRefNum, name);
                        #endif

                        sprintf(activeChat, "%s", name);
                        sprintf(chatFriendlyNames[i], "%s", name);
                    } else {

                        #ifdef MESSAGES_FOR_MACINTOSH_DEBUGGING
                            writeSerialPortDebug(boutRefNum, "clicked2 chatName");
                            writeSerialPortDebug(boutRefNum, chatFriendlyNames[i]);
                        #endif

                        sprintf(activeChat, "%s", chatFriendlyNames[i]);
                    }

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

    if ((forceRedraw) && nk_begin_titled(ctx, "Message", activeChat, messages_window_size, NK_WINDOW_BORDER|NK_WINDOW_TITLE|NK_WINDOW_NO_SCROLLBAR)) {

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
    memset(&chatCountFunctionResponse, '\0', 102400);
    memset(&previousChatCountFunctionResponse, '\0', 102400);

    graphql_input_window_size = nk_rect(WINDOW_WIDTH / 2 - 118, 80, 234, 100);
    chats_window_size = nk_rect(0, 0, 180, WINDOW_HEIGHT);
    messages_window_size = nk_rect(180, 0, 330, WINDOW_HEIGHT - 36);
    message_input_window_size = nk_rect(180, WINDOW_HEIGHT - 36, 330, 36);

    ctx = nk_quickdraw_init(WINDOW_WIDTH, WINDOW_HEIGHT);
    refreshNuklearApp(false);

    sprintf(ip_input_buffer, "http://"); // doesn't work due to bug, see variable definition

    return ctx;
}