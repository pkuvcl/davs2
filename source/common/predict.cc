/*
 * predict.cc
 *
 * Description of this file:
 *    Prediction functions definition of the davs2 library
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
#include "predict.h"
#include "block_info.h"


/**
 * ===========================================================================
 * local & global variables (const tables)
 * ===========================================================================
 */

/**
 * ===========================================================================
 * function defines
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 */
static ALWAYS_INLINE
void check_scaling_neighbor_mv(davs2_t *h, mv_t *mv, int mult_distance, int ref_neighbor)
{
    if (ref_neighbor >= 0) {
        int devide_distance = get_distance_index_p(h, ref_neighbor);
        int devide_distance_src = get_distance_index_p_scale(h, ref_neighbor);

        mv->y = scale_mv_default_y(h, mv->y, mult_distance, devide_distance, devide_distance_src);
        mv->x = scale_mv_default  (h, mv->x, mult_distance, devide_distance_src);
    } else {
        mv->v = 0;
    }
}

/* ---------------------------------------------------------------------------
 */
static ALWAYS_INLINE
void check_scaling_neighbor_mv_b(davs2_t *h, mv_t *mv, int mult_distance, int  mult_distance_src, int ref_neighbor)
{
    if (ref_neighbor >= 0) {
        mv->y = scale_mv_default_y(h, mv->y, mult_distance, mult_distance, mult_distance_src);
        mv->x = scale_mv_default(h, mv->x, mult_distance, mult_distance_src);
    } else {
        mv->v = 0;
    }
}

/* ---------------------------------------------------------------------------
 */
static ALWAYS_INLINE
int recheck_neighbor_ref_avail(davs2_t *h, int ref_frame, int neighbor_ref)
{
    if (neighbor_ref != -1) {
        if (((ref_frame == h->num_of_references - 1 && neighbor_ref != h->num_of_references - 1) ||
             (ref_frame != h->num_of_references - 1 && neighbor_ref == h->num_of_references - 1)) &&
             (h->i_frame_type == AVS2_P_SLICE || h->i_frame_type == AVS2_F_SLICE) && h->b_bkgnd_picture) {
            neighbor_ref = -1;
        }

        if (h->i_frame_type == AVS2_S_SLICE) {
            neighbor_ref = -1;
        }
    }

    return neighbor_ref;
}

/* ---------------------------------------------------------------------------
 */
static ALWAYS_INLINE
int derive_mv_pred_type(int ref_frame, int rFrameL, int rFrameU, int rFrameUR, 
                        int pu_type_for_mvp)
{
    int mvp_type = MVPRED_xy_MIN;

    if ((rFrameL != INVALID_REF) && (rFrameU == INVALID_REF) && (rFrameUR == INVALID_REF)) {
        mvp_type = MVPRED_L;
    } else if ((rFrameL == INVALID_REF) && (rFrameU != INVALID_REF) && (rFrameUR == INVALID_REF)) {
        mvp_type = MVPRED_U;
    } else if ((rFrameL == INVALID_REF) && (rFrameU == INVALID_REF) && (rFrameUR != INVALID_REF)) {
        mvp_type = MVPRED_UR;
    } else {
        switch (pu_type_for_mvp) {
        case 1:
        case 4:
            if (rFrameL == ref_frame) {
                mvp_type = MVPRED_L;
            }
            break;
        case 2:
            if (rFrameUR == ref_frame) {
                mvp_type = MVPRED_UR;
            }
            break;
        case 3:
            if (rFrameU == ref_frame) {
                mvp_type = MVPRED_U;
            }
            break;
        default:
            break;
        }
    }

    return mvp_type;
}

/* ---------------------------------------------------------------------------
 */
static ALWAYS_INLINE int16_t derive_median_mv(int mva, int mvb, int mvc)
{
    int mvp;

    if (((mva < 0) && (mvb > 0) && (mvc > 0)) || ((mva > 0) && (mvb < 0) && (mvc < 0))) {
        mvp = (mvb + mvc) / 2;  // b
    } else if (((mvb < 0) && (mva > 0) && (mvc > 0)) || ((mvb > 0) && (mva < 0) && (mvc < 0))) {
        mvp = (mvc + mva) / 2;  // c
    } else if (((mvc < 0) && (mva > 0) && (mvb > 0)) || ((mvc > 0) && (mva < 0) && (mvb < 0))) {
        mvp = (mva + mvb) / 2;  // a
    } else {
        const int dAB = DAVS2_ABS(mva - mvb);  // for Ax
        const int dBC = DAVS2_ABS(mvb - mvc);  // for Bx
        const int dCA = DAVS2_ABS(mvc - mva);  // for Cx
        const int min_diff = DAVS2_MIN(dAB, DAVS2_MIN(dBC, dCA));

        if (min_diff == dAB) {
            mvp = (mva + mvb) / 2;  // a;
        } else if (min_diff == dBC) {
            mvp = (mvb + mvc) / 2;  // b;
        } else {
            mvp = (mvc + mva) / 2;  // c;
        }
    }

    return (int16_t)mvp;
}

/* ---------------------------------------------------------------------------
 * get neighboring MVs for MVP
 */
static ALWAYS_INLINE
void cu_get_neighbors_default_mvp(davs2_t *h, cu_t *p_cu, int pix_cu_x, int pix_cu_y, int bsx)
{
    neighbor_inter_t *neighbors = h->lcu.neighbor_inter;
    int cur_slice_idx = p_cu->i_slice_nr;
    int x0 = pix_cu_x >> MIN_PU_SIZE_IN_BIT;
    int y0 = pix_cu_y >> MIN_PU_SIZE_IN_BIT;
    int x1 = (bsx     >> MIN_PU_SIZE_IN_BIT) + x0 - 1;

    /* 1. check whether the top-right 4x4 block is reconstructed */
    int x4_TR    = x1 - h->lcu.i_spu_x;
    int y4_TR    = y0 - h->lcu.i_spu_y;
    int avail_TR = h->p_tab_TR_avail[(y4_TR << (h->i_lcu_level - B4X4_IN_BIT)) + x4_TR];

    /* 2. get neighboring blocks */
    cu_get_neighbor_spatial(h, cur_slice_idx, &neighbors[BLK_LEFT    ], x0 - 1, y0    );
    cu_get_neighbor_spatial(h, cur_slice_idx, &neighbors[BLK_TOP     ], x0    , y0 - 1);
    cu_get_neighbor_spatial(h, cur_slice_idx, &neighbors[BLK_TOPLEFT ], x0 - 1, y0 - 1);

    cu_get_neighbor_spatial(h, cur_slice_idx, &neighbors[BLK_TOPRIGHT], avail_TR ? x1 + 1 : -1, y0 - 1);
}

/* ---------------------------------------------------------------------------
 * set motion vector predictor
 */
void get_mvp_default(davs2_t *h, cu_t *p_cu, int pix_x, int pix_y, mv_t *pmv, int bwd_2nd, 
                     int ref_frame, int bsx, int pu_type_for_mvp)
{
    int mvPredType, rFrameL, rFrameU, rFrameUL, rFrameUR;
    mv_t mva, mvb, mvc, mvd;
    int is_available_UR;

    cu_get_neighbors_default_mvp(h, p_cu, pix_x, pix_y, bsx);

    is_available_UR = h->lcu.neighbor_inter[BLK_TOPRIGHT].is_available;

    rFrameL = h->lcu.neighbor_inter[BLK_LEFT       ].ref_idx.r[bwd_2nd];
    rFrameU = h->lcu.neighbor_inter[BLK_TOP      ].ref_idx.r[bwd_2nd];
    rFrameUL = h->lcu.neighbor_inter[BLK_TOPLEFT].ref_idx.r[bwd_2nd];
    rFrameUR = is_available_UR ? h->lcu.neighbor_inter[BLK_TOPRIGHT].ref_idx.r[bwd_2nd] : rFrameUL;

    mva = h->lcu.neighbor_inter[BLK_LEFT   ].mv[bwd_2nd];
    mvb = h->lcu.neighbor_inter[BLK_TOP    ].mv[bwd_2nd];
    mvd = h->lcu.neighbor_inter[BLK_TOPLEFT].mv[bwd_2nd];
    mvc = is_available_UR ? h->lcu.neighbor_inter[BLK_TOPRIGHT].mv[bwd_2nd] : mvd;

    rFrameL  = recheck_neighbor_ref_avail(h, ref_frame, rFrameL);
    rFrameU  = recheck_neighbor_ref_avail(h, ref_frame, rFrameU);
    rFrameUR = recheck_neighbor_ref_avail(h, ref_frame, rFrameUR);

    mvPredType = derive_mv_pred_type(ref_frame, rFrameL, rFrameU, rFrameUR, pu_type_for_mvp);

    if (h->i_frame_type == AVS2_B_SLICE) {
        int mult_distance     = get_distance_index_b(h, bwd_2nd ? B_BWD : B_FWD);
        int mult_distance_src = get_distance_index_b_scale(h, bwd_2nd ? B_BWD : B_FWD);
        check_scaling_neighbor_mv_b(h, &mva, mult_distance, mult_distance_src, rFrameL);
        check_scaling_neighbor_mv_b(h, &mvb, mult_distance, mult_distance_src, rFrameU);
        check_scaling_neighbor_mv_b(h, &mvc, mult_distance, mult_distance_src, rFrameUR);
    } else {
        int mult_distance = get_distance_index_p(h, ref_frame);
        check_scaling_neighbor_mv(h, &mva, mult_distance, rFrameL);
        check_scaling_neighbor_mv(h, &mvb, mult_distance, rFrameU);
        check_scaling_neighbor_mv(h, &mvc, mult_distance, rFrameUR);
    }

    switch (mvPredType) {
    case MVPRED_xy_MIN:
        pmv->x = derive_median_mv(mva.x, mvb.x, mvc.x);  // x
        pmv->y = derive_median_mv(mva.y, mvb.y, mvc.y);  // y
        break;
    case MVPRED_L:
        pmv->v = mva.v;
        break;
    case MVPRED_U:
        pmv->v = mvb.v;
        break;
    default:    // case MVPRED_UR:
        pmv->v = mvc.v;
        break;
    }
}

/* ---------------------------------------------------------------------------
 */
static
void get_mv_bskip_spatial(davs2_t *h, mv_t *fw_pmv, mv_t *bw_pmv, int num_skip_dir)
{
    neighbor_inter_t *p_neighbors = h->lcu.neighbor_inter;
    mv_t *p_mv_1st = h->lcu.mv_tskip_1st;
    mv_t *p_mv_2nd = h->lcu.mv_tskip_2nd;
    int j;
    int bid_flag = 0, bw_flag = 0, fw_flag = 0, sym_flag = 0, bid2 = 0;

    memset(h->lcu.mv_tskip_1st, 0, sizeof(h->lcu.mv_tskip_1st) + sizeof(h->lcu.mv_tskip_2nd));

    for (j = 0; j < 6; j++) {
        if (p_neighbors[j].i_dir_pred == PDIR_BID) {
            p_mv_2nd[DS_B_BID] = p_neighbors[j].mv[1];
            p_mv_1st[DS_B_BID] = p_neighbors[j].mv[0];
            bid_flag++;
            if (bid_flag == 1) {
                bid2 = j;
            }
        } else if (p_neighbors[j].i_dir_pred == PDIR_SYM) {
            p_mv_2nd[DS_B_SYM] = p_neighbors[j].mv[1];
            p_mv_1st[DS_B_SYM] = p_neighbors[j].mv[0];
            sym_flag++;
        } else if (p_neighbors[j].i_dir_pred == PDIR_BWD) {
            p_mv_2nd[DS_B_BWD] = p_neighbors[j].mv[1];
            bw_flag++;
        } else if (p_neighbors[j].i_dir_pred == PDIR_FWD) {
            p_mv_1st[DS_B_FWD] = p_neighbors[j].mv[0];
            fw_flag++;
        }
    }

    if (bid_flag == 0 && fw_flag != 0 && bw_flag != 0) {
        p_mv_2nd[DS_B_BID] = p_mv_2nd[DS_B_BWD];
        p_mv_1st[DS_B_BID] = p_mv_1st[DS_B_FWD ];
    }

    if (sym_flag == 0 && bid_flag > 1) {
        p_mv_2nd[DS_B_SYM] = p_neighbors[bid2].mv[1];
        p_mv_1st[DS_B_SYM] = p_neighbors[bid2].mv[0];
    } else if (sym_flag == 0 && bw_flag != 0) {
        p_mv_2nd[DS_B_SYM].v =  p_mv_2nd[DS_B_BWD].v;
        p_mv_1st[DS_B_SYM].x = -p_mv_2nd[DS_B_BWD].x;
        p_mv_1st[DS_B_SYM].y = -p_mv_2nd[DS_B_BWD].y;
    } else if (sym_flag == 0 && fw_flag != 0) {
        p_mv_2nd[DS_B_SYM].x = -p_mv_1st[DS_B_FWD].x;
        p_mv_2nd[DS_B_SYM].y = -p_mv_1st[DS_B_FWD].y;
        p_mv_1st[DS_B_SYM].v =  p_mv_1st[DS_B_FWD].v;
    }

    if (bw_flag == 0 && bid_flag > 1) {
        p_mv_2nd[DS_B_BWD] = p_neighbors[bid2].mv[1];
    } else if (bw_flag == 0 && bid_flag != 0) {
        p_mv_2nd[DS_B_BWD] = p_mv_2nd[DS_B_BID];
    }

    if (fw_flag == 0 && bid_flag > 1) {
        p_mv_1st[DS_B_FWD] = p_neighbors[bid2].mv[0];
    } else if (fw_flag == 0 && bid_flag != 0) {
        p_mv_1st[DS_B_FWD] = p_mv_1st[DS_B_BID];
    }

    fw_pmv->v = p_mv_1st[num_skip_dir].v;
    bw_pmv->v = p_mv_2nd[num_skip_dir].v;
}

/* ---------------------------------------------------------------------------
 */
static ALWAYS_INLINE
void get_mv_pf_skip_temporal(davs2_t *h, mv_t *p_mv, int block_offset, int cur_dist)
{
    int refframe = h->fref[0]->refbuf[block_offset];

    if (refframe >= 0) {
        mv_t tmv     = h->fref[0]->mvbuf[block_offset];
        int col_dist = h->fref[0]->dist_scale_refs[refframe];

        p_mv->x = scale_mv_skip(h, tmv.x, cur_dist, col_dist);
        p_mv->y = scale_mv_skip(h, tmv.y, cur_dist, col_dist);
    } else {
        p_mv->v = 0;
    }
}

/* ---------------------------------------------------------------------------
 */
static void fill_mv_pf_skip_temporal(davs2_t *h, cu_t *p_cu, int pix_x, int pix_y, int cu_size)
{
    int spu_x = pix_x >> MIN_PU_SIZE_IN_BIT;
    int spu_y = pix_y >> MIN_PU_SIZE_IN_BIT;
    int size_in_spu  = cu_size >> MIN_PU_SIZE_IN_BIT;
    int width_in_spu = h->i_width_in_spu;
    int i, l, m;
    mv_t mv_1st, mv_2nd;
    ref_idx_t ref_idx;
    int delta[AVS2_MAX_REFS];
    int delta_src[AVS2_MAX_REFS];

    ref_idx.r[0] = 0;
    ref_idx.r[1] = (int8_t)(p_cu->i_weighted_skipmode != 0 ? p_cu->i_weighted_skipmode : INVALID_REF);

    for (i = 0; i < h->num_of_references; i++) {
        delta[i] = get_distance_index_p(h, i);
        delta_src[i] = get_distance_index_p_scale(h, i);
    }

    if (cu_size != MIN_CU_SIZE) {
        size_in_spu >>= 1;
        assert(p_cu->num_pu == 4);
    } else {
        assert(p_cu->num_pu == 1);
    }

    for (i = 0; i < p_cu->num_pu; i++) {
        int block_x       = spu_x + size_in_spu * (i  & 1);
        int block_y       = spu_y + size_in_spu * (i >> 1);
        int block_offset  = block_y * width_in_spu + block_x;
        mv_t *p_mv_1st    = h->p_tmv_1st + block_offset;
        mv_t *p_mv_2nd    = h->p_tmv_2nd + block_offset;
        ref_idx_t *p_ref_1st = h->p_ref_idx + block_offset;

        get_mv_pf_skip_temporal(h, &mv_1st, block_offset, delta[0]);

        if (ref_idx.r[1] != INVALID_REF) {
            mv_2nd.x = scale_mv_skip  (h, mv_1st.x, delta[ref_idx.r[1]], delta_src[0]);
            mv_2nd.y = scale_mv_skip_y(h, mv_1st.y, delta[ref_idx.r[1]], delta[0], delta_src[0]);
        } else {
            mv_2nd.v = 0;
        }

        p_cu->mv[i][0].v = mv_1st.v;
        p_cu->mv[i][1].v = mv_2nd.v;
        p_cu->ref_idx[i] = ref_idx;

        for (m = 0; m < size_in_spu; m++) {
            for (l = 0; l < size_in_spu; l++) {
                p_mv_1st[l] = mv_1st;
                p_mv_2nd[l] = mv_2nd;
                p_ref_1st[l] = ref_idx;
            }
            p_mv_1st += width_in_spu;
            p_mv_2nd += width_in_spu;
            p_ref_1st += width_in_spu;
        }
    }
}

/* ---------------------------------------------------------------------------
 */
static INLINE
void get_mv_fskip_spatial(davs2_t *h)
{
    neighbor_inter_t *p_neighbors = h->lcu.neighbor_inter;
    int bid_flag = 0, fw_flag = 0, bid2 = 0, fw2 = 0;
    int j;

    memset(h->lcu.ref_skip_1st, 0, sizeof(h->lcu.ref_skip_1st)
           + sizeof(h->lcu.ref_skip_2nd)
           + sizeof(h->lcu.mv_tskip_1st)
           + sizeof(h->lcu.mv_tskip_2nd));

    for (j = 0; j < 6; j++) {
        if (p_neighbors[j].ref_idx.r[0] != -1 && p_neighbors[j].ref_idx.r[1] != -1) {   // bid
            h->lcu.ref_skip_1st[DS_DUAL_1ST] = p_neighbors[j].ref_idx.r[0];
            h->lcu.ref_skip_2nd[DS_DUAL_1ST] = p_neighbors[j].ref_idx.r[1];
            h->lcu.mv_tskip_1st[DS_DUAL_1ST] = p_neighbors[j].mv[0];
            h->lcu.mv_tskip_2nd[DS_DUAL_1ST] = p_neighbors[j].mv[1];
            bid_flag++;
            if (bid_flag == 1) {
                bid2 = j;
            }
        } else if (p_neighbors[j].ref_idx.r[0] != -1 && p_neighbors[j].ref_idx.r[1] == -1) {  // fw
            h->lcu.ref_skip_1st[DS_SINGLE_1ST] = p_neighbors[j].ref_idx.r[0];
            h->lcu.mv_tskip_1st[DS_SINGLE_1ST] = p_neighbors[j].mv[0];
            fw_flag++;
            if (fw_flag == 1) {
                fw2 = j;
            }
        }
    }

    // first bid
    if (bid_flag == 0 && fw_flag > 1) {
        h->lcu.ref_skip_1st[DS_DUAL_1ST] = h->lcu.ref_skip_1st[DS_SINGLE_1ST];
        h->lcu.ref_skip_2nd[DS_DUAL_1ST] = p_neighbors[fw2].ref_idx.r[0];
        h->lcu.mv_tskip_1st[DS_DUAL_1ST] = h->lcu.mv_tskip_1st[DS_SINGLE_1ST];
        h->lcu.mv_tskip_2nd[DS_DUAL_1ST] = p_neighbors[fw2].mv[0];
    }

    // second bid
    if (bid_flag > 1) {
        h->lcu.ref_skip_1st[DS_DUAL_2ND] = p_neighbors[bid2].ref_idx.r[0];
        h->lcu.ref_skip_2nd[DS_DUAL_2ND] = p_neighbors[bid2].ref_idx.r[1];
        h->lcu.mv_tskip_1st[DS_DUAL_2ND] = p_neighbors[bid2].mv[0];
        h->lcu.mv_tskip_2nd[DS_DUAL_2ND] = p_neighbors[bid2].mv[1];
    } else if (bid_flag == 1 && fw_flag > 1) {
        h->lcu.ref_skip_1st[DS_DUAL_2ND] = h->lcu.ref_skip_1st[DS_SINGLE_1ST];
        h->lcu.ref_skip_2nd[DS_DUAL_2ND] = p_neighbors[fw2].ref_idx.r[0];
        h->lcu.mv_tskip_1st[DS_DUAL_2ND] = h->lcu.mv_tskip_1st[DS_SINGLE_1ST];
        h->lcu.mv_tskip_2nd[DS_DUAL_2ND] = p_neighbors[fw2].mv[0];
    }

    // first fwd
    h->lcu.ref_skip_2nd[DS_SINGLE_1ST] = INVALID_REF;
    h->lcu.mv_tskip_2nd [DS_SINGLE_1ST].v = 0;
    if (fw_flag == 0 && bid_flag > 1) {
        h->lcu.ref_skip_1st[DS_SINGLE_1ST] = p_neighbors[bid2].ref_idx.r[0];
        h->lcu.mv_tskip_1st[DS_SINGLE_1ST] = p_neighbors[bid2].mv[0];
    } else if (fw_flag == 0 && bid_flag == 1) {
        h->lcu.ref_skip_1st[DS_SINGLE_1ST] = h->lcu.ref_skip_1st[DS_DUAL_1ST];
        h->lcu.mv_tskip_1st[DS_SINGLE_1ST] = h->lcu.mv_tskip_1st[DS_DUAL_1ST];
    }

    // second fwd
    h->lcu.ref_skip_2nd[DS_SINGLE_2ND] = INVALID_REF;
    h->lcu.mv_tskip_2nd [DS_SINGLE_2ND].v = 0;
    if (fw_flag > 1) {
        h->lcu.ref_skip_1st[DS_SINGLE_2ND] = p_neighbors[fw2].ref_idx.r[0];
        h->lcu.mv_tskip_1st[DS_SINGLE_2ND] = p_neighbors[fw2].mv[0];
    } else if (bid_flag > 1) {
        h->lcu.ref_skip_1st[DS_SINGLE_2ND] = p_neighbors[bid2].ref_idx.r[1];
        h->lcu.mv_tskip_1st[DS_SINGLE_2ND] = p_neighbors[bid2].mv[1];
    } else if (bid_flag == 1) {
        h->lcu.ref_skip_1st[DS_SINGLE_2ND] = h->lcu.ref_skip_2nd[DS_DUAL_1ST];
        h->lcu.mv_tskip_1st[DS_SINGLE_2ND] = h->lcu.mv_tskip_2nd[DS_DUAL_1ST];
    }
}

/* ---------------------------------------------------------------------------
 */
static void fill_mv_bskip(davs2_t *h, cu_t *p_cu, int pix_x, int pix_y, int size_in_scu)
{
    int width_in_spu = h->i_width_in_spu;
    int i8_1st = pix_x >> MIN_PU_SIZE_IN_BIT;
    int j8_1st = pix_y >> MIN_PU_SIZE_IN_BIT;
    int i;
    int8_t *p_dirpred;
    ref_idx_t *p_ref_1st;
    mv_t *p_mv_1st;
    mv_t *p_mv_2nd;
    mv_t mv_1st, mv_2nd;
    int ds_mode = p_cu->i_md_directskip_mode;

    assert(h->i_frame_type == AVS2_B_SLICE);

    if (ds_mode != DS_NONE) {
        int offset_spu = j8_1st * width_in_spu + i8_1st;
        int r, c;
        int cu_size_in_spu = size_in_scu << (MIN_CU_SIZE_IN_BIT - MIN_PU_SIZE_IN_BIT);
        ref_idx_t ref_idx;
        int8_t  i_dir_pred;

        p_mv_1st  = h->p_tmv_1st + offset_spu;
        p_mv_2nd  = h->p_tmv_2nd + offset_spu;
        p_ref_1st = h->p_ref_idx + offset_spu;
        p_dirpred = h->p_dirpred + offset_spu;
        i_dir_pred = (int8_t)p_cu->b8pdir[0];

        switch (ds_mode) {
        case DS_B_SYM:
        case DS_B_BID:
            ref_idx.r[0] = B_FWD;
            ref_idx.r[1] = B_BWD;
            break;
        case DS_B_BWD:
            ref_idx.r[0] = INVALID_REF;
            ref_idx.r[1] = B_BWD;
            break;
        // case DS_B_FWD:
        default:
            ref_idx.r[0] = B_FWD;
            ref_idx.r[1] = INVALID_REF;
            break;
        }

        get_mv_bskip_spatial(h, &mv_1st, &mv_2nd, p_cu->i_md_directskip_mode);

        p_cu->mv[0][0].v = mv_1st.v;
        p_cu->mv[0][1].v = mv_2nd.v;
        p_cu->ref_idx[0] = ref_idx;

        for (r = 0; r < cu_size_in_spu; r++) {
            for (c = 0; c < cu_size_in_spu; c++) {
                p_ref_1st[c] = ref_idx;

                p_mv_1st [c] = mv_1st;
                p_mv_2nd [c] = mv_2nd;
                p_dirpred[c] = i_dir_pred;
            }
            p_ref_1st += width_in_spu;
            p_mv_1st  += width_in_spu;
            p_mv_2nd  += width_in_spu;
            p_dirpred += width_in_spu;
        }
    } else {   //    B_Skip_Sym 或 B_Direct_Sym
        int size_cu = size_in_scu << MIN_CU_SIZE_IN_BIT;
        int size_pu = size_cu >> (int)(p_cu->num_pu == 4);
        int size_pu_in_spu = size_pu >> MIN_PU_SIZE_IN_BIT;
        ref_idx_t ref_idx;

        ref_idx.r[0] = B_FWD;
        ref_idx.r[1] = B_BWD;

        for (i = 0; i < p_cu->num_pu; i++) {
            int i8 = i8_1st + (i &  1) * size_in_scu;
            int j8 = j8_1st + (i >> 1) * size_in_scu;
            int r, c;
            int offset_spu = j8 * width_in_spu + i8;
            const int8_t *refbuf = h->fref[0]->refbuf;
            int refframe = refbuf[j8 * width_in_spu + i8];

            p_mv_1st  = h->p_tmv_1st + offset_spu;
            p_mv_2nd  = h->p_tmv_2nd + offset_spu;
            p_ref_1st = h->p_ref_idx + offset_spu;
            p_dirpred  = h->p_dirpred + offset_spu;

            if (refframe == -1) {
                get_mvp_default(h, p_cu, pix_x, pix_y, &mv_1st, 0, 0, size_cu, 0);
                get_mvp_default(h, p_cu, pix_x, pix_y, &mv_2nd, 1, 0, size_cu, 0);
            } else { // next P is skip or inter mode
                int iTRp     = h->fref[0]->dist_refs[refframe];
                int iTRp_src = h->fref[0]->dist_scale_refs[refframe];
                int iTRd     = get_distance_index_b(h, B_BWD);  // bwd
                int iTRb     = get_distance_index_b(h, B_FWD);  // fwd
                mv_t tmv = h->fref[0]->mvbuf[j8 * width_in_spu + i8];

                mv_1st.x =  scale_mv_biskip(h, tmv.x, iTRb, iTRp_src);
                mv_2nd.x = -scale_mv_biskip(h, tmv.x, iTRd, iTRp_src);

                mv_1st.y =  scale_mv_biskip_y(h, tmv.y, iTRb, iTRp, iTRp_src);
                mv_2nd.y = -scale_mv_biskip_y(h, tmv.y, iTRd, iTRp, iTRp_src);
            }

            p_cu->mv[i][0].v = mv_1st.v;
            p_cu->mv[i][1].v = mv_2nd.v;
            p_cu->ref_idx[i].v = ref_idx.v;

            for (r = 0; r < size_pu_in_spu; r++) {
                for (c = 0; c < size_pu_in_spu; c++) {
                    p_mv_1st [c] = mv_1st;
                    p_mv_2nd [c] = mv_2nd;

                    p_ref_1st[c].v = ref_idx.v;
                    p_dirpred[c] = PDIR_SYM;
                }
                p_ref_1st += width_in_spu;
                p_mv_1st  += width_in_spu;
                p_mv_2nd  += width_in_spu;
                p_dirpred += width_in_spu;
            }
        }     // for loop all PUs
    }   //    B_Skip_Sym 或 B_Direct_Sym
}

/* ---------------------------------------------------------------------------
 * 在Skip/Direct划分模式下，利用相邻块运动信息设置当前CU内所有块的运动信息
 * 包括设置参考帧索引和运动矢量
 */
void fill_mv_and_ref_for_skip(davs2_t *h, cu_t *p_cu, int pix_x, int pix_y, int size_in_scu)
{
    assert(p_cu->i_cu_type == PRED_SKIP);

    if (h->i_frame_type == AVS2_B_SLICE) {
        fill_mv_bskip(h, p_cu, pix_x, pix_y, size_in_scu);
    } else if ((h->i_frame_type == AVS2_F_SLICE) || (h->i_frame_type == AVS2_P_SLICE)) {
        if (p_cu->i_md_directskip_mode == 0) {
            fill_mv_pf_skip_temporal(h, p_cu, pix_x, pix_y, size_in_scu << MIN_CU_SIZE_IN_BIT);
        } else {
            int width_in_spu = h->i_width_in_spu;
            int block_offset = (pix_y >> MIN_PU_SIZE_IN_BIT) * width_in_spu + (pix_x >> MIN_PU_SIZE_IN_BIT);
            ref_idx_t *p_ref_1st = h->p_ref_idx + block_offset;
            mv_t   *p_tmv_1st = h->p_tmv_1st + block_offset;
            mv_t   *p_tmv_2nd = h->p_tmv_2nd + block_offset;
            int i, j;
            mv_t mv_1st, mv_2nd;
            ref_idx_t ref_idx;
            int ds_mode = p_cu->i_md_directskip_mode;

            get_mv_fskip_spatial(h);

            mv_1st = h->lcu.mv_tskip_1st[ds_mode];
            mv_2nd = h->lcu.mv_tskip_2nd[ds_mode];

            ref_idx.r[0] = h->lcu.ref_skip_1st[ds_mode];
            ref_idx.r[1] = h->lcu.ref_skip_2nd[ds_mode];

            for (i = 0; i < 4; i++) {
                p_cu->mv[i][0].v = mv_1st.v;
                p_cu->mv[i][1].v = mv_2nd.v;
                p_cu->ref_idx[i] = ref_idx;
            }

            size_in_scu <<= (MIN_CU_SIZE_IN_BIT - MIN_PU_SIZE_IN_BIT);

            for (j = 0; j < size_in_scu; j++) {
                for (i = 0; i < size_in_scu; i++) {
                    p_ref_1st[i] = ref_idx;
                    p_tmv_1st[i] = mv_1st;
                    p_tmv_2nd[i] = mv_2nd;
                }
                p_ref_1st += width_in_spu;
                p_tmv_1st += width_in_spu;
                p_tmv_2nd += width_in_spu;
            }
        }
    }
}
