/*
 * deblock.cc
 *
 * Description of this file:
 *    Deblock functions definition of the davs2 library
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
#include "deblock.h"
#include "quant.h"

#if HAVE_MMX
#include "vec/intrinsic.h"
#endif
/* ---------------------------------------------------------------------------
 */
static const uint8_t ALPHA_TABLE[64] = {
     0,  0,  0,  0,  0,  0,  1,  1,
     1,  1,  1,  2,  2,  2,  3,  3,
     4,  4,  5,  5,  6,  7,  8,  9,
    10, 11, 12, 13, 15, 16, 18, 20,
    22, 24, 26, 28, 30, 33, 33, 35,
    35, 36, 37, 37, 39, 39, 42, 44,
    46, 48, 50, 52, 53, 54, 55, 56,
    57, 58, 59, 60, 61, 62, 63, 64
};

/* ---------------------------------------------------------------------------
 */
static const uint8_t BETA_TABLE[64] = {
     0,  0,  0,  0,  0,  0,  1,  1,
     1,  1,  1,  1,  1,  2,  2,  2,
     2,  2,  3,  3,  3,  3,  4,  4,
     4,  4,  5,  5,  5,  5,  6,  6,
     6,  7,  7,  7,  8,  8,  8,  9,
     9, 10, 10, 11, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22,
    23, 23, 24, 24, 25, 25, 26, 27
};

/* ---------------------------------------------------------------------------
 */
extern const uint8_t QP_SCALE_CR[64];

/* ---------------------------------------------------------------------------
 * edge direction for deblock
 */
enum edge_direction_e {
    EDGE_HOR = 1,           /* horizontal */
    EDGE_VER = 0            /* vertical */
};

/* ---------------------------------------------------------------------------
 * edge type for fitler control
 */
enum edge_type_e {
    EDGE_TYPE_NOFILTER  = 0,  /* no deblock filter */
    EDGE_TYPE_ONLY_LUMA = 1,  /* TU boundary in CU (chroma block does not have such boundaries) */
    EDGE_TYPE_BOTH      = 2   /* CU boundary and PU boundary */
};

/* ---------------------------------------------------------------------------
 */
static void lf_set_edge_filter_param(davs2_t *h, int i_level, int scu_x, int scu_y, int dir, int edge_type)
{
    const int w_in_scu = h->i_width_in_scu;
    // const int h_in_scu = h->i_height_in_mincu;
    int scu_num  = 1 << (i_level - MIN_CU_SIZE_IN_BIT);
    int scu_xy = scu_y * w_in_scu + scu_x;
    int i;

    if (dir == EDGE_VER) {
        /* set flag of vertical edges */
        if (scu_x == 0) {
            return;
        }

        /* Is left border Slice border?
         * check edge condition, can not filter beyond frame/slice boundaries */
        if (!h->seq_info.cross_loop_filter_flag &&
            h->scu_data[scu_xy].i_slice_nr != h->scu_data[scu_xy - 1].i_slice_nr) {
            return;
        }

        /* set filter type */
        // scu_num = DAVS2_MIN(scu_num, h_in_scu - scu_y);
        for (i = 0; i < scu_num; i++) {
            if (h->p_deblock_flag[EDGE_VER][(scu_y + i) * w_in_scu + scu_x] != EDGE_TYPE_NOFILTER) {
                break;
            }
            h->p_deblock_flag[EDGE_VER][(scu_y + i) * w_in_scu + scu_x] = (uint8_t)edge_type;
        }
    } else {
        /* set flag of horizontal edges */
        if (scu_y == 0) {
            return;
        }

        /* Is top border Slice border?
         * check edge condition, can not filter beyond frame/slice boundaries */
        if (!h->seq_info.cross_loop_filter_flag && 
            h->scu_data[scu_xy].i_slice_nr != h->scu_data[scu_xy - h->i_width_in_scu].i_slice_nr) {
            return;
        }

        /* set filter type */
        // scu_num = DAVS2_MIN(scu_num, w_in_scu - scu_x);
        for (i = 0; i < scu_num; i++) {
            if (h->p_deblock_flag[EDGE_HOR][scu_y * w_in_scu + scu_x + i] != EDGE_TYPE_NOFILTER) {
                break;
            }
            h->p_deblock_flag[EDGE_HOR][scu_y * w_in_scu + scu_x + i] = (uint8_t)edge_type;
        }
    }
}
/* ---------------------------------------------------------------------------
 */
static void lf_lcu_set_edge_filter(davs2_t *h, int i_level, int scu_x, int scu_y)
{
    const int w_in_scu = h->i_width_in_scu;
    cu_t *p_scu_data = &h->scu_data[scu_y * w_in_scu + scu_x];
    int i;

    if (p_scu_data->i_cu_level < i_level) {
        const int h_in_scu = h->i_height_in_scu;

        // 4 sub-cu
        for (i = 0; i < 4; i++) {
            int sub_cu_x = scu_x + ((i  & 1) << (i_level - MIN_CU_SIZE_IN_BIT - 1));
            int sub_cu_y = scu_y + ((i >> 1) << (i_level - MIN_CU_SIZE_IN_BIT - 1));

            if (sub_cu_x >= w_in_scu || sub_cu_y >= h_in_scu) {
                continue;       // is outside of the frame
            }

            lf_lcu_set_edge_filter(h, i_level - 1, sub_cu_x, sub_cu_y);
        }
    } else {
        // set the first left and top edge filter parameters
        lf_set_edge_filter_param(h, i_level, scu_x, scu_y, EDGE_VER, EDGE_TYPE_BOTH);  // left edge
        lf_set_edge_filter_param(h, i_level, scu_x, scu_y, EDGE_HOR, EDGE_TYPE_BOTH);  // top  edge

        // set other edge filter parameters
        if (p_scu_data->i_cu_level > B8X8_IN_BIT) {
            /* set prediction boundary */
            i = i_level - MIN_CU_SIZE_IN_BIT - 1;

            switch (p_scu_data->i_cu_type) {
                case PRED_2NxN:
                    lf_set_edge_filter_param(h, i_level, scu_x, scu_y + (1 << i), EDGE_HOR, EDGE_TYPE_BOTH);
                    break;
                case PRED_Nx2N:
                    lf_set_edge_filter_param(h, i_level, scu_x + (1 << i), scu_y, EDGE_VER, EDGE_TYPE_BOTH);
                    break;
                case PRED_I_NxN:
                    lf_set_edge_filter_param(h, i_level, scu_x + (1 << i), scu_y, EDGE_VER, EDGE_TYPE_BOTH);
                    lf_set_edge_filter_param(h, i_level, scu_x, scu_y + (1 << i), EDGE_HOR, EDGE_TYPE_BOTH);
                    break;
                case PRED_I_2Nxn:
                    if (i > 0) {
                        lf_set_edge_filter_param(h, i_level, scu_x, scu_y + (1 << (i - 1)),     EDGE_HOR, EDGE_TYPE_ONLY_LUMA);
                        lf_set_edge_filter_param(h, i_level, scu_x, scu_y + (1 << (i - 1)) * 2, EDGE_HOR, EDGE_TYPE_ONLY_LUMA);
                        lf_set_edge_filter_param(h, i_level, scu_x, scu_y + (1 << (i - 1)) * 3, EDGE_HOR, EDGE_TYPE_ONLY_LUMA);
                    } else {
                        lf_set_edge_filter_param(h, i_level, scu_x, scu_y + (1 << (i    )),     EDGE_HOR, EDGE_TYPE_ONLY_LUMA);
                    }
                    break;
                case PRED_I_nx2N:
                    if (i > 0) {
                        lf_set_edge_filter_param(h, i_level, scu_x + (1 << (i - 1)),     scu_y, EDGE_VER, EDGE_TYPE_ONLY_LUMA);
                        lf_set_edge_filter_param(h, i_level, scu_x + (1 << (i - 1)) * 2, scu_y, EDGE_VER, EDGE_TYPE_ONLY_LUMA);
                        lf_set_edge_filter_param(h, i_level, scu_x + (1 << (i - 1)) * 3, scu_y, EDGE_VER, EDGE_TYPE_ONLY_LUMA);
                    } else {
                        lf_set_edge_filter_param(h, i_level, scu_x + (1 << (i    )),     scu_y, EDGE_VER, EDGE_TYPE_ONLY_LUMA);
                    }
                    break;
                case PRED_2NxnU:
                    if (i > 0) {
                        lf_set_edge_filter_param(h, i_level, scu_x, scu_y + (1 << (i - 1)), EDGE_HOR, EDGE_TYPE_BOTH);
                    }
                    break;
                case PRED_2NxnD:
                    if (i > 0) {
                        lf_set_edge_filter_param(h, i_level, scu_x, scu_y + (1 << (i - 1)) * 3, EDGE_HOR, EDGE_TYPE_BOTH);
                    }
                    break;
                case PRED_nLx2N:
                    if (i > 0) {
                        lf_set_edge_filter_param(h, i_level, scu_x + (1 << (i - 1)), scu_y, EDGE_VER, EDGE_TYPE_BOTH);
                    }
                    break;
                case PRED_nRx2N:
                    if (i > 0) {
                        lf_set_edge_filter_param(h, i_level, scu_x + (1 << (i - 1)) * 3, scu_y, EDGE_VER, EDGE_TYPE_BOTH);
                    }
                    break;
                default:
                    // for other modes: direct/skip, 2Nx2N inter, 2Nx2N intra, no need to set
                    break;
            }

            /* set transform block boundary */
            if (p_scu_data->i_cu_type != PRED_I_NxN && p_scu_data->i_trans_size != TU_SPLIT_NON && p_scu_data->i_cbp != 0) {
                if (h->seq_info.enable_nsqt && IS_HOR_PU_PART(p_scu_data->i_cu_type)) {
                    if (p_scu_data->i_cu_level == B16X16_IN_BIT) {
                        lf_set_edge_filter_param(h, i_level, scu_x, scu_y + (1 << (i    )),                  EDGE_HOR, EDGE_TYPE_ONLY_LUMA);
                    } else {
                        lf_set_edge_filter_param(h, i_level, scu_x, scu_y + (1 << (i - 1)),                  EDGE_HOR, EDGE_TYPE_ONLY_LUMA);
                        lf_set_edge_filter_param(h, i_level, scu_x, scu_y + (1 << (i    )),                  EDGE_HOR, EDGE_TYPE_ONLY_LUMA);
                        lf_set_edge_filter_param(h, i_level, scu_x, scu_y + (1 << (i    )) + (1 << (i - 1)), EDGE_HOR, EDGE_TYPE_ONLY_LUMA);
                    }
                } else if (h->seq_info.enable_nsqt && IS_VER_PU_PART(p_scu_data->i_cu_type)) {
                    if (p_scu_data->i_cu_level == B16X16_IN_BIT) {
                        lf_set_edge_filter_param(h, i_level, scu_x + (1 << (i    )),                  scu_y, EDGE_VER, EDGE_TYPE_ONLY_LUMA);
                    } else {
                        lf_set_edge_filter_param(h, i_level, scu_x + (1 << (i - 1)),                  scu_y, EDGE_VER, EDGE_TYPE_ONLY_LUMA);
                        lf_set_edge_filter_param(h, i_level, scu_x + (1 << (i    )),                  scu_y, EDGE_VER, EDGE_TYPE_ONLY_LUMA);
                        lf_set_edge_filter_param(h, i_level, scu_x + (1 << (i    )) + (1 << (i - 1)), scu_y, EDGE_VER, EDGE_TYPE_ONLY_LUMA);
                    }
                } else {
                    lf_set_edge_filter_param(h, i_level, scu_x + (1 << i), scu_y, EDGE_VER, EDGE_TYPE_ONLY_LUMA);
                    lf_set_edge_filter_param(h, i_level, scu_x, scu_y + (1 << i), EDGE_HOR, EDGE_TYPE_ONLY_LUMA);
                }
            }
        }
    }
}

/* ---------------------------------------------------------------------------
 * return 1 if skip filtering is needed
 */
static uint8_t lf_skip_filter(davs2_t *h, cu_t *scuP, cu_t *scuQ, int dir, int block_x, int block_y)
{
    if (h->i_frame_type == AVS2_P_SLICE || h->i_frame_type == AVS2_F_SLICE) {
        const int width_in_spu = h->i_width_in_spu;
        int pos1 = block_y         * width_in_spu + block_x;
        int pos2 = (block_y - dir) * width_in_spu + (block_x - !dir);
        int ref1 = h->p_ref_idx[pos1].r[0];
        int ref2 = h->p_ref_idx[pos2].r[0];
        mv_t mv_1, mv_2;

        mv_1.v = h->p_tmv_1st[pos1].v;
        mv_2.v = h->p_tmv_1st[pos2].v;

        if ((scuP->i_cbp == 0) && (scuQ->i_cbp == 0) &&
            (DAVS2_ABS(mv_1.x - mv_2.x) < 4) &&
            (DAVS2_ABS(mv_1.y - mv_2.y) < 4) &&
            (ref1 != INVALID_REF && ref1 == ref2)) {
            return 0;
        }
    }

    return 1;
}

/* ---------------------------------------------------------------------------
 */
static void lf_edge_core(pel_t *src, int b_chroma, int ptr_inc, int inc1, int alpha, int beta, uint8_t *flt_flag)
{
    int inc2 = inc1 << 1;
    int inc3 = inc1 + inc2;
    int abs_delta;
    int L2, L1, L0, R0, R1, R2;
    int fs; // fs stands for filtering strength.  The larger fs is, the stronger filter is applied.
    int FlatnessL, FlatnessR;   // FlatnessL and FlatnessR describe how flat the curve is of one coding unit
    int flag;
    int pel;

    for (pel = 0; pel < MIN_CU_SIZE; pel++) {
        L2 = src[-inc3];
        L1 = src[-inc2];
        L0 = src[-inc1];
        R0 = src[    0];
        R1 = src[ inc1];
        R2 = src[ inc2];

        abs_delta = DAVS2_ABS(R0 - L0);
        flag = (pel < 4) ? flt_flag[0] : flt_flag[1];

        if (flag && (abs_delta < alpha) && (abs_delta > 1)) {
            FlatnessL  = (DAVS2_ABS(L1 - L0) < beta) ? 2 : 0;
            FlatnessL += (DAVS2_ABS(L2 - L0) < beta);

            FlatnessR  = (DAVS2_ABS(R0 - R1) < beta) ? 2 : 0;
            FlatnessR += (DAVS2_ABS(R0 - R2) < beta);

            switch (FlatnessL + FlatnessR) {
            case 6:
                fs = 3 + ((R1 == R0) && (L0 == L1));  // ((R1 == R0) && (L0 == L1)) ? 4 : 3;
                break;
            case 5:
                fs = 2 + ((R1 == R0) && (L0 == L1));  // ((R1 == R0) && (L0 == L1)) ? 3 : 2;
                break;
            case 4:
                fs = 1 + (FlatnessL == 2);            // (FlatnessL == 2) ? 2 : 1;
                break;
            case 3:
                fs = (DAVS2_ABS(L1 - R1) < beta);
                break;
            default:
                fs = 0;
            }

            fs -= (b_chroma && fs > 0);

            switch (fs) {
            case 4:
                src[-inc1] = (pel_t)((L0 + ((L0 + L2) << 3) + L2 + (R0 << 3) + (R2 << 2) + (R2 << 1) + 16) >> 5); // L0
                src[-inc2] = (pel_t)(((L0 << 3) - L0 + (L2 << 2) + (L2 << 1) + R0 + (R0 << 1) + 8) >> 4);         // L1
                src[-inc3] = (pel_t)(((L0 << 2) + L2 + (L2 << 1) + R0 + 4) >> 3);                                 // L2
                src[    0] = (pel_t)((R0 + ((R0 + R2) << 3) + R2 + (L0 << 3) + (L2 << 2) + (L2 << 1) + 16) >> 5); // R0
                src[ inc1] = (pel_t)(((R0 << 3) - R0 + (R2 << 2) + (R2 << 1) + L0 + (L0 << 1) + 8) >> 4);         // R1
                src[ inc2] = (pel_t)(((R0 << 2) + R2 + (R2 << 1) + L0 + 4) >> 3);                                 // R2
                break;
            case 3:
                src[-inc1] = (pel_t)((L2 + (L1 << 2) + (L0 << 2) + (L0 << 1) + (R0 << 2) + R1 + 8) >> 4);         // L0
                src[    0] = (pel_t)((L1 + (L0 << 2) + (R0 << 2) + (R0 << 1) + (R1 << 2) + R2 + 8) >> 4);         // R0
                src[-inc2] = (pel_t)((L2 * 3 + L1 * 8 + L0 * 4 + R0 + 8) >> 4);
                src[ inc1] = (pel_t)((R2 * 3 + R1 * 8 + R0 * 4 + L0 + 8) >> 4);
                break;
            case 2:
                src[-inc1] = (pel_t)(((L1 << 1) + L1 + (L0 << 3) + (L0 << 1) + (R0 << 1) + R0 + 8) >> 4);
                src[    0] = (pel_t)(((L0 << 1) + L0 + (R0 << 3) + (R0 << 1) + (R1 << 1) + R1 + 8) >> 4);
                break;
            case 1:
                src[-inc1] = (pel_t)((L0 * 3 + R0 + 2) >> 2);
                src[    0] = (pel_t)((R0 * 3 + L0 + 2) >> 2);
                break;
            default:
                break;
            }
        }

        src += ptr_inc;     // next row or column
        pel += b_chroma;
    }
}

/* ---------------------------------------------------------------------------
 */
static void deblock_edge_hor(pel_t *src, int stride, int alpha, int beta, uint8_t *flt_flag)
{
    lf_edge_core(src, 0, 1, stride, alpha, beta, flt_flag);
}

/* ---------------------------------------------------------------------------
 */
static void deblock_edge_ver(pel_t *src, int stride, int alpha, int beta, uint8_t *flt_flag)
{
    lf_edge_core(src, 0, stride, 1, alpha, beta, flt_flag);
}

/* ---------------------------------------------------------------------------
 */
#if HDR_CHROMA_DELTA_QP
static void deblock_edge_ver_c(pel_t *src_u, pel_t *src_v, int stride, int *alpha, int *beta, uint8_t *flt_flag)
#else
static void deblock_edge_ver_c(pel_t *src_u, pel_t *src_v, int stride, int alpha, int beta, uint8_t *flt_flag)
#endif
{
#if HDR_CHROMA_DELTA_QP
    lf_edge_core(src_u, 1, stride, 1, alpha[0], beta[0], flt_flag);
    lf_edge_core(src_v, 1, stride, 1, alpha[1], beta[1], flt_flag);
#else
    lf_edge_core(src_u, 1, stride, 1, alpha, beta, flt_flag);
    lf_edge_core(src_v, 1, stride, 1, alpha, beta, flt_flag);
#endif
}

/* ---------------------------------------------------------------------------
 */
#if HDR_CHROMA_DELTA_QP
static void deblock_edge_hor_c(pel_t *src_u, pel_t *src_v, int stride, int *alpha, int *beta, uint8_t *flt_flag)
#else
static void deblock_edge_hor_c(pel_t *src_u, pel_t *src_v, int stride, int alpha, int beta, uint8_t *flt_flag)
#endif
{
#if HDR_CHROMA_DELTA_QP
    lf_edge_core(src_u, 1, 1, stride, alpha[0], beta[0], flt_flag);
    lf_edge_core(src_v, 1, 1, stride, alpha[1], beta[1], flt_flag);
#else
    lf_edge_core(src_u, 1, 1, stride, alpha, beta, flt_flag);
    lf_edge_core(src_v, 1, 1, stride, alpha, beta, flt_flag);
#endif
}

/* ---------------------------------------------------------------------------
 * deblock one coding unit
 */
static void lf_scu_deblock(davs2_t *h, pel_t *p_dec[3], int stride, int stride_c, int scu_x, int scu_y, int dir)
{
    static const int max_qp_deblock = 63;
    const int scu_xy   = scu_y * h->i_width_in_scu + scu_x;
    cu_t     *scuQ     = &h->scu_data[scu_xy];
    int edge_condition = h->p_deblock_flag[dir][scu_xy];

    /* deblock edges */
    if (edge_condition != EDGE_TYPE_NOFILTER) {
        const int shift = h->sample_bit_depth - 8;
        cu_t  *scuP  = (dir) ? (scuQ - h->i_width_in_scu) : (scuQ - 1);
        uint8_t b_filter_flag[2];
        int QP;

        b_filter_flag[0] = lf_skip_filter(h, scuP, scuQ, dir, (scu_x << 1),       (scu_y << 1)       );
        b_filter_flag[1] = lf_skip_filter(h, scuP, scuQ, dir, (scu_x << 1) + dir, (scu_y << 1) + !dir);

        if (!b_filter_flag[0] && !b_filter_flag[1]) {
            return;  // 如果两个8x4都跳过滤波，则不需要继续调用后面的函数
        }

        /* deblock luma edge */
        {
            pel_t *src_y = p_dec[0] + (scu_y << MIN_CU_SIZE_IN_BIT) * stride + (scu_x << MIN_CU_SIZE_IN_BIT);
            int alpha, beta;
            QP = ((scuP->i_qp + scuQ->i_qp + 1) >> 1);  // average QP of the two blocks

            /* coded as 10/12 bit, QP is added by (8 * (h->param.sample_bit_depth - 8)) in config file */
            alpha = ALPHA_TABLE[DAVS2_CLIP3(0, max_qp_deblock, QP - (shift << 3) + h->i_alpha_offset)] << shift;
            beta  = BETA_TABLE [DAVS2_CLIP3(0, max_qp_deblock, QP - (shift << 3) + h->i_beta_offset )] << shift;

            gf_davs2.deblock_luma[dir](src_y, stride, alpha, beta, b_filter_flag);
        }

        /* deblock chroma edge */
        if (edge_condition == EDGE_TYPE_BOTH && h->i_chroma_format != CHROMA_400)
        if (((scu_y & 1) == 0 && dir) || (((scu_x & 1) == 0) && (!dir))) {
            int uv_offset = (scu_y << (MIN_CU_SIZE_IN_BIT - 1)) * stride_c + (scu_x << (MIN_CU_SIZE_IN_BIT - 1));
            pel_t *src_u = p_dec[1] + uv_offset;
            pel_t *src_v = p_dec[2] + uv_offset;
#if HDR_CHROMA_DELTA_QP
            int alpha[2], beta[2];
            int luma_qp = QP;
            int offset = shift << 3;
            /* coded as 10/12 bit, QP is added by (8 * (h->param.sample_bit_depth - 8)) in config file */
            QP = cu_get_chroma_qp(h, luma_qp, 0) - offset;
            alpha[0] = ALPHA_TABLE[DAVS2_CLIP3(0, max_qp_deblock, QP + h->i_alpha_offset)] << shift;
            beta[0]  = BETA_TABLE [DAVS2_CLIP3(0, max_qp_deblock, QP + h->i_beta_offset )] << shift;

            QP = cu_get_chroma_qp(h, luma_qp, 1) - offset;
            alpha[1] = ALPHA_TABLE[DAVS2_CLIP3(0, max_qp_deblock, QP + h->i_alpha_offset)] << shift;
            beta[1]  = BETA_TABLE [DAVS2_CLIP3(0, max_qp_deblock, QP + h->i_beta_offset )] << shift;

            gf_davs2.deblock_chroma[dir](src_u, src_v, stride_c, alpha, beta, b_filter_flag);
#else
            int alpha, beta;

            /* coded as 10/12 bit, QP is added by (8 * (h->param.sample_bit_depth - 8)) in config file */
            QP = cu_get_chroma_qp(h, QP, 0) - (shift << 3);
            alpha = ALPHA_TABLE[DAVS2_CLIP3(0, max_qp_deblock, QP + h->i_alpha_offset)] << shift;
            beta = BETA_TABLE[DAVS2_CLIP3(0, max_qp_deblock, QP + h->i_beta_offset)] << shift;

            gf_davs2.deblock_chroma[dir](src_u, src_v, stride_c, alpha, beta, b_filter_flag);
#endif
        }
    }
}

/**
 * ===========================================================================
 * interface function defines
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 * NOTE: only support I420 now
 */
void davs2_lcu_deblock(davs2_t *h, davs2_frame_t *frm, int i_lcu_x, int i_lcu_y)
{
    const int i_stride   = frm->i_stride[0];
    const int i_stride_c = frm->i_stride[1];
    const int w_in_scu   = h->i_width_in_scu;
    const int h_in_scu   = h->i_height_in_scu;
    const int num_in_scu = 1 << (h->i_lcu_level - MIN_CU_SIZE_IN_BIT);
    int scu_x            = i_lcu_x << (h->i_lcu_level - MIN_CU_SIZE_IN_BIT);
    int scu_y            = i_lcu_y << (h->i_lcu_level - MIN_CU_SIZE_IN_BIT);
    int num_of_scu_hor   = DAVS2_MIN(w_in_scu - scu_x, num_in_scu);
    int num_of_scu_ver   = DAVS2_MIN(h_in_scu - scu_y, num_in_scu);
    int i, j;

    /* -------------------------------------------------------------
     * init
     */

    /* set edge flags in one LCU */
    lf_lcu_set_edge_filter(h, h->i_lcu_level, scu_x, scu_y);

    /* -------------------------------------------------------------
     * vertical
     */

    /* deblock all vertical edges in one LCU */
    for (j = 0; j < num_of_scu_ver; j++) {
        for (i = 0; i < num_of_scu_hor; i++) {
            lf_scu_deblock(h, frm->planes, i_stride, i_stride_c, scu_x + i, scu_y + j, EDGE_VER);
        }
    }

    /* -------------------------------------------------------------
     * horizontal
     */

    /* adjust the value of scu_x and num_of_scu_hor */
    if (scu_x == 0) {
        /* the current LCU is the first LCU in a LCU row */
        num_of_scu_hor--; /* leave the last horizontal edge */
    } else {
        /* the current LCU is one of the rest LCUs in a row */
        if (scu_x + num_of_scu_hor == w_in_scu) {
            /* the current LCU is the last LCUs in a row,
             * need deblock one horizontal edge more */
            num_of_scu_hor++;
        }
        scu_x--;        /* begin from the last horizontal edge of previous LCU */
    }

    /* deblock all horizontal edges in one LCU */
    for (j = 0; j < num_of_scu_ver; j++) {
        for (i = 0; i < num_of_scu_hor; i++) {
            lf_scu_deblock(h, frm->planes, i_stride, i_stride_c, scu_x + i, scu_y + j, EDGE_HOR);
        }
    }
}

/* ---------------------------------------------------------------------------
 * init deblock function handles
 */
void davs2_deblock_init(uint32_t cpuid, ao_funcs_t* fh)
{
    UNUSED_PARAMETER(cpuid);

    fh->deblock_luma  [0] = deblock_edge_ver;
    fh->deblock_luma  [1] = deblock_edge_hor;
    fh->deblock_chroma[0] = deblock_edge_ver_c;
    fh->deblock_chroma[1] = deblock_edge_hor_c;

    fh->set_deblock_const = NULL;

    /* init asm function handles */
#if HAVE_MMX
    if ((cpuid & DAVS2_CPU_SSE4) && !HDR_CHROMA_DELTA_QP) {
#if !HIGH_BIT_DEPTH
        fh->deblock_luma  [0] = deblock_edge_ver_sse128;
        fh->deblock_luma  [1] = deblock_edge_hor_sse128;
        fh->deblock_chroma[0] = deblock_edge_ver_c_sse128;
        fh->deblock_chroma[1] = deblock_edge_hor_c_sse128;
#endif
    }
    if ((cpuid & DAVS2_CPU_AVX2) && !HDR_CHROMA_DELTA_QP) {
#if !HIGH_BIT_DEPTH
        // fh->deblock_luma[0] = deblock_edge_ver_avx2;  // @luofl i7-6700K 此函数慢于 sse128
        // fh->deblock_luma[1] = deblock_edge_hor_avx2;
        // fh->deblock_chroma[0] = deblock_edge_ver_c_avx2;
        // fh->deblock_chroma[1] = deblock_edge_hor_c_avx2;

#endif
    }
#endif  // HAVE_MMX
}
