/*
 * inputstream.h
 *
 * Description of this file:
 *    Inputstream Processing functions definition of the davs2 library
 *
 * --------------------------------------------------------------------------
 *
 *    davs2 - video decoder of AVS2/IEEE1857.4 video coding standard
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

#ifndef DAVS2_CHECKFRAME_H
#define DAVS2_CHECKFRAME_H

#include "utils.h"

#include <stdio.h>
#include <memory.h>
#include <math.h>
#include <time.h>
#include <assert.h>

/* ---------------------------------------------------------------------------
 */
#define ISPIC(x)  ((x) == 0xB3 || (x) == 0xB6)
#define ISUNIT(x) ((x) == 0xB0 || (x) == 0xB1 || (x) == 0xB7 || ISPIC(x))

/* ---------------------------------------------------------------------------
 */
static __inline 
const uint8_t *
find_start_code(const uint8_t *data, int len) 
{
    while (len >= 4 && (*(int *)data & 0x00FFFFFF) != 0x00010000) {
        ++data;
        --len;
    }

    return len >= 4 ? data : 0;
}

/* ---------------------------------------------------------------------------
 */
static int
check_frame(const uint8_t *data, int len)
{
    const uint8_t *p;
    const uint8_t *data0 = data;
    const int      len0  = len;

    while (((p = (uint8_t *)find_start_code(data, len)) != 0) && !ISUNIT(p[3])) {
        len -= (int)(p - data + 4);
        data = p + 4;
    }

    return (int)(p ? p - data0 : len0 + 1);
}

/* ---------------------------------------------------------------------------
 */
static int
find_one_frame(uint8_t * data, int len, int *start, int *end)
{
    if ((*start = check_frame(data, len)) > len) {
        return -1;
    }

    if ((*end = check_frame(data + *start + 4, len - *start - 4)) <= len) {
        *end += *start + 4;
    }

    return 0;
}

/* ---------------------------------------------------------------------------
 */
static int
count_frames(uint8_t *data, int size)
{
    int count = 0;
    int start, end;

    for (;;) {
        if (find_one_frame(data, size, &start, &end) < 0) {
            break;
        }

        if (ISPIC(data[start + 3])) {
            count++;
        }

        data += end;
        size -= end;
    }

    return count;
}


/* ---------------------------------------------------------------------------
*/
static int 
read_input_file(davs2_input_param_t *p_param, uint8_t **data, int *size, int *frames, float errrate)
{
    /* get size of input file */
    fseek(p_param->g_infile, 0, SEEK_END);
    *size = ftell(p_param->g_infile);
    fseek(p_param->g_infile, 0, SEEK_SET);

    /* memory for stream buffer */
    if ((*data = (uint8_t *)calloc(*size + 1024, sizeof(uint8_t))) == NULL) {
        show_message(CONSOLE_RED, "failed to alloc memory for input file.\n");
        return -1;
    }

    /* read stream data */
    if (fread(*data, *size, 1, p_param->g_infile) < 1) {
        show_message(CONSOLE_RED, "failed to read input file.\n");
        free(*data);
        *data = NULL;
        return -1;
    }

    if (errrate != 0) {
        show_message(CONSOLE_WHITE, "noise interfering is enabled:\n");
    }

    /* get total frames */
    *frames = count_frames(*data, *size);

    return 0;
}

#endif /// DAVS2_CHECKFRAME_H
