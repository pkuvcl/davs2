/*****************************************************************************
 * win32thread.h: windows threading
 *****************************************************************************
 * Copyright (C) 2010-2017 x264 project
 *
 * Authors: Steven Walters <kemuri9@gmail.com>
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
 * For more information, contact us at licensing@x264.com.
 *****************************************************************************/

/*
 * changes of this file:
 *    modified for davs2 library
 *
 * --------------------------------------------------------------------------
 *
 *    davs2 - video decoder of AVS2/IEEE1857.4 video coding standard
 *    Copyright (C) 2018~ VCL, NELVT, Peking University
 *
 *    Authors: Falei LUO <falei.luo@gmail.com>
 *             etc.
 */

#ifndef DAVS2_WIN32THREAD_H
#define DAVS2_WIN32THREAD_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
/* the following macro is used within xavs2 encoder */
#undef ERROR

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void *handle;
    void *(*func)(void *arg);
    void *arg;
    void *ret;
} davs2_thread_t;
#define davs2_thread_attr_t int

/* the conditional variable api for windows 6.0+ uses critical sections and not mutexes */
typedef CRITICAL_SECTION davs2_thread_mutex_t;
#define DAVS2_THREAD_MUTEX_INITIALIZER {0}
#define davs2_thread_mutexattr_t int
#define pthread_exit(a)
/* This is the CONDITIONAL_VARIABLE typedef for using Window's native conditional variables on kernels 6.0+.
 * MinGW does not currently have this typedef. */
typedef struct {
    void *ptr;
} davs2_thread_cond_t;
#define davs2_thread_condattr_t int

#define davs2_thread_create FPFX(thread_create)
int davs2_thread_create(davs2_thread_t *thread, const davs2_thread_attr_t *attr,
                        void *(*start_routine)(void *), void *arg);
#define davs2_thread_join FPFX(thread_join)
int davs2_thread_join(davs2_thread_t thread, void **value_ptr);

#define davs2_thread_mutex_init FPFX(thread_mutex_init)
int davs2_thread_mutex_init(davs2_thread_mutex_t *mutex, const davs2_thread_mutexattr_t *attr);
#define davs2_thread_mutex_destroy FPFX(thread_mutex_destroy)
int davs2_thread_mutex_destroy(davs2_thread_mutex_t *mutex);
#define davs2_thread_mutex_lock FPFX(thread_mutex_lock)
int davs2_thread_mutex_lock(davs2_thread_mutex_t *mutex);
#define davs2_thread_mutex_unlock FPFX(thread_mutex_unlock)
int davs2_thread_mutex_unlock(davs2_thread_mutex_t *mutex);

#define davs2_thread_cond_init FPFX(thread_cond_init)
int davs2_thread_cond_init(davs2_thread_cond_t *cond, const davs2_thread_condattr_t *attr);
#define davs2_thread_cond_destroy FPFX(thread_cond_destroy)
int davs2_thread_cond_destroy(davs2_thread_cond_t *cond);
#define davs2_thread_cond_broadcast FPFX(thread_cond_broadcast)
int davs2_thread_cond_broadcast(davs2_thread_cond_t *cond);
#define davs2_thread_cond_wait FPFX(thread_cond_wait)
int davs2_thread_cond_wait(davs2_thread_cond_t *cond, davs2_thread_mutex_t *mutex);
#define davs2_thread_cond_signal FPFX(thread_cond_signal)
int davs2_thread_cond_signal(davs2_thread_cond_t *cond);

#define davs2_thread_attr_init(a) 0
#define davs2_thread_attr_destroy(a) 0

#define davs2_win32_threading_init FPFX(win32_threading_init)
int  davs2_win32_threading_init(void);
#define davs2_win32_threading_destroy FPFX(win32_threading_destroy)
void davs2_win32_threading_destroy(void);

#define davs2_thread_num_processors_np FPFX(thread_num_processors_np)
int davs2_thread_num_processors_np(void);

#ifdef __cplusplus
}
#endif
#endif  // DAVS2_WIN32THREAD_H
