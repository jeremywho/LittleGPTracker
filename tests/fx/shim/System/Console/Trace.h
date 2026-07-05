// test shim: minimal Trace for standalone SendBus compilation
#ifndef _TRACE_SHIM_H_
#define _TRACE_SHIM_H_
#include <cstdarg>
#include <cstdio>
struct Trace {
    static void Log(const char *cat, const char *fmt, ...) {
        va_list a;
        va_start(a, fmt);
        printf("[%s] ", cat);
        vprintf(fmt, a);
        va_end(a);
        printf("\n");
    }
    static void Error(const char *fmt, ...) {
        va_list a;
        va_start(a, fmt);
        printf("[ERROR] ");
        vprintf(fmt, a);
        va_end(a);
        printf("\n");
    }
};
#endif
