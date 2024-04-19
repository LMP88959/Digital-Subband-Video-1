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

#ifndef _UTIL_H_
#define _UTIL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "dsv.h"

/* compute approximate bitrate for desired quality
 *
 * a quality of 80 for a CIF format video (4:2:0)
 *  at 30 fps will get you 1.0 MBits/s
 */
extern unsigned estimate_bitrate(int quality, int gop, DSV_META *md);

extern void conv444to422(DSV_PLANE *srcf, DSV_PLANE *dstf);
extern void conv422to420(DSV_PLANE *srcf, DSV_PLANE *dstf);

#ifdef __cplusplus
}
#endif

#endif
