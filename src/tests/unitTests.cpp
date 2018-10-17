#define CO_IMPL
#define CO_DEBUG
#include "../common.h"
#include "../config.cpp"

#define TEST_ASSERT_EQ(actual, expected, msg) do {\
    if ((actual) != (expected)) {\
       PRINT("Assertion failed on line %d -- Actual: %u, Expected: %u -- Msg: %s", __LINE__, (int)actual, (int)expected, msg);\
       exit(1);\
    }\
} while (0)

int main() {
    if (!initMemory(MB(11),MB(10))) {
        CO_LOG("Failed to init memeory for test");
        return 1;
    }
    
    PRINT("************************************");
    PRINT("****STARTING GBEMU UNIT TESTS******");
    PRINT("************************************");
  
    //utf8 tests
    TEST_ASSERT_EQ(utf8FromUTF32(L'a').data, 'a', "Bad translation from utf8 to utf32");
    TEST_ASSERT_EQ(utf32FromUTF8({'a'}), L'a', "Bad translation from utf32 to utf8");
    TEST_ASSERT_EQ(utf8FromUTF32(L'ç').data, 0xA7C3, "Bad translation from utf8 to utf32");
    TEST_ASSERT_EQ(utf32FromUTF8({0xA7C3}), L'ç', "Bad translation from utf32 to utf8");
    TEST_ASSERT_EQ(utf8FromUTF32(L'⌣').data, 0xA38CE2 , "Bad translation from utf8 to utf32");
    TEST_ASSERT_EQ(utf32FromUTF8({0xA38CE2}),  L'⌣', "Bad translation from utf32 to utf8");
    TEST_ASSERT_EQ(utf8FromUTF32(L'🎮').data, 0xAE8E9FF0 , "Bad translation from utf8 to utf32");
    TEST_ASSERT_EQ(utf32FromUTF8({0xAE8E9FF0}),  L'🎮', "Bad translation from utf32 to utf8");

    //config tests
    auto res = parseConfigFile("../src/tests/test.txt");
    TEST_ASSERT_EQ(res.fsResultCode, FileSystemResultCode::OK, "File should exist");
    TEST_ASSERT_EQ(res.status, ParserStatus::OK, "Parse should have succeded");
    TEST_ASSERT_EQ(res.numConfigPairs, 5, "Incorrect number of configs");
    TEST_ASSERT_EQ(res.configPairs[0].key.type, ConfigKeyType::A, "Wrong LHS value");
    TEST_ASSERT_EQ(res.configPairs[0].numValues, 1, "Only mapped to one key");
    TEST_ASSERT_EQ(res.configPairs[0].values[0].type, ConfigValueType::KeyMapping, "Should be key mapping");
    TEST_ASSERT_EQ(res.configPairs[0].values[0].keyMapping.characterValue.data, '/', "Wrong key mapped");
    TEST_ASSERT_EQ(res.configPairs[0].values[0].keyMapping.isCtrlHeld, false, "Ctrl not held");
    TEST_ASSERT_EQ(res.configPairs[0].values[0].keyMapping.type, KeyMappingType::Character, "Wrong key type mapped");
    
    TEST_ASSERT_EQ(res.configPairs[1].key.type, ConfigKeyType::B, "Wrong LHS value");
    TEST_ASSERT_EQ(res.configPairs[1].numValues, 1, "Only mapped to one key");
    TEST_ASSERT_EQ(res.configPairs[1].values[0].type, ConfigValueType::KeyMapping, "Should be key mapping");
    TEST_ASSERT_EQ(res.configPairs[1].values[0].keyMapping.characterValue.data, '.', "Wrong key mapped");
    TEST_ASSERT_EQ(res.configPairs[1].values[0].keyMapping.isCtrlHeld, false, "Ctrl not held");
    TEST_ASSERT_EQ(res.configPairs[1].values[0].keyMapping.type, KeyMappingType::Character, "Wrong key type mapped");
    
    TEST_ASSERT_EQ(res.configPairs[2].key.type, ConfigKeyType::Start, "Wrong LHS value");
    TEST_ASSERT_EQ(res.configPairs[2].numValues, 3, "Wrong number of RHS values");
    TEST_ASSERT_EQ(res.configPairs[2].values[0].type, ConfigValueType::ControllerMapping, "Wrong type of mapping");
    TEST_ASSERT_EQ(res.configPairs[2].values[0].controllerMapping.value, ControllerMappingValue::Start, "Wrong mapping");
    TEST_ASSERT_EQ(res.configPairs[2].values[1].type, ConfigValueType::KeyMapping, "Should be key mapping");
    TEST_ASSERT_EQ(res.configPairs[2].values[1].keyMapping.characterValue.data, '#', "Wrong key mapped");
    TEST_ASSERT_EQ(res.configPairs[2].values[1].keyMapping.isCtrlHeld, false, "Ctrl not held");
    TEST_ASSERT_EQ(res.configPairs[2].values[1].keyMapping.type, KeyMappingType::Character, "Wrong key type mapped");
    TEST_ASSERT_EQ(res.configPairs[2].values[2].type, ConfigValueType::KeyMapping, "Should be key mapping");
    TEST_ASSERT_EQ(res.configPairs[2].values[2].keyMapping.type, KeyMappingType::MovementKey, "Wrong key type mapped");
    TEST_ASSERT_EQ(res.configPairs[2].values[2].keyMapping.movementKeyValue, MovementKeyMappingValue::Enter, "Wrong key type mapped");
    TEST_ASSERT_EQ(res.configPairs[2].values[2].keyMapping.isCtrlHeld, false, "Ctrl not held");
    
    PRINT();
    PRINT("************************************");
    PRINT("************PASSED!!!***************");
    PRINT("************************************");
    return 0;
}
