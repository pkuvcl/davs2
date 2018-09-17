/*
 * alf.h
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

#ifndef DAVS2_ALF_H
#define DAVS2_ALF_H
#ifdef __cplusplus
extern "C" {
#endif

#define alf_get_buffer_size FPFX(alf_get_buffer_size)
size_t alf_get_buffer_size(davs2_t *h);
#define alf_init_buffer FPFX(alf_init_buffer)
void alf_init_buffer    (davs2_t *h);

#define alf_lcurow FPFX(alf_lcurow)
void alf_lcurow(davs2_t *h, alf_param_t *p_alf_param, davs2_frame_t *p_tmp_frm, davs2_frame_t *p_dec_frm, int i_lcu_y);

#define alf_read_param FPFX(alf_read_param)
void alf_read_param(davs2_t *h, davs2_bs_t *bs);

#define davs2_alf_init FPFX(alf_init)
void davs2_alf_init(uint32_t cpuid, ao_funcs_t *fh);

#ifdef __cplusplus
}
#endif
#endif  // DAVS2_ALF_H
