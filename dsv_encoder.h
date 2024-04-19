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

#ifndef _DSV_ENCODER_H_
#define _DSV_ENCODER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "dsv_internal.h"

#define DSV_GOP_INTRA 0
#define DSV_GOP_INF   INT_MAX

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
    
    int gop;
    int do_scd; /* scene change detection */
    
    /* rate control */
    int rc_mode; /* rate control mode */
    /* 0 = keep quality more or less constant through the entire video.
     * 1 = quantize P frames more in high motion areas. */
    int rc_high_motion_nudge;
    
    /* approximate average bitrate desired */
    unsigned bitrate;
    /* for ABR */
    int max_q_step;
    int min_quality; /* 0...DSV_MAX_QUALITY */
    int max_quality; /* 0...DSV_MAX_QUALITY */
    int min_I_frame_quality; /* 0...DSV_MAX_QUALITY */
    
    int intra_pct_thresh; /* 0-100% */
    int scene_change_delta;
    unsigned stable_refresh; /* # frames after which stability accum resets */
    int pyramid_levels;
    
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
extern void dsv_enc_force_metadata(DSV_ENCODER *enc);

extern void dsv_enc_start(DSV_ENCODER *enc);

/* returns number of buffers available in bufs ptr */
extern int dsv_enc(DSV_ENCODER *enc, DSV_FRAME *frame, DSV_BUF *bufs);
extern void dsv_enc_end_of_stream(DSV_ENCODER *enc, DSV_BUF *bufs);

/* used internally */
typedef struct {
    DSV_PARAMS *params;
    DSV_FRAME *src[DSV_MAX_PYRAMID_LEVELS + 1];
    DSV_FRAME *ref[DSV_MAX_PYRAMID_LEVELS + 1];
    DSV_MV *mvf[DSV_MAX_PYRAMID_LEVELS + 1];
    int levels;
} DSV_HME;

extern int dsv_hme(DSV_HME *hme);

#ifdef __cplusplus
}
#endif

#endif
