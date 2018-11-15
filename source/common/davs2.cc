/*
 * davs2.cc
 *
 * Description of this file:
 *    API functions definition of the davs2 library
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

#include "common.h"
#include "davs2.h"
#include "primitives.h"
#include "decoder.h"
#include "bitstream.h"
#include "header.h"
#include "version.h"
#include "decoder.h"
#include "frame.h"
#include "cpu.h"
#include "threadpool.h"
#include "version.h"

/**
 * ===========================================================================
 * macro defines
 * ===========================================================================
 */

#if DAVS2_TRACE_API
FILE *fp_trace_bs = NULL;
FILE *fp_trace_in = NULL;
#endif

/**
 * ===========================================================================
 * function defines
 * ===========================================================================
 */

/* --------------------------------------------------------------------------
 */
static es_unit_t *
es_unit_alloc(int buf_size)
{
    es_unit_t *es_unit = NULL;
    int bufsize = sizeof(es_unit_t) + buf_size;
    
    bufsize = ((bufsize + 31) >> 5 ) << 5;
    es_unit = (es_unit_t *)davs2_malloc(bufsize);

    if (es_unit == NULL) {
        davs2_log(NULL, DAVS2_LOG_ERROR, "failed to malloc memory in es_unit_alloc.\n");
        return NULL;
    }

    es_unit->size = buf_size;
    es_unit->len  = 0;
    es_unit->pts  = 0;
    es_unit->dts  = 0;

    return es_unit;
}

/* --------------------------------------------------------------------------
 */
static void
es_unit_free(es_unit_t *es_unit)
{
    if (es_unit) {
        davs2_free(es_unit);
    }
}

/* ---------------------------------------------------------------------------
 * push byte stream data of one frame to input list
 */
static
es_unit_t *davs2_pack_es_unit(davs2_mgr_t *mgr, const uint8_t *data, int len, int64_t pts, int64_t dts)
{
#define DAVS2_ISUNIT(x) ((x) == 0xB0 || (x) == 0xB1 || (x) == 0xB7 || (x) == 0xB3 || (x) == 0xB6)
    es_unit_t *es_unit     = NULL;
    es_unit_t *ret_es_unit = NULL;
    int start_code = data[3];

    if (mgr->es_unit == NULL) {
        mgr->es_unit = (es_unit_t *)xl_remove_head(&mgr->packets_idle, 1);
    }

    es_unit = mgr->es_unit;

    if (len > 0) {
        if (es_unit->size < es_unit->len + len) {
            /* reallocate frame buffer */
            int new_size = es_unit->len + len + MAX_ES_FRAME_SIZE * 2;
            es_unit_t *new_es_unit;

            if ((new_es_unit = es_unit_alloc(new_size)) == NULL) {
                return NULL;
            }

            memcpy(new_es_unit, es_unit, sizeof(es_unit_t));   /* copy ES Unit information */
            memcpy(new_es_unit->data, es_unit->data, es_unit->len * sizeof(uint8_t));

            es_unit_free(es_unit);

            mgr->es_unit = es_unit = new_es_unit;
        }

        /* copy stream data */
        if (DAVS2_ISUNIT(start_code) && es_unit->len > 0) {
            ret_es_unit = es_unit;
            /* fetch a node again from idle list */
            es_unit = (es_unit_t *)xl_remove_head(&mgr->packets_idle, 1);
            mgr->es_unit = es_unit;
        }
        memcpy(es_unit->data + es_unit->len, data, len * sizeof(uint8_t));
        es_unit->len += len;
        es_unit->pts  = pts;
        es_unit->dts  = dts;
    }

    /* check the pseudo start code */
    if (ret_es_unit != NULL) {
        ret_es_unit->len = bs_dispose_pseudo_code(ret_es_unit->data, ret_es_unit->data, ret_es_unit->len);
    }

#undef DAVS2_ISUNIT
    return ret_es_unit;
}

/* ---------------------------------------------------------------------------
 */
static void 
destroy_all_lists(davs2_mgr_t *mgr)
{
    es_unit_t *es_unit = NULL;
    davs2_picture_t *pic = NULL;

    /* idle list */
    for (;;) {
        if ((es_unit = (es_unit_t *)xl_remove_head_ex(&mgr->packets_idle)) == NULL) {
            break;
        }

        es_unit_free(es_unit);
    }

    /* recycle list */
    for (;;) {
        if ((pic = (davs2_picture_t *)xl_remove_head_ex(&mgr->pic_recycle)) == NULL) {
            break;
        }

        davs2_free(pic);
    }

    if (mgr->es_unit) {
        es_unit_free(mgr->es_unit);
        mgr->es_unit = NULL;
    }

    xl_destroy(&mgr->packets_idle);
    xl_destroy(&mgr->pic_recycle);
}

/* ---------------------------------------------------------------------------
 */
static int
create_all_lists(davs2_mgr_t *mgr)
{
    es_unit_t *es_unit = NULL;
    int i;

    if (xl_init(&mgr->packets_idle ) != 0 || 
        xl_init(&mgr->pic_recycle  ) != 0) {
        goto fail;
    }

    for (i = 0; i < MAX_ES_FRAME_NUM + mgr->param.threads; i++) {
        es_unit = es_unit_alloc(MAX_ES_FRAME_SIZE);

        if (es_unit) {
            xl_append(&mgr->packets_idle, es_unit);
        } else {
            goto fail;
        }
    }

    return 0;

fail:
    destroy_all_lists(mgr);

    return -1;
}

/* ---------------------------------------------------------------------------
 */
static
void output_list_recycle_picture(davs2_mgr_t *mgr, davs2_outpic_t *pic)
{
    pic->frame = NULL;
    /* picture may be obsolete(for new sequence with different resolution), we will release it later */
    xl_append(&mgr->pic_recycle, pic);
}

/* ---------------------------------------------------------------------------
 */
static 
int has_new_output_frame(davs2_mgr_t *mgr, davs2_t *h)
{
    // TODO: 待完善，确定当前图像解码完毕后是否应该等待输出
    UNUSED_PARAMETER(mgr);
    UNUSED_PARAMETER(h);

    return 1;  // 有图像输出返回非零，无图像输出返回0
}

/* ---------------------------------------------------------------------------
 */
static
davs2_outpic_t *output_list_get_one_output_picture(davs2_mgr_t *mgr)
{
    davs2_outpic_t *pic   = NULL;

    davs2_thread_mutex_lock(&mgr->mutex_mgr);

    while (mgr->outpics.pics) {
        davs2_frame_t *frame = mgr->outpics.pics->frame;
        assert(frame);

        if (frame->i_poc == mgr->outpics.output) {
            /* the next frame : output */
            pic = mgr->outpics.pics;
            mgr->outpics.pics = pic->next;

            /* move on to the next frame */
            mgr->outpics.output++;
            mgr->outpics.num_output_pic--;
            break;
        } else {
            /* TODO: 这里仍需要确认一下修改方式 
             * 如何保证输出顺序的有效性？需要在输出队列由多少帧时进行输出。
             */
            if (frame->i_poc > mgr->outpics.output) {
                /* the end of the stream occurs */
                if (mgr->b_flushing &&
                    mgr->num_frames_in == mgr->num_frames_out + mgr->outpics.num_output_pic) {
                    mgr->outpics.output++;
                    continue;
                }

                /* a future frame */
                int num_delayed_frames = 1;

                pic = mgr->outpics.pics;
                while (pic->next != NULL) {
                    num_delayed_frames++;
                    pic = pic->next;
                }

                if (num_delayed_frames < 8) {
                    /* keep waiting */
                    davs2_thread_mutex_unlock(&mgr->mutex_mgr);
                    davs2_sleep_ms(1);
                    davs2_thread_mutex_lock(&mgr->mutex_mgr);
                    continue;
                }
            }

            /* 目前输出队列的最小POC与已输出的POC之间间隔较大，将输出POC提前到当前最小POC */
            davs2_log(mgr, DAVS2_LOG_WARNING, "Advance to discontinuous POC: %d\n", frame->i_poc);
            mgr->outpics.output = frame->i_poc;
        }
    }

    mgr->outpics.busy = (pic != NULL);

    davs2_thread_mutex_unlock(&mgr->mutex_mgr);

    return pic;
}

/* --------------------------------------------------------------------------
 * Thread of decoder output (decoded raw data)
 */
int decoder_get_output(davs2_mgr_t *mgr, davs2_seq_info_t *headerset, davs2_picture_t *out_frame, int is_flush)
{
    davs2_outpic_t *pic   = NULL;
    int b_wait_new_frame = mgr->num_frames_in + mgr->num_decoders - mgr->num_frames_out > 8 + mgr->num_aec_thread;

    while (mgr->num_frames_in > mgr->num_frames_out && /* no more output */
           (b_wait_new_frame || is_flush)) {
        if (mgr->new_sps) {
            memcpy(headerset, &mgr->seq_info.head, sizeof(davs2_seq_info_t));
            mgr->new_sps = FALSE; /* set flag */
            out_frame->magic = NULL;
            return DAVS2_GOT_HEADER;
        }

        /* check for the next frame */
        pic = output_list_get_one_output_picture(mgr);

        if (pic == NULL) {
            davs2_sleep_ms(1);
        } else {
            break;
        }
    }

    if (pic == NULL) {
        if (mgr->new_sps) {
            memcpy(headerset, &mgr->seq_info.head, sizeof(davs2_seq_info_t));
            mgr->new_sps = FALSE; /* set flag */
            out_frame->magic = NULL;
            return DAVS2_GOT_HEADER;
        }
        return DAVS2_DEFAULT;
    }

    mgr->num_frames_out++;

    /* copy out */
    davs2_write_a_frame(pic->pic, pic->frame);

    /* release reference when it would no more be needed */
    if (pic->pic->dec_frame == NULL) {
        release_one_frame(pic->frame);
    }

    /* deliver this frame */
    memcpy(out_frame, pic->pic, sizeof(davs2_picture_t));
    out_frame->magic       = pic;
    return DAVS2_GOT_FRAME;
}

/**
 * ---------------------------------------------------------------------------
 * Function   : release one output frame
 * Parameters :
 *       [in] : decoder   - decoder handle
 *            : out_frame - frame to recycle
 * Return     : none
 * ---------------------------------------------------------------------------
 */
DAVS2_API void
davs2_decoder_frame_unref(void *decoder, davs2_picture_t *out_frame)
{
    davs2_mgr_t *mgr = (davs2_mgr_t *)decoder;
    if (mgr == NULL || out_frame == NULL) {
        return;
    }

    /* release the output */
    if (out_frame->magic != NULL) {
        davs2_outpic_t *pic = (davs2_outpic_t *)out_frame->magic;

        /* release reference when it would no more be needed */
        if (pic->pic->dec_frame != NULL) {
            release_one_frame(pic->frame);   // pic->pic->dec_frame == pic->frame
            pic->pic->dec_frame = NULL;
        }

        output_list_recycle_picture(mgr, pic);
    }
}

/* --------------------------------------------------------------------------
 */
static davs2_t *task_get_free_task(davs2_mgr_t *mgr)
{
    int i;

    for (; mgr->b_exit == 0;) {
        for (i = 0; i < mgr->num_decoders; i++) {
            davs2_t *h = &mgr->decoders[i];
            davs2_thread_mutex_lock(&mgr->mutex_mgr);
            if (h->task_info.task_status == TASK_FREE) {
                h->task_info.task_status = TASK_BUSY;
                davs2_thread_mutex_unlock(&mgr->mutex_mgr);
                return h;
            }
            davs2_thread_mutex_unlock(&mgr->mutex_mgr);
        }
    }

    return NULL;
}

/* --------------------------------------------------------------------------
 */
void task_unload_packet(davs2_t *h, es_unit_t *es_unit)
{
    davs2_mgr_t *mgr = h->task_info.taskmgr;

    if (es_unit) {
        /* packet is free */
        es_unit->len = 0;
        xl_append(&mgr->packets_idle, es_unit);
    }

    davs2_thread_mutex_lock(&mgr->mutex_mgr);
    h->task_info.task_status = TASK_FREE;
    davs2_thread_mutex_unlock(&mgr->mutex_mgr);
}


/**
 * ===========================================================================
 * interface function defines
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 */
DAVS2_API void *
davs2_decoder_open(davs2_param_t *param)
{
    const int max_num_thread = CTRL_AEC_THREAD ? AVS2_THREAD_MAX : AVS2_THREAD_MAX / 2;
    char buf_cpu[120] = "";
    davs2_mgr_t *mgr = NULL;
    uint8_t *mem_ptr;
    size_t mem_size;
    uint32_t cpuid = 0;
    int i;

    /* output version information */
    if (param->info_level <= DAVS2_LOG_INFO) {
        davs2_log(NULL, DAVS2_LOG_INFO, "davs2: %s.%d, %s",
                  XVERSION_STR, BIT_DEPTH, XBUILD_TIME);
    }

#if DAVS2_TRACE_API
    fp_trace_bs = fopen("trace_bitstream.avs", "wb");
    fp_trace_in = fopen("trace_input.txt", "w");
#endif

    /* check parameters */
    if (param == NULL) {
        davs2_log(NULL, DAVS2_LOG_ERROR, "Invalid input parameters: Null parameters\n");
        return 0;
    }

    /* init all function handlers */
#if HAVE_MMX
    cpuid = davs2_cpu_detect();
    if (param->disable_avx) {
         cpuid &= ~(DAVS2_CPU_AVX | DAVS2_CPU_AVX2);
    }
#endif
    init_all_primitives(cpuid);

    /* CPU capacities */
    davs2_get_simd_capabilities(buf_cpu, cpuid);
    if (param->info_level <= DAVS2_LOG_INFO) {
        davs2_log(mgr, DAVS2_LOG_INFO, "CPU Capabilities: %s", buf_cpu);
    }

    mem_size = sizeof(davs2_mgr_t) + CACHE_LINE_SIZE
        + AVS2_THREAD_MAX * (sizeof(davs2_t) + CACHE_LINE_SIZE);
    CHECKED_MALLOCZERO(mem_ptr, uint8_t *, mem_size);

    mgr = (davs2_mgr_t *)mem_ptr;
    mem_ptr += sizeof(davs2_mgr_t);
    ALIGN_POINTER(mem_ptr);
    memcpy(&mgr->param, param, sizeof(davs2_param_t));

    /* init log module */
    mgr->module_log.i_log_level = param->info_level;
    sprintf(mgr->module_log.module_name, "Manager %06llx", (intptr_t)(mgr));

    if (mgr->param.threads <= 0) {
        mgr->param.threads = davs2_cpu_num_processors();
    }
    if (mgr->param.threads > max_num_thread) {
        mgr->param.threads = max_num_thread;
        davs2_log(mgr, DAVS2_LOG_WARNING, "Max number of thread reached, forcing to be %d\n", max_num_thread);
    }

    /* init members that could not be zero */
    mgr->i_prev_coi       = -1;

    /* output pictures */
    mgr->outpics.output   = -1;
    mgr->outpics.pics     = NULL;
    mgr->outpics.num_output_pic = 0;

    mgr->num_decoders     = mgr->param.threads;
    mgr->num_total_thread = mgr->param.threads;
    mgr->num_aec_thread   = mgr->param.threads;
#if CTRL_AEC_THREAD
    if (mgr->num_total_thread > 3) {
        mgr->num_aec_thread = (mgr->param.threads >> 1) + 1;
        mgr->num_rec_thread = mgr->num_total_thread - mgr->num_aec_thread;
    } else {
        mgr->num_rec_thread = 0;
    }
    mgr->num_decoders += 1 + mgr->num_aec_thread;
#else
    mgr->num_rec_thread = 0;
#endif

    mgr->num_decoders++;

    mgr->decoders = (davs2_t *)mem_ptr;
    mem_ptr      += AVS2_THREAD_MAX * sizeof(davs2_t);
    ALIGN_POINTER(mem_ptr);
    davs2_thread_mutex_init(&mgr->mutex_mgr, NULL);
    davs2_thread_mutex_init(&mgr->mutex_aec, NULL);

    /* init input&output lists */
    if (create_all_lists(mgr) < 0) {
        goto fail;
    }

    /* 线程数量配置参数 */
    if (mgr->num_total_thread < 1 || mgr->num_decoders < mgr->num_aec_thread ||
        mgr->num_rec_thread < 0 ||
        mgr->num_aec_thread < 1 || mgr->num_aec_thread > mgr->num_total_thread) {
        davs2_log(mgr, DAVS2_LOG_ERROR,
                  "Invalid thread number configuration: num_task[%d], num_threads[%d], num_aec_thread[%d], num_pool[%d]\n",
                  mgr->num_decoders, mgr->num_total_thread, mgr->num_aec_thread, mgr->num_rec_thread);
        goto fail;
    }

    /* spawn the output thread */
    mgr->num_frames_in  = 0;
    mgr->num_frames_out = 0;

    /* init all the tasks */
    for (i = 0; i < mgr->num_decoders; i++) {
        davs2_t *h = &mgr->decoders[i];

        /* init the decode context */
        decoder_open(mgr, h, i);
        // davs2_log(h, DAVS2_LOG_WARNING, "Decoder [%2d]: %p", i, h);

        h->task_info.task_id     = i;
        h->task_info.task_status = TASK_FREE;
        h->task_info.taskmgr     = mgr;
    }

    /* initialize thread pool for AEC decoding and reconstruction */
    davs2_threadpool_init((davs2_threadpool_t **)&mgr->thread_pool, mgr->num_total_thread, NULL, NULL, 0);

    davs2_log(mgr, DAVS2_LOG_INFO, "using %d thread(s): %d(frame/AEC)+%d(pool/REC), %d tasks", 
        mgr->num_total_thread, mgr->num_aec_thread, mgr->num_rec_thread, mgr->num_decoders);

    return mgr;

fail:
    davs2_log(NULL, DAVS2_LOG_ERROR, "failed to open decoder\n");
    davs2_decoder_close(mgr);

    return NULL;
}

/* ---------------------------------------------------------------------------
 */
int decoder_decode_es_unit(davs2_mgr_t *mgr, es_unit_t *es_unit)
{
    davs2_t *h = NULL;
    int b_wait_output = 0;

    /* decode this frame
     * (1) init bs */
    bs_init(&es_unit->bs, es_unit->data, es_unit->len);

    h = task_get_free_task(mgr);
    mgr->h_dec = h;

    davs2_thread_mutex_lock(&mgr->mutex_aec);

    h->task_info.curr_es_unit = es_unit;     /* record the ES_unit to be decoded */

    /* (2) parse header */
    if (parse_header(h, &es_unit->bs) == 0) {
        h->p_bs = &es_unit->bs;
        /* TODO: 分析该图像头信息，确定当前时刻是否需要输出图像 */
        /* prepare the reference list and the reconstruction buffer */
        if (task_get_references(h, es_unit->pts, es_unit->dts) == 0) {
            b_wait_output = has_new_output_frame(mgr, h);
            mgr->num_frames_in++;

            /* 解锁 */
            davs2_thread_mutex_unlock(&mgr->mutex_aec);
            /* decode picture data */
            davs2_threadpool_run((davs2_threadpool_t *)mgr->thread_pool, decoder_decode_picture_data, h, 0, 0);
        }
    } else {
        davs2_thread_mutex_unlock(&mgr->mutex_aec);
        /* task is free */
        task_unload_packet(h, es_unit);
    }

    return b_wait_output;
}

/* ---------------------------------------------------------------------------
 */
DAVS2_API int
davs2_decoder_send_packet(void *decoder, davs2_packet_t *packet)
{
    davs2_mgr_t *mgr = (davs2_mgr_t *)decoder;
    es_unit_t *es_unit = NULL;
    int ret_type = DAVS2_DEFAULT;

#if DAVS2_TRACE_API
    if (fp_trace_bs != NULL && packet->len > 0) {
        fwrite(packet->data, packet->len, 1, fp_trace_bs);
        fflush(fp_trace_bs);
    }
    if (fp_trace_in) {
        fprintf(fp_trace_in, "%4d\t%d", packet->len, packet->marker);
        fflush(fp_trace_in);
    }
#endif

    /* check the input parameter: packet */
    if (packet == NULL || packet->data == NULL || packet->len <= 0) {
        davs2_log(mgr->decoders, DAVS2_LOG_DEBUG, "Null input packet");
        return DAVS2_ERROR;              /* error */
    }

    /* check packet length */
    if (packet->len < 4) {
        davs2_log(mgr, DAVS2_LOG_DEBUG, "Invalid packet, 4 bytes are needed for one packet (including start_code). Len = %d",
                  packet->len);
        return DAVS2_ERROR;              /* error */
    }
    /* check the first 3 bytes are START_CODE */
    if (packet->data[0] != 0x00 || packet->data[1] != 0x00 || packet->data[2] != 0x01) {
        davs2_log(mgr, DAVS2_LOG_ERROR, "Invalid input Byte-Stream, not start code: %02x%02x%02x",
                  packet->data[0], packet->data[1], packet->data[2]);
        return DAVS2_ERROR;
    }

    /* generate one es_unit for current byte-stream buffer */
    es_unit = davs2_pack_es_unit(mgr, packet->data, packet->len, packet->pts, packet->dts);
    if (es_unit == NULL && mgr->es_unit == NULL) {
        davs2_log(mgr, DAVS2_LOG_ERROR, "Failed to create an ES_UNIT, input Byte-Stream length %d",
                  packet->len);
        return DAVS2_ERROR;
    } else if (es_unit == NULL) {
        // davs2_log(mgr, DAVS2_LOG_DEBUG, "Buffered byte-stream length: %d",
        //           packet->len);
        return DAVS2_DEFAULT;
    }

    /* decode one frame */
    mgr->num_frames_to_output += decoder_decode_es_unit(mgr, es_unit);

#if DAVS2_TRACE_API
    if (fp_trace_in) {
        fprintf(fp_trace_in, "\t%8d\t%2d\t%4d\t%3d\t%3d\n", 
                packet->len, ret_type, out_frame->pic_order_count,
                mgr->num_frames_in, mgr->num_frames_out);
        fflush(fp_trace_in);
    }
#endif
    return ret_type;
}


/* ---------------------------------------------------------------------------
 */
DAVS2_API int
davs2_decoder_recv_frame(void *decoder, davs2_seq_info_t *headerset, davs2_picture_t *out_frame)
{
    davs2_mgr_t *mgr = (davs2_mgr_t *)decoder;
    int ret_type = DAVS2_DEFAULT;

    /* clear output frame data */
    out_frame->magic = NULL;

    /* get one frame or sequence header */
    if (mgr->num_frames_to_output || mgr->new_sps) {
        ret_type = decoder_get_output(mgr, headerset, out_frame, 0);
        if (ret_type == DAVS2_GOT_FRAME) {
            mgr->num_frames_to_output--;
        }
    }

    return ret_type;
}


/* ---------------------------------------------------------------------------
 */
DAVS2_API int
davs2_decoder_flush(void *decoder, davs2_seq_info_t *headerset, davs2_picture_t *out_frame)
{
    davs2_mgr_t *mgr = (davs2_mgr_t *)decoder;
    int ret;

#if DAVS2_TRACE_API
    if (fp_trace_in) {
        fprintf(fp_trace_in, "Flush 0x%p ", decoder);
        fflush(fp_trace_in);
    }
#endif

    if (decoder == NULL) {
        return DAVS2_ERROR;
    }

    mgr->b_flushing     = 1; // label the decoder being flushing
    out_frame->magic    = NULL;
    ret = DAVS2_DEFAULT;

#if DAVS2_TRACE_API
    if (fp_trace_in) {
        fprintf(fp_trace_in, "Fetch ");
        fflush(fp_trace_in);
    }
#endif

    // flush buffered bit-stream
    if (mgr->es_unit != NULL && mgr->es_unit->len >= 4) {
        es_unit_t *es_unit = mgr->es_unit;
        mgr->es_unit = NULL;
        decoder_decode_es_unit(mgr, es_unit);
    }

    ret = decoder_get_output(mgr, headerset, out_frame, 1);

#if DAVS2_TRACE_API
    if (fp_trace_in) {
        fprintf(fp_trace_in, "Ret %d, %3d\t%3d\n", ret, mgr->num_frames_in, mgr->num_frames_out);
        fflush(fp_trace_in);
    }
#endif

    if (ret != DAVS2_DEFAULT) {
        return ret;
    } else {
        return DAVS2_END;
    }
}

/* ---------------------------------------------------------------------------
 */
DAVS2_API void
davs2_decoder_close(void *decoder)
{
    davs2_mgr_t  *mgr = (davs2_mgr_t *)decoder;
    int i;

#if DAVS2_TRACE_API
    if (fp_trace_in != NULL) {
        fprintf(fp_trace_in, "Close 0x%p\n", decoder);
        fflush(fp_trace_in);
    }
#endif
    if (mgr == NULL) {
        return;
    }

    /* signal all decoding threads and the output thread to exit */
    mgr->b_exit = 1;

    /* destroy thread pool */
    if (mgr->num_total_thread != 0) {
        davs2_threadpool_delete((davs2_threadpool_t *)mgr->thread_pool);
    }

    /* close every task */
    for (i = 0; i < mgr->num_decoders; i++) {
        davs2_t *h = &mgr->decoders[i];

        /* free all resources of the decoder */
        decoder_close(h);
    }

    destroy_all_lists(mgr);     /* free all lists */
    destroy_dpb(mgr);           /* free dpb */

    /* destroy the mutex */
    davs2_thread_mutex_destroy(&mgr->mutex_mgr);
    davs2_thread_mutex_destroy(&mgr->mutex_aec);

    /* free memory */
    davs2_free(mgr);          /* free the mgr */

#if DAVS2_TRACE_API
    if (fp_trace_bs != NULL) {
        fclose(fp_trace_bs);
        fp_trace_bs = NULL;
    }
    if (fp_trace_in != NULL) {
        fclose(fp_trace_in);
        fp_trace_in = NULL;
    }
#endif
}
