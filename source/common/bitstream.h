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

#ifndef __AVS2_BITSTREAM_H__
#define __AVS2_BITSTREAM_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "common.h"

void bs_init(davs2_bs_t *bs, uint8_t *p_data, int i_data);
void bs_alain(davs2_bs_t *bs);
int  bs_left_bytes(davs2_bs_t *bs);
int  found_slice_header(davs2_bs_t *bs);
int  bs_get_start_code(davs2_bs_t *bs);
int  bs_dispose_pseudo_code(uint8_t *dst, uint8_t *src, int i_src);
const uint8_t * find_start_code(const uint8_t *data, int len);
int32_t find_pic_start_code(uint8_t prevbyte3, uint8_t prevbyte2, uint8_t prevbyte1, const uint8_t *data, int32_t len);

#ifdef __cplusplus
}
#endif
#endif  // __AVS2_BITSTREAM_H__
