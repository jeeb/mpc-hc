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

#pragma once

class CCpuID {
public:
    CCpuID();
    enum flag_t {mmx=1, ssemmx=2, ssefpu=4, sse2=8, _3dnow=16} m_flags;
};
extern CCpuID g_cpuid;

extern bool BitBltFromI420ToI420(int w, int h, BYTE *dsty, BYTE *dstu, BYTE *dstv, ptrdiff_t dstpitch, BYTE const *srcy, BYTE const *srcu, BYTE const *srcv, ptrdiff_t srcpitch);
extern bool BitBltFromI420ToNV12(int w, int h, BYTE *dsty, BYTE *dstuv, ptrdiff_t dstpitch, BYTE const *srcy, BYTE const *srcu, BYTE const *srcv, ptrdiff_t srcpitch);
extern bool BitBltFromNV12ToNV12(int w, int h, BYTE *dsty, BYTE *dstuv, ptrdiff_t dstpitch, BYTE const *srcy, BYTE const *srcuv, ptrdiff_t srcpitch);
extern bool BitBltFromI420ToYUY2(int w, int h, BYTE *dst, ptrdiff_t dstpitch, BYTE const *srcy, BYTE const *srcu, BYTE const *srcv, ptrdiff_t srcpitch);
extern bool BitBltFromI420ToYUY2Interlaced(int w, int h, BYTE *dst, ptrdiff_t dstpitch, BYTE const *srcy, BYTE const *srcu, BYTE const *srcv, ptrdiff_t srcpitch);
extern bool BitBltFromI420ToRGB(int w, int h, BYTE *dst, ptrdiff_t dstpitch, BYTE dbpp, BYTE const *srcy, BYTE const *srcu, BYTE const *srcv, ptrdiff_t srcpitch /* TODO: , bool fInterlaced = false */);
extern bool BitBltFromYUY2ToYUY2(int w, int h, BYTE *dst, ptrdiff_t dstpitch, BYTE const *src, ptrdiff_t srcpitch);
extern bool BitBltFromYUY2ToRGB(int w, int h, BYTE *dst, ptrdiff_t dstpitch, BYTE dbpp, BYTE const *src, ptrdiff_t srcpitch);
extern bool BitBltFromRGBToRGB(int w, int h, BYTE *dst, ptrdiff_t dstpitch, BYTE dbpp, BYTE const *src, ptrdiff_t srcpitch, BYTE sbpp);

extern void DeinterlaceBlend(void *dst, void const *src, unsigned __int32 w, unsigned __int32 h, ptrdiff_t dstpitch, ptrdiff_t srcpitch);
extern void DeinterlaceBob(void *dst, void const *src, unsigned __int32 w, unsigned __int32 h, ptrdiff_t dstpitch, ptrdiff_t srcpitch, bool topfield);
extern void DeinterlaceELA_X8R8G8B8(void *dst, void const *src, unsigned __int32 w, unsigned __int32 h, ptrdiff_t dstpitch, ptrdiff_t srcpitch, bool topfield);
extern void DeinterlaceELA(void *dst, void const *src, unsigned __int32 w, unsigned __int32 h, ptrdiff_t dstpitch, ptrdiff_t srcpitch, bool topfield);
