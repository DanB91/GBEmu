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
    UnexpectedNewLine,
    MissingComma
};

enum class ConfigKeyType{
    Up, Down, Left, Right,
    A, B, Start, Select, Rewind,
    Step, Continue, Mute,
    ScreenScale, Pause, ShowDebugger,
    Reset, ShowHomePath, FullScreen,
};

struct NonNullTerminatedString {
    char *data;
    i64 len; 
};
struct ConfigKey {
    NonNullTerminatedString textFromFile;
    ConfigKeyType type; 
    int posInLine;
    int line;
};

enum class ConfigValueType {
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
    A=0, B, X, Y, Start, Back, LeftBumper, RightBumper, 
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

struct ConfigValue {
    ConfigValueType type;
    NonNullTerminatedString textFromFile;
    union {
       KeyMapping keyMapping;
       ControllerMapping controllerMapping;
       int intValue; 
    };
    int posInLine;
    int line;
};

struct ConfigPair {
  ConfigKey key;
  ConfigValue *values;  
  isize numValues;
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
