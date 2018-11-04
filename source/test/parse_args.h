/*
 * parse_args.h
 *
 * Description of this file:
 *    Argument Parsing functions definition of the davs2 library
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

#ifndef DAVS2_GETOPT_H
#define DAVS2_GETOPT_H

#include <stdio.h>
#include <string.h>
#include <getopt.h>
#if _WIN32
#include <io.h>
#include <fcntl.h>
#endif
#include "utils.h"

typedef struct davs2_input_param_t {
    const char *s_infile;
    const char *s_outfile;
    const char *s_recfile;
    const char *s_md5;

    int g_verbose;
    int g_psnr;
    int g_threads;
    int b_y4m;     // Y4M or YUV

    FILE *g_infile;
    FILE *g_recfile;
    FILE *g_outfile;
} davs2_input_param_t;

#if defined(__ICL) || defined(_MSC_VER)
#define strcasecmp              _stricmp
#endif

/* 包含附加参数的，在字母后面需要加上冒号 */
static const char *optString = "i:o:r:m:t:vh?";

static const struct option longOpts[] = {
    {"input",   required_argument, NULL, 'i'},
    {"output",  required_argument, NULL, 'o'},
    {"psnr",    required_argument, NULL, 'r'},
    {"md5",     required_argument, NULL, 'm'},
    {"threads", required_argument, NULL, 't'},
    {"verbose", no_argument, NULL, 'v'},
    {"help",    no_argument, NULL, 'h'},
    {NULL, no_argument, NULL, 0}
};


/* ---------------------------------------------------------------------------
 */
static void display_usage(void)
{
    /* 运行参数说明 */
    const char * usage = "usage: davs2.exe --input avsfile --output=outputfile [--psnr=recfile] [--threads=threads] [--verbose]";

    show_message(CONSOLE_RED, "davs2.exe 运行参数说明：\n %s\n", usage);
    show_message(CONSOLE_RED, " --input=test.avs 或 -i test.avs          设置输入文件路径\n");
    show_message(CONSOLE_RED, " --output=test_dec.yuv 或 -o test_dec.yuv 设置输出路径\n");
    show_message(CONSOLE_RED, " --psnr=test_rec.yuv 或 -r test_rec,yuv   设置编码参考文件，用于计算是否匹配\n");
    show_message(CONSOLE_RED, " --md5=md5 或 -m md5                      设置编码参考MD5，用于计算是否匹配\n");
    show_message(CONSOLE_RED, " --threads=N 或 -t N                      设置解码线程数，默认1\n");
    show_message(CONSOLE_RED, " --verbose 或 -v                          设置显示每帧解码数据\n");
    show_message(CONSOLE_RED, " --help 或 -h                             显示此提示信息\n");
    show_message(CONSOLE_RED, "------------------------------------------------------------\n");
}


/* ---------------------------------------------------------------------------
 */
static int parse_args(davs2_input_param_t *p_param, int argc, char **argv)
{
    char title[1024] = {0};
    int i;
    int opt = 0;
    int longIndex = 0;
    for (i = 0; i < argc; ++i) {
        sprintf(&title[strlen(title)], "%s ", argv[i]);
    }
    show_message(CONSOLE_WHITE, "%s\n\n", title);

    if (argc < 2) {
        display_usage();
        return -1;
    }

    /* Initialize globalArgs before we get to work. */
    p_param->s_infile  = NULL;
    p_param->s_outfile = NULL;
    p_param->s_recfile = NULL;
    p_param->s_md5     = NULL;
    p_param->g_infile  = NULL;
    p_param->g_outfile = NULL;
    p_param->g_recfile = NULL;
    p_param->g_verbose = 0;
    p_param->g_psnr    = 0;
    p_param->g_threads = 1;
    p_param->b_y4m     = 0;

    opt = getopt_long(argc, argv, optString, longOpts, &longIndex);
    while (opt != -1) {
        switch (opt) {
        case 'i':
            p_param->s_infile = optarg;
            break;
        case 'o':
            p_param->s_outfile = optarg;
            break;
        case 'r':
            p_param->s_recfile = optarg;
            break;
        case 'm':
            p_param->s_md5 = optarg;
            break;
        case 'v':
            p_param->g_verbose = 1;
            break;
        case 't':
            p_param->g_threads = atoi(optarg);
            break;
        case 'h':   /* fall-through is intentional */
        case '?':
            display_usage();
            break;
        case 0:     /* long option without a short arg */
            break;
        default:
            /* You won't actually get here. */
            break;
        }

        opt = getopt_long(argc, argv, optString, longOpts, &longIndex);
    }

    if (p_param->s_infile == NULL) {
        display_usage();
        show_message(CONSOLE_RED, "missing input file.\n");
        return -1;
    }

    p_param->g_infile  = fopen(p_param->s_infile, "rb");

    if (p_param->s_recfile != NULL) {
        p_param->g_recfile = fopen(p_param->s_recfile, "rb");
    }

    if (p_param->s_outfile != NULL) {
        if (!strcmp(p_param->s_outfile, "stdout")) {
            p_param->g_outfile = stdout;
        } else {
            p_param->g_outfile = fopen(p_param->s_outfile, "wb");
        }
    } else if (p_param->g_outfile == NULL) {
        display_usage();
        show_message(CONSOLE_RED, "WARN: missing output file.\n");
    }

    /* open stream file */
    if (p_param->g_infile == NULL) {
        show_message(CONSOLE_RED, "ERROR: failed to open input file: %s\n", p_param->s_infile);
        return -1;
    }

    /* open rec file */
    if (p_param->s_recfile != NULL && p_param->g_recfile == NULL) {
        show_message(CONSOLE_RED, "ERROR: failed to open reference file: %s\n", p_param->s_recfile);
    }
    p_param->g_psnr = (p_param->g_recfile != NULL);

    /* open output file */
    if (p_param->s_outfile != NULL && p_param->g_outfile == NULL) {
        show_message(CONSOLE_RED, "ERROR: failed to open output file: %s\n", p_param->s_outfile);
    } else {
        int l = strlen(p_param->s_outfile);
        if (l > 4) {
            if (!strcmp(p_param->s_outfile + l - 4, ".y4m")) {
                p_param->b_y4m = 1;
            }
        }
        if (p_param->g_outfile == stdout) {
#if _WIN32
            setmode(fileno(stdout), O_BINARY);
#endif
            p_param->b_y4m = 1;
        }
    }

    /* get md5 */
    if (p_param->s_md5 && strlen(p_param->s_md5) != 32) {
        show_message(CONSOLE_RED, "ERROR: invalid md5 value");
    }

    show_message(CONSOLE_WHITE, "--------------------------------------------------\n");
    show_message(CONSOLE_WHITE, " AVS2 file       : %s\n", p_param->s_infile);
    show_message(CONSOLE_WHITE, " Reference file  : %s\n", p_param->s_recfile);
    show_message(CONSOLE_WHITE, " Output file     : %s\n", p_param->s_outfile);
    show_message(CONSOLE_WHITE, "--------------------------------------------------\n");

    return 0;
}

#endif /// DAVS2_GETOPT_H
