//Copyright (C) 2018 Daniel Bokser.  See LICENSE.txt for license
#ifndef COMMON_H
#define COMMON_H

#if defined(__APPLE__) || defined(__linux__)
#   define UNIX
#elif defined(_WIN32)
#   define WINDOWS
#else
#   error "Unsupported platform!"
#endif
#if defined(__APPLE__)
#   define MAC
#elif defined (__linux__)
#   define LINUX
#endif

#ifdef WINDOWS
#include <windows.h>
#define FILE_SEPARATOR "\\"
#define ENDL "\r\n"
#elif defined(UNIX)
#define FILE_SEPARATOR "/"
#define ENDL "\n"
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stddef.h>
#include <inttypes.h>
#include <stdarg.h>
#include <ctype.h>
#include <type_traits>

typedef int64_t i64;
typedef int32_t i32;
typedef int16_t i16;
typedef int8_t i8;

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef size_t usize;
#ifdef WINDOWS
typedef ptrdiff_t isize;
#else
typedef ssize_t isize;
#endif

//millisecond time
typedef i64 TimeMS;
//microsecond time
typedef i64 TimeUS;

//returns true if timer should continue, else false
typedef void (TimerFn)(void*);

#define ARRAY_LEN(array) ((i32)(sizeof(array)/sizeof(*array)))
#define UNUSED(x) ((void)(x))
#define fori(x) for (isize i = 0; i < (x); i++)
#define forj(x) for (isize j = 0; j < (x); j++)
#define foriarr(x) fori(ARRAY_LEN(x))
#define forjarr(x) forj(ARRAY_LEN(x))
#define forirange(from,to) for (i64 i = (from); i < (to); i++)
#define forjrange(from,to) for (i64 j = (from); j < (to); j++)
#define globalvar static
#define ALERT(fmt, ...)  do {\
    char *msg = nullptr;\
    buf_printf(msg, fmt, ##__VA_ARGS__);\
    CO_ERR(fmt, ##__VA_ARGS__);\
    alertDialog(msg);\
    buf_free(msg);\
}while(0)

#define ALERT_EXIT(fmt, ...) ALERT(fmt" Exiting...", ##__VA_ARGS__)

#define GB(n) (MB(n) * 1024)
#define MB(n) (KB(n) * 1024)
#define KB(n) (n * 1024)

#define BOOL_TO_STR(x) ((x) ? "true" : "false")

#define PRINT(format, ...) printf(format"\n", ##__VA_ARGS__)
#define PRINT_ERR(format, ...) fprintf(stderr, format"\n", ##__VA_ARGS__)

//debug tools
#ifdef CO_DEBUG 
#define BREAKPOINT() asm volatile ("int $3\n")
#define CO_ASSERT(expr) do {\
        if (!(expr)) {\
            CO_ERR("Assertion Failed on %s:%d.", __FILE__, __LINE__);\
            BREAKPOINT();\
        }\
} while(0)
#define CO_ASSERT_MSG(expr,...) do {\
        if (!(expr)) {\
            CO_ERR("Assertion Failed on %s:%d.", __FILE__, __LINE__);\
            CO_ERR("Assert failure reason: " __VA_ARGS__);\
            BREAKPOINT();\
        }\
} while(0)
#define CO_ASSERT_EQ(expected,actual) CO_ASSERT_MSG((expected) == (actual),\
    "Expected: %zd, Actual %zd", expected, actual);

#ifdef UNIX
#define CO_LOG(format, ...) do { double now = nowInSeconds();\
                                     printf("Time: %f -- Thread ID %" PRIu64 " -- %s:%d -- " format "\n", now, currentThreadID(), __FILE__, __LINE__, ##__VA_ARGS__);\
                                } while (0)
#define CO_ERR(format, ...) fprintf(stderr, "Thread ID %" PRIu64 " -- %s:%d -- " format "\n", currentThreadID(), __FILE__, __LINE__, ##__VA_ARGS__)
#elif defined(WINDOWS)
#define CO_LOG(format, ...) do { double now = nowInSeconds(); \
                                 char output[512];\
                                 snprintf(output, 511, "Time: %f -- Thread ID %llu -- %s:%d -- " format "\n", now, currentThreadID(), __FILE__, __LINE__, ##__VA_ARGS__);\
                                 OutputDebugString(output);\
                                 printf("%s", output); } while (0)
#define CO_ERR(format, ...) do { char output[512];\
                                 snprintf(output, 511, "Thread ID %llu -- %s:%d -- " format "\n", currentThreadID(), __FILE__, __LINE__, ##__VA_ARGS__);\
                                 OutputDebugString(output);\
                                 printf("%s", output);} while(0)

#endif
#define INVALID_CODE_PATH() CO_ASSERT(false)
#else 
#define CO_ASSERT(...)
#define CO_ASSERT_MSG(...)
#define CO_ASSERT_EQ(expected,actual)
#define CO_LOG(...) 
#define BREAKPOINT() 
#define INVALID_CODE_PATH()
#ifdef __APPLE__
#define CO_ERR(format, ...) fprintf(stderr, "ERROR: " format "\n", ##__VA_ARGS__)
#elif defined(__linux__)
#define CO_ERR(format, ...) fprintf(stderr,  "ERROR: " format "\n", ##__VA_ARGS__)
#elif defined(WINDOWS)
#define CO_ERR(format, ...) do { char output[512];\
                                 snprintf(output, 511, "ERROR: " format "\n", ##__VA_ARGS__);\
                                 OutputDebugString(output);\
                                 printf("%s", output);} while(0)

bool convertUTF8ToWChar(const char *in,  wchar_t *out, i64 outSize);
bool convertWCharToUTF8(const wchar_t *in, char *out, i64 outSize);
#endif
#endif


//time
struct Timer;

double nowInSeconds();

TimeMS nowInMilliseconds();
TimeUS nowInMicroseconds();

i64 unixWallClockTime();
Timer *startAsyncTimer(TimerFn *tf, void *arg, i64 timeInMilliseconds);
void exitAndFreeTimerAndWait(Timer *state);

//multithreading
struct Thread;
struct Mutex;
struct WaitCondition;

typedef void (*ThreadFn)(void *);

Thread *startThread(ThreadFn, void *arg);
Thread *startThread(ThreadFn);
Mutex *createMutex();
WaitCondition *createWaitCondition();
void lockMutex(Mutex *);
bool tryLockMutex(Mutex *);
void unlockMutex(Mutex *);
void waitForCondition(WaitCondition *, Mutex *);
void broadcastCondition(WaitCondition *);
void waitForAndFreeThread(Thread *);
u64 currentThreadID();
//TODO: put in clean up functions


//utilities
inline bool isBitSet(int bit, u8 val) {
    bool ret = (val & (1 << bit)) != 0;
    return ret;
}

inline bool isBitSet(int bit, u16 val) {
    bool ret = (val & (1 << bit)) != 0;
    return ret;
}

inline bool isBitSet(int bit, u32 val) {
    bool ret = (val & (1 << bit)) != 0;
    return ret;
}

inline bool isBitSet(int bit, u64 val) {
    bool ret = (val & (1 << bit)) != 0;
    return ret;
}

inline void setBit(int bit, u8 *value) {
    *value |= (1 << bit);
}

inline void setBit(int bit, u16 *value) {
    *value |= (1 << bit);
}

inline void setBit(int bit, u32 *value) {
    *value |= (1 << bit);
}

inline void setBit(int bit, u64 *value) {
    *value |= (1 << bit);
}

inline void clearBit(int bit, u8 *value) {
    *value &= ~(1 << bit);
}

inline void clearBit(int bit, u16 *value) {
    *value &= ~(1 << bit);
}

inline void clearBit(int bit, u32 *value) {
    *value &= ~(1 << bit);
}

inline void clearBit(int bit, u64 *value) {
    *value &= ~(1 << bit);
}

inline i64 intPow(i64 base, i64 exp) {
    if (exp <= 0) return 1;
    fori (exp - 1) {
        base *= base;   
    }
    
    return base;
}

//memory interface

#define PUSHM(n, type) (type*)pushMemory((n) * (isize)sizeof(type)) 
#define PUSHMCLR(n, type) (type*)pushMemory((n) * (isize)sizeof(type), true) 
#define PUSHMSTACK(stack, n, type) (type*)pushMemory(stack, (n) * (isize)sizeof(type)) 
#define PUSHMCLRSTACK(stack, n, type) (type*)pushMemory(stack, (n) * (isize)sizeof(type), true) 
#define RESIZEM(ptr, n, type) (type*)resizeMemory((u8*)ptr, (n) * (isize)sizeof(type)) 
#define RESIZEMSTACK(stack, ptr, n, type) (type*)resizeMemory((u8*)ptr, stack, (n) * (isize)sizeof(type)) 
#define POPMSTACK(mem, stack) popMemory((u8*)mem, stack)
#define POPM(mem) popMemory((u8*)mem)
#define ZEROM(mem, n, type) zeroMemory(mem, (n) * (isize)sizeof(type))

#define CO_MALLOC(n, type) (type*)chkMalloc((n)*sizeof(type))
#define CO_FREE(ptr) free(ptr)


struct MemoryContext;

struct MemoryStack {
    u8* baseAddress;
    u8* nextFreeAddress;
    i64 maxSize;
    char name[8];
    bool isInited;
};


#ifdef CO_DEBUG
#define MAX_STACKS 10
struct DebugMemorySnapshot {
    i64 numStacks;
    MemoryStack *existingStacks[MAX_STACKS];
    i64 usedSpacePerStack[MAX_STACKS];
};
DebugMemorySnapshot *memorySnapShot();
#endif

struct MemoryContext {
    u8* nextFreeAddress;
    void* memoryBase;
    i64 size;
    
    MemoryStack *generalMemory;

#ifdef CO_DEBUG
    DebugMemorySnapshot dms;
#endif
};

typedef void AlertDialogFn(const char *message);

struct AutoMemory {
    MemoryStack *stack;
    void *memory;

    AutoMemory(MemoryStack *stack, void *memory);
    AutoMemory(void *memory);
    ~AutoMemory(); 
};


void initPlatformFunctions(AlertDialogFn *alertDialogFn);
bool initMemory(i64 totalMemoryLength, i64 generalMemoryLength);
void makeMemoryStack(isize length, const char* name, MemoryStack *out);

extern AlertDialogFn *alertDialog;
extern MemoryStack *generalMemory;

MemoryContext *getMemoryContext();
#ifdef CO_DEBUG
extern "C"
#endif
void setPlatformContext(MemoryContext *mc, AlertDialogFn *alertDialogFn);

void *pushMemory(isize size, bool clearToZero = false);
void popMemory(u8 *memoryToPop); 
void *resizeMemory(u8 *memoryToResize, isize newSize);

void *pushMemory(MemoryStack* stack, isize size, bool clearToZero = false);
void popMemory(u8* memoryToPop, MemoryStack* stack); 
void *resizeMemory(u8* memoryToResize, MemoryStack* stack, isize newSize);
void resetStack(MemoryStack* stack, bool clearToZero);
isize getAmountOfMemoryLeft(MemoryStack *stack);

void *chkMalloc(size_t numBytes);

void copyMemory(const void* src, void* dest, i64 lenInBytes);
bool areStringsEqual(const char *lhs, const char *rhs, i64 lenInBytes);
bool isSubstringCaseInsensitive(const char *needle, char *haystack, i64 haystackLen);
bool isMemoryEqual(const void *lhs, const void *rhs, i64 lenInBytes);

int stringLength(const char *str);
int stringLength(const char *str, int maxAmount);
//dest should have at least len in bytes length
void copyString(const char *src, char *dest, i64 lenInBytes);
bool isEmptyString(const char *str);

inline void
fillMemory(void* memory, u8 fillValue, i64 lenInBytes) {
    u8* bytes = (u8*)memory;

    for(;lenInBytes;lenInBytes -= sizeof(u8)) {
        *bytes++ = fillValue;
    }
}

inline void
fillMemory(i16* memory, i16 fillValue, i64 lenInBytes) {
    CO_ASSERT((lenInBytes % (i64)sizeof(i16)) == 0);
    i16* bytes = (i16*)memory;

    for(;lenInBytes;lenInBytes -= sizeof(i16)) {
        *bytes++ = fillValue;
    }
}

inline void
zeroMemory(void* memory, i64 lenInBytes) {
    fillMemory(memory, (u8)0, lenInBytes);
}

struct BufHdr {
    size_t len;
    size_t cap;
    char buf[];
};

char *buf__printf(char *buf, const char *fmt, ...) __attribute__ ((format(printf, 2, 3)));
void *buf__grow(const void *buf, size_t new_len, size_t elem_size);
#ifndef MAX
#   define MAX(x,y) (((x) > (y)) ? (x) : (y))
#endif
#define buf__hdr(b) ((BufHdr *)((char *)(b) - offsetof(BufHdr, buf)))

#define buf_len(b) ((b) ? buf__hdr(b)->len : 0)
#define buf_cap(b) ((b) ? buf__hdr(b)->cap : 0)
#define buf_end(b) ((b) + buf_len(b))
#define buf_sizeof(b) ((b) ? buf_len(b)*sizeof(*b) : 0)

#define buf_free(b) ((b) ? (POPM(buf__hdr(b)), (b) = NULL) : 0)
#define buf_fit(b, n) ((n) <= buf_cap(b) ? 0 : ((b) = (std::remove_reference<decltype(b)>::type)buf__grow((void*)(b), (n), sizeof(*(b)))))
#define buf_push(b, data) (buf_fit((b), 1 + buf_len(b)), (b)[buf__hdr(b)->len++] = data)
#define buf_printf(b, ...) ((b) = buf__printf((b), __VA_ARGS__))
#define buf_clear(b) ((b) ? buf__hdr(b)->len = 0 : 0)


//file interface
#ifdef UNIX
#include <sys/param.h>
#define MAX_PATH_LEN (MAXPATHLEN * 4) // for unicode
#elif defined(WINDOWS)
#define MAX_PATH_LEN (MAX_PATH * 4)
#endif
enum class FileSystemResultCode {
    OK = 0,
    PermissionDenied,
    OutOfSpace,
    OutOfMemory,
    AlreadyExists,
    IOError,
    NotFound,
    Unknown,
    
    SizeOfEnumMinus1
};

struct ReadFileResult {
    u8* data;
    i64 size;
    FileSystemResultCode resultCode;
};

struct MemoryMappedFileHandle;


//outFilePath must be at least size MAX_PATH_LEN
bool getFilePathInHomeDir(const char *pathRelativeToHome, char *outFilePath);
//outFilePath must be at least size MAX_PATH_LEN
void getCurrentWorkingDirectory(char *outFilePath);
FileSystemResultCode changeDir(const char *path);
FileSystemResultCode createDir(const char *path);
bool doesDirectoryExistAndIsWritable(const char *path);

//returns the length of string not including null byte
//outStr must be at least MAX_PATH_LEN 
i64 timestampFileName(const char *fileName, const char *extension, char *outStr);

ReadFileResult readEntireFile(const char* fileName,  MemoryStack *memory);
void freeFileBuffer(ReadFileResult* fileDataToFree, MemoryStack *memory);
FileSystemResultCode writeDataToFile(const void* data, isize size, const char *fileName); 
FileSystemResultCode copyFile(const char *src, const char *dest,  MemoryStack *fileMemory);

MemoryMappedFileHandle *mapFileToMemory(const char *fileName, u8 **outData, usize *outLen);
void closeMemoryMappedFile(MemoryMappedFileHandle *);

//data structures
//Question: Why templates here and not on general purpose memory?
//Answer:  I don't plan on using this too much, and i want to see if it has any significant slow downs for compiling
template <typename T>
struct CircularBuffer {
    T *data;
    i64 len;
    i64 writeIndex, readIndex;
    i64 numItemsQueued;
};

template <typename T>
bool push(T val, CircularBuffer<T> *buffer) {

    if (buffer->numItemsQueued < buffer->len) {
        buffer->data[buffer->writeIndex++] = val;

        if (buffer->writeIndex >= buffer->len) {
            buffer->writeIndex = 0;
        }

        buffer->numItemsQueued++;
        return true;
    }
    else {
        return false;
    }
}
template <typename T>
void clear(CircularBuffer<T> *buffer) {
    buffer->numItemsQueued = 0;
    buffer->readIndex = 0;
    buffer->writeIndex = 0;
}

template <typename T>
i64 popn(i64 n, CircularBuffer<T> *buffer, T *output) {
    if (buffer->numItemsQueued <= 0) {
        return 0;
    }

    i64 numPopped = (n > buffer->numItemsQueued) ? buffer->numItemsQueued : n;
    if (buffer->readIndex + numPopped >= buffer->len) {
        i64 region1Len = buffer->len - buffer->readIndex;
        i64 region2Len = numPopped - region1Len;
        copyMemory(buffer->data + buffer->readIndex, output, region1Len * (i64)sizeof(T));
        copyMemory(buffer->data, output + region1Len, region2Len * (i64)sizeof(int));
    }
    else {
        copyMemory(buffer->data + buffer->readIndex, output, numPopped * (i64)sizeof(T));
    }

    buffer->readIndex += numPopped;
    buffer->readIndex %= buffer->len;

    buffer->numItemsQueued -= numPopped;

    return numPopped;

}

//profiler
#if defined(CO_DEBUG) && 0 
#define CO_PROFILE 
#endif

#ifdef CO_PROFILE
#define MAX_NESTED_PROFILE_SECTIONS 0x20
#define MAX_PROFILE_SECTIONS 0x1000
struct ProfileSectionState {
    double startTimeInSeconds;
    double maxRunTimeInSeconds;
    double totalRunTimeInSeconds;
    int numTimesCalled;
    const char *sectionName;
    bool isRoot;
    
    ProfileSectionState *children[MAX_NESTED_PROFILE_SECTIONS];
    int numChildren;
    
    bool isInitialized;
};
struct ProfileState { 
    ProfileSectionState *activeSections[MAX_NESTED_PROFILE_SECTIONS];
    ProfileSectionState **nextActiveSectionSlot;
    ProfileSectionState sectionStates[MAX_PROFILE_SECTIONS];
    ProfileSectionState *endSectionStateBucket;
    const char *sectionNames[MAX_PROFILE_SECTIONS];
    int numProfileSections;

    double programStartTime;
};
void profileInit(ProfileState *ps);
void profileStart(const char *sectionName, ProfileState *ps);
void profileEnd(ProfileState *ps);
ProfileSectionState *profileSectionStateForName(const char *name, ProfileState *ps);
#else
#define profileInit(...)
#define profileStart(...)
#define profileEnd(...)
#define profileSectionStateForName(...)
#endif

//Audio
union SoundFrame{

    struct {
        i16 leftChannel;
        i16 rightChannel;
    };
    i16 channels[2];
    i32 value;
};
typedef CircularBuffer<SoundFrame> SoundBuffer;
#endif

/***implementation start***/
#ifdef CO_IMPL
#undef CO_IMPL
#include <stdio.h>
#include <errno.h>
#include <limits.h>

struct Timer {
   TimerFn *timerFunc; 
   i64 timeInMilliseconds;
   void *arg;
   
   Thread *thread;
   volatile bool shouldExit;
};

struct _ThreadStartContext {
	ThreadFn fn;
	void *parameter;
};

/*******************
 * Platform agnostic
 *******************/
//memory
globalvar MemoryContext *memoryContext;
MemoryStack *generalMemory;
AlertDialogFn *alertDialog;

#ifdef CO_DEBUG
DebugMemorySnapshot *memorySnapShot() {
    auto dms = &memoryContext->dms;
    fori (memoryContext->dms.numStacks) {
        auto stack = dms->existingStacks[i];
        
        dms->usedSpacePerStack[i] = stack->nextFreeAddress - 
            stack->baseAddress;
    }

    return dms;
}
#endif

void makeMemoryStack(isize size, const char *name, MemoryStack *out) {
#ifdef CO_DEBUG
    CO_ASSERT(memoryContext->dms.numStacks < MAX_STACKS);
    memoryContext->dms.existingStacks[memoryContext->dms.numStacks++] = out;
#endif
    CO_ASSERT_MSG(memoryContext->nextFreeAddress + size <= 
            (u8*)memoryContext->memoryBase + memoryContext->size, "Request stack size %zd, bytes left: %ld", 
                  size, ((u8*)memoryContext->memoryBase + memoryContext->size) - memoryContext->nextFreeAddress);
    if (memoryContext->nextFreeAddress + size >
            (u8*)memoryContext->memoryBase + memoryContext->size) {
        ALERT_EXIT("Out of memory while creating memory stack! Request size %zd, bytes left: %ld",
                   size, ((u8*)memoryContext->memoryBase + memoryContext->size) - memoryContext->nextFreeAddress);
        exit(1);
    }
    
    fori (ARRAY_LEN(out->name) - 1) {
        out->name[i] = *name;
        if (!*name++) break;
    }
    out->name[ARRAY_LEN(out->name) - 1] = '\0';

    out->baseAddress =
        out->nextFreeAddress = memoryContext->nextFreeAddress;

    out->maxSize = size;

    memoryContext->nextFreeAddress += size;


    out->isInited = true;
    
} 

MemoryContext *getMemoryContext() {
    return memoryContext;
}

#ifdef CO_DEBUG
extern "C"
#endif
void setPlatformContext(MemoryContext *mc, AlertDialogFn *alertDialogFn) {
   memoryContext = mc; 
   generalMemory = mc->generalMemory;
   alertDialog = alertDialogFn;
}

int stringLength(const char *str) {
    int ret = 0;
    while (*str++)
       ret++; 
    return ret;
}
int stringLength(const char *str, int maxAmount) {
    int ret = 0;
    while (ret < maxAmount && *str++)
       ret++; 
    return ret;
}

bool isEmptyString(const char *str) {
    return *str == '\0';
}
void copyString(const char *src, char *dest, i64 lenInBytes) {
    strncpy(dest, src, (usize)lenInBytes);
    dest[lenInBytes] = '\0';
}

bool isMemoryEqual(const void *lhs, const void *rhs, i64 lenInBytes) {
    const u8 *lhsU8 = (const u8 *)lhs;
    const u8 *rhsU8 = (const u8 *)rhs;
    fori (lenInBytes) {
        if (*lhsU8++ != *rhsU8++) {
            return false;
        }
    }
    return true;
}
bool isSubstringCaseInsensitive(const char *needle, char *haystack, i64 haystackLen) {
    const char *tmpNeedle = needle;
    fori (haystackLen) {
        if (*tmpNeedle == '\0') {
            return true;
        }
        if (tolower(haystack[i]) != tolower(*tmpNeedle)) {
            if (haystack[i] == '\0') {
                return false;
            }
            else {
                tmpNeedle = needle;
            }
        }
        else {
            tmpNeedle++;
        }
    }
    
    return false;
}

bool areStringsEqual(const char *lhs, const char *rhs, i64 lenInBytes) {
    fori (lenInBytes) {
        if (*lhs != *rhs) {
            return false;
        }
        else if (*lhs == '\0') {
            return true;
        }
        lhs++; rhs++;
    }
    
    return true;
    
}
void copyMemory(const void* src, void* dest, i64 lenInBytes) {
    const u8* byteSrc = (const u8*)src;
    u8* byteDest = (u8*)dest;

    //TODO: use 64 bit copy for first n bytes divisible by 8
    if (byteSrc >= byteDest) {
        if (lenInBytes % 8 == 0) {
            const u64* src64 = (u64*)byteSrc;
            u64* dest64 = (u64*)byteDest;

            for(;lenInBytes > 0;lenInBytes -= 8){
                *dest64++ = *src64++;
            }
        }
        else {
            for(;lenInBytes > 0;lenInBytes--){
                *byteDest++ = *byteSrc++;
            }

        }
    }
    else {
        byteDest += lenInBytes;
        byteSrc += lenInBytes;

        if (lenInBytes % 8 == 0) {
            const u64* src64 = (u64*)byteSrc;
            u64* dest64 = (u64*)byteDest;

            for(;lenInBytes > 0;lenInBytes -= 8){
                *--dest64 = *--src64;
            }
        }
        else {
            for(;lenInBytes > 0;lenInBytes--){
                *--byteDest = *--byteSrc;
            }
        }

    }

}
void *pushMemory(isize size, bool clearToZero) {
    return pushMemory(generalMemory, size, clearToZero);
}
void popMemory(u8* memoryToPop) {
    popMemory(memoryToPop, generalMemory);
}
void *resizeMemory(u8 *memoryToResize, isize newSize) {
    return resizeMemory(memoryToResize, generalMemory, newSize);
}

void *pushMemory(MemoryStack* stack, isize size, bool clearToZero) {
    CO_ASSERT(stack->isInited);
    u8* ret = stack->nextFreeAddress;

    CO_ASSERT(stack->nextFreeAddress + size - stack->baseAddress <= 
            (i64)stack->maxSize);
    if (stack->nextFreeAddress + size - stack->baseAddress > 
            (i64)stack->maxSize) {
        ALERT_EXIT("Ran out of memory trying to allocate %zu bytes!", size);
        exit(1);
    }
    
    stack->nextFreeAddress += size;

    

    if (clearToZero) {
        zeroMemory(ret, size);
    }

    return ret;
}

void popMemory(u8* memoryToPop, MemoryStack* stack) {
    CO_ASSERT(stack->isInited);
    if (!memoryToPop) {
        return;
    }
    CO_ASSERT(memoryToPop - stack->baseAddress <= stack->maxSize &&
            memoryToPop <= stack->nextFreeAddress);
    stack->nextFreeAddress = memoryToPop;
}

void *resizeMemory(u8* memoryToResize, MemoryStack* stack, isize newSize) {
    CO_ASSERT(stack->isInited);
    popMemory(memoryToResize, stack);

    void *ret = pushMemory(stack, newSize, false);
    return ret;

}

void resetStack(MemoryStack* stack, bool clearToZero = false) {
    CO_ASSERT(stack->isInited);
    stack->nextFreeAddress = stack->baseAddress;

    if (clearToZero) {
        zeroMemory(stack->baseAddress, stack->maxSize);
    }
}

isize getAmountOfMemoryLeft(MemoryStack *stack) {
   isize ret = (stack->isInited) ?
               (stack->baseAddress + stack->maxSize) - stack->nextFreeAddress :
               0; 
   return ret >= 0 ? ret : 0;
}

void *chkMalloc(size_t numBytes) {
    void *ret = malloc(numBytes);
    if (!ret) {
        ALERT_EXIT("Ran out of memory trying to malloc %zu bytes!", numBytes);
        exit(1);
    }
    return ret;
}
AutoMemory::AutoMemory(MemoryStack *stack, void *memory)
    :stack(stack), memory(memory) {}


AutoMemory::AutoMemory(void *memory)
    :stack(generalMemory), memory(memory) {}

AutoMemory::~AutoMemory() {
    POPMSTACK(memory, stack);
}

void *buf__grow(const void *buf, size_t new_len, size_t elem_size) {
    CO_ASSERT(buf_cap(buf) <= (SIZE_MAX - 1)/2);
    size_t new_cap = MAX(16, MAX(1 + 2*buf_cap(buf), new_len));
    CO_ASSERT(new_len <= new_cap);
    CO_ASSERT(new_cap <= (SIZE_MAX - offsetof(BufHdr, buf))/elem_size);
    i64 new_size = (i64)(offsetof(BufHdr, buf) + new_cap*elem_size);
    BufHdr *new_hdr;
    if (buf) {
        new_hdr = (BufHdr*)RESIZEM(buf__hdr(buf), new_size, u8);
    } else {
        new_hdr = (BufHdr*)PUSHM(new_size, u8);
        new_hdr->len = 0;
    }   
    new_hdr->cap = new_cap;
    return new_hdr->buf;
}
char *buf__printf(char *buf, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    size_t cap = buf_cap(buf) - buf_len(buf);
    size_t n = 1 + (size_t)vsnprintf(buf_end(buf), cap, fmt, args);

    va_end(args);
    if (n > cap) {
        buf_fit(buf, n + buf_len(buf));
        va_start(args, fmt);
        cap = buf_cap(buf) - buf_len(buf);
        n = 1 + (size_t)vsnprintf(buf_end(buf), cap, fmt, args);
        CO_ASSERT(n <= cap);
        va_end(args);
    }
    buf__hdr(buf)->len += n - 1;
    return buf;
}

void freeFileBuffer(ReadFileResult* fileDataToFree, MemoryStack *fileMemory) {
    CO_ASSERT(fileMemory->isInited);

    POPMSTACK(fileDataToFree->data, fileMemory);

}

FileSystemResultCode copyFile(const char *src, const char *dest, MemoryStack *fileMemory) {
    auto readResult = readEntireFile(src, fileMemory);
    switch (readResult.resultCode) {
    case FileSystemResultCode::OK: break;
    default:  {
        auto result = readResult.resultCode;
        freeFileBuffer(&readResult, fileMemory);
        return result;
    } break;
    }
    auto writeResult = writeDataToFile(readResult.data, readResult.size, dest);
    freeFileBuffer(&readResult, fileMemory);
    return writeResult;
}


#ifdef CO_PROFILE

void profileInit(ProfileState *ps) {
   zeroMemory(ps, sizeof(*ps));
   ps->programStartTime = nowInSeconds();
   ps->nextActiveSectionSlot = ps->activeSections;
   ps->endSectionStateBucket = ps->sectionStates + MAX_PROFILE_SECTIONS - 1;
}

ProfileSectionState *profileSectionStateForName(const char *name, ProfileState *ps) {
   ProfileSectionState *startBucket = ps->sectionStates + (((usize)(void*)name) & (MAX_PROFILE_SECTIONS - 1)); 
   
   CO_ASSERT(startBucket - ps->sectionStates < MAX_PROFILE_SECTIONS);
   
   if (!startBucket->isInitialized || startBucket->sectionName == name) {
       return startBucket;
   }
   
   auto tmp = startBucket + 1;
   do {
       if (!tmp->isInitialized || tmp->sectionName == name) {
           return tmp;
       }
       tmp++;
       if (tmp == ps->endSectionStateBucket + 1) {
           tmp = ps->sectionStates;
       }
   } while (tmp != startBucket);
   
   CO_ASSERT_MSG(false, "Reached max profile sections!");
   return nullptr;
}

void profileStart(const char *sectionName, ProfileState *ps) {
    ProfileSectionState *pss = profileSectionStateForName(sectionName, ps);
    if (!pss->isInitialized) {
        pss->numChildren = 0;
        pss->isInitialized = true;
        pss->numTimesCalled = 0;
        pss->sectionName = sectionName;
        ps->sectionNames[ps->numProfileSections++] = sectionName;
        //set the parent
        if (ps->nextActiveSectionSlot > ps->activeSections) { 
            auto parent = ps->nextActiveSectionSlot[-1];
            parent->children[parent->numChildren++] = pss;
            pss->isRoot = false;
        }
        else {
            pss->isRoot = true;
        }
        CO_ASSERT_MSG(ps->numProfileSections <= MAX_PROFILE_SECTIONS, "Too many profile sections");
    }
    pss->startTimeInSeconds = nowInSeconds();
    *ps->nextActiveSectionSlot++ = pss;
    CO_ASSERT_MSG(ps->nextActiveSectionSlot - ps->activeSections < MAX_PROFILE_SECTIONS, "Too many profile sections");
}


void profileEnd(ProfileState *ps) {
    if (ps->nextActiveSectionSlot == ps->activeSections) {
        return;
    }
    auto pss = ps->nextActiveSectionSlot[-1];
    ps->nextActiveSectionSlot--;
    CO_ASSERT_MSG(ps->nextActiveSectionSlot >= ps->activeSections, "Profile end without matching profileStart");
    if (!pss->isInitialized){
        return;
    }
    
    double sectionTime = nowInSeconds() - pss->startTimeInSeconds;
    pss->numTimesCalled++; 
    pss->totalRunTimeInSeconds += sectionTime;
    if (sectionTime > pss->maxRunTimeInSeconds) pss->maxRunTimeInSeconds = sectionTime;
           
}
#endif


/*******************
 * Platform Specific
 *******************/

#ifdef UNIX
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
//timing

static void mainTimerLoop(void *state) {
    timespec timeToSleepFor;
    Timer *timerState = (Timer*)state;
    timeToSleepFor.tv_sec = timerState->timeInMilliseconds/1000;
    timeToSleepFor.tv_nsec = (timerState->timeInMilliseconds - (timeToSleepFor.tv_sec * 1000)) * 1000000;
            
    while(!timerState->shouldExit) {
        timerState->timerFunc(timerState->arg);
        while (nanosleep(&timeToSleepFor, &timeToSleepFor) != 0 && errno == EINTR)
            ;
    }
}

void exitAndFreeTimerAndWait(Timer *state) {
    state->shouldExit = true;
    waitForAndFreeThread(state->thread);
    CO_FREE(state);
}


double nowInSeconds() {
     double ts;
     timespec tspec;
     clock_gettime(CLOCK_MONOTONIC, &tspec);

     ts = tspec.tv_sec + tspec.tv_nsec / 1000000000.;

     return ts;
}

TimeMS nowInMilliseconds() {
	TimeMS ms;
	timespec tspec;
	clock_gettime(CLOCK_MONOTONIC, &tspec);

	ms = (tspec.tv_sec * 1000) + (tspec.tv_nsec / 1000000);
    
    return ms;
}

TimeUS nowInMicroseconds() {
	TimeUS ms;
	timespec tspec;
	clock_gettime(CLOCK_MONOTONIC, &tspec);

	ms = (tspec.tv_sec * 1000000) + (tspec.tv_nsec / 1000);
    
    return ms;
}
i64 unixWallClockTime() {
    return (i64)time(nullptr);
}
Timer *startAsyncTimer(TimerFn *tf, void *arg, TimeMS time) {
    
    Timer *timerState = CO_MALLOC(1, Timer); 
    *timerState = {};
    
    timerState->timerFunc = tf;
    timerState->timeInMilliseconds = time;
    timerState->arg = arg;
    timerState->thread = startThread(mainTimerLoop, timerState);
    
    return timerState;
}

//threading
struct Thread {
    pthread_t value;
};
struct Mutex {
    pthread_mutex_t value;
};
struct WaitCondition {
    pthread_cond_t value;
};


static void *threadStart(void *param) {
	auto threadStartContext = (_ThreadStartContext*) param; 
	threadStartContext->fn(threadStartContext->parameter);
	CO_FREE(threadStartContext);
	return nullptr;
}
Thread *startThread(ThreadFn fn, void *arg)  {
	_ThreadStartContext *tsc = CO_MALLOC(1, _ThreadStartContext);
    Thread *ret = CO_MALLOC(1, Thread);
                           
	tsc->fn = fn;
	tsc->parameter= arg;
    int result = pthread_create(&ret->value, nullptr, threadStart, tsc);
    UNUSED(result);
    CO_ASSERT_MSG(result == 0, "Failed to create timer thread!");
    
    return ret;
}
Thread *startThread(ThreadFn fn) {
    return startThread(fn, nullptr);
}
Mutex *createMutex() {
    Mutex *ret = CO_MALLOC(1, Mutex);
    int result = pthread_mutex_init(&ret->value, nullptr);
    UNUSED(result);
    CO_ASSERT_MSG(result == 0, "Could create mutex"); 
    return ret;
}
void lockMutex(Mutex *mutex) {
    int result = pthread_mutex_lock(&mutex->value);
    UNUSED(result);
    CO_ASSERT_MSG(result == 0, "Could not lock"); 
}
bool tryLockMutex(Mutex *mutex) {
   int result = pthread_mutex_trylock(&mutex->value);
   CO_ASSERT_MSG(result == 0 || result == EBUSY, "Could not try lock");
   return result == 0;
}
void unlockMutex(Mutex *mutex) {
    int result = pthread_mutex_unlock(&mutex->value);
    UNUSED(result);
    CO_ASSERT_MSG(result == 0, "Failed to unlock mutex"); 
}
//TODO
WaitCondition *createWaitCondition() {
    auto ret = CO_MALLOC(1, WaitCondition);
    int result = pthread_cond_init(&ret->value, nullptr);
    UNUSED(result);
    CO_ASSERT_MSG(result == 0, "Failed to create wait condition"); 
    return ret;
}
void waitForCondition(WaitCondition *waitCondition, Mutex *mutex) {
    int result = pthread_cond_wait(&waitCondition->value, &mutex->value);
    UNUSED(result);
    CO_ASSERT_MSG(result == 0, "Failed to wait"); 
}
void broadcastCondition(WaitCondition *waitCondition) {
    int result = pthread_cond_broadcast(&waitCondition->value);
    UNUSED(result);
    CO_ASSERT_MSG(result == 0, "Failed to wait"); 
}
void waitForAndFreeThread(Thread *thread) {
    int result = pthread_join(thread->value, nullptr);
    CO_FREE(thread);
    UNUSED(result);
    CO_ASSERT_MSG(result == 0, "Failed to join");
}
u64 currentThreadID() {
    return (u64)pthread_self();
}

//memory

bool initMemory(i64 totalMemoryLength, i64 generalMemoryLength) {
   {
        totalMemoryLength += sizeof(MemoryContext) + sizeof(MemoryStack);
        CO_ASSERT(totalMemoryLength >= generalMemoryLength);
        MemoryContext tmpMC = {};
        tmpMC.size = (i64)totalMemoryLength;

        u8* prgMemBlock = (u8*)mmap(nullptr, (u64)totalMemoryLength, PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);

        if (prgMemBlock == MAP_FAILED) {
            return false;
        }

        tmpMC.memoryBase = tmpMC.nextFreeAddress = prgMemBlock;

        memoryContext = (MemoryContext*)tmpMC.nextFreeAddress;
        tmpMC.nextFreeAddress += sizeof(MemoryContext);

        *memoryContext = tmpMC;
    }

    {
        generalMemory = (MemoryStack*)memoryContext->nextFreeAddress;
        memoryContext->nextFreeAddress += sizeof(MemoryStack);
        makeMemoryStack(generalMemoryLength, "general", generalMemory);
        memoryContext->generalMemory = generalMemory;
    }
    
    
    return true;
}

void initPlatformFunctions(AlertDialogFn *alertDialogFn) {
    alertDialog = alertDialogFn;
}

//file 
struct MemoryMappedFileHandle {
    int fd;
    u8 *data;
    usize len;
};


FileSystemResultCode writeDataToFile(const void *data, isize size, const char *fileName) {
    FILE *f = fopen(fileName, "wb");
    if (!f) {
        CO_ERR("Error opening file %s", fileName);
        switch (errno) {
        case EACCES: 
        case EPERM:
            return FileSystemResultCode::PermissionDenied;
        default:
            return FileSystemResultCode::Unknown;
        }
    }

    if (fwrite(data, (size_t)size, 1, f) != 1) {
        CO_ERR("Error writing %zd bytes to file %s", size, fileName);
        if (ferror(f)) {
           switch (errno) {
           case ENOSPC: return FileSystemResultCode::OutOfSpace;
           case EIO: return FileSystemResultCode::IOError;
           default: return FileSystemResultCode::Unknown;
           }
        }
        else if (size == 0) {
            return FileSystemResultCode::OK;
        }
        else {
            return FileSystemResultCode::Unknown;
        }
    }
    fclose(f);
    
    return FileSystemResultCode::OK;
}
ReadFileResult readEntireFile(const char *fileName, MemoryStack *fileMemory) {
    CO_ASSERT(fileMemory->isInited);

    ReadFileResult ret = {};
    
    u8 *data = nullptr;
    i64 size;
    FILE *f = fopen(fileName, "rb");

    if (!f) {
        CO_ERR("Error opening file: %s", fileName);
        switch (errno) {
        case ENOENT: 
		case EBADF:
            ret.resultCode = FileSystemResultCode::NotFound; 
            break;
        case EACCES: 
        case EPERM:
            ret.resultCode = FileSystemResultCode::PermissionDenied;
            break;
        default:
            ret.resultCode = FileSystemResultCode::Unknown;
            break;
        }
        goto error;
    }

    fseek(f, 0l, SEEK_END);
    size = ftell(f);
    fseek(f, 0l, SEEK_SET);
    
    data = PUSHMSTACK(fileMemory, size, u8);

    if (!data) {
        ret.resultCode = FileSystemResultCode::OutOfMemory; 
        goto error;
    }

    if (fread(data, (size_t)size, 1, f) != 1) {
        CO_ERR("Error reading file %s", fileName);
        if (ferror(f)) {
            switch (errno) {
            case EACCES:
            case EPERM:
                ret.resultCode = FileSystemResultCode::PermissionDenied; 
            case EIO:
                ret.resultCode = FileSystemResultCode::IOError;
            default:
                ret.resultCode = FileSystemResultCode::Unknown;
            }
        }
        else if (size == 0) {
            ret.resultCode = FileSystemResultCode::OK;
        }
        else {
            ret.resultCode = FileSystemResultCode::Unknown;
        }
        goto error;
    }

    ret.data = data;
    ret.size = size;
    ret.resultCode = FileSystemResultCode::OK;

    fclose(f);

    return ret;

error:

    if (f){
        fclose(f);
    }

    if (data) {
        POPMSTACK(data, fileMemory);
    }
    return ret;

}

void getCurrentWorkingDirectory(char *outFilePath) {
    getcwd(outFilePath, MAX_PATH_LEN);
}

bool getFilePathInHomeDir(const char *pathRelativeToHome, char *outFilePath) {
    char *homeDir = getenv("HOME");
    if (!homeDir) {
        CO_ERR("Failed to find HOME env var");
        return false;
    }
    snprintf(outFilePath, MAX_PATH_LEN, "%s/%s", homeDir, pathRelativeToHome);
    return true;
}

FileSystemResultCode createDir(const char *path) {
    int code = mkdir(path, 0755);
    
    if (code == 0) {
        return FileSystemResultCode::OK;
    }
    else {
        switch (errno) {
        case EEXIST:
            return FileSystemResultCode::AlreadyExists;
        case EACCES:
            return FileSystemResultCode::PermissionDenied; 
        case EIO:
            return FileSystemResultCode::IOError;
         default:
            return FileSystemResultCode::Unknown;
        }
    }
}

FileSystemResultCode changeDir(const char *path) {
    int code = chdir(path);
    if (code != 0) {
        CO_ERR("Could not change directory to %s. Error Code: %d", path, errno);
        switch (errno) {
        case EACCES:
            return FileSystemResultCode::PermissionDenied; 
        case EIO:
            return FileSystemResultCode::IOError;
         default:
            return FileSystemResultCode::Unknown;
        }
    }
    return FileSystemResultCode::OK;
}

bool doesDirectoryExistAndIsWritable(const char *path) {
   struct stat pathStat;
   if (stat(path, &pathStat) == 0) {
       return S_ISDIR(pathStat.st_mode) && access(path, W_OK) == 0; 
   }
   else {
       return false;
   }
}


i64 timestampFileName(const char *fileName, const char *extension, char *outStr) {
    time_t c = (time_t)clock();
    tm *t = localtime(&c);
    
    char datetime[255];
    usize result = strftime(datetime, 255, "%m%d%Y_%H%M%S", t); 
    UNUSED(result);
    CO_ASSERT(result < 255 && result > 0);
    
    auto len = (i64)snprintf(outStr, MAX_PATH_LEN, "%s_%s.%s", fileName, datetime, extension); 
    return len;
}

MemoryMappedFileHandle *mapFileToMemory(const char *fileName, u8 **outData, usize *outLen) {

    struct stat s;
    int result = stat(fileName, &s); 
    if (result != 0) {
        CO_ERR("Could not find file %s!", fileName); 
        return nullptr;
    }
    
    int fd = open(fileName, O_RDWR);
    if (fd <= 0) {
        CO_ERR("Could not open file %s!", fileName); 
        return nullptr;
    }
    u8 *fileMap = (u8*)mmap(nullptr, (usize)s.st_size, PROT_WRITE, MAP_SHARED, fd, 0);
    if (fileMap == MAP_FAILED) {
        CO_ERR("Could not map file to memory! Errno: %d", errno); 
        close(fd);
        return nullptr;
    }
    
    
    MemoryMappedFileHandle *ret = CO_MALLOC(1, MemoryMappedFileHandle);
    ret->fd = fd;
    ret->data = fileMap;
    ret->len = (usize)s.st_size;

    *outData = (u8*)fileMap;
    *outLen = (usize)s.st_size;
    
    return ret;
}
void closeMemoryMappedFile(MemoryMappedFileHandle *mmfh) {
    if (mmfh) {
        munmap(mmfh->data, mmfh->len);
        close(mmfh->fd);
        CO_FREE(mmfh);
    }
}
#elif defined(WINDOWS)
//timing
static void mainTimerLoop(void *state) {
    Timer *timerState = (Timer*)state;
            
    while(!timerState->shouldExit) {
        timerState->timerFunc(timerState->arg);
        Sleep((DWORD)timerState->timeInMilliseconds);
    }
}
double nowInSeconds() {
     double ts;
     LARGE_INTEGER counter, freq;
     bool result = QueryPerformanceCounter(&counter);
     CO_ASSERT(result);
     result = QueryPerformanceFrequency(&freq);
     CO_ASSERT(result);

     ts = (double)counter.QuadPart / (double)freq.QuadPart;

     return ts;
}

i64 timestampFileName(const char *fileName, const char *extension, char *outStr) {
	int expectedLen = GetDateFormatA(LOCALE_NAME_USER_DEFAULT, 0, nullptr, "MMddyyyy", nullptr, 0);
	char *date = PUSHM(expectedLen, char);
	expectedLen = GetDateFormatA(LOCALE_NAME_USER_DEFAULT, 0, nullptr, "MMddyyyy", date, expectedLen);
	CO_ASSERT_MSG(expectedLen != 0, "Could not get date");

	expectedLen = GetTimeFormatA(LOCALE_NAME_USER_DEFAULT, 0, nullptr, "HHmmss", nullptr, 0);
	char *time = PUSHM(expectedLen, char);
	expectedLen = GetTimeFormatA(LOCALE_NAME_USER_DEFAULT, 0, nullptr, "HHmmss", time, expectedLen);
	CO_ASSERT_MSG(expectedLen != 0, "Could not get time");
	
	i64 len = (i64)snprintf(outStr, MAX_PATH_LEN, "%s_%s_%s.%s", fileName, date, time, extension);

	POPM(date); //only need to POPM date.  unnecessary for time
	return len;
}


#define UNIX_TIME_START  0x019DB1DED53E8000 //January 1, 1970 (start of Unix epoch) in "ticks"
#define TICKS_PER_SECOND  10000000 //a tick is 100ns
i64 unixWallClockTime() {

	FILETIME ft;
	GetSystemTimeAsFileTime(&ft); //returns ticks in UTC

									 //Copy the low and high parts of FILETIME into a LARGE_INTEGER
									 //This is so we can access the full 64-bits as an Int64 without causing an alignment fault
	LARGE_INTEGER li;
	li.LowPart = ft.dwLowDateTime;
	li.HighPart = ft.dwHighDateTime;

	//Convert ticks since 1/1/1970 into seconds
	return (li.QuadPart - UNIX_TIME_START) / TICKS_PER_SECOND;
}
#undef TICKS_PER_SECOND
#undef UNIX_TIME_START

static LARGE_INTEGER performanceFreq;
static bool isFreqInited = false;

TimeMS nowInMilliseconds() {

	TimeMS ms;
    LARGE_INTEGER counter;
    bool result = QueryPerformanceCounter(&counter);
    CO_ASSERT(result);
	if (!isFreqInited) {
		isFreqInited = true;
		result = QueryPerformanceFrequency(&performanceFreq);
		CO_ASSERT(result);
	}
    ms = (counter.QuadPart * 1000) / performanceFreq.QuadPart;
    
    return ms;
}
TimeUS nowInMicroseconds() {

	TimeUS us;
    LARGE_INTEGER counter;
    bool result = QueryPerformanceCounter(&counter);
    CO_ASSERT(result);
	if (!isFreqInited) {
		isFreqInited = true;
		result = QueryPerformanceFrequency(&performanceFreq);
		CO_ASSERT(result);
	}
    CO_ASSERT(result);
    us = (counter.QuadPart * 1000000) / performanceFreq.QuadPart;
    
    return us;
}
Timer *startAsyncTimer(TimerFn *tf, void *arg, TimeMS time) {
    
    Timer *timerState = CO_MALLOC(1, Timer); 
    *timerState = {};
    
    timerState->timerFunc = tf;
    timerState->timeInMilliseconds = time;
    timerState->arg = arg;
    timerState->thread = startThread(mainTimerLoop, timerState);
    
    return timerState;
}
//threading
struct Thread {
    HANDLE value;
};
struct Mutex {
    CRITICAL_SECTION value;
};
struct WaitCondition {
	CONDITION_VARIABLE value;
};
static DWORD threadStart(LPVOID param) {
	auto threadStartContext = (_ThreadStartContext*) param; 
	threadStartContext->fn(threadStartContext->parameter);
	CO_FREE(threadStartContext);
	return 0;
}
Thread *startThread(ThreadFn fn, void *arg)  {
	_ThreadStartContext *tsc = CO_MALLOC(1, _ThreadStartContext);
    Thread *ret = CO_MALLOC(1, Thread);
	tsc->fn = fn;
	tsc->parameter= arg;
	HANDLE result = CreateThread(nullptr, 0, threadStart, tsc, 0, nullptr);
	CO_ASSERT_MSG(result, "Failed to create hread!");
    ret->value = result;
    return ret;
}
Thread *startThread(ThreadFn fn) {
    return startThread(fn, nullptr);
}
Mutex *createMutex() {
    Mutex *ret = CO_MALLOC(1, Mutex);
	InitializeCriticalSection(&ret->value);
    return ret;
}
void lockMutex(Mutex *mutex) {
	EnterCriticalSection(&mutex->value);
}
bool tryLockMutex(Mutex *mutex) {
	return TryEnterCriticalSection(&mutex->value);
}
void unlockMutex(Mutex *mutex) {
	LeaveCriticalSection(&mutex->value);
}
WaitCondition *createWaitCondition() {
	WaitCondition *ret = CO_MALLOC(1, WaitCondition);
	InitializeConditionVariable(&ret->value);
	return ret;
}
void waitForCondition(WaitCondition *waitCondition, Mutex *mutex) {
	DWORD result = SleepConditionVariableCS(&waitCondition->value, &mutex->value, INFINITE);
	CO_ASSERT_MSG(result != 0, "Failed to wait on condition!");
	UNUSED(result);
}
void broadcastCondition(WaitCondition *waitCondition) {
	WakeAllConditionVariable(&waitCondition->value);
}
void waitForAndFreeThread(Thread *thread) {
    DWORD result = WaitForSingleObject(thread->value, INFINITE);
    CO_FREE(thread);
	UNUSED(result);
    CO_ASSERT_MSG(result == WAIT_OBJECT_0, "Failed to join");
}
u64 currentThreadID() {
    return (u64)GetCurrentThreadId();
}
//memory
bool initMemory(i64 totalMemoryLength, i64 generalMemoryLength) {
   {
        CO_ASSERT(totalMemoryLength > generalMemoryLength);
        MemoryContext tmpMC = {};
        tmpMC.size = (i64)totalMemoryLength;

        //TODO: play with the COMMIT and RESERVE options
        u8* prgMemBlock = (u8*)VirtualAlloc(nullptr, (SIZE_T)totalMemoryLength, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

        if (prgMemBlock == nullptr) {
            return false;
        }

        tmpMC.memoryBase = tmpMC.nextFreeAddress = prgMemBlock;

        memoryContext = (MemoryContext*)tmpMC.nextFreeAddress;
        tmpMC.nextFreeAddress += sizeof(MemoryContext);

        *memoryContext = tmpMC;
    }

    {
        generalMemory = (MemoryStack*)memoryContext->nextFreeAddress;
        memoryContext->nextFreeAddress += sizeof(MemoryContext);
        makeMemoryStack(generalMemoryLength, "general", generalMemory);
        memoryContext->generalMemory = generalMemory;
    }
    
    
    return true;
}

//files
#include <direct.h>
#include <commdlg.h>
#include <io.h>
struct MemoryMappedFileHandle {
    HANDLE fileHandle;
    HANDLE mappingObject;
    u8 *data;
    long long len;
};
bool convertWCharToUTF8(const wchar_t *in,  char *out, i64 outSize) {
	int result = WideCharToMultiByte(CP_UTF8, 0, in, -1, out, outSize, nullptr, nullptr);
	return result != 0;
}
bool convertUTF8ToWChar(const char *in, wchar_t *out, i64 outSize) {
	int result = MultiByteToWideChar(CP_UTF8, 0, in, -1, out, outSize);
	return result != 0;
}
//file implementation
ReadFileResult 
readEntireFile(const char *fileName, MemoryStack *fileMemory) {
    CO_ASSERT(fileMemory->isInited);

    ReadFileResult ret = {};
    
    u8 *data = nullptr;
    i64 size;
	wchar_t fileNameWide[MAX_PATH + 1];
	FILE *f;

	if (!convertUTF8ToWChar(fileName, fileNameWide, MAX_PATH)) {
		ret.resultCode = FileSystemResultCode::Unknown;
		goto error;
	}
    f = _wfopen(fileNameWide, L"rb");

    if (!f) {
        CO_ERR("Error opening file: %s", fileName);
        switch (errno) {
        case ENOENT: 
		case EBADF:
            ret.resultCode = FileSystemResultCode::NotFound; 
            break;
        case EACCES: 
        case EPERM:
            ret.resultCode = FileSystemResultCode::PermissionDenied;
            break;
        default:
            ret.resultCode = FileSystemResultCode::Unknown;
            break;
        }
        goto error;
    }

    fseek(f, 0l, SEEK_END);
    size = ftell(f);
    fseek(f, 0l, SEEK_SET);

    data = PUSHMSTACK(fileMemory, size, u8);

    if (!data) {
        goto error;
    }

    if (fread(data, (size_t)size, 1, f) != 1) {
        CO_ERR("Error reading file %s", fileName);
        if (ferror(f)) {
            switch (errno) {
            case EACCES:
            case EPERM:
                ret.resultCode = FileSystemResultCode::PermissionDenied; 
            case EIO:
                ret.resultCode = FileSystemResultCode::IOError;
            default:
                ret.resultCode = FileSystemResultCode::Unknown;
            }
        }
        else if (size == 0) {
            ret.resultCode = FileSystemResultCode::OK;
        }
        else {
            ret.resultCode = FileSystemResultCode::Unknown;
        }
        goto error;
    }

    ret.data = data;
    ret.size = size;
    ret.resultCode = FileSystemResultCode::OK;

    fclose(f);

    return ret;

error:

    if (f){
        fclose(f);
    }

    if (data) {
        POPMSTACK(data, fileMemory);
    }
    return ret;

}
FileSystemResultCode writeDataToFile(const void *data, isize size, const char *fileName) {
	wchar_t fileNameWide[MAX_PATH + 1];

	if (!convertUTF8ToWChar(fileName, fileNameWide, MAX_PATH)) {
		return FileSystemResultCode::Unknown;
	}

    FILE *f = _wfopen(fileNameWide, L"wb");
    if (!f) {
        CO_ERR("Error opening file %s", fileName);
        switch (errno) {
        case EACCES: 
        case EPERM:
            return FileSystemResultCode::PermissionDenied;
        default:
            return FileSystemResultCode::Unknown;
        }
    }

    if (fwrite(data, (size_t)size, 1, f) != 1) {
        CO_ERR("Error writing %zd bytes to file %s", size, fileName);
        if (ferror(f)) {
           switch (errno) {
           case ENOSPC: return FileSystemResultCode::OutOfSpace;
           case EIO: return FileSystemResultCode::IOError;
           default: return FileSystemResultCode::Unknown;
           }
        }
        else if (size == 0) {
            return FileSystemResultCode::OK;
        }
        else {
            return FileSystemResultCode::Unknown;
        }
    }
    fclose(f);
    
    return FileSystemResultCode::OK;
}
bool getFilePathInHomeDir(const char *pathRelativeToHome, char *outFilePath) {
    char *homeDrive = getenv("HOMEDRIVE");
    if (!homeDrive) {
        CO_ERR("Failed to find HOMEDRIVE env var");
        return false;
    }
    char *homeDir = getenv("HOMEPATH");
    if (!homeDrive) {
        CO_ERR("Failed to find HOMEPATH env var");
        return false;
    }
	int homeDriveLen = stringLength(homeDrive);
    int homeDirLen = stringLength(homeDir);
    int pathRelativeToHomeLen = stringLength(pathRelativeToHome);
    int totalLen = homeDriveLen + homeDirLen + pathRelativeToHomeLen + 1; //+1 for slash

    if (totalLen >  MAX_PATH_LEN) {
        CO_ERR("HOME directory path is too long");
        return false; 
    }
	snprintf(outFilePath, MAX_PATH_LEN, "%s%s\\%s", homeDrive, homeDir, pathRelativeToHome);
    return true;
}
void getCurrentWorkingDirectory(char *outFilePath) {
	wchar_t widePath[MAX_PATH + 1];
    _wgetcwd(widePath, MAX_PATH);
	convertWCharToUTF8(widePath, outFilePath, MAX_PATH_LEN);
}
FileSystemResultCode createDir(const char *path) {
	wchar_t widePath[MAX_PATH + 1];
	if (!convertUTF8ToWChar(path, widePath, MAX_PATH)) {
		CO_ERR("Could not convert path to unicode. Error: %lu", GetLastError());
		return FileSystemResultCode::Unknown;
	}
	int code = _wmkdir(widePath);

	if (code == 0) {
		return FileSystemResultCode::OK;
	}
	else {
		switch (errno) {
		case EEXIST:
			return FileSystemResultCode::AlreadyExists;
		case EACCES:
			return FileSystemResultCode::PermissionDenied;
		case EIO:
			return FileSystemResultCode::IOError;
		default:
			return FileSystemResultCode::Unknown;
		}
	}

}
bool doesDirectoryExistAndIsWritable(const char *path) {
	wchar_t widePath[MAX_PATH + 1];
	if (!convertUTF8ToWChar(path, widePath, MAX_PATH)) {
		CO_ERR("Could not convert path to unicode. Error: %lu", GetLastError());
		return false;
	}
	return _waccess(widePath, 2) == 0;
}

FileSystemResultCode changeDir(const char *path) {
	wchar_t widePath[MAX_PATH + 1];
	if (!convertUTF8ToWChar(path, widePath, MAX_PATH)) {
		CO_ERR("Could not convert path to unicode. Error: %lu", GetLastError());
		return FileSystemResultCode::Unknown;
	}
    int ret = _wchdir(widePath);
    if (ret != 0) {
        CO_ERR("Could not change directory to %s. Error Code: %d", path, errno);
        switch (errno) {
        case EACCES:
            return FileSystemResultCode::PermissionDenied; 
        case EIO:
            return FileSystemResultCode::IOError;
         default:
            return FileSystemResultCode::Unknown;
        }
    }
    return FileSystemResultCode::OK;
}
MemoryMappedFileHandle *mapFileToMemory(const char *fileName, u8 **outData, usize *outLen) {
	wchar_t widePath[MAX_PATH + 1];
	if (!convertUTF8ToWChar(fileName, widePath, MAX_PATH)) {
		CO_ERR("Could not convert path to unicode. Error: %lu", GetLastError());
		return nullptr;
	}
    HANDLE fileHandle = CreateFileW(widePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!fileHandle) {
        return nullptr;
    }

    LARGE_INTEGER fileSize;
    if (GetFileSizeEx(fileHandle, &fileSize) == 0) {
        CloseHandle(fileHandle);
        return nullptr;
    }

    HANDLE mappingObject = CreateFileMapping(fileHandle, nullptr, PAGE_READWRITE, 0, 0, nullptr);
    if (!mappingObject) {
        CloseHandle(fileHandle);
        return nullptr;
    }

    void *fileMap = MapViewOfFile(mappingObject, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!fileMap) {
        CloseHandle(mappingObject);
        CloseHandle(fileHandle);
        return nullptr;
    }

    MemoryMappedFileHandle *ret = CO_MALLOC(1, MemoryMappedFileHandle);
    ret->fileHandle = fileHandle;
    ret->mappingObject = mappingObject;
    ret->data = (u8*)fileMap;
    ret->len = fileSize.QuadPart;

    *outData = (u8*)fileMap;
    *outLen = (usize)fileSize.QuadPart;

    return ret;
}
void closeMemoryMappedFile(MemoryMappedFileHandle *mmfh) {
    if (mmfh) {
        UnmapViewOfFile(mmfh->data);
        CloseHandle(mmfh->mappingObject);
        CloseHandle(mmfh->fileHandle);
        CO_FREE(mmfh);
    }
}

#else 
#error "Unsupported platform!"
#endif


#endif

