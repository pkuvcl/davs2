/*
 * intrinsic.h
 *
 * Description of this file:
 *    SIMD assembly functions definition of the davs2 library
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

#ifndef DAVS2_INTRINSIC_H
#define DAVS2_INTRINSIC_H

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(_MSC_VER) && !defined(__INTEL_COMPILER)
#define __int64     long long
#endif

/* ---------------------------------------------------------------------------
 * global variables
 */
#define intrinsic_mask FPFX(intrinsic_mask)
ALIGN32(extern const int8_t  intrinsic_mask[15][16]);
#define intrinsic_mask_256_8bit FPFX(intrinsic_mask_256_8bit)
ALIGN32(extern const int8_t  intrinsic_mask_256_8bit[16][32]);
#define intrinsic_mask32 FPFX(intrinsic_mask32)
ALIGN32(extern const int8_t  intrinsic_mask32[32][32]);
#define intrinsic_mask_10bit FPFX(intrinsic_mask_10bit)
ALIGN32(extern const int16_t intrinsic_mask_10bit[15][16]);
#define tab_log2 FPFX(tab_log2)
ALIGN32(extern const int8_t tab_log2[65]);
#define tab_coeff_mode_7 FPFX(tab_coeff_mode_7)
ALIGN16(extern const pel_t tab_coeff_mode_7[64][16]);
#define tab_idx_mode_7 FPFX(tab_idx_mode_7)
ALIGN32(extern const uint8_t tab_idx_mode_7[64]);
#define tab_coeff_mode_7_avx FPFX(tab_coeff_mode_7_avx)
ALIGN32(extern const pel_t tab_coeff_mode_7_avx[64][32]);

#if HIGH_BIT_DEPTH
#define tab_coeff_mode_9 FPFX(tab_coeff_mode_9)
ALIGN16(extern const int16_t tab_coeff_mode_9[64][16]);
#else
#define tab_coeff_mode_9 FPFX(tab_coeff_mode_9)
ALIGN16(extern const int8_t tab_coeff_mode_9[64][16]);
#endif

#define tab_idx_mode_9 FPFX(tab_idx_mode_9)
extern const uint8_t tab_idx_mode_9[64];
#if HIGH_BIT_DEPTH
#define tab_coeff_mode_11 FPFX(tab_coeff_mode_11)
ALIGN16(extern const int16_t tab_coeff_mode_11[64][16]);
#else
#define tab_coeff_mode_11 FPFX(tab_coeff_mode_11)
ALIGN16(extern const int8_t tab_coeff_mode_11[64][16]);
#endif

/* ---------------------------------------------------------------------------
 * macros used for quick access of __m128i
 */
#define M128_U64(mx, idx)  *(((uint64_t *)&mx) + idx)
#define M128_U32(mx, idx)  *(((uint32_t *)&mx) + idx)
#define M128_I32(mx, idx)  *((( int32_t *)&mx) + idx)
#define M128_U16(mx, idx)  *(((uint16_t *)&mx) + idx)
#define M128_I16(mx, idx)  *((( int16_t *)&mx) + idx)


#if _MSC_VER
//添加宏定义  解决当前immintrin.h中没有定义这些函数的问题      zhangjiaqi 2016-12-02
#define _mm256_extract_epi64(a, i) (a.m256i_i64[i])
#define _mm256_extract_epi32(a, i) (a.m256i_i32[i])
#define _mm256_extract_epi16(a, i) (a.m256i_i16[i])
#define _mm256_extract_epi8(a, i)  (a.m256i_i8 [i])
#define _mm256_insert_epi64(a, v, i) (a.m256i_i64[i] = v)
#define _mm_extract_epi64(r, i) r.m128i_i64[i]
#else
// 添加部分gcc中缺少的avx函数定义
#define _mm256_set_m128i(/* __m128i */ hi, /* __m128i */ lo) \
            _mm256_insertf128_si256(_mm256_castsi128_si256(lo), (hi), 0x1)
#define _mm256_loadu2_m128i(/* __m128i const* */ hiaddr, \
                            /* __m128i const* */ loaddr) \
            _mm256_set_m128i(_mm_loadu_si128(hiaddr), _mm_loadu_si128(loaddr))
#define _mm256_storeu2_m128i(/* __m128i* */ hiaddr, /* __m128i* */ loaddr, \
                             /* __m256i */ a) \
    do { \
        __m256i _a = (a); /* reference a only once in macro body */ \
        _mm_storeu_si128((loaddr), _mm256_castsi256_si128(_a)); \
        _mm_storeu_si128((hiaddr), _mm256_extractf128_si256(_a, 0x1)); \
    } while (0)
#endif

#define davs2_memzero_aligned_c_sse2 FPFX(memzero_aligned_c_sse2)
void *davs2_memzero_aligned_c_sse2(void *dst, size_t n);
#define davs2_memzero_aligned_c_avx FPFX(memzero_aligned_c_avx)
void *davs2_memzero_aligned_c_avx (void *dst, size_t n);
#define davs2_memcpy_aligned_c_sse2 FPFX(memcpy_aligned_c_sse2)
void *davs2_memcpy_aligned_c_sse2 (void *dst, const void *src, size_t n);

#define davs2_memcpy_aligned_mmx FPFX(memcpy_aligned_mmx)
void *davs2_memcpy_aligned_mmx(void *dst, const void *src, size_t n);
#define davs2_memcpy_aligned_sse FPFX(memcpy_aligned_sse)
void *davs2_memcpy_aligned_sse(void *dst, const void *src, size_t n);

#define davs2_fast_memcpy_mmx FPFX(fast_memcpy_mmx)
void *davs2_fast_memcpy_mmx(void *dst, const void *src, size_t n);
#define davs2_fast_memset_mmx FPFX(fast_memset_mmx)
void *davs2_fast_memset_mmx(void *dst, int val, size_t n);

#define davs2_memzero_aligned_mmx FPFX(memzero_aligned_mmx)
void *davs2_memzero_aligned_mmx  (void *dst, size_t n);
#define davs2_memzero_aligned_sse FPFX(memzero_aligned_sse)
void *davs2_memzero_aligned_sse  (void *dst, size_t n);
#define davs2_memzero_aligned_avx FPFX(memzero_aligned_avx)
void *davs2_memzero_aligned_avx  (void *dst, size_t n);

#define davs2_fast_memzero_mmx FPFX(fast_memzero_mmx)
void *davs2_fast_memzero_mmx     (void *dst, size_t n);

#define plane_copy_c_sse2 FPFX(plane_copy_c_sse2)
void plane_copy_c_sse2          (pel_t *dst, intptr_t i_dst, pel_t *src, intptr_t i_src, int w, int h);

#define intpl_copy_block_sse128 FPFX(intpl_copy_block_sse128)
void intpl_copy_block_sse128    (pel_t *dst, int i_dst, pel_t *src, int i_src, int width, int height);

#define intpl_luma_block_hor_sse128 FPFX(intpl_luma_block_hor_sse128)
void intpl_luma_block_hor_sse128(pel_t *dst, int i_dst, pel_t *src, int i_src, int width, int height, const int8_t *coeff);
#define intpl_luma_block_ver_sse128 FPFX(intpl_luma_block_ver_sse128)
void intpl_luma_block_ver_sse128(pel_t *dst, int i_dst, pel_t *src, int i_src, int width, int height, const int8_t *coeff);
#define intpl_luma_block_ver0_sse128 FPFX(intpl_luma_block_ver0_sse128)
void intpl_luma_block_ver0_sse128(pel_t *dst, int i_dst, pel_t *src, int i_src, int width, int height, const int8_t *coeff);
#define intpl_luma_block_ver1_sse128 FPFX(intpl_luma_block_ver1_sse128)
void intpl_luma_block_ver1_sse128(pel_t *dst, int i_dst, pel_t *src, int i_src, int width, int height, const int8_t *coeff);
#define intpl_luma_block_ver2_sse128 FPFX(intpl_luma_block_ver2_sse128)
void intpl_luma_block_ver2_sse128(pel_t *dst, int i_dst, pel_t *src, int i_src, int width, int height, const int8_t *coeff);
#define intpl_luma_block_ext_sse128 FPFX(intpl_luma_block_ext_sse128)
void intpl_luma_block_ext_sse128(pel_t *dst, int i_dst, pel_t *src, int i_src, int width, int height, const int8_t *coeff_h, const int8_t *coeff_v);

#define intpl_chroma_block_hor_sse128 FPFX(intpl_chroma_block_hor_sse128)
void intpl_chroma_block_hor_sse128(pel_t *dst, int i_dst, pel_t *src, int i_src, int width, int height, const int8_t *coeff);
#define intpl_chroma_block_ver_sse128 FPFX(intpl_chroma_block_ver_sse128)
void intpl_chroma_block_ver_sse128(pel_t *dst, int i_dst, pel_t *src, int i_src, int width, int height, const int8_t *coeff);
#define intpl_chroma_block_ext_sse128 FPFX(intpl_chroma_block_ext_sse128)
void intpl_chroma_block_ext_sse128(pel_t *dst, int i_dst, pel_t *src, int i_src, int width, int height, const int8_t *coeff_h, const int8_t *coeff_v);

#define intpl_luma_block_hor_avx2 FPFX(intpl_luma_block_hor_avx2)
void intpl_luma_block_hor_avx2(pel_t *dst, int i_dst, pel_t *src, int i_src, int width, int height, const int8_t *coeff);
#define intpl_luma_block_ver_avx2 FPFX(intpl_luma_block_ver_avx2)
void intpl_luma_block_ver_avx2(pel_t *dst, int i_dst, pel_t *src, int i_src, int width, int height, const int8_t *coeff);
#define intpl_luma_block_ver0_avx2 FPFX(intpl_luma_block_ver0_avx2)
void intpl_luma_block_ver0_avx2(pel_t *dst, int i_dst, pel_t *src, int i_src, int width, int height, const int8_t *coeff);
#define intpl_luma_block_ver1_avx2 FPFX(intpl_luma_block_ver1_avx2)
void intpl_luma_block_ver1_avx2(pel_t *dst, int i_dst, pel_t *src, int i_src, int width, int height, const int8_t *coeff);
#define intpl_luma_block_ver2_avx2 FPFX(intpl_luma_block_ver2_avx2)
void intpl_luma_block_ver2_avx2(pel_t *dst, int i_dst, pel_t *src, int i_src, int width, int height, const int8_t *coeff);
#define intpl_luma_block_ext_avx2 FPFX(intpl_luma_block_ext_avx2)
void intpl_luma_block_ext_avx2(pel_t *dst, int i_dst, pel_t *src, int i_src, int width, int height, const int8_t *coeff_h, const int8_t *coeff_v);


/* ---------------------------------------------------------------------------
 */
#define intpl_luma_hor_sse128 FPFX(intpl_luma_hor_sse128)
void intpl_luma_hor_sse128(pel_t *dst, int i_dst, mct_t *tmp, int i_tmp, pel_t *src, int i_src, int width, int height, int8_t const *coeff);
#define intpl_luma_hor_x3_sse128 FPFX(intpl_luma_hor_x3_sse128)
void intpl_luma_hor_x3_sse128(pel_t *const dst[3], int i_dst, mct_t *const tmp[3], int i_tmp, pel_t *src, int i_src, int width, int height, const int8_t **coeff);
#define intpl_luma_ver_x3_sse128 FPFX(intpl_luma_ver_x3_sse128)
void intpl_luma_ver_x3_sse128(pel_t *const dst[3], int i_dst, pel_t *src, int i_src, int width, int height, int8_t const **coeff);
#define intpl_luma_ext_x3_sse128 FPFX(intpl_luma_ext_x3_sse128)
void intpl_luma_ext_x3_sse128(pel_t *const dst[3], int i_dst, mct_t *tmp, int i_tmp, int width, int height, const int8_t **coeff);
#define intpl_luma_ext_sse128 FPFX(intpl_luma_ext_sse128)
void intpl_luma_ext_sse128(pel_t *dst, int i_dst, mct_t *tmp, int i_tmp, int width, int height, const int8_t *coeff);

#define avs_pixel_average_sse128 FPFX(avs_pixel_average_sse128)
void avs_pixel_average_sse128 (pel_t *dst, int i_dst, const pel_t *src0, int i_src0, const pel_t *src1, int i_src1, int width, int height);
#define davs2_pixel_average_avx FPFX(pixel_average_avx)
void davs2_pixel_average_avx  (pel_t *dst, int i_dst, const pel_t *src1, int i_src1, const pel_t *src2, int i_src2, int width, int height);
#define padding_rows_sse128 FPFX(padding_rows_sse128)
void padding_rows_sse128      (pel_t *src, int i_src, int width, int height, int start, int rows, int pad);
#define padding_rows_lr_sse128 FPFX(padding_rows_lr_sse128)
void padding_rows_lr_sse128   (pel_t *src, int i_src, int width, int height, int start, int rows, int pad);

#define intpl_chroma_block_hor_avx2 FPFX(intpl_chroma_block_hor_avx2)
void intpl_chroma_block_hor_avx2(pel_t *dst, int i_dst, pel_t *src, int i_src, int width, int height, const int8_t *coeff);
#define intpl_chroma_block_ver_avx2 FPFX(intpl_chroma_block_ver_avx2)
void intpl_chroma_block_ver_avx2(pel_t *dst, int i_dst, pel_t *src, int i_src, int width, int height, const int8_t *coeff);
#define intpl_chroma_block_ext_avx2 FPFX(intpl_chroma_block_ext_avx2)
void intpl_chroma_block_ext_avx2(pel_t *dst, int i_dst, pel_t *src, int i_src, int width, int height, const int8_t *coeff_h, const int8_t *coeff_v);
#define deblock_edge_ver_sse128 FPFX(deblock_edge_ver_sse128)
void deblock_edge_ver_sse128  (pel_t *SrcPtr, int stride, int Alpha, int Beta, uint8_t *flt_flag);
#define deblock_edge_hor_sse128 FPFX(deblock_edge_hor_sse128)
void deblock_edge_hor_sse128  (pel_t *SrcPtr, int stride, int Alpha, int Beta, uint8_t *flt_flag);
#if HDR_CHROMA_DELTA_QP
#define deblock_edge_ver_c_sse128 FPFX(deblock_edge_ver_c_sse128)
void deblock_edge_ver_c_sse128(pel_t *SrcPtrU, pel_t *SrcPtrV, int stride, int *Alpha, int *Beta, uint8_t *flt_flag);
#define deblock_edge_hor_c_sse128 FPFX(deblock_edge_hor_c_sse128)
void deblock_edge_hor_c_sse128(pel_t *SrcPtrU, pel_t *SrcPtrV, int stride, int *Alpha, int *Beta, uint8_t *flt_flag);
#else
#define deblock_edge_ver_c_sse128 FPFX(deblock_edge_ver_c_sse128)
void deblock_edge_ver_c_sse128(pel_t *SrcPtrU, pel_t *SrcPtrV, int stride, int Alpha, int Beta, uint8_t *flt_flag);
#define deblock_edge_hor_c_sse128 FPFX(deblock_edge_hor_c_sse128)
void deblock_edge_hor_c_sse128(pel_t *SrcPtrU, pel_t *SrcPtrV, int stride, int Alpha, int Beta, uint8_t *flt_flag);
#endif
//--------avx2--------
#define deblock_edge_hor_avx2 FPFX(deblock_edge_hor_avx2)
void deblock_edge_hor_avx2(pel_t *SrcPtr, int stride, int Alpha, int Beta, uint8_t *flt_flag);
#define deblock_edge_ver_avx2 FPFX(deblock_edge_ver_avx2)
void deblock_edge_ver_avx2(pel_t *SrcPtr, int stride, int Alpha, int Beta, uint8_t *flt_flag);
#define deblock_edge_hor_c_avx2 FPFX(deblock_edge_hor_c_avx2)
void deblock_edge_hor_c_avx2(pel_t *SrcPtrU, pel_t *SrcPtrV, int stride, int Alpha, int Beta, uint8_t *flt_flag);
#define deblock_edge_ver_c_avx2 FPFX(deblock_edge_ver_c_avx2)
void deblock_edge_ver_c_avx2(pel_t *SrcPtrU, pel_t *SrcPtrV, int stride, int Alpha, int Beta, uint8_t *flt_flag);


#define davs2_dequant_sse4 FPFX(dequant_sse4)
void davs2_dequant_sse4(coeff_t *coef, const int i_coef, const int scale, const int shift);

#define idct_4x4_sse128 FPFX(idct_4x4_sse128)
void idct_4x4_sse128  (const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_8x8_sse128 FPFX(idct_8x8_sse128)
void idct_8x8_sse128  (const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_16x16_sse128 FPFX(idct_16x16_sse128)
void idct_16x16_sse128(const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_32x32_sse128 FPFX(idct_32x32_sse128)
void idct_32x32_sse128(const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_64x64_sse128 FPFX(idct_64x64_sse128)
void idct_64x64_sse128(const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_16x4_sse128 FPFX(idct_16x4_sse128)
void idct_16x4_sse128 (const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_32x8_sse128 FPFX(idct_32x8_sse128)
void idct_32x8_sse128 (const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_64x16_sse128 FPFX(idct_64x16_sse128)
void idct_64x16_sse128(const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_4x16_sse128 FPFX(idct_4x16_sse128)
void idct_4x16_sse128 (const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_8x32_sse128 FPFX(idct_8x32_sse128)
void idct_8x32_sse128 (const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_16x64_sse128 FPFX(idct_16x64_sse128)
void idct_16x64_sse128(const coeff_t *src, coeff_t *dst, int i_dst);
#define inv_transform_4x4_2nd_sse128 FPFX(inv_transform_4x4_2nd_sse128)
void inv_transform_4x4_2nd_sse128(coeff_t *coeff, int i_coeff);
#define inv_transform_2nd_sse128 FPFX(inv_transform_2nd_sse128)
void inv_transform_2nd_sse128    (coeff_t *coeff, int i_coeff, int i_mode, int b_top, int b_left);
#define inv_wavelet_64x16_sse128 FPFX(inv_wavelet_64x16_sse128)
void inv_wavelet_64x16_sse128(coeff_t *coeff);
#define inv_wavelet_16x64_sse128 FPFX(inv_wavelet_16x64_sse128)
void inv_wavelet_16x64_sse128(coeff_t *coeff);

//futl add 2016.11.30    avx2
#define idct_8x8_avx2 FPFX(vec_idct_8x8_avx2)
void idct_8x8_avx2  (const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_16x16_avx2 FPFX(vec_idct_16x16_avx2)
void idct_16x16_avx2(const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_32x32_avx2 FPFX(vec_idct_32x32_avx2)
void idct_32x32_avx2(const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_64x64_avx2 FPFX(vec_idct_64x64_avx2)
void idct_64x64_avx2(const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_64x16_avx2 FPFX(vec_idct_64x16_avx2)
void idct_64x16_avx2(const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_16x64_avx2 FPFX(vec_idct_16x64_avx2)
void idct_16x64_avx2(const coeff_t *src, coeff_t *dst, int i_dst);
#define inv_wavelet_64x16_avx2 FPFX(inv_wavelet_64x16_avx2)
void inv_wavelet_64x16_avx2(coeff_t *coeff);
#define inv_wavelet_16x64_avx2 FPFX(inv_wavelet_16x64_avx2)
void inv_wavelet_16x64_avx2(coeff_t *coeff);
#define inv_wavelet_64x64_avx2 FPFX(inv_wavelet_64x64_avx2)
void inv_wavelet_64x64_avx2(coeff_t *coeff);

/* DCT half and quad */
#define idct_4x4_half_sse128 FPFX(idct_4x4_half_sse128)
void idct_4x4_half_sse128  (const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_8x8_half_sse128 FPFX(idct_8x8_half_sse128)
void idct_8x8_half_sse128  (const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_16x16_half_sse128 FPFX(idct_16x16_half_sse128)
void idct_16x16_half_sse128(const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_32x32_half_sse128 FPFX(idct_32x32_half_sse128)
void idct_32x32_half_sse128(const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_64x64_half_sse128 FPFX(idct_64x64_half_sse128)
void idct_64x64_half_sse128(const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_16x4_half_sse128 FPFX(idct_16x4_half_sse128)
void idct_16x4_half_sse128 (const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_32x8_half_sse128 FPFX(idct_32x8_half_sse128)
void idct_32x8_half_sse128 (const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_4x16_half_sse128 FPFX(idct_4x16_half_sse128)
void idct_4x16_half_sse128 (const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_8x32_half_sse128 FPFX(idct_8x32_half_sse128)
void idct_8x32_half_sse128 (const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_16x64_half_sse128 FPFX(idct_16x64_half_sse128)
void idct_16x64_half_sse128(const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_64x16_half_sse128 FPFX(idct_64x16_half_sse128)
void idct_64x16_half_sse128(const coeff_t *src, coeff_t *dst, int i_dst);

#define idct_4x4_quad_sse128 FPFX(idct_4x4_quad_sse128)
void idct_4x4_quad_sse128  (const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_8x8_quad_sse128 FPFX(idct_8x8_quad_sse128)
void idct_8x8_quad_sse128  (const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_16x16_quad_sse128 FPFX(idct_16x16_quad_sse128)
void idct_16x16_quad_sse128(const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_32x32_quad_sse128 FPFX(idct_32x32_quad_sse128)
void idct_32x32_quad_sse128(const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_64x64_quad_sse128 FPFX(idct_64x64_quad_sse128)
void idct_64x64_quad_sse128(const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_16x4_quad_sse128 FPFX(idct_16x4_quad_sse128)
void idct_16x4_quad_sse128 (const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_32x8_quad_sse128 FPFX(idct_32x8_quad_sse128)
void idct_32x8_quad_sse128 (const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_4x16_quad_sse128 FPFX(idct_4x16_quad_sse128)
void idct_4x16_quad_sse128 (const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_8x32_quad_sse128 FPFX(idct_8x32_quad_sse128)
void idct_8x32_quad_sse128 (const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_16x64_quad_sse128 FPFX(idct_16x64_quad_sse128)
void idct_16x64_quad_sse128(const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_64x16_quad_sse128 FPFX(idct_64x16_quad_sse128)
void idct_64x16_quad_sse128(const coeff_t *src, coeff_t *dst, int i_dst);

#define idct_8x8_half_avx2 FPFX(idct_8x8_half_avx2)
void idct_8x8_half_avx2  (const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_16x16_half_avx2 FPFX(idct_16x16_half_avx2)
void idct_16x16_half_avx2(const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_32x32_half_avx2 FPFX(idct_32x32_half_avx2)
void idct_32x32_half_avx2(const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_64x64_half_avx2 FPFX(idct_64x64_half_avx2)
void idct_64x64_half_avx2(const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_16x4_half_avx2 FPFX(idct_16x4_half_avx2)
void idct_16x4_half_avx2 (const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_32x8_half_avx2 FPFX(idct_32x8_half_avx2)
void idct_32x8_half_avx2 (const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_4x16_half_avx2 FPFX(idct_4x16_half_avx2)
void idct_4x16_half_avx2 (const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_8x32_half_avx2 FPFX(idct_8x32_half_avx2)
void idct_8x32_half_avx2 (const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_16x64_half_avx2 FPFX(idct_16x64_half_avx2)
void idct_16x64_half_avx2(const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_64x16_half_avx2 FPFX(idct_64x16_half_avx2)
void idct_64x16_half_avx2(const coeff_t *src, coeff_t *dst, int i_dst);

#define idct_8x8_quad_avx2 FPFX(idct_8x8_quad_avx2)
void idct_8x8_quad_avx2  (const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_16x16_quad_avx2 FPFX(idct_16x16_quad_avx2)
void idct_16x16_quad_avx2(const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_32x32_quad_avx2 FPFX(idct_32x32_quad_avx2)
void idct_32x32_quad_avx2(const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_64x64_quad_avx2 FPFX(idct_64x64_quad_avx2)
void idct_64x64_quad_avx2(const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_16x4_quad_avx2 FPFX(idct_16x4_quad_avx2)
void idct_16x4_quad_avx2 (const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_32x8_quad_avx2 FPFX(idct_32x8_quad_avx2)
void idct_32x8_quad_avx2 (const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_4x16_quad_avx2 FPFX(idct_4x16_quad_avx2)
void idct_4x16_quad_avx2 (const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_8x32_quad_avx2 FPFX(idct_8x32_quad_avx2)
void idct_8x32_quad_avx2 (const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_16x64_quad_avx2 FPFX(idct_16x64_quad_avx2)
void idct_16x64_quad_avx2(const coeff_t *src, coeff_t *dst, int i_dst);
#define idct_64x16_quad_avx2 FPFX(idct_64x16_quad_avx2)
void idct_64x16_quad_avx2(const coeff_t *src, coeff_t *dst, int i_dst);


/* ---------------------------------------------------------------------------
 * SAO
 */
#define SAO_on_block_bo_sse128 FPFX(SAO_on_block_bo_sse128)
void SAO_on_block_bo_sse128    (pel_t *p_dst, int i_dst, const pel_t *p_src, int i_src, int i_block_w, int i_block_h, int bit_depth, const sao_param_t *sao_param);
#define SAO_on_block_eo_0_sse128 FPFX(SAO_on_block_eo_0_sse128)
void SAO_on_block_eo_0_sse128  (pel_t *p_dst, int i_dst, const pel_t *p_src, int i_src, int i_block_w, int i_block_h, int bit_depth, const int *lcu_avail, const int *sao_offset);
#define SAO_on_block_eo_45_sse128 FPFX(SAO_on_block_eo_45_sse128)
void SAO_on_block_eo_45_sse128 (pel_t *p_dst, int i_dst, const pel_t *p_src, int i_src, int i_block_w, int i_block_h, int bit_depth, const int *lcu_avail, const int *sao_offset);
#define SAO_on_block_eo_90_sse128 FPFX(SAO_on_block_eo_90_sse128)
void SAO_on_block_eo_90_sse128 (pel_t *p_dst, int i_dst, const pel_t *p_src, int i_src, int i_block_w, int i_block_h, int bit_depth, const int *lcu_avail, const int *sao_offset);
#define SAO_on_block_eo_135_sse128 FPFX(SAO_on_block_eo_135_sse128)
void SAO_on_block_eo_135_sse128(pel_t *p_dst, int i_dst, const pel_t *p_src, int i_src, int i_block_w, int i_block_h, int bit_depth, const int *lcu_avail, const int *sao_offset);
#define SAO_on_block_bo_avx2 FPFX(SAO_on_block_bo_avx2)
void SAO_on_block_bo_avx2    (pel_t *p_dst, int i_dst, const pel_t *p_src, int i_src, int i_block_w, int i_block_h, int bit_depth, const sao_param_t *sao_param);
#define SAO_on_block_eo_0_avx2 FPFX(SAO_on_block_eo_0_avx2)
void SAO_on_block_eo_0_avx2  (pel_t *p_dst, int i_dst, const pel_t *p_src, int i_src, int i_block_w, int i_block_h, int bit_depth, const int *lcu_avail, const int *sao_offset);
#define SAO_on_block_eo_45_avx2 FPFX(SAO_on_block_eo_45_avx2)
void SAO_on_block_eo_45_avx2 (pel_t *p_dst, int i_dst, const pel_t *p_src, int i_src, int i_block_w, int i_block_h, int bit_depth, const int *lcu_avail, const int *sao_offset);
#define SAO_on_block_eo_90_avx2 FPFX(SAO_on_block_eo_90_avx2)
void SAO_on_block_eo_90_avx2 (pel_t *p_dst, int i_dst, const pel_t *p_src, int i_src, int i_block_w, int i_block_h, int bit_depth, const int *lcu_avail, const int *sao_offset);
#define SAO_on_block_eo_135_avx2 FPFX(SAO_on_block_eo_135_avx2)
void SAO_on_block_eo_135_avx2(pel_t *p_dst, int i_dst, const pel_t *p_src, int i_src, int i_block_w, int i_block_h, int bit_depth, const int *lcu_avail, const int *sao_offset);

/* ---------------------------------------------------------------------------
 * ALF
 */
#define alf_filter_block_sse128 FPFX(alf_filter_block_sse128)
void alf_filter_block_sse128(pel_t *p_dst, const pel_t *p_src, int stride,
    int lcu_pix_x, int lcu_pix_y, int lcu_width, int lcu_height,
    int *alf_coef, int b_top_avail, int b_down_avail);


/* ---------------------------------------------------------------------------
 * Intra Prediction
 */
#define fill_edge_samples_0_sse128 FPFX(fill_edge_samples_0_sse128)
void fill_edge_samples_0_sse128 (const pel_t *pTL, int i_TL, const pel_t *pLcuEP, pel_t *EP, uint32_t i_avai, int bsx, int bsy);
#define fill_edge_samples_x_sse128 FPFX(fill_edge_samples_x_sse128)
void fill_edge_samples_x_sse128 (const pel_t *pTL, int i_TL, const pel_t *pLcuEP, pel_t *EP, uint32_t i_avai, int bsx, int bsy);
#define fill_edge_samples_y_sse128 FPFX(fill_edge_samples_y_sse128)
void fill_edge_samples_y_sse128 (const pel_t *pTL, int i_TL, const pel_t *pLcuEP, pel_t *EP, uint32_t i_avai, int bsx, int bsy);
#define fill_edge_samples_xy_sse128 FPFX(fill_edge_samples_xy_sse128)
void fill_edge_samples_xy_sse128(const pel_t *pTL, int i_TL, const pel_t *pLcuEP, pel_t *EP, uint32_t i_avai, int bsx, int bsy);

#define intra_pred_dc_sse128 FPFX(intra_pred_dc_sse128)
void intra_pred_dc_sse128       (pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_plane_sse128 FPFX(intra_pred_plane_sse128)
void intra_pred_plane_sse128    (pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_bilinear_sse128 FPFX(intra_pred_bilinear_sse128)
void intra_pred_bilinear_sse128 (pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_hor_sse128 FPFX(intra_pred_hor_sse128)
void intra_pred_hor_sse128      (pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ver_sse128 FPFX(intra_pred_ver_sse128)
void intra_pred_ver_sse128      (pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);

#define intra_pred_ang_x_3_sse128 FPFX(intra_pred_ang_x_3_sse128)
void intra_pred_ang_x_3_sse128  (pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_x_4_sse128 FPFX(intra_pred_ang_x_4_sse128)
void intra_pred_ang_x_4_sse128  (pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_x_5_sse128 FPFX(intra_pred_ang_x_5_sse128)
void intra_pred_ang_x_5_sse128  (pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_x_6_sse128 FPFX(intra_pred_ang_x_6_sse128)
void intra_pred_ang_x_6_sse128  (pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_x_7_sse128 FPFX(intra_pred_ang_x_7_sse128)
void intra_pred_ang_x_7_sse128  (pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_x_8_sse128 FPFX(intra_pred_ang_x_8_sse128)
void intra_pred_ang_x_8_sse128  (pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_x_9_sse128 FPFX(intra_pred_ang_x_9_sse128)
void intra_pred_ang_x_9_sse128  (pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_x_10_sse128 FPFX(intra_pred_ang_x_10_sse128)
void intra_pred_ang_x_10_sse128 (pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_x_11_sse128 FPFX(intra_pred_ang_x_11_sse128)
void intra_pred_ang_x_11_sse128 (pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);

#define intra_pred_ang_y_25_sse128 FPFX(intra_pred_ang_y_25_sse128)
void intra_pred_ang_y_25_sse128 (pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_y_26_sse128 FPFX(intra_pred_ang_y_26_sse128)
void intra_pred_ang_y_26_sse128 (pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_y_27_sse128 FPFX(intra_pred_ang_y_27_sse128)
void intra_pred_ang_y_27_sse128(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_y_28_sse128 FPFX(intra_pred_ang_y_28_sse128)
void intra_pred_ang_y_28_sse128(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_y_29_sse128 FPFX(intra_pred_ang_y_29_sse128)
void intra_pred_ang_y_29_sse128(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_y_30_sse128 FPFX(intra_pred_ang_y_30_sse128)
void intra_pred_ang_y_30_sse128 (pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_y_31_sse128 FPFX(intra_pred_ang_y_31_sse128)
void intra_pred_ang_y_31_sse128 (pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_y_32_sse128 FPFX(intra_pred_ang_y_32_sse128)
void intra_pred_ang_y_32_sse128 (pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);

#define intra_pred_ang_xy_13_sse128 FPFX(intra_pred_ang_xy_13_sse128)
void intra_pred_ang_xy_13_sse128(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_xy_14_sse128 FPFX(intra_pred_ang_xy_14_sse128)
void intra_pred_ang_xy_14_sse128(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_xy_16_sse128 FPFX(intra_pred_ang_xy_16_sse128)
void intra_pred_ang_xy_16_sse128(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_xy_18_sse128 FPFX(intra_pred_ang_xy_18_sse128)
void intra_pred_ang_xy_18_sse128(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_xy_20_sse128 FPFX(intra_pred_ang_xy_20_sse128)
void intra_pred_ang_xy_20_sse128(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_xy_22_sse128 FPFX(intra_pred_ang_xy_22_sse128)
void intra_pred_ang_xy_22_sse128(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_xy_23_sse128 FPFX(intra_pred_ang_xy_23_sse128)
void intra_pred_ang_xy_23_sse128(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);

//intra prediction avx functions
#define intra_pred_ver_avx FPFX(intra_pred_ver_avx)
void intra_pred_ver_avx(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_hor_avx FPFX(intra_pred_hor_avx)
void intra_pred_hor_avx(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_dc_avx FPFX(intra_pred_dc_avx)
void intra_pred_dc_avx(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_plane_avx FPFX(intra_pred_plane_avx)
void intra_pred_plane_avx(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_bilinear_avx FPFX(intra_pred_bilinear_avx)
void intra_pred_bilinear_avx(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_x_3_avx FPFX(intra_pred_ang_x_3_avx)
void intra_pred_ang_x_3_avx(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_x_4_avx FPFX(intra_pred_ang_x_4_avx)
void intra_pred_ang_x_4_avx(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_x_5_avx FPFX(intra_pred_ang_x_5_avx)
void intra_pred_ang_x_5_avx(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_x_6_avx FPFX(intra_pred_ang_x_6_avx)
void intra_pred_ang_x_6_avx(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_x_7_avx FPFX(intra_pred_ang_x_7_avx)
void intra_pred_ang_x_7_avx(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_x_8_avx FPFX(intra_pred_ang_x_8_avx)
void intra_pred_ang_x_8_avx(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_x_9_avx FPFX(intra_pred_ang_x_9_avx)
void intra_pred_ang_x_9_avx(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_x_10_avx FPFX(intra_pred_ang_x_10_avx)
void intra_pred_ang_x_10_avx(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_x_11_avx FPFX(intra_pred_ang_x_11_avx)
void intra_pred_ang_x_11_avx(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);

#define intra_pred_ang_xy_13_avx FPFX(intra_pred_ang_xy_13_avx)
void intra_pred_ang_xy_13_avx(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_xy_14_avx FPFX(intra_pred_ang_xy_14_avx)
void intra_pred_ang_xy_14_avx(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_xy_16_avx FPFX(intra_pred_ang_xy_16_avx)
void intra_pred_ang_xy_16_avx(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_xy_18_avx FPFX(intra_pred_ang_xy_18_avx)
void intra_pred_ang_xy_18_avx(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_xy_20_avx FPFX(intra_pred_ang_xy_20_avx)
void intra_pred_ang_xy_20_avx(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_xy_22_avx FPFX(intra_pred_ang_xy_22_avx)
void intra_pred_ang_xy_22_avx(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_xy_23_avx FPFX(intra_pred_ang_xy_23_avx)
void intra_pred_ang_xy_23_avx(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);

#define intra_pred_ang_y_25_avx FPFX(intra_pred_ang_y_25_avx)
void intra_pred_ang_y_25_avx(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_y_26_avx FPFX(intra_pred_ang_y_26_avx)
void intra_pred_ang_y_26_avx(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_y_28_avx FPFX(intra_pred_ang_y_28_avx)
void intra_pred_ang_y_28_avx(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_y_30_avx FPFX(intra_pred_ang_y_30_avx)
void intra_pred_ang_y_30_avx(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_y_31_avx FPFX(intra_pred_ang_y_31_avx)
void intra_pred_ang_y_31_avx(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
#define intra_pred_ang_y_32_avx FPFX(intra_pred_ang_y_32_avx)
void intra_pred_ang_y_32_avx(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
/* Function declaration defines */

#define FUNCDEF_TU(ret, name, cpu, ...) \
    ret FPFX(name ## _4x4_   ## cpu(__VA_ARGS__));\
    ret FPFX(name ## _8x8_   ## cpu(__VA_ARGS__));\
    ret FPFX(name ## _16x16_ ## cpu(__VA_ARGS__));\
    ret FPFX(name ## _32x32_ ## cpu(__VA_ARGS__));\
    ret FPFX(name ## _64x64_ ## cpu(__VA_ARGS__))

#define FUNCDEF_TU_S(ret, name, cpu, ...) \
    ret FPFX(name ## _4_  ## cpu(__VA_ARGS__));\
    ret FPFX(name ## _8_  ## cpu(__VA_ARGS__));\
    ret FPFX(name ## _16_ ## cpu(__VA_ARGS__));\
    ret FPFX(name ## _32_ ## cpu(__VA_ARGS__));\
    ret FPFX(name ## _64_ ## cpu(__VA_ARGS__))

#define FUNCDEF_PU(ret, name, cpu, ...) \
    ret FPFX(name ## _4x4_   ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _8x8_   ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _16x16_ ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _32x32_ ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _64x64_ ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _8x4_   ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _4x8_   ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _16x8_  ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _8x16_  ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _16x32_ ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _32x16_ ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _64x32_ ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _32x64_ ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _16x12_ ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _12x16_ ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _16x4_  ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _4x16_  ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _32x24_ ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _24x32_ ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _32x8_  ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _8x32_  ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _64x48_ ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _48x64_ ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _64x16_ ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _16x64_ ## cpu)(__VA_ARGS__)

#define FUNCDEF_CHROMA_PU(ret, name, cpu, ...) \
    FUNCDEF_PU(ret, name, cpu, __VA_ARGS__);\
    ret FPFX(name ## _4x2_   ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _2x4_   ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _8x2_   ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _2x8_   ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _8x6_   ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _6x8_   ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _8x12_  ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _12x8_  ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _6x16_  ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _16x6_  ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _2x16_  ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _16x2_  ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _4x12_  ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _12x4_  ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _32x12_ ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _12x32_ ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _32x4_  ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _4x32_  ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _32x48_ ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _48x32_ ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _16x24_ ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _24x16_ ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _8x64_  ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _64x8_  ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _64x24_ ## cpu)(__VA_ARGS__);\
    ret FPFX(name ## _24x64_ ## cpu)(__VA_ARGS__);


#ifdef __cplusplus
}
#endif
#endif // #ifndef DAVS2_INTRINSIC_H
