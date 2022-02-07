#include "SerialHelper.h"
#include "stdio.h"

/*
Read more: http://stason.org/TULARC/os-macintosh/programming/7-1-How-do-I-get-at-the-serial-ports-Communications-and-N.html#ixzz4cIxU3Tob

Serial implementation: 

https://opensource.apple.com/source/gdb/gdb-186.1/src/gdb/ser-mac.c?txt 
http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/Devices/Devices-320.html
*/ 
OSErr writeSerialPortDebug(short refNum, const char* str)
{
    #ifdef PROFILING

    // we need to bail on profiling, because the profile watcher will be reading this serial port
    return;

    #endif
#define MODEM_PORT_OUT   "\p.AOut" 
#define PRINTER_PORT_OUT "\p.BOut" 
    // OSErr err;
    // return err;
    
    const unsigned char* nameStr = "\p";
    switch (refNum)
    {
        case aoutRefNum:
            nameStr = (const unsigned char*)MODEM_PORT_OUT;
            break;
        case boutRefNum:
            nameStr = (const unsigned char*)PRINTER_PORT_OUT;
            break;   
            
        default:
            return -1;        
    }
    
    OSErr err;
    short serialPort = 0;
    err = OpenDriver(nameStr, &serialPort);    
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