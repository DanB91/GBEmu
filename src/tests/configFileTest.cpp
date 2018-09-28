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
    TEST_ASSERT_EQ(res.configPairs[0].lhs.value, LHSValue::A, "Wrong LHS value");
    TEST_ASSERT_EQ(res.configPairs[0].numRightHandSideValues, 1, "Only mapped to one key");
    TEST_ASSERT_EQ(res.configPairs[0].rightHandSideValues[0].rhsType, RHSType::KeyMapping, "Should be key mapping");
    TEST_ASSERT_EQ(res.configPairs[0].rightHandSideValues[0].keyMapping.characterValue, '/', "Wrong key mapped");
    TEST_ASSERT_EQ(res.configPairs[0].rightHandSideValues[0].keyMapping.isCtrlHeld, false, "Ctrl not held");
    TEST_ASSERT_EQ(res.configPairs[0].rightHandSideValues[0].keyMapping.type, KeyMappingType::Character, "Wrong key type mapped");
    
    TEST_ASSERT_EQ(res.configPairs[1].lhs.value, LHSValue::B, "Wrong LHS value");
    TEST_ASSERT_EQ(res.configPairs[1].numRightHandSideValues, 1, "Only mapped to one key");
    TEST_ASSERT_EQ(res.configPairs[1].rightHandSideValues[0].rhsType, RHSType::KeyMapping, "Should be key mapping");
    TEST_ASSERT_EQ(res.configPairs[1].rightHandSideValues[0].keyMapping.characterValue, '.', "Wrong key mapped");
    TEST_ASSERT_EQ(res.configPairs[1].rightHandSideValues[0].keyMapping.isCtrlHeld, false, "Ctrl not held");
    TEST_ASSERT_EQ(res.configPairs[1].rightHandSideValues[0].keyMapping.type, KeyMappingType::Character, "Wrong key type mapped");
    
    TEST_ASSERT_EQ(res.configPairs[2].lhs.value, LHSValue::Start, "Wrong LHS value");
    TEST_ASSERT_EQ(res.configPairs[2].numRightHandSideValues, 3, "Wrong number of RHS values");
    TEST_ASSERT_EQ(res.configPairs[2].rightHandSideValues[0].rhsType, RHSType::ControllerMapping, "Wrong type of mapping");
    TEST_ASSERT_EQ(res.configPairs[2].rightHandSideValues[0].controllerMapping.value, ControllerMappingValue::Start, "Wrong mapping");
    TEST_ASSERT_EQ(res.configPairs[2].rightHandSideValues[1].rhsType, RHSType::KeyMapping, "Should be key mapping");
    TEST_ASSERT_EQ(res.configPairs[2].rightHandSideValues[1].keyMapping.characterValue, '#', "Wrong key mapped");
    TEST_ASSERT_EQ(res.configPairs[2].rightHandSideValues[1].keyMapping.isCtrlHeld, false, "Ctrl not held");
    TEST_ASSERT_EQ(res.configPairs[2].rightHandSideValues[1].keyMapping.type, KeyMappingType::Character, "Wrong key type mapped");
    TEST_ASSERT_EQ(res.configPairs[2].rightHandSideValues[2].rhsType, RHSType::KeyMapping, "Should be key mapping");
    TEST_ASSERT_EQ(res.configPairs[2].rightHandSideValues[2].keyMapping.type, KeyMappingType::MovementKey, "Wrong key type mapped");
    TEST_ASSERT_EQ(res.configPairs[2].rightHandSideValues[2].keyMapping.movementKeyValue, MovementKeyMappingValue::Enter, "Wrong key type mapped");
    TEST_ASSERT_EQ(res.configPairs[2].rightHandSideValues[2].keyMapping.isCtrlHeld, false, "Ctrl not held");
    
    PRINT();
    PRINT("************************************");
    PRINT("************PASSED!!!***************");
    PRINT("************************************");
    return 0;
}
