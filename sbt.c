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

/* C.3.1.1 LL Coefficient Scaling */
#define FWD_SCALE(x) ((x) * 4 / 5)
#define INV_SCALE(x) ((x) * 5 / 4)
#define LVL_TEST (isI ? 1 : lvl > 1)

/* Subband Transforms
 * 
 * Biorthogonal 4-tap filter used for highest frequency level in intra frames
 * Haar used for everything else
 */

static DSV_SBC *temp_buf = NULL;
static int temp_bufsz = 0;

static void
alloc_temp(int size)
{
    if (temp_bufsz < size) {
        temp_bufsz = size;
        
        if (temp_buf) {
            dsv_free(temp_buf);
            temp_buf = NULL;
        }

        temp_buf = dsv_alloc(temp_bufsz * sizeof(DSV_SBC));
        if (temp_buf == NULL) {
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
            if (LVL_TEST) {
                dpLL[idx] = FWD_SCALE(x0 + x1 + x2 + x3); /* LL */
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

            if (LVL_TEST) {
                dpLL[idx] = FWD_SCALE(2 * (x0 + x2)); /* LL */
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
            
            if (LVL_TEST) {
                dpLL[idx] = FWD_SCALE(2 * (x0 + x1)); /* LL */
            } else {
                dpLL[idx] = 2 * (x0 + x1); /* LL */
            }
            dpLH[idx] = 2 * (x0 - x1); /* LH */
        }
        if (oddw) {
            x0 = spA[x + 0];
            
            if (LVL_TEST) {
                dpLL[idx] = FWD_SCALE(x0 * 4); /* LL */ 
            } else {
                dpLL[idx] = (x0 * 4); /* LL */ 
            }
        }
    }
    cpysub(os, od, ws, hs, width);
}

/* C.3.1.3 Haar Simple Inverse Transform */
static void
inv_simple(DSV_SBC *src, DSV_SBC *dst, int width, int height, int lvl, int isI)
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
            if (LVL_TEST) {
                LL = INV_SCALE(spLL[idx]);
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
            if (LVL_TEST) {
                LL = INV_SCALE(spLL[idx]);
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
            if (LVL_TEST) {
                LL = INV_SCALE(spLL[idx]);
            } else {
                LL = spLL[idx];
            }
            LH = spLH[idx];
            
            dpA[x + 0] = (LL + LH) / 4; /* LL */
            dpA[x + 1] = (LL - LH) / 4; /* LH */
        }
        if (oddw) {
            if (LVL_TEST) {
                LL = INV_SCALE(spLL[idx]);
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
inv(DSV_SBC *src, DSV_SBC *dst, int width, int height, int lvl, int hqp, int isI)
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

            if (LVL_TEST) {
                LL = INV_SCALE(spLL[idx]);
            } else {
                LL = spLL[idx];
            }
            LH = spLH[idx];
            HL = spHL[idx];
            HH = spHH[idx];

            if (inX) {
                if (LVL_TEST) {
                    lp = INV_SCALE(spLL[idx - 1]); /* prev */
                    ln = INV_SCALE(spLL[idx + 1]); /* next */
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
                if (LVL_TEST) {
                    lp = INV_SCALE(spLL[idx - width]);
                    ln = INV_SCALE(spLL[idx + width]);
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
            if (LVL_TEST) {
                LL = INV_SCALE(spLL[idx]);
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
            if (LVL_TEST) {
                LL = INV_SCALE(spLL[idx]);
            } else {
                LL = spLL[idx];
            }
            LH = spLH[idx];
            
            dpA[x + 0] = (LL + LH) / 4; /* LL */
            dpA[x + 1] = (LL - LH) / 4; /* LH */
        }
        if (oddw) {
            if (LVL_TEST) {
                LL = INV_SCALE(spLL[idx]);
            } else {
                LL = spLL[idx];
            }
            
            dpA[x + 0] = LL / 4; /* LL */
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

extern void
dsv_fwd_sbt(DSV_PLANE *src, DSV_COEFS *dst, int isP)
{
    int lvls, i;
    int w = dst->width;
    int h = dst->height;
    DSV_SBC *temp_buf_pad;

    p2sbc(dst, src);
    
    lvls = nlevels(w, h);

    alloc_temp((w + 2) * (h + 2));
    temp_buf_pad = temp_buf + w;
    for (i = 1; i <= lvls; i++) {
        if (!isP && i == 1) {
            fwd_b4t_2d(temp_buf_pad, dst->data, w, h);
        } else {
            fwd(dst->data, temp_buf_pad, w, h, i, !isP);
        }
    }
}

/* C.3.3 Subband Recomposition */
extern void
dsv_inv_sbt(DSV_PLANE *dst, DSV_COEFS *src, int q, int isP, int c)
{
    int lvls, i;
    int w = src->width;
    int h = src->height;
    DSV_SBC *temp_buf_pad;

    lvls = nlevels(w, h);

    alloc_temp((w + 2) * (h + 2));

    temp_buf_pad = temp_buf + w;
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
                inv(src->data, temp_buf_pad, w, h, i, hqp, !isP);
            }
        }
    } else {
        for (i = lvls; i > 0; i--) {
            if (!isP && i == 1) {
                inv_b4t_2d(temp_buf_pad, src->data, w, h);
            } else {
                inv_simple(src->data, temp_buf_pad, w, h, i, !isP);
            }
        }
    }

    sbc2int(dst, src);
}
