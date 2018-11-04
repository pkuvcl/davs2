/*
 * utils.h
 *
 * Description of this file:
 *    functions definition of the davs2 library
 *
 * --------------------------------------------------------------------------
 *
 *    davs2 - video decoder of AVS2/IEEE1857.4 video coding standard
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

#ifndef DAVS2_UTILS_H
#define DAVS2_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "davs2.h"

#define CONSOLE_WHITE  0
#define CONSOLE_YELLOW 1
#define CONSOLE_RED    2
#define CONSOLE_GREEN  3

#if __ANDROID__
#include <jni.h>
#include <android/log.h>
#define LOGE(format,...) __android_log_print(ANDROID_LOG_ERROR,"davs2", format,##__VA_ARGS__)
#endif

#if _WIN32
#include <sys/types.h>
#include <sys/timeb.h>
#include <windows.h>
#else
#include <sys/time.h>
#endif
#include <time.h>

/* ---------------------------------------------------------------------------
 * time */
static __inline int64_t get_time()
{
#if _WIN32
    struct timeb tb;
    ftime(&tb);
    return ((int64_t)tb.time * CLOCKS_PER_SEC + (int64_t)tb.millitm);
#else
    struct timeval tv_date;
    gettimeofday(&tv_date, NULL);
    return (int64_t)(tv_date.tv_sec * CLOCKS_PER_SEC + (int64_t)tv_date.tv_usec);
#endif
}

#if _WIN32
/* ---------------------------------------------------------------------------
 */
static __inline void set_font_color(int color)
{
    WORD colors[] = {
        FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
        FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN,
        FOREGROUND_INTENSITY | FOREGROUND_RED,
        FOREGROUND_INTENSITY | FOREGROUND_GREEN,
    };
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), colors[color]);
}
#endif

/* ---------------------------------------------------------------------------
 */
static void show_message(int color, const char *format, ...)
{
    char message[1024] = { 0 };

    va_list arg_ptr;
    va_start(arg_ptr, format);

    vsprintf(message, format, arg_ptr);

    va_end(arg_ptr);

#if _WIN32
    set_font_color(color); /* set color */
    fprintf(stderr, "%s", message);
    set_font_color(0);     /* restore to white color */

#elif __ANDROID__
    LOGE("%s", message);
#else
    fprintf(stderr, "%s", message);
#endif
}

/* ---------------------------------------------------------------------------
 */
static __inline void show_progress(int frame, int frames)
{
    static int64_t first_time = 0;
    static int64_t last_time  = 0;
    float fps       = 0.0f;
    int64_t total_time  = 0;
    int64_t cur_time    = get_time();
    int eta;

    if (first_time == 0) {
        first_time = cur_time;
    } else {
        total_time = cur_time - first_time;
        fps = frame * 1.0f / total_time * CLOCKS_PER_SEC;
    }

    if (cur_time - last_time < 300 && frame != frames) {
        return;
    }

    last_time = cur_time;

    eta = (int)((frames - frame) * total_time / frame) / (CLOCKS_PER_SEC / 1000);

    show_message(CONSOLE_WHITE, "\r frames: %4d/%4d,  fps: %4.1f, LeftTime: %8.3f sec\r",
                 frame, frames, fps, eta * 0.001);
}

/* ---------------------------------------------------------------------------
 */
static
void write_frame_plane(FILE *fp_out, const uint8_t *p_src, int img_w, int img_h, int bytes_per_sample, int i_stride)
{
    const int size_line = img_w * bytes_per_sample;
    int i;

    for (i = 0; i < img_h; i++) {
        fwrite(p_src, size_line, 1, fp_out);
        p_src += i_stride;
    }
}


/* ---------------------------------------------------------------------------
 */
static 
void write_y4m_header(FILE *fp, int w, int h, int fps_num, int fps_den, int bit_depth)
{
    static int b_y4m_header_write = 0;

    if (fp != NULL && !b_y4m_header_write) {
        char buf[64];

        if (bit_depth != 8) {
            sprintf(buf, "YUV4MPEG2 W%d H%d F%d:%d Ip C%sp%d\n",
                    w, h, fps_num, fps_den, "420", bit_depth);
            fwrite(buf, 1, strlen(buf), fp);
        } else {
            sprintf(buf, "YUV4MPEG2 W%d H%d F%d:%d Ip C%s\n",
                    w, h, fps_num, fps_den, "420");
            fwrite(buf, 1, strlen(buf), fp);
        }

        b_y4m_header_write = 1;
    }
}


/* ---------------------------------------------------------------------------
 */
static
void write_frame(davs2_picture_t *pic, FILE *fp, int b_y4m)
{
    const int bytes_per_sample = pic->bytes_per_sample;

    if (b_y4m) {
        const char *s_frm = "FRAME\n";
        fwrite(s_frm, 1, strlen(s_frm), fp);
    }

    /* write y */
    write_frame_plane(fp, pic->planes[0], pic->widths[0], pic->lines[0], bytes_per_sample, pic->strides[0]);

    if (pic->num_planes == 3) {
        /* write u */
        write_frame_plane(fp, pic->planes[1], pic->widths[1], pic->lines[1], bytes_per_sample, pic->strides[1]);

        /* write v */
        write_frame_plane(fp, pic->planes[2], pic->widths[2], pic->lines[2], bytes_per_sample, pic->strides[2]);
    }
}

#endif /// DAVS2_UTILS_H
