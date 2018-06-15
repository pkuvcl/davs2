/*
 * pixel.cc
 *
 * Description of this file:
 *    Pixel processing functions definition of the davs2 library
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
#include "vec/intrinsic.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * ===========================================================================
 * local & global variables (const tables)
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 * partition map table
 */
const uint8_t g_partition_map_tab[] = {
    //  4      8          12          16          20   24          28   32          36   40   44   48           52   56   60   64
    PART_4x4,  PART_4x8,  255,        PART_4x16,  255, 255,        255, 255,        255, 255, 255, 255,        255, 255, 255, 255,          // 4
    PART_8x4,  PART_8x8,  255,        PART_8x16,  255, 255,        255, PART_8x32,  255, 255, 255, 255,        255, 255, 255, 255,          // 8
    255,       255,       255,        PART_12x16, 255, 255,        255, 255,        255, 255, 255, 255,        255, 255, 255, 255,          // 12
    PART_16x4, PART_16x8, PART_16x12, PART_16x16, 255, 255,        255, PART_16x32, 255, 255, 255, 255,        255, 255, 255, PART_16x64,   // 16
    255,       255,       255,        255,        255, 255,        255, 255,        255, 255, 255, 255,        255, 255, 255, 255,          // 20
    255,       255,       255,        255,        255, 255,        255, PART_24x32, 255, 255, 255, 255,        255, 255, 255, 255,          // 24
    255,       255,       255,        255,        255, 255,        255, 255,        255, 255, 255, 255,        255, 255, 255, 255,          // 28
    255,       PART_32x8, 255,        PART_32x16, 255, PART_32x24, 255, PART_32x32, 255, 255, 255, 255,        255, 255, 255, PART_32x64,   // 32
    255,       255,       255,        255,        255, 255,        255, 255,        255, 255, 255, 255,        255, 255, 255, 255,          // 36
    255,       255,       255,        255,        255, 255,        255, 255,        255, 255, 255, 255,        255, 255, 255, 255,          // 40
    255,       255,       255,        255,        255, 255,        255, 255,        255, 255, 255, 255,        255, 255, 255, 255,          // 44
    255,       255,       255,        255,        255, 255,        255, 255,        255, 255, 255, 255,        255, 255, 255, PART_48x64,   // 48
    255,       255,       255,        255,        255, 255,        255, 255,        255, 255, 255, 255,        255, 255, 255, 255,          // 52
    255,       255,       255,        255,        255, 255,        255, 255,        255, 255, 255, 255,        255, 255, 255, 255,          // 56
    255,       255,       255,        255,        255, 255,        255, 255,        255, 255, 255, 255,        255, 255, 255, 255,          // 60
    255,       255,       255,        PART_64x16, 255, 255,        255, PART_64x32, 255, 255, 255, PART_64x48, 255, 255, 255, PART_64x64    // 64
};


#define PIXEL_ADD_PS_C(w, h) \
static void davs2_pixel_add_ps_##w##x##h(pel_t *a, intptr_t dstride, const pel_t *b0, const coeff_t* b1, intptr_t sstride0, intptr_t sstride1)\
{\
    int x, y;\
    for (y = 0; y < h; y++) {\
        for (x = 0; x < w; x++) {\
            a[x] = (pel_t)DAVS2_CLIP1(b0[x] + b1[x]);\
        }\
        b0 += sstride0;\
        b1 += sstride1;\
        a  += dstride;\
    }\
}

#define BLOCKCOPY_PP_C(w, h) \
static void davs2_blockcopy_pp_##w##x##h(pel_t *a, intptr_t stridea, const pel_t *b, intptr_t strideb)\
{\
    int x, y;\
    for (y = 0; y < h; y++) {\
        for (x = 0; x < w; x++) {\
            a[x] = b[x];\
        }\
        a += stridea;\
        b += strideb;\
    }\
}

#define BLOCKCOPY_SS_C(w, h) \
static void davs2_blockcopy_ss_##w##x##h(coeff_t* a, intptr_t stridea, const coeff_t* b, intptr_t strideb)\
{\
    int x, y;\
    for (y = 0; y < h; y++) {\
        for (x = 0; x < w; x++) {\
            a[x] = b[x];\
        }\
        a += stridea;\
        b += strideb;\
    }\
}

#define BLOCK_OP_C(w, h) \
    PIXEL_ADD_PS_C(w, h); \
    BLOCKCOPY_PP_C(w, h); \
    BLOCKCOPY_SS_C(w, h);

BLOCK_OP_C(64, 64)  /* 64x64 */
BLOCK_OP_C(64, 32)
BLOCK_OP_C(32, 64)
BLOCK_OP_C(64, 16)
BLOCK_OP_C(64, 48)
BLOCK_OP_C(16, 64)
BLOCK_OP_C(48, 64)
BLOCK_OP_C(32, 32)  /* 32x32 */
BLOCK_OP_C(32, 16)
BLOCK_OP_C(16, 32)
BLOCK_OP_C(32,  8)
BLOCK_OP_C(32, 24)
BLOCK_OP_C( 8, 32)
BLOCK_OP_C(24, 32)
BLOCK_OP_C(16, 16)  /* 16x16 */
BLOCK_OP_C(16,  8)
BLOCK_OP_C( 8, 16)
BLOCK_OP_C(16,  4)
BLOCK_OP_C(16, 12)
BLOCK_OP_C( 4, 16)
BLOCK_OP_C(12, 16)
BLOCK_OP_C( 8,  8)  /* 8x8 */
BLOCK_OP_C( 8,  4)
BLOCK_OP_C( 4,  8)
BLOCK_OP_C( 4,  4)  /* 4x4 */

#define DECL_PIXELS(cpu) \
    FUNCDEF_PU(void,        pixel_avg,    cpu, pel_t* dst, intptr_t dstride, const pel_t* src0, intptr_t sstride0, const pel_t* src1, intptr_t sstride1, int);\
    FUNCDEF_PU(void,        pixel_add_ps, cpu, pel_t* a,   intptr_t dstride, const pel_t* b0, const int16_t* b1, intptr_t sstride0, intptr_t sstride1);\
    FUNCDEF_PU(void,        blockcopy_pp, cpu, pel_t *a, intptr_t stridea, const pel_t *b, intptr_t strideb);\
    FUNCDEF_PU(void,        blockcopy_ss, cpu, int16_t* a, intptr_t stridea, const int16_t* b, intptr_t strideb);\
    FUNCDEF_CHROMA_PU(void, addAvg,       cpu, const int16_t*, const int16_t*, pel_t*, intptr_t, intptr_t, intptr_t)

DECL_PIXELS(mmx);
DECL_PIXELS(mmx2);
DECL_PIXELS(sse2);
DECL_PIXELS(sse3);
DECL_PIXELS(sse4);
DECL_PIXELS(ssse3);
DECL_PIXELS(avx);
DECL_PIXELS(xop);
DECL_PIXELS(avx2);

#undef DECL_PIXELS


#define ALL_LUMA_PU(name1, name2, cpu) \
    pixf->name1[PART_64x64] = davs2_ ## name2 ## _64x64 ## cpu;  /* 64x64 */ \
    pixf->name1[PART_64x32] = davs2_ ## name2 ## _64x32 ## cpu;\
    pixf->name1[PART_32x64] = davs2_ ## name2 ## _32x64 ## cpu;\
    pixf->name1[PART_64x16] = davs2_ ## name2 ## _64x16 ## cpu;\
    pixf->name1[PART_64x48] = davs2_ ## name2 ## _64x48 ## cpu;\
    pixf->name1[PART_16x64] = davs2_ ## name2 ## _16x64 ## cpu;\
    pixf->name1[PART_48x64] = davs2_ ## name2 ## _48x64 ## cpu;\
    pixf->name1[PART_32x32] = davs2_ ## name2 ## _32x32 ## cpu;  /* 32x32 */ \
    pixf->name1[PART_32x16] = davs2_ ## name2 ## _32x16 ## cpu;\
    pixf->name1[PART_16x32] = davs2_ ## name2 ## _16x32 ## cpu;\
    pixf->name1[PART_32x8 ] = davs2_ ## name2 ## _32x8  ## cpu;\
    pixf->name1[PART_32x24] = davs2_ ## name2 ## _32x24 ## cpu;\
    pixf->name1[PART_8x32 ] = davs2_ ## name2 ## _8x32  ## cpu;\
    pixf->name1[PART_24x32] = davs2_ ## name2 ## _24x32 ## cpu;\
    pixf->name1[PART_16x16] = davs2_ ## name2 ## _16x16 ## cpu;  /* 16x16 */ \
    pixf->name1[PART_16x8 ] = davs2_ ## name2 ## _16x8  ## cpu;\
    pixf->name1[PART_8x16 ] = davs2_ ## name2 ## _8x16  ## cpu;\
    pixf->name1[PART_16x4 ] = davs2_ ## name2 ## _16x4  ## cpu;\
    pixf->name1[PART_16x12] = davs2_ ## name2 ## _16x12 ## cpu;\
    pixf->name1[PART_4x16 ] = davs2_ ## name2 ## _4x16  ## cpu;\
    pixf->name1[PART_12x16] = davs2_ ## name2 ## _12x16 ## cpu;\
    pixf->name1[PART_8x8  ] = davs2_ ## name2 ## _8x8   ## cpu;  /* 8x8 */ \
    pixf->name1[PART_8x4  ] = davs2_ ## name2 ## _8x4   ## cpu;\
    pixf->name1[PART_4x8  ] = davs2_ ## name2 ## _4x8   ## cpu;\
    pixf->name1[PART_4x4  ] = davs2_ ## name2 ## _4x4   ## cpu  /* 4x4 */

void davs2_pixel_init(uint32_t cpuid, ao_funcs_t* pixf)
{
    ALL_LUMA_PU(add_ps, pixel_add_ps,  );
    ALL_LUMA_PU(copy_pp, blockcopy_pp, );
    ALL_LUMA_PU(copy_ss, blockcopy_ss, );

#if HAVE_MMX
    if (cpuid & DAVS2_CPU_SSE2) {
#if HIGH_BIT_DEPTH
        //10bit assemble
        if (sizeof(pel_t) == sizeof(int16_t) && cpuid) {
            pixf->copy_pp[PART_64x64] = (copy_pp_t)davs2_blockcopy_ss_64x64_sse2;  /* 64x64 */
            pixf->copy_pp[PART_64x32] = (copy_pp_t)davs2_blockcopy_ss_64x32_sse2;
            pixf->copy_pp[PART_32x64] = (copy_pp_t)davs2_blockcopy_ss_32x64_sse2;
            pixf->copy_pp[PART_64x16] = (copy_pp_t)davs2_blockcopy_ss_64x16_sse2;
            pixf->copy_pp[PART_64x48] = (copy_pp_t)davs2_blockcopy_ss_64x48_sse2;
            pixf->copy_pp[PART_16x64] = (copy_pp_t)davs2_blockcopy_ss_16x64_sse2;
            pixf->copy_pp[PART_48x64] = (copy_pp_t)davs2_blockcopy_ss_48x64_sse2;
            pixf->copy_pp[PART_32x32] = (copy_pp_t)davs2_blockcopy_ss_32x32_sse2; /* 32x32 */
            pixf->copy_pp[PART_32x16] = (copy_pp_t)davs2_blockcopy_ss_32x16_sse2;
            pixf->copy_pp[PART_16x32] = (copy_pp_t)davs2_blockcopy_ss_16x32_sse2;
            pixf->copy_pp[PART_32x8 ] = (copy_pp_t)davs2_blockcopy_ss_32x8_sse2;
            pixf->copy_pp[PART_32x24] = (copy_pp_t)davs2_blockcopy_ss_32x24_sse2;
            pixf->copy_pp[PART_8x32 ] = (copy_pp_t)davs2_blockcopy_ss_8x32_sse2;
            pixf->copy_pp[PART_24x32] = (copy_pp_t)davs2_blockcopy_ss_24x32_sse2;
            pixf->copy_pp[PART_16x16] = (copy_pp_t)davs2_blockcopy_ss_16x16_sse2; /* 16x16 */
            pixf->copy_pp[PART_16x8 ] = (copy_pp_t)davs2_blockcopy_ss_16x8_sse2;
            pixf->copy_pp[PART_8x16 ] = (copy_pp_t)davs2_blockcopy_ss_8x16_sse2;
            pixf->copy_pp[PART_16x4 ] = (copy_pp_t)davs2_blockcopy_ss_16x4_sse2;
            pixf->copy_pp[PART_16x12] = (copy_pp_t)davs2_blockcopy_ss_16x12_sse2;
            pixf->copy_pp[PART_4x16 ] = (copy_pp_t)davs2_blockcopy_ss_4x16_sse2;
            pixf->copy_pp[PART_12x16] = (copy_pp_t)davs2_blockcopy_ss_12x16_sse2;
            pixf->copy_pp[PART_8x8  ] = (copy_pp_t)davs2_blockcopy_ss_8x8_sse2; /* 8x8 */
            pixf->copy_pp[PART_8x4  ] = (copy_pp_t)davs2_blockcopy_ss_8x4_sse2;
            pixf->copy_pp[PART_4x8  ] = (copy_pp_t)davs2_blockcopy_ss_4x8_sse2;
            pixf->copy_pp[PART_4x4  ] = (copy_pp_t)davs2_blockcopy_ss_4x4_sse2;  /* 4x4 */
        }
        if (sizeof(coeff_t) == sizeof(int16_t) && cpuid) {
            pixf->copy_ss[PART_64x64] = (copy_ss_t)davs2_blockcopy_ss_64x64_sse2;  /* 64x64 */
            pixf->copy_ss[PART_64x32] = (copy_ss_t)davs2_blockcopy_ss_64x32_sse2;
            pixf->copy_ss[PART_32x64] = (copy_ss_t)davs2_blockcopy_ss_32x64_sse2;
            pixf->copy_ss[PART_64x16] = (copy_ss_t)davs2_blockcopy_ss_64x16_sse2;
            pixf->copy_ss[PART_64x48] = (copy_ss_t)davs2_blockcopy_ss_64x48_sse2;
            pixf->copy_ss[PART_16x64] = (copy_ss_t)davs2_blockcopy_ss_16x64_sse2;
            pixf->copy_ss[PART_48x64] = (copy_ss_t)davs2_blockcopy_ss_48x64_sse2;
            pixf->copy_ss[PART_32x32] = (copy_ss_t)davs2_blockcopy_ss_32x32_sse2; /* 32x32 */
            pixf->copy_ss[PART_32x16] = (copy_ss_t)davs2_blockcopy_ss_32x16_sse2;
            pixf->copy_ss[PART_16x32] = (copy_ss_t)davs2_blockcopy_ss_16x32_sse2;
            pixf->copy_ss[PART_32x8 ] = (copy_ss_t)davs2_blockcopy_ss_32x8_sse2;
            pixf->copy_ss[PART_32x24] = (copy_ss_t)davs2_blockcopy_ss_32x24_sse2;
            pixf->copy_ss[PART_8x32 ] = (copy_ss_t)davs2_blockcopy_ss_8x32_sse2;
            pixf->copy_ss[PART_24x32] = (copy_ss_t)davs2_blockcopy_ss_24x32_sse2;
            pixf->copy_ss[PART_16x16] = (copy_ss_t)davs2_blockcopy_ss_16x16_sse2; /* 16x16 */
            pixf->copy_ss[PART_16x8 ] = (copy_ss_t)davs2_blockcopy_ss_16x8_sse2;
            pixf->copy_ss[PART_8x16 ] = (copy_ss_t)davs2_blockcopy_ss_8x16_sse2;
            pixf->copy_ss[PART_16x4 ] = (copy_ss_t)davs2_blockcopy_ss_16x4_sse2;
            pixf->copy_ss[PART_16x12] = (copy_ss_t)davs2_blockcopy_ss_16x12_sse2;
            pixf->copy_ss[PART_4x16 ] = (copy_ss_t)davs2_blockcopy_ss_4x16_sse2;
            pixf->copy_ss[PART_12x16] = (copy_ss_t)davs2_blockcopy_ss_12x16_sse2;
            pixf->copy_ss[PART_8x8  ] = (copy_ss_t)davs2_blockcopy_ss_8x8_sse2; /* 8x8 */
            pixf->copy_ss[PART_8x4  ] = (copy_ss_t)davs2_blockcopy_ss_8x4_sse2;
            pixf->copy_ss[PART_4x8  ] = (copy_ss_t)davs2_blockcopy_ss_4x8_sse2;
            pixf->copy_ss[PART_4x4  ] = (copy_ss_t)davs2_blockcopy_ss_4x4_sse2;  /* 4x4 */
        }
#else
        ALL_LUMA_PU(copy_pp, blockcopy_pp, _sse2);
        ALL_LUMA_PU(copy_ss, blockcopy_ss, _sse2);
#endif
    }

    if (cpuid & DAVS2_CPU_SSE4) {
#if HIGH_BIT_DEPTH
        //10bit assemble
#else
        pixf->add_ps[PART_4x4  ] = davs2_pixel_add_ps_4x4_sse4;
        pixf->add_ps[PART_4x8  ] = davs2_pixel_add_ps_4x8_sse4;
        pixf->add_ps[PART_4x16 ] = davs2_pixel_add_ps_4x16_sse4;
        pixf->add_ps[PART_8x8  ] = davs2_pixel_add_ps_8x8_sse4;
        pixf->add_ps[PART_8x16 ] = davs2_pixel_add_ps_8x16_sse4;
        pixf->add_ps[PART_8x32 ] = davs2_pixel_add_ps_8x32_sse4;
        pixf->add_ps[PART_16x4 ] = davs2_pixel_add_ps_16x4_sse4;
        pixf->add_ps[PART_16x8 ] = davs2_pixel_add_ps_16x8_sse4;
        pixf->add_ps[PART_16x12] = davs2_pixel_add_ps_16x12_sse4;
        pixf->add_ps[PART_16x16] = davs2_pixel_add_ps_16x16_sse4;
        pixf->add_ps[PART_16x64] = davs2_pixel_add_ps_16x64_sse4;
        pixf->add_ps[PART_32x8 ] = davs2_pixel_add_ps_32x8_sse4;
        pixf->add_ps[PART_32x16] = davs2_pixel_add_ps_32x16_sse4;
        pixf->add_ps[PART_32x24] = davs2_pixel_add_ps_32x24_sse4;
        pixf->add_ps[PART_32x32] = davs2_pixel_add_ps_32x32_sse4;
        pixf->add_ps[PART_32x64] = davs2_pixel_add_ps_32x64_sse4;
        pixf->add_ps[PART_64x16] = davs2_pixel_add_ps_64x16_sse4;
        pixf->add_ps[PART_64x32] = davs2_pixel_add_ps_64x32_sse4;
        pixf->add_ps[PART_64x48] = davs2_pixel_add_ps_64x48_sse4;
        pixf->add_ps[PART_64x64] = davs2_pixel_add_ps_64x64_sse4;
#endif
    }
    
    if (cpuid & DAVS2_CPU_AVX) {
#if HIGH_BIT_DEPTH
        //10bit assemble
        if (sizeof(pel_t) == sizeof(int16_t) && cpuid) {
            pixf->copy_pp[PART_64x64] = (copy_pp_t)davs2_blockcopy_ss_64x64_avx;
            pixf->copy_pp[PART_64x32] = (copy_pp_t)davs2_blockcopy_ss_64x32_avx;
            pixf->copy_pp[PART_32x64] = (copy_pp_t)davs2_blockcopy_ss_32x64_avx;
            pixf->copy_pp[PART_64x16] = (copy_pp_t)davs2_blockcopy_ss_64x16_avx;
            pixf->copy_pp[PART_64x48] = (copy_pp_t)davs2_blockcopy_ss_64x48_avx;
            pixf->copy_pp[PART_16x64] = (copy_pp_t)davs2_blockcopy_ss_16x64_avx;
            pixf->copy_pp[PART_48x64] = (copy_pp_t)davs2_blockcopy_ss_48x64_avx;
            pixf->copy_pp[PART_32x32] = (copy_pp_t)davs2_blockcopy_ss_32x32_avx;
            pixf->copy_pp[PART_32x16] = (copy_pp_t)davs2_blockcopy_ss_32x16_avx;
            pixf->copy_pp[PART_16x32] = (copy_pp_t)davs2_blockcopy_ss_16x32_avx;
            pixf->copy_pp[PART_32x8 ] = (copy_pp_t)davs2_blockcopy_ss_32x8_avx;
            pixf->copy_pp[PART_32x24] = (copy_pp_t)davs2_blockcopy_ss_32x24_avx;
            pixf->copy_pp[PART_24x32] = (copy_pp_t)davs2_blockcopy_ss_24x32_avx;
            pixf->copy_pp[PART_16x16] = (copy_pp_t)davs2_blockcopy_ss_16x16_avx;
            pixf->copy_pp[PART_16x8 ] = (copy_pp_t)davs2_blockcopy_ss_16x8_avx;
            pixf->copy_pp[PART_16x4 ] = (copy_pp_t)davs2_blockcopy_ss_16x4_avx;
            pixf->copy_pp[PART_16x12] = (copy_pp_t)davs2_blockcopy_ss_16x12_avx;
        }
        if (sizeof(coeff_t) == sizeof(int16_t) && cpuid) {
            pixf->copy_ss[PART_64x64] = (copy_ss_t)davs2_blockcopy_ss_64x64_avx;
            pixf->copy_ss[PART_64x32] = (copy_ss_t)davs2_blockcopy_ss_64x32_avx;
            pixf->copy_ss[PART_32x64] = (copy_ss_t)davs2_blockcopy_ss_32x64_avx;
            pixf->copy_ss[PART_64x16] = (copy_ss_t)davs2_blockcopy_ss_64x16_avx;
            pixf->copy_ss[PART_64x48] = (copy_ss_t)davs2_blockcopy_ss_64x48_avx;
            pixf->copy_ss[PART_16x64] = (copy_ss_t)davs2_blockcopy_ss_16x64_avx;
            pixf->copy_ss[PART_48x64] = (copy_ss_t)davs2_blockcopy_ss_48x64_avx;
            pixf->copy_ss[PART_32x32] = (copy_ss_t)davs2_blockcopy_ss_32x32_avx;
            pixf->copy_ss[PART_32x16] = (copy_ss_t)davs2_blockcopy_ss_32x16_avx;
            pixf->copy_ss[PART_16x32] = (copy_ss_t)davs2_blockcopy_ss_16x32_avx;
            pixf->copy_ss[PART_32x8 ] = (copy_ss_t)davs2_blockcopy_ss_32x8_avx;
            pixf->copy_ss[PART_32x24] = (copy_ss_t)davs2_blockcopy_ss_32x24_avx;
            pixf->copy_ss[PART_24x32] = (copy_ss_t)davs2_blockcopy_ss_24x32_avx;
            pixf->copy_ss[PART_16x16] = (copy_ss_t)davs2_blockcopy_ss_16x16_avx;
            pixf->copy_ss[PART_16x8 ] = (copy_ss_t)davs2_blockcopy_ss_16x8_avx;
            pixf->copy_ss[PART_16x4 ] = (copy_ss_t)davs2_blockcopy_ss_16x4_avx;
            pixf->copy_ss[PART_16x12] = (copy_ss_t)davs2_blockcopy_ss_16x12_avx;
        }
#else
        pixf->copy_pp[PART_64x64] = davs2_blockcopy_pp_64x64_avx;
        pixf->copy_pp[PART_64x32] = davs2_blockcopy_pp_64x32_avx;
        pixf->copy_pp[PART_32x64] = davs2_blockcopy_pp_32x64_avx;
        pixf->copy_pp[PART_64x16] = davs2_blockcopy_pp_64x16_avx;
        pixf->copy_pp[PART_64x48] = davs2_blockcopy_pp_64x48_avx;
        pixf->copy_pp[PART_48x64] = davs2_blockcopy_pp_48x64_avx;
        pixf->copy_pp[PART_32x32] = davs2_blockcopy_pp_32x32_avx;
        pixf->copy_pp[PART_32x16] = davs2_blockcopy_pp_32x16_avx;
        pixf->copy_pp[PART_32x8 ] = davs2_blockcopy_pp_32x8_avx;
        pixf->copy_pp[PART_32x24] = davs2_blockcopy_pp_32x24_avx;
        
        pixf->copy_ss[PART_64x64] = davs2_blockcopy_ss_64x64_avx;
        pixf->copy_ss[PART_64x32] = davs2_blockcopy_ss_64x32_avx;
        pixf->copy_ss[PART_32x64] = davs2_blockcopy_ss_32x64_avx;
        pixf->copy_ss[PART_64x16] = davs2_blockcopy_ss_64x16_avx;
        pixf->copy_ss[PART_64x48] = davs2_blockcopy_ss_64x48_avx;
        pixf->copy_ss[PART_16x64] = davs2_blockcopy_ss_16x64_avx;
        pixf->copy_ss[PART_48x64] = davs2_blockcopy_ss_48x64_avx;
        pixf->copy_ss[PART_32x32] = davs2_blockcopy_ss_32x32_avx;
        pixf->copy_ss[PART_32x16] = davs2_blockcopy_ss_32x16_avx;
        pixf->copy_ss[PART_16x32] = davs2_blockcopy_ss_16x32_avx;
        pixf->copy_ss[PART_32x8 ] = davs2_blockcopy_ss_32x8_avx;
        pixf->copy_ss[PART_32x24] = davs2_blockcopy_ss_32x24_avx;
        pixf->copy_ss[PART_24x32] = davs2_blockcopy_ss_24x32_avx;
        pixf->copy_ss[PART_16x16] = davs2_blockcopy_ss_16x16_avx;
        pixf->copy_ss[PART_16x8 ] = davs2_blockcopy_ss_16x8_avx;
        pixf->copy_ss[PART_16x4 ] = davs2_blockcopy_ss_16x4_avx;
        pixf->copy_ss[PART_16x12] = davs2_blockcopy_ss_16x12_avx;
#endif
    }

    if (cpuid & DAVS2_CPU_AVX2) {
#if HIGH_BIT_DEPTH
        //10bit assemble
#else
        pixf->add_ps[PART_16x4 ] = davs2_pixel_add_ps_16x4_avx2;
        pixf->add_ps[PART_16x8 ] = davs2_pixel_add_ps_16x8_avx2;
        pixf->add_ps[PART_16x12] = davs2_pixel_add_ps_16x12_avx2;
        pixf->add_ps[PART_16x16] = davs2_pixel_add_ps_16x16_avx2;
        pixf->add_ps[PART_16x64] = davs2_pixel_add_ps_16x64_avx2;
#if ARCH_X86_64
        pixf->add_ps[PART_32x8 ] = davs2_pixel_add_ps_32x8_avx2;
        pixf->add_ps[PART_32x16] = davs2_pixel_add_ps_32x16_avx2;
        pixf->add_ps[PART_32x24] = davs2_pixel_add_ps_32x24_avx2;
        pixf->add_ps[PART_32x32] = davs2_pixel_add_ps_32x32_avx2;
        pixf->add_ps[PART_32x64] = davs2_pixel_add_ps_32x64_avx2;
#endif
        pixf->add_ps[PART_64x16] = davs2_pixel_add_ps_64x16_avx2;
        pixf->add_ps[PART_64x32] = davs2_pixel_add_ps_64x32_avx2;
        pixf->add_ps[PART_64x48] = davs2_pixel_add_ps_64x48_avx2;
        pixf->add_ps[PART_64x64] = davs2_pixel_add_ps_64x64_avx2;
#endif
    }
#endif  // HAVE_MMX
}

#ifdef __cplusplus
}
#endif
