#include "UtilTestFixture.h"

#ifdef _WIN32
#define TEST_TEMP_DIR_PATH                                      (PCHAR) "C:\\Windows\\Temp\\"
#define TEST_TEMP_DIR_PATH_NO_ENDING_SEPARTOR                   (PCHAR) "C:\\Windows\\Temp"
#else
#define TEST_TEMP_DIR_PATH                                      (PCHAR) "/tmp/"
#define TEST_TEMP_DIR_PATH_NO_ENDING_SEPARTOR                   (PCHAR) "/tmp"
#endif

class FileIoFunctionalityTest : public UtilTestBase {
};

class FileIoUnitTest : public UtilTestBase {
};

TEST_F(FileIoUnitTest, readFile_filePathNull) {
    EXPECT_EQ(STATUS_NULL_ARG, readFile(NULL, TRUE, NULL, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, readFile(NULL, FALSE, NULL, NULL));
}

TEST_F(FileIoUnitTest, readFile_sizeNull) {
    EXPECT_EQ(STATUS_NULL_ARG, readFile((PCHAR) (TEST_TEMP_DIR_PATH "test"), TRUE, NULL, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, readFile((PCHAR) (TEST_TEMP_DIR_PATH "test"), FALSE, NULL, NULL));
}

TEST_F(FileIoUnitTest, readFile_asciiBufferTooSmall) {
    FILE *file = FOPEN((PCHAR) (TEST_TEMP_DIR_PATH "asciitest"), "w");
    UINT64 fileSize = 43;
    PCHAR fileBuffer = NULL;

    fprintf(file, "This is line 1\nThis is line 2\nThis is line 3\n");
    FCLOSE(file);

    EXPECT_EQ(STATUS_SUCCESS, readFile((PCHAR) (TEST_TEMP_DIR_PATH "asciitest"), FALSE, NULL, &fileSize));

// In Windows systems, the newline character is represented by a combination of 
// two characters: Carriage Return (CR) followed by Line Feed (LF), written as \r\n. 
// So, each newline character in a text file on Windows contributes two bytes to the file size.
#if defined _WIN32 || defined _WIN64    
    EXPECT_EQ(48, fileSize);
#else
    EXPECT_EQ(45, fileSize);
#endif

    fileSize = 43;

    fileBuffer = (PCHAR) MEMCALLOC(1, (fileSize + 1) * SIZEOF(CHAR));
    EXPECT_EQ(STATUS_BUFFER_TOO_SMALL, readFile((PCHAR) (TEST_TEMP_DIR_PATH "asciitest"), FALSE, (PBYTE) fileBuffer, &fileSize));

    remove((PCHAR) (TEST_TEMP_DIR_PATH "asciitest"));
    SAFE_MEMFREE(fileBuffer);
}

TEST_F(FileIoUnitTest, readFile_binaryBufferTooSmall) {
    FILE *file = fopen((PCHAR) (TEST_TEMP_DIR_PATH "binarytest"), "wb");
    UINT64 fileSize = 43;
    PCHAR fileBuffer = NULL;
    CHAR data[100] = "This is line 1\nThis is line 2\nThis is line 3\n";
   
    FWRITE(data, SIZEOF(CHAR), STRLEN(data), file);
    FCLOSE(file);

    EXPECT_EQ(STATUS_SUCCESS, readFile((PCHAR) (TEST_TEMP_DIR_PATH "binarytest"), TRUE, NULL, &fileSize));
    EXPECT_EQ(45, fileSize);

    fileSize = 43;

    fileBuffer = (PCHAR) MEMCALLOC(1, (fileSize + 1) * SIZEOF(CHAR));
    EXPECT_EQ(STATUS_BUFFER_TOO_SMALL, readFile((PCHAR) (TEST_TEMP_DIR_PATH "binarytest"), TRUE, (PBYTE) fileBuffer, &fileSize));

    remove((PCHAR) (TEST_TEMP_DIR_PATH "binarytest"));
    SAFE_MEMFREE(fileBuffer);
}

TEST_F(FileIoFunctionalityTest, readFile_asciiFileSize) {
    FILE *file = FOPEN((PCHAR) (TEST_TEMP_DIR_PATH "asciitest"), "w");
    CHAR fileBuffer[256];
    UINT64 fileSize = ARRAY_SIZE(fileBuffer);

    fprintf(file, "This is line 1\nThis is line 2\nThis is line 3\n");
    FCLOSE(file);

    EXPECT_EQ(STATUS_SUCCESS, readFile((PCHAR) (TEST_TEMP_DIR_PATH "asciitest"), FALSE, NULL, &fileSize));

// In Windows systems, the newline character is represented by a combination of 
// two characters: Carriage Return (CR) followed by Line Feed (LF), written as \r\n. 
// So, each newline character in a text file on Windows contributes two bytes to the file size.
#if defined _WIN32 || defined _WIN64    
    EXPECT_EQ(48, fileSize);
#else
    EXPECT_EQ(45, fileSize);
#endif

    remove((PCHAR) (TEST_TEMP_DIR_PATH "asciitest"));
}

TEST_F(FileIoFunctionalityTest, readFile_binaryFileSize) {
    FILE *file = fopen((PCHAR) (TEST_TEMP_DIR_PATH "binarytest"), "wb");
    CHAR fileBuffer[256];
    CHAR data[100] = "This is line 1\nThis is line 2\nThis is line 3\n";
    UINT64 fileSize = ARRAY_SIZE(fileBuffer);

    FWRITE(data, SIZEOF(CHAR), STRLEN(data), file);
    FCLOSE(file);

    EXPECT_EQ(STATUS_SUCCESS, readFile((PCHAR)(TEST_TEMP_DIR_PATH "binarytest"), TRUE, NULL, &fileSize));
    EXPECT_EQ(45, fileSize);

    remove((PCHAR) (TEST_TEMP_DIR_PATH "binarytest"));
}

TEST_F(FileIoFunctionalityTest, readFile_binary) {
    FILE *file = fopen((PCHAR) (TEST_TEMP_DIR_PATH "binarytest"), "wb");
    UINT64 fileSize;
    PCHAR fileBuffer = NULL;
    CHAR data[100] = "This is line 1\nThis is line 2\nThis is line 3\n";
   
    FWRITE(data, SIZEOF(CHAR), STRLEN(data), file);
    FCLOSE(file);

    EXPECT_EQ(STATUS_SUCCESS, readFile((PCHAR) (TEST_TEMP_DIR_PATH "binarytest"), TRUE, NULL, &fileSize));
    EXPECT_EQ(45, fileSize);

    fileBuffer = (PCHAR) MEMCALLOC(1, (fileSize + 1) * SIZEOF(CHAR));
    EXPECT_EQ(STATUS_SUCCESS, readFile((PCHAR) (TEST_TEMP_DIR_PATH "binarytest"), TRUE, (PBYTE) fileBuffer, &fileSize));
    EXPECT_STREQ(data, fileBuffer);

    remove((PCHAR) (TEST_TEMP_DIR_PATH "binarytest"));
    SAFE_MEMFREE(fileBuffer);
}

TEST_F(FileIoFunctionalityTest, readFile_ascii) {
    FILE *file = fopen((PCHAR) (TEST_TEMP_DIR_PATH "asciitest"), "w");
    UINT64 fileSize;
    PCHAR fileBuffer = NULL;
    CHAR data[100] = "This is line 1\nThis is line 2\nThis is line 3\n";
   
    FWRITE(data, SIZEOF(CHAR), STRLEN(data), file);
    FCLOSE(file);

    EXPECT_EQ(STATUS_SUCCESS, readFile((PCHAR) (TEST_TEMP_DIR_PATH "asciitest"), FALSE, NULL, &fileSize));

// In Windows systems, the newline character is represented by a combination of 
// two characters: Carriage Return (CR) followed by Line Feed (LF), written as \r\n. 
// So, each newline character in a text file on Windows contributes two bytes to the file size.
#if defined _WIN32 || defined _WIN64    
    EXPECT_EQ(48, fileSize);
#else
    EXPECT_EQ(45, fileSize);
#endif

    fileBuffer = (PCHAR) MEMCALLOC(1, (fileSize + 1) * SIZEOF(CHAR));
    EXPECT_EQ(STATUS_SUCCESS, readFile((PCHAR) (TEST_TEMP_DIR_PATH "asciitest"), FALSE, (PBYTE) fileBuffer, &fileSize));
    EXPECT_STREQ(data, fileBuffer);

    remove((PCHAR) (TEST_TEMP_DIR_PATH "asciitest"));
    SAFE_MEMFREE(fileBuffer);
}