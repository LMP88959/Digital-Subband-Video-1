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

#include "util.h"
#include "dsv_encoder.h"

/* totally based on heuristics */
extern unsigned
estimate_bitrate(int quality, int gop, DSV_META *md)
{
    int bpf; /* bytes per frame */
    int bps; /* bytes per second */
    int maxdimratio;
    int fps;
    
    fps = (md->fps_num + md->fps_den / 2) / md->fps_den;
    switch (md->subsamp) {
        case DSV_SUBSAMP_444:
            bpf = 352 * 288 * 3;
            break;
        case DSV_SUBSAMP_422:
            bpf = 352 * 288 * 2;
            break;
        case DSV_SUBSAMP_420:
        case DSV_SUBSAMP_411:
            bpf = 352 * 288 * 3 / 2;
            break;
    }
    if (gop == DSV_GOP_INTRA) {
        bpf *= 4;
    }
    if (md->width < 320 && md->height < 240) {
        bpf /= 4;
    }
    maxdimratio = (((md->width + md->height) / 2) << 8) / 352;
    bpf = bpf * maxdimratio >> 8;
    bps = bpf * fps;
    return (bps / ((26 - quality / 4))) * 3 / 2;
}

extern void
conv444to422(DSV_PLANE *srcf, DSV_PLANE *dstf)
{
    int i, j, w, h, n;
    uint8_t *src, *dst;
    
    w = srcf->w;
    h = srcf->h;
    src = srcf->data;
    dst = dstf->data;
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i += 2) {
            n = (i < w - 1) ? i + 1 : w - 1;
            dst[i >> 1] = (src[i] + src[n] + 1) >> 1;
        }
        src += srcf->stride;
        dst += dstf->stride;
    }
}

extern void
conv422to420(DSV_PLANE *srcf, DSV_PLANE *dstf)
{
    int i, j, w, h, n, s;
    uint8_t *src, *dst;
    
    w = srcf->w;
    h = srcf->h;
    src = srcf->data;
    dst = dstf->data;
    s = srcf->stride;
    for (i = 0; i < w; i++) {
        for (j = 0; j < h; j += 2) {
            n = (j < h - 1) ? j + 1 : h - 1;
            dst[dstf->stride * (j >> 1)] = (src[s * j] + src[s * n] + 1) >> 1;
        }
        src++;
        dst++;
    }
}
