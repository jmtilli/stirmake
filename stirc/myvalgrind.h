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

#elif defined(__aarch64__)
    // Array layout required by Valgrind's client request mechanism
    // args[0] is the Request Code (0x1001 for RUNNING_ON_VALGRIND)
    // args[1] to args[5] are optional request arguments (not needed here)
    volatile unsigned long args[6] = { 0 };
    args[0] = 0x1001; // VG_USERREQ__RUNNING_ON_VALGRIND

    unsigned long default_val = 0; // The value returned if running natively
    unsigned long result;

    __asm__ __volatile__ (
        "mov x3, %1\n\t"                // Load default value into x3
        "mov x4, %2\n\t"                // Load pointer to args array into x4
        // The Valgrind magic instruction signature for AArch64:
        "ror x12, x12, #3\n\t"
        "ror x12, x12, #13\n\t"
        "ror x12, x12, #51\n\t"
        "ror x12, x12, #61\n\t"         // Net rotation of x12 is 128 bits (no-op)
        "orr x10, x10, x10\n\t"         // Non-destructive pseudo-no-op
        "mov %0, x3\n\t"                // Move result from x3 to output variable
        : "=r" (result)                 // Output operand (%0)
        : "r" (default_val), "r" (args) // Input operands (%1, %2)
        : "cc", "memory", "x3", "x4"    // Clobbered registers
    );

    return result != 0;
#elif defined(__arm__)
    volatile unsigned int args[6];
    unsigned int result;

    // Set up the client request arguments
    args[0] = 0x1001; // VG_USERREQ__RUNNING_ON_VALGRIND
    args[1] = 0;
    args[2] = 0;
    args[3] = 0;
    args[4] = 0;
    args[5] = 0;

    __asm__ __volatile__ (
        // Load default return value (0 = not on Valgrind) into r3
        "mov r3, #0\n\t"
        // Load pointer to arguments array into r4
        "mov r4, %1\n\t"

        // --- VALGRIND 16-BYTE PREAMBLE FOR ARM32 ---
        "mov r12, r12, ror #3\n\t"
        "mov r12, r12, ror #13\n\t"
        "mov r12, r12, ror #29\n\t"
        "mov r12, r12, ror #19\n\t"

        // --- THE TRAPDOOR TRIGGER ---
        // Valgrind JIT intercepts this sequence. If Valgrind is active,
        // it overrides r3 with the actual status (1 if monitored).
        "orr r10, r10, r10\n\t"

        // Move register value out to our C output variable
        "mov %0, r3\n\t"
        : "=r" (result)              /* Output operand (%0) */
        : "r" (args)                 /* Input operand (%1) */
        : "r3", "r4", "r10", "r12", "cc", "memory" /* Clobbered registers */
    );

    return result != 0;
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
