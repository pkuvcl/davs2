/*
 * primitives.h
 *
 * Description of this file:
 *    function handles initialize functions definition of the davs2 library
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

#ifndef DAVS2_PRIMITIVES_H
#define DAVS2_PRIMITIVES_H
#ifdef __cplusplus
extern "C" {
#endif

/**
 * ===========================================================================
 * macros
 * ===========================================================================
 */

#if HIGH_BIT_DEPTH
#define MC_PART_INDEX(width, height)  (width >= 8)
#else
#define MC_PART_INDEX(width, height)  (width > 8)
#endif

/**
 * ===========================================================================
 * function definitions and structures
 * ===========================================================================
 */


/**
 * ===========================================================================
 * type defines
 * ===========================================================================
 */


/* ---------------------------------------------------------------------------
 * function handle types
 */
typedef void(*block_copy_pp_t)(pel_t *dst, intptr_t i_dst, pel_t *src, intptr_t i_src, int w, int h);
typedef void(*block_copy_sc_t)(coeff_t *dst, intptr_t i_dst, int16_t *src, intptr_t i_src, int w, int h);
typedef void(*block_intpl_t)(const pel_t* src, intptr_t srcStride, pel_t* dst, intptr_t dstStride, int coeffIdx);
typedef void(*block_intpl_ext_t)(const pel_t* src, intptr_t srcStride, pel_t* dst, intptr_t dstStride, int coeffIdxX, int coeffIdxY);
typedef void(*intpl_t)    (pel_t *dst, int i_dst, pel_t *src, int i_src, int width, int height, const int8_t *coeff);
typedef void(*intpl_ext_t)(pel_t *dst, int i_dst, pel_t *src, int i_src, int width, int height, const int8_t *coeff_x, const int8_t *coeff_y);
typedef void(*pixel_avg_pp_t)(pel_t *dst, int i_dst, const pel_t *src0, int i_src0, const pel_t *src1, int i_src1, int width, int height);
typedef void(*dct_t)(const coeff_t *src, coeff_t *dst, int i_src);

typedef void(*intra_pred_t)(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy);
typedef void(*fill_edge_t)(const pel_t *p_topleft, int i_topleft, const pel_t *p_lcu_ep, pel_t *EP, uint32_t i_avail, int bsx, int bsy);

typedef void *(*memcpy_t)(void *dst, const void *src, size_t n);
typedef void(*copy_pp_t)(pel_t* dst, intptr_t dstStride, const pel_t* src, intptr_t srcStride); // dst is aligned
typedef void(*copy_ss_t)(coeff_t* dst, intptr_t dstStride, const coeff_t* src, intptr_t srcStride);

typedef void(*pixel_add_ps_t)(pel_t* dst, intptr_t dstride, const pel_t* b0, const coeff_t* b1, intptr_t sstride0, intptr_t sstride1);

typedef void(*lcu_deblock_t)(davs2_t *h, davs2_frame_t *frm, int i_lcu_x, int i_lcu_y);

typedef void(*sao_flt_bo_t)(pel_t *p_dst, int i_dst, const pel_t *p_src, int i_src, int i_block_w, int i_block_h, int bit_depth, const sao_param_t *sao_param);
typedef void(*sao_flt_eo_t)(pel_t *p_dst, int i_dst, const pel_t *p_src, int i_src, int i_block_w, int i_block_h, int bit_depth, const int *lcu_avail, const int *sao_offset);


/* ---------------------------------------------------------------------------
 * assembly optimization functions
 */
typedef struct ao_funcs_t {
    ALIGN32(uint32_t    initial_count);
    uint32_t            cpuid;
    /* memory copy */
    memcpy_t            fast_memcpy;
    memcpy_t            memcpy_aligned;
    void*(*fast_memzero)   (void *dst, size_t n);
    void*(*memzero_aligned)(void *dst, size_t n);
    void*(*fast_memset)    (void *dst, int val, size_t n);

    /* plane copy */
    void(*plane_copy)(pel_t *dst, intptr_t i_dst, pel_t *src, intptr_t i_src, int w, int h);
    block_copy_pp_t block_copy;
    block_copy_sc_t block_coeff_copy;

    copy_pp_t       copy_pp[MAX_PART_NUM];
    copy_ss_t       copy_ss[MAX_PART_NUM];
    pixel_add_ps_t  add_ps[MAX_PART_NUM];

    /* block average */
    pixel_avg_pp_t  block_avg;

    /* interpolate */
#if USE_NEW_INTPL
    block_intpl_t       block_intpl_luma_hor[MAX_PART_NUM];
    block_intpl_t       block_intpl_luma_ver[MAX_PART_NUM];
    block_intpl_ext_t   block_intpl_luma_ext[MAX_PART_NUM];
#endif
    intpl_t         intpl_luma_ver[2][3];//[2]:根据块大小进行函数拆分（0:size<=8   1:size>=16）     [3]:根据权重系数进行拆分
    intpl_t         intpl_luma_hor[2][3];
    intpl_ext_t     intpl_luma_ext[2];

    intpl_t         intpl_chroma_ver[2];
    intpl_t         intpl_chroma_hor[2];
    intpl_ext_t     intpl_chroma_ext[2];

    /* intra prediction */
    intra_pred_t    intraf[NUM_INTRA_MODE];
    fill_edge_t     fill_edge_f[4];

    /* loop filter */
    void(*set_deblock_const)(void);

    void(*deblock_luma[2])  (pel_t *src, int stride, int alpha, int beta, uint8_t *flt_flag);
#if HDR_CHROMA_DELTA_QP
    void(*deblock_chroma[2])(pel_t *src_u, pel_t *src_v, int stride, int *alpha, int *beta, uint8_t *flt_flag);
#else
    void(*deblock_chroma[2])(pel_t *src_u, pel_t *src_v, int stride, int alpha, int beta, uint8_t *flt_flag);
#endif

    /* SAO filter */
    sao_flt_bo_t     sao_block_bo;          /* filter for bo type */
    sao_flt_eo_t     sao_filter_eo[4];      /* SAO filter for eo types */

    /* alf */
    void(*alf_block[2])(pel_t *p_dst, const pel_t *p_src, int stride,
        int lcu_pix_x, int lcu_pix_y, int lcu_width, int lcu_height,
        int *alf_coeff, int b_top_avail, int b_down_avail);

    /* dct */
    dct_t        idct[MAX_PART_NUM][DCT_PATTERN_NUM];  /* sqrt dct */

    /* 2nd transform */
    void(*inv_transform_4x4_2nd)(coeff_t *coeff, int i_coeff);
    void(*inv_transform_2nd)    (coeff_t *coeff, int i_coeff, int i_mode, int b_top, int b_left);

    /* quant */
    void(*dequant)(coeff_t *coef, const int i_coef, const int scale, const int shift);
} ao_funcs_t;

extern ao_funcs_t gf_davs2;


/**
 * ===========================================================================
 * interface function declares
 * ===========================================================================
 */
#define init_all_primitives FPFX(init_all_primitives)
void init_all_primitives(uint32_t cpuid);


/* ---------------------------------------------------------------------------
 * extern functions
 */
#define davs2_mc_init FPFX(mc_init)
void davs2_mc_init    (uint32_t cpuid, ao_funcs_t *pf);
#define davs2_pixel_init FPFX(pixel_init)
void davs2_pixel_init (uint32_t cpuid, ao_funcs_t* pixf);
#define davs2_memory_init FPFX(memory_init)
void davs2_memory_init(uint32_t cpuid, ao_funcs_t* pixf);

#ifdef __cplusplus
}
#endif
#endif  // DAVS2_PRIMITIVES_H
