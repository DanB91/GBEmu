#define CO_IMPL
#define CO_DEBUG
#include "../config.cpp"

#define TEST_ASSERT_EQ(actual, expected, msg) do {\
    if ((actual) != (expected)) {\
       PRINT("Assertion failed on line %d -- Actual: %d, Expected: %d -- Msg: %s", __LINE__, (int)actual, (int)expected, msg);\
       exit(1);\
    }\
} while (0)\

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
    
    PRINT();
    PRINT("************************************");
    PRINT("************PASSED!!!***************");
    PRINT("************************************");
    return 0;
}
