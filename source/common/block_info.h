/*
 * block_info.h
 *
 * Description of this file:
 *    Block Infomation functions definition of the davs2 library
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

#ifndef DAVS2_BLOCK_INFO_H
#define DAVS2_BLOCK_INFO_H
#ifdef __cplusplus
extern "C" {
#endif

#define get_neighbor_cbp_y FPFX(get_neighbor_cbp_y)
int  get_neighbor_cbp_y(davs2_t *h, int xN, int yN, int scu_x, int scu_y, cu_t *p_cu);

#ifdef __cplusplus
}
#endif
#endif // DAVS2_BLOCK_INFO_H
