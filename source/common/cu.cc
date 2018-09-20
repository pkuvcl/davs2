/*
 * cu.cc
 *
 * Description of this file:
 *    CU Processing functions definition of the davs2 library
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
#include "cu.h"
#include "vlc.h"
#include "transform.h"
#include "intra.h"
#include "predict.h"
#include "block_info.h"
#include "aec.h"
#include "mc.h"
#include "sao.h"
#include "quant.h"
#include "scantab.h"

/**
 * ===========================================================================
 * local & global variables (const tables)
 * ===========================================================================
 */

static const int tab_b8xy_to_zigzag[8][8] = {
    {  0,  1,  4,  5, 16, 17, 20, 21 },
    {  2,  3,  6,  7, 18, 19, 22, 23 },
    {  8,  9, 12, 13, 24, 25, 28, 29 },
    { 10, 11, 14, 15, 26, 27, 30, 31 },
    { 32, 33, 36, 37, 48, 49, 52, 53 },
    { 34, 35, 38, 39, 50, 51, 54, 55 },
    { 40, 41, 44, 45, 56, 57, 60, 61 },
    { 42, 43, 46, 47, 58, 59, 62, 63 }
};

/* ---------------------------------------------------------------------------
 */
const uint8_t QP_SCALE_CR[64] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
    40, 41, 42, 42, 43, 43, 44, 44, 45, 45,
    46, 46, 47, 47, 48, 48, 48, 49, 49, 49,
    50, 50, 50, 51,
};

/* ---------------------------------------------------------------------------
 */
static const int8_t dmh_pos[DMH_MODE_NUM + DMH_MODE_NUM - 1][2][2] = {
    { {  0,  0 }, { 0,  0 } },
    { { -1,  0 }, { 1,  0 } },
    { {  0, -1 }, { 0,  1 } },
    { { -1,  1 }, { 1, -1 } },
    { { -1, -1 }, { 1,  1 } },
    { { -2,  0 }, { 2,  0 } },
    { {  0, -2 }, { 0,  2 } },
    { { -2,  2 }, { 2, -2 } },
    { { -2, -2 }, { 2,  2 } }
};

/* ---------------------------------------------------------------------------
 */
const int16_t IQ_SHIFT[80] = {
    15, 15, 15, 15, 15, 15, 15, 15,
    14, 14, 14, 14, 14, 14, 14, 14,
    14, 13, 13, 13, 13, 13, 13, 13,
    12, 12, 12, 12, 12, 12, 12, 12,
    12, 11, 11, 11, 11, 11, 11, 11,
    11, 10, 10, 10, 10, 10, 10, 10,
    10,  9,  9,  9,  9,  9,  9,  9,
     8,  8,  8,  8,  8,  8,  8,  8,
     7,  7,  7,  7,  7,  7,  7,  7,
     6,  6,  6,  6,  6,  6,  6,  6
};

/* ---------------------------------------------------------------------------
 */
const uint16_t IQ_TAB[80] = {
    32768, 36061, 38968, 42495, 46341, 50535, 55437, 60424,
    32932, 35734, 38968, 42495, 46177, 50535, 55109, 59933,
    65535, 35734, 38968, 42577, 46341, 50617, 55027, 60097,
    32809, 35734, 38968, 42454, 46382, 50576, 55109, 60056,
    65535, 35734, 38968, 42495, 46320, 50515, 55109, 60076,
    65535, 35744, 38968, 42495, 46341, 50535, 55099, 60087,
    65535, 35734, 38973, 42500, 46341, 50535, 55109, 60097,
    32771, 35734, 38965, 42497, 46341, 50535, 55109, 60099,
    32768, 36061, 38968, 42495, 46341, 50535, 55437, 60424,
    32932, 35734, 38968, 42495, 46177, 50535, 55109, 59933
};

#if AVS2_TRACE
extern int symbolCount;
#endif

/**
 * ===========================================================================
 * function defines
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 * used for debug
 */
static INLINE
bool_t is_inside_cu(int cu_pix_x, int cu_pix_y, int i_cu_level, int i_pix_x, int i_pix_y)
{
    int cu_size = 1 << i_cu_level;
    return cu_pix_x <= i_pix_x && (cu_pix_x + cu_size) > i_pix_x &&
        cu_pix_y <= i_pix_y && (cu_pix_y + cu_size) > i_pix_y;
}

/* ---------------------------------------------------------------------------
 * obtain the pos and size of prediction units (PUs)
 */
static ALWAYS_INLINE
void cu_init_prediction_units(davs2_t *h, cu_t *p_cu)
{
    /* ---------------------------------------------------------------------------
    */
    static const int NUM_PREDICTION_UNIT[MAX_PRED_MODES] = {// [mode]
        1, // 0: 8x8, ---, ---, --- (PRED_SKIP   )
        1, // 1: 8x8, ---, ---, --- (PRED_2Nx2N  )
        2, // 2: 8x4, 8x4, ---, --- (PRED_2NxN   )
        2, // 3: 4x8, 4x8, ---, --- (PRED_Nx2N   )
        2, // 4: 8x2, 8x6, ---, --- (PRED_2NxnU  )
        2, // 5: 8x6, 8x2, ---, --- (PRED_2NxnD  )
        2, // 6: 2x8, 6x8, ---, --- (PRED_nLx2N  )
        2, // 7: 6x8, 2x8, ---, --- (PRED_nRx2N  )
        1, // 8: 8x8, ---, ---, --- (PRED_I_2Nx2N)
        4, // 9: 4x4, 4x4, 4x4, 4x4 (PRED_I_NxN  )
        4, //10: 8x2, 8x2, 8x2, 8x2 (PRED_I_2Nxn )
        4  //11: 2x8, 2x8, 2x8, 2x8 (PRED_I_nx2N )
    };

    static const cb_t CODING_BLOCK_INFO[MAX_PRED_MODES + 1][4] = {// [mode][block]
        //  x, y, w, h      x, y, w, h      x, y, w, h      x, y, w, h for block 0, 1, 2 and 3
        {{{0, 0, 8, 8}}, {{0, 0, 0, 0}}, {{0, 0, 0, 0}}, {{0, 0, 0, 0}}}, // 0: 8x8, ---, ---, --- (PRED_SKIP   )
        {{{0, 0, 8, 8}}, {{0, 0, 0, 0}}, {{0, 0, 0, 0}}, {{0, 0, 0, 0}}}, // 1: 8x8, ---, ---, --- (PRED_2Nx2N  )
        {{{0, 0, 8, 4}}, {{0, 4, 8, 4}}, {{0, 0, 0, 0}}, {{0, 0, 0, 0}}}, // 2: 8x4, 8x4, ---, --- (PRED_2NxN   )
        {{{0, 0, 4, 8}}, {{4, 0, 4, 8}}, {{0, 0, 0, 0}}, {{0, 0, 0, 0}}}, // 3: 4x8, 4x8, ---, --- (PRED_Nx2N   )
        {{{0, 0, 8, 2}}, {{0, 2, 8, 6}}, {{0, 0, 0, 0}}, {{0, 0, 0, 0}}}, // 4: 8x2, 8x6, ---, --- (PRED_2NxnU  )
        {{{0, 0, 8, 6}}, {{0, 6, 8, 2}}, {{0, 0, 0, 0}}, {{0, 0, 0, 0}}}, // 5: 8x6, 8x2, ---, --- (PRED_2NxnD  )
        {{{0, 0, 2, 8}}, {{2, 0, 6, 8}}, {{0, 0, 0, 0}}, {{0, 0, 0, 0}}}, // 6: 2x8, 6x8, ---, --- (PRED_nLx2N  )
        {{{0, 0, 6, 8}}, {{6, 0, 2, 8}}, {{0, 0, 0, 0}}, {{0, 0, 0, 0}}}, // 7: 6x8, 2x8, ---, --- (PRED_nRx2N  )
        {{{0, 0, 8, 8}}, {{0, 0, 0, 0}}, {{0, 0, 0, 0}}, {{0, 0, 0, 0}}}, // 8: 8x8, ---, ---, --- (PRED_I_2Nx2N)
        {{{0, 0, 4, 4}}, {{4, 0, 4, 4}}, {{0, 4, 4, 4}}, {{4, 4, 4, 4}}}, // 9: 4x4, 4x4, 4x4, 4x4 (PRED_I_NxN  )
        {{{0, 0, 8, 2}}, {{0, 2, 8, 2}}, {{0, 4, 8, 2}}, {{0, 6, 8, 2}}}, //10: 8x2, 8x2, 8x2, 8x2 (PRED_I_2Nxn )
        {{{0, 0, 2, 8}}, {{2, 0, 2, 8}}, {{4, 0, 2, 8}}, {{6, 0, 2, 8}}}, //11: 2x8, 2x8, 2x8, 2x8 (PRED_I_nx2N )
        {{{0, 0, 4, 4}}, {{4, 0, 4, 4}}, {{0, 4, 4, 4}}, {{4, 4, 4, 4}}}, // X: 4x4, 4x4, 4x4, 4x4
    };

    const int i_level = p_cu->i_cu_level;
    const int i_mode = p_cu->i_cu_type;
    const int shift_bits = i_level - MIN_CU_SIZE_IN_BIT;
    const int block_num = NUM_PREDICTION_UNIT[i_mode];
    int ds_mode = p_cu->i_md_directskip_mode;
    int i;
    cb_t *p_cb = p_cu->pu;

    // memset(p_cb, 0, 4 * sizeof(cb_t));

    // set for each block
    if (i_mode == PRED_SKIP) {
        ///! 一些特殊的Skip/Direct模式下如果CU超过8x8，则PU划分成4个
        if (i_level > 3 &&
            (h->i_frame_type == AVS2_P_SLICE
            || (h->i_frame_type == AVS2_F_SLICE && ds_mode == DS_NONE)
            || (h->i_frame_type == AVS2_B_SLICE && ds_mode == DS_NONE))) {
            p_cu->num_pu = 4;
            for (i = 0; i < 4; i++) {
                p_cb[i].v = CODING_BLOCK_INFO[PRED_I_nx2N + 1][i].v << shift_bits;
            }
        } else {
            p_cu->num_pu = 1;
            p_cb[0].v = CODING_BLOCK_INFO[PRED_SKIP][0].v << shift_bits;
        }
    } else {
        p_cu->num_pu = (int8_t)block_num;
        for (i = 0; i < block_num; i++) {
            p_cb[i].v = CODING_BLOCK_INFO[i_mode][i].v << shift_bits;
        }
    }
}


/* ---------------------------------------------------------------------------
 * obtain the pos and size of transform units (TUs)
 */
static ALWAYS_INLINE
void cu_init_transform_units(cu_t *p_cu, cb_t *p_tu)
{
    static const cb_t TU_SPLIT_INFO[TU_SPLIT_CROSS+1][4] = {// [mode][block]
        //  x, y, w, h      x, y, w, h      x, y, w, h      x, y, w, h for block 0, 1, 2 and 3
        {{{0, 0, 8, 8}}, {{0, 0, 0, 0}}, {{0, 0, 0, 0}}, {{0, 0, 0, 0}}}, // TU_SPLIT_NON
        {{{0, 0, 8, 2}}, {{0, 2, 8, 2}}, {{0, 4, 8, 2}}, {{0, 6, 8, 2}}}, // TU_SPLIT_HOR
        {{{0, 0, 2, 8}}, {{2, 0, 2, 8}}, {{4, 0, 2, 8}}, {{6, 0, 2, 8}}}, // TU_SPLIT_VER
        {{{0, 0, 4, 4}}, {{4, 0, 4, 4}}, {{0, 4, 4, 4}}, {{4, 4, 4, 4}}}, // TU_SPLIT_CROSS
    };

    const int shift_bits = p_cu->i_cu_level - MIN_CU_SIZE_IN_BIT;
    const int i_tu_type = p_cu->i_trans_size;

    p_tu[0].v = TU_SPLIT_INFO[i_tu_type][0].v << shift_bits;
    p_tu[1].v = TU_SPLIT_INFO[i_tu_type][1].v << shift_bits;
    p_tu[2].v = TU_SPLIT_INFO[i_tu_type][2].v << shift_bits;
    p_tu[3].v = TU_SPLIT_INFO[i_tu_type][3].v << shift_bits;
}

/* ---------------------------------------------------------------------------
 * get neighboring MVs for MVP
 */
static void cu_get_neighbors(davs2_t *h, cu_t *p_cu, int pix_x, int pix_y, int bsx, int bsy)
{
    neighbor_inter_t *neighbors = h->lcu.neighbor_inter;
    int cur_slice_idx = p_cu->i_slice_nr;
    int x0 = (pix_x >> MIN_PU_SIZE_IN_BIT);
    int y0 = (pix_y >> MIN_PU_SIZE_IN_BIT);
    int x1 = (bsx   >> MIN_PU_SIZE_IN_BIT) + x0 - 1;
    int y1 = (bsy   >> MIN_PU_SIZE_IN_BIT) + y0 - 1;

    /* 1. check whether the top-right 4x4 block is reconstructed */
    int x_top_right_4x4_in_lcu = x1 - h->lcu.i_spu_x;
    int y_top_right_4x4_in_lcu = y0 - h->lcu.i_spu_y;
    int block_available_TR = h->p_tab_TR_avail[(y_top_right_4x4_in_lcu << (h->i_lcu_level - B4X4_IN_BIT)) + x_top_right_4x4_in_lcu];

    /* 2. get neighboring blocks */
    cu_get_neighbor_spatial(h, cur_slice_idx, &neighbors[BLK_LEFT    ], x0 - 1, y0    );
    cu_get_neighbor_spatial(h, cur_slice_idx, &neighbors[BLK_TOP     ], x0    , y0 - 1);
    cu_get_neighbor_spatial(h, cur_slice_idx, &neighbors[BLK_TOP2    ], x1    , y0 - 1);
    cu_get_neighbor_spatial(h, cur_slice_idx, &neighbors[BLK_TOPLEFT ], x0 - 1, y0 - 1);
    cu_get_neighbor_spatial(h, cur_slice_idx, &neighbors[BLK_LEFT2   ], x0 - 1, y1    );

    cu_get_neighbor_spatial(h, cur_slice_idx, &neighbors[BLK_TOPRIGHT], block_available_TR ? x1 + 1 : -1, y0 - 1);

    cu_get_neighbor_temporal(h, &neighbors[BLK_COLLOCATED], x0, y0);
}

/* ---------------------------------------------------------------------------
 */
static INLINE
void cu_init(davs2_t *h, cu_t *p_cu, int i_level, int scu_xy, int pix_x)
{
    assert(scu_xy >= 0 && scu_xy < h->i_size_in_scu);

    // reset syntax element entries in cu_t
    p_cu->i_cu_level    = (int8_t)i_level;
    p_cu->i_qp          = (int8_t)h->i_qp;
    p_cu->i_cu_type     = PRED_SKIP;
    p_cu->i_cbp         = 0;
    p_cu->c_ipred_mode  = DC_PRED_C;
    p_cu->i_dmh_mode    = 0;
    memset(p_cu->dct_pattern, 0, sizeof(p_cu->dct_pattern));

    // check left CU
    h->lcu.i_left_cu_qp     = (int8_t)h->i_qp;
    h->lcu.c_ipred_mode_ctx = 0;

    if (pix_x > 0) {
        cu_t *p_left_cu = &h->scu_data[scu_xy - 1];

        if (p_left_cu->i_slice_nr == p_cu->i_slice_nr) {
            h->lcu.c_ipred_mode_ctx = p_left_cu->c_ipred_mode != 0;
            h->lcu.i_left_cu_qp     = p_left_cu->i_qp;
        }
    }
}

/* ---------------------------------------------------------------------------
 */
static INLINE
void cu_read_end(davs2_t *h, cu_t *p_cu, int i_level, int scu_xy)
{
    cu_t *p_cu_iter = &h->scu_data[scu_xy];
    int size_in_scu = 1 << (i_level - MIN_CU_SIZE_IN_BIT);
    int i;

    if (size_in_scu <= 1) {
        return;
    }

    /* the fist row */
    for (i = 1; i < size_in_scu; i++) {
        memcpy(p_cu_iter + i, p_cu, sizeof(cu_t));
    }

    /* the left rows */
    for (i = 1; i < size_in_scu; i++) {
        p_cu_iter += h->i_width_in_scu;
        memcpy(p_cu_iter, p_cu, size_in_scu * sizeof(cu_t));
    }
}

/* ---------------------------------------------------------------------------
 */
static int cu_read_intrapred_mode_luma(davs2_t *h, aec_t *p_aec, cu_t *p_cu, int b8, int bi, int bj)
{
    int size_in_scu = 1 << (p_cu->i_cu_level - MIN_CU_SIZE_IN_BIT);
    int i_intramode = h->i_ipredmode;
    int8_t *p_intramode = h->p_ipredmode + bj * i_intramode + bi;
    int intra_mode_top  = p_intramode[-i_intramode];
    int intra_mode_left = p_intramode[-1];
    int luma_mode = aec_read_intra_pmode(p_aec);
    int mpm[2];
    int8_t real_luma_mode;

#if AVS2_TRACE
    strncpy(p_aec->tracestring, "Ipred Mode", TRACESTRING_SIZE);
#endif
    assert(IS_INTRA(p_cu) && b8 < 4 && b8 >= 0);

    AEC_RETURN_ON_ERROR(-1);

    mpm[0] = DAVS2_MIN(intra_mode_top, intra_mode_left);
    mpm[1] = DAVS2_MAX(intra_mode_top, intra_mode_left);

    if (mpm[0] == mpm[1]) {
        mpm[0] = DC_PRED;
        mpm[1] = (mpm[1] == DC_PRED) ? BI_PRED : mpm[1];
    }

    real_luma_mode = (int8_t)((luma_mode < 0) ? mpm[luma_mode + 2] : luma_mode + (luma_mode >= mpm[0]) + (luma_mode + 1 >= mpm[1]));

    if (real_luma_mode < 0 || real_luma_mode >= NUM_INTRA_MODE) {
        davs2_log(h, DAVS2_LOG_ERROR, "invalid pred mode %2d. POC %3d, pixel (%3d, %3d), %2dx%2d",
                 real_luma_mode, h->i_poc, bi << MIN_PU_SIZE_IN_BIT, bj << MIN_PU_SIZE_IN_BIT,
                 size_in_scu << MIN_CU_SIZE_IN_BIT, size_in_scu << MIN_CU_SIZE_IN_BIT);
        real_luma_mode = (int8_t)davs2_clip3(real_luma_mode, 0, NUM_INTRA_MODE - 1);
    }
    p_cu->intra_pred_modes[b8] = real_luma_mode;

    // store intra prediction mode, for MPM of next blocks
    {
        int w_4x4 = size_in_scu << 1;
        int h_4x4 = size_in_scu << 1;
        int j;

        switch (p_cu->i_trans_size) {
        case TU_SPLIT_HOR:
            h_4x4 >>= 2;
            break;
        case TU_SPLIT_VER:
            w_4x4 >>= 2;
            break;
        case TU_SPLIT_CROSS:
            w_4x4 >>= 1;
            h_4x4 >>= 1;
            break;
        }

        for (j = 0; j < h_4x4; j++) {
            int i = (j == h_4x4 - 1) ? 0 : w_4x4 - 1;
            for (; i < w_4x4; i++) {
                p_intramode[i] = real_luma_mode;
            }
            p_intramode += i_intramode;
        }
    }

    return 0;
}

/* ---------------------------------------------------------------------------
 */
static
void cu_store_references(davs2_t *h, cu_t *p_cu, int pix_x, int pix_y)
{
    int width_in_spu = h->i_width_in_spu;
    int block8_y = pix_y >> MIN_PU_SIZE_IN_BIT;
    int block8_x = pix_x >> MIN_PU_SIZE_IN_BIT;
    int idx_pu;

    for (idx_pu = 0; idx_pu < p_cu->num_pu; idx_pu++) {
        ref_idx_t *p_ref_1st;
        int8_t    *p_dirpred;
        int8_t     i_dir_pred;
        ref_idx_t ref_idx;
        int b8_x, b8_y;
        int r, c;
        cb_t pu;

        pu.v = p_cu->pu[idx_pu].v >> 2;

        b8_x = block8_x + pu.x;
        b8_y = block8_y + pu.y;

        i_dir_pred = (int8_t)p_cu->b8pdir[idx_pu];
        ref_idx    = p_cu->ref_idx[idx_pu];
        p_dirpred  = h->p_dirpred + b8_y * width_in_spu + b8_x;
        p_ref_1st  = h->p_ref_idx + b8_y * width_in_spu + b8_x;

        for (r = pu.h; r != 0; r--) {
            for (c = 0; c < pu.w; c++) {
                p_ref_1st[c] = ref_idx;
                p_dirpred[c] = i_dir_pred;
            }
            p_ref_1st += width_in_spu;
            p_dirpred += width_in_spu;
        }
    }
}

/* ---------------------------------------------------------------------------
 */
static int cu_read_mv(davs2_t *h, aec_t *p_aec, int i_level, int scu_xy, int pix_x, int pix_y)
{
    cu_t *p_cu = &h->scu_data[scu_xy];
    int bframe = (h->i_frame_type == AVS2_B_SLICE);
    int idx_pu;
    int block8_y = pix_y >> MIN_PU_SIZE_IN_BIT;
    int block8_x = pix_x >> MIN_PU_SIZE_IN_BIT;
    int width_in_spu = h->i_width_in_spu;
    int distance_fwd;
    int distance_fwd_src;
    int distance_bwd;  // TODO: 非 B FRAME 情况的初始值？

    assert(p_cu->i_cu_type != PRED_SKIP);

    if (h->i_frame_type == AVS2_F_SLICE && /*h->b_dmh &&*/
        p_cu->b8pdir[0] == PDIR_FWD && p_cu->b8pdir[1] == PDIR_FWD &&
        p_cu->b8pdir[2] == PDIR_FWD && p_cu->b8pdir[3] == PDIR_FWD) {
        //has forward vector
        if (!(i_level == B8X8_IN_BIT && p_cu->i_cu_type >= PRED_2NxN && p_cu->i_cu_type <= PRED_nRx2N)) {
            p_cu->i_dmh_mode = (int8_t)aec_read_dmh_mode(p_aec, p_cu->i_cu_level);
            AEC_RETURN_ON_ERROR(-1);
#if AVS2_TRACE
            avs2_trace("dmh_mode = %3d\n", p_cu->i_dmh_mode);
#endif
        } else {
            p_cu->i_dmh_mode = 0;
        }
    }

    //=====  READ PDIR_FWD MOTION VECTORS =====
    for (idx_pu = 0; idx_pu < p_cu->num_pu; idx_pu++) {
        if (p_cu->b8pdir[idx_pu] != PDIR_BWD) {
            int pu_pix_x = p_cu->pu[idx_pu].x;
            int pu_pix_y = p_cu->pu[idx_pu].y;
            int bsx      = p_cu->pu[idx_pu].w;
            int bsy      = p_cu->pu[idx_pu].h;
            int i8       = block8_x + (pu_pix_x >> 2);
            int j8       = block8_y + (pu_pix_y >> 2);
            int refframe = h->p_ref_idx[j8 * width_in_spu + i8].r[0];
            mv_t mv, mvp;
            int ii, jj;

            // first make mv-prediction
            int pu_mvp_type = get_pu_type_for_mvp(bsx, bsy, pu_pix_x, pu_pix_y);
            get_mvp_default(h, p_cu, pix_x + pu_pix_x, pix_y + pu_pix_y, &mvp, 0, refframe, bsx, pu_mvp_type);

            bsx >>= MIN_PU_SIZE_IN_BIT;
            bsy >>= MIN_PU_SIZE_IN_BIT;
            if (h->i_frame_type != AVS2_S_SLICE) {  //no mvd for S frame, just set it to 0
                mv_t mvd;
                aec_read_mvds(p_aec, &mvd);
                pmvr_mv_derivation(h, &mv, &mvd, &mvp);
#if AVS2_TRACE
                avs2_trace("@%d FMVD (pred %3d)\t\t\t%d \n", symbolCount++, mvp.x, mvd.x);
                avs2_trace("@%d FMVD (pred %3d)\t\t\t%d \n", symbolCount++, mvp.y, mvd.y);
#endif
                AEC_RETURN_ON_ERROR(-1);
            } else {
                mv.v = mvp.v;
            }

            if (bframe) {
                mv_t *p_mv_1st = h->p_tmv_1st + j8 * width_in_spu + i8;
                for (jj = 0; jj < bsy; jj++) {
                    for (ii = 0; ii < bsx; ii++) {
                        p_mv_1st[ii] = mv;
                    }
                    p_mv_1st += width_in_spu;
                }
                p_cu->mv[idx_pu][0] = mv;
            } else {
                mv_t *p_mv_1st = h->p_tmv_1st + j8 * width_in_spu + i8;
                mv_t *p_mv_2nd = h->p_tmv_2nd + j8 * width_in_spu + i8;
                mv_t mv_2nd;
                if (p_cu->b8pdir[idx_pu] == PDIR_DUAL) {
                    int distance_1st     = get_distance_index_p(h, refframe);
                    int distance_1st_src = get_distance_index_p_scale(h, refframe);
                    int distance_2nd     = get_distance_index_p(h, !refframe);

                    mv_2nd.x = scale_mv_skip(h, mv.x, distance_2nd, distance_1st_src);
                    mv_2nd.y = scale_mv_skip_y(h, mv.y, distance_2nd, distance_1st, distance_1st_src);
                } else {
                    mv_2nd.v = 0;
                }

                p_cu->mv[idx_pu][0] = mv;
                p_cu->mv[idx_pu][1] = mv_2nd;

                for (jj = 0; jj < bsy; jj++) {
                    for (ii = 0; ii < bsx; ii++) {
                        p_mv_1st[ii] = mv;
                        p_mv_2nd[ii] = mv_2nd;
                    }
                    p_mv_1st += width_in_spu;
                    p_mv_2nd += width_in_spu;
                }
            }
        }
    }

    if (!bframe) {
        return 0;
    }

    assert(h->i_pic_coding_type == FRAME);
    {
        distance_fwd     = get_distance_index_b(h, B_FWD);  // fwd
        distance_fwd_src = get_distance_index_b_scale(h, B_FWD);
        distance_bwd     = get_distance_index_b(h, B_BWD);  // bwd
    }

    //=====  READ PDIR_BWD MOTION VECTORS =====
    for (idx_pu = 0; idx_pu< p_cu->num_pu; idx_pu++) {
        if (p_cu->b8pdir[idx_pu] != PDIR_FWD) {     //has backward vector
            int pu_pix_x = p_cu->pu[idx_pu].x;
            int pu_pix_y = p_cu->pu[idx_pu].y;
            int bsx      = p_cu->pu[idx_pu].w;
            int bsy      = p_cu->pu[idx_pu].h;
            int i8       = block8_x + (pu_pix_x >> 2);
            int j8       = block8_y + (pu_pix_y >> 2);
            int refframe = h->p_ref_idx[j8 * width_in_spu + i8].r[1];
            mv_t *p_mv_2nd = h->p_tmv_2nd + j8 * width_in_spu + i8;
            mv_t mv, mvp;
            int ii, jj;

            int pu_mvp_type = get_pu_type_for_mvp(bsx, bsy, pu_pix_x, pu_pix_y);
            get_mvp_default(h, p_cu, pix_x + pu_pix_x, pix_y + pu_pix_y, &mvp, 1, refframe, bsx, pu_mvp_type);

            bsx >>= MIN_PU_SIZE_IN_BIT;
            bsy >>= MIN_PU_SIZE_IN_BIT;

            if (p_cu->b8pdir[idx_pu] == PDIR_SYM) {
                mv_t mv_1st;

                mv_1st = h->p_tmv_1st[j8 * width_in_spu + i8];

                mv.x = -scale_mv_skip  (h, mv_1st.x, distance_bwd, distance_fwd_src);
                mv.y = -scale_mv_skip_y(h, mv_1st.y, distance_bwd, distance_fwd, distance_fwd_src);
            } else {
                mv_t mvd;
                aec_read_mvds(p_aec, &mvd);
                pmvr_mv_derivation(h, &mv, &mvd, &mvp);
#if AVS2_TRACE
                avs2_trace("@%d BMVD (pred %3d)\t\t\t%d \n", symbolCount++, mvp.x, mvd.x);
                avs2_trace("@%d BMVD (pred %3d)\t\t\t%d \n", symbolCount++, mvp.y, mvd.y);
#endif
                AEC_RETURN_ON_ERROR(-1);
            }

            p_cu->mv[idx_pu][1] = mv;
            for (jj = 0; jj < bsy; jj++) {
                for (ii = 0; ii < bsx; ii++) {
                    p_mv_2nd[ii] = mv;
                }

                p_mv_2nd += width_in_spu;
            }
        }
    }


    return 0;
}

/* ---------------------------------------------------------------------------
 * get all coefficients of one CU
 */
static int cu_read_all_coeffs(davs2_t *h, aec_t *p_aec, cu_t *p_cu)
{
    runlevel_t *runlevel = &h->lcu.cg_info;
    int idx_cu_zscan = h->lcu.idx_cu_zscan_aec;
#if CTRL_AEC_THREAD
    coeff_t *coeff_y = &h->lcu.lcu_aec->rec_info.coeff_buf_y    [idx_cu_zscan << 6];
    coeff_t *coeff_u = &h->lcu.lcu_aec->rec_info.coeff_buf_uv[0][idx_cu_zscan << 4];
    coeff_t *coeff_v = &h->lcu.lcu_aec->rec_info.coeff_buf_uv[1][idx_cu_zscan << 4];
#else
    coeff_t *coeff_y = &h->lcu.rec_info.coeff_buf_y    [idx_cu_zscan << 6];
    coeff_t *coeff_u = &h->lcu.rec_info.coeff_buf_uv[0][idx_cu_zscan << 4];
    coeff_t *coeff_v = &h->lcu.rec_info.coeff_buf_uv[1][idx_cu_zscan << 4];
#endif
    int bit_size   = p_cu->i_cu_level;
    int i_tu_level = p_cu->i_cu_level;  // 与变换块中包含的系数相关
    int b8;
    int uv;

    /*if (h->i_pic_coding_type == FRAME)*/ {
        runlevel->p_ctx_run            = p_aec->syn_ctx.coeff_run[0];
        runlevel->p_ctx_level          = p_aec->syn_ctx.coeff_level;
        runlevel->p_ctx_sig_cg         = p_aec->syn_ctx.sig_cg_contexts;
        runlevel->p_ctx_last_cg        = p_aec->syn_ctx.last_cg_contexts;
        runlevel->p_ctx_last_pos_in_cg = p_aec->syn_ctx.last_coeff_pos;
    }

    // luma coefficients
    if (p_cu->i_trans_size == TU_SPLIT_NON) {
        i_tu_level         = DAVS2_MIN(3, i_tu_level - B4X4_IN_BIT);
        runlevel->avs_scan = tab_scan_coeff[i_tu_level][TU_SPLIT_NON];
        runlevel->cg_scan  = tab_scan_cg[i_tu_level][TU_SPLIT_NON];

        if (p_cu->i_cbp & 0x0F) {
            int intra_pred_class = IS_INTRA(p_cu) ? tab_intra_mode_scan_type[p_cu->intra_pred_modes[0]] : INTRA_PRED_DC_DIAG;
            int b_swap_xy = (IS_INTRA(p_cu) && intra_pred_class == INTRA_PRED_HOR);
            int blocksize = 1 << (i_tu_level + B4X4_IN_BIT);
            int shift, scale;
            int wq_size_id = DAVS2_MIN(3, bit_size - B4X4_IN_BIT);

            cu_get_quant_params(h, p_cu->i_qp, bit_size - (p_cu->i_trans_size != TU_SPLIT_NON), &shift, &scale);
#if !CTRL_AEC_THREAD
            gf_davs2.fast_memzero(coeff_y, sizeof(coeff_t) * blocksize * blocksize);
#endif

            p_cu->dct_pattern[0] = cu_get_block_coeffs(p_aec, runlevel, p_cu, coeff_y,
                                                       blocksize, blocksize, i_tu_level,
                                                       1, intra_pred_class, b_swap_xy,
                                                       scale, shift, wq_size_id);
        }
    } else {
        int b_wavelet_conducted = (bit_size == B64X64_IN_BIT && p_cu->i_trans_size != TU_SPLIT_CROSS);
        cb_t tus[4];
        int shift, scale;
        int wq_size_id = DAVS2_MIN(3, bit_size - B4X4_IN_BIT);

        cu_init_transform_units(p_cu, tus);
        tus[0].v >>= b_wavelet_conducted;
        tus[1].v >>= b_wavelet_conducted;
        tus[2].v >>= b_wavelet_conducted;
        tus[3].v >>= b_wavelet_conducted;

        i_tu_level -= B8X8_IN_BIT;
        i_tu_level -= b_wavelet_conducted;
        cu_get_quant_params(h, p_cu->i_qp, p_cu->i_cu_level - (p_cu->i_trans_size != TU_SPLIT_NON), &shift, &scale);
        if (p_cu->i_trans_size == TU_SPLIT_CROSS) {
            wq_size_id = DAVS2_MIN(3, bit_size - B8X8_IN_BIT);
        } else {
            wq_size_id = bit_size - B8X8_IN_BIT;
            wq_size_id -= (p_cu->i_cu_level == B64X64_IN_BIT);
        }

        runlevel->avs_scan = tab_scan_coeff[i_tu_level][p_cu->i_trans_size];
        runlevel->cg_scan  = tab_scan_cg[i_tu_level][p_cu->i_trans_size];

        for (b8 = 0; b8 < 4; b8++) { /* all 4 blocks */
            if (p_cu->i_cbp & (1 << b8)) {
                int bsx = tus[b8].w;
                int bsy = tus[b8].h;
                int intra_pred_class = IS_INTRA(p_cu) ? tab_intra_mode_scan_type[p_cu->intra_pred_modes[b8]] : INTRA_PRED_DC_DIAG;
                int b_swap_xy = (IS_INTRA(p_cu) && intra_pred_class == INTRA_PRED_HOR && p_cu->i_cu_type != PRED_I_2Nxn && p_cu->i_cu_type != PRED_I_nx2N);
                coeff_t *p_res = coeff_y + (b8 << ((bit_size - 1) << 1));
#if !CTRL_AEC_THREAD
                gf_davs2.fast_memzero(p_res, sizeof(coeff_t) * bsx * bsy);
#endif
                p_cu->dct_pattern[b8] = cu_get_block_coeffs(p_aec, runlevel, p_cu, p_res,
                                                            bsx, bsy, i_tu_level,
                                                            1, intra_pred_class, b_swap_xy,
                                                            scale, shift, wq_size_id);
                if (p_cu->dct_pattern[b8] < 0) {
                    return -1;
                }
            }
        }
    }

    // adaptive frequency weighting quantization
    i_tu_level = p_cu->i_cu_level - B8X8_IN_BIT;
    runlevel->avs_scan = tab_scan_coeff[i_tu_level][TU_SPLIT_NON];
    runlevel->cg_scan  = tab_scan_cg[i_tu_level][TU_SPLIT_NON];

    /*if (h->i_pic_coding_type == FRAME)*/ {
        runlevel->p_ctx_run             = p_aec->syn_ctx.coeff_run[1];
        runlevel->p_ctx_level           = p_aec->syn_ctx.coeff_level + 20;
        runlevel->p_ctx_sig_cg          = p_aec->syn_ctx.sig_cg_contexts + NUM_SIGCG_CTX_LUMA;
        runlevel->p_ctx_last_cg         = p_aec->syn_ctx.last_cg_contexts + NUM_LAST_CG_CTX_LUMA;
        runlevel->p_ctx_last_pos_in_cg  = p_aec->syn_ctx.last_coeff_pos + NUM_LAST_POS_CTX_LUMA;
    }

    if (h->i_chroma_format != CHROMA_400) {
        int wq_size_id = p_cu->i_cu_level - 1;
        for (uv = 0; uv < 2; uv++) {
            if ((p_cu->i_cbp >> (uv + 4)) & 0x1) {
                int blocksize = 1 << wq_size_id;
                coeff_t *p_res = uv ? coeff_v : coeff_u;
                int shift, scale;
#if !CTRL_AEC_THREAD
                gf_davs2.fast_memzero(p_res, sizeof(coeff_t) * blocksize * blocksize);
#endif
                cu_get_quant_params(h, cu_get_chroma_qp(h, p_cu->i_qp, uv), wq_size_id, &shift, &scale);

                p_cu->dct_pattern[4 + uv] = cu_get_block_coeffs(p_aec, runlevel, p_cu, p_res,
                                                                blocksize, blocksize, i_tu_level,
                                                                0, INTRA_PRED_DC_DIAG, 0,
                                                                scale, shift, wq_size_id);
                if (p_cu->dct_pattern[4 + uv] < 0) {
                    return -1;
                }
            }
        }
    }

    return 0;
}

/* ---------------------------------------------------------------------------
 * get the syntax elements from the NAL, return cu_type
 */
static
int cu_read_header(davs2_t *h, aec_t *p_aec, cu_t *p_cu, int pix_x, int pix_y, int *p_real_cu_type)
{
    int real_cu_type;

    p_cu->i_md_directskip_mode = 0;
    if (h->i_frame_type == AVS2_S_SLICE) {
        real_cu_type = aec_read_cu_type_sframe(p_aec);
    } else {
        real_cu_type = aec_read_cu_type(p_aec, p_cu, h->i_frame_type, h->seq_info.enable_amp,
            h->seq_info.enable_mhp_skip, h->seq_info.enable_wsm, h->num_of_references);
    }

    AEC_RETURN_ON_ERROR(-1);

    *p_real_cu_type = real_cu_type;
    real_cu_type    = DAVS2_MAX(0, real_cu_type);
    p_cu->i_cu_type = (int8_t)real_cu_type;

    /* 帧间预测的方向解析 */
    if (h->i_frame_type != AVS2_I_SLICE && IS_INTER_MODE(real_cu_type)) {
        aec_read_inter_pred_dir(p_aec, p_cu, h);
        AEC_RETURN_ON_ERROR(-1);
    }

    if (IS_INTRA(p_cu)) {
        int size_8x8   = 1 << (p_cu->i_cu_level - B8X8_IN_BIT);
        int size_16x16 = 1 << (p_cu->i_cu_level - B16X16_IN_BIT);
        int y_4x4      = pix_y >> MIN_PU_SIZE_IN_BIT;
        int x_4x4      = pix_x >> MIN_PU_SIZE_IN_BIT;

        real_cu_type = aec_read_intra_cu_type(p_aec, p_cu, h->seq_info.enable_sdip, h);
        p_cu->i_cu_type = (int8_t)real_cu_type;
        AEC_RETURN_ON_ERROR(-1);

        /* Read luma block prediction modes */
        if (cu_read_intrapred_mode_luma(h, p_aec, p_cu, 0, x_4x4, y_4x4) < 0) {
            return -1;
        }

        switch (real_cu_type) {
        case PRED_I_2Nxn:
            if (cu_read_intrapred_mode_luma(h, p_aec, p_cu, 1, x_4x4, y_4x4 + 1 * size_16x16) < 0 ||
                cu_read_intrapred_mode_luma(h, p_aec, p_cu, 2, x_4x4, y_4x4 + 2 * size_16x16) < 0 ||
                cu_read_intrapred_mode_luma(h, p_aec, p_cu, 3, x_4x4, y_4x4 + 3 * size_16x16) < 0) {
                return -1;
            }

            break;
        case PRED_I_nx2N:
            if (cu_read_intrapred_mode_luma(h, p_aec, p_cu, 1, x_4x4 + 1 * size_16x16, y_4x4) < 0 ||
                cu_read_intrapred_mode_luma(h, p_aec, p_cu, 2, x_4x4 + 2 * size_16x16, y_4x4) < 0 ||
                cu_read_intrapred_mode_luma(h, p_aec, p_cu, 3, x_4x4 + 3 * size_16x16, y_4x4) < 0) {
                return -1;
            }

            break;
        case PRED_I_NxN:
            if (cu_read_intrapred_mode_luma(h, p_aec, p_cu, 1, x_4x4 + size_8x8, y_4x4 + 0) < 0 ||
                cu_read_intrapred_mode_luma(h, p_aec, p_cu, 2, x_4x4 + 0, y_4x4 + size_8x8) < 0 ||
                cu_read_intrapred_mode_luma(h, p_aec, p_cu, 3, x_4x4 + size_8x8, y_4x4 + size_8x8) < 0) {
                return -1;
            }

            break;
        default:
            break;
        }

#if AVS2_TRACE
        strncpy(p_aec->tracestring, "Chroma intra pred mode", TRACESTRING_SIZE);
#endif
        if (h->i_chroma_format != CHROMA_400) {
            p_cu->c_ipred_mode = (int8_t)aec_read_intra_pmode_c(p_aec, h, p_cu->intra_pred_modes[0]);
        } else {
            p_cu->c_ipred_mode = 0;
        }
        AEC_RETURN_ON_ERROR(-1);
    }

    return 0;
}

/* ---------------------------------------------------------------------------
 * read CU information from bitstream
 */
static int cu_read_info(davs2_t *h, cu_t *p_cu, int i_level, int scu_xy, int pix_x, int pix_y)
{
    aec_t *p_aec = &h->aec;
    int size_in_scu = 1 << (i_level - MIN_CU_SIZE_IN_BIT);
    int real_cu_type;

    /* 0, initial cu data */
    cu_init(h, p_cu, i_level, scu_xy, pix_x);

    /* 1, read cu type and delta_QP
     * including PU partition, intra prediction mode, reference indexes
     */
    if (cu_read_header(h, p_aec, p_cu, pix_x, pix_y, &real_cu_type) < 0) {
        return -1;
    }

    // get the size and pos of prediction units
    cu_init_prediction_units(h, p_cu);

    /* 2, read motion vectors and reference indexes */
    if (IS_INTRA(p_cu)) {
        int i = 0;
        for (i = 0; i < 4; i++) {
            p_cu->ref_idx[i].r[0] = INVALID_REF;
            p_cu->ref_idx[i].r[1] = INVALID_REF;
            p_cu->b8pdir[i]       = PDIR_INVALID;
        }
        // TODO: 由于帧级已初始化，此处无需重复设置 cu_store_references()
        cu_store_references(h, p_cu, pix_x, pix_y);
    } else if (p_cu->i_cu_type == PRED_SKIP) {
        cu_get_neighbors(h, p_cu, pix_x, pix_y, 1 << i_level, 1 << i_level);
        fill_mv_and_ref_for_skip(h, p_cu, pix_x, pix_y, size_in_scu);
    } else {
        cu_store_references(h, p_cu, pix_x, pix_y);
        if (cu_read_mv(h, p_aec, p_cu->i_cu_level, scu_xy, pix_x, pix_y) < 0) {
            return -1;
        }
    }

    /* 3, read CBP and coefficients */
    if (real_cu_type < 0) {  /* skip mode, no residual */
        p_cu->i_qp = h->lcu.i_left_cu_qp;
        p_cu->i_trans_size = TU_SPLIT_NON;      // cbp has been initialed as zero
    } else {    // non-skip mode
        // read CBP
        if (cu_read_cbp(h, p_aec, p_cu, pix_x >> MIN_CU_SIZE_IN_BIT, pix_y >> MIN_CU_SIZE_IN_BIT) < 0) {
            return -1;
        }

        if (p_cu->i_cbp != 0) {
            if (cu_read_all_coeffs(h, p_aec, p_cu) < 0) {   // read all coefficients
                return -1;
            }
        }
    }

    AEC_RETURN_ON_ERROR(-1);
    /* 4, finish decoding the cu data */
    cu_read_end(h, p_cu, i_level, scu_xy);

    return 0;
}

/* ---------------------------------------------------------------------------
 */
void decoder_wait_lcu_row(davs2_t *h, davs2_frame_t *frame, int line)
{
    line = DAVS2_MAX(line, 0);
    line = DAVS2_MIN(line, h->i_height_in_lcu - 1);

    if (frame->i_decoded_line < line && frame->num_decoded_lcu_in_row[line] < h->i_width_in_lcu + 1) {
        davs2_thread_mutex_lock(&frame->mutex_recon);

        while (frame->i_decoded_line < line && frame->num_decoded_lcu_in_row[line] < h->i_width_in_lcu + 1) {
            davs2_thread_cond_wait(&frame->conds_lcu_row[line], &frame->mutex_recon);
        }

        davs2_thread_mutex_unlock(&frame->mutex_recon);
    }
}

/* ---------------------------------------------------------------------------
 */
void decoder_wait_row(davs2_t *h, davs2_frame_t *frame, int max_y_in_pic)
{
    int line = (max_y_in_pic + 8) >> h->i_lcu_level;
    line = DAVS2_MAX(line, 0);
    line = DAVS2_MIN(line, h->i_height_in_lcu - 1);
    decoder_wait_lcu_row(h, frame, line);
}

/* ---------------------------------------------------------------------------
 * img_size: 整像素精度的图像 宽度或高度 （整像素精度）
 * blk_size: 当前预测块的 宽度或高度     （整像素精度）
 * blk_pos:  当前块在图像中的 x/y 坐标   （整像素精度）
 * mv     :  MV 的 x/y 分量             （1/4像素精度）
 */
static INLINE
int cu_get_mc_pos(int img_size, int blk_size, int blk_pos, int mv)
{
    int imv = mv >> 2;  // MV的整像素精度
    int fmv = mv & 7;   // MV的分像素精度部分，保留到 1/8 精度

    if (blk_pos + imv < -blk_size - 8) {
        return ((-blk_size - 8) << 2) + (fmv);
    } else if (blk_pos + imv > img_size + 4) {
        return ((img_size + 4) << 2) + (fmv);
    } else {
        return (blk_pos << 2) + mv;
    }
}

/* ---------------------------------------------------------------------------
 * clip mv
 */
static INLINE
void cu_get_mc_pos_mv(davs2_t *h, mv_t *mv, int pic_pix_x, int pic_pix_y, int blk_w, int blk_h)
{
    mv->x = (int16_t)cu_get_mc_pos(h->i_width,  blk_w, pic_pix_x, mv->x);
    mv->y = (int16_t)cu_get_mc_pos(h->i_height, blk_h, pic_pix_y, mv->y);
}

/* ---------------------------------------------------------------------------
 * decode one coding unit
 */
static int davs2_get_inter_pred(davs2_t *h, davs2_row_rec_t *row_rec, cu_t *p_cu, int ctu_x, int ctu_y)
{
    static const int mv_shift = 2;
    int pu_idx;

    for (pu_idx = 0; pu_idx < p_cu->num_pu; pu_idx++) {
        int pix_x, pix_y, width, height;
        int vec1_x, vec1_y, vec2_x, vec2_y;
        int pred_dir;
        int ref_1st, ref_2nd;
        cb_t *pu = &p_cu->pu[pu_idx];
        mv_t mv_1st, mv_2nd;
        davs2_frame_t *p_fref1, *p_fref2;

        p_fref1  = p_fref2  = NULL;
        ref_1st  = ref_2nd  = 0;
        mv_1st.v = mv_2nd.v = 0;

        pix_x  = ctu_x + pu->x;
        pix_y  = ctu_y + pu->y;
        width  = pu->w;
        height = pu->h;

        pred_dir  = p_cu->b8pdir[pu_idx];

        if (pred_dir == PDIR_BWD) {
            ref_1st = B_BWD;
            mv_1st  = p_cu->mv[pu_idx][1];
            p_fref1 = h->fref[B_BWD];
        } else if (pred_dir == PDIR_SYM || pred_dir == PDIR_BID) {
            mv_1st.v = p_cu->mv[pu_idx][0].v;
            mv_2nd.v = p_cu->mv[pu_idx][1].v;

            p_fref1 = h->fref[B_FWD];
            p_fref2 = h->fref[B_BWD];
        } else {
            /* FWD or DUAL */
            int dmh_mode = p_cu->i_dmh_mode;

            ref_1st = p_cu->ref_idx[pu_idx].r[0];
            mv_1st  = p_cu->mv[pu_idx][0];

            if (h->i_frame_type == AVS2_B_SLICE) {
                /* for B frame */
                ref_1st = 0;
                p_fref1 = h->fref[B_FWD];
            } else {
                if (pred_dir == PDIR_DUAL) {
                    mv_2nd  = p_cu->mv[pu_idx][1];
                    ref_2nd = p_cu->ref_idx[pu_idx].r[1];
                    p_fref1 = h->fref[ref_1st];
                    p_fref2 = h->fref[ref_2nd];
                } else if (dmh_mode) {
                    mv_2nd.x = mv_1st.x + dmh_pos[dmh_mode][1][0];
                    mv_2nd.y = mv_1st.y + dmh_pos[dmh_mode][1][1];

                    mv_1st.x += dmh_pos[dmh_mode][0][0];
                    mv_1st.y += dmh_pos[dmh_mode][0][1];

                    ref_2nd = ref_1st;
                    p_fref1 = p_fref2 = h->fref[ref_1st];
                } else {
                    p_fref1 = h->fref[ref_1st];
                }
            }
        }

        cu_get_mc_pos_mv(h, &mv_1st, pix_x + row_rec->ctu.i_pix_x, pix_y + row_rec->ctu.i_pix_y, width, height);
        vec1_x = mv_1st.x;
        vec1_y = mv_1st.y;

        cu_get_mc_pos_mv(h, &mv_2nd, pix_x + row_rec->ctu.i_pix_x, pix_y + row_rec->ctu.i_pix_y, width, height);
        vec2_x = mv_2nd.x;
        vec2_y = mv_2nd.y;

        // TODO: 出现背景帧参考情况下的参考帧管理需在RPS部分做好修改
        // if (h->b_bkgnd_reference && h->num_of_references >= 2 && ref_1st == h->num_of_references - 1 && (h->i_frame_type == AVS2_P_SLICE || h->i_frame_type == AVS2_F_SLICE) && h->i_typeb != AVS2_S_SLICE) {
        //     p_fref1 = h->f_background_ref;
        // } else if (h->i_typeb == AVS2_S_SLICE) {
        //     p_fref1 = h->f_background_ref;
        // }

        /* luma prediction */
        if (p_fref1 != NULL) {
            int i_pred = row_rec->ctu.i_fdec[IMG_Y];
            int i_fref = h->fref[0]->i_stride[IMG_Y];

            pel_t *p_pred = row_rec->ctu.p_fdec[IMG_Y] + pix_y * i_pred + pix_x;

            decoder_wait_row(h, p_fref1, (vec1_y >> mv_shift) + height + 8 + 4);

            mc_luma(h, p_pred, i_pred, vec1_x, vec1_y, width, height, p_fref1->planes[IMG_Y], i_fref);

            if (p_fref2 != NULL) {
                pel_t *p_temp = row_rec->pred_blk;

                decoder_wait_row(h, p_fref2, (vec2_y >> mv_shift) + height + 8 + 4);

                mc_luma(h, p_temp, LCU_STRIDE, vec2_x, vec2_y, width, height, p_fref2->planes[IMG_Y], i_fref);

                gf_davs2.block_avg(p_pred, i_pred, p_pred, i_pred, p_temp, LCU_STRIDE, width, height);
            }
        } else {
            davs2_log(h, DAVS2_LOG_ERROR, "non-existing reference frame. PB (%d, %d)", pix_x, pix_y);
            return -1;
        }

        /* chroma prediction */
        if (h->i_chroma_format == CHROMA_420) {
            pix_x >>= 1;
            pix_y >>= 1;
            width >>= 1;
            height >>= 1;

            if (p_fref2 == NULL) {
                int i_fref = p_fref1->i_stride[IMG_U];
                int i_pred = row_rec->ctu.i_fdec[IMG_U];

                pel_t *p_pred = row_rec->ctu.p_fdec[IMG_U] + pix_y * i_pred + pix_x;

                mc_chroma(h, p_pred, i_pred, vec1_x, vec1_y, width, height, p_fref1->planes[IMG_U], i_fref);

                i_fref = p_fref1->i_stride[IMG_V];
                i_pred = row_rec->ctu.i_fdec[IMG_V];
                p_pred = row_rec->ctu.p_fdec[IMG_V] + pix_y * i_pred + pix_x;

                mc_chroma(h, p_pred, i_pred, vec1_x, vec1_y, width, height, p_fref1->planes[IMG_V], i_fref);
            } else {
                /* u component */
                int i_fref = p_fref1->i_stride[IMG_U];
                int i_pred = row_rec->ctu.i_fdec[IMG_U];

                pel_t *p_pred = row_rec->ctu.p_fdec[IMG_U] + pix_y * i_pred + pix_x;
                pel_t *p_temp = row_rec->pred_blk;

                mc_chroma(h, p_pred, i_pred, vec1_x, vec1_y, width, height, p_fref1->planes[IMG_U], i_fref);
                mc_chroma(h, p_temp, LCU_STRIDE >> 1, vec2_x, vec2_y, width, height, p_fref2->planes[IMG_U], i_fref);

                gf_davs2.block_avg(p_pred, i_pred, p_pred, i_pred, p_temp, LCU_STRIDE >> 1, width, height);

                /* v component */
                i_fref = p_fref1->i_stride[IMG_V];
                i_pred = row_rec->ctu.i_fdec[IMG_V];
                p_pred = row_rec->ctu.p_fdec[IMG_V] + pix_y * i_pred + pix_x;

                mc_chroma(h, p_pred, i_pred, vec1_x, vec1_y, width, height, p_fref1->planes[IMG_V], i_fref);
                mc_chroma(h, p_temp, LCU_STRIDE >> 1, vec2_x, vec2_y, width, height, p_fref2->planes[IMG_V], i_fref);

                gf_davs2.block_avg(p_pred, i_pred, p_pred, i_pred, p_temp, LCU_STRIDE >> 1, width, height);
            }
        }   // chroma format YUV420
    }

    return 0;
}

/* ---------------------------------------------------------------------------
 * reconstruct a CU
 */
static int cu_recon(davs2_t *h, davs2_row_rec_t *row_rec, cu_t *p_cu, int pix_x, int pix_y)
{
    int ctu_x = pix_x - row_rec->ctu.i_pix_x;
    int ctu_y = pix_y - row_rec->ctu.i_pix_y;
    int ctu_c_x = ctu_x >> 1;
    int ctu_c_y = ctu_y >> 1;
    int blockidx;
    cb_t tus[4];

    cu_init_transform_units(p_cu, tus);

    if (IS_INTRA(p_cu)) {  /* intra cu */
        /* 1, luma component, prediction and residual coding */
        if (p_cu->i_trans_size == TU_SPLIT_NON) {
            davs2_get_intra_pred(row_rec, p_cu, p_cu->intra_pred_modes[0], ctu_x, ctu_y, tus[0].w, tus[0].h);
            if (p_cu->i_cbp & 0x0F) {
                davs2_get_recons(row_rec, p_cu, 0, &tus[0], ctu_x, ctu_y);
            }
        } else {
            for (blockidx = 0; blockidx < 4; blockidx++) {
                davs2_get_intra_pred(row_rec, p_cu, p_cu->intra_pred_modes[blockidx],
                    ctu_x + tus[blockidx].x, ctu_y + tus[blockidx].y,
                    tus[blockidx].w, tus[blockidx].h);
                if (p_cu->i_cbp & (1 << blockidx)) {
                    davs2_get_recons(row_rec, p_cu, blockidx, &tus[blockidx], ctu_x, ctu_y);
                }
            }
        }

        /* 2, chroma component prediction */
        if (h->i_chroma_format == CHROMA_420) {
            davs2_get_intra_pred_chroma(row_rec, p_cu, ctu_c_x, ctu_c_y);
        }
    } else {  /* inter cu */
        /* 1, prediction (including luma and chroma) */
        if (davs2_get_inter_pred(h, row_rec, p_cu, ctu_x, ctu_y) < 0) {
            return -1;
        }

        /* 2, luma residual decoding */
        if (p_cu->i_trans_size == TU_SPLIT_NON) {
            if (p_cu->i_cbp & 0x0F) {
                davs2_get_recons(row_rec, p_cu, 0, &tus[0], ctu_x, ctu_y);
            }
        } else {
            for (blockidx = 0; blockidx < 4; blockidx++) {
                if (p_cu->i_cbp & (1 << blockidx)) {
                    davs2_get_recons(row_rec, p_cu, blockidx, &tus[blockidx], ctu_x, ctu_y);
                }
            }
        }
    }

    /* 3, chroma residual decoding */
    if (h->i_chroma_format == CHROMA_420) {
        cb_t cur_cb;

        cur_cb.w = cur_cb.h = 1 << (p_cu->i_cu_level - 1);
        cur_cb.y = 1 << p_cu->i_cu_level;
        cur_cb.x = 0;
        if (p_cu->i_cbp & (1 << 4)) {
            davs2_get_recons(row_rec, p_cu, 4, &cur_cb, ctu_x, ctu_y);
        }

        cur_cb.x = (int8_t)cur_cb.h;
        if (p_cu->i_cbp & (1 << 5)) {
            davs2_get_recons(row_rec, p_cu, 5, &cur_cb, ctu_x, ctu_y);
        }
    }

    return 0;
}

/* ---------------------------------------------------------------------------
 */
static ALWAYS_INLINE void
copy_lcu_col1(pel_t *dst, pel_t *src, const int height, const int stride)
{
    int i, k;

    for (i = 0, k = 0; i < height; i++, k += stride) {
        dst[k] = src[k];
    }
}

/* ---------------------------------------------------------------------------
 */
void decode_lcu_init(davs2_t *h, int i_lcu_x, int i_lcu_y)
{
    const int num_in_scu   = 1 << (h->i_lcu_level - MIN_CU_SIZE_IN_BIT);
    const int width_in_scu = h->i_width_in_scu;
    int lcu_w_in_scu, lcu_h_in_scu;
    int i, j;

    assert(h->lcu.i_scu_xy >= 0 && h->lcu.i_scu_xy < h->i_size_in_scu);

    // update coordinates of the current coding unit
    h->lcu.i_scu_x  = i_lcu_x << (h->i_lcu_level - MIN_CU_SIZE_IN_BIT);
    h->lcu.i_scu_y  = i_lcu_y << (h->i_lcu_level - MIN_CU_SIZE_IN_BIT);
    h->lcu.i_scu_xy = h->lcu.i_scu_y * width_in_scu + h->lcu.i_scu_x;

    h->lcu.i_spu_x  = h->lcu.i_scu_x * BLOCK_MULTIPLE;                  // luma block position
    h->lcu.i_spu_y  = h->lcu.i_scu_y * BLOCK_MULTIPLE;                  // luma block position
    h->lcu.i_pix_x  = h->lcu.i_scu_x << MIN_CU_SIZE_IN_BIT;             // luma pixel position
    h->lcu.i_pix_y  = h->lcu.i_scu_y << MIN_CU_SIZE_IN_BIT;             // luma coding unit position

    h->lcu.i_pix_c_x = h->lcu.i_scu_x << (MIN_CU_SIZE_IN_BIT - 1);      // chroma pixel position
    if (h->i_chroma_format == CHROMA_420) {
        h->lcu.i_pix_c_y = h->lcu.i_scu_y << (MIN_CU_SIZE_IN_BIT - 1);  // chroma coding unit position
    }

    // actual width and height (in pixel) for current lcu
    lcu_w_in_scu = DAVS2_MIN((h->i_width  - h->lcu.i_pix_x) >> MIN_CU_SIZE_IN_BIT, num_in_scu);
    lcu_h_in_scu = DAVS2_MIN((h->i_height - h->lcu.i_pix_y) >> MIN_CU_SIZE_IN_BIT, num_in_scu);
    h->lcu.i_pix_width  = lcu_w_in_scu << MIN_CU_SIZE_IN_BIT;
    h->lcu.i_pix_height = lcu_h_in_scu << MIN_CU_SIZE_IN_BIT;

    // init slice index of current LCU
    for (i = 0; i < lcu_h_in_scu; i++) {
        cu_t *p_cu_iter = &h->scu_data[h->lcu.i_scu_xy + i * width_in_scu];

        for (j = 0; j < lcu_w_in_scu; j++) {
            p_cu_iter->i_slice_nr = (int8_t)h->i_slice_index;
            p_cu_iter++;
        }
    }
}

/* ---------------------------------------------------------------------------
 */
void rowrec_lcu_init(davs2_t *h, davs2_row_rec_t *row_rec, int i_lcu_x, int i_lcu_y)
{
#if CTRL_AEC_THREAD
    row_rec->p_rec_info = &row_rec->lcu_info->rec_info;
#else
    row_rec->p_rec_info = &h->lcu.rec_info;
#endif
    row_rec->idx_cu_zscan = 0;
    /* CTU position */
    row_rec->ctu.i_pix_x = i_lcu_x << h->i_lcu_level;
    row_rec->ctu.i_pix_y = i_lcu_y << h->i_lcu_level;
    row_rec->ctu.i_pix_x_c = i_lcu_x << (h->i_lcu_level - 1);
    row_rec->ctu.i_pix_y_c = i_lcu_y << (h->i_lcu_level - 1);

    row_rec->ctu.i_ctu_w = DAVS2_MIN(h->i_width  - row_rec->ctu.i_pix_x, 1 << h->i_lcu_level);
    row_rec->ctu.i_ctu_h = DAVS2_MIN(h->i_height - row_rec->ctu.i_pix_y, 1 << h->i_lcu_level);
    row_rec->ctu.i_ctu_w_c = row_rec->ctu.i_ctu_w >> 1;
    row_rec->ctu.i_ctu_h_c = row_rec->ctu.i_ctu_h >> 1;

    row_rec->ctu.i_scu_x = i_lcu_x << (h->i_lcu_level - MIN_CU_SIZE_IN_BIT);
    row_rec->ctu.i_scu_y = i_lcu_y << (h->i_lcu_level - MIN_CU_SIZE_IN_BIT);
    row_rec->ctu.i_scu_xy = row_rec->ctu.i_scu_y * h->i_width_in_scu + row_rec->ctu.i_scu_x;

    row_rec->ctu.i_spu_x = row_rec->ctu.i_scu_x * BLOCK_MULTIPLE;                  // luma block position
    row_rec->ctu.i_spu_y = row_rec->ctu.i_scu_y * BLOCK_MULTIPLE;                  // luma block position
    
    /* init pointers */
    row_rec->h = h;

    row_rec->ctu.i_frec[0] = h->fdec->i_stride[0];
    row_rec->ctu.i_frec[1] = h->fdec->i_stride[1];
    row_rec->ctu.i_frec[2] = h->fdec->i_stride[2];

    row_rec->ctu.p_frec[0] = h->fdec->planes[0] + row_rec->ctu.i_pix_y   * row_rec->ctu.i_frec[0] + row_rec->ctu.i_pix_x;
    row_rec->ctu.p_frec[1] = h->fdec->planes[1] + row_rec->ctu.i_pix_y_c * row_rec->ctu.i_frec[1] + row_rec->ctu.i_pix_x_c;
    row_rec->ctu.p_frec[2] = h->fdec->planes[2] + row_rec->ctu.i_pix_y_c * row_rec->ctu.i_frec[2] + row_rec->ctu.i_pix_x_c;

#if 1
    row_rec->ctu.i_fdec[0] = h->fdec->i_stride[0];
    row_rec->ctu.i_fdec[1] = h->fdec->i_stride[1];
    row_rec->ctu.i_fdec[2] = h->fdec->i_stride[2];

    row_rec->ctu.p_fdec[0] = h->fdec->planes[0] + row_rec->ctu.i_pix_y   * row_rec->ctu.i_fdec[0] + row_rec->ctu.i_pix_x;
    row_rec->ctu.p_fdec[1] = h->fdec->planes[1] + row_rec->ctu.i_pix_y_c * row_rec->ctu.i_fdec[1] + row_rec->ctu.i_pix_x_c;
    row_rec->ctu.p_fdec[2] = h->fdec->planes[2] + row_rec->ctu.i_pix_y_c * row_rec->ctu.i_fdec[2] + row_rec->ctu.i_pix_x_c;
#else
    row_rec->ctu.i_fdec[0] = MAX_CU_SIZE;
    row_rec->ctu.i_fdec[1] = MAX_CU_SIZE;
    row_rec->ctu.i_fdec[2] = MAX_CU_SIZE;

    row_rec->ctu.p_fdec[0] = row_rec->fdec_buf;
    row_rec->ctu.p_fdec[1] = row_rec->fdec_buf + MAX_CU_SIZE * MAX_CU_SIZE;
    row_rec->ctu.p_fdec[2] = row_rec->fdec_buf + MAX_CU_SIZE * MAX_CU_SIZE + (MAX_CU_SIZE / 2);
#endif
}

/* ---------------------------------------------------------------------------
 */
int decode_lcu_parse(davs2_t *h, int i_level, int pix_x, int pix_y)
{
    const int width_in_scu = h->i_width_in_scu;
    const int pix_x_end = pix_x + (1 << i_level);
    const int pix_y_end = pix_y + (1 << i_level);
    int b_cu_inside_pic = (pix_x_end <= h->i_width) && (pix_y_end <= h->i_height);
    int split_flag = (i_level != MIN_CU_SIZE_IN_BIT);

    assert((pix_x < h->i_width) && (pix_y < h->i_height));
    if (i_level > MIN_CU_SIZE_IN_BIT && b_cu_inside_pic) {
        split_flag = aec_read_split_flag(&h->aec, i_level);
    }

    if (split_flag) {
        int i_level_next = i_level - 1;
        int i;

        for (i = 0; i < 4; i++) {
            int sub_pix_x = pix_x + ((i & 1) << i_level_next);
            int sub_pix_y = pix_y + ((i >> 1) << i_level_next);

            if (sub_pix_x < h->i_width && sub_pix_y < h->i_height) {
                decode_lcu_parse(h, i_level_next, sub_pix_x, sub_pix_y);
            }
        }
    } else {
        int i_cu_x  = (pix_x >> MIN_CU_SIZE_IN_BIT);
        int i_cu_y  = (pix_y >> MIN_CU_SIZE_IN_BIT);
        int i_cu_xy = i_cu_y * width_in_scu + i_cu_x;
        cu_t *p_cu  = &h->scu_data[i_cu_xy];

        h->lcu.idx_cu_zscan_aec = tab_b8xy_to_zigzag[i_cu_y - h->lcu.i_scu_y][i_cu_x - h->lcu.i_scu_x];

        if (cu_read_info(h, p_cu, i_level, i_cu_xy, pix_x, pix_y) < 0) {
            p_cu->i_slice_nr = -1;  // set an invalid value to terminate the reconstruction
            return -1;
        }
    }

    return 0;
}


/* ---------------------------------------------------------------------------
 */
int decode_lcu_recon(davs2_t *h, davs2_row_rec_t *row_rec, int i_level, int pix_x, int pix_y)
{
    const int width_in_scu = h->i_width_in_scu;
    int i_cu_x     = (pix_x >> MIN_CU_SIZE_IN_BIT);
    int i_cu_y     = (pix_y >> MIN_CU_SIZE_IN_BIT);
    int i_cu_xy    = i_cu_y * width_in_scu + i_cu_x;
    cu_t *p_cu     = &h->scu_data[i_cu_xy];
    int split_flag = (p_cu->i_cu_level < i_level);

    assert((pix_x < h->i_width) && (pix_y < h->i_height));

    if (split_flag) {
        int i_level_next = i_level - 1;
        int i;

        for (i = 0; i < 4; i++) {
            int sub_pix_x = pix_x + ((i &  1) << i_level_next);
            int sub_pix_y = pix_y + ((i >> 1) << i_level_next);

            if (sub_pix_x < h->i_width && sub_pix_y < h->i_height) {
                decode_lcu_recon(h, row_rec, i_level_next, sub_pix_x, sub_pix_y);
            }
        }
    } else {
        int i_cu_mask = h->i_lcu_size_sub1 >> MIN_CU_SIZE_IN_BIT;
        row_rec->idx_cu_zscan = tab_b8xy_to_zigzag[i_cu_y & i_cu_mask][i_cu_x & i_cu_mask];

        if (p_cu->i_slice_nr == -1) {
            h->decoding_error = 1;
            davs2_log(h, DAVS2_LOG_WARNING, "invalid CU (%3d, %3d), POC %3d",
                     pix_x, pix_y, h->i_poc);
        }
        cu_recon(h, row_rec, p_cu, pix_x, pix_y);
    }

    return 0;
}
