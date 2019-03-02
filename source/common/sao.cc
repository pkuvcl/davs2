/*
 * sao.cc
 *
 * Description of this file:
 *    SAO functions definition of the davs2 library
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
#include "sao.h"
#include "aec.h"
#include "frame.h"
#include "vec/intrinsic.h"

#if defined(_MSC_VER) || defined(__ICL)
#pragma warning(disable: 4204)  // nonstandard extension used: non-constant aggregate initializer
#endif


/**
 * ===========================================================================
 * local & global variables (const tables)
 * ===========================================================================
 */

const int saoclip[NUM_SAO_OFFSET][3] = {
    //EO
    { -1, 6, 7 }, // low bound, upper bound, threshold
    {  0, 1, 1 },
    {  0, 0, 0 },
    { -1, 0, 1 },
    { -6, 1, 7 },
    { -7, 7, 7 } // BO
};

/* ---------------------------------------------------------------------------
 * lcu neighbor
 */
enum lcu_neighbor_e {
    SAO_T   = 0,    /* top        */
    SAO_D   = 1,    /* down       */
    SAO_L   = 2,    /* left       */
    SAO_R   = 3,    /* right      */
    SAO_TL  = 4,    /* top-left   */
    SAO_TR  = 5,    /* top-right  */
    SAO_DL  = 6,    /* down-left  */
    SAO_DR  = 7     /* down-right */
};

typedef struct sao_region_t {
    int    pix_x[IMG_COMPONENTS];       /* start pixel position in x */
    int    pix_y[IMG_COMPONENTS];       /* start pixel position in y */
    int    width[IMG_COMPONENTS];       /*  */
    int    height[IMG_COMPONENTS];      /*  */

    /* availabilities of neighboring blocks */
    int8_t b_left;
    int8_t b_top_left;
    int8_t b_top;
    int8_t b_top_right;
    int8_t b_right;
    int8_t b_right_down;
    int8_t b_down;
    int8_t b_down_left;
} sao_region_t;


/**
 * ===========================================================================
 * function defines
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 */
static ALWAYS_INLINE void sao_init_param(sao_t *lcu_sao)
{
    int i;

    for (i = 0; i < IMG_COMPONENTS; i++) {
        lcu_sao->planes[i].modeIdc    = SAO_MODE_OFF;
        lcu_sao->planes[i].typeIdc    = -1;
        lcu_sao->planes[i].startBand  = -1;
        lcu_sao->planes[i].startBand2 = -1;
        memset(lcu_sao->planes[i].offset, 0, MAX_NUM_SAO_CLASSES * sizeof(int));
    }
}

/* ---------------------------------------------------------------------------
 */
static ALWAYS_INLINE void sao_copy_param(sao_t *dst, sao_t *src)
{
    memcpy(dst, src, sizeof(sao_t));
}

/* ---------------------------------------------------------------------------
 */
static
void sao_block_eo_0_c(pel_t *p_dst, int i_dst,
                      const pel_t *p_src, int i_src,
                      int i_block_w, int i_block_h,
                      int bit_depth, const int *lcu_avail, const int *sao_offset)
{
    const int max_pel_val = (1 << bit_depth) - 1;
    int left_sign, right_sign;
    int edge_type;
    int x, y;
    int pel_diff;

    int sx = lcu_avail[SAO_L] ? 0 : 1;
    int ex = lcu_avail[SAO_R] ? i_block_w : (i_block_w - 1);
    for (y = 0; y < i_block_h; y++) {
        pel_diff = p_src[sx] - p_src[sx - 1];
        left_sign = pel_diff > 0? 1 : (pel_diff < 0? -1 : 0);
        for (x = sx; x < ex; x++) {
            pel_diff = p_src[x] - p_src[x + 1];
            right_sign = pel_diff > 0? 1 : (pel_diff < 0? -1 : 0);
            edge_type = left_sign + right_sign + 2;
            left_sign = -right_sign;
            p_dst[x] = (pel_t)DAVS2_CLIP3(0, max_pel_val, p_src[x] + sao_offset[edge_type]);
        }
        p_src += i_src;
        p_dst += i_dst;
    }
}

/* ---------------------------------------------------------------------------
 */
static
void sao_block_eo_90_c(pel_t *p_dst, int i_dst,
                       const pel_t *p_src, int i_src,
                       int i_block_w, int i_block_h,
                       int bit_depth, const int *lcu_avail, const int *sao_offset)
{
    const int max_pel_val = (1 << bit_depth) - 1;
    int edge_type;
    int x, y;

    int sy = lcu_avail[SAO_T] ? 0 : 1;
    int ey = lcu_avail[SAO_D] ? i_block_h : (i_block_h - 1);
    for (x = 0; x < i_block_w; x++) {
        int pel_diff = p_src[sy * i_src + x] - p_src[(sy - 1) * i_src + x];
        int top_sign = pel_diff > 0 ? 1 : (pel_diff < 0 ? -1 : 0);
        for (y = sy; y < ey; y++) {
            int pelDiff = p_src[y * i_src + x] - p_src[(y + 1) * i_src + x];
            int down_sign = pelDiff > 0 ? 1 : (pelDiff < 0 ? -1 : 0);
            edge_type = down_sign + top_sign + 2;
            top_sign = -down_sign;
            p_dst[y * i_dst + x] = (pel_t)DAVS2_CLIP3(0, max_pel_val, p_src[y * i_src + x] + sao_offset[edge_type]);
        }
    }
}

/* ---------------------------------------------------------------------------
 */
static
void sao_block_eo_135_c(pel_t *p_dst, int i_dst,
                        const pel_t *p_src, int i_src,
                        int i_block_w, int i_block_h,
                        int bit_depth, const int *lcu_avail, const int *sao_offset)
{
    int8_t SIGN_BUF[MAX_CU_SIZE + 32];  // sign of top line
    int8_t *UPROW_S = SIGN_BUF + 16;
    const int max_pel_val = (1 << bit_depth) - 1;
    int reg = 0;
    int sx, ex;               // start/end (x, y)
    int sx_0, ex_0, sx_n, ex_n;       // start/end x for first and last row
    int top_sign, down_sign;
    int edge_type;
    int pel_diff;
    int x, y;

    sx = lcu_avail[SAO_L] ? 0 : 1;
    ex = lcu_avail[SAO_R] ? i_block_w : (i_block_w - 1);

    // init the line buffer
    for (x = sx; x < ex; x++) {
        pel_diff = p_src[i_src + x + 1] - p_src[x];
        top_sign = pel_diff > 0 ? 1 : (pel_diff < 0 ? -1 : 0);
        UPROW_S[x + 1] = (int8_t)top_sign;
    }

    // first row
    sx_0 = lcu_avail[SAO_TL] ? 0 : 1;
    ex_0 = lcu_avail[SAO_T] ? (lcu_avail[SAO_R] ? i_block_w : (i_block_w - 1)) : 1;
    for (x = sx_0; x < ex_0; x++) {
        pel_diff = p_src[x] - p_src[-i_src + x - 1];
        top_sign = pel_diff > 0 ? 1 : (pel_diff < 0 ? -1 : 0);
        edge_type = top_sign - UPROW_S[x + 1] + 2;
        p_dst[x] = (pel_t)DAVS2_CLIP3(0, max_pel_val, p_src[x] + sao_offset[edge_type]);
    }

    // middle rows
    for (y = 1; y < i_block_h - 1; y++) {
        p_src += i_src;
        p_dst += i_dst;
        for (x = sx; x < ex; x++) {
            if (x == sx) {
                pel_diff = p_src[x] - p_src[-i_src + x - 1];
                top_sign = pel_diff > 0 ? 1 : (pel_diff < 0 ? -1 : 0);
                UPROW_S[x] = (int8_t)top_sign;
            }
            pel_diff = p_src[x] - p_src[i_src + x + 1];
            down_sign = pel_diff > 0 ? 1 : (pel_diff < 0 ? -1 : 0);
            edge_type = down_sign + UPROW_S[x] + 2;
            p_dst[x] = (pel_t)DAVS2_CLIP3(0, max_pel_val, p_src[x] + sao_offset[edge_type]);
            UPROW_S[x] = (int8_t)reg;
            reg = -down_sign;
        }
    }

    // last row
    sx_n = lcu_avail[SAO_D] ? (lcu_avail[SAO_L] ? 0 : 1) : (i_block_w - 1);
    ex_n = lcu_avail[SAO_DR] ? i_block_w : (i_block_w - 1);
    p_src += i_src;
    p_dst += i_dst;
    for (x = sx_n; x < ex_n; x++) {
        if (x == sx) {
            pel_diff = p_src[x] - p_src[-i_src + x - 1];
            top_sign = pel_diff > 0 ? 1 : (pel_diff < 0 ? -1 : 0);
            UPROW_S[x] = (int8_t)top_sign;
        }
        pel_diff = p_src[x] - p_src[i_src + x + 1];
        down_sign = pel_diff > 0 ? 1 : (pel_diff < 0 ? -1 : 0);
        edge_type = down_sign + UPROW_S[x] + 2;
        p_dst[x] = (pel_t)DAVS2_CLIP3(0, max_pel_val, p_src[x] + sao_offset[edge_type]);
    }
}

/* ---------------------------------------------------------------------------
 */
static
void sao_block_eo_45_c(pel_t *p_dst, int i_dst,
                       const pel_t *p_src, int i_src,
                       int i_block_w, int i_block_h,
                       int bit_depth, const int *lcu_avail, const int *sao_offset)
{
    int8_t SIGN_BUF[MAX_CU_SIZE + 32];  // sign of top line
    int8_t *UPROW_S = SIGN_BUF + 16;
    const int max_pel_val = (1 << bit_depth) - 1;
    int sx, ex;               // start/end (x, y)
    int sx_0, ex_0, sx_n, ex_n;       // start/end x for first and last row
    int top_sign, down_sign;
    int edge_type;
    int pel_diff;
    int x, y;

    sx = lcu_avail[SAO_L] ? 0 : 1;
    ex = lcu_avail[SAO_R] ? i_block_w : (i_block_w - 1);

    // init the line buffer
    for (x = sx; x < ex; x++) {
        pel_diff = p_src[i_src + x - 1] - p_src[x];
        top_sign = pel_diff > 0 ? 1 : (pel_diff < 0 ? -1 : 0);
        UPROW_S[x - 1] = (int8_t)top_sign;
    }

    // first row
    sx_0 = lcu_avail[SAO_T] ? (lcu_avail[SAO_L] ? 0 : 1) : (i_block_w - 1);
    ex_0 = lcu_avail[SAO_TR] ? i_block_w : (i_block_w - 1);
    for (x = sx_0; x < ex_0; x++) {
        pel_diff = p_src[x] - p_src[-i_src + x + 1];
        top_sign = pel_diff > 0 ? 1 : (pel_diff < 0 ? -1 : 0);
        edge_type = top_sign - UPROW_S[x - 1] + 2;
        p_dst[x] = (pel_t)DAVS2_CLIP3(0, max_pel_val, p_src[x] + sao_offset[edge_type]);
    }

    // middle rows
    for (y = 1; y < i_block_h - 1; y++) {
        p_src += i_src;
        p_dst += i_dst;
        for (x = sx; x < ex; x++) {
            if (x == ex - 1) {
                pel_diff = p_src[x] - p_src[-i_src + x + 1];
                top_sign = pel_diff > 0 ? 1 : (pel_diff < 0 ? -1 : 0);
                UPROW_S[x] = (int8_t)top_sign;
            }
            pel_diff = p_src[x] - p_src[i_src + x - 1];
            down_sign = pel_diff > 0 ? 1 : (pel_diff < 0 ? -1 : 0);
            edge_type = down_sign + UPROW_S[x] + 2;
            p_dst[x] = (pel_t)DAVS2_CLIP3(0, max_pel_val, p_src[x] + sao_offset[edge_type]);
            UPROW_S[x - 1] = (int8_t)(-down_sign);
        }
    }

    // last row
    sx_n = lcu_avail[SAO_DL] ? 0 : 1;
    ex_n = lcu_avail[SAO_D] ? (lcu_avail[SAO_R] ? i_block_w : (i_block_w - 1)) : 1;
    p_src += i_src;
    p_dst += i_dst;
    for (x = sx_n; x < ex_n; x++) {
        if (x == ex - 1) {
            pel_diff = p_src[x] - p_src[-i_src + x + 1];
            top_sign = pel_diff > 0 ? 1 : (pel_diff < 0 ? -1 : 0);
            UPROW_S[x] = (int8_t)top_sign;
        }
        pel_diff = p_src[x] - p_src[i_src + x - 1];
        down_sign = pel_diff > 0 ? 1 : (pel_diff < 0 ? -1 : 0);
        edge_type = down_sign + UPROW_S[x] + 2;
        p_dst[x] = (pel_t)DAVS2_CLIP3(0, max_pel_val, p_src[x] + sao_offset[edge_type]);
    }
}

/* ---------------------------------------------------------------------------
 */
static
void sao_block_bo_c(pel_t *p_dst, int i_dst,
                    const pel_t *p_src, int i_src,
                    int i_block_w, int i_block_h,
                    int bit_depth, const sao_param_t *sao_param)
{
    const int max_pel_val = (1 << bit_depth) - 1;
    const int *sao_offset = sao_param->offset;
    int edge_type;
    int x, y;

    const int band_shift = g_bit_depth - NUM_SAO_BO_CLASSES_IN_BIT;

    for (y = 0; y < i_block_h; y++) {
        for (x = 0; x < i_block_w; x++) {
            edge_type = p_src[x] >> band_shift;
            p_dst[x] = (pel_t)DAVS2_CLIP3(0, max_pel_val, p_src[x] + sao_offset[edge_type]);
        }
        p_src += i_src;
        p_dst += i_dst;
    }
}

/* ---------------------------------------------------------------------------
 */
static void sao_read_lcu(davs2_t *h, int lcu_xy, bool_t *slice_sao_on, sao_t *cur_sao_param)
{
    const int w_in_scu = h->i_width_in_scu;
    const int scu_x    = h->lcu.i_scu_x;
    const int scu_y    = h->lcu.i_scu_y;
    const int scu_xy   = h->lcu.i_scu_xy;
    int merge_mode     = 0;
    int merge_top_avail, merge_left_avail;

    /* neighbor available? */
    merge_top_avail  = (scu_y == 0) ? 0 : (h->scu_data[scu_xy].i_slice_nr == h->scu_data[scu_xy - w_in_scu].i_slice_nr);
    merge_left_avail = (scu_x == 0) ? 0 : (h->scu_data[scu_xy].i_slice_nr == h->scu_data[scu_xy -        1].i_slice_nr);

    if (merge_left_avail || merge_top_avail) {
        merge_mode = aec_read_sao_mergeflag(&h->aec, merge_left_avail, merge_top_avail);
    }

    if (merge_mode) {
        if (merge_mode == 2) {
            sao_copy_param(cur_sao_param, &h->lcu_infos[lcu_xy - 1].sao_param);  // copy left
        } else {
            assert(merge_mode == 1);
            sao_copy_param(cur_sao_param, &h->lcu_infos[lcu_xy - h->i_width_in_lcu].sao_param);  // copy above
        }
    } else {
        int offset[4];
        int stBnd[2];
        int db_temp;
        int sao_mode, sao_type;
        int i;

        for (i = 0; i < IMG_COMPONENTS; i++) {
            if (!slice_sao_on[i]) {
                cur_sao_param->planes[i].modeIdc = SAO_MODE_OFF;
            } else {
                sao_mode = aec_read_sao_mode(&h->aec);
                switch (sao_mode) {
                case 0:
                    cur_sao_param->planes[i].modeIdc = SAO_MODE_OFF;
                    break;
                case 1:
                    cur_sao_param->planes[i].modeIdc = SAO_MODE_NEW;
                    cur_sao_param->planes[i].typeIdc = SAO_TYPE_BO;
                    break;
                case 3:
                    cur_sao_param->planes[i].modeIdc = SAO_MODE_NEW;
                    cur_sao_param->planes[i].typeIdc = SAO_TYPE_EO_0;
                    break;
                default:
                    assert(1);
                    break;
                }

                if (cur_sao_param->planes[i].modeIdc == SAO_MODE_NEW) {
                    aec_read_sao_offsets(&h->aec, &cur_sao_param->planes[i], offset);
                    sao_type = aec_read_sao_type(&h->aec, &cur_sao_param->planes[i]);

                    if (cur_sao_param->planes[i].typeIdc == SAO_TYPE_BO) {
                        memset(cur_sao_param->planes[i].offset, 0, MAX_NUM_SAO_CLASSES * sizeof(int));
                        db_temp  = sao_type >> NUM_SAO_BO_CLASSES_LOG2;
                        stBnd[0] = sao_type - (db_temp << NUM_SAO_BO_CLASSES_LOG2);
                        stBnd[1] = (stBnd[0] + db_temp) % 32;
                        cur_sao_param->planes[i].startBand = stBnd[0];
                        cur_sao_param->planes[i].startBand2 = stBnd[1];
                        cur_sao_param->planes[i].offset[(stBnd[0]    )     ] = offset[0];
                        cur_sao_param->planes[i].offset[(stBnd[0] + 1) % 32] = offset[1];
                        cur_sao_param->planes[i].offset[(stBnd[1]    )     ] = offset[2];
                        cur_sao_param->planes[i].offset[(stBnd[1] + 1) % 32] = offset[3];
                        //memcpy(cur_sao_param->planes[i].offset, offset, 4 * sizeof(int));
                    } else {
                        assert(cur_sao_param->planes[i].typeIdc == SAO_TYPE_EO_0);
                        cur_sao_param->planes[i].typeIdc                          = sao_type;
                        cur_sao_param->planes[i].offset[SAO_CLASS_EO_FULL_VALLEY] = offset[0];
                        cur_sao_param->planes[i].offset[SAO_CLASS_EO_HALF_VALLEY] = offset[1];
                        cur_sao_param->planes[i].offset[SAO_CLASS_EO_PLAIN      ] = 0;
                        cur_sao_param->planes[i].offset[SAO_CLASS_EO_HALF_PEAK  ] = offset[2];
                        cur_sao_param->planes[i].offset[SAO_CLASS_EO_FULL_PEAK  ] = offset[3];
                    }
                }
            }
        }
    }
}

/* ---------------------------------------------------------------------------
 */
void sao_read_lcu_param(davs2_t *h, int lcu_xy, bool_t *slice_sao_on, sao_t *sao_param)
{
    if (slice_sao_on[0] || slice_sao_on[1] || slice_sao_on[2]) {
        sao_read_lcu(h, lcu_xy, slice_sao_on, sao_param);
    } else {
        sao_init_param(sao_param);
    }
}

/* ---------------------------------------------------------------------------
*/
static
void sao_get_neighbor_avail(davs2_t *h, sao_region_t *p_avail, int i_lcu_x, int i_lcu_y)
{
    int i_lcu_level = h->i_lcu_level;
    int pix_x = i_lcu_x << i_lcu_level;
    int pix_y = i_lcu_y << i_lcu_level;
    int width = DAVS2_MIN(1 << i_lcu_level, h->i_width - pix_x);
    int height = DAVS2_MIN(1 << i_lcu_level, h->i_height - pix_y);
    int pix_x_c = pix_x >> 1;
    int chroma_v_shift = (h->i_chroma_format == CHROMA_420);
    int pix_y_c = pix_y >> chroma_v_shift;
    int width_c = width >> 1;
    int height_c = height >> 1;

    /* 可用性获取 */
    p_avail->b_left = i_lcu_x != 0;
    p_avail->b_top  = i_lcu_y != 0;
    p_avail->b_right = (i_lcu_x < h->i_width_in_lcu - 1);
    p_avail->b_down  = (i_lcu_y < h->i_height_in_lcu - 1);

    if (h->seq_info.cross_loop_filter_flag == FALSE) {
        int scu_x = i_lcu_x << (h->i_lcu_level - MIN_CU_SIZE_IN_BIT);
        int scu_y = i_lcu_y << (h->i_lcu_level - MIN_CU_SIZE_IN_BIT);
        if (p_avail->b_top) {
            p_avail->b_top = h->scu_data[scu_y * h->i_width_in_scu + scu_x].i_slice_nr == h->scu_data[(scu_y - 1) * h->i_width_in_scu + scu_x].i_slice_nr;
        }
        if (p_avail->b_down) {
            scu_y += 1 << (h->i_lcu_level - MIN_CU_SIZE_IN_BIT);
            p_avail->b_down = h->scu_data[scu_y * h->i_width_in_scu + scu_x].i_slice_nr == h->scu_data[(scu_y - 1) * h->i_width_in_scu + scu_x].i_slice_nr;
        }
    }

    p_avail->b_top_left = p_avail->b_top && p_avail->b_left;
    p_avail->b_top_right = p_avail->b_top && p_avail->b_right;
    p_avail->b_down_left = p_avail->b_down && p_avail->b_left;
    p_avail->b_right_down = p_avail->b_down && p_avail->b_right;

    /* 滤波区域的调整 */
    if (!p_avail->b_right) {
        width += SAO_SHIFT_PIX_NUM;
        width_c += SAO_SHIFT_PIX_NUM;
    }

    if (!p_avail->b_down) {
        height += SAO_SHIFT_PIX_NUM;
        height_c += SAO_SHIFT_PIX_NUM;
    }

    if (p_avail->b_left) {
        pix_x -= SAO_SHIFT_PIX_NUM;
        pix_x_c -= SAO_SHIFT_PIX_NUM;
    }
    else {
        width -= SAO_SHIFT_PIX_NUM;
        width_c -= SAO_SHIFT_PIX_NUM;
    }

    if (p_avail->b_top) {
        pix_y -= SAO_SHIFT_PIX_NUM;
        pix_y_c -= SAO_SHIFT_PIX_NUM;
    }
    else {
        height -= SAO_SHIFT_PIX_NUM;
        height_c -= SAO_SHIFT_PIX_NUM;
    }

    /* make sure the width and height is not outside a picture */
    width = DAVS2_MIN(width, h->i_width - pix_x);
    width_c = DAVS2_MIN(width_c, (h->i_width >> 1) - pix_x_c);
    height = DAVS2_MIN(height, h->i_height - pix_y);
    height_c = DAVS2_MIN(height_c, (h->i_height >> 1) - pix_y_c);

    /* luma component */
    p_avail->pix_x[0] = pix_x;
    p_avail->pix_y[0] = pix_y;
    p_avail->width[0] = width;
    p_avail->height[0] = height;

    /* chroma components */
    p_avail->pix_x[1] = p_avail->pix_x[2] = pix_x_c;
    p_avail->pix_y[1] = p_avail->pix_y[2] = pix_y_c;
    p_avail->width[1] = p_avail->width[2] = width_c;
    p_avail->height[1] = p_avail->height[2] = height_c;
}

/* ---------------------------------------------------------------------------
 */
void sao_lcu(davs2_t *h, davs2_frame_t *p_tmp_frm, davs2_frame_t *p_dec_frm, int i_lcu_x, int i_lcu_y)
{
    const int width_in_lcu = h->i_width_in_lcu;
    sao_t *lcu_param = &h->lcu_infos[i_lcu_y * width_in_lcu + i_lcu_x].sao_param;

    /* copy one decoded LCU */
    davs2_frame_copy_lcu(h, p_tmp_frm, p_dec_frm, i_lcu_x, i_lcu_y, 0, 0);

    /* SAO one LCU */
    sao_region_t region;
    int comp_idx;
    sao_get_neighbor_avail(h, &region, i_lcu_x, i_lcu_y);
    for (comp_idx = 0; comp_idx < IMG_COMPONENTS; comp_idx++) {
        if (h->slice_sao_on[comp_idx] == 0 || lcu_param->planes[comp_idx].modeIdc == SAO_MODE_OFF) {
            continue;
        }

        int filter_type = lcu_param->planes[comp_idx].typeIdc;
        assert(filter_type >= SAO_TYPE_EO_0 && filter_type <= SAO_TYPE_BO);

        int pix_y = region.pix_y[comp_idx];
        int pix_x = region.pix_x[comp_idx];
        const int bit_depth = h->sample_bit_depth;
        int blkoffset = pix_y * p_dec_frm->i_stride[comp_idx] + pix_x;
        pel_t *dst = p_dec_frm->planes[comp_idx] + blkoffset;
        pel_t *src = p_tmp_frm->planes[comp_idx] + blkoffset;
        if (filter_type == SAO_TYPE_BO) {
            sao_block_bo_c(dst, p_dec_frm->i_stride[comp_idx], src, p_dec_frm->i_stride[comp_idx],
                           region.width[comp_idx], region.height[comp_idx], bit_depth, &lcu_param->planes[comp_idx]);
        } else {
            int avail[8];
            avail[0] = region.b_top;
            avail[1] = region.b_down;
            avail[2] = region.b_left;
            avail[3] = region.b_right;
            avail[4] = region.b_top_left;
            avail[5] = region.b_top_right;
            avail[6] = region.b_down_left;
            avail[7] = region.b_right_down;
            gf_davs2.sao_filter_eo[filter_type](dst, p_dec_frm->i_stride[comp_idx], src, p_dec_frm->i_stride[comp_idx],
                                                region.width[comp_idx], region.height[comp_idx],
                                                bit_depth, avail, lcu_param->planes[comp_idx].offset);
        }
    }
}

/* ---------------------------------------------------------------------------
 */
void sao_lcurow(davs2_t *h, davs2_frame_t *p_tmp_frm, davs2_frame_t *p_dec_frm, int i_lcu_y)
{
    const int width_in_lcu = h->i_width_in_lcu;
    int lcu_xy             = i_lcu_y * width_in_lcu;
    int lcu_x;

    /* copy one decoded LCU-row */
    davs2_frame_copy_lcurow(h, p_tmp_frm, p_dec_frm, i_lcu_y, -4, 0);

    /* SAO one LCU-row */
    for (lcu_x = 0; lcu_x < h->i_width_in_lcu; lcu_x++) {
        sao_region_t region;
        sao_t *lcu_param = &h->lcu_infos[lcu_xy++].sao_param;
        int comp_idx;
        sao_get_neighbor_avail(h, &region, lcu_x, i_lcu_y);
        for (comp_idx = 0; comp_idx < IMG_COMPONENTS; comp_idx++) {
            if (h->slice_sao_on[comp_idx] == 0 || lcu_param->planes[comp_idx].modeIdc == SAO_MODE_OFF){
                continue;
            }
            int filter_type = lcu_param->planes[comp_idx].typeIdc;
            assert(filter_type >= SAO_TYPE_EO_0 && filter_type <= SAO_TYPE_BO);

            int pix_y = region.pix_y[comp_idx];
            int pix_x = region.pix_x[comp_idx];
            const int bit_depth = h->sample_bit_depth;
            int blkoffset = pix_y * p_dec_frm->i_stride[comp_idx] + pix_x;
            pel_t *dst = p_dec_frm->planes[comp_idx] + blkoffset;
            pel_t *src = p_tmp_frm->planes[comp_idx] + blkoffset;
            if (filter_type == SAO_TYPE_BO) {
                gf_davs2.sao_block_bo(dst, p_dec_frm->i_stride[comp_idx], src, p_dec_frm->i_stride[comp_idx],
                                      region.width[comp_idx], region.height[comp_idx], bit_depth, &lcu_param->planes[comp_idx]);
            } else {
                int avail[8];
                avail[0] = region.b_top;
                avail[1] = region.b_down;
                avail[2] = region.b_left;
                avail[3] = region.b_right;
                avail[4] = region.b_top_left;
                avail[5] = region.b_top_right;
                avail[6] = region.b_down_left;
                avail[7] = region.b_right_down;
                gf_davs2.sao_filter_eo[filter_type](dst, p_dec_frm->i_stride[comp_idx], src, p_dec_frm->i_stride[comp_idx],
                                                    region.width[comp_idx], region.height[comp_idx],
                                                    bit_depth, avail, lcu_param->planes[comp_idx].offset);
            }
        }
    }
}

/* ---------------------------------------------------------------------------
 */
void davs2_sao_init(uint32_t cpuid, ao_funcs_t *fh)
{
    /* init c function handles */
    fh->sao_block_bo                   = sao_block_bo_c;
    fh->sao_filter_eo[SAO_TYPE_EO_0]   = sao_block_eo_0_c;
    fh->sao_filter_eo[SAO_TYPE_EO_45]  = sao_block_eo_45_c;
    fh->sao_filter_eo[SAO_TYPE_EO_90]  = sao_block_eo_90_c;
    fh->sao_filter_eo[SAO_TYPE_EO_135] = sao_block_eo_135_c;

    /* init asm function handles */
#if HAVE_MMX
    if (cpuid & DAVS2_CPU_SSE4) {
        fh->sao_block_bo                   = SAO_on_block_bo_sse128;
        fh->sao_filter_eo[SAO_TYPE_EO_0]   = SAO_on_block_eo_0_sse128;
        fh->sao_filter_eo[SAO_TYPE_EO_45]  = SAO_on_block_eo_45_sse128;
        fh->sao_filter_eo[SAO_TYPE_EO_90]  = SAO_on_block_eo_90_sse128;
        fh->sao_filter_eo[SAO_TYPE_EO_135] = SAO_on_block_eo_135_sse128;
    }
    if (cpuid & DAVS2_CPU_AVX2) {
        fh->sao_block_bo                   = SAO_on_block_bo_avx2;
        fh->sao_filter_eo[SAO_TYPE_EO_0]   = SAO_on_block_eo_0_avx2;
        fh->sao_filter_eo[SAO_TYPE_EO_45]  = SAO_on_block_eo_45_avx2;
        fh->sao_filter_eo[SAO_TYPE_EO_90]  = SAO_on_block_eo_90_avx2;
        fh->sao_filter_eo[SAO_TYPE_EO_135] = SAO_on_block_eo_135_avx2;
    }
#endif
}

