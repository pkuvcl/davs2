/*
 * cpu.cc
 *
 * Description of this file:
 *    CPU-Processing functions definition of the davs2 library
 *
 * --------------------------------------------------------------------------
 *
 *    davs2 - video encoder of AVS2/IEEE1857.4 video coding standard
 *    Copyright (C) 2018~ VCL, NELVT, Peking University
 *
 * Authors: Falei LUO     <falei.luo@gmail.com>
 *
 * --------------------------------------------------------------------------
 * Copyright (C) 2013-2017 MulticoreWare, Inc
 *
 * Authors: Loren Merritt <lorenm@u.washington.edu>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Fiona Glaser  <fiona@x264.com>
 *          Steve Borho   <steve@borho.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *
 * This program is also available under a commercial proprietary license.
 * For more information, contact us at license @ x265.com.
 */

#include "common.h"
#include "cpu.h"

#if SYS_MACOSX || SYS_FREEBSD
#include <sys/types.h>
#include <sys/sysctl.h>
#endif
#if SYS_OPENBSD
#include <sys/param.h>
#include <sys/sysctl.h>
#include <machine/cpu.h>
#endif

#if ARCH_ARM
#include <signal.h>
#include <setjmp.h>
static sigjmp_buf jmpbuf;
static volatile sig_atomic_t canjump = 0;

static void sigill_handler(int sig)
{
    if (!canjump)
    {
        signal(sig, SIG_DFL);
        raise(sig);
    }

    canjump = 0;
    siglongjmp(jmpbuf, 1);
}

#endif // if ARCH_ARM

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 */
typedef struct {
    const char *name;
    int flags;
} davs2_cpu_name_t;

/* ---------------------------------------------------------------------------
 */
static const davs2_cpu_name_t davs2_cpu_names[] = {
#if ARCH_X86 || ARCH_X86_64
#define MMX2            DAVS2_CPU_MMX | DAVS2_CPU_MMX2 | DAVS2_CPU_CMOV
    { "MMX2",           MMX2 },
    { "MMXEXT",         MMX2 },
    { "SSE",            MMX2 | DAVS2_CPU_SSE },
#define SSE2            MMX2 | DAVS2_CPU_SSE | DAVS2_CPU_SSE2
    { "SSE2Slow",       SSE2 | DAVS2_CPU_SSE2_IS_SLOW },
    { "SSE2",           SSE2 },
    { "SSE2Fast",       SSE2 | DAVS2_CPU_SSE2_IS_FAST },
    { "SSE3",           SSE2 | DAVS2_CPU_SSE3 },
    { "SSSE3",          SSE2 | DAVS2_CPU_SSE3 | DAVS2_CPU_SSSE3 },
    { "SSE4.1",         SSE2 | DAVS2_CPU_SSE3 | DAVS2_CPU_SSSE3 | DAVS2_CPU_SSE4 },
    { "SSE4",           SSE2 | DAVS2_CPU_SSE3 | DAVS2_CPU_SSSE3 | DAVS2_CPU_SSE4 },
    { "SSE4.2",         SSE2 | DAVS2_CPU_SSE3 | DAVS2_CPU_SSSE3 | DAVS2_CPU_SSE4 | DAVS2_CPU_SSE42 },
#define AVX             SSE2 | DAVS2_CPU_SSE3 | DAVS2_CPU_SSSE3 | DAVS2_CPU_SSE4 | DAVS2_CPU_SSE42 | DAVS2_CPU_AVX
    { "AVX",            AVX },
    { "XOP",            AVX | DAVS2_CPU_XOP },
    { "FMA4",           AVX | DAVS2_CPU_FMA4 },
    { "AVX2",           AVX | DAVS2_CPU_AVX2 },
    { "FMA3",           AVX | DAVS2_CPU_FMA3 },
#undef AVX
#undef SSE2
#undef MMX2
    { "Cache32",        DAVS2_CPU_CACHELINE_32 },
    { "Cache64",        DAVS2_CPU_CACHELINE_64 },
    { "LZCNT",          DAVS2_CPU_LZCNT },
    { "BMI1",           DAVS2_CPU_BMI1 },
    { "BMI2",           DAVS2_CPU_BMI1 | DAVS2_CPU_BMI2 },
    { "SlowCTZ",        DAVS2_CPU_SLOW_CTZ },
    { "SlowAtom",       DAVS2_CPU_SLOW_ATOM },
    { "SlowPshufb",     DAVS2_CPU_SLOW_PSHUFB },
    { "SlowPalignr",    DAVS2_CPU_SLOW_PALIGNR },
    { "SlowShuffle",    DAVS2_CPU_SLOW_SHUFFLE },
    { "UnalignedStack", DAVS2_CPU_STACK_MOD4 },

#elif ARCH_ARM
    { "ARMv6",          DAVS2_CPU_ARMV6 },
    { "NEON",           DAVS2_CPU_NEON },
    { "FastNeonMRC",    DAVS2_CPU_FAST_NEON_MRC },
#endif // if DAVS2_ARCH_X86
    { "", 0 }
};

#ifdef __cplusplus
}
#endif

/* ---------------------------------------------------------------------------
 */
char *davs2_get_simd_capabilities(char *buf, uint32_t cpuid)
{
    char *p = buf;

    for (int i = 0; davs2_cpu_names[i].flags; i++) {
        if (!strcmp(davs2_cpu_names[i].name, "SSE")
            && (cpuid & DAVS2_CPU_SSE2))
            continue;
        if (!strcmp(davs2_cpu_names[i].name, "SSE2")
            && (cpuid & (DAVS2_CPU_SSE2_IS_FAST | DAVS2_CPU_SSE2_IS_SLOW)))
            continue;
        if (!strcmp(davs2_cpu_names[i].name, "SSE3")
            && (cpuid & DAVS2_CPU_SSSE3 || !(cpuid & DAVS2_CPU_CACHELINE_64)))
            continue;
        if (!strcmp(davs2_cpu_names[i].name, "SSE4.1")
            && (cpuid & DAVS2_CPU_SSE42))
            continue;
        if (!strcmp(davs2_cpu_names[i].name, "BMI1")
            && (cpuid & DAVS2_CPU_BMI2))
            continue;
        if ((cpuid & davs2_cpu_names[i].flags) == (uint32_t)davs2_cpu_names[i].flags
            && (!i || davs2_cpu_names[i].flags != davs2_cpu_names[i - 1].flags))
            p += sprintf(p, " %s", davs2_cpu_names[i].name);
    }

    if (p == buf) {
        sprintf(p, " none! 0x%x", cpuid);
    }
    return buf;
}

#if !ARCH_X86_64
/*  */
int  davs2_cpu_cpuid_test(void);
#endif

#if HAVE_MMX
/* ---------------------------------------------------------------------------
 */
uint32_t davs2_cpu_detect(void)
{
    uint32_t cpuid = 0;

    uint32_t eax, ebx, ecx, edx;
    uint32_t vendor[4] = { 0 };
    uint32_t max_extended_cap, max_basic_cap;

#if !ARCH_X86_64
    if (!davs2_cpu_cpuid_test()) {
        return 0;
    }
#endif

    davs2_cpu_cpuid(0, &eax, vendor + 0, vendor + 2, vendor + 1);
    max_basic_cap = eax;
    if (max_basic_cap == 0) {
        return 0;
    }

    davs2_cpu_cpuid(1, &eax, &ebx, &ecx, &edx);
    if (edx & 0x00800000) {
        cpuid |= DAVS2_CPU_MMX;
    } else {
        return cpuid;
    }

    if (edx & 0x02000000) {
        cpuid |= DAVS2_CPU_MMX2 | DAVS2_CPU_SSE;
    }
    if (edx & 0x00008000) {
        cpuid |= DAVS2_CPU_CMOV;
    } else {
        return cpuid;
    }

    if (edx & 0x04000000) {
        cpuid |= DAVS2_CPU_SSE2;
    }
    if (ecx & 0x00000001) {
        cpuid |= DAVS2_CPU_SSE3;
    }
    if (ecx & 0x00000200) {
        cpuid |= DAVS2_CPU_SSSE3;
    }
    if (ecx & 0x00080000) {
        cpuid |= DAVS2_CPU_SSE4;
    }
    if (ecx & 0x00100000) {
        cpuid |= DAVS2_CPU_SSE42;
    }

    /* Check OXSAVE and AVX bits */
    if ((ecx & 0x18000000) == 0x18000000) {
        /* Check for OS support */
        davs2_cpu_xgetbv(0, &eax, &edx);
        if ((eax & 0x6) == 0x6) {
            cpuid |= DAVS2_CPU_AVX;
            if (ecx & 0x00001000) {
                cpuid |= DAVS2_CPU_FMA3;
            }
        }
    }

    if (max_basic_cap >= 7) {
        davs2_cpu_cpuid(7, &eax, &ebx, &ecx, &edx);
        /* AVX2 requires OS support, but BMI1/2 don't. */
        if ((cpuid & DAVS2_CPU_AVX) && (ebx & 0x00000020)) {
            cpuid |= DAVS2_CPU_AVX2;
        }
        if (ebx & 0x00000008) {
            cpuid |= DAVS2_CPU_BMI1;
            if (ebx & 0x00000100) {
                cpuid |= DAVS2_CPU_BMI2;
            }
        }
    }

    if (cpuid & DAVS2_CPU_SSSE3) {
        cpuid |= DAVS2_CPU_SSE2_IS_FAST;
    }

    davs2_cpu_cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
    max_extended_cap = eax;

    if (max_extended_cap >= 0x80000001) {
        davs2_cpu_cpuid(0x80000001, &eax, &ebx, &ecx, &edx);

        if (ecx & 0x00000020)
            cpuid |= DAVS2_CPU_LZCNT;               /* Supported by Intel chips starting with Haswell */
        if (ecx & 0x00000040) {                     /* SSE4a, AMD only */
            int family = ((eax >> 8) & 0xf) + ((eax >> 20) & 0xff);
            cpuid |= DAVS2_CPU_SSE2_IS_FAST;        /* Phenom and later CPUs have fast SSE units */
            if (family == 0x14) {
                cpuid &= ~DAVS2_CPU_SSE2_IS_FAST;   /* SSSE3 doesn't imply fast SSE anymore... */
                cpuid |= DAVS2_CPU_SSE2_IS_SLOW;    /* Bobcat has 64-bit SIMD units */
                cpuid |= DAVS2_CPU_SLOW_PALIGNR;    /* palignr is insanely slow on Bobcat */
            }
            if (family == 0x16) {
                cpuid |= DAVS2_CPU_SLOW_PSHUFB;     /* Jaguar's pshufb isn't that slow, but it's slow enough
                                                     * compared to alternate instruction sequences that this
                                                     * is equal or faster on almost all such functions. */
            }
        }

        if (cpuid & DAVS2_CPU_AVX)
        {
            if (ecx & 0x00000800) {   /* XOP */
                cpuid |= DAVS2_CPU_XOP;
            }
            if (ecx & 0x00010000) {   /* FMA4 */
                cpuid |= DAVS2_CPU_FMA4;
            }
        }

        if (!strcmp((char*)vendor, "AuthenticAMD")) {
            if (edx & 0x00400000) {
                cpuid |= DAVS2_CPU_MMX2;
            }
            if (!(cpuid & DAVS2_CPU_LZCNT)) {
                cpuid |= DAVS2_CPU_SLOW_CTZ;
            }
            if ((cpuid & DAVS2_CPU_SSE2) && !(cpuid & DAVS2_CPU_SSE2_IS_FAST)) {
                cpuid |= DAVS2_CPU_SSE2_IS_SLOW; /* AMD CPUs come in two types: terrible at SSE and great at it */
            }
        }
    }

    if (!strcmp((char*)vendor, "GenuineIntel")) {
        int family, model;
        davs2_cpu_cpuid(1, &eax, &ebx, &ecx, &edx);
        family = ((eax >> 8) & 0xf) + ((eax >> 20) & 0xff);
        model = ((eax >> 4) & 0xf) + ((eax >> 12) & 0xf0);
        if (family == 6) {
            /* 6/9 (pentium-m "banias"), 6/13 (pentium-m "dothan"), and 6/14 (core1 "yonah")
             * theoretically support sse2, but it's significantly slower than mmx for
             * almost all of x264's functions, so let's just pretend they don't. */
            if (model == 9 || model == 13 || model == 14) {
                cpuid &= ~(DAVS2_CPU_SSE2 | DAVS2_CPU_SSE3);
                //DAVS2_CHECK(!(cpuid & (DAVS2_CPU_SSSE3 | DAVS2_CPU_SSE4)), "unexpected CPU ID %d\n", cpuid);
            } else if (model == 28) {
                /* Detect Atom CPU */
                cpuid |= DAVS2_CPU_SLOW_ATOM;
                cpuid |= DAVS2_CPU_SLOW_CTZ;
                cpuid |= DAVS2_CPU_SLOW_PSHUFB;
            } else if ((cpuid & DAVS2_CPU_SSSE3) && !(cpuid & DAVS2_CPU_SSE4) && model < 23) {
                /* Conroe has a slow shuffle unit. Check the model number to make sure not
                 * to include crippled low-end Penryns and Nehalems that don't have SSE4. */
                cpuid |= DAVS2_CPU_SLOW_SHUFFLE;
            }
        }
    }

    if ((!strcmp((char*)vendor, "GenuineIntel") || !strcmp((char*)vendor, "CyrixInstead")) && !(cpuid & DAVS2_CPU_SSE42)) {
        /* cacheline size is specified in 3 places, any of which may be missing */
        int cache;
        davs2_cpu_cpuid(1, &eax, &ebx, &ecx, &edx);
        cache = (ebx & 0xff00) >> 5; // cflush size
        if (!cache && max_extended_cap >= 0x80000006) {
            davs2_cpu_cpuid(0x80000006, &eax, &ebx, &ecx, &edx);
            cache = ecx & 0xff; // cacheline size
        }
        if (!cache && max_basic_cap >= 2) {
            // Cache and TLB Information
            static const uint8_t cache32_ids[] = { 0x0a, 0x0c, 0x41, 0x42, 0x43, 0x44, 0x45, 0x82, 0x83, 0x84, 0x85, 0 };
            static const uint8_t cache64_ids[] = { 0x22, 0x23, 0x25, 0x29, 0x2c, 0x46, 0x47, 0x49, 0x60, 0x66, 0x67,
                0x68, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7c, 0x7f, 0x86, 0x87, 0 };
            uint32_t buf[4];
            int max, i = 0, j;
            do {
                davs2_cpu_cpuid(2, buf + 0, buf + 1, buf + 2, buf + 3);
                max = buf[0] & 0xff;
                buf[0] &= ~0xff;
                for (j = 0; j < 4; j++) {
                    if (!(buf[j] >> 31)) {
                        while (buf[j]) {
                            if (strchr((const char *)cache32_ids, buf[j] & 0xff)) {
                                cache = 32;
                            }
                            if (strchr((const char *)cache64_ids, buf[j] & 0xff)) {
                                cache = 64;
                            }
                            buf[j] >>= 8;
                        }
                    }
                }
            } while (++i < max);
        }

        if (cache == 32) {
            cpuid |= DAVS2_CPU_CACHELINE_32;
        } else if (cache == 64) {
            cpuid |= DAVS2_CPU_CACHELINE_64;
        } else {
            davs2_log(NULL, DAVS2_LOG_WARNING, "unable to determine cacheline size\n");
        }
    }

#ifdef BROKEN_STACK_ALIGNMENT
    cpuid |= DAVS2_CPU_STACK_MOD4;
#endif

    return cpuid;
}
#endif // if DAVS2_ARCH_X86

#if SYS_LINUX && !(defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7__))
/* ---------------------------------------------------------------------------
 */
int sched_getaffinity(pid_t pid, size_t cpusetsize, cpu_set_t *mask);
#endif

/* ---------------------------------------------------------------------------
 */
int davs2_cpu_num_processors(void)
{
#if !HAVE_THREAD
    return 1;
#elif defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7__)
    return 2;
#elif SYS_WINDOWS
    return davs2_thread_num_processors_np();
#elif SYS_LINUX
    unsigned int bit;
    int np = 0;
    cpu_set_t p_aff;

    memset(&p_aff, 0, sizeof(p_aff));
    sched_getaffinity(0, sizeof(p_aff), &p_aff);
    for (bit = 0; bit < sizeof(p_aff); bit++) {
        np += (((uint8_t *)& p_aff)[bit / 8] >> (bit % 8)) & 1;
    }
    return np;

#elif SYS_BEOS
    system_info info;

    get_system_info(&info);
    return info.cpu_count;

#elif SYS_MACOSX || SYS_FREEBSD || SYS_OPENBSD
    int numberOfCPUs;
    size_t length = sizeof (numberOfCPUs);
#if SYS_OPENBSD
    int mib[2] = { CTL_HW, HW_NCPU };
    if(sysctl(mib, 2, &numberOfCPUs, &length, NULL, 0))
#else
    if(sysctlbyname("hw.ncpu", &numberOfCPUs, &length, NULL, 0))
#endif
    {
        numberOfCPUs = 1;
    }
    return numberOfCPUs;

#else
    return 1;
#endif
}
