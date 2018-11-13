/*
 * bitstream.h
 *
 * Description of this file:
 *    Bitstream functions definition of the davs2 library
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

#ifndef DAVS2_BITSTREAM_H
#define DAVS2_BITSTREAM_H
#ifdef __cplusplus
extern "C" {
#endif

#include "common.h"

#define bs_init FPFX(bs_init)
void bs_init(davs2_bs_t *bs, uint8_t *p_data, int i_data);
#define bs_align FPFX(bs_align)
void bs_align(davs2_bs_t *bs);
#define bs_left_bytes FPFX(bs_left_bytes)
int  bs_left_bytes(davs2_bs_t *bs);
#define found_slice_header FPFX(found_slice_header)
int  found_slice_header(davs2_bs_t *bs);
#define bs_get_start_code FPFX(bs_get_start_code)
int  bs_get_start_code(davs2_bs_t *bs);
#define bs_dispose_pseudo_code FPFX(bs_dispose_pseudo_code)
int  bs_dispose_pseudo_code(uint8_t *dst, uint8_t *src, int i_src);
#define find_start_code FPFX(find_start_code)
const uint8_t * find_start_code(const uint8_t *data, int len);
#define find_pic_start_code FPFX(find_pic_start_code)
int32_t find_pic_start_code(uint8_t prevbyte3, uint8_t prevbyte2, uint8_t prevbyte1, const uint8_t *data, int32_t len);

#ifdef __cplusplus
}
#endif
#endif  // DAVS2_BITSTREAM_H
