/*
 * alf.cc
 *
 * Description of this file:
 *    ALF functions definition of the davs2 library
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
#include "alf.h"
#include "aec.h"
#include "vlc.h"
#include "frame.h"

#if HAVE_MMX
#include "vec/intrinsic.h"
#endif


/**
 * ===========================================================================
 * local function defines
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 */
static void alf_recon_coefficients(alf_param_t *p_alf_param, int p_filter_coeff[][ALF_MAX_NUM_COEF])
{
    int num_coeff = p_alf_param->num_coeff - 1;
    int alf_num   = 1 << ALF_NUM_BIT_SHIFT;
    int sum;
    int i, j;

    for (j = 0; j < p_alf_param->filters_per_group; j++) {
        sum = 0;
        for (i = 0; i < num_coeff; i++) {
            sum += (2 * p_alf_param->coeffmulti[j][i]);
            p_filter_coeff[j][i] = p_alf_param->coeffmulti[j][i];
        }
        p_filter_coeff[j][num_coeff] = (alf_num - sum) + p_alf_param->coeffmulti[j][num_coeff];
    }
}

/* ---------------------------------------------------------------------------
 */
static void alf_init_var_table(alf_param_t *p_alf_param, int *p_var_tab)
{
    if (p_alf_param->filters_per_group > 1) {
        int i;

        p_var_tab[0] = 0;
        for (i = 1; i < ALF_NUM_VARS; ++i) {
            p_var_tab[i] = (p_alf_param->filterPattern[i]) ? (p_var_tab[i - 1] + 1) : p_var_tab[i - 1];
        }
    } else {
        memset(p_var_tab, 0, ALF_NUM_VARS * sizeof(int));
    }
}

/* ---------------------------------------------------------------------------
 */
static
void alf_filter_block1(pel_t *p_dst, const pel_t *p_src, int stride,
                       int lcu_pix_x, int lcu_pix_y, int lcu_width, int lcu_height,
                       int *alf_coeff, int b_top_avail, int b_down_avail)
{
    const int pel_add  = 1 << (ALF_NUM_BIT_SHIFT - 1);
    const int pel_max  = max_pel_value;
    const int min_x    = -3;
    const int max_x    = lcu_width - 1 + 3;
    int x, y;
    const pel_t *imgPad1, *imgPad2, *imgPad3, *imgPad4, *imgPad5, *imgPad6;

    {
        int startPos = b_top_avail ? (lcu_pix_y - 4) : lcu_pix_y;
        int endPos = b_down_avail ? (lcu_pix_y + lcu_height - 4) : (lcu_pix_y + lcu_height);
        p_src += (startPos * stride) + lcu_pix_x;
        p_dst += (startPos * stride) + lcu_pix_x;
        lcu_height = endPos - startPos;
        lcu_height--;
    }

    for (y = 0; y <= lcu_height; y++) {
        int yUp, yBottom;
        yUp     = DAVS2_CLIP3(0, lcu_height, y - 1);
        yBottom = DAVS2_CLIP3(0, lcu_height, y + 1);
        imgPad1 = p_src + (yBottom - y) * stride;
        imgPad2 = p_src + (yUp     - y) * stride;

        yUp     = DAVS2_CLIP3(0, lcu_height, y - 2);
        yBottom = DAVS2_CLIP3(0, lcu_height, y + 2);
        imgPad3 = p_src + (yBottom - y) * stride;
        imgPad4 = p_src + (yUp     - y) * stride;

        yUp     = DAVS2_CLIP3(0, lcu_height, y - 3);
        yBottom = DAVS2_CLIP3(0, lcu_height, y + 3);
        imgPad5 = p_src + (yBottom - y) * stride;
        imgPad6 = p_src + (yUp     - y) * stride;

        for (x = 0; x < lcu_width; x++) {
            int xLeft, xRight;
            int pel_val;
            pel_val  = alf_coeff[0] * (imgPad5[x] + imgPad6[x]);
            pel_val += alf_coeff[1] * (imgPad3[x] + imgPad4[x]);

            xLeft    = DAVS2_CLIP3(min_x, max_x, x - 1);
            xRight   = DAVS2_CLIP3(min_x, max_x, x + 1);
            pel_val += alf_coeff[2] * (imgPad1[xRight] + imgPad2[xLeft ]);
            pel_val += alf_coeff[3] * (imgPad1[x     ] + imgPad2[x     ]);
            pel_val += alf_coeff[4] * (imgPad1[xLeft ] + imgPad2[xRight]);
            pel_val += alf_coeff[7] * (p_src [xRight] + p_src [xLeft ]);

            xLeft    = DAVS2_CLIP3(min_x, max_x, x - 2);
            xRight   = DAVS2_CLIP3(min_x, max_x, x + 2);
            pel_val += alf_coeff[6] * (p_src [xRight] + p_src [xLeft ]);

            xLeft    = DAVS2_CLIP3(min_x, max_x, x - 3);
            xRight   = DAVS2_CLIP3(min_x, max_x, x + 3);
            pel_val += alf_coeff[5] * (p_src [xRight] + p_src [xLeft ]);
            pel_val += alf_coeff[8] * (p_src [x     ]);

            pel_val   = (pel_val + pel_add) >> ALF_NUM_BIT_SHIFT;
            p_dst[x] = (pel_t)DAVS2_CLIP3(0, pel_max, pel_val);
        }

        p_src += stride;
        p_dst += stride;
    }
}

/* ---------------------------------------------------------------------------
 */
static
void alf_filter_block2(pel_t *p_dst, const pel_t *p_src, int i_src,
                       int lcu_pix_x, int lcu_pix_y, int lcu_width, int lcu_height,
                       int *alf_coeff, int b_top_avail, int b_down_avail)
{
    const pel_t *p_src1, *p_src2, *p_src3, *p_src4, *p_src5, *p_src6;
    int i_dst = i_src;
    int pixelInt;
    int startPos = b_top_avail ? (lcu_pix_y - 4) : lcu_pix_y;
    int endPos = b_down_avail ? (lcu_pix_y + lcu_height - 4) : (lcu_pix_y + lcu_height);

    /* first line */
    p_src += (startPos * i_src) + lcu_pix_x;
    p_dst += (startPos * i_dst) + lcu_pix_x;

    if (p_src[0] != p_src[-1]) {
        p_src1 = p_src + 1 * i_src;
        p_src2 = p_src;
        p_src3 = p_src + 2 * i_src;
        p_src4 = p_src;
        p_src5 = p_src + 3 * i_src;
        p_src6 = p_src;

        pixelInt  = alf_coeff[0] * (p_src5[ 0] + p_src6[ 0]);
        pixelInt += alf_coeff[1] * (p_src3[ 0] + p_src4[ 0]);
        pixelInt += alf_coeff[2] * (p_src1[ 1] + p_src2[ 0]);
        pixelInt += alf_coeff[3] * (p_src1[ 0] + p_src2[ 0]);
        pixelInt += alf_coeff[4] * (p_src1[-1] + p_src2[ 1]);
        pixelInt += alf_coeff[7] * (p_src [ 1] + p_src [-1]);
        pixelInt += alf_coeff[6] * (p_src [ 2] + p_src [-2]);
        pixelInt += alf_coeff[5] * (p_src [ 3] + p_src [-3]);
        pixelInt += alf_coeff[8] * (p_src [ 0]);

        pixelInt = (int)((pixelInt + 32) >> 6);
        p_dst[0] = (pel_t)DAVS2_CLIP1(pixelInt);
    }

    p_src += lcu_width - 1;
    p_dst += lcu_width - 1;

    if (p_src[0] != p_src[1]) {
        p_src1 = p_src + 1 * i_src;
        p_src2 = p_src;
        p_src3 = p_src + 2 * i_src;
        p_src4 = p_src;
        p_src5 = p_src + 3 * i_src;
        p_src6 = p_src;

        pixelInt  = alf_coeff[0] * (p_src5[ 0] + p_src6[ 0]);
        pixelInt += alf_coeff[1] * (p_src3[ 0] + p_src4[ 0]);
        pixelInt += alf_coeff[2] * (p_src1[ 1] + p_src2[-1]);
        pixelInt += alf_coeff[3] * (p_src1[ 0] + p_src2[ 0]);
        pixelInt += alf_coeff[4] * (p_src1[-1] + p_src2[ 0]);
        pixelInt += alf_coeff[7] * (p_src [ 1] + p_src [-1]);
        pixelInt += alf_coeff[6] * (p_src [ 2] + p_src [-2]);
        pixelInt += alf_coeff[5] * (p_src [ 3] + p_src [-3]);
        pixelInt += alf_coeff[8] * (p_src [ 0]);

        pixelInt = (int)((pixelInt + 32) >> 6);
        p_dst[0] = (pel_t)DAVS2_CLIP1(pixelInt);
    }

    /* last line */
    p_src -= lcu_width - 1;
    p_dst -= lcu_width - 1;
    p_src += ((endPos - startPos - 1) * i_src);
    p_dst += ((endPos - startPos - 1) * i_dst);

    if (p_src[0] != p_src[-1]) {
        p_src1 = p_src;
        p_src2 = p_src - 1 * i_src;
        p_src3 = p_src;
        p_src4 = p_src - 2 * i_src;
        p_src5 = p_src;
        p_src6 = p_src - 3 * i_src;

        pixelInt  = alf_coeff[0] * (p_src5[ 0] + p_src6[ 0]);
        pixelInt += alf_coeff[1] * (p_src3[ 0] + p_src4[ 0]);
        pixelInt += alf_coeff[2] * (p_src1[ 1] + p_src2[-1]);
        pixelInt += alf_coeff[3] * (p_src1[ 0] + p_src2[ 0]);
        pixelInt += alf_coeff[4] * (p_src1[ 0] + p_src2[ 1]);
        pixelInt += alf_coeff[7] * (p_src [ 1] + p_src [-1]);
        pixelInt += alf_coeff[6] * (p_src [ 2] + p_src [-2]);
        pixelInt += alf_coeff[5] * (p_src [ 3] + p_src [-3]);
        pixelInt += alf_coeff[8] * (p_src [ 0]);

        pixelInt = (int)((pixelInt + 32) >> 6);
        p_dst[0] = (pel_t)DAVS2_CLIP1(pixelInt);
    }

    p_src += lcu_width - 1;
    p_dst += lcu_width - 1;

    if (p_src[0] != p_src[1]) {
        p_src1 = p_src;
        p_src2 = p_src - 1 * i_src;
        p_src3 = p_src;
        p_src4 = p_src - 2 * i_src;
        p_src5 = p_src;
        p_src6 = p_src - 3 * i_src;

        pixelInt  = alf_coeff[0] * (p_src5[ 0] + p_src6[ 0]);
        pixelInt += alf_coeff[1] * (p_src3[ 0] + p_src4[ 0]);
        pixelInt += alf_coeff[2] * (p_src1[ 0] + p_src2[-1]);
        pixelInt += alf_coeff[3] * (p_src1[ 0] + p_src2[ 0]);
        pixelInt += alf_coeff[4] * (p_src1[-1] + p_src2[ 1]);
        pixelInt += alf_coeff[7] * (p_src [ 1] + p_src [-1]);
        pixelInt += alf_coeff[6] * (p_src [ 2] + p_src [-2]);
        pixelInt += alf_coeff[5] * (p_src [ 3] + p_src [-3]);
        pixelInt += alf_coeff[8] * (p_src [ 0]);

        pixelInt = (int)((pixelInt + 32) >> 6);
        p_dst[0] = (pel_t)DAVS2_CLIP1(pixelInt);
    }
}

/* ---------------------------------------------------------------------------
 */
static void deriveBoundaryAvail(davs2_t *h, int lcu_xy, int width_in_lcu, int height_in_lcu,
    int *b_top_avail, int *b_down_avail)
{
    *b_top_avail  = (lcu_xy >= width_in_lcu);
    *b_down_avail = (lcu_xy < (height_in_lcu - 1) * width_in_lcu);

    if (!h->seq_info.cross_loop_filter_flag) {
        int width_in_scu = h->i_width_in_scu;
        int lcu_pic_x = (lcu_xy % width_in_lcu) << h->i_lcu_level;
        int lcu_pic_y = (lcu_xy / width_in_lcu) << h->i_lcu_level;
        int scu_xy    = (lcu_pic_y >> MIN_CU_SIZE_IN_BIT) * width_in_scu + (lcu_pic_x >> MIN_CU_SIZE_IN_BIT);
        // int scu_xy_next_row = scu_xy + (1 << (h->i_lcu_level - MIN_CU_SIZE_IN_BIT)) * width_in_scu;
        int slice_idx_top = *b_top_avail ? h->scu_data[scu_xy - width_in_scu].i_slice_nr : -1;
        // int slice_idx_down = *b_down_avail ? h->scu_data[scu_xy_next_row].i_slice_nr : -1;
        int slcie_idx_cur  = h->scu_data[scu_xy].i_slice_nr;

        *b_top_avail  = (slcie_idx_cur == slice_idx_top) ? TRUE : FALSE;
        // *b_down_avail = (slcie_idx_cur == slice_idx_down) ? TRUE : FALSE;
    }
}

/* ---------------------------------------------------------------------------
 */
static void alf_param_init(alf_param_t *alf_par, int cID)
{
    alf_par->num_coeff             = ALF_MAX_NUM_COEF;
    alf_par->filters_per_group     = 1;
    alf_par->componentID           = cID;
    memset(alf_par->filterPattern, 0, sizeof(alf_par->filterPattern));
    memset(alf_par->coeffmulti,    0, sizeof(alf_par->coeffmulti));
}

/**
 * ===========================================================================
 * interface function defines
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 */
size_t alf_get_buffer_size(davs2_t *h)
{
    size_t width_in_lcu  = h->i_width_in_lcu;
    size_t height_in_lcu = h->i_height_in_lcu;

    return  sizeof(alf_var_t) + height_in_lcu * width_in_lcu * sizeof(uint8_t);
}

/* ---------------------------------------------------------------------------
 */
void alf_init_buffer(davs2_t *h)
{
    static const uint8_t regionTable[ALF_NUM_VARS] = { 0, 1, 4, 5, 15, 2, 3, 6, 14, 11, 10, 7, 13, 12, 9, 8 };
    int width_in_lcu  = h->i_width_in_lcu;
    int height_in_lcu = h->i_height_in_lcu;
    int quad_w_in_lcu = ((width_in_lcu  + 1) >> 2);
    int quad_h_in_lcu = ((height_in_lcu + 1) >> 2);
    int region_idx_x;
    int region_idx_y;
    int i, j;
    uint8_t *mem_ptr  = (uint8_t *)h->p_alf;

    h->p_alf->tab_lcu_region = (uint8_t *)(mem_ptr + sizeof(alf_var_t));

    memset(h->p_alf->filterCoeffSym, 0, sizeof(h->p_alf->filterCoeffSym));

    for (j = 0; j < height_in_lcu; j++) {
        region_idx_y = (quad_h_in_lcu == 0) ? 3 : DAVS2_MIN(j / quad_h_in_lcu, 3);
        for (i = 0; i < width_in_lcu; i++) {
            region_idx_x = (quad_w_in_lcu == 0) ? 3 : DAVS2_MIN(i / quad_w_in_lcu, 3);
            h->p_alf->tab_lcu_region[j * width_in_lcu + i] = regionTable[region_idx_y * 4 + region_idx_x];
        }
    }

    for (i = 0; i < IMG_COMPONENTS; i++) {
        alf_param_init(&h->p_alf->img_param[i], i);
    }
}

/* ---------------------------------------------------------------------------
 */
static void vlc_read_alf_coeff(davs2_bs_t *bs, alf_param_t *alf_param)
{
    const int numCoeff = ALF_MAX_NUM_COEF;
    int f, symbol, pre_symbole;
    int pos;

    switch (alf_param->componentID) {
    case IMG_U:
    case IMG_V:
        for (pos = 0; pos < numCoeff; pos++) {
            alf_param->coeffmulti[0][pos] = se_v(bs, "Chroma ALF coefficients");
        }
        break;
    case IMG_Y:
        alf_param->filters_per_group = ue_v(bs, "ALF filter number");
        alf_param->filters_per_group = alf_param->filters_per_group + 1;

        memset(alf_param->filterPattern, 0, ALF_NUM_VARS * sizeof(int));
        pre_symbole = 0;
        symbol = 0;
        for (f = 0; f < alf_param->filters_per_group; f++) {
            if (f > 0) {
                if (alf_param->filters_per_group != 16) {
                    symbol = ue_v(bs, "Region distance");
                } else {
                    symbol = 1;
                }
                alf_param->filterPattern[symbol + pre_symbole] = 1;
                pre_symbole += symbol;
            }

            for (pos = 0; pos < numCoeff; pos++) {
                alf_param->coeffmulti[f][pos] = se_v(bs, "Luma ALF coefficients");
            }
        }
        break;
    default:
        /// Not a legal component ID
        assert(0);
        exit(-1);
    }
}

/* ---------------------------------------------------------------------------
 */
void alf_read_param(davs2_t *h, davs2_bs_t *bs)
{
    if (h->b_alf) {
        h->pic_alf_on[IMG_Y] = u_flag(bs, "alf_pic_flag_Y");
        h->pic_alf_on[IMG_U] = u_flag(bs, "alf_pic_flag_Cb");
        h->pic_alf_on[IMG_V] = u_flag(bs, "alf_pic_flag_Cr");

        if (h->pic_alf_on[0] || h->pic_alf_on[1] || h->pic_alf_on[2]) {
            int component_idx;

            for (component_idx = 0; component_idx < IMG_COMPONENTS; component_idx++) {
                if (h->pic_alf_on[component_idx]) {
                    vlc_read_alf_coeff(bs, &h->p_alf->img_param[component_idx]);
                }
            }
        }
    }
}

/* ---------------------------------------------------------------------------
 * ALF one LCU block
 */
static void alf_lcu_block(davs2_t *h, alf_param_t *p_alf_param, davs2_frame_t *p_tmp_frm, davs2_frame_t *p_dec_frm, int i_lcu_x, int i_lcu_y)
{
    int lcu_size      = h->i_lcu_size;
    int img_height    = h->i_height;
    int img_width     = h->i_width;
    int width_in_lcu  = h->i_width_in_lcu;
    int height_in_lcu = h->i_height_in_lcu;
    int lcu_pix_x     = i_lcu_x << h->i_lcu_level;
    int lcu_pix_y     = i_lcu_y << h->i_lcu_level;
    int lcu_width     = (lcu_pix_x + lcu_size > img_width ) ? (img_width  - lcu_pix_x) : lcu_size;
    int lcu_height    = (lcu_pix_y + lcu_size > img_height) ? (img_height - lcu_pix_y) : lcu_size;
    int lcu_xy        = i_lcu_y * width_in_lcu + i_lcu_x;
    int b_top_avail, b_down_avail;
    int lcu_region_idx = h->p_alf->tab_lcu_region[lcu_xy];
    int *alf_coef;

    // derive CTU boundary availabilities
    deriveBoundaryAvail(h, lcu_xy, width_in_lcu, height_in_lcu, &b_top_avail, &b_down_avail);

    if (h->lcu_infos[lcu_xy].enable_alf[0]) {
        alf_init_var_table(&p_alf_param[0], h->p_alf->tab_region_coeff_idx);

        // reconstruct ALF coefficients & related parameters
        alf_recon_coefficients(&p_alf_param[0], h->p_alf->filterCoeffSym);
        alf_coef = h->p_alf->filterCoeffSym[h->p_alf->tab_region_coeff_idx[lcu_region_idx]];

        gf_davs2.alf_block[0](p_dec_frm->planes[0], p_tmp_frm->planes[0], p_dec_frm->i_stride[0],
            lcu_pix_x, lcu_pix_y, lcu_width, lcu_height,
            alf_coef, b_top_avail, b_down_avail);
        gf_davs2.alf_block[1](p_dec_frm->planes[0], p_tmp_frm->planes[0], p_dec_frm->i_stride[0],
            lcu_pix_x, lcu_pix_y, lcu_width, lcu_height,
            alf_coef, b_top_avail, b_down_avail);
    }

    lcu_pix_x  >>= 1;
    lcu_pix_y  >>= 1;
    lcu_width  >>= 1;
    lcu_height >>= 1;
    if (h->lcu_infos[lcu_xy].enable_alf[1]) {
        // reconstruct ALF coefficients & related parameters
        alf_recon_coefficients(&p_alf_param[1], h->p_alf->filterCoeffSym);
        alf_coef = h->p_alf->filterCoeffSym[0];

        gf_davs2.alf_block[0](p_dec_frm->planes[1], p_tmp_frm->planes[1], p_dec_frm->i_stride[1],
            lcu_pix_x, lcu_pix_y, lcu_width, lcu_height,
            alf_coef, b_top_avail, b_down_avail);
        gf_davs2.alf_block[1](p_dec_frm->planes[1], p_tmp_frm->planes[1], p_dec_frm->i_stride[1],
            lcu_pix_x, lcu_pix_y, lcu_width, lcu_height,
            alf_coef, b_top_avail, b_down_avail);
    }

    if (h->lcu_infos[lcu_xy].enable_alf[2]) {
        // reconstruct ALF coefficients & related parameters
        alf_recon_coefficients(&p_alf_param[2], h->p_alf->filterCoeffSym);
        alf_coef = h->p_alf->filterCoeffSym[0];

        gf_davs2.alf_block[0](p_dec_frm->planes[2], p_tmp_frm->planes[2], p_dec_frm->i_stride[2],
            lcu_pix_x, lcu_pix_y, lcu_width, lcu_height,
            alf_coef, b_top_avail, b_down_avail);
        gf_davs2.alf_block[1](p_dec_frm->planes[2], p_tmp_frm->planes[2], p_dec_frm->i_stride[2],
            lcu_pix_x, lcu_pix_y, lcu_width, lcu_height,
            alf_coef, b_top_avail, b_down_avail);
    }
}

/* ---------------------------------------------------------------------------
 */
void alf_lcurow(davs2_t *h, alf_param_t *p_alf_param, davs2_frame_t *p_tmp_frm, davs2_frame_t *p_dec_frm, int i_lcu_y)
{
    const int w_in_lcu = h->i_width_in_lcu;
    int i_lcu_x;

    /* copy one decoded LCU-row (with padding left and right edges) */
    davs2_frame_copy_lcurow(h, p_tmp_frm, p_dec_frm, i_lcu_y, -4, 8);

    /* ALF one LCU-row */
    for (i_lcu_x = 0; i_lcu_x < w_in_lcu; i_lcu_x++) {
        alf_lcu_block(h, p_alf_param, p_tmp_frm, p_dec_frm, i_lcu_x, i_lcu_y);
    }
}

/* ---------------------------------------------------------------------------
 */
void davs2_alf_init(uint32_t cpuid, ao_funcs_t *fh)
{
    UNUSED_PARAMETER(cpuid);

    /* init c function handles */
    fh->alf_block[0] = alf_filter_block1;
    fh->alf_block[1] = alf_filter_block2;

    /* init asm function handles */
#if HAVE_MMX
#if HIGH_BIT_DEPTH
#else
    if (cpuid & DAVS2_CPU_SSE4) {
        fh->alf_block[0] = alf_filter_block_sse128;
    }
#endif
#endif
}
