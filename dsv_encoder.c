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

#include "dsv_encoder.h"

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
            bytes = dsv_bs_end_rle(&rle, 0);

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
    bytes = dsv_bs_end_rle(&stabrle, 0);
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
encode_picture(DSV_ENCODER *enc, DSV_ENCDATA *d, DSV_BUF *output_buf)
{
    DSV_BS bs;
    unsigned upperbound;
    DSV_STABILITY stab;
    DSV_COEFS coefs[3];
    int i, width, height, xf_width, xf_height;
    
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
    dsv_get_xf_dims(&enc->vidmeta, &xf_width, &xf_height);
    dsv_mk_coefs(coefs, enc->vidmeta.subsamp, xf_width, xf_height);

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
    int w, h, xf_width, xf_height;
    int nbuf = 0;
    DSV_BUF outbuf;

    if (bufs == NULL) {
        DSV_ERROR(("null buffer list passed to encoder!"));
        return 0;
    }
    d = dsv_alloc(sizeof(DSV_ENCDATA));
    
    d->refcount = 1;
    
    dsv_get_xf_dims(&enc->vidmeta, &xf_width, &xf_height);
    d->xf_frame = dsv_mk_frame(enc->vidmeta.subsamp, xf_width, xf_height, 1);
    w = enc->vidmeta.width;
    h = enc->vidmeta.height;
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
