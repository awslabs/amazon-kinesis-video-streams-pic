#include "UtilTestFixture.h"

class MathTest : public UtilTestBase {
};

TEST_F(MathTest, testPowerAPI)
{
    EXPECT_EQ(0, power(0, 2));
    EXPECT_EQ(1, power(5, 0));
    EXPECT_EQ(25, power(5, 2));
    EXPECT_EQ(81, power(9, 2));
}
