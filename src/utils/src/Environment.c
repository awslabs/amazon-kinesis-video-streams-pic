#include "Include_i.h"

BOOL isEnvVarEnabledWithDefault(PCHAR envVarName, BOOL defaultVal)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BOOL retBool = defaultVal;

    // Null or empty envVarName to GETENV is undefined behavior.
    CHK_ERR(envVarName != NULL && envVarName[0] != '\0', STATUS_NULL_ARG, "Environment variable name is NULL or empty.");

    PCHAR envVarVal = (PCHAR) GETENV(envVarName);
    CHK(envVarVal != NULL, retStatus);

    if (STRCMPI(envVarVal, "1") == 0 || STRCMPI(envVarVal, "true") == 0 || STRCMPI(envVarVal, "on") == 0) {
        retBool = TRUE;
    } else if (STRCMPI(envVarVal, "0") == 0 || STRCMPI(envVarVal, "false") == 0 || STRCMPI(envVarVal, "off") == 0) {
        retBool = FALSE;
    } else {
        DLOGW("Unrecognized value for %s, using default: %d", envVarName, defaultVal);
    }

CleanUp:
    CHK_LOG_ERR(retStatus);
    LEAVES();
    return retBool;
}

BOOL isEnvVarEnabled(PCHAR envVarName)
{
    return isEnvVarEnabledWithDefault(envVarName, FALSE);
}