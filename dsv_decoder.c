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

#include "dsv_decoder.h"
#include "dsv_internal.h"

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
    dsv_bs_end_rle(&rle, 1);
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
    dsv_bs_end_rle(&qualrle, 1);
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
    int xf_width, xf_height;
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

    dsv_get_xf_dims(meta, &xf_width, &xf_height);
        
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
    residual = dsv_mk_frame(subsamp, xf_width, xf_height, 1);
    
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
        
        coefs.width = residual->planes[c].w;
        coefs.height = residual->planes[c].h;
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
