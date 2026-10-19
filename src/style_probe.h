// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int ioctl_rc;
    int saved_errno;
    uint32_t device_error;
} decrypt_result;
typedef decrypt_result (*decrypt_fn)(void*,int);

int style_probe(const char* input,const char* output,const char* report,
                uint32_t firmware,decrypt_fn decrypt);

#ifdef __cplusplus
}
#endif
