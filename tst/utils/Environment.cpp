#include "UtilTestFixture.h"

class EnvironmentFunctionalityTest : public UtilTestBase {};

TEST_F(EnvironmentFunctionalityTest, CheckTrueCases)
{
    std::string envVar = "KVS_TEST_ENV_VAR_TRUE";

#ifdef _WIN32
    CHAR envBuf[256];
#endif

    // Common potential true values to test.
    std::vector<std::string> trueValues = {
        "1", "true", "True", "TRUE", "on", "On", "ON",
    };

    for (std::string val : trueValues) {
// Unset env var first.
#ifdef _WIN32
        SNPRINTF(envBuf, sizeof(envBuf), "%s=", envVar.c_str());
        _putenv(envBuf);
#else
        unsetenv(envVar.c_str());
#endif

        // Check unset case.
        EXPECT_FALSE(isEnvVarEnabled((PCHAR) envVar.c_str()));

// Set to the TRUE value.
#ifdef _WIN32
        SNPRINTF(envBuf, sizeof(envBuf), "%s=%s", envVar.c_str(), val.c_str());
        _putenv(envBuf);
#else
        setenv(envVar.c_str(), val.c_str(), 1);
#endif

        EXPECT_TRUE(isEnvVarEnabled((PCHAR) envVar.c_str()));
    }

// Cleanup the test environment variable.
#ifdef _WIN32
    SNPRINTF(envBuf, sizeof(envBuf), "%s=", envVar.c_str());
    _putenv(envBuf);
#else
    unsetenv(envVar.c_str());
#endif
}

TEST_F(EnvironmentFunctionalityTest, CheckFalseCases)
{
    std::string envVar = "KVS_TEST_ENV_VAR_FALSE";

#ifdef _WIN32
    CHAR envBuf[256];
#endif

    // Common potential false values to test, including near-true ones.
    std::vector<std::string> falseValues = {
        "0", "false", "off", "random_string", "", " ", "tru", "rue", "onn", "yes", "no", "2", "-1",
    };

    for (std::string val : falseValues) {
// Unset env var first.
#ifdef _WIN32
        SNPRINTF(envBuf, sizeof(envBuf), "%s=", envVar.c_str());
        _putenv(envBuf);
#else
        unsetenv(envVar.c_str());
#endif

        // Check unset case.
        EXPECT_FALSE(isEnvVarEnabled((PCHAR) envVar.c_str()));

// Set to the false value
#ifdef _WIN32
        SNPRINTF(envBuf, sizeof(envBuf), "%s=%s", envVar.c_str(), val.c_str());
        _putenv(envBuf);
#else
        setenv(envVar.c_str(), val.c_str(), 1);
#endif

        EXPECT_FALSE(isEnvVarEnabled((PCHAR) envVar.c_str()));
    }

    // Null variable name check.
    EXPECT_FALSE(isEnvVarEnabled((PCHAR) NULL));

    // Empty variable name check.
    EXPECT_FALSE(isEnvVarEnabled((PCHAR) ""));

// Cleanup the test environment variable.
#ifdef _WIN32
    SNPRINTF(envBuf, sizeof(envBuf), "%s=", envVar.c_str());
    _putenv(envBuf);
#else
    unsetenv(envVar.c_str());
#endif
}
