//Copyright (C) 2018 Daniel Bokser.  See LICENSE.txt for license

#pragma once
#include "gbemu.h"

#define DEBUG_WINDOW_MIN_HEIGHT 800
#define DEBUG_WINDOW_MIN_WIDTH 800
constexpr int SCALED_TILE_HEIGHT = TILE_HEIGHT * DEFAULT_SCREEN_SCALE;
constexpr int SCALED_TILE_WIDTH = TILE_WIDTH * DEFAULT_SCREEN_SCALE;


struct DebuggerPlatformContext;
struct SDL_Window;

DebuggerPlatformContext *initDebugger(GameBoyDebug *gbDebug, ProgramState *programState, SDL_Window **outDebuggerWindow, 
                                      int mainScreenX, int mainScreenY);
void closeDebugger(DebuggerPlatformContext *debuggerContext);
void signalRenderDebugger(DebuggerPlatformContext *platformContext);
void newDebuggerFrame(DebuggerPlatformContext *platformContext);

enum class BreakpointOP {
  Equal, LessThan, GreaterThan
};

enum class BreakpointType {
   Regular = 0, Register, Hardware
};

enum class BreakpointExpectedValueType {
  None = 0, Any, Custom, BitSet, BitClear, OnePastLast  
};
struct Breakpoint {
    BreakpointType type;
    bool isUsed;
    bool isDisabled;
    BreakpointOP op;

    union {
        u16 address;
        char reg[3];
        struct {
            u16 addressStart, addressEnd;
        };
    };
    
    //fields only for HW and register bps
    BreakpointExpectedValueType expectedValueType;
    u16 valueBefore;
    u16 valueAfter;
    
    u16 expectedValue;
};

struct GameBoyDebug {
    Breakpoint breakpoints[16];
    Breakpoint *hitBreakpoint;
    bool isEnabled;
    bool isTypingInTextBox;
    
    bool isDisassemblerOpen;
    bool isMemoryViewOpen;
    bool isSoundViewOpen;
    bool isOAMViewOpen;
    bool isBreakpointsViewOpen;
    bool isBackgroundTileMapOpen;
#ifdef CO_PROFILE
    bool isProfilerOpen;
    char lastFileNameWritten[MAX_PATH_LEN];
#endif
    
    bool shouldDisableSQ1;
    bool shouldDisableSQ2;
    bool shouldDisableWave;
    bool shouldDisableNoise;

    i64 numBreakpoints;
    i64 debugNumPreviousSoundSamples;

    u16 disassembledInstructionAddresses[MAX_INSTRUCTIONS];
    char disassembledInstructions[MAX_INSTRUCTIONS][MAX_INSTRUCTION_LEN];
    char disassemblerSearchString[MAX_INSTRUCTION_LEN];
    bool shouldHighlightSearch;
    bool didSearchFail;
    i64 lineNumberOfHighlightedSearch;
    i64 numDisassembledInstructions;
    bool shouldRefreshDisassembler;
    bool wasCPUPaused;
    
    i64 nextFreeGBStateIndex;
    isize numGBStates;
    TimeUS elapsedTimeSinceLastRecord;
    double frameTimeMS;
    GameBoyState recordedGBStates[60];
    
    GameBoyState prevGBDebugStates[60]; //used for debugging
    int numDebugStates;
    i64 currentDebugState;
    bool isRecordDebugStateEnabled;
    
    struct Tile {
        bool needsUpdate;
        ColorID pixels[64];
        void *textureID;
    }; 
    Tile tiles[0x200]; 
    
    //Input
    char inputText[32]; //32 is based on SDL
    char *nextTextPos;
    int key;
    bool isKeyDown;
    bool isShiftDown;
    bool isCtrlDown;
    bool isAltDown;
    bool isSuperDown;
    
    int mouseX;
    int mouseY;
    int mouseScrollY;
    bool mouseDownState[3];
    bool isWindowInFocus;
};
Breakpoint *hardwareBreakpointForAddress(u16 address, BreakpointExpectedValueType expectedValueType, GameBoyDebug *gbDebug);
void continueFromBreakPoint(GameBoyDebug *gbDebug, MMU *mmu, CPU *cpu, ProgramState *programState);
