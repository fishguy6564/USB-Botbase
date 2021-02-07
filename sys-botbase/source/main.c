#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <switch.h>
#include "commands.h"
#include "args.h"
#include "util.h"
#include "pm_ams.h"
#include "time.h"

#define TITLE_ID 0x430000000000000B
#define HEAP_SIZE 0x000540000

typedef struct
{
    u64 size;
    void* data;
}USBResponse;

// we aren't an applet
u32 __nx_applet_type = AppletType_None;
TimeServiceType __nx_time_service_type = TimeServiceType_System;

// setup a fake heap (we don't need the heap anyway)
char fake_heap[HEAP_SIZE];

// we override libnx internals to do a minimal init
void __libnx_initheap(void)
{
    extern char *fake_heap_start;
    extern char *fake_heap_end;

    // setup newlib fake heap
    fake_heap_start = fake_heap;
    fake_heap_end = fake_heap + HEAP_SIZE;
}

void sendUsbResponse(USBResponse response)
{
    usbCommsWrite((void*)&response, 4);

    if (response.size > 0)
        usbCommsWrite(response.data, response.size);
}

void __appInit(void)
{
    Result rc;
    svcSleepThread(20000000000L);
    rc = smInitialize();
    if (R_FAILED(rc))
        fatalThrow(rc);
    rc = apmInitialize();
    if(R_FAILED(rc))
        fatalThrow(rc);
    if (hosversionGet() == 0)
    {
        rc = setsysInitialize();
        if (R_SUCCEEDED(rc))
        {
            SetSysFirmwareVersion fw;
            rc = setsysGetFirmwareVersion(&fw);
            if (R_SUCCEEDED(rc))
                hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro));
            setsysExit();
        }
    }
    rc = fsInitialize();
    if (R_FAILED(rc))
        fatalThrow(rc);
    rc = fsdevMountSdmc();
    if (R_FAILED(rc))
        fatalThrow(rc);
    rc = timeInitialize();
    if (R_FAILED(rc))
    {
        timeExit();
        __nx_time_service_type = TimeServiceType_User;
        rc = timeInitialize();
        if(R_FAILED(rc))
            fatalThrow(rc);
    }
    rc = pmdmntInitialize();
    if (R_FAILED(rc))
        fatalThrow(rc);
    rc = ldrDmntInitialize();
    if (R_FAILED(rc))
        fatalThrow(rc);
    rc = pminfoInitialize();
    if (R_FAILED(rc))
        fatalThrow(rc);
    rc = capsscInitialize();
    if (R_FAILED(rc))
        fatalThrow(rc);
    rc = usbCommsInitialize();
    if (R_FAILED(rc))
        fatalThrow(rc);
    rc = socketInitializeDefault();
    if (R_FAILED(rc))
        fatalThrow(rc);
}

void __appExit(void)
{
    fsdevUnmountAll();
    fsExit();
    smExit();
    audoutExit();
    timeExit();
    socketExit();
}

u64 mainLoopSleepTime = 50;
bool debugResultCodes = false;
bool echoCommands = false;

int argmain(int argc, char **argv)
{
    USBResponse response;
    if (argc == 0)
        return 0;

    //peek <address in hex or dec> <amount of bytes in hex or dec>
    if (!strcmp(argv[0], "peek"))
    {
        if(argc != 3)
            return 0;

        MetaData meta = getMetaData();
        u64 offset = parseStringToInt(argv[1]);
        u64 size = parseStringToInt(argv[2]);
        u8 data[size];

        peek(data, meta.heap_base + offset, size);
        response.size = size;
        response.data = &data[0];
        sendUsbResponse(response);
    }

    if (!strcmp(argv[0], "peekAbsolute"))
    {
        if(argc != 3)
            return 0;

        u64 offset = parseStringToInt(argv[1]);
        u64 size = parseStringToInt(argv[2]);
        u8 data[size];

        peek(data, offset, size);
        response.size = size;
        response.data = &data[0];
        sendUsbResponse(response);
    }

    if (!strcmp(argv[0], "peekMain"))
    {
        if(argc != 3)
            return 0;

        MetaData meta = getMetaData();
        u64 offset = parseStringToInt(argv[1]);
        u64 size = parseStringToInt(argv[2]);
        u8 data[size];

        peek(data, meta.main_nso_base + offset, size);
        response.size = size;
        response.data = &data[0];
        sendUsbResponse(response);
    }

    //poke <address in hex or dec> <amount of bytes in hex or dec> <data in hex or dec>
    if (!strcmp(argv[0], "poke"))
    {
        if(argc != 3)
            return 0;

        MetaData meta = getMetaData();

        u64 offset = parseStringToInt(argv[1]);
        u64 size = 0;
        u8* data = parseStringToByteBuffer(argv[2], &size);

        poke(meta.heap_base + offset, size, data);
        free(data);
    }

   if (!strcmp(argv[0], "pokeAbsolute"))
    {
        if(argc != 3)
            return 0;

        u64 offset = parseStringToInt(argv[1]);
        u64 size = 0;
        u8* data = parseStringToByteBuffer(argv[2], &size);

        poke(offset, size, data);
        free(data);
    }

    if (!strcmp(argv[0], "pokeMain"))
    {
        if(argc != 3)
            return 0;

        MetaData meta = getMetaData();

        u64 offset = parseStringToInt(argv[1]);
        u64 size = 0;
        u8* data = parseStringToByteBuffer(argv[2], &size);

        poke(meta.main_nso_base + offset, size, data);
        free(data);
    }

    //click <buttontype>
    if (!strcmp(argv[0], "click"))
    {
        if(argc != 2)
            return 0;
        HidNpadButton key = parseStringToButton(argv[1]);
        click(key);
    }

    //hold <buttontype>
    if (!strcmp(argv[0], "press"))
    {
        if(argc != 2)
            return 0;
        HidNpadButton key = parseStringToButton(argv[1]);
        press(key);
    }

    //release <buttontype>
    if (!strcmp(argv[0], "release"))
    {
        if(argc != 2)
            return 0;
        HidNpadButton key = parseStringToButton(argv[1]);
        release(key);
    }

    //setStick <left or right stick> <x value> <y value>
    if (!strcmp(argv[0], "setStick"))
    {
        if(argc != 4)
            return 0;

        int side = 0;
        if(!strcmp(argv[1], "LEFT"))
            side = 0;
        else if(!strcmp(argv[1], "RIGHT"))
            side = 1;
        else return 0;

        int dxVal = strtol(argv[2], NULL, 0);
        if(dxVal > JOYSTICK_MAX)
            dxVal = JOYSTICK_MAX; //0x7FFF
        if(dxVal < JOYSTICK_MIN)
            dxVal = JOYSTICK_MIN; //-0x8000

        int dyVal = strtol(argv[3], NULL, 0);
        if(dyVal > JOYSTICK_MAX)
            dyVal = JOYSTICK_MAX;
        if(dyVal < JOYSTICK_MIN)
            dyVal = JOYSTICK_MIN;

        setStickState(side, dxVal, dyVal);
    }

    //detachController
    if(!strcmp(argv[0], "detachController"))
    {
        Result rc = hiddbgDetachHdlsVirtualDevice(controllerHandle);
        if (R_FAILED(rc) && debugResultCodes)
            fatalThrow(rc);
        rc = hiddbgReleaseHdlsWorkBuffer();
        if (R_FAILED(rc) && debugResultCodes)
            fatalThrow(rc);
        hiddbgExit();
        bControllerIsInitialised = false;
    }

    //configure <mainLoopSleepTime or buttonClickSleepTime> <time in ms>
    if(!strcmp(argv[0], "configure"))
    {
        if(argc != 3)
            return 0;


        if(!strcmp(argv[1], "mainLoopSleepTime"))
        {
            u64 time = parseStringToInt(argv[2]);
            mainLoopSleepTime = time;
        }

        if(!strcmp(argv[1], "buttonClickSleepTime"))
        {
            u64 time = parseStringToInt(argv[2]);
            buttonClickSleepTime = time;
        }

        if(!strcmp(argv[1], "echoCommands"))
        {
            u64 shouldActivate = parseStringToInt(argv[2]);
            echoCommands = shouldActivate != 0;
        }

        if(!strcmp(argv[1], "printDebugResultCodes"))
        {
            u64 shouldActivate = parseStringToInt(argv[2]);
            debugResultCodes = shouldActivate != 0;
        }
    }

    if(!strcmp(argv[0], "getTitleID"))
    {
        MetaData meta = getMetaData();
        response.size = sizeof(meta.titleID);
        response.data = &meta.titleID;
        sendUsbResponse(response);
    }

    if(!strcmp(argv[0], "getSystemLanguage"))
    {
        //thanks zaksa
        setInitialize();
        u64 languageCode = 0;
        SetLanguage language = SetLanguage_ENUS;
        setGetSystemLanguage(&languageCode);
        setMakeLanguage(languageCode, &language);
        response.size = sizeof(language);
        response.data = &language;
        sendUsbResponse(response);
    }

    if(!strcmp(argv[0], "getMainNsoBase"))
    {
        MetaData meta = getMetaData();
        response.size = sizeof(meta.main_nso_base);
        response.data = &meta.main_nso_base;
        sendUsbResponse(response);
    }

    if(!strcmp(argv[0], "getHeapBase"))
    {
        MetaData meta = getMetaData();
        response.size = sizeof(meta.heap_base);
        response.data = &meta.heap_base;
        sendUsbResponse(response);
    }

    if(!strcmp(argv[0], "pixelPeek"))
    {
        u64 bufferSize = 0x7D000;
        char* buf = malloc(bufferSize);
        u64 outSize = 0;

        Result rc = capsscCaptureForDebug(buf, bufferSize, &outSize);

        if(R_FAILED(rc) && debugResultCodes) fatalThrow(rc);

        response.data = &buf[0];
        response.size = outSize;

        sendUsbResponse(response);

        free(buf);
    }

    if(!strcmp(argv[0], "daySkip"))
    {
        int resetTimeAfterSkips = parseStringToInt(argv[1]);
        int resetNTP = parseStringToInt(argv[2]);
        dateSkip(resetTimeAfterSkips, resetNTP);
    }

    if(!strcmp(argv[0], "resetTime"))
        resetTime();

    if(!strcmp(argv[0], "resetTimeNTP"))
        resetTimeNTP();

    return 0;
}

int main()
{
    USBResponse response;
    while (appletMainLoop())
    {
        int len;
        usbCommsRead(&len, sizeof(len));

		//Should use malloc
        char linebuf[len + 1];

        for(int i = 0; i < len+1; i++)
            linebuf[i] = 0;

        usbCommsRead(&linebuf, len);

		//Adds necessary escape characters for pasrser
        linebuf[len-1] = '\n';
        linebuf[len-2] = '\r';

        fflush(stdout);
        parseArgs(linebuf, &argmain);

        if(echoCommands){
            response.size = sizeof(linebuf);
            response.data = &linebuf;
            sendUsbResponse(response);
        }

        svcSleepThread(mainLoopSleepTime * 1e+6L);
    }

    return 0;
}
