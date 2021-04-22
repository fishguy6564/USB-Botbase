#include <switch.h>
#define MAX_LINE_LENGTH 344 * 32 * 2

extern u64 mainLoopSleepTime;
extern bool debugResultCodes;

u64 parseStringToInt(char* arg);
u8* parseStringToByteBuffer(char* arg, u64* size);
HidNpadButton parseStringToButton(char* arg);
Result capsscCaptureForDebug(void *buffer, size_t buffer_size, u64 *size);
