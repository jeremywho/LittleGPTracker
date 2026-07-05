// test shim: minimal System for standalone SendBus compilation
#ifndef _SYSTEM_SHIM_H_
#define _SYSTEM_SHIM_H_
#include <cstdlib>
#include <cstring>
#define SYS_MALLOC malloc
#define SYS_FREE free
#define SYS_MEMSET memset
#endif
