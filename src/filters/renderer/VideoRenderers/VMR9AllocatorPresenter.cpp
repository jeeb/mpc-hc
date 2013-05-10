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
#include "VMR9AllocatorPresenter.h"
#include <intrin.h>
#include "../../../thirdparty/VirtualDub/h/vd2/system/cpuaccel.h"

using namespace DSObjects;

//
// CVMR9AllocatorPresenter
//
// IUnknown

__declspec(nothrow noalias) STDMETHODIMP CVMR9AllocatorPresenter::QueryInterface(REFIID riid, __deref_out void** ppv)
{
    ASSERT(ppv);
    __assume(this);// fix assembly: the compiler generated tests for null pointer input on static_cast<T>(this)
    // __assume(&m_OuterVMR); didn't work, resolved the issue by manually offsetting the pointers on reinterpret_cast
    // static_assert won't take anything with static_cast, so we test at debug runtime
    // the fake pointer can't be 0 on input, else the compiler will not offset it on static_cast
    ASSERT(reinterpret_cast<uintptr_t>(static_cast<IBaseFilter*>(reinterpret_cast<COuterVMR*>(64))) == 64);
    ASSERT(reinterpret_cast<uintptr_t>(static_cast<IMediaFilter*>(reinterpret_cast<COuterVMR*>(64))) == 64);
    ASSERT(reinterpret_cast<uintptr_t>(static_cast<IPersist*>(reinterpret_cast<COuterVMR*>(64))) == 64);
    ASSERT(reinterpret_cast<uintptr_t>(static_cast<IVideoWindow*>(reinterpret_cast<COuterVMR*>(64))) == 64 + sizeof(uintptr_t));
    ASSERT(reinterpret_cast<uintptr_t>(static_cast<IBasicVideo2*>(reinterpret_cast<COuterVMR*>(64))) == 64 + 2 * sizeof(uintptr_t));
    ASSERT(reinterpret_cast<uintptr_t>(static_cast<IBasicVideo*>(reinterpret_cast<COuterVMR*>(64))) == 64 + 2 * sizeof(uintptr_t));
    ASSERT(reinterpret_cast<uintptr_t>(static_cast<IKsPropertySet*>(reinterpret_cast<COuterVMR*>(64))) == 64 + 3 * sizeof(uintptr_t));

    void* pv = static_cast<IUnknown*>(static_cast<CSubPicAllocatorPresenterImpl*>(this));// CSubPicAllocatorPresenterImpl is at Vtable location 0
    if (CPUGetEnabledExtensions() & CPUF_SUPPORTS_SSE41) {// SSE4.1 code path
        {
            __m128i xIIDin = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&riid));
            __m128i xIID_IUnknown = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IUnknown));
            __m128i xIID_SubPicAllocatorPresenterImpl = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&__uuidof(CSubPicAllocatorPresenterImpl)));
            __m128i xIID_IVMRMixerBitmap9 = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IVMRMixerBitmap9));
            __m128i xIID_IVMRffdshow9 = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IVMRffdshow9));
            __m128i xIID_IVMRSurfaceAllocator9 = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IVMRSurfaceAllocator9));
            __m128i xIID_IVMRImagePresenter9 = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IVMRImagePresenter9));
            __m128i xIID_IVMRWindowlessControl9 = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IVMRWindowlessControl9));
            xIID_IUnknown = _mm_xor_si128(xIID_IUnknown, xIIDin);
            xIID_SubPicAllocatorPresenterImpl = _mm_xor_si128(xIID_SubPicAllocatorPresenterImpl, xIIDin);
            xIID_IVMRMixerBitmap9 = _mm_xor_si128(xIID_IVMRMixerBitmap9, xIIDin);
            // this class
            if (_mm_testz_si128(xIID_IUnknown, xIID_IUnknown)) {
                goto exitRThisSSE41;
            }
            __m128i xIID_IBaseFilter = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IBaseFilter));
            xIID_IVMRffdshow9 = _mm_xor_si128(xIID_IVMRffdshow9, xIIDin);
            pv = static_cast<CSubPicAllocatorPresenterImpl*>(this);
            if (_mm_testz_si128(xIID_SubPicAllocatorPresenterImpl, xIID_SubPicAllocatorPresenterImpl)) {
                goto exitRThisSSE41;
            }
            __m128i xIID_IMediaFilter = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IMediaFilter));
            xIID_IVMRSurfaceAllocator9 = _mm_xor_si128(xIID_IVMRSurfaceAllocator9, xIIDin);
            pv = static_cast<IVMRMixerBitmap9*>(this);
            if (_mm_testz_si128(xIID_IVMRMixerBitmap9, xIID_IVMRMixerBitmap9)) {
                goto exitRThisSSE41;
            }
            __m128i xIID_IPersist = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IPersist));
            xIID_IVMRImagePresenter9 = _mm_xor_si128(xIID_IVMRImagePresenter9, xIIDin);
            pv = static_cast<IVMRffdshow9*>(this);
            if (_mm_testz_si128(xIID_IVMRffdshow9, xIID_IVMRffdshow9)) {
                goto exitRThisSSE41;
            }
            __m128i xIID_IVideoWindow = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IVideoWindow));
            xIID_IVMRWindowlessControl9 = _mm_xor_si128(xIID_IVMRWindowlessControl9, xIIDin);
            pv = static_cast<IVMRSurfaceAllocator9*>(this);
            if (_mm_testz_si128(xIID_IVMRSurfaceAllocator9, xIID_IVMRSurfaceAllocator9)) {
                goto exitRThisSSE41;
            }
            __m128i xIID_IBasicVideo2 = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IBasicVideo2));
            xIID_IBaseFilter = _mm_xor_si128(xIID_IBaseFilter, xIIDin);
            pv = static_cast<IVMRImagePresenter9*>(this);
            if (_mm_testz_si128(xIID_IVMRImagePresenter9, xIID_IVMRImagePresenter9)) {
                goto exitRThisSSE41;
            }
            __m128i xIID_IBasicVideo = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IBasicVideo));
            xIID_IMediaFilter = _mm_xor_si128(xIID_IMediaFilter, xIIDin);
            pv = static_cast<IVMRWindowlessControl9*>(this);
            if (_mm_testz_si128(xIID_IVMRWindowlessControl9, xIID_IVMRWindowlessControl9)) {
                goto exitRThisSSE41;
            }
            __m128i xIID_IKsPropertySet = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IKsPropertySet));
            xIID_IPersist = _mm_xor_si128(xIID_IPersist, xIIDin);
            // COuterVMR
            pv = reinterpret_cast<void*>(&m_OuterVMR);
            if (_mm_testz_si128(xIID_IBaseFilter, xIID_IBaseFilter)) {
                goto exitROuterVMRSSE41;
            }
            xIID_IVideoWindow = _mm_xor_si128(xIID_IVideoWindow, xIIDin);
            if (_mm_testz_si128(xIID_IMediaFilter, xIID_IMediaFilter)) {
                goto exitROuterVMRSSE41;
            }
            xIID_IBasicVideo2 = _mm_xor_si128(xIID_IBasicVideo2, xIIDin);
            if (_mm_testz_si128(xIID_IPersist, xIID_IPersist)) {
                goto exitROuterVMRSSE41;
            }
            xIID_IBasicVideo = _mm_xor_si128(xIID_IBasicVideo, xIIDin);
            pv = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(&m_OuterVMR) + sizeof(uintptr_t));
            if (_mm_testz_si128(xIID_IVideoWindow, xIID_IVideoWindow)) {
                goto exitROuterVMRSSE41;
            }
            xIID_IKsPropertySet = _mm_xor_si128(xIID_IKsPropertySet, xIIDin);
            pv = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(&m_OuterVMR) + 2 * sizeof(uintptr_t));
            if (_mm_testz_si128(xIID_IBasicVideo2, xIID_IBasicVideo2)) {
                goto exitROuterVMRSSE41;
            }
            if (_mm_testz_si128(xIID_IBasicVideo, xIID_IBasicVideo)) {
                goto exitROuterVMRSSE41;
            }
            pv = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(&m_OuterVMR) + 3 * sizeof(uintptr_t));
            if (_mm_testz_si128(xIID_IKsPropertySet, xIID_IKsPropertySet)) {
                goto exitROuterVMRSSE41;
            }
        }
        // lastly, try the external VMR
        return m_OuterVMR.m_pVMR->QueryInterface(riid, ppv);
exitROuterVMRSSE41:
        *ppv = pv;
        ULONG ulRefOuterVMR = _InterlockedIncrement(reinterpret_cast<LONG volatile*>(&m_OuterVMR.mv_ulReferenceCount));
        ASSERT(ulRefOuterVMR);
        UNREFERENCED_PARAMETER(ulRefOuterVMR);
        return NOERROR;
exitRThisSSE41:
        *ppv = pv;
        ULONG ulRef = _InterlockedIncrement(reinterpret_cast<LONG volatile*>(&mv_ulReferenceCount));
        ASSERT(ulRef);
        UNREFERENCED_PARAMETER(ulRef);
        return NOERROR;
    }

    // non-SSE4.1 code path
    __int64 lo = reinterpret_cast<__int64 const*>(&riid)[0], hi = reinterpret_cast<__int64 const*>(&riid)[1];
    // this class
    if (lo == reinterpret_cast<__int64 const*>(&IID_IUnknown)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IUnknown)[1]) {
        goto exitRThis;
    }
    pv = static_cast<CSubPicAllocatorPresenterImpl*>(this);
    if (lo == reinterpret_cast<__int64 const*>(&__uuidof(CSubPicAllocatorPresenterImpl))[0] && hi == reinterpret_cast<__int64 const*>(&__uuidof(CSubPicAllocatorPresenterImpl))[1]) {
        goto exitRThis;
    }
    pv = static_cast<IVMRMixerBitmap9*>(this);
    if (lo == reinterpret_cast<__int64 const*>(&IID_IVMRMixerBitmap9)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IVMRMixerBitmap9)[1]) {
        goto exitRThis;
    }
    pv = static_cast<IVMRffdshow9*>(this);
    if (lo == reinterpret_cast<__int64 const*>(&IID_IVMRffdshow9)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IVMRffdshow9)[1]) {
        goto exitRThis;
    }
    pv = static_cast<IVMRSurfaceAllocator9*>(this);
    if (lo == reinterpret_cast<__int64 const*>(&IID_IVMRSurfaceAllocator9)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IVMRSurfaceAllocator9)[1]) {
        goto exitRThis;
    }
    pv = static_cast<IVMRImagePresenter9*>(this);
    if (lo == reinterpret_cast<__int64 const*>(&IID_IVMRImagePresenter9)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IVMRImagePresenter9)[1]) {
        goto exitRThis;
    }
    pv = static_cast<IVMRWindowlessControl9*>(this);
    if (lo == reinterpret_cast<__int64 const*>(&IID_IVMRWindowlessControl9)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IVMRWindowlessControl9)[1]) {
        goto exitRThis;
    }
    // COuterVMR
    pv = reinterpret_cast<void*>(&m_OuterVMR);
    if (lo == reinterpret_cast<__int64 const*>(&IID_IBaseFilter)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IBaseFilter)[1]) {
        goto exitROuterVMR;
    }
    if (lo == reinterpret_cast<__int64 const*>(&IID_IMediaFilter)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IMediaFilter)[1]) {
        goto exitROuterVMR;
    }
    if (lo == reinterpret_cast<__int64 const*>(&IID_IPersist)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IPersist)[1]) {
        goto exitROuterVMR;
    }
    pv = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(&m_OuterVMR) + sizeof(uintptr_t));
    if (lo == reinterpret_cast<__int64 const*>(&IID_IVideoWindow)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IVideoWindow)[1]) {
        goto exitROuterVMR;
    }
    pv = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(&m_OuterVMR) + 2 * sizeof(uintptr_t));
    if (lo == reinterpret_cast<__int64 const*>(&IID_IBasicVideo2)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IBasicVideo2)[1]) {
        goto exitROuterVMR;
    }
    if (lo == reinterpret_cast<__int64 const*>(&IID_IBasicVideo)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IBasicVideo)[1]) {
        goto exitROuterVMR;
    }
    pv = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(&m_OuterVMR) + 3 * sizeof(uintptr_t));
    if (lo == reinterpret_cast<__int64 const*>(&IID_IKsPropertySet)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IKsPropertySet)[1]) {
        goto exitROuterVMR;
    }
    // lastly, try the external VMR
    return m_OuterVMR.m_pVMR->QueryInterface(riid, ppv);
exitROuterVMR:
    *ppv = pv;
    ULONG ulRefOuterVMR = _InterlockedIncrement(reinterpret_cast<LONG volatile*>(&m_OuterVMR.mv_ulReferenceCount));
    ASSERT(ulRefOuterVMR);
    UNREFERENCED_PARAMETER(ulRefOuterVMR);
    return NOERROR;
exitRThis:
    *ppv = pv;
    ULONG ulRef = _InterlockedIncrement(reinterpret_cast<LONG volatile*>(&mv_ulReferenceCount));
    ASSERT(ulRef);
    UNREFERENCED_PARAMETER(ulRef);
    return NOERROR;
}

__declspec(nothrow noalias) STDMETHODIMP_(ULONG) CVMR9AllocatorPresenter::AddRef()
{
    // based on CUnknown::NonDelegatingAddRef()
    // the original CUnknown::NonDelegatingAddRef() has a version that keeps compatibility for Windows 95, Windows NT 3.51 and earlier, this one doesn't
    ULONG ulRef = _InterlockedIncrement(reinterpret_cast<LONG volatile*>(&mv_ulReferenceCount));
    ASSERT(ulRef);
    return ulRef;
}

__declspec(nothrow noalias) STDMETHODIMP_(ULONG) CVMR9AllocatorPresenter::Release()
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
        this->~CVMR9AllocatorPresenter();
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

__declspec(nothrow noalias) void CVMR9AllocatorPresenter::ResetDevice()
{
    m_csRenderLock.Lock();

    IDirect3DDevice9* pDev;
    if (!m_bPartialExDeviceReset) {// note: partial resets are available for Vista and newer only
        pDev = m_pD3DDev;
        pDev->AddRef();
    }

    ResetMainDevice();

    if (!m_bPartialExDeviceReset) {
        ASSERT(!m_hEvtReset);
        m_hEvtReset = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        ASSERT(m_hEvtReset);
        // the external VMR-9 will renegotiate the video input media and renew surfaces
        HRESULT hr;
        if (FAILED(hr = m_pIVMRSurfAllocNotify->ChangeD3DDevice(reinterpret_cast<IDirect3DDevice9* volatile>(m_pD3DDev), m_hCurrentMonitor))) {// volatile, to prevent the compiler from re-using the old pointer
            ErrBox(hr, L"IVMRSurfaceAllocatorNotify9::ChangeD3DDevice() failed");
        }

        m_csRenderLock.Unlock();
        WaitForSingleObject(m_hEvtReset, INFINITE);
        EXECUTE_ASSERT(CloseHandle(m_hEvtReset));
        m_hEvtReset = nullptr;

        // delayed release of the old device for the VMR-9 mixer
        ULONG u = pDev->Release();
        ASSERT(!u);
    } else {
        m_bPartialExDeviceReset = false;
        m_csRenderLock.Unlock();
    }
}

// IVMRSurfaceAllocator9

__declspec(nothrow noalias) STDMETHODIMP CVMR9AllocatorPresenter::InitializeDevice(DWORD_PTR dwUserID, VMR9AllocationInfo* lpAllocInfo, DWORD* lpNumBuffers)
{
    ASSERT(lpAllocInfo);
    ASSERT(lpNumBuffers);

    // warning: currently we assume the only incoming formats use the truncated D65 white point
    // should input formats with other white points be accepted and processed correctly by the color management section, additional code has to be added for this purpose
    // notes about handling the input white point in the initial pass pixel shaders can be found in the renderer's cpp file

    CAutoLock cRenderLock(&m_csRenderLock);

    if (!lpAllocInfo || !lpNumBuffers) {
        ASSERT(0);
        return E_POINTER;
    }

    m_u8MixerSurfaceCount = mk_pRendererSettings->MixerBuffers;
    *lpNumBuffers = static_cast<DWORD>(m_u8MixerSurfaceCount);
    m_bNeedCheckSample = true;

    // these two are not linked to any conditional things here
    m_u32AspectRatioWidth = lpAllocInfo->szAspectRatio.cx;
    m_u32AspectRatioHeight = lpAllocInfo->szAspectRatio.cy;

    // surface size change check
    DWORD dwNW = lpAllocInfo->dwWidth, dwNH = lpAllocInfo->dwHeight,
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
        __m128i xVS = _mm_loadl_epi64(reinterpret_cast<__m128i*>(&lpAllocInfo->dwWidth));
        __m128d x0 = _mm_cvtepi32_pd(xVS);// __int32 to double
        _mm_storel_epi64(reinterpret_cast<__m128i*>(&m_u32VideoWidth), xVS);// also stores m_u32VideoHeight
        __m128 x1 = _mm_cvtpd_ps(x0);// double to float
        _mm_store_pd(&m_dVideoWidth, x0);;// also stores m_dVideoHeight
        x2 = _mm_div_ps(x2, x1);// reciprocal trough _mm_rcp_ps() isn't accurate
        _mm_storel_pi(reinterpret_cast<__m64*>(&m_fVideoWidth), x1);// not an MMX function, also stores m_fVideoHeight
        _mm_storel_pi(reinterpret_cast<__m64*>(&m_fVideoWidthr), x2);// not an MMX function, also stores m_fVideoHeightr
#else
        m_u32VideoWidth = lpAllocInfo->dwWidth;
        m_u32VideoHeight = lpAllocInfo->dwHeight;
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

        DeleteSurfaces();
        AllocSurfaces();
        HRESULT hr;
        if (FAILED(hr = m_pD3DDev->ColorFill(m_apVideoSurface[m_u8CurrentMixerSurface], nullptr, 0))) { return hr; }
        Paint(FRENDERPAINT_INIT);// initialize the main renderer loop

        // GPU command queue flush
        ASSERT(!m_pUtilEventQuery);
        if (SUCCEEDED(m_pD3DDev->CreateQuery(D3DQUERYTYPE_EVENT, &m_pUtilEventQuery))) {
            m_pUtilEventQuery->Issue(D3DISSUE_END);
            EXECUTE_ASSERT(QueryPerformanceCounter(&m_liLastPerfCnt));
            __int64 i64CntMax = m_liLastPerfCnt.QuadPart + (m_u64PerfFreq << 3);// timeout after 8 s

            while (S_FALSE == m_pUtilEventQuery->GetData(nullptr, 0, D3DGETDATA_FLUSH)) {
                EXECUTE_ASSERT(QueryPerformanceCounter(&m_liLastPerfCnt));
                if (m_liLastPerfCnt.QuadPart > i64CntMax) {
                    break;
                }
            }
            m_pUtilEventQuery->Release();
            m_pUtilEventQuery = nullptr;
        }
    } else if (!m_apVideoSurface[0]) {
        AllocSurfaces();// just renew surfaces
    }

    if (m_hEvtReset) {
        // when in a complete reset, keep MainThread on hold until the mixer is alive again
        SetEvent(m_hEvtReset);
    }
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CVMR9AllocatorPresenter::TerminateDevice(DWORD_PTR dwUserID)
{
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CVMR9AllocatorPresenter::GetSurface(DWORD_PTR dwUserID, DWORD SurfaceIndex, DWORD SurfaceFlags, IDirect3DSurface9** lplpSurface)
{
    ASSERT(lplpSurface);

    if (!m_apVideoSurface[0]) {
        return E_FAIL;
    }

    // rotate the index
    ASSERT(!SurfaceIndex);// never changes, so we have to do it ourselves
    unsigned __int8 u8ISI = m_u8InputVMR9MixerSurface + 1;
    if (u8ISI >= m_u8MixerSurfaceCount) {
        u8ISI = 0;
    }
    m_u8InputVMR9MixerSurface = u8ISI;
    if (m_u8CurrentMixerSurface == m_u8InputVMR9MixerSurface) {
        CAutoLock cRenderLock(&m_csRenderLock);
    }
    m_u8MixerSurfacesUsed = (m_u8InputVMR9MixerSurface > m_u8CurrentMixerSurface) ? m_u8InputVMR9MixerSurface - m_u8CurrentMixerSurface : m_u8InputVMR9MixerSurface + m_u8MixerSurfaceCount - m_u8CurrentMixerSurface;
    // m_u8CurrentMixerSurface is set when the mixer calls into PresentImage()
    IDirect3DSurface9* pSurface = m_apVideoSurface[m_u8InputVMR9MixerSurface];
    *lplpSurface = pSurface;
    pSurface->AddRef();
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CVMR9AllocatorPresenter::AdviseNotify(IVMRSurfaceAllocatorNotify9* lpIVMRSurfAllocNotify)
{
    ASSERT(lpIVMRSurfAllocNotify);

    CAutoLock cRenderLock(&m_csRenderLock);
    if (m_pIVMRSurfAllocNotify) {
        m_pIVMRSurfAllocNotify->Release();
    }
    m_pIVMRSurfAllocNotify = lpIVMRSurfAllocNotify;// reference inherited
    return lpIVMRSurfAllocNotify->SetD3DDevice(m_pD3DDev, m_hCurrentMonitor);
}

// IVMRImagePresenter9

__declspec(nothrow noalias) STDMETHODIMP CVMR9AllocatorPresenter::StartPresenting(DWORD_PTR dwUserID)
{
    return m_pD3DDev ? S_OK : E_FAIL;
}
//  DEBUG_ONLY(SetThreadName(0xFFFFFFFF, "CVMR9AllocatorPresenter"));

__declspec(nothrow noalias) STDMETHODIMP CVMR9AllocatorPresenter::StopPresenting(DWORD_PTR dwUserID)
{
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CVMR9AllocatorPresenter::PresentImage(DWORD_PTR dwUserID, VMR9PresentationInfo* lpPresInfo)
{
    ASSERT(lpPresInfo);

    if (!m_dStreamReferenceVideoFrameRate || m_bNeedCheckSample) {
        m_bNeedCheckSample = false;

        IPin* pPin;
        if (SUCCEEDED(m_OuterVMR.m_pBaseFilter->FindPin(L"VMR Input0", &pPin))) {
            AM_MEDIA_TYPE mt;
            if (SUCCEEDED(pPin->ConnectionMediaType(&mt))) {
                // the frame time and aspect ratio are the only things that are sometimes reported wrong during mixer initialization
                // the input video format isn't indicated at all during initialization
                if ((mt.formattype == FORMAT_VideoInfo) || (mt.formattype == FORMAT_MPEGVideo)) {
                    VIDEOINFOHEADER* fV = reinterpret_cast<VIDEOINFOHEADER*>(mt.pbFormat);
                    unsigned __int8 u8ChromaType = GetChromaType(fV->bmiHeader.biCompression);
                    if (u8ChromaType) {// if the input is 4:4:4, the chroma cositing data is irrelevant
                        u8ChromaType |= 0x80;// the high bit is used to indicate that the Y'CbCr input is not horizontally chroma cosited (MPEG1-type), which is what we assume for types using VIDEOINFOHEADER
                    }
                    m_u8ChromaType = u8ChromaType;
                    m_szChromaCositing[0] = '0' + (u8ChromaType >> 7);// m_szChromaCositing and m_u8ChromaType should always be synchronized
                    __int64 i64StreamATPF = fV->AvgTimePerFrame;
                    if (!i64StreamATPF) {
                        goto NoFrameTimeData;
                    }
                    double dRate = 10000000.0 / static_cast<double>(i64StreamATPF);
                    m_dStreamReferenceVideoFrameRate = dRate;
                    dRate = RoundCommonRates(dRate);
                    m_dRoundedStreamReferenceVideoFrameRate = m_dDetectedVideoFrameRate = dRate;
                    m_dDetectedVideoTimePerFrame = 1.0 / dRate;
                } else if ((mt.formattype == FORMAT_VideoInfo2) || (mt.formattype == FORMAT_MPEG2Video) || (mt.formattype == FORMAT_DiracVideoInfo)) {
                    VIDEOINFOHEADER2* fV = reinterpret_cast<VIDEOINFOHEADER2*>(mt.pbFormat);
                    unsigned __int8 u8ChromaType = GetChromaType(fV->bmiHeader.biCompression);
                    if (u8ChromaType) {// if the input is 4:4:4, the chroma cositing data is irrelevant
                        DWORD dwControlFlags = fV->dwControlFlags;
                        if ((dwControlFlags & AMCONTROL_USED) && (dwControlFlags & AMCONTROL_COLORINFO_PRESENT)) {
                            // see DXVA_ExtendedFormat and DXVA_VideoChromaSubsampling for reference
                            ASSERT(!(dwControlFlags & 0x200));// see DXVA_VideoChromaSubsampling_Vertically_Cosited, the renderer can't handle vertically cosited chroma (yet)
                            u8ChromaType |= dwControlFlags >> 3 & 0x80;// see DXVA_VideoChromaSubsampling_Horizontally_Cosited, tranfer bit to high bit in m_u8ChromaType
                        }// the high bit of m_u8ChromaType is used to indicate that the Y'CbCr input is not horizontally chroma cosited (MPEG1-type), which is rare among modern video formats, so we don't use it by default
                    }
                    m_u8ChromaType = u8ChromaType;
                    m_szChromaCositing[0] = '0' + (u8ChromaType >> 7);// m_szChromaCositing and m_u8ChromaType should always be synchronized
                    m_u32AspectRatioWidth = fV->dwPictAspectRatioX;
                    m_u32AspectRatioHeight = fV->dwPictAspectRatioY;
                    __int64 i64StreamATPF = fV->AvgTimePerFrame;
                    if (!i64StreamATPF) {
                        goto NoFrameTimeData;
                    }
                    double dRate = 10000000.0 / static_cast<double>(i64StreamATPF);
                    m_dStreamReferenceVideoFrameRate = dRate;
                    dRate = RoundCommonRates(dRate);
                    m_dRoundedStreamReferenceVideoFrameRate = m_dDetectedVideoFrameRate = dRate;
                    m_dDetectedVideoTimePerFrame = 1.0 / dRate;
                }
            }
NoFrameTimeData:

            IPin* pPinTo;
            if (SUCCEEDED(pPin->ConnectedTo(&pPinTo))) {
                m_strDecoder = GetFilterName(GetFilterFromPin(pPinTo));
                pPinTo->Release();
            }
            pPin->Release();
        }

        if (!m_dStreamReferenceVideoFrameRate) {// framerate not set by video decoder, choose 25 fps, like the subtitle renderer's default
            m_dStreamReferenceVideoFrameRate = m_dRoundedStreamReferenceVideoFrameRate = m_dDetectedVideoFrameRate = 25.0;
            m_dDetectedVideoTimePerFrame = 0.04;
        }

        m_pSubPicQueue->SetFPS(m_dDetectedVideoFrameRate);
    }

    if (m_bUseInternalTimer && !g_bExternalSubtitleTime && (lpPresInfo->rtEnd > lpPresInfo->rtStart)) {
        CSubPicAllocatorPresenterImpl::SetTime(g_tSegmentStart + g_tSampleStart);
    }

    IDirect3DSurface9* pInputVideoSurface = lpPresInfo->lpSurf;
    unsigned __int8 i = m_u8MixerSurfaceCount - 1;
    do {// optimization: 0 is default output from this loop, so it isn't checked
        if (pInputVideoSurface == m_apVideoSurface[i]) {
            break;
        }
    } while (--i); // not ideal, but the mixer core scheduler doesn't expose much
    m_u8CurrentMixerSurface = i;

    Paint(FRENDERPAINT_NORMAL);
    return S_OK;
}

// IVMRWindowlessControl9
//
// It is only implemented (partially) for the dvd navigator's
// menu handling, which needs to know a few things about the
// location of our window.

__declspec(nothrow noalias) STDMETHODIMP CVMR9AllocatorPresenter::GetNativeVideoSize(LONG* lpWidth, LONG* lpHeight, LONG* lpARWidth, LONG* lpARHeight)
{
    ASSERT(lpWidth);
    ASSERT(lpHeight);
    ASSERT(lpARWidth);
    ASSERT(lpARHeight);

    *lpWidth = m_u32VideoWidth;
    *lpHeight = m_u32VideoHeight;
    *lpARWidth = m_u32AspectRatioWidth;
    *lpARHeight = m_u32AspectRatioHeight;
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CVMR9AllocatorPresenter::GetMinIdealVideoSize(LONG* lpWidth, LONG* lpHeight)
{
    return E_NOTIMPL;
}

__declspec(nothrow noalias) STDMETHODIMP CVMR9AllocatorPresenter::GetMaxIdealVideoSize(LONG* lpWidth, LONG* lpHeight)
{
    return E_NOTIMPL;
}

__declspec(nothrow noalias) STDMETHODIMP CVMR9AllocatorPresenter::SetVideoPosition(LPRECT const lpSRCRect, LPRECT const lpDSTRect)
{
    return E_NOTIMPL;// we have our own method for this
}

__declspec(nothrow noalias) STDMETHODIMP CVMR9AllocatorPresenter::GetVideoPosition(LPRECT lpSRCRect, LPRECT lpDSTRect)
{
    ASSERT(lpSRCRect);
    ASSERT(lpDSTRect);

    lpSRCRect->left = 0;
    lpSRCRect->top = 0;
    lpSRCRect->right = m_u32VideoWidth;
    lpSRCRect->bottom = m_u32VideoHeight;
    *lpDSTRect = m_VideoRect;
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CVMR9AllocatorPresenter::GetAspectRatioMode(DWORD* lpAspectRatioMode)
{
    ASSERT(lpAspectRatioMode);

    *lpAspectRatioMode = AM_ARMODE_LETTER_BOX;
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CVMR9AllocatorPresenter::SetAspectRatioMode(DWORD AspectRatioMode)
{
    return E_NOTIMPL;
}

__declspec(nothrow noalias) STDMETHODIMP CVMR9AllocatorPresenter::SetVideoClippingWindow(HWND hwnd)
{
    return E_NOTIMPL;
}

__declspec(nothrow noalias) STDMETHODIMP CVMR9AllocatorPresenter::RepaintVideo(HWND hwnd, HDC hdc)
{
    return E_NOTIMPL;
}

__declspec(nothrow noalias) STDMETHODIMP CVMR9AllocatorPresenter::DisplayModeChanged()
{
    return E_NOTIMPL;
}

__declspec(nothrow noalias) STDMETHODIMP CVMR9AllocatorPresenter::GetCurrentImage(BYTE** lpDib)
{
    return E_NOTIMPL;
}

__declspec(nothrow noalias) STDMETHODIMP CVMR9AllocatorPresenter::SetBorderColor(COLORREF Clr)
{
    return E_NOTIMPL;
}

__declspec(nothrow noalias) STDMETHODIMP CVMR9AllocatorPresenter::GetBorderColor(COLORREF* lpClr)
{
    ASSERT(lpClr);

    *lpClr = 0;
    return S_OK;
}
