//Copyright (C) 2018 Daniel Bokser.  See LICENSE.txt for license

#pragma once
#include "common.h"
enum class ParserStatus {
    OK = 0,
    BadRHSToken,
    BadTokenStartOfLine,
    UnknownLHSConfig,
    NumberKeyMappingNotAllowed,
    MissingEquals,
    ExtraneousToken,
    UnrecognizedKeyMapping,
    UnrecognizedControllerMapping,
    UnexpectedNewLine
};

enum class LHSValue{
    Up, Down, Left, Right,
    A, B, Start, Select, Rewind,
    Step, Continue, Mute,
    ScreenScale, Pause, ShowDebugger,
    Reset, ShowHomePath, FullScreen
};

//non null terminated
struct ConfigString {
    char *data;
    i64 len; 
};
struct LHS {
    LHSValue value; 
    int posInLine;
    int line;
};

enum class RHSType {
    ControllerMapping, KeyMapping, Integer
};

enum class KeyMappingType {
    Character, MovementKey
};
enum class MovementKeyMappingValue {
    Enter=0, Backspace, Tab, SpaceBar,
    Up, Down, Left, Right
};

enum class ControllerMappingValue {
    A=0, B, X, Y, Start, Select, LeftBumper, RightBumper, 
    LeftTrigger, RightTrigger, Up, Down, Left, Right, Home
};
struct KeyMapping {
    KeyMappingType type; 
    union {
        MovementKeyMappingValue movementKeyValue;
        int intValue;
        int characterValue;
    };
    bool isCtrlHeld;
    int posInLine;
    int line;
};
struct ControllerMapping {
   ControllerMappingValue value; 
   int posInLine;
   int line;
};

struct RHS {
    RHSType rhsType;
    union {
       KeyMapping keyMapping;
       ControllerMapping controllerMapping;
       int intValue; 
    };
    int posInLine;
    int line;
};

struct ConfigPair {
  LHS lhs;
  RHS rhs;  
};
struct ParserResult {
    //check this first
    FileSystemResultCode fsResultCode;
    ParserStatus status;
    
    const char *errorLine;
    isize errorLineLen;
    int errorLineNumber;
    int errorColumn;
    
    //These 2 members are not applicable to ParserStatus::UnexpectedNewLine
    char *errorToken;
    //character after the end
    char *stringAfterErrorToken;
    
    ConfigPair *configPairs;
    i64 numConfigPairs;
    
    //internal
    char *stream;
};

ParserResult parseConfigFile(const char *fileName);
void freeParserResult(ParserResult *parserResult);
