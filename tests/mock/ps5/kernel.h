// Test-only declarations for the real media registration implementation.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif
int kernel_dynlib_handle(pid_t,const char*,uint32_t*);
intptr_t kernel_dynlib_dlsym(pid_t,uint32_t,const char*);
uint64_t kernel_get_ucred_authid(pid_t);
int kernel_set_ucred_authid(pid_t,uint64_t);
int kernel_get_vmem_protection(pid_t,intptr_t,size_t);
int kernel_mprotect(pid_t,intptr_t,size_t,int);
#ifdef __cplusplus
}
#endif
