/*
 * block_info.cc
 *
 * Description of this file:
 *    Block-infomation functions definition of the davs2 library
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
cu_t *get_neighbor_cu_in_slice(davs2_t *h, cu_t *p_cur, int scu_x, int scu_y, int x4x4, int y4x4)
{
    const int shift_4x4 = MIN_CU_SIZE_IN_BIT - MIN_PU_SIZE_IN_BIT;

    if (x4x4 < 0 || y4x4 < 0 || x4x4 >= h->i_width_in_spu || y4x4 >= h->i_height_in_spu) {
        return NULL;
    } else if ((scu_x << shift_4x4) <= x4x4 && (scu_y << shift_4x4) <= y4x4) {
        return p_cur;
    } else {
        cu_t *p_neighbor = &h->scu_data[(y4x4 >> 1) * h->i_width_in_scu + (x4x4 >> 1)];
        return p_neighbor->i_slice_nr == p_cur->i_slice_nr ? p_neighbor : NULL;
    }
}

/* ---------------------------------------------------------------------------
 * (x_4x4, y_4x4) - 相邻变换块的4x4地址（图像）
 * (scu_x, scu_y) - 当前CU的SCU地址（图像）
 */
int get_neighbor_cbp_y(davs2_t *h, int x_4x4, int y_4x4, int scu_x, int scu_y, cu_t *p_cu)
{
    cu_t *p_neighbor = get_neighbor_cu_in_slice(h, p_cu, scu_x, scu_y, x_4x4, y_4x4);

    if (p_neighbor == NULL) {
        return 0;
    } else if (p_neighbor->i_trans_size == TU_SPLIT_NON) {
        return p_neighbor->i_cbp & 1;   // TU不划分时，直接返回对应亮度块CBP
    } else {
        int cbp     = p_neighbor->i_cbp;
        int level   = p_neighbor->i_cu_level - MIN_PU_SIZE_IN_BIT;
        int cu_mask = (1 << level) - 1;

        x_4x4 &= cu_mask;
        y_4x4 &= cu_mask;

        if (p_neighbor->i_trans_size == TU_SPLIT_VER) {           // 垂直划分
            x_4x4 >>= (level - 2);
            return (cbp >> x_4x4) & 1;
        } else if (p_neighbor->i_trans_size == TU_SPLIT_HOR) {    // 水平划分
            y_4x4 >>= (level - 2);
            return (cbp >> y_4x4) & 1;
        } else {                                                  // 四叉划分
            x_4x4 >>= (level - 1);
            y_4x4 >>= (level - 1);
            return (cbp >> (x_4x4 + (y_4x4 << 1))) & 1;
        }
    }
}
