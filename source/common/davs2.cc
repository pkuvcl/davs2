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
        davs2_log(NULL, AVS2_LOG_ERROR, "failed to malloc memory in es_unit_alloc.\n");
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
 */
static int
es_unit_pack(assembler_t *assembler, uint8_t *data, int len, int64_t pts, int64_t dts)
{
    es_unit_t *es_unit = assembler->es_unit;

    if (len > 0) {
        if (es_unit->size < es_unit->len + len) {
            /* reallocate frame buffer */
            int new_size = es_unit->len + len + MAX_ES_FRAME_SIZE * 2;
            es_unit_t *new_es_unit;

            if ((new_es_unit = es_unit_alloc(new_size)) == NULL) {
                return 0;
            }

            memcpy(new_es_unit, es_unit, sizeof(es_unit_t));   /* copy ES Unit information */
            memcpy(new_es_unit->data, es_unit->data, es_unit->len * sizeof(uint8_t));

            es_unit_free(es_unit);

            assembler->es_unit = es_unit = new_es_unit;
        }

        /* copy stream data */
        memcpy(es_unit->data + es_unit->len, data, len * sizeof(uint8_t));
        es_unit->len += len;
        es_unit->pts  = pts;
        es_unit->dts  = dts;
    }

    return 1;
}

/* ---------------------------------------------------------------------------
 * push byte stream data of one frame to input list
 */
static int es_unit_push(davs2_mgr_t *mgr, assembler_t *assembler)
{
    es_unit_t *es_unit = NULL;
    /* check flag */
    if (!assembler->header_found) {
        return 0;
    }
    assembler->header_found = FALSE;

    /* check the pseudo start code */
    es_unit = assembler->es_unit;
    es_unit->len = bs_dispose_pseudo_code(es_unit->data, es_unit->data, es_unit->len);

    /* append current node to the ready list */
    xl_append(&mgr->packets_ready, (node_t *)(es_unit));

    /* fetch a node again from idle list */
    assembler->es_unit = (es_unit_t *)xl_remove_head(&mgr->packets_idle, 1);
    

    return 1;
}

/* ---------------------------------------------------------------------------
 */
static void 
destroy_all_lists(davs2_mgr_t *mgr)
{
    es_unit_t *es_unit = NULL;
    davs2_picture_t *pic = NULL;

    /* ready list */
    for (;;) {
        if ((es_unit = (es_unit_t *)xl_remove_head_ex(&mgr->packets_ready)) == NULL) {
            break;
        }

        es_unit_free(es_unit);
    }

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

    if (mgr->assembler.es_unit) {
        es_unit_free(mgr->assembler.es_unit);
        mgr->assembler.es_unit = NULL;
    }

    xl_destroy(&mgr->packets_idle);
    xl_destroy(&mgr->packets_ready);
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
        xl_init(&mgr->packets_ready) != 0 ||
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

#if DAVS2_API_VERSION == 2
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
#endif

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
                if (mgr->b_flushing && mgr->packets_ready.i_node_num == 0 &&
                    mgr->num_frames_in == mgr->num_frames_out + mgr->outpics.num_output_pic) {
                    mgr->outpics.output++;
                    continue;
                }
#if DAVS2_API_VERSION >= 2
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
                    Sleep(1);
                    davs2_thread_mutex_lock(&mgr->mutex_mgr);
                    continue;
                }
#else
                break;
#endif
            }

            /* 目前输出队列的最小POC与已输出的POC之间间隔较大，将输出POC提前到当前最小POC */
            davs2_log(&mgr->decoders[0], AVS2_LOG_WARNING, "Advance to discontinuous POC: %d\n", frame->i_poc);
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
#if DAVS2_API_VERSION < 2
static void *proc_decoder_output(void *arg)
{
    davs2_mgr_t    *mgr   = (davs2_mgr_t *)arg;
    davs2_param_t *param = &mgr->param;
    davs2_outpic_t *pic   = NULL;

    do {
        /* check for the next frame */
        pic = output_list_get_one_output_picture(mgr);

        if (pic == NULL) {
            /* next frame unavailable, try later */
#if FAST_GET_SPS
            if (mgr->new_sps) {
                davs2_seq_info_t headset;
                memcpy(&headset, &mgr->seq_info.head, sizeof(davs2_seq_info_t));
                param->output_f(NULL, &headset, 0, param->opaque);
                mgr->new_sps = FALSE; /* set flag */
            }
#endif
            Sleep(1);
            continue;
        }

        /* copy out */
        davs2_write_a_frame(pic->pic, pic->frame);

        /* release reference by 1 */
        release_one_frame(pic->frame);

        /* deliver this frame */
        param->output_f(pic->pic, pic->head, 0, param->opaque);

        /* release the output */
        output_list_recycle_picture(mgr, pic);

    } while (mgr->b_exit == 0);

    return NULL;
}
#else
int decoder_get_output(davs2_mgr_t *mgr, davs2_seq_info_t *headerset, davs2_picture_t *out_frame, int is_flush)
{
    davs2_outpic_t *pic   = NULL;
    int b_wait_new_frame = mgr->num_frames_in + mgr->packets_ready.i_node_num - mgr->num_frames_out > 8 + mgr->num_aec_thread;

    while (mgr->num_frames_in + mgr->packets_ready.i_node_num > mgr->num_frames_out && /* no more output */
           (b_wait_new_frame || is_flush)) {
        if (mgr->new_sps) {
            memcpy(headerset, &mgr->seq_info.head, sizeof(davs2_seq_info_t));
            mgr->new_sps = FALSE; /* set flag */
            out_frame->ret_type = DAVS2_GOT_HEADER;
            out_frame->magic = NULL;
            return DAVS2_GOT_HEADER;
        }

        /* check for the next frame */
        pic = output_list_get_one_output_picture(mgr);

        if (pic == NULL) {
            Sleep(1);
        } else {
            break;
        }
    }

    if (pic == NULL) {
        if (mgr->new_sps) {
            memcpy(headerset, &mgr->seq_info.head, sizeof(davs2_seq_info_t));
            mgr->new_sps = FALSE; /* set flag */
            out_frame->ret_type = DAVS2_GOT_HEADER;
            out_frame->magic = NULL;
            return DAVS2_GOT_HEADER;
        }
        return DAVS2_DEFAULT;
    }

    mgr->num_frames_out++;

    /* copy out */
    davs2_write_a_frame(pic->pic, pic->frame);

    /* release reference by 1 */
    release_one_frame(pic->frame);

    /* deliver this frame */
    memcpy(out_frame, pic->pic, sizeof(davs2_picture_t));
    out_frame->magic       = pic;
    out_frame->ret_type    = mgr->new_sps ? DAVS2_GOT_BOTH : DAVS2_GOT_FRAME;

    return DAVS2_GOT_FRAME;
}
#endif


#if DAVS2_API_VERSION >= 2
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
        output_list_recycle_picture(mgr, (davs2_outpic_t *)out_frame->magic);
    }
}
#endif

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
static es_unit_t *task_load_packet(davs2_mgr_t *mgr)
{
    es_unit_t *es_unit = NULL;

    /* try to get one frame to decode */
    es_unit = (es_unit_t *)xl_remove_head(&mgr->packets_ready, 0);

    return es_unit;
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

#if (DAVS2_API_VERSION & 1)
/* --------------------------------------------------------------------------
 * Thread of decoding process (decoding and reconstruction)
 */
static void *proc_decoder_decode(void *arg, int arg2)
{
    davs2_mgr_t  *mgr = (davs2_mgr_t *)arg;
    davs2_t *h = NULL;

    UNUSED_PARAMETER(arg2);
#if !CTRL_AEC_THREAD
    /* get a free task handler and run the decoder */
    h = task_get_free_task(mgr);
#endif

    while (mgr->b_exit == 0) {
        int b_has_picture_data = 0;
        es_unit_t *es_unit = NULL;

        davs2_thread_mutex_lock(&mgr->mutex_aec);
        /* get one frame to decode */
        es_unit = task_load_packet(mgr);

        if (es_unit == NULL) {
            davs2_thread_mutex_unlock(&mgr->mutex_aec);
            /* no frame, we try later */
            Sleep(1);
            continue;
        }

#if CTRL_AEC_THREAD
        /* get a free task handler and run the decoder */
        h = task_get_free_task(mgr);
#endif

        /* task is busy */
        h->task_info.task_status  = TASK_BUSY;
        h->task_info.curr_es_unit = es_unit;
        // mgr->num_active_decoders++;

        /* decode this frame
         * (1) init bs */
        bs_init(&h->bs, es_unit->data, es_unit->len);

        /* (2) parse header */
        if (parse_header(h) == 0) {
            b_has_picture_data = 1;
            mgr->num_frames_in++;
        }

        if (b_has_picture_data && task_get_references(h, es_unit->pts, es_unit->dts) == 0) {
            davs2_thread_mutex_unlock(&mgr->mutex_aec);
            /* decode picture data */
            decoder_decode_picture_data(h, 0);
        } else {
            davs2_thread_mutex_unlock(&mgr->mutex_aec);
            /* task is free */
            task_unload_packet(h, es_unit);
        }
    }   // while (mgr->b_exit == 0)

    return NULL;
}
#endif  //  DAVS2_API_VERSION < 2


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
    davs2_log(NULL, AVS2_LOG_WARNING, "davs2: %s.%d, %s",
               XVERSION_STR, BIT_DEPTH, XBUILD_TIME);

#if DAVS2_TRACE_API
    fp_trace_bs = fopen("trace_bitstream.avs", "wb");
    fp_trace_in = fopen("trace_input.txt", "w");
#endif

    /* check parameters */
    if (param == NULL) {
        davs2_log(NULL, AVS2_LOG_ERROR, "Invalid input parameters: Null parameters\n");
        return 0;
    }
#if DAVS2_API_VERSION < 2
    if (param->output_f == NULL) {
        davs2_log(NULL, AVS2_LOG_ERROR, "Invalid input parameters: Null output functions\n");
        return 0;
    }
#endif

    /* init all function handlers */
#if HAVE_MMX
    cpuid = davs2_cpu_detect();
#endif
    init_all_primitives(cpuid);

    /* CPU capacities */
    davs2_get_simd_capabilities(buf_cpu, cpuid);
    davs2_log(NULL, AVS2_LOG_WARNING, "CPU Capabilities: %s\n", buf_cpu);

    mem_size = sizeof(davs2_mgr_t) + CACHE_LINE_SIZE
        + AVS2_THREAD_MAX * (sizeof(davs2_t) + CACHE_LINE_SIZE);
    CHECKED_MALLOCZERO(mem_ptr, uint8_t *, mem_size);

    mgr = (davs2_mgr_t *)mem_ptr;
    mem_ptr += sizeof(davs2_mgr_t);
    ALIGN_POINTER(mem_ptr);
    memcpy(&mgr->param, param, sizeof(davs2_param_t));

    if (mgr->param.threads <= 0) {
        mgr->param.threads = davs2_cpu_num_processors();
    }
    if (mgr->param.threads > max_num_thread) {
        mgr->param.threads = max_num_thread;
        davs2_log(NULL, AVS2_LOG_WARNING, "Max number of thread reached, forcing to be %d\n", max_num_thread);
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

#if DAVS2_API_VERSION >= 2
    mgr->num_decoders++;
#endif

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
        davs2_log(NULL, AVS2_LOG_ERROR, 
            "Invalid thread number configuration: num_task[%d], num_threads[%d], num_aec_thread[%d], num_pool[%d]\n",
            mgr->num_decoders, mgr->num_total_thread, mgr->num_aec_thread, mgr->num_rec_thread);
        goto fail;
    }

    /* spawn the output thread */
    mgr->num_frames_in  = 0;
    mgr->num_frames_out = 0;
#if DAVS2_API_VERSION < 2
    if (davs2_thread_create(&mgr->thread_output, NULL, proc_decoder_output, mgr)) {
        goto fail;
    }
#endif

    /* init all the tasks */
    for (i = 0; i < mgr->num_decoders; i++) {
        davs2_t *h = &mgr->decoders[i];

        /* init the decode context */
        decoder_open(mgr, h);
        // davs2_log(h, AVS2_LOG_WARNING, "Decoder [%2d]: %p", i, h);

        h->task_info.task_id     = i;
        h->task_info.task_status = TASK_FREE;
        h->task_info.taskmgr     = mgr;
    }

    /* initialize thread pool for AEC decoding and reconstruction */
    davs2_threadpool_init((davs2_threadpool_t **)&mgr->thread_pool, mgr->num_total_thread, NULL, NULL, 0);

#if (DAVS2_API_VERSION & 1)
    /* spawn every AEC decoding task */
    for (i = 0; i < mgr->num_aec_thread; i++) {
        davs2_threadpool_run((davs2_threadpool_t *)mgr->thread_pool, proc_decoder_decode, mgr, 0, 0);
    }
#endif

    davs2_log(NULL, AVS2_LOG_INFO, "using %d thread(s): %d(frame/AEC)+%d(pool/REC), %d tasks", 
        mgr->num_total_thread, mgr->num_aec_thread, mgr->num_rec_thread, mgr->num_decoders);

    return mgr;

fail:
    davs2_log(NULL, AVS2_LOG_ERROR, "failed to open decoder\n");
    davs2_decoder_close(mgr);

    return NULL;
}

/* ---------------------------------------------------------------------------
 */
static 
int decoder_find_pictures(davs2_mgr_t *mgr, davs2_packet_t *packet)
{
    /* write new header */
#define HEADLEN             4         /* header length */
#define WRITEHEADER(x1, x2, x3, x4)\
{\
    assembler->es_unit->data[0] = (x1);\
    assembler->es_unit->data[1] = (x2);\
    assembler->es_unit->data[2] = (x3);\
    assembler->es_unit->data[3] = (x4);\
    assembler->es_unit->len     = HEADLEN;\
    assembler->header_found     = TRUE;\
}

    assembler_t *assembler = &mgr->assembler;
    uint8_t  pre3rdbyte = 0xff;
    uint8_t  pre2ndbyte = 0xff;
    uint8_t  pre1stbyte = 0xff;
    uint8_t *es_buf;
    int      es_len;
    int      offset;
    int      b_picture_data_found = 0;

    /* check the input parameter: packet */
    assert(packet != NULL && packet->data > 0 && packet->len > 0);
    if (packet == NULL || packet->data == 0 || packet->len <= 0) {
        return -1;              /* error */
    }

    es_buf = packet->data;
    es_len = packet->len;

    if (assembler->es_unit == NULL) {
        assembler->es_unit = (es_unit_t *)xl_remove_head(&mgr->packets_idle, 1);
    }

    if (assembler->es_unit->len >= 3) {
        pre3rdbyte = assembler->es_unit->data[assembler->es_unit->len - 3];
        pre2ndbyte = assembler->es_unit->data[assembler->es_unit->len - 2];
        pre1stbyte = assembler->es_unit->data[assembler->es_unit->len - 1];
    } else if (assembler->es_unit->len == 2) {
        pre2ndbyte = assembler->es_unit->data[assembler->es_unit->len - 2];
        pre1stbyte = assembler->es_unit->data[assembler->es_unit->len - 1];
    } else if (assembler->es_unit->len == 1) {
        pre1stbyte = assembler->es_unit->data[assembler->es_unit->len - 1];
    }

    for (; (offset = find_pic_start_code(pre3rdbyte, pre2ndbyte, pre1stbyte, es_buf, es_len)) < es_len;) {
        if (offset < 0) {
            assert(offset > -HEADLEN);

            assembler->es_unit->len += offset;
            b_picture_data_found = assembler->header_found;

            if (assembler->header_found) {
                if (!es_unit_push(mgr, assembler)) {
                    return -1;
                }
            }

            /* write start code to packet header */
            if (offset == -3) {
                WRITEHEADER(pre3rdbyte, pre2ndbyte, pre1stbyte, es_buf[0]);
            } else if (offset == -2) {
                WRITEHEADER(pre2ndbyte, pre1stbyte, es_buf[0], es_buf[1]);
            } else if (offset == -1) {
                WRITEHEADER(pre1stbyte, es_buf[0], es_buf[1], es_buf[2]);
            }
        } else {
            b_picture_data_found = assembler->header_found;

            if (assembler->header_found) {
                assert(assembler->es_unit->len > 0);

                if (!es_unit_pack(assembler, es_buf, offset, packet->pts, packet->dts)) {
                    return -1;
                }

                if (!es_unit_push(mgr, assembler)) {
                    return -1;
                }
            }
            /* write start code to packet header */
            WRITEHEADER(es_buf[offset], es_buf[offset + 1], es_buf[offset + 2], es_buf[offset + 3]);
        }

        es_buf += (offset + HEADLEN);
        es_len -= (offset + HEADLEN);
#if DAVS2_API_VERSION == 2
        if (b_picture_data_found) {
            break;
        }
#else
        b_picture_data_found = 0;
#endif
        pre3rdbyte = 0xff;
        pre2ndbyte = 0xff;
        pre1stbyte = 0xff;
    }

    /* when no start-code is found, copy remaining bytes */
    if (!b_picture_data_found) {
        if (!es_unit_pack(assembler, packet->data + (packet->len - es_len), es_len, packet->pts, packet->dts)) {
            return -1;
        }
        es_len = 0;
    }

    return packet->len - es_len;
#undef HEADLEN
#undef WRITEHEADER
}

#if DAVS2_API_VERSION >= 2
/* ---------------------------------------------------------------------------
 */
int decoder_decode_es_unit(davs2_mgr_t *mgr, davs2_t *h)
{
    es_unit_t *es_unit;
    int b_wait_output = 0;

    davs2_thread_mutex_lock(&mgr->mutex_aec);
    /* get one frame to decode */
    es_unit = task_load_packet(mgr);
    assert(es_unit != NULL);

    /* task is busy */
    h->task_info.task_status = TASK_BUSY;
    h->task_info.curr_es_unit = es_unit;

    /* decode this frame
    * (1) init bs */
    bs_init(&h->bs, es_unit->data, es_unit->len);

    /* (2) parse header */
    if (parse_header(h) == 0) {
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
#endif

/* ---------------------------------------------------------------------------
 */
DAVS2_API int
#if DAVS2_API_VERSION >= 2
davs2_decoder_decode(void *decoder, davs2_packet_t *packet, davs2_seq_info_t *headerset, davs2_picture_t *out_frame)
#else
davs2_decoder_decode(void *decoder, davs2_packet_t *packet)
#endif
{
    davs2_mgr_t *mgr = (davs2_mgr_t *)decoder;
    int num_bytes_read;
#if DAVS2_API_VERSION >= 2
    /* clear output frame data */
    out_frame->ret_type = DAVS2_DEFAULT;
    out_frame->magic    = NULL;
#endif

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
    num_bytes_read = decoder_find_pictures(mgr, packet);

#if DAVS2_API_VERSION == 2
    int b_wait_output = 0;
    /* 参考代码： proc_decoder_decode() */
    /* 解码一帧图像头 */
    if (mgr->packets_ready.i_node_num > 0) {
        davs2_t *h = task_get_free_task(mgr);
        b_wait_output = decoder_decode_es_unit(mgr, h);
    }

    /* get one frame or sequence header */
    if (b_wait_output || mgr->new_sps) {
        decoder_get_output(mgr, headerset, out_frame, 0);
    }
#elif DAVS2_API_VERSION == 3
    decoder_get_output(mgr, headerset, out_frame, 0);
#endif

#if DAVS2_TRACE_API
    if (fp_trace_in) {
        fprintf(fp_trace_in, "\t%8d\t%2d\t%4d\t%3d\t%3d\n", 
                num_bytes_read, out_frame->ret_type, out_frame->pic_order_count,
                mgr->num_frames_in, mgr->num_frames_out);
        fflush(fp_trace_in);
    }
#endif
    return num_bytes_read;
}


/* ---------------------------------------------------------------------------
 */
#if DAVS2_API_VERSION >= 2
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
        return DAVS2_END;
    }


    mgr->b_flushing     = 1; // label the decoder being flushing
    out_frame->magic    = NULL;
    out_frame->ret_type = DAVS2_DEFAULT;

    /* the last packet of the bitstream; decoding ends here */
    if (mgr->assembler.header_found && mgr->assembler.es_unit->len > 0) {
        if (!es_unit_push(mgr, &mgr->assembler)) {
#if DAVS2_TRACE_API
            if (fp_trace_in) {
                fprintf(fp_trace_in, "Ret %d\n", -1);
                fflush(fp_trace_in);
            }
#endif
            return -1;
        }
    }

    /* check is there any packets left? */
    if (mgr->packets_ready.i_node_num > 0) {
        davs2_t *h = task_get_free_task(mgr);
        decoder_decode_es_unit(mgr, h);
    }

#if DAVS2_TRACE_API
    if (fp_trace_in) {
        fprintf(fp_trace_in, "Fetch ");
        fflush(fp_trace_in);
    }
#endif
    ret = decoder_get_output(mgr, headerset, out_frame, 1);

#if DAVS2_TRACE_API
    if (fp_trace_in) {
        fprintf(fp_trace_in, "Ret %d, %3d\t%3d\n", ret, mgr->num_frames_in, mgr->num_frames_out);
        fflush(fp_trace_in);
    }
#endif

    if (ret != DAVS2_DEFAULT && ret != DAVS2_END) {
        return ret;
    } else {
        return DAVS2_END;
    }
#else
DAVS2_API void
davs2_decoder_flush(void *decoder)
{
    davs2_mgr_t *mgr = (davs2_mgr_t *)decoder;
    if (decoder == NULL) {
        return;
    }

    mgr->b_flushing     = 1; // label the decoder being flushing
    decoder_flush(mgr);
#endif
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

    /* wait until the output thread exit */
#if DAVS2_API_VERSION < 2
    davs2_thread_join(mgr->thread_output, NULL);
#endif

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
