/*
 * test.c
 *
 * Description of this file:
 *    test the AVS2 Video Decoder —— davs2 library
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

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#define _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "davs2.h"
#include "utils.h"
#include "psnr.h"
#include "parse_args.h"
#include "inputstream.h"
#include "md5.h"

#if defined(_MSC_VER)
#pragma comment(lib, "libdavs2.lib")
#endif

#if defined(__cplusplus)
extern "C" {
#endif  /* __cplusplus */

/**
 * ===========================================================================
 * macro defines
 * ===========================================================================
 */
#define CTRL_LOOP_DEC_FILE    0   /* 循环解码一个ES文件 */

/* ---------------------------------------------------------------------------
 * disable warning C4100: : unreferenced formal parameter
 */
#ifndef UNREFERENCED_PARAMETER
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
#define UNREFERENCED_PARAMETER(v) (v)
#else
#define UNREFERENCED_PARAMETER(v) (void)(v)
#endif
#endif


/**
 * ===========================================================================
 * global variables
 * ===========================================================================
 */
int g_frmcount = 0;
int g_psnrfail = 0;
unsigned int   MD5val[4];
char           MD5str[33];

davs2_input_param_t inputparam = {
    NULL, NULL, NULL, NULL, 0, 0, 0, 0
};


/**
 * ===========================================================================
 * function defines
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 */
static 
void output_decoded_frame(davs2_picture_t *pic, davs2_seq_info_t *headerset, int ret_type, int num_frames)
{
    static char IMGTYPE[] = {'I', 'P', 'B', 'G', 'F', 'S', '\x0'};
    double psnr_y = 0.0f, psnr_u = 0.0f, psnr_v = 0.0f;

    if (headerset == NULL) {
        return;
    }

    if (pic == NULL || ret_type == DAVS2_GOT_HEADER) {
        show_message(CONSOLE_GREEN,
            "  Sequence size: %dx%d, %d/%d-bit %.3lf Hz. ProfileLevel: 0x%x-0x%x\n\n", 
            headerset->width, headerset->height, 
            headerset->internal_bit_depth, headerset->output_bit_depth,
            headerset->frame_rate,
            headerset->profile_id, headerset->level_id);

        if (inputparam.b_y4m) {
            static const int FRAME_RATE[9][2] = {
                { 0, 1},  // invalid
                { 24000, 1001 },
                { 24, 1 },
                { 25, 1 },
                { 30000, 1001 },
                { 30, 1 }, 
                { 50, 1 },
                { 60000, 1001 },
                { 60, 1 }
            };
            int fps_num = FRAME_RATE[headerset->frame_rate_id][0];
            int fps_den = FRAME_RATE[headerset->frame_rate_id][1];
            write_y4m_header(inputparam.g_outfile, headerset->width, headerset->height,
                             fps_num, fps_den, headerset->output_bit_depth);
        }
        return;
    }

    if (inputparam.g_psnr) {
        int ret = cal_psnr(pic->pic_order_count, pic->planes, pic->strides, inputparam.g_recfile,
                           pic->widths[0], pic->lines[0], pic->num_planes,
                           &psnr_y, &psnr_u, &psnr_v, 
                           pic->bytes_per_sample, pic->bit_depth);
        int psnr = (psnr_y != 0 || psnr_u != 0 || psnr_v != 0);

        if (ret < 0) {
            g_psnrfail = 1;
            show_message(CONSOLE_RED, "failed to cal psnr for frame %d(%d).\t\t\t\t\n", g_frmcount, pic->pic_order_count);
        } else {
            if (inputparam.g_verbose || psnr) {
                show_message(psnr ? CONSOLE_RED : CONSOLE_WHITE,
                    "%5d(%d)\t(%c) %3d\t%8.4lf %8.4lf %8.4lf \t%6lld %6lld\n", 
                    g_frmcount, pic->pic_order_count,
                    IMGTYPE[pic->type], pic->qp, psnr_y, psnr_u, psnr_v,
                    pic->pts, pic->dts);
            }
        }
    } else if (inputparam.g_verbose) {
        show_message(CONSOLE_WHITE,
            "%5d(%d)\t(%c)\t%3d\n", g_frmcount, pic->pic_order_count, IMGTYPE[pic->type], pic->qp);
    }

    g_frmcount++;

    if (inputparam.g_verbose == 0) {
        show_progress(g_frmcount, num_frames);
    }

    if (inputparam.g_outfile) {
        write_frame(pic, inputparam.g_outfile, inputparam.b_y4m);
    }
}

/* ---------------------------------------------------------------------------
 * data_buf - pointer to bitstream buffer
 * data_len - number of bytes in bitstream buffer
 * frames   - number of frames in bitstream buffer
 */
void test_decoder(uint8_t *data_buf, int data_len, int num_frames, char *dst)
{
    const double f_time_fac = 1.0 / (double)CLOCKS_PER_SEC;
    davs2_param_t    param;      // decoding parameters
    davs2_packet_t   packet;     // input bitstream
    davs2_picture_t  out_frame;  // output data, frame data
    davs2_seq_info_t headerset;  // output data, sequence header
    int got_frame;

#if CTRL_LOOP_DEC_FILE
    uint8_t *bak_data_buf = data_buf;
    int      bak_data_len = data_len;
    int      num_loop     = 5;      // 循环解码次数
#endif
    int64_t time0, time1;
    void *decoder;

    const uint8_t *data = data_buf;
    const uint8_t *data_next_start_code;
    int user_dts = 0; // only used to check the returning value of DTS and PTS

    /* init the decoder */
    memset(&param, 0, sizeof(param));
    param.threads      = inputparam.g_threads;
    param.opaque       = (void *)(intptr_t)num_frames;
    param.info_level   = DAVS2_LOG_DEBUG;
    param.disable_avx  = 0; // on some platforms, disable AVX (setting to 1) would be faster

    decoder = davs2_decoder_open(&param);

    time0 = get_time();

    /* do decoding */
    for (;;) {
        int len;

        data_next_start_code = find_start_code(data + 4, data_len - 4);

        if (data_next_start_code) {
            len = data_next_start_code - data;
        } else {
            len = data_len;
        }

        packet.data = data;
        packet.len  = len;

        // set PTS/DTS, which was only used to check whether they could be passed out rightly
        packet.pts  =  user_dts;
        packet.dts  = -user_dts;
        user_dts++;

        got_frame = davs2_decoder_send_packet(decoder, &packet);
        if (got_frame == DAVS2_ERROR) {
            show_message(CONSOLE_RED, "Error: An decoder error counted\n");
            break;
        }

        got_frame = davs2_decoder_recv_frame(decoder, &headerset, &out_frame);
        if (got_frame != DAVS2_DEFAULT) {
            output_decoded_frame(&out_frame, &headerset, got_frame, num_frames);
            davs2_decoder_frame_unref(decoder, &out_frame);
        }

        data_len -= len;
        data += len; // could not be [data = data_next_start_code]

        if (!data_len) {
#if CTRL_LOOP_DEC_FILE
            data_len = bak_data_len;
            data = data_buf;
            num_loop--;
            if (num_loop <= 0) {
                break;
            }
#else
            break;              /* end of bitstream */
#endif
        }
    }

    /* flush the decoder */
    for (;;) {
        got_frame = davs2_decoder_flush(decoder, &headerset, &out_frame);
        if (got_frame == DAVS2_ERROR || got_frame == DAVS2_END) {
            break;
        }
        if (got_frame != DAVS2_DEFAULT) {
            output_decoded_frame(&out_frame, &headerset, got_frame, num_frames);
            davs2_decoder_frame_unref(decoder, &out_frame);
        }
    }

    time1 = get_time();

    /* close the decoder */
    davs2_decoder_close(decoder);

    /* statistics */
    show_message(CONSOLE_WHITE, "\n--------------------------------------------------\n");

    show_message(CONSOLE_GREEN, "total frames: %d/%d\n", g_frmcount, num_frames);
    if (inputparam.g_psnr) {
        if (g_psnrfail == 0 && g_frmcount != 0) {
            show_message(CONSOLE_GREEN,
                         "average PSNR:\t%8.4f, %8.4f, %8.4f\n\n", 
                         g_sum_psnr_y / g_frmcount, g_sum_psnr_u / g_frmcount, g_sum_psnr_v / g_frmcount);

            sprintf(dst, "  Frames: %d/%d\n  TIME : %.3lfs, %6.2lf fps\n  PSNR : %8.4f, %8.4f, %8.4f\n",
                    g_frmcount, num_frames,
                    (double)((time1 - time0) * f_time_fac),
                    (double)(g_frmcount / ((time1 - time0) * f_time_fac)),
                    g_sum_psnr_y / g_frmcount, g_sum_psnr_u / g_frmcount, g_sum_psnr_v / g_frmcount);
        } else {
            show_message(CONSOLE_RED, "average PSNR:\tNaN, \tNaN, \tNaN\n\n"); /* 'NaN' for 'Not a Number' */
        }
    }

    show_message(CONSOLE_GREEN, "total decoding time: %.3lfs, %6.2lf fps\n", 
        (double)((time1 - time0) * f_time_fac), 
        (double)(g_frmcount / ((time1 - time0) * f_time_fac)));
}

/* ---------------------------------------------------------------------------
 */
int main(int argc, char *argv[])
{
    char dst[1024] = "> no decode data\n";
    uint8_t *data = NULL;
    clock_t tm_start = clock();
    int size;
    int frames;
    long long filelength;

    memset(MD5val, 0, 16);
    memset(MD5str, 0, 33);

    /* parse params */
    if (parse_args(&inputparam, argc, argv) < 0) {
        sprintf(dst, "Failed to parse input parameters\n");
        goto fail;
    }

    /* read input data */
    if (read_input_file(&inputparam, &data, &size, &frames, 0.0f) < 0) {
        sprintf(dst, "Failed to read input bit-stream or create output file\n");
        goto fail;
    }

    /* test decoding */
    test_decoder(data, size, frames, dst);

    show_message(CONSOLE_WHITE, "\n Decoder Total Time: %.3lf s\n", (clock() - tm_start) / (double)(CLOCKS_PER_SEC));

fail:
    /* tidy up */
    if (data) {
        free(data);
    }

    if (g_recbuf) {
        free(g_recbuf);
    }

    if (inputparam.g_infile) {
        fclose(inputparam.g_infile);
    }

    if (inputparam.g_recfile) {
        fclose(inputparam.g_recfile);
    }

    if (inputparam.g_outfile) {
        fclose(inputparam.g_outfile);
    }

    /* calculate MD5 */
    if (inputparam.s_md5 && strlen(inputparam.s_md5) == 32) {
        filelength = FileMD5(inputparam.s_outfile, MD5val);
        sprintf (MD5str,"%08X%08X%08X%08X", MD5val[0], MD5val[1], MD5val[2], MD5val[3]);
        if (strcmp(MD5str,inputparam.s_md5)) {
            show_message(CONSOLE_RED, "\n  MD5 match failed\n");
            show_message(CONSOLE_WHITE, "  Input  MD5 : %s \n", inputparam.s_md5);
            show_message(CONSOLE_WHITE, "  Output MD5 : %s \n", MD5str);
        } else {
            show_message(CONSOLE_WHITE, "\n  MD5 match success \n");
        }
    }

    show_message(CONSOLE_WHITE, " Decoder Exit, Time: %.3lf s\n", (clock() - tm_start) / (double)(CLOCKS_PER_SEC));
    return 0;
}

#if defined(__cplusplus)
}
#endif  /* __cplusplus */
