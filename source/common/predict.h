/*
 * predict.h
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
 
#ifndef DAVS2_PRED_H
#define DAVS2_PRED_H
#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * 计算参考帧距离时，归并到有效范围内 */
#define AVS2_DISTANCE_INDEX(distance)    (((distance) + 512) & 511)

/* ---------------------------------------------------------------------------
 * 返回P/F帧的参考帧与当前帧之间的距离 */
static ALWAYS_INLINE
int get_distance_index_p(davs2_t *h, int refidx)
{
    return h->fdec->dist_refs[refidx];
}

/* ---------------------------------------------------------------------------
* 返回P/F帧的参考帧与当前帧之间的距离 */
static ALWAYS_INLINE
int get_distance_index_p_scale(davs2_t *h, int refidx)
{
    return h->fdec->dist_scale_refs[refidx];
}

/* ---------------------------------------------------------------------------
 * 返回B帧的参考帧与当前帧之间的距离 */
static ALWAYS_INLINE
int get_distance_index_b(davs2_t *h, int b_fwd)
{
    return h->fdec->dist_refs[b_fwd];
}

/* ---------------------------------------------------------------------------
* 返回B帧的参考帧与当前帧之间的距离 */
static ALWAYS_INLINE
int get_distance_index_b_scale(davs2_t *h, int b_fwd)
{
    return h->fdec->dist_scale_refs[b_fwd];
}

/* ---------------------------------------------------------------------------
* 用于场编码中Y分量缩放的偏置 */
static ALWAYS_INLINE
int getDeltas(davs2_t *h, int *delt, int *delt2, int OriPOC, int OriRefPOC, int ScaledPOC, int ScaledRefPOC)
{
    int factor = 2;

    *delt = 0;
    *delt2 = 0;

    assert(h->seq_info.b_field_coding);
    assert(h->i_pic_coding_type == FRAME);

    OriPOC       = AVS2_DISTANCE_INDEX(OriPOC);
    OriRefPOC    = AVS2_DISTANCE_INDEX(OriRefPOC);
    ScaledPOC    = AVS2_DISTANCE_INDEX(ScaledPOC);
    ScaledRefPOC = AVS2_DISTANCE_INDEX(ScaledRefPOC);

    assert((OriPOC % factor) + (OriRefPOC % factor) + (ScaledPOC % factor) + (ScaledRefPOC % factor) == 0);

    OriPOC /= factor;
    OriRefPOC /= factor;
    ScaledPOC /= factor;
    ScaledRefPOC /= factor;

    if (h->b_top_field) {  // scaled is top field
        *delt2 = (ScaledRefPOC % 2) != (ScaledPOC % 2) ? 2 : 0;

        if ((ScaledPOC % 2) == (OriPOC % 2)) { // ori is top
            *delt = (OriRefPOC % 2) != (OriPOC % 2) ? 2 : 0;
        } else {
            *delt = (OriRefPOC % 2) != (OriPOC % 2) ? -2 : 0;
        }
    } else { // scaled is bottom field
        *delt2 = (ScaledRefPOC % 2) != (ScaledPOC % 2) ? -2 : 0;
        if ((ScaledPOC % 2) == (OriPOC % 2)) { // ori is bottom
            *delt = (OriRefPOC % 2) != (OriPOC % 2) ? -2 : 0;
        } else {
            *delt = (OriRefPOC % 2) != (OriPOC % 2) ? 2 : 0;
        }
    }

    return 0;
}


/* ---------------------------------------------------------------------------
 * MV scaling for Normal Inter Mode (MVP + MVD) */
static ALWAYS_INLINE
int16_t scale_mv_default(davs2_t *h, int mv, int dist_dst, int dist_src)
{
    UNUSED_PARAMETER(h);
    mv = davs2_sign3(mv) * ((DAVS2_ABS(mv) * dist_dst * dist_src + HALF_MULTI) >> OFFSET);
    return (int16_t)(DAVS2_CLIP3(-32768, 32767, mv));
}

/* ---------------------------------------------------------------------------
 */
static ALWAYS_INLINE
int16_t scale_mv_default_y(davs2_t *h, int mvy, int dist_dst, int dist_src, int dist_src_mul)
{
    if (h->seq_info.b_field_coding) {
        int oriPOC    = h->fdec->i_poc;
        int oriRefPOC = oriPOC - dist_src;
        int scaledPOC = h->fdec->i_poc;
        int scaledRefPOC = scaledPOC - dist_dst;
        int delta, delta2;

        getDeltas(h, &delta, &delta2, oriPOC, oriRefPOC, scaledPOC, scaledRefPOC);
        return (int16_t)(scale_mv_default(h, mvy + delta, dist_dst, dist_src_mul) - delta2);
    } else {
        return scale_mv_default(h, mvy, dist_dst, dist_src_mul);
    }
}

// ----------------------------------------------------------
// MV scaling for Skip/Direct Mode
static ALWAYS_INLINE
int16_t scale_mv_skip(davs2_t *h, int mv, int dist_dst, int dist_src)
{
    UNUSED_PARAMETER(h);
    mv = (int16_t)((mv * dist_dst * dist_src + HALF_MULTI) >> OFFSET);
    return (int16_t)(DAVS2_CLIP3(-32768, 32767, mv));
}

static ALWAYS_INLINE
int16_t scale_mv_skip_y(davs2_t *h, int mvy, int dist_dst, int dist_src ,int dist_src_mul)
{
    if (h->seq_info.b_field_coding) {
        int oriPOC    = h->fdec->i_poc;
        int oriRefPOC = oriPOC - dist_src;
        int scaledPOC = h->fdec->i_poc;
        int scaledRefPOC = scaledPOC - dist_dst;
        int delta, delta2;

        getDeltas(h, &delta, &delta2, oriPOC, oriRefPOC, scaledPOC, scaledRefPOC);
        return (int16_t)(scale_mv_skip(h, mvy + delta, dist_dst, dist_src_mul) - delta2);
    } else {
        return scale_mv_skip(h, mvy, dist_dst, dist_src_mul);
    }
}

// ----------------------------------------------------------
// MV scaling for Bi-Skip/Direct Mode
static ALWAYS_INLINE
int16_t scale_mv_biskip(davs2_t *h, int mv, int dist_dst, int dist_src)
{
    UNUSED_PARAMETER(h);
    mv = (int16_t)(davs2_sign3(mv) * ((dist_src * (1 + DAVS2_ABS(mv) * dist_dst) - 1) >> OFFSET));
    return (int16_t)(DAVS2_CLIP3(-32768, 32767, mv));
}

static ALWAYS_INLINE
int16_t scale_mv_biskip_y(davs2_t *h, int mvy, int dist_dst, int dist_src, int dist_src_mul)
{
    if (h->seq_info.b_field_coding) {
        int oriPOC    = h->fdec->i_poc;
        int oriRefPOC = oriPOC - dist_src;
        int scaledPOC = h->fdec->i_poc;
        int scaledRefPOC = scaledPOC - dist_dst;
        int delta, delta2;

        getDeltas(h, &delta, &delta2, oriPOC, oriRefPOC, scaledPOC, scaledRefPOC);
        return (int16_t)(scale_mv_biskip(h, mvy + delta, dist_dst, dist_src_mul) - delta2);
    } else {
        return scale_mv_biskip(h, mvy, dist_dst, dist_src_mul);
    }
}

/* ---------------------------------------------------------------------------
 */
static ALWAYS_INLINE
void pmvr_mv_derivation(davs2_t *h, mv_t *mv, mv_t *mvd, mv_t *mvp)
{
    int mvx, mvy;

    if (h->seq_info.enable_pmvr) {
        int ctr_x, ctr_y;

        ctr_x = ((mvp->x >> 1) << 1) - mvp->x;
        ctr_y = ((mvp->y >> 1) << 1) - mvp->y;

        if (DAVS2_ABS(mvd->x - ctr_x) > THRESHOLD_PMVR) {
            mvx = mvp->x + (mvd->x << 1) - ctr_x - davs2_sign2(mvd->x - ctr_x) * THRESHOLD_PMVR;
            mvy = mvp->y + (mvd->y << 1) + ctr_y;
        } else if (DAVS2_ABS(mvd->y - ctr_y) > THRESHOLD_PMVR) {
            mvx = mvp->x + (mvd->x << 1) + ctr_x;
            mvy = mvp->y + (mvd->y << 1) - ctr_y - davs2_sign2(mvd->y - ctr_y) * THRESHOLD_PMVR;
        } else {
            mvx = mvd->x + mvp->x;
            mvy = mvd->y + mvp->y;
        }
    } else {
        mvx = mvd->x + mvp->x;
        mvy = mvd->y + mvp->y;
    }

    mv->x = (int16_t)DAVS2_CLIP3(-32768, 32767, mvx);
    mv->y = (int16_t)DAVS2_CLIP3(-32768, 32767, mvy);
}

/* ---------------------------------------------------------------------------
 * get spatial neighboring MV
 */
static ALWAYS_INLINE
void cu_get_neighbor_spatial(davs2_t *h, int cur_slice_idx, neighbor_inter_t *p_neighbor, int x4, int y4)
{
    int b_outside_pic = y4 < 0 || y4 >= h->i_height_in_spu || x4 < 0 || x4 >= h->i_width_in_spu;
    int scu_xy = (y4 >> 1) * h->i_width_in_scu + (x4 >> 1);

    if (b_outside_pic || h->scu_data[scu_xy].i_slice_nr != cur_slice_idx) {
        p_neighbor->is_available = 0;
        p_neighbor->i_dir_pred = PDIR_INVALID;
        p_neighbor->ref_idx.r[0] = INVALID_REF;
        p_neighbor->ref_idx.r[1] = INVALID_REF;
        p_neighbor->mv[0].v = 0;
        p_neighbor->mv[1].v = 0;
    } else {
        const int w_in_4x4 = h->i_width_in_spu;
        const int pos = y4 * w_in_4x4 + x4;
        p_neighbor->is_available = 1;
        p_neighbor->i_dir_pred = h->p_dirpred[pos];
        p_neighbor->ref_idx = h->p_ref_idx[pos];
        p_neighbor->mv[0] = h->p_tmv_1st[pos];
        p_neighbor->mv[1] = h->p_tmv_2nd[pos];
    }
}

/* ---------------------------------------------------------------------------
 * get temporal MV predictor
 */
static ALWAYS_INLINE
void cu_get_neighbor_temporal(davs2_t *h, neighbor_inter_t *p_neighbor, int x4, int y4)
{
    int w_in_16x16 = (h->i_width_in_spu + 3) >> 2;
    int pos = (y4 /*>> 2*/) * w_in_16x16 + (x4 /*>> 2*/);

    p_neighbor->is_available = 1;
    p_neighbor->i_dir_pred = PDIR_FWD;
    p_neighbor->ref_idx.r[0] = h->fref[0]->refbuf[pos];
    p_neighbor->mv[0] = h->fref[0]->mvbuf[pos];

    p_neighbor->ref_idx.r[1] = INVALID_REF;
    p_neighbor->mv[1].v = 0;
}


/* ---------------------------------------------------------------------------
 */
static ALWAYS_INLINE
int get_pu_type_for_mvp(int bsx, int bsy, int cu_pix_x, int cu_pix_y)
{
    if (bsx < bsy) {
        if (cu_pix_x == 0) {
            return 1;
        } else {
            return 2;
        }
    } else if (bsx > bsy) {
        if (cu_pix_y == 0) {
            return 3;
        } else {
            return 4;
        }
    }

    return 0;  // default
}

#define get_mvp_default FPFX(get_mvp_default)
void get_mvp_default(davs2_t *h, cu_t *p_cu, int pix_x, int pix_y, mv_t *pmv, int bwd_2nd, 
                     int ref_frame, int bsx, int pu_type_for_mvp);

#define fill_mv_and_ref_for_skip FPFX(fill_mv_and_ref_for_skip)
void fill_mv_and_ref_for_skip(davs2_t *h, cu_t *p_cu, int pix_x, int pix_y, int size_in_scu);

#ifdef __cplusplus
}
#endif
#endif  // DAVS2_PRED_H
