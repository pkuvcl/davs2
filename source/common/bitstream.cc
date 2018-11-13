/*
 * bitstream.cc
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

#include "common.h"
#include "aec.h"
#include "bitstream.h"


/* ---------------------------------------------------------------------------
 * start code (in 32-bit)
 */
#define SEQENCE_START_CODE      0xB0010000
#define I_FRAME_START_CODE      0xB3010000
#define PB_FRAME_START_CODE     0xB6010000


/**
 * ===========================================================================
 * function defines
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 */
void bs_init(davs2_bs_t *bs, uint8_t *p_data, int i_data)
{
    bs->p_stream  = p_data;
    bs->i_stream  = i_data;
    bs->i_bit_pos = 0;
}

/* ---------------------------------------------------------------------------
 * align position in bitstream */
void bs_align(davs2_bs_t *bs)
{
    bs->i_bit_pos = ((bs->i_bit_pos + 7) >> 3) << 3;
}

/* ---------------------------------------------------------------------------
 */
int bs_left_bytes(davs2_bs_t *bs)
{
    return (bs->i_stream - (bs->i_bit_pos >> 3));
}

/* ---------------------------------------------------------------------------
 * Function   : try to find slice header in next forward bytes
 * Parameters :
 *      [in ] : bs_data   - pointer to the bit-stream data buffer
 * Return     : TRUE for slice header, otherwise FALSE
 * ---------------------------------------------------------------------------
 */
int found_slice_header(davs2_bs_t *bs)
{
    int num_bytes = 4;

    for (; num_bytes; num_bytes--) {
        uint8_t *data = bs->p_stream + ((bs->i_bit_pos + 7) >> 3);
        uint32_t code = *(uint32_t *)data;
        if ((code & 0x00FFFFFF) == 0x00010000 && ((code >> 24) <= SC_SLICE_CODE_MAX)) {
            return 1;
        }
        bs->i_bit_pos += 8;
    }

    return 0;
}

/* ---------------------------------------------------------------------------
 */
int bs_get_start_code(davs2_bs_t *bs)
{
    uint8_t *p_data  = bs->p_stream + ((bs->i_bit_pos + 7) >> 3);
    int i_left_bytes = bs_left_bytes(bs);
    int i_used_bytes = 0;

    /* find the start code '00 00 01 xx' */
    while (i_left_bytes >= 4 && (*(uint32_t *)p_data & 0x00FFFFFF) != 0x00010000) {
        p_data++;
        i_left_bytes--;
        i_used_bytes++;
    }

    if (i_left_bytes >= 4) {
        bs->i_bit_pos += (i_used_bytes << 3);
        return p_data[3];
    } else {
        return -1;
    }
}

/* ---------------------------------------------------------------------------
 * Function   : check bitstream & dispose the pseudo start code
 * Parameters :
 *       [in] : dst   - pointer to dst byte buffer
 *   [in/out] : src   - pointer to source byte buffer
 *   [in/out] : i_src - byte number of src
 * Return     : byte number of dst
 * ---------------------------------------------------------------------------
 */
int bs_dispose_pseudo_code(uint8_t *dst, uint8_t *src, int i_src)
{
    static const int BITMASK[] = { 0x00, 0x00, 0xc0, 0x00, 0xf0, 0x00, 0xfc, 0x00 };
    int b_found_start_code = 0;
    int leading_zeros  = 0;
    int last_bit_count = 0;
    int curr_bit_count = 0;
    int b_dispose = 0;
    int i_pos = 0;
    int i_dst = 0;
    uint8_t last_byte = 0;
    uint8_t curr_byte = 0;

    /* checking... */
    while (i_pos < i_src) {
        curr_byte = src[i_pos++];
        curr_bit_count = 8;
        switch (curr_byte) {
        case 0:
            if (b_found_start_code) {
                b_dispose          = 1; /* start code of first slice: [00 00 01 00] */
                b_found_start_code = 0;
            }
            leading_zeros++;
            break;
        case 1:
            if (leading_zeros >= 2) {
                /* find start code: [00 00 01] */
                b_found_start_code = 1;
                if (last_bit_count) {
                    /* terminate the fixing work before new start code */
                    last_bit_count = 0;
                    dst[i_dst++]   = 0; /* insert the dispose byte */
                }
            }
            leading_zeros = 0;
            break;
        case 2:
            if (b_dispose && leading_zeros == 2) {
                /* dispose the pseudo code, two bits */
                curr_bit_count = 6;
            }
            leading_zeros = 0;
            break;
        default:
            if (b_found_start_code) {
                if (curr_byte == SC_SEQUENCE_HEADER || curr_byte == SC_USER_DATA || curr_byte == SC_EXTENSION) {
                    b_dispose = 0;
                } else {
                    b_dispose = 1;
                }
                b_found_start_code = 0;
            }
            leading_zeros = 0;
            break;
        }

        if (curr_bit_count == 8) {
            if (last_bit_count == 0) {
                dst[i_dst++] = curr_byte;
            } else {
                dst[i_dst++] = ((last_byte & BITMASK[last_bit_count]) | ((curr_byte & BITMASK[8 - last_bit_count]) >> last_bit_count));
                last_byte    = (curr_byte << (8 - last_bit_count)) & BITMASK[last_bit_count];
            }
        } else {
            if (last_bit_count == 0) {
                last_byte      = curr_byte;
                last_bit_count = curr_bit_count;
            } else {
                dst[i_dst++]   = ((last_byte & BITMASK[last_bit_count]) | ((curr_byte & BITMASK[8 - last_bit_count]) >> last_bit_count));
                last_byte      = (curr_byte << (8 - last_bit_count)) & BITMASK[last_bit_count - 2];
                last_bit_count = last_bit_count - 2;
            }
        }
    }

    if (last_bit_count != 0 && last_byte != 0) {
        dst[i_dst++] = last_byte;
    }

    return i_dst;
}

// ---------------------------------------------------------------------------
// find the first start code in byte stream
// return the byte address if found, or NULL on failure
const uint8_t *
find_start_code(const uint8_t *data, int len)
{
    while (len >= 4 && (*(uint32_t *)data & 0x00FFFFFF) != 0x00010000) {
        data++;
        len--;
    }

    return len >= 4 ? data : NULL;
}

// ---------------------------------------------------------------------------
// find the first picture or sequence start code in byte stream
int32_t
find_pic_start_code(uint8_t prevbyte3, uint8_t prevbyte2, uint8_t prevbyte1, const uint8_t *data, int32_t len)
{
#define ISPIC(x) ((x) == 0xB0 || (x) == 0xB1 || (x) == 0xB3 || (x) == 0xB6 || (x) == 0xB7)

    const uint8_t *p = NULL;
    const uint8_t *data0 = data;
    const int32_t  len0  = len;

    /* check start code: 00 00 01 xx */
    if (/*..*/ len >= 1 && (prevbyte3 == 0) && (prevbyte2 == 0) && (prevbyte1 == 1)) {
        if (ISPIC(data[0])) {
            return -3;          // found start code (position: -3)
        }
    } else if (len >= 2 && (prevbyte2 == 0) && (prevbyte1 == 0) && (data[0] == 1)) {
        if (ISPIC(data[1])) {
            return -2;          // found start code (position: -2)
        }
    } else if (len >= 3 && (prevbyte1 == 0) && (data[0] == 0) && (data[1] == 1)) {
        if (ISPIC(data[2])) {
            return -1;          // found start code (position: -1)
        }
    }

    /* check start code: 00 00 01 xx, ONLY in data buffer */
    while (((p = (uint8_t *)find_start_code(data, len)) != NULL) && !ISPIC(p[3])) {
        len -= (int32_t)(p - data + 4);
        data = p + 4;
    }

    return (int32_t)(p != NULL ? p - data0 : len0 + 1);

#undef ISPIC
}
