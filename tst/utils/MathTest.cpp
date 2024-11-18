#include "UtilTestFixture.h"

class MathTest : public UtilTestBase {};

TEST_F(MathTest, testPowerAPI)
{
    UINT64 result = 0;

    EXPECT_EQ(STATUS_SUCCESS, computePower(0, 0, &result));
    EXPECT_EQ(1, result);

    EXPECT_EQ(STATUS_SUCCESS, computePower(0, 2, &result));
    EXPECT_EQ(0, result);

    result = 0;
    EXPECT_EQ(STATUS_SUCCESS, computePower(5, 0, &result));
    EXPECT_EQ(1, result);

    result = 0;
    EXPECT_EQ(STATUS_SUCCESS, computePower(5, 2, &result));
    EXPECT_EQ(25, result);

    result = 0;
    EXPECT_EQ(STATUS_SUCCESS, computePower(9, 2, &result));
    EXPECT_EQ(81, result);

    result = 0;
    EXPECT_EQ(STATUS_SUCCESS, computePower(5, 27, &result));
    EXPECT_EQ(7450580596923828125, result);

    result = 0;
    EXPECT_EQ(STATUS_SUCCESS, computePower(100, 9, &result));
    EXPECT_EQ(1000000000000000000, result);

    result = 0;
    EXPECT_EQ(STATUS_SUCCESS, computePower(1000, 6, &result));
    EXPECT_EQ(1000000000000000000, result);

    // Checks for arguments within acceptable range to prevent overflows
    EXPECT_EQ(STATUS_INVALID_ARG, computePower(5, 28, &result));
    EXPECT_EQ(STATUS_INVALID_ARG, computePower(6, 27, &result));
    EXPECT_EQ(STATUS_INVALID_ARG, computePower(100, 10, &result));
    EXPECT_EQ(STATUS_INVALID_ARG, computePower(101, 9, &result));
    EXPECT_EQ(STATUS_INVALID_ARG, computePower(1000, 7, &result));
    EXPECT_EQ(STATUS_INVALID_ARG, computePower(1001, 6, &result));
}
