#pragma once
#include "common.h"
enum class ParserStatus {
    OK = 0,
    BadRHSToken,
    BadLHSToken,
    UnknownLHSConfig,
    BadMappingToken,
    MissingEquals,
    UnexpectedToken
};

enum class LHSValue{
    Up, Down, Left, Right,
    A, B, Start, Select, Rewind,
    Step, Continue, Mute,
    Scale
};

struct String {
    char *data;
    i64 len; 
};
struct LHS {
    LHSValue value; 
};


enum class RHSType {
    ControllerMapping, KeyMapping, RawValue
};
enum class RHSValueType {
    Identifier, Integer, Character,
};

struct RHS {
    RHSType rhsType;
    RHSValueType valueType;
    union {
        String identValue;
        int intValue;
        int characterValue;
    };
};

struct ConfigPair {
  LHS lhs;
  RHS rhs;  
};
struct ParserResult {
    //check this first
    FileSystemResultCode fsResultCode;
    int errorLine;
    int errorColumn;
    ParserStatus status;
    ConfigPair *configPairs;
    i64 numConfigPairs;
    
    //internal
    char *stream;
};

ParserResult parseConfigFile(const char *fileName);
void freeParserResult(ParserResult *parserResult);
