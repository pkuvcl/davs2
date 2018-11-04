/*
 * psnr.h
 *
 * Description of this file:
 *    PSNR Calculating functions definition of the davs2 library
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

#ifndef DAVS2_PSNR_H
#define DAVS2_PSNR_H

#ifdef _MSC_VER
#undef fseek
#define fseek               _fseeki64
#else  //! for linux
#define _FILE_OFFSET_BITS   64       // for 64 bit fseeko
#define fseek               fseeko
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#if HAVE_STDINT_H
#include <stdint.h>
#else
#include <inttypes.h>
#endif

int g_width = 0;
int g_lines = 0;
int b_output_error_position = 1;

double g_sum_psnr_y = 0.0;
double g_sum_psnr_u = 0.0;
double g_sum_psnr_v = 0.0;

uint8_t *g_recbuf = NULL;

/* ---------------------------------------------------------------------------
 */
static __inline uint64_t
cal_ssd_16bit(int width, int height, uint16_t *rec, int rec_stride, uint16_t *dst, int dst_stride)
{
    uint64_t d = 0;
    int i, j;

    if (rec_stride == dst_stride) {
        if (memcmp(dst, rec, rec_stride * height * 2) == 0) {
            return 0;
        }
    }

    for (j = 0; j < height; j++) {
        for (i = 0; i < width; i++) {
            int t = dst[i] - rec[i];
            d += t * t;
        }

        rec += rec_stride;
        dst += dst_stride;
    }

    return d;
}

/* ---------------------------------------------------------------------------
 */
static __inline uint64_t
cal_ssd_8bit(int width, int height, uint8_t *rec, int rec_stride, uint8_t *dst, int dst_stride)
{
    uint64_t d = 0;
    int i, j;

    if (rec_stride == dst_stride) {
        if (memcmp(dst, rec, rec_stride * height) == 0) {
            return 0;
        }
    }

    for (j = 0; j < height; j++) {
        for (i = 0; i < width; i++) {
            int t = dst[i] - rec[i];
            d += t * t;
        }

        rec += rec_stride;
        dst += dst_stride;
    }

    return d;
}

/* ---------------------------------------------------------------------------
 * Function   : calculate the SSD of 2 frames
 * Parameters :
 *      [in ] : width      - width   of frame
 *            : height     - height  of frame
 *            : rec        - pointer to reconstructed frame buffer
 *            : rec_stride - stride  of reconstructed frame
 *            : dst        - pointer to decoded frame buffer
 *            : dst_stride - stride  of decoded frame
 *      [out] : none
 * Return     : mad of 2 frames
 * ---------------------------------------------------------------------------
 */
static __inline uint64_t
cal_ssd(int width, int height, uint8_t *rec, int rec_stride, uint8_t *dst, int dst_stride, int bytes_per_sample)
{
    if (bytes_per_sample == 2) {
        return cal_ssd_16bit(width, height, (uint16_t *)rec, rec_stride, (uint16_t *)dst, dst_stride >> 1);
    } else {
        return cal_ssd_8bit(width, height, rec, rec_stride, dst, dst_stride);
    }
}

/* ---------------------------------------------------------------------------
 */
static void
find_first_mismatch_point_16bit(int width, int height, uint16_t *rec, int rec_stride, uint16_t *dst, int dst_stride, int *x, int *y)
{
    int i, j;

    *x = -1;
    *y = -1;

    for (j = 0; j < height; j++) {
        for (i = 0; i < width; i++) {
            int t = dst[i] - rec[i];
            if (t != 0) {
                *x = i;
                *y = j;
                break;
            }
        }

        rec += rec_stride;
        dst += dst_stride;
    }
}

/* ---------------------------------------------------------------------------
 */
static void
find_first_mismatch_point_8bit(int width, int height, uint8_t *rec, int rec_stride, uint8_t *dst, int dst_stride, int *x, int *y)
{
    int i, j;

    *x = -1;
    *y = -1;

    for (j = 0; j < height; j++) {
        for (i = 0; i < width; i++) {
            int t = dst[i] - rec[i];
            if (t != 0) {
                *x = i;
                *y = j;
                break;
            }
        }

        rec += rec_stride;
        dst += dst_stride;
    }
}

/* ---------------------------------------------------------------------------
 * Function   : calculate the SSD of 2 frames
 * Parameters :
 *      [in ] : width      - width   of frame
 *            : height     - height  of frame
 *            : rec        - pointer to reconstructed frame buffer
 *            : rec_stride - stride  of reconstructed frame
 *            : dst        - pointer to decoded frame buffer
 *            : dst_stride - stride  of decoded frame
 *      [out] : none
 * Return     : x, y position of first mismatch point
 * ---------------------------------------------------------------------------
 */
static void
find_first_mismatch_point(int width, int height, uint8_t *rec, int rec_stride, uint8_t *dst, int dst_stride, int bytes_per_sample, int *x, int *y)
{
    if (bytes_per_sample == 2) {
        find_first_mismatch_point_16bit(width, height, (uint16_t *)rec, rec_stride, (uint16_t *)dst, dst_stride >> 1, x, y);
    } else {
        find_first_mismatch_point_8bit(width, height, rec, rec_stride, dst, dst_stride, x, y);
    }
}

/* ---------------------------------------------------------------------------
 */
static
double get_psnr_with_ssd(double f_max, uint64_t diff)
{
    if (diff > 0) {
        return 10.0 * log10(f_max / diff);
    } else {
        return 0;
    }
}

/* ---------------------------------------------------------------------------
* Function   : calculate and output the psnr (only for YUV 4:2:0)
* Parameters :
*      [in ] : rec    - pointer to buffer of reconstructed picture
*            : dst    - pointer to buffer of decoded picture
*            : width  - width  of picture
*            : height - height of picture
*      [out] : none
* Return     : void
* ---------------------------------------------------------------------------
*/
int 
cal_psnr(int number, uint8_t *dst[3], int strides[3], FILE *f_rec, int width, int height, int num_planes,
         double *psnr_y, double *psnr_u, double *psnr_v, int bytes_per_sample, int bit_depth)
{
    int stride_ref = width;          /* stride of frame/field (luma) */
    int size_l = width * height; /* size   of frame/field (luma) */
    uint8_t *p1;                 /* pointer to buffer of reconstructed picture */
    uint8_t *p2;                 /* pointer to buffer of decoded picture */
    uint64_t diff;               /* difference between decoded and reconstructed picture */
    size_t size_frame = num_planes == 3 ? (bytes_per_sample * size_l * 3) >> 1 : (bytes_per_sample * size_l); //solve warning C4018 
    double f_max_signal = ((1 << bit_depth) - 1) * ((1 << bit_depth) - 1);
    int64_t frameno = number;

    *psnr_y = *psnr_u = *psnr_v = 0.f;

    if (width != g_width || height != g_lines) {
        if (g_recbuf) {
            free(g_recbuf);
            g_recbuf = NULL;
        }

        g_recbuf = (uint8_t *)malloc(size_frame);

        if (g_recbuf == NULL) {
            return -1;
        }

        g_width = width;
        g_lines = height;
    }

    if (g_recbuf == 0) {
        return -1;
    }

    fseek(f_rec, size_frame * frameno, SEEK_SET);

    if (fread(g_recbuf, 1, size_frame, f_rec) < size_frame) {
        return -1;
    }

    p1 = g_recbuf;
    p2 = dst[0];

    diff = cal_ssd(width, height, p1, stride_ref, p2, strides[0], bytes_per_sample);
    *psnr_y = get_psnr_with_ssd(f_max_signal * size_l, diff);
    g_sum_psnr_y += *psnr_y;
    if (diff != 0 && b_output_error_position) {
        int x, y;
        find_first_mismatch_point(width, height, p1, stride_ref, p2, strides[0], bytes_per_sample, &x, &y);
        show_message(CONSOLE_RED, "mismatch POC: %3d, Y(%d, %d)\n", number, x, y);
        b_output_error_position = 0;
    }

    if (num_planes == 3) {
        width >>= 1;               // width  of frame/field  (chroma)
        height >>= 1;              // height of frame/field  (chroma, with padding)
        stride_ref >>= 1;          // stride of frame/field  (chroma)

        /* PSNR U */
        p1 += size_l * bytes_per_sample;
        p2 = dst[1];

        diff = cal_ssd(width, height, p1, stride_ref, p2, strides[1], bytes_per_sample);
        *psnr_u = get_psnr_with_ssd(f_max_signal * size_l, diff << 2);
        g_sum_psnr_u += *psnr_u;
        if (diff != 0 && b_output_error_position) {
            int x, y;
            find_first_mismatch_point(width, height, p1, stride_ref, p2, strides[1], bytes_per_sample, &x, &y);
            show_message(CONSOLE_RED, "mismatch POC: %3d, U (%d, %d) => Y(%d, %d)\n", number, x, y, 2 * x, 2 * y);
            b_output_error_position = 0;
        }

        /* PSNR V */
        p1 += (size_l * bytes_per_sample) >> 2;
        p2 = dst[2];

        diff = cal_ssd(width, height, p1, stride_ref, p2, strides[2], bytes_per_sample);
        *psnr_v = get_psnr_with_ssd(f_max_signal * size_l, diff << 2);
        g_sum_psnr_v += *psnr_v;
        if (diff != 0 && b_output_error_position) {
            int x, y;
            find_first_mismatch_point(width, height, p1, stride_ref, p2, strides[2], bytes_per_sample, &x, &y);
            show_message(CONSOLE_RED, "mismatch POC: %3d, V (%d, %d) => Y(%d, %d)\n", number, x, y, 2 * x, 2 * y);
            b_output_error_position = 0;
        }
    }

    return 0;
}

#endif /// DAVS2_PSNR_H
