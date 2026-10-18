#ifndef _VALGRIND_H_
#define _VALGRIND_H_

#if defined(__clang__)
// ok
#elif defined(__GNUC__)
// ok
#else
#undef VALGRIND_DETECTION // we don't know if __asm__ works
#endif
#include <stdint.h>

// Assisted-By: Google Gemini

#if defined(VALGRIND_DETECTION)
static inline int my_is_running_on_valgrind(void) {
    // Valgrind client request magic sequence
#if defined(__x86_64__)
    volatile uint64_t args[6];
    unsigned long long res;
    args[0] = 0x1001; // VG_USERREQ__RUNNING_ON_VALGRIND request code
    args[1] = 0;      // arg 1
    args[2] = 0;      // arg 2
    args[3] = 0;      // arg 3
    args[4] = 0;      // arg 4
    args[5] = 0;      // arg 5
    __asm__ __volatile__(
        "rolq $3, %%rdi\n\t"
        "rolq $13, %%rdi\n\t"
        "rolq $61, %%rdi\n\t"
        "rolq $51, %%rdi\n\t"
        "xchgq %%rbx, %%rbx" // Valgrind backdoor interception point
        : "=d" (res)      // Output: Valgrind puts the return value into RDX ("=d")
        : "a" (&args[0]),    // Input: RAX ("a") must point to the arguments array
          "0" (0)            // Input: RDX is initialized to 0 as the default native return value
        : "cc", "memory"     // Tells the compiler condition codes and memory are modified
    );
    return res != 0;

#elif defined(__i386__)
    volatile uint32_t args[6];
    unsigned int res;
    args[0] = 0x1001; // VG_USERREQ__RUNNING_ON_VALGRIND request code
    args[1] = 0;      // arg 1
    args[2] = 0;      // arg 2
    args[3] = 0;      // arg 3
    args[4] = 0;      // arg 4
    args[5] = 0;      // arg 5
    __asm__ __volatile__(
        "roll $3, %%edi\n\t"
        "roll $13, %%edi\n\t"
        "roll $29, %%edi\n\t"
        "roll $19, %%edi\n\t"
        "xchgl %%ebx, %%ebx" // Valgrind backdoor interception point
        : "=d" (res)      // Output: Valgrind puts the return value into EDX ("=d")
        : "a" (&args[0]),       // Input: EAX ("a") must point to the arguments array
          "0" (0)            // Input: EDX ("0" references constraint "=d") initialized to 0
        : "cc", "memory"     // Tells the compiler condition codes and memory are modified
    );
    return res != 0;

#else
    // Fallback if compilation architecture isn't i386/amd64
    return 0;
#endif
}
#else
static inline int my_is_running_on_valgrind(void) {
    return 0;
}
#endif

#endif
