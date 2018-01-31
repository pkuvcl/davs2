/*
 * threadpool.cc
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

#include "common.h"
#include "threadpool.h"


/**
 * ===========================================================================
 * type defines
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 * job
 */
typedef struct threadpool_job_t {
    davs2_threadpool_func_t func;
    void                   *arg1;
    int                     arg2;
    void                   *ret;
    int                     wait;
} threadpool_job_t;

/* ---------------------------------------------------------------------------
 * synchronized job list
 */
typedef struct davs2_sync_job_list_t {
    int                     i_max_size;
    int                     i_size;
    davs2_thread_mutex_t    mutex;
    davs2_thread_cond_t     cv_fill;  /* event signaling that the list became fuller */
    davs2_thread_cond_t     cv_empty; /* event signaling that the list became emptier */
    threadpool_job_t       *list[DAVS2_WORK_MAX + 2];
} davs2_sync_job_list_t;

/* ---------------------------------------------------------------------------
 * thread pool
 */
struct davs2_threadpool_t {
    int                 i_exit;               /* exit flag */
    int                 num_total_threads;    /* thread number in pool */
    int                 num_run_threads;      /* thread number running */
    davs2_threadpool_func_t init_func;
    void               *init_arg;
    int                 init_arg2;

    /* requires a synchronized list structure and associated methods,
       so use what is already implemented for jobs */
    davs2_sync_job_list_t uninit;   /* list of jobs that are awaiting use */
    davs2_sync_job_list_t run;      /* list of jobs that are queued for processing by the pool */
    davs2_sync_job_list_t done;     /* list of jobs that have finished processing */

    /* handler of threads in the pool */
    davs2_thread_t        thread_handle[AVS2_THREAD_MAX];
};


/**
 * ===========================================================================
 * list operators
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 */
static threadpool_job_t *davs2_job_shift(threadpool_job_t **list)
{
    threadpool_job_t *job = list[0];
    int i;

    for (i = 0; list[i]; i++) {
        list[i] = list[i + 1];
    }
    assert(job);

    return job;
}

/**
 * ===========================================================================
 * list operators
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 */
static int davs2_sync_job_list_init(davs2_sync_job_list_t *slist, int i_max_size)
{
    if (i_max_size < 0) {
        return -1;
    }

    slist->i_max_size = i_max_size;
    slist->i_size     = 0;
    memset(slist->list, 0, sizeof(slist->list));

    if (davs2_thread_mutex_init(&slist->mutex, NULL) ||
        davs2_thread_cond_init(&slist->cv_fill, NULL) ||
        davs2_thread_cond_init(&slist->cv_empty, NULL)) {
        return -1;
    }

    return 0;
}

/* ---------------------------------------------------------------------------
 */
static void davs2_threadpool_list_delete(davs2_sync_job_list_t *slist)
{
    davs2_thread_mutex_destroy(&slist->mutex);
    davs2_thread_cond_destroy(&slist->cv_fill);
    davs2_thread_cond_destroy(&slist->cv_empty);
    slist->i_size = 0;
}

/* ---------------------------------------------------------------------------
 */
static void davs2_sync_job_list_push(davs2_sync_job_list_t *slist, threadpool_job_t *job)
{
    davs2_thread_mutex_lock(&slist->mutex);      /* lock */
    while (slist->i_size == slist->i_max_size) {
        davs2_thread_cond_wait(&slist->cv_empty, &slist->mutex);
    }
    slist->list[slist->i_size++] = job;
    davs2_thread_mutex_unlock(&slist->mutex);    /* unlock */

    davs2_thread_cond_broadcast(&slist->cv_fill);
}

/* ---------------------------------------------------------------------------
 */
static threadpool_job_t *davs2_sync_job_list_pop(davs2_sync_job_list_t *slist)
{
    threadpool_job_t *job;

    davs2_thread_mutex_lock(&slist->mutex);      /* lock */
    while (!slist->i_size) {
        davs2_thread_cond_wait(&slist->cv_fill, &slist->mutex);
    }
    job = slist->list[--slist->i_size];
    slist->list[slist->i_size] = NULL;
    davs2_thread_cond_broadcast(&slist->cv_empty);
    davs2_thread_mutex_unlock(&slist->mutex);    /* unlock */

    return job;
}


/**
 * ===========================================================================
 * thread pool operators
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 */
static
void *davs2_threadpool_thread(void *arg)
{
    davs2_threadpool_t *pool = (davs2_threadpool_t *)arg;

    /* init */
    if (pool->init_func) {
        pool->init_func(pool->init_arg, pool->init_arg2);
    }

    /* loop until exit flag is set */
    while (pool->i_exit != AVS2_EXIT_THREAD) {
        threadpool_job_t *job = NULL;

        /* fetch a job */
        davs2_thread_mutex_lock(&pool->run.mutex);   /* lock */
        while (pool->i_exit != AVS2_EXIT_THREAD && !pool->run.i_size) {
            davs2_thread_cond_wait(&pool->run.cv_fill, &pool->run.mutex);
        }
        if (pool->run.i_size) {
            job = davs2_job_shift(pool->run.list);
            pool->run.i_size--;
        }
        davs2_thread_mutex_unlock(&pool->run.mutex); /* unlock */

        /* do the job */
        if (!job) {
            continue;
        }
        job->ret = job->func(job->arg1, job->arg2); /* execute the function */

        /* the job is done */
        if (job->wait) {
            davs2_sync_job_list_push(&pool->done, job);
        } else {
            davs2_sync_job_list_push(&pool->uninit, job);
        }
    }

    return NULL;
}

/* ---------------------------------------------------------------------------
 */
int davs2_threadpool_init(davs2_threadpool_t **p_pool, int threads, davs2_threadpool_func_t init_func, void *init_arg1, int init_arg2)
{
    davs2_threadpool_t *pool;
    uint32_t mem_size;
    uint8_t *mem_ptr;
    int i;

    if (threads <= 0) {
        return -1;
    }

    mem_size = sizeof(davs2_threadpool_t)
        + DAVS2_WORK_MAX * sizeof(threadpool_job_t)
        + CACHE_LINE_SIZE * (DAVS2_WORK_MAX + 2);

    CHECKED_MALLOCZERO(mem_ptr, uint8_t *, mem_size);
    *p_pool = pool = (davs2_threadpool_t *)mem_ptr;
    mem_ptr += sizeof(davs2_threadpool_t);
    ALIGN_POINTER(mem_ptr);

    pool->init_func = init_func;
    pool->init_arg  = init_arg1;
    pool->init_arg2 = init_arg2;
    pool->num_total_threads = DAVS2_MIN(threads, AVS2_THREAD_MAX);
    pool->num_run_threads   = 0;

    if (davs2_sync_job_list_init(&pool->uninit, DAVS2_WORK_MAX) ||
        davs2_sync_job_list_init(&pool->run,    DAVS2_WORK_MAX) ||
        davs2_sync_job_list_init(&pool->done,   DAVS2_WORK_MAX)) {
        goto fail;
    }

    for (i = 0; i < DAVS2_WORK_MAX; i++) {
        threadpool_job_t *job = (threadpool_job_t *)mem_ptr;
        mem_ptr += sizeof(threadpool_job_t);
        ALIGN_POINTER(mem_ptr);
        davs2_sync_job_list_push(&pool->uninit, job);
    }

    for (i = 0; i < pool->num_total_threads; i++) {
        if (davs2_thread_create(pool->thread_handle + i, NULL, davs2_threadpool_thread, pool)) {
            goto fail;
        }
    }

    return 0;

fail:
    return -1;
}

/* ---------------------------------------------------------------------------
 */
void davs2_threadpool_run(davs2_threadpool_t *pool, davs2_threadpool_func_t func, void *arg1, int arg2, int wait_sign)
{
    threadpool_job_t *job = davs2_sync_job_list_pop(&pool->uninit);

    job->func = func;
    job->arg1 = arg1;
    job->arg2 = arg2;
    job->wait = wait_sign;
    davs2_sync_job_list_push(&pool->run, job);
}

/* ---------------------------------------------------------------------------
 * 查询线程池是否在空转
 */
int davs2_threadpool_is_free(davs2_threadpool_t *pool)
{
    return pool->run.i_size <= 0;
}

/* ---------------------------------------------------------------------------
 */
void *davs2_threadpool_wait(davs2_threadpool_t *pool, void *arg1, int arg2)
{
    threadpool_job_t *job = NULL;
    void *ret;
    int i;

    davs2_thread_mutex_lock(&pool->done.mutex);      /* lock */
    while (!job) {
        for (i = 0; i < pool->done.i_size; i++) {
            threadpool_job_t *t = pool->done.list[i];
            if (t->arg1 == arg1 && t->arg2 == arg2) {
                job = davs2_job_shift(pool->done.list + i);
                pool->done.i_size--;
                break;          /* found the job according to arg */
            }
        }
        if (!job) {
            davs2_thread_cond_wait(&pool->done.cv_fill, &pool->done.mutex);
        }
    }
    davs2_thread_mutex_unlock(&pool->done.mutex);    /* unlock */

    ret = job->ret;
    davs2_sync_job_list_push(&pool->uninit, job);

    return ret;
}

/* ---------------------------------------------------------------------------
 */
void davs2_threadpool_delete(davs2_threadpool_t *pool)
{
    int i;

    davs2_thread_mutex_lock(&pool->run.mutex);   /* lock */
    pool->i_exit = AVS2_EXIT_THREAD;
    davs2_thread_cond_broadcast(&pool->run.cv_fill);
    davs2_thread_mutex_unlock(&pool->run.mutex); /* unlock */

    for (i = 0; i < pool->num_total_threads; i++) {
        davs2_thread_join(pool->thread_handle[i], NULL);
    }

    davs2_threadpool_list_delete(&pool->uninit);
    davs2_threadpool_list_delete(&pool->run);
    davs2_threadpool_list_delete(&pool->done);
    davs2_free(pool);
}
