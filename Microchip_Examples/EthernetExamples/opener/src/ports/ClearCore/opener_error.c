#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "opener_error.h"
#include "lwip/errno.h"

const int kErrorMessageBufferSize = 255;

int GetSocketErrorNumber(void) {
    return errno;
}

char *GetErrorMessage(int error_number) {
    static char error_message[255];
    snprintf(error_message, sizeof(error_message), "Error %d", error_number);
    return error_message;
}

void FreeErrorMessage(char *error_message) {
    (void) error_message;
}

