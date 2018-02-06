/*
 * intra.cc
 *
 * Description of this file:
 *    Intra prediction functions definition of the davs2 library
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
#include "block_info.h"
#include "intra.h"
#include "vec/intrinsic.h"

// ---------------------------------------------------------------------------
// disable warning
#if defined(_MSC_VER) || defined(__ICL)
#pragma warning(disable: 4100)  // unreferenced formal parameter
#endif

/*
 * ===========================================================================
 * global & local variable defines
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 */
static const int8_t g_aucXYflg[NUM_INTRA_MODE] = {
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    1, 1, 1, 1, 1,
    1, 1, 1
};

/* ---------------------------------------------------------------------------
 */
static const int8_t tab_auc_dir_dx[NUM_INTRA_MODE] = {
     0,  0,  0, 11,  2,
    11,  1,  8,  1,  4,
     1,  1,  0,  1,  1,
     4,  1,  8,  1, 11,
     2, 11,  4,  8,  0,
     8,  4, 11,  2, 11,
     1,  8,  1
};

/* ---------------------------------------------------------------------------
 */
static const int8_t tab_auc_dir_dy[NUM_INTRA_MODE] = {
     0,   0,   0, -4,  -1,
    -8,  -1, -11, -2, -11,
    -4,  -8,   0,  8,   4,
    11,   2,  11,  1,   8,
     1,   4,   1,  1,   0,
    -1,  -1,  -4, -1,  -8,
    -1, -11,  -2
};

/* ---------------------------------------------------------------------------
 */
static const int8_t g_aucSign[NUM_INTRA_MODE] = {
     0,  0,  0, -1, -1,
    -1, -1, -1, -1, -1,
    -1, -1,  0,  1,  1,
     1,  1,  1,  1,  1,
     1,  1,  1,  1,  0,
    -1, -1, -1, -1, -1,
    -1, -1, -1
};

/* ---------------------------------------------------------------------------
 */
static const int8_t tab_auc_dir_dxdy[2][NUM_INTRA_MODE][2] = {
    {
        // dx/dy
        {  0, 0 }, {  0, 0 }, {  0, 0 }, { 11, 2 }, {  2, 0 },
        { 11, 3 }, {  1, 0 }, { 93, 7 }, {  1, 1 }, { 93, 8 },
        {  1, 2 }, {  1, 3 }, {  0, 0 }, {  1, 3 }, {  1, 2 },
        { 93, 8 }, {  1, 1 }, { 93, 7 }, {  1, 0 }, { 11, 3 },
        {  2, 0 }, { 11, 2 }, {  4, 0 }, {  8, 0 }, {  0, 0 },
        {  8, 0 }, {  4, 0 }, { 11, 2 }, {  2, 0 }, { 11, 3 },
        {  1, 0 }, { 93, 7 }, {  1, 1 },
    }, {
        // dy/dx
        {  0, 0 }, {  0, 0 }, {  0, 0 }, { 93, 8 }, {  1, 1 },
        { 93, 7 }, {  1, 0 }, { 11, 3 }, {  2, 0 }, { 11, 2 },
        {  4, 0 }, {  8, 0 }, {  0, 0 }, {  8, 0 }, {  4, 0 },
        { 11, 2 }, {  2, 0 }, { 11, 3 }, {  1, 0 }, { 93, 7 },
        {  1, 1 }, { 93, 8 }, {  1, 2 }, {  1, 3 }, {  0, 0 },
        {  1, 3 }, {  1, 2 }, { 93, 8 }, {  1, 1 }, { 93, 7 },
        {  1, 0 }, { 11, 3 }, {  2, 0 }
    }
};

/* ---------------------------------------------------------------------------
 */
static const int8_t tab_log2size[MAX_CU_SIZE + 1] = {
    -1, -1, -1, -1,  2, -1, -1, -1,
     3, -1, -1, -1, -1, -1, -1, -1,
     4, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
     5, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    6
};

/* ---------------------------------------------------------------------------
 */
const int8_t tab_DL_Avail64[16 * 16] = {
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
    1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
    1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
    1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
    1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* ---------------------------------------------------------------------------
 */
const int8_t tab_DL_Avail32[8 * 8] = {
    1, 0, 1, 0, 1, 0, 1, 0,
    1, 0, 0, 0, 1, 0, 0, 0,
    1, 0, 1, 0, 1, 0, 1, 0,
    1, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 1, 0, 1, 0, 1, 0,
    1, 0, 0, 0, 1, 0, 0, 0,
    1, 0, 1, 0, 1, 0, 1, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

/* ---------------------------------------------------------------------------
 */
const int8_t tab_DL_Avail16[4 * 4] = {
    1, 0, 1, 0,
    1, 0, 0, 0,
    1, 0, 1, 0,
    0, 0, 0, 0
};

/* ---------------------------------------------------------------------------
 */
const int8_t tab_DL_Avail8[2 * 2] = {
    1, 0,
    0, 0
};


/* ---------------------------------------------------------------------------
 */
const int8_t tab_TR_Avail64[16 * 16] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
    1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
    1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
    1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
    1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
    1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
    1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0
};

/* ---------------------------------------------------------------------------
 */
const int8_t tab_TR_Avail32[8 * 8] = {
    // 0: 8 1:16 2: 32  pu size
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0,
    1, 1, 1, 0, 1, 1, 1, 0,
    1, 0, 1, 0, 1, 0, 1, 0,
    1, 1, 1, 1, 1, 1, 1, 0,
    1, 0, 1, 0, 1, 0, 1, 0,
    1, 1, 1, 0, 1, 1, 1, 0,
    1, 0, 1, 0, 1, 0, 1, 0
};

/* ---------------------------------------------------------------------------
 */
const int8_t tab_TR_Avail16[4 * 4] = {
    1, 1, 1, 1,
    1, 0, 1, 0,
    1, 1, 1, 0,
    1, 0, 1, 0
};

/* ---------------------------------------------------------------------------
 */
const int8_t tab_TR_Avail8[2 * 2] = {
    1, 1,
    1, 0
};

/* ---------------------------------------------------------------------------
 */
const int8_t *tab_DL_Avails[MAX_CU_SIZE_IN_BIT + 1] = {
    NULL, NULL, NULL, tab_DL_Avail8, tab_DL_Avail16, tab_DL_Avail32, tab_DL_Avail64
};


/* ---------------------------------------------------------------------------
 */
const int8_t *tab_TR_Avails[MAX_CU_SIZE_IN_BIT + 1] = {
    NULL, NULL, NULL, tab_TR_Avail8, tab_TR_Avail16, tab_TR_Avail32, tab_TR_Avail64
};


/* records the sample bit depth for intra predeiction
 */

/**
 * ===========================================================================
 * local function definition
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 */
static ALWAYS_INLINE int is_block_available(davs2_t *h, int x_4x4, int y_4x4, int dx_4x4, int dy_4x4, int cur_slice_idx)
{
    int x2_4x4 = x_4x4 + dx_4x4;
    int y2_4x4 = y_4x4 + dy_4x4;

    if (x2_4x4 < 0 || y2_4x4 < 0 || x2_4x4 >= h->i_width_in_spu || y2_4x4 >= h->i_height_in_spu) {
        return 0;
    } else {
        return cur_slice_idx == h->scu_data[(y2_4x4 >> 1) * h->i_width_in_scu + (x2_4x4 >> 1)].i_slice_nr;
    }
}

/* ---------------------------------------------------------------------------
 */
static ALWAYS_INLINE
uint32_t get_intra_neighbors(davs2_t *h, int x_4x4, int y_4x4, int bsx, int bsy, int cur_slice_idx)
{
    /* 1. 检查相邻块是否属于同一个Slice */
    int b_LEFT      = is_block_available(h, x_4x4, y_4x4, -1, 0, cur_slice_idx);
    int b_TOP       = is_block_available(h, x_4x4, y_4x4,  0, -1, cur_slice_idx);
    int b_TOP_LEFT  = is_block_available(h, x_4x4, y_4x4, -1, -1, cur_slice_idx);
    int b_LEFT_DOWN = is_block_available(h, x_4x4, y_4x4, -1, (bsy >> 1) - 1, cur_slice_idx);  // (bsy >> MIN_PU_SIZE_IN_BIT << 1)
    int b_TOP_RIGHT = is_block_available(h, x_4x4, y_4x4, (bsx >> 1) - 1, -1, cur_slice_idx);  // (bsx >> MIN_PU_SIZE_IN_BIT << 1)

    int leftdown;
    int upright;
    int log2_lcu_size_in_spu = (h->i_lcu_level - B4X4_IN_BIT);
    int i_lcu_mask = (1 << log2_lcu_size_in_spu) - 1;

    /* 2. 检查相邻块是否在当前块之前重构 */
    x_4x4 = x_4x4 & i_lcu_mask;
    y_4x4 = y_4x4 & i_lcu_mask;

    leftdown = h->p_tab_DL_avail[((y_4x4 + (bsy >> 2) - 1) << log2_lcu_size_in_spu) + (x_4x4)];
    upright  = h->p_tab_TR_avail[((y_4x4) << log2_lcu_size_in_spu) + (x_4x4 + (bsx >> 2) - 1)];

    b_LEFT_DOWN = b_LEFT_DOWN && leftdown;
    b_TOP_RIGHT = b_TOP_RIGHT && upright;

    return (b_LEFT << MD_I_LEFT) | (b_TOP << MD_I_TOP) | (b_TOP_LEFT << MD_I_TOP_LEFT) |
        (b_TOP_RIGHT << MD_I_TOP_RIGHT) | (b_LEFT_DOWN << MD_I_LEFT_DOWN);
}

/* ---------------------------------------------------------------------------
 */
static void ALWAYS_INLINE mem_repeat_p(pel_t *dst, pel_t val, size_t num)
{
    while (num--) {
        *dst++ = val;
    }
}

/* ---------------------------------------------------------------------------
 */
static void ALWAYS_INLINE memcpy_vh_pp_c(pel_t *dst, pel_t *src, int i_src, size_t num)
{
    while (num--) {
        *dst++ = *src;
        src += i_src;
    }
}

/* ---------------------------------------------------------------------------
 */
static void intra_pred_ver_c(pel_t *src, pel_t *dst, int i_dst, int mode, int width, int height)
{
    pel_t *p_src = src + 1;
    int y;

    UNUSED_PARAMETER(mode);

    for (y = height; y != 0; y--) {
        memcpy(dst, p_src, width * sizeof(pel_t));
        dst += i_dst;
    }
}

/* ---------------------------------------------------------------------------
 */
static void intra_pred_hor_c(pel_t *src, pel_t *dst, int i_dst, int mode, int width, int height)
{
    pel_t *p_src = src - 1;
    int x, y;

    UNUSED_PARAMETER(mode);

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            dst[x] = p_src[-y];
        }
        dst += i_dst;
    }
}

/* ---------------------------------------------------------------------------
 */
static void intra_pred_dc_c(pel_t *src, pel_t *dst, int i_dst, int mode, int width, int height)
{
    int b_top  = mode >> 8;
    int b_left = mode & 0xFF;
    pel_t *p_src = src - 1;
    int dc_value = 0;
    int x, y;

    /* get DC value */
    if (b_left) {
        for (y = 0; y < height; y++) {
            dc_value += p_src[-y];
        }

        p_src = src + 1;
        if (b_top) {
            for (x = 0; x < width; x++) {
                dc_value += p_src[x];
            }

            dc_value += ((width + height) >> 1);
            dc_value = (dc_value * (512 / (width + height))) >> 9;
        } else {
            dc_value += height / 2;
            dc_value /= height;
        }
    } else {
        p_src = src + 1;
        if (b_top) {
            for (x = 0; x < width; x++) {
                dc_value += p_src[x];
            }

            dc_value += width / 2;
            dc_value /= width;
        } else {
            dc_value = 1 << (g_bit_depth - 1);
        }
    }

    /* fill the block */
    x        = (1 << g_bit_depth) - 1;     /* max pixel value */
    dc_value = DAVS2_CLIP3(0, x, dc_value);
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            dst[x] = (pel_t)dc_value;
        }
        dst += i_dst;
    }
}

/* ---------------------------------------------------------------------------
 */
static void intra_pred_plane_c(pel_t *src, pel_t *dst, int i_dst, int mode, int width, int height)
{
    int ib_mult[5] = { 13, 17, 5, 11, 23 };
    int ib_shift[5] = { 7, 10, 11, 15, 19 };
    int im_h = ib_mult [tab_log2size[width ] - 2];
    int im_v = ib_mult [tab_log2size[height] - 2];
    int is_h = ib_shift[tab_log2size[width ] - 2];
    int is_v = ib_shift[tab_log2size[height] - 2];
    int iW2 = width >> 1;
    int iH2 = height >> 1;
    int iH = 0;
    int iV = 0;
    int iA, iB, iC;
    int x, y;
    int iTmp, iTmp2;
    int max_val = (1 << g_bit_depth) - 1;
    pel_t *p_src;

    UNUSED_PARAMETER(mode);

    p_src = src + 1;
    p_src += (iW2 - 1);
    for (x = 1; x < iW2 + 1; x++) {
        iH += x * (p_src[x] - p_src[-x]);
    }

    p_src = src - 1;
    p_src -= (iH2 - 1);
    for (y = 1; y < iH2 + 1; y++) {
        iV += y * (p_src[-y] - p_src[y]);
    }

    p_src = src;
    iA = (p_src[-1 - (height - 1)] + p_src[1 + width - 1]) << 4;
    iB = ((iH << 5) * im_h + (1 << (is_h - 1))) >> is_h;
    iC = ((iV << 5) * im_v + (1 << (is_v - 1))) >> is_v;

    iTmp = iA - (iH2 - 1) * iC - (iW2 - 1) * iB + 16;
    for (y = 0; y < height; y++) {
        iTmp2 = iTmp;
        for (x = 0; x < width; x++) {
            dst[x] = (pel_t)DAVS2_CLIP3(0, max_val, iTmp2 >> 5);
            iTmp2 += iB;
        }

        dst += i_dst;
        iTmp += iC;
    }
}

/* ---------------------------------------------------------------------------
 */
static void intra_pred_bilinear_c(pel_t *src, pel_t *dst, int i_dst, int mode, int width, int height)
{
    itr_t pTop[MAX_CU_SIZE];
    itr_t pLeft[MAX_CU_SIZE];
    itr_t pT[MAX_CU_SIZE];
    itr_t pL[MAX_CU_SIZE];
    itr_t wy[MAX_CU_SIZE];
    int ishift_x  = tab_log2size[width];
    int ishift_y  = tab_log2size[height];
    int ishift    = DAVS2_MIN(ishift_x, ishift_y);
    int ishift_xy = ishift_x + ishift_y + 1;
    int offset    = 1 << (ishift_x + ishift_y);
    int a, b, c, w, wxy, t;
    int predx;
    int x, y;
    int max_value = (1 << g_bit_depth) - 1;

    UNUSED_PARAMETER(mode);

    for (x = 0; x < width; x++) {
        pTop[x] = src[1 + x];
    }

    for (y = 0; y < height; y++) {
        pLeft[y] = src[-1 - y];
    }

    a = pTop[width - 1];
    b = pLeft[height - 1];
    c = (width == height) ? (a + b + 1) >> 1 : (((a << ishift_x) + (b << ishift_y)) * 13 + (1 << (ishift + 5))) >> (ishift + 6);
    w = (c << 1) - a - b;

    for (x = 0; x < width; x++) {
        pT[x] = (itr_t)(b - pTop[x]);
        pTop[x] <<= ishift_y;
    }
    t = 0;
    for (y = 0; y < height; y++) {
        pL[y] = (itr_t)(a - pLeft[y]);
        pLeft[y] <<= ishift_x;
        wy[y] = (itr_t)t;
        t += w;
    }

    for (y = 0; y < height; y++) {
        predx = pLeft[y];
        wxy = -wy[y];

        for (x = 0; x < width; x++) {
            predx += pL[y];
            wxy += wy[y];
            pTop[x] += pT[x];
            dst[x] = (pel_t)DAVS2_CLIP3(0, max_value, (((predx << ishift_y) + (pTop[x] << ishift_x) + wxy + offset) >> ishift_xy));
        }

        dst += i_dst;
    }
}

/* ---------------------------------------------------------------------------
 */
static int get_context_pixel(int mode, int uiXYflag, int iTempD, int *offset)
{
    int imult = tab_auc_dir_dxdy[uiXYflag][mode][0];
    int ishift = tab_auc_dir_dxdy[uiXYflag][mode][1];
    int iTempDn = iTempD * imult >> ishift;

    *offset = ((iTempD * imult * 32) >> ishift) - iTempDn * 32;

    return iTempDn;
}

/* ---------------------------------------------------------------------------
 */
static void intra_pred_ang_x_c(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy)
{
    int iDx = tab_auc_dir_dx[dir_mode];
    int iDy = tab_auc_dir_dy[dir_mode];
#if BUGFIX_PREDICTION_INTRA
    int iX;
#else
    int top_width = bsx - iDx;
    int iW2 = (bsx << 1) - 1;
    int iX, idx;
#endif
    int c1, c2, c3, c4;
    int i, j;
    pel_t *dst_base = dst + iDy * i_dst + iDx;

    for (j = 0; j < bsy; j++, iDy++) {
        iX = get_context_pixel(dir_mode, 0, j + 1, &c4);
        c1 = 32 - c4;
        c2 = 64 - c4;
        c3 = 32 + c4;

#if BUGFIX_PREDICTION_INTRA
        i = 0;
#else
        if (iDy >= 0 && top_width > 0) {
            memcpy(dst, dst_base, top_width * sizeof(pel_t));
            i = top_width;
            iX += top_width;
        } else {
            i = 0;
        }
#endif

        for (; i < bsx; i++) {
#if BUGFIX_PREDICTION_INTRA
            dst[i] = (pel_t)((src[iX] * c1 + src[iX + 1] * c2 + src[iX + 2] * c3 + src[iX + 3] * c4 + 64) >> 7);
#else
            idx = DAVS2_MIN(iW2, iX);
            dst[i] = (pel_t)((src[idx] * c1 + src[idx + 1] * c2 + src[idx + 2] * c3 + src[idx + 3] * c4 + 64) >> 7);
#endif
            iX++;
        }

        dst_base += i_dst;
        dst += i_dst;
    }
}


/* ---------------------------------------------------------------------------
 */
static void intra_pred_ang_y_c(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy)
{
    int offsets[64];
    int xsteps[64];
    int iDx = tab_auc_dir_dx[dir_mode];
    int iDy = tab_auc_dir_dy[dir_mode];
#if !BUGFIX_PREDICTION_INTRA
    int iHeight2 = 1 - (bsy << 1);
    int top_width = bsx - iDx;
#endif
    int i, j;
    int offset;
    int iY;
    pel_t *dst_base = dst + iDy * i_dst + iDx;

    for (i = 0; i < bsx; i++) {
        xsteps[i] = get_context_pixel(dir_mode, 1, i + 1, &offsets[i]);
    }

    for (j = 0; j < bsy; j++) {
        for (i = 0; i < bsx; i++) {
#if !BUGFIX_PREDICTION_INTRA
            if (j >= -iDy && i < top_width) {
                dst[i] = dst_base[i];
            } else {
#endif
                int idx;
                iY = j + xsteps[i];
#if BUGFIX_PREDICTION_INTRA
                idx = -iY;
#else
                idx = DAVS2_MAX(iHeight2, -iY);
#endif
                offset = offsets[i];
                dst[i] = (pel_t)((src[idx] * (32 - offset) + src[idx - 1] * (64 - offset) + src[idx - 2] * (32 + offset) + src[idx - 3] * offset + 64) >> 7);
#if !BUGFIX_PREDICTION_INTRA
            }
#endif
        }
        dst_base += i_dst;
        dst += i_dst;
    }
}


/* ---------------------------------------------------------------------------
 */
static void intra_pred_ang_xy_c(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy)
{
    ALIGN16(int xoffsets[64]);
    ALIGN16(int xsteps[64]);
    const int iDx = tab_auc_dir_dx[dir_mode];
    const int iDy = tab_auc_dir_dy[dir_mode];
    pel_t *dst_base = dst - iDy * i_dst - iDx;
    int i, j, iXx, iYy;
    int offsetx, offsety;

    for (i = 0; i < bsx; i++) {
        xsteps[i] = get_context_pixel(dir_mode, 1, i + 1, &xoffsets[i]);
    }

    for (j = 0; j < bsy; j++) {
        iXx = -get_context_pixel(dir_mode, 0, j + 1, &offsetx);

        for (i = 0; i < bsx; i++) {
#if !BUGFIX_PREDICTION_INTRA
            if (j >= iDy && i >= iDx) {
                dst[i] = dst_base[i];
            } else {
#endif
                iYy = j - xsteps[i];

                if (iYy <= -1) {
                    dst[i] = (pel_t)((src[ iXx + 2] * (32 - offsetx) + src[ iXx + 1] * (64 - offsetx) + src[ iXx] * (32 + offsetx) + src[ iXx - 1] * offsetx + 64) >> 7);
                } else {
                    offsety = xoffsets[i];
                    dst[i] = (pel_t)((src[-iYy - 2] * (32 - offsety) + src[-iYy - 1] * (64 - offsety) + src[-iYy] * (32 + offsety) + src[-iYy + 1] * offsety + 64) >> 7);
                }
#if !BUGFIX_PREDICTION_INTRA
            }
#endif
            iXx++;
        }
        dst_base += i_dst;
        dst      += i_dst;
    }
}

/* ---------------------------------------------------------------------------
 */
static void intra_pred_ang_x_3_c(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy)
{
    ALIGN16(pel_t first_line[(64 + 176) << 2]);
    int line_size = bsx + (bsy >> 2) * 11 - 1;
#if !BUGFIX_PREDICTION_INTRA
    int real_size = STARAVS_MIN(line_size, bsx * 2);
#endif

    int aligned_line_size = 64 + 176;
    int i_dst4 = i_dst << 2;
    int i;
#if !BUGFIX_PREDICTION_INTRA
    pel_t pad1, pad2, pad3, pad4;
#endif
    pel_t *pfirst[4];

    pfirst[0] = first_line;
    pfirst[1] = pfirst[0] + aligned_line_size;
    pfirst[2] = pfirst[1] + aligned_line_size;
    pfirst[3] = pfirst[2] + aligned_line_size;

#if BUGFIX_PREDICTION_INTRA
    for (i = 0; i < line_size; i++, src++) {
#else
    for (i = 0; i < real_size; i++, src++) {
#endif
        pfirst[0][i] = (pel_t)((    src[2] + 5 * src[3] + 7 * src[4] + 3 * src[5] + 8) >> 4);
        pfirst[1][i] = (pel_t)((    src[5] + 3 * src[6] + 3 * src[7] +     src[8] + 4) >> 3);
        pfirst[2][i] = (pel_t)((3 * src[8] + 7 * src[9] + 5 * src[10] +     src[11] + 8) >> 4);
        pfirst[3][i] = (pel_t)((    src[11] + 2 * src[12] +   src[13] + 0 * src[14] + 2) >> 2);
    }

#if !BUGFIX_PREDICTION_INTRA
    // padding
    if (real_size < line_size) {
        int iW2 = (bsx << 1) - 1;
        int j;

        src -= real_size;

        pad1 = (pel_t)((    src[iW2] + 5 * src[iW2 + 1] + 7 * src[iW2 + 2] + 3 * src[iW2 + 3] + 8) >> 4);
        pad2 = (pel_t)((    src[iW2] + 3 * src[iW2 + 1] + 3 * src[iW2 + 2] +     src[iW2 + 3] + 4) >> 3);
        pad3 = (pel_t)((3 * src[iW2] + 7 * src[iW2 + 1] + 5 * src[iW2 + 2] +     src[iW2 + 3] + 8) >> 4);
        pad4 = (pel_t)((    src[iW2] + 2 * src[iW2 + 1] +     src[iW2 + 2] + 0 * src[iW2 + 3] + 2) >> 2);

        for (j = real_size - 1; j > iW2 - 2; j--) {
            pfirst[3][j] = pad4;
            pfirst[2][j] = pad3;
            pfirst[1][j] = pad2;
            pfirst[0][j] = pad1;
        }
        for (; j > iW2 - 5; j--) {
            pfirst[3][j] = pad4;
            pfirst[2][j] = pad3;
            pfirst[1][j] = pad2;
        }
        for (; j > iW2 - 8; j--) {
            pfirst[3][j] = pad4;
            pfirst[2][j] = pad3;
        }
        for (; j > iW2 - 11; j--) {
            pfirst[3][j] = pad4;
        }

        for (; i < line_size; i++) {
            pfirst[0][i] = pad1;
            pfirst[1][i] = pad2;
            pfirst[2][i] = pad3;
            pfirst[3][i] = pad4;
        }
    }
#endif

    bsy >>= 2;
    for (i = 0; i < bsy; i++) {
        memcpy(dst            , pfirst[0] + i * 11, bsx * sizeof(pel_t));
        memcpy(dst +     i_dst, pfirst[1] + i * 11, bsx * sizeof(pel_t));
        memcpy(dst + 2 * i_dst, pfirst[2] + i * 11, bsx * sizeof(pel_t));
        memcpy(dst + 3 * i_dst, pfirst[3] + i * 11, bsx * sizeof(pel_t));
        dst += i_dst4;
    }
}

/* ---------------------------------------------------------------------------
 */
static void intra_pred_ang_x_4_c(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy)
{
    ALIGN16(pel_t first_line[64 + 128]);
    int line_size = bsx + ((bsy - 1) << 1);
#if !BUGFIX_PREDICTION_INTRA
    int real_size = STARAVS_MIN(line_size, (bsx << 1) - 2);
#endif
    int iHeight2 = bsy << 1;
    int i;

    src += 3;
#if BUGFIX_PREDICTION_INTRA
    for (i = 0; i < line_size; i++, src++) {
#else
    for (i = 0; i < real_size; i++, src++) {
#endif
        first_line[i] = (pel_t)((src[-1] + src[0] * 2 + src[1] + 2) >> 2);
    }

#if !BUGFIX_PREDICTION_INTRA
    // padding
    for (; i < line_size; i++) {
        first_line[i] = first_line[real_size - 1];
    }
#endif

    for (i = 0; i < iHeight2; i += 2) {
        memcpy(dst, first_line + i, bsx * sizeof(pel_t));
        dst += i_dst;
    }
}

/* ---------------------------------------------------------------------------
 */
static void intra_pred_ang_x_5_c(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy)
{
    int i;

    if (((bsy > 4) && (bsx > 8))) {
        ALIGN16(pel_t first_line[(64 + 80) << 3]);
#if !BUGFIX_PREDICTION_INTRA
        int iW2 = bsx * 2 - 1;
#endif
        int line_size = bsx + (((bsy - 8) * 11) >> 3);
#if !BUGFIX_PREDICTION_INTRA
        int real_size = STARAVS_MIN(line_size, iW2 + 1);
#endif
        int aligned_line_size = ((line_size + 15) >> 4) << 4;
        pel_t *pfirst[8];
#if !BUGFIX_PREDICTION_INTRA
        pel_t *src_org = src;
#endif

        pel_t *dst1 = dst;
        pel_t *dst2 = dst1 + i_dst;
        pel_t *dst3 = dst2 + i_dst;
        pel_t *dst4 = dst3 + i_dst;
        pel_t *dst5 = dst4 + i_dst;
        pel_t *dst6 = dst5 + i_dst;
        pel_t *dst7 = dst6 + i_dst;
        pel_t *dst8 = dst7 + i_dst;

        pfirst[0] = first_line;
        pfirst[1] = pfirst[0] + aligned_line_size;
        pfirst[2] = pfirst[1] + aligned_line_size;
        pfirst[3] = pfirst[2] + aligned_line_size;
        pfirst[4] = pfirst[3] + aligned_line_size;
        pfirst[5] = pfirst[4] + aligned_line_size;
        pfirst[6] = pfirst[5] + aligned_line_size;
        pfirst[7] = pfirst[6] + aligned_line_size;

#if BUGFIX_PREDICTION_INTRA
        for (i = 0; i < line_size; src++, i++) {
#else
        for (i = 0; i < real_size; src++, i++) {
#endif
            pfirst[0][i] = (pel_t)((5 * src[1] + 13 * src[2] + 11 * src[3] + 3 * src[4] + 16) >> 5);
            pfirst[1][i] = (pel_t)((    src[2] +  5 * src[3] +  7 * src[4] + 3 * src[5] + 8) >> 4);
            pfirst[2][i] = (pel_t)((7 * src[4] + 15 * src[5] +  9 * src[6] +     src[7] + 16) >> 5);
            pfirst[3][i] = (pel_t)((    src[5] +  3 * src[6] +  3 * src[7] +     src[8] + 4) >> 3);

            pfirst[4][i] = (pel_t)((     src[6] +  9 * src[7]  + 15 * src[8]  +  7 * src[9]  + 16) >> 5);
            pfirst[5][i] = (pel_t)(( 3 * src[8] +  7 * src[9]  +  5 * src[10] +      src[11] +  8) >> 4);
            pfirst[6][i] = (pel_t)(( 3 * src[9] + 11 * src[10] + 13 * src[11] +  5 * src[12] + 16) >> 5);
            pfirst[7][i] = (pel_t)((    src[11] +  2 * src[12] +      src[13]                 + 2) >> 2);
        }

#if !BUGFIX_PREDICTION_INTRA
        //padding
        if (((real_size - 1) + 11) > iW2) {
            src = src_org + iW2;
            pel_t pad1 = pfirst[0][iW2 - 1];
            pel_t pad2 = pfirst[1][iW2 - 2];
            pel_t pad3 = pfirst[2][iW2 - 4];
            pel_t pad4 = pfirst[3][iW2 - 5];

            pel_t pad5 = pfirst[4][iW2 - 6];
            pel_t pad6 = pfirst[5][iW2 - 8];
            pel_t pad7 = pfirst[6][iW2 - 9];
            pel_t pad8 = pfirst[7][iW2 - 11];

            int start1 = iW2;
            int start2 = iW2 - 1;
            int start3 = iW2 - 3;
            int start4 = iW2 - 4;
            int start5 = iW2 - 5;
            int start6 = iW2 - 7;
            int start7 = iW2 - 8;
            int start8 = iW2 - 10;

            for (i = start1; i < line_size; i++) {
                pfirst[0][i] = pad1;
            }
            for (i = start2; i < line_size; i++) {
                pfirst[1][i] = pad2;
            }
            for (i = start3; i < line_size; i++) {
                pfirst[2][i] = pad3;
            }
            for (i = start4; i < line_size; i++) {
                pfirst[3][i] = pad4;
            }
            for (i = start5; i < line_size; i++) {
                pfirst[4][i] = pad5;
            }
            for (i = start6; i < line_size; i++) {
                pfirst[5][i] = pad6;
            }
            for (i = start7; i < line_size; i++) {
                pfirst[6][i] = pad7;
            }
            for (i = start8; i < line_size; i++) {
                pfirst[7][i] = pad8;
            }
        }
#endif

        bsy  >>= 3;
        for (i = 0; i < bsy; i++) {
            memcpy(dst1, pfirst[0] + i * 11, bsx * sizeof(pel_t));
            memcpy(dst2, pfirst[1] + i * 11, bsx * sizeof(pel_t));
            memcpy(dst3, pfirst[2] + i * 11, bsx * sizeof(pel_t));
            memcpy(dst4, pfirst[3] + i * 11, bsx * sizeof(pel_t));
            memcpy(dst5, pfirst[4] + i * 11, bsx * sizeof(pel_t));
            memcpy(dst6, pfirst[5] + i * 11, bsx * sizeof(pel_t));
            memcpy(dst7, pfirst[6] + i * 11, bsx * sizeof(pel_t));
            memcpy(dst8, pfirst[7] + i * 11, bsx * sizeof(pel_t));

            dst1 = dst8 + i_dst;
            dst2 = dst1 + i_dst;
            dst3 = dst2 + i_dst;
            dst4 = dst3 + i_dst;
            dst5 = dst4 + i_dst;
            dst6 = dst5 + i_dst;
            dst7 = dst6 + i_dst;
            dst8 = dst7 + i_dst;
        }
    } else if (bsx == 16) {
        pel_t *dst1 = dst;
        pel_t *dst2 = dst1 + i_dst;
        pel_t *dst3 = dst2 + i_dst;
        pel_t *dst4 = dst3 + i_dst;

        for (i = 0; i < bsx; i++, src++) {
            dst1[i]  = (pel_t)((5 * src[1] + 13 * src[2] + 11 * src[3] + 3 * src[4] + 16) >> 5);
            dst2[i]  = (pel_t)((    src[2] +  5 * src[3] +  7 * src[4] + 3 * src[5] + 8) >> 4);
            dst3[i]  = (pel_t)((7 * src[4] + 15 * src[5] +  9 * src[6] +     src[7] + 16) >> 5);
            dst4[i]  = (pel_t)((    src[5] +  3 * src[6] +  3 * src[7] +     src[8] + 4) >> 3);
        }
    } else if (bsx == 8) {
        pel_t *dst1 = dst;
        pel_t *dst2 = dst1 + i_dst;
        pel_t *dst3 = dst2 + i_dst;
        pel_t *dst4 = dst3 + i_dst;
        pel_t *dst5 = dst4 + i_dst;
        pel_t *dst6 = dst5 + i_dst;
        pel_t *dst7 = dst6 + i_dst;
        pel_t *dst8 = dst7 + i_dst;

        for (i = 0; i < 8; src++, i++) {
            dst1[i]  = (pel_t)((5 * src[1] + 13 * src[2] + 11 * src[3] + 3 * src[4] + 16) >> 5);
            dst2[i]  = (pel_t)((    src[2] +  5 * src[3] +  7 * src[4] + 3 * src[5] + 8) >> 4);
            dst3[i]  = (pel_t)((7 * src[4] + 15 * src[5] +  9 * src[6] +     src[7] + 16) >> 5);
            dst4[i]  = (pel_t)((    src[5] +  3 * src[6] +  3 * src[7] +     src[8] + 4) >> 3);

            dst5[i] = (pel_t)((     src[6] +  9 * src[7]  + 15 * src[8]  +  7 * src[9]  + 16) >> 5);
            dst6[i] = (pel_t)(( 3 * src[8] +  7 * src[9]  +  5 * src[10] +      src[11] + 8) >> 4);
            dst7[i] = (pel_t)(( 3 * src[9] + 11 * src[10] + 13 * src[11] +  5 * src[12] + 16) >> 5);
            dst8[i] = (pel_t)((    src[11] +  2 * src[12] +      src[13]                 + 2) >> 2);
        }
#if !BUGFIX_PREDICTION_INTRA
        dst7[7] = dst7[6];
        dst8[7] = dst8[4];
        dst8[6] = dst8[4];
        dst8[5] = dst8[4];
#endif
        if (bsy == 32) {
            //src -> 8,src[8] -> 16
#if BUGFIX_PREDICTION_INTRA
            pel_t pad1 = src[8];
            dst1 = dst8 + i_dst;
            int j;
            for (j = 0; j < 24; j++) {
                for (i = 0; i < 8; i++) {
                    dst1[i] = pad1;
                }
                dst1 += i_dst;
            }

            dst1 = dst8 + i_dst;
            dst2 = dst1 + i_dst;
            dst3 = dst2 + i_dst;

            src += 4;
            dst1[0] = (pel_t)((5 * src[0] + 13 * src[1] + 11 * src[2] + 3 * src[3] + 16) >> 5);
            dst1[1] = (pel_t)((5 * src[1] + 13 * src[2] + 11 * src[3] + 3 * src[4] + 16) >> 5);
            dst1[2] = (pel_t)((5 * src[2] + 13 * src[3] + 11 * src[4] + 3 * src[5] + 16) >> 5);
            dst1[3] = (pel_t)((5 * src[3] + 13 * src[4] + 11 * src[5] + 3 * src[6] + 16) >> 5);
            dst2[0] = (pel_t)((src[1] + 5 * src[2] + 7 * src[3] + 3 * src[4] + 8) >> 4);
            dst2[1] = (pel_t)((src[2] + 5 * src[3] + 7 * src[4] + 3 * src[5] + 8) >> 4);
            dst2[2] = (pel_t)((src[3] + 5 * src[4] + 7 * src[5] + 3 * src[6] + 8) >> 4);
            dst3[0] = (pel_t)((7 * src[3] + 15 * src[4] +  9 * src[5] +     src[6] + 16) >> 5);
#else
            //src -> 8,src[7] -> 15
            pel_t pad1 = (pel_t)((5 * src[7] + 13 * src[8] + 11 * src[9] + 3 * src[10] + 16) >> 5);
            pel_t pad2 = (pel_t)((src[7] + 5 * src[8] + 7 * src[9] + 3 * src[10] + 8) >> 4);
            pel_t pad3 = (pel_t)((7 * src[7] + 15 * src[8] + 9 * src[9] + src[10] + 16) >> 5);
            pel_t pad4 = (pel_t)((src[7] + 3 * src[8] + 3 * src[9] + src[10] + 4) >> 3);

            pel_t pad5 = (pel_t)((src[7] + 9 * src[8] + 15 * src[9] + 7 * src[10] + 16) >> 5);
            pel_t pad6 = dst6[7];
            pel_t pad7 = dst7[7];
            pel_t pad8 = dst8[7];

            dst1 = dst8 + i_dst;
            dst2 = dst1 + i_dst;
            dst3 = dst2 + i_dst;
            dst4 = dst3 + i_dst;
            dst5 = dst4 + i_dst;
            dst6 = dst5 + i_dst;
            dst7 = dst6 + i_dst;
            dst8 = dst7 + i_dst;

            for (i = 0; i < 8; i++) {
                dst1[i] = pad1;
                dst2[i] = pad2;
                dst3[i] = pad3;
                dst4[i] = pad4;

                dst5[i] = pad5;
                dst6[i] = pad6;
                dst7[i] = pad7;
                dst8[i] = pad8;
            }
            src += 4;
            dst1[0] = (pel_t)((5 * src[0] + 13 * src[1] + 11 * src[2] + 3 * src[3] + 16) >> 5);
            dst1[1] = (pel_t)((5 * src[1] + 13 * src[2] + 11 * src[3] + 3 * src[4] + 16) >> 5);
            dst1[2] = (pel_t)((5 * src[2] + 13 * src[3] + 11 * src[4] + 3 * src[5] + 16) >> 5);
            dst2[0] = (pel_t)((    src[1] +  5 * src[2] +  7 * src[3] + 3 * src[4] +  8) >> 4);
            dst2[1] = (pel_t)((    src[2] +  5 * src[3] +  7 * src[4] + 3 * src[5] +  8) >> 4);

            dst1 = dst8 + i_dst;
            dst2 = dst1 + i_dst;
            dst3 = dst2 + i_dst;
            dst4 = dst3 + i_dst;
            dst5 = dst4 + i_dst;
            dst6 = dst5 + i_dst;
            dst7 = dst6 + i_dst;
            dst8 = dst7 + i_dst;

            for (i = 0; i < 8; i++) {
                dst1[i] = pad1;
                dst2[i] = pad2;
                dst3[i] = pad3;
                dst4[i] = pad4;

                dst5[i] = pad5;
                dst6[i] = pad6;
                dst7[i] = pad7;
                dst8[i] = pad8;
            }

            dst1 = dst8 + i_dst;
            dst2 = dst1 + i_dst;
            dst3 = dst2 + i_dst;
            dst4 = dst3 + i_dst;
            dst5 = dst4 + i_dst;
            dst6 = dst5 + i_dst;
            dst7 = dst6 + i_dst;
            dst8 = dst7 + i_dst;

            for (i = 0; i < 8; i++) {
                dst1[i] = pad1;
                dst2[i] = pad2;
                dst3[i] = pad3;
                dst4[i] = pad4;

                dst5[i] = pad5;
                dst6[i] = pad6;
                dst7[i] = pad7;
                dst8[i] = pad8;
            }
#endif
        }
    } else {
        pel_t *dst1 = dst;
        pel_t *dst2 = dst1 + i_dst;
        pel_t *dst3 = dst2 + i_dst;
        pel_t *dst4 = dst3 + i_dst;

        for (i = 0; i < 4; i++, src++) {
            dst1[i]  = (pel_t)((5 * src[1] + 13 * src[2] + 11 * src[3] + 3 * src[4] + 16) >> 5);
            dst2[i]  = (pel_t)((    src[2] +  5 * src[3] +  7 * src[4] + 3 * src[5] + 8) >> 4);
            dst3[i]  = (pel_t)((7 * src[4] + 15 * src[5] +  9 * src[6] +     src[7] + 16) >> 5);
            dst4[i]  = (pel_t)((    src[5] +  3 * src[6] +  3 * src[7] +     src[8] + 4) >> 3);
        }
#if !BUGFIX_PREDICTION_INTRA
        dst4[3] = dst4[2];
#endif
        if (bsy == 16) {
#if BUGFIX_PREDICTION_INTRA
            pel_t *dst5 = dst4 + i_dst;

            src += 4;
            pel_t pad1 = src[0];

            int j;
            for (j = 0; j < 12; j++) {
                for (i = 0; i < 4; i++) {
                    dst5[i] = pad1;
                }
                dst5 += i_dst;
            }
            dst5 = dst4 + i_dst;
            dst5[0] = (pel_t)((src[-2] + 9 * src[-1] + 15 * src[0] + 7 * src[1] + 16) >> 5);
            dst5[1] = (pel_t)((src[-1] + 9 * src[ 0] + 15 * src[1] + 7 * src[2] + 16) >> 5);
#else
            pel_t *dst5 = dst4 + i_dst;
            pel_t *dst6 = dst5 + i_dst;
            pel_t *dst7 = dst6 + i_dst;
            pel_t *dst8 = dst7 + i_dst;

            src += 3;
            pel_t pad1 = (pel_t)((5 * src[0] + 13 * src[1] + 11 * src[2] + 3 * src[3] + 16) >> 5);
            pel_t pad2 = (pel_t)((    src[0] +  5 * src[1] +  7 * src[2] + 3 * src[3] + 8) >> 4);
            pel_t pad3 = (pel_t)((7 * src[0] + 15 * src[1] +  9 * src[2] + 1 * src[3] + 16) >> 5);
            pel_t pad4 = dst4[3];

            pel_t pad5 = (pel_t)((    src[0] +  9 * src[1] + 15 * src[2] + 7 * src[3] + 16) >> 5);
            pel_t pad6 = (pel_t)((3 * src[0] +  7 * src[1] +  5 * src[2] +     src[3] + 8) >> 4);
            pel_t pad7 = (pel_t)((3 * src[0] + 11 * src[1] + 13 * src[2] + 5 * src[3] + 16) >> 5);
            pel_t pad8 = (pel_t)((    src[0] +  2 * src[1] +      src[2]              + 2) >> 2);

            for (i = 0; i < 4; i++) {
                dst5[i] = pad5;
                dst6[i] = pad6;
                dst7[i] = pad7;
                dst8[i] = pad8;
            }
            dst5[0] = (pel_t)((src[-1] + 9 * src[0] + 15 * src[1] + 7 * src[2] + 16) >> 5);

            dst1 = dst8 + i_dst;
            dst2 = dst1 + i_dst;
            dst3 = dst2 + i_dst;
            dst4 = dst3 + i_dst;
            dst5 = dst4 + i_dst;
            dst6 = dst5 + i_dst;
            dst7 = dst6 + i_dst;
            dst8 = dst7 + i_dst;

            for (i = 0; i < 4; i++) {
                dst1[i] = pad1;
                dst2[i] = pad2;
                dst3[i] = pad3;
                dst4[i] = pad4;
                dst5[i] = pad5;
                dst6[i] = pad6;
                dst7[i] = pad7;
                dst8[i] = pad8;
            }
#endif
        }
    }
}

/* ---------------------------------------------------------------------------
 */
static void intra_pred_ang_x_6_c(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy)
{
    ALIGN16(pel_t first_line[64 + 64]);
    int line_size = bsx + bsy - 1;
#if !BUGFIX_PREDICTION_INTRA
    int real_size = STARAVS_MIN(line_size, (bsx << 1) - 1);
#endif
    int i;
#if !BUGFIX_PREDICTION_INTRA
    pel_t pad;
#endif

#if BUGFIX_PREDICTION_INTRA
    for (i = 0; i < line_size; i++, src++) {
#else
    for (i = 0; i < real_size; i++, src++) {
#endif
        first_line[i] = (pel_t)((src[1] + (src[2] << 1) + src[3] + 2) >> 2);
    }

#if !BUGFIX_PREDICTION_INTRA
    // padding
    pad = first_line[real_size - 1];
    for (; i < line_size; i++) {
        first_line[i] = pad;
    }
#endif

    for (i = 0; i < bsy; i++) {
        memcpy(dst, first_line + i, bsx * sizeof(pel_t));
        dst += i_dst;
    }
}

/* ---------------------------------------------------------------------------
 */
static void intra_pred_ang_x_7_c(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy)
{
    int i;
    pel_t *dst1 = dst;
    pel_t *dst2 = dst1 + i_dst;
    pel_t *dst3 = dst2 + i_dst;
    pel_t *dst4 = dst3 + i_dst;
    if (bsy == 4) {
        for (i = 0; i < bsx; src++, i++){
            dst1[i] = (pel_t)((src[0] *  9 + src[1] * 41 + src[2] * 55 + src[3] * 23 + 64) >> 7);
            dst2[i] = (pel_t)((src[1] *  9 + src[2] * 25 + src[3] * 23 + src[4] *  7 + 32) >> 6);
            dst3[i] = (pel_t)((src[2] * 27 + src[3] * 59 + src[4] * 37 + src[5] *  5 + 64) >> 7);
            dst4[i] = (pel_t)((src[2] *  3 + src[3] * 35 + src[4] * 61 + src[5] * 29 + 64) >> 7);
        }
    } else if (bsy == 8) {
        pel_t *dst5 = dst4 + i_dst;
        pel_t *dst6 = dst5 + i_dst;
        pel_t *dst7 = dst6 + i_dst;
        pel_t *dst8 = dst7 + i_dst;
        for (i = 0; i < bsx; src++, i++){
            dst1[i] = (pel_t)((src[0] *  9 + src[1] * 41 + src[2] * 55 + src[3] * 23 + 64) >> 7);
            dst2[i] = (pel_t)((src[1] *  9 + src[2] * 25 + src[3] * 23 + src[4] *  7 + 32) >> 6);
            dst3[i] = (pel_t)((src[2] * 27 + src[3] * 59 + src[4] * 37 + src[5] *  5 + 64) >> 7);
            dst4[i] = (pel_t)((src[2] *  3 + src[3] * 35 + src[4] * 61 + src[5] * 29 + 64) >> 7);
            dst5[i] = (pel_t)((src[3] *  3 + src[4] * 11 + src[5] * 13 + src[6] *  5 + 16) >> 5);
            dst6[i] = (pel_t)((src[4] * 21 + src[5] * 53 + src[6] * 43 + src[7] * 11 + 64) >> 7);
            dst7[i] = (pel_t)((src[5] * 15 + src[6] * 31 + src[7] * 17 + src[8] + 32)      >> 6);
            dst8[i] = (pel_t)((src[5] *  3 + src[6] * 19 + src[7] * 29 + src[8] * 13 + 32) >> 6);
        }
    } else {
        intra_pred_ang_x_c(src, dst, i_dst, dir_mode, bsx, bsy);
    }
}

/* ---------------------------------------------------------------------------
 */
static void intra_pred_ang_x_8_c(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy)
{
    ALIGN16(pel_t first_line[2 * (64 + 32)]);
    int line_size = bsx + (bsy >> 1) - 1;
#if !BUGFIX_PREDICTION_INTRA
    int real_size = STARAVS_MIN(line_size, bsx * 2);
#endif
    int aligned_line_size = ((line_size + 15) >> 4) << 4;
    int i_dst2 = i_dst << 1;
    int i;
#if !BUGFIX_PREDICTION_INTRA
    pel_t pad1, pad2;
#endif
    pel_t *pfirst[2];

    pfirst[0] = first_line;
    pfirst[1] = first_line + aligned_line_size;
#if BUGFIX_PREDICTION_INTRA
    for (i = 0; i < line_size; i++, src++) {
#else
    for (i = 0; i < real_size; i++, src++) {
#endif
        pfirst[0][i] = (pel_t)((src[0] + (src[1] + src[2]) * 3 + src[3] + 4) >> 3);
        pfirst[1][i] = (pel_t)((src[1] + (src[2] << 1)         + src[3] + 2) >> 2);
    }

#if !BUGFIX_PREDICTION_INTRA
    // padding
    if (real_size < line_size) {
        pfirst[1][real_size - 1] = pfirst[1][real_size - 2];

        pad1 = pfirst[0][real_size - 1];
        pad2 = pfirst[1][real_size - 1];
        for (; i < line_size; i++) {
            pfirst[0][i] = pad1;
            pfirst[1][i] = pad2;
        }
    }
#endif

    bsy >>= 1;
    for (i = 0; i < bsy; i++) {
        memcpy(dst        , pfirst[0] + i, bsx * sizeof(pel_t));
        memcpy(dst + i_dst, pfirst[1] + i, bsx * sizeof(pel_t));
        dst += i_dst2;
    }
}

/* ---------------------------------------------------------------------------
 */
static void intra_pred_ang_x_9_c(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy)
{
    if (bsy > 8){
        intra_pred_ang_x_c(src, dst, i_dst, dir_mode, bsx, bsy);
        /*
        ALIGN16(pel_t first_line[(64 + 32) * 11]);
        int line_size = bsx + (bsy * 93 >> 8) - 1;
        int real_size = STARAVS_MIN(line_size, bsx * 2);
        int aligned_line_size = ((line_size + 31) >> 5) << 5;
        int i_dst11 = i_dst * 11;
        int i;
        pel_t pad1, pad2, pad3, pad4, pad5, pad6, pad7, pad8, pad9, pad10, pad11;
        pel_t *pfirst[11];

        pfirst[0] = first_line;
        pfirst[1] = pfirst[0] + aligned_line_size;
        pfirst[2] = pfirst[1] + aligned_line_size;
        pfirst[3] = pfirst[2] + aligned_line_size;
        pfirst[4] = pfirst[3] + aligned_line_size;
        pfirst[5] = pfirst[4] + aligned_line_size;
        pfirst[6] = pfirst[5] + aligned_line_size;
        pfirst[7] = pfirst[6] + aligned_line_size;
        pfirst[8] = pfirst[7] + aligned_line_size;
        pfirst[9] = pfirst[8] + aligned_line_size;
        pfirst[10] = pfirst[9] + aligned_line_size;
        for (i = 0; i < real_size; i++, src++) {
            pfirst[0][i] = (pel_t)((21 * src[0] + 53 * src[1] + 43 * src[2] + 11 * src[3] + 64) >> 7);
            pfirst[1][i] = (pel_t)((9 * src[0] + 41 * src[1] + 55 * src[2] + 23 * src[3] + 64) >> 7);
            pfirst[2][i] = (pel_t)((15 * src[1] + 31 * src[2] + 17 * src[3] + 1 * src[4] + 32) >> 6);
            pfirst[3][i] = (pel_t)((9 * src[1] + 25 * src[2] + 23 * src[3] + 7 * src[4] + 32) >> 6);
            pfirst[4][i] = (pel_t)((3 * src[1] + 19 * src[2] + 29 * src[3] + 13 * src[4] + 32) >> 6);
            pfirst[5][i] = (pel_t)((27 * src[2] + 59 * src[3] + 37 * src[4] + 5 * src[5] + 64) >> 7);
            pfirst[6][i] = (pel_t)((15 * src[2] + 47 * src[3] + 49 * src[4] + 17 * src[5] + 64) >> 7);
            pfirst[7][i] = (pel_t)((3 * src[2] + 35 * src[3] + 61 * src[4] + 29 * src[5] + 64) >> 7);
            pfirst[8][i] = (pel_t)((3 * src[3] + 7 * src[4] + 5 * src[5] + 1 * src[6] + 8) >> 4);
            pfirst[9][i] = (pel_t)((3 * src[3] + 11 * src[4] + 13 * src[5] + 5 * src[6] + 16) >> 5);
            pfirst[10][i] = (pel_t)((1 * src[3] + 33 * src[4] + 63 * src[5] + 31 * src[6] + 64) >> 7);
        }

        // padding
        if (real_size < line_size) {
            pfirst[8][real_size - 3] = pfirst[8][real_size - 4];
            pfirst[9][real_size - 3] = pfirst[9][real_size - 4];
            pfirst[10][real_size - 3] = pfirst[10][real_size - 4];
            pfirst[8][real_size - 2] = pfirst[8][real_size - 3];
            pfirst[9][real_size - 2] = pfirst[9][real_size - 3];
            pfirst[10][real_size - 2] = pfirst[10][real_size - 3];
            pfirst[8][real_size - 1] = pfirst[8][real_size - 2];
            pfirst[9][real_size - 1] = pfirst[9][real_size - 2];
            pfirst[10][real_size - 1] = pfirst[10][real_size - 2];

            pfirst[5][real_size - 2] = pfirst[5][real_size - 3];
            pfirst[6][real_size - 2] = pfirst[6][real_size - 3];
            pfirst[7][real_size - 2] = pfirst[7][real_size - 3];
            pfirst[5][real_size - 1] = pfirst[5][real_size - 2];
            pfirst[6][real_size - 1] = pfirst[6][real_size - 2];
            pfirst[7][real_size - 1] = pfirst[7][real_size - 2];

            pfirst[2][real_size - 1] = pfirst[2][real_size - 2];
            pfirst[3][real_size - 1] = pfirst[3][real_size - 2];
            pfirst[4][real_size - 1] = pfirst[4][real_size - 2];


            pad1 = pfirst[0][real_size - 1];
            pad2 = pfirst[1][real_size - 1];
            pad3 = pfirst[2][real_size - 1];
            pad4 = pfirst[3][real_size - 1];
            pad5 = pfirst[4][real_size - 1];
            pad6 = pfirst[5][real_size - 1];
            pad7 = pfirst[6][real_size - 1];
            pad8 = pfirst[7][real_size - 1];
            pad9 = pfirst[8][real_size - 1];
            pad10 = pfirst[9][real_size - 1];
            pad11 = pfirst[10][real_size - 1];
            for (; i < line_size; i++) {
                pfirst[0][i] = pad1;
                pfirst[1][i] = pad2;
                pfirst[2][i] = pad3;
                pfirst[3][i] = pad4;
                pfirst[4][i] = pad5;
                pfirst[5][i] = pad6;
                pfirst[6][i] = pad7;
                pfirst[7][i] = pad8;
                pfirst[8][i] = pad9;
                pfirst[9][i] = pad10;
                pfirst[10][i] = pad11;
            }
        }

        int bsy_b = bsy / 11;
        for (i = 0; i < bsy_b; i++) {
            memcpy(dst, pfirst[0] + i, bsx * sizeof(pel_t));
            memcpy(dst + i_dst, pfirst[1] + i, bsx * sizeof(pel_t));
            memcpy(dst + 2 * i_dst, pfirst[2] + i, bsx * sizeof(pel_t));
            memcpy(dst + 3 * i_dst, pfirst[3] + i, bsx * sizeof(pel_t));
            memcpy(dst + 4 * i_dst, pfirst[4] + i, bsx * sizeof(pel_t));
            memcpy(dst + 5 * i_dst, pfirst[5] + i, bsx * sizeof(pel_t));
            memcpy(dst + 6 * i_dst, pfirst[6] + i, bsx * sizeof(pel_t));
            memcpy(dst + 7 * i_dst, pfirst[7] + i, bsx * sizeof(pel_t));
            memcpy(dst + 8 * i_dst, pfirst[8] + i, bsx * sizeof(pel_t));
            memcpy(dst + 9 * i_dst, pfirst[9] + i, bsx * sizeof(pel_t));
            memcpy(dst + 10 * i_dst, pfirst[10] + i, bsx * sizeof(pel_t));
            dst += i_dst11;
        }
        int bsy_r = bsy - bsy_b * 11;
        for (i = 0; i < bsy_r; i++) {
            memcpy(dst, pfirst[i] + bsy_b, bsx * sizeof(pel_t));
            dst += i_dst;
        }
        */
    } else if (bsy == 8) {
        pel_t *dst1 = dst;
        pel_t *dst2 = dst1 + i_dst;
        pel_t *dst3 = dst2 + i_dst;
        pel_t *dst4 = dst3 + i_dst;
        pel_t *dst5 = dst4 + i_dst;
        pel_t *dst6 = dst5 + i_dst;
        pel_t *dst7 = dst6 + i_dst;
        pel_t *dst8 = dst7 + i_dst;
        for (int i = 0; i < bsx; i++, src++) {
            dst1[i] = (pel_t)((21 * src[0] + 53 * src[1] + 43 * src[2] + 11 * src[3] + 64) >> 7);
            dst2[i] = (pel_t)((9  * src[0] + 41 * src[1] + 55 * src[2] + 23 * src[3] + 64) >> 7);
            dst3[i] = (pel_t)((15 * src[1] + 31 * src[2] + 17 * src[3] +      src[4] + 32) >> 6);
            dst4[i] = (pel_t)((9  * src[1] + 25 * src[2] + 23 * src[3] + 7  * src[4] + 32) >> 6);

            dst5[i] = (pel_t)((3  * src[1] + 19 * src[2] + 29 * src[3] + 13 * src[4] + 32) >> 6);
            dst6[i] = (pel_t)((27 * src[2] + 59 * src[3] + 37 * src[4] + 5  * src[5] + 64) >> 7);
            dst7[i] = (pel_t)((15 * src[2] + 47 * src[3] + 49 * src[4] + 17 * src[5] + 64) >> 7);
            dst8[i] = (pel_t)((3  * src[2] + 35 * src[3] + 61 * src[4] + 29 * src[5] + 64) >> 7);
        }
    } else /*if (bsy == 4)*/ {
        pel_t *dst1 = dst;
        pel_t *dst2 = dst1 + i_dst;
        pel_t *dst3 = dst2 + i_dst;
        pel_t *dst4 = dst3 + i_dst;
        for (int i = 0; i < bsx; i++, src++) {
            dst1[i] = (pel_t)((21 * src[0] + 53 * src[1] + 43 * src[2] + 11 * src[3] + 64) >> 7);
            dst2[i] = (pel_t)((9  * src[0] + 41 * src[1] + 55 * src[2] + 23 * src[3] + 64) >> 7);
            dst3[i] = (pel_t)((15 * src[1] + 31 * src[2] + 17 * src[3] +      src[4] + 32) >> 6);
            dst4[i] = (pel_t)((9  * src[1] + 25 * src[2] + 23 * src[3] + 7  * src[4] + 32) >> 6);
        }
    }

}

/* ---------------------------------------------------------------------------
 */
static void intra_pred_ang_x_10_c(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy)
{
    pel_t *dst1 = dst;
    pel_t *dst2 = dst1 + i_dst;
    pel_t *dst3 = dst2 + i_dst;
    pel_t *dst4 = dst3 + i_dst;
    int i;

    if (bsy != 4) {
        ALIGN16(pel_t first_line[4 * (64 + 16)]);
        int line_size = bsx + bsy / 4 - 1;
        int aligned_line_size = ((line_size + 15) >> 4) << 4;
        pel_t *pfirst[4];

        pfirst[0] = first_line;
        pfirst[1] = first_line + aligned_line_size;
        pfirst[2] = first_line + aligned_line_size * 2;
        pfirst[3] = first_line + aligned_line_size * 3;

        for (i = 0; i < line_size; i++, src++) {
            pfirst[0][i] = (pel_t)((src[0] * 3 +  src[1] * 7 + src[2]  * 5 + src[3]     + 8) >> 4);
            pfirst[1][i] = (pel_t)((src[0]     + (src[1]     + src[2]) * 3 + src[3]     + 4) >> 3);
            pfirst[2][i] = (pel_t)((src[0]     +  src[1] * 5 + src[2]  * 7 + src[3] * 3 + 8) >> 4);
            pfirst[3][i] = (pel_t)((src[1]     +  src[2] * 2 + src[3]                   + 2) >> 2);
        }

        bsy   >>= 2;
        i_dst <<= 2;
        for (i = 0; i < bsy; i++) {
            memcpy(dst1, pfirst[0] + i, bsx * sizeof(pel_t));
            memcpy(dst2, pfirst[1] + i, bsx * sizeof(pel_t));
            memcpy(dst3, pfirst[2] + i, bsx * sizeof(pel_t));
            memcpy(dst4, pfirst[3] + i, bsx * sizeof(pel_t));
            dst1 += i_dst;
            dst2 += i_dst;
            dst3 += i_dst;
            dst4 += i_dst;
        }
    } else {
        for (i = 0; i < bsx; i++, src++) {
            dst1[i] = (pel_t)((src[0] * 3 +  src[1] * 7 + src[2]  * 5 + src[3]     + 8) >> 4);
            dst2[i] = (pel_t)((src[0]     + (src[1]     + src[2]) * 3 + src[3]     + 4) >> 3);
            dst3[i] = (pel_t)((src[0]     +  src[1] * 5 + src[2]  * 7 + src[3] * 3 + 8) >> 4);
            dst4[i] = (pel_t)((src[1]     +  src[2] * 2 + src[3]                   + 2) >> 2);
        }
    }
}

/* ---------------------------------------------------------------------------
 */
static void intra_pred_ang_x_11_c(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy)
{
    int i;
    if (bsy > 8) {
        ALIGN16(pel_t first_line[(64 + 16) << 3]);
        int line_size = bsx + (bsy >> 3) - 1;
        int aligned_line_size = ((line_size + 15) >> 4) << 4;
        int i_dst8 = i_dst << 3;
        pel_t *pfirst[8];

        pfirst[0] = first_line;
        pfirst[1] = pfirst[0] + aligned_line_size;
        pfirst[2] = pfirst[1] + aligned_line_size;
        pfirst[3] = pfirst[2] + aligned_line_size;
        pfirst[4] = pfirst[3] + aligned_line_size;
        pfirst[5] = pfirst[4] + aligned_line_size;
        pfirst[6] = pfirst[5] + aligned_line_size;
        pfirst[7] = pfirst[6] + aligned_line_size;
        for (i = 0; i < line_size; i++, src++) {
            pfirst[0][i] = (pel_t)((7 * src[0] + 15 * src[1] +  9 * src[2] +     src[3] + 16) >> 5);
            pfirst[1][i] = (pel_t)((3 * src[0] +  7 * src[1] +  5 * src[2] +     src[3] +  8) >> 4);
            pfirst[2][i] = (pel_t)((5 * src[0] + 13 * src[1] + 11 * src[2] + 3 * src[3] + 16) >> 5);
            pfirst[3][i] = (pel_t)((    src[0] +  3 * src[1] +  3 * src[2] +     src[3] +  4) >> 3);

            pfirst[4][i] = (pel_t)((3 * src[0] + 11 * src[1] + 13 * src[2] + 5 * src[3] + 16) >> 5);
            pfirst[5][i] = (pel_t)((    src[0] +  5 * src[1] +  7 * src[2] + 3 * src[3] +  8) >> 4);
            pfirst[6][i] = (pel_t)((    src[0] +  9 * src[1] + 15 * src[2] + 7 * src[3] + 16) >> 5);
            pfirst[7][i] = (pel_t)((    src[1] +  2 * src[2] +      src[3] + 0 * src[4] +  2) >> 2);
        }

        bsy >>= 3;
        for (i = 0; i < bsy; i++) {
            memcpy(dst            , pfirst[0] + i, bsx * sizeof(pel_t));
            memcpy(dst +     i_dst, pfirst[1] + i, bsx * sizeof(pel_t));
            memcpy(dst + 2 * i_dst, pfirst[2] + i, bsx * sizeof(pel_t));
            memcpy(dst + 3 * i_dst, pfirst[3] + i, bsx * sizeof(pel_t));
            memcpy(dst + 4 * i_dst, pfirst[4] + i, bsx * sizeof(pel_t));
            memcpy(dst + 5 * i_dst, pfirst[5] + i, bsx * sizeof(pel_t));
            memcpy(dst + 6 * i_dst, pfirst[6] + i, bsx * sizeof(pel_t));
            memcpy(dst + 7 * i_dst, pfirst[7] + i, bsx * sizeof(pel_t));
            dst += i_dst8;
        }
    } else if (bsy == 8) {
        pel_t *dst1 = dst;
        pel_t *dst2 = dst1 + i_dst;
        pel_t *dst3 = dst2 + i_dst;
        pel_t *dst4 = dst3 + i_dst;
        pel_t *dst5 = dst4 + i_dst;
        pel_t *dst6 = dst5 + i_dst;
        pel_t *dst7 = dst6 + i_dst;
        pel_t *dst8 = dst7 + i_dst;
        for (i = 0; i < bsx; i++, src++) {
            dst1[i] = (pel_t)((7 * src[0] + 15 * src[1] +  9 * src[2] +     src[3] + 16) >> 5);
            dst2[i] = (pel_t)((3 * src[0] +  7 * src[1] +  5 * src[2] +     src[3] + 8) >> 4);
            dst3[i] = (pel_t)((5 * src[0] + 13 * src[1] + 11 * src[2] + 3 * src[3] + 16) >> 5);
            dst4[i] = (pel_t)((    src[0] +  3 * src[1] +  3 * src[2] +     src[3] + 4) >> 3);

            dst5[i] = (pel_t)((3 * src[0] + 11 * src[1] + 13 * src[2] + 5 * src[3] + 16) >> 5);
            dst6[i] = (pel_t)((    src[0] +  5 * src[1] +  7 * src[2] + 3 * src[3] +  8) >> 4);
            dst7[i] = (pel_t)((    src[0] +  9 * src[1] + 15 * src[2] + 7 * src[3] + 16) >> 5);
            dst8[i] = (pel_t)((    src[1] +  2 * src[2] +      src[3] +            +  2) >> 2);
        }
    } else {
        for (i = 0; i < bsx; i++, src++) {
            pel_t *dst1 = dst;
            pel_t *dst2 = dst1 + i_dst;
            pel_t *dst3 = dst2 + i_dst;
            pel_t *dst4 = dst3 + i_dst;
            dst1[i] = (pel_t)(( 7 * src[0] + 15 * src[1] +  9 * src[2] +      src[3] + 16) >> 5);
            dst2[i] = (pel_t)(( 3 * src[0] +  7 * src[1] +  5 * src[2] +      src[3] +  8) >> 4);
            dst3[i] = (pel_t)(( 5 * src[0] + 13 * src[1] + 11 * src[2] +  3 * src[3] + 16) >> 5);
            dst4[i] = (pel_t)((     src[0] +  3 * src[1] +  3 * src[2] +      src[3] +  4) >> 3);
        }
    }
}


/* ---------------------------------------------------------------------------
 */
static void intra_pred_ang_y_25_c(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy)
{
    int i;

    if (bsx > 8) {
            ALIGN16(pel_t first_line[64 + (64 << 3)]);
            int line_size = bsx + ((bsy - 1) << 3);
            int iHeight8 = bsy << 3;
            for (i = 0; i < line_size; i += 8, src--) {
                first_line[0 + i] = (pel_t)((src[0] * 7 + src[-1] * 15 + src[-2] *  9 + src[-3] * 1 + 16) >> 5);
                first_line[1 + i] = (pel_t)((src[0] * 3 + src[-1] * 7  + src[-2] *  5 + src[-3] * 1 + 8) >> 4);
                first_line[2 + i] = (pel_t)((src[0] * 5 + src[-1] * 13 + src[-2] * 11 + src[-3] * 3 + 16) >> 5);
                first_line[3 + i] = (pel_t)((src[0] * 1 + src[-1] * 3  + src[-2] *  3 + src[-3] * 1 + 4) >> 3);

                first_line[4 + i] = (pel_t)((src[0] * 3 + src[-1] * 11 + src[-2] * 13 + src[-3] * 5 + 16) >> 5);
                first_line[5 + i] = (pel_t)((src[0] * 1 + src[-1] *  5 + src[-2] *  7 + src[-3] * 3 + 8) >> 4);
                first_line[6 + i] = (pel_t)((src[0] * 1 + src[-1] *  9 + src[-2] * 15 + src[-3] * 7 + 16) >> 5);
                first_line[7 + i] = (pel_t)((             src[-1] *  1 + src[-2] *  2 + src[-3] * 1 + 2) >> 2);
            }
            for (i = 0; i < iHeight8; i += 8) {
                memcpy(dst, first_line + i, bsx * sizeof(pel_t));
                dst += i_dst;
            }
    } else if (bsx == 8) {
        for (i = 0; i < bsy; i++, src--) {
            dst[0] = (pel_t)((src[0] * 7 + src[-1] * 15 + src[-2] *  9 + src[-3] * 1 + 16) >> 5);
            dst[1] = (pel_t)((src[0] * 3 + src[-1] *  7 + src[-2] *  5 + src[-3] * 1 + 8) >> 4);
            dst[2] = (pel_t)((src[0] * 5 + src[-1] * 13 + src[-2] * 11 + src[-3] * 3 + 16) >> 5);
            dst[3] = (pel_t)((src[0] * 1 + src[-1] *  3 + src[-2] *  3 + src[-3] * 1 + 4) >> 3);

            dst[4] = (pel_t)((src[0] * 3 + src[-1] * 11 + src[-2] * 13 + src[-3] * 5 + 16) >> 5);
            dst[5] = (pel_t)((src[0] * 1 + src[-1] *  5 + src[-2] *  7 + src[-3] * 3 + 8) >> 4);
            dst[6] = (pel_t)((src[0] * 1 + src[-1] *  9 + src[-2] * 15 + src[-3] * 7 + 16) >> 5);
            dst[7] = (pel_t)((             src[-1] *  1 + src[-2] *  2 + src[-3] * 1 + 2) >> 2);
            dst += i_dst;
        }
    } else {
        for (i = 0; i < bsy; i++, src--) {
            dst[0] = (pel_t)((src[0] * 7 + src[-1] * 15 + src[-2] *  9 + src[-3] * 1 + 16) >> 5);
            dst[1] = (pel_t)((src[0] * 3 + src[-1] *  7 + src[-2] *  5 + src[-3] * 1 + 8) >> 4);
            dst[2] = (pel_t)((src[0] * 5 + src[-1] * 13 + src[-2] * 11 + src[-3] * 3 + 16) >> 5);
            dst[3] = (pel_t)((src[0] * 1 + src[-1] *  3 + src[-2] *  3 + src[-3] * 1 + 4) >> 3);
            dst += i_dst;
        }
    }
}

/* ---------------------------------------------------------------------------
 */
static void intra_pred_ang_y_26_c(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy)
{
    int i;

    if (bsx != 4) {
        ALIGN16(pel_t first_line[64 + 256]);
        int line_size = bsx + ((bsy - 1) << 2);
        int iHeight4 = bsy << 2;

        for (i = 0; i < line_size; i += 4, src--) {
            first_line[i    ] = (pel_t)((src[ 0] * 3 +  src[-1] * 7 + src[-2]  * 5 + src[-3]     + 8) >> 4);
            first_line[i + 1] = (pel_t)((src[ 0]     + (src[-1]     + src[-2]) * 3 + src[-3]     + 4) >> 3);
            first_line[i + 2] = (pel_t)((src[ 0]     +  src[-1] * 5 + src[-2]  * 7 + src[-3] * 3 + 8) >> 4);
            first_line[i + 3] = (pel_t)((src[-1]     +  src[-2] * 2 + src[-3]                    + 2) >> 2);
        }

        for (i = 0; i < iHeight4; i += 4) {
            memcpy(dst, first_line + i, bsx * sizeof(pel_t));
            dst += i_dst;
        }
    } else {
        for (i = 0; i < bsy; i++, src--) {
            dst[0] = (pel_t)((src[ 0] * 3 +  src[-1] * 7 + src[-2]  * 5 + src[-3]     + 8) >> 4);
            dst[1] = (pel_t)((src[ 0]     + (src[-1]     + src[-2]) * 3 + src[-3]     + 4) >> 3);
            dst[2] = (pel_t)((src[ 0]     +  src[-1] * 5 + src[-2]  * 7 + src[-3] * 3 + 8) >> 4);
            dst[3] = (pel_t)((src[-1]     +  src[-2] * 2 + src[-3]                    + 2) >> 2);
            dst += i_dst;
        }
    }
}

/* ---------------------------------------------------------------------------
 */
static void intra_pred_ang_y_27_c(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy)
{
    int i;
    if (bsx > 8){
        intra_pred_ang_y_c(src, dst, i_dst, dir_mode, bsx, bsy);
    } else if (bsx == 8){
        for (i = 0; i < bsy; i++, src--) {
            dst[0] = (pel_t)((21 * src[0] +  53 * src[-1] + 43 * src[-2] + 11 * src[-3] + 64) >> 7);
            dst[1] = (pel_t)(( 9 * src[0] +  41 * src[-1] + 55 * src[-2] + 23 * src[-3] + 64) >> 7);
            dst[2] = (pel_t)((15 * src[-1] + 31 * src[-2] + 17 * src[-3] +  1 * src[-4] + 32) >> 6);
            dst[3] = (pel_t)(( 9 * src[-1] + 25 * src[-2] + 23 * src[-3] +  7 * src[-4] + 32) >> 6);

            dst[4] = (pel_t)(( 3 * src[-1] + 19 * src[-2] + 29 * src[-3] + 13 * src[-4] + 32) >> 6);
            dst[5] = (pel_t)((27 * src[-2] + 59 * src[-3] + 37 * src[-4] +  5 * src[-5] + 64) >> 7);
            dst[6] = (pel_t)((15 * src[-2] + 47 * src[-3] + 49 * src[-4] + 17 * src[-5] + 64) >> 7);
            dst[7] = (pel_t)(( 3 * src[-2] + 35 * src[-3] + 61 * src[-4] + 29 * src[-5] + 64) >> 7);
            dst += i_dst;
        }
    } else{
        for (i = 0; i < bsy; i++, src--) {
            dst[0] = (pel_t)((21 * src[0]  + 53 * src[-1] + 43 * src[-2] + 11 * src[-3] + 64) >> 7);
            dst[1] = (pel_t)(( 9 * src[0]  + 41 * src[-1] + 55 * src[-2] + 23 * src[-3] + 64) >> 7);
            dst[2] = (pel_t)((15 * src[-1] + 31 * src[-2] + 17 * src[-3] +  1 * src[-4] + 32) >> 6);
            dst[3] = (pel_t)(( 9 * src[-1] + 25 * src[-2] + 23 * src[-3] +  7 * src[-4] + 32) >> 6);
            dst += i_dst;
        }
    }
}

/* ---------------------------------------------------------------------------
 */
static void intra_pred_ang_y_28_c(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy)
{
    ALIGN16(pel_t first_line[64 + 128]);
    int line_size = bsx + ((bsy - 1) << 1);
#if !BUGFIX_PREDICTION_INTRA
    int real_size = STARAVS_MIN(line_size, (bsy << 2));
#endif
    int iHeight2 = bsy << 1;
    int i;
#if !BUGFIX_PREDICTION_INTRA
    pel_t pad1, pad2;
#endif

#if BUGFIX_PREDICTION_INTRA
    for (i = 0; i < line_size; i += 2, src--) {
#else
    for (i = 0; i < real_size; i += 2, src--) {
#endif
        first_line[i    ] = (pel_t)((src[ 0] + (src[-1] + src[-2]) * 3 + src[-3] + 4) >> 3);
        first_line[i + 1] = (pel_t)((src[-1] + (src[-2] << 1)          + src[-3] + 2) >> 2);
    }

#if !BUGFIX_PREDICTION_INTRA
    // padding
    if (real_size < line_size) {
        first_line[i - 1] = first_line[i - 3];

        pad1 = first_line[i - 2];
        pad2 = first_line[i - 1];
        for (; i < line_size; i += 2) {
            first_line[i    ] = pad1;
            first_line[i + 1] = pad2;
        }
    }
#endif

    for (i = 0; i < iHeight2; i += 2) {
        memcpy(dst, first_line + i, bsx * sizeof(pel_t));
        dst += i_dst;
    }
}

/* ---------------------------------------------------------------------------
 */
static void intra_pred_ang_y_29_c(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy)
{
    int i;
    if (bsx > 8) {
        intra_pred_ang_y_c(src, dst, i_dst, dir_mode, bsx, bsy);
    } else if (bsx == 8) {
        for (i = 0; i < bsy; i++, src--) {
            dst[0] = (pel_t)((src[0] * 9 + src[-1] * 41 + src[-2] * 55 + src[-3] * 23 + 64) >> 7);
            dst[1] = (pel_t)((src[-1] * 9 + src[-2] * 25 + src[-3] * 23 + src[-4] * 7 + 32) >> 6);
            dst[2] = (pel_t)((src[-2] * 27 + src[-3] * 59 + src[-4] * 37 + src[-5] * 5 + 64) >> 7);
            dst[3] = (pel_t)((src[-2] * 3 + src[-3] * 35 + src[-4] * 61 + src[-5] * 29 + 64) >> 7);

            dst[4] = (pel_t)((src[-3] * 3 + src[-4] * 11 + src[-5] * 13 + src[-6] * 5 + 16) >> 5);
            dst[5] = (pel_t)((src[-4] * 21 + src[-5] * 53 + src[-6] * 43 + src[-7] * 11 + 64) >> 7);
            dst[6] = (pel_t)((src[-5] * 15 + src[-6] * 31 + src[-7] * 17 + src[-8] + 32) >> 6);
            dst[7] = (pel_t)((src[-5] * 3 + src[-6] * 19 + src[-7] * 29 + src[-8] * 13 + 32) >> 6);
            dst += i_dst;
        }
    } else {
        for (i = 0; i < bsy; i++, src--) {
            dst[0] = (pel_t)((src[0] * 9 + src[-1] * 41 + src[-2] * 55 + src[-3] * 23 + 64) >> 7);
            dst[1] = (pel_t)((src[-1] * 9 + src[-2] * 25 + src[-3] * 23 + src[-4] * 7 + 32) >> 6);
            dst[2] = (pel_t)((src[-2] * 27 + src[-3] * 59 + src[-4] * 37 + src[-5] * 5 + 64) >> 7);
            dst[3] = (pel_t)((src[-2] * 3 + src[-3] * 35 + src[-4] * 61 + src[-5] * 29 + 64) >> 7);
            dst += i_dst;
        }
    }
}

/* ---------------------------------------------------------------------------
 */
static void intra_pred_ang_y_30_c(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy)
{
    ALIGN16(pel_t first_line[64 + 64]);
    int line_size = bsx + bsy - 1;
#if !BUGFIX_PREDICTION_INTRA
    int real_size = STARAVS_MIN(line_size, (bsy << 1) - 1);
#endif
    int i;
#if !BUGFIX_PREDICTION_INTRA
    pel_t pad;
#endif

    src -= 2;
#if BUGFIX_PREDICTION_INTRA
    for (i = 0; i < line_size; i++, src--) {
#else
    for (i = 0; i < real_size; i++, src--) {
#endif
        first_line[i] = (pel_t)((src[-1] + (src[0] << 1) + src[1] + 2) >> 2);
    }

#if !BUGFIX_PREDICTION_INTRA
    // padding
    pad = first_line[real_size - 1];
    for (; i < line_size; i++) {
        first_line[i] = pad;
    }
#endif

    for (i = 0; i < bsy; i++) {
        memcpy(dst, first_line + i, bsx * sizeof(pel_t));
        dst += i_dst;
    }
}

/* ---------------------------------------------------------------------------
 */
static void intra_pred_ang_y_31_c(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy)
{
    ALIGN16(pel_t dst_tran[MAX_CU_SIZE * MAX_CU_SIZE]);
    ALIGN16(pel_t src_tran[MAX_CU_SIZE << 3]);
    int i;
    if (bsx >= bsy){
        // transposition
#if BUGFIX_PREDICTION_INTRA
        //i < (bsx * 19 / 8 + 3)
        for (i = 0; i < (bsy + bsx * 11 / 8 + 3); i++){
#else
        for (i = 0; i < (2 * bsy + 3); i++){
#endif
            src_tran[i] = src[-i];
        }
        intra_pred_ang_x_5_c(src_tran, dst_tran, bsy, 5, bsy, bsx);
        for (i = 0; i < bsy; i++){
            for (int j = 0; j < bsx; j++){
                dst[j + i_dst * i] = dst_tran[i + bsy * j];
            }
        }
    } else if (bsx == 8){
        for (i = 0; i < bsy; i++, src--) {
            dst[0] = (pel_t)((5 * src[-1] + 13 * src[-2] + 11 * src[-3] + 3 * src[-4] + 16) >> 5);
            dst[1] = (pel_t)((1 * src[-2] + 5 * src[-3] + 7 * src[-4] + 3 * src[-5] + 8) >> 4);
            dst[2] = (pel_t)((7 * src[-4] + 15 * src[-5] + 9 * src[-6] + 1 * src[-7] + 16) >> 5);
            dst[3] = (pel_t)((1 * src[-5] + 3 * src[-6] + 3 * src[-7] + 1 * src[-8] + 4) >> 3);

            dst[4] = (pel_t)((1 * src[-6] + 9 * src[-7] + 15 * src[-8] + 7 * src[-9] + 16) >> 5);
            dst[5] = (pel_t)((3 * src[-8] + 7 * src[-9] + 5 * src[-10] + 1 * src[-11] + 8) >> 4);
            dst[6] = (pel_t)((3 * src[-9] + 11 * src[-10] + 13 * src[-11] + 5 * src[-12] + 16) >> 5);
            dst[7] = (pel_t)((1 * src[-11] + 2 * src[-12] + 1 * src[-13] + 0 * src[-14] + 2) >> 2);
            dst += i_dst;
        }
    } else {
        for (i = 0; i < bsy; i++, src--) {
            dst[0] = (pel_t)((5 * src[-1] + 13 * src[-2] + 11 * src[-3] + 3 * src[-4] + 16) >> 5);
            dst[1] = (pel_t)((1 * src[-2] + 5 * src[-3] + 7 * src[-4] + 3 * src[-5] + 8) >> 4);
            dst[2] = (pel_t)((7 * src[-4] + 15 * src[-5] + 9 * src[-6] + 1 * src[-7] + 16) >> 5);
            dst[3] = (pel_t)((1 * src[-5] + 3 * src[-6] + 3 * src[-7] + 1 * src[-8] + 4) >> 3);
            dst += i_dst;
        }
    }
}

/* ---------------------------------------------------------------------------
 */
static void intra_pred_ang_y_32_c(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy)
{
    ALIGN16(pel_t first_line[2 * (32 + 64)]);
    int line_size = (bsy >> 1) + bsx - 1;
#if !BUGFIX_PREDICTION_INTRA
    int real_size = STARAVS_MIN(line_size, bsy - 1);
#endif
    int aligned_line_size = ((line_size + 15) >> 4) << 4;
    int i_dst2 = i_dst << 1;
    int i;
#if !BUGFIX_PREDICTION_INTRA
    pel_t pad;
#endif
    pel_t *pfirst[2];

    pfirst[0] = first_line;
    pfirst[1] = first_line + aligned_line_size;

    src -= 3;
#if BUGFIX_PREDICTION_INTRA
    for (i = 0; i < line_size; i++, src -= 2) {
#else
    for (i = 0; i < real_size; i++, src -= 2) {
#endif
        pfirst[0][i] = (pel_t)((src[1] + (src[ 0] << 1) + src[-1] + 2) >> 2);
        pfirst[1][i] = (pel_t)((src[0] + (src[-1] << 1) + src[-2] + 2) >> 2);
    }

#if !BUGFIX_PREDICTION_INTRA
    // padding
    pad = pfirst[1][i - 1];
    for (; i < line_size; i++) {
        pfirst[0][i] = pad;
        pfirst[1][i] = pad;
    }
#endif

    bsy >>= 1;
    for (i = 0; i < bsy; i++) {
        memcpy(dst        , pfirst[0] + i, bsx * sizeof(pel_t));
        memcpy(dst + i_dst, pfirst[1] + i, bsx * sizeof(pel_t));
        dst += i_dst2;
    }
}

/* ---------------------------------------------------------------------------
 */
static void intra_pred_ang_xy_13_c(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy)
{
    int i;
    if (bsy > 8) {
        ALIGN16(pel_t first_line[(64 + 16) << 3]);
        int line_size = bsx + (bsy >> 3) - 1;
        int left_size = line_size - bsx;
        int aligned_line_size = ((line_size + 15) >> 4) << 4;
        pel_t *pfirst[8];

        pfirst[0] = first_line;
        pfirst[1] = pfirst[0] + aligned_line_size;
        pfirst[2] = pfirst[1] + aligned_line_size;
        pfirst[3] = pfirst[2] + aligned_line_size;
        pfirst[4] = pfirst[3] + aligned_line_size;
        pfirst[5] = pfirst[4] + aligned_line_size;
        pfirst[6] = pfirst[5] + aligned_line_size;
        pfirst[7] = pfirst[6] + aligned_line_size;

        src -= bsy - 8;
        for (i = 0; i < left_size; i++, src += 8) {
            pfirst[0][i] = (pel_t)((src[6] + (src[7] << 1) + src[8] + 2) >> 2);
            pfirst[1][i] = (pel_t)((src[5] + (src[6] << 1) + src[7] + 2) >> 2);
            pfirst[2][i] = (pel_t)((src[4] + (src[5] << 1) + src[6] + 2) >> 2);
            pfirst[3][i] = (pel_t)((src[3] + (src[4] << 1) + src[5] + 2) >> 2);

            pfirst[4][i] = (pel_t)((src[2] + (src[3] << 1) + src[4] + 2) >> 2);
            pfirst[5][i] = (pel_t)((src[1] + (src[2] << 1) + src[3] + 2) >> 2);
            pfirst[6][i] = (pel_t)((src[0] + (src[1] << 1) + src[2] + 2) >> 2);
            pfirst[7][i] = (pel_t)((src[-1] + (src[0] << 1) + src[1] + 2) >> 2);
        }

        for (; i < line_size; i++, src++) {
            pfirst[0][i] = (pel_t)((7 * src[2] + 15 * src[1] + 9 * src[0] + src[-1] + 16) >> 5);
            pfirst[1][i] = (pel_t)((3 * src[2] + 7 * src[1] + 5 * src[0] + src[-1] + 8) >> 4);
            pfirst[2][i] = (pel_t)((5 * src[2] + 13 * src[1] + 11 * src[0] + 3 * src[-1] + 16) >> 5);
            pfirst[3][i] = (pel_t)((src[2] + 3 * src[1] + 3 * src[0] + src[-1] + 4) >> 3);

            pfirst[4][i] = (pel_t)((3 * src[2] + 11 * src[1] + 13 * src[0] + 5 * src[-1] + 16) >> 5);
            pfirst[5][i] = (pel_t)((src[2] + 5 * src[1] + 7 * src[0] + 3 * src[-1] + 8) >> 4);
            pfirst[6][i] = (pel_t)((src[2] + 9 * src[1] + 15 * src[0] + 7 * src[-1] + 16) >> 5);
            pfirst[7][i] = (pel_t)((src[1] + 2 * src[0] + src[-1] + 2) >> 2);
        }

        pfirst[0] += left_size;
        pfirst[1] += left_size;
        pfirst[2] += left_size;
        pfirst[3] += left_size;
        pfirst[4] += left_size;
        pfirst[5] += left_size;
        pfirst[6] += left_size;
        pfirst[7] += left_size;

        bsy >>= 3;
        for (i = 0; i < bsy; i++) {
            memcpy(dst, pfirst[0] - i, bsx * sizeof(pel_t));  dst += i_dst;
            memcpy(dst, pfirst[1] - i, bsx * sizeof(pel_t));  dst += i_dst;
            memcpy(dst, pfirst[2] - i, bsx * sizeof(pel_t));  dst += i_dst;
            memcpy(dst, pfirst[3] - i, bsx * sizeof(pel_t));  dst += i_dst;
            memcpy(dst, pfirst[4] - i, bsx * sizeof(pel_t));  dst += i_dst;
            memcpy(dst, pfirst[5] - i, bsx * sizeof(pel_t));  dst += i_dst;
            memcpy(dst, pfirst[6] - i, bsx * sizeof(pel_t));  dst += i_dst;
            memcpy(dst, pfirst[7] - i, bsx * sizeof(pel_t));  dst += i_dst;
        }
    } else if (bsy == 8) {
        pel_t *dst1 = dst;
        pel_t *dst2 = dst1 + i_dst;
        pel_t *dst3 = dst2 + i_dst;
        pel_t *dst4 = dst3 + i_dst;
        pel_t *dst5 = dst4 + i_dst;
        pel_t *dst6 = dst5 + i_dst;
        pel_t *dst7 = dst6 + i_dst;
        pel_t *dst8 = dst7 + i_dst;
        for (i = 0; i < bsx; i++, src++) {
            dst1[i] = (pel_t)((7 * src[2] + 15 * src[1] + 9 * src[0] + src[-1] + 16) >> 5);
            dst2[i] = (pel_t)((3 * src[2] + 7 * src[1] + 5 * src[0] + src[-1] + 8) >> 4);
            dst3[i] = (pel_t)((5 * src[2] + 13 * src[1] + 11 * src[0] + 3 * src[-1] + 16) >> 5);
            dst4[i] = (pel_t)((src[2] + 3 * src[1] + 3 * src[0] + src[-1] + 4) >> 3);

            dst5[i] = (pel_t)((3 * src[2] + 11 * src[1] + 13 * src[0] + 5 * src[-1] + 16) >> 5);
            dst6[i] = (pel_t)((src[2] + 5 * src[1] + 7 * src[0] + 3 * src[-1] + 8) >> 4);
            dst7[i] = (pel_t)((src[2] + 9 * src[1] + 15 * src[0] + 7 * src[-1] + 16) >> 5);
            dst8[i] = (pel_t)((src[1] + 2 * src[0] + src[-1]  + 2) >> 2);
        }
    } else {
        for (i = 0; i < bsx; i++, src++) {
            pel_t *dst1 = dst;
            pel_t *dst2 = dst1 + i_dst;
            pel_t *dst3 = dst2 + i_dst;
            pel_t *dst4 = dst3 + i_dst;
            dst1[i] = (pel_t)((7 * src[2] + 15 * src[1] +  9 * src[0] +     src[-1] + 16) >> 5);
            dst2[i] = (pel_t)((3 * src[2] +  7 * src[1] +  5 * src[0] +     src[-1] + 8) >> 4);
            dst3[i] = (pel_t)((5 * src[2] + 13 * src[1] + 11 * src[0] + 3 * src[-1] + 16) >> 5);
            dst4[i] = (pel_t)((    src[2] +  3 * src[1] +  3 * src[0] +     src[-1] + 4) >> 3);
        }
    }
}
static void intra_pred_ang_xy_14_c(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy)
{
    int i;

    if (bsy != 4) {
        ALIGN16(pel_t first_line[4 * (64 + 16)]);
        int line_size = bsx + (bsy >> 2) - 1;
        int left_size = line_size - bsx;
        int aligned_line_size = ((line_size + 15) >> 4) << 4;
        pel_t *pfirst[4];

        pfirst[0] = first_line;
        pfirst[1] = first_line + aligned_line_size;
        pfirst[2] = first_line + aligned_line_size * 2;
        pfirst[3] = first_line + aligned_line_size * 3;

        src -= bsy - 4;
        for (i = 0; i < left_size; i++, src += 4) {
            pfirst[0][i] = (pel_t)((src[ 2] + (src[3] << 1) + src[4] + 2) >> 2);
            pfirst[1][i] = (pel_t)((src[ 1] + (src[2] << 1) + src[3] + 2) >> 2);
            pfirst[2][i] = (pel_t)((src[ 0] + (src[1] << 1) + src[2] + 2) >> 2);
            pfirst[3][i] = (pel_t)((src[-1] + (src[0] << 1) + src[1] + 2) >> 2);
        }

        for (; i < line_size; i++, src++) {
            pfirst[0][i] = (pel_t)((src[-1]     +  src[0] * 5 + src[1]  * 7 + src[2] * 3 + 8) >> 4);
            pfirst[1][i] = (pel_t)((src[-1]     + (src[0]     + src[1]) * 3 + src[2]     + 4) >> 3);
            pfirst[2][i] = (pel_t)((src[-1] * 3 +  src[0] * 7 + src[1]  * 5 + src[2]     + 8) >> 4);
            pfirst[3][i] = (pel_t)((src[-1]     +  src[0] * 2 + src[1]                   + 2) >> 2);
        }

        pfirst[0] += left_size;
        pfirst[1] += left_size;
        pfirst[2] += left_size;
        pfirst[3] += left_size;

        bsy >>= 2;
        for (i = 0; i < bsy; i++) {
            memcpy(dst, pfirst[0] - i, bsx * sizeof(pel_t));
            dst += i_dst;
            memcpy(dst, pfirst[1] - i, bsx * sizeof(pel_t));
            dst += i_dst;
            memcpy(dst, pfirst[2] - i, bsx * sizeof(pel_t));
            dst += i_dst;
            memcpy(dst, pfirst[3] - i, bsx * sizeof(pel_t));
            dst += i_dst;
        }
    } else {
        pel_t *dst1 = dst;
        pel_t *dst2 = dst1 + i_dst;
        pel_t *dst3 = dst2 + i_dst;
        pel_t *dst4 = dst3 + i_dst;

        for (i = 0; i < bsx; i++, src++) {
            dst1[i] = (pel_t)((src[-1]     +  src[0] * 5 + src[1]  * 7 + src[2] * 3 + 8) >> 4);
            dst2[i] = (pel_t)((src[-1]     + (src[0]     + src[1]) * 3 + src[2]     + 4) >> 3);
            dst3[i] = (pel_t)((src[-1] * 3 +  src[0] * 7 + src[1]  * 5 + src[2]     + 8) >> 4);
            dst4[i] = (pel_t)((src[-1]     +  src[0] * 2 + src[1]                   + 2) >> 2);
        }
    }
}

/* ---------------------------------------------------------------------------
 */
static void intra_pred_ang_xy_16_c(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy)
{
    ALIGN16(pel_t first_line[2 * (64 + 32)]);
    int line_size = bsx + (bsy >> 1) - 1;
    int left_size = line_size - bsx;
    int aligned_line_size = ((line_size + 15) >> 4) << 4;
    int i_dst2 = i_dst << 1;
    pel_t *pfirst[2];
    int i;

    pfirst[0] = first_line;
    pfirst[1] = first_line + aligned_line_size;

    src -= bsy - 2;
    for (i = 0; i < left_size; i++, src += 2) {
        pfirst[0][i] = (pel_t)((src[ 0] + (src[1] << 1) + src[2] + 2) >> 2);
        pfirst[1][i] = (pel_t)((src[-1] + (src[0] << 1) + src[1] + 2) >> 2);
    }

    for (; i < line_size; i++, src++) {
        pfirst[0][i] = (pel_t)((src[-1] + (src[0]       + src[1]) * 3 + src[2] + 4) >> 3);
        pfirst[1][i] = (pel_t)((src[-1] + (src[0] << 1) + src[1]               + 2) >> 2);
    }

    pfirst[0] += left_size;
    pfirst[1] += left_size;

    bsy >>= 1;
    for (i = 0; i < bsy; i++) {
        memcpy(dst        , pfirst[0] - i, bsx * sizeof(pel_t));
        memcpy(dst + i_dst, pfirst[1] - i, bsx * sizeof(pel_t));
        dst += i_dst2;
    }
}

/* ---------------------------------------------------------------------------
 */
static void intra_pred_ang_xy_18_c(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy)
{
    ALIGN16(pel_t first_line[64 + 64]);
    int line_size = bsx + bsy - 1;
    int i;
    pel_t *pfirst = first_line + bsy - 1;

    src -= bsy - 1;
    for (i = 0; i < line_size; i++, src++) {
        first_line[i] = (pel_t)((src[-1] + (src[0] << 1) + src[1] + 2) >> 2);
    }

    for (i = 0; i < bsy; i++) {
        memcpy(dst, pfirst, bsx * sizeof(pel_t));
        pfirst--;
        dst += i_dst;
    }
}

/* ---------------------------------------------------------------------------
 */
static void intra_pred_ang_xy_20_c(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy)
{
    ALIGN16(pel_t first_line[64 + 128]);
    int left_size = ((bsy - 1) << 1) + 1;
    int top_size = bsx - 1;
    int line_size = left_size + top_size;
    int i;
    pel_t *pfirst = first_line + left_size - 1;

    src -= bsy;
    for (i = 0; i < left_size; i += 2, src++) {
        first_line[i    ] = (pel_t)((src[-1] + (src[0] +  src[1]) * 3  + src[2] + 4) >> 3);
        first_line[i + 1] = (pel_t)((           src[0] + (src[1] << 1) + src[2] + 2) >> 2);
    }
    i--;

    for (; i < line_size; i++, src++) {
        first_line[i] = (pel_t)((src[-1] + (src[0] << 1) + src[1] + 2) >> 2);
    }

    for (i = 0; i < bsy; i++) {
        memcpy(dst, pfirst, bsx * sizeof(pel_t));
        pfirst -= 2;
        dst    += i_dst;
    }
}

/* ---------------------------------------------------------------------------
 */
static void intra_pred_ang_xy_22_c(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy)
{
    int i;

    if (bsx != 4) {
        src -= bsy;
        ALIGN16(pel_t first_line[64 + 256]);
        int left_size = ((bsy - 1) << 2) + 3;
        int top_size  = bsx - 3;
        int line_size = left_size + top_size;
        pel_t *pfirst = first_line + left_size - 3;

        for (i = 0; i < left_size; i += 4, src++) {
            first_line[i    ] = (pel_t)((src[-1] * 3 +  src[0] * 7 + src[1]  * 5 + src[2]     + 8) >> 4);
            first_line[i + 1] = (pel_t)((src[-1]     + (src[0]     + src[1]) * 3 + src[2]     + 4) >> 3);
            first_line[i + 2] = (pel_t)((src[-1]     +  src[0] * 5 + src[1]  * 7 + src[2] * 3 + 8) >> 4);
            first_line[i + 3] = (pel_t)((               src[0]     + src[1]  * 2 + src[2]     + 2) >> 2);
        }
        i--;

        for (; i < line_size; i++, src++) {
            first_line[i] = (pel_t)((src[-1] + (src[0] << 1) + src[1] + 2) >> 2);
        }

        for (i = 0; i < bsy; i++) {
            memcpy(dst, pfirst, bsx * sizeof(pel_t));
            dst    += i_dst;
            pfirst -= 4;
        }
    } else {
        for (i = 0; i < bsy; i++, src--) {
            dst[0] = (pel_t)((src[-2] * 3 +  src[-1] * 7 + src[0]  * 5 + src[1]     + 8) >> 4);
            dst[1] = (pel_t)((src[-2]     + (src[-1]     + src[0]) * 3 + src[1]     + 4) >> 3);
            dst[2] = (pel_t)((src[-2]     +  src[-1] * 5 + src[0]  * 7 + src[1] * 3 + 8) >> 4);
            dst[3] = (pel_t)((               src[-1]     + src[0]  * 2 + src[1]     + 2) >> 2);
            dst += i_dst;
        }
        // needn't pad, (3,0) is equal for ang_x and ang_y
    }
}

/* ---------------------------------------------------------------------------
*/
static void intra_pred_ang_xy_23_c(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsx, int bsy)
{
    int i;

    if (bsx > 8) {
        ALIGN16(pel_t first_line[64 + 512]);
        int left_size = (bsy << 3) - 1;
        int top_size = bsx - 7;
        int line_size = left_size + top_size;
        pel_t *pfirst = first_line + left_size - 7;

        src -= bsy;
        for (i = 0; i < left_size; i += 8, src++) {
            first_line[i    ] = (pel_t)((7 * src[-1] + 15 * src[0] +  9 * src[1] +     src[2] + 16) >> 5);
            first_line[i + 1] = (pel_t)((3 * src[-1] +  7 * src[0] +  5 * src[1] +     src[2] +  8) >> 4);
            first_line[i + 2] = (pel_t)((5 * src[-1] + 13 * src[0] + 11 * src[1] + 3 * src[2] + 16) >> 5);
            first_line[i + 3] = (pel_t)((    src[-1] +  3 * src[0] +  3 * src[1] +     src[2] +  4) >> 3);

            first_line[i + 4] = (pel_t)((3 * src[-1] + 11 * src[0] + 13 * src[1] + 5 * src[2] + 16) >> 5);
            first_line[i + 5] = (pel_t)((    src[-1] +  5 * src[0] +  7 * src[1] + 3 * src[2] +  8) >> 4);
            first_line[i + 6] = (pel_t)((    src[-1] +  9 * src[0] + 15 * src[1] + 7 * src[2] + 16) >> 5);
            first_line[i + 7] = (pel_t)((    src[ 0] +  2 * src[1] +      src[2] + 0 * src[3] +  2) >> 2);
        }
        i--;

        for (; i < line_size; i++, src++) {
            first_line[i] = (pel_t)((src[1] + (src[0] << 1) + src[-1] + 2) >> 2);
        }

        for (i = 0; i < bsy; i++) {
            memcpy(dst, pfirst, bsx * sizeof(pel_t));
            dst += i_dst;
            pfirst -= 8;
        }
    } else if (bsx == 8) {
        for (i = 0; i < bsy; i++, src--) {
            dst[0] = (pel_t)((7 * src[-2] + 15 * src[-1] +  9 * src[0] +     src[1] + 16) >> 5);
            dst[1] = (pel_t)((3 * src[-2] +  7 * src[-1] +  5 * src[0] +     src[1] +  8) >> 4);
            dst[2] = (pel_t)((5 * src[-2] + 13 * src[-1] + 11 * src[0] + 3 * src[1] + 16) >> 5);
            dst[3] = (pel_t)((    src[-2] +  3 * src[-1] +  3 * src[0] +     src[1] +  4) >> 3);

            dst[4] = (pel_t)((3 * src[-2] + 11 * src[-1] + 13 * src[0] + 5 * src[1] + 16) >> 5);
            dst[5] = (pel_t)((    src[-2] +  5 * src[-1] +  7 * src[0] + 3 * src[1] +  8) >> 4);
            dst[6] = (pel_t)((    src[-2] +  9 * src[-1] + 15 * src[0] + 7 * src[1] + 16) >> 5);
            dst[7] = (pel_t)((    src[-1] +  2 * src[ 0] +      src[1] + 0 * src[2] +  2) >> 2);
            dst += i_dst;
        }
        // needn't pad, (7,0) is equal for ang_x and ang_y
    } else {
        for (i = 0; i < bsy; i++, src--) {
            dst[0] = (pel_t)((7 * src[-2] + 15 * src[-1] + 9 * src[0] + src[1] + 16) >> 5);
            dst[1] = (pel_t)((3 * src[-2] + 7 * src[-1] + 5 * src[0] + src[1] + 8) >> 4);
            dst[2] = (pel_t)((5 * src[-2] + 13 * src[-1] + 11 * src[0] + 3 * src[1] + 16) >> 5);
            dst[3] = (pel_t)((src[-2] + 3 * src[-1] + 3 * src[0] + src[1] + 4) >> 3);
            dst += i_dst;
        }
    }
}

/* ---------------------------------------------------------------------------
 * fill reference samples for intra prediction
 * LCU内在上边界的PU
 */
static 
void fill_reference_samples_0_c(const pel_t *pTL, int i_TL, const pel_t *pLcuEP, pel_t *EP, uint32_t i_avai, int bsx, int bsy)
{
    int num_padding = 0;

    /* fill default value */
    mem_repeat_p(&EP[-(bsy << 1)], g_dc_value, ((bsy + bsx) << 1) + 1);

    /* get prediction pixels ---------------------------------------
     * extra pixels          | left-down pixels   | left pixels   | top-left | top pixels  | top-right pixels  | extra pixels
     * -2*bsy-4 ... -2*bsy-1 | -bsy-bsy ... -bsy-1| -bsy -3 -2 -1 |     0    | 1 2 ... bsx | bsx+1 ... bsx+bsx | 2*bsx+1 ... 2*bsx+4
     */

    /* fill top & top-right pixels */
    if (IS_NEIGHBOR_AVAIL(i_avai, MD_I_TOP)) {
        /* fill top pixels */
        gf_davs2.fast_memcpy(&EP[1], &pLcuEP[1], bsx * sizeof(pel_t));
    }

    /* fill top-right pixels */
    if (IS_NEIGHBOR_AVAIL(i_avai, MD_I_TOP_RIGHT)) {
        gf_davs2.fast_memcpy(&EP[bsx + 1], &pLcuEP[bsx + 1], bsx * sizeof(pel_t));
    } else {
        mem_repeat_p(&EP[bsx + 1], EP[bsx], bsx);   // repeat the last pixel
    }

    /* fill extra pixels */
    num_padding = bsy * 11 / 4 - bsx + 4;
    if (num_padding > 0) {
        mem_repeat_p(&EP[2 * bsx + 1], EP[2 * bsx], num_padding); // from (2*bsx) to (iX + 3) = (bsy *11/4 + bsx - 1) + 3
    }

    /* fill left & left-down pixels */
    if (IS_NEIGHBOR_AVAIL(i_avai, MD_I_LEFT)) {
        /* fill left pixels */
        memcpy(&EP[-bsy], &pLcuEP[-bsy], bsy * sizeof(pel_t));
    }

    /* fill left-down pixels */
    if (IS_NEIGHBOR_AVAIL(i_avai, MD_I_LEFT_DOWN)) {
        memcpy(&EP[-2 * bsy], &pLcuEP[-2 * bsy], bsy * sizeof(pel_t));
    } else {
        mem_repeat_p(&EP[-(bsy << 1)], EP[-bsy], bsy);
    }

    /* fill top-left pixel */
    if (IS_NEIGHBOR_AVAIL(i_avai, MD_I_TOP_LEFT)) {
        EP[0] = pLcuEP[0];
    } else if (IS_NEIGHBOR_AVAIL(i_avai, MD_I_TOP)) {
        EP[0] = pLcuEP[1];
    } else if (IS_NEIGHBOR_AVAIL(i_avai, MD_I_LEFT)) {
        EP[0] = pLcuEP[-1];
    }

    /* fill extra pixels */
    num_padding = bsx * 11 / 4 - bsy + 4;
    if (num_padding > 0) {
        mem_repeat_p(&EP[-2 * bsy - num_padding], EP[-2 * bsy], num_padding); // from (-2*bsy) to (-iY - 3) = -(bsx *11/4 + bsy - 1) - 3
    }
}

/* ---------------------------------------------------------------------------
 * fill reference samples for intra prediction
 * LCU内在上边界的PU
 */
static 
void fill_reference_samples_x_c(const pel_t *pTL, int i_TL, const pel_t *pLcuEP, pel_t *EP, uint32_t i_avai, int bsx, int bsy)
{
    const pel_t *pL = pTL + i_TL;
    int num_padding = 0;

    /* fill default value */
    mem_repeat_p(&EP[-(bsy << 1)], g_dc_value, ((bsy + bsx) << 1) + 1);

    /* get prediction pixels ---------------------------------------
     * extra pixels          | left-down pixels   | left pixels   | top-left | top pixels  | top-right pixels  | extra pixels
     * -2*bsy-4 ... -2*bsy-1 | -bsy-bsy ... -bsy-1| -bsy -3 -2 -1 |     0    | 1 2 ... bsx | bsx+1 ... bsx+bsx | 2*bsx+1 ... 2*bsx+4
     */

    /* fill top & top-right pixels */
    if (IS_NEIGHBOR_AVAIL(i_avai, MD_I_TOP)) {
        /* fill top pixels */
        gf_davs2.fast_memcpy(&EP[1], &pLcuEP[1], bsx * sizeof(pel_t));
    }

    /* fill top-right pixels */
    if (IS_NEIGHBOR_AVAIL(i_avai, MD_I_TOP_RIGHT)) {
        gf_davs2.fast_memcpy(&EP[bsx + 1], &pLcuEP[bsx + 1], bsx * sizeof(pel_t));
    } else {
        mem_repeat_p(&EP[bsx + 1], EP[bsx], bsx);   // repeat the last pixel
    }

    /* fill extra pixels */
    num_padding = bsy * 11 / 4 - bsx + 4;
    if (num_padding > 0) {
        mem_repeat_p(&EP[2 * bsx + 1], EP[2 * bsx], num_padding); // from (2*bsx) to (iX + 3) = (bsy *11/4 + bsx - 1) + 3
    }

    /* fill left & left-down pixels */
    if (IS_NEIGHBOR_AVAIL(i_avai, MD_I_LEFT)) {
        const pel_t *p_l = pL;
        int y;
        /* fill left pixels */
        for (y = 0; y < bsy; y++) {
            EP[-1 - y] = *p_l;
            p_l += i_TL;
        }
    }

    /* fill left-down pixels */
    if (IS_NEIGHBOR_AVAIL(i_avai, MD_I_LEFT_DOWN)) {
        int y;
        const pel_t *p_l = pL + bsy * i_TL;

        for (y = 0; y < bsy; y++) {
            EP[-bsy - 1 - y] = *p_l;
            p_l += i_TL;
        }
    } else {
        mem_repeat_p(&EP[-(bsy << 1)], EP[-bsy], bsy);
    }

    /* fill top-left pixel */
    if (IS_NEIGHBOR_AVAIL(i_avai, MD_I_TOP_LEFT)) {
        EP[0] = pLcuEP[0];
    } else if (IS_NEIGHBOR_AVAIL(i_avai, MD_I_TOP)) {
        EP[0] = pLcuEP[1];
    } else if (IS_NEIGHBOR_AVAIL(i_avai, MD_I_LEFT)) {
        EP[0] = pL[0];
    }

    /* fill extra pixels */
    num_padding = bsx * 11 / 4 - bsy + 4;
    if (num_padding > 0) {
        mem_repeat_p(&EP[-2 * bsy - num_padding], EP[-2 * bsy], num_padding); // from (-2*bsy) to (-iY - 3) = -(bsx *11/4 + bsy - 1) - 3
    }
}

/* ---------------------------------------------------------------------------
 * fill reference samples for intra prediction
 * LCU内在左边界上的PU
 */
static 
void fill_reference_samples_y_c(const pel_t *pTL, int i_TL, const pel_t *pLcuEP, pel_t *EP, uint32_t i_avai, int bsx, int bsy)
{
    const pel_t *pT = pTL + 1;
    int num_padding = 0;

    /* fill default value */
    mem_repeat_p(&EP[-(bsy << 1)], g_dc_value, ((bsy + bsx) << 1) + 1);

    /* get prediction pixels ---------------------------------------
     * extra pixels          | left-down pixels   | left pixels   | top-left | top pixels  | top-right pixels  | extra pixels
     * -2*bsy-4 ... -2*bsy-1 | -bsy-bsy ... -bsy-1| -bsy -3 -2 -1 |     0    | 1 2 ... bsx | bsx+1 ... bsx+bsx | 2*bsx+1 ... 2*bsx+4
     */

    /* fill top & top-right pixels */
    if (IS_NEIGHBOR_AVAIL(i_avai, MD_I_TOP)) {
        /* fill top pixels */
        gf_davs2.fast_memcpy(&EP[1], pT, bsx * sizeof(pel_t));
    }

    /* fill top-right pixels */
    if (IS_NEIGHBOR_AVAIL(i_avai, MD_I_TOP_RIGHT)) {
        gf_davs2.fast_memcpy(&EP[bsx + 1], &pT[bsx], bsx * sizeof(pel_t));
    } else {
        mem_repeat_p(&EP[bsx + 1], EP[bsx], bsx);   // repeat the last pixel
    }

    /* fill extra pixels */
    num_padding = bsy * 11 / 4 - bsx + 4;
    if (num_padding > 0) {
        mem_repeat_p(&EP[2 * bsx + 1], EP[2 * bsx], num_padding); // from (2*bsx) to (iX + 3) = (bsy *11/4 + bsx - 1) + 3
    }

    /* fill left & left-down pixels */
    if (IS_NEIGHBOR_AVAIL(i_avai, MD_I_LEFT)) {
        /* fill left pixels */
        memcpy(&EP[-bsy], &pLcuEP[-bsy], bsy * sizeof(pel_t));
    }

    /* fill left-down pixels */
    if (IS_NEIGHBOR_AVAIL(i_avai, MD_I_LEFT_DOWN)) {
        memcpy(&EP[-2 * bsy], &pLcuEP[-2 * bsy], bsy * sizeof(pel_t));
    } else {
        mem_repeat_p(&EP[-(bsy << 1)], EP[-bsy], bsy);
    }

    /* fill top-left pixel */
    if (IS_NEIGHBOR_AVAIL(i_avai, MD_I_TOP_LEFT)) {
        EP[0] = pLcuEP[0];
    } else if (IS_NEIGHBOR_AVAIL(i_avai, MD_I_TOP)) {
        EP[0] = pT[0];
    } else if (IS_NEIGHBOR_AVAIL(i_avai, MD_I_LEFT)) {
        EP[0] = pLcuEP[-1];
    }

    /* fill extra pixels */
    num_padding = bsx * 11 / 4 - bsy + 4;
    if (num_padding > 0) {
        mem_repeat_p(&EP[-2 * bsy - num_padding], EP[-2 * bsy], num_padding); // from (-2*bsy) to (-iY - 3) = -(bsx *11/4 + bsy - 1) - 3
    }
}

/* ---------------------------------------------------------------------------
 * fill reference samples for intra prediction
 * LCU内不在边界上的PU
 */
static 
void fill_reference_samples_xy_c(const pel_t *pTL, int i_TL, const pel_t *pLcuEP, pel_t *EP, uint32_t i_avai, int bsx, int bsy)
{
    const pel_t *pT = pTL + 1;
    const pel_t *pL = pTL + i_TL;
    int num_padding = 0;

    /* fill default value */
    mem_repeat_p(&EP[-(bsy << 1)], g_dc_value, ((bsy + bsx) << 1) + 1);

    /* get prediction pixels ---------------------------------------
     * extra pixels          | left-down pixels   | left pixels   | top-left | top pixels  | top-right pixels  | extra pixels
     * -2*bsy-4 ... -2*bsy-1 | -bsy-bsy ... -bsy-1| -bsy -3 -2 -1 |     0    | 1 2 ... bsx | bsx+1 ... bsx+bsx | 2*bsx+1 ... 2*bsx+4
     */

    /* fill top & top-right pixels */
    if (IS_NEIGHBOR_AVAIL(i_avai, MD_I_TOP)) {
        /* fill top pixels */
        gf_davs2.fast_memcpy(&EP[1], pT, bsx * sizeof(pel_t));
    }

    /* fill top-right pixels */
    if (IS_NEIGHBOR_AVAIL(i_avai, MD_I_TOP_RIGHT)) {
        gf_davs2.fast_memcpy(&EP[bsx + 1], &pT[bsx], bsx * sizeof(pel_t));
    } else {
        mem_repeat_p(&EP[bsx + 1], EP[bsx], bsx);   // repeat the last pixel
    }

    /* fill extra pixels */
    num_padding = bsy * 11 / 4 - bsx + 4;
    if (num_padding > 0) {
        mem_repeat_p(&EP[2 * bsx + 1], EP[2 * bsx], num_padding); // from (2*bsx) to (iX + 3) = (bsy *11/4 + bsx - 1) + 3
    }

    /* fill left & left-down pixels */
    if (IS_NEIGHBOR_AVAIL(i_avai, MD_I_LEFT)) {
        const pel_t *p_l = pL;
        int y;
        /* fill left pixels */
        for (y = 0; y < bsy; y++) {
            EP[-1 - y] = *p_l;
            p_l += i_TL;
        }
    }

    /* fill left-down pixels */
    if (IS_NEIGHBOR_AVAIL(i_avai, MD_I_LEFT_DOWN)) {
        int y;
        const pel_t *p_l = pL + bsy * i_TL;

        for (y = 0; y < bsy; y++) {
            EP[-bsy - 1 - y] = *p_l;
            p_l += i_TL;
        }
    } else {
        mem_repeat_p(&EP[-(bsy << 1)], EP[-bsy], bsy);
    }

    /* fill top-left pixel */
    if (IS_NEIGHBOR_AVAIL(i_avai, MD_I_TOP_LEFT)) {
        EP[0] = pTL[0];
    } else if (IS_NEIGHBOR_AVAIL(i_avai, MD_I_TOP)) {
        EP[0] = pT[0];
    } else if (IS_NEIGHBOR_AVAIL(i_avai, MD_I_LEFT)) {
        EP[0] = pL[0];
    }

    /* fill extra pixels */
    num_padding = bsx * 11 / 4 - bsy + 4;
    if (num_padding > 0) {
        mem_repeat_p(&EP[-2 * bsy - num_padding], EP[-2 * bsy], num_padding); // from (-2*bsy) to (-iY - 3) = -(bsx *11/4 + bsy - 1) - 3
    }
}

/* ---------------------------------------------------------------------------
 * make intra prediction for luma block
 */
void davs2_get_intra_pred(davs2_row_rec_t *row_rec, cu_t *p_cu, int predmode, int ctu_x, int ctu_y, int bsx, int bsy)
{
    const int xy = ((ctu_y != 0) << 1) + (ctu_x != 0);
    pel_t *EP = row_rec->buf_edge_pixels + (MAX_CU_SIZE << 2) - 1;
    int b8_x = (ctu_x >> MIN_PU_SIZE_IN_BIT) + row_rec->ctu.i_spu_x;
    int b8_y = (ctu_y >> MIN_PU_SIZE_IN_BIT) + row_rec->ctu.i_spu_y;
    int i_pred = row_rec->ctu.i_fdec[0];
    pel_t *p_pred = row_rec->ctu.p_fdec[0] + ctu_y * i_pred + ctu_x;
    pel_t *pTL;
    int i_src;
    uint32_t avail;

    assert(predmode >= 0 && predmode < NUM_INTRA_MODE);
    avail = get_intra_neighbors(row_rec->h, b8_x, b8_y, bsx, bsy, p_cu->i_slice_nr);

    row_rec->b_block_avail_top  = (bool_t)IS_NEIGHBOR_AVAIL(avail, MD_I_TOP ); // used for second transform
    row_rec->b_block_avail_left = (bool_t)IS_NEIGHBOR_AVAIL(avail, MD_I_LEFT); // used for second transform

    i_src = i_pred;
    pTL   = p_pred - i_src - 1;

    gf_davs2.fill_edge_f[xy](pTL, i_src, row_rec->ctu_border[0].rec_top + ctu_x - ctu_y, EP, avail, bsx, bsy);

    intra_pred(EP, p_pred, i_pred, predmode, bsy, bsx, avail);
}


/* ---------------------------------------------------------------------------
 * make intra prediction for chroma block
 */
void davs2_get_intra_pred_chroma(davs2_row_rec_t *row_rec, cu_t *p_cu, int ctu_c_x, int ctu_c_y)
{
    static const int TAB_CHROMA_MODE_TO_REAL_MODE[NUM_INTRA_MODE_CHROMA] = {
        DC_PRED, DC_PRED, HOR_PRED, VERT_PRED, BI_PRED
    };
    const int xy = ((ctu_c_y != 0) << 1) + (ctu_c_x != 0);
    pel_t *EP_u     = row_rec->buf_edge_pixels + (MAX_CU_SIZE << 1) - 1;
    pel_t *EP_v     = EP_u + (MAX_CU_SIZE << 2);
    int bsize_c     = 1 << (p_cu->i_cu_level - 1);
    int b8_x        = ((ctu_c_x << 1) >> MIN_PU_SIZE_IN_BIT) + row_rec->ctu.i_spu_x;
    int b8_y        = ((ctu_c_y << 1) >> MIN_PU_SIZE_IN_BIT) + row_rec->ctu.i_spu_y;
    int luma_mode   = p_cu->intra_pred_modes[0];
    int chroma_mode = p_cu->c_ipred_mode;
    int real_mode   = (chroma_mode == DM_PRED_C) ? luma_mode : TAB_CHROMA_MODE_TO_REAL_MODE[chroma_mode];
    uint32_t avail;

    /* 预测块位置 */
    int i_pred      = row_rec->ctu.i_fdec[1];
    pel_t *p_pred_u = row_rec->ctu.p_fdec[1] + ctu_c_y * i_pred + ctu_c_x;
    pel_t *p_pred_v = row_rec->ctu.p_fdec[2] + ctu_c_y * i_pred + ctu_c_x;

    /* 计算U、V分量的左上角像素点的位置 */
    int i_src       = i_pred;
    pel_t *pTL_u    = p_pred_u - i_src - 1;
    pel_t *pTL_v    = p_pred_v - i_src - 1;

    /* 参考边界可用性判断与参考边界填充 */
    avail = get_intra_neighbors(row_rec->h, b8_x, b8_y, bsize_c << 1, bsize_c << 1, p_cu->i_slice_nr);

    gf_davs2.fill_edge_f[xy](pTL_u, i_src, row_rec->ctu_border[1].rec_top + ctu_c_x - ctu_c_y, EP_u, avail, bsize_c, bsize_c);
    gf_davs2.fill_edge_f[xy](pTL_v, i_src, row_rec->ctu_border[2].rec_top + ctu_c_x - ctu_c_y, EP_v, avail, bsize_c, bsize_c);

    /* 执行预测 */
    intra_pred(EP_u, p_pred_u, i_pred, real_mode, bsize_c, bsize_c, avail);
    intra_pred(EP_v, p_pred_v, i_pred, real_mode, bsize_c, bsize_c, avail);
}


/* ---------------------------------------------------------------------------
 */
void davs2_intra_pred_init(uint32_t cpuid, ao_funcs_t *pf)
{
#define ANG_X_OFFSET    3
#define ANG_XY_OFFSET   13
#define ANG_Y_OFFSET    25
    int i;
    intra_pred_t *ipred = pf->intraf;

    pf->fill_edge_f[0]      = fill_reference_samples_0_c;
    pf->fill_edge_f[1]      = fill_reference_samples_x_c;
    pf->fill_edge_f[2]      = fill_reference_samples_y_c;
    pf->fill_edge_f[3]      = fill_reference_samples_xy_c;
    ipred[DC_PRED   ] = intra_pred_dc_c;                // 0
    ipred[PLANE_PRED] = intra_pred_plane_c;             // 1
    ipred[BI_PRED   ] = intra_pred_bilinear_c;          // 2

    for (i = ANG_X_OFFSET; i < VERT_PRED; i++) {
        ipred[i     ] = intra_pred_ang_x_c;             // 3 ~ 11
    }
    ipred[VERT_PRED ] = intra_pred_ver_c;               // 12

    for (i = ANG_XY_OFFSET; i < HOR_PRED; i++) {
        ipred[i     ] = intra_pred_ang_xy_c;            // 13 ~ 23
    }

    ipred[HOR_PRED  ] = intra_pred_hor_c;               // 24
    for (i = ANG_Y_OFFSET; i < NUM_INTRA_MODE; i++) {
        ipred[i     ] = intra_pred_ang_y_c;             // 25 ~ 32
    }

    ipred[INTRA_ANG_X_3 ]  = intra_pred_ang_x_3_c;
    ipred[INTRA_ANG_X_4 ]  = intra_pred_ang_x_4_c;
    ipred[INTRA_ANG_X_5 ]  = intra_pred_ang_x_5_c;
    ipred[INTRA_ANG_X_6 ]  = intra_pred_ang_x_6_c;
    ipred[INTRA_ANG_X_7 ]  = intra_pred_ang_x_7_c;
    ipred[INTRA_ANG_X_8 ]  = intra_pred_ang_x_8_c;
    ipred[INTRA_ANG_X_9 ]  = intra_pred_ang_x_9_c;
    ipred[INTRA_ANG_X_10]  = intra_pred_ang_x_10_c;
    ipred[INTRA_ANG_X_11]  = intra_pred_ang_x_11_c;

    ipred[INTRA_ANG_XY_13] = intra_pred_ang_xy_13_c;
    ipred[INTRA_ANG_XY_14] = intra_pred_ang_xy_14_c;
    ipred[INTRA_ANG_XY_16] = intra_pred_ang_xy_16_c;
    ipred[INTRA_ANG_XY_18] = intra_pred_ang_xy_18_c;
    ipred[INTRA_ANG_XY_20] = intra_pred_ang_xy_20_c;
    ipred[INTRA_ANG_XY_22] = intra_pred_ang_xy_22_c;
    ipred[INTRA_ANG_XY_23] = intra_pred_ang_xy_23_c;

    ipred[INTRA_ANG_Y_25]  = intra_pred_ang_y_25_c;
    ipred[INTRA_ANG_Y_26]  = intra_pred_ang_y_26_c;
    ipred[INTRA_ANG_Y_27]  = intra_pred_ang_y_27_c;
    ipred[INTRA_ANG_Y_28]  = intra_pred_ang_y_28_c;
    ipred[INTRA_ANG_Y_29]  = intra_pred_ang_y_29_c;
    ipred[INTRA_ANG_Y_30]  = intra_pred_ang_y_30_c;
    ipred[INTRA_ANG_Y_31]  = intra_pred_ang_y_31_c;
    ipred[INTRA_ANG_Y_32]  = intra_pred_ang_y_32_c;

#if HAVE_MMX
    if (cpuid & DAVS2_CPU_SSE4) {
#if !HIGH_BIT_DEPTH
        ipred[DC_PRED   ] = intra_pred_dc_sse128;
        ipred[PLANE_PRED] = intra_pred_plane_sse128;
        ipred[BI_PRED   ] = intra_pred_bilinear_sse128;
        ipred[HOR_PRED  ] = intra_pred_hor_sse128;
        ipred[VERT_PRED ] = intra_pred_ver_sse128;

        ipred[INTRA_ANG_X_3  ] = intra_pred_ang_x_3_sse128;
        ipred[INTRA_ANG_X_4  ] = intra_pred_ang_x_4_sse128;
        ipred[INTRA_ANG_X_6  ] = intra_pred_ang_x_6_sse128;
        ipred[INTRA_ANG_X_8  ] = intra_pred_ang_x_8_sse128;
        ipred[INTRA_ANG_X_10 ] = intra_pred_ang_x_10_sse128;

        ipred[INTRA_ANG_XY_14] = intra_pred_ang_xy_14_sse128;
        ipred[INTRA_ANG_XY_16] = intra_pred_ang_xy_16_sse128;
        ipred[INTRA_ANG_XY_18] = intra_pred_ang_xy_18_sse128;
        ipred[INTRA_ANG_XY_20] = intra_pred_ang_xy_20_sse128;

        ipred[INTRA_ANG_X_5  ] = intra_pred_ang_x_5_sse128;
        //ipred[INTRA_ANG_X_7  ] = intra_pred_ang_x_7_sse128;
        //ipred[INTRA_ANG_X_9  ] = intra_pred_ang_x_9_sse128;
        //ipred[INTRA_ANG_X_11 ] = intra_pred_ang_x_11_sse128;

        ipred[INTRA_ANG_XY_13] = intra_pred_ang_xy_13_sse128;
        ipred[INTRA_ANG_XY_22] = intra_pred_ang_xy_22_sse128;
        ipred[INTRA_ANG_XY_23] = intra_pred_ang_xy_23_sse128;

        ipred[INTRA_ANG_Y_25 ] = intra_pred_ang_y_25_sse128;
        ipred[INTRA_ANG_Y_26 ] = intra_pred_ang_y_26_sse128;
        ipred[INTRA_ANG_Y_28 ] = intra_pred_ang_y_28_sse128;
        ipred[INTRA_ANG_Y_30 ] = intra_pred_ang_y_30_sse128;
        ipred[INTRA_ANG_Y_31 ] = intra_pred_ang_y_31_sse128;
        ipred[INTRA_ANG_Y_32 ] = intra_pred_ang_y_32_sse128;

        pf->fill_edge_f[0]      = fill_edge_samples_0_sse128;
        pf->fill_edge_f[1]      = fill_edge_samples_x_sse128;
        pf->fill_edge_f[2]      = fill_edge_samples_y_sse128;
        pf->fill_edge_f[3]      = fill_edge_samples_xy_sse128;
#endif
    }

    /* 8/10bit assemble*/
    if (cpuid & DAVS2_CPU_AVX2 ) {
#if !HIGH_BIT_DEPTH
        ipred[DC_PRED        ] = intra_pred_dc_avx;
        ipred[HOR_PRED       ] = intra_pred_hor_avx;
        ipred[VERT_PRED      ] = intra_pred_ver_avx;

        ipred[PLANE_PRED     ] = intra_pred_plane_avx;
        ipred[BI_PRED        ] = intra_pred_bilinear_avx;

        ipred[INTRA_ANG_X_3  ] = intra_pred_ang_x_3_avx;
        ipred[INTRA_ANG_X_4  ] = intra_pred_ang_x_4_avx;
        ipred[INTRA_ANG_X_5  ] = intra_pred_ang_x_5_avx;
        ipred[INTRA_ANG_X_6  ] = intra_pred_ang_x_6_avx;
        //ipred[INTRA_ANG_X_7  ] = intra_pred_ang_x_7_avx;
        ipred[INTRA_ANG_X_8  ] = intra_pred_ang_x_8_avx;
        //ipred[INTRA_ANG_X_9  ] = intra_pred_ang_x_9_avx;
        ipred[INTRA_ANG_X_10 ] = intra_pred_ang_x_10_avx;
        //ipred[INTRA_ANG_X_11 ] = intra_pred_ang_x_11_avx;

        ipred[INTRA_ANG_XY_13] = intra_pred_ang_xy_13_avx;
        ipred[INTRA_ANG_XY_14] = intra_pred_ang_xy_14_avx;
        ipred[INTRA_ANG_XY_16] = intra_pred_ang_xy_16_avx;
        ipred[INTRA_ANG_XY_18] = intra_pred_ang_xy_18_avx;
        ipred[INTRA_ANG_XY_20] = intra_pred_ang_xy_20_avx;
#if _MSC_VER  // TODO: 20180206 cause unextended exit on Linux
        ipred[INTRA_ANG_XY_22] = intra_pred_ang_xy_22_avx;
#endif
        ipred[INTRA_ANG_XY_23] = intra_pred_ang_xy_23_avx;

        ipred[INTRA_ANG_Y_25 ] = intra_pred_ang_y_25_avx;
        ipred[INTRA_ANG_Y_26 ] = intra_pred_ang_y_26_avx;
        ipred[INTRA_ANG_Y_28 ] = intra_pred_ang_y_28_avx;
        ipred[INTRA_ANG_Y_30 ] = intra_pred_ang_y_30_avx;
        ipred[INTRA_ANG_Y_31 ] = intra_pred_ang_y_31_avx;
        ipred[INTRA_ANG_Y_32 ] = intra_pred_ang_y_32_avx;
#endif
    }
#else
    UNUSED_PARAMETER(cpuid);
#endif //if HAVE_MMX

#undef ANG_X_OFFSET
#undef ANG_XY_OFFSET
#undef ANG_Y_OFFSET
}
