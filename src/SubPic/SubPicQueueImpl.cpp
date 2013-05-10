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
#include "ISubPic.h"
#include "../DSUtil/DSUtil.h"
#include <intrin.h>

//
// SPArrayQueue
//

__declspec(nothrow noalias) __forceinline void SPArrayQueue::ReleaseOnAllAndReset()
{
    ASSERT((m_u8QueueElemCount == (m_u8QueueListEnd - m_u8QueueListBeginning & 63)) || ((m_u8QueueElemCount == 64) && !(m_u8QueueListEnd - m_u8QueueListBeginning & 63)));

    unsigned __int8 u8QueueElemCount = m_u8QueueElemCount;
    if (u8QueueElemCount) {
        unsigned __int8 u8QueueListBeginning = m_u8QueueListBeginning;
        do {
            m_paData[u8QueueListBeginning]->Release();
            DEBUG_ONLY(m_paData[u8QueueListBeginning] = reinterpret_cast<CBSubPic*>(MAXSIZE_T));// error check: make sure the pointer is invalid
            u8QueueListBeginning = u8QueueListBeginning + 1 & 63;
        } while (--u8QueueElemCount);
        m_u8QueueListBeginning = m_u8QueueListEnd = m_u8QueueElemCount = 0;
    }
}

__declspec(nothrow noalias) __forceinline void SPArrayQueue::MoveContentToOther(SPArrayQueue* pOther)
{
    ASSERT((m_u8QueueElemCount == (m_u8QueueListEnd - m_u8QueueListBeginning & 63)) || ((m_u8QueueElemCount == 64) && !(m_u8QueueListEnd - m_u8QueueListBeginning & 63)));
    ASSERT(pOther && (pOther != this));
    ASSERT(pOther->m_u8QueueElemCount + m_u8QueueElemCount <= 64);
    ASSERT((pOther->m_u8QueueElemCount == (pOther->m_u8QueueListEnd - pOther->m_u8QueueListBeginning & 63)) || ((pOther->m_u8QueueElemCount == 64) && !(pOther->m_u8QueueListEnd - pOther->m_u8QueueListBeginning & 63)));

    unsigned __int8 u8QueueElemCount = m_u8QueueElemCount;
    if (u8QueueElemCount) {
        unsigned __int8 u8QueueListBeginning = m_u8QueueListBeginning;
        unsigned __int8 u8OtherQueueListEnd = pOther->m_u8QueueListEnd;
        pOther->m_u8QueueElemCount += u8QueueElemCount;
        do {
            pOther->m_paData[u8OtherQueueListEnd] = m_paData[u8QueueListBeginning];
            DEBUG_ONLY(m_paData[u8QueueListBeginning] = reinterpret_cast<CBSubPic*>(MAXSIZE_T));// error check: make sure the pointer is invalid
            u8OtherQueueListEnd = u8OtherQueueListEnd + 1 & 63;
            u8QueueListBeginning = u8QueueListBeginning + 1 & 63;
        } while (--u8QueueElemCount);
        pOther->m_u8QueueListEnd = u8OtherQueueListEnd;
        m_u8QueueListBeginning = m_u8QueueListEnd = m_u8QueueElemCount = 0;
    }
}

__declspec(nothrow noalias) __forceinline void SPArrayQueue::Enqueue(CBSubPic* pItem)
{
    ASSERT((m_u8QueueElemCount == (m_u8QueueListEnd - m_u8QueueListBeginning & 63)) || ((m_u8QueueElemCount == 64) && !(m_u8QueueListEnd - m_u8QueueListBeginning & 63)));
    ASSERT(pItem && (pItem != reinterpret_cast<CBSubPic*>(MAXSIZE_T)));// error check: make sure the pointer is valid
    ASSERT(m_u8QueueElemCount < 64);// error check: make sure we aren't exceeding our maximum storage space

    unsigned __int8 u8QueueListEnd = m_u8QueueListEnd;
    m_paData[u8QueueListEnd] = pItem;
    ++m_u8QueueElemCount;
    m_u8QueueListEnd = u8QueueListEnd + 1 & 63;
}

__declspec(nothrow noalias) __forceinline void SPArrayQueue::EnqueueReverse(CBSubPic* pItem)
{
    ASSERT((m_u8QueueElemCount == (m_u8QueueListEnd - m_u8QueueListBeginning & 63)) || ((m_u8QueueElemCount == 64) && !(m_u8QueueListEnd - m_u8QueueListBeginning & 63)));
    ASSERT(pItem && (pItem != reinterpret_cast<CBSubPic*>(MAXSIZE_T)));// error check: make sure the pointer is valid
    ASSERT(m_u8QueueElemCount < 64);// error check: make sure we aren't dequeueing from an empty queue

    ++m_u8QueueElemCount;
    unsigned __int8 u8QueueListBeginning = m_u8QueueListBeginning - 1 & 63;
    m_u8QueueListBeginning = u8QueueListBeginning;
    m_paData[u8QueueListBeginning] = pItem;
}

__declspec(nothrow noalias restrict) __forceinline CBSubPic* SPArrayQueue::Dequeue()
{
    ASSERT((m_u8QueueElemCount == (m_u8QueueListEnd - m_u8QueueListBeginning & 63)) || ((m_u8QueueElemCount == 64) && !(m_u8QueueListEnd - m_u8QueueListBeginning & 63)));
    ASSERT(m_u8QueueElemCount);// error check: make sure we aren't dequeueing from an empty queue

    unsigned __int8 u8QueueListBeginning = m_u8QueueListBeginning;
    CBSubPic* pReturnValue = m_paData[u8QueueListBeginning];
    DEBUG_ONLY(m_paData[u8QueueListBeginning] = reinterpret_cast<CBSubPic*>(MAXSIZE_T));// error check: make sure the pointer is invalid
    --m_u8QueueElemCount;
    m_u8QueueListBeginning = u8QueueListBeginning + 1 & 63;
    return pReturnValue;
}

__declspec(nothrow noalias restrict) __forceinline CBSubPic* SPArrayQueue::ReadFirstElem()
{
    ASSERT((m_u8QueueElemCount == (m_u8QueueListEnd - m_u8QueueListBeginning & 63)) || ((m_u8QueueElemCount == 64) && !(m_u8QueueListEnd - m_u8QueueListBeginning & 63)));
    ASSERT(m_u8QueueElemCount);// error check: make sure we aren't reading from an empty queue

    return m_paData[m_u8QueueListBeginning];
}

__declspec(nothrow noalias) __forceinline void SPArrayQueue::DiscardFirstElem()
{
    ASSERT((m_u8QueueElemCount == (m_u8QueueListEnd - m_u8QueueListBeginning & 63)) || ((m_u8QueueElemCount == 64) && !(m_u8QueueListEnd - m_u8QueueListBeginning & 63)));
    ASSERT(m_u8QueueElemCount);// error check: make sure we aren't discarding from an empty queue

    unsigned __int8 u8QueueListBeginning = m_u8QueueListBeginning;
    DEBUG_ONLY(m_paData[u8QueueListBeginning] = reinterpret_cast<CBSubPic*>(MAXSIZE_T));// error check: make sure the pointer is invalid
    --m_u8QueueElemCount;
    m_u8QueueListBeginning = u8QueueListBeginning + 1 & 63;
}

__declspec(nothrow noalias restrict) __forceinline CBSubPic* SPArrayQueue::ReadSecondElem()
{
    ASSERT((m_u8QueueElemCount == (m_u8QueueListEnd - m_u8QueueListBeginning & 63)) || ((m_u8QueueElemCount == 64) && !(m_u8QueueListEnd - m_u8QueueListBeginning & 63)));
    ASSERT(m_u8QueueElemCount > 1);// error check: make sure we aren't reading from an empty or nearly empty queue

    return m_paData[m_u8QueueListBeginning + 1 & 63];
}

__declspec(nothrow noalias restrict) __forceinline CBSubPic* SPArrayQueue::ReadLastElem()
{
    ASSERT((m_u8QueueElemCount == (m_u8QueueListEnd - m_u8QueueListBeginning & 63)) || ((m_u8QueueElemCount == 64) && !(m_u8QueueListEnd - m_u8QueueListBeginning & 63)));
    ASSERT(m_u8QueueElemCount);// error check: make sure we aren't reading from an empty queue

    return m_paData[m_u8QueueListEnd - 1 & 63];
}

__declspec(nothrow noalias restrict) __forceinline CBSubPic* SPArrayQueue::ReadElemAtOffset(unsigned __int8 u8Offset)
{
    ASSERT((m_u8QueueElemCount == (m_u8QueueListEnd - m_u8QueueListBeginning & 63)) || ((m_u8QueueElemCount == 64) && !(m_u8QueueListEnd - m_u8QueueListBeginning & 63)));
    ASSERT(m_u8QueueElemCount > u8Offset);// error check: make sure we aren't reading from an empty queue slot

    return m_paData[m_u8QueueListBeginning + u8Offset & 63];
}

__declspec(nothrow noalias) __forceinline void SPArrayQueue::Lock()
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

__declspec(nothrow noalias) __forceinline void SPArrayQueue::Unlock()
{
    _ReadWriteBarrier();// a no-op, this is only prevent the compiler from reordering
    ASSERT(!mv_cLock);// it should be locked
    mv_cLock = 1;
}

//
// CSubPicQueueImpl
//
// IUnknown

__declspec(nothrow noalias) STDMETHODIMP CSubPicQueueImpl::QueryInterface(REFIID riid, __deref_out void** ppv)
{
    ASSERT(ppv);
    __assume(this);// fix assembly: the compiler generated tests for null pointer input on static_cast<T>(this)

    __int64 lo = reinterpret_cast<__int64 const*>(&riid)[0], hi = reinterpret_cast<__int64 const*>(&riid)[1];
    void* pv = static_cast<IUnknown*>(this);
    if (lo == reinterpret_cast<__int64 const*>(&IID_IUnknown)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IUnknown)[1]) {
        goto exit;
    }
    pv = this;
    if (lo == reinterpret_cast<__int64 const*>(&__uuidof(CSubPicQueueImpl))[0] && hi == reinterpret_cast<__int64 const*>(&__uuidof(CSubPicQueueImpl))[1]) {
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

__declspec(nothrow noalias) STDMETHODIMP_(ULONG) CSubPicQueueImpl::AddRef()
{
    // based on CUnknown::NonDelegatingAddRef()
    // the original CUnknown::NonDelegatingAddRef() has a version that keeps compatibility for Windows 95, Windows NT 3.51 and earlier, this one doesn't
    ULONG ulRef = _InterlockedIncrement(reinterpret_cast<LONG volatile*>(&mv_ulReferenceCount));
    ASSERT(ulRef);
    return ulRef;
}

__declspec(nothrow noalias) STDMETHODIMP_(ULONG) CSubPicQueueImpl::Release()
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
        this->~CSubPicQueueImpl();
        free(this);
        return 0;
    } else {
        // Don't touch the counter again even in this leg as the object
        // may have just been released on another thread too
        return ulRef;
    }
}

//
// CSubPicQueue
//
// CSubPicQueueImpl

__declspec(nothrow noalias) void CSubPicQueue::SetSubPicProvider(__inout CSubPicProviderImpl* pSubPicProvider)
{
#ifdef DSubPicTrace
    TRACE("SetSubPicProvider()\n");
#endif
    if (m_hQueueThread) {
        m_csSubPicProvider.Lock();
        // break the loop, remove all existing subtitles if invalidation is required, the interlocked operands guarantee proper cross-thread memory handling
        InterlockedExchange64(&mv_i64Invalidate, -1);
        _InterlockedExchange8(&mv_cBreakBuffering, FSUBPICQUEUE_BREAKBUFFERING);
    }

    if (CSubPicProviderImpl* pNonVolaSubPicProvider = reinterpret_cast<CSubPicProviderImpl*>(InterlockedExchangePointer(reinterpret_cast<void* volatile*>(&mv_pSubPicProvider), pSubPicProvider))) {// interlocked operand guarantees proper cross-thread memory handling
        pNonVolaSubPicProvider->Release();
    }
    if (pSubPicProvider) {
        pSubPicProvider->AddRef();
        if (!m_hQueueThread) { // start thread when adding a SubPicProvider
            mv_i64CurrentRenderStart = mv_i64CurrentRenderStop = -1;// initialization to invalid, CreateThread() will issue the memory lock here
            *reinterpret_cast<__int16 volatile*>(&mv_cBreakBuffering) = 0;// also clears mv_cbWaitableHandleForQueueSet, CreateThread() will issue the memory lock here
            m_hQueueThread = ::CreateThread(nullptr, 0x20000, QueueThreadStatic, this, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
            ASSERT(m_hQueueThread);
        } else {
            m_csSubPicProvider.Unlock();
            SetEvent(m_hEvtTime);// make sure the thread is active
        }
    } else if (m_hQueueThread) { // close open thread
        _InterlockedExchange8(&mv_cBreakBuffering, FSUBPICQUEUE_BREAKBUFFERING | FSUBPICQUEUE_QUIT);// interlocked operand guarantees proper cross-thread memory handling
        m_csSubPicProvider.Unlock();
        // make sure we don't lock up a thread in LookupSubPic()
        if (mv_cbWaitableHandleForQueueSet) {
            _InterlockedExchange8(&mv_cbWaitableHandleForQueueSet, false);// interlocked operand guarantees proper cross-thread memory handling
            SetEvent(m_hWaitableForQueue);
        }
        SetEvent(m_hEvtTime);// make sure that the queue thread is active
        if (WaitForSingleObject(m_hQueueThread, 10000) == WAIT_TIMEOUT) {
            ASSERT(0);
            TerminateThread(m_hQueueThread, 0xDEAD);
            // ugly, but we need to guarantee that all critical sections are unlocked
            m_csSubPicProvider.~CCritSec();// DeleteCriticalSection()
            new(&m_csSubPicProvider) CCritSec();// InitializeCriticalSection()
        }
        CloseHandle(m_hQueueThread);
        m_hQueueThread = nullptr;
        ResetEvent(m_hEvtTime);
        // remove all allocated SubPics
        m_Queue.ReleaseOnAllAndReset();// remove the references kept in the queue
        m_pSubPicAllocator->DeallocStaticSubPic();// if no SubPicProvider is set, the SubPicQueue can have no resources allocated in the SubPicAllocator, except when handling the destructor of the SubPicQueue
    }
}

__declspec(nothrow noalias) void CSubPicQueue::SetFPS(__in double fps)
{
    if (mv_dDetectedFrameRate != fps) {
        __int64 i64encdDetectedFrameRate = reinterpret_cast<__int64&>(fps);
        __int64 i64TimePerFrame = static_cast<__int64>(10000000.0 / fps + 0.5);
        // break the loop, don't remove existing subtitles, the interlocked operands guarantee proper cross-thread memory handling
        _InterlockedExchange8(&mv_cBreakBuffering, FSUBPICQUEUE_BREAKBUFFERING);
        InterlockedExchange64(&mv_i64Invalidate, MAXINT64);
        InterlockedExchange64(reinterpret_cast<__int64 volatile*>(&mv_dDetectedFrameRate), i64encdDetectedFrameRate);
        InterlockedExchange64(&mv_i64TimePerFrame, i64TimePerFrame);
        SetEvent(m_hEvtTime);
    }
}

__declspec(nothrow noalias) void CSubPicQueue::SetTime(__in __int64 i64Now)
{
    __int64 i64NonVolaNow = mv_i64Now, i64NonVolaTimePerFrame = mv_i64TimePerFrame;
    // do not accept time changes from .75 second under mv_i64Now to half a frame time over mv_i64Now
    if ((i64Now - static_cast<__int64>(static_cast<unsigned __int64>(i64NonVolaTimePerFrame) >> 1) > i64NonVolaNow) || (i64Now + 7500000 <= i64NonVolaNow)) {
        InterlockedExchange64(&mv_i64Now, i64Now);// the interlocked operand guarantees proper cross-thread memory handling
        __int64 i64SeekLimit = i64NonVolaTimePerFrame << 3;
        if (abs(i64Now - i64NonVolaNow) >= i64SeekLimit) {
            // break the loop, remove all existing subtitles if invalidation is required, the interlocked operands guarantee proper cross-thread memory handling
            InterlockedExchange64(&mv_i64Invalidate, -1);
            _InterlockedExchange8(&mv_cBreakBuffering, FSUBPICQUEUE_BREAKBUFFERING);
        }
        SetEvent(m_hEvtTime);
    }
}

__declspec(nothrow noalias) void CSubPicQueue::InvalidateSubPic(__in __int64 i64Invalidate)
{
    // break the loop, remove some of the existing subtitles, the interlocked operands guarantee proper cross-thread memory handling
    InterlockedExchange64(&mv_i64Invalidate, i64Invalidate);
    _InterlockedExchange8(&mv_cBreakBuffering, FSUBPICQUEUE_BREAKBUFFERING);
#ifdef DSubPicTrace
    TRACE("InvalidateSubPic(): %f\n", static_cast<double>(i64Invalidate) / 10000000.0);
#endif
    SetEvent(m_hEvtTime);
}

__declspec(nothrow noalias restrict) CBSubPic* CSubPicQueue::LookupSubPic(__in __int64 i64Now, __in __int32 i32msWaitable)
{
#ifdef DSubPicTrace
    TRACE("LookupSubPic()\n");
#endif
    if (!mv_pSubPicProvider) {
        return nullptr;
    }
Restart:
    m_Queue.Lock();
    // the time stamps of the video will always be very variable, a path layed out by mv_i64TimePerFrame might be slightly below the intended frame start point created in the queue, so we compensate a quarter of a frame time
    i64Now += static_cast<unsigned __int64>(mv_i64TimePerFrame) >> 2;

    while (unsigned __int8 u8QueueElemCount = m_Queue.m_u8QueueElemCount) {
        CBSubPic* pSubPic = m_Queue.ReadFirstElem();

        __int64 i64SegmentStop = pSubPic->GetSegmentStop();
        if (i64Now >= i64SegmentStop) { // remove old subtitle
#ifdef DSubPicTrace
            __int64 i64RStart = pSubPic->GetStart();
            __int64 i64RStop = pSubPic->GetStop();
            TRACE("Remove SubPic: %f->%f\n", static_cast<double>(i64RStart) / 10000000.0, static_cast<double>(i64RStop) / 10000000.0);
#endif
            pSubPic->Release();
            m_Queue.DiscardFirstElem();
            SetEvent(m_hEvtTime);// the queue thread can start to render a new picture
            continue;
        }

        __int64 i64Start = pSubPic->GetStart();
        if (i64Now >= i64Start) {
            if (u8QueueElemCount > 1) {// compare to next item
                CBSubPic* pSubPicNext = m_Queue.ReadSecondElem();
                __int64 i64StartNext = pSubPicNext->GetStart();
                if (i64Now >= i64StartNext) {// remove old animated items, as long as there are newer ones available
#ifdef DSubPicTrace
                    __int64 i64RStop = pSubPic->GetStop();
                    TRACE("Remove SubPic: %f->%f\n", static_cast<double>(i64Start) / 10000000.0, static_cast<double>(i64RStop) / 10000000.0);
#endif
                    pSubPic->Release();
                    m_Queue.DiscardFirstElem();
                    SetEvent(m_hEvtTime);// the queue thread can start to render a new picture
                    continue;
                }
            }
            pSubPic->AddRef();
#ifdef DSubPicTrace
            __int64 i64Start = pSubPic->GetStart();
            __int64 i64SegmentStop = pSubPic->GetSegmentStop();
            RECT r = pSubPic->GetDirtyRect();
            TRACE("Display SubPic: %f->%f   %f    %d×%d\n", static_cast<double>(i64Start) / 10000000.0, static_cast<double>(i64SegmentStop) / 10000000.0, static_cast<double>(i64Now) / 10000000.0, r.right - r.left, r.bottom - r.top);
#endif
            m_Queue.Unlock();
            return pSubPic;
        } else {
            m_Queue.Unlock();
            return nullptr;
        }
    }
    m_Queue.Unlock();

    if (i32msWaitable > 0) {
        // mv_i64CurrentRenderStart and mv_i64CurrentRenderStop should be good indicators, with proper cross-thread handling by this class
        // if these two variables are not handled propely, the renderer may stall for nothing, or not wait on subtitles
        // I designed this function to not have any heavy thread locking at all on the host renderer (m_Queue.Lock() is a light contention lock, only used on a short section)
        // this function should guarantee this way to only take minimal time if i32msWaitable is 0 or lower and else no more than indicated by i32msWaitable, a heavy thread lock would make that impossible
        if ((mv_i64CurrentRenderStart > i64Now) || (i64Now >= mv_i64CurrentRenderStop)) {// only wait if a subtitle is being produced for this frame
            return nullptr;
        }
        // wait for a subtitle to enter the queue
        EXECUTE_ASSERT(ResetEvent(m_hWaitableForQueue));// make sure it's not set yet
        _InterlockedExchange8(&mv_cbWaitableHandleForQueueSet, true);// interlocked operand guarantees proper cross-thread memory handling
#ifdef DSubPicTrace
        TRACE("Waiting on SubPic: %u ms max\n", i32msWaitable);
#endif
        DWORD Ret = WaitForSingleObject(m_hWaitableForQueue, i32msWaitable);
        if (Ret == WAIT_OBJECT_0) {
            i32msWaitable = 0;// only run once
#ifdef DSubPicTrace
            TRACE("New SubPic ready, possibly suitable for display\n");
#endif
            goto Restart;
        }
        ASSERT(Ret == WAIT_TIMEOUT);// WAIT_ABANDONED or WAIT_FAILED can also occur, but really shouldn't happen
#ifdef DSubPicTrace
        TRACE("Waiting on SubPic timed out\n");
#endif
        return nullptr;
    }
#ifdef DSubPicTrace
    TRACE("No SubPic available, nor does the does the host allow waiting for one\n");
#endif
    return nullptr;
}

__declspec(nothrow noalias) void CSubPicQueue::GetStats(__out SubPicQueueStats* pStats)
{
    ASSERT(pStats);

    __int64 i64Start = 0, i64Stop = 0;
    m_Queue.Lock();
    unsigned __int8 u8SubPics = m_Queue.m_u8QueueElemCount;
    pStats->u8SubPics = u8SubPics;
    if (u8SubPics) {
        __int64 i64Now = mv_i64Now;
        i64Start = m_Queue.ReadFirstElem()->GetStart() - i64Now;
        i64Stop = m_Queue.ReadLastElem()->GetSegmentStop() - i64Now;// if the generation of subpics is slow, older subpics may extend their regular usage duration even up to the segment stop time (trading off smooth animation), represent this feature in the stats
    }
    m_Queue.Unlock();
    pStats->i64Start = i64Start;
    pStats->i64Stop = i64Stop;
}

__declspec(nothrow noalias) void CSubPicQueue::GetStats(__in unsigned __int8 u8SubPic, __out_ecount(2) __int64 pStartStop[2])
{
    ASSERT(pStartStop);

    __int64 i64Start = 0, i64Stop = 0;
    m_Queue.Lock();
    if (u8SubPic < m_Queue.m_u8QueueElemCount) {
        CBSubPic* pSubPic = m_Queue.ReadElemAtOffset(u8SubPic);
        i64Start = pSubPic->GetStart();
        i64Stop = pSubPic->GetStop();
    }
    m_Queue.Unlock();
    pStartStop[0] = i64Start;
    pStartStop[1] = i64Stop;
}

// private

__declspec(nothrow noalias) DWORD WINAPI CSubPicQueue::QueueThreadStatic(__in LPVOID lpParam)
{
    ASSERT(lpParam);

    DEBUG_ONLY(SetThreadName(0xFFFFFFFF, "CSubPicQueue::QueueThread"));
    reinterpret_cast<CSubPicQueue*>(lpParam)->QueueThread();
    return 0;
}

__declspec(nothrow noalias) __forceinline void CSubPicQueue::QueueThread()
{
    SetThreadPriority(m_hQueueThread, mk_bDisableAnim ? THREAD_PRIORITY_LOWEST : THREAD_PRIORITY_ABOVE_NORMAL);

    for (;;) {
        ResetEvent(m_hEvtTime);// make sure the thread is put to sleep
        WaitForSingleObject(m_hEvtTime, INFINITE);

HandleBreakBuffering:
        if (mv_cBreakBuffering) {
            if (mv_cBreakBuffering & FSUBPICQUEUE_QUIT) {// SetSubPicProvider() and the destructor can just wipe the items in the queue afterwards
                break;
            }
            __int64 i64Invalidate = mv_i64Invalidate;
            if (i64Invalidate != MAXINT64) {// don't remove existing subtitles if mv_i64Invalidate is flagged
                m_Queue.Lock();
                // remove the references kept in the queue
                if (i64Invalidate == -1) {
                    m_Queue.ReleaseOnAllAndReset();
                } else while (m_Queue.m_u8QueueElemCount) {
                        CBSubPic* pSubPic = m_Queue.ReadFirstElem();
                        __int64 i64Stop = pSubPic->GetStop();
                        if (i64Stop <= i64Invalidate) {
                            break;
                        }
#ifdef DSubPicTrace
                        __int64 i64RStart = pSubPic->GetStart();
                        TRACE("Removed subtitle because of invalidation: %f->%f\n", static_cast<double>(i64RStart) / 10000000.0, static_cast<double>(i64Stop) / 10000000.0);
#endif
                        pSubPic->Release();
                        m_Queue.DiscardFirstElem();
                    }
                m_Queue.Unlock();
            }
            char cbBreakBuffering = _InterlockedExchange8(&mv_cBreakBuffering, 0);// interlocked operand guarantees proper cross-thread memory handling
            if (cbBreakBuffering & FSUBPICQUEUE_QUIT) {// the second check is important here, as mv_cBreakBuffering could have been altered while processing the previous instructions
                break;
            }
        }

        if (m_Queue.m_u8QueueElemCount >= m_u8MaxSubPic) {
            continue;// this condition is sometimes triggered because various functions trigger m_hEvtTime and sometimes the jump to HandleBreakBuffering is taken
        }

        {
            // get the non-volatile pointer to mv_pSubPicProvider
            m_csSubPicProvider.Lock();// if a change or release of mv_pSubPicProvider would happen, a fatal exception would follow
            if (mv_cBreakBuffering) {// mv_cBreakBuffering will be set to quit mode if there's no SubPicProvider anymore
                if (mv_cbWaitableHandleForQueueSet) {// always make sure that the other thread isn't locked
                    _InterlockedExchange8(&mv_cbWaitableHandleForQueueSet, false);// interlocked operand guarantees proper cross-thread memory handling
                    SetEvent(m_hWaitableForQueue);
                }
                m_csSubPicProvider.Unlock();
                goto HandleBreakBuffering;
            }
            CSubPicProviderImpl* pNonVolaSubPicProvider(mv_pSubPicProvider);
            pNonVolaSubPicProvider->AddRef();
            m_csSubPicProvider.Unlock();
            pNonVolaSubPicProvider->Lock();// can absolutely not be combined with m_csSubPicProvider

            // read the current time from the queue
            m_Queue.Lock();
            __int64 i64Now = 0;// if mv_i64Now is higher, this value will be overwritten later on in the loop
            if (m_Queue.m_u8QueueElemCount) {
                i64Now = m_Queue.ReadLastElem()->GetStop();
            }
            m_Queue.Unlock();
            double dNonVolaDetectedFrameRate = mv_dDetectedFrameRate;
            __int64 i64TimePerFrame = mv_i64TimePerFrame;

            // search SubPicProvider for subtitles to render
TryNextPosRenewNow:
            __int64 i64NonVolaNow = mv_i64Now;
            if (i64NonVolaNow > i64Now) {
                i64Now = i64NonVolaNow;
            } else {
TryNextPos:
                if (i64Now >= i64NonVolaNow + 600000000) { // we are already one minute ahead, this should be enough
                    if (mv_cbWaitableHandleForQueueSet) {// always make sure that the other thread isn't locked
                        _InterlockedExchange8(&mv_cbWaitableHandleForQueueSet, false);// interlocked operand guarantees proper cross-thread memory handling
                        SetEvent(m_hWaitableForQueue);
                    }
                    pNonVolaSubPicProvider->Unlock();
                    pNonVolaSubPicProvider->Release();
                    continue;// full buffer, so sleep
                }
            }

            POSITION pos = pNonVolaSubPicProvider->GetStartPosition(i64Now, dNonVolaDetectedFrameRate);
            if (!pos) {
                i64Now += i64TimePerFrame;// try using the next frame time
                goto TryNextPos;
            }
            __int64 i64Start = pNonVolaSubPicProvider->GetStart(pos, dNonVolaDetectedFrameRate);
            if (i64Start > i64Now) {
                i64Now += i64TimePerFrame;// try using the next frame time
                goto TryNextPos;
            }
            __int64 i64Stop = pNonVolaSubPicProvider->GetStop(pos, dNonVolaDetectedFrameRate);
            if (i64Now >= i64Stop) {
                i64Now += i64TimePerFrame;// try using the next frame time
                goto TryNextPos;
            }

            if (unsigned __int64 u64TextureSize = pNonVolaSubPicProvider->GetTextureSize(pos)) {// used for bitmapped subtitles that need fixed size textures
                m_pSubPicAllocator->SetCurSize(u64TextureSize);
            }

            CBSubPic* pStatic = m_pSubPicAllocator->AllocStaticSubPic();
            if (!pStatic) {
                goto BreakBuffering;
            }

            // track current start and stop time for usage in LookupSubPic(), the interlocked operands guarantee proper cross-thread memory handling
            InterlockedExchange64(&mv_i64CurrentRenderStart, i64Start);
            InterlockedExchange64(&mv_i64CurrentRenderStop, i64Stop);

            __int64 i64CurrentStart;
            if (!mk_bDisableAnim && pNonVolaSubPicProvider->IsAnimated(pos)) {
                i64CurrentStart = i64Now;
                __int64 i64OneFramePastCurrent = i64Now + i64TimePerFrame;
                i64Now = (i64OneFramePastCurrent + 100000 < i64Stop) ? i64OneFramePastCurrent : i64Stop;// do not produce entire subtitle frames that last less than 10 ms
                pStatic->SetSegmentStart(i64Start);
                pStatic->SetSegmentStop(i64Stop);
#ifdef DSubPicTrace
                TRACE("Render animated SubPic: frame [%f, %f), segment [%f, %f)\n", static_cast<double>(i64CurrentStart) / 10000000.0, static_cast<double>(i64Now) / 10000000.0, static_cast<double>(i64Start) / 10000000.0, static_cast<double>(i64Stop) / 10000000.0);
#endif
            } else {
                i64CurrentStart = i64Start;
                i64Now = i64Stop;
#ifdef DSubPicTrace
                TRACE("Render unanimated SubPic: segment [%f, %f)\n", static_cast<double>(i64CurrentStart) / 10000000.0, static_cast<double>(i64Now) / 10000000.0);
#endif
            }

            {
                SubPicDesc spd;
                if (FAILED(pStatic->LockAndClearDirtyRect(&spd))) {
                    pStatic->Release();
                    goto BreakBuffering;
                }
                RECT r = {0, 0, 0, 0};
                HRESULT hr = pNonVolaSubPicProvider->Render(spd, i64CurrentStart, dNonVolaDetectedFrameRate, r);
#ifdef DSubPicTrace
                TRACE("SubPic rendered size: %d×%d\n", r.right - r.left, r.bottom - r.top);
                if (mv_i64Now > i64Now) {
                    TRACE("SubPic queue behind schedule\n");
                }
#endif
                pStatic->Unlock(r);
                // Render() is slow, so it is sensible to check mv_cBreakBuffering here
                if (mv_cBreakBuffering) {
                    pStatic->Release();
                    goto BreakBuffering;
                }
                pStatic->SetStart(i64CurrentStart);
                pStatic->SetStop(i64Now);

                if ((S_OK != hr) || (!(r.right - r.left) || !(r.bottom - r.top))) {// the SubPic can be empty on output sometimes
                    pStatic->Release();
                    if (FAILED(hr)) {// a real failure
                        ASSERT(0);
                        goto BreakBuffering;
                    }
                    goto TryNextPosRenewNow;
                }
            }

            CBSubPic* pDynamic = m_pSubPicAllocator->AllocDynamicSubPic();
            if (!pDynamic) {
                ASSERT(0);
                pStatic->Release();
                goto BreakBuffering;
            }

            HRESULT hr = pStatic->CopyTo(pDynamic);
            pStatic->Release();
            if (S_OK != hr) {
                ASSERT(0);
                pDynamic->Release();
                goto BreakBuffering;
            }

            m_Queue.Lock();
            m_Queue.Enqueue(pDynamic);// reference inherited
            m_Queue.Unlock();

            if (mv_cBreakBuffering) {
                goto BreakBuffering;
            }

            if (mv_cbWaitableHandleForQueueSet) {// the other thread can be unlocked and it may take the new entry that was pDynamic in m_Queue
                _InterlockedExchange8(&mv_cbWaitableHandleForQueueSet, false);// interlocked operand guarantees proper cross-thread memory handling
                SetEvent(m_hWaitableForQueue);
            }

            if (m_Queue.m_u8QueueElemCount >= m_u8MaxSubPic) {
                pNonVolaSubPicProvider->Unlock();
                pNonVolaSubPicProvider->Release();
                continue;// full buffer, so sleep
            }
            goto TryNextPosRenewNow;

BreakBuffering:
            if (mv_cbWaitableHandleForQueueSet) {// always make sure that the other thread isn't locked
                _InterlockedExchange8(&mv_cbWaitableHandleForQueueSet, false);// interlocked operand guarantees proper cross-thread memory handling
                SetEvent(m_hWaitableForQueue);
            }
            pNonVolaSubPicProvider->Unlock();
            pNonVolaSubPicProvider->Release();
            goto HandleBreakBuffering;
        }
    }

    return;
}

//
// CSubPicQueueNoThread
//
// CSubPicQueueImpl

__declspec(nothrow noalias) void CSubPicQueueNoThread::SetSubPicProvider(__inout CSubPicProviderImpl* pSubPicProvider)
{
    ASSERT(pSubPicProvider);

    CAutoLock cAutoLock(&m_csSubPicProvider);
    if (pSubPicProvider) {
        pSubPicProvider->AddRef();
    }
    if (CSubPicProviderImpl* pNonVolaSubPicProvider = reinterpret_cast<CSubPicProviderImpl*>(InterlockedExchangePointer(reinterpret_cast<void* volatile*>(&mv_pSubPicProvider), pSubPicProvider))) {// interlocked operand guarantees proper cross-thread memory handling
        pNonVolaSubPicProvider->Release();
    }
    // remove all allocated SubPics
    if (m_pSubPic) {
        m_pSubPic->Release();
        m_pSubPic = nullptr;
    }
    m_pSubPicAllocator->DeallocStaticSubPic();// if no SubPicProvider is set, the SubPicQueue can have no resources allocated in the SubPicAllocator, except when handling the destructor of the SubPicQueue
}

__declspec(nothrow noalias) void CSubPicQueueNoThread::SetFPS(__in double fps)
{
    mv_dDetectedFrameRate = fps;
}

__declspec(nothrow noalias) void CSubPicQueueNoThread::SetTime(__in __int64 i64Now)
{
    mv_i64Now = i64Now;
}

__declspec(nothrow noalias) void CSubPicQueueNoThread::InvalidateSubPic(__in __int64 i64Invalidate)
{
    CAutoLock cAutoLock(&m_csSubPicProvider);
    if (m_pSubPic) {
        m_pSubPic->Release();
        m_pSubPic = nullptr;
    }
}

__declspec(nothrow noalias restrict) CBSubPic* CSubPicQueueNoThread::LookupSubPic(__in __int64 i64Now, __in __int32 i32msWaitable)
{
    // get the non-volatile pointer to mv_pSubPicProvider
    m_csSubPicProvider.Lock();// if a change or release of mv_pSubPicProvider would happen, a fatal exception would follow
    CSubPicProviderImpl* pNonVolaSubPicProvider(mv_pSubPicProvider);
    if (!pNonVolaSubPicProvider) {
        m_csSubPicProvider.Unlock();
        return nullptr;
    }

    if (m_pSubPic) { // re-use if possible
        if ((m_pSubPic->GetStart() <= i64Now) && (i64Now < m_pSubPic->GetStop())) {
            m_pSubPic->AddRef();
            m_csSubPicProvider.Unlock();
            return m_pSubPic;
        }
        m_pSubPic->Release();// discard
        m_pSubPic = nullptr;
    }

    pNonVolaSubPicProvider->AddRef();
    m_csSubPicProvider.Unlock();
    pNonVolaSubPicProvider->Lock();// can absolutely not be combined with m_csSubPicProvider
    double dNonVolaDetectedFrameRate = mv_dDetectedFrameRate;
    POSITION pos = pNonVolaSubPicProvider->GetStartPosition(i64Now, dNonVolaDetectedFrameRate);
    if (pos) {
        __int64 i64Start = pNonVolaSubPicProvider->GetStart(pos, dNonVolaDetectedFrameRate);
        __int64 i64Stop = pNonVolaSubPicProvider->GetStop(pos, dNonVolaDetectedFrameRate);

        if ((i64Start <= i64Now) && (i64Now < i64Stop)) {
            if (pNonVolaSubPicProvider->IsAnimated(pos)) {
                i64Start = i64Now;
                i64Stop = i64Now + 1;
            }

            if (unsigned __int64 u64TextureSize = pNonVolaSubPicProvider->GetTextureSize(pos)) {// used for bitmapped subtitles that need fixed size textures
                m_pSubPicAllocator->SetCurSize(u64TextureSize);
            }

            if (CBSubPic* pDynamic = m_pSubPicAllocator->AllocDynamicSubPic()) {
                SubPicDesc spd;
                RECT r = {0, 0, 0, 0};

                if (m_pSubPicAllocator->IsDynamicWriteOnly()) {
                    CBSubPic* pStatic = m_pSubPicAllocator->AllocStaticSubPic();
                    if (!pStatic) {
                        ASSERT(0);
                        pDynamic->Release();
                        goto failed;
                    }

                    if (FAILED(pStatic->LockAndClearDirtyRect(&spd))) {
                        pDynamic->Release();
                        goto failed;
                    }
                    HRESULT hr = pNonVolaSubPicProvider->Render(spd, i64Start, dNonVolaDetectedFrameRate, r);
                    pStatic->Unlock(r);
                    pStatic->SetStart(i64Start);
                    pStatic->SetStop(i64Stop);

                    if ((S_OK != hr) || (!(r.right - r.left) || !(r.bottom - r.top))) {// the SubPic can be empty on output sometimes
                        ASSERT(SUCCEEDED(hr));// in case of a real failure
                        pStatic->Release();
                        pDynamic->Release();
                        goto failed;
                    }

                    hr = pStatic->CopyTo(pDynamic);
                    pStatic->Release();
                    if (FAILED(hr)) {
                        ASSERT(0);
                        pDynamic->Release();
                        goto failed;
                    }
                } else {
                    if (FAILED(pDynamic->LockAndClearDirtyRect(&spd))) {
                        pDynamic->Release();
                        goto failed;
                    }

                    HRESULT hr = pNonVolaSubPicProvider->Render(spd, i64Start, dNonVolaDetectedFrameRate, r);
                    pDynamic->Unlock(r);
                    pDynamic->SetStart(i64Start);
                    pDynamic->SetStop(i64Stop);

                    if ((S_OK != hr) || (!(r.right - r.left) || !(r.bottom - r.top))) {// the SubPic can be empty on output sometimes
                        ASSERT(SUCCEEDED(hr));// in case of a real failure
                        pDynamic->Release();
                        goto failed;
                    }
                }
                m_pSubPic = pDynamic;
                pDynamic->AddRef();
            }
        }
    }
failed:
    pNonVolaSubPicProvider->Unlock();
    pNonVolaSubPicProvider->Release();

    return m_pSubPic;// can return nullptr
}

__declspec(nothrow noalias) void CSubPicQueueNoThread::GetStats(__out SubPicQueueStats* pStats)
{
    ASSERT(pStats);

    CAutoLock cAutoLock(&m_csSubPicProvider);
    __int64 i64Start = 0, i64Stop = 0;
    unsigned __int8 u8SubPics = 0;
    if (m_pSubPic) {
        u8SubPics = 1;
        __int64 i64Now = mv_i64Now;
        i64Start = m_pSubPic->GetStart() - i64Now;
        i64Stop = m_pSubPic->GetStop() - i64Now;
    }
    pStats->i64Start = i64Start;
    pStats->i64Stop = i64Stop;
    pStats->u8SubPics = u8SubPics;
}

__declspec(nothrow noalias) void CSubPicQueueNoThread::GetStats(__in unsigned __int8 u8SubPic, __out_ecount(2) __int64 pStartStop[2])
{
    ASSERT(!u8SubPic);
    ASSERT(pStartStop);

    CAutoLock cAutoLock(&m_csSubPicProvider);
    __int64 i64Start = 0, i64Stop = 0;
    if (m_pSubPic) {
        i64Start = m_pSubPic->GetStart();
        i64Stop = m_pSubPic->GetStop();
    }
    pStartStop[0] = i64Start;
    pStartStop[1] = i64Stop;
}

#ifdef DSubPicTrace
#ifndef _DEBUG
#error DSubPicTrace can only be defined for when debugging
#endif
#endif
