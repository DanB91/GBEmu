//Copyright (C) 2018 Daniel Bokser.  See LICENSE.txt for license

#ifndef GBEMU_H
#define GBEMU_H

#include "common.h"

#ifndef MAC
# define MT_RENDER //OpenGL mutli-threaded render is broken on macOS Mojave
#endif

#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#include "3rdparty/imgui.h"

#define RTC_TICKS_PER_SECOND 32768
#define CYCLES_PER_RTC_TICK (CLOCK_SPEED_HZ / RTC_TICKS_PER_SECOND)

#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT 144
#define DEFAULT_SCREEN_SCALE 4

#define TILE_WIDTH  8
#define TILE_HEIGHT  8

#define PALETTE_LEN 4
#define CLOCK_SPEED_HZ 4194304

#define MAX_ROM_NAME_LEN 16

#define BYTES_PER_TILE_ROW  2
#define BYTES_PER_TILE  16

#define TILE_MAP_WIDTH  32
#define TILE_MAP_HEIGHT  32
#define MAX_SPRITES_PER_SCANLINE  10

#define TALL_SPRITE_HEIGHT  16
#define SHORT_SPRITE_HEIGHT  8

#define MAX_SPRITE_HEIGHT  16
#define MAX_SPRITE_WIDTH  8

#define GENERAL_MEMORY_SIZE MB(50)
#define FILE_MEMORY_SIZE MB(10)

#define FRAME_SEQUENCER_PERIOD (CLOCK_SPEED_HZ/512)
#define LENGTH_TIMER_PERIOD (512/256)
#define SWEEP_TIMER_PERIOD (512/128)
#define VOLUME_ENVELOPE_TIMER_PERIOD (512/64)

#define NUM_US_BETWEEN_RECORDS 5000000

#define MAX_NOTIFICATION_LEN 127
#define MAX_NOTIFICATIONS 10
#define MAX_NOTIFICATION_TIME_US 2000000

#define MAX_INSTRUCTIONS 0x10000
#define MAX_INSTRUCTION_LEN 32

#define NUM_SAVE_SLOTS 10

struct PlatformState {
//To be inherited from    
};

struct RTC;
struct RTCFileState;

struct CartRAMPlatformState {
    u8 *cartRAMFileMap;
    usize ramLen;
    MemoryMappedFileHandle *cartRAMFileHandle;
    RTCFileState *rtcFileMap;
};

enum class Flag {
    Z = 0x80,
    N = 0x40,
    H = 0x20,
    C = 0x10
};

enum class MBCType : i32 {
    MBC0,
    MBC1,
    //TODO: MBC2
    MBC3,
    MBC5
};
enum class BankingMode : i32 {
    Mode0 = 0, //8kbyte ram; 2MB rom
    Mode1 = 1  //32kbyte ram; 512kb ROM
};

enum class LCDMode : i32 {
    HBlank = 0,
    VBlank = 1, 
    ScanOAM = 2,
    ScanVRAMAndOAM = 3
};

enum class SpriteHeight : i32 {
    Short = 8,
    Tall = 16
};

enum class PaletteColor : i32 {
    White = 0, LightGray, DarkGray, Black
};

enum class InterruptRequestedBit {
    VBlankRequested = 0,
    LCDRequested = 1,
    TimerRequested = 2
};

enum class LCDCBit {
    LY_LYCCoincidenceInterrupt = 6,
    OAMInterrupt = 5,
    VBlankInterrupt = 4,
    HBlankInterrupt = 3,
    LY_LYCCoincidenceOccurred  = 2
};

enum class TimerIncrementRate : i32 {
    TIR_0 = 1024,
    TIR_1 = 16,
    TIR_2 = 64,
    TIR_3 = 256
};

enum class ColorID {
    Color0 = 0,
    Color1 = 1,
    Color2 = 2,
    Color3 = 3
};

enum class SpritePalette {
    Palette0 = 0,
    Palette1 = 1
};

enum class JPButtonState : i32 {
    Down = 0,
    Up = 1
};

enum class JPButtonGroup : i32 {
    FaceButtons, //Bit 4
    DPad,        //Bit 5 
    Nothing      //Bit 4 and 5 both turned off or on at the same time 
};

struct SoundState {
    int volume; //0 to 100
    bool isMuted;
};


struct JoyPad {  
    //Bits:       3      2       1  0
    JPButtonState start, select, b, a;

    //Bits:       3      2  1      0
    JPButtonState down, up, left, right;

    JPButtonGroup selectedButtonGroup;
};

struct Sprite {
    u8 x, y;
    u8 tileReference;

    union {
        struct {
            u8 _unused: 4;
            //all need to be unsinged so fields don't reverse on windows
            unsigned char selectedSpritePalette: 1;
            unsigned char isXFlipped: 1;
            unsigned char isYFlipped: 1;
            unsigned char isBelowBackground: 1;
        };

        u8 flags;
    };
    
    bool isLowPriority;
};
struct LCD {
    PaletteColor backgroundPalette[PALETTE_LEN];
    PaletteColor spritePalette0[PALETTE_LEN];
    PaletteColor spritePalette1[PALETTE_LEN];
    u8 videoRAM[0x2000];
    
    //TODO: hashmap of addresses to sprites
    
    u8 oam[0xA0]; //sprite memory
    LCDMode mode;
    i32 modeClock;
    SpriteHeight spriteHeight;
    bool isBackgroundEnabled;
    bool isWindowEnabled;
    bool isEnabled;
    bool isOAMEnabled;

    u8 scx; //scroll x
    u8 scy; //scroll y
    u8 wx; //window x
    u8 wy; //window y
    u8 ly; //curr scan line
    u8 stat;
    u8 lyc; //used to compare to ly
    u8 backgroundTileMap;  //which background tile map to use (0 or 1)
    u8 backgroundTileSet;  //which background tile set to use (0 or 1)
    u8 windowTileMap;

    i32 numScreensToSkip;

    PaletteColor *screen;
    PaletteColor *backBuffer;
    
    PaletteColor screenStorage[SCREEN_WIDTH*SCREEN_HEIGHT];
    PaletteColor backBufferStorage[SCREEN_WIDTH*SCREEN_HEIGHT];
    
};

struct RTC {
    u8 latchState;
    bool isStopped;
    bool didOverflow;
    i64 wallClockTime;
    
    u8 seconds, minutes, hours;
    u8 days, daysHigh;
    u8 latchedSeconds, latchedMinutes, latchedHours, latchedDays, latchedMisc;
};

#pragma pack(push, 4)
struct RTCFileState {
    u32 seconds, minutes, hours, days, daysHigh;
    u32 latchedSeconds, latchedMinutes, latchedHours, latchedDays, latchedDaysHigh;
    i64 unixTimestamp;
};
#pragma pack(pop)

enum class WaveForm : i32 {
    WF_12Pct = 1,
    WF_25Pct = 0x81,
    WF_50Pct = 0x87,
    WF_75Pct = 0x7E
};

enum class ChannelEnabledState : i32 {
    None = 0,
    Right = 1,
    Left = 2,
    Both = 3
};
enum class WaveVolumeShift : i32 {
    WV_0,
    WV_25,
    WV_50,
    WV_100,
};


struct MMU {
    struct SquareWave1 {
          //NR10 FF10 -PPP NSSS Sweep period, negate, shift
          //NR11 FF11 DDLL LLLL Duty, Length load (64-L)
          //NR12 FF12 VVVV APPP Starting volume, Envelope add mode, envelope period
          //NR13 FF13 FFFF FFFF Frequency LSB
          //NR14 FF14 TL-- -FFF Trigger, Length enable, Frequency MSB
       
        u8 sweepPeriod; //7 bits
        u8 sweepShift; //7 bits
        bool isSweepNegated;

        WaveForm waveForm;
        u8 positionInWaveForm;
        u8 lengthCounter; //6 bits
        bool isLengthCounterEnabled;

        u8 startingVolume, currentVolume; //4 bits
        bool shouldIncreaseVolume;
        u8 volumeEnvelopPeriod;
        
        u16 toneFrequency, tonePeriod; //12 bits
        bool isEnabled;
        bool isDACEnabled;

        ChannelEnabledState channelEnabledState;

        i32 frequencyClock, volumeEnvelopeClock, sweepClock;
        u16 sweepShadowReg;
        bool isSweepEnabled;
    };
    
    struct SquareWave2 {
          //NR20 FF15 unused
          //NR21 FF16 DDLL LLLL Duty, Length load (64-L)
          //NR22 FF17 VVVV APPP Starting volume, Envelope add mode, envelope period
          //NR23 FF18 FFFF FFFF Frequency LSB
          //NR24 FF19 TL-- -FFF Trigger, Length enable, Frequency MSB
       
        WaveForm waveForm;
        u8 positionInWaveForm;
        u8 lengthCounter; //6 bits
        bool isLengthCounterEnabled;

        u8 startingVolume, currentVolume; //4 bits
        bool shouldIncreaseVolume;
        u8 volumeEnvelopPeriod;
        
        u16 toneFrequency, tonePeriod; //12 bits
        bool isEnabled;
        bool isDACEnabled;

        ChannelEnabledState channelEnabledState;

        int frequencyClock, volumeEnvelopeClock;
    };
    
    struct Wave {
        //NR30 FF1A E--- ---- DAC power
        //NR31 FF1B LLLL LLLL Length load (256-L)
        //NR32 FF1C -VV- ---- Volume code (00=0%, 01=100%, 10=50%, 11=25%)
        //NR33 FF1D FFFF FFFF Frequency LSB
        //NR34 FF1E TL-- -FFF Trigger, Length enable, Frequency MSB
        
        u16 lengthCounter; 
        bool isLengthCounterEnabled;
        
        u16 toneFrequency, tonePeriod; //12 bits
        
        bool isEnabled;
        bool isDACEnabled;
        
        u8 currentSample;
        u8 currentSampleIndex;
        
        WaveVolumeShift volumeShift;
        
        int frequencyClock;
        
        u8 waveTable[16];
        
        ChannelEnabledState channelEnabledState;
    };
    
    struct Noise {
        //NR41 FF20 --LL LLLL Length load (64-L)
        //NR42 FF21 VVVV APPP Starting volume, Envelope add mode, period
        //NR43 FF22 SSSS WDDD Clock shift, Width mode of LFSR, Divisor code
        //NR44 FF23 TL-- ---- Trigger, Length enable 
        
        u16 lengthCounter; 
        bool isLengthCounterEnabled;
        
        u8 startingVolume, currentVolume; //4 bits
        bool shouldIncreaseVolume;
        u8 volumeEnvelopPeriod;
        i32 volumeEnvelopeClock;
        
        bool isEnabled;
        bool isDACEnabled;
        
        u8 divisorCode;
        i32 tonePeriod; 
        u16 shiftValue; 
        bool is7BitMode;
        i32 frequencyClock;
        
        ChannelEnabledState channelEnabledState;
        
    };
         
    
    SoundBuffer soundFramesBuffer; 
    
    u8 workingRAM[0x2000];
    u8 zeroPageRAM[0x7F];

    LCD lcd;
    JoyPad joyPad;

    //Cart data
    u8 *romData;
    i64 romSize;
    u16 currentROMBank;
    char *romName;
    i64 romNameLen;
    i32 maxROMBank;
    
    MBCType mbcType;
    u8 *cartRAM;
    i64 cartRAMSize;
    i32 maxCartRAMBank;
    
    //TODO: Contains platform specific data.  Move?
    CartRAMPlatformState cartRAMPlatformState;
    
    u8 currentRAMBank;
    bool isCartRAMEnabled;
    bool hasRAM;
    
    bool hasBattery;
    
    u8 requestedInterrupts;
    u8 enabledInterrupts;
    BankingMode bankingMode;
    
    bool hasRTC;
    RTC rtc;

    //DMA
    bool isDMAOccurring;
    u16 currentDMAAddress;
    int cyclesSinceLastDMACopy;

    //divider
    int cyclesSinceDividerIncrement;
    u8 divider;

    //timer
    int cyclesSinceTimerIncrement;
    TimerIncrementRate timerIncrementRate;
    u8 timer;
    u8 timerModulo;
    bool isTimerEnabled;

    //Sound
    u8 NR10;
    u8 NR11;
    u8 NR12;
    u8 NR13;
    u8 NR14;
    u8 NR21;
    u8 NR22;
    u8 NR23;
    u8 NR24;
    u8 NR30;
    u8 NR31;
    u8 NR32;
    u8 NR33;
    u8 NR34;
    u8 NR41;
    u8 NR42;
    u8 NR43;
    u8 NR44;
    u8 NR50;
    u8 NR51;
    u8 NR52;
    bool isSoundEnabled;
    SquareWave1 squareWave1Channel;
    SquareWave2 squareWave2Channel;
    Wave waveChannel;
    Noise noiseChannel;
    i32 ticksSinceLastLengthCounter,  ticksSinceLastEnvelop, ticksSinceLastSweep;
    i32 cyclesSinceLastSoundSample, cyclesSinceLastFrameSequencer;
    i32 masterLeftVolume, masterRightVolume;
};

struct CPU {
    u16 PC, SP;
    u8 A, B, C, D; 
    u8 E, F, H, L; 
    i64 totalCycles;  //total cycles since game has been loaded
    i32 instructionCycles; //number of cycles in a given instruction
    i32 leftOverCyclesFromPreviousFrame; 
    i32 cylesExecutedThisFrame;
    bool enableInterrupts; 
    bool isHalted;
    bool isPaused;
    bool didHitIllegalOpcode;
};


struct GameBoyState {
    CPU cpu;
    MMU mmu;
};


struct NotificationState {
    char notifications[MAX_NOTIFICATIONS][MAX_NOTIFICATION_LEN + 1];
    i64 writeIndex, readIndex;
    i64 numItemsQueued;
};
struct GameBoyDebug;


#define NO_INPUT_MAPPING -1
struct Input {
    enum class Action {
        Left = 0, Right, Up, Down,
        A, B, Start, Select,

        Rewind, DebuggerContinue, DebuggerStep, Mute,
        Pause, Reset, ShowDebugger, ShowHomePath,
        FullScreen, ShowControls,

        NumActions
    };
    

    struct State {
        //emu controls
        bool saveState, restoreState, escapeFullScreen, enterPressed;
        
        bool actionsHit[(int)Action::NumActions];
        
        int slotToRestoreOrSave;
        
        //analog stick controls
        i16 xAxis, yAxis;

        //debug controls
        i32 mouseX, mouseY;
    };
    struct Mapping {
        UTF32Character code;
        Action action; 
    };
    struct MappingBucketNode {
        Mapping *mapping;
    };
    
    struct CodeToActionMap {
        //TODO: keys and values in separate arrays?
        Mapping *mappings;
        Mapping *buckets[64];
    };
    
    State newState;
    State oldState;
    
    CodeToActionMap keysMap;
    CodeToActionMap ctrlKeysMap;
    CodeToActionMap gamepadMap;

    //debug controls
};
static const char *inputActionToStr[] = {
    "Left", "Right", "Up", "Down",
    "A", "B", "Start", "Select",

    "Rewind", "DebuggerContinue", "DebuggerStep", "Mute",
    "Pause", "Reset", "ShowDebugger", "ShowHomePath",
    "FullScreen",

    "NumActions"
};

void registerInputMapping(int inputCode, Input::Action action, Input::CodeToActionMap *mappings);
bool retrieveActionForInputCode(int inputCode, Input::Action *outAction, Input::CodeToActionMap *mappings);

struct ProgramState {
    char loadedROMName[MAX_ROM_NAME_LEN + 1];
    char homeDirectoryPath[MAX_PATH_LEN + 1];
    char romSpecificPath[MAX_PATH_LEN + 1];
    SoundState soundState;
#ifdef CO_PROFILE
    ProfileState profileState;
#endif
                      
    bool isFullScreen;
    Input input; 
    NotificationState notifications;
    bool shouldUpdateTitleBar;
    MemoryStack fileMemory;
    void *guiContext; 
    
    int screenScale;
};
inline u8 lb(u16 word) {
    return (u8)(word & 0xFF);
}

inline u8 hb(u16 word) {
    return (u8)((word >> 8) & 0xFF);
}

inline u16 word(u8 h, u8 l) {
    u16 ret = (u16)((u16)h << 8) | (u16)l;
    return ret;
}

inline bool isActionDown(Input::Action action, const Input *input) {
   return input->newState.actionsHit[(int)action];
}
inline bool isActionPressed(Input::Action action, const Input *input) {
   return input->newState.actionsHit[(int)action] && !input->oldState.actionsHit[(int)action];
}

bool pushNotification(const char *notification, int len, NotificationState *buffer);
bool popNotification(NotificationState *buffer, char *outNotification, int len = MAX_NOTIFICATION_LEN);
void syncRTCTime(RTC *rtc, RTCFileState *rtcFS);
i32 calculateMaxBank(i64 size);
inline void setPausedState(bool isPaused, ProgramState *programState, CPU *cpu) {
    programState->shouldUpdateTitleBar = true;
    cpu->isPaused = isPaused;
}

u8 readByte(u16 address, MMU *mmu);
u16 readWord(u16 address, MMU *mmu);
void writeByte(u8 byte, u16 address, MMU *mmu, GameBoyDebug *gbDebug);
void writeWord(u16 word, u16 address, MMU *mmu, GameBoyDebug *gbDebug);
void step(CPU *cpu, MMU* mmu, GameBoyDebug *gbDebug, int volume);
    
#ifdef CO_DEBUG
    extern "C"
#endif
void reset(CPU *cpu, MMU *mmu, GameBoyDebug *gbDebug, ProgramState *programState);

#define NOTIFY(buffer, fmt, ...) do {\
    char *str = nullptr;\
    buf_gen_memory_printf(str, fmt, ##__VA_ARGS__);\
    pushNotification(str, stringLength(str) + 1, buffer);\
    buf_gen_memory_free(str);\
    } while (0)


#endif
//shared implementation (to be moved into gbemu.cpp once we get rid of dynamic linking)


#ifdef GB_IMPL
#undef GB_IMPL
void registerInputMapping(int inputCode, Input::Action action, Input::CodeToActionMap *mappings) {
    u64 index = hashU64((u64)inputCode) & (ARRAY_LEN(mappings->buckets) - 1);
    Input::Mapping mapping;
    mapping.code = inputCode;
    mapping.action = action;
    
    Input::Mapping *bucket = mappings->buckets[index];
    //modify existing
    foribuf (bucket) {
        if (inputCode == bucket[i].code) {
            bucket[i].action = action;
            foribuf (mappings->mappings) {
                if (inputCode == bucket[i].code) {
                    bucket[i].action = action;
                }
            }
            return;
        }
    }

    buf_malloc_push(mappings->mappings, mapping);
    buf_malloc_push(bucket, mapping);
    mappings->buckets[index] = bucket;
}
bool retrieveActionForInputCode(int inputCode, Input::Action *outAction, Input::CodeToActionMap *mappings) {
    u64 index = hashU64((u64)inputCode) & (ARRAY_LEN(mappings->buckets) - 1);
    Input::Mapping *bucket = mappings->buckets[index];
    if (bucket) {
        foribuf (bucket) {
           if (bucket[i].code == inputCode) {
               if (outAction) {
                   *outAction = bucket[i].action;
               }
               return true;
           }
        }
    }
    
    return false;
}
bool pushNotification(const char *notification, int len, NotificationState *buffer) {

    if (buffer->numItemsQueued < MAX_NOTIFICATIONS) {
        len = (len > MAX_NOTIFICATION_LEN) ? MAX_NOTIFICATION_LEN : len;
        copyMemory(notification, buffer->notifications[buffer->writeIndex], 
                len);
        buffer->notifications[buffer->writeIndex++][len] = '\0';
        

        if (buffer->writeIndex >= MAX_NOTIFICATIONS) {
            buffer->writeIndex = 0;
        }
            
        buffer->numItemsQueued++;
        return true;
    }
    else {
        return false;
    }
}

bool popNotification(NotificationState *buffer, char *outNotification, int len) {
    if (buffer->numItemsQueued <= 0) {
        return false;
    }
    
    strncpy(outNotification, buffer->notifications[buffer->readIndex], 
             (len > MAX_NOTIFICATION_LEN) ? MAX_NOTIFICATION_LEN : (usize)len);
    buffer->readIndex++;
    buffer->readIndex %= MAX_NOTIFICATIONS;

    buffer->numItemsQueued--;

    return true;
}

void syncRTCTime(RTC *rtc, RTCFileState *rtcFS) {
    i64 tmpWallClockTime = unixWallClockTime();
    i64 timeToAdd = tmpWallClockTime - rtcFS->unixTimestamp;
    if (timeToAdd > 0) {
        rtc->wallClockTime = rtcFS->unixTimestamp = tmpWallClockTime;
    }
    if (timeToAdd > 0 && !rtc->isStopped) {
        i64 totalSeconds = rtcFS->seconds + timeToAdd;
        rtc->seconds = (u8)(rtcFS->seconds = totalSeconds % 60);
        if (totalSeconds >= 60) {
            i64 totalMinutes = rtcFS->minutes + (totalSeconds / 60);
            rtc->minutes = (u8)(rtcFS->minutes = totalMinutes % 60);
            if (totalMinutes >= 60) {
                i64 totalHours = rtcFS->hours + (totalMinutes / 60);
                rtc->hours = (u8)(rtcFS->hours = totalHours % 24);
                if (totalHours >= 24) {
                    i64 totalDays = ((rtcFS->daysHigh << 8) | rtcFS->days) + (totalHours / 24);
                    rtc->days = rtcFS->days = totalDays & 0xFF;
                    if (totalDays >= 0x100) {
                        rtc->daysHigh = (u8)(rtcFS->daysHigh = ((totalDays & 0x100) != 0) ? 1 : 0);
                        rtc->didOverflow = totalDays > 0x1FF;
                    }
                }
            }
        }
    }
}
i32 calculateMaxBank(i64 size) {
    i32 maxCartRAMBank = (i32)((size / KB(8)) - 1);
    if (maxCartRAMBank < 0) maxCartRAMBank = 0;
    return maxCartRAMBank;
}
#endif
