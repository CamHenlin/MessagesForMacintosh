#include <stdio.h>
#include <Serial.h>
#include <math.h>
#include <Devices.h>
#include "string.h"
#include <stdbool.h>
#include <time.h>
#include "SerialHelper.h"
#include "coprocessorjs.h"

IOParam outgoingSerialPortReference;
IOParam incomingSerialPortReference;
const bool PRINT_ERRORS = false;
const bool DEBUGGING = false;
const int RECEIVE_WINDOW_SIZE = 102400; // receive in up to 100kb chunks?
const int MAX_RECEIVE_SIZE = RECEIVE_WINDOW_SIZE; // not sure if these ever need to be different
char GlobalSerialInputBuffer[102400]; // make this match MAX_RECEIVE_SIZE

char application_id[255];
int call_counter = 0;

// from: https://stackoverflow.com/questions/29847915/implementing-strtok-whose-delimiter-has-more-than-one-character
// basically multichar delimter strtok
char *strtokm(char *str, const char *delim)
{
    static char *tok;
    static char *next;
    char *m;

    if (delim == NULL) return NULL;

    tok = (str) ? str : next;
    if (tok == NULL) return NULL;

    m = strstr(tok, delim);

    if (m) {
        next = m + strlen(delim);
        *m = '\0';
    } else {
        next = NULL;
    }

    return tok;
}

/*
// http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/Devices/Devices-320.html
Read more: http://stason.org/TULARC/os-macintosh/programming/7-1-How-do-I-get-at-the-serial-ports-Communications-and-N.html#ixzz4cIxU3Tob this one is only useful for enumerating ports
// https://developer.apple.com/library/archive/documentation/mac/pdf/Devices/Serial_Driver.pdf
Serial implementation:

https://opensource.apple.com/source/gdb/gdb-186.1/src/gdb/ser-mac.c?txt another example of a serial library
http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/Devices/Devices-320.html
*/

// notes from above article:
// You can use OpenDriver, SetReset, SetHShake, SetSetBuf, SerGetBuf and
// the other Serial Manager functions on these drivers.

// To write to the
// serial port, use FSWrite for synchronous writes that wait until all is
// written, or PBWrite asynchronously for queuing up data that is supposed
// to go out but you don't want to wait for it.

// At least once each time
// through your event loop, you should call SerGetBuf on the in driver
// reference number you got from OpenDriver, and call FSRead for that many
// bytes - neither more nor less.

// TODO: handle all OSErr - they are all unhandled at the moment
void setupPBControlForSerialPort(short serialPortShort) {

    CntrlParam cb;
    cb.ioCRefNum = serialPortShort; // TODO: this is always 0 - does it matter? should we hard code 0 here? research
    cb.csCode = 8; // TODO: need to look up and document what csCode = 8 means
    cb.csParam[0] = stop10 | noParity | data8 | baud28800; // TODO: can we achieve higher than 9600 baud? - should be able to achieve at least 19.2k on a 68k machine
    OSErr err = PBControl ((ParmBlkPtr) & cb, 0); // PBControl definition: http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/Networking/Networking-296.html

    if (PRINT_ERRORS) {

        char errMessage[100];
        sprintf(errMessage, "err:%d\n", err);
        printf(errMessage);
    }

    if (err < 0) {

        return;
    }
}



void setupSerialPort(const char *name) {
#define MODEM_PORT_OUT   "\p.AOut"
#define MODEM_PORT_IN    "\p.AIn"
#define PRINTER_PORT_OUT "\p.BOut"
#define PRINTER_PORT_IN  "\p.BIn"

    const char* serialPortOutputName = "";
    const char* serialPortInputName = "";

    if (strcmp (name, "modem") == 0) {

        serialPortOutputName = MODEM_PORT_OUT;
        serialPortInputName = MODEM_PORT_IN;
    } else if (strcmp (name, "printer") == 0) {

        serialPortOutputName = PRINTER_PORT_OUT;
        serialPortInputName = MODEM_PORT_IN;
    } else {

        return;
    }

    short serialPortOutput = 0; // TODO: why is this always 0? is this right?
    short serialPortInput = 0; // TODO: not realy sure what this should be - just incrementing from the last item here

    OSErr err = MacOpenDriver(serialPortOutputName, &serialPortOutput);
    
    if (PRINT_ERRORS) {

        char errMessage[100];
        sprintf(errMessage, "err:%d\n", err);
        printf(errMessage);
    }

    if (err < 0) {

        return;
    }

    err = MacOpenDriver(serialPortInputName, &serialPortInput); // result in 0 but still doesn't work

    if (PRINT_ERRORS) {

        char errMessage[100];
        sprintf(errMessage, "err:%d\n", err);
        printf(errMessage);
    }

    if (err < 0) {

        return;
    }

    // From https://developer.apple.com/library/archive/documentation/mac/pdf/Devices/Serial_Driver.pdf
    // Set baud rate and data format. Note that you only need to set the
    // output driver; the settings are reflected on the input side
    setupPBControlForSerialPort(serialPortOutput);

    outgoingSerialPortReference.ioRefNum = serialPortOutput;
    incomingSerialPortReference.ioRefNum = serialPortInput;
    
    // the next 2 commands set up the receive window size to whatever we want, in bytes.
    // as far as i can tell, this needs to be set before any data begins flowing, so it seemed
    // like a good call to make the buffer a global that gets instantiated at serial port setup
    incomingSerialPortReference.ioBuffer = (Ptr)GlobalSerialInputBuffer;
    SerSetBuf(incomingSerialPortReference.ioRefNum, incomingSerialPortReference.ioBuffer, RECEIVE_WINDOW_SIZE);
}

void wait(float timeInSeconds) {

    // from "Inside Macintosh: Macintosh Toolbox Essentials" pg 2-112 
    // You can use the TickCount function to get the current number of ticks (a tick is
    // approximately 1/60 of a second) since the system last started up.
    // FUNCTION TickCount: LongInt;

    // previous implementation, which might work on more modern platforms (which is why this is left as a comment), was:
    // note that this appeared to sometimes be off by as much as 1s on a Macintosh classic (using other normal C time functions to measure...)
    // time_t start;
    // time_t end;

    // time(&start);

    // do {

    //     time(&end);
    // } while (difftime(end, start) <= timeInSeconds);

    long start; 
    long end;
    long waitTicks = (long)timeInSeconds * 60;

    start = TickCount();

    do {

        end = TickCount();
    } while (end - start <= waitTicks);

    // char log[255];
    // sprintf(log, "start time was %ld end time was %ld split was %ld and wait ticks were %ld, input was %f\n ", start, end, end - start, waitTicks, timeInSeconds);
    // printf(log);
}

// void because this function re-assigns respo
void readSerialPort(char* output) {

    if (DEBUGGING) {

        printf("readSerialPort\n");
    }
    
    // make sure output variable is clear
    memset(&output[0], 0, MAX_RECEIVE_SIZE);

    bool done = false;
    char tempOutput[MAX_RECEIVE_SIZE];
    long int totalByteCount = 0;
    incomingSerialPortReference.ioReqCount = 0;

    while (!done) {

        long int byteCount = 0;
        long int lastByteCount = 0;
        bool doByteCountsMatch = false;
        short serGetBufStatus;

        // the byteCount != lastByteCount portion of the loop means that we want to wait 
        // for the byteCounts to match between SerGetBuf calls - this means that the buffer
        // is full and ready to be read
        while (!doByteCountsMatch || byteCount == 0) {

            if (DEBUGGING) {

                char debugMessage[100];
                sprintf(debugMessage, "receive loop: byteCount: %d, lastByteCount: %d\n", byteCount, lastByteCount);
                printf(debugMessage);
            }

            lastByteCount = (long int)byteCount;

            wait(0.01); // give the buffer a moment to fill

            serGetBufStatus = SerGetBuf(incomingSerialPortReference.ioRefNum, &byteCount);

            if (serGetBufStatus != 0 && PRINT_ERRORS) {
                
                printf("potential problem with serGetBufStatus:\n");
                char debugMessage[100];
                sprintf(debugMessage, "serGetBufStatus: %d\n", serGetBufStatus);
                printf(debugMessage);
            }

            if (byteCount == lastByteCount && byteCount != 0 && lastByteCount != 0) {

                if (DEBUGGING) {

                    char debugMessage[100];
                    sprintf(debugMessage, "receive loop setting last doByteCountsMatch to true: byteCount: %d, lastByteCount: %d\n", byteCount, lastByteCount);
                    printf(debugMessage);
                }

                doByteCountsMatch = true;
            }
        }

        if (DEBUGGING) {

            char debugMessage[100];
            sprintf(debugMessage, "receive loop complete: byteCount: %d, lastByteCount: %d\n", byteCount, lastByteCount);
            printf(debugMessage);
        }

        incomingSerialPortReference.ioReqCount = byteCount;

        OSErr err = PBRead((ParmBlkPtr)&incomingSerialPortReference, 0);

        if (PRINT_ERRORS) {

            char errMessage[100];
            sprintf(errMessage, "err:%d\n", err);
            printf(errMessage);
        }

        memcpy(tempOutput, GlobalSerialInputBuffer, byteCount);
        
        totalByteCount += byteCount;

        if (strstr(tempOutput, ";;@@&&") != NULL) {
            
            if (DEBUGGING) {

                printf("done building temp output\n");
                printf(tempOutput);

                char *debugOutput;
                char tempString[MAX_RECEIVE_SIZE];
                strncat(tempString, tempOutput, totalByteCount);
                sprintf(debugOutput, "\n'%d'\n", strlen(tempString));
                printf(debugOutput);
                printf("\ndone with output\n");
                printf("\n");
            }

            done = true;
        }
    }

    // attach the gathered up output from the buffer to the output variable
    strncat(output, tempOutput, totalByteCount);

    // once we are done reading the buffer entirely, we need to clear it. i'm not sure if this is the best way or not but seems to work
    memset(&GlobalSerialInputBuffer[0], 0, MAX_RECEIVE_SIZE);

    return;
}


OSErr writeSerialPort(const char* stringToWrite) {
    
    if (DEBUGGING) {
        
        printf("writeSerialPort\n");
    }

    outgoingSerialPortReference.ioBuffer = (Ptr) stringToWrite;
    outgoingSerialPortReference.ioReqCount = strlen(stringToWrite);
    
    if (DEBUGGING) {

        printf("attempting to write string to serial port\n");
        printf(stringToWrite);
        printf("\n");
    }

    // PBWrite Definition From Inside Macintosh Volume II-185:
    // PBWrite takes ioReqCount bytes from the buffer pointed to by ioBuffer and attempts to write them to the device driver having the reference number ioRefNum.
    // The drive number, if any, of the device to be written to is specified by ioVRefNum. After the write is completed, the position is returned in ioPosOffset and the number of bytes actually written is returned in ioActCount.
    OSErr err = PBWrite((ParmBlkPtr)& outgoingSerialPortReference, 0);
    
    if (PRINT_ERRORS) {

        char errMessage[100];
        sprintf(errMessage, "err:%d\n", err);
        printf(errMessage);
    }

    return err;
}

void setupCoprocessor(char *applicationId, const char *serialDeviceName) {
    
    strcpy(application_id, applicationId);

    setupSerialPort(serialDeviceName);

    return;
}

OSErr closeSerialPort() {

    OSErr err = MacCloseDriver(outgoingSerialPortReference.ioRefNum);
    
    if (PRINT_ERRORS) {

        char errMessage[100];
        sprintf(errMessage, "err:%d\n", err);
        printf(errMessage);
    }

    return err;
}

// return time is char but this is only for error messages - final param is output variable that will be re-assigned within this function
char* _getReturnValueFromResponse(char* response, char* application_id, char* call_counter, char* operation, char* output) {
    
    if (DEBUGGING) {
        
        printf("_getReturnValueFromResponse\n");
        printf(response);
        printf("\n");
    }

    // get the first token in to memory
    char *token = strtokm(response, ";;;");
    
    // we need to track the token that we are on because the coprocessor.js responses are standardized
    // so the tokens at specific positions will map to specific items in the response
    int tokenCounter = 0;
    const int MAX_ATTEMPTS = 10;

    // loop through the string to extract all other tokens
    while (token != NULL) {

        if (tokenCounter > MAX_ATTEMPTS) {
            
            return "max attempts exceeded";
        }
        
        if (DEBUGGING) {

            char *debugOutput;
            sprintf(debugOutput, "inspect token %d: %s\n", tokenCounter, token);
            printf(debugOutput);
        }

        switch (tokenCounter) {

            case 0: // APPLICATION ID

                if (strcmp(token, application_id) != 0) {

                    return "application id mismatch"; // TODO figure out better error handling
                }

                break;
            case 1: // CALL COUNTER

                if (strcmp(token, call_counter) != 0) {

                    return "call counter mismatch"; // TODO figure out better error handling
                }
 
                break;
            case 2: // OPERATION

                if (strcmp(token, operation) != 0) {

                    return "operation mismatch"; // TODO figure out better error handling
                }

                break;
            case 3: // STATUS

                if (strcmp(token, "SUCCESS") != 0) {

                    return "operation failed"; // TODO figure out better error handling
                }

                break;
            case 4:
            
                if (DEBUGGING) {

                    printf("setting output to token:\n");
                    printf(token);
                    char *debugOutput;
                    sprintf(debugOutput, "\n'%d'\n", strlen(token));
                    printf(debugOutput);
                    printf("\ndone with output\n");
                }

                strncat(output, token, strlen(token) - 6); // the -6 here is to drop the ;;@@&& off the end of the response
                
                return NULL;

            default:

                break;
        }

        // get the next token. strtokm has some weird syntax
        token = strtokm(NULL, ";;;");
        tokenCounter++;
    }

    return NULL;
}

void writeToCoprocessor(char* operation, char* operand) {
    
    if (DEBUGGING) {
        
        printf("writeToCoprocessor\n");
    }
    
    const char* messageTemplate = "%s;;;%s;;;%s;;;%s;;@@&&"; // see: https://github.com/CamHenlin/coprocessor.js/blob/main/index.js#L25
    char call_id[32];

    // over-allocate by 1kb for the operand (which could be an entire nodejs app) + message template wrapper
    // and other associated info. wasting a tiny bit of memory here, could get more precise if memory becomes a problem.
    char messageToSend[strlen(operand) + 1024];

    sprintf(call_id, "%d", call_counter++);

    // application_id is globally defined for now, how will that work in a library?
    sprintf(messageToSend, messageTemplate, application_id, call_id, operation, operand);

    OSErr err = writeSerialPort(messageToSend);
    
    if (PRINT_ERRORS) {

        char errMessage[100];
        sprintf(errMessage, "writeToCoprocessor err:%d\n", err);
        printf(errMessage);
    }

    return;
}

// must be called after writeToCoprocessor and before other writeToCoprocessor
// operations because we depend on the location of call_counter
char* getReturnValueFromResponse(char *response, char *operation, char *output) {
    
    if (DEBUGGING) {
        
        printf("getReturnValueFromResponse\n");
    }

    char call_id[32];
    sprintf(call_id, "%d", call_counter - 1);

    char *err = _getReturnValueFromResponse(response, application_id, call_id, operation, output);
    
    if (err != NULL && PRINT_ERRORS) {
        
        printf("error getting return value from response:\n");
        printf(err);
        printf("\n");
    }
}

// TODO: this is a function we would want to expose in a library
// TODO: these should all bubble up and return legible errors
void sendProgramToCoprocessor(char* program, char *output) {

    if (DEBUGGING) {

        printf("sendProgramToCoprocessor\n");
    }

    SetCursor(*GetCursor(watchCursor));

    writeToCoprocessor("PROGRAM", program);

    char serialPortResponse[MAX_RECEIVE_SIZE];
    readSerialPort(serialPortResponse);

    getReturnValueFromResponse(serialPortResponse, "PROGRAM", output);

    SetCursor(&qd.arrow);
    
    return;
}

// TODO: this is a function we would want to expose in a library
void callFunctionOnCoprocessor(char* functionName, char* parameters, char* output) {
    
    if (DEBUGGING) {

        printf("callFunctionOnCoprocessor\n");
    }

    const char* functionTemplate = "%s&&&%s";

    // over-allocate by 1kb for the operand (which could be whatever a programmer sends to this function) + message template wrapper
    // and other associated info. wasting a tiny bit of memory here, could get more precise if memory becomes a problem.
    char functionCallMessage[strlen(parameters) + 1024];

    // delimeter for function paramters is &&& - user must do this on their own via sprintf call or other construct - this is easiest for us to deal with
    sprintf(functionCallMessage, functionTemplate, functionName, parameters);

    SetCursor(*GetCursor(watchCursor));
    //                         writeSerialPortDebug(boutRefNum, functionCallMessage);
    writeToCoprocessor("FUNCTION", functionCallMessage);

    char serialPortResponse[MAX_RECEIVE_SIZE];
    readSerialPort(serialPortResponse);
    // writeSerialPortDebug(boutRefNum, "========================Got response from serial port");
    // writeSerialPortDebug(boutRefNum, serialPortResponse);

    memset(output, '\0', RECEIVE_WINDOW_SIZE);
    getReturnValueFromResponse(serialPortResponse, "FUNCTION", output);

    SetCursor(&qd.arrow);
    
    return;
}

// TODO: this is a function we would want to expose in a library
void callEvalOnCoprocessor(char* toEval, char* output) {
    
    if (DEBUGGING) {
        
        printf("callEvalOnCoprocessor\n");
    }

    writeToCoprocessor("EVAL", toEval);

    char serialPortResponse[MAX_RECEIVE_SIZE];
    readSerialPort(serialPortResponse);
    getReturnValueFromResponse(serialPortResponse, "EVAL", output);

    return;
}