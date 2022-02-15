#include <Serial.h>
#include <Devices.h>
#include <stdio.h>
#include <string.h>

OSErr setupDebugSerialPort(short refNum);
OSErr writeSerialPortDebug(short refNum, const char* str);