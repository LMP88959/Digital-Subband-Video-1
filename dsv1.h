/*****************************************************************************/
/*
 * Digital Subband Video 1
 *   DSV-1
 *   
 *     -
 *    =--  2023-2024 EMMIR
 *   ==---  Envel Graphics
 *  ===----
 *  
 *   GitHub : https://github.com/LMP88959
 *   YouTube: https://www.youtube.com/@EMMIR_KC/videos
 *   Discord: https://discord.com/invite/hdYctSmyQJ
 */
/*****************************************************************************/
/*
 * The accompanying software was designed and written by
 * EMMIR, 2023-2024 of Envel Graphics.
 * No responsibility is assumed by the author.
 * 
 * Feel free to use the code in any way you would like, however, if you release
 * anything with it, a comment in your code/README saying where you got this
 * code would be a nice gesture but it’s not mandatory.
 * 
 * The software is provided "as is", without warranty of any kind, express or
 * implied, including but not limited to the warranties of merchantability,
 * fitness for a particular purpose and noninfringement. In no event shall the
 * authors or copyright holders be liable for any claim, damages or other
 * liability, whether in an action of contract, tort or otherwise, arising from,
 * out of or in connection with the software or the use or other dealings
 * in the software.
 */
#ifndef _DSV1_H_
#define _DSV1_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * HOW TO INCLUDE DSV1 IN YOUR PROGRAM:
 * 
 * In one translation unit (usually .c/.cpp file), do the following:
 *
 * #define _DSV1_IMPL_
 * #define _DSV1_INCLUDE_ENCODER_
 * #define _DSV1_INCLUDE_DECODER_
 * #include "dsv1.h"
 * 
 * NOTE: if you only want the encoder, don't #define _DSV1_INCLUDE_DECODER_
 *       same applies to the decoder
 *       
 * if you need access to DSV1 types and defines in another file, all you need
 * to do is:
 * 
 * #include "dsv1.h"
 * 
 * if you need function declarations for the encoder or decoder as well, then:
 * 
 * #define _DSV1_INCLUDE_ENCODER_
 * #define _DSV1_INCLUDE_DECODER_
 * #include "dsv1.h"
 * 
 * OPTIONS:
 * 
 * _DSV1_NO_STDIO_ - define to omit stdio based functions, you will need to
 *                   provide your version of the following macro:
 *                   
 *        DSV_LOG_LVL(level, x)
 *              level - level of the log call
 *              x - printf style parameters ("format", data0, data1, etc..)
 *        
 *        
 * _DSV1_NO_STDINT_ - define if you don't have stdint.h available. You will
 *                    need to provide your own typedefs.
 *        
 * _DSV1_NO_ALLOC_ - define to provide your own memory allocation and freeing
 *                   functions, you will need to provide your version of the
 *                   following macros:
 *                   
 *        DSV1_ALLOC_FUNC(num_bytes)
 *        DSV1_FREE_FUNC(pointer)
 *                   
 *                   !!!!! NOTE !!!!!
 *                   The alloc function MUST return a pointer to a block of
 *                   ZEROED OUT memory that is "num_bytes" in size.
 *                   If the memory returned by your alloc is not zeroed,
 *                   the behavior of this DSV1 implementation will be undefined.
 *                   
 *                   
 * _DSV_MEMORY_STATS_ - define to enable memory counting and basic statistics
 * 
 ******************************************************************************
 * 
 * ENCODING / DECODING
 * 
 * The best reference is the dsv_main.c program,
 * but a quick synopsis is given here:
 * 
 * Encoding:
 * 
 * - fill DSV_META struct with video metadata
 * - fill DSV_ENCODER struct with parameters
 * dsv_enc_init
 * dsv_enc_set_metadata
 * dsv_enc_start
 * 
 * finished = 0
 * while (!finished) {
 *     DSV_BUF bufs[4];
 * 
 *     f = read_frame();
 *     if (done_with_video) {
 *         dsv_enc_end_of_stream(bufs);
 *         savebuffer(&bufs[0]);
 *         dsv_buf_free(&bufs[0]);
 *         break;
 *     }
 *     dsv_load_planar_frame(f);
 *     state = dsv_enc(f, bufs);
 *     finished = (state & DSV_ENC_FINISHED);
 *     num_bufs = (state & DSV_ENC_NUM_BUFS);
 *     for (i = 0; i < num_bufs; i++) {
 *          savebuffer(&bufs[i]);
 *          dsv_buf_free(&bufs[i]);
 *     }
 * }
 * 
 * dsv_enc_free
 * 
 * 
 * Decoding:
 * 
 * - fill DSV_DECODER struct with parameters
 * 
 * while (1) {
 *    DSV_FRAME *frame;
 *    DSV_BUF packet_buf;
 *    int packet_type = read_packet(packet_buf);
 *    
 *    code = dsv_dec(packet_buf, &frame);
 *    if (code == DSV_DEC_GOT_META) {
 *        - do something with metadata
 *    } else {
 *        if (code == DSV_DEC_EOS) {
 *            - end of stream
 *            break;
 *        }
 *        
 *        - do what you need to do to decoded frame
 *        
 *        dsv_frame_ref_dec(frame);
 *    }
 *    
 *    dsv_dec_free
 * }
 * 
 */


/*********************** BEGINNING OF PUBLIC INTERFACE ***********************/
#ifndef _DSV1_NO_STDINT_
#include <stdint.h>
#endif
#ifndef _DSV1_NO_STDIO_
#include <stdio.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <limits.h>

typedef uint32_t DSV_FNUM; /* frame number */

typedef struct {
    int width;
    int height;
    int subsamp;
    
    int fps_num;
    int fps_den;
    int aspect_num;
    int aspect_den;
} DSV_META;

typedef struct {
    uint8_t *data;
    int len;
    int format;
    int stride;
    int w, h;
    int hs, vs; /* horizontal and vertical shift for subsampling */
} DSV_PLANE;

typedef struct {
    uint8_t *alloc;
    
    DSV_PLANE planes[3];
    
    int refcount;
    
    int format;
    int width;
    int height;
    
    int border;
} DSV_FRAME;

typedef struct {
    DSV_META *vidmeta;
    
    int is_ref;
    int has_ref;
        
    /* block sizes */
    int blk_w;
    int blk_h;
    /* number of blocks horizontally and vertically in the image */
    int nblocks_h;
    int nblocks_v;
} DSV_PARAMS;

typedef struct {
    unsigned char *data;
    unsigned len;    
} DSV_BUF;

typedef struct {
    union {
        struct {
            int16_t x;
            int16_t y;
        } mv;
        int32_t all;
    } u;
    uint8_t mode;
    uint8_t submask;
    uint8_t lo_var;
    uint8_t lo_tex;
    uint8_t high_detail;
} DSV_MV; /* motion vector */

/* allocates a block of zeroed out memory */
extern void *dsv_alloc(int size);
/* frees the memory pointer. Must have been allocated by dsv_alloc */
extern void dsv_free(void *ptr);
/* print a report of statistics */
extern void dsv_memory_report(void);

extern void dsv_set_log_level(int level);
extern int dsv_get_log_level(void);

#ifdef _DSV1_INCLUDE_DECODER_ 

typedef struct {
    DSV_PARAMS params;
    DSV_FRAME *out_frame;
    DSV_FRAME *ref_frame;
    
    unsigned char *stable_blocks;
    int refcount;
} DSV_IMAGE;

typedef struct {
    DSV_META vidmeta;
    DSV_IMAGE *ref;
#define DSV_DRAW_STABHQ 1 /* stable / high quality blocks */
#define DSV_DRAW_MOVECS 2 /* motion vectors */
#define DSV_DRAW_IBLOCK 4 /* intra subblocks */
    int draw_info; /* set by user */
    int got_metadata;
} DSV_DECODER;

#define DSV_DEC_OK        0
#define DSV_DEC_ERROR     1
#define DSV_DEC_EOS       2
#define DSV_DEC_GOT_META  3
#define DSV_DEC_NEED_NEXT 4

/* decode a buffer, returns a frame in *out and the frame number in *fn */
extern int dsv_dec(DSV_DECODER *d, DSV_BUF *buf, DSV_FRAME **out, DSV_FNUM *fn);

/* get the metadata that was decoded. NOTE: if no metadata has been decoded
 * yet, the returned struct will not contain any useful values. */
extern DSV_META *dsv_get_metadata(DSV_DECODER *d);

/* free anything the decoder was holding on to */
extern void dsv_dec_free(DSV_DECODER *d);

#endif /* decoder include */

#ifdef _DSV1_INCLUDE_ENCODER_ 

#define DSV_ENC_NUM_BUFS  0x03 /* mask */
#define DSV_ENC_FINISHED  0x04

#define DSV_RATE_CONTROL_CRF  0 /* constant rate factor */
#define DSV_RATE_CONTROL_ABR  1 /* one pass average bitrate */

#define DSV_MAX_PYRAMID_LEVELS 5

typedef struct _DSV_ENCDATA {
    int refcount;
    
    DSV_FNUM fnum;
    DSV_FRAME *input_frame;
    DSV_FRAME *padded_frame;
    DSV_FRAME *pyramid[DSV_MAX_PYRAMID_LEVELS];
    DSV_FRAME *recon_frame;
    DSV_FRAME *xf_frame;
    DSV_FRAME *residual;
    
    DSV_PARAMS params;
    
    int quant;
    int isP;
    
    struct _DSV_ENCDATA *refdata;
    
    DSV_MV *final_mvs;
} DSV_ENCDATA;

typedef struct {
    int quality; /* user configurable, 0...DSV_MAX_QUALITY  */
    
    int gop; /* gop length in frames */
    int do_scd; /* scene change detection */
    
    /* rate control */
    int rc_mode; /* rate control mode */
    /* 0 = keep quality more or less constant through the entire video.
     * 1 = quantize P frames more in high motion areas. */
    int rc_high_motion_nudge;
    
    /* approximate average bitrate desired */
    unsigned bitrate;
    /* for ABR */
    int max_q_step; /* max quality step */
    int min_quality; /* 0...DSV_MAX_QUALITY */
    int max_quality; /* 0...DSV_MAX_QUALITY */
    int min_I_frame_quality; /* 0...DSV_MAX_QUALITY */
    
    int intra_pct_thresh; /* 0-100% */
    int scene_change_delta;
    unsigned stable_refresh; /* # frames after which stability accum resets */
    int pyramid_levels; /* 0 = auto determine */
    
    /* used internally */
    unsigned rc_quant;
    /* bpf = bytes per frame */
#define DSV_BPF_RESET 256 /* # frames after which average bpf resets */
    unsigned bpf_total;
    unsigned bpf_reset;
    int bpf_avg;
    int total_P_frame_q;
    int avg_P_frame_q;
    int last_P_frame_over;
    int back_into_range;
    
    DSV_FNUM next_fnum;
    DSV_ENCDATA *ref;
    DSV_META vidmeta;
    int prev_link;
    int force_metadata;
    
    struct DSV_STAB_ACC {
        signed x : 16;
        signed y : 16;
    } *stability;
    unsigned refresh_ctr;
    unsigned char *stable_blocks;
    
    DSV_FNUM prev_gop;
    int prev_avg_luma;
} DSV_ENCODER;

extern void dsv_enc_init(DSV_ENCODER *enc);
extern void dsv_enc_free(DSV_ENCODER *enc);
extern void dsv_enc_set_metadata(DSV_ENCODER *enc, DSV_META *md);
/* force metadata to be added to the next frame */
extern void dsv_enc_force_metadata(DSV_ENCODER *enc);

extern void dsv_enc_start(DSV_ENCODER *enc);

/* returns number of buffers available in bufs ptr */
extern int dsv_enc(DSV_ENCODER *enc, DSV_FRAME *frame, DSV_BUF *bufs);
extern void dsv_enc_end_of_stream(DSV_ENCODER *enc, DSV_BUF *bufs);

#endif /* encoder impl */

/* create a frame */
extern DSV_FRAME *dsv_mk_frame(int format, int width, int height, int border);
/* wrap a frame around existing data */
extern DSV_FRAME *dsv_load_planar_frame(int format, void *data, int width, int height);

/* reference counting */
extern DSV_FRAME *dsv_frame_ref_inc(DSV_FRAME *frame);
extern void dsv_frame_ref_dec(DSV_FRAME *frame);

extern void dsv_frame_copy(DSV_FRAME *dst, DSV_FRAME *src);

extern DSV_FRAME *dsv_clone_frame(DSV_FRAME *f, int border);
/* extend edges of frame if it has a border */
extern DSV_FRAME *dsv_extend_frame(DSV_FRAME *frame);

extern void dsv_mk_buf(DSV_BUF *buf, int size);
extern void dsv_buf_free(DSV_BUF *buffer);

#ifndef _DSV1_NO_STDIO_
extern int dsv_yuv_write(FILE *out, int fno, DSV_PLANE *fd);
extern int dsv_yuv_read(FILE *in, int fno, uint8_t *o, int w, int h, int subsamp);
#endif


/************************** END OF PUBLIC INTERFACE **************************/



/* B.1 Packet Header */
#define DSV_FOURCC_0     'D'
#define DSV_FOURCC_1     'S'
#define DSV_FOURCC_2     'V'
#define DSV_FOURCC_3     '1'
#define DSV_VERSION_MINOR 0

/* B.1.1 Packet Type */
#define DSV_PT_META 0x00
#define DSV_PT_PIC  0x04
#define DSV_PT_EOS  0x10
#define DSV_MAKE_PT(is_ref, has_ref) (DSV_PT_PIC | ((is_ref) << 1) | (has_ref))

#define DSV_PT_IS_PIC(x)   ((x) & 0x4)
#define DSV_PT_IS_REF(x)  (((x) & 0x6) == 0x6)
#define DSV_PT_HAS_REF(x)  ((x) & 0x1)

#define DSV_PACKET_HDR_SIZE (4 + 1 + 1 + 4 + 4)
#define DSV_PACKET_TYPE_OFFSET 5
#define DSV_PACKET_PREV_OFFSET 6
#define DSV_PACKET_NEXT_OFFSET 10

/* B.2.3 Picture Packet */
#define DSV_MIN_BLOCK_SIZE 16
#define DSV_MAX_BLOCK_SIZE 64

#define DSV_GOP_INTRA 0
#define DSV_GOP_INF   INT_MAX

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef CLAMP
#define CLAMP(x, a, b) ((x) < (a) ? (a) : ((x) > (b) ? (b) : (x)))
#endif
#define DSV_ROUND_SHIFT(x, shift) (((x) + (1 << (shift)) - 1) >> (shift))
#define DSV_ROUND_POW2(x, pwr) (((x) + (1 << (pwr)) - 1) & ((unsigned)(~0) << (pwr)))
#define DSV_DIV_ROUND(a,b) (((a) + (b) - 1) / (b))

#define DSV_FMT_FULL_V 0x0
#define DSV_FMT_DIV2_V 0x1
#define DSV_FMT_DIV4_V 0x2
#define DSV_FMT_FULL_H 0x0
#define DSV_FMT_DIV2_H 0x4
#define DSV_FMT_DIV4_H 0x8

/* unsigned 8 bit per channel required, subsampling is only for chroma.
 * Only planar YUV is supported.
 */
#define DSV_SUBSAMP_444  (DSV_FMT_FULL_H | DSV_FMT_FULL_V)
#define DSV_SUBSAMP_422  (DSV_FMT_DIV2_H | DSV_FMT_FULL_V)
#define DSV_SUBSAMP_420  (DSV_FMT_DIV2_H | DSV_FMT_DIV2_V)
#define DSV_SUBSAMP_411  (DSV_FMT_DIV4_H | DSV_FMT_FULL_V)

#define DSV_FORMAT_H_SHIFT(format) (((format) >> 2) & 0x3)
#define DSV_FORMAT_V_SHIFT(format) ((format) & 0x3)

/* B.2.3.2 Motion Data - Intra Sub-Block Masks */
#define DSV_MODE_INTER   0 /* whole block is inter */
#define DSV_MODE_INTRA   1 /* some or all of the block is intra */
#define DSV_MASK_INTRA00 1 /* top left is intra */
#define DSV_MASK_INTRA01 2 /* top right is intra */
#define DSV_MASK_INTRA10 4 /* bottom left is intra */
#define DSV_MASK_INTRA11 8 /* bottom right is intra */
#define DSV_MASK_ALL_INTRA (DSV_MASK_INTRA00 | DSV_MASK_INTRA01 | DSV_MASK_INTRA10 | DSV_MASK_INTRA11)

#define DSV_GET_LINE(p, y) ((p)->data + (y) * (p)->stride)
#define DSV_GET_XY(p, x, y) ((p)->data + (x) + (y) * (p)->stride)

/* B.2.3.3 Image Data - Quantization Parameter Bits */
#define DSV_MAX_QP_BITS 11
#define DSV_MAX_QUALITY ((1 << DSV_MAX_QP_BITS) - 1)
#define DSV_QUALITY_PERCENT(pct) (DSV_MAX_QUALITY * (pct) / 100) 

#define DSV_LEVEL_NONE    0
#define DSV_LEVEL_ERROR   1
#define DSV_LEVEL_WARNING 2
#define DSV_LEVEL_INFO    3
#define DSV_LEVEL_DEBUG   4

extern char *dsv_lvlname[DSV_LEVEL_DEBUG + 1];

#ifndef _DSV1_NO_STDIO_
#define DSV_LOG_LVL(level, x) \
    do { if (level <= dsv_get_log_level()) { \
      printf("[DSV][%s] ", dsv_lvlname[level]); \
      printf("%s: %s(%d): ", __FILE__,  __FUNCTION__, __LINE__); \
      printf x; \
      printf("\n"); \
    }} while(0)\

#endif

#define DSV_ERROR(x)   DSV_LOG_LVL(DSV_LEVEL_ERROR, x)
#define DSV_WARNING(x) DSV_LOG_LVL(DSV_LEVEL_WARNING, x)
#define DSV_INFO(x)    DSV_LOG_LVL(DSV_LEVEL_INFO, x)
#define DSV_DEBUG(x)   DSV_LOG_LVL(DSV_LEVEL_DEBUG, x)

#if 1
#define DSV_ASSERT(x) do {                  \
    if (!(x)) {                             \
        DSV_ERROR(("assert: " #x));         \
        exit(-1);                           \
    }                                       \
} while(0)
#else
#define DSV_ASSERT(x)
#endif

/* subsections of the encoded data */
#define DSV_SUB_MODE 0 /* block modes */
#define DSV_SUB_MV_X 1 /* motion vector x coordinates */
#define DSV_SUB_MV_Y 2 /* motion vector y coordinates */
#define DSV_SUB_SBIM 3 /* sub block intra masks */
#define DSV_SUB_NSUB 4

#define DSV_FRAME_BORDER DSV_MAX_BLOCK_SIZE

/* macros for really simple operations */
#define dsv_bs_aligned(bs) (((bs)->pos & 7) == 0)
#define dsv_bs_ptr(bs) ((bs)->pos / 8)
#define dsv_bs_set(bs, ptr) ((bs)->pos = (ptr) * 8)
#define dsv_bs_skip(bs, n_bytes) ((bs)->pos += (n_bytes) * 8)

#define DSV_MAXLVL 3

/* for highest freq */
#define DSV_QP_I 3
#define DSV_QP_P 1

#define DSV_HP_COEF 9

#ifdef _DSV1_IMPL_
#ifndef _DSV1_IMPL_GUARD_
#define _DSV1_IMPL_GUARD_

/* subband coefs */
typedef int32_t DSV_SBC;
typedef struct {
    DSV_SBC *data;
    int width;
    int height;
} DSV_COEFS;

typedef struct {
    DSV_PARAMS *params;
    unsigned char *stable_blocks;
    unsigned char cur_plane;
    unsigned char isP; /* is P frame */
} DSV_STABILITY;

typedef struct {
    uint8_t *start;
    unsigned pos;
} DSV_BS;

typedef struct {
    DSV_BS bs;
    int nz;
} DSV_ZBRLE;

char *dsv_lvlname[DSV_LEVEL_DEBUG + 1] = {
    "NONE",
    "ERROR",
    "WARNING",
    "INFO",
    "DEBUG"
};

static int dsv_log_lvl = DSV_LEVEL_ERROR;

extern void
dsv_set_log_level(int level)
{
    dsv_log_lvl = level;
}

extern int
dsv_get_log_level(void)
{
    return dsv_log_lvl;
}

#ifndef _DSV1_NO_ALLOC_
#define DSV1_ALLOC_FUNC(num_bytes) calloc(1, num_bytes)
#define DSV1_FREE_FUNC(pointer) free(pointer)
#endif

#ifdef _DSV_MEMORY_STATS_
static unsigned allocated = 0;
static unsigned freed = 0;
static unsigned allocated_bytes = 0;
static unsigned freed_bytes = 0;

extern void *
dsv_alloc(int size)
{
    void *p;

    p = DSV1_ALLOC_FUNC(size + 16);
    *((int32_t *) p) = size;
    allocated++;
    allocated_bytes += size;
    return (uint8_t *) p + 16;
}

extern void
dsv_free(void *ptr)
{
    uint8_t *p;
    freed++;
    p = ((uint8_t *) ptr) - 16;
    freed_bytes += *((int32_t *) p);
    DSV1_FREE_FUNC(p);
}

extern void
dsv_memory_report(void)
{
    DSV_DEBUG(("n alloc: %u", allocated));
    DSV_DEBUG(("n freed: %u", freed));
    DSV_DEBUG(("alloc bytes: %u", allocated_bytes));
    DSV_DEBUG(("freed bytes: %u", freed_bytes));
    DSV_DEBUG(("bytes not freed: %d", allocated_bytes - freed_bytes));
}
#else
extern void *
dsv_alloc(int size)
{
    return DSV1_ALLOC_FUNC(size);
}

extern void
dsv_free(void *ptr)
{
    DSV1_FREE_FUNC(ptr);
}

extern void
dsv_memory_report(void)
{
    DSV_DEBUG(("memory stats are disabled"));
}
#endif

#ifndef _DSV1_NO_STDIO_

extern int
dsv_yuv_write(FILE *out, int fno, DSV_PLANE *p)
{
    unsigned lens[3];
    unsigned offset, framesz;
    int c, y;
    
    if (out == NULL) {
        return -1;
    }
    if (fno < 0) {
        return -1;
    }
    lens[0] = p[0].w * p[0].h;
    lens[1] = p[1].w * p[1].h;
    lens[2] = p[2].w * p[2].h;
    framesz = lens[0] + lens[1] + lens[2];
    offset = fno * framesz;

    if (fseek(out, offset, SEEK_SET)) {
        return -1;
    }
    for (c = 0; c < 3; c++) {
        for (y = 0; y < p[c].h; y++) {
            uint8_t *line = DSV_GET_LINE(&p[c], y);
            if (fwrite(line, p[c].w, 1, out) != 1) {
                return -1;
            }
        }
    }
    return 0;
}

extern int
dsv_yuv_read(FILE *in, int fno, uint8_t *o, int width, int height, int subsamp)
{
    unsigned npix, offset, chrsz = 0;
    unsigned offsets[3];
    
    if (in == NULL) {
        return -1;
    }
    if (fno < 0) {
        return -1;
    }
    npix = width * height;
    offsets[0] = 0;
    offsets[1] = npix;
    switch (subsamp) {
        case DSV_SUBSAMP_444:
            offset = fno * npix * 3;
            chrsz = npix;
            break;
        case DSV_SUBSAMP_422:
            offset = fno * npix * 2;
            chrsz = (width / 2) * height;
            break;
        case DSV_SUBSAMP_420:
        case DSV_SUBSAMP_411:
            offset = fno * npix * 3 / 2;
            chrsz = npix / 4;
            break;
        default:
            DSV_ERROR(("unsupported format"));
            DSV_ASSERT(0);
            break;
    }
    offsets[2] = (npix + chrsz);

    if (fseek(in, offset, SEEK_SET)) {
        return -1;
    }
    if (fread(o, 1, npix + chrsz + chrsz, in) != (npix + chrsz + chrsz)) {
        return -1;
    }
    return 0;
}
#endif

extern void
dsv_buf_free(DSV_BUF *buf)
{
    if (buf->data) {
        dsv_free(buf->data);
        buf->data = NULL;
    }
}

extern void
dsv_mk_buf(DSV_BUF *buf, int size)
{
    memset(buf, 0, sizeof(*buf));
    buf->data = dsv_alloc(size);
    buf->len = size;
}

static DSV_FRAME *
alloc_frame(void)
{
    DSV_FRAME *frame;
    
    frame = dsv_alloc(sizeof(*frame));
    frame->refcount = 1;
    return frame;
}

extern DSV_FRAME *
dsv_mk_frame(int format, int width, int height, int border)
{
    DSV_FRAME *f = alloc_frame();
    int h_shift, v_shift;
    int chroma_width;
    int chroma_height;
    int ext = 0;

    f->format = format;
    f->width = width;
    f->height = height;
    f->border = !!border;
    
    if (f->border) {
        ext = DSV_FRAME_BORDER;
    }
    
    h_shift = DSV_FORMAT_H_SHIFT(format);
    v_shift = DSV_FORMAT_V_SHIFT(format);
    chroma_width = DSV_ROUND_SHIFT(width, h_shift);
    chroma_height = DSV_ROUND_SHIFT(height, v_shift);

    f->planes[0].format = format;
    f->planes[0].w = width;
    f->planes[0].h = height;
    f->planes[0].stride = DSV_ROUND_POW2((width + ext * 2), 4);

    f->planes[0].len = f->planes[0].stride * (f->planes[0].h + ext * 2);
    f->planes[0].hs = 0;
    f->planes[0].vs = 0;
    
    f->planes[1].format = format;
    f->planes[1].w = chroma_width;
    f->planes[1].h = chroma_height;
    f->planes[1].stride = DSV_ROUND_POW2((chroma_width + ext * 2), 4);

    f->planes[1].len = f->planes[1].stride * (f->planes[1].h + ext * 2);
    f->planes[1].hs = h_shift;
    f->planes[1].vs = v_shift;
    
    f->planes[2].format = format;
    f->planes[2].w = chroma_width;
    f->planes[2].h = chroma_height;
    f->planes[2].stride = DSV_ROUND_POW2((chroma_width + ext * 2), 4);

    f->planes[2].len = f->planes[2].stride * (f->planes[2].h + ext * 2);
    f->planes[2].hs = h_shift;
    f->planes[2].vs = v_shift;
    
    f->alloc = dsv_alloc(f->planes[0].len + f->planes[1].len + f->planes[2].len);
    
    f->planes[0].data = f->alloc + f->planes[0].stride * ext + ext;
    f->planes[1].data = f->alloc + f->planes[0].len + f->planes[1].stride * ext + ext;
    f->planes[2].data = f->alloc + f->planes[0].len + f->planes[1].len + f->planes[2].stride * ext + ext;
    
    return f;
}

extern DSV_FRAME *
dsv_load_planar_frame(int format, void *data, int width, int height)
{    
    DSV_FRAME *f = alloc_frame();
    int hs = 0, vs = 0;
    f->format = format;
    hs = DSV_FORMAT_H_SHIFT(format);
    vs = DSV_FORMAT_V_SHIFT(format);
    
    f->width = width;
    f->height = height;
    
    f->planes[0].format = f->format;
    f->planes[0].w = width;
    f->planes[0].h = height;
    f->planes[0].stride = width;
    f->planes[0].data = data;
    f->planes[0].len = f->planes[0].stride * f->planes[0].h;
    f->planes[0].hs = 0;
    f->planes[0].vs = 0;
    
    width = DSV_ROUND_SHIFT(width, hs);
    height = DSV_ROUND_SHIFT(height, vs);
    
    f->planes[1].format = f->format;
    f->planes[1].w = width;
    f->planes[1].h = height;
    f->planes[1].stride = f->planes[1].w;
    f->planes[1].len = f->planes[1].stride * f->planes[1].h;
    f->planes[1].data = f->planes[0].data + f->planes[0].len;
    f->planes[1].hs = hs;
    f->planes[1].vs = vs;
    
    f->planes[2].format = f->format;
    f->planes[2].w = width;
    f->planes[2].h = height;
    f->planes[2].stride = f->planes[2].w;
    f->planes[2].len = f->planes[2].stride * f->planes[2].h;
    f->planes[2].data = f->planes[1].data + f->planes[1].len;
    f->planes[2].hs = hs;
    f->planes[2].vs = vs;
    return f;
}

extern DSV_FRAME *
dsv_clone_frame(DSV_FRAME *s, int border)
{
    DSV_FRAME *d;
    
    d = dsv_mk_frame(s->format, s->width, s->height, border);
    dsv_frame_copy(d, s);
    dsv_extend_frame(d);
    return d;
}

extern DSV_FRAME *
dsv_frame_ref_inc(DSV_FRAME *frame)
{
    DSV_ASSERT(frame && frame->refcount > 0);
    frame->refcount++;
    return frame;
}

extern void
dsv_frame_ref_dec(DSV_FRAME *frame)
{    
    DSV_ASSERT(frame && frame->refcount > 0);
    
    frame->refcount--;
    if (frame->refcount == 0) {
        if (frame->alloc) {
            dsv_free(frame->alloc);
        }
        dsv_free(frame);
    }
}

extern void
dsv_frame_copy(DSV_FRAME *dst, DSV_FRAME *src)
{
    int i, c;

    for (c = 0; c < 3; c++) {
        DSV_PLANE *cs, *cd;
        uint8_t *sp, *dp;
        
        cs = src->planes + c;
        cd = dst->planes + c;
        sp = cs->data;
        dp = cd->data;
        for (i = 0; i < dst->planes[c].h; i++) {
            memcpy(dp, sp, src->planes[c].w);
            sp += cs->stride;
            dp += cd->stride;
        }
    }
    if (dst->border) {
        dsv_extend_frame(dst);
    }
}

extern DSV_FRAME *
dsv_extend_frame(DSV_FRAME *frame)
{
    int j, k;
    
    if (!frame->border) {
        return frame;
    }

    for (k = 0; k < 3; k++) {
        DSV_PLANE *c = frame->planes + k;
        int width = c->w;
        int height = c->h;
        int total_w = width + DSV_FRAME_BORDER * 2;
        uint8_t *src, *dst, *line;
        
        for (j = 0; j < c->h; j++) {
            line = DSV_GET_LINE(c, j);
            memset(line - DSV_FRAME_BORDER, line[0], DSV_FRAME_BORDER);
            memset(line + width - 1, line[width - 1], DSV_FRAME_BORDER + 1);
        }
        
        for (j = 0; j < DSV_FRAME_BORDER; j++) {
            dst = DSV_GET_XY(c, -DSV_FRAME_BORDER, -j - 1);
            src = DSV_GET_XY(c, -DSV_FRAME_BORDER, 0);
            memcpy(dst, src, total_w);
            dst = DSV_GET_XY(c, -DSV_FRAME_BORDER, height + j);
            src = DSV_GET_XY(c, -DSV_FRAME_BORDER, height - 1);
            memcpy(dst, src, total_w);
        }
    }
    return frame;
}

static int
pred(int left, int top, int topleft) 
{
    int dif = left + top - topleft;
    if (abs(dif - left) < abs(dif - top)) {
        return left;
    }
    return top;
}

/* B.2.3.2 Motion Data - Motion Vector Prediction */
static void
dsv_movec_pred(DSV_MV *vecs, DSV_PARAMS *p, int x, int y, int *px, int *py)
{
    DSV_MV *mv;
    int vx[3] = { 0, 0, 0 };
    int vy[3] = { 0, 0, 0 };
    
    if (x > 0) { /* left */
        mv = (vecs + y * p->nblocks_h + (x - 1));
        if (mv->mode == DSV_MODE_INTER) {
            vx[0] = mv->u.mv.x;
            vy[0] = mv->u.mv.y;
        }
    }
    if (y > 0) { /* top */
        mv = (vecs + (y - 1) * p->nblocks_h + x);
        if (mv->mode == DSV_MODE_INTER) {
            vx[1] = mv->u.mv.x;
            vy[1] = mv->u.mv.y;
        }
    }
    if (x > 0 && y > 0) { /* top-left */
        mv = (vecs + (y - 1) * p->nblocks_h + (x - 1));
        if (mv->mode == DSV_MODE_INTER) {
            vx[2] = mv->u.mv.x;
            vy[2] = mv->u.mv.y;
        }
    }
    
    *px = pred(vx[0], vx[1], vx[2]);
    *py = pred(vy[0], vy[1], vy[2]);
}

/* B. Bitstream */

static void
dsv_bs_init(DSV_BS *s, uint8_t *buffer)
{
    s->start = buffer;
    s->pos = 0;
}

static void
dsv_bs_align(DSV_BS *s)
{
    if (dsv_bs_aligned(s)) {
        return; /* already aligned */
    }
    s->pos = ((s->pos + 7) & ((unsigned) (~0) << 3)); /* byte align */
}

/* B. Encoding Format: Zero Bit Run-Length Encoding (ZBRLE) */
static void
dsv_bs_init_rle(DSV_ZBRLE *rle, unsigned char *buf)
{
    memset(rle, 0, sizeof(*rle));
    dsv_bs_init(&rle->bs, buf);
}

#define SBT_LVL_TEST (isI ? 1 : lvl > 1)

/* Subband Transforms
 * 
 * Biorthogonal 4-tap filter used for highest frequency level in intra frames
 * Haar used for everything else
 */

static DSV_SBC *sbc_temp_buf = NULL;
static int sbc_temp_bufsz = 0;

static void
alloc_temp(int size)
{
    if (sbc_temp_bufsz < size) {
        sbc_temp_bufsz = size;
        
        if (sbc_temp_buf) {
            dsv_free(sbc_temp_buf);
            sbc_temp_buf = NULL;
        }

        sbc_temp_buf = dsv_alloc(sbc_temp_bufsz * sizeof(DSV_SBC));
        if (sbc_temp_buf == NULL) {
            DSV_ERROR(("out of memory"));
        }
    }
}

static void
cpysub(DSV_SBC *dst, DSV_SBC *src, unsigned w, unsigned h, unsigned stride)
{
    w *= sizeof(DSV_SBC);
    while (h-- > 0) {
        memcpy(dst, src, w);
        src += stride;
        dst += stride;
    }
}

/* C.3 Rounding Divisions */
static int
round2(int v)
{
    if (v < 0) {
        return -(((-v) + 1) >> 1);
    }
    return (v + 1) >> 1;
}

static int
dsv_lb2(unsigned n)
{
    unsigned i = 1, log2 = 0;

    while (i < n) {
        i <<= 1;
        log2++;
    }
    return log2;
}

/* C.3.3 Subband Recomposition - num_levels */
static int
nlevels(int w, int h)
{
    int lb2, mx;
    
    mx = (w > h) ? w : h;
    lb2 = dsv_lb2(mx);
    if (mx > (1 << lb2)) {
        lb2++;
    }
    return lb2;
}

#define EOP_SYMBOL 0x55 /* B.2.3.3 Image Data - Coefficient Coding */

#define CHROMA_LIMIT 512  /* C.2 Dequantization - chroma limit */

#define NSUBBAND 4 /* 0 = LL, 1 = LH, 2 = HL, 3 = HH */

#define MINQUANT 16  /* C.2 MINQUANT */

/* C.1 Subband Order and Traversal */
static int
subband(int level, int sub, int w, int h)
{
    int offset = 0;
    if (sub & 1) { /* L */
        offset += DSV_ROUND_SHIFT(w, DSV_MAXLVL - level);
    }
    if (sub & 2) { /* H */
        offset += DSV_ROUND_SHIFT(h, DSV_MAXLVL - level) * w;
    }
    return offset;
}

/* C.1 Subband Order and Traversal */
static int
dimat(int level, int v) /* dimension at level */
{
    return DSV_ROUND_SHIFT(v, DSV_MAXLVL - level);
}

static int
fix_quant(int q, DSV_STABILITY *stab)
{
    if (stab->cur_plane > 0 && q > CHROMA_LIMIT) {
        q = CHROMA_LIMIT;
    }
    return q;
}

#define BLOCK_P     14
#define IS_STABLE   1
#define IS_INTRA    2

/* C.2.4 Higher Level Subband Dequantization - TMQ_for_position */
static int
tmq4pos(int q, int stable)
{
    if (stable & IS_INTRA) {
        return q >> 2;
    }
    if (stable) {
        return q >> 1;
    }
    return q;
}

/* C.2.2 Quantization Parameter Derivation - get_quant_lower_frequency */
static int
dsv_get_quant(int q, int isP, int level)
{
    if (isP) {
        q = (q * 3) / 2; /* compensate for B4T creating different LL */
    }
    if (level == 1) {
        q = (q * 2) / 3;
    } else if (level == 2) {
        q = (q * 3) / 2;
    }
    if (q < MINQUANT) {
        q = MINQUANT;
    }
    return q;
}

/* C.2.1 Dequantization Functions - dequantize_lower_frequency */
static DSV_SBC
dequant(int v, int q)
{    
    if (v < 0) {
        return -((-v * (q << 1) + q) >> 1);
    }
    return (v * (q << 1) + q) >> 1;
}

/* C.2.1 dequantize_highest_frequency */
static DSV_SBC
dequantH(int v, unsigned q)
{
    return (v << q);
}

static uint8_t
clamp_u8(int v)
{
    return v > 255 ? 255 : v < 0 ? 0 : v;
}

/* D.1.1 Luma Half-Pixel Filter */
static int
hpfh(uint8_t *p)
{
    return DSV_HP_COEF * (p[0] + p[1]) - (p[-1] + p[2]);
}
static int
hpfv(uint8_t *p, int s)
{
    return DSV_HP_COEF * (p[0] + p[s]) - (p[-s] + p[2 * s]);
}

/* D.1.2 Chroma Half-Pixel Filter */
static void
hpelC(uint8_t *dec, uint8_t *ref, int xh, int yh, int dw, int rw, int w, int h)
{
    int i, j;
    uint8_t *refx, *refy, *refxy;
    
    switch ((xh << 1) | yh) {
        case 0:
            for (j = 0; j < h; j++) {
                memcpy(dec, ref, w);
                ref += rw;
                dec += dw;
            }
            break;
        case 1:
            refy = ref + rw;
            for (j = 0; j < h; j++) {
                for (i = 0; i < w; i++) {
                    dec[i] = (ref[i] + refy[i] + 1) >> 1;
                }
                ref += rw;
                refy += rw;
                dec += dw;
            }
            break;
        case 2:
            refx = ref + 1;
            for (j = 0; j < h; j++) {
                for (i = 0; i < w; i++) {
                    dec[i] = (ref[i] + refx[i] + 1) >> 1;
                }
                ref += rw;
                refx += rw;
                dec += dw;
            }
            break;
        case 3:
            refx = ref + 1;
            refy = ref + rw;
            refxy = refy + 1;
            for (j = 0; j < h; j++) {
                for (i = 0; i < w; i++) {
                    dec[i] = (ref[i] + refx[i] + refy[i] + refxy[i] + 2) >> 2;
                }
                ref += rw;
                refx += rw;
                refy += rw;
                refxy += rw;
                dec += dw;
            }
            break;
    }
}

static void
hpelL(uint8_t *dec, uint8_t *ref, int xh, int yh, int dw, int rw, int w, int h)
{
    static int16_t buf[(DSV_MAX_BLOCK_SIZE + 16) * (DSV_MAX_BLOCK_SIZE + 16)];
    int x, y, i, c;
    
    switch ((xh << 1) | yh) {
        case 0:
            for (y = 0; y < h; y++) {
                memcpy(dec, ref, w);
                ref += rw;
                dec += dw;
            }
            break;
        case 1:
            for (y = 0; y < h; y++) {
                for (x = 0; x < w; x++) {
                    dec[x] = clamp_u8((hpfv(ref + x, rw) + 8) >> 4);
                }
                ref += rw;
                dec += dw;
            }
            break;
        case 2:
            for (y = 0; y < h; y++) {
                for (x = 0; x < w; x++) {
                    dec[x] = clamp_u8((hpfh(ref + x) + 8) >> 4);
                }
                ref += rw;
                dec += dw;
            }
            break;
        case 3:
            for (y = 0; y < h + 4; y++) {
                for (x = 0; x < w; x++) {
                    buf[y * w + x] = hpfh(ref + (y - 1) * rw + x);
                }
            }
            for (y = 0; y < h; y++) {
                for (x = 0; x < w; x++) {
                    i = y * w + x;
                    c = DSV_HP_COEF * (buf[i + 1 * w] + buf[i + 2 * w])
                                    - (buf[i + 0 * w] + buf[i + 3 * w]);
                                        
                    dec[x] = clamp_u8((c + 128) >> 8);
                }
                dec += dw;
            }
            break;
    }
}

static int
avgval(uint8_t *dec, int dw, int w, int h)
{
    int i, j;
    int avg = 0;
    
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            avg += dec[i];
        }
        dec += dw;
    }
    return avg / (w * h);
}

/* copy directly from previous reference block */
static void
cpyzero(uint8_t *dec, uint8_t *ref, int dw, int rw, int w, int h)
{
    int j;
    
    for (j = 0; j < h; j++) {
        memcpy(dec, ref, w);
        ref += rw;
        dec += dw;
    }
}

static void
compensate(DSV_MV *vecs, DSV_PARAMS *p, int c, DSV_FRAME *ref, DSV_PLANE *dp)
{
    int i, j, r, x, y, dx, dy, px, py, bw, bh, cw, ch, sh, sv, limx, limy;
    DSV_PLANE *rp;
    DSV_MV *mv;

    if (c == 0) {
        sh = 0;
        sv = 0;
    } else {
        sh = DSV_FORMAT_H_SHIFT(p->vidmeta->subsamp);
        sv = DSV_FORMAT_V_SHIFT(p->vidmeta->subsamp);
    }
    bw = p->blk_w >> sh;
    bh = p->blk_h >> sv;
    
    limx = (dp->w - bw) + DSV_FRAME_BORDER - 1;
    limy = (dp->h - bh) + DSV_FRAME_BORDER - 1;
    
    rp = ref->planes + c;
    
    for (j = 0; j < p->nblocks_v; j++) {
        y = j * bh;
        ch = bh;
        if (y + bh >= dp->h) {
            ch = dp->h - y;
        }
        for (i = 0; i < p->nblocks_h; i++) {
            x = i * bw;
            cw = bw;
            if (x + bw >= dp->w) {
                cw = dp->w - x;
            }
            
            mv = &vecs[i + j * p->nblocks_h];
            
            if (mv->mode == DSV_MODE_INTER) {
                /* D.1 Compensating Inter Blocks */
                dx = mv->u.mv.x >> sh;
                dy = mv->u.mv.y >> sv;
                
                px = x + (dx >> 1);
                py = y + (dy >> 1);
                px = CLAMP(px, -DSV_FRAME_BORDER, limx);
                py = CLAMP(py, -DSV_FRAME_BORDER, limy);
                /* different hpel filter for luma */
                (c == 0 ? hpelL : hpelC)
                       (DSV_GET_XY(dp, x, y),
                        DSV_GET_XY(rp, px, py),
                        dx & 1, dy & 1,
                        dp->stride, rp->stride, cw, ch);
            } else { /* intra */
                /* D.2 Compensating Intra Blocks */
                uint8_t *dec;
                int avgc;
                
                if (mv->submask == DSV_MASK_ALL_INTRA) {
                    avgc = avgval(DSV_GET_XY(rp, x, y), rp->stride, cw, ch);
                    dec = DSV_GET_XY(dp, x, y);
                    for (r = 0; r < ch; r++) {
                        memset(dec, avgc, cw);
                        dec += dp->stride;
                    }
                } else {
                    int f, g, sbx, sby, sbw, sbh, mask_index;
                    uint8_t masks[4] = {
                        DSV_MASK_INTRA00,
                        DSV_MASK_INTRA01,
                        DSV_MASK_INTRA10,
                        DSV_MASK_INTRA11,
                    };
                    sbw = cw / 2;
                    sbh = ch / 2;
                    mask_index = 0;
                    for (g = 0; g <= sbh; g += sbh) {
                        for (f = 0; f <= sbw; f += sbw) {
                            sbx = x + f;
                            sby = y + g;
                            if (mv->submask & masks[mask_index]) {
                                avgc = avgval(DSV_GET_XY(rp, sbx, sby), rp->stride, sbw, sbh);
                                dec = DSV_GET_XY(dp, sbx, sby);
                                for (r = 0; r < sbh; r++) {
                                    memset(dec, avgc, sbw);
                                    dec += dp->stride;
                                }
                            } else {
                                cpyzero(DSV_GET_XY(dp, sbx, sby),
                                        DSV_GET_XY(rp, sbx, sby),
                                        dp->stride, rp->stride, sbw, sbh);
                            }
                            mask_index++;
                        }
                    }
                }
            }
        }
    }
}

/* NOTE: intentionally disregarding the expanded dynamic range,
 * the intra block test defined in subsection D.3 of the specification
 * should serve to prevent the artifacting from this limitation
 */
static void
addf(uint8_t *out, int os, uint8_t *dif, int ds, int w, int h)
{
    int x, y;

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            out[x] = clamp_u8((out[x] + dif[x]) - 128); /* add and adjust */
        }
        out += os;
        dif += ds;
    }
}

/* C.3.1.1 LL Coefficient Scaling */
#define SBT_INV_SCALE(x) ((x) * 5 / 4)

static int
round4(int v)
{
    if (v < 0) {
        return -(((-v) + 2) >> 2);
    }
    return (v + 2) >> 2;
}

static int
round8(int v)
{
    if (v < 0) {
        return -(((-v) + 4) >> 3);
    }
    return (v + 4) >> 3;
}

/* C.3.2.2 Inverse B4T */
static void
inv_b4t_h(DSV_SBC *out, DSV_SBC *in, int n)
{
    int i;
    int L0, L1, H0, H1, L3, H3;
    /* left edge */
    L0 = in[0 + ((0 + 0) >> 1) * 1];
    H0 = in[0 + ((0 + n) >> 1) * 1];
    L1 = L0;
    H1 = H0;
    L3 = L1 * 3;
    H3 = H1 * 3;
    out[0] = round8(L0 + L3 + H0 - H3);
    L0 = L1;
    H0 = H1;
    L1 = in[0 + ((1 + 1) >> 1) * 1];
    H1 = in[0 + ((1 + 1 + n) >> 1) * 1];
    out[1] = round8(L3 + L1 + H3 - H1);

    for (i = 1; i < n - 4; i += 2) {
        L3 = L1 * 3;
        H3 = H1 * 3;
        out[1 + (i + 0) * 1] = round8(L0 + L3 + H0 - H3);
        L0 = L1;
        H0 = H1;
        L1 = in[1 + ((i + 1) >> 1) * 1];
        H1 = in[1 + ((i + 1 + n) >> 1) * 1];
        out[1 + (i + 1) * 1] = round8(L3 + L1 + H3 - H1);
    }
    /* right edge */
    L3 = L1 * 3;
    H3 = H1 * 3;
    out[1 + (i + 0) * 1] = round8(L0 + L3 + H0 - H3);
    out[1 + (i + 1) * 1] = round8(L3 + L1 + H3 - H1);
}

/* C.3.2.2 Inverse B4T */
static void
inv_b4t_v(DSV_SBC *out, DSV_SBC *in, int n, int s)
{
    int i;
    int L0, L1, H0, H1, L3, H3;
    /* top */
    L0 = in[0 + ((0 + 0) >> 1) * s];
    H0 = in[0 + ((0 + n) >> 1) * s];
    L1 = L0;
    H1 = H0;
    L3 = L1 * 3;
    H3 = H1 * 3;
    out[0] = round8(L0 + L3 + H0 - H3);
    L0 = L1;
    H0 = H1;
    L1 = in[0 + ((1 + 1) >> 1) * s];
    H1 = in[0 + ((1 + 1 + n) >> 1) * s];
    out[s] = round8(L3 + L1 + H3 - H1);

    for (i = 1; i < n - 4; i += 2) {
        L3 = L1 * 3;
        H3 = H1 * 3;
        out[s + (i + 0) * s] = round8(L0 + L3 + H0 - H3);
        L0 = L1;
        H0 = H1;
        L1 = in[s + ((i + 1) >> 1) * s];
        H1 = in[s + ((i + 1 + n) >> 1) * s];
        out[s + (i + 1) * s] = round8(L3 + L1 + H3 - H1);
    }
    /* bottom */
    L3 = L1 * 3;
    H3 = H1 * 3;
    out[s + (i + 0) * s] = round8(L0 + L3 + H0 - H3);
    out[s + (i + 1) * s] = round8(L3 + L1 + H3 - H1);
}

static void
inv_b4t_2d(DSV_SBC *tmp, DSV_SBC *in, int w, int h)
{
    int i, j;

    for (i = 0; i < w; i++) {
        inv_b4t_v(tmp + i, in + i, h, w);
    }

    for (j = 0; j < h; j++) {
        inv_b4t_h(in + w * j, tmp + w * j, w);
    }
}

/* C.3.1.3 Haar Simple Inverse Transform */
static void
inv_haar_s(DSV_SBC *src, DSV_SBC *dst, int width, int height, int lvl, int isI)
{
    int x, y, woff, hoff, ws, hs, oddw, oddh;
    int LL, LH, HL, HH;
    int idx;
    DSV_SBC *os, *od, *spLL, *spLH, *spHL, *spHH;
    
    os = src;
    od = dst;
    
    woff = DSV_ROUND_SHIFT(width, lvl);
    hoff = DSV_ROUND_SHIFT(height, lvl);

    ws = DSV_ROUND_SHIFT(width, lvl - 1);
    hs = DSV_ROUND_SHIFT(height, lvl - 1);
    oddw = (ws & 1);
    oddh = (hs & 1);
    
    spLL = src;
    spLH = src + woff;
    spHL = src + hoff * width;
    spHH = src + woff + hoff * width;
    for (y = 0; y < hs - oddh; y += 2) {
        DSV_SBC *dpA, *dpB;
        
        dpA = dst + (y + 0) * width;
        dpB = dst + (y + 1) * width;
        for (x = 0, idx = 0; x < ws - oddw; x += 2, idx++) {
            if (SBT_LVL_TEST) {
                LL = SBT_INV_SCALE(spLL[idx]);
            } else {
                LL = spLL[idx];
            }
            LH = spLH[idx];
            HL = spHL[idx];
            HH = spHH[idx];
            
            dpA[x + 0] = (LL + LH + HL + HH) / 4; /* LL */
            dpA[x + 1] = (LL - LH + HL - HH) / 4; /* LH */
            dpB[x + 0] = (LL + LH - HL - HH) / 4; /* HL */
            dpB[x + 1] = (LL - LH - HL + HH) / 4; /* HH */
        }
        if (oddw) {
            if (SBT_LVL_TEST) {
                LL = SBT_INV_SCALE(spLL[idx]);
            } else {
                LL = spLL[idx];
            }
            HL = spHL[idx];
            
            dpA[x + 0] = (LL + HL) / 4; /* LL */
            dpB[x + 0] = (LL - HL) / 4; /* HL */
        }
        spLL += width;
        spLH += width;
        spHL += width;
        spHH += width;
    }
    if (oddh) {
        DSV_SBC *dpA = dst + (y + 0) * width;
        for (x = 0, idx = 0; x < ws - oddw; x += 2, idx++) {
            if (SBT_LVL_TEST) {
                LL = SBT_INV_SCALE(spLL[idx]);
            } else {
                LL = spLL[idx];
            }
            LH = spLH[idx];
            
            dpA[x + 0] = (LL + LH) / 4; /* LL */
            dpA[x + 1] = (LL - LH) / 4; /* LH */
        }
        if (oddw) {
            if (SBT_LVL_TEST) {
                LL = SBT_INV_SCALE(spLL[idx]);
            } else {
                LL = spLL[idx];
            }
            
            dpA[x + 0] = (LL / 4); /* LL */
        }
    }
    cpysub(os, od, ws, hs, width);
}

/* C.3.1.4 Haar Filtered Inverse Transform */
static void
inv_haar(DSV_SBC *src, DSV_SBC *dst, int width, int height, int lvl, int hqp, int isI)
{
    int x, y, woff, hoff, ws, hs, oddw, oddh;
    int LL, LH, HL, HH;
    int idx;
    DSV_SBC *os, *od, *spLL, *spLH, *spHL, *spHH;
    
    os = src;
    od = dst;
    
    woff = DSV_ROUND_SHIFT(width, lvl);
    hoff = DSV_ROUND_SHIFT(height, lvl);

    ws = DSV_ROUND_SHIFT(width, lvl - 1);
    hs = DSV_ROUND_SHIFT(height, lvl - 1);
    oddw = (ws & 1);
    oddh = (hs & 1);
    
    spLL = src;
    spLH = src + woff;
    spHL = src + hoff * width;
    spHH = src + woff + hoff * width;
    for (y = 0; y < hs - oddh; y += 2) {
        DSV_SBC *dpA, *dpB;
        int inY = y > 0 && y < (hs - oddh - 1);
        
        dpA = dst + (y + 0) * width;
        dpB = dst + (y + 1) * width;
        for (x = 0, idx = 0; x < ws - oddw; x += 2, idx++) {
            int inX = x > 0 && x < (ws - oddw - 1);
            int nudge, t, lp, ln, mn, mx;

            if (SBT_LVL_TEST) {
                LL = SBT_INV_SCALE(spLL[idx]);
            } else {
                LL = spLL[idx];
            }
            LH = spLH[idx];
            HL = spHL[idx];
            HH = spHH[idx];

            if (inX) {
                if (SBT_LVL_TEST) {
                    lp = SBT_INV_SCALE(spLL[idx - 1]); /* prev */
                    ln = SBT_INV_SCALE(spLL[idx + 1]); /* next */
                } else {
                    lp = spLL[idx - 1]; /* prev */
                    ln = spLL[idx + 1]; /* next */
                }
                mx = LL - ln; /* find difference between LL values */
                mn = lp - LL;
                if (mn > mx) {
                    t = mn;
                    mn = mx;
                    mx = t;
                }
                mx = MIN(mx, 0); /* must be negative or zero */
                mn = MAX(mn, 0); /* must be positive or zero */
                /* if they are not zero, then there is a potential
                 * consistent smooth gradient between them */
                if (mx != mn) {
                    t = round4(lp - ln);
                    nudge = round2(CLAMP(t, mx, mn) - (LH << 1));
                    LH += CLAMP(nudge, -hqp, hqp); /* nudge LH to smooth it */
                }
            }
            if (inY) { /* do the same as above but in the Y direction */
                if (SBT_LVL_TEST) {
                    lp = SBT_INV_SCALE(spLL[idx - width]);
                    ln = SBT_INV_SCALE(spLL[idx + width]);
                } else {
                    lp = spLL[idx - width];
                    ln = spLL[idx + width];
                }
                mx = LL - ln;
                mn = lp - LL;
                if (mn > mx) {
                    t = mn;
                    mn = mx;
                    mx = t;
                }
                mx = MIN(mx, 0);
                mn = MAX(mn, 0);
                if (mx != mn) {
                    t = round4(lp - ln);
                    nudge = round2(CLAMP(t, mx, mn) - (HL << 1));
                    HL += CLAMP(nudge, -hqp, hqp); /* nudge HL to smooth it */
                }
            }
            
            dpA[x + 0] = (LL + LH + HL + HH) / 4; /* LL */
            dpA[x + 1] = (LL - LH + HL - HH) / 4; /* LH */
            dpB[x + 0] = (LL + LH - HL - HH) / 4; /* HL */
            dpB[x + 1] = (LL - LH - HL + HH) / 4; /* HH */
        }
        if (oddw) {
            if (SBT_LVL_TEST) {
                LL = SBT_INV_SCALE(spLL[idx]);
            } else {
                LL = spLL[idx];
            }
            HL = spHL[idx];
            
            dpA[x + 0] = (LL + HL) / 4; /* LL */
            dpB[x + 0] = (LL - HL) / 4; /* HL */
        }
        spLL += width;
        spLH += width;
        spHL += width;
        spHH += width;
    }
    if (oddh) {
        DSV_SBC *dpA = dst + (y + 0) * width;
        for (x = 0, idx = 0; x < ws - oddw; x += 2, idx++) {
            if (SBT_LVL_TEST) {
                LL = SBT_INV_SCALE(spLL[idx]);
            } else {
                LL = spLL[idx];
            }
            LH = spLH[idx];
            
            dpA[x + 0] = (LL + LH) / 4; /* LL */
            dpA[x + 1] = (LL - LH) / 4; /* LH */
        }
        if (oddw) {
            if (SBT_LVL_TEST) {
                LL = SBT_INV_SCALE(spLL[idx]);
            } else {
                LL = spLL[idx];
            }
            
            dpA[x + 0] = LL / 4; /* LL */
        }
    }
    cpysub(os, od, ws, hs, width);
}

/* C.3.3 Subband Recomposition */
static void
sbc2int(DSV_PLANE *p, DSV_COEFS *dc)
{
    int x, y, w, h;
    DSV_SBC *d;
    DSV_SBC v;
    
    d = dc->data;
    w = p->w;
    h = p->h;

    for (y = 0; y < h; y++) {
        uint8_t *line = DSV_GET_LINE(p, y);
        for (x = 0; x < w; x++) {
            v = (d[x] + 128);
            line[x] = v > 255 ? 255 : v < 0 ? 0 : v;
        }
        d += dc->width;
    }
}

/* C.3.3 Subband Recomposition */
static void
dsv_inv_sbt(DSV_PLANE *dst, DSV_COEFS *src, int q, int isP, int c)
{
    int lvls, i;
    int w = src->width;
    int h = src->height;
    DSV_SBC *temp_buf_pad;

    lvls = nlevels(w, h);

    alloc_temp((w + 2) * (h + 2));

    temp_buf_pad = sbc_temp_buf + w;
    if (c == 0) {
        int llq;
        
        /* C.3.1.4
         * smoothing filter's coefficient nudge bounds.
         * approximately half the quantization value used for the subband.
         * due to the adaptive quantization, we cater to the regions that are
         * stable and smooth those properly since they are more likely to be
         * noticed when improperly filtered.
         */
        llq = dsv_get_quant(q, isP, 0) / 2;
        for (i = lvls; i > 0; i--) {
            /* C.3.1.4 get_HQP */
            int hqp;
            if (i > 3) {
                hqp = llq;
            } else {
                hqp = dsv_get_quant(q, isP, DSV_MAXLVL - i);
                if (i == 1) {
                    hqp = dsv_lb2(hqp);
                    if (isP) {
                        hqp = CLAMP(hqp - DSV_QP_P, 1, 24);
                    } else {
                        hqp = CLAMP(hqp - DSV_QP_I, 1, 24);
                    }
                    hqp = 1 << hqp;
                    hqp >>= 1;
                }
                hqp /= 2;
            }
            if (!isP && i == 1) {
                inv_b4t_2d(temp_buf_pad, src->data, w, h);
            } else {
                inv_haar(src->data, temp_buf_pad, w, h, i, hqp, !isP);
            }
        }
    } else {
        for (i = lvls; i > 0; i--) {
            if (!isP && i == 1) {
                inv_b4t_2d(temp_buf_pad, src->data, w, h);
            } else {
                inv_haar_s(src->data, temp_buf_pad, w, h, i, !isP);
            }
        }
    }

    sbc2int(dst, src);
}

#ifdef _DSV1_INCLUDE_ENCODER_ 
#ifndef _DSV1_ENCODER_IMPL_GUARD_
#define _DSV1_ENCODER_IMPL_GUARD_

/* static versions to possibly make the compiler more likely to inline */
static void
local_put_bit(DSV_BS *s, int v)
{
    if (v) {
        s->start[dsv_bs_ptr(s)] |= 1 << (7 - (s->pos & 7));
    }
    s->pos++;
}

static void
local_put_one(DSV_BS *s)
{
    s->start[dsv_bs_ptr(s)] |= 1 << (7 - (s->pos & 7));
    s->pos++;
}

static void
local_put_bits(DSV_BS *s, unsigned n, unsigned v)
{
    unsigned rem, bit;
    uint8_t data;
    
    while (n > 0) {
        rem = 8 - (s->pos & 7);
        rem = MIN(n, rem);
        bit = (7 - (s->pos & 7)) - rem + 1;
        data = (v >> (n - rem)) & ((1 << rem) - 1);
        s->start[dsv_bs_ptr(s)] |= data << bit;
        n -= rem;
        s->pos += rem;
    }
}

static void
dsv_bs_put_bit(DSV_BS *s, int v)
{
    local_put_bit(s, v);
}

static void
dsv_bs_put_bits(DSV_BS *s, unsigned n, unsigned v)
{
    local_put_bits(s, n, v);
}

/* B. Encoding Type: unsigned interleaved exp-Golomb code (UEG) */
static void
dsv_bs_put_ueg(DSV_BS *s, unsigned v)
{
    int i, n_bits;
    unsigned x;

    v++;
    x = v;
    for (n_bits = -1; x; n_bits++) {
        x >>= 1;
    }
    for (i = 0; i < n_bits; i++) {
        s->pos++; /* equivalent to putting a zero, assuming buffer was clear */
        local_put_bit(s, v & (1 << (n_bits - 1 - i)));
    }
    local_put_one(s);
}

/* B. Encoding Type: signed interleaved exp-Golomb code (SEG) */
static void
dsv_bs_put_seg(DSV_BS *bs, int v)
{
    int s;

    if (v < 0) {
        s = 1;
        v = -v;
    } else {
        s = 0;
    }
    dsv_bs_put_ueg(bs, v);
    if (v) {
        local_put_bit(bs, s);
    }
}

/* B. Encoding Type: non-zero interleaved exp-Golomb code (NEG) */
static void
dsv_bs_put_neg(DSV_BS *bs, int v)
{
    int s;

    if (v < 0) {
        s = 1;
        v = -v;
    } else {
        s = 0;
    }
    dsv_bs_put_ueg(bs, v - 1);
    if (v) {
        local_put_bit(bs, s);
    }
}

static void
dsv_bs_concat(DSV_BS *s, uint8_t *data, int len)
{
    if (!dsv_bs_aligned(s)) {
        DSV_ERROR(("concat to unaligned bs"));
    }
    
    memcpy(s->start + dsv_bs_ptr(s), data, len);
    s->pos += len * 8;
}

/* B. Encoding Format: Zero Bit Run-Length Encoding (ZBRLE) */
static void
dsv_bs_put_rle(DSV_ZBRLE *rle, int b)
{
    if (b) {
        dsv_bs_put_ueg(&rle->bs, rle->nz);
        rle->nz = 0;
        return;
    }
    rle->nz++;
}

static int
dsv_bs_end_rle_w(DSV_ZBRLE *rle)
{   
    dsv_bs_put_ueg(&rle->bs, rle->nz);
    rle->nz = 0;
    dsv_bs_align(&rle->bs);
    return dsv_bs_ptr(&rle->bs);
}

/* C.3.1.1 LL Coefficient Scaling */
#define SBT_FWD_SCALE(x) ((x) * 4 / 5)

/* C.3.2.1 Forward B4T */
static void
fwd_b4t_h(DSV_SBC *out, DSV_SBC *in, int n)
{
    int i;
    int x0, x1, x2, x3, t1, t2;
    /* left edge */
    x0 = in[1 * 1];
    x1 = in[0 * 1];
    x2 = in[1 * 1];
    x3 = in[2 * 1];
    t1 = x1 * 3;
    t2 = x2 * 3;
    out[0 + ((0 + 0) >> 1) * 1] = round2(t1 + t2 - x0 - x3);
    out[0 + ((0 + 0 + n) >> 1) * 1] = round2(x0 - t1 + t2 - x3);
    x0 = x2;
    x1 = x3;

    for (i = 1; i < n - 4; i += 2) {
        x2 = in[1 + (i + 1) * 1];
        x3 = in[1 + (i + 2) * 1];
        t1 = x1 * 3;
        t2 = x2 * 3;
        out[1 + ((i + 0) >> 1) * 1] = round2(t1 + t2 - x0 - x3);
        out[1 + ((i + 0 + n) >> 1) * 1] = round2(x0 - t1 + t2 - x3);
        
        x0 = x2;
        x1 = x3;
    }
    /* right edge */
    x2 = in[1 + (i + 1) * 1];
    x3 = x2;
    t1 = x1 * 3;
    t2 = x2 * 3;
    out[1 + ((i + 0) >> 1) * 1] = round2(t1 + t2 - x0 - x3);
    out[1 + ((i + 0 + n) >> 1) * 1] = round2(x0 - t1 + t2 - x3);
}

/* C.3.2.1 Forward B4T */
static void
fwd_b4t_v(DSV_SBC *out, DSV_SBC *in, int n, int s)
{
    int i;
    int x0, x1, x2, x3, t1, t2;
    /* top */
    x0 = in[1 * s];
    x1 = in[0 * s];
    x2 = in[1 * s];
    x3 = in[2 * s];
    t1 = x1 * 3;
    t2 = x2 * 3;
    out[0 + ((0 + 0) >> 1) * s] = round2(t1 + t2 - x0 - x3);
    out[0 + ((0 + 0 + n) >> 1) * s] = round2(x0 - t1 + t2 - x3);
    x0 = x2;
    x1 = x3;

    for (i = 1; i < n - 4; i += 2) {
        x2 = in[s + (i + 1) * s];
        x3 = in[s + (i + 2) * s];
        t1 = x1 * 3;
        t2 = x2 * 3;
        out[s + ((i + 0) >> 1) * s] = round2(t1 + t2 - x0 - x3);
        out[s + ((i + 0 + n) >> 1) * s] = round2(x0 - t1 + t2 - x3);
        
        x0 = x2;
        x1 = x3;
    }
    /* bottom */
    x2 = in[s + (i + 1) * s];
    x3 = x2;
    t1 = x1 * 3;
    t2 = x2 * 3;
    out[s + ((i + 0) >> 1) * s] = round2(t1 + t2 - x0 - x3);
    out[s + ((i + 0 + n) >> 1) * s] = round2(x0 - t1 + t2 - x3);
}

static void
fwd_b4t_2d(DSV_SBC *tmp, DSV_SBC *in, int w, int h)
{
    int i, j;

    for (j = 0; j < h; j++) {
        fwd_b4t_h(tmp + w * j, in + w * j, w);
    }
    for (i = 0; i < w; i++) {
        fwd_b4t_v(in + i, tmp + i, h, w);
    }
}

/* C.3.1.2 Haar Forward Transform */
static void
fwd(DSV_SBC *src, DSV_SBC *dst, int width, int height, int lvl, int isI)
{
    DSV_SBC *os, *od, *dpLL, *dpLH, *dpHL, *dpHH;
    int x0, x1, x2, x3;
    int x, y, woff, hoff, ws, hs, oddw, oddh;
    int idx;

    woff = DSV_ROUND_SHIFT(width, lvl);
    hoff = DSV_ROUND_SHIFT(height, lvl);
    
    ws = DSV_ROUND_SHIFT(width, lvl - 1);
    hs = DSV_ROUND_SHIFT(height, lvl - 1);
    oddw = (ws & 1);
    oddh = (hs & 1);
    os = src;
    od = dst;
    
    dpLL = dst;
    dpLH = dst + (woff);
    dpHL = dst + (hoff) * width;
    dpHH = dst + (woff) + (hoff) * width;
    for (y = 0; y < hs - oddh; y += 2) {
        DSV_SBC *spA, *spB;
        
        spA = src + (y + 0) * width;
        spB = src + (y + 1) * width;
        for (x = 0, idx = 0; x < ws - oddw; x += 2, idx++) {
            x0 = spA[x + 0];
            x1 = spA[x + 1];
            x2 = spB[x + 0];
            x3 = spB[x + 1];
            if (SBT_LVL_TEST) {
                dpLL[idx] = SBT_FWD_SCALE(x0 + x1 + x2 + x3); /* LL */
            } else {
                dpLL[idx] = (x0 + x1 + x2 + x3); /* LL */
            }
            dpLH[idx] = (x0 - x1 + x2 - x3); /* LH */
            dpHL[idx] = (x0 + x1 - x2 - x3); /* HL */
            dpHH[idx] = (x0 - x1 - x2 + x3); /* HH */      
        }
        if (oddw) {
            x0 = spA[x + 0];
            x2 = spB[x + 0];

            if (SBT_LVL_TEST) {
                dpLL[idx] = SBT_FWD_SCALE(2 * (x0 + x2)); /* LL */
            } else {
                dpLL[idx] = 2 * (x0 + x2); /* LL */
            }
            dpHL[idx] = 2 * (x0 - x2); /* HL */
        }
        dpLL += width;
        dpLH += width;
        dpHL += width;
        dpHH += width;
    }
    if (oddh) {
        DSV_SBC *spA = src + (y + 0) * width;
        for (x = 0, idx = 0; x < ws - oddw; x += 2, idx++) {
            x0 = spA[x + 0];
            x1 = spA[x + 1];
            
            if (SBT_LVL_TEST) {
                dpLL[idx] = SBT_FWD_SCALE(2 * (x0 + x1)); /* LL */
            } else {
                dpLL[idx] = 2 * (x0 + x1); /* LL */
            }
            dpLH[idx] = 2 * (x0 - x1); /* LH */
        }
        if (oddw) {
            x0 = spA[x + 0];
            
            if (SBT_LVL_TEST) {
                dpLL[idx] = SBT_FWD_SCALE(x0 * 4); /* LL */ 
            } else {
                dpLL[idx] = (x0 * 4); /* LL */ 
            }
        }
    }
    cpysub(os, od, ws, hs, width);
}

static void
p2sbc(DSV_COEFS *dc, DSV_PLANE *p)
{
    int x, y, h;
    DSV_SBC *d;
    
    d = dc->data;
    h = p->h;
    for (y = 0; y < h; y++) {
        uint8_t *line = DSV_GET_LINE(p, y);
        for (x = 0; x < dc->width; x++) {
            /* subtract 128 to center plane around zero */
            d[x] = line[x] - 128;
        }
        d += dc->width;
    }
}

static void
dsv_fwd_sbt(DSV_PLANE *src, DSV_COEFS *dst, int isP)
{
    int lvls, i;
    int w = dst->width;
    int h = dst->height;
    DSV_SBC *temp_buf_pad;

    p2sbc(dst, src);
    
    lvls = nlevels(w, h);

    alloc_temp((w + 2) * (h + 2));
    temp_buf_pad = sbc_temp_buf + w;
    for (i = 1; i <= lvls; i++) {
        if (!isP && i == 1) {
            fwd_b4t_2d(temp_buf_pad, dst->data, w, h);
        } else {
            fwd(dst->data, temp_buf_pad, w, h, i, !isP);
        }
    }
}

static int
quant(DSV_SBC v, int q)
{
    if (v == 0) {
        return 0;
    }
    if (v < 0) {
        v = (-v) << 1;
        if (v <= q) {
            return 0;
        }
        return -((v + 1) / (q << 1));
    }
    v <<= 1;
    if (v <= q) {
        return 0;
    }
    return (v + 1) / (q << 1);
}

static int
quantH(DSV_SBC v, unsigned q)
{
    return (v < 0) ? -((-v) >> q) : (v >> q);
}

static void
hzcc_enc(DSV_BS *bs, DSV_SBC *src, int w, int h, int q, DSV_STABILITY *stab)
{
    int x, y, l, s, o, v;
    int sw, sh;
    int bx, by;
    int dbx, dby;
    int qp;
    int run = 0;
    int nruns = 0;
    int stored_v = 0;
    DSV_SBC *srcp;
    int startp, endp;
    
    dsv_bs_align(bs);
    startp = dsv_bs_ptr(bs);
    dsv_bs_put_bits(bs, 32, 0);
    dsv_bs_align(bs);
    
    q = fix_quant(q, stab);

    s = l = 0;
    
    sw = dimat(l, w);
    sh = dimat(l, h);
    qp = dsv_get_quant(q, stab->isP, l);
    
    /* write the 'LL' part */
    o = subband(l, s, w, h);
    src[0] = 0;
    srcp = src + o;
    
    /* C.2.3 LL Subband */
    for (y = 0; y < sh; y++) {
        for (x = 0; x < sw; x++) {
            v = quant(srcp[x], qp);        
            if (v) {
                srcp[x] = dequant(v, qp);   
                dsv_bs_put_ueg(bs, run);
                if (stored_v) {
                    dsv_bs_put_neg(bs, stored_v);
                }
                run = -1;
                nruns++;
                stored_v = v;
            } else {
                srcp[x] = 0;
            }
            run++;
        }
        srcp += w;
    }

    for (l = 0; l < DSV_MAXLVL; l++) {
        unsigned char *blockrow;
        int tmq;
        
        sw = dimat(l, w);
        sh = dimat(l, h);
        dbx = (stab->params->nblocks_h << BLOCK_P) / sw;
        dby = (stab->params->nblocks_v << BLOCK_P) / sh;        
        qp = dsv_get_quant(q, stab->isP, l);
        if (l == (DSV_MAXLVL - 1)) {
            /* C.2.2 Quantization Parameter Derivation
             *     - get_quant_highest_frequency */
            int qp_h;
            qp = dsv_lb2(qp);
            if (!stab->isP) {
                qp_h = CLAMP(qp - DSV_QP_I, 1, 24);
            } else {
                qp_h = CLAMP(qp - DSV_QP_P, 1, 24);
            }
            
            /* C.2.5 Highest Level Subband */
            for (s = 1; s < NSUBBAND; s++) {
                o = subband(l, s, w, h);
                srcp = src + o;
                by = 0;
                for (y = 0; y < sh; y++) {
                    bx = 0;
                    blockrow = stab->stable_blocks + (by >> BLOCK_P) * stab->params->nblocks_h;
                    for (x = 0; x < sw; x++) {
                        tmq = qp;

                        if (blockrow[bx >> BLOCK_P]) {
                            tmq = qp_h; /* keep it very high quality */
                        }
                    
                        v = quantH(srcp[x], tmq);
                        if (v) {
                            srcp[x] = dequantH(v, tmq);
                            dsv_bs_put_ueg(bs, run);
                            if (stored_v) {
                                dsv_bs_put_neg(bs, stored_v);
                            }
                            run = -1;
                            nruns++;
                            stored_v = v;
                        } else {
                            srcp[x] = 0;
                        }
                        run++;
                        bx += dbx;
                    }
                    srcp += w;
                    by += dby;
                }
            }
        } else {
            /* C.2.4 Higher Level Subbands */
            for (s = 1; s < NSUBBAND; s++) {
                o = subband(l, s, w, h);
                srcp = src + o;
                by = 0;
                for (y = 0; y < sh; y++) {
                    bx = 0;
                    blockrow = stab->stable_blocks + (by >> BLOCK_P) * stab->params->nblocks_h;
                    for (x = 0; x < sw; x++) {
                        tmq = tmq4pos(qp, blockrow[bx >> BLOCK_P]);
                        if (tmq < MINQUANT) {
                            tmq = MINQUANT;
                        }
                    
                        v = quant(srcp[x], tmq);         
                        if (v) {
                            srcp[x] = dequant(v, tmq);
                            dsv_bs_put_ueg(bs, run);
                            if (stored_v) {
                                dsv_bs_put_neg(bs, stored_v);
                            }
                            run = -1;
                            nruns++;
                            stored_v = v;
                        } else {
                            srcp[x] = 0;
                        }
                        run++;
                        bx += dbx;
                    }
                    srcp += w;
                    by += dby;
                }
            }
        }
    }

    if (stored_v) {
        dsv_bs_put_neg(bs, stored_v);        
    }

    dsv_bs_align(bs);
    endp = dsv_bs_ptr(bs);
    dsv_bs_set(bs, startp);
    dsv_bs_put_bits(bs, 32, nruns);
    dsv_bs_set(bs, endp);
    dsv_bs_align(bs);
}

extern void
dsv_encode_plane(DSV_BS *bs, DSV_COEFS *src, int q, DSV_STABILITY *stab)
{
    int w = src->width;
    int h = src->height;
    DSV_SBC *d = src->data;
    int LL, startp, endp;
    
    dsv_bs_align(bs);
    startp = dsv_bs_ptr(bs);
    
    dsv_bs_put_bits(bs, 32, 0);

    LL = d[0]; /* save the LL value because we don't want to quantize it */
    dsv_bs_put_seg(bs, LL);
    hzcc_enc(bs, d, w, h, q, stab);
    d[0] = LL; /* restore unquantized LL */

    dsv_bs_put_bits(bs, 8, EOP_SYMBOL); /* 'end of plane' symbol */
    dsv_bs_align(bs);

    endp = dsv_bs_ptr(bs);
    dsv_bs_set(bs, startp);
    dsv_bs_put_bits(bs, 32, (endp - startp) - 4);
    dsv_bs_set(bs, endp);
    dsv_bs_align(bs);
    DSV_INFO(("encoded plane (%dx%d) to %u bytes. quant = %d", src->width, src->height, endp - startp, q));
}

/* Hierarchical Motion Estimation */

typedef struct {
    DSV_PARAMS *params;
    DSV_FRAME *src[DSV_MAX_PYRAMID_LEVELS + 1];
    DSV_FRAME *ref[DSV_MAX_PYRAMID_LEVELS + 1];
    DSV_MV *mvf[DSV_MAX_PYRAMID_LEVELS + 1];
    int levels;
} DSV_HME;

/* HP_SAD_SZ + 2 should be a power of two for performance reasons */
#define HP_SAD_SZ 14
#define HP_DIM    (HP_SAD_SZ + 2)
#define HP_STRIDE (HP_DIM * 2)

#define MAKE_SAD(w) \
static int                                                                    \
sad_ ##w## xh(uint8_t *a, int as, uint8_t *b, int bs, int h)\
{                                                                             \
    int i, j, acc = 0;                                                        \
    for (j = 0; j < h; j++) {                                                 \
        for (i = 0; i < w; i++) {                                             \
            acc += abs(a[i] - b[i]);                                          \
        }                                                                     \
        a += as;                                                              \
        b += bs;                                                              \
    }                                                                         \
    return acc;                                                               \
}

MAKE_SAD(16)
MAKE_SAD(24)
MAKE_SAD(32)
MAKE_SAD(48)
MAKE_SAD(64)

static int
sad_wxh(uint8_t *a, int as, uint8_t *b, int bs, int w, int h)
{
    int i, j, acc = 0;
    
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            acc += abs(a[i] - b[i]);
        }
        a += as;
        b += bs;
    }
    return acc;
}

static int
fastsad(uint8_t *a, int as, uint8_t *b, int bs, int w, int h)
{
    switch (w) {
        case 16:
            return sad_16xh(a, as, b, bs, h);
        case 24:
            return sad_24xh(a, as, b, bs, h);
        case 32:
            return sad_32xh(a, as, b, bs, h);
        case 48:
            return sad_48xh(a, as, b, bs, h);
        case 64:
            return sad_64xh(a, as, b, bs, h);
        default:
            return sad_wxh(a, as, b, bs, w, h);
    }
}

/* this function is intended to 'prove' to the intra decision
 * that the ref block with (0,0) motion does more good than evil */
static int
intra_metric(uint8_t *a, int as, uint8_t *b, int bs, int w, int h)
{
    int i, j;
    unsigned nevil = 0;
    unsigned ngood = 0;
    int prevA, prevB;
    uint8_t *prevptrA = a;
    uint8_t *prevptrB = b;
    for (j = 0; j < h; j++) {
        prevA = a[0];
        prevB = b[0];
        for (i = 0; i < w; i++) {
            int pa = a[i];
            int pb = b[i];
            int dif = abs(pa - pb);
            /* high texture = beneficial to 'good' decision (inter)
             * because intra blocks don't keep high frequency details */
            ngood += abs(pa - prevA);
            ngood += abs(pa - prevptrA[i]);
            ngood += abs(pb - prevB);
            ngood += abs(pb - prevptrB[i]);
            switch (dif) {
                case 0:
                    ngood += 192;
                    break;
                case 1:
                    ngood += 128;
                    break;
                case 2:
                    ngood += 96;
                    break;
                default:
                    nevil += dif;
                    break;
            }
            prevA = pa;
            prevB = pb;
        }
        prevptrA = a;
        prevptrB = b;
        a += as;
        b += bs;
    }
    return ngood >= (((w + h) >> 1) * nevil);
}

static int
invalid_block(DSV_FRAME *f, int x, int y, int sx, int sy)
{
    int b = f->border;
    return x < -b || y < -b || x + sx > f->width + b || y + sy > f->height + b;
}

/* D.3 - Caveat for Encoder
 * simulate reduced range intra BMC to see if this block would
 * not be able to be represented properly as intra
 */
static unsigned
block_intra_test(DSV_PLANE *p, DSV_PLANE *rp, int w, int h)
{
    int i, j, dif, thresh;
    int ravg = 0;
    int nb = 0;
    uint8_t *ref = rp->data;
    uint8_t *dec = p->data;
    
    thresh = 0;
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            ravg += ref[i];
        }
        ref += rp->stride;
    }
    ravg /= (w * h);

    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            dif = clamp_u8((ravg + clamp_u8((dec[i] - ravg) + 128)) - 128);
            if (abs(dif - dec[i]) != 0) {
                nb++;
            }
        }
        if (nb > thresh) {
            return 1;
        }
        dec += p->stride;
    }
    return 0;
}

/* variance, texture, and average for a HP_SAD_SZ x HP_SAD_SZ block */
static unsigned
block_texture(uint8_t *ptr, int stride, int *avg, int *var)
{
    int i, j;
    int px;
    int prev;
    unsigned sh = 0;
    unsigned sv = 0;
    unsigned av = 0, avs = 0;
    uint8_t *prevptr = ptr;
    j = HP_SAD_SZ;
    while (j-- > 0) {
        i = HP_SAD_SZ;
        prev = ptr[i - 1];
        while (i-- > 0) {
            px = ptr[i];
            sh += abs(px - prev);
            sv += abs(px - prevptr[i]);
            av += px;
            avs += px * px;
            prev = px;
        }
        prevptr = ptr;
        ptr += stride;
    }
    sh = (sh + sv) / 2;
    *avg = av / (HP_SAD_SZ * HP_SAD_SZ);
    *var = (avs - (av * av) / (HP_SAD_SZ * HP_SAD_SZ));
    return (sh / (HP_SAD_SZ * HP_SAD_SZ));
}

/* variance and texture */
static unsigned
block_analysis(DSV_PLANE *p, int w, int h, unsigned *texture)
{
    int i, j;
    int prev;
    int px;
    unsigned s = 0, ss = 0;
    unsigned sh = 0;
    unsigned sv = 0;
    uint8_t *ptr;
    uint8_t *prevptr;

    ptr = p->data;
    j = h;
    prevptr = ptr;
    while (j-- > 0) {
        i = w;
        prev = ptr[i - 1];
        while (i-- > 0) {
            px = ptr[i];
            sh += abs(px - prev);
            sv += abs(px - prevptr[i]);
            s += px;
            ss += px * px;
            prev = px;
        }
        prevptr = ptr;
        ptr += p->stride;
    }
    sh = (sh + sv) / 2;
    *texture = (sh / (w * h));
    return (ss - (s * s) / (w * h));
}

static unsigned
y_sqrvar(DSV_PLANE *p, int w, int h)
{
    int i, j;
    unsigned px;
    unsigned s = 0, ss = 0;
    uint8_t *ptr;

    ptr = p->data;
    j = h;
    while (j-- > 0) {
        i = w;
        while (i-- > 0) {
            px = ptr[i];
            s += px;
            ss += px * px;
        }
        ptr += p->stride;
    }
    return (ss - (s * s) / (w * h));
}

static unsigned
c_maxvar(DSV_PLANE *p, int x, int y, int w, int h)
{
    DSV_PLANE *u = &p[1];
    DSV_PLANE *v = &p[2];
    int i, j;
    unsigned px;
    unsigned vu, vv;
    unsigned su = 0, ssu = 0;
    unsigned sv = 0, ssv = 0;
    uint8_t *ptrU, *ptrV;

    ptrU = u->data + x + y * u->stride;
    ptrV = v->data + x + y * v->stride;
    j = h;
    while (j-- > 0) {
        i = w;
        while (i-- > 0) {
            px = ptrU[i];
            su += px;
            ssu += px * px;
            px = ptrV[i];
            sv += px;
            ssv += px * px;
        }
        ptrU += u->stride;
        ptrV += v->stride;
    }
    vu = ssu - (su * su) / (w * h);
    vv = ssv - (sv * sv) / (w * h);
    return MAX(vu, vv);
}

static int
hpsad(uint8_t *a, int as, uint8_t *b)
{
    int i, j, acc = 0;
    for (j = 0; j < HP_SAD_SZ; j++) {
        for (i = 0; i < HP_SAD_SZ; i++) {
            acc += abs(a[i] - b[i << 1]);
        }
        a += as;
        b += HP_STRIDE * 2; /* one more * 2 in order to skip every other row */
    }
    return acc;
}
static void
hpcpy(uint8_t *a, int as, uint8_t *b)
{
    int i, j;
    for (j = 0; j < HP_SAD_SZ; j++) {
        for (i = 0; i < HP_SAD_SZ; i++) {
            a[i] = b[i << 1];
        }
        a += as;
        b += HP_STRIDE * 2; /* one more * 2 in order to skip every other row */
    }
}
static void
fpcpy(uint8_t *a, int as, uint8_t *b, int bs)
{
    int j;
    for (j = 0; j < HP_SAD_SZ; j++) {
        memcpy(a, b, HP_SAD_SZ);
        a += as;
        b += bs;
    }
}

static void
hpel(uint8_t *dec, uint8_t *ref, int rw)
{
    static int16_t buf[DSV_MAX_BLOCK_SIZE * DSV_MAX_BLOCK_SIZE];
    uint8_t *decrow;
    int i, j, c, x;

    for (j = 0; j < HP_DIM + 4; j++) {
        for (i = 0; i < HP_DIM; i++) {
            buf[j * HP_DIM + i] = hpfh(ref + (j - 1) * rw + i);
        }
    }
    for (j = 0; j < HP_DIM; j++) {
        decrow = dec;
        for (i = 0; i < HP_DIM; i++) {
            x = j * HP_DIM + i;
            decrow[HP_STRIDE] = clamp_u8((hpfv(ref + i, rw) + 8) >> 4);
            *decrow++ = ref[i];
            c = DSV_HP_COEF * (buf[x + 1 * HP_DIM] + buf[x + 2 * HP_DIM])
                            - (buf[x + 0 * HP_DIM] + buf[x + 3 * HP_DIM]);
            decrow[HP_STRIDE] = clamp_u8((c + 128) >> 8);
            *decrow++ = clamp_u8((hpfh(ref + i) + 8) >> 4);
        }
        ref += rw;
        dec += HP_STRIDE << 1; /* skip a row */
    }
}

/* fills plane struct at (x, y), no error/bounds checking */
static void
dsv_plane_xy(DSV_FRAME *frame, DSV_PLANE *out, int c, int x, int y)
{
    DSV_PLANE *p = frame->planes + c;
    
    out->format = p->format;
    out->data = DSV_GET_XY(p, x, y);
    out->stride = p->stride;
    out->w = MAX(0, p->w - x);
    out->h = MAX(0, p->h - y);
    out->hs = p->hs;
    out->vs = p->vs;
}

static int
refine_level(DSV_HME *hme, int level)
{
    DSV_FRAME *src, *ref;
    DSV_MV *mv;
    DSV_MV *mf, *parent = NULL;
    DSV_PARAMS *params = hme->params;
    DSV_MV zero;
    int i, j, y_w, y_h, nxb, nyb, step;
    unsigned parent_mask;
    int nintra = 0; /* number of intra blocks */
    DSV_PLANE *sp, *rp;
    int hpel_thresh, nhp, nsk;
    
    y_w = params->blk_w;
    y_h = params->blk_h;
    hpel_thresh = (y_w * y_h);
    nhp = 0;
    nsk = 0;
    
    nxb = params->nblocks_h;
    nyb = params->nblocks_v;

    src = hme->src[level];
    ref = hme->ref[level];
    
    sp = src->planes + 0;
    rp = ref->planes + 0;
    
    hme->mvf[level] = dsv_alloc(sizeof(DSV_MV) * nxb * nyb);

    mf = hme->mvf[level];
    
    if (level < hme->levels) {
        parent = hme->mvf[level + 1];
    }
    
    memset(&zero, 0, sizeof(zero));
    
    step = 1 << level;
    parent_mask = ~((step << 1) - 1);
    
    for (j = 0; j < nyb; j += step) {
        for (i = 0; i < nxb; i += step) {
#define FPEL_NSEARCH 9 /* search points for full-pel search */
            static int xf[FPEL_NSEARCH] = { 0,  1, -1, 0,  0, -1,  1, -1, 1 };
            static int yf[FPEL_NSEARCH] = { 0,  0,  0, 1, -1 - 1, -1,  1, 1 };
#define HPEL_NSEARCH 8 /* search points for half-pel search */
            static int xh[HPEL_NSEARCH] = { 1, -1, 0,  0, -1,  1, -1, 1 };
            static int yh[HPEL_NSEARCH] = { 0,  0, 1, -1, -1, -1,  1, 1 };
            DSV_PLANE srcp;
            DSV_PLANE zerorefp;
            int dx, dy, bestdx, bestdy;
            int best, score;
            int bx, by, bw, bh;
            int k, xx, yy, m, n = 0;
            DSV_MV *inherited[8];
            DSV_MV best_mv = { 0 };
            
            best_mv.mode = DSV_MODE_INTER;

            bx = (i * y_w) >> level;
            by = (j * y_h) >> level;
            
            if ((bx >= src->width) || (by >= src->height)) {
                mf[i + j * nxb] = best_mv; /* bounds check for safety */
                continue;
            }

            dsv_plane_xy(src, &srcp, 0, bx, by);
            dsv_plane_xy(ref, &zerorefp, 0, bx, by);
            bw = MIN(srcp.w, y_w);
            bh = MIN(srcp.h, y_h);
            
            inherited[n++] = &zero;
            if (parent != NULL) {
                static int pt[5 * 2] = { 0, 0,  -2, 0,  2, 0,  0, -2,  0, 2 };
                int x, y, pi, pj;
                
                pi = i & parent_mask;
                pj = j & parent_mask;
                for (m = 0; m < 5; m++) {
                    x = pi + pt[(m << 1) + 0] * step;
                    y = pj + pt[(m << 1) + 1] * step;
                    if (x >= 0 && x < nxb &&
                        y >= 0 && y < nyb) {
                        mv = parent + x + y * nxb;
                        /* we only care about unique non-zero parents */
                        if (mv->u.all) {
                            int exists = 0;
                            for (k = 0; k < n; k++) {
                                if (inherited[k]->u.all == mv->u.all) {
                                    exists = 1;
                                    break;
                                }
                            }
                            if (!exists) {
                                inherited[n++] = mv;
                            }
                        }
                    }
                }
            }
            /* find best inherited vector */ 
            best = n - 1;
            bestdx = inherited[best]->u.mv.x;
            bestdy = inherited[best]->u.mv.y;
            if (n > 1) {
                int best_score = INT_MAX;
                for (k = 0; k < n; k++) {
                    DSV_PLANE refp;

                    if (invalid_block(src, bx, by, bw, bh)) {
                        continue;
                    }
                    
                    dx = inherited[k]->u.mv.x >> level;
                    dy = inherited[k]->u.mv.y >> level;

                    if (invalid_block(ref, bx + dx, by + dy, bw, bh)) {
                        continue;
                    }
                    
                    dsv_plane_xy(ref, &refp, 0, bx + dx, by + dy);
                    score = fastsad(srcp.data, srcp.stride, refp.data, refp.stride, bw, bh);
                    if (best_score > score) {
                        best_score = score;
                        best = k;
                    }
                }
                bestdx = inherited[best]->u.mv.x;
                bestdy = inherited[best]->u.mv.y;
            }
            
            dx = bestdx >> level;
            dy = bestdy >> level;

            mv = &mf[i + j * nxb];
            mv->mode = DSV_MODE_INTER;

            /* full-pel search around inherited best vector */
            dx = CLAMP(dx, -bw - bx, ref->width - bx);
            dy = CLAMP(dy, -bh - by, ref->height - by);
            
            best = INT_MAX;
            xx = bx + dx;
            yy = by + dy;
            
            m = 0;
            for (k = 0; k < FPEL_NSEARCH; k++) {
                score = fastsad(srcp.data, sp->stride,
                        DSV_GET_XY(rp, xx + xf[k], yy + yf[k]),
                        rp->stride, bw, bh);
                if (best > score) {
                    best = score;
                    m = k;
                }
            }
            
            dx += xf[m];
            dy += yf[m];
            
            mv->u.mv.x = dx << level;
            mv->u.mv.y = dy << level;
            
            /* hpel refine at base level */
            if (level == 0) {
                static uint8_t refblock[DSV_MAX_BLOCK_SIZE * DSV_MAX_BLOCK_SIZE];
                unsigned yarea = bw * bh;
                unsigned yareasq = yarea * yarea;
                int has_hp_block = 0;
                
                /* only if prediction is bad enough */
                if (best > hpel_thresh) {
                    static uint8_t tmp[(2 + HP_STRIDE) * (2 + HP_STRIDE)];
                    int best_hp;
                    DSV_PLANE srcp_h;
                    DSV_PLANE refp_h;
                    uint8_t *tmph;
                    
                    /* scale down to match area */
                    best_hp = best * (HP_SAD_SZ * HP_SAD_SZ) / yarea;
                    xx = bx + ((bw >> 1) - (HP_SAD_SZ / 2));
                    yy = by + ((bh >> 1) - (HP_SAD_SZ / 2));
                    dsv_plane_xy(src, &srcp_h, 0, xx, yy);
                    dsv_plane_xy(ref, &refp_h, 0, xx + mv->u.mv.x, yy + mv->u.mv.y);
                    m = -1;
                    hpel(tmp, refp_h.data - 1 - refp_h.stride, refp_h.stride);
                    
                    /* start at (1, 1) */
                    tmph = tmp + 2 + 2 * HP_STRIDE;
                    for (k = 0; k < HPEL_NSEARCH; k++) {
                        score = hpsad(srcp_h.data, srcp_h.stride,
                                tmph + xh[k] + (yh[k] * HP_STRIDE));
                        if (best_hp > score) {
                            best_hp = score;
                            m = k;
                        }
                    }
                    mv->u.mv.x <<= 1;
                    mv->u.mv.y <<= 1;
                    if (m != -1) {
                        mv->u.mv.x += xh[m];
                        mv->u.mv.y += yh[m];
                        hpcpy(refblock, DSV_MAX_BLOCK_SIZE, tmph + xh[m] + (yh[m] * HP_STRIDE));
                        has_hp_block = 1;
                        best = best_hp * yarea / (HP_SAD_SZ * HP_SAD_SZ);
                    }
                    nhp++;
                } else {
                    nsk++;
                    mv->u.mv.x <<= 1;
                    mv->u.mv.y <<= 1;
                }
                if (!has_hp_block) { /* use full pel ref */
                    DSV_PLANE refp;
                    xx = bx + ((bw >> 1) - (HP_SAD_SZ / 2));
                    yy = by + ((bh >> 1) - (HP_SAD_SZ / 2));
                    dsv_plane_xy(ref, &refp, 0, xx + (mv->u.mv.x >> 1), yy + (mv->u.mv.y >> 1));
                    fpcpy(refblock, DSV_MAX_BLOCK_SIZE, refp.data, refp.stride);
                }
                /* intra decision + block metric gathering */ {
                    DSV_PLANE srcp_l;
                    unsigned ubest, luma_tex, luma_var;
                    int src_avg, ref_avg;
                    int src_var, ref_var;
                    int src_tex, ref_tex;
                    DSV_MV *pmv;
                    unsigned thresh_tex = 1;
                    int thresh_var = HP_SAD_SZ * HP_SAD_SZ;
                    
                    xx = bx + ((bw >> 1) - (HP_SAD_SZ / 2));
                    yy = by + ((bh >> 1) - (HP_SAD_SZ / 2));
                    dsv_plane_xy(src, &srcp_l, 0, xx, yy);
                    
                    ubest = best;
                    luma_var = block_analysis(&srcp, bw, bh, &luma_tex);
                    mv->lo_tex = (luma_tex <= 2);
                    mv->lo_var = (luma_var < yareasq);
                    
                    src_tex = block_texture(srcp_l.data, srcp_l.stride, &src_avg, &src_var);
                    ref_tex = block_texture(refblock, DSV_MAX_BLOCK_SIZE, &ref_avg, &ref_var);
                    /* use neighboring blocks to help estimate its detail importance */
                    if (i > 0) {
                        pmv = (mf + j * nxb + (i - 1));
                        if (pmv->mode == DSV_MODE_INTER) {
                            if ((!pmv->lo_tex && !pmv->lo_var)) {
                                thresh_var *= HP_SAD_SZ;
                                thresh_tex++;
                            }
                        }
                    }
                    if (j > 0) {
                        pmv = (mf + (j - 1) * nxb + i);
                        if (pmv->mode == DSV_MODE_INTER) {
                            if ((!pmv->lo_tex && !pmv->lo_var)) {
                                thresh_var *= HP_SAD_SZ;
                                thresh_tex++;
                            }
                        }
                    }
                    if (i > 0 && j > 0) {
                        pmv = (mf + (j - 1) * nxb + (i - 1));
                        if (pmv->mode == DSV_MODE_INTER) {
                            if ((!pmv->lo_tex && !pmv->lo_var)) {
                                thresh_var *= HP_SAD_SZ / 4; /* diagonal is less important */
                                thresh_tex++;
                            }
                        }
                    }
                    mv->high_detail = luma_tex > thresh_tex && src_var > thresh_var;
                    
                    /* using gotos to make it a bit easier to read (for myself) */
#if 1 /* have intra blocks */
                    if (src_tex < 2 && y_sqrvar(&zerorefp, bw, bh) > (luma_var * 2)) {
                        goto intra;
                    }
                    if (ref_var > (src_var * 2)) {
                        goto intra;
                    }
                    if (src_tex == 0 && ref_tex != 0) {
                        goto intra;
                    }
                    if (abs(src_avg - ref_avg) > 8) {
                        goto intra;
                    }
                    if (luma_tex <= 10 && ubest > (yareasq / 16)) {
                        goto intra;
                    }
#if 1 /* chroma check */
                    {
                        int cbx, cby, subsamp;
                        unsigned cbw, cbh, cvarS, cvarR;
                        subsamp = params->vidmeta->subsamp;
                        cbx = i * (y_w >> DSV_FORMAT_H_SHIFT(subsamp));
                        cby = j * (y_h >> DSV_FORMAT_V_SHIFT(subsamp));
                        cbw = bw >> DSV_FORMAT_H_SHIFT(subsamp);
                        cbh = bh >> DSV_FORMAT_V_SHIFT(subsamp);
                        cvarS = c_maxvar(sp, cbx, cby, cbw, cbh);
                        cvarR = c_maxvar(rp, cbx, cby, cbw, cbh);
                        if (cvarR > (4 * cvarS)) {
                            goto intra;
                        }
                    }
#endif
                    goto inter;
intra:
                    if (block_intra_test(&srcp, &zerorefp, bw, bh)) {
                        goto inter;
                    }
                    /* do extra checks for 4 quadrants */
                    mv->submask = DSV_MASK_ALL_INTRA;
                    /* don't give low texture intra blocks the opportunity to cause trouble */
                    if (src_tex > 1) {
                        int f, g, sbw, sbh, mask_index;
                        uint8_t masks[4] = {
                                ~DSV_MASK_INTRA00,
                                ~DSV_MASK_INTRA01,
                                ~DSV_MASK_INTRA10,
                                ~DSV_MASK_INTRA11,
                        };
                        sbw = bw / 2;
                        sbh = bh / 2;
                        mask_index = 0;
                        for (g = 0; g <= sbh; g += sbh) {
                            for (f = 0; f <= sbw; f += sbw) {
                                if (intra_metric(srcp.data + (f + g * srcp.stride), srcp.stride,
                                        zerorefp.data + (f + g * zerorefp.stride), zerorefp.stride, sbw, sbh)) {
                                    /* mark as inter with zero vector */
                                    mv->submask &= masks[mask_index];
                                }
                                mask_index++;
                            }
                        }
                    }
                    if (mv->submask) {
                        mv->mode = DSV_MODE_INTRA;
                        nintra++;
                    }
inter:
                    ;
#endif
                }
            }
        }
    }
    if (level == 0) {
        DSV_DEBUG(("num half pel: %d num skipped: %d", nhp, nsk));
    }
    return nintra;
}

static int
dsv_hme(DSV_HME *hme)
{
    int i = hme->levels;
    int nintra = 0;

    while (i >= 0) {
        nintra = refine_level(hme, i);
        i--;
    }
    return (nintra * 100) / (hme->params->nblocks_h * hme->params->nblocks_v);
}

static void
dsv_frame_add(DSV_FRAME *dst, DSV_FRAME *src)
{
    DSV_PLANE *s, *d;
    int c;
    
    for (c = 0; c < 3; c++) {
        s = src->planes + c;
        d = dst->planes + c;
        
        addf(d->data, d->stride, s->data, s->stride, d->w, d->h);
    }
}

static void
subf(uint8_t *inp, int is, uint8_t *dif, int ds, int w, int h)
{
    int x, y;

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            inp[x] = clamp_u8((inp[x] - dif[x]) + 128); /* sub and adjust */
        }
        inp += is;
        dif += ds;
    }
}

static void
dsv_sub_pred(DSV_MV *mv, DSV_PARAMS *p, DSV_FRAME *dif, DSV_FRAME *inp, DSV_FRAME *ref)
{
    DSV_PLANE *d, *i;
    int c;

    for (c = 0; c < 3; c++) {
        d = dif->planes + c;
        i = inp->planes + c;
        
        compensate(mv, p, c, ref, d);
        subf(i->data, i->stride, d->data, d->stride, d->w, d->h);
    }
}

static void
encdat_ref(DSV_ENCDATA *d)
{
    DSV_ASSERT(d && d->refcount > 0);
    d->refcount++;
}

static void
encdat_unref(DSV_ENCODER *enc, DSV_ENCDATA *d)
{
    int i;
    
    DSV_ASSERT(d && d->refcount > 0);
    d->refcount--;
    
    if (d->refcount != 0) {
        return;
    }
    
    if (d->input_frame) {
        dsv_frame_ref_dec(d->input_frame);
    }
    if (d->padded_frame) {
        dsv_frame_ref_dec(d->padded_frame);
    }
    for (i = 0; i < enc->pyramid_levels; i++) {
        if (d->pyramid[i]) {
            dsv_frame_ref_dec(d->pyramid[i]);
        }
    }
    if (d->recon_frame) {
        dsv_frame_ref_dec(d->recon_frame);
    }
    if (d->xf_frame) {
        dsv_frame_ref_dec(d->xf_frame);
    }
    if (d->residual) {
        dsv_frame_ref_dec(d->residual);
    }
    if (d->refdata) {
        encdat_unref(enc, d->refdata);
        d->refdata = NULL;
    }    
    if (d->final_mvs) {
        dsv_free(d->final_mvs);
        d->final_mvs = NULL;
    }

    dsv_free(d);
}

static void
quality2quant(DSV_ENCODER *enc, DSV_ENCDATA *d, int forced_intra)
{    
    int q;
    
    if (d->params.has_ref) {
        DSV_INFO(("P FRAME!"));
        d->isP = 1;
    } else {
        DSV_INFO(("I FRAME!"));
        d->isP = 0;
    }
    
    q = enc->rc_quant;
    if (enc->rc_mode != DSV_RATE_CONTROL_CRF) {
        DSV_META *vfmt = d->params.vidmeta;
        int fps, bpf, needed_bpf, dir, delta, low_p, minq, nudged = 0;
        
        fps = (vfmt->fps_num << 5) / vfmt->fps_den;
        if (fps == 0) {
            fps = 1;
        }
        /* bpf = bytes per frame */
        needed_bpf = ((enc->bitrate << 5) / fps) >> 3;
        
        bpf = enc->bpf_avg;
        if (bpf == 0) {
            bpf = needed_bpf;
        }
        dir = (bpf - needed_bpf) > 0 ? -1 : 1;

        delta = (abs(bpf - needed_bpf) << 9) / needed_bpf;
        if (dir == 1) {
            delta *= 2;
        }
        if (enc->rc_high_motion_nudge) {
            if (d->isP) {
                if (enc->last_P_frame_over) {
                    delta++;
                    delta *= 2;
                    dir = -1;
                    nudged = 1;
                } else if (enc->back_into_range) {
                    delta++;
                    delta *= 2;
                    dir = 1;
                    nudged = 1;
                }
            } else {
                if (enc->back_into_range) {
                    delta++;
                    delta *= 2;
                    dir = 1;
                    nudged = 1;
                }
            }
        }
        delta = (q * delta >> 9);

        enc->max_q_step = CLAMP(enc->max_q_step, 1, DSV_MAX_QUALITY);
        if (nudged) {
            if (delta > enc->max_q_step * 16) {
                delta = enc->max_q_step * 16; /* limit rate */
            }
        } else {
            if (delta > enc->max_q_step) {
                delta = enc->max_q_step; /* limit rate */
            }
        }

        delta *= dir;
        
        q += delta;
        low_p = enc->avg_P_frame_q - DSV_QUALITY_PERCENT(4);
        low_p = CLAMP(low_p, enc->min_quality, enc->max_quality);
        minq = d->isP ? low_p : enc->min_I_frame_quality;
        if (forced_intra) {
            if (q < DSV_QUALITY_PERCENT(60)) {
                q += DSV_QUALITY_PERCENT(15);
            } else if (q < DSV_QUALITY_PERCENT(70)) {
                q += DSV_QUALITY_PERCENT(8);
            } else if (q < DSV_QUALITY_PERCENT(75)) {
                q += DSV_QUALITY_PERCENT(3);
            }
            q = CLAMP(q, 0, enc->max_quality - DSV_QUALITY_PERCENT(5));
        }
        q = CLAMP(q, minq, enc->max_quality);
        q = CLAMP(q, 0, DSV_MAX_QUALITY); /* double validate range */
        DSV_INFO(("RC Q = %d delta = %d bpf: %d, avg: %d, dif: %d",
                q, delta, needed_bpf, bpf, abs(bpf - needed_bpf)));
        enc->rc_quant = q;
    } else {
        q = enc->quality;
        enc->rc_quant = q;
    }
    d->quant = DSV_MAX_QUALITY - ((DSV_MAX_QUALITY - 5) * q / DSV_MAX_QUALITY);

    DSV_DEBUG(("frame quant = %d", d->quant));
}

/* B.1 Packet Header Link Offsets */
static void
set_link_offsets(DSV_ENCODER *enc, DSV_BUF *buffer, int is_eos)
{
    uint8_t *data = buffer->data;
    unsigned next_link;
    unsigned prev_start = DSV_PACKET_PREV_OFFSET;
    unsigned next_start = DSV_PACKET_NEXT_OFFSET;

    next_link = is_eos ? 0 : buffer->len;

    data[prev_start + 0] = (enc->prev_link >> 24) & 0xff;
    data[prev_start + 1] = (enc->prev_link >> 16) & 0xff;
    data[prev_start + 2] = (enc->prev_link >>  8) & 0xff;
    data[prev_start + 3] = (enc->prev_link >>  0) & 0xff;

    data[next_start + 0] = (next_link >> 24) & 0xff;
    data[next_start + 1] = (next_link >> 16) & 0xff;
    data[next_start + 2] = (next_link >>  8) & 0xff;
    data[next_start + 3] = (next_link >>  0) & 0xff;
    
    enc->prev_link = next_link;
}

static DSV_FRAME *
dsv_extend_frame_luma(DSV_FRAME *frame)
{
    int j;
    DSV_PLANE *c = frame->planes + 0;
    int width = c->w;
    int height = c->h;
    int total_w = width + DSV_FRAME_BORDER * 2;
    uint8_t *src, *dst, *line;
    
    if (!frame->border) {
        return frame;
    }
    
    for (j = 0; j < c->h; j++) {
        line = DSV_GET_LINE(c, j);
        memset(line - DSV_FRAME_BORDER, line[0], DSV_FRAME_BORDER);
        memset(line + width - 1, line[width - 1], DSV_FRAME_BORDER + 1);
    }
    
    for (j = 0; j < DSV_FRAME_BORDER; j++) {
        dst = DSV_GET_XY(c, -DSV_FRAME_BORDER, -j - 1);
        src = DSV_GET_XY(c, -DSV_FRAME_BORDER, 0);
        memcpy(dst, src, total_w);
        dst = DSV_GET_XY(c, -DSV_FRAME_BORDER, height + j);
        src = DSV_GET_XY(c, -DSV_FRAME_BORDER, height - 1);
        memcpy(dst, src, total_w);
    }
    
    return frame;
}

static void
dsv_ds2x_frame_luma(DSV_FRAME *dst, DSV_FRAME *src)
{
    int i, j;
    DSV_PLANE *s = src->planes + 0;
    DSV_PLANE *d = dst->planes + 0;

    for (j = 0; j < d->h; j++) {
        uint8_t *sp = DSV_GET_LINE(s, (j << 1));
        uint8_t *dp = DSV_GET_LINE(d, j);
        int bp = 0;
        for (i = 0; i < d->w; i++) {            
            int p1, p2, p3, p4;
            p1 = sp[bp];
            p2 = sp[bp + 1];
            p3 = sp[bp + s->stride];
            p4 = sp[bp + 1 + s->stride];
            dp[i] = ((p1 + p2 + p3 + p4 + 2) >> 2);
            bp += 2;
        }
    }
}

static void
mk_pyramid(DSV_ENCODER *enc, DSV_ENCDATA *d)
{
    int i, fmt;
    DSV_FRAME *prev;
    int orig_w, orig_h;
    
    fmt = d->padded_frame->format;
    orig_w = d->padded_frame->width;
    orig_h = d->padded_frame->height;
    
    prev = d->padded_frame;
    for (i = 0; i < enc->pyramid_levels; i++) {
        d->pyramid[i] = dsv_mk_frame(
                fmt,
                DSV_ROUND_SHIFT(orig_w, i + 1),
                DSV_ROUND_SHIFT(orig_h, i + 1),
                1);
        /* only do luma plane because motion estimation does not use chroma */
        dsv_ds2x_frame_luma(d->pyramid[i], prev);
        dsv_extend_frame_luma(d->pyramid[i]);
        prev = d->pyramid[i];
    }
}

static int
motion_est(DSV_ENCODER *enc, DSV_ENCDATA *d)
{
    int i, intra_pct;
    DSV_PARAMS *p = &d->params;
    DSV_HME hme;
    DSV_ENCDATA *ref = d->refdata;
    
    memset(&hme, 0, sizeof(hme));
    hme.levels = enc->pyramid_levels;
    hme.params = &d->params;
    
    hme.src[0] = d->padded_frame;
    hme.ref[0] = ref->padded_frame;
    for (i = 0; i < hme.levels; i++) {
        hme.src[i + 1] = d->pyramid[i];
        hme.ref[i + 1] = ref->pyramid[i];
    }

    intra_pct = dsv_hme(&hme);
    d->final_mvs = hme.mvf[0]; /* save result of HME */
    for (i = 1; i < hme.levels + 1; i++) {
        if (hme.mvf[i]) {
            dsv_free(hme.mvf[i]);
        }
    }

    DSV_DEBUG(("intra block percent for frame %d = %d%%", d->fnum, intra_pct));
    
    if (intra_pct > enc->intra_pct_thresh) {
        p->has_ref = 0;
        DSV_INFO(("too much intra, inserting I frame %d%%", intra_pct));
        return 1;
    }
    return 0;
}

/* B.2.3.2 Motion Data */
static void
encode_motion(DSV_ENCDATA *d, DSV_BS *bs)
{
    uint8_t *bufs[DSV_SUB_NSUB];
    DSV_PARAMS *params = &d->params;
    int i, j;
    DSV_BS mbs[DSV_SUB_NSUB];
    DSV_ZBRLE rle;
    unsigned upperbound;
    int mesize = 0;

    upperbound = (params->nblocks_h * params->nblocks_v * 32);
    
    for (i = 0; i < DSV_SUB_NSUB; i++) {
        bufs[i] = dsv_alloc(upperbound);
        if (i != DSV_SUB_MODE) {
            dsv_bs_init(&mbs[i], bufs[i]);
        } else {
            dsv_bs_init_rle(&rle, bufs[i]);
        }
    }

    for (j = 0; j < params->nblocks_v; j++) {
        for (i = 0; i < params->nblocks_h; i++) {
            int idx = i + j * params->nblocks_h;
            DSV_MV *mv = &d->final_mvs[idx];
            
            dsv_bs_put_rle(&rle, mv->mode); /* zeros are more common */

            if (mv->mode == DSV_MODE_INTER) {
                int x, y;
                /* B.2.3.2 Motion Data - Motion Vector Prediction */
                dsv_movec_pred(d->final_mvs, params, i, j, &x, &y);
                dsv_bs_put_seg(mbs + DSV_SUB_MV_X, mv->u.mv.x - x);
                dsv_bs_put_seg(mbs + DSV_SUB_MV_Y, mv->u.mv.y - y);
            } else {
                /* B.2.3.2 Motion Data - Intra Sub-Block Mask */
                if (mv->submask == DSV_MASK_ALL_INTRA) {
                    dsv_bs_put_bit(mbs + DSV_SUB_SBIM, 1);
                } else {
                    dsv_bs_put_bit(mbs + DSV_SUB_SBIM, 0);
                    dsv_bs_put_bits(mbs + DSV_SUB_SBIM, 4, mv->submask);
                }
            }
        }
    }

    for (i = 0; i < DSV_SUB_NSUB; i++) {
        int bytes;
        
        dsv_bs_align(bs);
        if (i == DSV_SUB_MODE) {
            bytes = dsv_bs_end_rle_w(&rle);

            dsv_bs_put_ueg(bs, bytes);
            dsv_bs_align(bs);
            dsv_bs_concat(bs, bufs[i], bytes);
            mesize += bytes;
        } else {
            dsv_bs_align(&mbs[i]);
            bytes = dsv_bs_ptr(&mbs[i]);

            dsv_bs_put_ueg(bs, bytes);
            dsv_bs_align(bs);
            dsv_bs_concat(bs, mbs[i].start, bytes);
            mesize += bytes;
        }
        dsv_free(bufs[i]);
    }
    DSV_DEBUG(("motion bytes %d", mesize));
}

/* B.2.3.1 Stability Blocks */
static void
encode_stable_blocks(DSV_ENCODER *enc, DSV_ENCDATA *d, DSV_BS *bs)
{
    uint8_t *stabbuf;
    DSV_PARAMS *params = &d->params;
    int i, nblk, avgdiv, bytes;
    DSV_ZBRLE stabrle;
    unsigned upperbound;

    nblk = params->nblocks_h * params->nblocks_v;
    upperbound = (nblk * 32);

    stabbuf = dsv_alloc(upperbound);
    dsv_bs_init_rle(&stabrle, stabbuf);

    if (enc->refresh_ctr >= enc->stable_refresh) {
        enc->refresh_ctr = 0;
        memset(enc->stability, 0, sizeof(*enc->stability) * nblk);
    }
    avgdiv = enc->refresh_ctr;
    if (avgdiv <= 0) {
        avgdiv = 1;
    }
    
    for (i = 0; i < nblk; i++) {
        int ax, ay, stable = 0;
        int intra_block = 0;
        if (d->isP) {
            DSV_MV *mv = &d->final_mvs[i];
            if (mv->mode == DSV_MODE_INTER) {
                /* with abs - all forms of motion are punished */
                /* divide by 4 to help keep slow moving areas high quality */
                enc->stability[i].x += abs(mv->u.mv.x) >> 2;
                enc->stability[i].y += abs(mv->u.mv.y) >> 2;
                stable = mv->high_detail;

                ax = enc->stability[i].x / avgdiv;
                ay = enc->stability[i].y / avgdiv;
                /* if average was approx zero */
                stable |= (ax == 0 && ay == 0 && !mv->lo_tex && !mv->lo_var);
            } else {
                /* if mostly intra, keep it looking good */
                int nblks = 0;
                if (mv->submask & DSV_MASK_INTRA00) {
                    nblks++;
                }
                if (mv->submask & DSV_MASK_INTRA01) {
                    nblks++;
                }
                if (mv->submask & DSV_MASK_INTRA10) {
                    nblks++;
                }
                if (mv->submask & DSV_MASK_INTRA11) {
                    nblks++;
                }
                intra_block = 1;
            }
            /* if not important, don't bother wasting bits on it */
            if (mv->lo_tex || mv->lo_var) {
                enc->stability[i].x = 0x3fff;
                enc->stability[i].y = 0x3fff;
            }
        } else {
            ax = enc->stability[i].x / avgdiv;
            ay = enc->stability[i].y / avgdiv;
            stable = (ax == 0 && ay == 0); /* if average was approx zero */
        }

        enc->stable_blocks[i] = stable | (intra_block << 1);
        dsv_bs_put_rle(&stabrle, enc->stable_blocks[i] & 1);
    }
    dsv_bs_align(bs);
    bytes = dsv_bs_end_rle_w(&stabrle);
    dsv_bs_put_ueg(bs, bytes);
    dsv_bs_align(bs);
    dsv_bs_concat(bs, stabbuf, bytes);
    dsv_free(stabbuf);
    DSV_DEBUG(("stab bytes %d", bytes));
}

static void
encode_packet_hdr(DSV_BS *bs, int pkt_type)
{
    dsv_bs_put_bits(bs, 8, DSV_FOURCC_0);
    dsv_bs_put_bits(bs, 8, DSV_FOURCC_1);
    dsv_bs_put_bits(bs, 8, DSV_FOURCC_2);
    dsv_bs_put_bits(bs, 8, DSV_FOURCC_3);
    dsv_bs_put_bits(bs, 8, DSV_VERSION_MINOR);
    
    dsv_bs_put_bits(bs, 8, pkt_type);
    
    /* reserve space for link offsets */
    dsv_bs_put_bits(bs, 32, 0);
    dsv_bs_put_bits(bs, 32, 0);
}

/* B.2.1 Metadata Packet */
static void
encode_metadata(DSV_ENCODER *enc, DSV_BUF *buf)
{
    DSV_BS bs;
    unsigned next_link;
    DSV_META *meta = &enc->vidmeta;
    unsigned next_start = DSV_PACKET_NEXT_OFFSET;

    dsv_mk_buf(buf, 64);
    
    dsv_bs_init(&bs, buf->data);
    
    encode_packet_hdr(&bs, DSV_PT_META);
   
    dsv_bs_put_ueg(&bs, meta->width);
    dsv_bs_put_ueg(&bs, meta->height);
    
    dsv_bs_put_ueg(&bs, meta->subsamp);

    dsv_bs_put_ueg(&bs, meta->fps_num);
    dsv_bs_put_ueg(&bs, meta->fps_den);
    
    dsv_bs_put_ueg(&bs, meta->aspect_num);
    dsv_bs_put_ueg(&bs, meta->aspect_den);
    
    dsv_bs_align(&bs);
    
    next_link = dsv_bs_ptr(&bs);
    buf->data[next_start + 0] = (next_link >> 24) & 0xff;
    buf->data[next_start + 1] = (next_link >> 16) & 0xff;
    buf->data[next_start + 2] = (next_link >>  8) & 0xff;
    buf->data[next_start + 3] = (next_link >>  0) & 0xff;
    
    buf->len = next_link; /* trim length to actual size */
}

static void
dsv_mk_coefs(DSV_COEFS *c, int format, int width, int height)
{
    int h_shift, v_shift;
    int chroma_width;
    int chroma_height;
    int c0len, c1len, c2len;

    h_shift = DSV_FORMAT_H_SHIFT(format);
    v_shift = DSV_FORMAT_V_SHIFT(format);
    chroma_width = DSV_ROUND_SHIFT(width, h_shift);
    chroma_height = DSV_ROUND_SHIFT(height, v_shift);
    chroma_width = DSV_ROUND_POW2(chroma_width, 1);
    chroma_height = DSV_ROUND_POW2(chroma_height, 1);
    c[0].width = width;
    c[0].height = height;
    
    c0len = c[0].width * c[0].height;

    c[1].width = chroma_width;
    c[1].height = chroma_height;
    
    c1len = c[1].width * c[1].height;

    c[2].width = chroma_width;
    c[2].height = chroma_height;
    
    c2len = c[2].width * c[2].height;

    c[0].data = dsv_alloc((c0len + c1len + c2len) * sizeof(DSV_SBC));
    c[1].data = c[0].data + c0len;
    c[2].data = c[0].data + c0len + c1len;
}

static void
encode_picture(DSV_ENCODER *enc, DSV_ENCDATA *d, DSV_BUF *output_buf)
{
    DSV_BS bs;
    unsigned upperbound;
    DSV_STABILITY stab;
    DSV_COEFS coefs[3];
    int i, width, height;
    
    width = enc->vidmeta.width;
    height = enc->vidmeta.height;
    upperbound = width * height;
    switch (enc->vidmeta.subsamp) {
        case DSV_SUBSAMP_444:
            upperbound *= 6;
            break;
        case DSV_SUBSAMP_422:
            upperbound *= 4;
            break;
        case DSV_SUBSAMP_420:
        case DSV_SUBSAMP_411:
            upperbound *= 2;
            break;
        default:
            DSV_ASSERT(0);
            break;
    }

    dsv_mk_buf(output_buf, upperbound);
    
    dsv_bs_init(&bs, output_buf->data);
    /* B.2.3 Picture Packet */
    encode_packet_hdr(&bs, DSV_MAKE_PT(d->params.is_ref, d->params.has_ref));

    dsv_bs_align(&bs);
    dsv_bs_put_bits(&bs, 32, d->fnum);

    dsv_bs_align(&bs);
    /* encode the quarter block sizes */
    dsv_bs_put_ueg(&bs, d->params.blk_w >> 2);
    dsv_bs_put_ueg(&bs, d->params.blk_h >> 2);
    dsv_bs_align(&bs);
    /* encode stability data */
    encode_stable_blocks(enc, d, &bs);
    if (d->params.has_ref) {
        dsv_bs_align(&bs);
        /* encode motion vecs and intra blocks */
        encode_motion(d, &bs);
    }
    
    /* B.2.3.3 Image Data */
    dsv_bs_align(&bs);
    stab.params = &d->params;
    stab.stable_blocks = enc->stable_blocks;
    stab.isP = d->isP;
    dsv_bs_put_bits(&bs, DSV_MAX_QP_BITS, d->quant);
    dsv_mk_coefs(coefs, enc->vidmeta.subsamp, width, height);

    for (i = 0; i < 3; i++) {
        stab.cur_plane = i;
        dsv_fwd_sbt(&d->xf_frame->planes[i], &coefs[i], stab.isP);
        dsv_encode_plane(&bs, &coefs[i], d->quant, &stab);
        dsv_inv_sbt(&d->xf_frame->planes[i], &coefs[i], d->quant, stab.isP, i);
    }

    if (coefs[0].data) { /* only the first pointer is actual allocated data */
        dsv_free(coefs[0].data);
        coefs[0].data = NULL;
    }
 
    dsv_bs_align(&bs);

    output_buf->len = dsv_bs_ptr(&bs);
}

static int
dsv_frame_avg_luma(DSV_FRAME *frame)
{
    DSV_PLANE *plane;
    int i, j;
    int acc = 0;
    
    plane = frame->planes + 0;
    for (j = 0; j < plane->h; j++) {
        uint8_t *line = DSV_GET_LINE(plane, j);
        for (i = 0; i < plane->w; i++) {
            acc += line[i];
        }
    }
    return acc / (plane->w * plane->h);
}

static int
check_scene_change(DSV_ENCODER *enc, DSV_ENCDATA *d)
{
    int al, delta, did_sc = 0;
    /* use smallest pyramid level to compute average luma */
    al = dsv_frame_avg_luma(d->pyramid[enc->pyramid_levels - 1]);
    delta = abs(enc->prev_avg_luma - al);
    
    if (delta > enc->scene_change_delta) {
        d->params.has_ref = 0;
        DSV_DEBUG(("scene change %d [%d %d]", delta, al, enc->prev_avg_luma));
        DSV_INFO(("scene change detected, inserting I frame [%d]", d->fnum));
        did_sc = 1;
    }
    enc->prev_avg_luma = al;
    return did_sc;
}

static int
size4dim(int dim)
{
    if (dim > 1280) {
        return DSV_MAX_BLOCK_SIZE;
    } 
    if (dim > 1024) {
        return 48;
    } 
    if (dim > 704) {
       return 32;
    } 
    if (dim > 352) {
       return 24;
    }
    return DSV_MIN_BLOCK_SIZE;
}

static int
encode_one_frame(DSV_ENCODER *enc, DSV_ENCDATA *d, DSV_BUF *output_buf)
{
    DSV_PARAMS *p;
    int i, w, h;
    int gop_start = 0;
    int forced_intra = 0;
    
    p = &d->params;
    p->vidmeta = &enc->vidmeta;

    w = p->vidmeta->width;
    h = p->vidmeta->height;

    p->blk_w = size4dim(w) & ((unsigned) (~0) << 3);
    p->blk_h = size4dim(h) & ((unsigned) (~0) << 3);

    p->blk_w = CLAMP(p->blk_w, DSV_MIN_BLOCK_SIZE, DSV_MAX_BLOCK_SIZE);
    p->blk_h = CLAMP(p->blk_h, DSV_MIN_BLOCK_SIZE, DSV_MAX_BLOCK_SIZE);

    p->nblocks_h = DSV_DIV_ROUND(w, p->blk_w);
    p->nblocks_v = DSV_DIV_ROUND(h, p->blk_h);
    DSV_DEBUG(("block size %dx%d", p->blk_w, p->blk_h));
    if (enc->stability == NULL) {
        enc->stability = dsv_alloc(sizeof(*enc->stability) * p->nblocks_h * p->nblocks_v);
        enc->stable_blocks = dsv_alloc(p->nblocks_h * p->nblocks_v);
    }
    
    if (enc->pyramid_levels == 0) {
        int maxdim, lvls;
        
        maxdim = MIN(w, h);
        lvls = dsv_lb2(maxdim);
        maxdim = MAX(d->params.nblocks_h, d->params.nblocks_v);
        /* important for HBM, otherwise we'll be doing extra work for no reason */
        while ((1 << lvls) > maxdim) {
            lvls--;
        }
        enc->pyramid_levels = CLAMP(lvls, 3, DSV_MAX_PYRAMID_LEVELS);
    }

    DSV_DEBUG(("gop length %d", enc->gop));
    if (enc->gop != DSV_GOP_INTRA) {
        d->padded_frame = dsv_clone_frame(d->input_frame, 1);
        
        dsv_extend_frame(d->padded_frame);
        mk_pyramid(enc, d);
    } else {
        d->padded_frame = dsv_clone_frame(d->input_frame, 0);
    }
    if (enc->force_metadata || ((enc->prev_gop + enc->gop) <= d->fnum)) {
        gop_start = 1;
        enc->prev_gop = d->fnum;
        enc->force_metadata = 0;
    }

    if (enc->gop == DSV_GOP_INTRA) {
        d->params.is_ref = 0;
        d->params.has_ref = 0;
    } else {
        d->params.is_ref = 1;
        if (gop_start) {
            d->params.has_ref = 0;
        } else {
            d->params.has_ref = 1;
            d->refdata = enc->ref;
            encdat_ref(d->refdata);
        }
        if (enc->ref) {
            encdat_unref(enc, enc->ref);
            enc->ref = NULL;
        }
        enc->ref = d;
        encdat_ref(d);
        
        if (enc->do_scd) {
            forced_intra = check_scene_change(enc, d);
        }
    }
    if (d->params.has_ref) {
        forced_intra = motion_est(enc, d);
    }
    quality2quant(enc, d, forced_intra);
    dsv_frame_copy(d->xf_frame, d->padded_frame);
    if (d->params.has_ref) {        
        dsv_sub_pred(d->final_mvs, &d->params, d->residual, d->xf_frame, d->refdata->recon_frame);
    }
    encode_picture(enc, d, output_buf);
    if (d->params.has_ref) {
        dsv_frame_add(d->xf_frame, d->residual);
    }
    if (d->params.is_ref && enc->gop != DSV_GOP_INTRA) {
        DSV_FRAME *frame;
        int fmt;
        
        fmt = enc->vidmeta.subsamp;
        frame = dsv_mk_frame(fmt, enc->vidmeta.width, enc->vidmeta.height, 1);
        dsv_frame_copy(frame, d->xf_frame);
        dsv_extend_frame(frame);
        d->recon_frame = frame;
    }

    if (d->final_mvs) {
        dsv_free(d->final_mvs);
        d->final_mvs = NULL;
    }
    if (d->refdata) {
        encdat_unref(enc, d->refdata);
        d->refdata = NULL;
    }

    if (!d->params.is_ref) {
        for (i = 0; i < enc->pyramid_levels; i++) {
            if (d->pyramid[i]) {
                dsv_frame_ref_dec(d->pyramid[i]);
                d->pyramid[i] = NULL;
            }
        }
    }
    return gop_start;
}

extern void
dsv_enc_init(DSV_ENCODER *enc)
{    
    memset(enc, 0, sizeof(*enc));
    enc->prev_gop = -1;
    
    /* default config */
    enc->quality = DSV_QUALITY_PERCENT(85);
    enc->gop = 24;
    enc->pyramid_levels = 0;
    enc->rc_mode = DSV_RATE_CONTROL_CRF;
    enc->bitrate = INT_MAX;
    enc->max_q_step = DSV_MAX_QUALITY * 1 / 200;
    enc->min_quality = DSV_QUALITY_PERCENT(1);
    enc->max_quality = DSV_QUALITY_PERCENT(95);
    enc->min_I_frame_quality = DSV_QUALITY_PERCENT(5);
    enc->rc_high_motion_nudge = 1;
    enc->bpf_total = 0;
    enc->bpf_avg = 0;
    enc->last_P_frame_over = 0;
    
    enc->intra_pct_thresh = 50;
    enc->prev_avg_luma = 0;
    enc->stable_refresh = 14;
    enc->scene_change_delta = 4;
    enc->do_scd = 1;
}

extern void
dsv_enc_start(DSV_ENCODER *enc)
{
    enc->quality = CLAMP(enc->quality, 0, DSV_MAX_QUALITY);
    if (enc->rc_mode != DSV_RATE_CONTROL_CRF) {
        enc->rc_quant = enc->quality;
        enc->avg_P_frame_q = enc->quality * 4 / 5;
    }

    enc->force_metadata = 1;
}

extern void
dsv_enc_free(DSV_ENCODER *enc)
{    
    if (enc->ref) {
        encdat_unref(enc, enc->ref);
        enc->ref = NULL;
    }
    if (enc->stability) {
        dsv_free(enc->stability);
        enc->stability = NULL;
    }
    if (enc->stable_blocks) {
        dsv_free(enc->stable_blocks);
        enc->stable_blocks = NULL;
    }
}

extern void
dsv_enc_set_metadata(DSV_ENCODER *enc, DSV_META *md)
{
    memcpy(&enc->vidmeta, md, sizeof(DSV_META));
}

extern void
dsv_enc_force_metadata(DSV_ENCODER *enc)
{
    enc->force_metadata = 1;
}

/* B.2.2 End of Stream Packet */
extern void
dsv_enc_end_of_stream(DSV_ENCODER *enc, DSV_BUF *bufs)
{
    DSV_BS bs;
    
    dsv_mk_buf(&bufs[0], DSV_PACKET_HDR_SIZE);
    dsv_bs_init(&bs, bufs[0].data);
    
    encode_packet_hdr(&bs, DSV_PT_EOS);

    set_link_offsets(enc, &bufs[0], 1);
    DSV_INFO(("creating end of stream packet"));
}

extern int
dsv_enc(DSV_ENCODER *enc, DSV_FRAME *frame, DSV_BUF *bufs)
{
    DSV_ENCDATA *d;
    int w, h;
    int nbuf = 0;
    DSV_BUF outbuf;

    if (bufs == NULL) {
        DSV_ERROR(("null buffer list passed to encoder!"));
        return 0;
    }
    d = dsv_alloc(sizeof(DSV_ENCDATA));
    
    d->refcount = 1;
    
    w = enc->vidmeta.width;
    h = enc->vidmeta.height;
    d->xf_frame = dsv_mk_frame(enc->vidmeta.subsamp, w, h, 1);
    d->residual = dsv_mk_frame(enc->vidmeta.subsamp, w, h, 1);
    
    d->input_frame = frame;
    d->fnum = enc->next_fnum++;

    if (encode_one_frame(enc, d, &outbuf)) {
        DSV_BUF metabuf;
        encode_metadata(enc, &metabuf);
        /* send metadata first, then compressed frame */
        bufs[nbuf++] = metabuf;
    }
    bufs[nbuf++] = outbuf;
    
    if (d->isP) {
        enc->refresh_ctr++; /* for averaging stable blocks */
    }
    /* rate control statistics */
    if (enc->rc_mode != DSV_RATE_CONTROL_CRF) {
        enc->bpf_total += outbuf.len;
        enc->bpf_reset++;
        if (d->isP) {
            unsigned fps, needed_bpf;
            int went_over;
            int went_under;
            
            enc->total_P_frame_q += enc->rc_quant;
            enc->avg_P_frame_q = enc->total_P_frame_q / enc->bpf_reset;
            fps = (enc->vidmeta.fps_num << 5) / enc->vidmeta.fps_den;
            if (fps == 0) {
                fps = 1;
            }
            /* bpf = bytes per frame */
            needed_bpf = ((enc->bitrate << 5) / fps) >> 3;
            went_under = outbuf.len < (needed_bpf * 3 / 4);
            needed_bpf = (needed_bpf * 7 / 8);
            went_over = outbuf.len > needed_bpf;
            enc->back_into_range = (enc->last_P_frame_over && went_under);
            enc->last_P_frame_over = went_over;
            DSV_INFO(("RC last P over ? (%d > %d) : %d", outbuf.len, needed_bpf, enc->last_P_frame_over));
        } else {
            enc->last_P_frame_over = 0;
            enc->back_into_range = 0;
        }
        enc->bpf_avg = enc->bpf_total / enc->bpf_reset;
        if (enc->bpf_reset >= DSV_BPF_RESET) {
            enc->bpf_total = enc->bpf_avg;
            enc->total_P_frame_q = enc->total_P_frame_q / enc->bpf_reset;
            enc->bpf_reset = 1;
        }
    }
    
    encdat_unref(enc, d);

    set_link_offsets(enc, &bufs[nbuf - 1], 0);
    return nbuf;
}

#endif /* encoder impl guard */
#endif /* encoder include */

/******/

/* TODO */
#ifdef _DSV1_INCLUDE_DECODER_ 
#ifndef _DSV1_DECODER_IMPL_GUARD_
#define _DSV1_DECODER_IMPL_GUARD_

static unsigned
local_get_bit(DSV_BS *s)
{
    unsigned out;

    out = s->start[dsv_bs_ptr(s)] >> (7 - (s->pos & 7));
    s->pos++;

    return out & 1;
}

static unsigned
dsv_bs_get_bit(DSV_BS *s)
{
    return local_get_bit(s);
}

static unsigned
dsv_bs_get_bits(DSV_BS *s, unsigned n)
{
    unsigned rem, bit, out = 0;
    
    while (n > 0) {
        rem = 8 - (s->pos & 7);
        rem = MIN(n, rem);
        bit = (7 - (s->pos & 7)) - rem + 1;
        out <<= rem;
        out |= (s->start[dsv_bs_ptr(s)] & (((1 << rem) - 1) << bit)) >> bit;
        n -= rem;
        s->pos += rem;
    }
    return out;
}

/* B. Encoding Type: unsigned interleaved exp-Golomb code (UEG) */
static unsigned
dsv_bs_get_ueg(DSV_BS *s)
{
    unsigned v = 1;
    
    while (!local_get_bit(s)) {
        v = (v << 1) | local_get_bit(s);
    }
    return v - 1;
}

/* B. Encoding Type: signed interleaved exp-Golomb code (SEG) */
static int
dsv_bs_get_seg(DSV_BS *s)
{
    int v;
    
    v = dsv_bs_get_ueg(s);
    if (v && local_get_bit(s)) {
        return -v;
    }
    return v;
}

/* B. Encoding Type: non-zero interleaved exp-Golomb code (NEG) */
static int
dsv_bs_get_neg(DSV_BS *bs)
{
    int v;
    
    v = dsv_bs_get_ueg(bs) + 1;
    if (v && local_get_bit(bs)) {
        return -v;
    }
    return v;
}

/* B. Encoding Format: Zero Bit Run-Length Encoding (ZBRLE) */
static int
dsv_bs_get_rle(DSV_ZBRLE *rle)
{
    if (rle->nz == 0) {
        rle->nz = dsv_bs_get_ueg(&rle->bs);
        return (rle->nz == 0);
    }
    rle->nz--;
    return (rle->nz == 0);
}

/* B. Encoding Format: Zero Bit Run-Length Encoding (ZBRLE) */
static int
dsv_bs_end_rle_r(DSV_ZBRLE *rle)
{   
    if (rle->nz > 1) { /* early termination */
        DSV_ERROR(("%d remaining in run", rle->nz));
    }
    return 0;
}

static void
hzcc_dec(DSV_BS *bs, unsigned bufsz, DSV_COEFS *dst, int q, DSV_STABILITY *stab)
{
    int x, y, l, s, o, v;
    int qp;
    int sw, sh;
    int bx, by;
    int dbx, dby;
    int run, runs;
    DSV_SBC *out = dst->data;
    DSV_SBC *outp;
    int w = dst->width;
    int h = dst->height;
    
    dsv_bs_align(bs);
    runs = dsv_bs_get_bits(bs, 32);
    dsv_bs_align(bs);
    if (runs-- > 0) {
        run = dsv_bs_get_ueg(bs);
    } else {
        run = INT_MAX;
    }
    q = fix_quant(q, stab);
    s = l = 0;
    
    sw = dimat(l, w);
    sh = dimat(l, h);
    qp = dsv_get_quant(q, stab->isP, l);

    o = subband(l, s, w, h);
    outp = out + o;
    
    /* C.2.3 LL Subband */
    for (y = 0; y < sh; y++) {
        for (x = 0; x < sw; x++) {
            if (!run--) {
                if (runs-- > 0) {
                    run = dsv_bs_get_ueg(bs);
                } else {
                    run = INT_MAX;
                }
                v = dsv_bs_get_neg(bs);
                if (dsv_bs_ptr(bs) >= bufsz) {
                    return;
                }
                outp[x] = dequant(v, qp);
            }
        }
        outp += w;
    }

    for (l = 0; l < DSV_MAXLVL; l++) {
        unsigned char *blockrow;
        int tmq;
        
        sw = dimat(l, w);
        sh = dimat(l, h);
        dbx = (stab->params->nblocks_h << BLOCK_P) / sw;
        dby = (stab->params->nblocks_v << BLOCK_P) / sh;
        qp = dsv_get_quant(q, stab->isP, l);
        /* C.2.5 Highest Level Subband Dequantization */
        if (l == (DSV_MAXLVL - 1)) {
            /* C.2.2 Quantization Parameter Derivation
             *     - get_quant_highest_frequency */
            int qp_h;
            qp = dsv_lb2(qp);
            if (!stab->isP) {
                qp_h = CLAMP(qp - DSV_QP_I, 1, 24);
            } else {
                qp_h = CLAMP(qp - DSV_QP_P, 1, 24);
            }
            
            for (s = 1; s < NSUBBAND; s++) {
                o = subband(l, s, w, h);
                outp = out + o;
                by = 0;
                for (y = 0; y < sh; y++) {
                    bx = 0;
                    blockrow = stab->stable_blocks + (by >> BLOCK_P) * stab->params->nblocks_h;
                    for (x = 0; x < sw; x++) {
                        if (!run--) {
                            if (runs-- > 0) {
                                run = dsv_bs_get_ueg(bs);
                            } else {
                                run = INT_MAX;
                            }
                            v = dsv_bs_get_neg(bs);
                            if (dsv_bs_ptr(bs) >= bufsz) {
                                return;
                            }
                            tmq = qp;

                            if (blockrow[bx >> BLOCK_P]) {
                                tmq = qp_h; /* keep it very high quality */
                            }
                                                  
                            outp[x] = dequantH(v, tmq);
                        }
                        bx += dbx;
                    }
                    outp += w;
                    by += dby;
                }
            }
        } else {
            /* C.2.4 Higher Level Subband Dequantization */
            for (s = 1; s < NSUBBAND; s++) {
                o = subband(l, s, w, h);
                outp = out + o;
                by = 0;
                for (y = 0; y < sh; y++) {
                    bx = 0;
                    blockrow = stab->stable_blocks + (by >> BLOCK_P) * stab->params->nblocks_h;
                    for (x = 0; x < sw; x++) {
                        if (!run--) {
                            if (runs-- > 0) {
                                run = dsv_bs_get_ueg(bs);
                            } else {
                                run = INT_MAX;
                            }
                            v = dsv_bs_get_neg(bs);
                            if (dsv_bs_ptr(bs) >= bufsz) {
                                return;
                            }
                            tmq = tmq4pos(qp, blockrow[bx >> BLOCK_P]);
                            if (tmq < MINQUANT) {
                                tmq = MINQUANT;
                            }
                            outp[x] = dequant(v, tmq);
                        }
                        bx += dbx;
                    }
                    outp += w;
                    by += dby;
                }
            }
        }
    }

    dsv_bs_align(bs);
}

/* B.2.3.3 Image Data - Coefficient Decoding */
static void
dsv_decode_plane(uint8_t *in, unsigned s, DSV_COEFS *dst, int q, DSV_STABILITY *stab)
{
    DSV_BS bs;
    int LL;
    
    dsv_bs_init(&bs, in);
    LL = dsv_bs_get_seg(&bs);
    hzcc_dec(&bs, s, dst, q, stab);

    /* error detection */
    if (dsv_bs_get_bits(&bs, 8) != EOP_SYMBOL) {
        DSV_ERROR(("bad eop, frame data incomplete and/or corrupt"));
    }
    dsv_bs_align(&bs);

    dst->data[0] = LL;
}

static void
dsv_add_pred(DSV_MV *mv, DSV_PARAMS *p, DSV_FRAME *dif, DSV_FRAME *out, DSV_FRAME *ref)
{
    DSV_PLANE *d, *o;
    int c;

    for (c = 0; c < 3; c++) {
        d = dif->planes + c;
        o = out->planes + c;
        
        compensate(mv, p, c, ref, o);
        addf(o->data, o->stride, d->data, d->stride, o->w, o->h);
    }
}

/* B.1 Packet Header */
static int
decode_packet_hdr(DSV_BS *bs)
{
    int c0, c1, c2, c3;
    int pkt_type;
    int ver_min;
    
    c0 = dsv_bs_get_bits(bs, 8);
    c1 = dsv_bs_get_bits(bs, 8);
    c2 = dsv_bs_get_bits(bs, 8);
    c3 = dsv_bs_get_bits(bs, 8);
    if (c0 != DSV_FOURCC_0 || c1 != DSV_FOURCC_1 || c2 != DSV_FOURCC_2 || c3 != DSV_FOURCC_3) {
        DSV_ERROR(("bad 4cc (%c %c %c %c)\n", c0, c1, c2, c3));
        return -1;
    }
    
    ver_min = dsv_bs_get_bits(bs, 8);
    DSV_DEBUG(("version 1.%d", ver_min));

    /* B.1.1 Packet Type */
    pkt_type = dsv_bs_get_bits(bs, 8);
    DSV_DEBUG(("packet type %02x", pkt_type));
    /* link offsets */
    dsv_bs_get_bits(bs, 32);
    dsv_bs_get_bits(bs, 32);
    
    return pkt_type;
}

/* B.2.1 Metadata Packet */
static void
decode_meta(DSV_DECODER *d, DSV_BS *bs)
{
    DSV_META *fmt = &d->vidmeta;
        
    fmt->width = dsv_bs_get_ueg(bs);
    fmt->height = dsv_bs_get_ueg(bs);
    DSV_DEBUG(("dimensions = %d x %d", fmt->width, fmt->height));
    
    fmt->subsamp = dsv_bs_get_ueg(bs);
    DSV_DEBUG(("subsamp %d", fmt->subsamp));
    
    fmt->fps_num = dsv_bs_get_ueg(bs);
    fmt->fps_den = dsv_bs_get_ueg(bs);
    DSV_DEBUG(("fps %d/%d", fmt->fps_num, fmt->fps_den));
    
    fmt->aspect_num = dsv_bs_get_ueg(bs);
    fmt->aspect_den = dsv_bs_get_ueg(bs);
    DSV_DEBUG(("aspect ratio %d/%d", fmt->aspect_num, fmt->aspect_den));
}

/* B.2.3.2 Motion Data */
static void
decode_motion(DSV_IMAGE *img, DSV_MV *mvs, DSV_BS *inbs, DSV_BUF *buf)
{
    DSV_PARAMS *params = &img->params;
    DSV_BS bs[DSV_SUB_NSUB];
    DSV_ZBRLE rle;
    int i, j;

    dsv_bs_align(inbs);

    for (i = 0; i < DSV_SUB_NSUB; i++) {
        int len;
        
        len = dsv_bs_get_ueg(inbs);
        dsv_bs_align(inbs);

        if (i != DSV_SUB_MODE) {
            dsv_bs_init(bs + i, buf->data + dsv_bs_ptr(inbs));
        } else {
            dsv_bs_init_rle(&rle, buf->data + dsv_bs_ptr(inbs));
        }
    
        dsv_bs_skip(inbs, len);
    }
    
    for (j = 0; j < params->nblocks_v; j++) {
        for (i = 0; i < params->nblocks_h; i++) {
            DSV_MV *mv = &mvs[i + j * params->nblocks_h];
            
            mv->mode = dsv_bs_get_rle(&rle);

            if (mv->mode == DSV_MODE_INTER) {
                int px, py;

                /* B.2.3.2 Motion Data - Motion Vector Prediction */
                dsv_movec_pred(mvs, params, i, j, &px, &py);
                mv->u.mv.x = dsv_bs_get_seg(bs + DSV_SUB_MV_X) + px;
                mv->u.mv.y = dsv_bs_get_seg(bs + DSV_SUB_MV_Y) + py;
            } else {
                /* B.2.3.2 Motion Data - Intra Sub-Block Mask Decoding */
                if (dsv_bs_get_bit(bs + DSV_SUB_SBIM)) {
                    mv->submask = DSV_MASK_ALL_INTRA;
                } else {
                    mv->submask = dsv_bs_get_bits(bs + DSV_SUB_SBIM, 4);
                }
                /* set as intra */
                img->stable_blocks[i + j * params->nblocks_h] |= (1 << 1);
            }
        }
    }
    dsv_bs_end_rle_r(&rle);
}

/* B.2.3.1 Stability Blocks */
static void
decode_stability_blocks(DSV_IMAGE *img, DSV_BS *inbs, DSV_BUF *buf)
{
    DSV_PARAMS *params = &img->params;
    DSV_ZBRLE qualrle;
    int i, nblk;
    int len;

    dsv_bs_align(inbs);
    len = dsv_bs_get_ueg(inbs);
    dsv_bs_align(inbs);
    dsv_bs_init_rle(&qualrle, buf->data + dsv_bs_ptr(inbs));
    dsv_bs_skip(inbs, len);
    nblk = params->nblocks_h * params->nblocks_v;
    for (i = 0; i < nblk; i++) {
        img->stable_blocks[i] = dsv_bs_get_rle(&qualrle);
    }
    dsv_bs_end_rle_r(&qualrle);
}

static void
drawvec(DSV_PLANE *fd, int x0, int y0, int x1, int y1, int bw, int bh)
{
    int sx = -1, sy = -1, dx, dy, err, e2;
    x0 = x0 + bw / 2;
    y0 = y0 + bh / 2;
    x1 += x0;
    y1 += y0;
    dx = abs(x1 - x0);
    dy = abs(y1 - y0);
    if (x0 < x1) {
        sx = 1;
    }
    if (y0 < y1) {
        sy = 1;
    }
    err = dx - dy;
    
    if (y0 >= 0 && y0 < fd->h && x0 >= 0 && x0 < fd->w) {
        *DSV_GET_XY(fd, x0, y0) = 0;
    }
    while (x0 != x1 || y0 != y1) {
        if (y0 >= 0 && y0 < fd->h && x0 >= 0 && x0 < fd->w) {
            *DSV_GET_XY(fd, x0, y0) = 0;
        }
        e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void
draw_info(DSV_IMAGE *img, DSV_FRAME *output_pic, DSV_MV *mvs, int mode)
{
    int i, j, k, x, y, a, b;
    int bw, bh;
    DSV_PLANE *lp;
    DSV_PARAMS *p = &img->params;
    
    lp = output_pic->planes + 0; /* luma plane */
    bw = p->blk_w;
    bh = p->blk_h;
    for (j = 0; j < p->nblocks_v; j++) {
        y = j * bh;
        memset(DSV_GET_LINE(lp, y), 0, lp->stride);
        for (i = 0; i < p->nblocks_h; i++) {
            DSV_MV *mv = &mvs[i + j * p->nblocks_h];
            x = i * bw;
            for (k = y; k < y + bh && k < lp->h; k++) {
                if (x < lp->w) {
                    DSV_GET_LINE(lp, k)[x] = 0;
                }
            }
            if ((mode & DSV_DRAW_STABHQ) && (img->stable_blocks[i + j * p->nblocks_h] & 1)) {
                a = x + bw / 2;
                b = y + bh / 2;
                for (k = -bw / 4; k <= bw / 4; k++) {
                    if (b >= 0 && b < lp->h && a + k >= 0 && a + k < lp->w) {
                        *DSV_GET_XY(lp, a + k, b) = (k & 1) * 255;
                    }
                }
            }
            if ((mode & DSV_DRAW_MOVECS) && mv->mode == DSV_MODE_INTER) {
                drawvec(lp, x, y, mv->u.mv.x, mv->u.mv.y, bw, bh);
            }
            if ((mode & DSV_DRAW_IBLOCK) && mv->mode == DSV_MODE_INTRA) {
                if (mv->submask & DSV_MASK_INTRA00) {
                    a = x + bw * 1 / 4;
                    b = y + bh * 1  / 4;
                    *DSV_GET_XY(lp, a, b) = 255;
                }
                if (mv->submask & DSV_MASK_INTRA01) {
                    a = x + bw * 3 / 4;
                    b = y + bh * 1 / 4;
                    *DSV_GET_XY(lp, a, b) = 255;
                }
                if (mv->submask & DSV_MASK_INTRA10) {
                    a = x + bw * 1 / 4;
                    b = y + bh * 3 / 4;
                    *DSV_GET_XY(lp, a, b) = 255;
                }
                if (mv->submask & DSV_MASK_INTRA11) {
                    a = x + bw * 3 / 4;
                    b = y + bh * 3 / 4;
                    *DSV_GET_XY(lp, a, b) = 255;
                }
            }
        
        }
    }
}

static void
img_unref(DSV_IMAGE *img)
{
    DSV_ASSERT(img && img->refcount > 0);
    img->refcount--;
    
    if (img->refcount != 0) {
        return;
    }
    if (img->stable_blocks) {
        dsv_free(img->stable_blocks);
        img->stable_blocks = NULL;
    }
    if (img->out_frame) {
        dsv_frame_ref_dec(img->out_frame);
    }
    if (img->ref_frame) {
        dsv_frame_ref_dec(img->ref_frame);
    }
    dsv_free(img);
}

extern void
dsv_dec_free(DSV_DECODER *d)
{
    if (d->ref) {
        img_unref(d->ref);
    }
}

extern DSV_META *
dsv_get_metadata(DSV_DECODER *d)
{
    DSV_META *meta;
    
    meta = dsv_alloc(sizeof(DSV_META));
    memcpy(meta, &d->vidmeta, sizeof(DSV_META));
    
    return meta;
}

extern int
dsv_dec(DSV_DECODER *d, DSV_BUF *buffer, DSV_FRAME **out, DSV_FNUM *fn)
{
    DSV_BS bs;
    DSV_IMAGE *img;
    DSV_PARAMS *p;
    int c, quant, is_ref, pkt_type, subsamp;
    DSV_META *meta = &d->vidmeta;
    DSV_FRAME *residual;
    DSV_MV *mvs = NULL;
    DSV_FNUM fno;
    DSV_STABILITY stab;

    *fn = -1;
    
    dsv_bs_init(&bs, buffer->data);
    pkt_type = decode_packet_hdr(&bs);
    
    if (pkt_type == -1) {
        dsv_buf_free(buffer);
        return DSV_DEC_ERROR;
    }
    
    if (!DSV_PT_IS_PIC(pkt_type)) {
        int ret = DSV_DEC_ERROR;
        switch (pkt_type) {
            case DSV_PT_META:
                DSV_DEBUG(("decoding metadata"));
                decode_meta(d, &bs);
                d->got_metadata = 1;
                ret = DSV_DEC_GOT_META;
                break;
            case DSV_PT_EOS:
                DSV_DEBUG(("decoding end of stream"));
                ret = DSV_DEC_EOS;
                break;
        }
        dsv_buf_free(buffer);
        return ret;
    }

    if (!d->got_metadata) {
        DSV_WARNING(("no metadata, skipping frame"));
        dsv_buf_free(buffer);
        return DSV_DEC_OK;
    }

    img = dsv_alloc(sizeof(DSV_IMAGE));
    img->refcount = 1;
        
    img->params.vidmeta = meta;
    
    subsamp = meta->subsamp;
        
    p = &img->params;
    
    p->has_ref = DSV_PT_HAS_REF(pkt_type);
    is_ref = DSV_PT_IS_REF(pkt_type);
    
    dsv_bs_align(&bs);
    
    fno = dsv_bs_get_bits(&bs, 32);
    
    dsv_bs_align(&bs);
    
    p->blk_w = dsv_bs_get_ueg(&bs) << 2;
    p->blk_h = dsv_bs_get_ueg(&bs) << 2;

    if (p->blk_w < DSV_MIN_BLOCK_SIZE || p->blk_h < DSV_MIN_BLOCK_SIZE || 
        p->blk_w > DSV_MAX_BLOCK_SIZE || p->blk_h > DSV_MAX_BLOCK_SIZE) {
        dsv_buf_free(buffer);
        return DSV_DEC_ERROR;
    }
    p->nblocks_h = DSV_DIV_ROUND(meta->width, p->blk_w);
    p->nblocks_v = DSV_DIV_ROUND(meta->height, p->blk_h);

    img->stable_blocks = dsv_alloc(p->nblocks_h * p->nblocks_v);
    decode_stability_blocks(img, &bs, buffer);
    if (p->has_ref) {
        mvs = dsv_alloc(sizeof(DSV_MV) * p->nblocks_h * p->nblocks_v);
        decode_motion(img, mvs, &bs, buffer);
    }
    residual = dsv_mk_frame(subsamp, meta->width, meta->height, 1);
    
    /* B.2.3.3 Image Data */
    dsv_bs_align(&bs);
    quant = dsv_bs_get_bits(&bs, DSV_MAX_QP_BITS);

    stab.params = p;
    stab.stable_blocks = img->stable_blocks;
    stab.isP = p->has_ref;

    /* B.2.3.3 Image Data - Plane Decoding */
    for (c = 0; c < 3; c++) {
        uint8_t *encoded_buf;
        DSV_COEFS coefs;
        int plen, framesz;
        
        dsv_bs_align(&bs);
        
        plen = dsv_bs_get_bits(&bs, 32);
        
        dsv_bs_align(&bs);
        if (c > 0) {
            coefs.width = DSV_ROUND_POW2(residual->planes[c].w, 1);
            coefs.height = DSV_ROUND_POW2(residual->planes[c].h, 1);
        } else {
            coefs.width = residual->planes[c].w;
            coefs.height = residual->planes[c].h;
        }

        framesz = coefs.width * coefs.height * sizeof(int);        
        if (plen <= 0 || plen > (framesz * 2)) {
            DSV_ERROR(("plane length was strange: %d", plen));
            break;
        }
        encoded_buf = buffer->data + dsv_bs_ptr(&bs);
        dsv_bs_skip(&bs, plen);
    
        coefs.data = dsv_alloc(framesz);
        stab.cur_plane = c;
        dsv_decode_plane(encoded_buf, plen, &coefs, quant, &stab);
        dsv_inv_sbt(&residual->planes[c], &coefs, quant, stab.isP, c);
        if (coefs.data) {
            dsv_free(coefs.data);
        }
    }

    *fn = fno;

    img->refcount++;

    if (!img->out_frame) {
        img->out_frame = dsv_mk_frame(subsamp, meta->width, meta->height, 1);
    }

    if (p->has_ref) {
        DSV_IMAGE *ref = d->ref;
        if (ref == NULL) {
            DSV_WARNING(("reference frame not found"));
            return DSV_DEC_ERROR;
        }

#if 0 /* SHOW RESIDUAL */
        dsv_frame_copy(img->out_frame, residual);
#else
        dsv_add_pred(mvs, p, residual, img->out_frame, ref->ref_frame);
#endif
    } else {
        dsv_frame_copy(img->out_frame, residual);
    }

    if (is_ref) {
        img->ref_frame = dsv_extend_frame(dsv_frame_ref_inc(img->out_frame));  
    }

    /* draw debug information on the frame */
    if (d->draw_info && p->has_ref) {
        DSV_FRAME *tmp = dsv_clone_frame(img->out_frame, 0);
        draw_info(img, tmp, mvs, d->draw_info);
        dsv_frame_ref_dec(img->out_frame);
        img->out_frame = tmp;
    }
    /* release resources */
    if (is_ref) {
        if (d->ref) {
            img_unref(d->ref);
        }
        img->refcount++;
        d->ref = img;
    }
    
    dsv_frame_ref_dec(residual);
    if (mvs) {
        dsv_free(mvs);
    }
    if (buffer) {
        dsv_buf_free(buffer);
    }
        
    img_unref(img);
    
    *out = dsv_frame_ref_inc(img->out_frame);
    
    img_unref(img);
    return DSV_DEC_OK;
}

#endif /* decoder impl guard */
#endif /* decoder include */

/******/

#endif /* dsv1 impl guard */
#endif /* dsv1 impl */

#ifdef __cplusplus
}
#endif

#endif /* _DSV1_H_ */

