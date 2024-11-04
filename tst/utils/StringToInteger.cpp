#include "UtilTestFixture.h"

class StoIFunctionalityTest : public UtilTestBase {};

TEST_F(StoIFunctionalityTest, NegativeInvalidInput_NullStrInput)
{
    UINT64 ui64;
    INT64 i64;
    UINT32 ui32;
    INT32 i32;
    EXPECT_NE(STATUS_SUCCESS, STRTOI64(NULL, NULL, 10, &i64));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI64(NULL, NULL, 10, &ui64));
    EXPECT_NE(STATUS_SUCCESS, STRTOI32(NULL, NULL, 10, &i32));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI32(NULL, NULL, 10, &ui32));
}

TEST_F(StoIFunctionalityTest, NegativeInvalidInput_NullRetInput)
{
    CHAR str[] = "123";
    EXPECT_NE(STATUS_SUCCESS, STRTOI64(str, NULL, 10, NULL));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI64(str, NULL, 10, NULL));
    EXPECT_NE(STATUS_SUCCESS, STRTOI32(str, NULL, 10, NULL));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI32(str, NULL, 10, NULL));
}

TEST_F(StoIFunctionalityTest, NegativeInvalidInput_EmptyStrInput)
{
    UINT64 ui64;
    INT64 i64;
    UINT32 ui32;
    INT32 i32;
    CHAR str[] = "";
    EXPECT_NE(STATUS_SUCCESS, STRTOI64(str, NULL, 10, &i64));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI64(str, NULL, 10, &ui64));
    EXPECT_NE(STATUS_SUCCESS, STRTOI32(str, NULL, 10, &i32));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI32(str, NULL, 10, &ui32));
}

TEST_F(StoIFunctionalityTest, NegativeInvalidInput_WhitespaceStrInput)
{
    UINT64 ui64;
    INT64 i64;
    UINT32 ui32;
    INT32 i32;
    CHAR str[] = " 123";
    EXPECT_NE(STATUS_SUCCESS, STRTOI64(str, NULL, 10, &i64));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI64(str, NULL, 10, &ui64));
    EXPECT_NE(STATUS_SUCCESS, STRTOI32(str, NULL, 10, &i32));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI32(str, NULL, 10, &ui32));
}

TEST_F(StoIFunctionalityTest, NegativeInvalidInput_WhitespaceStrAfterInput)
{
    UINT64 ui64;
    INT64 i64;
    UINT32 ui32;
    INT32 i32;
    CHAR str[] = "123 ";
    EXPECT_NE(STATUS_SUCCESS, STRTOI64(str, NULL, 10, &i64));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI64(str, NULL, 10, &ui64));
    EXPECT_NE(STATUS_SUCCESS, STRTOI32(str, NULL, 10, &i32));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI32(str, NULL, 10, &ui32));
}

TEST_F(StoIFunctionalityTest, NegativeInvalidInput_PlusSignOnlyStrInput)
{
    UINT64 ui64;
    INT64 i64;
    UINT32 ui32;
    INT32 i32;
    CHAR str[] = "+";
    EXPECT_NE(STATUS_SUCCESS, STRTOI64(str, NULL, 10, &i64));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI64(str, NULL, 10, &ui64));
    EXPECT_NE(STATUS_SUCCESS, STRTOI32(str, NULL, 10, &i32));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI32(str, NULL, 10, &ui32));
}

TEST_F(StoIFunctionalityTest, NegativeInvalidInput_MultiplePlusSignsStrInput)
{
    UINT64 ui64;
    INT64 i64;
    UINT32 ui32;
    INT32 i32;
    CHAR str[] = "++123";
    EXPECT_NE(STATUS_SUCCESS, STRTOI64(str, NULL, 10, &i64));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI64(str, NULL, 10, &ui64));
    EXPECT_NE(STATUS_SUCCESS, STRTOI32(str, NULL, 10, &i32));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI32(str, NULL, 10, &ui32));
}

TEST_F(StoIFunctionalityTest, NegativeInvalidInput_MinusSignOnlyStrInput)
{
    UINT64 ui64;
    INT64 i64;
    UINT32 ui32;
    INT32 i32;
    CHAR str[] = "-";
    EXPECT_NE(STATUS_SUCCESS, STRTOI64(str, NULL, 10, &i64));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI64(str, NULL, 10, &ui64));
    EXPECT_NE(STATUS_SUCCESS, STRTOI32(str, NULL, 10, &i32));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI32(str, NULL, 10, &ui32));
}

TEST_F(StoIFunctionalityTest, NegativeInvalidInput_MultipleMinusSignsStrInput)
{
    UINT64 ui64;
    INT64 i64;
    UINT32 ui32;
    INT32 i32;
    CHAR str[] = "--123";
    EXPECT_NE(STATUS_SUCCESS, STRTOI64(str, NULL, 10, &i64));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI64(str, NULL, 10, &ui64));
    EXPECT_NE(STATUS_SUCCESS, STRTOI32(str, NULL, 10, &i32));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI32(str, NULL, 10, &ui32));
}

TEST_F(StoIFunctionalityTest, NegativeInvalidInput_MinusInputWithUnsignedInput)
{
    UINT64 ui64;
    UINT32 ui32;
    CHAR str[] = "-123";
    CHAR strZero[] = "-0";
    EXPECT_NE(STATUS_SUCCESS, STRTOUI64(str, NULL, 10, &ui64));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI32(str, NULL, 10, &ui32));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI64(strZero, NULL, 10, &ui64));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI32(strZero, NULL, 10, &ui32));
}

TEST_F(StoIFunctionalityTest, NegativeInvalidInput_SignInStringInput)
{
    UINT64 ui64;
    INT64 i64;
    UINT32 ui32;
    INT32 i32;
    CHAR strPlus[] = "12+3";
    CHAR strMinus[] = "12-3";
    EXPECT_NE(STATUS_SUCCESS, STRTOI64(strPlus, NULL, 10, &i64));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI64(strPlus, NULL, 10, &ui64));
    EXPECT_NE(STATUS_SUCCESS, STRTOI32(strPlus, NULL, 10, &i32));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI32(strPlus, NULL, 10, &ui32));
    EXPECT_NE(STATUS_SUCCESS, STRTOI64(strMinus, NULL, 10, &i64));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI64(strMinus, NULL, 10, &ui64));
    EXPECT_NE(STATUS_SUCCESS, STRTOI32(strMinus, NULL, 10, &i32));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI32(strMinus, NULL, 10, &ui32));
}

TEST_F(StoIFunctionalityTest, NegativeInvalidInput_SameEndInputInput)
{
    UINT64 ui64;
    INT64 i64;
    UINT32 ui32;
    INT32 i32;
    CHAR str[] = "123";
    EXPECT_NE(STATUS_SUCCESS, STRTOI64(str, str, 10, &i64));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI64(str, str, 10, &ui64));
    EXPECT_NE(STATUS_SUCCESS, STRTOI32(str, str, 10, &i32));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI32(str, str, 10, &ui32));
}

TEST_F(StoIFunctionalityTest, NegativeInvalidInput_InvalidSmallBaseInput)
{
    UINT64 ui64;
    INT64 i64;
    UINT32 ui32;
    INT32 i32;
    CHAR str[] = "123";
    EXPECT_NE(STATUS_SUCCESS, STRTOI64(str, NULL, 0, &i64));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI64(str, NULL, 0, &ui64));
    EXPECT_NE(STATUS_SUCCESS, STRTOI32(str, NULL, 0, &i32));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI32(str, NULL, 0, &ui32));
    EXPECT_NE(STATUS_SUCCESS, STRTOI64(str, NULL, 1, &i64));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI64(str, NULL, 1, &ui64));
    EXPECT_NE(STATUS_SUCCESS, STRTOI32(str, NULL, 1, &i32));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI32(str, NULL, 1, &ui32));
}

TEST_F(StoIFunctionalityTest, NegativeInvalidInput_InvalidBigBaseInput)
{
    UINT64 ui64;
    INT64 i64;
    UINT32 ui32;
    INT32 i32;
    CHAR str[] = "123";
    EXPECT_NE(STATUS_SUCCESS, STRTOI64(str, NULL, 37, &i64));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI64(str, NULL, 37, &ui64));
    EXPECT_NE(STATUS_SUCCESS, STRTOI32(str, NULL, 37, &i32));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI32(str, NULL, 37, &ui32));
}

TEST_F(StoIFunctionalityTest, NegativeInvalidInput_InvalidDigitBaseInput)
{
    UINT64 ui64;
    INT64 i64;
    UINT32 ui32;
    INT32 i32;
    CHAR str[] = "12a3";
    CHAR str2[] = "101020";
    EXPECT_NE(STATUS_SUCCESS, STRTOI64(str, NULL, 10, &i64));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI64(str, NULL, 10, &ui64));
    EXPECT_NE(STATUS_SUCCESS, STRTOI32(str, NULL, 10, &i32));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI32(str, NULL, 10, &ui32));
    EXPECT_NE(STATUS_SUCCESS, STRTOI64(str2, NULL, 2, &i64));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI64(str2, NULL, 2, &ui64));
    EXPECT_NE(STATUS_SUCCESS, STRTOI32(str2, NULL, 2, &i32));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI32(str2, NULL, 2, &ui32));
}

TEST_F(StoIFunctionalityTest, NegativeInvalidInput_OverflowUnderflowInput)
{
    UINT64 ui64;
    INT64 i64;
    UINT32 ui32;
    INT32 i32;
    CHAR s1[] = "-123";
    CHAR s2[] = "100000000";
    CHAR s3[] = "80000000";
    CHAR s4[] = "-80000001";
    CHAR s5[] = "10000000000000000";
    CHAR s6[] = "8000000000000000";
    CHAR s7[] = "-8000000000000001";

    EXPECT_NE(STATUS_SUCCESS, STRTOUI64(s1, NULL, 10, &ui64));
    EXPECT_NE(STATUS_SUCCESS, STRTOUI32(s1, NULL, 10, &ui32));

    EXPECT_NE(STATUS_SUCCESS, STRTOUI32(s2, NULL, 16, &ui32));

    EXPECT_NE(STATUS_SUCCESS, STRTOI32(s3, NULL, 16, &i32));

    EXPECT_NE(STATUS_SUCCESS, STRTOI32(s4, NULL, 16, &i32));

    EXPECT_NE(STATUS_SUCCESS, STRTOUI64(s5, NULL, 16, &ui64));

    EXPECT_NE(STATUS_SUCCESS, STRTOI64(s6, NULL, 16, &i64));

    EXPECT_NE(STATUS_SUCCESS, STRTOI64(s7, NULL, 16, &i64));
}

TEST_F(StoIFunctionalityTest, PreceedingZeros)
{
    UINT64 ui64;
    INT64 i64;
    UINT32 ui32;
    INT32 i32;
    CHAR str[] = "000000000000000000000000000000000000000000000000000000000000000";
    EXPECT_EQ(STATUS_SUCCESS, STRTOI64(str, NULL, 10, &i64));
    EXPECT_EQ(STATUS_SUCCESS, STRTOUI64(str, NULL, 10, &ui64));
    EXPECT_EQ(STATUS_SUCCESS, STRTOI32(str, NULL, 10, &i32));
    EXPECT_EQ(STATUS_SUCCESS, STRTOUI32(str, NULL, 10, &ui32));

    EXPECT_EQ(0, i64);
    EXPECT_EQ(0, ui64);
    EXPECT_EQ(0, i32);
    EXPECT_EQ(0, ui32);
}

TEST_F(StoIFunctionalityTest, UpperLowerCase)
{
    UINT64 ui64;
    INT64 i64;
    UINT32 ui32;
    INT32 i32;
    CHAR str[] = "+aBcDeF";
    EXPECT_EQ(STATUS_SUCCESS, STRTOI64(str, NULL, 16, &i64));
    EXPECT_EQ(STATUS_SUCCESS, STRTOUI64(str, NULL, 16, &ui64));
    EXPECT_EQ(STATUS_SUCCESS, STRTOI32(str, NULL, 16, &i32));
    EXPECT_EQ(STATUS_SUCCESS, STRTOUI32(str, NULL, 16, &ui32));

    EXPECT_EQ(0xabcdef, i64);
    EXPECT_EQ(0xabcdef, ui64);
    EXPECT_EQ(0xabcdef, i32);
    EXPECT_EQ(0xabcdef, ui32);
}

TEST_F(StoIFunctionalityTest, EndPointerBeforeStringEnd)
{
    UINT64 ui64;
    INT64 i64;
    UINT32 ui32;
    INT32 i32;
    CHAR str[] = "+100000000000000000000000000000000000000000000000000000000000000000000000";
    EXPECT_EQ(STATUS_SUCCESS, STRTOI64(str, str + 11, 10, &i64));
    EXPECT_EQ(STATUS_SUCCESS, STRTOUI64(str, str + 11, 10, &ui64));
    EXPECT_EQ(STATUS_SUCCESS, STRTOI32(str, str + 11, 10, &i32));
    EXPECT_EQ(STATUS_SUCCESS, STRTOUI32(str, str + 11, 10, &ui32));

    EXPECT_EQ(1000000000, i64);
    EXPECT_EQ(1000000000, ui64);
    EXPECT_EQ(1000000000, i32);
    EXPECT_EQ(1000000000, ui32);
}

TEST_F(StoIFunctionalityTest, EndOfStringBeforeEndPointer)
{
    UINT64 ui64;
    INT64 i64;
    UINT32 ui32;
    INT32 i32;
    CHAR str[200] = "+1000000000";
    EXPECT_EQ(STATUS_SUCCESS, STRTOI64(str, str + 100, 10, &i64));
    EXPECT_EQ(STATUS_SUCCESS, STRTOUI64(str, str + 100, 10, &ui64));
    EXPECT_EQ(STATUS_SUCCESS, STRTOI32(str, str + 100, 10, &i32));
    EXPECT_EQ(STATUS_SUCCESS, STRTOUI32(str, str + 100, 10, &ui32));

    EXPECT_EQ(1000000000, i64);
    EXPECT_EQ(1000000000, ui64);
    EXPECT_EQ(1000000000, i32);
    EXPECT_EQ(1000000000, ui32);
}

TEST_F(StoIFunctionalityTest, PreceedingZerosWithSign)
{
    UINT64 ui64;
    INT64 i64;
    UINT32 ui32;
    INT32 i32;
    CHAR strPlus[] = "+000000000000000000000000000000000000000000000000000000000000000";
    CHAR strMinus[] = "-000000000000000000000000000000000000000000000000000000000000000";
    EXPECT_EQ(STATUS_SUCCESS, STRTOI64(strPlus, NULL, 10, &i64));
    EXPECT_EQ(STATUS_SUCCESS, STRTOUI64(strPlus, NULL, 10, &ui64));
    EXPECT_EQ(STATUS_SUCCESS, STRTOI32(strPlus, NULL, 10, &i32));
    EXPECT_EQ(STATUS_SUCCESS, STRTOUI32(strPlus, NULL, 10, &ui32));

    EXPECT_EQ(0, i64);
    EXPECT_EQ(0, ui64);
    EXPECT_EQ(0, i32);
    EXPECT_EQ(0, ui32);

    EXPECT_EQ(STATUS_SUCCESS, STRTOI64(strMinus, NULL, 10, &i64));
    EXPECT_EQ(STATUS_SUCCESS, STRTOI32(strMinus, NULL, 10, &i32));

    EXPECT_EQ(0, i64);
    EXPECT_EQ(0, ui64);
    EXPECT_EQ(0, i32);
    EXPECT_EQ(0, ui32);
}

#if !defined(__MINGW64__) && !defined(__MINGW32__)

TEST_F(StoIFunctionalityTest, CheckLimits)
{
    UINT64 ui64;
    INT64 i64;
    UINT32 ui32;
    INT32 i32;
    CHAR s1[] = "ffffffff";
    CHAR s2[] = "7fffffff";
    CHAR s3[] = "-80000000";
    CHAR s4[] = "ffffffffffffffff";
    CHAR s5[] = "7fffffffffffffff";
    CHAR s6[] = "-8000000000000000";

    EXPECT_EQ(STATUS_SUCCESS, STRTOUI32(s1, NULL, 16, &ui32));
    EXPECT_EQ(0xffffffffUL, ui32);
    EXPECT_EQ(STATUS_SUCCESS, STRTOI32(s2, NULL, 16, &i32));
    EXPECT_EQ(2147483647L, i32);
    EXPECT_EQ(STATUS_SUCCESS, STRTOI32(s3, NULL, 16, &i32));
    EXPECT_EQ(-2147483648LL, i32);

    EXPECT_EQ(STATUS_SUCCESS, STRTOUI64(s4, NULL, 16, &ui64));
    EXPECT_EQ(0xffffffffffffffffULL, ui64);
    EXPECT_EQ(STATUS_SUCCESS, STRTOI64(s5, NULL, 16, &i64));
    EXPECT_EQ(9223372036854775807LL, i64);
    EXPECT_EQ(STATUS_SUCCESS, STRTOI64(s6, NULL, 16, &i64));
    EXPECT_EQ(-9223372036854775807LL - 1, i64);
}

TEST_F(StoIFunctionalityTest, CheckSSCANFSupport)
{
    INT32 i32;
    UINT32 ui32;
    EXPECT_EQ(1, SSCANF("20", "%x", &i32));
    EXPECT_EQ(0x20, i32);
    EXPECT_EQ(1, SSCANF("30;", "%x", &i32));
    EXPECT_EQ(0x30, i32);

    EXPECT_EQ(1, SSCANF("20", "%d", &i32));
    EXPECT_EQ(20, i32);
    EXPECT_EQ(1, SSCANF("30;", "%d", &i32));
    EXPECT_EQ(30, i32);

    EXPECT_EQ(1, SSCANF("20", "%d", &ui32));
    EXPECT_EQ(20, ui32);
    EXPECT_EQ(1, SSCANF("30;", "%d", &ui32));
    EXPECT_EQ(30, ui32);
}

#endif
