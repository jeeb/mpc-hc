/*
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
#include "QT9AllocatorPresenter.h"
#include <intrin.h>

using namespace DSObjects;

//
// CQT9AllocatorPresenter
//
// IUnknown

__declspec(nothrow noalias) STDMETHODIMP CQT9AllocatorPresenter::QueryInterface(REFIID riid, __deref_out void** ppv)
{
    ASSERT(ppv);
    __assume(this);// fix assembly: the compiler generated tests for null pointer input on static_cast<T>(this)

    __int64 lo = reinterpret_cast<__int64 const*>(&riid)[0], hi = reinterpret_cast<__int64 const*>(&riid)[1];
    void* pv = static_cast<IUnknown*>(static_cast<CSubPicAllocatorPresenterImpl*>(this));// CSubPicAllocatorPresenterImpl is at Vtable location 0
    if (lo == reinterpret_cast<__int64 const*>(&IID_IUnknown)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IUnknown)[1]) {
        goto exit;
    }
    pv = static_cast<CSubPicAllocatorPresenterImpl*>(this);
    if (lo == reinterpret_cast<__int64 const*>(&__uuidof(CSubPicAllocatorPresenterImpl))[0] && hi == reinterpret_cast<__int64 const*>(&__uuidof(CSubPicAllocatorPresenterImpl))[1]) {
        goto exit;
    }
    pv = static_cast<IVMRMixerBitmap9*>(this);
    if (lo == reinterpret_cast<__int64 const*>(&IID_IVMRMixerBitmap9)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IVMRMixerBitmap9)[1]) {
        goto exit;
    }
    pv = static_cast<IVMRffdshow9*>(this);
    if (lo == reinterpret_cast<__int64 const*>(&IID_IVMRffdshow9)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IVMRffdshow9)[1]) {
        goto exit;
    }
    pv = static_cast<IQTVideoSurface*>(this);
    if (lo == reinterpret_cast<__int64 const*>(&__uuidof(IQTVideoSurface))[0] && hi == reinterpret_cast<__int64 const*>(&__uuidof(IQTVideoSurface))[1]) {
        goto exit;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
exit:
    *ppv = pv;
    ULONG ulRef = _InterlockedIncrement(reinterpret_cast<LONG volatile*>(&mv_ulReferenceCount));
    ASSERT(ulRef);
    UNREFERENCED_PARAMETER(ulRef);
    return NOERROR;
}

__declspec(nothrow noalias) STDMETHODIMP_(ULONG) CQT9AllocatorPresenter::AddRef()
{
    // based on CUnknown::NonDelegatingAddRef()
    // the original CUnknown::NonDelegatingAddRef() has a version that keeps compatibility for Windows 95, Windows NT 3.51 and earlier, this one doesn't
    ULONG ulRef = _InterlockedIncrement(reinterpret_cast<LONG volatile*>(&mv_ulReferenceCount));
    ASSERT(ulRef);
    return ulRef;
}

__declspec(nothrow noalias) STDMETHODIMP_(ULONG) CQT9AllocatorPresenter::Release()
{
    // based on CUnknown::NonDelegatingRelease()
    // If the reference count drops to zero delete ourselves
    ULONG ulRef = _InterlockedDecrement(reinterpret_cast<LONG volatile*>(&mv_ulReferenceCount));

    if (!ulRef) {
        // COM rules say we must protect against re-entrancy.
        // If we are an aggregator and we hold our own interfaces
        // on the aggregatee, the QI for these interfaces will
        // addref ourselves. So after doing the QI we must release
        // a ref count on ourselves. Then, before releasing the
        // private interface, we must addref ourselves. When we do
        // this from the destructor here it will result in the ref
        // count going to 1 and then back to 0 causing us to
        // re-enter the destructor. Hence we add an extra refcount here
        // once we know we will delete the object.
        // for an example aggregator see filgraph\distrib.cpp.
        ++mv_ulReferenceCount;

        // class created with placement new
        this->~CQT9AllocatorPresenter();
#ifdef _WIN64
        free(this);
#else
        _aligned_free(this);
#endif
        return 0;
    } else {
        // Don't touch the counter again even in this leg as the object
        // may have just been released on another thread too
        return ulRef;
    }
}

// CSubPicAllocatorPresenterImpl

__declspec(nothrow noalias) void CQT9AllocatorPresenter::ResetDevice()
{
    CAutoLock cRenderLock(&m_csRenderLock);

    ResetMainDevice();

    if (!m_bPartialExDeviceReset) {// note: partial resets are available for Vista and newer only
        if (m_pVideoSurfaceOff) {
            ULONG u = m_pVideoSurfaceOff->Release();
            ASSERT(!u);
            m_pVideoSurfaceOff = nullptr;
        }

        HRESULT hr;
        if (FAILED(hr = m_pD3DDev->CreateOffscreenPlainSurface(m_u32VideoWidth, m_u32VideoHeight, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &m_pVideoSurfaceOff, nullptr))) {
            ErrBox(hr, L"creating offscreen plain surface for the mixer failed");
        }

        AllocSurfaces();
    }
    m_bPartialExDeviceReset = false;
}

// IQTVideoSurface

__declspec(nothrow noalias) STDMETHODIMP CQT9AllocatorPresenter::BeginBlt(BITMAP const& bm)
{
    CAutoLock cRenderLock(&m_csRenderLock);

    // warning: currently we assume the only incoming formats use the truncated D65 white point
    // should input formats with other white points be accepted and processed correctly by the color management section, additional code has to be added for this purpose
    // notes about handling the input white point in the initial pass pixel shaders can be found in the renderer's cpp file

    // surface size change check
    DWORD dwNW = bm.bmWidth, dwNH = bm.bmHeight,
          dwOW = m_u32VideoWidth, dwOH = m_u32VideoHeight;
    if ((dwNW != dwOW) || (dwNH != dwOH)) {
        // SD-HD switch check
        bool bNewVideoUsingSD = (dwNW < 1120) && (dwNH < 630);
        bool bRendererUsingSD = (dwOW < 1120) && (dwOH < 630);
        if (bNewVideoUsingSD != bRendererUsingSD) {// make the renderer reset some things that depend on SD and HD matrices
            m_u8VMR9ChromaFixCurrent = 127;// to pass the check list for re-compiling the initial pass shaders (as 127 is not an option for this item)
        }

        // video size parts
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
        __m128 x2 = _mm_set_ps1(1.0f);
        __m128i xVS = _mm_loadl_epi64(reinterpret_cast<__m128i const*>(&bm.bmWidth));
        __m128d x0 = _mm_cvtepi32_pd(xVS);// __int32 to double
        _mm_storel_epi64(reinterpret_cast<__m128i*>(&m_u32VideoWidth), xVS);// also stores m_u32VideoHeight
        _mm_storel_epi64(reinterpret_cast<__m128i*>(&m_u32AspectRatioWidth), xVS);// also stores m_u32AspectRatioHeight
        __m128 x1 = _mm_cvtpd_ps(x0);// double to float
        _mm_store_pd(&m_dVideoWidth, x0);;// also stores m_dVideoHeight
        x2 = _mm_div_ps(x2, x1);// reciprocal trough _mm_rcp_ps() isn't accurate
        _mm_storel_pi(reinterpret_cast<__m64*>(&m_fVideoWidth), x1);// not an MMX function, also stores m_fVideoHeight
        _mm_storel_pi(reinterpret_cast<__m64*>(&m_fVideoWidthr), x2);// not an MMX function, also stores m_fVideoHeightr
#else
        m_u32VideoWidth = m_u32AspectRatioWidth = bm.bmWidth;
        m_u32VideoHeight = m_u32AspectRatioHeight = bm.bmHeight;
        m_dVideoWidth = static_cast<double>(static_cast<__int32>(m_u32VideoWidth));// the standard converter only does a proper job with signed values
        m_dVideoHeight = static_cast<double>(static_cast<__int32>(m_u32VideoHeight));
        m_fVideoWidth = static_cast<float>(m_dVideoWidth);
        m_fVideoHeight = static_cast<float>(m_dVideoHeight);
        m_fVideoWidthr = 1.0f / m_fVideoWidth;
        m_fVideoHeightr = 1.0f / m_fVideoHeight;
#endif
        unsigned __int8 u8Nibble;// note: these two strings are partially initialized in the CDX9AllocatorPresenter initializer
        // m_szVideoWidth; standard method for converting numbers to hex strings
        ASSERT(m_u32VideoWidth <= 0x9FFFF);// the method implementation limit here
        u8Nibble = static_cast<unsigned __int8>(m_u32VideoWidth >> 16); // each hexadecimal char stores 4 bits
        m_szVideoWidth[2] = '0' + u8Nibble;
        u8Nibble = (m_u32VideoWidth >> 12) & 15;
        u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
        m_szVideoWidth[3] = u8Nibble;
        u8Nibble = (m_u32VideoWidth >> 8) & 15;
        u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
        m_szVideoWidth[4] = u8Nibble;
        u8Nibble = (m_u32VideoWidth >> 4) & 15;
        u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
        m_szVideoWidth[5] = u8Nibble;
        u8Nibble = m_u32VideoWidth & 15;
        u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
        m_szVideoWidth[6] = u8Nibble;

        // m_szVideoHeight; standard method for converting numbers to hex strings
        ASSERT(m_u32VideoHeight <= 0x9FFFF);// the method implementation limit here
        u8Nibble = static_cast<unsigned __int8>(m_u32VideoHeight >> 16); // each hexadecimal char stores 4 bits
        m_szVideoHeight[2] = '0' + u8Nibble;
        u8Nibble = (m_u32VideoHeight >> 12) & 15;
        u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
        m_szVideoHeight[3] = u8Nibble;
        u8Nibble = (m_u32VideoHeight >> 8) & 15;
        u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
        m_szVideoHeight[4] = u8Nibble;
        u8Nibble = (m_u32VideoHeight >> 4) & 15;
        u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
        m_szVideoHeight[5] = u8Nibble;
        u8Nibble = m_u32VideoHeight & 15;
        u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
        m_szVideoHeight[6] = u8Nibble;

        HRESULT hr;
        if (m_pVideoSurfaceOff) {
            ULONG u = m_pVideoSurfaceOff->Release();
            ASSERT(!u);
            m_pVideoSurfaceOff = nullptr;
        }

        if (FAILED(hr = m_pD3DDev->CreateOffscreenPlainSurface(m_u32VideoWidth, m_u32VideoHeight, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &m_pVideoSurfaceOff, nullptr))) {
            return hr;
        }

        DeleteSurfaces();
        AllocSurfaces();
    }
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CQT9AllocatorPresenter::DoBlt(BITMAP const& bm)
{
    if (!m_apVideoSurface[m_u8CurrentMixerSurface] || !m_pVideoSurfaceOff) {
        return E_FAIL;
    }

    bool fOk = false;

    D3DSURFACE_DESC d3dsd;
    ZeroMemory(&d3dsd, sizeof(d3dsd));
    if (FAILED(m_pVideoSurfaceOff->GetDesc(&d3dsd))) {
        return E_FAIL;
    }

    UINT w = bm.bmWidth;
    UINT h = abs(bm.bmHeight);
    unsigned __int8 bpp = static_cast<unsigned __int8>(bm.bmBitsPixel);
    unsigned __int8 dbpp =
        d3dsd.Format == D3DFMT_R8G8B8 || d3dsd.Format == D3DFMT_X8R8G8B8 || d3dsd.Format == D3DFMT_A8R8G8B8 ? 32 :
        d3dsd.Format == D3DFMT_R5G6B5 ? 16 : 0;

    if ((bpp == 16 || bpp == 24 || bpp == 32) && w == d3dsd.Width && h == d3dsd.Height) {
        D3DLOCKED_RECT r;
        if (SUCCEEDED(m_pVideoSurfaceOff->LockRect(&r, nullptr, 0))) {
            BitBltFromRGBToRGB(
                w, h,
                reinterpret_cast<BYTE*>(r.pBits), r.Pitch, dbpp,
                reinterpret_cast<BYTE*>(bm.bmBits), bm.bmWidthBytes, bpp);
            m_pVideoSurfaceOff->UnlockRect();
            fOk = true;
        }
    }

    if (!fOk) {
        m_pD3DDev->ColorFill(m_pVideoSurfaceOff, nullptr, 0);

        HDC hDC;
        if (SUCCEEDED(m_pVideoSurfaceOff->GetDC(&hDC))) {
            wchar_t const str[] = L"Sorry, this format is not supported";
            SetBkColor(hDC, 0);
            SetTextColor(hDC, 0x404040);
            TextOutW(hDC, 10, 10, str, _countof(str) - 1);// remove trailing nullptr
            m_pVideoSurfaceOff->ReleaseDC(hDC);
        }
    }

    // use the next buffer in line
    ++m_u8CurrentMixerSurface;
    if (m_u8CurrentMixerSurface >= m_u8MixerSurfaceCount) {
        m_u8CurrentMixerSurface = 0;
    }

    m_pD3DDev->StretchRect(m_pVideoSurfaceOff, nullptr, m_apVideoSurface[m_u8CurrentMixerSurface], nullptr, D3DTEXF_POINT);

    Paint(FRENDERPAINT_NORMAL);
    return S_OK;
}
