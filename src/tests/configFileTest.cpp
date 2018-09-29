#define CO_IMPL
#define CO_DEBUG
#include "../config.cpp"

#define TEST_ASSERT_EQ(actual, expected, msg) do {\
    if ((actual) != (expected)) {\
       PRINT("Assertion failed on line %d -- Actual: %d, Expected: %d -- Msg: %s", __LINE__, (int)actual, (int)expected, msg);\
       exit(1);\
    }\
} while (0)

int main() {
    if (!initMemory(MB(11),MB(10))) {
        CO_LOG("Failed to init memeory for test");
        return 1;
    }
    
    PRINT("************************************");
    PRINT("****STARTING CONFIG FILE TESTS******");
    PRINT("************************************");
    
    auto res = parseConfigFile("src/tests/test.txt");
    TEST_ASSERT_EQ(res.fsResultCode, FileSystemResultCode::OK, "File should exist");
    TEST_ASSERT_EQ(res.status, ParserStatus::OK, "Parse should have succeded");
    TEST_ASSERT_EQ(res.numConfigPairs, 5, "Incorrect number of configs");
    TEST_ASSERT_EQ(res.configPairs[0].key.type, ConfigKeyType::A, "Wrong LHS value");
    TEST_ASSERT_EQ(res.configPairs[0].numValues, 1, "Only mapped to one key");
    TEST_ASSERT_EQ(res.configPairs[0].values[0].type, ConfigValueType::KeyMapping, "Should be key mapping");
    TEST_ASSERT_EQ(res.configPairs[0].values[0].keyMapping.characterValue, '/', "Wrong key mapped");
    TEST_ASSERT_EQ(res.configPairs[0].values[0].keyMapping.isCtrlHeld, false, "Ctrl not held");
    TEST_ASSERT_EQ(res.configPairs[0].values[0].keyMapping.type, KeyMappingType::Character, "Wrong key type mapped");
    
    TEST_ASSERT_EQ(res.configPairs[1].key.type, ConfigKeyType::B, "Wrong LHS value");
    TEST_ASSERT_EQ(res.configPairs[1].numValues, 1, "Only mapped to one key");
    TEST_ASSERT_EQ(res.configPairs[1].values[0].type, ConfigValueType::KeyMapping, "Should be key mapping");
    TEST_ASSERT_EQ(res.configPairs[1].values[0].keyMapping.characterValue, '.', "Wrong key mapped");
    TEST_ASSERT_EQ(res.configPairs[1].values[0].keyMapping.isCtrlHeld, false, "Ctrl not held");
    TEST_ASSERT_EQ(res.configPairs[1].values[0].keyMapping.type, KeyMappingType::Character, "Wrong key type mapped");
    
    TEST_ASSERT_EQ(res.configPairs[2].key.type, ConfigKeyType::Start, "Wrong LHS value");
    TEST_ASSERT_EQ(res.configPairs[2].numValues, 3, "Wrong number of RHS values");
    TEST_ASSERT_EQ(res.configPairs[2].values[0].type, ConfigValueType::ControllerMapping, "Wrong type of mapping");
    TEST_ASSERT_EQ(res.configPairs[2].values[0].controllerMapping.value, ControllerMappingValue::Start, "Wrong mapping");
    TEST_ASSERT_EQ(res.configPairs[2].values[1].type, ConfigValueType::KeyMapping, "Should be key mapping");
    TEST_ASSERT_EQ(res.configPairs[2].values[1].keyMapping.characterValue, '#', "Wrong key mapped");
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
