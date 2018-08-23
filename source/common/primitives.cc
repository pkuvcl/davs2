/*
 * primitives.cc
 *
 * Description of this file:
 *    function handles initialize functions definition of the davs2 library
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
#include "primitives.h"
#include "cpu.h"
#include "intra.h"
#include "mc.h"
#include "transform.h"
#include "quant.h"
#include "deblock.h"
#include "sao.h"
#include "alf.h"

/* ---------------------------------------------------------------------------
 */
ao_funcs_t gf_davs2 = {0};

/* ---------------------------------------------------------------------------
 */
void init_all_primitives(uint32_t cpuid)
{
    if (gf_davs2.initial_count != 0) {
        // already initialed
        gf_davs2.initial_count++;
        return;
    }

    gf_davs2.initial_count = 1;
    gf_davs2.cpuid         = cpuid;

    /* init function handles */
    davs2_memory_init    (cpuid, &gf_davs2);
    davs2_intra_pred_init(cpuid, &gf_davs2);
    davs2_pixel_init     (cpuid, &gf_davs2);
    davs2_mc_init        (cpuid, &gf_davs2);
    davs2_quant_init     (cpuid, &gf_davs2);
    davs2_dct_init       (cpuid, &gf_davs2);
    davs2_deblock_init   (cpuid, &gf_davs2);
    davs2_sao_init       (cpuid, &gf_davs2);
    davs2_alf_init       (cpuid, &gf_davs2);

}
