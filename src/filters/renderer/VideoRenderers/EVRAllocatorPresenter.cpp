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
#include "OuterEVR.h"
#include "EVRAllocatorPresenter.h"
#include <Mferror.h>
#include <moreuuids.h>
#include <intrin.h>
#include "../../../thirdparty/VirtualDub/h/vd2/system/cpuaccel.h"

typedef enum {
    MSG_MIXERIN,
    MSG_MIXEROUT
} EVR_STATS_MSG;

/*
// === Helper functions

static __declspec(nothrow noalias) __forceinline MFOffset MakeOffset(double v)
{
    MFOffset offset;
    double im = floor(v);
    offset.value = static_cast<short>(v);
    offset.fract = static_cast<__int32>(65536.0 * (v - im));// the standard converter only does a proper job with signed values
    return offset;
}

static __declspec(nothrow noalias) __forceinline MFVideoArea MakeArea(double x, double y, LONG width, LONG height)
{
    MFVideoArea area;
    area.OffsetX = MakeOffset(x);
    area.OffsetY = MakeOffset(y);
    area.Area.cx = width;
    area.Area.cy = height;
    return area;
}
*/

using namespace DSObjects;

// MSArrayQueue

__declspec(nothrow noalias) __forceinline void MSArrayQueue::ReleaseOnAllDestructorType()
{
    ASSERT((m_u8QueueElemCount == (m_u8QueueListEnd - m_u8QueueListBeginning & 15)) || ((m_u8QueueElemCount == 16) && !(m_u8QueueListEnd - m_u8QueueListBeginning & 15)));

    unsigned __int8 u8QueueElemCount = m_u8QueueElemCount;
    if (u8QueueElemCount) {
        unsigned __int8 u8QueueListBeginning = m_u8QueueListBeginning;
        do {
            ULONG u = m_paData[u8QueueListBeginning]->Release();
            ASSERT(!u);
            UNREFERENCED_PARAMETER(u);
            DEBUG_ONLY(m_paData[u8QueueListBeginning] = reinterpret_cast<IMFSample*>(MAXSIZE_T));// error check: make sure the pointer is invalid
            u8QueueListBeginning = u8QueueListBeginning + 1 & 15;
        } while (--u8QueueElemCount);
    }
    DEBUG_ONLY(m_u8QueueListBeginning = m_u8QueueListEnd = m_u8QueueElemCount = MAXUINT8);// error check: make sure the values controlling the queue are invalid
}

__declspec(nothrow noalias) __forceinline void MSArrayQueue::ReleaseOnAllAndReset()
{
    ASSERT((m_u8QueueElemCount == (m_u8QueueListEnd - m_u8QueueListBeginning & 15)) || ((m_u8QueueElemCount == 16) && !(m_u8QueueListEnd - m_u8QueueListBeginning & 15)));

    unsigned __int8 u8QueueElemCount = m_u8QueueElemCount;
    if (u8QueueElemCount) {
        unsigned __int8 u8QueueListBeginning = m_u8QueueListBeginning;
        do {
            m_paData[u8QueueListBeginning]->Release();
            DEBUG_ONLY(m_paData[u8QueueListBeginning] = reinterpret_cast<IMFSample*>(MAXSIZE_T));// error check: make sure the pointer is invalid
            u8QueueListBeginning = u8QueueListBeginning + 1 & 15;
        } while (--u8QueueElemCount);
        m_u8QueueListBeginning = m_u8QueueListEnd = m_u8QueueElemCount = 0;
    }
}

__declspec(nothrow noalias) __forceinline void MSArrayQueue::MoveContentToOther(MSArrayQueue* pOther)
{
    ASSERT((m_u8QueueElemCount == (m_u8QueueListEnd - m_u8QueueListBeginning & 15)) || ((m_u8QueueElemCount == 16) && !(m_u8QueueListEnd - m_u8QueueListBeginning & 15)));
    ASSERT(pOther && (pOther != this));
    ASSERT(pOther->m_u8QueueElemCount + m_u8QueueElemCount <= 16);
    ASSERT((pOther->m_u8QueueElemCount == (pOther->m_u8QueueListEnd - pOther->m_u8QueueListBeginning & 15)) || ((pOther->m_u8QueueElemCount == 16) && !(pOther->m_u8QueueListEnd - pOther->m_u8QueueListBeginning & 15)));

    unsigned __int8 u8QueueElemCount = m_u8QueueElemCount;
    if (u8QueueElemCount) {
        unsigned __int8 u8QueueListBeginning = m_u8QueueListBeginning;
        unsigned __int8 u8OtherQueueListEnd = pOther->m_u8QueueListEnd;
        pOther->m_u8QueueElemCount += u8QueueElemCount;
        do {
            pOther->m_paData[u8OtherQueueListEnd] = m_paData[u8QueueListBeginning];
            DEBUG_ONLY(m_paData[u8QueueListBeginning] = reinterpret_cast<IMFSample*>(MAXSIZE_T));// error check: make sure the pointer is invalid
            u8OtherQueueListEnd = u8OtherQueueListEnd + 1 & 15;
            u8QueueListBeginning = u8QueueListBeginning + 1 & 15;
        } while (--u8QueueElemCount);
        pOther->m_u8QueueListEnd = u8OtherQueueListEnd;
        m_u8QueueListBeginning = m_u8QueueListEnd = m_u8QueueElemCount = 0;
    }
}

__declspec(nothrow noalias) __forceinline void MSArrayQueue::Enqueue(IMFSample* pItem)
{
    ASSERT((m_u8QueueElemCount == (m_u8QueueListEnd - m_u8QueueListBeginning & 15)) || ((m_u8QueueElemCount == 16) && !(m_u8QueueListEnd - m_u8QueueListBeginning & 15)));
    ASSERT(pItem && (pItem != reinterpret_cast<IMFSample*>(MAXSIZE_T)));// error check: make sure the pointer is valid
    ASSERT(m_u8QueueElemCount < 16);// error check: make sure we aren't exceeding our maximum storage space

    unsigned __int8 u8QueueListEnd = m_u8QueueListEnd;
    m_paData[u8QueueListEnd] = pItem;
    ++m_u8QueueElemCount;
    m_u8QueueListEnd = u8QueueListEnd + 1 & 15;
}

__declspec(nothrow noalias) __forceinline void MSArrayQueue::EnqueueReverse(IMFSample* pItem)
{
    ASSERT((m_u8QueueElemCount == (m_u8QueueListEnd - m_u8QueueListBeginning & 15)) || ((m_u8QueueElemCount == 16) && !(m_u8QueueListEnd - m_u8QueueListBeginning & 15)));
    ASSERT(pItem && (pItem != reinterpret_cast<IMFSample*>(MAXSIZE_T)));// error check: make sure the pointer is valid
    ASSERT(m_u8QueueElemCount < 16);// error check: make sure we aren't dequeueing from an empty queue

    unsigned __int8 u8QueueListBeginning = m_u8QueueListBeginning - 1 & 15;
    ++m_u8QueueElemCount;
    m_u8QueueListBeginning = u8QueueListBeginning;
    m_paData[u8QueueListBeginning] = pItem;
}

__declspec(nothrow noalias restrict) __forceinline IMFSample* MSArrayQueue::Dequeue()
{
    ASSERT((m_u8QueueElemCount == (m_u8QueueListEnd - m_u8QueueListBeginning & 15)) || ((m_u8QueueElemCount == 16) && !(m_u8QueueListEnd - m_u8QueueListBeginning & 15)));
    ASSERT(m_u8QueueElemCount);// error check: make sure we aren't dequeueing from an empty queue

    unsigned __int8 u8QueueListBeginning = m_u8QueueListBeginning;
    IMFSample* pReturnValue = m_paData[u8QueueListBeginning];
    DEBUG_ONLY(m_paData[u8QueueListBeginning] = reinterpret_cast<IMFSample*>(MAXSIZE_T));// error check: make sure the pointer is invalid
    --m_u8QueueElemCount;
    m_u8QueueListBeginning = u8QueueListBeginning + 1 & 15;
    return pReturnValue;
}

__declspec(nothrow noalias) __forceinline void MSArrayQueue::Lock()
{
    goto StronglyOrderedLoopSkipFirst;
    do {
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
        _mm_pause();
#else
        YieldProcessor();
#endif
StronglyOrderedLoopSkipFirst:
        ;
    } while (!_InterlockedCompareExchange8(&mv_cLock, 0, 1));
}

__declspec(nothrow noalias) __forceinline void MSArrayQueue::Unlock()
{
    _ReadWriteBarrier();// a no-op, this is only prevent the compiler from reordering
    ASSERT(!mv_cLock);// it should be locked
    mv_cLock = 1;
}

// CEVRAllocatorPresenter

__declspec(nothrow noalias) __forceinline HRESULT CEVRAllocatorPresenter::CheckShutdown() const
{
    if (m_nRenderState == Shutdown) {
        return MF_E_SHUTDOWN;
    } else {
        return S_OK;
    }
}

// IUnknown

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::QueryInterface(REFIID riid, __deref_out void** ppv)
{
    ASSERT(ppv);
    __assume(this);// fix assembly: the compiler generated tests for null pointer input on static_cast<T>(this)
    // __assume(&m_OuterEVR); didn't work, resolved the issue by manually offsetting the pointers on reinterpret_cast
    // static_assert won't take anything with static_cast, so we test at debug runtime
    // the fake pointer can't be 0 on input, else the compiler will not offset it on static_cast
    ASSERT(reinterpret_cast<uintptr_t>(static_cast<IBaseFilter*>(reinterpret_cast<COuterEVR*>(64))) == 64);
    ASSERT(reinterpret_cast<uintptr_t>(static_cast<IMediaFilter*>(reinterpret_cast<COuterEVR*>(64))) == 64);
    ASSERT(reinterpret_cast<uintptr_t>(static_cast<IPersist*>(reinterpret_cast<COuterEVR*>(64))) == 64);
    ASSERT(reinterpret_cast<uintptr_t>(static_cast<IKsPropertySet*>(reinterpret_cast<COuterEVR*>(64))) == 64 + sizeof(uintptr_t));

    void* pv = static_cast<IUnknown*>(static_cast<CSubPicAllocatorPresenterImpl*>(this));// CSubPicAllocatorPresenterImpl is at Vtable location 0
    if (CPUGetEnabledExtensions() & CPUF_SUPPORTS_SSE41) {// SSE4.1 code path
        {
            __m128i xIIDin = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&riid));
            __m128i xIID_IUnknown = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IUnknown));
            __m128i xIID_SubPicAllocatorPresenterImpl = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&__uuidof(CSubPicAllocatorPresenterImpl)));
            __m128i xIID_IVMRMixerBitmap9 = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IVMRMixerBitmap9));
            __m128i xIID_IVMRffdshow9 = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IVMRffdshow9));
            __m128i xIID_IMFVideoPresenter = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IMFVideoPresenter));
            __m128i xIID_IDirect3DDeviceManager9 = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IDirect3DDeviceManager9));
            __m128i xIID_IMFTopologyServiceLookupClient = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IMFTopologyServiceLookupClient));
            xIID_IUnknown = _mm_xor_si128(xIID_IUnknown, xIIDin);
            xIID_SubPicAllocatorPresenterImpl = _mm_xor_si128(xIID_SubPicAllocatorPresenterImpl, xIIDin);
            xIID_IVMRMixerBitmap9 = _mm_xor_si128(xIID_IVMRMixerBitmap9, xIIDin);
            // this class
            if (_mm_testz_si128(xIID_IUnknown, xIID_IUnknown)) {
                goto exitRThisSSE41;
            }
            __m128i xIID_IMFVideoDeviceID = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IMFVideoDeviceID));
            xIID_IVMRffdshow9 = _mm_xor_si128(xIID_IVMRffdshow9, xIIDin);
            pv = static_cast<CSubPicAllocatorPresenterImpl*>(this);
            if (_mm_testz_si128(xIID_SubPicAllocatorPresenterImpl, xIID_SubPicAllocatorPresenterImpl)) {
                goto exitRThisSSE41;
            }
            __m128i xIID_IMFVideoDisplayControl = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IMFVideoDisplayControl));
            xIID_IMFVideoPresenter = _mm_xor_si128(xIID_IMFVideoPresenter, xIIDin);
            pv = static_cast<IVMRMixerBitmap9*>(this);
            if (_mm_testz_si128(xIID_IVMRMixerBitmap9, xIID_IVMRMixerBitmap9)) {
                goto exitRThisSSE41;
            }
            __m128i xIID_IEVRTrustedVideoPlugin = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IEVRTrustedVideoPlugin));
            xIID_IDirect3DDeviceManager9 = _mm_xor_si128(xIID_IDirect3DDeviceManager9, xIIDin);
            pv = static_cast<IVMRffdshow9*>(this);
            if (_mm_testz_si128(xIID_IVMRffdshow9, xIID_IVMRffdshow9)) {
                goto exitRThisSSE41;
            }
            __m128i xIID_IQualProp = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IQualProp));
            xIID_IMFTopologyServiceLookupClient = _mm_xor_si128(xIID_IMFTopologyServiceLookupClient, xIIDin);
            pv = static_cast<IMFVideoPresenter*>(this);
            if (_mm_testz_si128(xIID_IMFVideoPresenter, xIID_IMFVideoPresenter)) {
                goto exitRThisSSE41;
            }
            __m128i xIID_IMFRateSupport = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IMFRateSupport));
            xIID_IMFVideoDeviceID = _mm_xor_si128(xIID_IMFVideoDeviceID, xIIDin);
            pv = static_cast<IDirect3DDeviceManager9*>(this);
            if (_mm_testz_si128(xIID_IDirect3DDeviceManager9, xIID_IDirect3DDeviceManager9)) {
                goto exitRThisSSE41;
            }
            __m128i xIID_IMFAsyncCallback = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IMFAsyncCallback));
            xIID_IMFVideoDisplayControl = _mm_xor_si128(xIID_IMFVideoDisplayControl, xIIDin);
            pv = static_cast<IMFTopologyServiceLookupClient*>(this);
            if (_mm_testz_si128(xIID_IMFTopologyServiceLookupClient, xIID_IMFTopologyServiceLookupClient)) {
                goto exitRThisSSE41;
            }
            __m128i xIID_IMFGetService = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IMFGetService));
            xIID_IEVRTrustedVideoPlugin = _mm_xor_si128(xIID_IEVRTrustedVideoPlugin, xIIDin);
            pv = static_cast<IMFVideoDeviceID*>(this);
            if (_mm_testz_si128(xIID_IMFVideoDeviceID, xIID_IMFVideoDeviceID)) {
                goto exitRThisSSE41;
            }
            __m128i xIID_IMFClockStateSink = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IMFClockStateSink));
            xIID_IQualProp = _mm_xor_si128(xIID_IQualProp, xIIDin);
            pv = static_cast<IMFVideoDisplayControl*>(this);
            if (_mm_testz_si128(xIID_IMFVideoDisplayControl, xIID_IMFVideoDisplayControl)) {
                goto exitRThisSSE41;
            }
            __m128i xIID_IBaseFilter = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IBaseFilter));
            xIID_IMFRateSupport = _mm_xor_si128(xIID_IMFRateSupport, xIIDin);
            pv = static_cast<IEVRTrustedVideoPlugin*>(this);
            if (_mm_testz_si128(xIID_IEVRTrustedVideoPlugin, xIID_IEVRTrustedVideoPlugin)) {
                goto exitRThisSSE41;
            }
            __m128i xIID_IMediaFilter = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IMediaFilter));
            xIID_IMFAsyncCallback = _mm_xor_si128(xIID_IMFAsyncCallback, xIIDin);
            pv = static_cast<IQualProp*>(this);
            if (_mm_testz_si128(xIID_IQualProp, xIID_IQualProp)) {
                goto exitRThisSSE41;
            }
            __m128i xIID_IPersist = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IPersist));
            xIID_IMFGetService = _mm_xor_si128(xIID_IMFGetService, xIIDin);
            pv = static_cast<IMFRateSupport*>(this);
            if (_mm_testz_si128(xIID_IMFRateSupport, xIID_IMFRateSupport)) {
                goto exitRThisSSE41;
            }
            __m128i xIID_IKsPropertySet = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IKsPropertySet));
            xIID_IMFClockStateSink = _mm_xor_si128(xIID_IMFClockStateSink, xIIDin);
            pv = static_cast<IMFAsyncCallback*>(this);
            if (_mm_testz_si128(xIID_IMFAsyncCallback, xIID_IMFAsyncCallback)) {
                goto exitRThisSSE41;
            }
            __m128i xIID_IDirectXVideoDecoderService = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IDirectXVideoDecoderService));
            xIID_IBaseFilter = _mm_xor_si128(xIID_IBaseFilter, xIIDin);
            pv = static_cast<IMFGetService*>(this);
            if (_mm_testz_si128(xIID_IMFGetService, xIID_IMFGetService)) {
                goto exitRThisSSE41;
            }
            __m128i xIID_IDirectXVideoProcessorService = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IDirectXVideoProcessorService));
            xIID_IMediaFilter = _mm_xor_si128(xIID_IMediaFilter, xIIDin);
            pv = static_cast<IMFClockStateSink*>(this);
            if (_mm_testz_si128(xIID_IMFClockStateSink, xIID_IMFClockStateSink)) {
                goto exitRThisSSE41;
            }
            __m128i xIID_IDirectXVideoAccelerationService = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&IID_IDirectXVideoAccelerationService));
            xIID_IPersist = _mm_xor_si128(xIID_IPersist, xIIDin);
            // COuterEVR
            pv = reinterpret_cast<void*>(&m_OuterEVR);
            if (_mm_testz_si128(xIID_IBaseFilter, xIID_IBaseFilter)) {
                goto exitROuterEVRSSE41;
            }
            xIID_IKsPropertySet = _mm_xor_si128(xIID_IKsPropertySet, xIIDin);
            if (_mm_testz_si128(xIID_IMediaFilter, xIID_IMediaFilter)) {
                goto exitROuterEVRSSE41;
            }
            xIID_IDirectXVideoDecoderService = _mm_xor_si128(xIID_IDirectXVideoDecoderService, xIIDin);
            if (_mm_testz_si128(xIID_IPersist, xIID_IPersist)) {
                goto exitROuterEVRSSE41;
            }
            xIID_IDirectXVideoProcessorService = _mm_xor_si128(xIID_IDirectXVideoProcessorService, xIIDin);
            pv = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(&m_OuterEVR) + sizeof(uintptr_t));
            if (_mm_testz_si128(xIID_IKsPropertySet, xIID_IKsPropertySet)) {
                goto exitROuterEVRSSE41;
            }
            xIID_IDirectXVideoAccelerationService = _mm_xor_si128(xIID_IDirectXVideoAccelerationService, xIIDin);
            // external Direct3DDeviceManager9 object
            if (_mm_testz_si128(xIID_IDirectXVideoDecoderService, xIID_IDirectXVideoDecoderService)) {
                goto exitQD3DManagerSSE41;
            }
            if (_mm_testz_si128(xIID_IDirectXVideoProcessorService, xIID_IDirectXVideoProcessorService)) {
                goto exitQD3DManagerSSE41;
            }
            if (_mm_testz_si128(xIID_IDirectXVideoAccelerationService, xIID_IDirectXVideoAccelerationService)) {
                goto exitQD3DManagerSSE41;
            }
        }
        // lastly, try the external EVR
        return m_OuterEVR.m_pEVR->QueryInterface(riid, ppv);
exitQD3DManagerSSE41:
        return m_pD3DManager->QueryInterface(riid, ppv);
exitROuterEVRSSE41:
        *ppv = pv;
        ULONG ulRefOuterEVR = _InterlockedIncrement(reinterpret_cast<LONG volatile*>(&m_OuterEVR.mv_ulReferenceCount));
        ASSERT(ulRefOuterEVR);
        UNREFERENCED_PARAMETER(ulRefOuterEVR);
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
    pv = static_cast<IMFVideoPresenter*>(this);
    if (lo == reinterpret_cast<__int64 const*>(&IID_IMFVideoPresenter)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IMFVideoPresenter)[1]) {
        goto exitRThis;
    }
    pv = static_cast<IDirect3DDeviceManager9*>(this);
    if (lo == reinterpret_cast<__int64 const*>(&IID_IDirect3DDeviceManager9)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IDirect3DDeviceManager9)[1]) {
        goto exitRThis;
    }
    pv = static_cast<IMFTopologyServiceLookupClient*>(this);
    if (lo == reinterpret_cast<__int64 const*>(&IID_IMFTopologyServiceLookupClient)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IMFTopologyServiceLookupClient)[1]) {
        goto exitRThis;
    }
    pv = static_cast<IMFVideoDeviceID*>(this);
    if (lo == reinterpret_cast<__int64 const*>(&IID_IMFVideoDeviceID)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IMFVideoDeviceID)[1]) {
        goto exitRThis;
    }
    pv = static_cast<IMFVideoDisplayControl*>(this);
    if (lo == reinterpret_cast<__int64 const*>(&IID_IMFVideoDisplayControl)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IMFVideoDisplayControl)[1]) {
        goto exitRThis;
    }
    pv = static_cast<IEVRTrustedVideoPlugin*>(this);
    if (lo == reinterpret_cast<__int64 const*>(&IID_IEVRTrustedVideoPlugin)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IEVRTrustedVideoPlugin)[1]) {
        goto exitRThis;
    }
    pv = static_cast<IQualProp*>(this);
    if (lo == reinterpret_cast<__int64 const*>(&IID_IQualProp)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IQualProp)[1]) {
        goto exitRThis;
    }
    pv = static_cast<IMFRateSupport*>(this);
    if (lo == reinterpret_cast<__int64 const*>(&IID_IMFRateSupport)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IMFRateSupport)[1]) {
        goto exitRThis;
    }
    pv = static_cast<IMFAsyncCallback*>(this);
    if (lo == reinterpret_cast<__int64 const*>(&IID_IMFAsyncCallback)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IMFAsyncCallback)[1]) {
        goto exitRThis;
    }
    pv = static_cast<IMFGetService*>(this);
    if (lo == reinterpret_cast<__int64 const*>(&IID_IMFGetService)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IMFGetService)[1]) {
        goto exitRThis;
    }
    pv = static_cast<IMFClockStateSink*>(this);
    if (lo == reinterpret_cast<__int64 const*>(&IID_IMFClockStateSink)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IMFClockStateSink)[1]) {
        goto exitRThis;
    }
    // COuterEVR
    pv = reinterpret_cast<void*>(&m_OuterEVR);
    if (lo == reinterpret_cast<__int64 const*>(&IID_IBaseFilter)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IBaseFilter)[1]) {
        goto exitROuterEVR;
    }
    if (lo == reinterpret_cast<__int64 const*>(&IID_IMediaFilter)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IMediaFilter)[1]) {
        goto exitROuterEVR;
    }
    if (lo == reinterpret_cast<__int64 const*>(&IID_IPersist)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IPersist)[1]) {
        goto exitROuterEVR;
    }
    pv = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(&m_OuterEVR) + sizeof(uintptr_t));
    if (lo == reinterpret_cast<__int64 const*>(&IID_IKsPropertySet)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IKsPropertySet)[1]) {
        goto exitROuterEVR;
    }
    // external Direct3DDeviceManager9 object
    if (lo == reinterpret_cast<__int64 const*>(&IID_IDirectXVideoDecoderService)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IDirectXVideoDecoderService)[1]) {
        goto exitQD3DManager;
    }
    if (lo == reinterpret_cast<__int64 const*>(&IID_IDirectXVideoProcessorService)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IDirectXVideoProcessorService)[1]) {
        goto exitQD3DManager;
    }
    if (lo == reinterpret_cast<__int64 const*>(&IID_IDirectXVideoAccelerationService)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IDirectXVideoAccelerationService)[1]) {
        goto exitQD3DManager;
    }
    // lastly, try the external EVR
    return m_OuterEVR.m_pEVR->QueryInterface(riid, ppv);
exitQD3DManager:
    return m_pD3DManager->QueryInterface(riid, ppv);
exitROuterEVR:
    *ppv = pv;
    ULONG ulRefOuterEVR = _InterlockedIncrement(reinterpret_cast<LONG volatile*>(&m_OuterEVR.mv_ulReferenceCount));
    ASSERT(ulRefOuterEVR);
    UNREFERENCED_PARAMETER(ulRefOuterEVR);
    return NOERROR;
exitRThis:
    *ppv = pv;
    ULONG ulRef = _InterlockedIncrement(reinterpret_cast<LONG volatile*>(&mv_ulReferenceCount));
    ASSERT(ulRef);
    UNREFERENCED_PARAMETER(ulRef);
    return NOERROR;
}

__declspec(nothrow noalias) STDMETHODIMP_(ULONG) CEVRAllocatorPresenter::AddRef()
{
    // based on CUnknown::NonDelegatingAddRef()
    // the original CUnknown::NonDelegatingAddRef() has a version that keeps compatibility for Windows 95, Windows NT 3.51 and earlier, this one doesn't
    ULONG ulRef = _InterlockedIncrement(reinterpret_cast<LONG volatile*>(&mv_ulReferenceCount));
    ASSERT(ulRef);
    return ulRef;
}

__declspec(nothrow noalias) STDMETHODIMP_(ULONG) CEVRAllocatorPresenter::Release()
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
        this->~CEVRAllocatorPresenter();
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

// IMFClockStateSink

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::OnClockStart(MFTIME hnsSystemTime, LONGLONG llClockStartOffset)
{
    m_nRenderState = Started;

    TRACE(L"EVR: OnClockStart() hnsSystemTime = %I64d, llClockStartOffset = %I64d\n", hnsSystemTime, llClockStartOffset);
    m_dModeratedTimeLast = -1.0;
    m_dModeratedClockLast = -1.0;
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::OnClockStop(MFTIME hnsSystemTime)
{
    TRACE(L"EVR: OnClockStop() hnsSystemTime = %I64d\n", hnsSystemTime);
    m_nRenderState = Stopped;

    m_dModeratedClockLast = -1.0;
    m_dModeratedTimeLast = -1.0;
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::OnClockPause(MFTIME hnsSystemTime)
{
    TRACE(L"EVR: OnClockPause() hnsSystemTime = %I64d\n", hnsSystemTime);
    if (!m_bSignaledStarvation) {
        m_nRenderState = Paused;
    }
    m_dModeratedTimeLast = -1.0;
    m_dModeratedClockLast = -1.0;
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::OnClockRestart(MFTIME hnsSystemTime)
{
    m_nRenderState = Started;

    m_dModeratedTimeLast = -1.0;
    m_dModeratedClockLast = -1.0;
    TRACE(L"EVR: OnClockRestart() hnsSystemTime = %I64d\n", hnsSystemTime);
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::OnClockSetRate(MFTIME hnsSystemTime, float flRate)
{
    ASSERT(0);
    return E_NOTIMPL;
}

// IQualProp

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::get_FramesDroppedInRenderer(THIS_ __out int* pcFrames)
{
    ASSERT(pcFrames);

    *pcFrames = m_pcFramesDropped;
    return S_OK;
}
__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::get_FramesDrawn(THIS_ __out int* pcFramesDrawn)
{
    ASSERT(pcFramesDrawn);

    *pcFramesDrawn = m_pcFramesDrawn;
    return S_OK;
}
__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::get_AvgFrameRate(THIS_ __out int* piAvgFrameRate)
{
    ASSERT(piAvgFrameRate);

    *piAvgFrameRate = static_cast<int>(m_dAverageFrameRate * 100.0 + 0.5);
    return S_OK;
}
__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::get_Jitter(THIS_ __out int* iJitter)
{
    ASSERT(iJitter);

    *iJitter = static_cast<int>(m_dJitterStdDev * 1000.0 + 0.5);
    return S_OK;
}
__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::get_AvgSyncOffset(THIS_ __out int* piAvg)
{
    ASSERT(piAvg);

    *piAvg = static_cast<int>(m_dSyncOffsetAvr * 1000.0 + 0.5);
    return S_OK;
}
__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::get_DevSyncOffset(THIS_ __out int* piDev)
{
    ASSERT(piDev);

    *piDev = static_cast<int>(m_dSyncOffsetStdDev * 1000.0 + 0.5);
    return S_OK;
}

// IMFRateSupport

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::GetSlowestRate(MFRATE_DIRECTION eDirection, BOOL fThin, __RPC__out float* pflRate)
{
    ASSERT(pflRate);

    // TODO: not finished
    *pflRate = 0.0f;
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::GetFastestRate(MFRATE_DIRECTION eDirection, BOOL fThin, __RPC__out float* pflRate)
{
    ASSERT(pflRate);

    HRESULT hr;
    if (FAILED(hr = CheckShutdown())) {
        return hr;
    }

    float fMaxRate = FLT_MAX;// Default
    // Find the maximum forward rate.
    if (!fThin && m_pMixerType && m_dStreamReferenceVideoFrameRate) { // non-thinned: use the frame rate and monitor refresh rate
        UINT64 u64Packed;// 2-part fraction of the video input stream frame rate
        hr = m_pMixerType->GetUINT64(MF_MT_FRAME_RATE, &u64Packed);
        // UINT32 fpsNumerator = reinterpret_cast<UINT32*>(&unPacked)[1], fpsDenominator = reinterpret_cast<UINT32*>(&unPacked)[0];
        if (SUCCEEDED(hr)) {// max Rate = refresh rate / frame rate
            fMaxRate = static_cast<float>(m_dDetectedRefreshRate * static_cast<double>(reinterpret_cast<INT32*>(&u64Packed)[0]) / static_cast<double>(reinterpret_cast<INT32*>(&u64Packed)[1]));// the standard converter only does a proper job with signed values
        }
    }

    // For reverse playback, swap the sign.
    if (eDirection == MFRATE_REVERSE) {
        fMaxRate = -fMaxRate;
    }

    *pflRate = fMaxRate;
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::IsRateSupported(BOOL fThin, float flRate, __RPC__inout_opt float* pflNearestSupportedRate)
{
    HRESULT hr;
    if (FAILED(hr = CheckShutdown())) {
        return hr;
    }
    // fRate can be negative for reverse playback.
    // pfNearestSupportedRate can be nullptr.

    float fMaxRate = FLT_MAX, fNearestRate = flRate;// Defaults
    // Find the maximum forward rate.
    if (!fThin && m_pMixerType && m_dStreamReferenceVideoFrameRate) { // non-thinned: use the frame rate and monitor refresh rate
        UINT64 u64Packed;// 2-part fraction of the video input stream frame rate
        hr = m_pMixerType->GetUINT64(MF_MT_FRAME_RATE, &u64Packed);
        // UINT32 fpsNumerator = reinterpret_cast<UINT32*>(&unPacked)[1], fpsDenominator = reinterpret_cast<UINT32*>(&unPacked)[0];
        if (SUCCEEDED(hr)) {// max Rate = refresh rate / frame rate
            fMaxRate = static_cast<float>(m_dDetectedRefreshRate * static_cast<double>(reinterpret_cast<INT32*>(&u64Packed)[0]) / static_cast<double>(reinterpret_cast<INT32*>(&u64Packed)[1]));// the standard converter only does a proper job with signed values
        }
    }

    if (fabs(flRate) > fMaxRate) {
        // The (absolute) requested rate exceeds the maximum rate.
        hr = MF_E_UNSUPPORTED_RATE;

        // The nearest supported rate is fMaxRate.
        fNearestRate = fMaxRate;
        if (flRate < 0) {
            // For reverse playback, swap the sign.
            fNearestRate = -fNearestRate;
        }
    }

    // Return the nearest supported rate if the caller requested it.
    if (pflNearestSupportedRate) {
        *pflNearestSupportedRate = fNearestRate;
    }

    return hr;
}

__declspec(nothrow noalias) __forceinline void CEVRAllocatorPresenter::CompleteFrameStep(bool bCancel)
{
    if (m_nStepCount) {
        if (bCancel || (m_nStepCount == 1)) {
            m_nStepCount = 0;
            TRACE(L"EVR: Notify external EVR for frame step complete\n");
            m_pSink->Notify(EC_STEP_COMPLETE, static_cast<LONG_PTR>(bCancel), 0);
        } else {
            --m_nStepCount;
        }
    }
}

// IMFVideoPresenter

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::ProcessMessage(MFVP_MESSAGE_TYPE eMessage, ULONG_PTR ulParam)
{
    switch (eMessage) {
        case MFVP_MESSAGE_BEGINSTREAMING :          // The EVR switched from stopped to paused. The presenter should allocate resources
            TRACE(L"EVR: MFVP_MESSAGE_BEGINSTREAMING\n");
            ResetStats();
            break;

        case MFVP_MESSAGE_CANCELSTEP :              // Cancels a frame step
            TRACE(L"EVR: MFVP_MESSAGE_CANCELSTEP\n");
            CompleteFrameStep(true);
            break;

        case MFVP_MESSAGE_ENDOFSTREAM :             // All input streams have ended.
            TRACE(L"EVR: MFVP_MESSAGE_ENDOFSTREAM\n");
            m_bPendingMediaFinished = true;
            break;

        case MFVP_MESSAGE_ENDSTREAMING :            // The EVR switched from running or paused to stopped. The presenter should free resources
            TRACE(L"EVR: MFVP_MESSAGE_ENDSTREAMING\n");
            break;

        case MFVP_MESSAGE_FLUSH :                   // The presenter should discard any pending samples
            TRACE(L"EVR: MFVP_MESSAGE_FLUSH\n");
            if (!m_hRenderThread) {// handle things here
                // Flush pending samples
                FlushSamples();
                TRACE(L"EVR: Flush done!\n");
            } else {
                SetEvent(m_hEvtFlush);
                m_bEvtFlush = true;
                while (WaitForSingleObject(m_hEvtFlush, 1) == WAIT_OBJECT_0);
            }
            break;

        case MFVP_MESSAGE_INVALIDATEMEDIATYPE :     // The mixer's output format has changed. The EVR will initiate format negotiation, as described previously
            TRACE(L"EVR: MFVP_MESSAGE_INVALIDATEMEDIATYPE\n");
            /*
                1) The EVR sets the media type on the reference stream.
                2) The EVR calls IMFVideoPresenter::ProcessMessage on the presenter with the MFVP_MESSAGE_INVALIDATEMEDIATYPE message.
                3) The presenter sets the media type on the mixer's output stream.
                4) The EVR sets the media type on the substreams.
            */
            if (!m_hRenderThread) {// handle things here
                CAutoLock cExternalMixerLock(&m_csExternalMixerLock);
                CAutoLock cRenderLock(&m_csRenderLock);
                RenegotiateMediaType();
            } else {// leave it to the other thread
                m_hEvtRenegotiate = CreateEventW(nullptr, TRUE, FALSE, nullptr);
                WaitForSingleObject(m_hEvtRenegotiate, INFINITE);
                CloseHandle(m_hEvtRenegotiate);
                m_hEvtRenegotiate = nullptr;
            }
            break;

        case MFVP_MESSAGE_PROCESSINPUTNOTIFY :      // One input stream on the mixer has received a new sample
            TRACE(L"EVR: MFVP_MESSAGE_PROCESSINPUTNOTIFY\n");
            // GetImageFromMixer();
            break;

        case MFVP_MESSAGE_STEP :                    // Requests a frame step.
            TRACE(L"EVR: MFVP_MESSAGE_STEP\n");
            m_nStepCount = ulParam;
            break;

        default :
            ASSERT(0);
    }
    return S_OK;
}

static __declspec(nothrow noalias) __int8 GetMediaTypeMerit(IMFMediaType* pMixerType)
{
    ASSERT(pMixerType);

    GUID guidSubType;
    if (SUCCEEDED(pMixerType->GetGUID(MF_MT_SUBTYPE, &guidSubType))) {
        DWORD dwSubType = guidSubType.Data1;
        // see "video media types" in mfapi.h for types the EVR can return
        return (dwSubType == FCC('AI44')) ? 32 // Palettized, 4:4:4
               : (dwSubType == FCC('YVU9')) ? 31 // 8-bit, 16:1:1
               : (dwSubType == FCC('NV11')) ? 30 // 8-bit, 4:1:1
               : (dwSubType == FCC('Y41P')) ? 29
               : (dwSubType == FCC('Y41T')) ? 28
               : (dwSubType == FCC('P016')) ? 27 // 4:2:0
               : (dwSubType == FCC('P010')) ? 26
               : (dwSubType == FCC('NV12')) ? 25
               : (dwSubType == FCC('I420')) ? 24
               : (dwSubType == FCC('IYUV')) ? 23
               : (dwSubType == FCC('YV12')) ? 22
               : (dwSubType == FCC('Y216')) ? 21 // 4:2:2
               : (dwSubType == FCC('P216')) ? 20
               : (dwSubType == FCC('v216')) ? 19
               : (dwSubType == FCC('Y210')) ? 18
               : (dwSubType == FCC('P210')) ? 17
               : (dwSubType == FCC('v210')) ? 16
               : (dwSubType == FCC('YUY2')) ? 15
               : (dwSubType == FCC('YVYU')) ? 14
               : (dwSubType == FCC('UYVY')) ? 13
               : (dwSubType == FCC('Y42T')) ? 12
               : (dwSubType == FCC('Y416')) ? 11 // 4:4:4
               : (dwSubType == FCC('Y410')) ? 10
               : (dwSubType == FCC('v410')) ? 9
               : (dwSubType == FCC('AYUV')) ? 0 // To keep track of for further developments: there's currently no GPU available with native support of AYUV surfaces, to prevent the software mixer from using a very slow converter to X8R8G8B8, AYUV is disabled for now
               : (dwSubType == D3DFMT_X8R8G8B8) ? 6 // always rank RGB types lower than the rest of the types to avoid the many problems with RGB conversions before the mixer
               : (dwSubType == D3DFMT_R8G8B8) ? 5
               : (dwSubType == D3DFMT_A8R8G8B8) ? 4
               : (dwSubType == D3DFMT_R5G6B5) ? 3
               : (dwSubType == D3DFMT_X1R5G5B5) ? 2
               : (dwSubType == D3DFMT_P8) ? 1
               : 7;
    }
    return 0;
}

__declspec(nothrow noalias) HRESULT CEVRAllocatorPresenter::RenegotiateMediaType()
{
    m_dStreamReferenceVideoFrameRate = 0.0;// this is to reset the stream reference data afterwards in GetMixerThread()
    if (!m_pMixer) {
        ASSERT(0);
        return MF_E_INVALIDREQUEST;
    }
    if (m_pMixerType) {
        m_pMixerType->Release();// no assertion on the reference count
        m_pMixerType = nullptr;
    }

    // Loop through all of the mixer's proposed output types.
    HRESULT hr;
    // Step 1. Get the first media type supported by mixer, prepare the base variables for playback
    if (FAILED(hr = m_pMixer->GetOutputAvailableType(0, 0, &m_pMixerType))) {
        ASSERT(0);
        return hr;
    }

    ULARGE_INTEGER uliVideoHeightAndWidth;
    if (FAILED(hr = m_pMixerType->GetUINT64(MF_MT_FRAME_SIZE, &uliVideoHeightAndWidth.QuadPart))) {
        ASSERT(0);
        m_pMixerType->Release();// no assertion on the reference count
        m_pMixerType = nullptr;
        return hr;
    }

    // warning: currently we assume the only incoming formats use the truncated D65 white point
    // should input formats with other white points be accepted and processed correctly by the color management section, additional code has to be added for this purpose
    // notes about handling the input white point in the initial pass pixel shaders can be found in the renderer's cpp file

    bool bPaintOnFinish = false;// used to signal that Paint() should be called after successfully negotiating a type to initialize the main renderer loop
    // surface size change check
    DWORD dwNH = uliVideoHeightAndWidth.LowPart, dwNW = uliVideoHeightAndWidth.HighPart,
          dwOW = m_u32VideoWidth, dwOH = m_u32VideoHeight;
    if ((dwNW != dwOW) || (dwNH != dwOH)) {
        {
            // SD-HD switch check
            bool bNewVideoUsingSD = (dwNW < 1120) && (dwNH < 630);
            bool bRendererUsingSD = (dwOW < 1120) && (dwOH < 630);
            if (bNewVideoUsingSD != bRendererUsingSD) {// make the renderer reset some things that depend on SD and HD matrices
                m_u8VMR9ChromaFixCurrent = 127;// to pass the check list for re-compiling the initial pass shaders (as 127 is not an option for this item)
            }

            // video size parts
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
            __m128 x2 = _mm_set_ps1(1.0f);
            __m128i xVS = _mm_loadl_epi64(reinterpret_cast<__m128i*>(&uliVideoHeightAndWidth));
            xVS = _mm_shuffle_epi32(xVS, _MM_SHUFFLE(3, 2, 0, 1));// set width to lowest, height to second lowest
            __m128d x0 = _mm_cvtepi32_pd(xVS);// __int32 to double
            _mm_storel_epi64(reinterpret_cast<__m128i*>(&m_mfvCurrentFrameArea.Area.cx), xVS);// also stores cy
            _mm_storel_epi64(reinterpret_cast<__m128i*>(&m_u32VideoWidth), xVS);// also stores m_u32VideoHeight
            __m128 x1 = _mm_cvtpd_ps(x0);// double to float
            _mm_store_pd(&m_dVideoWidth, x0);// also stores m_dVideoHeight
            x2 = _mm_div_ps(x2, x1);// reciprocal trough _mm_rcp_ps() isn't accurate
            _mm_storel_pi(reinterpret_cast<__m64*>(&m_fVideoWidth), x1);// not an MMX function, also stores m_fVideoHeight
            _mm_storel_pi(reinterpret_cast<__m64*>(&m_fVideoWidthr), x2);// not an MMX function, also stores m_fVideoHeightr
#else
            m_u32VideoWidth = uliVideoHeightAndWidth.HighPart;
            m_u32VideoHeight = uliVideoHeightAndWidth.LowPart;
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

            // initial cleanup
            DeleteSurfaces();
        }
OnlyReCreateMixerSurfaces:
        RemoveAllSamples();// cleanup of old surface tags

        // create working surfaces
        m_FreeSamplesQueue.Lock();
        AllocSurfaces();

        ptrdiff_t i = m_u8MixerSurfaceCount - 1;
        do {
            IMFSample* pMFSample;
            EXECUTE_ASSERT(S_OK == (hr = m_fnMFCreateVideoSampleFromSurface(m_apVideoSurface[i], &pMFSample)));
            EXECUTE_ASSERT(S_OK == (hr = pMFSample->SetUINT32(GUID_SURFACE_INDEX, static_cast<UINT32>(i))));
            m_FreeSamplesQueue.Enqueue(pMFSample);// reference inherited
        } while (--i >= 0);
        m_FreeSamplesQueue.Unlock();
        bPaintOnFinish = true;
    } else if (!m_apVideoSurface[0]) {// this can happen during resets
        goto OnlyReCreateMixerSurfaces;
    }

    ULARGE_INTEGER uliUtil;
    if (FAILED(hr = m_pMixerType->GetUINT32(MF_MT_INTERLACE_MODE, reinterpret_cast<UINT32*>(&uliUtil.LowPart)))) {
        ASSERT(0);
        m_pMixerType->Release();// no assertion on the reference count
        m_pMixerType = nullptr;
        return hr;
    }
    m_bInterlaced = uliUtil.LowPart != MFVideoInterlace_Progressive;

    // using MF_MT_FRAME_RATE to get the frame rate here doesn't work correctly for interlaced types

    if (FAILED(hr = m_pMixerType->GetUINT64(MF_MT_PIXEL_ASPECT_RATIO, &uliUtil.QuadPart))) {
        ASSERT(0);
        m_pMixerType->Release();// no assertion on the reference count
        m_pMixerType = nullptr;
        return hr;
    }
    UINT32 uiARx = uliVideoHeightAndWidth.HighPart * uliUtil.HighPart;
    UINT32 uiARy = uliVideoHeightAndWidth.LowPart * uliUtil.LowPart;
    if (!uiARx || !uiARy) { // if either of these is 0, it will get stuck into an infinite loop
        ASSERT(0);
        m_pMixerType->Release();// no assertion on the reference count
        m_pMixerType = nullptr;
        return MF_E_INVALIDMEDIATYPE;
    }

    // division reduction
    UINT32 a = uiARx, b = uiARy;
    do {
        UINT32 tmp = a;
        a = b % tmp;
        b = tmp;
    } while (a);
    m_u32AspectRatioWidth = uiARx / b;
    m_u32AspectRatioHeight = uiARy / b;

    GUID guidSubType;
    CInterfaceArray<IMFMediaType> paValidMixerTypes;
    DWORD dwTypeIndex = 1;
    goto FirstInput;
    for (;;) {
        // Step 1. Get the next media type supported by mixer
        if (m_pMixerType) {
            m_pMixerType->Release();// no assertion on the reference count
            m_pMixerType = nullptr;
        }
        hr = m_pMixer->GetOutputAvailableType(0, dwTypeIndex, &m_pMixerType);// will output hr = MF_E_NO_MORE_TYPES when no more types are available
        if (hr == MF_E_NO_MORE_TYPES) {
            break;// m_pMixerType will always be nullptr on breaking
        }
        ++dwTypeIndex;
        if (FAILED(hr)) {
            ASSERT(0);
            continue;
        }
FirstInput:
#ifdef _DEBUG
        if (SUCCEEDED(m_pMixerType->GetGUID(MF_MT_SUBTYPE, &guidSubType))) {
            TRACE(L"EVR: Incoming mixer input type: %s\n", GetSurfaceFormatName(static_cast<D3DFORMAT>(guidSubType.Data1)));
        }
#endif
        // Step 2. Construct a working media type
        // the presenter can't handle limited range inputs, later in the final pass section a converter can compress ranges
        EXECUTE_ASSERT(S_OK == (hr = m_pMixerType->SetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, MFNominalRange_0_255)));
        EXECUTE_ASSERT(S_OK == (hr = m_pMixerType->SetBlob(MF_MT_GEOMETRIC_APERTURE, reinterpret_cast<UINT8*>(&m_mfvCurrentFrameArea), sizeof(MFVideoArea))));
        EXECUTE_ASSERT(S_OK == (hr = m_pMixerType->SetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE, reinterpret_cast<UINT8*>(&m_mfvCurrentFrameArea), sizeof(MFVideoArea))));

        // Step 3. Check if the mixer will accept this media type
        if (FAILED(hr = m_pMixer->SetOutputType(0, m_pMixerType, MFT_SET_TYPE_TEST_ONLY))) {
            continue;// no ASSERT() here, this just happens sometimes
        }
        __int8 i8Merit = GetMediaTypeMerit(m_pMixerType);
        if (!i8Merit) {// reject types set to 0 in the list
            continue;
        }

        // construct the list
        size_t upInsertPos = 0;
        ptrdiff_t j = paValidMixerTypes.GetCount();
        while (--j >= 0) {// the first item will set -1 here
            __int8 i8ThisMerit = GetMediaTypeMerit(paValidMixerTypes[j]);
            if (i8Merit < i8ThisMerit) {
                upInsertPos = j;
                break;
            } else {
                upInsertPos = j + 1;
            }
        }
        paValidMixerTypes.InsertAt(upInsertPos, m_pMixerType);
    }

    // Step 4. Adjust the mixer's type to match our requirements
    ptrdiff_t k = paValidMixerTypes.GetCount() - 1;
    if (k < 0) {
        ASSERT(0);
        return MF_E_INVALIDMEDIATYPE;
    }
#ifdef _DEBUG
    do {
        if (SUCCEEDED(paValidMixerTypes[k]->GetGUID(MF_MT_SUBTYPE, &guidSubType))) {
            TRACE(L"EVR: Valid mixer input type: %s\n", GetSurfaceFormatName(static_cast<D3DFORMAT>(guidSubType.Data1)));
        }
    } while (--k >= 0);
    k = paValidMixerTypes.GetCount() - 1;
#endif
    do {
        m_pMixerType = paValidMixerTypes[k];
#ifdef _DEBUG
        if (SUCCEEDED(m_pMixerType->GetGUID(MF_MT_SUBTYPE, &guidSubType))) {
            TRACE(L"EVR: Trying mixer input type: %s\n", GetSurfaceFormatName(static_cast<D3DFORMAT>(guidSubType.Data1)));
        }
#endif
        // Step 5. Set output media type on mixer
        if (FAILED(hr = m_pMixer->SetOutputType(0, m_pMixerType, 0))) {
            m_pMixerType = nullptr;
            continue;// no ASSERT() here, this just happens sometimes
        }

        // Step 6. Use the media type on ourselves
        m_pMixerType->AddRef();
        m_pMixerType->GetGUID(MF_MT_SUBTYPE, &guidSubType);// already used previously, no ASSERT() here
        m_u8ChromaType = GetChromaType(guidSubType.Data1);// the high bit is set later on, if required
        m_szChromaCositing[0] = '0';// m_szChromaCositing and m_u8ChromaType should always be synchronized

        m_strMixerStatus = L"Mixer format : Input ";
        m_strMixerStatus += GetSurfaceFormatName(guidSubType.Data1);
        m_strMixerStatus += L", Output ";// note: this line is completed at the stats streen constructor

        if (bPaintOnFinish) {// initialize the main renderer loop
            m_u8CurrentMixerSurface = 0;// just use surface 0 to initialize the renderer, rendering a black screen
            m_pD3DDev->ColorFill(m_apVideoSurface[0], nullptr, 0);
            Paint(FRENDERPAINT_INIT);

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
        }
        hr = S_OK;
        goto exit;
    } while (--k >= 0);
    // for when the previous loop only resulted in failed initializations
    m_pMixer->SetOutputType(0, nullptr, 0);
    ASSERT(0);
exit:
    if (m_hEvtReset) {// when in a complete reset, keep MainThread on hold until the mixer is alive again
        SetEvent(m_hEvtReset);
    }
    return hr;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::GetCurrentMediaType(__deref_out IMFVideoMediaType** ppMediaType)
{
    ASSERT(ppMediaType);

    if (!m_pMixerType) {
        return MF_E_NOT_INITIALIZED;
    }
    HRESULT hr;
    if (FAILED(hr = CheckShutdown())) {
        return hr;
    }

    return m_pMixerType->QueryInterface(IID_IMFVideoMediaType, reinterpret_cast<void**>(ppMediaType));
}

// IMFTopologyServiceLookupClient

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::InitServicePointers(__in IMFTopologyServiceLookup* pLookup)
{
    ASSERT(pLookup);

    TRACE(L"EVR: InitServicePointers()\n");
    ASSERT(m_nRenderState == Shutdown);
    HRESULT hr;
    DWORD dwObjects = 1;

    ASSERT(!m_pMixer);
    hr = pLookup->LookupService(MF_SERVICE_LOOKUP_GLOBAL, 0, MR_VIDEO_MIXER_SERVICE, IID_IMFTransform, reinterpret_cast<void**>(&m_pMixer), &dwObjects);
    if (FAILED(hr)) {
        ASSERT(0);
        return hr;
    }

    ASSERT(!m_pSink);
    hr = pLookup->LookupService(MF_SERVICE_LOOKUP_GLOBAL, 0, MR_VIDEO_RENDER_SERVICE, IID_IMediaEventSink, reinterpret_cast<void**>(&m_pSink), &dwObjects);
    if (FAILED(hr)) {
        ASSERT(0);
        m_pMixer->Release();
        m_pMixer = nullptr;
        return hr;
    }

    m_nRenderState = Stopped;// this is important for both when the next call succeeds and fails

    ASSERT(!m_pClock);
    hr = pLookup->LookupService(MF_SERVICE_LOOKUP_GLOBAL, 0, MR_VIDEO_RENDER_SERVICE, IID_IMFClock, reinterpret_cast<void**>(&m_pClock), &dwObjects);
    if (FAILED(hr)) {// IMFClock can't be guaranteed to exist during first initialization. After negotiating the media type, it should initialize okay.
        return S_OK;
    }

    ASSERT(!m_hEvtQuit);
    m_hEvtQuit = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    ASSERT(m_hEvtQuit);
    ASSERT(!m_hEvtFlush);
    m_hEvtFlush = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    ASSERT(m_hEvtFlush);

    ASSERT(!m_hRenderThread);
    m_hRenderThread = ::CreateThread(nullptr, 0x20000, PresentThread, this, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
    ASSERT(m_hRenderThread);
    EXECUTE_ASSERT(SetThreadPriority(m_hRenderThread, THREAD_PRIORITY_TIME_CRITICAL));

    ASSERT(!m_hMixerThread);
    m_hMixerThread = ::CreateThread(nullptr, 0x10000, GetMixerThreadStatic, this, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
    ASSERT(m_hMixerThread);
    EXECUTE_ASSERT(SetThreadPriority(m_hMixerThread, THREAD_PRIORITY_HIGHEST));

    TRACE(L"EVR: Worker threads started\n");
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::ReleaseServicePointers()
{
    TRACE(L"EVR: ReleaseServicePointers()\n");
    if (m_nRenderState != Shutdown) {
        if (m_pClock) {// if m_pClock is active, everything has been initialized from the above InitServicePointers()
            m_bEvtFlush = m_bEvtQuit = true;
            SetEvent(m_hEvtQuit);
            SetEvent(m_hEvtFlush);

            if (WaitForSingleObject(m_hRenderThread, 10000) == WAIT_TIMEOUT) {
                ASSERT(0);
                TerminateThread(m_hRenderThread, 0xDEAD);
            }
            CloseHandle(m_hRenderThread);
            m_hRenderThread = nullptr;

            if (WaitForSingleObject(m_hMixerThread, 10000) == WAIT_TIMEOUT) {
                ASSERT(0);
                TerminateThread(m_hMixerThread, 0xDEAD);
            }
            CloseHandle(m_hMixerThread);
            m_hMixerThread = nullptr;

            CloseHandle(m_hEvtFlush);
            m_hEvtFlush = nullptr;
            CloseHandle(m_hEvtQuit);
            m_hEvtQuit = nullptr;

            m_bEvtQuit = m_bEvtFlush = false;
            TRACE(L"EVR: Worker threads stopped\n");

            // the external EVR will keep hold of these objects at first, so no assertions on the reference counts here
            m_pClock->Release();
            m_pClock = nullptr;
        }

        // these two are guaranteed to be valid if m_nRenderState is not Shutdown
        m_pMixer->Release();
        m_pMixer = nullptr;
        m_pSink->Release();
        m_pSink = nullptr;

        m_nRenderState = Shutdown;
    }
    return S_OK;
}

// IMFVideoDeviceID

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::GetDeviceID(__out IID* pDeviceID)
{
    ASSERT(pDeviceID);

    *pDeviceID = IID_IDirect3DDevice9;
    return S_OK;
}

// IMFGetService

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::GetService(__RPC__in REFGUID guidService, __RPC__in REFIID riid, __RPC__deref_out_opt LPVOID* ppvObject)
{
    ASSERT(ppvObject);

    // not the most intelligent type of querying used here (guidService unused), but that's quite okay
    return QueryInterface(riid, ppvObject);
}

// IMFAsyncCallback

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::GetParameters(__RPC__out DWORD* pdwFlags, __RPC__out DWORD* pdwQueue)
{
    return E_NOTIMPL;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::Invoke(__RPC__in_opt IMFAsyncResult* pAsyncResult)
{
    return E_NOTIMPL;
}

// IMFVideoDisplayControl

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::GetNativeVideoSize(__RPC__inout_opt SIZE* pszVideo, __RPC__inout_opt SIZE* pszARVideo)
{
    ASSERT(pszVideo || pszARVideo);

    if (pszVideo) {
        pszVideo->cx = m_u32VideoWidth;
        pszVideo->cy = m_u32VideoHeight;
    }
    if (pszARVideo) {
        pszARVideo->cx = m_u32AspectRatioWidth;
        pszARVideo->cy = m_u32AspectRatioHeight;
    }
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::GetIdealVideoSize(__RPC__inout_opt SIZE* pszMin, __RPC__inout_opt SIZE* pszMax)
{
    ASSERT(pszMin || pszMax);

    if (pszMin) {
        (*pszMin).cx = (*pszMin).cy = 1;
    }
    if (pszMax) {
        (*pszMax).cx = m_dpPParam.BackBufferWidth;
        (*pszMax).cy = m_dpPParam.BackBufferHeight;
    }
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::SetVideoPosition(__RPC__in_opt MFVideoNormalizedRect const* pnrcSource, __RPC__in_opt LPRECT const prcDest)
{
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::GetVideoPosition(__RPC__out MFVideoNormalizedRect* pnrcSource, __RPC__out LPRECT prcDest)
{
    ASSERT(pnrcSource || prcDest);

    // Always all source rectangle ?
    if (pnrcSource) {
        pnrcSource->left    = 0.0f;
        pnrcSource->top     = 0.0f;
        pnrcSource->right   = 1.0f;
        pnrcSource->bottom  = 1.0f;
    }

    if (prcDest) {
        *prcDest = m_VideoRect;
    }
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::SetAspectRatioMode(DWORD dwAspectRatioMode)
{
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::GetAspectRatioMode(__RPC__out DWORD* pdwAspectRatioMode)
{
    ASSERT(pdwAspectRatioMode);

    *pdwAspectRatioMode = MFVideoARMode_PreservePicture;
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::SetVideoWindow(__RPC__in HWND hwndVideo)
{
    ASSERT(m_hVideoWnd == hwndVideo);// What to do if it is not the same?
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::GetVideoWindow(__RPC__deref_out_opt HWND* phwndVideo)
{
    ASSERT(phwndVideo);

    *phwndVideo = m_hVideoWnd;
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::RepaintVideo()
{
    ASSERT(0);
    return E_NOTIMPL;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::GetCurrentImage(__RPC__inout BITMAPINFOHEADER* pBih, __RPC__deref_out_ecount_full_opt(*pcbDib) BYTE** pDib, __RPC__out DWORD* pcbDib, __RPC__inout_opt LONGLONG* pTimeStamp)
{
    ASSERT(0);
    return E_NOTIMPL;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::SetBorderColor(COLORREF Clr)
{
    m_BorderColor = Clr;
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::GetBorderColor(__RPC__out COLORREF* pClr)
{
    ASSERT(pClr);

    *pClr = m_BorderColor;
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::SetRenderingPrefs(DWORD dwRenderFlags)
{
    m_dwVideoRenderPrefs = static_cast<MFVideoRenderPrefs>(dwRenderFlags);
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::GetRenderingPrefs(__RPC__out DWORD* pdwRenderFlags)
{
    ASSERT(pdwRenderFlags);

    *pdwRenderFlags = m_dwVideoRenderPrefs;
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::SetFullscreen(BOOL fFullscreen)
{
    ASSERT(0);
    return E_NOTIMPL;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::GetFullscreen(__RPC__out BOOL* pfFullscreen)
{
    ASSERT(0);
    return E_NOTIMPL;
}

// IEVRTrustedVideoPlugin

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::IsInTrustedVideoMode(BOOL* pYes)
{
    ASSERT(pYes);

    *pYes = TRUE;
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::CanConstrict(BOOL* pYes)
{
    ASSERT(pYes);

    *pYes = TRUE;
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::SetConstriction(DWORD dwKPix)
{
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::DisableImageExport(BOOL bDisable)
{
    return S_OK;
}

// IDirect3DDeviceManager9

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::ResetDevice(__in IDirect3DDevice9* pDevice, __in UINT resetToken)
{
    HRESULT hr = m_pD3DManager->ResetDevice(pDevice, resetToken);
    ASSERT(SUCCEEDED(hr));
    return hr;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::OpenDeviceHandle(__out HANDLE* phDevice)
{
    HRESULT hr = m_pD3DManager->OpenDeviceHandle(phDevice);
    ASSERT(SUCCEEDED(hr));
    return hr;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::CloseDeviceHandle(__in HANDLE hDevice)
{
    HRESULT hr = m_pD3DManager->CloseDeviceHandle(hDevice);
    ASSERT(SUCCEEDED(hr));
    return hr;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::TestDevice(__in HANDLE hDevice)
{
    HRESULT hr = m_pD3DManager->TestDevice(hDevice);
    ASSERT(SUCCEEDED(hr));
    return hr;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::LockDevice(__in HANDLE hDevice, __deref_out IDirect3DDevice9** ppDevice, __in BOOL fBlock)
{
    HRESULT hr = m_pD3DManager->LockDevice(hDevice, ppDevice, fBlock);
    ASSERT(SUCCEEDED(hr));
    return hr;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::UnlockDevice(__in HANDLE hDevice, __in BOOL fSaveState)
{
    HRESULT hr = m_pD3DManager->UnlockDevice(hDevice, fSaveState);
    ASSERT(SUCCEEDED(hr));
    return hr;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::GetVideoService(__in HANDLE hDevice, __in REFIID riid, __deref_out void** ppService)
{
    HRESULT hr = m_pD3DManager->GetVideoService(hDevice, riid, ppService);
    // ASSERT(SUCCEEDED(hr)); this case can occur with a normal full device reset
    return hr;
}
/*
uintptr_t g_pHandleDeviceManager9n = 251;
// we currently issue number 251 and onward the base number identifier for the handles

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::ResetDevice(__in IDirect3DDevice9* pDevice, __in UINT resetToken)
{
    ASSERT(0);// as IDirect3DDeviceManager9 functionality has been made native of CEVRAllocatorPresenter, this function may never be called
    return E_INVALIDARG;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::OpenDeviceHandle(__out HANDLE* phDevice)
{
    ASSERT(phDevice);

    *phDevice = reinterpret_cast<HANDLE>(g_pHandleDeviceManager9n);
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::CloseDeviceHandle(__in HANDLE hDevice)
{
    if ((hDevice < reinterpret_cast<HANDLE>(251)) || hDevice > reinterpret_cast<HANDLE>(g_pHandleDeviceManager9n)) {
        ASSERT(0);
        return E_HANDLE;
    }
    //if (hDevice != reinterpret_cast<HANDLE>(g_pHandleDeviceManager9n)) {
    // ASSERT(0); this case can occur with a normal full device reset
    //}
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::TestDevice(__in HANDLE hDevice)
{
    if ((hDevice < reinterpret_cast<HANDLE>(251)) || hDevice > reinterpret_cast<HANDLE>(g_pHandleDeviceManager9n)) {
        ASSERT(0);
        return E_HANDLE;
    }
    if (hDevice != reinterpret_cast<HANDLE>(g_pHandleDeviceManager9n)) {
        return DXVA2_E_NEW_VIDEO_DEVICE;
    }
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::LockDevice(__in HANDLE hDevice, __deref_out IDirect3DDevice9** ppDevice, __in BOOL fBlock)
{
    ASSERT(ppDevice);

    if ((hDevice < reinterpret_cast<HANDLE>(251)) || hDevice > reinterpret_cast<HANDLE>(g_pHandleDeviceManager9n)) {
        ASSERT(0);
        return E_HANDLE;
    }
    if (hDevice != reinterpret_cast<HANDLE>(g_pHandleDeviceManager9n)) {
        return DXVA2_E_NEW_VIDEO_DEVICE;
    }

    // lock both the renderer and the mixer
    if (fBlock) {
        m_csExternalMixerLock.Lock();
        m_csRenderLock.Lock();
    } else {
        // TryEnterCriticalSection wrapper isn't available as a member of CCritSec
        BOOL b = TryEnterCriticalSection(reinterpret_cast<LPCRITICAL_SECTION>(&m_csExternalMixerLock));
        if (!b) { return DXVA2_E_VIDEO_DEVICE_LOCKED; }
        b = TryEnterCriticalSection(reinterpret_cast<LPCRITICAL_SECTION>(&m_csRenderLock));
        if (!b) {
            m_csExternalMixerLock.Unlock();
            return DXVA2_E_VIDEO_DEVICE_LOCKED;
        }
    }
    *ppDevice = m_pD3DDev;
    m_pD3DDev->AddRef();
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::UnlockDevice(__in HANDLE hDevice, __in BOOL fSaveState)
{
    ASSERT(!fSaveState);// TODO: fSaveState currently ignored, and we should test if the lock was placed at all
    if ((hDevice < reinterpret_cast<HANDLE>(251)) || hDevice > reinterpret_cast<HANDLE>(g_pHandleDeviceManager9n)) {
        ASSERT(0);
        return E_HANDLE;
    }
    if (hDevice != reinterpret_cast<HANDLE>(g_pHandleDeviceManager9n)) {
        return DXVA2_E_NEW_VIDEO_DEVICE;
    }
    m_csExternalMixerLock.Unlock();
    m_csRenderLock.Unlock();
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CEVRAllocatorPresenter::GetVideoService(__in HANDLE hDevice, __in REFIID riid, __deref_out void** ppService)
{
    ASSERT(ppService);

    if ((hDevice < reinterpret_cast<HANDLE>(251)) || hDevice > reinterpret_cast<HANDLE>(g_pHandleDeviceManager9n)) {
        ASSERT(0);
        return E_HANDLE;
    }
    if (hDevice != reinterpret_cast<HANDLE>(g_pHandleDeviceManager9n)) {
        return DXVA2_E_NEW_VIDEO_DEVICE;
    }

    CAutoLock Lock(&m_csRenderLock);// to avoid problems during resets
    HRESULT hr;
    if (riid == IID_IDirectXVideoDecoderService) {
        if (!m_pVideoDecoderService) {// not an obligated object
            hr = DXVA2_E_NOT_AVAILABLE;
        } else {
            *ppService = m_pVideoDecoderService;
            m_pVideoDecoderService->AddRef();
            hr = S_OK;
        }
    } else if (riid == IID_IDirectXVideoProcessorService) {
        *ppService = m_pVideoProcessorService;
        m_pVideoProcessorService->AddRef();
        hr = S_OK;
    } else {
        ASSERT(0);
        hr = E_NOINTERFACE;
    }
    return hr;
}
*/

// static functions for multi-threading

__declspec(nothrow noalias) DWORD WINAPI CEVRAllocatorPresenter::GetMixerThreadStatic(LPVOID lpParam)
{
    ASSERT(lpParam);

    DEBUG_ONLY(SetThreadName(0xFFFFFFFF, "CEVRPresenter::MixerThread"));
    reinterpret_cast<CEVRAllocatorPresenter*>(lpParam)->GetMixerThread();
    return 0;
}

__declspec(nothrow noalias) DWORD WINAPI CEVRAllocatorPresenter::PresentThread(LPVOID lpParam)
{
    ASSERT(lpParam);

    DEBUG_ONLY(SetThreadName(0xFFFFFFFF, "CEVRPresenter::PresentThread"));
    reinterpret_cast<CEVRAllocatorPresenter*>(lpParam)->RenderThread();
    return 0;
}

__declspec(nothrow noalias) __forceinline void CEVRAllocatorPresenter::CheckWaitingSampleFromMixer()
{
    // if (m_bWaitingSample) {
    m_bWaitingSample = false;
    // GetImageFromMixer(); Do this in mixer thread instead
    // }
}

__declspec(nothrow noalias) __forceinline void CEVRAllocatorPresenter::GetMixerThread()
{
    // Tell Vista Multimedia Class Scheduler we are doing threaded playback (increase priority)
    HANDLE hAvrt = nullptr;
    DWORD dwTaskIndex = 0;
    if (m_hAVRTLib) {
        hAvrt = m_fnAvSetMmThreadCharacteristicsW(L"Playback", &dwTaskIndex);
        // if (hAvrt) m_fnAvSetMmThreadPriority(hAvrt, AVRT_PRIORITY_NORMAL); the normal setting is default
    }

    for (;;) {
        DWORD dwObject = WaitForSingleObject(m_hEvtQuit, 1);
        switch (dwObject) {
            case WAIT_OBJECT_0 :
                goto exit;
            case WAIT_TIMEOUT : {
                if (m_nRenderState == Stopped) {
                    continue;// nothing sensible to do here
                }

                bool bDoneSomething = false;
                {
                    CAutoLock Lock(&m_csExternalMixerLock);
                    MFT_OUTPUT_DATA_BUFFER Buffer;
                    Buffer.dwStreamID = 0;
                    for (;;) {
                        // get a free sample
                        m_FreeSamplesQueue.Lock();
                        if (!m_FreeSamplesQueue.m_u8QueueElemCount) {
                            m_bWaitingSample = true;
                            m_FreeSamplesQueue.Unlock();
                            break;
                        }

                        Buffer.pSample = m_FreeSamplesQueue.Dequeue();// reference inherited
                        ASSERT(Buffer.pSample);
                        ++m_u8MixerSurfacesUsed;
                        m_FreeSamplesQueue.Unlock();

                        LARGE_INTEGER liLatencyPerfCnt;
                        EXECUTE_ASSERT(QueryPerformanceCounter(&liLatencyPerfCnt));
                        DWORD dwStatus;
                        HRESULT hr = m_pMixer->ProcessOutput(0, 1, &Buffer, &dwStatus);
                        if (FAILED(hr)) {
                            // ASSERT(hr == MF_E_TRANSFORM_NEED_MORE_INPUT); this should be the only status that ProcessOutput() returns, but during resets it will fail a few times
                            MoveToFreeList(Buffer.pSample, false);// reference inherited
                            break;
                        }
                        EXECUTE_ASSERT(QueryPerformanceCounter(&m_liLastPerfCnt));
                        liLatencyPerfCnt.QuadPart = static_cast<__int64>(static_cast<double>(m_liLastPerfCnt.QuadPart - liLatencyPerfCnt.QuadPart) * m_dPerfFreqr * 10000000.0 + 0.5);
                        // TRACE(L"EVR: Notify external EVR for processing latency\n");
                        EXECUTE_ASSERT(S_OK == (hr = m_pSink->Notify(EC_PROCESSING_LATENCY, reinterpret_cast<LONG_PTR>(&liLatencyPerfCnt), 0)));
#ifdef _DEBUG
                        UINT32 ui32Surface;
                        Buffer.pSample->GetUINT32(GUID_SURFACE_INDEX, &ui32Surface);
                        LONGLONG llSampleTime100ns;
                        Buffer.pSample->GetSampleTime(&llSampleTime100ns);
                        TRACE(L"EVR: Mixer output on surface %u, %I64d * 100 ns\n", ui32Surface, llSampleTime100ns);
#endif
                        MoveToScheduledList(Buffer.pSample, false);// reference inherited
                        bDoneSomething = true;
                    }
                }

                if (bDoneSomething) {
                    if (!m_dStreamReferenceVideoFrameRate) {
                        // Use the connection media type instead of EVR's native media format interfaces to get the frame rate, as this method is somewhat more reliable.
                        IPin* pPin;
                        if (SUCCEEDED(m_OuterEVR.m_pBaseFilter->FindPin(L"EVR Input0", &pPin))) {
                            AM_MEDIA_TYPE mt;
                            if (SUCCEEDED(pPin->ConnectionMediaType(&mt))) {
                                if ((mt.formattype == FORMAT_VideoInfo) || (mt.formattype == FORMAT_MPEGVideo)) {
                                    VIDEOINFOHEADER* fV = reinterpret_cast<VIDEOINFOHEADER*>(mt.pbFormat);
                                    unsigned __int8 u8ChromaType = m_u8ChromaType;
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
                                    unsigned __int8 u8ChromaType = m_u8ChromaType;
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
                                // } else if (mt.formattype == FORMAT_MFVideoFormat) { requires MediaFoundation functions
                                // MFVIDEOFORMAT* fV = reinterpret_cast<MFVIDEOFORMAT*>(mt.pbFormat);
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
                }
            }
            break;
            default:
                ASSERT(0);
        }
    }

exit:
    if (hAvrt) {
        EXECUTE_ASSERT(m_fnAvRevertMmThreadCharacteristics(hAvrt));
    }
}

__declspec(nothrow noalias) double CEVRAllocatorPresenter::GetClockTime(double dPerformanceCounter)
{
    LONGLONG llCorrelatedTime;
    HRESULT hr = m_pClock->GetCorrelatedTime(0, &llCorrelatedTime, &m_liLastPerfCnt.QuadPart);// m_liLastPerfCnt is ignored here, the pre-converted output in 100 ns units isn't ideal to use here
    ASSERT((hr == S_OK) || (hr == S_FALSE));// it can be either, there is no documentation on why S_FALSE could be returned
    EXECUTE_ASSERT(QueryPerformanceCounter(&m_liLastPerfCnt));// guarantee the usage of the system clock, this is also accuately relatable to m_i64PerfCntInit where dPerformanceCounter derives from
    double dClockTime = static_cast<double>(llCorrelatedTime);
    DWORD Characteristics = 0;
    EXECUTE_ASSERT(S_OK == (hr = m_pClock->GetClockCharacteristics(&Characteristics)));

    if (Characteristics & MFCLOCK_CHARACTERISTICS_FLAG_FREQUENCY_10MHZ) {
        dClockTime *= 0.0000001;// 10 MHz to Hz
    } else {
        MFCLOCK_PROPERTIES Props;
        EXECUTE_ASSERT(S_OK == (hr = m_pClock->GetProperties(&Props)));
        dClockTime /= static_cast<double>(static_cast<__int64>(Props.qwClockFrequency));// scale to Hz, the standard converter only does a proper job with signed values
    }

    double dPerf = static_cast<double>(m_liLastPerfCnt.QuadPart - m_i64PerfCntInit) * m_dPerfFreqr;
    double dTarget = dPerformanceCounter - dPerf;

    // don't use the moderation methods for asynchronous schedulers
    if (((m_dfSurfaceType == D3DFMT_X8R8G8B8) || (m_dcCaps.PixelShaderVersion < D3DPS_VERSION(3, 0)) || !mk_pRendererSettings->iVMR9FrameInterpolation)// frame interpolation is allowed for all versions
            && (!mk_pRendererSettings->iEVRAlternativeScheduler || (!m_boCompositionEnabled && (m_dpPParam.Windowed || (m_u8OSVersionMajor < 6))))) {// Vista and newer only
        dTarget *= m_dModeratedTimeSpeed;

        MFCLOCK_STATE State;
        EXECUTE_ASSERT(S_OK == (hr = m_pClock->GetState(0, &State)));

        // test for reset conditions
        if (m_dModeratedTimeLast < 0.0 || State != m_mfcLastClockState || m_dModeratedClockLast < 0.0) {
            m_mfcLastClockState = State;
            m_dModeratedTimeSpeed = 1.0;
            m_dModeratedTimeSpeedPrim = 0.0;
            m_upClockTimeChangeHistoryPos = 0;
            // zero-initialize two arrays
            __m128 xZero = _mm_setzero_ps();// only a command to set a register to zero, this should not add constant value to the assembly
            static_assert(!(offsetof(CEVRAllocatorPresenter, m_adTimeChangeHistory) & 15), "structure alignment test failed, edit this class to correct the issue");
            static_assert(!((offsetof(CEVRAllocatorPresenter, m_adClockChangeHistory) + sizeof(m_adClockChangeHistory) - offsetof(CEVRAllocatorPresenter, m_adTimeChangeHistory)) & 15), "modulo 16 byte count for routine data set test failed, edit this class to correct the issue");
            unsigned __int32 u32Erase = static_cast<unsigned __int32>((offsetof(CEVRAllocatorPresenter, m_adClockChangeHistory) + sizeof(m_adClockChangeHistory) - offsetof(CEVRAllocatorPresenter, m_adTimeChangeHistory)) >> 4);
            float* pDst = reinterpret_cast<float*>(m_adTimeChangeHistory);
            do {
                _mm_stream_ps(pDst, xZero);// zero-fills target
                pDst += 4;
            } while (--u32Erase); // 16 aligned bytes are written every time
        }

        m_dModeratedClockLast = dClockTime;
        if (dPerformanceCounter != m_dModeratedTimeLast) {
            m_dModeratedTimeLast = dPerformanceCounter;

            if (m_upClockTimeChangeHistoryPos > 50) {
                size_t upLastPos = (m_upClockTimeChangeHistoryPos - 1) & 127; // (signed in unsigned) modulo action by low bitmask
                double dTimeChange = m_dModeratedTimeLast - m_adTimeChangeHistory[upLastPos];

                double dClockSpeedTarget = dTimeChange ? (m_dModeratedClockLast - m_adClockChangeHistory[upLastPos]) / dTimeChange : 1.0;
                double dChangeSpeedPP, dChangeSpeedPPMR;
                if (dClockSpeedTarget > m_dModeratedTimeSpeed) {
                    if (dClockSpeedTarget / m_dModeratedTimeSpeed > 0.1) {
                        dChangeSpeedPP = -0.1 + 1.0;
                        dChangeSpeedPPMR = (0.1 * 0.1) * -0.25;
                    } else {
                        dChangeSpeedPP = -0.01 + 1.0;
                        dChangeSpeedPPMR = (0.01 * 0.01) * -0.25;
                    }
                } else {
                    if (m_dModeratedTimeSpeed / dClockSpeedTarget > 0.1) {
                        dChangeSpeedPP = -0.1 + 1.0;
                        dChangeSpeedPPMR = (0.1 * 0.1) * -0.25;
                    } else {
                        dChangeSpeedPP = -0.01 + 1.0;
                        dChangeSpeedPPMR = (0.01 * 0.01) * -0.25;
                    }
                }
                // moderate the values
                m_dModeratedTimeSpeedPrim = dChangeSpeedPP * m_dModeratedTimeSpeedPrim + (m_dModeratedTimeSpeed - dClockSpeedTarget) * dChangeSpeedPPMR;
                m_dModeratedTimeSpeed += m_dModeratedTimeSpeedPrim;
            }
            size_t Pos = m_upClockTimeChangeHistoryPos & 127; // modulo action by low bitmask
            ++m_upClockTimeChangeHistoryPos;// this only increments, so it will wrap around, on a 32-bit integer it's once a year with 120 fps video
            m_adTimeChangeHistory[Pos] = m_dModeratedTimeLast;
            m_adClockChangeHistory[Pos] = m_dModeratedClockLast;
        }
    }
    return dTarget + dClockTime;
}

__declspec(nothrow noalias) void CEVRAllocatorPresenter::ResetDevice()
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
        // Reset DXVA Manager, and get new buffers
        //++g_pHandleDeviceManager9n;// invalidate handles
        HRESULT hr;
        if (FAILED(hr = m_pD3DManager->ResetDevice(reinterpret_cast<IDirect3DDevice9* volatile>(m_pD3DDev), m_nResetToken))) {// volatile, to prevent the compiler from re-using the old pointer
            ErrBox(hr, L"IDirect3DDeviceManager9::ResetDevice() failed");
        }
        TRACE(L"EVR: Notify external EVR for reset\n");
        EXECUTE_ASSERT(S_OK == (hr = m_pSink->Notify(EC_DISPLAY_CHANGED, 0, 0)));

        m_csRenderLock.Unlock();
        WaitForSingleObject(m_hEvtReset, INFINITE);
        EXECUTE_ASSERT(CloseHandle(m_hEvtReset));
        m_hEvtReset = nullptr;

        // delayed release of the old device for the EVR mixer
        ULONG u = pDev->Release();
        ASSERT(!u);
    } else {
        m_bPartialExDeviceReset = false;
        m_csRenderLock.Unlock();
    }
}

__declspec(nothrow noalias) __forceinline void CEVRAllocatorPresenter::RenderThread()
{
    // Tell Vista Multimedia Class Scheduler we are doing threaded playback (increase priority)
    HANDLE hAvrt = nullptr;
    DWORD dwTaskIndex = 0;
    if (m_hAVRTLib) {
        hAvrt = m_fnAvSetMmThreadCharacteristicsW(L"Playback", &dwTaskIndex);
        if (hAvrt) {
            m_fnAvSetMmThreadPriority(hAvrt, AVRT_PRIORITY_HIGH);
        }
    }

    int NextSleepTime = 1;
    for (;;) {
        DWORD dwObject = WaitForMultipleObjects(2, &m_hEvtQuit, FALSE, (NextSleepTime == MININT) ? 0 : (NextSleepTime <= 1) ? 1 : NextSleepTime);
        /*      dwObject = WAIT_TIMEOUT;
                if (m_bEvtFlush)
                    dwObject = WAIT_OBJECT_0 + 1;
                else if (m_bEvtQuit)
                    dwObject = WAIT_OBJECT_0;*/
        //      if (NextSleepTime)
        //          TRACE(L"EVR: Sleep: %7.3f\n", ~);

        if (m_hEvtRenegotiate) {
            CAutoLock Lock(&m_csExternalMixerLock);
            CAutoLock cRenderLock(&m_csRenderLock);
            RenegotiateMediaType();
            SetEvent(m_hEvtRenegotiate);
        }

        if (NextSleepTime > 1) {
            NextSleepTime = 0;
        } else if (!NextSleepTime) {
            NextSleepTime = -1;
        }
        switch (dwObject) {
            case WAIT_OBJECT_0 :
                goto exit;
            case WAIT_OBJECT_0 + 1 :
                // Flush pending samples
                FlushSamples();
                m_bEvtFlush = false;
                ResetEvent(m_hEvtFlush);
                TRACE(L"EVR: Flush done!\n");
                break;

                // Discard timer events if playback stop
                // if ((dwObject == WAIT_OBJECT_0 + 3) && (m_nRenderState != Started)) continue;
                // TRACE ("EVR: RenderThread ==>> Waiting buffer\n");
                // if (WaitForMultipleObjects (_countof(hEvtsBuff), hEvtsBuff, FALSE, INFINITE) == WAIT_OBJECT_0+2)

            case WAIT_TIMEOUT : {
                EXECUTE_ASSERT(QueryPerformanceCounter(&m_liLastPerfCnt));
                double dCurrentCounter = static_cast<double>(m_liLastPerfCnt.QuadPart - m_i64PerfCntInit) * m_dPerfFreqr;

                // take care of the paused mode, DVD menus and such, m_dStreamReferenceVideoFrameRate is only set when the mixer and renderer are initialized, m_dPrevStartPaint is updated on every frame
                if (m_dStreamReferenceVideoFrameRate && ((dCurrentCounter - m_dPrevStartPaint) > 0.5)) {// twice per second
                    if (m_apVideoSurface[0]) {
                        Paint(0);
                    }
                    NextSleepTime = 4;// 4 ms is a resonable time for this thread to sleep in paused mode
                    break;
                }

                // get scheduled sample
                ASSERT(!m_pCurrentDisplayedSample);
                m_ScheduledSamplesQueue.Lock();
                unsigned __int8 u8SamplesLeft = m_ScheduledSamplesQueue.m_u8QueueElemCount;
                if (!u8SamplesLeft) {
                    m_ScheduledSamplesQueue.Unlock();
                    if (m_bLastSampleOffsetValid && (m_dLastSampleOffset < -1.0)) { // Only starve if we are 1 second behind
                        if (m_nRenderState == Started && !g_bNoDuration) {
                            TRACE(L"EVR: Notify external EVR for input frame starvation\n");
                            m_pSink->Notify(EC_STARVATION, 0, 0);
                            m_bSignaledStarvation = true;
                        }
                    }
                    break;
                } else {
                    --u8SamplesLeft;
                    m_pCurrentDisplayedSample = m_ScheduledSamplesQueue.Dequeue();// reference inherited
                    m_ScheduledSamplesQueue.Unlock();

                    // aquire sample time
                    bool bValidSampleTime = false;
                    LONGLONG llSampleTime;
                    HRESULT hrGetSampleTime = m_pCurrentDisplayedSample->GetSampleTime(&llSampleTime);
                    if (hrGetSampleTime == S_OK) {
                        bValidSampleTime = true;
                        // only register if the sample time is changing
                        if (llSampleTime != m_i64LastSampleTime) {
                            // m_dLastFrameDuration is the recorded sample-to-sample duration, it's less accurate than m_dDetectedVideoTimePerFrame for regular uses
                            double dCurrentDuration = static_cast<double>(llSampleTime - m_i64LastSampleTime) * 0.0000001;
                            // filter out seeking
                            if (abs(dCurrentDuration) < (8.0 * m_dDetectedVideoTimePerFrame)) {
                                m_dLastFrameDuration = dCurrentDuration;
                            } else {
                                m_dLastFrameDuration = m_dDetectedVideoTimePerFrame; // this also initializes it to a reasonable value
                                m_bSignaledStarvation = false;
                                m_bDetectedLock = false;
                                m_u8SchedulerAdjustTimeOut = 0;
                            }
                            m_i64LastSampleTime = llSampleTime;
                        }

                        if (!g_bExternalSubtitleTime) {
                            CSubPicAllocatorPresenterImpl::SetTime(g_tSegmentStart + llSampleTime);
                        }
                    }
                    // note: always create dSampleTime, the legacy schedulers are sensitive to it
                    double dSampleTime = static_cast<double>(m_i64LastSampleTime) * 0.0000001;

                    // TRACE ("EVR: RenderThread ==>> Presenting surface %d  (%I64d)\n", m_u8CurrentMixerSurface, nsSampleTime);

                    bool bStepForward = false;

                    if (m_nStepCount) {
                        UINT32 uiSI;
                        if (FAILED(m_pCurrentDisplayedSample->GetUINT32(GUID_SURFACE_INDEX, &uiSI))) {
                            break;
                        }
                        m_u8CurrentMixerSurface = static_cast<unsigned __int8>(uiSI);

                        Paint(0);
                        CompleteFrameStep(false);
                        bStepForward = true;

                    } else if (m_nRenderState == Started) {
                        if (!bValidSampleTime
                                || ((m_dfSurfaceType != D3DFMT_X8R8G8B8) && (m_dcCaps.PixelShaderVersion >= D3DPS_VERSION(3, 0)) && mk_pRendererSettings->iVMR9FrameInterpolation)// frame interpolation is allowed for all versions
                                || (mk_pRendererSettings->iEVRAlternativeScheduler && (m_boCompositionEnabled || (!m_dpPParam.Windowed && (m_u8OSVersionMajor >= 6))))) {// Vista and newer only
                            // Just play as fast as possible
                            bStepForward = true;
                            UINT32 uiSI;
                            if (FAILED(m_pCurrentDisplayedSample->GetUINT32(GUID_SURFACE_INDEX, &uiSI))) {
                                break;
                            }
                            m_u8CurrentMixerSurface = static_cast<unsigned __int8>(uiSI);
                            NextSleepTime = MININT;// just to disable the the sleep time of this loop for when this routine is used

                            Paint(FRENDERPAINT_NORMAL);
                        } else {
                            double dClockTime;
                            // Calculate wake up timer
                            if (!m_bSignaledStarvation) {
                                dClockTime = GetClockTime(dCurrentCounter);
                                m_dStarvationClock = dClockTime;
                            } else {
                                dClockTime = m_dStarvationClock;
                            }

                            double SyncOffset = 0.0;
                            double VSyncTime = 0.0;
                            double TimeToNextVSync = -1.0;
                            bool bVSyncCorrection = false;
                            double DetectedRefreshTime;
                            double DetectedScanlinesPerFrame;
                            double DetectedScanlineTime;
                            {
                                CAutoLock Lock(&m_csRefreshRateLock);
                                DetectedRefreshTime = m_dDetectedRefreshTime;
                                DetectedScanlinesPerFrame = m_dDetectedScanlinesPerFrame;
                                DetectedScanlineTime = m_dDetectedScanlineTime ? m_dDetectedScanlineTime : DetectedRefreshTime / DetectedScanlinesPerFrame;
                            }

                            if (m_bAlternativeVSync) {
                                bVSyncCorrection = true;
                                double TargetVSyncPos = mk_pRendererSettings->iVMR9VSyncOffset;
                                double RefreshLines = DetectedScanlinesPerFrame;
                                double ScanlinesPerSecond = 1.0 / DetectedScanlineTime;
                                double CurrentVSyncPos = fmod(static_cast<double>(m_i32VBlankStartMeasure) + ScanlinesPerSecond * (dCurrentCounter - m_dVBlankStartMeasureTime), RefreshLines);
                                double LinesUntilVSync = 0.0;
                                //TargetVSyncPos -= ScanlinesPerSecond * (DrawTime/10000000.0);
                                //TargetVSyncPos -= 10;
                                TargetVSyncPos = fmod(TargetVSyncPos, RefreshLines);
                                if (TargetVSyncPos < 0) {
                                    TargetVSyncPos += RefreshLines;
                                }
                                if (TargetVSyncPos > CurrentVSyncPos) {
                                    LinesUntilVSync = TargetVSyncPos - CurrentVSyncPos;
                                } else {
                                    LinesUntilVSync = (RefreshLines - CurrentVSyncPos) + TargetVSyncPos;
                                }
                                double TimeUntilVSync = LinesUntilVSync * DetectedScanlineTime;
                                TimeToNextVSync = TimeUntilVSync;
                                VSyncTime = DetectedRefreshTime;

                                double ClockTimeAtNextVSync = dClockTime + (TimeUntilVSync * m_dModeratedTimeSpeed);

                                SyncOffset = dSampleTime - ClockTimeAtNextVSync;

                                // if (SyncOffset < 0)
                                //    TRACE(L"EVR: SyncOffset(%d): %I64d     %I64d     %I64d\n", m_u8CurrentMixerSurface, SyncOffset, TimePerFrame, VSyncTime);
                            } else {
                                SyncOffset = dSampleTime - dClockTime;
                            }

                            TRACE(L"EVR: SyncOffset: %f SampleFrame: %f ClockFrame: %f\n", SyncOffset, m_dStreamReferenceVideoFrameRate ? dSampleTime * m_dDetectedVideoFrameRate : 0.0, m_dStreamReferenceVideoFrameRate ? dClockTime * m_dDetectedVideoFrameRate : 0.0);
                            double TimePerFrame = m_bDetectedLock ? m_dDetectedVideoTimePerFrame : m_dLastFrameDuration;

                            double MinMargin;
                            //if (m_u8FrameTimeCorrection && 0) MinMargin = 0.0015;
                            //else
                            MinMargin = 0.0015 + (m_dDetectedFrameTimeStdDev < 0.002) ? m_dDetectedFrameTimeStdDev : 0.002;
                            double larger = TimePerFrame * (1.0 / 9.0);
                            double smaller = TimePerFrame * (1.0 / 50.0);
                            double TimePerFrameMargin = (MinMargin < smaller) ? smaller : (MinMargin > larger) ? larger : MinMargin;
                            double TimePerFrameMargin0 = TimePerFrameMargin * 0.5;
                            double TimePerFrameMargin1 = 0.0;

                            if (m_bDetectedLock && TimePerFrame < VSyncTime) {
                                VSyncTime = TimePerFrame;
                            }

                            if (m_u8VSyncMode == 1) {
                                TimePerFrameMargin1 = -TimePerFrameMargin;
                            } else if (m_u8VSyncMode == 2) {
                                TimePerFrameMargin1 = TimePerFrameMargin;
                            }

                            m_dLastSampleOffset = SyncOffset;
                            m_bLastSampleOffsetValid = true;

                            double VSyncOffset0 = 0.0;
                            bool bDoVSyncCorrection = false;
                            if ((SyncOffset < -(TimePerFrame + TimePerFrameMargin0 - TimePerFrameMargin1)) && u8SamplesLeft > 0) { // Only drop if we have something else to display at once
                                // Drop frame
                                TRACE(L"EVR: Dropped frame\n");
                                ++m_pcFramesDropped;
                                bStepForward = true;
                                NextSleepTime = 0;
                                //VSyncOffset0 = (-SyncOffset) - VSyncTime;
                                //VSyncOffset0 = (-SyncOffset) - VSyncTime + TimePerFrameMargin1;
                                //m_dLastPredictedSync = VSyncOffset0;
                                bDoVSyncCorrection = false;
                            } else if (SyncOffset < TimePerFrameMargin1) {

                                if (bVSyncCorrection) {
                                    VSyncOffset0 = -SyncOffset;
                                    bDoVSyncCorrection = true;
                                }

                                // Paint and prepare for next frame
                                TRACE(L"EVR: Normal frame\n");
                                bStepForward = true;
                                UINT32 uiSI;
                                if (FAILED(m_pCurrentDisplayedSample->GetUINT32(GUID_SURFACE_INDEX, &uiSI))) {
                                    break;
                                }
                                m_u8CurrentMixerSurface = static_cast<unsigned __int8>(uiSI);
                                m_dLastPredictedSync = VSyncOffset0;

                                Paint(FRENDERPAINT_NORMAL);

                                NextSleepTime = 0;
                                ++m_pcFramesDrawn;
                            } else {
                                /*
                                if (TimeToNextVSync >= 0.0 && SyncOffset > 0.0) {
                                    NextSleepTime = static_cast<int>(TimeToNextVSync * 1000.0) - 2;
                                } else {
                                    NextSleepTime = static_cast<int>(SyncOffset * 1000.0) - 2;
                                }

                                if (NextSleepTime > TimePerFrame) {
                                    NextSleepTime = 1;
                                }

                                if (NextSleepTime < 0) {
                                //    NextSleepTime = 0;
                                }
                                */
                                NextSleepTime = 1;
                                //TRACE ("EVR: Delay\n");
                            }

                            if (bDoVSyncCorrection) {
                                //double VSyncOffset0 = (((SyncOffset) % VSyncTime) + VSyncTime) % VSyncTime;
                                double Margin = TimePerFrameMargin;

                                double VSyncOffsetMin = m_adVSyncOffsetHistory[0];
                                if (VSyncOffsetMin > m_adVSyncOffsetHistory[1]) {
                                    VSyncOffsetMin = m_adVSyncOffsetHistory[1];
                                }
                                double VSyncOffsetMax = m_adVSyncOffsetHistory[0];
                                if (VSyncOffsetMax < m_adVSyncOffsetHistory[1]) {
                                    VSyncOffsetMax = m_adVSyncOffsetHistory[1];
                                }

                                m_adVSyncOffsetHistory[m_u8VSyncOffsetHistoryPos] = VSyncOffset0;
                                m_u8VSyncOffsetHistoryPos ^= 1; // make it constantly switch between 0 and 1

                                // remove modulo on implementation!
                                //double VSyncTime2 = VSyncTime2 + (VSyncOffsetMax - VSyncOffsetMin);
                                //VSyncOffsetMin; = (((VSyncOffsetMin) % VSyncTime) + VSyncTime) % VSyncTime;
                                //VSyncOffsetMax = (((VSyncOffsetMax) % VSyncTime) + VSyncTime) % VSyncTime;

                                //TRACE(L"EVR: SyncOffset(%d, %d): %8I64d     %8I64d     %8I64d     %8I64d\n", m_u8CurrentMixerSurface, m_u8VSyncMode,VSyncOffset0, VSyncOffsetMin, VSyncOffsetMax, VSyncOffsetMax - VSyncOffsetMin);

                                //TRACE ("MODE = %d, OFFSET0 = %8I64d, VSYNCTIME = %8I64d, OFFSETMIN = %8I64d, OFFSETMAX = %8I64d, MARGIN = %8I64d\n", m_VSyncMode, VSyncOffset0, VSyncTime, VSyncOffsetMin, VSyncOffsetMax, Margin);
                                if (!m_u8VSyncMode) {
                                    // 24/1.001 in 60 Hz
                                    if ((VSyncOffset0 < Margin) && (VSyncOffsetMin < Margin)) {
                                        m_u8VSyncMode = 2;
                                    } else if ((VSyncOffset0 > VSyncTime - Margin) && (VSyncOffsetMax > VSyncTime - Margin)) {
                                        m_u8VSyncMode = 1;
                                    }
                                } else if (m_u8VSyncMode == 2) {
                                    if ((VSyncOffset0 > Margin) && ((VSyncOffsetMin > Margin) || (VSyncOffsetMax < Margin))) {
                                        m_u8VSyncMode = 0;
                                    }
                                } else if (m_u8VSyncMode == 1) {
                                    if ((VSyncOffset0 < VSyncTime - Margin) && ((VSyncOffsetMax < VSyncTime - Margin) || (VSyncOffsetMin > VSyncTime - Margin))) {
                                        m_u8VSyncMode = 0;
                                    }
                                }
                            }

                        }
                    }

                    if (bStepForward) {
                        MoveToFreeList(m_pCurrentDisplayedSample, true);// reference inherited
                        CheckWaitingSampleFromMixer();
                        if (m_dMaxSampleDuration < m_dLastFrameDuration) {
                            m_dMaxSampleDuration = m_dLastFrameDuration;
                        }
                    } else {
                        MoveToScheduledList(m_pCurrentDisplayedSample, true);// reference inherited
                    }
                    m_pCurrentDisplayedSample = nullptr;// its reference is always taken by either of the two previous functions
                }
            }
            break;
            default:
                ASSERT(0);
        }
    }

exit:
    if (hAvrt) {
        EXECUTE_ASSERT(m_fnAvRevertMmThreadCharacteristics(hAvrt));
    }
}

__declspec(nothrow noalias) void CEVRAllocatorPresenter::RemoveAllSamples()
{
    CAutoLock Lock(&m_csExternalMixerLock);
    FlushSamples();
    if (m_pCurrentDisplayedSample) {
        m_pCurrentDisplayedSample->Release();
        m_pCurrentDisplayedSample = nullptr;
    }
    // remove the references kept in the queue (scheduled samples will be empty because of FlushSamples()) and reset them
    m_FreeSamplesQueue.Lock();
    m_FreeSamplesQueue.ReleaseOnAllAndReset();
    m_FreeSamplesQueue.Unlock();

    m_i64LastScheduledSampleTime = -1;
    m_i64LastScheduledUncorrectedSampleTime = -1;
    m_u8MixerSurfacesUsed = 0;
}

__declspec(nothrow noalias) void CEVRAllocatorPresenter::MoveToFreeList(IMFSample* pSample, bool bTail)
{
    if (!pSample) {
        return;// this can happen while flushing samples
    }

    m_FreeSamplesQueue.Lock();
    --m_u8MixerSurfacesUsed;
    if (!m_u8MixerSurfacesUsed && m_bPendingMediaFinished) {
        m_bPendingMediaFinished = false;
        TRACE(L"EVR: Notify external EVR for end of stream\n");
        m_pSink->Notify(EC_COMPLETE, 0, 0);
    }
    if (bTail) {
        m_FreeSamplesQueue.Enqueue(pSample);// reference inherited
    } else {
        m_FreeSamplesQueue.EnqueueReverse(pSample);// reference inherited
    }
    m_FreeSamplesQueue.Unlock();
}

__declspec(nothrow noalias) void CEVRAllocatorPresenter::MoveToScheduledList(IMFSample* pSample, bool bSorted)
{
    if (!pSample) {
        return;// this can happen while flushing samples
    }
    m_ScheduledSamplesQueue.Lock();
    if (bSorted) {
        // Insert sorted
        /*
        POSITION Iterator = m_ScheduledSamples.GetHeadPosition();

        LONGLONG NewSampleTime;
        pSample->GetSampleTime(&NewSampleTime);

        while (Iterator != nullptr)
        {
            POSITION CurrentPos = Iterator;
            IMFSample *pIter = m_ScheduledSamples.GetNext(Iterator);
            LONGLONG SampleTime;
            pIter->GetSampleTime(&SampleTime);
            if (NewSampleTime < SampleTime)
            {
                m_ScheduledSamples.InsertBefore(CurrentPos, pSample);// reference inherited
                return;
            }
        }
        */
        m_ScheduledSamplesQueue.EnqueueReverse(pSample);// reference inherited
    } else {
        //m_dDetectedVideoFrameRate = 60.0 / 1.001;
        //m_dDetectedVideoFrameRate = 24.0 / 1.001;
        //m_dDetectedVideoTimePerFrame = 1.0 / m_dDetectedVideoFrameRate;// only useful to force FPS on input video for testing purposes

        LONGLONG PrevTime = m_i64LastScheduledUncorrectedSampleTime;
        LONGLONG Time, SetDuration;
        pSample->GetSampleDuration(&SetDuration);
        pSample->GetSampleTime(&Time);
        m_i64LastScheduledUncorrectedSampleTime = Time;
        m_bCorrectedFrameTime = 0;

        LONGLONG Diff = (PrevTime == -1) ? 0 : Time - PrevTime;
        double SeekLimit = 8.0 * m_dDetectedVideoTimePerFrame;
        // filter out seeking
        if (m_dStreamReferenceVideoFrameRate && (abs(static_cast<double>(Diff) * 0.0000001) < SeekLimit) && (abs(static_cast<double>(PrevTime) * 0.0000001 - m_dLastScheduledSampleTimeFP) < SeekLimit)) {
            //double PredictedNext = PrevTime+m_dDetectedVideoTimePerFrame;
            //double PredictedDiff = abs(PredictedNext-Time);

            if (/*PredictedDiff > 15000 &&*/ m_bDetectedLock && mk_pRendererSettings->iEVREnableFrameTimeCorrection) {
                m_dLastScheduledSampleTimeFP = static_cast<double>(Time) * 0.0000001;
                double PredictedTime = m_dLastScheduledSampleTimeFP + m_dDetectedVideoTimePerFrame;
                if (fabs(PredictedTime - m_dLastScheduledSampleTimeFP) > 0.0015) { // 1.5 ms wrong, lets correct
                    m_dLastScheduledSampleTimeFP = PredictedTime;
                    pSample->SetSampleTime(static_cast<LONGLONG>(m_dLastScheduledSampleTimeFP * 10000000.0 + 0.5));
                    pSample->SetSampleDuration(static_cast<LONGLONG>(m_dDetectedVideoTimePerFrame * 10000000.0 + 0.5));
                    m_bCorrectedFrameTime = true;
                    m_u8FrameTimeCorrection = 30;
                }
            } else {
                m_dLastScheduledSampleTimeFP = static_cast<double>(Time) * 0.0000001;
            }
        } else {
            m_dLastScheduledSampleTimeFP = static_cast<double>(Time) * 0.0000001;
            if (abs(static_cast<double>(Diff) * 0.0000001) > SeekLimit) {
                // Seek
                m_bSignaledStarvation = false;
                m_bDetectedLock = false;
                m_u8SchedulerAdjustTimeOut = 0;
            }
        }

        // TRACE(L"EVR: Time: %f %f %f\n", static_cast<double>(Time) * 0.0000001, static_cast<double>(SetDuration) * 0.0000001, m_dDetectedVideoFrameRate);
        if (!m_bCorrectedFrameTime && m_u8FrameTimeCorrection) {
            --m_u8FrameTimeCorrection;
        }
#if 0
        if (Time <= m_i64LastScheduledUncorrectedSampleTime && m_i64LastScheduledSampleTime >= 0) {
            PrevTime = m_i64LastScheduledSampleTime;
        }

        m_bCorrectedFrameTime = 0;
        if (PrevTime != -1 && (Time >= PrevTime - ((Duration * 20) / 9) || !Time) || ForceFPS) {
            if (Time - PrevTime > ((Duration * 20) / 9) && Time - PrevTime < Duration * 8 || !Time || ((Time - PrevTime) < (Duration / 11)) || ForceFPS) {
                // Error!!!!
                Time = PrevTime + Duration;
                pSample->SetSampleTime(Time);
                pSample->SetSampleDuration(Duration);
                m_bCorrectedFrameTime = 1;
                TRACE(L"EVR: Corrected invalid sample time\n");
            }
        }
        if (Time + Duration * 10 < m_i64LastScheduledSampleTime) {
            // Flush when repeating movie
            FlushSamplesInternal();
        }
#endif

#if 0
        //static LONGLONG LastDuration = 0;
        double SetDuration = m_dDetectedVideoTimePerFrame;
        pSample->GetSampleDuration(&SetDuration);
        if (SetDuration != LastDuration) {
            TRACE(L"EVR: Old duration: %I64d New duration: %I64d\n", LastDuration, SetDuration);
        }
        LastDuration = SetDuration;
#endif
        m_i64LastScheduledSampleTime = Time;

        m_ScheduledSamplesQueue.Enqueue(pSample);// reference inherited
    }
    m_ScheduledSamplesQueue.Unlock();
}

__declspec(nothrow noalias) __forceinline void CEVRAllocatorPresenter::FlushSamples()
{
    FlushSamplesInternal();
    m_i64LastScheduledSampleTime = -1;
}

__declspec(nothrow noalias) void CEVRAllocatorPresenter::FlushSamplesInternal()
{
    CAutoLock Lock(&m_csExternalMixerLock);
    m_FreeSamplesQueue.Lock();
    m_ScheduledSamplesQueue.Lock();

    // move all samples to the free samples queue
    if (m_pCurrentDisplayedSample) {
        m_FreeSamplesQueue.Enqueue(m_pCurrentDisplayedSample);// reference inherited
        m_pCurrentDisplayedSample = nullptr;
    }
    m_ScheduledSamplesQueue.MoveContentToOther(&m_FreeSamplesQueue);

    m_u8MixerSurfacesUsed = 0;
    m_dLastSampleOffset = 0;
    m_bLastSampleOffsetValid = false;
    m_bSignaledStarvation = false;
    if (m_bPendingMediaFinished) { // a end-of-media status during a flush is technically possible, but rare
        m_bPendingMediaFinished = false;
        TRACE(L"EVR: Notify external EVR for end of stream\n");
        m_pSink->Notify(EC_COMPLETE, 0, 0);
    }
    m_ScheduledSamplesQueue.Unlock();
    m_FreeSamplesQueue.Unlock();
}
