/*
 * intra.h
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

#ifndef DAVS2_INTRA_H
#define DAVS2_INTRA_H
#ifdef __cplusplus
extern "C" {
#endif
    
/* ---------------------------------------------------------------------------
 */
static ALWAYS_INLINE
void intra_pred(pel_t *src, pel_t *dst, int i_dst, int dir_mode, int bsy, int bsx, int i_avail)
{
    if (dir_mode != DC_PRED) {
        gf_davs2.intraf[dir_mode](src, dst, i_dst, dir_mode, bsx, bsy);
    } else {
        int b_top  = !!IS_NEIGHBOR_AVAIL(i_avail, MD_I_TOP);
        int b_left = !!IS_NEIGHBOR_AVAIL(i_avail, MD_I_LEFT);
        int mode_ex = ((b_top << 8) + b_left);

        gf_davs2.intraf[dir_mode](src, dst, i_dst, mode_ex, bsx, bsy);
    }
}

#define davs2_intra_pred_init FPFX(intra_pred_init)
void davs2_intra_pred_init(uint32_t cpuid, ao_funcs_t *pf);
#define davs2_get_intra_pred FPFX(get_intra_pred)
void davs2_get_intra_pred(davs2_row_rec_t *row_rec, cu_t *p_cu, int predmode, int ctu_x, int ctu_y, int bsx, int bsy);
#define davs2_get_intra_pred_chroma FPFX(get_intra_pred_chroma)
void davs2_get_intra_pred_chroma(davs2_row_rec_t *h, cu_t *p_cu, int ctu_c_x, int ctu_c_y);

#ifdef __cplusplus
}
#endif
#endif  // DAVS2_INTRA_H
