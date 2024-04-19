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

#include "dsv_internal.h"

static uint8_t
clamp_u8(int v)
{
    return v > 255 ? 255 : v < 0 ? 0 : v;
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

/* D.1.2 Chroma Half-Pixel Filter */
static void
hpel(uint8_t *dec, uint8_t *ref, int xh, int yh, int dw, int rw, int w, int h)
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
                (c == 0 ? hpelL : hpel)
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

extern void
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

extern void
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

extern void
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
