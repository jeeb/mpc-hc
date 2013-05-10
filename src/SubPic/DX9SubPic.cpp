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
#include "DX9SubPic.h"
#include <vmr9.h>
#include <intrin.h>

//
// CDX9SubPic
//
// CBSubPic

__declspec(nothrow noalias) void CDX9SubPic::GetDesc(__out SubPicDesc* pTarget) const
{
    ASSERT(pTarget);

    pTarget->type = 0;
    pTarget->w = m_maxsize.cx;
    pTarget->h = m_maxsize.cy;
    pTarget->bpp = 32;
    pTarget->pitch = 0;
    pTarget->bits = nullptr;
    pTarget->vidrect = m_vidrect;
}

__declspec(nothrow noalias) HRESULT CDX9SubPic::CopyTo(__out_opt CBSubPic* pSubPic) const
{
    ASSERT(pSubPic);

    if (!(m_rcDirty.right - m_rcDirty.left) || !(m_rcDirty.bottom - m_rcDirty.top)) {
        ASSERT(0);
        return E_ABORT;
    }

    HRESULT hr;
    CDX9SubPic* pDstSP = static_cast<CDX9SubPic*>(pSubPic);
    pDstSP->m_rtStart = m_rtStart;
    pDstSP->m_rtStop = m_rtStop;
    pDstSP->m_rtSegmentStart = m_rtSegmentStart;
    pDstSP->m_rtSegmentStop = m_rtSegmentStop;
    pDstSP->m_rcDirty = m_rcDirty;
    pDstSP->m_vidrect = m_vidrect;
    pDstSP->m_maxsize = m_maxsize;

    if (FAILED(hr = pDstSP->m_pD3DDev->CreateTexture(m_rcDirty.right - m_rcDirty.left, m_rcDirty.bottom - m_rcDirty.top, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &pDstSP->m_pTexture, nullptr))) {
        ASSERT(0);
        return hr;
    }
    if (FAILED(hr = pDstSP->m_pTexture->GetSurfaceLevel(0, &pDstSP->m_pSurface))) {
        ASSERT(0);
        return hr;
    }

    static POINT const zp = {0, 0};// obligated when using partial updates
    hr = pDstSP->m_pD3DDev->UpdateSurface(m_pSurface, &m_rcDirty, pDstSP->m_pSurface, &zp);
    ASSERT(hr == S_OK);
    return hr;
}

__declspec(nothrow noalias) HRESULT CDX9SubPic::LockAndClearDirtyRect(__out_opt SubPicDesc* pTarget)
{
    ASSERT(pTarget);

    HRESULT hr;
    D3DLOCKED_RECT LockedRect;
    if (FAILED(hr = m_pTexture->LockRect(0, &LockedRect, nullptr, 0))) {
        ASSERT(0);
        return hr;
    }

    pTarget->type = 0;
    pTarget->w = m_maxsize.cx;
    pTarget->h = m_maxsize.cy;
    pTarget->bpp = 32;
    pTarget->pitch = LockedRect.Pitch;
    pTarget->pitchUV = 0;
    pTarget->bits = LockedRect.pBits;
    pTarget->bitsU = nullptr;
    pTarget->bitsV = nullptr;
    pTarget->vidrect = m_vidrect;

    if ((m_rcDirty.right - m_rcDirty.left) && (m_rcDirty.bottom - m_rcDirty.top)) {// an empty status can happen when all subtitle elements move out of view, it's not an error
        // the method here is wrong: the do while loop is very unwelcome as alignment is important, and on top of that, this operation should be done on a custom-sized texture made to fit the contents of the subtitle without padding, so only a linear fill is required
        __declspec(align(16)) static __int32 const iFillVal[4] = {0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000};
        __m128 xFillVal = _mm_load_ps(reinterpret_cast<const float*>(iFillVal));
#ifdef _M_X64// also pre-load the smaller fill values in gpr registers
        __int64 iFillValPad = 0xFF000000FF000000;
#elif _M_IX86_FP != 1// SSE2 code, don't use on SSE builds
        __int32 iFillValPad = 0xFF000000;
#endif
        size_t upWidth = m_rcDirty.right - m_rcDirty.left, upHeight = m_rcDirty.bottom - m_rcDirty.top, upPitch = m_maxsize.cx << 2;
        uintptr_t pRow = reinterpret_cast<uintptr_t>(LockedRect.pBits) + upPitch * m_rcDirty.top;
        if (m_maxsize.cx - upWidth <= 16) { // linear fill
            upWidth = upHeight * (upPitch >> 2);
            upHeight = 1;
        } else {// fill per row
            pRow += m_rcDirty.left << 2;
        }

        do {
            uintptr_t pDst = pRow;
            size_t upCount = upWidth;
            // fill the output to meet 16-byte alignment, sorted for aligned access
            // note: when clearing entire textures, the initial realignment fill parts are also useless, the driver always allocates textures at 16-byte boundaries
            if (pDst & 4 && upCount) { // 4-to-8-byte alignment
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
                _mm_stream_si32(reinterpret_cast<__int32*>(pDst), iFillValPad); // x64: copies lower bytes in the register
#else
                _mm_store_ss(reinterpret_cast<float*>(pDst), xFillVal);
#endif
                pDst += 4;
                --upCount;
            }
            if (pDst & 8 && (upCount >= 2)) { // 8-to-16-byte alignment
#ifdef _M_X64
                _mm_stream_si64x(reinterpret_cast<__int64*>(pDst), iFillValPad);
#elif _M_IX86_FP != 1// SSE2 code, don't use on SSE builds
                _mm_stream_si32(reinterpret_cast<__int32*>(pDst), iFillValPad);
                _mm_stream_si32(reinterpret_cast<__int32*>(pDst + 4), iFillValPad);
#else
                _mm_storel_pi(reinterpret_cast<__m64*>(pDst), xFillVal); // not related to MMX
#endif
                pDst += 8;
                upCount -= 2;
            }
            ASSERT(!(pDst & 15) || (upCount < 4)); // if not 16-byte aligned, _mm_stream_ps will fail

            // excludes the last the last 3 optional values (in the bit shift), as the next function only targets 128-bit fills
            if (size_t i = upCount >> 2) do {
                    _mm_stream_ps(reinterpret_cast<float*>(pDst), xFillVal);
                    pDst += 16;
                } while (--i);

            if (upCount & 2) { // finalize the last 3 optional values, sorted for aligned access
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
            if (upCount & 1) { // no address increment for the last possible value
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
                _mm_stream_si32(reinterpret_cast<__int32*>(pDst), iFillValPad);// x64: copies lower bytes in the register
#else
                _mm_store_ss(reinterpret_cast<float*>(pDst), xFillVal);
#endif
            }
            pRow += upPitch;
        } while (--upHeight); // advance pointer to the next row in the texture

        ZeroMemory(&m_rcDirty, sizeof(RECT));
    }
    return S_OK;
}

__declspec(nothrow noalias) void CDX9SubPic::Unlock(__in RECT const rDirtyRect)
{
    m_rcDirty = rDirtyRect;
    EXECUTE_ASSERT(S_OK == m_pTexture->UnlockRect(0));// should only fail if m_pTexture wasn't locked in the first place
}

//
// CDX9SubPicAllocator
//

__declspec(nothrow noalias restrict) CBSubPic* CDX9SubPicAllocator::Alloc(__in bool fStatic) const
{
    void* pRawMem = malloc(sizeof(CDX9SubPic));
    if (!pRawMem) {
        ASSERT(0);
        return nullptr;
    }

    CDX9SubPic* pSubPic;
    if (fStatic) {
        IDirect3DTexture9* pTexture;
        if (FAILED(m_pD3DDev->CreateTexture(m_u32Width, m_u32Height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &pTexture, nullptr))) {
            ASSERT(0);
            free(pRawMem);
            return nullptr;
        }

        // note: this constructor is modified to inherit the reference of pTexture
        pSubPic = new(pRawMem) CDX9SubPic(m_u32Width, m_u32Height, pTexture);
    } else {
        pSubPic = new(pRawMem) CDX9SubPic(m_u32Width, m_u32Height, m_pD3DDev);
    }

    return pSubPic;
}
