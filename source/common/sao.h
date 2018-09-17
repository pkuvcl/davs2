/*
 * sao.h
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

#ifndef DAVS2_SAO_H
#define DAVS2_SAO_H
#ifdef __cplusplus
extern "C" {
#endif

#define sao_read_lcu_param FPFX(sao_read_lcu_param)
void sao_read_lcu_param(davs2_t *h, int lcu_xy, bool_t *slice_sao_on, sao_t *sao_param);

#define sao_lcu FPFX(sao_lcu)
void sao_lcu(davs2_t *h, davs2_frame_t *p_tmp_frm, davs2_frame_t *p_dec_frm, int i_lcu_x, int i_lcu_y);
#define sao_lcurow FPFX(sao_lcurow)
void sao_lcurow(davs2_t *h, davs2_frame_t *p_tmp_frm, davs2_frame_t *p_dec_frm, int i_lcu_y);

#define davs2_sao_init FPFX(sao_init)
void davs2_sao_init(uint32_t cpuid, ao_funcs_t *fh);

#ifdef __cplusplus
}
#endif
#endif  // DAVS2_SAO_H
