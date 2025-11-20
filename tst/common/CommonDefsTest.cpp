#include "gtest/gtest.h"

#include "src/common/include/com/amazonaws/kinesis/video/common/CommonDefs.h"

TEST(CommonDefsTest, SizeTMatches)
{
    EXPECT_EQ(SIZEOF(size_t), SIZEOF(SIZE_T));
}

TEST(CommonDefsTest, UINT32Matches)
{
    EXPECT_EQ(SIZEOF(UINT32), 4);
}

TEST(CommonDefsTest, GetEnvWorks)
{
    EXPECT_NE(nullptr, GETENV("PATH")) << "PATH environment variable is unexpectedly unset, or GETENV doesn't work";
}


// On Windows, getenv() doesn't work with SetEnvironmentVariable().
// We'll need to use SetEnvironmentVariable() and GetEnvironmentVariable() together for runtime changes.
// However, getenv does work in Windows (above test: GetEnvWorks) when the variable is set before program execution.
// Note: SetEnvironmentVariable and GetEnvironmentVariable have different APIs to getenv and setenv.
#ifndef __WINDOWS_BUILD__
TEST(CommonDefsTest, SetEnvWorks)
{
    const char* test_var = "MY_TEST_VAR";
    const char* test_value = "test_value";

    // Save the original value of the environment variable, if it exists
    const char* original_value = GETENV(test_var);

    // Set the environment variable to a new value
    setenv(test_var, test_value, 1);

    // Verify if the environment variable was set correctly
    EXPECT_STREQ(test_value, GETENV(test_var)) << "Failed to set environment variable " << test_var;

    // Restore the original environment variable value (if any)
    if (original_value) {
        setenv(test_var, original_value, 1);
    } else {
        // If it was not set, unset the variable
        unsetenv(test_var);
    }
}

TEST(CommonDefsTest, UnsetEnvWorks)
{
    const char* test_var = "MY_TEST_VAR";
    const char* test_value = "test_value";

    // Save the original value of the environment variable, if it exists
    const char* original_value = GETENV(test_var);

    // Set the environment variable to a new value
    setenv(test_var, test_value, 1);

    // Verify if the environment variable was set correctly
    EXPECT_STREQ(test_value, GETENV(test_var)) << "Failed to set environment variable " << test_var;

    // Unset the environment variable
    unsetenv(test_var);

    // Verify if the environment variable is properly unset
    EXPECT_EQ(nullptr, GETENV(test_var)) << "Failed to unset environment variable " << test_var;

    // Restore the original environment variable value (if any)
    if (original_value) {
        setenv(test_var, original_value, 1);
    } else {
        // If it was not set, unset the variable
        unsetenv(test_var);
    }
}
#endif
