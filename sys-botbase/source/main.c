#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <math.h>
#include <switch.h>
#include "commands.h"
#include "args.h"
#include "util.h"
#include <poll.h>
#include "pm_ams.h"

#define TITLE_ID 0x430000000000000B
#define HEAP_SIZE 0x000540000

typedef struct{
	u64 size;
	void* data;
}USBResponse;

// we aren't an applet
u32 __nx_applet_type = AppletType_None;

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

void sendUsbResponse(USBResponse x)
{
    usbCommsWrite((void*)&x, 4);

    if (x.size > 0) {
        usbCommsWrite(x.data, x.size);
    }
}

void __appInit(void)
{
    Result rc;
    svcSleepThread(20000000000L);
    rc = smInitialize();
    if (R_FAILED(rc))
        fatalThrow(rc);
    if (hosversionGet() == 0) {
        rc = setsysInitialize();
        if (R_SUCCEEDED(rc)) {
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
	USBResponse x;
    if (argc == 0)
        return 0;


    //peek <address in hex or dec> <amount of bytes in hex or dec>
    if (!strcmp(argv[0], "peek"))
    {
        if(argc != 3)
            return 0;

        attach();
        u64 offset = parseStringToInt(argv[1]);
        u64 size = parseStringToInt(argv[2]);
        u8* data[size];
		dmntchtReadCheatProcessMemory(metaData.heap_extents.base + offset, &data, size);
		x.size = size;
		x.data = &data[0];
		sendUsbResponse(x);
    }

    if (!strcmp(argv[0], "peekAbsolute"))
    {
        if(argc != 3)
            return 0;

        attach();
        u64 offset = parseStringToInt(argv[1]);
        u64 size = parseStringToInt(argv[2]);
        u8* data = peek(offset, size);
		x.size = size;
		x.data = &data[0];
		sendUsbResponse(x);
    }

    //poke <address in hex or dec> <amount of bytes in hex or dec> <data in hex or dec>
    if (!strcmp(argv[0], "poke"))
    {
        if(argc != 3)
            return 0;
        attach();
        u64 offset = parseStringToInt(argv[1]);
        u64 size = 0;
        u8* data = parseStringToByteBuffer(argv[2], &size);
        poke(metaData.heap_extents.base + offset, size, data);
        free(data);
    } 
    
    if (!strcmp(argv[0], "pokeAbsolute"))
    {
        if(argc != 3)
            return 0;
        attach();
        u64 offset = parseStringToInt(argv[1]);
        u64 size = 0;
        u8* data = parseStringToByteBuffer(argv[2], &size);
        poke(offset, size, data);
        free(data);
    }

    //click <buttontype>
    if (!strcmp(argv[0], "click"))
    {
        if(argc != 2)
            return 0;
        HidControllerKeys key = parseStringToButton(argv[1]);
        click(key);
    }

    //hold <buttontype>
    if (!strcmp(argv[0], "press"))
    {
        if(argc != 2)
            return 0;
        HidControllerKeys key = parseStringToButton(argv[1]);
        press(key);
    }

    //release <buttontype>
    if (!strcmp(argv[0], "release"))
    {
        if(argc != 2)
            return 0;
        HidControllerKeys key = parseStringToButton(argv[1]);
        release(key);
    }

    //setStick <left or right stick> <x value> <y value>
    if (!strcmp(argv[0], "setStick"))
    {
        if(argc != 4)
            return 0;
        
        int side = 0;
        if(!strcmp(argv[1], "LEFT")){
            side = JOYSTICK_LEFT;
        }else if(!strcmp(argv[1], "RIGHT")){
            side = JOYSTICK_RIGHT;
        }else{
            return 0;
        }

        int dxVal = strtol(argv[2], NULL, 0);
        if(dxVal > JOYSTICK_MAX) dxVal = JOYSTICK_MAX; //0x7FFF
        if(dxVal < JOYSTICK_MIN) dxVal = JOYSTICK_MIN; //-0x8000
        int dyVal = strtol(argv[3], NULL, 0);
        if(dxVal > JOYSTICK_MAX) dxVal = JOYSTICK_MAX;
        if(dxVal < JOYSTICK_MIN) dxVal = JOYSTICK_MIN;

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
    if(!strcmp(argv[0], "configure")){
        if(argc != 3)
            return 0;


        if(!strcmp(argv[1], "mainLoopSleepTime")){
            u64 time = parseStringToInt(argv[2]);
            mainLoopSleepTime = time;
        }

        if(!strcmp(argv[1], "buttonClickSleepTime")){
            u64 time = parseStringToInt(argv[2]);
            buttonClickSleepTime = time;
        }

        if(!strcmp(argv[1], "echoCommands")){
            u64 shouldActivate = parseStringToInt(argv[2]);
            echoCommands = shouldActivate != 0;
        }

        if(!strcmp(argv[1], "printDebugResultCodes")){
            u64 shouldActivate = parseStringToInt(argv[2]);
            debugResultCodes = shouldActivate != 0;
        }
    }

    if(!strcmp(argv[0], "getTitleID")){
        attach();
		x.size = sizeof(metaData.title_id);
		x.data = &metaData.title_id;
        sendUsbResponse(x);
    }

    if(!strcmp(argv[0], "getSystemLanguage")){
        //thanks zaksa
        setInitialize();
        u64 languageCode = 0;   
        SetLanguage language = SetLanguage_ENUS;
        setGetSystemLanguage(&languageCode);   
        setMakeLanguage(languageCode, &language);
		x.size = sizeof(language);
		x.data = &language;
		sendUsbResponse(x);
    }
 
    if(!strcmp(argv[0], "getMainNsoBase")){
        attach();
		x.size = sizeof(metaData.main_nso_extents.base);
		x.data = &metaData.main_nso_extents.base;
        sendUsbResponse(x);
    }

    if(!strcmp(argv[0], "getHeapBase")){
        attach();
		x.size = sizeof(metaData.heap_extents.base);
		x.data = &metaData.heap_extents.base;
        sendUsbResponse(x);
    }

    if(!strcmp(argv[0], "pixelPeek")){
        /*
        //take a screenshot:
        click(KEY_CAPTURE);
        svcSleepThread(300 * 1e+6L);
        capsaInitialize();

        u64 fileCount = 0;
        capsaGetAlbumFileCount(CapsAlbumStorage.CapsAlbumStorage_Sd, &fileCount);

        CapsAlbumEntry *entries = malloc(sizeof(CapsAlbumEntry) * fileCount);

        capsaGetAlbumFileList(CapsAlbumStorage.CapsAlbumStorage_Sd, &fileCount, entries, fileCount);

        CapsAlbumEntry entry = entries[fileCount - 1]; //latest entry?

        u64 fileSize = 0;
        capsaGetAlbumFileSize(entry.file_id, &fileSize);

        char *workBuf = malloc(fileSize);

        u64 imageBufferSize = 4 * 1280 * 720; 
        char *imageBuf = malloc(imageBufferSize);

        capsaLoadAlbumScreenShotImage(NULL, NULL, entry.file_id, imageBuf, imageBufferSize, workBuf, fileSize);

        printf("0,0: A %d, B %d, G %d, R %d\n", imageBuf[0], imageBuf[1], imageBuf[2], imageBuf[3]); //??
        capsaDeleteAlbumFile(entry.file_id);
        free(entries);
        free(workBuf);
        free(imageBuf);


        */
        /*
        int width, height, bpp;

        uint8_t* rgb_image = stbi_load("image.png", &width, &height, &bpp, 3);

        stbi_image_free(rgb_image);
        */

        //typedef enum {
        //    CapsAlbumStorage_Nand = 0,                  ///< Nand
        //    CapsAlbumStorage_Sd   = 1,                  ///< Sd
        //} CapsAlbumStorage;

        /// Initialize caps:a.
        //Result capsaInitialize(void);


        /**
         * @brief Gets a listing of \ref CapsAlbumEntry, where the AlbumFile's storage matches the input one.
         * @param[in] storage \ref CapsAlbumStorage
         * @param[out] out Total output entries.
         * @param[out] entries Output array of \ref CapsAlbumEntry.
         * @param[in] count Reserved entry count.
         */
        //Result capsaGetAlbumFileList(CapsAlbumStorage storage, u64 *out, CapsAlbumEntry *entries, u64 count);


        /**
         * @brief Gets the size for the specified AlbumFile.
         * @param[in] file_id \ref CapsAlbumFileId
         * @param[out] size Size of the file.
         */
        //Result capsaGetAlbumFileSize(const CapsAlbumFileId *file_id, u64 *size);

        /**
         * @brief Loads a file into the specified buffer.
         * @param[in] file_id \ref CapsAlbumFileId
         * @param[out] out_size Size of the AlbumFile.
         * @param[out] filebuf File output buffer.
         * @param[in] filebuf_size Size of the filebuf.
         */
        //Result capsaLoadAlbumFile(const CapsAlbumFileId *file_id, u64 *out_size, void* filebuf, u64 filebuf_size);

        /**
         * @brief Load the ScreenShotImage for the specified AlbumFile.
         * @note Only available on [2.0.0+].
         * @param[out] width Output image width. Optional, can be NULL.
         * @param[out] height Output image height. Optional, can be NULL.
         * @param[in] file_id \ref CapsAlbumFileId
         * @param[out] image RGBA8 image output buffer.
         * @param[in] image_size Image buffer size, should be at least large enough for RGBA8 1280x720.
         * @param[out] workbuf Work buffer, cleared to 0 by the cmd before it returns.
         * @param[in] workbuf_size Work buffer size, must be at least the size of the JPEG within the AlbumFile.
         */
        //Result capsaLoadAlbumScreenShotImage(u64 *width, u64 *height, const CapsAlbumFileId *file_id, void* image, u64 image_size, void* workbuf, u64 workbuf_size);





        /**
         * @brief Deletes an AlbumFile corresponding to the specified \ref CapsAlbumFileId.
         * @param[in] file_id \ref CapsAlbumFileId
         */
        //Result capsaDeleteAlbumFile(const CapsAlbumFileId *file_id);





        /// Initialize caps:dc
        //Result capsdcInitialize(void);
        /**
        * @brief Decodes a jpeg buffer into RGBX.
        * @param[in] width Image width.
        * @param[in] height Image height.
        * @param[in] opts \ref CapsScreenShotDecodeOption.
        * @param[in] jpeg Jpeg image input buffer.
        * @param[in] jpeg_size Input image buffer size.
        * @param[out] out_image RGBA8 image output buffer.
        * @param[in] out_image_size Output image buffer size, should be at least large enough for RGBA8 width x height.
        */
        //Result capsdcDecodeJpeg(u32 width, u32 height, const CapsScreenShotDecodeOption *opts, const void* jpeg, size_t jpeg_size, void* out_image, size_t out_image_size);

        

    }

    return 0;
}

int main()
{
	Result rc;
    rc = pmdmntInitialize();
	USBResponse x;
	
    if (rc) {
        // Failed to get PM debug interface.
        fatalThrow(222 | (6 << 9));
    }
	
	rc = usbCommsInitialize();
    if (rc) {
        fatalThrow(rc);
    }
	
    while (appletMainLoop())
    {
		int len;
		usbCommsRead(&len, sizeof(len));
		
		//Should use malloc
		char linebuf[len + 1];
		
		for(int i = 0; i < len+1; i++){
			linebuf[i] = 0;
		}
		
		usbCommsRead(&linebuf, len);
		
		//Adds necessary escape characters for pasrser
		linebuf[len-1] = '\n';
		linebuf[len-2] = '\r';
		
		fflush(stdout);
		parseArgs(linebuf, &argmain);
		
		
		if(echoCommands){
			x.size = sizeof(linebuf);
			x.data = &linebuf;
			sendUsbResponse(x);
		}
        
        svcSleepThread(mainLoopSleepTime * 1e+6L);
    }

    return 0;
}
