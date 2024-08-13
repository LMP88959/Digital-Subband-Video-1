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

/* Hierarchical Motion Estimation */

static uint8_t
clamp_u8(int v)
{
    return v > 255 ? 255 : v < 0 ? 0 : v;
}

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
    int b = f->border * DSV_FRAME_BORDER;
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
            static int yf[FPEL_NSEARCH] = { 0,  0,  0, 1, -1, -1, -1,  1, 1 };
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

extern int
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

