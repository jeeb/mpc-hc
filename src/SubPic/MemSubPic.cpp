/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2013 see Authors.txt
 *
 * This file is part of MPC-HC.
 *
 * MPC-HC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-HC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "stdafx.h"
#include "MemSubPic.h"

// For CPUID usage
#include "../DSUtil/vd.h"
#include <intrin.h>

// color conv

__int32 const c2y_cyb = static_cast<__int32>(0.114 * 219.0 / 255.0 * 65536.0 + 0.5);
__int32 const c2y_cyg = static_cast<__int32>(0.587 * 219.0 / 255.0 * 65536.0 + 0.5);
__int32 const c2y_cyr = static_cast<__int32>(0.299 * 219.0 / 255.0 * 65536.0 + 0.5);
__int32 const c2y_cyb_hd = static_cast<__int32>(0.0722 * 219.0 / 255.0 * 65536.0 + 0.5);
__int32 const c2y_cyg_hd = static_cast<__int32>(0.7152 * 219.0 / 255.0 * 65536.0 + 0.5);
__int32 const c2y_cyr_hd = static_cast<__int32>(0.2126 * 219.0 / 255.0 * 65536.0 + 0.5);
__int32 c2y_cu = static_cast<__int32>(224.0 / 255.0 / 1.772 * 1024.0 + 0.5);
__int32 c2y_cv = static_cast<__int32>(224.0 / 255.0 / 1.402 * 1024.0 + 0.5);

__int32 c2y_yb[256];
__int32 c2y_yg[256];
__int32 c2y_yr[256];

__int32 const y2c_cbu = static_cast<__int32>(1.772 * 255.0 / 224.0 * 65536.0 + 0.5);
__int32 const y2c_cgu = static_cast<__int32>(0.202008 / 0.587 * 255.0 / 224.0 * 65536.0 + 0.5);
__int32 const y2c_cgv = static_cast<__int32>(0.419198 / 0.587 * 255.0 / 224.0 * 65536.0 + 0.5);
__int32 const y2c_crv = static_cast<__int32>(1.402 * 255.0 / 224.0 * 65536.0 + 0.5);
__int32 const y2c_cbu_hd = static_cast<__int32>(1.8556 * 255.0 / 224.0 * 65536.0 + 0.5);
__int32 const y2c_cgu_hd = static_cast<__int32>(0.1674679 / 0.894 * 255.0 / 224.0 * 65536.0 + 0.5);
__int32 const y2c_cgv_hd = static_cast<__int32>(0.4185031 / 0.894 * 255.0 / 224.0 * 65536.0 + 0.5);
__int32 const y2c_crv_hd = static_cast<__int32>(1.5748 * 255.0 / 224.0 * 65536.0 + 0.5);
__int32 y2c_bu[256];
__int32 y2c_gu[256];
__int32 y2c_gv[256];
__int32 y2c_rv[256];

__int32 const cy_cy = static_cast<__int32>(255.0 / 219.0 * 65536.0 + 0.5);
__int32 const cy_cy2 = static_cast<__int32>(255.0 / 219.0 * 32768.0 + 0.5);

bool fColorConvInitOK = false;
bool useBT709 = false;
__declspec(nothrow noalias) void ColorConvInit(__in const bool BT709)
{
    if (fColorConvInitOK && useBT709 == BT709) {
        return;
    }

    if (BT709) {
        c2y_cu = static_cast<__int32>(224.0 / 255.0 / 1.8556 * 1024.0 + 0.5);
        c2y_cv = static_cast<__int32>(224.0 / 255.0 / 1.5748 * 1024.0 + 0.5);
        ptrdiff_t i = 255;
        do {
            __int32 iv = static_cast<__int32>(i);
            c2y_yb[i] = c2y_cyb_hd * iv;
            c2y_yg[i] = c2y_cyg_hd * iv;
            c2y_yr[i] = c2y_cyr_hd * iv;

            __int32 is = static_cast<__int32>(i) - 128;
            y2c_bu[i] = y2c_cbu_hd * is;
            y2c_gu[i] = y2c_cgu_hd * is;
            y2c_gv[i] = y2c_cgv_hd * is;
            y2c_rv[i] = y2c_crv_hd * is;
        } while (--i >= 0);
    } else {
        ptrdiff_t i = 255;
        do {
            __int32 iv = static_cast<__int32>(i);
            c2y_yb[i] = c2y_cyb * iv;
            c2y_yg[i] = c2y_cyg * iv;
            c2y_yr[i] = c2y_cyr * iv;

            __int32 is = i - 128;
            y2c_bu[i] = y2c_cbu * is;
            y2c_gu[i] = y2c_cgu * is;
            y2c_gv[i] = y2c_cgv * is;
            y2c_rv[i] = y2c_crv * is;
        } while (--i >= 0);
    }

    fColorConvInitOK = true;
    useBT709 = BT709;
}

static __declspec(nothrow noalias) __forceinline BYTE Clamp(__int32 n)
{
    // will compile with cmov on both conditions on any decent compiler
    n = (n > 255) ? 255 : n;
    return static_cast<BYTE>((n < 0) ? 0 : n);
}

/*
#define rgb2yuv(r1,g1,b1,r2,g2,b2)                                                                       \
    __int32 y1 = (c2y_yb[b1] + c2y_yg[g1] + c2y_yr[r1] + 0x108000) >> 16;                                \
    __int32 y2 = (c2y_yb[b2] + c2y_yg[g2] + c2y_yr[r2] + 0x108000) >> 16;                                \
                                                                                                         \
    __int32 scaled_y = (y1 + y2 - 32) * cy_cy2;                                                          \
    unsigned __int8 u = Clamp((((((b1+b2) << 15) - scaled_y) >> 10) * c2y_cu + 0x800000 + 0x8000) >> 16);\
    unsigned __int8 v = Clamp((((((r1+r2) << 15) - scaled_y) >> 10) * c2y_cv + 0x800000 + 0x8000) >> 16)
*/

//
// CMemSubPic
//
// CBSubPic

__declspec(nothrow noalias) void CMemSubPic::GetDesc(__out SubPicDesc* pTarget) const
{
    pTarget->type = m_spd.type;
    pTarget->w = m_maxsize.cx;
    pTarget->h = m_maxsize.cy;
    pTarget->bpp = m_spd.bpp;
    pTarget->pitch = m_spd.pitch;
    pTarget->bits = m_spd.bits;
    pTarget->bitsU = m_spd.bitsU;
    pTarget->bitsV = m_spd.bitsV;
    pTarget->vidrect = m_vidrect;
}

__declspec(nothrow noalias) HRESULT CMemSubPic::CopyTo(__out_opt CBSubPic* pSubPic) const
{
    ASSERT(pSubPic);

    if (!(m_rcDirty.right - m_rcDirty.left) || !(m_rcDirty.bottom - m_rcDirty.top)) {
        ASSERT(0);
        return E_ABORT;
    }

    CMemSubPic* pDstSP = static_cast<CMemSubPic*>(pSubPic);
    pDstSP->m_rtStart = m_rtStart;
    pDstSP->m_rtStop = m_rtStop;
    pDstSP->m_rtSegmentStart = m_rtSegmentStart;
    pDstSP->m_rtSegmentStop = m_rtSegmentStop;
    pDstSP->m_rcDirty = m_rcDirty;
    pDstSP->m_vidrect = m_vidrect;
    pDstSP->m_maxsize = m_maxsize;

    // the method here is wrong: the do while loop is very unwelcome as alignment is important, and on top of that, this operation should be done on a custom-sized texture made to fit the contents of the subtitle without padding, so only a linear fill is required
    size_t upWidth = m_rcDirty.right - m_rcDirty.left, upHeight = m_rcDirty.bottom - m_rcDirty.top, upPitch = m_spd.pitch;
    size_t upOffsetC = upPitch * m_rcDirty.top;
    uintptr_t pSrcRow = reinterpret_cast<uintptr_t>(m_spd.bits) + upOffsetC, pDstRow = reinterpret_cast<uintptr_t>(pDstSP->m_spd.bits) + upOffsetC;
    if (m_maxsize.cx - upWidth <= 16) { // linear copy
        upWidth = upHeight * (upPitch >> 2);
        upHeight = 1;
    } else { // copy per row
        size_t uOffsetR = m_rcDirty.left << 2;
        pSrcRow += uOffsetR;
        pDstRow += uOffsetR;
    }
    // note: because the bases of the textures are both at least 2048 byte aligned, the alignment throughout the copy also remains the same

    do {
        uintptr_t pSrc = pSrcRow;
        uintptr_t pDst = pDstRow;
        size_t uCount = upWidth;
        // fill the output to meet 16-byte alignment, sorted for aligned access
        // note: when copying entire textures, the initial realignment fill parts are also useless, the allocator always allocates textures at 16-byte boundaries
        if ((pDst & 4) && uCount) { // 4-to-8-byte alignment
            __int32 y = *reinterpret_cast<__int32*>(pSrc);
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
            _mm_stream_si32(reinterpret_cast<__int32*>(pDst), y);
#else
            *reinterpret_cast<__int32*>(pDst) = y;
#endif
            pSrc += 4, pDst += 4;
            --uCount;
        }
        if ((pDst & 8) && (uCount >= 2)) { // 8-to-16-byte alignment
#ifdef _M_X64
            __int64 y = *reinterpret_cast<__int64*>(pSrc);
            _mm_stream_si64x(reinterpret_cast<__int64*>(pDst), y);
#else
            __int32 y = *reinterpret_cast<__int32*>(pSrc);
            __int32 z = *reinterpret_cast<__int32*>(pSrc + 4);
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds
            _mm_stream_si32(reinterpret_cast<__int32*>(pDst), y);
            _mm_stream_si32(reinterpret_cast<__int32*>(pDst + 4), z);
#else
            *reinterpret_cast<__int32*>(pDst) = y;
            *reinterpret_cast<__int32*>(pDst + 4) = z;
#endif
#endif
            pSrc += 8, pDst += 8;
            uCount -= 2;
        }
        if ((pDst & 16) && (uCount >= 4)) { // 16-to-32-byte alignment
            __m128 x0 = _mm_load_ps(reinterpret_cast<float*>(pSrc));
            _mm_stream_ps(reinterpret_cast<float*>(pDst), x0);
            pSrc += 16, pDst += 16;
            uCount -= 4;
        }
        if ((pDst & 32) && (uCount >= 8)) { // 32-to-64-byte alignment
            __m128 x0 = _mm_load_ps(reinterpret_cast<float*>(pSrc));
            __m128 x1 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 16));
            _mm_stream_ps(reinterpret_cast<float*>(pDst), x0);
            _mm_stream_ps(reinterpret_cast<float*>(pDst + 16), x1);
            pSrc += 32, pDst += 32;
            uCount -= 8;
        }
        if ((pDst & 64) && (uCount >= 16)) { // 64-to-128-byte alignment
            __m128 x0 = _mm_load_ps(reinterpret_cast<float*>(pSrc));
            __m128 x1 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 16));
            __m128 x2 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 32));
            __m128 x3 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 48));
            _mm_stream_ps(reinterpret_cast<float*>(pDst), x0);
            _mm_stream_ps(reinterpret_cast<float*>(pDst + 16), x1);
            _mm_stream_ps(reinterpret_cast<float*>(pDst + 32), x2);
            _mm_stream_ps(reinterpret_cast<float*>(pDst + 48), x3);
            pSrc += 64, pDst += 64;
            uCount -= 16;
        }
        ASSERT((!(pSrc & 127) && !(pDst & 127)) || (uCount < 32)); // if not 128-byte aligned, the next loop will fail

        if (size_t j = uCount >> 5) do { // excludes the last the last 31 optional values (in the bit shift)
                __m128 x0 = _mm_load_ps(reinterpret_cast<float*>(pSrc));
                __m128 x1 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 16));
                __m128 x2 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 32));
                __m128 x3 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 48));
                __m128 x4 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 64));
                __m128 x5 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 80));
                __m128 x6 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 96));
                __m128 x7 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 112));
                _mm_stream_ps(reinterpret_cast<float*>(pDst), x0);
                _mm_stream_ps(reinterpret_cast<float*>(pDst + 16), x1);
                _mm_stream_ps(reinterpret_cast<float*>(pDst + 32), x2);
                _mm_stream_ps(reinterpret_cast<float*>(pDst + 48), x3);
                _mm_stream_ps(reinterpret_cast<float*>(pDst + 64), x4);
                _mm_stream_ps(reinterpret_cast<float*>(pDst + 80), x5);
                _mm_stream_ps(reinterpret_cast<float*>(pDst + 96), x6);
                _mm_stream_ps(reinterpret_cast<float*>(pDst + 112), x7);
                pSrc += 128, pDst += 128;
            } while (--j);

        if (uCount & 16) { // finalize the last 31 optional values, sorted for aligned access
            __m128 x0 = _mm_load_ps(reinterpret_cast<float*>(pSrc));
            __m128 x1 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 16));
            __m128 x2 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 32));
            __m128 x3 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 48));
            _mm_stream_ps(reinterpret_cast<float*>(pDst), x0);
            _mm_stream_ps(reinterpret_cast<float*>(pDst + 16), x1);
            _mm_stream_ps(reinterpret_cast<float*>(pDst + 32), x2);
            _mm_stream_ps(reinterpret_cast<float*>(pDst + 48), x3);
            pSrc += 64, pDst += 64;
        }
        if (uCount & 8) {
            __m128 x0 = _mm_load_ps(reinterpret_cast<float*>(pSrc));
            __m128 x1 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 16));
            _mm_stream_ps(reinterpret_cast<float*>(pDst), x0);
            _mm_stream_ps(reinterpret_cast<float*>(pDst + 16), x1);
            pSrc += 32, pDst += 32;
        }
        if (uCount & 4) {
            __m128 x0 = _mm_load_ps(reinterpret_cast<float*>(pSrc));
            _mm_stream_ps(reinterpret_cast<float*>(pDst), x0);
            pSrc += 16, pDst += 16;
        }
        if (uCount & 2) {
#ifdef _M_X64
            __int64 y = *reinterpret_cast<__int64*>(pSrc);
            _mm_stream_si64x(reinterpret_cast<__int64*>(pDst), y);
#else
            __int32 y = *reinterpret_cast<__int32*>(pSrc);
            __int32 z = *reinterpret_cast<__int32*>(pSrc + 4);
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds
            _mm_stream_si32(reinterpret_cast<__int32*>(pDst), y);
            _mm_stream_si32(reinterpret_cast<__int32*>(pDst + 4), z);
#else
            *reinterpret_cast<__int32*>(pDst) = y;
            *reinterpret_cast<__int32*>(pDst + 4) = z;
#endif
#endif
            pSrc += 8, pDst += 8;
        }
        if (uCount & 1) { // no address increments for the last possible value
            __int32 y = *reinterpret_cast<__int32*>(pSrc);
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
            _mm_stream_si32(reinterpret_cast<__int32*>(pDst), y);
#else
            *reinterpret_cast<__int32*>(pDst) = y;
#endif
        }
        // advance pointers to the next row in the texture
        pSrcRow += upPitch;
        pDstRow += upPitch;
    } while (--upHeight);

    return S_OK;
}

__declspec(nothrow noalias) HRESULT CMemSubPic::LockAndClearDirtyRect(__out_opt SubPicDesc* pTarget)
{
    ASSERT(pTarget);

    GetDesc(pTarget);

    if ((m_rcDirty.right - m_rcDirty.left) && (m_rcDirty.bottom - m_rcDirty.top)) {// an empty status can happen when all subtitle elements move out of view, it's not an error
        // the method here is wrong: the do while loop is very unwelcome as alignment is important, and on top of that, this operation should be done on a custom-sized texture made to fit the contents of the subtitle without padding, so only a linear fill is required
        __declspec(align(16)) static __int32 const iFillVal[4] = {0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000};
        __m128 xFillVal = _mm_load_ps(reinterpret_cast<float const*>(iFillVal));
#ifdef _M_X64// also pre-load the smaller fill values in gpr registers
        __int64 iFillValPad = 0xFF000000FF000000;
#elif _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
        __int32 iFillValPad = 0xFF000000;
#endif
        size_t upWidth = m_rcDirty.right - m_rcDirty.left, upHeight = m_rcDirty.bottom - m_rcDirty.top, upPitch = m_spd.pitch;// TODO: get rid of all RECT and similar objects, all values here are unsigned, the values are unfit to do pointer operations with on x64, and passing structs in function calls isn't a pleasant thing at all
        uintptr_t pRow = reinterpret_cast<uintptr_t>(m_spd.bits) + upPitch * m_rcDirty.top;
        if (m_maxsize.cx - upWidth <= 16) {// linear fill
            upWidth = upHeight * (upPitch >> 2);
            upHeight = 1;
        } else {// fill per row
            pRow += m_rcDirty.left << 2;
        }

        do {
            uintptr_t pDst = pRow;
            size_t uCount = upWidth;
            // fill the output to meet 16-byte alignment, sorted for aligned access
            // note: when clearing entire textures, the initial realignment fill parts are also useless, the driver always allocates textures at 16-byte boundaries
            if ((pDst & 4) && uCount) { // 4-to-8-byte alignment
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
                _mm_stream_si32(reinterpret_cast<__int32*>(pDst), iFillValPad); // x64: copies lower bytes in the register
#else
                _mm_store_ss(reinterpret_cast<float*>(pDst), xFillVal);
#endif
                pDst += 4;
                --uCount;
            }
            if ((pDst & 8) && (uCount >= 2)) { // 8-to-16-byte alignment
#ifdef _M_X64
                _mm_stream_si64x(reinterpret_cast<__int64*>(pDst), iFillValPad);
#elif _M_IX86_FP != 1// SSE2 code, don't use on SSE builds
                _mm_stream_si32(reinterpret_cast<__int32*>(pDst), iFillValPad);
                _mm_stream_si32(reinterpret_cast<__int32*>(pDst + 4), iFillValPad);
#else
                _mm_storel_pi(reinterpret_cast<__m64*>(pDst), xFillVal); // not related to MMX
#endif
                pDst += 8;
                uCount -= 2;
            }
            ASSERT(!(pDst & 15) || (uCount < 4)); // if not 16-byte aligned, _mm_stream_ps will fail

            // excludes the last the last 3 optional values (in the bit shift), as the next function only targets 128-bit fills
            if (size_t i = uCount >> 2) do {
                    _mm_stream_ps(reinterpret_cast<float*>(pDst), xFillVal);
                    pDst += 16;
                } while (--i);

            if (uCount & 2) { // finalize the last 3 optional values, sorted for aligned access
#ifdef _M_X64
                _mm_stream_si64x(reinterpret_cast<__int64*>(pDst), iFillValPad);
#elif _M_IX86_FP != 1// SSE2 code, don't use on SSE builds
                _mm_stream_si32(reinterpret_cast<__int32*>(pDst), iFillValPad);
                _mm_stream_si32(reinterpret_cast<__int32*>(pDst + 4), iFillValPad);
#else
                _mm_storel_pi(reinterpret_cast<__m64*>(pDst), xFillVal); // not related to MMX
#endif
                pDst += 8;
            }
            if (uCount & 1) { // no address increment for the last possible value
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
                _mm_stream_si32(reinterpret_cast<__int32*>(pDst), iFillValPad);// x64: copies lower bytes in the register
#else
                _mm_store_ss(reinterpret_cast<float*>(pDst), xFillVal);
#endif
            }
            pRow += upPitch;// advance pointer to the next row in the texture
        } while (--upHeight);

        ZeroMemory(&m_rcDirty, sizeof(RECT));
    }
    return S_OK;
}

__declspec(nothrow noalias) void CMemSubPic::Unlock(__in RECT const rDirtyRect)
{
    m_rcDirty = rDirtyRect;
    if (!(m_rcDirty.right - m_rcDirty.left) || !(m_rcDirty.bottom - m_rcDirty.top)) {
        return;// an empty status can happen when all subtitle elements move out of view, it's not an error
    }

    if (m_spd.type >= MSP_AYUV) { // YUV colorspace
        ColorConvInit(m_spd.w >= 1120 || m_spd.h >= 630);// detect HD or SD

        if (m_spd.type >= MSP_YUY2) { // 4:2:2 or 4:2:0, round up to even width
            m_rcDirty.left &= ~1;
            m_rcDirty.right = (m_rcDirty.right + 1) & ~1;

            if (m_spd.type >= MSP_YV12) { // 4:2:0, round up to even height
                m_rcDirty.top &= ~1;
                m_rcDirty.bottom = (m_rcDirty.bottom + 1) & ~1;
            }
        }
    }

    size_t w = m_rcDirty.right - m_rcDirty.left, h = m_rcDirty.bottom - m_rcDirty.top;

    BYTE* top = reinterpret_cast<BYTE*>(m_spd.bits) + m_spd.pitch * m_rcDirty.top + (m_rcDirty.left << 2);
    BYTE* bottom = top + m_spd.pitch * h;

    switch (m_spd.type) {
        case MSP_RGB16:
            for (; top < bottom; top += m_spd.pitch) {
                DWORD* s = reinterpret_cast<DWORD*>(top);
                for (size_t i = w; i; ++s, --i) {
                    *s = ((*s >> 3) & 0x1f000000) | ((*s >> 8) & 0xf800) | ((*s >> 5) & 0x07e0) | ((*s >> 3) & 0x001f);
                    // *s = (*s&0xff000000)|((*s>>8)&0xf800)|((*s>>5)&0x07e0)|((*s>>3)&0x001f);
                }
            }
            break;
        case MSP_RGB15:
            for (; top < bottom; top += m_spd.pitch) {
                DWORD* s = reinterpret_cast<DWORD*>(top);
                for (size_t i = w; i; ++s, --i) {
                    *s = ((*s >> 3) & 0x1f000000) | ((*s >> 9) & 0x7c00) | ((*s >> 6) & 0x03e0) | ((*s >> 3) & 0x001f);
                    // *s = (*s&0xff000000)|((*s>>9)&0x7c00)|((*s>>6)&0x03e0)|((*s>>3)&0x001f);
                }
            }
            break;
        case MSP_YUY2:
        case MSP_YV12:
        case MSP_IYUV:
        case MSP_NV12:
        case MSP_NV21: {
            size_t wm = w >> 1; // two pixels per loop
            for (; top < bottom; top += m_spd.pitch) {
                BYTE* s = top;
                for (size_t i = wm; i; s += 8, --i) { // ARGB ARGB -> AxYU AxYV
                    if ((s[3] + s[7]) < 0x1fe) {
                        s[1] = (c2y_yb[s[0]] + c2y_yg[s[1]] + c2y_yr[s[2]] + 0x108000) >> 16;
                        s[5] = (c2y_yb[s[4]] + c2y_yg[s[5]] + c2y_yr[s[6]] + 0x108000) >> 16;

                        __int32 scaled_y = (s[1] + s[5] - 32) * cy_cy2;
                        s[0] = Clamp((((((s[0] + s[4]) << 15) - scaled_y) >> 10) * c2y_cu + 0x800000 + 0x8000) >> 16);
                        s[4] = Clamp((((((s[2] + s[6]) << 15) - scaled_y) >> 10) * c2y_cv + 0x800000 + 0x8000) >> 16);
                    } else {
                        s[1] = s[5] = 0x10;
                        s[0] = s[4] = 0x80;
                    }
                }
            }
        }
        break;
        case MSP_AYUV: {
            for (; top < bottom ; top += m_spd.pitch) {
                BYTE* s = top;
                for (size_t i = w; i; s += 4, --i) { // ARGB -> AYUV
                    if (s[3] < 0xff) {
                        __int32 y = (c2y_yb[s[0]] + c2y_yg[s[1]] + c2y_yr[s[2]] + 0x108000) >> 16;
                        __int32 scaled_y = (y - 16) * cy_cy;
                        s[1] = Clamp(((((s[0] << 16) - scaled_y) >> 10) * c2y_cu + 0x800000 + 0x8000) >> 16);
                        s[0] = Clamp(((((s[2] << 16) - scaled_y) >> 10) * c2y_cv + 0x800000 + 0x8000) >> 16);
                        s[2] = y;
                    } else {
                        s[0] = s[1] = 0x80;
                        s[2] = 0x10;
                    }
                }
            }
        }
    }
}

__declspec(nothrow noalias) __forceinline void AlphaBlt_YUY2(__in size_t w, __in size_t h, __out BYTE* d, __in const size_t dstpitch, __in const BYTE* s, __in const size_t srcpitch)
{
    ASSERT(w);
    ASSERT(h);
    ASSERT(d);
    ASSERT(dstpitch);
    ASSERT(s);
    ASSERT(srcpitch);

#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
    __m128i mm_zero = _mm_setzero_si128();// only a command to set a register to zero, this should not add constant value to the assembly
    static __int64 const _8181 = 0x0080001000800010i64;
    __m128i mm_8181 = _mm_loadl_epi64(reinterpret_cast<__m128i const*>(&_8181));

    w >>= 1;
    for (; h; s += srcpitch, d += dstpitch, --h) {
        const BYTE* s2 = s;
        DWORD* d2 = reinterpret_cast<DWORD*>(d);
        for (size_t i = w; i; s2 += 8, ++d2, --i) {
            DWORD ia = s2[3] + s2[7];
            if (ia < 0x1fe) {
                ia >>= 1;
                DWORD c = (s2[4] << 24) | (s2[5] << 16) | (s2[0] << 8) | s2[1]; // (v<<24)|(y2<<16)|(u<<8)|y1;
                ia = (ia << 24) | (s2[7] << 16) | (ia << 8) | s2[3];

                __m128i mm_c = _mm_cvtsi32_si128(c);
                mm_c = _mm_unpacklo_epi8(mm_c, mm_zero);
                __m128i mm_d = _mm_cvtsi32_si128(*d2);
                mm_d = _mm_unpacklo_epi8(mm_d, mm_zero);
                __m128i mm_a = _mm_cvtsi32_si128(ia);
                mm_a = _mm_unpacklo_epi8(mm_a, mm_zero);
                mm_a = _mm_srli_epi16(mm_a, 1);
                mm_d = _mm_sub_epi16(mm_d, mm_8181);
                mm_d = _mm_mullo_epi16(mm_d, mm_a);
                mm_d = _mm_srai_epi16(mm_d, 7);
                mm_d = _mm_adds_epi16(mm_d, mm_c);
                mm_d = _mm_packus_epi16(mm_d, mm_d);
                *d2 = _mm_cvtsi128_si32(mm_d);
            }
        }
    }
#else
    static __m64 const _8181 = {0x0080001000800010LL};

    w >>= 1;
    for (; h; s += srcpitch, d += dstpitch, --h) {
        const BYTE* s2 = s;
        DWORD* d2 = reinterpret_cast<DWORD*>(d);
        for (size_t i = w; i; s2 += 8, ++d2, --i) {
            DWORD ia = s2[3] + s2[7];
            if (ia < 0x1fe) {
                ia >>= 1;
                DWORD c = (s2[4] << 24) | (s2[5] << 16) | (s2[0] << 8) | s2[1]; // (v<<24)|(y2<<16)|(u<<8)|y1;
                ia = (ia << 24) | (s2[7] << 16) | (ia << 8) | s2[3];

                __m64 mm0, mm2, mm3, mm4;
                mm0 = _mm_setzero_si64();
                mm2 = _mm_cvtsi32_si64(c);
                mm2 = _mm_unpacklo_pi8(mm2, mm0);
                mm3 = _mm_cvtsi32_si64(*d2);
                mm3 = _mm_unpacklo_pi8(mm3, mm0);
                mm4 = _mm_cvtsi32_si64(ia);
                mm4 = _mm_unpacklo_pi8(mm4, mm0);
                mm4 = _mm_srli_pi16(mm4, 1);
                mm3 = _mm_sub_pi16(mm3, _8181);
                mm3 = _mm_mullo_pi16(mm3, mm4);
                mm3 = _mm_srai_pi16(mm3, 7);
                mm3 = _mm_add_pi16(mm3, mm2);
                mm3 = _mm_packs_pu16(mm3, mm3);
                *d2 = _mm_cvtsi64_si32(mm3);
            }
        }
    }
    _mm_empty();
#endif
}

__declspec(nothrow noalias) void alpha_blt_rgb32(__in const RECT rcSrc, __in const RECT rcDst, __in const SubPicDesc* pTarget, __in const SubPicDesc& src)
{
    ASSERT(pTarget);

    SubPicDesc dst = *pTarget; // copy, because we might modify it
    if (!dst.pitchUV) {
        dst.pitchUV = dst.pitch;
    }

    // neither source nor destination rectangles can contain negative values here and neither can be empty
    ASSERT(rcSrc.left >= 0 && rcSrc.top >= 0 && rcSrc.right > 0 && rcSrc.bottom > 0);
    ASSERT(rcDst.left >= 0 && rcDst.top >= 0 && rcDst.right > 0 && rcDst.bottom > 0);
    size_t w = static_cast<size_t>(static_cast<ULONG>(rcSrc.right - rcSrc.left)), h = static_cast<size_t>(static_cast<ULONG>(rcSrc.bottom - rcSrc.top));
    // the pitch could be negative, so order a signed multiplication here
    const BYTE* s = reinterpret_cast<const BYTE*>(src.bits) + static_cast<ptrdiff_t>(src.pitch) * static_cast<ptrdiff_t>(static_cast<ULONG>(rcSrc.top)) + (static_cast<size_t>(static_cast<ULONG>(rcSrc.left)) << 2);
    const BYTE* s1 = s;

    BYTE* d, *dU;// destination pointers
    if (dst.h < 0) {// bottom to top
        // note: all pitches here are negative, for multiplications first cast them to ptrdiff_t
        ptrdiff_t top = -dst.h - static_cast<ptrdiff_t>(static_cast<ULONG>(rcSrc.top)) - 1;
        d = reinterpret_cast<BYTE*>(dst.bits) + static_cast<ptrdiff_t>(dst.pitch) * top + static_cast<size_t>(static_cast<ULONG>(rcSrc.left) * static_cast<ULONG>(dst.bpp) >> 3);

        if (!dst.bitsU) {
            dst.bitsU = reinterpret_cast<BYTE*>(dst.bits) + static_cast<ptrdiff_t>(dst.pitch) * -dst.h;
            dst.bitsV = dst.bitsU + ((static_cast<ptrdiff_t>(dst.pitchUV) * -dst.h) >> 1);// for 3-plane types only

            if (dst.type == MSP_YV12) {// compensate for CrCb plane ordering
                BYTE* p = dst.bitsU;
                dst.bitsU = dst.bitsV;
                dst.bitsV = p;
            }
        }
        ASSERT(dst.bitsV || ((dst.type != MSP_YV12) && (dst.type != MSP_IYUV)));// if dst.bitsU is available, dst.bitsV should be too for 3-plane types
        size_t leftoffset = static_cast<size_t>(static_cast<ULONG>(rcSrc.left));
        if ((dst.type == MSP_YV12) || (dst.type == MSP_IYUV)) {// chroma strides are half-width (NV12 and NV21 heve packed chroma)
            leftoffset >>= 1;
        }
        dU = dst.bitsU + (static_cast<ptrdiff_t>(dst.pitchUV) * (-dst.h - static_cast<ptrdiff_t>(static_cast<ULONG>(rcSrc.top)) - 2) >> 1) + leftoffset;

        // These two pitches are made negative relative to pointers for local usage only. It's okay that these are size_t (unsigned, pointer-sized), as only left shifting, substraction and addition is used, which doesn't matter for signed and unsigned processor arithmetic (two's complement).
        dst.pitch = -static_cast<ptrdiff_t>(dst.pitch);
        dst.pitchUV = -static_cast<ptrdiff_t>(dst.pitchUV);
    } else {// top to bottom
        // note: none of the values here are negative, we can permit to only do unsigned operations here
        d = reinterpret_cast<BYTE*>(dst.bits) + dst.pitch * static_cast<size_t>(static_cast<ULONG>(rcSrc.top)) + static_cast<size_t>(static_cast<ULONG>(rcSrc.left) * static_cast<ULONG>(dst.bpp) >> 3);

        if (!dst.bitsU) {
            dst.bitsU = reinterpret_cast<BYTE*>(dst.bits) + dst.pitch * static_cast<size_t>(dst.h);
            dst.bitsV = dst.bitsU + (dst.pitchUV * static_cast<size_t>(dst.h) >> 1);// for 3-plane types only

            if (dst.type == MSP_YV12) {// compensate for CrCb plane ordering
                BYTE* p = dst.bitsU;
                dst.bitsU = dst.bitsV;
                dst.bitsV = p;
            }
        }
        ASSERT(dst.bitsV || ((dst.type != MSP_YV12) && (dst.type != MSP_IYUV)));// if dst.bitsU is available, dst.bitsV should be too for 3-plane types
        size_t leftoffset = static_cast<size_t>(static_cast<ULONG>(rcSrc.left));
        if ((dst.type == MSP_YV12) || (dst.type == MSP_IYUV)) {// chroma strides are half-width (NV12 and NV21 heve packed chroma)
            leftoffset >>= 1;
        }
        dU = dst.bitsU + (dst.pitchUV * static_cast<size_t>(static_cast<ULONG>(rcSrc.top)) >> 1) + leftoffset;
    }

    switch (dst.type) { // note: the s and dU pointers should never be modified within this switch, but may be changed afterwards, the s1 and d pointers are free to use
        case MSP_RGBA:
            for (size_t j = h; j; s1 += src.pitch, d += dst.pitch, --j) {
                const BYTE* s2 = s1;
                DWORD* d2 = reinterpret_cast<DWORD*>(d);
                for (size_t i = w; i; s2 += 4, ++d2, --i) {
                    if (s2[3] < 0xff) {
                        DWORD bd = 0x00000100 - static_cast<DWORD>(s2[3]);
                        DWORD current = *reinterpret_cast<const DWORD*>(s2);

                        DWORD B = ((current & 0x000000ff) << 8) / bd;
                        DWORD V = ((current & 0x0000ff00) / bd) << 8;
                        DWORD R = (((current & 0x00ff0000) >> 8) / bd) << 16;
                        *d2 = B | V | R
                              | (0xff000000 - (current & 0xff000000)) & 0xff000000;
                    }
                }
            }
            break;
        case MSP_RGB32:
        case MSP_AYUV:
            for (size_t j = h; j; s1 += src.pitch, d += dst.pitch, --j) {
                const BYTE* s2 = s1;
                DWORD* d2 = reinterpret_cast<DWORD*>(d);
                for (size_t i = w; i; s2 += 4, ++d2, --i) {
                    if (s2[3] < 0xff) {
                        DWORD current = *reinterpret_cast<const DWORD*>(s2);
                        *d2 = ((((*d2 & 0x00ff00ff) * s2[3]) >> 8) + (current & 0x00ff00ff) & 0x00ff00ff)
                              | ((((*d2 & 0x0000ff00) * s2[3]) >> 8) + (current & 0x0000ff00) & 0x0000ff00);
                    }
                }
            }
            break;
        case MSP_RGB24:
            for (size_t j = h; j; s1 += src.pitch, d += dst.pitch, --j) {
                const BYTE* s2 = s1;
                BYTE* d2 = d;
                for (size_t i = w; i; s2 += 4, d2 += 3, --i) {
                    if (s2[3] < 0xff) {
                        d2[0] = ((d2[0] * s2[3]) >> 8) + s2[0];
                        d2[1] = ((d2[1] * s2[3]) >> 8) + s2[1];
                        d2[2] = ((d2[2] * s2[3]) >> 8) + s2[2];
                    }
                }
            }
            break;
        case MSP_RGB16:
            for (size_t j = h; j; s1 += src.pitch, d += dst.pitch, --j) {
                const BYTE* s2 = s1;
                WORD* d2 = reinterpret_cast<WORD*>(d);
                for (size_t i = w; i; s2 += 4, ++d2, --i) {
                    if (s2[3] < 0x1f) {
                        DWORD current = *reinterpret_cast<const DWORD*>(s2);
                        *d2 = (((((*d2 & 0xf81f) * s2[3]) >> 5) + (current & 0xf81f)) & 0xf81f)
                              | (((((*d2 & 0x07e0) * s2[3]) >> 5) + (current & 0x07e0)) & 0x07e0);
                    }
                }
            }
            break;
        case MSP_RGB15:
            for (size_t j = h; j; s1 += src.pitch, d += dst.pitch, --j) {
                const BYTE* s2 = s1;
                WORD* d2 = reinterpret_cast<WORD*>(d);
                for (size_t i = w; i; s2 += 4, ++d2, --i) {
                    if (s2[3] < 0x1f) {
                        DWORD current = *reinterpret_cast<const DWORD*>(s2);
                        *d2 = (((((*d2 & 0x7c1f) * s2[3]) >> 5) + (current & 0x7c1f)) & 0x7c1f)
                              | (((((*d2 & 0x03e0) * s2[3]) >> 5) + (current & 0x03e0)) & 0x03e0);
                    }
                }
            }
            break;
        case MSP_YUY2:
            AlphaBlt_YUY2(w, h, d, dst.pitch, s, src.pitch);// compiler context switchable MMX or SSE2
            break;
        default:
            ASSERT((dst.type == MSP_YV12) || (dst.type == MSP_IYUV) || (dst.type == MSP_NV12) || (dst.type == MSP_NV21));
            for (size_t j = h; j; s1 += src.pitch, d += dst.pitch, --j) {
                const BYTE* s2 = s1;
                BYTE* d2 = d;
                for (size_t i = w; i; s2 += 4, ++d2, --i) {
                    if (s2[3] < 0xff) {
                        d2[0] = (((d2[0] - 0x10) * s2[3]) >> 8) + s2[1];
                    }
                }
            }
    }

    if (dst.type == MSP_YV12 || dst.type == MSP_IYUV) {
        size_t h2 = h >> 1;
        size_t w2 = w >> 1; // blends two each loop
        ptrdiff_t offset = dst.bitsV - dst.bitsU;
        if (dst.type == MSP_YV12) {
            offset = -offset;
        }
        for (size_t j = h2; j; s += src.pitch << 1, dU += dst.pitchUV, --j) {
            const BYTE* s2 = s;
            BYTE* d2 = dU;
            for (size_t i = w2; i; s2 += 8, ++d2, --i) {
                DWORD ia = s2[3] + s2[7] + s2[3 + src.pitch] + s2[7 + src.pitch];
                if (ia < 0x3fc) {
                    d2[0] = (((d2[0] - 0x80) * ia) >> 9) + ((s2[0] + s2[src.pitch]) >> 1);
                    d2[offset] = (((d2[offset] - 0x80) * ia) >> 9) + ((s2[4] + s2[src.pitch + 4]) >> 1);
                }
            }
        }
    } else if (dst.type == MSP_NV12 || dst.type == MSP_NV21) {
        size_t h2 = h >> 1;
        size_t w2 = w >> 1; // blends two each loop
        if (dst.type == MSP_NV12) {
            for (size_t j = h2; j; s += src.pitch << 1, dU += dst.pitchUV, --j) {
                const BYTE* s2 = s;
                BYTE* d2 = dU;
                for (size_t i = w2; i; s2 += 8, d2 += 2, --i) {
                    DWORD ia = s2[3] + s2[7] + s2[3 + src.pitch] + s2[7 + src.pitch];
                    if (ia < 0x3fc) {
                        d2[0] = (((d2[0] - 0x80) * ia) >> 9) + ((s2[0] + s2[src.pitch]) >> 1);
                        d2[1] = (((d2[1] - 0x80) * ia) >> 9) + ((s2[4] + s2[src.pitch + 4]) >> 1);
                    }
                }
            }
        } else {
            for (size_t j = h2; j; s += src.pitch << 1, dU += dst.pitchUV, --j) {
                const BYTE* s2 = s;
                BYTE* d2 = dU;
                for (size_t i = w2; i; s2 += 8, d2 += 2, --i) {
                    DWORD ia = s2[3] + s2[7] + s2[3 + src.pitch] + s2[7 + src.pitch];
                    if (ia < 0x3fc) {
                        d2[0] = (((d2[0] - 0x80) * ia) >> 9) + ((s2[4] + s2[src.pitch + 4]) >> 1);
                        d2[1] = (((d2[1] - 0x80) * ia) >> 9) + ((s2[0] + s2[src.pitch]) >> 1);
                    }
                }
            }
        }
    }
}

//
// CMemSubPicAllocator
//

__declspec(nothrow noalias restrict) CBSubPic* CMemSubPicAllocator::Alloc(__in bool fStatic) const
{
    SubPicDesc spd;
    spd.type = mk_type;
    spd.w = m_u32Width;
    spd.h = m_u32Height;
    spd.bpp = 32;
    unsigned __int32 u32Pitch = m_u32Width << 2;
    spd.pitch = u32Pitch;
    spd.pitchUV = 0;
    unsigned __int32 u32Bytes = u32Pitch * m_u32Height;
    spd.bits = fStatic ? _aligned_offset_malloc(u32Bytes, 4096, 2048) : _aligned_malloc(u32Bytes, 4096); // optimize for the SSE copy functions, make the buffers differ by half a page size
    if (!spd.bits) {
        ASSERT(0);
        return nullptr;
    }
    spd.bitsU = nullptr;
    spd.bitsV = nullptr;
    spd.vidrect.left = 0;
    spd.vidrect.top = 0;
    spd.vidrect.right = 0;
    spd.vidrect.bottom = 0;

    void* pRawMem = malloc(sizeof(CMemSubPic));
    if (!pRawMem) {
        _aligned_free(spd.bits);
        ASSERT(0);
        return nullptr;
    }
    return new(pRawMem) CMemSubPic(spd);
}
