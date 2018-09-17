/*
 * mc.h
 *
 * Description of this file:
 *    MC functions definition of the davs2 library
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

#ifndef DAVS2_MC_H
#define DAVS2_MC_H
#ifdef __cplusplus
extern "C" {
#endif

#define mc_luma FPFX(mc_luma)
void mc_luma  (davs2_t *h, pel_t *dst, int i_dst, int posx, int posy, int width, int height, pel_t *p_fref, int i_fref);
#define mc_chroma FPFX(mc_chroma)
void mc_chroma(davs2_t *h, pel_t *dst, int i_dst, int posx, int posy, int width, int height, pel_t *p_fref, int i_fref);

#ifdef __cplusplus
}
#endif
#endif  // DAVS2_MC_H
