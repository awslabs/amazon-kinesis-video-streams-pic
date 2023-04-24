#include "Include_i.h"

#define AES_256_KEY_LENGTH          32
#define AES_256_KEY_LENGTH_BITS     256
#define AES_256_IV_LENGTH           12
//#define AES_256_IV_GCM_TAG_LENGTH   16

PUBLIC_API STATUS aesCipherSizeNeeded(INT64 inputLength, PINT64 pCipherLength) {
    STATUS retStatus = STATUS_SUCCESS;
    CHK(pCipherLength != NULL, STATUS_NULL_ARG);
    *pCipherLength = ((inputLength + EVP_CIPHER_block_size(EVP_aes_256_cbc()) - 1) / EVP_CIPHER_block_size(EVP_aes_256_cbc())) * EVP_CIPHER_block_size(EVP_aes_256_cbc());
CleanUp:
    return retStatus;
}

PUBLIC_API STATUS aesEncrypt(EVP_CIPHER_CTX * ctx, PBYTE pInput, INT64 inputLength, PBYTE key, PBYTE initialVector, PBYTE pOutput, PINT64 pOutputLength) {
    STATUS retStatus = STATUS_SUCCESS;
    printf("%d\n", __LINE__);
    CHK(ctx != NULL && pInput != NULL && key != NULL && initialVector != NULL && pOutput != NULL && pOutputLength != NULL, STATUS_NULL_ARG);
    printf("%d\n", __LINE__);
    CHK(inputLength > 0, STATUS_INVALID_ARG);

    //CHK(EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, NULL, NULL);

    //EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, idea_IV_LENGTH, NULL);
    ctx = EVP_CIPHER_CTX_new();
    printf("%d\n", __LINE__);
    EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, initialVector);
    printf("%d\n", __LINE__);

    EVP_EncryptUpdate(ctx, pOutput, pOutputLength, pInput, inputLength);
    printf("%d\n", __LINE__);

    EVP_EncryptFinal_ex(ctx, pOutput + *pOutputLength, pOutputLength);

    printf("%d\n", __LINE__);
    EVP_CIPHER_CTX_free(ctx);
    printf("%d\n", __LINE__);
CleanUp:
    if(STATUS_FAILED(retStatus)) {
        char buf[256] = {0};
        int err = ERR_get_error();
        ERR_error_string(ERR_get_error(), buf);
        printf("%d %s\n", err, buf);
    }
    return retStatus;
}

PUBLIC_API STATUS aesDecrypt(EVP_CIPHER_CTX * ctx, PBYTE pInput, INT64 inputLength, PBYTE key, PBYTE initialVector, PBYTE pOutput, PINT64 pOutputLength) {
    STATUS retStatus = STATUS_SUCCESS;
    printf("%d\n", __LINE__);
    CHK(ctx != NULL && pInput != NULL && key != NULL && initialVector != NULL && pOutput != NULL && pOutputLength != NULL, STATUS_NULL_ARG);
    printf("%d\n", __LINE__);
    CHK(inputLength > 0, STATUS_INVALID_ARG);
    //CHK(EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, NULL, NULL);

    //EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, idea_IV_LENGTH, NULL);
    ctx = EVP_CIPHER_CTX_new();
    printf("%d\n", __LINE__);
    EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, initialVector);

    printf("%d\n", __LINE__);
    EVP_DecryptUpdate(ctx, pOutput, pOutputLength, pInput, inputLength);

    printf("%d\n", __LINE__);
    EVP_DecryptFinal_ex(ctx, pOutput + *pOutputLength, pOutputLength);

    printf("%d\n", __LINE__);
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
    RAND_priv_bytes(key, EVP_MAX_KEY_LENGTH); 
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
    RAND_priv_bytes(iv, EVP_MAX_IV_LENGTH); 
CleanUp:
    if(STATUS_FAILED(retStatus)) {
        char buf[256] = {0};
        int err = ERR_get_error();
        ERR_error_string(ERR_get_error(), buf);
        printf("%d %s\n", err, buf);
    }
    return retStatus;
}
