/* Copyright 2015, Kenneth MacKay. Licensed under the BSD 2-clause license. */

#ifndef _UECC_ASM_ARM_MULT_SQUARE_H_
#define _UECC_ASM_ARM_MULT_SQUARE_H_

#define FAST_MULT_ASM_5                \
    "add r0, 12 \n\t"                  \
    "add r2, 12 \n\t"                  \
    "ldmia r1!, {r3,r4} \n\t"          \
    "ldmia r2!, {r6,r7} \n\t"          \
                                       \
    "umull r11, r12, r3, r6 \n\t"      \
    "stmia r0!, {r11} \n\t"            \
                                       \
    "mov r10, #0 \n\t"                 \
    "umull r11, r9, r3, r7 \n\t"       \
    "adds r12, r12, r11 \n\t"          \
    "adc r9, r9, #0 \n\t"              \
    "umull r11, r14, r4, r6 \n\t"      \
    "adds r12, r12, r11 \n\t"          \
    "adcs r9, r9, r14 \n\t"            \
    "adc r10, r10, #0 \n\t"            \
    "stmia r0!, {r12} \n\t"            \
                                       \
    "umull r12, r14, r4, r7 \n\t"      \
    "adds r9, r9, r12 \n\t"            \
    "adc r10, r10, r14 \n\t"           \
    "stmia r0!, {r9, r10} \n\t"        \
                                       \
    "sub r0, 28 \n\t"                  \
    "sub r2, 20 \n\t"                  \
    "ldmia r2!, {r6,r7,r8} \n\t"       \
    "ldmia r1!, {r5} \n\t"             \
                                       \
    "umull r11, r12, r3, r6 \n\t"      \
    "stmia r0!, {r11} \n\t"            \
                                       \
    "mov r10, #0 \n\t"                 \
    "umull r11, r9, r3, r7 \n\t"       \
    "adds r12, r12, r11 \n\t"          \
    "adc r9, r9, #0 \n\t"              \
    "umull r11, r14, r4, r6 \n\t"      \
    "adds r12, r12, r11 \n\t"          \
    "adcs r9, r9, r14 \n\t"            \
    "adc r10, r10, #0 \n\t"            \
    "stmia r0!, {r12} \n\t"            \
                                       \
    "mov r11, #0 \n\t"                 \
    "umull r12, r14, r3, r8 \n\t"      \
    "adds r9, r9, r12 \n\t"            \
    "adcs r10, r10, r14 \n\t"          \
    "adc r11, r11, #0 \n\t"            \
    "umull r12, r14, r4, r7 \n\t"      \
    "adds r9, r9, r12 \n\t"            \
    "adcs r10, r10, r14 \n\t"          \
    "adc r11, r11, #0 \n\t"            \
    "umull r12, r14, r5, r6 \n\t"      \
    "adds r9, r9, r12 \n\t"            \
    "adcs r10, r10, r14 \n\t"          \
    "adc r11, r11, #0 \n\t"            \
    "stmia r0!, {r9} \n\t"             \
                                       \
    "ldmia r1!, {r3} \n\t"             \
    "mov r12, #0 \n\t"                 \
    "umull r14, r9, r4, r8 \n\t"       \
    "adds r10, r10, r14 \n\t"          \
    "adcs r11, r11, r9 \n\t"           \
    "adc r12, r12, #0 \n\t"            \
    "umull r14, r9, r5, r7 \n\t"       \
    "adds r10, r10, r14 \n\t"          \
    "adcs r11, r11, r9 \n\t"           \
    "adc r12, r12, #0 \n\t"            \
    "umull r14, r9, r3, r6 \n\t"       \
    "adds r10, r10, r14 \n\t"          \
    "adcs r11, r11, r9 \n\t"           \
    "adc r12, r12, #0 \n\t"            \
    "ldr r14, [r0] \n\t"               \
    "adds r10, r10, r14 \n\t"          \
    "adcs r11, r11, #0 \n\t"           \
    "adc r12, r12, #0 \n\t"            \
    "stmia r0!, {r10} \n\t"            \
                                       \
    "ldmia r1!, {r4} \n\t"             \
    "mov r14, #0 \n\t"                 \
    "umull r9, r10, r5, r8 \n\t"       \
    "adds r11, r11, r9 \n\t"           \
    "adcs r12, r12, r10 \n\t"          \
    "adc r14, r14, #0 \n\t"            \
    "umull r9, r10, r3, r7 \n\t"       \
    "adds r11, r11, r9 \n\t"           \
    "adcs r12, r12, r10 \n\t"          \
    "adc r14, r14, #0 \n\t"            \
    "umull r9, r10, r4, r6 \n\t"       \
    "adds r11, r11, r9 \n\t"           \
    "adcs r12, r12, r10 \n\t"          \
    "adc r14, r14, #0 \n\t"            \
    "ldr r9, [r0] \n\t"                \
    "adds r11, r11, r9 \n\t"           \
    "adcs r12, r12, #0 \n\t"           \
    "adc r14, r14, #0 \n\t"            \
    "stmia r0!, {r11} \n\t"            \
                                       \
    "ldmia r2!, {r6} \n\t"             \
    "mov r9, #0 \n\t"                  \
    "umull r10, r11, r5, r6 \n\t"      \
    "adds r12, r12, r10 \n\t"          \
    "adcs r14, r14, r11 \n\t"          \
    "adc r9, r9, #0 \n\t"              \
    "umull r10, r11, r3, r8 \n\t"      \
    "adds r12, r12, r10 \n\t"          \
    "adcs r14, r14, r11 \n\t"          \
    "adc r9, r9, #0 \n\t"              \
    "umull r10, r11, r4, r7 \n\t"      \
    "adds r12, r12, r10 \n\t"          \
    "adcs r14, r14, r11 \n\t"          \
    "adc r9, r9, #0 \n\t"              \
    "ldr r10, [r0] \n\t"               \
    "adds r12, r12, r10 \n\t"          \
    "adcs r14, r14, #0 \n\t"           \
    "adc r9, r9, #0 \n\t"              \
    "stmia r0!, {r12} \n\t"            \
                                       \
    "ldmia r2!, {r7} \n\t"             \
    "mov r10, #0 \n\t"                 \
    "umull r11, r12, r5, r7 \n\t"      \
    "adds r14, r14, r11 \n\t"          \
    "adcs r9, r9, r12 \n\t"            \
    "adc r10, r10, #0 \n\t"            \
    "umull r11, r12, r3, r6 \n\t"      \
    "adds r14, r14, r11 \n\t"          \
    "adcs r9, r9, r12 \n\t"            \
    "adc r10, r10, #0 \n\t"            \
    "umull r11, r12, r4, r8 \n\t"      \
    "adds r14, r14, r11 \n\t"          \
    "adcs r9, r9, r12 \n\t"            \
    "adc r10, r10, #0 \n\t"            \
    "ldr r11, [r0] \n\t"               \
    "adds r14, r14, r11 \n\t"          \
    "adcs r9, r9, #0 \n\t"             \
    "adc r10, r10, #0 \n\t"            \
    "stmia r0!, {r14} \n\t"            \
                                       \
    "mov r11, #0 \n\t"                 \
    "umull r12, r14, r3, r7 \n\t"      \
    "adds r9, r9, r12 \n\t"            \
    "adcs r10, r10, r14 \n\t"          \
    "adc r11, r11, #0 \n\t"            \
    "umull r12, r14, r4, r6 \n\t"      \
    "adds r9, r9, r12 \n\t"            \
    "adcs r10, r10, r14 \n\t"          \
    "adc r11, r11, #0 \n\t"            \
    "stmia r0!, {r9} \n\t"             \
                                       \
    "umull r14, r9, r4, r7 \n\t"       \
    "adds r10, r10, r14 \n\t"          \
    "adc r11, r11, r9 \n\t"            \
    "stmia r0!, {r10, r11} \n\t"

#define FAST_MULT_ASM_6             \
    "add r0, 12 \n\t"               \
    "add r2, 12 \n\t"               \
    "ldmia r1!, {r3,r4,r5} \n\t"    \
    "ldmia r2!, {r6,r7,r8} \n\t"    \
                                    \
    "umull r11, r12, r3, r6 \n\t"   \
    "stmia r0!, {r11} \n\t"         \
                                    \
    "mov r10, #0 \n\t"              \
    "umull r11, r9, r3, r7 \n\t"    \
    "adds r12, r12, r11 \n\t"       \
    "adc r9, r9, #0 \n\t"           \
    "umull r11, r14, r4, r6 \n\t"   \
    "adds r12, r12, r11 \n\t"       \
    "adcs r9, r9, r14 \n\t"         \
    "adc r10, r10, #0 \n\t"         \
    "stmia r0!, {r12} \n\t"         \
                                    \
    "mov r11, #0 \n\t"              \
    "umull r12, r14, r3, r8 \n\t"   \
    "adds r9, r9, r12 \n\t"         \
    "adcs r10, r10, r14 \n\t"       \
    "adc r11, r11, #0 \n\t"         \
    "umull r12, r14, r4, r7 \n\t"   \
    "adds r9, r9, r12 \n\t"         \
    "adcs r10, r10, r14 \n\t"       \
    "adc r11, r11, #0 \n\t"         \
    "umull r12, r14, r5, r6 \n\t"   \
    "adds r9, r9, r12 \n\t"         \
    "adcs r10, r10, r14 \n\t"       \
    "adc r11, r11, #0 \n\t"         \
    "stmia r0!, {r9} \n\t"          \
                                    \
    "mov r12, #0 \n\t"              \
    "umull r14, r9, r4, r8 \n\t"    \
    "adds r10, r10, r14 \n\t"       \
    "adcs r11, r11, r9 \n\t"        \
    "adc r12, r12, #0 \n\t"         \
    "umull r14, r9, r5, r7 \n\t"    \
    "adds r10, r10, r14 \n\t"       \
    "adcs r11, r11, r9 \n\t"        \
    "adc r12, r12, #0 \n\t"         \
    "stmia r0!, {r10} \n\t"         \
                                    \
    "umull r9, r10, r5, r8 \n\t"    \
    "adds r11, r11, r9 \n\t"        \
    "adc r12, r12, r10 \n\t"        \
    "stmia r0!, {r11, r12} \n\t"    \
                                    \
    "sub r0, 36 \n\t"               \
    "sub r2, 24 \n\t"               \
    "ldmia r2!, {r6,r7,r8} \n\t"    \
                                    \
    "umull r11, r12, r3, r6 \n\t"   \
    "stmia r0!, {r11} \n\t"         \
                                    \
    "mov r10, #0 \n\t"              \
    "umull r11, r9, r3, r7 \n\t"    \
    "adds r12, r12, r11 \n\t"       \
    "adc r9, r9, #0 \n\t"           \
    "umull r11, r14, r4, r6 \n\t"   \
    "adds r12, r12, r11 \n\t"       \
    "adcs r9, r9, r14 \n\t"         \
    "adc r10, r10, #0 \n\t"         \
    "stmia r0!, {r12} \n\t"         \
                                    \
    "mov r11, #0 \n\t"              \
    "umull r12, r14, r3, r8 \n\t"   \
    "adds r9, r9, r12 \n\t"         \
    "adcs r10, r10, r14 \n\t"       \
    "adc r11, r11, #0 \n\t"         \
    "umull r12, r14, r4, r7 \n\t"   \
    "adds r9, r9, r12 \n\t"         \
    "adcs r10, r10, r14 \n\t"       \
    "adc r11, r11, #0 \n\t"         \
    "umull r12, r14, r5, r6 \n\t"   \
    "adds r9, r9, r12 \n\t"         \
    "adcs r10, r10, r14 \n\t"       \
    "adc r11, r11, #0 \n\t"         \
    "stmia r0!, {r9} \n\t"          \
                                    \
    "ldmia r1!, {r3} \n\t"          \
    "mov r12, #0 \n\t"              \
    "umull r14, r9, r4, r8 \n\t"    \
    "adds r10, r10, r14 \n\t"       \
    "adcs r11, r11, r9 \n\t"        \
    "adc r12, r12, #0 \n\t"         \
    "umull r14, r9, r5, r7 \n\t"    \
    "adds r10, r10, r14 \n\t"       \
    "adcs r11, r11, r9 \n\t"        \
    "adc r12, r12, #0 \n\t"         \
    "umull r14, r9, r3, r6 \n\t"    \
    "adds r10, r10, r14 \n\t"       \
    "adcs r11, r11, r9 \n\t"        \
    "adc r12, r12, #0 \n\t"         \
    "ldr r14, [r0] \n\t"            \
    "adds r10, r10, r14 \n\t"       \
    "adcs r11, r11, #0 \n\t"        \
    "adc r12, r12, #0 \n\t"         \
    "stmia r0!, {r10} \n\t"         \
                                    \
    "ldmia r1!, {r4} \n\t"          \
    "mov r14, #0 \n\t"              \
    "umull r9, r10, r5, r8 \n\t"    \
    "adds r11, r11, r9 \n\t"        \
    "adcs r12, r12, r10 \n\t"       \
    "adc r14, r14, #0 \n\t"         \
    "umull r9, r10, r3, r7 \n\t"    \
    "adds r11, r11, r9 \n\t"        \
    "adcs r12, r12, r10 \n\t"       \
    "adc r14, r14, #0 \n\t"         \
    "umull r9, r10, r4, r6 \n\t"    \
    "adds r11, r11, r9 \n\t"        \
    "adcs r12, r12, r10 \n\t"       \
    "adc r14, r14, #0 \n\t"         \
    "ldr r9, [r0] \n\t"             \
    "adds r11, r11, r9 \n\t"        \
    "adcs r12, r12, #0 \n\t"        \
    "adc r14, r14, #0 \n\t"         \
    "stmia r0!, {r11} \n\t"         \
                                    \
    "ldmia r1!, {r5} \n\t"          \
    "mov r9, #0 \n\t"               \
    "umull r10, r11, r3, r8 \n\t"   \
    "adds r12, r12, r10 \n\t"       \
    "adcs r14, r14, r11 \n\t"       \
    "adc r9, r9, #0 \n\t"           \
    "umull r10, r11, r4, r7 \n\t"   \
    "adds r12, r12, r10 \n\t"       \
    "adcs r14, r14, r11 \n\t"       \
    "adc r9, r9, #0 \n\t"           \
    "umull r10, r11, r5, r6 \n\t"   \
    "adds r12, r12, r10 \n\t"       \
    "adcs r14, r14, r11 \n\t"       \
    "adc r9, r9, #0 \n\t"           \
    "ldr r10, [r0] \n\t"            \
    "adds r12, r12, r10 \n\t"       \
    "adcs r14, r14, #0 \n\t"        \
    "adc r9, r9, #0 \n\t"           \
    "stmia r0!, {r12} \n\t"         \
                                    \
    "ldmia r2!, {r6} \n\t"          \
    "mov r10, #0 \n\t"              \
    "umull r11, r12, r3, r6 \n\t"   \
    "adds r14, r14, r11 \n\t"       \
    "adcs r9, r9, r12 \n\t"         \
    "adc r10, r10, #0 \n\t"         \
    "umull r11, r12, r4, r8 \n\t"   \
    "adds r14, r14, r11 \n\t"       \
    "adcs r9, r9, r12 \n\t"         \
    "adc r10, r10, #0 \n\t"         \
    "umull r11, r12, r5, r7 \n\t"   \
    "adds r14, r14, r11 \n\t"       \
    "adcs r9, r9, r12 \n\t"         \
    "adc r10, r10, #0 \n\t"         \
    "ldr r11, [r0] \n\t"            \
    "adds r14, r14, r11 \n\t"       \
    "adcs r9, r9, #0 \n\t"          \
    "adc r10, r10, #0 \n\t"         \
    "stmia r0!, {r14} \n\t"         \
                                    \
    "ldmia r2!, {r7} \n\t"          \
    "mov r11, #0 \n\t"              \
    "umull r12, r14, r3, r7 \n\t"   \
    "adds r9, r9, r12 \n\t"         \
    "adcs r10, r10, r14 \n\t"       \
    "adc r11, r11, #0 \n\t"         \
    "umull r12, r14, r4, r6 \n\t"   \
    "adds r9, r9, r12 \n\t"         \
    "adcs r10, r10, r14 \n\t"       \
    "adc r11, r11, #0 \n\t"         \
    "umull r12, r14, r5, r8 \n\t"   \
    "adds r9, r9, r12 \n\t"         \
    "adcs r10, r10, r14 \n\t"       \
    "adc r11, r11, #0 \n\t"         \
    "ldr r12, [r0] \n\t"            \
    "adds r9, r9, r12 \n\t"         \
    "adcs r10, r10, #0 \n\t"        \
    "adc r11, r11, #0 \n\t"         \
    "stmia r0!, {r9} \n\t"          \
                                    \
    "ldmia r2!, {r8} \n\t"          \
    "mov r12, #0 \n\t"              \
    "umull r14, r9, r3, r8 \n\t"    \
    "adds r10, r10, r14 \n\t"       \
    "adcs r11, r11, r9 \n\t"        \
    "adc r12, r12, #0 \n\t"         \
    "umull r14, r9, r4, r7 \n\t"    \
    "adds r10, r10, r14 \n\t"       \
    "adcs r11, r11, r9 \n\t"        \
    "adc r12, r12, #0 \n\t"         \
    "umull r14, r9, r5, r6 \n\t"    \
    "adds r10, r10, r14 \n\t"       \
    "adcs r11, r11, r9 \n\t"        \
    "adc r12, r12, #0 \n\t"         \
    "ldr r14, [r0] \n\t"            \
    "adds r10, r10, r14 \n\t"       \
    "adcs r11, r11, #0 \n\t"        \
    "adc r12, r12, #0 \n\t"         \
    "stmia r0!, {r10} \n\t"         \
                                    \
    "mov r14, #0 \n\t"              \
    "umull r9, r10, r4, r8 \n\t"    \
    "adds r11, r11, r9 \n\t"        \
    "adcs r12, r12, r10 \n\t"       \
    "adc r14, r14, #0 \n\t"         \
    "umull r9, r10, r5, r7 \n\t"    \
    "adds r11, r11, r9 \n\t"        \
    "adcs r12, r12, r10 \n\t"       \
    "adc r14, r14, #0 \n\t"         \
    "stmia r0!, {r11} \n\t"         \
                                    \
    "umull r10, r11, r5, r8 \n\t"   \
    "adds r12, r12, r10 \n\t"       \
    "adc r14, r14, r11 \n\t"        \
    "stmia r0!, {r12, r14} \n\t"

#define FAST_MULT_ASM_7                \
    "add r0, 24 \n\t"                  \
    "add r2, 24 \n\t"                  \
    "ldmia r1!, {r3} \n\t"             \
    "ldmia r2!, {r6} \n\t"             \
                                       \
    "umull r9, r10, r3, r6 \n\t"       \
    "stmia r0!, {r9, r10} \n\t"        \
                                       \
    "sub r0, 20 \n\t"                  \
    "sub r2, 16 \n\t"                  \
    "ldmia r2!, {r6, r7, r8} \n\t"     \
    "ldmia r1!, {r4, r5} \n\t"         \
                                       \
    "umull r9, r10, r3, r6 \n\t"       \
    "stmia r0!, {r9} \n\t"             \
                                       \
    "mov r14, #0 \n\t"                 \
    "umull r9, r12, r3, r7 \n\t"       \
    "adds r10, r10, r9 \n\t"           \
    "adc r12, r12, #0 \n\t"            \
    "umull r9, r11, r4, r6 \n\t"       \
    "adds r10, r10, r9 \n\t"           \
    "adcs r12, r12, r11 \n\t"          \
    "adc r14, r14, #0 \n\t"            \
    "stmia r0!, {r10} \n\t"            \
                                       \
    "mov r9, #0 \n\t"                  \
    "umull r10, r11, r3, r8 \n\t"      \
    "adds r12, r12, r10 \n\t"          \
    "adcs r14, r14, r11 \n\t"          \
    "adc r9, r9, #0 \n\t"              \
    "umull r10, r11, r4, r7 \n\t"      \
    "adds r12, r12, r10 \n\t"          \
    "adcs r14, r14, r11 \n\t"          \
    "adc r9, r9, #0 \n\t"              \
    "umull r10, r11, r5, r6 \n\t"      \
    "adds r12, r12, r10 \n\t"          \
    "adcs r14, r14, r11 \n\t"          \
    "adc r9, r9, #0 \n\t"              \
    "stmia r0!, {r12} \n\t"            \
                                       \
    "ldmia r1!, {r3} \n\t"             \
    "mov r10, #0 \n\t"                 \
    "umull r11, r12, r4, r8 \n\t"      \
    "adds r14, r14, r11 \n\t"          \
    "adcs r9, r9, r12 \n\t"            \
    "adc r10, r10, #0 \n\t"            \
    "umull r11, r12, r5, r7 \n\t"      \
    "adds r14, r14, r11 \n\t"          \
    "adcs r9, r9, r12 \n\t"            \
    "adc r10, r10, #0 \n\t"            \
    "umull r11, r12, r3, r6 \n\t"      \
    "adds r14, r14, r11 \n\t"          \
    "adcs r9, r9, r12 \n\t"            \
    "adc r10, r10, #0 \n\t"            \
    "ldr r11, [r0] \n\t"               \
    "adds r14, r14, r11 \n\t"          \
    "adcs r9, r9, #0 \n\t"             \
    "adc r10, r10, #0 \n\t"            \
    "stmia r0!, {r14} \n\t"            \
                                       \
    "ldmia r2!, {r6} \n\t"             \
    "mov r11, #0 \n\t"                 \
    "umull r12, r14, r4, r6 \n\t"      \
    "adds r9, r9, r12 \n\t"            \
    "adcs r10, r10, r14 \n\t"          \
    "adc r11, r11, #0 \n\t"            \
    "umull r12, r14, r5, r8 \n\t"      \
    "adds r9, r9, r12 \n\t"            \
    "adcs r10, r10, r14 \n\t"          \
    "adc r11, r11, #0 \n\t"            \
    "umull r12, r14, r3, r7 \n\t"      \
    "adds r9, r9, r12 \n\t"            \
    "adcs r10, r10, r14 \n\t"          \
    "adc r11, r11, #0 \n\t"            \
    "ldr r12, [r0] \n\t"               \
    "adds r9, r9, r12 \n\t"            \
    "adcs r10, r10, #0 \n\t"           \
    "adc r11, r11, #0 \n\t"            \
    "stmia r0!, {r9} \n\t"             \
                                       \
    "mov r12, #0 \n\t"                 \
    "umull r14, r9, r5, r6 \n\t"       \
    "adds r10, r10, r14 \n\t"          \
    "adcs r11, r11, r9 \n\t"           \
    "adc r12, r12, #0 \n\t"            \
    "umull r14, r9, r3, r8 \n\t"       \
    "adds r10, r10, r14 \n\t"          \
    "adcs r11, r11, r9 \n\t"           \
    "adc r12, r12, #0 \n\t"            \
    "stmia r0!, {r10} \n\t"            \
                                       \
    "umull r9, r10, r3, r6 \n\t"       \
    "adds r11, r11, r9 \n\t"           \
    "adc r12, r12, r10 \n\t"           \
    "stmia r0!, {r11, r12} \n\t"       \
                                       \
    "sub r0, 44 \n\t"                  \
    "sub r1, 16 \n\t"                  \
    "sub r2, 28 \n\t"                  \
    "ldmia r1!, {r3,r4,r5} \n\t"       \
    "ldmia r2!, {r6,r7,r8} \n\t"       \
                                       \
    "umull r9, r10, r3, r6 \n\t"       \
    "stmia r0!, {r9} \n\t"             \
                                       \
    "mov r14, #0 \n\t"                 \
    "umull r9, r12, r3, r7 \n\t"       \
    "adds r10, r10, r9 \n\t"           \
    "adc r12, r12, #0 \n\t"            \
    "umull r9, r11, r4, r6 \n\t"       \
    "adds r10, r10, r9 \n\t"           \
    "adcs r12, r12, r11 \n\t"          \
    "adc r14, r14, #0 \n\t"            \
    "stmia r0!, {r10} \n\t"            \
                                       \
    "mov r9, #0 \n\t"                  \
    "umull r10, r11, r3, r8 \n\t"      \
    "adds r12, r12, r10 \n\t"          \
    "adcs r14, r14, r11 \n\t"          \
    "adc r9, r9, #0 \n\t"              \
    "umull r10, r11, r4, r7 \n\t"      \
    "adds r12, r12, r10 \n\t"          \
    "adcs r14, r14, r11 \n\t"          \
    "adc r9, r9, #0 \n\t"              \
    "umull r10, r11, r5, r6 \n\t"      \
    "adds r12, r12, r10 \n\t"          \
    "adcs r14, r14, r11 \n\t"          \
    "adc r9, r9, #0 \n\t"              \
    "stmia r0!, {r12} \n\t"            \
                                       \
    "ldmia r1!, {r3} \n\t"             \
    "mov r10, #0 \n\t"                 \
    "umull r11, r12, r4, r8 \n\t"      \
    "adds r14, r14, r11 \n\t"          \
    "adcs r9, r9, r12 \n\t"            \
    "adc r10, r10, #0 \n\t"            \
    "umull r11, r12, r5, r7 \n\t"      \
    "adds r14, r14, r11 \n\t"          \
    "adcs r9, r9, r12 \n\t"            \
    "adc r10, r10, #0 \n\t"            \
    "umull r11, r12, r3, r6 \n\t"      \
    "adds r14, r14, r11 \n\t"          \
    "adcs r9, r9, r12 \n\t"            \
    "adc r10, r10, #0 \n\t"            \
    "ldr r11, [r0] \n\t"               \
    "adds r14, r14, r11 \n\t"          \
    "adcs r9, r9, #0 \n\t"             \
    "adc r10, r10, #0 \n\t"            \
    "stmia r0!, {r14} \n\t"            \
                                       \
    "ldmia r1!, {r4} \n\t"             \
    "mov r11, #0 \n\t"                 \
    "umull r12, r14, r5, r8 \n\t"      \
    "adds r9, r9, r12 \n\t"            \
    "adcs r10, r10, r14 \n\t"          \
    "adc r11, r11, #0 \n\t"            \
    "umull r12, r14, r3, r7 \n\t"      \
    "adds r9, r9, r12 \n\t"            \
    "adcs r10, r10, r14 \n\t"          \
    "adc r11, r11, #0 \n\t"            \
    "umull r12, r14, r4, r6 \n\t"      \
    "adds r9, r9, r12 \n\t"            \
    "adcs r10, r10, r14 \n\t"          \
    "adc r11, r11, #0 \n\t"            \
    "ldr r12, [r0] \n\t"               \
    "adds r9, r9, r12 \n\t"            \
    "adcs r10, r10, #0 \n\t"           \
    "adc r11, r11, #0 \n\t"            \
    "stmia r0!, {r9} \n\t"             \
                                       \
    "ldmia r1!, {r5} \n\t"             \
    "mov r12, #0 \n\t"                 \
    "umull r14, r9, r3, r8 \n\t"       \
    "adds r10, r10, r14 \n\t"          \
    "adcs r11, r11, r9 \n\t"           \
    "adc r12, r12, #0 \n\t"            \
    "umull r14, r9, r4, r7 \n\t"       \
    "adds r10, r10, r14 \n\t"          \
    "adcs r11, r11, r9 \n\t"           \
    "adc r12, r12, #0 \n\t"            \
    "umull r14, r9, r5, r6 \n\t"       \
    "adds r10, r10, r14 \n\t"          \
    "adcs r11, r11, r9 \n\t"           \
    "adc r12, r12, #0 \n\t"            \
    "ldr r14, [r0] \n\t"               \
    "adds r10, r10, r14 \n\t"          \
    "adcs r11, r11, #0 \n\t"           \
    "adc r12, r12, #0 \n\t"            \
    "stmia r0!, {r10} \n\t"            \
                                       \
    "ldmia r1!, {r3} \n\t"             \
    "mov r14, #0 \n\t"                 \
    "umull r9, r10, r4, r8 \n\t"       \
    "adds r11, r11, r9 \n\t"           \
    "adcs r12, r12, r10 \n\t"          \
    "adc r14, r14, #0 \n\t"            \
    "umull r9, r10, r5, r7 \n\t"       \
    "adds r11, r11, r9 \n\t"           \
    "adcs r12, r12, r10 \n\t"          \
    "adc r14, r14, #0 \n\t"            \
    "umull r9, r10, r3, r6 \n\t"       \
    "adds r11, r11, r9 \n\t"           \
    "adcs r12, r12, r10 \n\t"          \
    "adc r14, r14, #0 \n\t"            \
    "ldr r9, [r0] \n\t"                \
    "adds r11, r11, r9 \n\t"           \
    "adcs r12, r12, #0 \n\t"           \
    "adc r14, r14, #0 \n\t"            \
    "stmia r0!, {r11} \n\t"            \
                                       \
    "ldmia r2!, {r6} \n\t"             \
    "mov r9, #0 \n\t"                  \
    "umull r10, r11, r4, r6 \n\t"      \
    "adds r12, r12, r10 \n\t"          \
    "adcs r14, r14, r11 \n\t"          \
    "adc r9, r9, #0 \n\t"              \
    "umull r10, r11, r5, r8 \n\t"      \
    "adds r12, r12, r10 \n\t"          \
    "adcs r14, r14, r11 \n\t"          \
    "adc r9, r9, #0 \n\t"              \
    "umull r10, r11, r3, r7 \n\t"      \
    "adds r12, r12, r10 \n\t"          \
    "adcs r14, r14, r11 \n\t"          \
    "adc r9, r9, #0 \n\t"              \
    "ldr r10, [r0] \n\t"               \
    "adds r12, r12, r10 \n\t"          \
    "adcs r14, r14, #0 \n\t"           \
    "adc r9, r9, #0 \n\t"              \
    "stmia r0!, {r12} \n\t"            \
                                       \
    "ldmia r2!, {r7} \n\t"             \
    "mov r10, #0 \n\t"                 \
    "umull r11, r12, r4, r7 \n\t"      \
    "adds r14, r14, r11 \n\t"          \
    "adcs r9, r9, r12 \n\t"            \
    "adc r10, r10, #0 \n\t"            \
    "umull r11, r12, r5, r6 \n\t"      \
    "adds r14, r14, r11 \n\t"          \
    "adcs r9, r9, r12 \n\t"            \
    "adc r10, r10, #0 \n\t"            \
    "umull r11, r12, r3, r8 \n\t"      \
    "adds r14, r14, r11 \n\t"          \
    "adcs r9, r9, r12 \n\t"            \
    "adc r10, r10, #0 \n\t"            \
    "ldr r11, [r0] \n\t"               \
    "adds r14, r14, r11 \n\t"          \
    "adcs r9, r9, #0 \n\t"             \
    "adc r10, r10, #0 \n\t"            \
    "stmia r0!, {r14} \n\t"            \
                                       \
    "ldmia r2!, {r8} \n\t"             \
    "mov r11, #0 \n\t"                 \
    "umull r12, r14, r4, r8 \n\t"      \
    "adds r9, r9, r12 \n\t"            \
    "adcs r10, r10, r14 \n\t"          \
    "adc r11, r11, #0 \n\t"            \
    "umull r12, r14, r5, r7 \n\t"      \
    "adds r9, r9, r12 \n\t"            \
    "adcs r10, r10, r14 \n\t"          \
    "adc r11, r11, #0 \n\t"            \
    "umull r12, r14, r3, r6 \n\t"      \
    "adds r9, r9, r12 \n\t"            \
    "adcs r10, r10, r14 \n\t"          \
    "adc r11, r11, #0 \n\t"            \
    "ldr r12, [r0] \n\t"               \
    "adds r9, r9, r12 \n\t"            \
    "adcs r10, r10, #0 \n\t"           \
    "adc r11, r11, #0 \n\t"            \
    "stmia r0!, {r9} \n\t"             \
                                       \
    "ldmia r2!, {r6} \n\t"             \
    "mov r12, #0 \n\t"                 \
    "umull r14, r9, r4, r6 \n\t"       \
    "adds r10, r10, r14 \n\t"          \
    "adcs r11, r11, r9 \n\t"           \
    "adc r12, r12, #0 \n\t"            \
    "umull r14, r9, r5, r8 \n\t"       \
    "adds r10, r10, r14 \n\t"          \
    "adcs r11, r11, r9 \n\t"           \
    "adc r12, r12, #0 \n\t"            \
    "umull r14, r9, r3, r7 \n\t"       \
    "adds r10, r10, r14 \n\t"          \
    "adcs r11, r11, r9 \n\t"           \
    "adc r12, r12, #0 \n\t"            \
    "ldr r14, [r0] \n\t"               \
    "adds r10, r10, r14 \n\t"          \
    "adcs r11, r11, #0 \n\t"           \
    "adc r12, r12, #0 \n\t"            \
    "stmia r0!, {r10} \n\t"            \
                                       \
    "mov r14, #0 \n\t"                 \
    "umull r9, r10, r5, r6 \n\t"       \
    "adds r11, r11, r9 \n\t"           \
    "adcs r12, r12, r10 \n\t"          \
    "adc r14, r14, #0 \n\t"            \
    "umull r9, r10, r3, r8 \n\t"       \
    "adds r11, r11, r9 \n\t"           \
    "adcs r12, r12, r10 \n\t"          \
    "adc r14, r14, #0 \n\t"            \
    "stmia r0!, {r11} \n\t"            \
                                       \
    "umull r10, r11, r3, r6 \n\t"      \
    "adds r12, r12, r10 \n\t"          \
    "adc r14, r14, r11 \n\t"           \
    "stmia r0!, {r12, r14} \n\t"

#define FAST_MULT_ASM_8             \
    "add r0, 24 \n\t"               \
    "add r2, 24 \n\t"               \
    "ldmia r1!, {r3,r4} \n\t"       \
    "ldmia r2!, {r6,r7} \n\t"       \
                                    \
    "umull r11, r12, r3, r6 \n\t"   \
    "stmia r0!, {r11} \n\t"         \
                                    \
    "mov r10, #0 \n\t"              \
    "umull r11, r9, r3, r7 \n\t"    \
    "adds r12, r12, r11 \n\t"       \
    "adc r9, r9, #0 \n\t"           \
    "umull r11, r14, r4, r6 \n\t"   \
    "adds r12, r12, r11 \n\t"       \
    "adcs r9, r9, r14 \n\t"         \
    "adc r10, r10, #0 \n\t"         \
    "stmia r0!, {r12} \n\t"         \
                                    \
    "umull r12, r14, r4, r7 \n\t"   \
    "adds r9, r9, r12 \n\t"         \
    "adc r10, r10, r14 \n\t"        \
    "stmia r0!, {r9, r10} \n\t"     \
                                    \
    "sub r0, 28 \n\t"               \
    "sub r2, 20 \n\t"               \
    "ldmia r2!, {r6,r7,r8} \n\t"    \
    "ldmia r1!, {r5} \n\t"          \
                                    \
    "umull r11, r12, r3, r6 \n\t"   \
    "stmia r0!, {r11} \n\t"         \
                                    \
    "mov r10, #0 \n\t"              \
    "umull r11, r9, r3, r7 \n\t"    \
    "adds r12, r12, r11 \n\t"       \
    "adc r9, r9, #0 \n\t"           \
    "umull r11, r14, r4, r6 \n\t"   \
    "adds r12, r12, r11 \n\t"       \
    "adcs r9, r9, r14 \n\t"         \
    "adc r10, r10, #0 \n\t"         \
    "stmia r0!, {r12} \n\t"         \
                                    \
    "mov r11, #0 \n\t"              \
    "umull r12, r14, r3, r8 \n\t"   \
    "adds r9, r9, r12 \n\t"         \
    "adcs r10, r10, r14 \n\t"       \
    "adc r11, r11, #0 \n\t"         \
    "umull r12, r14, r4, r7 \n\t"   \
    "adds r9, r9, r12 \n\t"         \
    "adcs r10, r10, r14 \n\t"       \
    "adc r11, r11, #0 \n\t"         \
    "umull r12, r14, r5, r6 \n\t"   \
    "adds r9, r9, r12 \n\t"         \
    "adcs r10, r10, r14 \n\t"       \
    "adc r11, r11, #0 \n\t"         \
    "stmia r0!, {r9} \n\t"          \
                                    \
    "ldmia r1!, {r3} \n\t"          \
    "mov r12, #0 \n\t"              \
    "umull r14, r9, r4, r8 \n\t"    \
    "adds r10, r10, r14 \n\t"       \
    "adcs r11, r11, r9 \n\t"        \
    "adc r12, r12, #0 \n\t"         \
    "umull r14, r9, r5, r7 \n\t"    \
    "adds r10, r10, r14 \n\t"       \
    "adcs r11, r11, r9 \n\t"        \
    "adc r12, r12, #0 \n\t"         \
    "umull r14, r9, r3, r6 \n\t"    \
    "adds r10, r10, r14 \n\t"       \
    "adcs r11, r11, r9 \n\t"        \
    "adc r12, r12, #0 \n\t"         \
    "ldr r14, [r0] \n\t"            \
    "adds r10, r10, r14 \n\t"       \
    "adcs r11, r11, #0 \n\t"        \
    "adc r12, r12, #0 \n\t"         \
    "stmia r0!, {r10} \n\t"         \
                                    \
    "ldmia r1!, {r4} \n\t"          \
    "mov r14, #0 \n\t"              \
    "umull r9, r10, r5, r8 \n\t"    \
    "adds r11, r11, r9 \n\t"        \
    "adcs r12, r12, r10 \n\t"       \
    "adc r14, r14, #0 \n\t"         \
    "umull r9, r10, r3, r7 \n\t"    \
    "adds r11, r11, r9 \n\t"        \
    "adcs r12, r12, r10 \n\t"       \
    "adc r14, r14, #0 \n\t"         \
    "umull r9, r10, r4, r6 \n\t"    \
    "adds r11, r11, r9 \n\t"        \
    "adcs r12, r12, r10 \n\t"       \
    "adc r14, r14, #0 \n\t"         \
    "ldr r9, [r0] \n\t"             \
    "adds r11, r11, r9 \n\t"        \
    "adcs r12, r12, #0 \n\t"        \
    "adc r14, r14, #0 \n\t"         \
    "stmia r0!, {r11} \n\t"         \
                                    \
    "ldmia r2!, {r6} \n\t"          \
    "mov r9, #0 \n\t"               \
    "umull r10, r11, r5, r6 \n\t"   \
    "adds r12, r12, r10 \n\t"       \
    "adcs r14, r14, r11 \n\t"       \
    "adc r9, r9, #0 \n\t"           \
    "umull r10, r11, r3, r8 \n\t"   \
    "adds r12, r12, r10 \n\t"       \
    "adcs r14, r14, r11 \n\t"       \
    "adc r9, r9, #0 \n\t"           \
    "umull r10, r11, r4, r7 \n\t"   \
    "adds r12, r12, r10 \n\t"       \
    "adcs r14, r14, r11 \n\t"       \
    "adc r9, r9, #0 \n\t"           \
    "ldr r10, [r0] \n\t"            \
    "adds r12, r12, r10 \n\t"       \
    "adcs r14, r14, #0 \n\t"        \
    "adc r9, r9, #0 \n\t"           \
    "stmia r0!, {r12} \n\t"         \
                                    \
    "ldmia r2!, {r7} \n\t"          \
    "mov r10, #0 \n\t"              \
    "umull r11, r12, r5, r7 \n\t"   \
    "adds r14, r14, r11 \n\t"       \
    "adcs r9, r9, r12 \n\t"         \
    "adc r10, r10, #0 \n\t"         \
    "umull r11, r12, r3, r6 \n\t"   \
    "adds r14, r14, r11 \n\t"       \
    "adcs r9, r9, r12 \n\t"         \
    "adc r10, r10, #0 \n\t"         \
    "umull r11, r12, r4, r8 \n\t"   \
    "adds r14, r14, r11 \n\t"       \
    "adcs r9, r9, r12 \n\t"         \
    "adc r10, r10, #0 \n\t"         \
    "ldr r11, [r0] \n\t"            \
    "adds r14, r14, r11 \n\t"       \
    "adcs r9, r9, #0 \n\t"          \
    "adc r10, r10, #0 \n\t"         \
    "stmia r0!, {r14} \n\t"         \
                                    \
    "mov r11, #0 \n\t"              \
    "umull r12, r14, r3, r7 \n\t"   \
    "adds r9, r9, r12 \n\t"         \
    "adcs r10, r10, r14 \n\t"       \
    "adc r11, r11, #0 \n\t"         \
    "umull r12, r14, r4, r6 \n\t"   \
    "adds r9, r9, r12 \n\t"         \
    "adcs r10, r10, r14 \n\t"       \
    "adc r11, r11, #0 \n\t"         \
    "stmia r0!, {r9} \n\t"          \
                                    \
    "umull r14, r9, r4, r7 \n\t"    \
    "adds r10, r10, r14 \n\t"       \
    "adc r11, r11, r9 \n\t"         \
    "stmia r0!, {r10, r11} \n\t"    \
                                    \
    "sub r0, 52 \n\t"               \
    "sub r1, 20 \n\t"               \
    "sub r2, 32 \n\t"               \
    "ldmia r1!, {r3,r4,r5} \n\t"    \
    "ldmia r2!, {r6,r7,r8} \n\t"    \
                                    \
    "umull r11, r12, r3, r6 \n\t"   \
    "stmia r0!, {r11} \n\t"         \
                                    \
    "mov r10, #0 \n\t"              \
    "umull r11, r9, r3, r7 \n\t"    \
    "adds r12, r12, r11 \n\t"       \
    "adc r9, r9, #0 \n\t"           \
    "umull r11, r14, r4, r6 \n\t"   \
    "adds r12, r12, r11 \n\t"       \
    "adcs r9, r9, r14 \n\t"         \
    "adc r10, r10, #0 \n\t"         \
    "stmia r0!, {r12} \n\t"         \
                                    \
    "mov r11, #0 \n\t"              \
    "umull r12, r14, r3, r8 \n\t"   \
    "adds r9, r9, r12 \n\t"         \
    "adcs r10, r10, r14 \n\t"       \
    "adc r11, r11, #0 \n\t"         \
    "umull r12, r14, r4, r7 \n\t"   \
    "adds r9, r9, r12 \n\t"         \
    "adcs r10, r10, r14 \n\t"       \
    "adc r11, r11, #0 \n\t"         \
    "umull r12, r14, r5, r6 \n\t"   \
    "adds r9, r9, r12 \n\t"         \
    "adcs r10, r10, r14 \n\t"       \
    "adc r11, r11, #0 \n\t"         \
    "stmia r0!, {r9} \n\t"          \
                                    \
    "ldmia r1!, {r3} \n\t"          \
    "mov r12, #0 \n\t"              \
    "umull r14, r9, r4, r8 \n\t"    \
    "adds r10, r10, r14 \n\t"       \
    "adcs r11, r11, r9 \n\t"        \
    "adc r12, r12, #0 \n\t"         \
    "umull r14, r9, r5, r7 \n\t"    \
    "adds r10, r10, r14 \n\t"       \
    "adcs r11, r11, r9 \n\t"        \
    "adc r12, r12, #0 \n\t"         \
    "umull r14, r9, r3, r6 \n\t"    \
    "adds r10, r10, r14 \n\t"       \
    "adcs r11, r11, r9 \n\t"        \
    "adc r12, r12, #0 \n\t"         \
    "ldr r14, [r0] \n\t"            \
    "adds r10, r10, r14 \n\t"       \
    "adcs r11, r11, #0 \n\t"        \
    "adc r12, r12, #0 \n\t"         \
    "stmia r0!, {r10} \n\t"         \
                                    \
    "ldmia r1!, {r4} \n\t"          \
    "mov r14, #0 \n\t"              \
    "umull r9, r10, r5, r8 \n\t"    \
    "adds r11, r11, r9 \n\t"        \
    "adcs r12, r12, r10 \n\t"       \
    "adc r14, r14, #0 \n\t"         \
    "umull r9, r10, r3, r7 \n\t"    \
    "adds r11, r11, r9 \n\t"        \
    "adcs r12, r12, r10 \n\t"       \
    "adc r14, r14, #0 \n\t"         \
    "umull r9, r10, r4, r6 \n\t"    \
    "adds r11, r11, r9 \n\t"        \
    "adcs r12, r12, r10 \n\t"       \
    "adc r14, r14, #0 \n\t"         \
    "ldr r9, [r0] \n\t"             \
    "adds r11, r11, r9 \n\t"        \
    "adcs r12, r12, #0 \n\t"        \
    "adc r14, r14, #0 \n\t"         \
    "stmia r0!, {r11} \n\t"         \
                                    \
    "ldmia r1!, {r5} \n\t"          \
    "mov r9, #0 \n\t"               \
    "umull r10, r11, r3, r8 \n\t"   \
    "adds r12, r12, r10 \n\t"       \
    "adcs r14, r14, r11 \n\t"       \
    "adc r9, r9, #0 \n\t"           \
    "umull r10, r11, r4, r7 \n\t"   \
    "adds r12, r12, r10 \n\t"       \
    "adcs r14, r14, r11 \n\t"       \
    "adc r9, r9, #0 \n\t"           \
    "umull r10, r11, r5, r6 \n\t"   \
    "adds r12, r12, r10 \n\t"       \
    "adcs r14, r14, r11 \n\t"       \
    "adc r9, r9, #0 \n\t"           \
    "ldr r10, [r0] \n\t"            \
    "adds r12, r12, r10 \n\t"       \
    "adcs r14, r14, #0 \n\t"        \
    "adc r9, r9, #0 \n\t"           \
    "stmia r0!, {r12} \n\t"         \
                                    \
    "ldmia r1!, {r3} \n\t"          \
    "mov r10, #0 \n\t"              \
    "umull r11, r12, r4, r8 \n\t"   \
    "adds r14, r14, r11 \n\t"       \
    "adcs r9, r9, r12 \n\t"         \
    "adc r10, r10, #0 \n\t"         \
    "umull r11, r12, r5, r7 \n\t"   \
    "adds r14, r14, r11 \n\t"       \
    "adcs r9, r9, r12 \n\t"         \
    "adc r10, r10, #0 \n\t"         \
    "umull r11, r12, r3, r6 \n\t"   \
    "adds r14, r14, r11 \n\t"       \
    "adcs r9, r9, r12 \n\t"         \
    "adc r10, r10, #0 \n\t"         \
    "ldr r11, [r0] \n\t"            \
    "adds r14, r14, r11 \n\t"       \
    "adcs r9, r9, #0 \n\t"          \
    "adc r10, r10, #0 \n\t"         \
    "stmia r0!, {r14} \n\t"         \
                                    \
    "ldmia r1!, {r4} \n\t"          \
    "mov r11, #0 \n\t"              \
    "umull r12, r14, r5, r8 \n\t"   \
    "adds r9, r9, r12 \n\t"         \
    "adcs r10, r10, r14 \n\t"       \
    "adc r11, r11, #0 \n\t"         \
    "umull r12, r14, r3, r7 \n\t"   \
    "adds r9, r9, r12 \n\t"         \
    "adcs r10, r10, r14 \n\t"       \
    "adc r11, r11, #0 \n\t"         \
    "umull r12, r14, r4, r6 \n\t"   \
    "adds r9, r9, r12 \n\t"         \
    "adcs r10, r10, r14 \n\t"       \
    "adc r11, r11, #0 \n\t"         \
    "ldr r12, [r0] \n\t"            \
    "adds r9, r9, r12 \n\t"         \
    "adcs r10, r10, #0 \n\t"        \
    "adc r11, r11, #0 \n\t"         \
    "stmia r0!, {r9} \n\t"          \
                                    \
    "ldmia r2!, {r6} \n\t"          \
    "mov r12, #0 \n\t"              \
    "umull r14, r9, r5, r6 \n\t"    \
    "adds r10, r10, r14 \n\t"       \
    "adcs r11, r11, r9 \n\t"        \
    "adc r12, r12, #0 \n\t"         \
    "umull r14, r9, r3, r8 \n\t"    \
    "adds r10, r10, r14 \n\t"       \
    "adcs r11, r11, r9 \n\t"        \
    "adc r12, r12, #0 \n\t"         \
    "umull r14, r9, r4, r7 \n\t"    \
    "adds r10, r10, r14 \n\t"       \
    "adcs r11, r11, r9 \n\t"        \
    "adc r12, r12, #0 \n\t"         \
    "ldr r14, [r0] \n\t"            \
    "adds r10, r10, r14 \n\t"       \
    "adcs r11, r11, #0 \n\t"        \
    "adc r12, r12, #0 \n\t"         \
    "stmia r0!, {r10} \n\t"         \
                                    \
    "ldmia r2!, {r7} \n\t"          \
    "mov r14, #0 \n\t"              \
    "umull r9, r10, r5, r7 \n\t"    \
    "adds r11, r11, r9 \n\t"        \
    "adcs r12, r12, r10 \n\t"       \
    "adc r14, r14, #0 \n\t"         \
    "umull r9, r10, r3, r6 \n\t"    \
    "adds r11, r11, r9 \n\t"        \
    "adcs r12, r12, r10 \n\t"       \
    "adc r14, r14, #0 \n\t"         \
    "umull r9, r10, r4, r8 \n\t"    \
    "adds r11, r11, r9 \n\t"        \
    "adcs r12, r12, r10 \n\t"       \
    "adc r14, r14, #0 \n\t"         \
    "ldr r9, [r0] \n\t"             \
    "adds r11, r11, r9 \n\t"        \
    "adcs r12, r12, #0 \n\t"        \
    "adc r14, r14, #0 \n\t"         \
    "stmia r0!, {r11} \n\t"         \
                                    \
    "ldmia r2!, {r8} \n\t"          \
    "mov r9, #0 \n\t"               \
    "umull r10, r11, r5, r8 \n\t"   \
    "adds r12, r12, r10 \n\t"       \
    "adcs r14, r14, r11 \n\t"       \
    "adc r9, r9, #0 \n\t"           \
    "umull r10, r11, r3, r7 \n\t"   \
    "adds r12, r12, r10 \n\t"       \
    "adcs r14, r14, r11 \n\t"       \
    "adc r9, r9, #0 \n\t"           \
    "umull r10, r11, r4, r6 \n\t"   \
    "adds r12, r12, r10 \n\t"       \
    "adcs r14, r14, r11 \n\t"       \
    "adc r9, r9, #0 \n\t"           \
    "ldr r10, [r0] \n\t"            \
    "adds r12, r12, r10 \n\t"       \
    "adcs r14, r14, #0 \n\t"        \
    "adc r9, r9, #0 \n\t"           \
    "stmia r0!, {r12} \n\t"         \
                                    \
    "ldmia r2!, {r6} \n\t"          \
    "mov r10, #0 \n\t"              \
    "umull r11, r12, r5, r6 \n\t"   \
    "adds r14, r14, r11 \n\t"       \
    "adcs r9, r9, r12 \n\t"         \
    "adc r10, r10, #0 \n\t"         \
    "umull r11, r12, r3, r8 \n\t"   \
    "adds r14, r14, r11 \n\t"       \
    "adcs r9, r9, r12 \n\t"         \
    "adc r10, r10, #0 \n\t"         \
    "umull r11, r12, r4, r7 \n\t"   \
    "adds r14, r14, r11 \n\t"       \
    "adcs r9, r9, r12 \n\t"         \
    "adc r10, r10, #0 \n\t"         \
    "ldr r11, [r0] \n\t"            \
    "adds r14, r14, r11 \n\t"       \
    "adcs r9, r9, #0 \n\t"          \
    "adc r10, r10, #0 \n\t"         \
    "stmia r0!, {r14} \n\t"         \
                                    \
    "ldmia r2!, {r7} \n\t"          \
    "mov r11, #0 \n\t"              \
    "umull r12, r14, r5, r7 \n\t"   \
    "adds r9, r9, r12 \n\t"         \
    "adcs r10, r10, r14 \n\t"       \
    "adc r11, r11, #0 \n\t"         \
    "umull r12, r14, r3, r6 \n\t"   \
    "adds r9, r9, r12 \n\t"         \
    "adcs r10, r10, r14 \n\t"       \
    "adc r11, r11, #0 \n\t"         \
    "umull r12, r14, r4, r8 \n\t"   \
    "adds r9, r9, r12 \n\t"         \
    "adcs r10, r10, r14 \n\t"       \
    "adc r11, r11, #0 \n\t"         \
    "ldr r12, [r0] \n\t"            \
    "adds r9, r9, r12 \n\t"         \
    "adcs r10, r10, #0 \n\t"        \
    "adc r11, r11, #0 \n\t"         \
    "stmia r0!, {r9} \n\t"          \
                                    \
    "mov r12, #0 \n\t"              \
    "umull r14, r9, r3, r7 \n\t"    \
    "adds r10, r10, r14 \n\t"       \
    "adcs r11, r11, r9 \n\t"        \
    "adc r12, r12, #0 \n\t"         \
    "umull r14, r9, r4, r6 \n\t"    \
    "adds r10, r10, r14 \n\t"       \
    "adcs r11, r11, r9 \n\t"        \
    "adc r12, r12, #0 \n\t"         \
    "stmia r0!, {r10} \n\t"         \
                                    \
    "umull r9, r10, r4, r7 \n\t"    \
    "adds r11, r11, r9 \n\t"        \
    "adc r12, r12, r10 \n\t"        \
    "stmia r0!, {r11, r12} \n\t"

#define FAST_SQUARE_ASM_5               \
    "ldmia r1!, {r2,r3,r4,r5,r6} \n\t"  \
                                        \
    "umull r11, r12, r2, r2 \n\t"       \
    "stmia r0!, {r11} \n\t"             \
                                        \
    "mov r9, #0 \n\t"                   \
    "umull r10, r11, r2, r3 \n\t"       \
    "adds r12, r12, r10 \n\t"           \
    "adcs r8, r11, #0 \n\t"             \
    "adc r9, r9, #0 \n\t"               \
    "adds r12, r12, r10 \n\t"           \
    "adcs r8, r8, r11 \n\t"             \
    "adc r9, r9, #0 \n\t"               \
    "stmia r0!, {r12} \n\t"             \
                                        \
    "mov r10, #0 \n\t"                  \
    "umull r11, r12, r2, r4 \n\t"       \
    "adds r11, r11, r11 \n\t"           \
    "adcs r12, r12, r12 \n\t"           \
    "adc r10, r10, #0 \n\t"             \
    "adds r8, r8, r11 \n\t"             \
    "adcs r9, r9, r12 \n\t"             \
    "adc r10, r10, #0 \n\t"             \
    "umull r11, r12, r3, r3 \n\t"       \
    "adds r8, r8, r11 \n\t"             \
    "adcs r9, r9, r12 \n\t"             \
    "adc r10, r10, #0 \n\t"             \
    "stmia r0!, {r8} \n\t"              \
                                        \
    "mov r12, #0 \n\t"                  \
    "umull r8, r11, r2, r5 \n\t"        \
    "umull r1, r14, r3, r4 \n\t"        \
    "adds r8, r8, r1 \n\t"              \
    "adcs r11, r11, r14 \n\t"           \
    "adc r12, r12, #0 \n\t"             \
    "adds r8, r8, r8 \n\t"              \
    "adcs r11, r11, r11 \n\t"           \
    "adc r12, r12, r12 \n\t"            \
    "adds r8, r8, r9 \n\t"              \
    "adcs r11, r11, r10 \n\t"           \
    "adc r12, r12, #0 \n\t"             \
    "stmia r0!, {r8} \n\t"              \
                                        \
    "mov r10, #0 \n\t"                  \
    "umull r8, r9, r2, r6 \n\t"         \
    "umull r1, r14, r3, r5 \n\t"        \
    "adds r8, r8, r1 \n\t"              \
    "adcs r9, r9, r14 \n\t"             \
    "adc r10, r10, #0 \n\t"             \
    "adds r8, r8, r8 \n\t"              \
    "adcs r9, r9, r9 \n\t"              \
    "adc r10, r10, r10 \n\t"            \
    "umull r1, r14, r4, r4 \n\t"        \
    "adds r8, r8, r1 \n\t"              \
    "adcs r9, r9, r14 \n\t"             \
    "adc r10, r10, #0 \n\t"             \
    "adds r8, r8, r11 \n\t"             \
    "adcs r9, r9, r12 \n\t"             \
    "adc r10, r10, #0 \n\t"             \
    "stmia r0!, {r8} \n\t"              \
                                        \
    "mov r12, #0 \n\t"                  \
    "umull r8, r11, r3, r6 \n\t"        \
    "umull r1, r14, r4, r5 \n\t"        \
    "adds r8, r8, r1 \n\t"              \
    "adcs r11, r11, r14 \n\t"           \
    "adc r12, r12, #0 \n\t"             \
    "adds r8, r8, r8 \n\t"              \
    "adcs r11, r11, r11 \n\t"           \
    "adc r12, r12, r12 \n\t"            \
    "adds r8, r8, r9 \n\t"              \
    "adcs r11, r11, r10 \n\t"           \
    "adc r12, r12, #0 \n\t"             \
    "stmia r0!, {r8} \n\t"              \
                                        \
    "mov r8, #0 \n\t"                   \
    "umull r1, r10, r4, r6 \n\t"        \
    "adds r1, r1, r1 \n\t"              \
    "adcs r10, r10, r10 \n\t"           \
    "adc r8, r8, #0 \n\t"               \
    "adds r11, r11, r1 \n\t"            \
    "adcs r12, r12, r10 \n\t"           \
    "adc r8, r8, #0 \n\t"               \
    "umull r1, r10, r5, r5 \n\t"        \
    "adds r11, r11, r1 \n\t"            \
    "adcs r12, r12, r10 \n\t"           \
    "adc r8, r8, #0 \n\t"               \
    "stmia r0!, {r11} \n\t"             \
                                        \
    "mov r11, #0 \n\t"                  \
    "umull r1, r10, r5, r6 \n\t"        \
    "adds r1, r1, r1 \n\t"              \
    "adcs r10, r10, r10 \n\t"           \
    "adc r11, r11, #0 \n\t"             \
    "adds r12, r12, r1 \n\t"            \
    "adcs r8, r8, r10 \n\t"             \
    "adc r11, r11, #0 \n\t"             \
    "stmia r0!, {r12} \n\t"             \
                                        \
    "umull r1, r10, r6, r6 \n\t"        \
    "adds r8, r8, r1 \n\t"              \
    "adcs r11, r11, r10 \n\t"           \
    "stmia r0!, {r8, r11} \n\t"

#define FAST_SQUARE_ASM_6                  \
    "ldmia r1!, {r2,r3,r4,r5,r6,r7} \n\t"  \
                                           \
    "umull r11, r12, r2, r2 \n\t"          \
    "stmia r0!, {r11} \n\t"                \
                                           \
    "mov r9, #0 \n\t"                      \
    "umull r10, r11, r2, r3 \n\t"          \
    "adds r12, r12, r10 \n\t"              \
    "adcs r8, r11, #0 \n\t"                \
    "adc r9, r9, #0 \n\t"                  \
    "adds r12, r12, r10 \n\t"              \
    "adcs r8, r8, r11 \n\t"                \
    "adc r9, r9, #0 \n\t"                  \
    "stmia r0!, {r12} \n\t"                \
                                           \
    "mov r10, #0 \n\t"                     \
    "umull r11, r12, r2, r4 \n\t"          \
    "adds r11, r11, r11 \n\t"              \
    "adcs r12, r12, r12 \n\t"              \
    "adc r10, r10, #0 \n\t"                \
    "adds r8, r8, r11 \n\t"                \
    "adcs r9, r9, r12 \n\t"                \
    "adc r10, r10, #0 \n\t"                \
    "umull r11, r12, r3, r3 \n\t"          \
    "adds r8, r8, r11 \n\t"                \
    "adcs r9, r9, r12 \n\t"                \
    "adc r10, r10, #0 \n\t"                \
    "stmia r0!, {r8} \n\t"                 \
                                           \
    "mov r12, #0 \n\t"                     \
    "umull r8, r11, r2, r5 \n\t"           \
    "umull r1, r14, r3, r4 \n\t"           \
    "adds r8, r8, r1 \n\t"                 \
    "adcs r11, r11, r14 \n\t"              \
    "adc r12, r12, #0 \n\t"                \
    "adds r8, r8, r8 \n\t"                 \
    "adcs r11, r11, r11 \n\t"              \
    "adc r12, r12, r12 \n\t"               \
    "adds r8, r8, r9 \n\t"                 \
    "adcs r11, r11, r10 \n\t"              \
    "adc r12, r12, #0 \n\t"                \
    "stmia r0!, {r8} \n\t"                 \
                                           \
    "mov r10, #0 \n\t"                     \
    "umull r8, r9, r2, r6 \n\t"            \
    "umull r1, r14, r3, r5 \n\t"           \
    "adds r8, r8, r1 \n\t"                 \
    "adcs r9, r9, r14 \n\t"                \
    "adc r10, r10, #0 \n\t"                \
    "adds r8, r8, r8 \n\t"                 \
    "adcs r9, r9, r9 \n\t"                 \
    "adc r10, r10, r10 \n\t"               \
    "umull r1, r14, r4, r4 \n\t"           \
    "adds r8, r8, r1 \n\t"                 \
    "adcs r9, r9, r14 \n\t"                \
    "adc r10, r10, #0 \n\t"                \
    "adds r8, r8, r11 \n\t"                \
    "adcs r9, r9, r12 \n\t"                \
    "adc r10, r10, #0 \n\t"                \
    "stmia r0!, {r8} \n\t"                 \
                                           \
    "mov r12, #0 \n\t"                     \
    "umull r8, r11, r2, r7 \n\t"           \
    "umull r1, r14, r3, r6 \n\t"           \
    "adds r8, r8, r1 \n\t"                 \
    "adcs r11, r11, r14 \n\t"              \
    "adc r12, r12, #0 \n\t"                \
    "umull r1, r14, r4, r5 \n\t"           \
    "adds r8, r8, r1 \n\t"                 \
    "adcs r11, r11, r14 \n\t"              \
    "adc r12, r12, #0 \n\t"                \
    "adds r8, r8, r8 \n\t"                 \
    "adcs r11, r11, r11 \n\t"              \
    "adc r12, r12, r12 \n\t"               \
    "adds r8, r8, r9 \n\t"                 \
    "adcs r11, r11, r10 \n\t"              \
    "adc r12, r12, #0 \n\t"                \
    "stmia r0!, {r8} \n\t"                 \
                                           \
    "mov r10, #0 \n\t"                     \
    "umull r8, r9, r3, r7 \n\t"            \
    "umull r1, r14, r4, r6 \n\t"           \
    "adds r8, r8, r1 \n\t"                 \
    "adcs r9, r9, r14 \n\t"                \
    "adc r10, r10, #0 \n\t"                \
    "adds r8, r8, r8 \n\t"                 \
    "adcs r9, r9, r9 \n\t"                 \
    "adc r10, r10, r10 \n\t"               \
    "umull r1, r14, r5, r5 \n\t"           \
    "adds r8, r8, r1 \n\t"                 \
    "adcs r9, r9, r14 \n\t"                \
    "adc r10, r10, #0 \n\t"                \
    "adds r8, r8, r11 \n\t"                \
    "adcs r9, r9, r12 \n\t"                \
    "adc r10, r10, #0 \n\t"                \
    "stmia r0!, {r8} \n\t"                 \
                                           \
    "mov r12, #0 \n\t"                     \
    "umull r8, r11, r4, r7 \n\t"           \
    "umull r1, r14, r5, r6 \n\t"           \
    "adds r8, r8, r1 \n\t"                 \
    "adcs r11, r11, r14 \n\t"              \
    "adc r12, r12, #0 \n\t"                \
    "adds r8, r8, r8 \n\t"                 \
    "adcs r11, r11, r11 \n\t"              \
    "adc r12, r12, r12 \n\t"               \
    "adds r8, r8, r9 \n\t"                 \
    "adcs r11, r11, r10 \n\t"              \
    "adc r12, r12, #0 \n\t"                \
    "stmia r0!, {r8} \n\t"                 \
                                           \
    "mov r8, #0 \n\t"                      \
    "umull r1, r10, r5, r7 \n\t"           \
    "adds r1, r1, r1 \n\t"                 \
    "adcs r10, r10, r10 \n\t"              \
    "adc r8, r8, #0 \n\t"                  \
    "adds r11, r11, r1 \n\t"               \
    "adcs r12, r12, r10 \n\t"              \
    "adc r8, r8, #0 \n\t"                  \
    "umull r1, r10, r6, r6 \n\t"           \
    "adds r11, r11, r1 \n\t"               \
    "adcs r12, r12, r10 \n\t"              \
    "adc r8, r8, #0 \n\t"                  \
    "stmia r0!, {r11} \n\t"                \
                                           \
    "mov r11, #0 \n\t"                     \
    "umull r1, r10, r6, r7 \n\t"           \
    "adds r1, r1, r1 \n\t"                 \
    "adcs r10, r10, r10 \n\t"              \
    "adc r11, r11, #0 \n\t"                \
    "adds r12, r12, r1 \n\t"               \
    "adcs r8, r8, r10 \n\t"                \
    "adc r11, r11, #0 \n\t"                \
    "stmia r0!, {r12} \n\t"                \
                                           \
    "umull r1, r10, r7, r7 \n\t"           \
    "adds r8, r8, r1 \n\t"                 \
    "adcs r11, r11, r10 \n\t"              \
    "stmia r0!, {r8, r11} \n\t"

#define FAST_SQUARE_ASM_7                      \
    "ldmia r1!, {r2} \n\t"                     \
    "add r1, 20 \n\t"                          \
    "ldmia r1!, {r5} \n\t"                     \
    "add r0, 24 \n\t"                          \
    "umull r8, r9, r2, r5 \n\t"                \
    "stmia r0!, {r8, r9} \n\t"                 \
    "sub r0, 32 \n\t"                          \
    "sub r1, 28 \n\t"                          \
                                               \
    "ldmia r1!, {r2, r3, r4, r5, r6, r7} \n\t" \
                                               \
    "umull r11, r12, r2, r2 \n\t"              \
    "stmia r0!, {r11} \n\t"                    \
                                               \
    "mov r9, #0 \n\t"                          \
    "umull r10, r11, r2, r3 \n\t"              \
    "adds r12, r12, r10 \n\t"                  \
    "adcs r8, r11, #0 \n\t"                    \
    "adc r9, r9, #0 \n\t"                      \
    "adds r12, r12, r10 \n\t"                  \
    "adcs r8, r8, r11 \n\t"                    \
    "adc r9, r9, #0 \n\t"                      \
    "stmia r0!, {r12} \n\t"                    \
                                               \
    "mov r10, #0 \n\t"                         \
    "umull r11, r12, r2, r4 \n\t"              \
    "adds r11, r11, r11 \n\t"                  \
    "adcs r12, r12, r12 \n\t"                  \
    "adc r10, r10, #0 \n\t"                    \
    "adds r8, r8, r11 \n\t"                    \
    "adcs r9, r9, r12 \n\t"                    \
    "adc r10, r10, #0 \n\t"                    \
    "umull r11, r12, r3, r3 \n\t"              \
    "adds r8, r8, r11 \n\t"                    \
    "adcs r9, r9, r12 \n\t"                    \
    "adc r10, r10, #0 \n\t"                    \
    "stmia r0!, {r8} \n\t"                     \
                                               \
    "mov r12, #0 \n\t"                         \
    "umull r8, r11, r2, r5 \n\t"               \
    "mov r14, r11 \n\t"                        \
    "umlal r8, r11, r3, r4 \n\t"               \
    "cmp r14, r11 \n\t"                        \
    "it hi \n\t"                               \
    "adchi r12, r12, #0 \n\t"                  \
    "adds r8, r8, r8 \n\t"                     \
    "adcs r11, r11, r11 \n\t"                  \
    "adc r12, r12, r12 \n\t"                   \
    "adds r8, r8, r9 \n\t"                     \
    "adcs r11, r11, r10 \n\t"                  \
    "adc r12, r12, #0 \n\t"                    \
    "stmia r0!, {r8} \n\t"                     \
                                               \
    "mov r10, #0 \n\t"                         \
    "umull r8, r9, r2, r6 \n\t"                \
    "mov r14, r9 \n\t"                         \
    "umlal r8, r9, r3, r5 \n\t"                \
    "cmp r14, r9 \n\t"                         \
    "it hi \n\t"                               \
    "adchi r10, r10, #0 \n\t"                  \
    "adds r8, r8, r8 \n\t"                     \
    "adcs r9, r9, r9 \n\t"                     \
    "adc r10, r10, r10 \n\t"                   \
    "mov r14, r9 \n\t"                         \
    "umlal r8, r9, r4, r4 \n\t"                \
    "cmp r14, r9 \n\t"                         \
    "it hi \n\t"                               \
    "adchi r10, r10, #0 \n\t"                  \
    "adds r8, r8, r11 \n\t"                    \
    "adcs r9, r9, r12 \n\t"                    \
    "adc r10, r10, #0 \n\t"                    \
    "stmia r0!, {r8} \n\t"                     \
                                               \
    "mov r12, #0 \n\t"                         \
    "umull r8, r11, r2, r7 \n\t"               \
    "mov r14, r11 \n\t"                        \
    "umlal r8, r11, r3, r6 \n\t"               \
    "cmp r14, r11 \n\t"                        \
    "it hi \n\t"                               \
    "adchi r12, r12, #0 \n\t"                  \
    "mov r14, r11 \n\t"                        \
    "umlal r8, r11, r4, r5 \n\t"               \
    "cmp r14, r11 \n\t"                        \
    "it hi \n\t"                               \
    "adchi r12, r12, #0 \n\t"                  \
    "adds r8, r8, r8 \n\t"                     \
    "adcs r11, r11, r11 \n\t"                  \
    "adc r12, r12, r12 \n\t"                   \
    "adds r8, r8, r9 \n\t"                     \
    "adcs r11, r11, r10 \n\t"                  \
    "adc r12, r12, #0 \n\t"                    \
    "stmia r0!, {r8} \n\t"                     \
                                               \
    "ldmia r1!, {r2} \n\t"                     \
    "mov r10, #0 \n\t"                         \
    "umull r8, r9, r3, r7 \n\t"                \
    "mov r14, r9 \n\t"                         \
    "umlal r8, r9, r4, r6 \n\t"                \
    "cmp r14, r9 \n\t"                         \
    "it hi \n\t"                               \
    "adchi r10, r10, #0 \n\t"                  \
    "ldr r14, [r0] \n\t"                       \
    "adds r8, r8, r14 \n\t"                    \
    "adcs r9, r9, #0 \n\t"                     \
    "adc r10, r10, #0 \n\t"                    \
    "adds r8, r8, r8 \n\t"                     \
    "adcs r9, r9, r9 \n\t"                     \
    "adc r10, r10, r10 \n\t"                   \
    "mov r14, r9 \n\t"                         \
    "umlal r8, r9, r5, r5 \n\t"                \
    "cmp r14, r9 \n\t"                         \
    "it hi \n\t"                               \
    "adchi r10, r10, #0 \n\t"                  \
    "adds r8, r8, r11 \n\t"                    \
    "adcs r9, r9, r12 \n\t"                    \
    "adc r10, r10, #0 \n\t"                    \
    "stmia r0!, {r8} \n\t"                     \
                                               \
    "mov r12, #0 \n\t"                         \
    "umull r8, r11, r3, r2 \n\t"               \
    "mov r14, r11 \n\t"                        \
    "umlal r8, r11, r4, r7 \n\t"               \
    "cmp r14, r11 \n\t"                        \
    "it hi \n\t"                               \
    "adchi r12, r12, #0 \n\t"                  \
    "mov r14, r11 \n\t"                        \
    "umlal r8, r11, r5, r6 \n\t"               \
    "cmp r14, r11 \n\t"                        \
    "it hi \n\t"                               \
    "adchi r12, r12, #0 \n\t"                  \
    "ldr r14, [r0] \n\t"                       \
    "adds r8, r8, r14 \n\t"                    \
    "adcs r11, r11, #0 \n\t"                   \
    "adc r12, r12, #0 \n\t"                    \
    "adds r8, r8, r8 \n\t"                     \
    "adcs r11, r11, r11 \n\t"                  \
    "adc r12, r12, r12 \n\t"                   \
    "adds r8, r8, r9 \n\t"                     \
    "adcs r11, r11, r10 \n\t"                  \
    "adc r12, r12, #0 \n\t"                    \
    "stmia r0!, {r8} \n\t"                     \
                                               \
    "mov r10, #0 \n\t"                         \
    "umull r8, r9, r4, r2 \n\t"                \
    "mov r14, r9 \n\t"                         \
    "umlal r8, r9, r5, r7 \n\t"                \
    "cmp r14, r9 \n\t"                         \
    "it hi \n\t"                               \
    "adchi r10, r10, #0 \n\t"                  \
    "adds r8, r8, r8 \n\t"                     \
    "adcs r9, r9, r9 \n\t"                     \
    "adc r10, r10, r10 \n\t"                   \
    "mov r14, r9 \n\t"                         \
    "umlal r8, r9, r6, r6 \n\t"                \
    "cmp r14, r9 \n\t"                         \
    "it hi \n\t"                               \
    "adchi r10, r10, #0 \n\t"                  \
    "adds r8, r8, r11 \n\t"                    \
    "adcs r9, r9, r12 \n\t"                    \
    "adc r10, r10, #0 \n\t"                    \
    "stmia r0!, {r8} \n\t"                     \
                                               \
    "mov r12, #0 \n\t"                         \
    "umull r8, r11, r5, r2 \n\t"               \
    "mov r14, r11 \n\t"                        \
    "umlal r8, r11, r6, r7 \n\t"               \
    "cmp r14, r11 \n\t"                        \
    "it hi \n\t"                               \
    "adchi r12, r12, #0 \n\t"                  \
    "adds r8, r8, r8 \n\t"                     \
    "adcs r11, r11, r11 \n\t"                  \
    "adc r12, r12, r12 \n\t"                   \
    "adds r8, r8, r9 \n\t"                     \
    "adcs r11, r11, r10 \n\t"                  \
    "adc r12, r12, #0 \n\t"                    \
    "stmia r0!, {r8} \n\t"                     \
                                               \
    "mov r8, #0 \n\t"                          \
    "umull r1, r10, r6, r2 \n\t"               \
    "adds r1, r1, r1 \n\t"                     \
    "adcs r10, r10, r10 \n\t"                  \
    "adc r8, r8, #0 \n\t"                      \
    "adds r11, r11, r1 \n\t"                   \
    "adcs r12, r12, r10 \n\t"                  \
    "adc r8, r8, #0 \n\t"                      \
    "umull r1, r10, r7, r7 \n\t"               \
    "adds r11, r11, r1 \n\t"                   \
    "adcs r12, r12, r10 \n\t"                  \
    "adc r8, r8, #0 \n\t"                      \
    "stmia r0!, {r11} \n\t"                    \
                                               \
    "mov r11, #0 \n\t"                         \
    "umull r1, r10, r7, r2 \n\t"               \
    "adds r1, r1, r1 \n\t"                     \
    "adcs r10, r10, r10 \n\t"                  \
    "adc r11, r11, #0 \n\t"                    \
    "adds r12, r12, r1 \n\t"                   \
    "adcs r8, r8, r10 \n\t"                    \
    "adc r11, r11, #0 \n\t"                    \
    "stmia r0!, {r12} \n\t"                    \
                                               \
    "umull r1, r10, r2, r2 \n\t"               \
    "adds r8, r8, r1 \n\t"                     \
    "adcs r11, r11, r10 \n\t"                  \
    "stmia r0!, {r8, r11} \n\t"

#define FAST_SQUARE_ASM_8                   \
    "ldmia r1!, {r2, r3} \n\t"              \
    "add r1, 16 \n\t"                       \
    "ldmia r1!, {r5, r6} \n\t"              \
    "add r0, 24 \n\t"                       \
                                            \
    "umull r8, r9, r2, r5 \n\t"             \
    "stmia r0!, {r8} \n\t"                  \
                                            \
    "umull r12, r10, r2, r6 \n\t"           \
    "adds r9, r9, r12 \n\t"                 \
    "adc r10, r10, #0 \n\t"                 \
    "stmia r0!, {r9} \n\t"                  \
                                            \
    "umull r8, r9, r3, r6 \n\t"             \
    "adds r10, r10, r8 \n\t"                \
    "adc r11, r9, #0 \n\t"                  \
    "stmia r0!, {r10, r11} \n\t"            \
                                            \
    "sub r0, 40 \n\t"                       \
    "sub r1, 32 \n\t"                       \
    "ldmia r1!, {r2,r3,r4,r5,r6,r7} \n\t"   \
                                            \
    "umull r11, r12, r2, r2 \n\t"           \
    "stmia r0!, {r11} \n\t"                 \
                                            \
    "mov r9, #0 \n\t"                       \
    "umull r10, r11, r2, r3 \n\t"           \
    "adds r12, r12, r10 \n\t"               \
    "adcs r8, r11, #0 \n\t"                 \
    "adc r9, r9, #0 \n\t"                   \
    "adds r12, r12, r10 \n\t"               \
    "adcs r8, r8, r11 \n\t"                 \
    "adc r9, r9, #0 \n\t"                   \
    "stmia r0!, {r12} \n\t"                 \
                                            \
    "mov r10, #0 \n\t"                      \
    "umull r11, r12, r2, r4 \n\t"           \
    "adds r11, r11, r11 \n\t"               \
    "adcs r12, r12, r12 \n\t"               \
    "adc r10, r10, #0 \n\t"                 \
    "adds r8, r8, r11 \n\t"                 \
    "adcs r9, r9, r12 \n\t"                 \
    "adc r10, r10, #0 \n\t"                 \
    "umull r11, r12, r3, r3 \n\t"           \
    "adds r8, r8, r11 \n\t"                 \
    "adcs r9, r9, r12 \n\t"                 \
    "adc r10, r10, #0 \n\t"                 \
    "stmia r0!, {r8} \n\t"                  \
                                            \
    "mov r12, #0 \n\t"                      \
    "umull r8, r11, r2, r5 \n\t"            \
    "mov r14, r11 \n\t"                     \
    "umlal r8, r11, r3, r4 \n\t"            \
    "cmp r14, r11 \n\t"                     \
    "it hi \n\t"                            \
    "adchi r12, r12, #0 \n\t"               \
    "adds r8, r8, r8 \n\t"                  \
    "adcs r11, r11, r11 \n\t"               \
    "adc r12, r12, r12 \n\t"                \
    "adds r8, r8, r9 \n\t"                  \
    "adcs r11, r11, r10 \n\t"               \
    "adc r12, r12, #0 \n\t"                 \
    "stmia r0!, {r8} \n\t"                  \
                                            \
    "mov r10, #0 \n\t"                      \
    "umull r8, r9, r2, r6 \n\t"             \
    "mov r14, r9 \n\t"                      \
    "umlal r8, r9, r3, r5 \n\t"             \
    "cmp r14, r9 \n\t"                      \
    "it hi \n\t"                            \
    "adchi r10, r10, #0 \n\t"               \
    "adds r8, r8, r8 \n\t"                  \
    "adcs r9, r9, r9 \n\t"                  \
    "adc r10, r10, r10 \n\t"                \
    "mov r14, r9 \n\t"                      \
    "umlal r8, r9, r4, r4 \n\t"             \
    "cmp r14, r9 \n\t"                      \
    "it hi \n\t"                            \
    "adchi r10, r10, #0 \n\t"               \
    "adds r8, r8, r11 \n\t"                 \
    "adcs r9, r9, r12 \n\t"                 \
    "adc r10, r10, #0 \n\t"                 \
    "stmia r0!, {r8} \n\t"                  \
                                            \
    "mov r12, #0 \n\t"                      \
    "umull r8, r11, r2, r7 \n\t"            \
    "mov r14, r11 \n\t"                     \
    "umlal r8, r11, r3, r6 \n\t"            \
    "cmp r14, r11 \n\t"                     \
    "it hi \n\t"                            \
    "adchi r12, r12, #0 \n\t"               \
    "mov r14, r11 \n\t"                     \
    "umlal r8, r11, r4, r5 \n\t"            \
    "cmp r14, r11 \n\t"                     \
    "it hi \n\t"                            \
    "adchi r12, r12, #0 \n\t"               \
    "adds r8, r8, r8 \n\t"                  \
    "adcs r11, r11, r11 \n\t"               \
    "adc r12, r12, r12 \n\t"                \
    "adds r8, r8, r9 \n\t"                  \
    "adcs r11, r11, r10 \n\t"               \
    "adc r12, r12, #0 \n\t"                 \
    "stmia r0!, {r8} \n\t"                  \
                                            \
    "ldmia r1!, {r2} \n\t"                  \
    "mov r10, #0 \n\t"                      \
    "umull r8, r9, r3, r7 \n\t"             \
    "mov r14, r9 \n\t"                      \
    "umlal r8, r9, r4, r6 \n\t"             \
    "cmp r14, r9 \n\t"                      \
    "it hi \n\t"                            \
    "adchi r10, r10, #0 \n\t"               \
    "ldr r14, [r0] \n\t"                    \
    "adds r8, r8, r14 \n\t"                 \
    "adcs r9, r9, #0 \n\t"                  \
    "adc r10, r10, #0 \n\t"                 \
    "adds r8, r8, r8 \n\t"                  \
    "adcs r9, r9, r9 \n\t"                  \
    "adc r10, r10, r10 \n\t"                \
    "mov r14, r9 \n\t"                      \
    "umlal r8, r9, r5, r5 \n\t"             \
    "cmp r14, r9 \n\t"                      \
    "it hi \n\t"                            \
    "adchi r10, r10, #0 \n\t"               \
    "adds r8, r8, r11 \n\t"                 \
    "adcs r9, r9, r12 \n\t"                 \
    "adc r10, r10, #0 \n\t"                 \
    "stmia r0!, {r8} \n\t"                  \
                                            \
    "mov r12, #0 \n\t"                      \
    "umull r8, r11, r3, r2 \n\t"            \
    "mov r14, r11 \n\t"                     \
    "umlal r8, r11, r4, r7 \n\t"            \
    "cmp r14, r11 \n\t"                     \
    "it hi \n\t"                            \
    "adchi r12, r12, #0 \n\t"               \
    "mov r14, r11 \n\t"                     \
    "umlal r8, r11, r5, r6 \n\t"            \
    "cmp r14, r11 \n\t"                     \
    "it hi \n\t"                            \
    "adchi r12, r12, #0 \n\t"               \
    "ldr r14, [r0] \n\t"                    \
    "adds r8, r8, r14 \n\t"                 \
    "adcs r11, r11, #0 \n\t"                \
    "adc r12, r12, #0 \n\t"                 \
    "adds r8, r8, r8 \n\t"                  \
    "adcs r11, r11, r11 \n\t"               \
    "adc r12, r12, r12 \n\t"                \
    "adds r8, r8, r9 \n\t"                  \
    "adcs r11, r11, r10 \n\t"               \
    "adc r12, r12, #0 \n\t"                 \
    "stmia r0!, {r8} \n\t"                  \
                                            \
    "ldmia r1!, {r3} \n\t"                  \
    "mov r10, #0 \n\t"                      \
    "umull r8, r9, r4, r2 \n\t"             \
    "mov r14, r9 \n\t"                      \
    "umlal r8, r9, r5, r7 \n\t"             \
    "cmp r14, r9 \n\t"                      \
    "it hi \n\t"                            \
    "adchi r10, r10, #0 \n\t"               \
    "ldr r14, [r0] \n\t"                    \
    "adds r8, r8, r14 \n\t"                 \
    "adcs r9, r9, #0 \n\t"                  \
    "adc r10, r10, #0 \n\t"                 \
    "adds r8, r8, r8 \n\t"                  \
    "adcs r9, r9, r9 \n\t"                  \
    "adc r10, r10, r10 \n\t"                \
    "mov r14, r9 \n\t"                      \
    "umlal r8, r9, r6, r6 \n\t"             \
    "cmp r14, r9 \n\t"                      \
    "it hi \n\t"                            \
    "adchi r10, r10, #0 \n\t"               \
    "adds r8, r8, r11 \n\t"                 \
    "adcs r9, r9, r12 \n\t"                 \
    "adc r10, r10, #0 \n\t"                 \
    "stmia r0!, {r8} \n\t"                  \
                                            \
    "mov r12, #0 \n\t"                      \
    "umull r8, r11, r4, r3 \n\t"            \
    "mov r14, r11 \n\t"                     \
    "umlal r8, r11, r5, r2 \n\t"            \
    "cmp r14, r11 \n\t"                     \
    "it hi \n\t"                            \
    "adchi r12, r12, #0 \n\t"               \
    "mov r14, r11 \n\t"                     \
    "umlal r8, r11, r6, r7 \n\t"            \
    "cmp r14, r11 \n\t"                     \
    "it hi \n\t"                            \
    "adchi r12, r12, #0 \n\t"               \
    "ldr r14, [r0] \n\t"                    \
    "adds r8, r8, r14 \n\t"                 \
    "adcs r11, r11, #0 \n\t"                \
    "adc r12, r12, #0 \n\t"                 \
    "adds r8, r8, r8 \n\t"                  \
    "adcs r11, r11, r11 \n\t"               \
    "adc r12, r12, r12 \n\t"                \
    "adds r8, r8, r9 \n\t"                  \
    "adcs r11, r11, r10 \n\t"               \
    "adc r12, r12, #0 \n\t"                 \
    "stmia r0!, {r8} \n\t"                  \
                                            \
    "mov r10, #0 \n\t"                      \
    "umull r8, r9, r5, r3 \n\t"             \
    "mov r14, r9 \n\t"                      \
    "umlal r8, r9, r6, r2 \n\t"             \
    "cmp r14, r9 \n\t"                      \
    "it hi \n\t"                            \
    "adchi r10, r10, #0 \n\t"               \
    "adds r8, r8, r8 \n\t"                  \
    "adcs r9, r9, r9 \n\t"                  \
    "adc r10, r10, r10 \n\t"                \
    "mov r14, r9 \n\t"                      \
    "umlal r8, r9, r7, r7 \n\t"             \
    "cmp r14, r9 \n\t"                      \
    "it hi \n\t"                            \
    "adchi r10, r10, #0 \n\t"               \
    "adds r8, r8, r11 \n\t"                 \
    "adcs r9, r9, r12 \n\t"                 \
    "adc r10, r10, #0 \n\t"                 \
    "stmia r0!, {r8} \n\t"                  \
                                            \
    "mov r12, #0 \n\t"                      \
    "umull r8, r11, r6, r3 \n\t"            \
    "mov r14, r11 \n\t"                     \
    "umlal r8, r11, r7, r2 \n\t"            \
    "cmp r14, r11 \n\t"                     \
    "it hi \n\t"                            \
    "adchi r12, r12, #0 \n\t"               \
    "adds r8, r8, r8 \n\t"                  \
    "adcs r11, r11, r11 \n\t"               \
    "adc r12, r12, r12 \n\t"                \
    "adds r8, r8, r9 \n\t"                  \
    "adcs r11, r11, r10 \n\t"               \
    "adc r12, r12, #0 \n\t"                 \
    "stmia r0!, {r8} \n\t"                  \
                                            \
    "mov r8, #0 \n\t"                       \
    "umull r1, r10, r7, r3 \n\t"            \
    "adds r1, r1, r1 \n\t"                  \
    "adcs r10, r10, r10 \n\t"               \
    "adc r8, r8, #0 \n\t"                   \
    "adds r11, r11, r1 \n\t"                \
    "adcs r12, r12, r10 \n\t"               \
    "adc r8, r8, #0 \n\t"                   \
    "umull r1, r10, r2, r2 \n\t"            \
    "adds r11, r11, r1 \n\t"                \
    "adcs r12, r12, r10 \n\t"               \
    "adc r8, r8, #0 \n\t"                   \
    "stmia r0!, {r11} \n\t"                 \
                                            \
    "mov r11, #0 \n\t"                      \
    "umull r1, r10, r2, r3 \n\t"            \
    "adds r1, r1, r1 \n\t"                  \
    "adcs r10, r10, r10 \n\t"               \
    "adc r11, r11, #0 \n\t"                 \
    "adds r12, r12, r1 \n\t"                \
    "adcs r8, r8, r10 \n\t"                 \
    "adc r11, r11, #0 \n\t"                 \
    "stmia r0!, {r12} \n\t"                 \
                                            \
    "umull r1, r10, r3, r3 \n\t"            \
    "adds r8, r8, r1 \n\t"                  \
    "adcs r11, r11, r10 \n\t"               \
    "stmia r0!, {r8, r11} \n\t"

#endif /* _UECC_ASM_ARM_MULT_SQUARE_H_ */
