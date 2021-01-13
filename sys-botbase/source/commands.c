#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "commands.h"
#include "util.h"
#include "time.h"
#include "ntp.h"

Mutex actionLock;

//Controller:
bool bControllerIsInitialised = false;
time_t curTime = 0;
time_t origTime = 0;
int resetSkips = 0;
HiddbgHdlsHandle controllerHandle = {0};
HiddbgHdlsDeviceInfo controllerDevice = {0};
HiddbgHdlsState controllerState = {0};
Handle debughandle = 0;
u64 buttonClickSleepTime = 50;

void attach()
{
    u64 pid = 0;
    Result rc = pmdmntGetApplicationProcessId(&pid);

    if (R_FAILED(rc) && debugResultCodes)
        fatalThrow(rc);
    if (debughandle != 0)
        svcCloseHandle(debughandle);
    rc = svcDebugActiveProcess(&debughandle, pid);
    if (R_FAILED(rc) && debugResultCodes)
        fatalThrow(rc);
}

void detach(){
    if (debughandle != 0)
        svcCloseHandle(debughandle);
}

u64 getMainNsoBase(u64 pid){
    LoaderModuleInfo proc_modules[2];
    s32 numModules = 0;
    Result rc = ldrDmntGetProcessModuleInfo(pid, proc_modules, 2, &numModules);
    if (R_FAILED(rc) && debugResultCodes)
        fatalThrow(rc);

    LoaderModuleInfo *proc_module = 0;
    if(numModules == 2){
        proc_module = &proc_modules[1];
    }else{
        proc_module = &proc_modules[0];
    }
    return proc_module->base_address;
}

u64 getHeapBase(Handle handle){
    MemoryInfo meminfo;
    memset(&meminfo, 0, sizeof(MemoryInfo));
    u64 heap_base = 0;
    u64 lastaddr = 0;
    do
    {
        lastaddr = meminfo.addr;
        u32 pageinfo;
        svcQueryDebugProcessMemory(&meminfo, &pageinfo, handle, meminfo.addr + meminfo.size);
        if((meminfo.type & MemType_Heap) == MemType_Heap){
            heap_base = meminfo.addr;
            break;
        }
    } while (lastaddr < meminfo.addr + meminfo.size);

    return heap_base;
}

u64 getTitleId(u64 pid){
    u64 titleId = 0;
    Result rc = pminfoGetProgramId(&titleId, pid);
    if (R_FAILED(rc) && debugResultCodes)
        fatalThrow(rc);
    return titleId;
}

void getBuildID(MetaData* meta, u64 pid){
    LoaderModuleInfo proc_modules[2];
    s32 numModules = 0;
    Result rc = ldrDmntGetProcessModuleInfo(pid, proc_modules, 2, &numModules);
    if (R_FAILED(rc) && debugResultCodes)
        fatalThrow(rc);

    LoaderModuleInfo *proc_module = 0;
    if(numModules == 2){
        proc_module = &proc_modules[1];
    }else{
        proc_module = &proc_modules[0];
    }
    memcpy(meta->buildID, proc_module->build_id, 0x20);
}

MetaData getMetaData(){
    MetaData meta;
    attach();
    u64 pid = 0;
    Result rc = pmdmntGetApplicationProcessId(&pid);
    if (R_FAILED(rc) && debugResultCodes)
        fatalThrow(rc);

    meta.main_nso_base = getMainNsoBase(pid);
    meta.heap_base =  getHeapBase(debughandle);
    meta.titleID = getTitleId(pid);
    getBuildID(&meta, pid);

    detach();
    return meta;
}

void initController()
{
    if(bControllerIsInitialised) return;
    //taken from switchexamples github
    Result rc = hiddbgInitialize();
    if (R_FAILED(rc) && debugResultCodes)
        fatalThrow(rc);
    // Set the controller type to Pro-Controller, and set the npadInterfaceType.
    controllerDevice.deviceType = HidDeviceType_FullKey3;
    controllerDevice.npadInterfaceType = HidNpadInterfaceType_Bluetooth;
    // Set the controller colors. The grip colors are for Pro-Controller on [9.0.0+].
    controllerDevice.singleColorBody = RGBA8_MAXALPHA(255,255,255);
    controllerDevice.singleColorButtons = RGBA8_MAXALPHA(0,0,0);
    controllerDevice.colorLeftGrip = RGBA8_MAXALPHA(230,255,0);
    controllerDevice.colorRightGrip = RGBA8_MAXALPHA(0,40,20);

    // Setup example controller state.
    controllerState.battery_level = 4; // Set battery charge to full.
    controllerState.analog_stick_l.x = 0x0;
    controllerState.analog_stick_l.y = -0x0;
    controllerState.analog_stick_r.x = 0x0;
    controllerState.analog_stick_r.y = -0x0;
    rc = hiddbgAttachHdlsWorkBuffer();
    if (R_FAILED(rc) && debugResultCodes)
        fatalThrow(rc);
    rc = hiddbgAttachHdlsVirtualDevice(&controllerHandle, &controllerDevice);
    if (R_FAILED(rc) && debugResultCodes)
        fatalThrow(rc);
    bControllerIsInitialised = true;
}



void poke(u64 offset, u64 size, u8* val)
{
    attach();
    Result rc = svcWriteDebugProcessMemory(debughandle, val, offset, size);
    if (R_FAILED(rc) && debugResultCodes)
        fatalThrow(rc);
    detach();
}

void peek(u8 outData[], u64 offset, u64 size)
{
    attach();
    Result rc = svcReadDebugProcessMemory(outData, debughandle, offset, size);
    if(R_FAILED(rc) && debugResultCodes)
        fatalThrow(rc);
    detach();
}

void click(HidControllerKeys btn)
{
    initController();
    press(btn);
    svcSleepThread(buttonClickSleepTime * 1e+6L);
    release(btn);
}
void press(HidControllerKeys btn)
{
    initController();
    controllerState.buttons |= btn;
    hiddbgSetHdlsState(controllerHandle, &controllerState);
}

void release(HidControllerKeys btn)
{
    initController();
    controllerState.buttons &= ~btn;
    hiddbgSetHdlsState(controllerHandle, &controllerState);
}

void setStickState(int side, int dxVal, int dyVal)
{
    initController();
    if(side == 0)
    {
        controllerState.analog_stick_l.x = dxVal;
        controllerState.analog_stick_l.y = dyVal;
    }
    if(side == 1)
    {
        controllerState.analog_stick_r.x = dxVal;
        controllerState.analog_stick_r.y = dyVal;
    }
    hiddbgSetHdlsState(controllerHandle, &controllerState);
}

void dateSkip(int resetTimeAfterSkips, int skipForward, int resetNTP)
{
    int direction = 1; //Possible utility to roll back time for weather? If neg, roll backwards
    if(skipForward != 1)
        direction = direction * -1;

    if(origTime == 0)
    {
        Result ot = timeGetCurrentTime(TimeType_UserSystemClock, (u64*)&origTime);
        if(R_FAILED(ot))
            fatalThrow(ot);
    }

    Result tg = timeGetCurrentTime(TimeType_UserSystemClock, (u64*)&curTime); //Current system time
    if(R_FAILED(tg))
        fatalThrow(tg);

    time_t advanceTime = curTime + (direction * 86400);
    Result ts = timeSetCurrentTime(TimeType_NetworkSystemClock, (uint64_t)advanceTime); //Set new time
    if(R_FAILED(ts))
        fatalThrow(ts);

    resetSkips++;
    if(resetTimeAfterSkips != 0 && (resetTimeAfterSkips == resetSkips)) //Reset time after # of skips
        resetTime();
    if(resetNTP != 0)
        resetTimeNTP();
}

void resetTime()
{
    if(curTime == 0)
    {
        Result ct = timeGetCurrentTime(TimeType_UserSystemClock, (u64*)&curTime); //Current system time
        if(R_FAILED(ct))
            fatalThrow(ct);
    }

    struct tm currentTime = *localtime(&curTime);
    struct tm timeReset = *localtime(&origTime);
    timeReset.tm_hour = currentTime.tm_hour;
    timeReset.tm_min = currentTime.tm_min;
    timeReset.tm_sec = currentTime.tm_sec;
    Result rt = timeSetCurrentTime(TimeType_NetworkSystemClock, mktime(&timeReset));
    if(R_FAILED(rt))
        fatalThrow(rt);

    resetSkips = 0;
}

void resetTimeNTP()
{
    Result ts = timeSetCurrentTime(TimeType_NetworkSystemClock, (uint64_t)ntpGetTime());
    if(R_FAILED(ts))
        fatalThrow(ts);
}
