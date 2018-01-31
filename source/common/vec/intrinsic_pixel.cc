/*
 * intrinsic_pixel.cc
 *
 * Description of this file:
 *    SSE assembly functions of Pixel-Processing module of the davs2 library
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

#include "../common.h"
#include "intrinsic.h"

#include <mmintrin.h>
#include <emmintrin.h>
#include <tmmintrin.h>
#include <smmintrin.h>



void avs_pixel_average_sse128(pel_t *dst, int i_dst, const pel_t *src0, int i_src0, const pel_t *src1, int i_src1, int width, int height)
{
#if HIGH_BIT_DEPTH
    int j;
    __m128i D;

    if (width & 7) {
        __m128i mask = _mm_load_si128((const __m128i *)intrinsic_mask_10bit[(width & 7) - 1]);

        while (height--) {
            for (j = 0; j < width - 7; j += 8) {
                D = _mm_avg_epu16(_mm_loadu_si128((const __m128i *)(src0 + j)), _mm_loadu_si128((const __m128i *)(src1 + j)));
                _mm_storeu_si128((__m128i *)(dst + j), D);
            }

            D = _mm_avg_epu16(_mm_loadu_si128((const __m128i *)(src0 + j)), _mm_loadu_si128((const __m128i *)(src1 + j)));
            _mm_maskmoveu_si128(D, mask, (char *)&dst[j]);

            src0 += i_src0;
            src1 += i_src1;
            dst += i_dst;
        }
    } else {
        while (height--) {
            for (j = 0; j < width; j += 8) {
                D = _mm_avg_epu16(_mm_loadu_si128((const __m128i *)(src0 + j)), _mm_loadu_si128((const __m128i *)(src1 + j)));
                _mm_storeu_si128((__m128i *)(dst + j), D);
            }
            src0 += i_src0;
            src1 += i_src1;
            dst += i_dst;
        }
    }
#else
    int i, j;
    __m128i S1, S2, D;

    if (width & 15) {
        __m128i mask = _mm_load_si128((const __m128i*)intrinsic_mask[(width & 15) - 1]);

        for (i = 0; i < height; i++) {
            for (j = 0; j < width - 15; j += 16) {
                S1 = _mm_loadu_si128((const __m128i*)(src0 + j));
                S2 = _mm_loadu_si128((const __m128i*)(src1 + j));
                D  = _mm_avg_epu8(S1, S2);
                _mm_storeu_si128((__m128i*)(dst + j), D);
            }

            S1 = _mm_loadu_si128((const __m128i*)(src0 + j));
            S2 = _mm_loadu_si128((const __m128i*)(src1 + j));
            D  = _mm_avg_epu8(S1, S2);
            _mm_maskmoveu_si128(D, mask, (char*)(dst + j));

            src0 += i_src0;
            src1 += i_src1;
            dst  += i_dst;
        }
    } else {
        for (i = 0; i < height; i++) {
            for (j = 0; j < width; j += 16) {
                S1 = _mm_loadu_si128((const __m128i*)(src0 + j));
                S2 = _mm_loadu_si128((const __m128i*)(src1 + j));
                D  = _mm_avg_epu8(S1, S2);
                _mm_storeu_si128((__m128i*)(dst + j), D);
            }
            src0 += i_src0;
            src1 += i_src1;
            dst += i_dst;
        }
    }
#endif
}

/* ---------------------------------------------------------------------------
 */
void *davs2_memzero_aligned_c_sse2(void *dst, size_t n)
{
    __m128i *p_dst = (__m128i *)dst;
    __m128i m0 = _mm_setzero_si128();
    int i = (int)(n >> 4);

    for (; i != 0; i--) {
        _mm_store_si128(p_dst, m0);
        p_dst++;
    }

    return dst;
}

/* ---------------------------------------------------------------------------
 */
void *davs2_memcpy_aligned_c_sse2(void *dst, const void *src, size_t n)
{
    __m128i *p_dst = (__m128i *)dst;
    const __m128i *p_src = (const __m128i *)src;
    int i = (int)(n >> 4);

    for (; i != 0; i--) {
        _mm_store_si128(p_dst, _mm_load_si128(p_src));
        p_src++;
        p_dst++;
    }

    return dst;
}

/* ---------------------------------------------------------------------------
 */
void plane_copy_c_sse2(pel_t *dst, intptr_t i_dst, pel_t *src, intptr_t i_src, int w, int h)
{
    const int n128 = (w * sizeof(pel_t)) >> 4;
    int n_left = (w * sizeof(pel_t)) - (n128 << 4);

    if (n_left) {
        int n_offset = (n128 << 4);
        while (h--) {
            const __m128i *p_src = (const __m128i *)src;
            __m128i *p_dst = (__m128i *)dst;
            int n = n128;
            for (; n != 0; n--) {
                _mm_storeu_si128(p_dst, _mm_loadu_si128(p_src));
                p_dst++;
                p_src++;
            }
            memcpy((uint8_t *)(dst) + n_offset, (uint8_t *)(src) + n_offset, n_left);
            dst += i_dst;
            src += i_src;
        }
    } else {
        while (h--) {
            const __m128i *p_src = (const __m128i *)src;
            __m128i *p_dst = (__m128i *)dst;
            int n = n128;
            for (; n != 0; n--) {
                _mm_storeu_si128(p_dst, _mm_loadu_si128(p_src));
                p_dst++;
                p_src++;
            }
            dst += i_dst;
            src += i_src;
        }
    }
}
