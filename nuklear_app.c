#include <Types.h>

char jsFunctionResponse[102400]; // Matches MAX_RECEIVE_SIZE

Boolean gotMouseEvent = false;
Boolean firstOrMouseMove = true;
int haveRun = 0;
int chatFriendlyNamesCounter = 0;
int ipAddressSet = 0;
int sendNewChat = 0;
char chatFriendlyNames[16][64];
char activeChat[64];
int activeMessageCounter = 0;
char activeChatMessages[15][2048]; // this should match to MAX_ROWS in index.js
char box_input_buffer[2048];
char ip_input_buffer[255];
char new_message_input_buffer[255];
short box_len;
short box_input_len;
short new_message_input_buffer_len;
static short ip_input_buffer_len; // TODO: setting a length here will make the default `http://...` work, but doesn't work right -- maybe due to perf work in nuklear
int forceRedraw = 2; // this is how many 'iterations' of the UI that we need to see every element for
int drawChatsOneMoreTime = 2; // this is how many 'iterations' of the UI that we need to see every element for
int coprocessorLoaded = 0;

void getMessagesFromjsFunctionResponse() {

	for (int i = 0; i < 15; i++) {

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

	callFunctionOnCoprocessor("sendMessage", output, jsFunctionResponse);

	getMessagesFromjsFunctionResponse();

	forceRedraw = 3;
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
			nk_layout_row_push(ctx, 320); // 40% wide

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
		        
				nk_layout_row_push(ctx, 305); // 40% wide
				// message label
	            // writeSerialPortDebug(boutRefNum, "create label!");
	            // writeSerialPortDebug(boutRefNum, activeChatMessages[i]);

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