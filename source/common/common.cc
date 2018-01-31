/*
 * common.cc
 *
 * Description of this file:
 *    misc common functionsdefinition of the davs2 library
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
#include <stdarg.h>

#if __ARM_ARCH_7__
#include <android/log.h>
#define LOGI(format,...) __android_log_print(ANDROID_LOG_INFO, "davs2",format,##__VA_ARGS__)
#endif

/**
 * ===========================================================================
 * macros
 * ===========================================================================
 */
#define CONSOLE_WHITE  0
#define CONSOLE_YELLOW 1
#define CONSOLE_RED    2

/**
 * ===========================================================================
 * global variables
 * ===========================================================================
 */
#if HIGH_BIT_DEPTH
int max_pel_value = 255;
int g_bit_depth   = 8;
int g_dc_value    = 128;
#endif


/**
 * ===========================================================================
 * trace
 * ===========================================================================
 */

#if AVS2_TRACE

/**
 * ===========================================================================
 * trace file
 * ===========================================================================
 */

FILE *h_trace = NULL;           /* global file handle for trace file */
int g_bit_count = 0;            /* global bit    count for trace */

/* ---------------------------------------------------------------------------
 */
int avs2_trace_init(davs2_t *h, char *psz_trace_file)
{
    if (strlen(psz_trace_file) > 0) {
        /* create or truncate the trace file */
        h_trace = fopen(psz_trace_file, "wt");
        if (!h_trace) {
            davs2_log(h, AVS2_LOG_ERROR, "trace: can't write to trace file");
            return -1;
        } else if (!davs2_is_regular_file(fileno(h_trace))) {
            davs2_log(h, AVS2_LOG_ERROR, "trace: incompatible with non-regular file");
            return -1;
        }
    }

    return 0;
}

/* ---------------------------------------------------------------------------
 */
void avs2_trace_destroy(void)
{
    if (h_trace) {
        fclose(h_trace);
    }
}

/* ---------------------------------------------------------------------------
 */
int avs2_trace(const char *psz_fmt, ...)
{
    int len = 0;

    /* append to the trace file */
    if (h_trace) {
        va_list arg;
        va_start(arg, psz_fmt);

        len = vfprintf(h_trace, psz_fmt, arg);
        fflush(h_trace);
        va_end(arg);
    }

    return len;
}

void avs2_trace_string(char *trace_string, int value, int len)
{
    int i, chars;

    avs2_trace("@");
    chars = avs2_trace("%i", g_bit_count);

    while (chars++ < 6) {
        avs2_trace(" ");
    }

    chars += avs2_trace("%s", trace_string);

    while (chars++ < 55) {
        avs2_trace(" ");
    }

    // align bit-pattern
    if (len < 15) {
        for (i = 0; i < 15 - len; i++) {
            avs2_trace(" ");
        }
    }

    g_bit_count += len;
    while (len >= 32) {
        for (i = 0; i < 8; i++) {
            avs2_trace("0");
        }

        len -= 8;
    }

    // print bit-pattern
    for (i = 0; i < len; i++) {
        if (0x01 & (value >> (len - i - 1))) {
            avs2_trace("1");
        } else {
            avs2_trace("0");
        }
    }

    avs2_trace("  (%3d)\n", value);
}

/* ---------------------------------------------------------------------------
 * write out a trace string to the trace file
 */
void avs2_trace_string2(char *trace_string, int bit_pattern, int value, int len)
{
    int i, chars;

    avs2_trace("@");
    chars = avs2_trace("%i", g_bit_count);

    while (chars++ < 6) {
        avs2_trace(" ");
    }

    chars += avs2_trace("%s", trace_string);

    while (chars++ < 55) {
        avs2_trace(" ");
    }

    // align bit-pattern
    if (len < 15) {
        for (i = 0; i < 15 - len; i++) {
            avs2_trace(" ");
        }
    }

    // print bit-pattern
    g_bit_count += len;
    for (i = 1; i <= len; i++) {
        if ((bit_pattern >> (len - i)) & 0x1) {
            avs2_trace("1");
        } else {
            avs2_trace("0");
        }
    }

    avs2_trace("  (%3d)\n", value);
}
#endif

static ALWAYS_INLINE int
create_semaphore(semaphore_t *sem, void *attributes, int init_count, int max_count, const char *name)
{
#if (defined(__ICL) || defined(_MSC_VER)) && defined(_WIN32)
    *sem = CreateSemaphore((LPSECURITY_ATTRIBUTES)attributes, init_count, max_count, name);

    if (*sem == 0) {
        return GetLastError();
    } else {
        return 0;
    }

#else
    return sem_init(sem, 0, init_count);
#endif
}

static ALWAYS_INLINE int
release_semaphore(semaphore_t *sem)
{
#if (defined(__ICL) || defined(_MSC_VER)) && defined(_WIN32)
    if (ReleaseSemaphore(*sem, 1, NULL) == TRUE) {
        return 0;
    } else {
        return errno;
    }

#else
    return sem_post(sem);
#endif
}

static ALWAYS_INLINE int
close_semaphore(semaphore_t *sem)
{
#if (defined(__ICL) || defined(_MSC_VER)) && defined(_WIN32)
    if (CloseHandle(*sem) == TRUE) {
        return 0;
    }
    else {
        return errno;
    }

#else
    return sem_destroy(sem);
#endif
}

static int
wait_for_object(void *sem)
{
#if (defined(__ICL) || defined(_MSC_VER)) && defined(_WIN32)
    return WaitForSingleObject((void *)(*(semaphore_t *)sem), INFINITE);
#else
    int ret;
    while ((ret = sem_wait((sem_t *)sem)) == -1/* && errno == EINTR*/) {
        continue;
    }
    return ret;
#endif
}

int xl_init(xlist_t *const xlist)
{
    if (xlist == NULL) {
        return -1;
    }

    /* set list empty */
    xlist->p_list_head = NULL;
    xlist->p_list_tail = NULL;

    /* set node number */
    xlist->i_node_num = 0;

    /* create semaphore */
    create_semaphore(&(xlist->list_sem), NULL, 0, 1 << 30, NULL);

    /* init list lock */
    SPIN_INIT(xlist->list_lock);

    return 0;
}

void xl_destroy(xlist_t *const xlist)
{
    if (xlist == NULL) {
        return;
    }

    /* destroy the spin lock */
    SPIN_DESTROY(xlist->list_lock);

    /* close handles */
    close_semaphore(&(xlist->list_sem));

    /* clear */
    memset(xlist, 0, sizeof(xlist_t));
}

void xl_append(xlist_t *const xlist, void *node)
{
    node_t *new_node = (node_t *)node;

    if (xlist == NULL) {
        return;                       /* error */
    }

    new_node->next = NULL;            /* set NULL */

    /* append this node */
    SPIN_LOCK(xlist->list_lock);

    if (xlist->p_list_tail != NULL) {
        /* append this node at tail */
        xlist->p_list_tail->next = new_node;
    } else {
        xlist->p_list_head = new_node;
    }

    xlist->p_list_tail = new_node;    /* point to the tail node */
    xlist->i_node_num++;              /* increase the node number */
    SPIN_UNLOCK(xlist->list_lock);

    /* all is done, release a semaphore */
    release_semaphore(&(xlist->list_sem));
}

void *xl_remove_head(xlist_t *const xlist, const int wait)
{
    node_t *node = NULL;

    if (xlist == NULL) {
        return NULL;                  /* error */
    }

    if (wait) {
        wait_for_object(&(xlist->list_sem));
    }

    SPIN_LOCK(xlist->list_lock);

    /* remove the header node */
    if (xlist->i_node_num > 0) {
        node = xlist->p_list_head;    /* point to the header node */

        /* modify the list */
        xlist->p_list_head = node->next;

        if (xlist->p_list_head == NULL) {
            /* there are no any node in this list, reset the tail pointer */
            xlist->p_list_tail = NULL;
        }

        xlist->i_node_num--;          /* decrease the number */
    }

    SPIN_UNLOCK(xlist->list_lock);

    return node;
}

void *xl_remove_head_ex(xlist_t *const xlist)
{
    node_t *node = NULL;

    if (xlist == NULL) {
        return NULL;                  /* error */
    }

    /* remove the header node */
    if (xlist->i_node_num > 0) {
        node = xlist->p_list_head;    /* point to the header node */

        /* modify the list */
        xlist->p_list_head = node->next;

        if (xlist->p_list_head == NULL) {
            /* there are no any node in this list, reset the tail pointer */
            xlist->p_list_tail = NULL;
        }

        xlist->i_node_num--;          /* decrease the number */
    }

    return node;
}

/**
 * ===========================================================================
 * davs2_log
 * ===========================================================================
 */

#ifdef _MSC_VER
/* ---------------------------------------------------------------------------
 */
static ALWAYS_INLINE
void davs2_set_font_color(int color)
{
    static const WORD colors[] = {
        FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,       // 白色
        FOREGROUND_INTENSITY | FOREGROUND_GREEN,                   // 绿色
        FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE, // cyan
        FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN,  // 黄色
        FOREGROUND_INTENSITY | FOREGROUND_RED,                     // 红色
    };
    color = DAVS2_MIN(4, color);
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), colors[color]);
}
#endif

/* ---------------------------------------------------------------------------
 */
static void
davs2_log_default(int i_log_level, const char *psz_fmt)
{
#if !defined(_MSC_VER)
    static const char str_color_clear[] = "\033[0m";  // "\033[0m"
    static const char str_color[][16] = {
    /*  (default)     green         cyan         yellow             red     */
        "\033[0m", "\033[1;32m", "\033[1;36m", "\033[1;33m",   "\033[1;31m"
    };
    const char *cur_color = str_color[i_log_level];
#endif
    static const char *null_prefix = "";
    const char *psz_prefix = null_prefix;

    switch (i_log_level) {
    case AVS2_LOG_ERROR:
        psz_prefix = "[davs2 error]: ";
        break;
    case AVS2_LOG_WARNING:
        psz_prefix = "[davs2 warn]: ";
        break;
    case AVS2_LOG_INFO:
        psz_prefix = "[davs2 info]: ";
        break;
    case AVS2_LOG_DEBUG:
        psz_prefix = "[davs2 debug]: ";
        break;
    default:
        psz_prefix = "[davs2 *]: ";
#if !defined(_MSC_VER)
        cur_color  = str_color[0];
#endif
        break;
    }
#if defined(_MSC_VER)
    davs2_set_font_color(i_log_level); /* set color */
    fprintf(stdout, "%s%s\n", psz_prefix, psz_fmt);
    davs2_set_font_color(0);     /* restore to white color */
#else
    if (i_log_level != AVS2_LOG_INFO) {
        fprintf(stdout, "%s%s%s%s\n", cur_color, psz_prefix, psz_fmt, str_color_clear);
    } else {
        fprintf(stdout, "%s%s\n", psz_prefix, psz_fmt);
    }
#endif
}


/* ---------------------------------------------------------------------------
 */
void davs2_log(void *handle, int level, const char *format, ...)
{
    davs2_t *h = (davs2_t *)handle;
    int i_enable_level = 0;

    if (h != NULL) {
        i_enable_level = h->task_info.taskmgr->param.i_info_level;
    }

    assert(level >= 0 && level < AVS2_LOG_MAX);

    if (level >= i_enable_level) {
        char message[2048] = { 0 };
        
        if (h != NULL) {
            sprintf(message, "Dec[%d] %06llx: ", h->task_info.task_id, (uint64_t)(h));
        }

        va_list arg_ptr;
        va_start(arg_ptr, format);
        vsprintf(message + strlen(message), format, arg_ptr);
        va_end(arg_ptr);

        davs2_log_default(level, message);
    }
}
