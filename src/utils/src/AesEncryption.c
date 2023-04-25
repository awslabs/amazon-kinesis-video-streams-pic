#include "Include_i.h"

#define AES_256_KEY_LENGTH          32
#define AES_256_KEY_LENGTH_BITS     256
#define AES_256_IV_LENGTH           12
//#define AES_256_IV_GCM_TAG_LENGTH   16

PUBLIC_API STATUS aesCipherSizeNeeded(INT64 inputLength, PINT64 pCipherLength) {
    STATUS retStatus = STATUS_SUCCESS;
    CHK(pCipherLength != NULL, STATUS_NULL_ARG);
    *pCipherLength = ((inputLength + EVP_CIPHER_block_size(EVP_aes_256_cbc()) - 1));
CleanUp:
    return retStatus;
}

PUBLIC_API STATUS aesEncrypt(PBYTE pInput, INT64 inputLength, PBYTE key, PBYTE initialVector, PBYTE pOutput, PINT64 pOutputLength) {
    STATUS retStatus = STATUS_SUCCESS;
    EVP_CIPHER_CTX * ctx;
    INT64 writtenData = 0;
    CHK(pInput != NULL && key != NULL && initialVector != NULL && pOutput != NULL && pOutputLength != NULL, STATUS_NULL_ARG);
    CHK(inputLength > 0, STATUS_INVALID_ARG);

    //CHK(EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, NULL, NULL) == 1, STATUS_INVALID_ARG);

    //EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, idea_IV_LENGTH, NULL);
    ctx = EVP_CIPHER_CTX_new();
    CHK(EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, initialVector) == 1, STATUS_INVALID_ARG);

    CHK(EVP_EncryptUpdate(ctx, pOutput, &writtenData, pInput, inputLength) == 1, STATUS_INVALID_ARG);

    *pOutputLength = writtenData;

    CHK(EVP_EncryptFinal_ex(ctx, pOutput + writtenData, &writtenData) == 1, STATUS_INVALID_ARG);

    *pOutputLength += writtenData;

    EVP_CIPHER_CTX_free(ctx);
CleanUp:
    if(STATUS_FAILED(retStatus)) {
        char buf[256] = {0};
        int err = ERR_get_error();
        ERR_error_string(ERR_get_error(), buf);
        printf("%d %s\n", err, buf);
    }
    return retStatus;
}

PUBLIC_API STATUS aesDecrypt(PBYTE pInput, INT64 inputLength, PBYTE key, PBYTE initialVector, PBYTE pOutput, PINT64 pOutputLength) {
    STATUS retStatus = STATUS_SUCCESS;
    EVP_CIPHER_CTX * ctx;
    CHK(pInput != NULL && key != NULL && initialVector != NULL && pOutput != NULL && pOutputLength != NULL, STATUS_NULL_ARG);
    CHK(inputLength > 0, STATUS_INVALID_ARG);
    //CHK(EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, NULL, NULL) == 1, STATUS_INVALID_ARG);

    //EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, idea_IV_LENGTH, NULL);
    ctx = EVP_CIPHER_CTX_new();
    CHK(EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, initialVector) == 1, STATUS_INVALID_ARG);

    CHK(EVP_DecryptUpdate(ctx, pOutput, pOutputLength, pInput, inputLength) == 1, STATUS_INVALID_ARG);

    CHK(EVP_DecryptFinal_ex(ctx, pOutput + *pOutputLength, pOutputLength) == 1, STATUS_INVALID_ARG);

    EVP_CIPHER_CTX_free(ctx);
CleanUp:
    if(STATUS_FAILED(retStatus)) {
        char buf[256] = {0};
        int err = ERR_get_error();
        ERR_error_string(ERR_get_error(), buf);
        printf("%d %s\n", err, buf);
    }
    return retStatus;
}

PUBLIC_API STATUS aesCreateContext(EVP_CIPHER_CTX ** ctx) {
    STATUS retStatus = STATUS_SUCCESS;
    CHK(ctx != NULL, STATUS_NULL_ARG);
    *ctx = EVP_CIPHER_CTX_new();
CleanUp:
    return retStatus;
}

PUBLIC_API STATUS aesFreeContext(EVP_CIPHER_CTX ** ctx) {
    STATUS retStatus = STATUS_SUCCESS;
    CHK(ctx != NULL && *ctx != NULL, STATUS_NULL_ARG);

    //free and remove access
    EVP_CIPHER_CTX_free(*ctx);
    *ctx = NULL;

CleanUp:
    return retStatus;
}

PUBLIC_API STATUS aesGenerateKey(PBYTE key) {
    STATUS retStatus = STATUS_SUCCESS;
    CHK(RAND_priv_bytes(key, EVP_MAX_KEY_LENGTH) == 1, STATUS_INVALID_ARG); 
CleanUp:
    if(STATUS_FAILED(retStatus)) {
        char buf[256] = {0};
        int err = ERR_get_error();
        ERR_error_string(ERR_get_error(), buf);
        printf("%d %s\n", err, buf);
    }
    return retStatus;
}

PUBLIC_API STATUS aesGenerateIV(PBYTE iv) {
    STATUS retStatus = STATUS_SUCCESS;
    CHK(RAND_priv_bytes(iv, EVP_MAX_IV_LENGTH) == 1, STATUS_INVALID_ARG); 
CleanUp:
    if(STATUS_FAILED(retStatus)) {
        char buf[256] = {0};
        int err = ERR_get_error();
        ERR_error_string(ERR_get_error(), buf);
        printf("%d %s\n", err, buf);
    }
    return retStatus;
}
