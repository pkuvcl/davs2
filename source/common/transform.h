/*
 *  transform.h
 *
 * Description of this file:
 *    Transform functions definition of the davs2 library
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

#ifndef DAVS2_TRANSFORM_H
#define DAVS2_TRANSFORM_H
#ifdef __cplusplus
extern "C" {
#endif

#define davs2_dct_init FPFX(dct_init)
void davs2_dct_init(uint32_t cpuid, ao_funcs_t *fh);

#define davs2_get_recons FPFX(get_recons)
void davs2_get_recons(davs2_row_rec_t *row_rec, cu_t *p_cu, int blockidx, cb_t *p_tu, int ctu_x, int ctu_y);

#ifdef __cplusplus
}
#endif
#endif  // DAVS2_TRANSFORM_H
