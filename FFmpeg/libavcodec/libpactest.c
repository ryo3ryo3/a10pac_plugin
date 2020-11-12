
/**
 * @file
 * Intel PAC plug-in test 
 */

#include "libavutil/imgutils.h"
#include "libavutil/avassert.h"
#include "libavutil/pixdesc.h"
#include "libavutil/opt.h"
#include "libavutil/mem.h"
#include "libavutil/thread.h"
#include "avcodec.h"
#include "internal.h"
#include "bytestream.h"
#include "pactest/pacif.h"


typedef struct {
    AVCodecContext *avctx;
    AVPacket *pkt;
    int result;
    AVPacket src;
    uint8_t *data;
    int alloc_size;
    int size;
} PACWriteContext;

typedef struct {
    AVClass *class;
    int src_fnum;
    AVFrame *frame;
    PACWriteContext wctx;
} PACContext;

static const AVOption options[] = {
    { NULL },
};

static void write_callback(void *context, uint8_t *dst_buf, int dst_size, int eopic)
{
    PACWriteContext *c = (PACWriteContext*)context;
    int new_size;

    new_size = c->size + dst_size;
    if (new_size > c->alloc_size) {
        av_log(c->avctx, AV_LOG_ERROR, "Compressed buffer overflow %d > %d\n", new_size, c->alloc_size);
        c->result = AVERROR(ERANGE);
    } else {
        if (c->size == 0) {
            c->result = ff_alloc_packet2(c->avctx, &c->src, c->alloc_size, 0);
            c->data = c->src.data;
        }

        if (c->result == 0) {
            memcpy(c->data, dst_buf, dst_size);
            c->data += dst_size;
            c->size = new_size;
        }
    }

    if (eopic) {
        if (c->size != 0) {
            av_packet_move_ref(c->pkt, &c->src);
            c->pkt->size = c->size;
            c->pkt->flags |= AV_PKT_FLAG_KEY; 
        }
        c->size = 0;
    }
}


/////////////////////////////////////////////////////////////////////////////////////// Single thread API
static av_cold int pactest_init(AVCodecContext *avctx)
{
    int err;
    PACContext *ctx = avctx->priv_data;
    av_log(avctx, AV_LOG_DEBUG, "pactest_init\n");
    ctx->frame = NULL;

    ctx->src_fnum = 0;
    ctx->wctx.avctx  = avctx;
    av_init_packet(&ctx->wctx.src);
    ctx->wctx.alloc_size = avctx->width * avctx->height * 2;
    if (ctx->wctx.alloc_size < 0x8000) {
        ctx->wctx.alloc_size = 0x8000;
    }
    ctx->wctx.size = 0;


    err = init_pac();
    if (err) {
        av_log(avctx, AV_LOG_ERROR, "init_pac() failed with %d\n", err);
        return AVERROR(EINVAL);
    }

    return 0;
    
}

static av_cold int pactest_close(AVCodecContext *avctx)
{
    av_log(avctx, AV_LOG_DEBUG, "pactest_close draining:%d\n", avctx->internal->draining);

    close_pac();

    return 0;
}

static av_cold int pactest_frame(AVCodecContext *avctx, AVPacket *pkt, const AVFrame *frame, int *got_packet)
{
    PACContext *ctx = avctx->priv_data;
    *got_packet = 0;
    av_log(avctx, AV_LOG_DEBUG, "pactest_frame src:%d\n",ctx->src_fnum);

    if (frame) {
    	ctx->wctx.pkt = pkt;
        frame_pac((const void**)frame->data, ctx->wctx.alloc_size, write_callback, (void*)&ctx->wctx);
        pkt->pts = frame->pts;
        pkt->dts = frame->pts;
        ++ctx->src_fnum;
        *got_packet = 1;
    }
    return 0;
}


/////////////////////////////////////////////////////////////////////////////////////// Encoder defines

static const AVClass pac_plugin_class = {
    .class_name = "pac_plugin",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

AVCodec ff_libpactest_encoder = {
    .name           = "pac_plugin",
    .long_name      = NULL_IF_CONFIG_SMALL("PAC Plug-in TEST"),
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_NONE,
    .priv_data_size = sizeof(PACContext),
    .init           = pactest_init,
    .close          = pactest_close,
    .encode2        = pactest_frame,
    .pix_fmts       = (const enum AVPixelFormat[]){
                      AV_PIX_FMT_UYVY422,
                      AV_PIX_FMT_NONE},
    .capabilities   = AV_CODEC_CAP_AUTO_THREADS | AV_CODEC_CAP_DELAY,
    .caps_internal  = FF_CODEC_CAP_INIT_CLEANUP,
    .priv_class     = &pac_plugin_class,
};


