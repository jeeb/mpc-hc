//  VirtualDub - Video processing and capture application
//  Graphics support library
//  Copyright (C) 1998-2007 Avery Lee
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
//  Notes:
//  - VDPixmapBlt is from VirtualDub
//  - sse2 yv12 to yuy2 conversion by Haali
//  (- vd.cpp/h should be renamed to something more sensible already :)


#include "stdafx.h"
#include "vd.h"
#include "vd_asm.h"
#include <intrin.h>

#include "vd2/system/cpuaccel.h"
#include "vd2/system/memory.h"
#include "vd2/system/vdstl.h"

#include "vd2/Kasumi/pixmap.h"
#include "vd2/Kasumi/pixmaputils.h"
#include "vd2/Kasumi/pixmapops.h"

#pragma warning(disable : 4799) // no emms... blahblahblah

void VDCPUTest() {
    SYSTEM_INFO si;

    long lEnableFlags = CPUCheckForExtensions();

    GetSystemInfo(&si);

    if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL)
        if (si.wProcessorLevel < 4)
            lEnableFlags &= ~CPUF_SUPPORTS_FPU;     // Not strictly true, but very slow anyway

    // Enable FPU support...

    CPUEnableExtensions(lEnableFlags);

    VDFastMemcpyAutodetect();
}

CCpuID g_cpuid;

CCpuID::CCpuID()
{
    VDCPUTest();

    long lEnableFlags = CPUGetEnabledExtensions();

    int flags = 0;
    flags |= !!(lEnableFlags & CPUF_SUPPORTS_MMX)           ? mmx       : 0;            // STD MMX
    flags |= !!(lEnableFlags & CPUF_SUPPORTS_INTEGER_SSE)   ? ssemmx    : 0;            // SSE MMX
    flags |= !!(lEnableFlags & CPUF_SUPPORTS_SSE)           ? ssefpu    : 0;            // STD SSE
    flags |= !!(lEnableFlags & CPUF_SUPPORTS_SSE2)          ? sse2      : 0;            // SSE2
    flags |= !!(lEnableFlags & CPUF_SUPPORTS_3DNOW)         ? _3dnow    : 0;            // 3DNow

    // result
    m_flags = (flag_t)flags;
}

bool BitBltFromI420ToI420(int w, int h, BYTE *dsty, BYTE *dstu, BYTE *dstv, ptrdiff_t dstpitch, BYTE const *srcy, BYTE const *srcu, BYTE const *srcv, ptrdiff_t srcpitch)
{
    VDPixmap srcbm = {const_cast<BYTE *>(srcy), NULL, w, h, srcpitch, nsVDPixmap::kPixFormat_YUV420_Planar, const_cast<BYTE *>(srcu), srcpitch>>1, const_cast<BYTE *>(srcv), srcpitch>>1};
    VDPixmap dstpxm = {dsty, NULL, w, h, dstpitch, nsVDPixmap::kPixFormat_YUV420_Planar, dstu, dstpitch>>1, dstv, dstpitch>>1};
    return VDPixmapBlt(dstpxm, srcbm);
}

bool BitBltFromI420ToNV12(int w, int h, BYTE *dsty, BYTE *dstuv, ptrdiff_t dstpitch, BYTE const *srcy, BYTE const *srcu, BYTE const *srcv, ptrdiff_t srcpitch)
{
    VDPixmap srcbm = {const_cast<BYTE *>(srcy), NULL, w, h, srcpitch, nsVDPixmap::kPixFormat_YUV420_Planar, const_cast<BYTE *>(srcu), srcpitch>>1, const_cast<BYTE *>(srcv), srcpitch>>1};
    VDPixmap dstpxm = {dsty, NULL, w, h, dstpitch, nsVDPixmap::kPixFormat_YUV420_NV12, dstuv, dstpitch};
    return VDPixmapBlt(dstpxm, srcbm);
}

bool BitBltFromNV12ToNV12(int w, int h, BYTE *dsty, BYTE *dstuv, ptrdiff_t dstpitch, BYTE const *srcy, BYTE const *srcuv, ptrdiff_t srcpitch)
{
    VDPixmap srcbm = {const_cast<BYTE *>(srcy), NULL, w, h, srcpitch, nsVDPixmap::kPixFormat_YUV420_NV12, const_cast<BYTE *>(srcuv), srcpitch};
    VDPixmap dstpxm = {dsty, NULL, w, h, dstpitch, nsVDPixmap::kPixFormat_YUV420_NV12, dstuv, dstpitch};
    return VDPixmapBlt(dstpxm, srcbm);
}

bool BitBltFromYUY2ToYUY2(int w, int h, BYTE *dst, ptrdiff_t dstpitch, BYTE const *src, ptrdiff_t srcpitch)
{
    VDPixmap srcbm = {const_cast<BYTE *>(src), NULL, w, h, srcpitch, nsVDPixmap::kPixFormat_YUV422_YUYV};
    VDPixmap dstpxm = {dst, NULL, w, h, dstpitch, nsVDPixmap::kPixFormat_YUV422_YUYV};
    return VDPixmapBlt(dstpxm, srcbm);
}

bool BitBltFromI420ToRGB(int w, int h, BYTE *dst, ptrdiff_t dstpitch, BYTE dbpp, BYTE const *srcy, BYTE const *srcu, BYTE const *srcv, ptrdiff_t srcpitch)
{
    VDPixmap srcbm = {const_cast<BYTE *>(srcy), NULL, w, h, srcpitch, nsVDPixmap::kPixFormat_YUV420_Planar, const_cast<BYTE *>(srcu), srcpitch>>1, const_cast<BYTE *>(srcv), srcpitch>>1};
    VDPixmap dstpxm = {dst+dstpitch*(h-1), NULL, w, h, -dstpitch,
        (dbpp == 16)? nsVDPixmap::kPixFormat_RGB565 : (dbpp == 24)? nsVDPixmap::kPixFormat_RGB888 : nsVDPixmap::kPixFormat_XRGB8888};
    return VDPixmapBlt(dstpxm, srcbm);
}

bool BitBltFromI420ToYUY2(int w, int h, BYTE *dst, ptrdiff_t dstpitch, BYTE const *srcy, BYTE const *srcu, BYTE const *srcv, ptrdiff_t srcpitch)
{
    if (srcpitch == 0) srcpitch = w;

#ifndef _WIN64
    if ((g_cpuid.m_flags & CCpuID::sse2)
        && !((DWORD_PTR)srcy&15) && !((DWORD_PTR)srcu&15) && !((DWORD_PTR)srcv&15) && !(srcpitch&31)
        && !((DWORD_PTR)dst&15) && !(dstpitch&15))
    {
        if (w<=0 || h<=0 || (w&1) || (h&1))
            return false;

        yv12_yuy2_sse2(srcy, srcu, srcv, srcpitch >> 1, w >> 1, h, dst, dstpitch);
        return true;
    }
#endif

    VDPixmap srcbm = {const_cast<BYTE *>(srcy), NULL, w, h, srcpitch, nsVDPixmap::kPixFormat_YUV420_Planar, const_cast<BYTE *>(srcu), srcpitch>>1, const_cast<BYTE *>(srcv), srcpitch>>1};
    VDPixmap dstpxm = {dst, NULL, w, h, dstpitch, nsVDPixmap::kPixFormat_YUV422_YUYV};
    return VDPixmapBlt(dstpxm, srcbm);
}

bool BitBltFromRGBToRGB(int w, int h, BYTE *dst, ptrdiff_t dstpitch, BYTE dbpp, BYTE const *src, ptrdiff_t srcpitch, BYTE sbpp)
{
    VDPixmap srcbm = {const_cast<BYTE *>(src), NULL, w, h, srcpitch,
        (sbpp == 16)? nsVDPixmap::kPixFormat_RGB565 : (sbpp == 24)? nsVDPixmap::kPixFormat_RGB888 : nsVDPixmap::kPixFormat_XRGB8888};
    VDPixmap dstpxm = {dst, NULL, w, h, dstpitch,
        (dbpp == 16)? nsVDPixmap::kPixFormat_RGB565 : (dbpp == 24)? nsVDPixmap::kPixFormat_RGB888 : nsVDPixmap::kPixFormat_XRGB8888};
    return VDPixmapBlt(dstpxm, srcbm);
}

bool BitBltFromYUY2ToRGB(int w, int h, BYTE *dst, ptrdiff_t dstpitch, BYTE dbpp, BYTE const *src, ptrdiff_t srcpitch)
{
    if (srcpitch == 0) srcpitch = w;

    VDPixmap srcbm = {const_cast<BYTE *>(src), NULL, w, h, srcpitch, nsVDPixmap::kPixFormat_YUV422_YUYV};
    VDPixmap dstpxm = {dst+dstpitch*(h-1), NULL, w, h, -dstpitch,
        (dbpp == 16)? nsVDPixmap::kPixFormat_RGB565 : (dbpp == 24)? nsVDPixmap::kPixFormat_RGB888 : nsVDPixmap::kPixFormat_XRGB8888};
    return VDPixmapBlt(dstpxm, srcbm);
}

static void yuvtoyuy2row_c(BYTE *dst, BYTE const *srcy, BYTE const *srcu, BYTE const *srcv, size_t width)
{
    WORD* dstw = (WORD*)dst;
    for (; width > 1; width -= 2)
    {
        *dstw++ = (*srcu++<<8)|*srcy++;
        *dstw++ = (*srcv++<<8)|*srcy++;
    }
}

static void yuvtoyuy2row_avg_c(BYTE *dst, BYTE const *srcy, BYTE const *srcu, BYTE const *srcv, size_t width, ptrdiff_t pitchuv)
{
    WORD* dstw = (WORD*)dst;
    for (; width > 1; width -= 2, srcu++, srcv++)
    {
        *dstw++ = (((srcu[0]+srcu[pitchuv])>>1)<<8)|*srcy++;
        *dstw++ = (((srcv[0]+srcv[pitchuv])>>1)<<8)|*srcy++;
    }
}

bool BitBltFromI420ToYUY2Interlaced(int w, int h, BYTE *dst, ptrdiff_t dstpitch, BYTE const *srcy, BYTE const *srcu, BYTE const *srcv, ptrdiff_t srcpitch)
{
    if (w <= 0 || h <= 0 || (w & 1) || (h & 1)) {
        return false;
    }

    if (srcpitch == 0) {
        srcpitch = w;
    }

#ifndef _WIN64
    if ((g_cpuid.m_flags & CCpuID::sse2)
        && !((DWORD_PTR)srcy & 15) && !((DWORD_PTR)srcu & 15) && !((DWORD_PTR)srcv & 15) && !(srcpitch & 31) 
        && !((DWORD_PTR)dst & 15) && !(dstpitch & 15))
    {
        yv12_yuy2_sse2_interlaced(srcy, srcu, srcv, srcpitch >> 1, w >> 1, h, dst, dstpitch);
        return true;
    }

    void (*yuvtoyuy2row)(BYTE *dst, BYTE const *srcy, BYTE const *srcu, BYTE const *srcv, size_t width);
    void (*yuvtoyuy2row_avg)(BYTE *dst, BYTE const *srcy, BYTE const *srcu, BYTE const *srcv, size_t width, ptrdiff_t pitchuv);
    if (!(w & 7))
    {
        yuvtoyuy2row = yuvtoyuy2row_MMX;
        yuvtoyuy2row_avg = yuvtoyuy2row_avg_MMX;
    }
    else
    {
        yuvtoyuy2row = yuvtoyuy2row_c;
        yuvtoyuy2row_avg = yuvtoyuy2row_avg_c;
    }
#else
#define yuvtoyuy2row yuvtoyuy2row_c
#define yuvtoyuy2row_avg yuvtoyuy2row_avg_c
#endif

    ptrdiff_t halfsrcpitch = srcpitch >> 1;
    do
    {
        yuvtoyuy2row(dst, srcy, srcu, srcv, w);
        yuvtoyuy2row_avg(dst + dstpitch, srcy + srcpitch, srcu, srcv, w, halfsrcpitch);

        dst += 2*dstpitch;
        srcy += 2*srcpitch;
        srcu += halfsrcpitch;
        srcv += halfsrcpitch;
    }
    while ((h -= 2) > 2);

    yuvtoyuy2row(dst, srcy, srcu, srcv, w);
    yuvtoyuy2row(dst + dstpitch, srcy + srcpitch, srcu, srcv, w);

#ifndef _WIN64
    if (!(w & 7)) {
        _m_empty();
    }
#endif

    return true;
}
