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

#pragma once

#include "DX9AllocatorPresenter.h"
#include "OuterEVR.h"
#include "IPinHook.h"
#include <mfapi.h>
#include <evr9.h>

namespace DSObjects
{
#pragma pack(push, 1)// the struct is embedded with decent alignment and is internally packed as well
    struct MSArrayQueue {
        __declspec(nothrow noalias) __forceinline MSArrayQueue();

        __declspec(nothrow noalias) __forceinline void ReleaseOnAllDestructorType();            // this should be called before destruction to Release() all
        __declspec(nothrow noalias) __forceinline void ReleaseOnAllAndReset();                  // Release() all and reset the queue
        __declspec(nothrow noalias) __forceinline void MoveContentToOther(MSArrayQueue* pOther);// move the queue content to another queue and reset this one

        __declspec(nothrow noalias) __forceinline void Enqueue(IMFSample* pItem);               // adds item to the queue's end
        __declspec(nothrow noalias) __forceinline void EnqueueReverse(IMFSample* pItem);        // adds item to the queue's beginning

        __declspec(nothrow noalias restrict) __forceinline IMFSample* Dequeue();                // returns item from the queue's beginning

        __declspec(nothrow noalias) __forceinline void Lock();                                  // compare-and-swap lock, this can deadlock if not used responsibly
        __declspec(nothrow noalias) __forceinline void Unlock();                                // this should follow just a few instructions after Lock()

        IMFSample* m_paData[16];// the actual data array
        char volatile mv_cLock;// 1 is unlocked, 0 is locked
        // if the element count becomes larger than 128, change the type of the 8-bit unsigned integer values controlling the queue, along with the code for the functions
        unsigned __int8 m_u8QueueListBeginning, m_u8QueueListEnd;// numbered location of the start and end
        unsigned __int8 m_u8QueueElemCount;// tracked number of elements
    };
#pragma pack(pop)

    // MSArrayQueue, the constructor needs to be here because it gets inlined externally
    __declspec(nothrow noalias) __forceinline MSArrayQueue::MSArrayQueue()
    {
        *reinterpret_cast<__int32 volatile*>(&mv_cLock) = 1;// four bytes at once
        static_assert((sizeof(m_u8QueueElemCount) == 1) && (sizeof(m_u8QueueListEnd) == 1) && (sizeof(m_u8QueueListBeginning) == 1), "the values controlling the queue changed in size, adjust all functions accordingly");
        DEBUG_ONLY(memset(m_paData, -1, sizeof(m_paData)));// error check: make sure the pointers are invalid
    }

    class CEVRAllocatorPresenter :
        public CDX9AllocatorPresenter,
        public IMFGetService,
        public IMFTopologyServiceLookupClient,
        public IMFVideoDeviceID,
        public IMFVideoPresenter,
        public IDirect3DDeviceManager9,
        public IMFAsyncCallback,
        public IQualProp,
        public IMFRateSupport,
        public IMFVideoDisplayControl,
        public IEVRTrustedVideoPlugin
    /*  public IMFVideoPositionMapper,      // Non mandatory EVR Presenter Interfaces (see later...)
    */
    {
        __declspec(nothrow noalias) __forceinline ~CEVRAllocatorPresenter() {
            if (m_nRenderState != Shutdown) {
                m_bEvtFlush = m_bEvtQuit = true;
                if (m_hEvtFlush) {
                    SetEvent(m_hEvtFlush);
                }
                if (m_hEvtQuit) {
                    SetEvent(m_hEvtQuit);
                }

                if (m_hRenderThread) {
                    if (WaitForSingleObject(m_hRenderThread, 10000) == WAIT_TIMEOUT) {
                        ASSERT(0);
                        TerminateThread(m_hRenderThread, 0xDEAD);
                    }
                    CloseHandle(m_hRenderThread);
                }
                if (m_hMixerThread) {
                    if (WaitForSingleObject(m_hMixerThread, 10000) == WAIT_TIMEOUT) {
                        ASSERT(0);
                        TerminateThread(m_hMixerThread, 0xDEAD);
                    }
                    CloseHandle(m_hMixerThread);
                }
                if (m_hEvtFlush) {
                    CloseHandle(m_hEvtFlush);
                }
                if (m_hEvtQuit) {
                    CloseHandle(m_hEvtQuit);
                }

                TRACE(L"EVR: Worker threads stopped by class destructor, abnormal event\n");
            }

            ULONG u;
            if (m_pCurrentDisplayedSample) {
                u = m_pCurrentDisplayedSample->Release();
                ASSERT(!u);
            }
            // remove the references kept in the queues
            m_ScheduledSamplesQueue.ReleaseOnAllDestructorType();
            m_FreeSamplesQueue.ReleaseOnAllDestructorType();

            // no assertion on the reference count
            if (m_pMixerType) {
                m_pMixerType->Release();
            }
            // the external EVR will always call ReleaseServicePointers() when closing, these three are only here for safety
            if (m_pMixer) {
                m_pMixer->Release();
            }
            if (m_pSink) {
                m_pSink->Release();
            }
            if (m_pClock) {
                m_pClock->Release();
            }

            if (m_pD3DManager) {
                u = m_pD3DManager->Release();
                ASSERT(!u);
            }
            //if (m_pVideoDecoderService) {
            //    u = m_pVideoDecoderService->Release();
            //    ASSERT(!u);
            //}
            //if (m_pVideoProcessorService) {
            //    u = m_pVideoProcessorService->Release();
            //    ASSERT(!u);
            //}

            UnhookNewSegmentAndReceive();

            if (m_hAVRTLib) {
                EXECUTE_ASSERT(FreeLibrary(m_hAVRTLib));
            }
            if (m_hEVRLib) {
                EXECUTE_ASSERT(FreeLibrary(m_hEVRLib));
            }
        }

        __int64                                 m_i64LastSampleTime;
        double                                  m_dModeratedTime;
        double                                  m_dModeratedTimeLast;
        double                                  m_dModeratedClockLast;

        double                                  m_dStarvationClock;
        __int64                                 m_i64LastScheduledSampleTime;
        double                                  m_dLastScheduledSampleTimeFP;
        __int64                                 m_i64LastScheduledUncorrectedSampleTime;
        double                                  m_dMaxSampleDuration;
        double                                  m_dLastSampleOffset;
        double                                  m_adVSyncOffsetHistory[2];
        double                                  m_dLastPredictedSync;

        __declspec(align(8)) MFVideoArea        m_mfvCurrentFrameArea;// explicit alignment doesn't really change much here

#ifdef _WIN64
        __declspec(align(8))
#else
        __declspec(align(4))// I tried, but 'align' only takes immediates
#endif
        MSArrayQueue                            m_FreeSamplesQueue;// needs 4 trailing bytes to re-align
        UINT                                    m_nResetToken;
        MSArrayQueue                            m_ScheduledSamplesQueue;// needs 4 trailing bytes to re-align
        COLORREF                                m_BorderColor;

        IMFSample*                              m_pCurrentDisplayedSample;
    public:
        COuterEVR                               m_OuterEVR;// warning: m_OuterEVR does not hold a reference inside this class, it's given away in CFGFilterVideoRenderer::Create() and m_OuterEVR is in command of doing the last Release() of this class
    private:
        CCritSec                                m_csExternalMixerLock;

        IMFMediaType*                           m_pMixerType;
        IMFTransform*                           m_pMixer;
        IMediaEventSink*                        m_pSink;
        IMFClock*                               m_pClock;
        IDirect3DDeviceManager9*                m_pD3DManager;

        HINSTANCE                               m_hEVRLib, m_hAVRTLib;
        // evr.dll
        typedef HRESULT(WINAPI* MFCreateVideoSampleFromSurfacePtr)(__in IUnknown* pUnkSurface, __out IMFSample** ppSample);
        // AVRT.dll
        typedef HANDLE(WINAPI* AvSetMmThreadCharacteristicsWPtr)(__in LPCTSTR TaskName, __inout LPDWORD TaskIndex);
        typedef BOOL (WINAPI* AvSetMmThreadPriorityPtr)(__in HANDLE AvrtHandle, __in  AVRT_PRIORITY Priority);
        typedef BOOL (WINAPI* AvRevertMmThreadCharacteristicsPtr)(__in HANDLE AvrtHandle);
        // mf.dll
        //typedef HRESULT (WINAPI *MFCreatePresentationClockPtr)(IMFPresentationClock **ppPresentationClock);
        //typedef HRESULT (WINAPI *MFCreateMediaTypePtr)(__deref_out IMFMediaType **ppMFType);
        //typedef HRESULT (WINAPI *MFInitMediaTypeFromAMMediaTypePtr)(__in IMFMediaType *pMFType, __in const AM_MEDIA_TYPE *pAMType throw()
        //typedef HRESULT (WINAPI *MFInitAMMediaTypeFromMFMediaTypePtr)(__in IMFMediaType *pMFType, __in GUID guidFormatBlockType, __inout AM_MEDIA_TYPE *pAMType);

        // evr.dll
        MFCreateVideoSampleFromSurfacePtr       m_fnMFCreateVideoSampleFromSurface;
        // AVRT.dll
        AvSetMmThreadCharacteristicsWPtr        m_fnAvSetMmThreadCharacteristicsW;
        AvSetMmThreadPriorityPtr                m_fnAvSetMmThreadPriority;
        AvRevertMmThreadCharacteristicsPtr      m_fnAvRevertMmThreadCharacteristics;

        HANDLE                                  m_hEvtQuit, m_hEvtFlush;// Stop rendering thread event + Discard all buffers, these two are used as a pair for the mixer thread
        HANDLE                                  m_hRenderThread;
        HANDLE                                  m_hMixerThread;
        HANDLE                                  m_hEvtReset;
        HANDLE                                  m_hEvtRenegotiate;

        ULONG_PTR                               m_nStepCount;

        MFCLOCK_STATE                           m_mfcLastClockState;
        enum RENDER_STATE {// based on FILTER_STATE
            Stopped = State_Stopped,
            Paused = State_Paused,
            Started = State_Running,
            Shutdown = State_Running + 1
        }                                       m_nRenderState;
        MFVideoRenderPrefs                      m_dwVideoRenderPrefs;

        // for IQualProp
        UINT                                    m_pcFramesDropped;
        UINT                                    m_pcFramesDrawn;// Retrieves the number of frames drawn since streaming started
        UINT                                    m_piAvg;
        UINT                                    m_piDev;

        bool                                    m_bWaitingSample;
        bool                                    m_bPendingMediaFinished;
        bool                                    m_bUseInternalTimer;
        bool                                    m_bEvtQuit;
        bool                                    m_bEvtFlush;
        bool                                    m_bLastSampleOffsetValid;
        bool                                    m_bSignaledStarvation;
        unsigned __int8                         m_u8VSyncOffsetHistoryPos;
        unsigned __int8                         m_u8FrameTimeCorrection;

        __declspec(nothrow noalias) void __forceinline GetMixerThread();
        static __declspec(nothrow noalias) DWORD WINAPI GetMixerThreadStatic(LPVOID lpParam);
        __declspec(nothrow noalias) void __forceinline RenderThread();
        static __declspec(nothrow noalias) DWORD WINAPI PresentThread(LPVOID lpParam);

        __declspec(nothrow noalias) double GetClockTime(double dPerformanceCounter);
        __declspec(nothrow noalias) __forceinline HRESULT CheckShutdown() const;
        __declspec(nothrow noalias) __forceinline void CompleteFrameStep(bool bCancel);
        __declspec(nothrow noalias) __forceinline void CheckWaitingSampleFromMixer();
        __declspec(nothrow noalias) void RemoveAllSamples();
        __declspec(nothrow noalias) void MoveToFreeList(IMFSample* pSample, bool bTail);
        __declspec(nothrow noalias) void MoveToScheduledList(IMFSample* pSample, bool bSorted);
        __declspec(nothrow noalias) __forceinline void FlushSamples();
        __declspec(nothrow noalias) void FlushSamplesInternal();
        __declspec(nothrow noalias) HRESULT RenegotiateMediaType();
    public:
        // IUnknown
        __declspec(nothrow noalias) STDMETHODIMP QueryInterface(REFIID riid, __deref_out void** ppv);
        __declspec(nothrow noalias) STDMETHODIMP_(ULONG) AddRef();
        __declspec(nothrow noalias) STDMETHODIMP_(ULONG) Release();

        // CSubPicAllocatorPresenterImpl
        __declspec(nothrow noalias) void ResetDevice();

        // IMFClockStateSink
        __declspec(nothrow noalias) STDMETHODIMP OnClockStart(MFTIME hnsSystemTime, LONGLONG llClockStartOffset);
        __declspec(nothrow noalias) STDMETHODIMP OnClockStop(MFTIME hnsSystemTime);
        __declspec(nothrow noalias) STDMETHODIMP OnClockPause(MFTIME hnsSystemTime);
        __declspec(nothrow noalias) STDMETHODIMP OnClockRestart(MFTIME hnsSystemTime);
        __declspec(nothrow noalias) STDMETHODIMP OnClockSetRate(MFTIME hnsSystemTime, float flRate);

        // IQualProp (EVR statistics window)
        __declspec(nothrow noalias) STDMETHODIMP get_FramesDroppedInRenderer(THIS_ __out int* pcFrames);
        __declspec(nothrow noalias) STDMETHODIMP get_FramesDrawn(THIS_ __out int* pcFramesDrawn);
        __declspec(nothrow noalias) STDMETHODIMP get_AvgFrameRate(THIS_ __out int* piAvgFrameRate);
        __declspec(nothrow noalias) STDMETHODIMP get_Jitter(THIS_ __out int* iJitter);
        __declspec(nothrow noalias) STDMETHODIMP get_AvgSyncOffset(THIS_ __out int* piAvg);
        __declspec(nothrow noalias) STDMETHODIMP get_DevSyncOffset(THIS_ __out int* piDev);

        // IMFRateSupport
        __declspec(nothrow noalias) STDMETHODIMP GetSlowestRate(MFRATE_DIRECTION eDirection, BOOL fThin, __RPC__out float* pflRate);
        __declspec(nothrow noalias) STDMETHODIMP GetFastestRate(MFRATE_DIRECTION eDirection, BOOL fThin, __RPC__out float* pflRate);
        __declspec(nothrow noalias) STDMETHODIMP IsRateSupported(BOOL fThin, float flRate, __RPC__inout_opt float* pflNearestSupportedRate);

        // IMFVideoPresenter
        __declspec(nothrow noalias) STDMETHODIMP ProcessMessage(MFVP_MESSAGE_TYPE eMessage, ULONG_PTR ulParam);
        __declspec(nothrow noalias) STDMETHODIMP GetCurrentMediaType(__deref_out IMFVideoMediaType** ppMediaType);

        // IMFTopologyServiceLookupClient
        __declspec(nothrow noalias) STDMETHODIMP InitServicePointers(__in IMFTopologyServiceLookup* pLookup);
        __declspec(nothrow noalias) STDMETHODIMP ReleaseServicePointers();

        // IMFVideoDeviceID
        __declspec(nothrow noalias) STDMETHODIMP GetDeviceID(__out IID* pDeviceID);

        // IMFGetService
        __declspec(nothrow noalias) STDMETHODIMP GetService(__RPC__in REFGUID guidService, __RPC__in REFIID riid, __RPC__deref_out_opt LPVOID* ppvObject);

        // IMFAsyncCallback
        __declspec(nothrow noalias) STDMETHODIMP GetParameters(__RPC__out DWORD* pdwFlags, __RPC__out DWORD* pdwQueue);
        __declspec(nothrow noalias) STDMETHODIMP Invoke(__RPC__in_opt IMFAsyncResult* pAsyncResult);

        // IMFVideoDisplayControl
        __declspec(nothrow noalias) STDMETHODIMP GetNativeVideoSize(__RPC__inout_opt SIZE* pszVideo, __RPC__inout_opt SIZE* pszARVideo);
        __declspec(nothrow noalias) STDMETHODIMP GetIdealVideoSize(__RPC__inout_opt SIZE* pszMin, __RPC__inout_opt SIZE* pszMax);
        __declspec(nothrow noalias) STDMETHODIMP SetVideoPosition(__RPC__in_opt MFVideoNormalizedRect const* pnrcSource, __RPC__in_opt LPRECT const prcDest);
        __declspec(nothrow noalias) STDMETHODIMP GetVideoPosition(__RPC__out MFVideoNormalizedRect* pnrcSource, __RPC__out LPRECT prcDest);
        __declspec(nothrow noalias) STDMETHODIMP SetAspectRatioMode(DWORD dwAspectRatioMode);
        __declspec(nothrow noalias) STDMETHODIMP GetAspectRatioMode(__RPC__out DWORD* pdwAspectRatioMode);
        __declspec(nothrow noalias) STDMETHODIMP SetVideoWindow(__RPC__in HWND hwndVideo);
        __declspec(nothrow noalias) STDMETHODIMP GetVideoWindow(__RPC__deref_out_opt HWND* phwndVideo);
        __declspec(nothrow noalias) STDMETHODIMP RepaintVideo();
        __declspec(nothrow noalias) STDMETHODIMP GetCurrentImage(__RPC__inout BITMAPINFOHEADER* pBih, __RPC__deref_out_ecount_full_opt(*pcbDib) BYTE** pDib, __RPC__out DWORD* pcbDib, __RPC__inout_opt LONGLONG* pTimeStamp);
        __declspec(nothrow noalias) STDMETHODIMP SetBorderColor(COLORREF Clr);
        __declspec(nothrow noalias) STDMETHODIMP GetBorderColor(__RPC__out COLORREF* pClr);
        __declspec(nothrow noalias) STDMETHODIMP SetRenderingPrefs(DWORD dwRenderFlags);
        __declspec(nothrow noalias) STDMETHODIMP GetRenderingPrefs(__RPC__out DWORD* pdwRenderFlags);
        __declspec(nothrow noalias) STDMETHODIMP SetFullscreen(BOOL fFullscreen);
        __declspec(nothrow noalias) STDMETHODIMP GetFullscreen(__RPC__out BOOL* pfFullscreen);

        // IEVRTrustedVideoPlugin
        __declspec(nothrow noalias) STDMETHODIMP IsInTrustedVideoMode(BOOL* pYes);
        __declspec(nothrow noalias) STDMETHODIMP CanConstrict(BOOL* pYes);
        __declspec(nothrow noalias) STDMETHODIMP SetConstriction(DWORD dwKPix);
        __declspec(nothrow noalias) STDMETHODIMP DisableImageExport(BOOL bDisable);

        // IDirect3DDeviceManager9
        __declspec(nothrow noalias) STDMETHODIMP ResetDevice(__in IDirect3DDevice9* pDevice, __in UINT resetToken);
        __declspec(nothrow noalias) STDMETHODIMP OpenDeviceHandle(__out HANDLE* phDevice);
        __declspec(nothrow noalias) STDMETHODIMP CloseDeviceHandle(__in HANDLE hDevice);
        __declspec(nothrow noalias) STDMETHODIMP TestDevice(__in HANDLE hDevice);
        __declspec(nothrow noalias) STDMETHODIMP LockDevice(__in HANDLE hDevice, __deref_out IDirect3DDevice9** ppDevice, __in BOOL fBlock);
        __declspec(nothrow noalias) STDMETHODIMP UnlockDevice(__in HANDLE hDevice, __in BOOL fSaveState);
        __declspec(nothrow noalias) STDMETHODIMP GetVideoService(__in HANDLE hDevice, __in REFIID riid, __deref_out void** ppService);

        __declspec(nothrow noalias) __forceinline CEVRAllocatorPresenter(__in HWND hWnd, __inout CStringW* pstrError)
            : CDX9AllocatorPresenter(hWnd, pstrError, true)
            , m_u8FrameTimeCorrection(0)
            , m_nResetToken(0)
            , m_hRenderThread(nullptr)
            , m_hMixerThread(nullptr)
            , m_hEvtFlush(nullptr)
            , m_hEvtQuit(nullptr)
            , m_hEvtReset(nullptr)
            , m_hEvtRenegotiate(nullptr)
            , m_bEvtQuit(0)
            , m_bEvtFlush(0)
            , m_i64LastSampleTime(0)
            , m_dModeratedTime(0.0)
            , m_dModeratedTimeLast(-1.0)
            , m_dModeratedClockLast(-1.0)
            , m_nRenderState(Shutdown)
            , m_bUseInternalTimer(false)
            , m_bPendingMediaFinished(false)
            , m_bWaitingSample(false)
            , m_pCurrentDisplayedSample(nullptr)
            , m_nStepCount(0)
            , m_dwVideoRenderPrefs()
            , m_BorderColor(RGB(0, 0, 0))
            , m_bSignaledStarvation(false)
            , m_dStarvationClock(0.0)
            , m_i64LastScheduledSampleTime(-1)
            , m_i64LastScheduledUncorrectedSampleTime(-1)
            , m_dMaxSampleDuration(0.0)
            , m_dLastSampleOffset(0.0)
            , m_u8VSyncOffsetHistoryPos(0)
            , m_bLastSampleOffsetValid(false)
            , m_pcFramesDropped(0)
            , m_pcFramesDrawn(0)
            , m_piAvg(0)
            , m_piDev(0)
            , m_hEVRLib(LoadLibraryW(L"evr.dll"))
            , m_hAVRTLib(LoadLibraryW(L"AVRT.dll"))
#pragma warning(disable: 4351)// the standard C4351 warning when default initializing arrays is irrelevant
            , m_adVSyncOffsetHistory()
            , m_pMixerType(nullptr)
            , m_pMixer(nullptr)
            , m_pSink(nullptr)
            , m_pClock(nullptr)
            , m_pD3DManager(nullptr) {
            ASSERT(pstrError);

            if (!pstrError->IsEmpty()) {// CDX9AllocatorPresenter() failed
                return;
            }

            if (!m_hDXVA2Lib) {// duplicate in CDX9AllocatorPresenter constructor only tests for dxva2.dll on Vista and newer
                *pstrError = L"Could not find dxva2.dll\n";
                ASSERT(0);
                return;
            }

            {
                // import function from evr.dll
                uintptr_t pModule = reinterpret_cast<uintptr_t>(m_hEVRLib);// just an named alias
                if (!pModule) {
                    *pstrError = L"Could not find evr.dll\n";
                    ASSERT(0);
                    return;
                }
                IMAGE_DOS_HEADER const* pDOSHeader = reinterpret_cast<IMAGE_DOS_HEADER const*>(pModule);
                IMAGE_NT_HEADERS const* pNTHeader = reinterpret_cast<IMAGE_NT_HEADERS const*>(pModule + static_cast<size_t>(static_cast<ULONG>(pDOSHeader->e_lfanew)));
                IMAGE_EXPORT_DIRECTORY const* pEAT = reinterpret_cast<IMAGE_EXPORT_DIRECTORY const*>(pModule + static_cast<size_t>(pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress));
                uintptr_t pAONbase = pModule + static_cast<size_t>(pEAT->AddressOfNames);

                __declspec(align(8)) static char const kszFunc[] = "MFCreateVideoSampleFromSurface";// 8-byte alignment used to facititate optimal 8-byte comparisons for the memcmp() intrinsic
                ptrdiff_t i = static_cast<size_t>(pEAT->NumberOfNames - 1);// convert to signed for the loop system and pointer-sized for the pointer operations
                for (;;) {
                    unsigned __int32 u32AON = *reinterpret_cast<unsigned __int32 const*>(pAONbase + i * 4);// table of four-byte elements
                    char const* kszName = reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON));
                    if (*reinterpret_cast<__int64 __unaligned const*>(kszName) == *reinterpret_cast<__int64 const*>(kszFunc)
                            && *reinterpret_cast<__int64 __unaligned const*>(kszName + 8) == *reinterpret_cast<__int64 const*>(kszFunc + 8)
                            && *reinterpret_cast<__int64 __unaligned const*>(kszName + 16) == *reinterpret_cast<__int64 const*>(kszFunc + 16)
                            && *reinterpret_cast<__int32 __unaligned const*>(kszName + 24) == *reinterpret_cast<__int32 const*>(kszFunc + 24)
                            && *reinterpret_cast<__int16 __unaligned const*>(kszName + 28) == *reinterpret_cast<__int16 const*>(kszFunc + 28)
                            && kszName[30] == kszFunc[30]) {// note that this part must compare zero end inclusive
                        // if (!memcmp(reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON)), kszFunc, sizeof(kszFunc))) { assembly checked; inlining failed
                        break;
                    } else if (--i < 0) {
                        ASSERT(0);
                        *pstrError = L"Could not read data from evr.dll.\n";
                        return;
                    }
                }
                unsigned __int16 u16AONO = *reinterpret_cast<unsigned __int16 const*>(pModule + static_cast<size_t>(pEAT->AddressOfNameOrdinals) + i * 2);// table of two-byte elements
                unsigned __int32 u32AOF = *reinterpret_cast<unsigned __int32 const*>(pModule + static_cast<size_t>(pEAT->AddressOfFunctions) + static_cast<size_t>(u16AONO) * 4);// table of four-byte elements
                m_fnMFCreateVideoSampleFromSurface = reinterpret_cast<MFCreateVideoSampleFromSurfacePtr>(pModule + static_cast<size_t>(u32AOF));
            }

            uintptr_t pModule = reinterpret_cast<uintptr_t>(m_hAVRTLib);// just a named alias
            if (pModule) {// import functions from AVRT.dll, optional, Vista and newer only
                IMAGE_DOS_HEADER const* pDOSHeader = reinterpret_cast<IMAGE_DOS_HEADER const*>(pModule);
                IMAGE_NT_HEADERS const* pNTHeader = reinterpret_cast<IMAGE_NT_HEADERS const*>(pModule + static_cast<size_t>(static_cast<ULONG>(pDOSHeader->e_lfanew)));
                IMAGE_EXPORT_DIRECTORY const* pEAT = reinterpret_cast<IMAGE_EXPORT_DIRECTORY const*>(pModule + static_cast<size_t>(pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress));
                uintptr_t pAONbase = pModule + static_cast<size_t>(pEAT->AddressOfNames);
                uintptr_t pAONObase = pModule + static_cast<size_t>(pEAT->AddressOfNameOrdinals);
                uintptr_t pAOFbase = pModule + static_cast<size_t>(pEAT->AddressOfFunctions);
                DWORD dwLoopCount = pEAT->NumberOfNames - 1;
                {
                    __declspec(align(8)) static char const kszFunc[] = "AvSetMmThreadCharacteristicsW";// 8-byte alignment used to facititate optimal 8-byte comparisons for the memcmp() intrinsic
                    ptrdiff_t i = static_cast<size_t>(dwLoopCount);// convert to signed for the loop system and pointer-sized for the pointer operations
                    for (;;) {
                        unsigned __int32 u32AON = *reinterpret_cast<unsigned __int32 const*>(pAONbase + i * 4);// table of four-byte elements
                        char const* kszName = reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON));
                        if (*reinterpret_cast<__int64 __unaligned const*>(kszName) == *reinterpret_cast<__int64 const*>(kszFunc)
                                && *reinterpret_cast<__int64 __unaligned const*>(kszName + 8) == *reinterpret_cast<__int64 const*>(kszFunc + 8)
                                && *reinterpret_cast<__int64 __unaligned const*>(kszName + 16) == *reinterpret_cast<__int64 const*>(kszFunc + 16)
                                && *reinterpret_cast<__int32 __unaligned const*>(kszName + 24) == *reinterpret_cast<__int32 const*>(kszFunc + 24)
                                && *reinterpret_cast<__int16 __unaligned const*>(kszName + 28) == *reinterpret_cast<__int16 const*>(kszFunc + 28)) {// note that this part must compare zero end inclusive
                            // if (!memcmp(reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON)), kszFunc, sizeof(kszFunc))) { assembly checked; inlining failed
                            break;
                        } else if (--i < 0) {
                            ASSERT(0);
                            goto AVRTEHandling;
                        }
                    }
                    unsigned __int16 u16AONO = *reinterpret_cast<unsigned __int16 const*>(pAONObase + i * 2);// table of two-byte elements
                    unsigned __int32 u32AOF = *reinterpret_cast<unsigned __int32 const*>(pAOFbase + static_cast<size_t>(u16AONO) * 4);// table of four-byte elements
                    m_fnAvSetMmThreadCharacteristicsW = reinterpret_cast<AvSetMmThreadCharacteristicsWPtr>(pModule + static_cast<size_t>(u32AOF));
                }
                {
                    __declspec(align(8)) static char const kszFunc[] = "AvSetMmThreadPriority";// 8-byte alignment used to facititate optimal 8-byte comparisons for the memcmp() intrinsic
                    ptrdiff_t i = static_cast<size_t>(dwLoopCount);// convert to signed for the loop system and pointer-sized for the pointer operations
                    for (;;) {
                        unsigned __int32 u32AON = *reinterpret_cast<unsigned __int32 const*>(pAONbase + i * 4);// table of four-byte elements
                        char const* kszName = reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON));
                        if (*reinterpret_cast<__int64 __unaligned const*>(kszName) == *reinterpret_cast<__int64 const*>(kszFunc)
                                && *reinterpret_cast<__int64 __unaligned const*>(kszName + 8) == *reinterpret_cast<__int64 const*>(kszFunc + 8)
                                && *reinterpret_cast<__int32 __unaligned const*>(kszName + 16) == *reinterpret_cast<__int32 const*>(kszFunc + 16)
                                && *reinterpret_cast<__int16 __unaligned const*>(kszName + 20) == *reinterpret_cast<__int16 const*>(kszFunc + 20)) {// note that this part must compare zero end inclusive
                            // if (!memcmp(reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON)), kszFunc, sizeof(kszFunc))) { assembly checked; inlining failed
                            break;
                        } else if (--i < 0) {
                            ASSERT(0);
                            goto AVRTEHandling;
                        }
                    }
                    unsigned __int16 u16AONO = *reinterpret_cast<unsigned __int16 const*>(pAONObase + i * 2);// table of two-byte elements
                    unsigned __int32 u32AOF = *reinterpret_cast<unsigned __int32 const*>(pAOFbase + static_cast<size_t>(u16AONO) * 4);// table of four-byte elements
                    m_fnAvSetMmThreadPriority = reinterpret_cast<AvSetMmThreadPriorityPtr>(pModule + static_cast<size_t>(u32AOF));
                }
                {
                    __declspec(align(8)) static char const kszFunc[] = "AvRevertMmThreadCharacteristics";// 8-byte alignment used to facititate optimal 8-byte comparisons for the memcmp() intrinsic
                    ptrdiff_t i = static_cast<size_t>(dwLoopCount);// convert to signed for the loop system and pointer-sized for the pointer operations
                    for (;;) {
                        unsigned __int32 u32AON = *reinterpret_cast<unsigned __int32 const*>(pAONbase + i * 4);// table of four-byte elements
                        char const* kszName = reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON));
                        if (*reinterpret_cast<__int64 __unaligned const*>(kszName) == *reinterpret_cast<__int64 const*>(kszFunc)
                                && *reinterpret_cast<__int64 __unaligned const*>(kszName + 8) == *reinterpret_cast<__int64 const*>(kszFunc + 8)
                                && *reinterpret_cast<__int64 __unaligned const*>(kszName + 16) == *reinterpret_cast<__int64 const*>(kszFunc + 16)
                                && *reinterpret_cast<__int64 __unaligned const*>(kszName + 24) == *reinterpret_cast<__int64 const*>(kszFunc + 24)) {// note that this part must compare zero end inclusive
                            // if (!memcmp(reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON)), kszFunc, sizeof(kszFunc))) { assembly checked; inlining failed
                            break;
                        } else if (--i < 0) {
                            ASSERT(0);
                            goto AVRTEHandling;
                        }
                    }
                    unsigned __int16 u16AONO = *reinterpret_cast<unsigned __int16 const*>(pAONObase + i * 2);// table of two-byte elements
                    unsigned __int32 u32AOF = *reinterpret_cast<unsigned __int32 const*>(pAOFbase + static_cast<size_t>(u16AONO) * 4);// table of four-byte elements
                    m_fnAvRevertMmThreadCharacteristics = reinterpret_cast<AvRevertMmThreadCharacteristicsPtr>(pModule + static_cast<size_t>(u32AOF));
                }
                goto SkipAVRTEHandling;
AVRTEHandling:
                *pstrError = L"Could not read data from AVRT.dll.\n";
                return;
            }
SkipAVRTEHandling:

            // dxva2.dll
            HRESULT hr;
            if (FAILED(hr = m_fnDXVA2CreateDirect3DDeviceManager9(&m_nResetToken, &m_pD3DManager))) {
                *pstrError = L"DXVA2CreateDirect3DDeviceManager9() failed\n";
                *pstrError += GetWindowsErrorMessage(hr, nullptr);
                ASSERT(0);
                return;
            }
            if (FAILED(hr = m_pD3DManager->ResetDevice(m_pD3DDev, m_nResetToken))) {
                *pstrError = L"IDirect3DDeviceManager9::ResetDevice() failed\n";
                *pstrError += GetWindowsErrorMessage(hr, nullptr);
                ASSERT(0);
                return;
            }

            void* pVoid;
            HANDLE hDevice;
            if (SUCCEEDED(m_pD3DManager->OpenDeviceHandle(&hDevice))) {
                if (SUCCEEDED(m_pD3DManager->GetVideoService(hDevice, IID_IDirectXVideoDecoderService, &pVoid))) {
                    TRACE(L"EVR: DXVA2 device handle = %Ix\n", hDevice);
                    HookDirectXVideoDecoderService(reinterpret_cast<IDirectXVideoDecoderService*>(pVoid));
                    reinterpret_cast<IDirectXVideoDecoderService*>(pVoid)->Release();
                }
                m_pD3DManager->CloseDeviceHandle(hDevice);
            }
            /*
            m_fnDXVA2CreateVideoService ~link~ "DXVA2CreateVideoService"
            HRESULT hr = m_fnDXVA2CreateVideoService(m_pD3DDev, IID_IDirectXVideoProcessorService, reinterpret_cast<void**>(&m_pVideoProcessorService));
            if (FAILED(hr)) {
                *phr = hr;
                _Error += L"DXVA2CreateVideoService() failed\n";
                return;
            }
            hr = m_fnDXVA2CreateVideoService(m_pD3DDev, IID_IDirectXVideoDecoderService, reinterpret_cast<void**>(&m_pVideoDecoderService));
            if (SUCCEEDED(hr)) {// not an obligated interface
                TRACE(L"EVR: HookDirectXVideoDecoderService()\n");
                HookDirectXVideoDecoderService(m_pVideoDecoderService);
            }
            */
            /*
            // mfplat.dll
            to initializer: LoadLibrary(L"mfplat.dll");, "MFCreateMediaType", "MFInitMediaTypeFromAMMediaType", "MFInitAMMediaTypeFromMFMediaType"
            */

            static_assert(sizeof(MFVideoArea) == 16, "struct MFVideoArea or platform settings changed");
            *reinterpret_cast<__int64*>(&m_mfvCurrentFrameArea) = 0;// clear the two MFOffset items
            m_u8MixerSurfaceCount = mk_pRendererSettings->MixerBuffers;// set the number of buffers
            ASSERT((m_u8MixerSurfaceCount >= 4) && (m_u8MixerSurfaceCount <= 16));// the mixer can't deal with a small buffer and the queue system has 16 slots

            // create the outer EVR
            hr = CoCreateInstance(CLSID_EnhancedVideoRenderer, static_cast<IUnknown*>(static_cast<IBaseFilter*>(&m_OuterEVR)), CLSCTX_ALL, IID_IUnknown, reinterpret_cast<void**>(&m_OuterEVR.m_pEVR));// IBaseFilter is at Vtable location 0
            if (FAILED(hr)) {
                *pstrError = L"CoCreateInstance() of EVR failed\n";
                *pstrError += GetWindowsErrorMessage(hr, nullptr);
                ASSERT(0);
                return;
            }

            // note: m_OuterEVR gets a reference count of 1, because of the following:
            hr = m_OuterEVR.m_pEVR->QueryInterface(IID_IBaseFilter, reinterpret_cast<void**>(&m_OuterEVR.m_pBaseFilter));// m_pBaseFilter does not hold a reference inside COuterEVR
            ASSERT(hr == S_OK);// interface is known to be part of EVR
            // on all return because of failure cases of this function use: m_OuterEVR.m_pEVR->Release(); do not let the reference count of m_OuterEVR hit 0 because of self-destruction issues

            hr = m_OuterEVR.m_pEVR->QueryInterface(IID_IMFVideoRenderer, &pVoid);
            ASSERT(hr == S_OK);// interface is known to be part of EVR

            __assume(this);// fix assembly: the compiler generated tests for null pointer input on static_cast<T>(this)
            hr = reinterpret_cast<IMFVideoRenderer*>(pVoid)->InitializeRenderer(nullptr, static_cast<IMFVideoPresenter*>(this));
            reinterpret_cast<IMFVideoRenderer*>(pVoid)->Release();
            if (FAILED(hr)) {
                *pstrError = L"IMFVideoRenderer::InitializeRenderer() failed\n";
                *pstrError += GetWindowsErrorMessage(hr, nullptr);
                ASSERT(0);
                m_OuterEVR.m_pEVR->Release();
                return;
            }

            // Set EVR custom presenter
            // EVRFilterConfig::SetNumberOfStreams() (GUID: IID_IEVRFilterConfig) allows additional substreams to connect next to the main one. This is currently unused.

            IPin* pPin = GetFirstPin(m_OuterEVR.m_pBaseFilter);// GetFirstPin returns a pointer without incrementing the reference count
            if (SUCCEEDED(pPin->QueryInterface(IID_IMemInputPin, &pVoid))) {
                // No NewSegment : no chocolate :o)
                m_bUseInternalTimer = HookNewSegmentAndReceive(pPin, reinterpret_cast<IMemInputPin*>(pVoid));
                reinterpret_cast<IMemInputPin*>(pVoid)->Release();
            }
        }
        __declspec(nothrow noalias) __forceinline bool TestForIntermediateState() {
            // callback for COuterEVR to handle some IBaseFilter parts
            if (m_bSignaledStarvation) {
                unsigned __int8 u8Samples = m_u8MixerSurfaceCount >> 1;
                // no lock required on m_ScheduledSamplesQueue
                if (!g_bNoDuration && ((m_ScheduledSamplesQueue.m_u8QueueElemCount < u8Samples) || (m_dLastSampleOffset < (-2.0 * m_dDetectedVideoTimePerFrame)))) {
                    return true;// signals VFW_S_STATE_INTERMEDIATE and State_Paused in COuterEVR
                }
                m_bSignaledStarvation = false;
            }
            return false;
        }
        __declspec(nothrow noalias) __forceinline void ResetStats() {
            // callback for CDX9AllocatorPresenter
            TRACE(L"EVR: Reset stats\n");
            m_pcFramesDropped = 0;
            m_pcFramesDrawn   = 0;
            m_piAvg           = 0;
            m_piDev           = 0;
        }
        __declspec(nothrow noalias) __forceinline void DropFrame() {
            // callback for CDX9AllocatorPresenter
            TRACE(L"EVR: Dropped frame\n");
            ++m_pcFramesDropped;
        }
        __declspec(nothrow noalias) __forceinline void OnVBlankFinished(__in double dPerformanceCounter) {
            // callback for CDX9AllocatorPresenter to gather timing stats
            if (!m_pCurrentDisplayedSample) {// this can happen while flushing samples
                TRACE(L"EVR: OnVBlankFinished() skipped due to unavailable current mixer sample\n");
                return;
            }
            if (!m_dStreamReferenceVideoFrameRate) {// this can happen during negotiating the media type
                TRACE(L"EVR: OnVBlankFinished() skipped due to unavailable reference video frame rate\n");
                return;
            }

            double dClockTime;
            if (!m_bSignaledStarvation) {
                dClockTime = GetClockTime(dPerformanceCounter);
                m_dStarvationClock = dClockTime;
            } else {
                TRACE(L"EVR: starvation clock used\n");
                dClockTime = m_dStarvationClock;
            }

            double dSampleDuration;
            LONGLONG llUtil;
            if (SUCCEEDED(m_pCurrentDisplayedSample->GetSampleDuration(&llUtil))) {
                // note: highly variable in some video streams, can even be 0 sometimes
                dSampleDuration = static_cast<double>(llUtil) * 0.0000001;
            } else {
                ASSERT(0);// bugged duration
                return;
            }
            double dSampleTime = dClockTime;
            if (SUCCEEDED(m_pCurrentDisplayedSample->GetSampleTime(&llUtil))) {
                dSampleTime = static_cast<double>(llUtil) * 0.0000001;
            }

            ++m_upNextSyncOffsetPos;// note: this index will start at 1, not 0 on initialization, but that's intended
            m_upNextSyncOffsetPos &= NB_JITTER - 1; // modulo action by low bitmask
            double dSyncOffset = dSampleTime - dClockTime;
            m_adSyncOffset[m_upNextSyncOffsetPos] = dSyncOffset;
            if (dSyncOffset < -6.0 * m_dDetectedRefreshTime) {
                TRACE(L"EVR: sync offset deviated much too low\n");
            }

            // TRACE(L"EVR: SyncOffset(%d, %d): %8I64d     %8I64d     %8I64d \n", m_u8CurrentMixerSurface, m_u8VSyncMode, m_dLastPredictedSync, -SyncOffset, m_dLastPredictedSync - (-SyncOffset));

            m_dMaxSyncOffset = -2147483648.0;
            m_dMinSyncOffset = 2147483648.0;

            double dAvrageSum = 0.0;
            ptrdiff_t i = NB_JITTER - 1;
            do {
                dAvrageSum += m_adSyncOffset[i];
                if (m_dMaxSyncOffset < m_adSyncOffset[i]) {
                    m_dMaxSyncOffset = m_adSyncOffset[i];
                }
                if (m_dMinSyncOffset > m_adSyncOffset[i]) {
                    m_dMinSyncOffset = m_adSyncOffset[i];
                }
            } while (--i >= 0);

            double dMeanOffset = dAvrageSum / NB_JITTER;
            double dDeviationSum = 0.0;
            i = NB_JITTER - 1;
            do {
                double dDeviation = m_adSyncOffset[i] - dMeanOffset;
                dDeviationSum += dDeviation * dDeviation;
            } while (--i >= 0);

            m_dSyncOffsetStdDev = sqrt(dDeviationSum / NB_JITTER);
            m_dSyncOffsetAvr = dMeanOffset;
            m_bSyncStatsAvailable = true;
        }
    };
}
