//Copyright (C) 2018 Daniel Bokser.  See LICENSE.txt for license

#include "config.h"
#define CMP_STR(stringLiteral) areStringsEqualCaseInsenstive(currentToken.value.data, stringLiteral, currentToken.value.len, sizeof(stringLiteral) - 1) 

enum class TokenType {
    Begin, Identifier, Integer,
    Other, KeyKW, ControllerKW,
    CtrlKW,
    Equals, NewLine, End
};

struct Token {
    TokenType type;
    ConfigString value;
    int posInLine;
    int line;
};

static Token currentToken;
static char *stream;
static int currentLine = 1;
static int currentPosInLine = 1;

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
    currentToken.line = currentLine;
    currentToken.posInLine = currentPosInLine;
    
    if (*stream == '\0') {
        currentToken.type = TokenType::End; 
    }
    else if (isblank(*stream)) {
        while (*stream != '\0' && isblank(*stream)) {
            stream++;
            currentPosInLine++;
        }
        goto repeat;
    }
    else if (isNewLineChar(*stream)) {
        currentToken.type = TokenType::NewLine;
        while (*stream != '\0' && isspace(*stream))  {
            switch (*stream) {
            case '\n':
                stream++; 
                currentPosInLine = 1;
                currentLine++;
                break;
            case '\r':
                stream++;
                if (*stream == '\n') {
                    stream++;
                }
                currentPosInLine = 1;
                currentLine++;
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
        currentToken.type = TokenType::Integer;
        currentToken.value.data = stream;
        while (*stream != '\0' &&
               isdigit(*stream)) {
            stream++;
            currentPosInLine++;
        }
        currentToken.value.len = stream - currentToken.value.data ;
    }
    else if (tolower(stream[0]) == 'k' && tolower(stream[1]) == 'e' && stream[2] == 'y') {
       currentToken.type = TokenType::KeyKW;
       stream += 3;
       currentPosInLine += 3;
    }
    else if (tolower(stream[0]) == 'c' && tolower(stream[1]) == 'o' && tolower(stream[2]) == 'n' && 
             tolower(stream[3]) == 't' && tolower(stream[4]) == 'r' && tolower(stream[5]) == 'o' &&
             tolower(stream[6]) == 'l' && tolower(stream[7]) == 'l' && tolower(stream[8]) == 'e' && tolower(stream[9]) == 'r') {
       currentToken.type = TokenType::ControllerKW;
       stream += 10 ;
       currentPosInLine += 10;
    }
    else if (tolower(stream[0]) == 'c' && tolower(stream[1]) == 'o' && tolower(stream[2]) == 'm' && 
             tolower(stream[3]) == 'm' && tolower(stream[4]) == 'a' && tolower(stream[5]) == 'n' &&
             tolower(stream[6]) == 'd' && tolower(stream[7]) == '-' ) {
       currentToken.type = TokenType::CtrlKW;
       stream += 8 ;
       currentPosInLine += 8;
                
    }
    else if (tolower(stream[0]) == 'c' && tolower(stream[1]) == 't' && tolower(stream[2]) == 'r' && 
             tolower(stream[3]) == 'l' && tolower(stream[4]) == '-') {
       currentToken.type = TokenType::CtrlKW;
       stream += 5;
       currentPosInLine += 5;
    }
    else if (isIdentifierChar(*stream)) {
        currentToken.type = TokenType::Identifier;
        currentToken.value.data = stream;
        while (*stream != '\0' &&
               (isIdentifierChar(*stream) ||
                isdigit(*stream))) {
            stream++;
            currentPosInLine++;
        }
        currentToken.value.len = stream - currentToken.value.data ;
    }
    else if (*stream == '=') {
        currentToken.type = TokenType::Equals; 
        stream++;
        currentPosInLine++;
    }
    else {
        currentToken.value.data = stream;
        //TODO: support utf-8
        currentToken.value.len = 1;
        
        currentToken.type = TokenType::Other;
        stream++;
        currentPosInLine++;
    }
    
    
}

static bool accept(TokenType tt) {
    if (currentToken.type == tt) {
        nextToken();
        return true;
    }
    return false;
}
static inline bool isToken(TokenType tokenType) {
    return currentToken.type == tokenType;
}

static int createIntFromCurrentToken() {
    i64 tokenLen = currentToken.value.len;
    char *intStr = PUSHM(tokenLen + 1, char);
    memcpy(intStr, currentToken.value.data, (usize)tokenLen);
    intStr[tokenLen] = '\0';
    
    int ret = atoi(intStr);
    
    POPM(intStr);
    return ret;
}

static ParserStatus controllerMapping(ControllerMapping *outControllerMapping) {
    outControllerMapping->posInLine = currentPosInLine;
    outControllerMapping->line = currentLine;
    switch (currentToken.type) {
    case TokenType::Identifier:  {
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
        else if (CMP_STR("select")) {
            outControllerMapping->value = ControllerMappingValue::Select;
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
        return ParserStatus::BadControllerMappingToken; 
    } break;
    }
    
    return ParserStatus::OK;
}
static ParserStatus keyMapping(KeyMapping *outKeyMapping) {
    outKeyMapping->posInLine = currentPosInLine;
    outKeyMapping->line = currentLine;
    
    if (currentToken.type == TokenType::CtrlKW) {
       outKeyMapping->isCtrlHeld = true; 
       nextToken();
    }
    else {
       outKeyMapping->isCtrlHeld = false; 
    }

    switch (currentToken.type) {
    case TokenType::Equals:
    case TokenType::Other:  {
        if (*currentToken.value.data <= 'z' && *currentToken.value.data >= '!') {
            outKeyMapping->type = KeyMappingType::Character;
            outKeyMapping->characterValue = *currentToken.value.data;
        }
        else {
            return ParserStatus::UnrecognizedKeyMapping;
        }
    } break;
    case TokenType::Identifier:  {
        outKeyMapping->type = KeyMappingType::MovementKey;
        if (currentToken.value.len == 1) {
           outKeyMapping->type = KeyMappingType::Character;
           outKeyMapping->characterValue = tolower(*currentToken.value.data);
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
    case TokenType::Integer: {
        return ParserStatus::NumberKeyMappingNotAllowed;
    } break;
    default:  {
        return ParserStatus::BadKeyMappingToken; 
    } break;
    }
    
    return ParserStatus::OK;
}

static ParserStatus rhs(RHS *outRHS) {
    outRHS->line = currentToken.line;
    outRHS->posInLine = currentToken.posInLine;
    
    if (isToken(TokenType::KeyKW))  {
        outRHS->rhsType = RHSType::KeyMapping;
        nextToken();
        auto res = keyMapping(&outRHS->keyMapping);
        if (res != ParserStatus::OK) {
            return res;
        }
    }
    else if (isToken(TokenType::ControllerKW)) {
        outRHS->rhsType = RHSType::ControllerMapping;
        nextToken();
        auto res = controllerMapping(&outRHS->controllerMapping);
        if (res != ParserStatus::OK) {
            return res;
        }
    }
    else if (isToken(TokenType::Integer)) {
       outRHS->rhsType = RHSType::Integer;
       outRHS->intValue = createIntFromCurrentToken();
    }
    else {
        return ParserStatus::BadRHSToken;
    }
    
    
    nextToken();
    return ParserStatus::OK;
}

static ParserStatus lhs(LHS *outLHS) {
    outLHS->line = currentToken.line;
    outLHS->posInLine = currentToken.posInLine;
    if (isToken(TokenType::Identifier)) {
        if (CMP_STR("up")) {
            outLHS->value = LHSValue::Up;
        }
        else if (CMP_STR("down")) {
            outLHS->value = LHSValue::Down;
        }
        else if (CMP_STR("left")) {
            outLHS->value = LHSValue::Left;
        }
        else if (CMP_STR("right")) {
            outLHS->value = LHSValue::Right;
        }
        else if (CMP_STR("a")) {
            outLHS->value = LHSValue::A;
        }
        else if (CMP_STR("b")) {
            outLHS->value = LHSValue::B;
        }
        else if (CMP_STR("start")) {
            outLHS->value = LHSValue::Start;
        }
        else if (CMP_STR("select")) {
            outLHS->value = LHSValue::Select;
        }
        else if (CMP_STR("rewind")) {
            outLHS->value = LHSValue::Rewind;
        }
        else if (CMP_STR("step")) {
            outLHS->value = LHSValue::Step;
        }
        else if (CMP_STR("continue")) {
            outLHS->value = LHSValue::Continue;
        }
        else if (CMP_STR("mute")) {
            outLHS->value = LHSValue::Mute;
        }
        else if (CMP_STR("screenscale")) {
            outLHS->value = LHSValue::ScreenScale;
        }
        else if (CMP_STR("pause")) {
            outLHS->value = LHSValue::Pause;
        }
        else if (CMP_STR("showdebugger")) {
            outLHS->value = LHSValue::ShowDebugger;
        }
        else if (CMP_STR("reset")) {
            outLHS->value = LHSValue::Reset;
        }
        else if (CMP_STR("showhomepath")) {
            outLHS->value = LHSValue::ShowHomePath;
        }
        else if (CMP_STR("fullscreen")) {
            outLHS->value = LHSValue::FullScreen;
        }
        else {
            return ParserStatus::UnknownLHSConfig;
        }
        
        nextToken();
    }
    else {
        return ParserStatus::BadLHSToken;
    }
    
    
    return ParserStatus::OK;
}

static ParserStatus pair(ConfigPair **outputPairs) {
    ConfigPair configPair;
    while (accept(TokenType::NewLine)) 
        ;
    
    auto res = lhs(&configPair.lhs);
    if (res != ParserStatus::OK) {
        return res;
    }
    
    if (!accept(TokenType::Equals)) {
        return ParserStatus::MissingEquals; 
    }
    
    res = rhs(&configPair.rhs);
    if (res != ParserStatus::OK) {
        return res;
    }
    
    if (!accept(TokenType::NewLine) && !accept(TokenType::End)) {
        return ParserStatus::UnexpectedToken;
    }
    
    buf_push(*outputPairs, configPair);
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
    
    currentToken.type = TokenType::Begin;
    stream = (char*)fileResult.data;
    RESIZEM(stream, fileResult.size + 1, char);
    stream[fileResult.size] = '\0';
    nextToken();
    
    while (currentToken.type != TokenType::End) {
        auto res = pair(&ret.configPairs);
        switch (res) {
        case ParserStatus::OK:
            break;
        default:
            ret.errorLine = currentToken.line;
            ret.errorColumn = currentToken.posInLine;
            ret.status = res;
            return ret;
        }
    }
    
    ret.numConfigPairs = (i64)buf_len(ret.configPairs);
    ret.status = ParserStatus::OK;
    ret.stream = stream;
    return ret;
}

void freeParserResult(ParserResult *parserResult) {
   POPM(parserResult->stream); 
}
