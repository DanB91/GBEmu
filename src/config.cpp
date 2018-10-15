//Copyright (C) 2018 Daniel Bokser.  See LICENSE.txt for license

#include "config.h"
#define CMP_STR(stringLiteral) areStringsEqualCaseInsenstive(currentToken.stringValue.data, stringLiteral, currentToken.stringValue.len, sizeof(stringLiteral) - 1) 

enum class ConfigTokenType {
    Begin, Identifier, Integer,
    Character, KeyKW, ControllerKW,
    CtrlKW, Comma,
    Equals, NewLine, End
};

struct ConfigToken {
    NonNullTerminatedString stringValue;
    int posInLine;
    int line;
    
    ConfigTokenType type;
    union {
      UTF8Character charValue; 
      int intValue;
    };
};

globalvar ConfigToken currentToken;
globalvar char *stream;
globalvar int currentLineNumber = 1;
globalvar int currentPosInLine = 1;
globalvar char *currentLine;
globalvar char *previousLine;

static inline bool isIdentifierChar(char c) {
    return isalpha(c) || c == '_';
}
static inline bool isNewLineChar(char c) {
    switch (c) {
        case '\r':
        case '\n':
        return true;
    }
    
    return false;
}

static bool areStringsEqualCaseInsenstive(const char *l, const char *r, i64 ln, i64 rn) {
    if (ln != rn) {
        return false;
    }
    i64 n  = ln;
    for (int i = 0; i < n && *l && *r ;l++,r++, i++) {
        if (tolower(*l) != tolower(*r)) {
            return false;
        }
    }
    return true;
}


static void nextToken() {
repeat:
    currentToken.line = currentLineNumber;
    currentToken.posInLine = currentPosInLine;
    
    if (*stream == '\0') {
        currentToken.type = ConfigTokenType::End; 
    }
    else if (isblank(*stream)) {
        while (*stream != '\0' && isblank(*stream)) {
            stream++;
            currentPosInLine++;
        }
        goto repeat;
    }
    else if (isNewLineChar(*stream)) {
        currentToken.type = ConfigTokenType::NewLine;
        currentToken.stringValue.data = stream;
        currentToken.stringValue.len = 1;
        while (*stream != '\0' && isspace(*stream))  {
            switch (*stream) {
            case '\n':
                stream++; 
                currentPosInLine = 1;
                currentLineNumber++;
                previousLine = currentLine;
                currentLine = stream;
                break;
            case '\r':
                stream++;
                if (*stream == '\n') {
                    stream++;
                }
                currentPosInLine = 1;
                currentLineNumber++;
                previousLine = currentLine;
                currentLine = stream;
                break;
            default:
                stream++;
                currentPosInLine++;
            }
        }
    }
    else if (stream[0] == '/' && stream[1] == '/') { //comment
        while (*stream != '\0' && !isNewLineChar(*stream)) {
            stream++;
            currentPosInLine++;
        }
        goto repeat;
    }
    else if (isdigit(*stream)) {
        currentToken.type = ConfigTokenType::Integer;
        currentToken.stringValue.data = stream;
        while (*stream != '\0' &&
               isdigit(*stream)) {
            stream++;
            currentPosInLine++;
        }
        currentToken.stringValue.len = stream - currentToken.stringValue.data ;
        {
            char tmp = *stream;
            *stream = '\0';
            currentToken.intValue = atoi(currentToken.stringValue.data);
            *stream = tmp;
        }
    }
    else if (tolower(stream[0]) == 'k' && tolower(stream[1]) == 'e' && stream[2] == 'y') {
       currentToken.type = ConfigTokenType::KeyKW;
       currentToken.stringValue.data = stream;
       currentToken.stringValue.len = 3;
       stream += 3;
       currentPosInLine += 3;
    }
    else if (tolower(stream[0]) == 'c' && tolower(stream[1]) == 'o' && tolower(stream[2]) == 'n' && 
             tolower(stream[3]) == 't' && tolower(stream[4]) == 'r' && tolower(stream[5]) == 'o' &&
             tolower(stream[6]) == 'l' && tolower(stream[7]) == 'l' && tolower(stream[8]) == 'e' && tolower(stream[9]) == 'r') {
       currentToken.type = ConfigTokenType::ControllerKW;
       currentToken.stringValue.data = stream;
       currentToken.stringValue.len = 10;
       stream += 10 ;
       currentPosInLine += 10;
    }
    else if (tolower(stream[0]) == 'c' && tolower(stream[1]) == 'o' && tolower(stream[2]) == 'm' && 
             tolower(stream[3]) == 'm' && tolower(stream[4]) == 'a' && tolower(stream[5]) == 'n' &&
             tolower(stream[6]) == 'd' && tolower(stream[7]) == '-' ) {
       currentToken.type = ConfigTokenType::CtrlKW;
       currentToken.stringValue.data = stream;
       currentToken.stringValue.len = 8;
       stream += 8 ;
       currentPosInLine += 8;
                
    }
    else if (tolower(stream[0]) == 'c' && tolower(stream[1]) == 't' && tolower(stream[2]) == 'r' && 
             tolower(stream[3]) == 'l' && tolower(stream[4]) == '-') {
       currentToken.type = ConfigTokenType::CtrlKW;
       currentToken.stringValue.data = stream;
       currentToken.stringValue.len = 5;
       stream += 5;
       currentPosInLine += 5;
    }
    else if (isIdentifierChar(*stream)) {
        currentToken.type = ConfigTokenType::Identifier;
        currentToken.stringValue.data = stream;
        while (*stream != '\0' &&
               (isIdentifierChar(*stream) ||
                isdigit(*stream))) {
            stream++;
            currentPosInLine++;
        }
        currentToken.stringValue.len = stream - currentToken.stringValue.data ;
    }
    else if (*stream == '=') {
        currentToken.type = ConfigTokenType::Equals; 
        currentToken.charValue.data = '=';
        currentToken.stringValue.data = stream;
        currentToken.stringValue.len = 1;
        stream++;
        currentPosInLine++;
    }
    else if (*stream == ',') {
        currentToken.type = ConfigTokenType::Comma;
        currentToken.charValue.data = ',';
        currentToken.stringValue.data = stream;
        currentToken.stringValue.len = 1;
        stream++;
        currentPosInLine++;
    }
    else {
        u8 leadByte = (u8)*stream;
        currentToken.stringValue.data = stream;
        int expectedNumBytes;
        switch (leadByte & 0xF0) {
        case 0: expectedNumBytes = 1; 
            break;
        case 0xC0: 
        case 0xD0: 
            expectedNumBytes = 2; 
            break;
        case 0xE0:
            expectedNumBytes = 3; 
            break;
        case 0xF0: expectedNumBytes = 4;
            break;
            //TODO: bad lead byte, should assign bad token type
        }
        int byteLen = 0;
        int i;
        for (i = 0; i < expectedNumBytes; i++) {
            currentToken.charValue.string[i] = *stream++;
            //TODO: handle failure case with 0b11xxxxxx
            byteLen++;
            if (!isBitSet(7,(u8)*stream)) {
                break;
            }
        }
        currentToken.charValue.string[i+1] = '\0';
        //TODO: invalid byte TokenType for bad bytes
        
        currentToken.stringValue.len = byteLen;
        currentToken.type = ConfigTokenType::Character;
        currentPosInLine++;
    }
    
    
}

static bool accept(ConfigTokenType tt) {
    if (currentToken.type == tt) {
        nextToken();
        return true;
    }
    return false;
}
static inline bool isToken(ConfigTokenType tokenType) {
    return currentToken.type == tokenType;
}

static ParserStatus controllerMapping(ControllerMapping *outControllerMapping) {
    outControllerMapping->posInLine = currentPosInLine;
    outControllerMapping->line = currentLineNumber;
    switch (currentToken.type) {
    case ConfigTokenType::Identifier:  {
        if (CMP_STR("a")) {
            outControllerMapping->value = ControllerMappingValue::A;
        }
        else if (CMP_STR("b")) {
            outControllerMapping->value = ControllerMappingValue::B;
        }
        else if (CMP_STR("x")) {
            outControllerMapping->value = ControllerMappingValue::X;
        }
        else if (CMP_STR("y")) {
            outControllerMapping->value = ControllerMappingValue::Y;
        }
        else if (CMP_STR("up")) {
            outControllerMapping->value = ControllerMappingValue::Up;
        }
        else if (CMP_STR("down")) {
            outControllerMapping->value = ControllerMappingValue::Down;
        }
        else if (CMP_STR("left")) {
            outControllerMapping->value = ControllerMappingValue::Left;
        }
        else if (CMP_STR("right")) {
            outControllerMapping->value = ControllerMappingValue::Right;
        }
        else if (CMP_STR("start")) {
            outControllerMapping->value = ControllerMappingValue::Start;
        }
        else if (CMP_STR("back")) {
            outControllerMapping->value = ControllerMappingValue::Back;
        }
        else if (CMP_STR("leftbumper")) {
            outControllerMapping->value = ControllerMappingValue::LeftBumper;
        }
        else if (CMP_STR("rightbumper")) {
            outControllerMapping->value = ControllerMappingValue::RightBumper;
        }
        else if (CMP_STR("lefttrigger")) {
            outControllerMapping->value = ControllerMappingValue::LeftTrigger;
        }
        else if (CMP_STR("righttrigger")) {
            outControllerMapping->value = ControllerMappingValue::RightTrigger;
        }
        else if (CMP_STR("home")) {
            outControllerMapping->value = ControllerMappingValue::Home;
        }
        else {
            return ParserStatus::UnrecognizedControllerMapping;
        }
    } break;
    default:  {
        return ParserStatus::UnrecognizedControllerMapping; 
    } break;
    }
    
    return ParserStatus::OK;
}
static ParserStatus keyMapping(KeyMapping *outKeyMapping) {
    outKeyMapping->posInLine = currentPosInLine;
    outKeyMapping->line = currentLineNumber;
    outKeyMapping->textFromFile = currentToken.stringValue;  
    NonNullTerminatedString startToken = currentToken.stringValue;

    if (currentToken.type == ConfigTokenType::CtrlKW) {
       outKeyMapping->isCtrlHeld = true; 
       nextToken();
       isize spaceInBetween = currentToken.stringValue.data - 
                                (startToken.data + startToken.len);
       outKeyMapping->textFromFile.len += spaceInBetween + currentToken.stringValue.len;  
    }
    else {
       outKeyMapping->isCtrlHeld = false; 
    }

    switch (currentToken.type) {
    case ConfigTokenType::Equals:
    case ConfigTokenType::Comma:
    case ConfigTokenType::Character:  {
        outKeyMapping->type = KeyMappingType::Character;
        outKeyMapping->characterValue = currentToken.charValue;
    } break;
    case ConfigTokenType::Identifier:  {
        outKeyMapping->type = KeyMappingType::MovementKey;
        if (currentToken.stringValue.len == 1) {
           outKeyMapping->type = KeyMappingType::Character;
           outKeyMapping->characterValue.data = (u64)tolower(*currentToken.stringValue.data);
        }
        else if (CMP_STR("enter")) {
            outKeyMapping->movementKeyValue = MovementKeyMappingValue::Enter;
        }
        else if (CMP_STR("backspace")) {
            outKeyMapping->movementKeyValue = MovementKeyMappingValue::Backspace;
        }
        else if (CMP_STR("spacebar")) {
            outKeyMapping->movementKeyValue = MovementKeyMappingValue::SpaceBar;
        }
        else if (CMP_STR("tab")) {
            outKeyMapping->movementKeyValue = MovementKeyMappingValue::Tab;
        }
        else if (CMP_STR("left")) {
            outKeyMapping->movementKeyValue = MovementKeyMappingValue::Left;
        }
        else if (CMP_STR("right")) {
            outKeyMapping->movementKeyValue = MovementKeyMappingValue::Right;
        }
        else if (CMP_STR("up")) {
            outKeyMapping->movementKeyValue = MovementKeyMappingValue::Up;
        }
        else if (CMP_STR("down")) {
            outKeyMapping->movementKeyValue = MovementKeyMappingValue::Down;
        }
        else {
            return ParserStatus::UnrecognizedKeyMapping;
        }
    } break;
    case ConfigTokenType::Integer: {
        return ParserStatus::NumberKeyMappingNotAllowed;
    } break;
    default:  {
            return ParserStatus::UnrecognizedKeyMapping;
    } break;
    }
    
    return ParserStatus::OK;
}

static ParserStatus configValue(ConfigValue *outConfigValue) {
    outConfigValue->line = currentToken.line;
    outConfigValue->posInLine = currentToken.posInLine;

    if (isToken(ConfigTokenType::KeyKW))  {
        outConfigValue->type = ConfigValueType::KeyMapping;
        outConfigValue->textFromFile = currentToken.stringValue;
        nextToken();
        auto res = keyMapping(&outConfigValue->keyMapping);
        if (res != ParserStatus::OK) {
            return res;
        }
        outConfigValue->textFromFile.len += 
                (outConfigValue->keyMapping.textFromFile.data - (outConfigValue->textFromFile.data + outConfigValue->textFromFile.len)) + 
                outConfigValue->keyMapping.textFromFile.len;
    }
    else if (isToken(ConfigTokenType::ControllerKW)) {
        outConfigValue->type = ConfigValueType::ControllerMapping;
        outConfigValue->textFromFile = currentToken.stringValue;
        nextToken();
        outConfigValue->textFromFile.len += (currentToken.stringValue.data - outConfigValue->textFromFile.data - 
                                             outConfigValue->textFromFile.len) + currentToken.stringValue.len;
        auto res = controllerMapping(&outConfigValue->controllerMapping);
        if (res != ParserStatus::OK) {
            return res;
        }
    }
    else if (isToken(ConfigTokenType::Integer)) {
        outConfigValue->type = ConfigValueType::Integer;
        outConfigValue->intValue = currentToken.intValue;
        outConfigValue->textFromFile = currentToken.stringValue;
    }
    else {
        return ParserStatus::UnknownConfigValue;
    }
    
    nextToken();
    return ParserStatus::OK;
}

static ParserStatus configKey(ConfigKey *outConfigKey) {
    outConfigKey->line = currentToken.line;
    outConfigKey->posInLine = currentToken.posInLine;
    if (isToken(ConfigTokenType::Identifier)) {
        if (CMP_STR("up")) {
            outConfigKey->type = ConfigKeyType::Up;
        }
        else if (CMP_STR("down")) {
            outConfigKey->type = ConfigKeyType::Down;
        }
        else if (CMP_STR("left")) {
            outConfigKey->type = ConfigKeyType::Left;
        }
        else if (CMP_STR("right")) {
            outConfigKey->type = ConfigKeyType::Right;
        }
        else if (CMP_STR("a")) {
            outConfigKey->type = ConfigKeyType::A;
        }
        else if (CMP_STR("b")) {
            outConfigKey->type = ConfigKeyType::B;
        }
        else if (CMP_STR("start")) {
            outConfigKey->type = ConfigKeyType::Start;
        }
        else if (CMP_STR("select")) {
            outConfigKey->type = ConfigKeyType::Select;
        }
        else if (CMP_STR("rewind")) {
            outConfigKey->type = ConfigKeyType::Rewind;
        }
        else if (CMP_STR("step")) {
            outConfigKey->type = ConfigKeyType::Step;
        }
        else if (CMP_STR("continue")) {
            outConfigKey->type = ConfigKeyType::Continue;
        }
        else if (CMP_STR("mute")) {
            outConfigKey->type = ConfigKeyType::Mute;
        }
        else if (CMP_STR("screenscale")) {
            outConfigKey->type = ConfigKeyType::ScreenScale;
        }
        else if (CMP_STR("pause")) {
            outConfigKey->type = ConfigKeyType::Pause;
        }
        else if (CMP_STR("showdebugger")) {
            outConfigKey->type = ConfigKeyType::ShowDebugger;
        }
        else if (CMP_STR("reset")) {
            outConfigKey->type = ConfigKeyType::Reset;
        }
        else if (CMP_STR("showhomepath")) {
            outConfigKey->type = ConfigKeyType::ShowHomePath;
        }
        else if (CMP_STR("fullscreen")) {
            outConfigKey->type = ConfigKeyType::FullScreen;
        }
        else if (CMP_STR("showinputmap")) {
            outConfigKey->type = ConfigKeyType::ShowInputMap;
        }
        else {
            return ParserStatus::UnknownConfigKey;
        }
        
        outConfigKey->textFromFile = currentToken.stringValue;
        nextToken();
    }
    else {
        return ParserStatus::BadTokenStartOfLine;
    }
    
    
    return ParserStatus::OK;
}

static ParserStatus pair(ConfigPair **outputPairs) {
    ConfigPair configPair;
    ConfigValue tmpRHS;
    configPair.values = nullptr;
    
    auto res = configKey(&configPair.key);
    if (res != ParserStatus::OK) {
        return res;
    }
    
    if (!accept(ConfigTokenType::Equals)) {
        return ParserStatus::MissingEquals; 
    }
    
    for (;;) {
        res = configValue(&tmpRHS);
        if (res != ParserStatus::OK) {
            return res;
        }
        
        if (accept(ConfigTokenType::Comma)) {
            buf_malloc_push(configPair.values, tmpRHS); 
        }
        else if(accept(ConfigTokenType::NewLine) || accept(ConfigTokenType::End)) {
            buf_malloc_push(configPair.values, tmpRHS); 
            break;
        }
        else {
            return ParserStatus::MissingComma;
        }
    } 
    configPair.numValues = (isize)buf_len(configPair.values);
    
    buf_malloc_push(*outputPairs, configPair);
    return ParserStatus::OK;
}

ParserResult parseConfigFile(const char *fileName) {
    ParserResult ret = {};
    auto fileResult = readEntireFile(fileName, generalMemory);
    
    switch (fileResult.resultCode) {
    case FileSystemResultCode::OK: 
        break;
    default:
        freeFileBuffer(&fileResult, generalMemory);
        ret.fsResultCode = fileResult.resultCode;
        return ret;
    }
    
    currentToken.type = ConfigTokenType::Begin;
    stream = currentLine = (char*)fileResult.data;
    
    RESIZEM(stream, fileResult.size + 1, char);
    stream[fileResult.size] = '\0';
    nextToken();
    
    while (currentToken.type != ConfigTokenType::End) {
        while (accept(ConfigTokenType::NewLine)) 
            ;
        auto res = !isToken(ConfigTokenType::End) ?
             pair(&ret.configPairs) :
                    ParserStatus::OK;
        switch (res) {
        case ParserStatus::OK:
            break;
        default: {
            ret.errorLineNumber = currentToken.line;
            ret.errorColumn = currentToken.posInLine;
            if (currentToken.type != ConfigTokenType::NewLine) {
                ret.errorToken = currentToken.stringValue.data;
                ret.errorLine = currentLine;
                ret.stringAfterErrorToken = ret.errorToken + currentToken.stringValue.len;
                {
                    char *tmp = currentLine;
                    while (!isNewLineChar(*tmp) && *tmp != '\0') {
                        tmp++;
                    }
                    if (isNewLineChar(*tmp)) {
                        *tmp++ = ' ';
                    }
                    *tmp = '\0';
                    ret.errorLineLen = tmp - currentLine;
                    ret.errorLineLen--; //minus out '\0'
                }
                ret.status = res;
            }
            else {
                *currentToken.stringValue.data = '\0';
                ret.errorLine = previousLine;
                ret.status = ParserStatus::UnexpectedNewLine;
            }
            return ret;
        } break;
        }
    }

    ret.numConfigPairs = (i64)buf_len(ret.configPairs);
    ret.status = ParserStatus::OK;
    ret.stream = stream;
    return ret;
}

void freeParserResult(ParserResult *parserResult) {
   POPM(parserResult->stream); 
   fori((isize)buf_len(parserResult->configPairs)) {
       buf_malloc_free(parserResult->configPairs[i].values);
   }
   buf_malloc_free(parserResult->configPairs);
}
