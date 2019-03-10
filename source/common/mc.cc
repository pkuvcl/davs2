/*
 * mc.cc
 *
 * Description of this file:
 *    MC functions definition of the davs2 library
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

#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "mc.h"

#if HAVE_MMX
#include "vec/intrinsic.h"
#include "x86/ipfilter8.h"
#endif


#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
/* ---------------------------------------------------------------------------
 * disable warning C4127: conditional expression is constant.
 */
#pragma warning(disable: 4127)
#endif


/**
 * ===========================================================================
 * local & global variables
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 * interpolate filter (luma) */
ALIGN16(static const int8_t INTPL_FILTERS[4][8]) = {
    {  0, 0,   0, 64,  0,  0,  0,  0 }, /* for full-pixel, no use */
    { -1, 4, -10, 57, 19, -7,  3, -1 },
    { -1, 4, -11, 40, 40, -11, 4, -1 },
    { -1, 3,  -7, 19, 57, -10, 4, -1 }
};

/* ---------------------------------------------------------------------------
 * interpolate filter (chroma) */
ALIGN16(static const int8_t INTPL_FILTERS_C[8][4]) = {
    {  0, 64,  0,  0 },                 /* for full-pixel, no use */
    { -4, 62,  6,  0 },
    { -6, 56, 15, -1 },
    { -5, 47, 25, -3 },
    { -4, 36, 36, -4 },
    { -3, 25, 47, -5 },
    { -1, 15, 56, -6 },
    {  0,  6, 62, -4 }
};


/**
 * ===========================================================================
 * macros
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 * for luma interpolating (horizontal) */
#define FLT_8TAP_HOR(src, i, coef) (\
    (src)[i - 3] * (coef)[0] + \
    (src)[i - 2] * (coef)[1] + \
    (src)[i - 1] * (coef)[2] + \
    (src)[i    ] * (coef)[3] + \
    (src)[i + 1] * (coef)[4] + \
    (src)[i + 2] * (coef)[5] + \
    (src)[i + 3] * (coef)[6] + \
    (src)[i + 4] * (coef)[7])

/* ---------------------------------------------------------------------------
 * for luma interpolating (vertical) */
#define FLT_8TAP_VER(src, i, i_src, coef) (\
    (src)[i - 3 * i_src] * (coef)[0] + \
    (src)[i - 2 * i_src] * (coef)[1] + \
    (src)[i -     i_src] * (coef)[2] + \
    (src)[i            ] * (coef)[3] + \
    (src)[i +     i_src] * (coef)[4] + \
    (src)[i + 2 * i_src] * (coef)[5] + \
    (src)[i + 3 * i_src] * (coef)[6] + \
    (src)[i + 4 * i_src] * (coef)[7])

/* ---------------------------------------------------------------------------
 * for chroma interpolating (horizontal) */
#define FLT_4TAP_HOR(src, i, coef) (\
    (src)[i - 1] * (coef)[0] + \
    (src)[i    ] * (coef)[1] + \
    (src)[i + 1] * (coef)[2] + \
    (src)[i + 2] * (coef)[3])

/* ---------------------------------------------------------------------------
 * for chroma interpolating (vertical) */
#define FLT_4TAP_VER(src, i, i_src, coef) (\
    (src)[i -     i_src] * (coef)[0] + \
    (src)[i            ] * (coef)[1] + \
    (src)[i +     i_src] * (coef)[2] + \
    (src)[i + 2 * i_src] * (coef)[3])


/**
 * ===========================================================================
 * interpolate
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 */
static void
mc_block_copy_c(pel_t *dst, intptr_t i_dst, pel_t *src, intptr_t i_src, int w, int h)
{
    while (h--) {
        memcpy(dst, src, w * sizeof(pel_t));
        dst += i_dst;
        src += i_src;
    }
}

/* ---------------------------------------------------------------------------
 */
static void
mc_block_copy_sc_c(coeff_t *dst, intptr_t i_dst, int16_t *src, intptr_t i_src, int w, int h)
{
    int i;

    if (sizeof(coeff_t) == sizeof(int16_t)) {
        while (h--) {
            memcpy(dst, src, w * sizeof(coeff_t));
            dst += i_dst;
            src += i_src;
        }
    } else {
        while (h--) {
            for (i = 0; i < w; i++) {
                dst[i] = src[i];
            }
            dst += i_dst;
            src += i_src;
        }
    }
}

/* ---------------------------------------------------------------------------
 */
static void
intpl_chroma_block_hor_c(pel_t *dst, int i_dst, pel_t *src, int i_src, int width, int height, int8_t const *coeff)
{
    int x, y, v;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            v = (FLT_4TAP_HOR(src, x, coeff) + 32) >> 6;
            dst[x] = (pel_t)DAVS2_CLIP1(v);
        }

        src += i_src;
        dst += i_dst;
    }
}

/* ---------------------------------------------------------------------------
 */
static void
intpl_chroma_block_ver_c(pel_t *dst, int i_dst, pel_t *src, int i_src, int width, int height, int8_t const *coeff)
{
    int x, y, v;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            v = (FLT_4TAP_VER(src, x, i_src, coeff) + 32) >> 6;
            dst[x] = (pel_t)DAVS2_CLIP1(v);
        }

        src += i_src;
        dst += i_dst;
    }
}

/* ---------------------------------------------------------------------------
 */
static void
intpl_chroma_block_ext_c(pel_t *dst, int i_dst, pel_t *src, int i_src, int width, int height, const int8_t *coeff_h, const int8_t *coeff_v)
{
    // TODO: 与luma统一数据类型
    ALIGN16(int32_t tmp_res[(32 + 3) * 32]);
    int32_t *tmp = tmp_res;
    const int shift1 = g_bit_depth - 8;
    const int add1   = (1 << shift1) >> 1;
    const int shift2 = 20 - g_bit_depth;
    const int add2   = 1 << (shift2 - 1); // 1<<(19-g_bit_depth)
    int x, y, v;

    src -= i_src;
    for (y = -1; y < height + 2; y++) {
        for (x = 0; x < width; x++) {
            v = FLT_4TAP_HOR(src, x, coeff_h);
            tmp[x] = (v + add1) >> shift1;
        }
        src += i_src;
        tmp += 32;
    }
    tmp = tmp_res + 32;
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            v = (FLT_4TAP_VER(tmp, x, 32, coeff_v) + add2) >> shift2;
            dst[x] = (pel_t)DAVS2_CLIP1(v);
        }
        dst += i_dst;
        tmp += 32;
    }
}

/* ---------------------------------------------------------------------------
 */
static void
intpl_luma_block_hor_c(pel_t *dst, int i_dst, pel_t *src, int i_src, int width, int height, int8_t const *coeff)
{
    int x, y, v;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            v = (FLT_8TAP_HOR(src, x, coeff) + 32) >> 6;
            dst[x] = (pel_t)DAVS2_CLIP1(v);
        }

        src += i_src;
        dst += i_dst;
    }
}

/* ---------------------------------------------------------------------------
 */
static void
intpl_luma_block_ver_c(pel_t *dst, int i_dst, pel_t *src, int i_src, int width, int height, int8_t const *coeff)
{
    int x, y, v;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            v = (FLT_8TAP_VER(src, x, i_src, coeff) + 32) >> 6;
            dst[x] = (pel_t)DAVS2_CLIP1(v);
        }

        src += i_src;
        dst += i_dst;
    }
}

/* ---------------------------------------------------------------------------
 */
static void
intpl_luma_block_ext_c(pel_t *dst, int i_dst, pel_t *src, int i_src, int width, int height, const int8_t *coeff_h, const int8_t *coeff_v)
{
#define TMP_STRIDE      64
    
    const int shift1 = g_bit_depth - 8;
    const int add1   = (1 << shift1) >> 1;
    const int shift2 = 20 - g_bit_depth;
    const int add2   = 1 << (shift2 - 1);//1<<(19-bit_depth)

    ALIGN16(mct_t tmp_buf[(64 + 7) * TMP_STRIDE]);
    mct_t *tmp = tmp_buf;
    int x, y, v;

    src -= 3 * i_src;
    for (y = -3; y < height + 4; y++) {
        for (x = 0; x < width; x++) {
            v = FLT_8TAP_HOR(src, x, coeff_h);
            tmp[x] = (mct_t)((v + add1) >> shift1);
        }
        src += i_src;
        tmp += TMP_STRIDE;
    }

    tmp = tmp_buf + 3 * TMP_STRIDE;
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            v = (FLT_8TAP_VER(tmp, x, TMP_STRIDE, coeff_v) + add2) >> shift2;
            dst[x] = (pel_t)DAVS2_CLIP1(v);
        }

        dst += i_dst;
        tmp += TMP_STRIDE;
    }

#undef TMP_STRIDE
}

/* ---------------------------------------------------------------------------
 */
#define INTERP_HOR_C(width, height) \
static void interp_horiz_pp_##width##x##height##_c(const pel_t* src, intptr_t srcStride, pel_t* dst, intptr_t dstStride, int coeffIdx) \
{ \
    const int N = 8;  /* 8-tap Luma interpolation */ \
    const int8_t* coeff = (N == 4) ? INTPL_FILTERS_C[coeffIdx] : INTPL_FILTERS[coeffIdx]; \
    int headRoom = 6; /* Log2 of sum of filter taps */ \
    int offset = (1 << (headRoom - 1)); \
    uint16_t maxVal = (1 << BIT_DEPTH) - 1; \
    int cStride = 1; \
    src -= (N / 2 - 1) * cStride; \
    int row, col; \
    for (row = 0; row < height; row++) {     \
        for (col = 0; col < width; col++) {  \
            int sum = src[col + 0 * cStride] * coeff[0];   \
            sum += src[col + 1 * cStride] * coeff[1];      \
            sum += src[col + 2 * cStride] * coeff[2];      \
            sum += src[col + 3 * cStride] * coeff[3];      \
            if (N == 8) {                                  \
                sum += src[col + 4 * cStride] * coeff[4];  \
                sum += src[col + 5 * cStride] * coeff[5];  \
                sum += src[col + 6 * cStride] * coeff[6];  \
                sum += src[col + 7 * cStride] * coeff[7];  \
            }                                              \
            int16_t val = (int16_t)((sum + offset) >> headRoom); \
            val = DAVS2_CLIP3(0, maxVal, val);                    \
            dst[col] = (pel_t)val;                               \
        } \
        src += srcStride; \
        dst += dstStride; \
    } \
}

/* ---------------------------------------------------------------------------
 */
#define INTERP_PS_HOR_C(width, height) \
static void interp_horiz_ps_##width##x##height##_c(const pel_t* src, intptr_t srcStride, int16_t* dst, intptr_t dstStride, int coeffIdx) \
{ \
    const int N = 8;  /* 8-tap Luma interpolation */ \
    const int8_t* coeff = (N == 4) ? INTPL_FILTERS_C[coeffIdx] : INTPL_FILTERS[coeffIdx]; \
    int headRoom = 6; /* Log2 of sum of filter taps */ \
    int offset = (1 << (headRoom - 1)); \
    uint16_t maxVal = (1 << BIT_DEPTH) - 1; \
    int cStride = 1; \
    src -= (N / 2 - 1) * cStride; \
    int row, col; \
    for (row = 0; row < height; row++) {     \
        for (col = 0; col < width; col++) {  \
            int sum = src[col + 0 * cStride] * coeff[0];   \
            sum += src[col + 1 * cStride] * coeff[1];      \
            sum += src[col + 2 * cStride] * coeff[2];      \
            sum += src[col + 3 * cStride] * coeff[3];      \
            if (N == 8) {                                  \
                sum += src[col + 4 * cStride] * coeff[4];  \
                sum += src[col + 5 * cStride] * coeff[5];  \
                sum += src[col + 6 * cStride] * coeff[6];  \
                sum += src[col + 7 * cStride] * coeff[7];  \
            }                                              \
            int16_t val = (int16_t)((sum + offset) >> headRoom); \
            val = DAVS2_CLIP3(0, maxVal, val);                    \
            dst[col] = (pel_t)val;                               \
        } \
        src += srcStride; \
        dst += dstStride; \
    } \
}

/* ---------------------------------------------------------------------------
 */
#define INTERP_VER_C(width, height) \
static void interp_vert_pp_##width##x##height##_c(const pel_t* src, intptr_t srcStride, pel_t* dst, intptr_t dstStride, int coeffIdx) \
{ \
    const int N = 8;  /* 8-tap Luma interpolation */ \
    const int8_t* c = (N == 4) ? INTPL_FILTERS_C[coeffIdx] : INTPL_FILTERS[coeffIdx]; \
    int shift = 6; \
    int offset = 1 << (shift - 1); \
    uint16_t maxVal = (1 << BIT_DEPTH) - 1; \
    src -= (N / 2 - 1) * srcStride; \
    int row, col; \
    for (row = 0; row < height; row++) {    \
        for (col = 0; col < width; col++) { \
            int sum = src[col + 0 * srcStride] * c[0];  \
            sum += src[col + 1 * srcStride] * c[1];     \
            sum += src[col + 2 * srcStride] * c[2];     \
            sum += src[col + 3 * srcStride] * c[3];     \
            if (N == 8) {                               \
                sum += src[col + 4 * srcStride] * c[4]; \
                sum += src[col + 5 * srcStride] * c[5]; \
                sum += src[col + 6 * srcStride] * c[6]; \
                sum += src[col + 7 * srcStride] * c[7]; \
            }                                           \
            int16_t val = (int16_t)((sum + offset) >> shift); \
            val = DAVS2_CLIP3(0, maxVal, val);                 \
            dst[col] = (pel_t)val;                            \
        } \
        src += srcStride;    \
        dst += dstStride;    \
    }                        \
}

/* ---------------------------------------------------------------------------
 */
#define INTERP_SP_VER_C(w, h) \
static void filterVertical_sp_##w##x##h##_c(const int16_t* src, intptr_t srcStride, pel_t* dst, intptr_t dstStride, int coeffIdx) \
{ \
    const int N = 8;  /* 8-tap Luma interpolation */ \
    int headRoom = 14 - BIT_DEPTH; \
    int shift = 6 + headRoom; \
    int offset = (1 << (shift - 1)) + ((1 << 13) << 6); \
    const int8_t* c = (N == 8 ? INTPL_FILTERS_C[coeffIdx] : INTPL_FILTERS[coeffIdx]); \
    int16_t maxVal = (1 << BIT_DEPTH) - 1;  \
    src -= (N / 2 - 1) * srcStride; \
    int row, col; \
    for (row = 0; row < h; row++) {     \
        for (col = 0; col < w; col++) { \
            int sum = src[col + 0 * srcStride] * c[0];  \
            sum += src[col + 1 * srcStride] * c[1];     \
            sum += src[col + 2 * srcStride] * c[2];     \
            sum += src[col + 3 * srcStride] * c[3];     \
            if (N == 8) {                               \
                sum += src[col + 4 * srcStride] * c[4]; \
                sum += src[col + 5 * srcStride] * c[5]; \
                sum += src[col + 6 * srcStride] * c[6]; \
                sum += src[col + 7 * srcStride] * c[7]; \
            }                                           \
            int16_t val = (int16_t)((sum + offset) >> shift); \
            val = DAVS2_CLIP3(0, maxVal, val);                 \
            dst[col] = (pel_t)val;                            \
        } \
        src += srcStride;    \
        dst += dstStride;    \
    }                        \
}

/* ---------------------------------------------------------------------------
 */
#define INTERP_EXT_C(width, height) \
static void interp_hv_pp_##width##x##height##_c(const pel_t* src, intptr_t srcStride, pel_t* dst, intptr_t dstStride, int idxX, int idxY) \
{ \
    int16_t immedVals[(64 + 8) * (64 + 8)]; \
    interp_horiz_ps_##width##x##height##_c(src, srcStride, immedVals, width, idxX); \
    filterVertical_sp_##width##x##height##_c(immedVals + 3 * width, width, dst, dstStride, idxY); \
}

#define INTPL_OP_C(w, h) \
    INTERP_HOR_C(w, h) \
    INTERP_PS_HOR_C(w, h) \
    INTERP_VER_C(w, h)    \
    INTERP_SP_VER_C(w, h) \
    INTERP_EXT_C(w, h)

//INTPL_OP_C(64, 64)  /* 64x64 */
//INTPL_OP_C(64, 32)
//INTPL_OP_C(32, 64)
//INTPL_OP_C(64, 16)
//INTPL_OP_C(64, 48)
//INTPL_OP_C(16, 64)
//INTPL_OP_C(48, 64)
//INTPL_OP_C(32, 32)  /* 32x32 */
//INTPL_OP_C(32, 16)
//INTPL_OP_C(16, 32)
//INTPL_OP_C(32, 8)
//INTPL_OP_C(32, 24)
//INTPL_OP_C(8, 32)
//INTPL_OP_C(24, 32)
//INTPL_OP_C(16, 16)  /* 16x16 */
//INTPL_OP_C(16, 8)
//INTPL_OP_C(8, 16)
//INTPL_OP_C(16, 4)
//INTPL_OP_C(16, 12)
//INTPL_OP_C(4, 16)
//INTPL_OP_C(12, 16)
//INTPL_OP_C(8, 8)  /* 8x8 */
//INTPL_OP_C(8, 4)
//INTPL_OP_C(4, 8)
//INTPL_OP_C(4, 4)  /* 4x4 */

/* ---------------------------------------------------------------------------
 * interpolation of 1/4 subpixel
 *      A  dst  1  src  B
 *      c  d  e  f
 *      2  h  3  i
 *      j  k  l  m
 *      C           D
 */
void mc_luma(davs2_t *h, pel_t *dst, int i_dst, int posx, int posy, int width, int height, pel_t *p_fref, int i_fref)
{
    const int dx = posx & 3;
    const int dy = posy & 3;
    const int mc_part_index = MC_PART_INDEX(width, height);


    UNUSED_PARAMETER(h);
    posx >>= 2;
    posy >>= 2;
    

    p_fref += posy * i_fref + posx;
    
    if (dx == 0 && dy == 0) {
        gf_davs2.copy_pp[PART_INDEX(width, height)](dst, i_dst, p_fref, i_fref);
    } else if (dx == 0) {
#if USE_NEW_INTPL
        gf_davs2.block_intpl_luma_ver[PART_INDEX(width, height)](p_fref, i_fref, dst, i_dst, dy);
#else
        gf_davs2.intpl_luma_ver[mc_part_index][dy - 1](dst, i_dst, p_fref, i_fref, width, height, INTPL_FILTERS[dy]);
#endif
    } else if (dy == 0) {
#if USE_NEW_INTPL
        gf_davs2.block_intpl_luma_hor[PART_INDEX(width, height)](p_fref, i_fref, dst, i_dst, dx);
#else
        gf_davs2.intpl_luma_hor[mc_part_index][dx - 1](dst, i_dst, p_fref, i_fref, width, height, INTPL_FILTERS[dx]);
#endif
    } else {
        gf_davs2.intpl_luma_ext[mc_part_index](dst, i_dst, p_fref, i_fref, width, height, INTPL_FILTERS[dx], INTPL_FILTERS[dy]);
    }
}

/* ---------------------------------------------------------------------------
 */
void mc_chroma(davs2_t *h, pel_t *dst, int i_dst, int posx, int posy, int width, int height, pel_t *p_fref, int i_fref)
{
    const int dx = posx & 7;
    const int dy = posy & 7;
    const int mc_part_index = MC_PART_INDEX(width, height);

    UNUSED_PARAMETER(h);
    posx >>= 3;
    posy >>= 3;

    p_fref += posy * i_fref + posx;

    if (dx == 0 && dy == 0) {
        if (width != 2 && width != 6 && height != 2 && height != 6) {
            gf_davs2.copy_pp[PART_INDEX(width, height)](dst, i_dst, p_fref, i_fref);
        } else {
            gf_davs2.block_copy(dst, i_dst, p_fref, i_fref, width, height);
        }
    } else if (dx == 0) {
        gf_davs2.intpl_chroma_ver[mc_part_index](dst, i_dst, p_fref, i_fref, width, height, INTPL_FILTERS_C[dy]);
    } else if (dy == 0) {
        gf_davs2.intpl_chroma_hor[mc_part_index](dst, i_dst, p_fref, i_fref, width, height, INTPL_FILTERS_C[dx]);
    } else {
        gf_davs2.intpl_chroma_ext[mc_part_index](dst, i_dst, p_fref, i_fref, width, height, INTPL_FILTERS_C[dx], INTPL_FILTERS_C[dy]);
    }
}


/**
 * ===========================================================================
 * pixel block average
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 */
static void davs2_pixel_average_c(pel_t *dst, int i_dst, const pel_t *src0, int i_src0, const pel_t *src1, int i_src1, int width, int height)
{
    int i, j;

    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++) {
            dst[j] = (pel_t)((src0[j] + src1[j] + 1) >> 1);
        }
        dst  += i_dst;
        src0 += i_src0;
        src1 += i_src1;
    }
}

/**
 * ===========================================================================
 * plane copy
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 */
#define plane_copy_c          mc_block_copy_c

#define ALL_LUMA_PU(name1, name2, cpu) \
    pf->name1[PART_64x64] = name2 ## _64x64 ##_## cpu;  /* 64x64 */ \
    pf->name1[PART_64x32] = name2 ## _64x32 ##_## cpu;\
    pf->name1[PART_32x64] = name2 ## _32x64 ##_## cpu;\
    pf->name1[PART_64x16] = name2 ## _64x16 ##_## cpu;\
    pf->name1[PART_64x48] = name2 ## _64x48 ##_## cpu;\
    pf->name1[PART_16x64] = name2 ## _16x64 ##_## cpu;\
    pf->name1[PART_48x64] = name2 ## _48x64 ##_## cpu;\
    pf->name1[PART_32x32] = name2 ## _32x32 ##_## cpu;  /* 32x32 */ \
    pf->name1[PART_32x16] = name2 ## _32x16 ##_## cpu;\
    pf->name1[PART_16x32] = name2 ## _16x32 ##_## cpu;\
    pf->name1[PART_32x8 ] = name2 ## _32x8  ##_## cpu;\
    pf->name1[PART_32x24] = name2 ## _32x24 ##_## cpu;\
    pf->name1[PART_8x32 ] = name2 ## _8x32  ##_## cpu;\
    pf->name1[PART_24x32] = name2 ## _24x32 ##_## cpu;\
    pf->name1[PART_16x16] = name2 ## _16x16 ##_## cpu;  /* 16x16 */ \
    pf->name1[PART_16x8 ] = name2 ## _16x8  ##_## cpu;\
    pf->name1[PART_8x16 ] = name2 ## _8x16  ##_## cpu;\
    pf->name1[PART_16x4 ] = name2 ## _16x4  ##_## cpu;\
    pf->name1[PART_16x12] = name2 ## _16x12 ##_## cpu;\
    pf->name1[PART_4x16 ] = name2 ## _4x16  ##_## cpu;\
    pf->name1[PART_12x16] = name2 ## _12x16 ##_## cpu;\
    pf->name1[PART_8x8  ] = name2 ## _8x8   ##_## cpu;  /* 8x8 */ \
    pf->name1[PART_8x4  ] = name2 ## _8x4   ##_## cpu;\
    pf->name1[PART_4x8  ] = name2 ## _4x8   ##_## cpu;\
    pf->name1[PART_4x4  ] = name2 ## _4x4   ##_## cpu  /* 4x4 */

/**
 * ===========================================================================
 * interface function defines
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 */
void davs2_mc_init(uint32_t cpuid, ao_funcs_t *pf)
{
    UNUSED_PARAMETER(cpuid);

    /* plane copy */
    pf->plane_copy       = plane_copy_c;

    pf->block_copy       = mc_block_copy_c;
    pf->block_coeff_copy = mc_block_copy_sc_c;

    /* block average */
    pf->block_avg        = davs2_pixel_average_c;

    /* interpolate */
#if USE_NEW_INTPL
    ALL_LUMA_PU(block_intpl_luma_hor, interp_horiz_pp, c);
    ALL_LUMA_PU(block_intpl_luma_ver, interp_vert_pp, c);
    ALL_LUMA_PU(block_intpl_luma_ext, interp_hv_pp, c);
#endif
    pf->intpl_luma_ver[0][0] = intpl_luma_block_ver_c;
    pf->intpl_luma_ver[0][1] = intpl_luma_block_ver_c;
    pf->intpl_luma_ver[0][2] = intpl_luma_block_ver_c;
    pf->intpl_luma_hor[0][0] = intpl_luma_block_hor_c;
    pf->intpl_luma_hor[0][1] = intpl_luma_block_hor_c;
    pf->intpl_luma_hor[0][2] = intpl_luma_block_hor_c;
    pf->intpl_luma_ext[0] = intpl_luma_block_ext_c;

    pf->intpl_chroma_ver[0] = intpl_chroma_block_ver_c;
    pf->intpl_chroma_hor[0] = intpl_chroma_block_hor_c;
    pf->intpl_chroma_ext[0] = intpl_chroma_block_ext_c;

    pf->intpl_luma_ver[1][0] = intpl_luma_block_ver_c;
    pf->intpl_luma_ver[1][1] = intpl_luma_block_ver_c;
    pf->intpl_luma_ver[1][2] = intpl_luma_block_ver_c;
    pf->intpl_luma_hor[1][0] = intpl_luma_block_hor_c;
    pf->intpl_luma_hor[1][1] = intpl_luma_block_hor_c;
    pf->intpl_luma_hor[1][2] = intpl_luma_block_hor_c;
    pf->intpl_luma_ext[1] = intpl_luma_block_ext_c;

    pf->intpl_chroma_ver[1] = intpl_chroma_block_ver_c;
    pf->intpl_chroma_hor[1] = intpl_chroma_block_hor_c;
    pf->intpl_chroma_ext[1] = intpl_chroma_block_ext_c;

    /* init asm function handles */
#if HAVE_MMX
    if (cpuid & DAVS2_CPU_SSE42) {
#if HIGH_BIT_DEPTH
        //10bit assemble
#else
#if USE_NEW_INTPL
        ALL_LUMA_PU(block_intpl_luma_hor, davs2_interp_8tap_horiz_pp, sse4);
        pf->block_intpl_luma_hor[PART_4x4] = davs2_interp_8tap_horiz_pp_4x4_sse4;
        ALL_LUMA_PU(block_intpl_luma_ver, davs2_interp_8tap_vert_pp, sse4);
        pf->block_intpl_luma_ver[PART_4x4] = davs2_interp_8tap_vert_pp_4x4_sse4;
        /* linking error */
        // ALL_LUMA_PU(block_intpl_luma_ext, davs2_interp_8tap_hv_pp, sse4);
        // pf->block_intpl_luma_ext[PART_4x4] = davs2_interp_8tap_hv_pp_4x4_sse4;
#endif
#endif //if HIGH_BIT_DEPTH
    }

    if (cpuid & DAVS2_CPU_SSE2) {
        /* memory copy */
        pf->plane_copy = plane_copy_c_sse2;
    }

    if (cpuid & DAVS2_CPU_SSE4) {
        /* block average */
        pf->block_avg        = avs_pixel_average_sse128;

#if !HIGH_BIT_DEPTH
        /* interpolate */
        pf->intpl_luma_hor[0][0] = intpl_luma_block_hor_sse128;
        pf->intpl_luma_hor[0][1] = intpl_luma_block_hor_sse128;
        pf->intpl_luma_hor[0][2] = intpl_luma_block_hor_sse128;
        pf->intpl_luma_ext[0] = intpl_luma_block_ext_sse128;

        /*两个差值函数有不匹配问题。
          修改时请注意关闭avx2汇编函数。
         */
        //pf->intpl_chroma_ver[0] = intpl_chroma_block_ver_sse128;
        pf->intpl_chroma_hor[0] = intpl_chroma_block_hor_sse128;
        pf->intpl_chroma_ext[0] = intpl_chroma_block_ext_sse128;
        
        pf->intpl_luma_hor[1][0] = intpl_luma_block_hor_sse128;
        pf->intpl_luma_hor[1][1] = intpl_luma_block_hor_sse128;
        pf->intpl_luma_hor[1][2] = intpl_luma_block_hor_sse128;
        pf->intpl_luma_ext[1] = intpl_luma_block_ext_sse128;
        
        //pf->intpl_chroma_ver[1] = intpl_chroma_block_ver_sse128;
        pf->intpl_chroma_hor[1] = intpl_chroma_block_hor_sse128;
        pf->intpl_chroma_ext[1] = intpl_chroma_block_ext_sse128;

        pf->intpl_luma_ver[0][0] = intpl_luma_block_ver_sse128;
        pf->intpl_luma_ver[0][1] = intpl_luma_block_ver_sse128;
        pf->intpl_luma_ver[0][2] = intpl_luma_block_ver_sse128;
        pf->intpl_luma_ver[1][0] = intpl_luma_block_ver_sse128;
        pf->intpl_luma_ver[1][1] = intpl_luma_block_ver_sse128;
        pf->intpl_luma_ver[1][2] = intpl_luma_block_ver_sse128;

        pf->intpl_luma_ver[0][0] = intpl_luma_block_ver0_sse128;
        pf->intpl_luma_ver[0][1] = intpl_luma_block_ver1_sse128;
        pf->intpl_luma_ver[0][2] = intpl_luma_block_ver2_sse128;
        pf->intpl_luma_ver[1][0] = intpl_luma_block_ver0_sse128;
        pf->intpl_luma_ver[1][1] = intpl_luma_block_ver1_sse128;
        pf->intpl_luma_ver[1][2] = intpl_luma_block_ver2_sse128;
#endif
    }
    
    if (cpuid & DAVS2_CPU_AVX2) {
#if !HIGH_BIT_DEPTH
        pf->intpl_luma_hor[1][0] = intpl_luma_block_hor_avx2;
        pf->intpl_luma_hor[1][1] = intpl_luma_block_hor_avx2;
        pf->intpl_luma_hor[1][2] = intpl_luma_block_hor_avx2;
        pf->intpl_luma_ext[1] = intpl_luma_block_ext_avx2;

        pf->intpl_chroma_ver[1] = intpl_chroma_block_ver_avx2;
        pf->intpl_chroma_hor[1] = intpl_chroma_block_hor_avx2;
        pf->intpl_chroma_ext[1] = intpl_chroma_block_ext_avx2;

        pf->intpl_luma_ver[1][0] = intpl_luma_block_ver_avx2;
        pf->intpl_luma_ver[1][1] = intpl_luma_block_ver_avx2;
        pf->intpl_luma_ver[1][2] = intpl_luma_block_ver_avx2;

        pf->intpl_luma_ver[1][0] = intpl_luma_block_ver0_avx2;
        pf->intpl_luma_ver[1][1] = intpl_luma_block_ver1_avx2;
        pf->intpl_luma_ver[1][2] = intpl_luma_block_ver2_avx2;
#endif
    }
#endif  //if HAVE_MMX
}
