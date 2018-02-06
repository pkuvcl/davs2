/*
 *  vlc.h
 *
 * Description of this file:
 *    VLC functions definition of the davs2 library
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

#ifndef DAVS2_VLC_H
#define DAVS2_VLC_H
#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
* reads bits from the bitstream buffer
* Input:
*      p_buf     - containing VLC-coded data bits
*      i_bit_pos - bit offset from start of partition
*      i_buf     - total bytes in bitstream
*      i_bits    - number of bits to read
* return 0 for success, otherwise failure
*/
static INLINE
int read_bits(uint8_t *p_buf, int i_buf, int i_bit_pos, int *p_info, int i_bits)
{
    int byte_offset = i_bit_pos >> 3;        // byte from start of buffer
    int bit_offset  = 7 - (i_bit_pos & 7);   // bit  from start of byte
    int inf = 0;

    while (i_bits--) {
        inf <<= 1;
        inf |= (p_buf[byte_offset] & (1 << bit_offset)) >> bit_offset;

        bit_offset--;
        if (bit_offset < 0) {
            byte_offset++;
            bit_offset += 8;

            if (byte_offset > i_buf) {
                return -1;      /* error */
            }
        }
    }
    *p_info = inf;

    return 0;
}

/* ---------------------------------------------------------------------------
* RETURN: the length of symbol, or -1 on error
*/
static INLINE
int get_vlc_symbol(uint8_t *p_buf, int i_bit_pos, int *info, int i_buf)
{
    int byte_offset = i_bit_pos >> 3;         // byte from start of buffer
    int bit_offset  = 7 - (i_bit_pos & 7);    // bit from start of byte
    int bit_counter = 1;
    int len = 1;
    int ctr_bit;         // control bit for current bit position
    int info_bit;
    int inf;

    ctr_bit = (p_buf[byte_offset] & (1 << bit_offset));     // set up control bit
    while (ctr_bit == 0) {
        // find leading 1 bit
        len++;
        bit_offset -= 1;
        bit_counter++;

        if (bit_offset < 0) {
            // finish with current byte ?
            bit_offset = bit_offset + 8;
            byte_offset++;
        }

        ctr_bit = (p_buf[byte_offset] & (1 << bit_offset));  // set up control bit
    }

    // make info-word
    inf = 0;        // shortest possible code is 1, then info is always 0
    for (info_bit = 0; (info_bit < (len - 1)); info_bit++) {
        bit_counter++;
        bit_offset--;

        if (bit_offset < 0) {
            // finished with current byte ?
            bit_offset = bit_offset + 8;
            byte_offset++;
        }

        if (byte_offset > i_buf) {
            return -1;          /* error */
        }

        inf = (inf << 1);
        if (p_buf[byte_offset] & (0x01 << (bit_offset))) {
            inf |= 1;
        }
    }
    *info = inf;

    // return absolute offset in bit from start of frame
    return bit_counter;
}

/* ---------------------------------------------------------------------------
* reads an u(v) syntax element (FLC codeword) from UVLC-partition
* RETURN: the value of the coded syntax element, or -1 on error
*/
static INLINE
int vlc_u_v(davs2_bs_t *bs, int i_bits
#if AVS2_TRACE
            , char *tracestring
#endif
            )
{
    int ret_val = 0;

    if (read_bits(bs->p_stream, bs->i_stream, bs->i_bit_pos, &ret_val, i_bits) == 0) {
        bs->i_bit_pos += i_bits;    /* move bitstream pointer */

#if AVS2_TRACE
        avs2_trace_string(tracestring, ret_val, i_bits);
#endif

        return ret_val;
    }

    return -1;
}

/* ---------------------------------------------------------------------------
* reads an ue(v) syntax element
* RETURN: the value of the coded syntax element, or -1 on error
*/
static INLINE
int vlc_ue_v(davs2_bs_t *bs
#if AVS2_TRACE
             , char *tracestring
#endif
             )
{
    int len, info;
    int ret_val;

    len = get_vlc_symbol(bs->p_stream, bs->i_bit_pos, &info, bs->i_stream);
    if (len == -1) {
        return -1;              /* error */
    }

    bs->i_bit_pos += len;

    // cal:   pow(2, (len / 2)) + info - 1;
    ret_val = (1 << (len >> 1)) + info - 1;

#if AVS2_TRACE
    avs2_trace_string2(tracestring, ret_val + 1, ret_val, len);
#endif

    return ret_val;
}

/* ---------------------------------------------------------------------------
* reads an se(v) syntax element
* RETURN: the value of the coded syntax element, or -1 on error
*/
static INLINE
int vlc_se_v(davs2_bs_t *bs
#if AVS2_TRACE
             , char *tracestring
#endif
             )
{
    int len, info;
    int ret_val;
    int n;

    len = get_vlc_symbol(bs->p_stream, bs->i_bit_pos, &info, bs->i_stream);
    if (len == -1) {
        return -1;              /* error */
    }

    bs->i_bit_pos += len;

    // cal: (int)pow(2, (len / 2)) + info - 1;
    n = (1 << (len >> 1)) + info - 1;
    ret_val = (n + 1) >> 1;
    if ((n & 1) == 0) {         /* lsb is signed bit */
        ret_val = -ret_val;
    }

#if AVS2_TRACE
    avs2_trace_string2(tracestring, n + 1, ret_val, len);
#endif

    return ret_val;
}


#if AVS2_TRACE
#define u_flag(bs, tracestring)         (bool_t)vlc_u_v(bs, 1, tracestring)
#define u_v(bs, i_bits, tracestring)    vlc_u_v(bs, i_bits, tracestring)
#define ue_v(bs, tracestring)           vlc_ue_v(bs, tracestring)
#define se_v(bs, tracestring)           vlc_se_v(bs, tracestring)
#else
#define u_flag(bs, tracestring)         (bool_t)vlc_u_v(bs, 1)
#define u_v(bs, i_bits, tracestring)    vlc_u_v(bs, i_bits)
#define ue_v(bs, tracestring)           vlc_ue_v(bs)
#define se_v(bs, tracestring)           vlc_se_v(bs)
#endif

#ifdef __cplusplus
}
#endif
#endif  // DAVS2_VLC_H
