/*
 * threadpool.h
 *
 * Description of this file:
 *    thread pooling functions definition of the davs2 library
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

#ifndef DAVS2_THREADPOOL_H
#define DAVS2_THREADPOOL_H
#ifdef __cplusplus
extern "C" {
#endif

typedef struct davs2_threadpool_t davs2_threadpool_t;
typedef void *(*davs2_threadpool_func_t)(void *arg1, int arg2);

#define davs2_threadpool_init FPFX(threadpool_init)
int   davs2_threadpool_init  (davs2_threadpool_t **p_pool, int threads,
                              davs2_threadpool_func_t init_func, void *init_arg1, int init_arg2);
#define davs2_threadpool_run FPFX(threadpool_run)
void  davs2_threadpool_run   (davs2_threadpool_t *pool, davs2_threadpool_func_t func, void *arg1, int arg2, int wait_sign);
#define davs2_threadpool_is_free FPFX(threadpool_is_free)
int   davs2_threadpool_is_free(davs2_threadpool_t *pool);
#define davs2_threadpool_wait FPFX(threadpool_wait)
void *davs2_threadpool_wait  (davs2_threadpool_t *pool, void *arg1, int arg2);
#define davs2_threadpool_delete FPFX(threadpool_delete)
void  davs2_threadpool_delete(davs2_threadpool_t *pool);

#ifdef __cplusplus
}
#endif
#endif  // __STARAVS_THREADPOOL_H
