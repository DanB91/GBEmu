#include "config.h"

enum class TokenType {
    Begin, Identifier, Integer,
    Other, KeyKW, ControllerKW,
    Equals, NewLine, End
};

struct Token {
    TokenType type;
    String value;
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

static bool areStringsEqualCaseInsenstive(const char *l, const char *r, i64 n) {
    for (int i = 0; i < n && *l && *r ;l++,r++, i++) {
        if (tolower(*l) != tolower(*r)) {
            return false;
        }
    }
    return true;
}


static void nextToken() {
    repeat:
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
    else if ((currentToken.type == TokenType::NewLine || currentToken.type == TokenType::Begin)
             && *stream == '#') { //comment
        while (*stream != '\0' &&
               !isNewLineChar(*stream)) {
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

static ParserStatus rhs(RHS *outRHS) {
    
    if (isToken(TokenType::KeyKW) || isToken(TokenType::ControllerKW))  {
       outRHS->rhsType = isToken(TokenType::KeyKW) ? RHSType::KeyMapping : RHSType::ControllerMapping;
       
       nextToken(); 
       switch (currentToken.type) {
       case TokenType::Equals:
       case TokenType::Other:  {
           outRHS->valueType = RHSValueType::Character;
           outRHS->characterValue = *currentToken.value.data;
       } break;
       case TokenType::Identifier:  {
           outRHS->valueType = RHSValueType::Identifier;
           outRHS->identValue = currentToken.value;
       } break;
       case TokenType::Integer: {
           outRHS->valueType = RHSValueType::Integer;
           outRHS->intValue = createIntFromCurrentToken();
       } break;
       default:  {
           //TODO: bad mapping token for controller if its not identifier
           return ParserStatus::BadMappingToken; 
       } break;
       }
    }
    else if (isToken(TokenType::Integer)) {
       outRHS->rhsType = RHSType::RawValue;
       outRHS->intValue = createIntFromCurrentToken();
    }
    else {
        return ParserStatus::BadRHSToken;
    }
    
    nextToken();
    return ParserStatus::OK;
}

static ParserStatus lhs(LHS *outLHS) {
    if (isToken(TokenType::Identifier)) {
#define CMP_STR(s) areStringsEqualCaseInsenstive(currentToken.value.data, s, currentToken.value.len) 
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
        else if (CMP_STR("scale")) {
            outLHS->value = LHSValue::Scale;
        }
        else {
            return ParserStatus::UnknownLHSConfig;
        }
        nextToken();
#undef CMP_STR
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
            ret.errorLine = currentLine;
            ret.errorColumn = currentPosInLine;
            ret.status = res;
            return ret;
        }
    }
    
    ret.numConfigPairs = (i64)buf_len(ret.configPairs);
    ret.status = ParserStatus::OK;
    return ret;
}

void freeParserResult(ParserResult *parserResult) {
   POPM(parserResult->stream); 
}
