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

#pragma once

#include "SubPicProviderImpl.h"
#include <intrin.h>

#pragma pack(push, 1)// the struct is embedded with decent alignment and is internally packed as well
struct SPArrayQueue {
    __declspec(nothrow noalias) __forceinline SPArrayQueue();

    __declspec(nothrow noalias) __forceinline void ReleaseOnAllDestructorType();                            // this should be called before destruction to Release() all
    __declspec(nothrow noalias) __forceinline void ReleaseOnAllAndReset();                                  // Release() all and reset the queue
    __declspec(nothrow noalias) __forceinline void MoveContentToOther(SPArrayQueue* pOther);                // move the queue content to another queue and reset this one

    __declspec(nothrow noalias) __forceinline void Enqueue(CBSubPic* pItem);                                // adds item to the queue's end
    __declspec(nothrow noalias) __forceinline void EnqueueReverse(CBSubPic* pItem);                         // adds item to the queue's beginning

    __declspec(nothrow noalias restrict) __forceinline CBSubPic* Dequeue();                                 // returns item from the queue's beginning
    // Dequeue() can be split into these two actions:
    __declspec(nothrow noalias restrict) __forceinline CBSubPic* ReadFirstElem();                           // reads item from the queue's beginning
    __declspec(nothrow noalias) __forceinline void DiscardFirstElem();                                      // discards item from the queue's beginning

    __declspec(nothrow noalias restrict) __forceinline CBSubPic* ReadSecondElem();                          // reads the next item from the queue's beginning
    __declspec(nothrow noalias restrict) __forceinline CBSubPic* ReadLastElem();                            // reads item from the queue's end
    __declspec(nothrow noalias restrict) __forceinline CBSubPic* ReadElemAtOffset(unsigned __int8 u8Offset);// reads item from the queue's beginning plus offset

    __declspec(nothrow noalias) __forceinline void Lock();                                                  // compare-and-swap lock, this can deadlock if not used responsibly
    __declspec(nothrow noalias) __forceinline void Unlock();                                                // this should follow just a few instructions after Lock()

    CBSubPic* m_paData[64];// the actual data array
    char volatile mv_cLock;// 1 is unlocked, 0 is locked
    // if the element count becomes larger than 128, change the type of the 8-bit unsigned integer values controlling the queue
    unsigned __int8 m_u8QueueListBeginning, m_u8QueueListEnd;// numbered location of the start and end
    unsigned __int8 m_u8QueueElemCount;// tracked number of elements
};
#pragma pack(pop)

// SPArrayQueue, the constructor and destructor need to be here because these get inlined externally
__declspec(nothrow noalias) __forceinline SPArrayQueue::SPArrayQueue()
{
    *reinterpret_cast<__int32 volatile*>(&mv_cLock) = 1;// four bytes at once
    static_assert((sizeof(m_u8QueueElemCount) == 1) && (sizeof(m_u8QueueListEnd) == 1) && (sizeof(m_u8QueueListBeginning) == 1), "the values controlling the queue changed in size, adjust all functions accordingly");
    DEBUG_ONLY(memset(m_paData, -1, sizeof(m_paData)));// error check: make sure the pointers are invalid
}
__declspec(nothrow noalias) __forceinline void SPArrayQueue::ReleaseOnAllDestructorType()
{
    ASSERT((m_u8QueueElemCount == (m_u8QueueListEnd - m_u8QueueListBeginning & 63)) || ((m_u8QueueElemCount == 64) && !(m_u8QueueListEnd - m_u8QueueListBeginning & 63)));

    unsigned __int8 u8QueueElemCount = m_u8QueueElemCount;
    if (u8QueueElemCount) {
        unsigned __int8 u8QueueListBeginning = m_u8QueueListBeginning;
        do {
            ULONG u = m_paData[u8QueueListBeginning]->Release();
            ASSERT(!u);
            UNREFERENCED_PARAMETER(u);
            DEBUG_ONLY(m_paData[u8QueueListBeginning] = reinterpret_cast<CBSubPic*>(MAXSIZE_T));// error check: make sure the pointer is invalid
            u8QueueListBeginning = u8QueueListBeginning + 1 & 63;
        } while (--u8QueueElemCount);
    }
    DEBUG_ONLY(m_u8QueueListBeginning = m_u8QueueListEnd = m_u8QueueElemCount = MAXUINT8);// error check: make sure the values controlling the queue are invalid
}

struct SubPicQueueStats {
    __int64 i64Start, i64Stop;
    unsigned __int8 u8SubPics;// keep start and stop in the current placement, various parts re-use them as a pair
};

class __declspec(uuid("C8334466-CD1E-4AD1-9D2D-8EE8519BD180") novtable) CSubPicQueueImpl
    : public IUnknown
{
protected:
    virtual __declspec(nothrow noalias) __forceinline ~CSubPicQueueImpl() {// polymorphic class implementing IUnknown, so a virtual destructor
        CSubPicProviderImpl* pNonVolaSubPicProvider(mv_pSubPicProvider);
        if (pNonVolaSubPicProvider) {
            pNonVolaSubPicProvider->Release();
        }
    }
    CSubPicAllocatorImpl* const m_pSubPicAllocator;// doesn't hold a reference
    __int64 volatile mv_i64Now;
    double volatile mv_dDetectedFrameRate;
    CCritSec m_csSubPicProvider;
    CSubPicProviderImpl* volatile mv_pSubPicProvider;
    ULONG volatile mv_ulReferenceCount;

public:
    // IUnknown
    __declspec(nothrow noalias) STDMETHODIMP QueryInterface(REFIID riid, __deref_out void** ppv);
    __declspec(nothrow noalias) STDMETHODIMP_(ULONG) AddRef();
    __declspec(nothrow noalias) STDMETHODIMP_(ULONG) Release();

    virtual __declspec(nothrow noalias) void SetSubPicProvider(__inout CSubPicProviderImpl* pSubPicProvider) = 0;
    virtual __declspec(nothrow noalias) void SetFPS(__in double fps) = 0;
    virtual __declspec(nothrow noalias) void SetTime(__in __int64 i64Now) = 0;
    virtual __declspec(nothrow noalias) void InvalidateSubPic(__in __int64 i64Invalidate) = 0;
    virtual __declspec(nothrow noalias restrict) CBSubPic* LookupSubPic(__in __int64 i64Now, __in __int32 i32msWaitable = 0) = 0;
    virtual __declspec(nothrow noalias) void GetStats(__out SubPicQueueStats* pStats) = 0;
    virtual __declspec(nothrow noalias) void GetStats(__in unsigned __int8 u8SubPic, __out_ecount(2) __int64 pStartStop[2]) = 0;

    __declspec(nothrow noalias) __forceinline CSubPicQueueImpl(__inout CSubPicAllocatorImpl* pAllocator, __in double dDetectedFrameRate)
        : mv_ulReferenceCount(1)
        , mv_i64Now(0)
        , mv_dDetectedFrameRate(dDetectedFrameRate)
        , mv_pSubPicProvider(nullptr)
        , m_pSubPicAllocator(pAllocator) {
        ASSERT(pAllocator);
        ASSERT(dDetectedFrameRate);
    }
};

class CSubPicQueue : public CSubPicQueueImpl
{
    // flags for mv_cBreakBuffering
#define FSUBPICQUEUE_BREAKBUFFERING 1
#define FSUBPICQUEUE_QUIT 2// always combined with the other flag if set

    __declspec(nothrow noalias) __forceinline ~CSubPicQueue() {
        // break the loop, don't remove existing subtitles, the interlocked operand guarantees proper cross-thread memory handling
        InterlockedExchange64(&mv_i64Invalidate, MAXINT64);
        if (m_hQueueThread) { // close open thread
            _InterlockedExchange8(&mv_cBreakBuffering, FSUBPICQUEUE_BREAKBUFFERING | FSUBPICQUEUE_QUIT);// interlocked operand guarantees proper cross-thread memory handling
            SetEvent(m_hEvtTime);// make sure the thread is active
            if (WaitForSingleObject(m_hQueueThread, 10000) == WAIT_TIMEOUT) {
                ASSERT(0);
                TerminateThread(m_hQueueThread, 0xDEAD);
            }
            CloseHandle(m_hQueueThread);
        }
        if (m_hEvtTime) {
            CloseHandle(m_hEvtTime);
        }
        if (m_hWaitableForQueue) {
            CloseHandle(m_hWaitableForQueue);
        }
        m_Queue.ReleaseOnAllDestructorType();// remove the references kept in the queue
    }

#ifdef _WIN64
    __declspec(align(8))
#else
    __declspec(align(4))// I tried, but 'align' only takes immediates
#endif
    SPArrayQueue m_Queue;// needs four trailing bytes to re-align to pointer alignment
    char volatile mv_cBreakBuffering, mv_cbWaitableHandleForQueueSet;// initialized as a set in SetSubPicProvider()
    unsigned __int8 const m_u8MaxSubPic;
    bool const mk_bDisableAnim;
    HANDLE m_hQueueThread, m_hEvtTime, m_hWaitableForQueue;
    __int64 volatile mv_i64CurrentRenderStart, mv_i64CurrentRenderStop;
    __int64 volatile mv_i64Invalidate;
    __int64 volatile mv_i64TimePerFrame;

    static __declspec(nothrow noalias) DWORD WINAPI QueueThreadStatic(__in LPVOID lpParam);
    __declspec(nothrow noalias) __forceinline void QueueThread();
public:

    // CSubPicQueueImpl
    __declspec(nothrow noalias) void SetSubPicProvider(__inout CSubPicProviderImpl* pSubPicProvider);
    __declspec(nothrow noalias) void SetFPS(__in double fps);
    __declspec(nothrow noalias) void SetTime(__in __int64 i64Now);
    __declspec(nothrow noalias) void InvalidateSubPic(__in __int64 i64Invalidate);
    __declspec(nothrow noalias restrict) CBSubPic* LookupSubPic(__in __int64 i64Now, __in __int32 i32msWaitable);
    __declspec(nothrow noalias) void GetStats(__out SubPicQueueStats* pStats);
    __declspec(nothrow noalias) void GetStats(__in unsigned __int8 u8SubPic, __out_ecount(2) __int64 pStartStop[2]);

    __declspec(nothrow noalias) __forceinline CSubPicQueue(__inout CSubPicAllocatorImpl* pAllocator, __in double dDetectedFrameRate, __in unsigned __int8 u8MaxSubPic, __in bool bDisableAnim)
        : CSubPicQueueImpl(pAllocator, dDetectedFrameRate)
        , mv_i64TimePerFrame(static_cast<__int64>(10000000.0 / dDetectedFrameRate + 0.5))
        , m_u8MaxSubPic(u8MaxSubPic)
        , m_hQueueThread(nullptr)
        , mk_bDisableAnim(bDisableAnim) {// purposely not initialized here: mv_i64CurrentRenderStart, mv_i64CurrentRenderStop, mv_cBreakBuffering and mv_cbWaitableHandleForQueueSet are to be initialized in SetSubPicProvider(), mv_i64Invalidate is an accessory, it given a proper value when mv_cBreakBuffering is set in various functions
        ASSERT(u8MaxSubPic && (u8MaxSubPic <= 64));// the queue can't deal with a small buffer and the queue system has 64 slots

        m_hEvtTime = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        m_hWaitableForQueue = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        ASSERT(m_hEvtTime && m_hWaitableForQueue);// the handles are simple, they should never fail
    }
};

class CSubPicQueueNoThread : public CSubPicQueueImpl
{
    __declspec(nothrow noalias) __forceinline ~CSubPicQueueNoThread() {
        if (m_pSubPic) {
            m_pSubPic->Release();
        }
    }

    // note: due to not having to deal with multi-threading as much, access to the volatile parts of CSubPicQueueImpl is relaxed compared to CSubPicQueue
    CBSubPic* m_pSubPic;// protected by m_csSubPicProvider in general
public:
    // CSubPicQueueImpl
    __declspec(nothrow noalias) void SetSubPicProvider(__inout CSubPicProviderImpl* pSubPicProvider);
    __declspec(nothrow noalias) void SetFPS(__in double fps);
    __declspec(nothrow noalias) void SetTime(__in __int64 i64Now);
    __declspec(nothrow noalias) void InvalidateSubPic(__in __int64 i64Invalidate);
    __declspec(nothrow noalias restrict) CBSubPic* LookupSubPic(__in __int64 i64Now, __in __int32 i32msWaitable);
    __declspec(nothrow noalias) void GetStats(__out SubPicQueueStats* pStats);
    __declspec(nothrow noalias) void GetStats(__in unsigned __int8 u8SubPic, __out_ecount(2) __int64 i64StartStop[2]);

    __declspec(nothrow noalias) __forceinline CSubPicQueueNoThread(__out CSubPicAllocatorImpl* pAllocator, __in double dDetectedFrameRate)
        : CSubPicQueueImpl(pAllocator, dDetectedFrameRate)
        , m_pSubPic(nullptr) {}
};
