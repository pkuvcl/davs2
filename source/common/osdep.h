/*
 * osdep.h
 *
 * Description of this file:
 *    platform-specific code functions definition of the davs2 library
 *
 * --------------------------------------------------------------------------
 *
 *    davs2 - video encoder of AVS2/IEEE1857.4 video coding standard
 *    Copyright (C) 2018~ VCL, NELVT, Peking University
 *
 *    Authors: Falei LUO <falei.luo@gmail.com>
 *             etc.
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *
 *    This program is also available under a commercial proprietary license.
 *    For more information, contact us at sswang @ pku.edu.cn.
 */

#ifndef DAVS2_OSDEP_H
#define DAVS2_OSDEP_H
#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * disable warning C4996: functions or variables may be unsafe.
 */
#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#define _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable:4324)     /* disable warning C4324: 由于 __declspec(align())，结构被填充 */
#endif

/**
 * ===========================================================================
 * includes
 * ===========================================================================
 */

#define _LARGEFILE_SOURCE 1
#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <sys/stat.h>
#include <stdarg.h>

/* ---------------------------------------------------------------------------
 * disable warning C4996: functions or variables may be unsafe.
 */
#if defined(_MSC_VER)
#include <intrin.h>
#include <windows.h>
#endif

#if defined(__ICL) || defined(_MSC_VER)
#include "configw.h"
#else
#include "config.h"
#endif

#if HAVE_STDINT_H
#include <stdint.h>
#else
#include <inttypes.h>
#endif

#if defined(__INTEL_COMPILER)
#include <mathimf.h>
#else
#include <math.h>
#endif

#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
#include <float.h>
#endif

/* disable warning C4100: : unreferenced formal parameter */
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
#define UNUSED_PARAMETER(v) (v) /* same as UNREFERENCED_PARAMETER */
#else
#define UNUSED_PARAMETER(v) (void)(v)
#endif



/**
 * ===========================================================================
 * const defines
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 * Specifies the number of bits per pixel that DAVS2 uses
 */
#define AVS2_BIT_DEPTH          BIT_DEPTH

#define WORD_SIZE               sizeof(void*)


/**
 * ===========================================================================
 * const defines
 * ===========================================================================
 */

#if defined(__GNUC__) && (__GNUC__ > 3 || __GNUC__ == 3 && __GNUC_MINOR__ > 0)
#define UNINIT(x)               x = x
#define UNUSED                  __attribute__((unused))
#define ALWAYS_INLINE           __attribute__((always_inline)) inline
#define NOINLINE                __attribute__((noinline))
#define MAY_ALIAS               __attribute__((may_alias))
#define davs2_constant_p(x)   __builtin_constant_p(x)
#define davs2_nonconstant_p(x)    (!__builtin_constant_p(x))
#define INLINE                  __inline
#else
#define UNINIT(x)               x
#if defined(__ICL)
#define ALWAYS_INLINE           __forceinline
#define NOINLINE                __declspec(noinline)
#else
#define ALWAYS_INLINE           INLINE
#define NOINLINE
#endif
#define UNUSED
#define MAY_ALIAS
#define davs2_constant_p(x)       0
#define davs2_nonconstant_p(x)    0
#endif

/* string operations */
#if defined(__ICL) || defined(_MSC_VER)
#define INLINE                  __inline
#define strcasecmp              _stricmp
#define strncasecmp             _strnicmp
#define snprintf                _snprintf
#define S_ISREG(x)              (((x) & S_IFMT) == S_IFREG)
#if !HAVE_POSIXTHREAD
#define strtok_r                strtok_s
#endif
#else
#include <strings.h>
#endif

#if (defined(__GNUC__) || defined(__INTEL_COMPILER)) && (ARCH_X86 || ARCH_X86_64)
#ifndef HAVE_X86_INLINE_ASM
#define HAVE_X86_INLINE_ASM     1
#endif
#endif

// #if defined(_WIN32)
// /* POSIX says that rename() removes the destination, but win32 doesn't. */
// #define rename(src,dst)         (unlink(dst), rename(src,dst))
// #if !HAVE_POSIXTHREAD
// #ifndef strtok_r
// #define strtok_r(str,delim,save)    strtok(str, delim)
// #endif
// #endif
// #endif


/* ---------------------------------------------------------------------------
 * align
 */
/* align a pointer */
#  define CACHE_LINE_SIZE       32    /* for x86-64 and x86 */
#  define ALIGN_POINTER(p)      (p) = (uint8_t *)((intptr_t)((p) + (CACHE_LINE_SIZE - 1)) & (~(intptr_t)(CACHE_LINE_SIZE - 1)))
#  define CACHE_LINE_256B       32    /* for x86-64 and x86 */
#  define ALIGN_256_PTR(p)      (p) = (uint8_t *)((intptr_t)((p) + (CACHE_LINE_256B - 1)) & (~(intptr_t)(CACHE_LINE_256B - 1)))

#if defined(_MSC_VER)
#define DECLARE_ALIGNED(var, n) __declspec(align(n)) var
// #elif defined(__INTEL_COMPILER)
// #define DECLARE_ALIGNED(var, n) var __declspec(align(n))
#else
#define DECLARE_ALIGNED(var, n) var __attribute__((aligned(n)))
#endif
#define ALIGN32(var)        DECLARE_ALIGNED(var, 32)
#define ALIGN16(var)        DECLARE_ALIGNED(var, 16)
#define ALIGN8(var)         DECLARE_ALIGNED(var, 8)


// ARM compiliers don't reliably align stack variables
// - EABI requires only 8 byte stack alignment to be maintained
// - gcc can't align stack variables to more even if the stack were to be correctly aligned outside the function
// - armcc can't either, but is nice enough to actually tell you so
// - Apple gcc only maintains 4 byte alignment
// - llvm can align the stack, but only in svn and (unrelated) it exposes bugs in all released GNU binutils...

#define ALIGNED_ARRAY_EMU( mask, type, name, sub1, ... )\
    uint8_t name##_u [sizeof(type sub1 __VA_ARGS__) + mask]; \
    type (*name) __VA_ARGS__ = (void*)((intptr_t)(name##_u+mask) & ~mask)

#if ARCH_ARM && SYS_MACOSX
#define ALIGNED_ARRAY_8( ... ) ALIGNED_ARRAY_EMU( 7, __VA_ARGS__ )
#else
#define ALIGNED_ARRAY_8( type, name, sub1, ... )\
    ALIGN8( type name sub1 __VA_ARGS__ )
#endif

#if ARCH_ARM
#define ALIGNED_ARRAY_16( ... ) ALIGNED_ARRAY_EMU( 15, __VA_ARGS__ )
#else
#define ALIGNED_ARRAY_16( type, name, sub1, ... )\
    ALIGN16( type name sub1 __VA_ARGS__ )
#endif

#define EXPAND(x) x

#if defined(STACK_ALIGNMENT) && STACK_ALIGNMENT >= 32
#define ALIGNED_ARRAY_32( type, name, sub1, ... )\
    ALIGN32( type name sub1 __VA_ARGS__ )
#else
#define ALIGNED_ARRAY_32( ... ) EXPAND( ALIGNED_ARRAY_EMU( 31, __VA_ARGS__ ) )
#endif

#define ALIGNED_ARRAY_64( ... ) EXPAND( ALIGNED_ARRAY_EMU( 63, __VA_ARGS__ ) )

/* For AVX2 */
#if ARCH_X86 || ARCH_X86_64
#define NATIVE_ALIGN 32
#define ALIGNED_N ALIGN32
#define ALIGNED_ARRAY_N ALIGNED_ARRAY_32
#else
#define NATIVE_ALIGN 16
#define ALIGNED_N ALIGN16
#define ALIGNED_ARRAY_N ALIGNED_ARRAY_16
#endif


/* ---------------------------------------------------------------------------
 * threads
 */
#if HAVE_BEOSTHREAD
#include <kernel/OS.h>
#define davs2_thread_t       thread_id
static int ALWAYS_INLINE
avs2dec_pthread_create(davs2_thread_t *t, void *a, void *(*f)(void *), void *d)
{
    *t = spawn_thread(f, "", 10, d);
    if (*t < B_NO_ERROR) {
        return -1;
    }
    resume_thread(*t);
    return 0;
}

#define davs2_thread_join(t,s)\
    {\
        long tmp; \
        wait_for_thread(t,(s)?(long*)(*(s)):&tmp);\
    }

#elif HAVE_POSIXTHREAD
#if defined(_MSC_VER) || defined(__ICL)
#if _MSC_VER >= 1900
#define HAVE_STRUCT_TIMESPEC    1       /* for struct timespec */
#endif
#pragma comment(lib, "pthread_lib.lib")
#endif
#include <pthread.h>
#define davs2_thread_t                   pthread_t
#define davs2_thread_create              pthread_create
#define davs2_thread_join                pthread_join
#define davs2_thread_exit                pthread_exit
#define davs2_thread_mutex_t             pthread_mutex_t
#define davs2_thread_mutex_init          pthread_mutex_init
#define davs2_thread_mutex_destroy       pthread_mutex_destroy
#define davs2_thread_mutex_lock          pthread_mutex_lock
#define davs2_thread_mutex_unlock        pthread_mutex_unlock
#define davs2_thread_cond_t              pthread_cond_t
#define davs2_thread_cond_init           pthread_cond_init
#define davs2_thread_cond_destroy        pthread_cond_destroy
#define davs2_thread_cond_signal         pthread_cond_signal
#define davs2_thread_cond_broadcast      pthread_cond_broadcast
#define davs2_thread_cond_wait           pthread_cond_wait
#define davs2_thread_attr_t              pthread_attr_t
#define davs2_thread_attr_init           pthread_attr_init
#define davs2_thread_attr_destroy        pthread_attr_destroy
#if defined(__ARM_ARCH_7A__) || SYS_LINUX
#define davs2_thread_num_processors_np   get_nprocs
#else
#define davs2_thread_num_processors_np   pthread_num_processors_np
#endif
#define AVS2_PTHREAD_MUTEX_INITIALIZER   PTHREAD_MUTEX_INITIALIZER

#elif HAVE_WIN32THREAD
#include "win32thread.h"

#else
#define davs2_thread_t                   int
#define davs2_thread_create(t,u,f,d)     0
#define davs2_thread_join(t,s)
#endif //HAVE_*THREAD

#if !HAVE_POSIXTHREAD && !HAVE_WIN32THREAD
#define davs2_thread_mutex_t             int
#define davs2_thread_mutex_init(m,f)     0
#define davs2_thread_mutex_destroy(m)
#define davs2_thread_mutex_lock(m)
#define davs2_thread_mutex_unlock(m)
#define davs2_thread_cond_t              int
#define davs2_thread_cond_init(c,f)      0
#define davs2_thread_cond_destroy(c)
#define davs2_thread_cond_broadcast(c)
#define davs2_thread_cond_wait(c,m)
#define davs2_thread_attr_t              int
#define davs2_thread_attr_init(a)        0
#define davs2_thread_attr_destroy(a)
#define AVS2_PTHREAD_MUTEX_INITIALIZER   0
#endif

#if HAVE_WIN32THREAD || PTW32_STATIC_LIB
int davs2_threading_init(void);
#else
#define davs2_threading_init()   0
#endif

#if HAVE_POSIXTHREAD
#if SYS_WINDOWS
#define davs2_lower_thread_priority(p)\
    {\
        davs2_thread_t handle = pthread_self();\
        struct sched_param sp;\
        int policy = SCHED_OTHER;\
        pthread_getschedparam(handle, &policy, &sp);\
        sp.sched_priority -= p;\
        pthread_setschedparam(handle, policy, &sp);\
    }
#else
#include <unistd.h>
#define davs2_lower_thread_priority(p) { int nice_ret = nice(p); }
#define davs2_thread_spin_init(plock,pshare) pthread_spin_init(plock, pshare)
#endif /* SYS_WINDOWS */
#elif HAVE_WIN32THREAD
#define davs2_lower_thread_priority(p) SetThreadPriority(GetCurrentThread(), DAVS2_MAX(-2, -p))
#else
#define davs2_lower_thread_priority(p)
#endif

#if SYS_WINDOWS
#define davs2_sleep_ms(x)              Sleep(x)
#else
#define davs2_sleep_ms(x)              usleep(x * 1000)
#endif


/**
 * ===========================================================================
 * inline functions
 * ===========================================================================
 */
static int ALWAYS_INLINE davs2_is_regular_file(int filehandle)
{
    struct stat file_stat;
    if (fstat(filehandle, &file_stat)) {
        return -1;
    }
    return S_ISREG(file_stat.st_mode);
}

static int ALWAYS_INLINE davs2_is_regular_file_path(const char *filename)
{
    struct stat file_stat;
    if (stat(filename, &file_stat)) {
        return -1;
    }
    return S_ISREG(file_stat.st_mode);
}

#ifdef __cplusplus
}
#endif
#endif /* DAVS2_OSDEP_H */
