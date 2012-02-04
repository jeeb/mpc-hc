/*
 * Misc image conversion routines
 * Copyright (c) 2001, 2002, 2003 Fabrice Bellard
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * misc image conversion routines
 */

/* TODO:
 * - write 'ffimg' program to test all the image related stuff
 * - move all api to slice based system
 * - integrate deinterlacing, postprocessing and scaling in the conversion process
 */

#include "avcodec.h"
#include "dsputil.h"
#include "internal.h"
#include "imgconvert.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"

#if HAVE_MMX && HAVE_YASM
#include "x86/dsputil_mmx.h"
#endif

#define FF_COLOR_RGB      0 /**< RGB color space */
#define FF_COLOR_GRAY     1 /**< gray color space */
#define FF_COLOR_YUV      2 /**< YUV color space. 16 <= Y <= 235, 16 <= U, V <= 240 */
#define FF_COLOR_YUV_JPEG 3 /**< YUV color space. 0 <= Y <= 255, 0 <= U, V <= 255 */

typedef struct PixFmtInfo {
    uint8_t color_type;      /**< color type (see FF_COLOR_xxx constants) */
    uint8_t is_alpha : 1;    /**< true if alpha can be specified */
    uint8_t padded_size;     /**< padded size in bits if different from the non-padded size */
} PixFmtInfo;

/* this table gives more information about formats */
static const PixFmtInfo pix_fmt_info[PIX_FMT_NB] = {
    /* YUV formats */
    [PIX_FMT_YUV420P] = {
        .color_type = FF_COLOR_YUV,
    },
    [PIX_FMT_YUV422P] = {
        .color_type = FF_COLOR_YUV,
    },
    [PIX_FMT_YUV444P] = {
        .color_type = FF_COLOR_YUV,
    },
    [PIX_FMT_YUYV422] = {
        .color_type = FF_COLOR_YUV,
    },
    [PIX_FMT_UYVY422] = {
        .color_type = FF_COLOR_YUV,
    },
    [PIX_FMT_YUV410P] = {
        .color_type = FF_COLOR_YUV,
    },
    [PIX_FMT_YUV411P] = {
        .color_type = FF_COLOR_YUV,
    },
    [PIX_FMT_YUV440P] = {
        .color_type = FF_COLOR_YUV,
    },
    [PIX_FMT_YUV420P16LE] = {
        .color_type = FF_COLOR_YUV,
    },
    [PIX_FMT_YUV422P16LE] = {
        .color_type = FF_COLOR_YUV,
    },
    [PIX_FMT_YUV444P16LE] = {
        .color_type = FF_COLOR_YUV,
    },
    [PIX_FMT_YUV420P16BE] = {
        .color_type = FF_COLOR_YUV,
    },
    [PIX_FMT_YUV422P16BE] = {
        .color_type = FF_COLOR_YUV,
    },
    [PIX_FMT_YUV444P16BE] = {
        .color_type = FF_COLOR_YUV,
    },

    /* YUV formats with alpha plane */
    [PIX_FMT_YUVA420P] = {
        .is_alpha = 1,
        .color_type = FF_COLOR_YUV,
    },

    [PIX_FMT_YUVA444P] = {
        .is_alpha = 1,
        .color_type = FF_COLOR_YUV,
    },

    /* JPEG YUV */
    [PIX_FMT_YUVJ420P] = {
        .color_type = FF_COLOR_YUV_JPEG,
    },
    [PIX_FMT_YUVJ422P] = {
        .color_type = FF_COLOR_YUV_JPEG,
    },
    [PIX_FMT_YUVJ444P] = {
        .color_type = FF_COLOR_YUV_JPEG,
    },
    [PIX_FMT_YUVJ440P] = {
        .color_type = FF_COLOR_YUV_JPEG,
    },

    /* RGB formats */
    [PIX_FMT_RGB24] = {
        .color_type = FF_COLOR_RGB,
    },
    [PIX_FMT_BGR24] = {
        .color_type = FF_COLOR_RGB,
    },
    [PIX_FMT_ARGB] = {
        .is_alpha = 1,
        .color_type = FF_COLOR_RGB,
    },
    [PIX_FMT_RGB48BE] = {
        .color_type = FF_COLOR_RGB,
    },
    [PIX_FMT_RGB48LE] = {
        .color_type = FF_COLOR_RGB,
    },
    [PIX_FMT_RGBA64BE] = {
        .is_alpha = 1,
        .color_type = FF_COLOR_RGB,
    },
    [PIX_FMT_RGBA64LE] = {
        .is_alpha = 1,
        .color_type = FF_COLOR_RGB,
    },
    [PIX_FMT_RGB565BE] = {
        .color_type = FF_COLOR_RGB,
    },
    [PIX_FMT_RGB565LE] = {
        .color_type = FF_COLOR_RGB,
    },
    [PIX_FMT_RGB555BE] = {
        .color_type = FF_COLOR_RGB,
        .padded_size = 16,
    },
    [PIX_FMT_RGB555LE] = {
        .color_type = FF_COLOR_RGB,
        .padded_size = 16,
    },
    [PIX_FMT_RGB444BE] = {
        .color_type = FF_COLOR_RGB,
        .padded_size = 16,
    },
    [PIX_FMT_RGB444LE] = {
        .color_type = FF_COLOR_RGB,
        .padded_size = 16,
    },

    /* gray / mono formats */
    [PIX_FMT_GRAY16BE] = {
        .color_type = FF_COLOR_GRAY,
    },
    [PIX_FMT_GRAY16LE] = {
        .color_type = FF_COLOR_GRAY,
    },
    [PIX_FMT_GRAY8] = {
        .color_type = FF_COLOR_GRAY,
    },
    [PIX_FMT_GRAY8A] = {
        .is_alpha = 1,
        .color_type = FF_COLOR_GRAY,
    },
    [PIX_FMT_MONOWHITE] = {
        .color_type = FF_COLOR_GRAY,
    },
    [PIX_FMT_MONOBLACK] = {
        .color_type = FF_COLOR_GRAY,
    },

    /* paletted formats */
    [PIX_FMT_PAL8] = {
        .is_alpha = 1,
        .color_type = FF_COLOR_RGB,
    },
    [PIX_FMT_UYYVYY411] = {
        .color_type = FF_COLOR_YUV,
    },
    [PIX_FMT_ABGR] = {
        .is_alpha = 1,
        .color_type = FF_COLOR_RGB,
    },
    [PIX_FMT_BGR48BE] = {
        .color_type = FF_COLOR_RGB,
    },
    [PIX_FMT_BGR48LE] = {
        .color_type = FF_COLOR_RGB,
    },
    [PIX_FMT_BGRA64BE] = {
        .is_alpha = 1,
        .color_type = FF_COLOR_RGB,
    },
    [PIX_FMT_BGRA64LE] = {
        .is_alpha = 1,
        .color_type = FF_COLOR_RGB,
    },
    [PIX_FMT_BGR565BE] = {
        .color_type = FF_COLOR_RGB,
        .padded_size = 16,
    },
    [PIX_FMT_BGR565LE] = {
        .color_type = FF_COLOR_RGB,
        .padded_size = 16,
    },
    [PIX_FMT_BGR555BE] = {
        .color_type = FF_COLOR_RGB,
        .padded_size = 16,
    },
    [PIX_FMT_BGR555LE] = {
        .color_type = FF_COLOR_RGB,
        .padded_size = 16,
    },
    [PIX_FMT_BGR444BE] = {
        .color_type = FF_COLOR_RGB,
        .padded_size = 16,
    },
    [PIX_FMT_BGR444LE] = {
        .color_type = FF_COLOR_RGB,
        .padded_size = 16,
    },
    [PIX_FMT_RGB8] = {
        .color_type = FF_COLOR_RGB,
    },
    [PIX_FMT_RGB4] = {
        .color_type = FF_COLOR_RGB,
    },
    [PIX_FMT_RGB4_BYTE] = {
        .color_type = FF_COLOR_RGB,
        .padded_size = 8,
    },
    [PIX_FMT_BGR8] = {
        .color_type = FF_COLOR_RGB,
    },
    [PIX_FMT_BGR4] = {
        .color_type = FF_COLOR_RGB,
    },
    [PIX_FMT_BGR4_BYTE] = {
        .color_type = FF_COLOR_RGB,
        .padded_size = 8,
    },
    [PIX_FMT_NV12] = {
        .color_type = FF_COLOR_YUV,
    },
    [PIX_FMT_NV21] = {
        .color_type = FF_COLOR_YUV,
    },

    [PIX_FMT_BGRA] = {
        .is_alpha = 1,
        .color_type = FF_COLOR_RGB,
    },
    [PIX_FMT_RGBA] = {
        .is_alpha = 1,
        .color_type = FF_COLOR_RGB,
    },
};

void avcodec_get_chroma_sub_sample(enum PixelFormat pix_fmt, int *h_shift, int *v_shift)
{
    *h_shift = av_pix_fmt_descriptors[pix_fmt].log2_chroma_w;
    *v_shift = av_pix_fmt_descriptors[pix_fmt].log2_chroma_h;
}

int ff_is_hwaccel_pix_fmt(enum PixelFormat pix_fmt)
{
    return av_pix_fmt_descriptors[pix_fmt].flags & PIX_FMT_HWACCEL;
}

static int avg_bits_per_pixel(enum PixelFormat pix_fmt)
{
    const PixFmtInfo *info = &pix_fmt_info[pix_fmt];
    const AVPixFmtDescriptor *desc = &av_pix_fmt_descriptors[pix_fmt];

    return info->padded_size ?
        info->padded_size : av_get_bits_per_pixel(desc);
}

void av_picture_copy(AVPicture *dst, const AVPicture *src,
                     enum PixelFormat pix_fmt, int width, int height)
{
    av_image_copy(dst->data, dst->linesize, src->data,
                  src->linesize, pix_fmt, width, height);
}

/* 2x2 -> 1x1 */
void ff_shrink22(uint8_t *dst, int dst_wrap,
                     const uint8_t *src, int src_wrap,
                     int width, int height)
{
    int w;
    const uint8_t *s1, *s2;
    uint8_t *d;

    for(;height > 0; height--) {
        s1 = src;
        s2 = s1 + src_wrap;
        d = dst;
        for(w = width;w >= 4; w-=4) {
            d[0] = (s1[0] + s1[1] + s2[0] + s2[1] + 2) >> 2;
            d[1] = (s1[2] + s1[3] + s2[2] + s2[3] + 2) >> 2;
            d[2] = (s1[4] + s1[5] + s2[4] + s2[5] + 2) >> 2;
            d[3] = (s1[6] + s1[7] + s2[6] + s2[7] + 2) >> 2;
            s1 += 8;
            s2 += 8;
            d += 4;
        }
        for(;w > 0; w--) {
            d[0] = (s1[0] + s1[1] + s2[0] + s2[1] + 2) >> 2;
            s1 += 2;
            s2 += 2;
            d++;
        }
        src += 2 * src_wrap;
        dst += dst_wrap;
    }
}

/* 4x4 -> 1x1 */
void ff_shrink44(uint8_t *dst, int dst_wrap,
                     const uint8_t *src, int src_wrap,
                     int width, int height)
{
    int w;
    const uint8_t *s1, *s2, *s3, *s4;
    uint8_t *d;

    for(;height > 0; height--) {
        s1 = src;
        s2 = s1 + src_wrap;
        s3 = s2 + src_wrap;
        s4 = s3 + src_wrap;
        d = dst;
        for(w = width;w > 0; w--) {
            d[0] = (s1[0] + s1[1] + s1[2] + s1[3] +
                    s2[0] + s2[1] + s2[2] + s2[3] +
                    s3[0] + s3[1] + s3[2] + s3[3] +
                    s4[0] + s4[1] + s4[2] + s4[3] + 8) >> 4;
            s1 += 4;
            s2 += 4;
            s3 += 4;
            s4 += 4;
            d++;
        }
        src += 4 * src_wrap;
        dst += dst_wrap;
    }
}

/* 8x8 -> 1x1 */
void ff_shrink88(uint8_t *dst, int dst_wrap,
                     const uint8_t *src, int src_wrap,
                     int width, int height)
{
    int w, i;

    for(;height > 0; height--) {
        for(w = width;w > 0; w--) {
            int tmp=0;
            for(i=0; i<8; i++){
                tmp += src[0] + src[1] + src[2] + src[3] + src[4] + src[5] + src[6] + src[7];
                src += src_wrap;
            }
            *(dst++) = (tmp + 32)>>6;
            src += 8 - 8*src_wrap;
        }
        src += 8*src_wrap - 8*width;
        dst += dst_wrap - width;
    }
}
