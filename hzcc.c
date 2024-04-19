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

/* Hierarchical Zero Coefficient Coding */

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
extern int
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

extern int
dsv_lb2(unsigned n)
{
    unsigned i = 1, log2 = 0;

    while (i < n) {
        i <<= 1;
        log2++;
    }
    return log2;
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

/* B.2.3.3 Image Data - Coefficient Decoding */
extern void
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
